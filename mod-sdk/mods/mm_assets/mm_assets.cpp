// Los recursos de Majora's Mask para Unbound. Al arrancar, si ningún archivo cargado los trae, pide la ROM
// y la extrae entera con el ZAPD que ya va dentro del exe, el mismo que saca oot.o2r. El mod solo pone lo
// que a ZAPD le falta: el árbol de XML que describe la ROM, que viaja dentro de su propio .o2r. Leer la ROM,
// escribir mm.o2r y reiniciar lo hace el juego, que se lo pregunta antes al jugador.
#include "soh/ModApi/ModApi.h"
#include "sfx/mm_sfx_service.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// An archive extracted before the icons were is missing them, so it has to prove it has both kinds.
static const char* const kMarkerResources[] = {
    "objects/object_link_goron/gLinkGoronGoronPunchEffectDL",
    "icon_item_static_yar/gItemIconKafeisMaskTex",
};
#define INSTALLED_ARCHIVE_NAME "mm.o2r"
#define DIALOG_TITLE "Majora's Mask assets"
// Los XML que el mod trae dentro; el juego los deja en disco bajo ZAPD_ASSET_ROOT para que ZAPD los lea.
#define ZAPD_ASSET_MASK "mm_zapd/*"
#define ZAPD_ASSET_ROOT "mm_zapd"

// Palabra 4 de la cabecera de la ROM: la que distingue las versiones que ZAPD sabe leer.
#define MM_VERSION_US_10 0x5354631C
#define MM_VERSION_US_10_UNCOMPRESSED 0xDA6983E7
#define MM_VERSION_US_GC 0xB443EB08

#define ROM_MAGIC_BIG_ENDIAN 0x80371240
#define ROM_MAGIC_BYTE_SWAPPED 0x37804012
#define ROM_MAGIC_LITTLE_ENDIAN 0x40123780
#define ROM_CRC_OFFSET 0x10
#define DMA_ENTRY_SIZE 16
#define YAZ0_HEADER_SIZE 16

static const SOHModApi* sApi;

namespace {

// Cada versión de la ROM tiene su propio árbol de XML, porque los offsets no son los mismos.
struct RomLayout {
    uint32_t crc;
    const char* xmlVersion;
    uint32_t dmaOffset;
};

const RomLayout kRomLayouts[] = {
    { MM_VERSION_US_10, "N64_US", 0x1A500 },
    { MM_VERSION_US_10_UNCOMPRESSED, "N64_US", 0x1A500 },
    { MM_VERSION_US_GC, "GC_US", 0x1AE90 },
};

// The icon, item name and map archives: dmadata rows ZAPD only unpacks when built with GAME_MM. Unbound builds
// it with GAME_OOT, which hands the raw archive to every texture and reads past its end.
const int32_t kYarEntries[] = { 15, 16, 17, 18, 19, 20, 22 };

uint32_t ReadU32BE(const uint8_t* at) {
    return ((uint32_t)at[0] << 24) | ((uint32_t)at[1] << 16) | ((uint32_t)at[2] << 8) | (uint32_t)at[3];
}

void WriteU32BE(uint8_t* at, uint32_t value) {
    at[0] = (uint8_t)(value >> 24);
    at[1] = (uint8_t)(value >> 16);
    at[2] = (uint8_t)(value >> 8);
    at[3] = (uint8_t)value;
}

bool NormalizeByteOrder(std::vector<uint8_t>& rom) {
    if (rom.size() < 0x40 || rom.size() % 4 != 0) {
        return false;
    }
    switch (ReadU32BE(rom.data())) {
        case ROM_MAGIC_BIG_ENDIAN:
            return true;
        case ROM_MAGIC_BYTE_SWAPPED:
            for (size_t at = 0; at < rom.size(); at += 2) {
                std::swap(rom[at], rom[at + 1]);
            }
            return true;
        case ROM_MAGIC_LITTLE_ENDIAN:
            for (size_t at = 0; at < rom.size(); at += 4) {
                std::swap(rom[at], rom[at + 3]);
                std::swap(rom[at + 1], rom[at + 2]);
            }
            return true;
        default:
            return false;
    }
}

std::vector<uint8_t> ReadRom(const SOHUserFile& file) {
    std::vector<uint8_t> rom(file.data, file.data + file.size);

    if (!NormalizeByteOrder(rom)) {
        rom.clear();
    }
    return rom;
}

const RomLayout* FindLayout(const std::vector<uint8_t>& rom) {
    const uint32_t crc = ReadU32BE(&rom[ROM_CRC_OFFSET]);

    for (const RomLayout& layout : kRomLayouts) {
        if (layout.crc == crc) {
            return &layout;
        }
    }
    return nullptr;
}

bool DecodeYaz0(const uint8_t* data, size_t size, size_t at, std::vector<uint8_t>& out) {
    if (at + YAZ0_HEADER_SIZE > size || memcmp(data + at, "Yaz0", 4) != 0) {
        return false;
    }
    const size_t end = out.size() + ReadU32BE(data + at + 4);
    size_t src = at + YAZ0_HEADER_SIZE;

    while (out.size() < end) {
        if (src >= size) {
            return false;
        }
        const uint8_t codes = data[src++];
        for (int32_t bit = 7; bit >= 0 && out.size() < end; bit--) {
            if (codes & (1 << bit)) {
                if (src >= size) {
                    return false;
                }
                out.push_back(data[src++]);
                continue;
            }
            if (src + 1 >= size) {
                return false;
            }
            const size_t distance = (((data[src] & 0xF) << 8) | data[src + 1]) + 1;
            size_t count = data[src] >> 4;
            src += 2;
            if (count == 0) {
                if (src >= size) {
                    return false;
                }
                count = data[src++] + 0x12;
            } else {
                count += 2;
            }
            if (distance > out.size()) {
                return false;
            }
            for (size_t i = 0; i < count; i++) {
                out.push_back(out[out.size() - distance]);
            }
        }
    }
    return true;
}

// A yar archive is a table of block offsets followed by one Yaz0 block per texture; the table's last word
// marks where the archive ends, not a block.
bool DecodeYar(const uint8_t* data, size_t size, std::vector<uint8_t>& out) {
    if (size < 8) {
        return false;
    }
    const uint32_t tableSize = ReadU32BE(data);
    if (tableSize < 8 || tableSize > size || tableSize % 4 != 0) {
        return false;
    }
    for (uint32_t i = 0; i + 1 < tableSize / 4; i++) {
        const size_t block = tableSize + (i == 0 ? 0 : ReadU32BE(data + (i * 4)));
        if (!DecodeYaz0(data, size, block, out)) {
            return false;
        }
    }
    return true;
}

// ZAPD reads an uncompressed row straight from physStart for virtEnd - virtStart bytes, so each archive is
// decoded, appended to this private copy of the ROM and its row pointed at the result.
bool UnpackYarArchives(std::vector<uint8_t>& rom, const RomLayout& layout) {
    for (int32_t index : kYarEntries) {
        const size_t row = layout.dmaOffset + (size_t)index * DMA_ENTRY_SIZE;
        if (row + DMA_ENTRY_SIZE > rom.size()) {
            return false;
        }
        const uint32_t virtStart = ReadU32BE(&rom[row]);
        const uint32_t virtEnd = ReadU32BE(&rom[row + 4]);
        const uint32_t physStart = ReadU32BE(&rom[row + 8]);
        const uint32_t physEnd = ReadU32BE(&rom[row + 12]);
        if (physEnd != 0 || virtEnd < virtStart || (size_t)physStart + (virtEnd - virtStart) > rom.size()) {
            return false;
        }

        std::vector<uint8_t> decoded;
        if (!DecodeYar(&rom[physStart], virtEnd - virtStart, decoded)) {
            return false;
        }
        rom.resize((rom.size() + 15) & ~(size_t)15);
        const uint32_t decodedStart = (uint32_t)rom.size();
        rom.insert(rom.end(), decoded.begin(), decoded.end());
        WriteU32BE(&rom[row + 4], virtStart + (uint32_t)decoded.size());
        WriteU32BE(&rom[row + 8], decodedStart);
    }
    return true;
}

bool HasExtractedAssets() {
    for (const char* marker : kMarkerResources) {
        if (!sApi->HasResource(marker)) {
            return false;
        }
    }
    return true;
}

// El juego deja los XML en disco, escribe la ROM, corre ZAPD y pone mm.o2r en mods/ (al lado, si el viejo
// está montado, y lo cambia al siguiente arranque).
bool BuildArchiveFromRom(std::vector<uint8_t>& rom, const RomLayout& layout) {
    if (!UnpackYarArchives(rom, layout)) {
        sApi->TellPlayer(DIALOG_TITLE, "Could not unpack the icon archives of that ROM.");
        return false;
    }

    const std::string xmlDirectory = std::string("assets/xml/") + layout.xmlVersion;
    const std::string configPath = std::string("assets/Config_") + layout.xmlVersion + ".xml";
    SOHRomExtractRequest request = {};

    request.structSize = sizeof(request);
    request.rom = rom.data();
    request.romSize = rom.size();
    request.zapdAssetMask = ZAPD_ASSET_MASK;
    request.zapdAssetRoot = ZAPD_ASSET_ROOT;
    request.xmlDir = xmlDirectory.c_str();
    request.configPath = configPath.c_str();
    request.filelistDir = "assets/filelists";
    request.outputFileName = INSTALLED_ARCHIVE_NAME;
    return sApi->ExtractRomToModsFolder(&request);
}

void ExtractFromRom() {
    if (!sApi->AskPlayer(DIALOG_TITLE, "Some installed mods need assets from Majora's Mask, and they are not here "
                                       "yet.\n\nRead them out of your Majora's Mask ROM now?")) {
        return;
    }

    SOHUserFile file = {};
    file.structSize = sizeof(file);
    if (!sApi->PickUserFile("Select your Majora's Mask ROM", "Nintendo 64 ROM", "*.z64 *.n64 *.v64", &file)) {
        return;
    }
    std::vector<uint8_t> rom = ReadRom(file);
    sApi->FreeUserFile(&file);

    const RomLayout* layout = rom.empty() ? nullptr : FindLayout(rom);
    if (layout == nullptr) {
        sApi->TellPlayer(DIALOG_TITLE,
                         "That is not a Majora's Mask US 1.0 or US GameCube ROM, which are the two this mod can read.");
        return;
    }
    if (!BuildArchiveFromRom(rom, *layout)) {
        sApi->TellPlayer(DIALOG_TITLE, INSTALLED_ARCHIVE_NAME " was not created. The log says why.");
        return;
    }
    sApi->RequestRestart(INSTALLED_ARCHIVE_NAME " is ready, and the mods waiting for it load after a restart");
}

} // namespace

extern "C" SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

extern "C" SOH_MOD_EXPORT void ModInit(void) {
    MmSfxService_Init(sApi);
    if (HasExtractedAssets()) {
        return;
    }
    if (!SOH_MOD_API_HAS(sApi, RequestRestart)) {
        sApi->Log("mm_assets: this Unbound is too old to extract Majora's Mask assets");
        return;
    }
    ExtractFromRom();
}
