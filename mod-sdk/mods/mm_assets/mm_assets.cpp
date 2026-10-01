// Los recursos de Majora's Mask para Unbound. Al arrancar, si ningún archivo cargado los trae, pide la ROM
// y la extrae entera con el ZAPD que ya va dentro del exe, el mismo que saca oot.o2r. El mod solo pone lo
// que a ZAPD le falta: el árbol de XML que describe la ROM, que viaja dentro de su propio .o2r.
#include "soh/ModApi/ModApi.h"
#include "soh/Extractor/portable-file-dialogs.h"
#include "sfx/mm_sfx_service.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// An archive extracted before the icons were is missing them, so it has to prove it has both kinds.
static const char* const kMarkerResources[] = {
    "objects/object_link_goron/gLinkGoronGoronPunchEffectDL",
    "icon_item_static_yar/gItemIconKafeisMaskTex",
};
#define INSTALLED_ARCHIVE_NAME "mm.o2r"
// The installed archive is mounted while the game runs, so the new one waits beside it until the restart.
#define PENDING_ARCHIVE_NAME "mm.o2r.new"
// Los XML que el mod trae dentro, y el sitio donde hay que dejarlos para que ZAPD los lea del disco.
#define ZAPD_ASSET_MASK "mm_zapd/*"
#define ZAPD_ASSET_ROOT "mm_zapd"
#define ZAPD_ROM_NAME "mm.z64"
#define EXTRACT_DIRECTORY ".mm-extract"

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

std::vector<uint8_t> LoadRom(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

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

bool WriteRom(const std::filesystem::path& path, const std::vector<uint8_t>& rom) {
    std::ofstream file(path, std::ios_base::openmode(std::ios_base::binary | std::ios_base::trunc));

    file.write((const char*)rom.data(), (std::streamsize)rom.size());
    return file.good();
}

bool HasExtractedAssets() {
    for (const char* marker : kMarkerResources) {
        if (!sApi->HasResource(marker)) {
            return false;
        }
    }
    return true;
}

std::filesystem::path GameDirectory() {
#ifdef _WIN32
    wchar_t executable[MAX_PATH];
    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) != 0) {
        return std::filesystem::path(executable).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

void Complain(const std::string& message) {
    pfd::message("Majora's Mask assets", message, pfd::choice::ok, pfd::icon::error).result();
}

#ifdef _WIN32
// Los argumentos con los que arrancó el juego, sin el ejecutable: relanzarlo sin ellos perdería cosas como
// el modo hijo de Fleet Ship.
std::wstring OriginalArguments() {
    const wchar_t* line = GetCommandLineW();

    if (*line == L'"') {
        line = wcschr(line + 1, L'"');
        line = line == nullptr ? L"" : line + 1;
    } else {
        const wchar_t* space = wcschr(line, L' ');
        line = space == nullptr ? L"" : space;
    }
    return std::wstring(line);
}
#endif

// El archivo recién escrito solo se monta al arrancar, y los mods que lo esperaban ya se comprobaron. El
// relanzado espera un momento antes de abrir el juego nuevo: si los dos procesos se solapan, el segundo se
// encuentra el log y los archivos en manos del primero.
void RestartGame(const std::filesystem::path& pendingArchive, const std::filesystem::path& installedArchive) {
#ifdef _WIN32
    wchar_t executable[MAX_PATH];

    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) != 0) {
        std::wstring command = L"cmd.exe /c ping -n 3 127.0.0.1 >nul & move /y \"" + pendingArchive.wstring() +
                               L"\" \"" + installedArchive.wstring() + L"\" >nul & start \"\" \"" +
                               std::wstring(executable) + L"\"" + OriginalArguments();
        STARTUPINFOW startup = {};
        PROCESS_INFORMATION process = {};

        startup.cb = sizeof(startup);
        if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS,
                           nullptr, GameDirectory().wstring().c_str(), &startup, &process)) {
            CloseHandle(process.hProcess);
            CloseHandle(process.hThread);
        }
    }
#endif
    exit(0);
}

bool ExtractRom(std::vector<uint8_t>& rom, const RomLayout& layout, const std::filesystem::path& archivePath) {
    const std::filesystem::path workspace = GameDirectory() / EXTRACT_DIRECTORY;
    std::error_code error;

    std::filesystem::remove_all(workspace, error);
    if (sApi->ExportArchiveFiles(ZAPD_ASSET_MASK, workspace.string().c_str()) == 0) {
        Complain("This mod's copy of the Majora's Mask asset descriptions is missing.");
        return false;
    }
    const std::filesystem::path zapdRom = workspace / ZAPD_ROM_NAME;
    if (!UnpackYarArchives(rom, layout) || !WriteRom(zapdRom, rom)) {
        Complain("Could not unpack the icon archives of that ROM.");
        std::filesystem::remove_all(workspace, error);
        return false;
    }

    const std::string xmlDirectory = std::string("assets/xml/") + layout.xmlVersion;
    const std::string configPath = std::string("assets/Config_") + layout.xmlVersion + ".xml";
    const std::string assetsDirectory = (workspace / ZAPD_ASSET_ROOT).string();
    const std::string outputPath = archivePath.string();
    const std::string romArgument = zapdRom.string();
    SOHO2rExtractRequest request = {};

    request.structSize = sizeof(request);
    request.romPath = romArgument.c_str();
    request.assetsDir = assetsDirectory.c_str();
    request.xmlDir = xmlDirectory.c_str();
    request.configPath = configPath.c_str();
    request.filelistDir = "assets/filelists";
    request.outputPath = outputPath.c_str();

    const bool extracted = sApi->ExtractRom(&request);
    std::filesystem::remove_all(workspace, error);
    return extracted;
}

void ExtractFromRom() {
    const pfd::button answer = pfd::message("Majora's Mask assets",
                                            "Some installed mods need assets from Majora's Mask, and they are not "
                                            "here yet.\n\nRead them out of your Majora's Mask ROM now?",
                                            pfd::choice::yes_no, pfd::icon::question)
                                   .result();
    if (answer != pfd::button::yes) {
        return;
    }

    const std::vector<std::string> chosen =
        pfd::open_file("Select your Majora's Mask ROM", ".",
                       { "Nintendo 64 ROM", "*.z64 *.n64 *.v64", "All files", "*" })
            .result();
    if (chosen.empty()) {
        return;
    }

    std::vector<uint8_t> rom = LoadRom(chosen[0]);
    const RomLayout* layout = rom.empty() ? nullptr : FindLayout(rom);
    if (layout == nullptr) {
        Complain("That is not a Majora's Mask US 1.0 or US GameCube ROM, which are the two this mod can read.");
        return;
    }

    pfd::message("Majora's Mask assets",
                 "Extraction will run now and takes a few minutes. The game will look frozen; do not close it.",
                 pfd::choice::ok, pfd::icon::info)
        .result();

    std::error_code error;
    const std::filesystem::path mods = GameDirectory() / "mods";
    const std::filesystem::path pendingArchive = mods / PENDING_ARCHIVE_NAME;
    std::filesystem::create_directories(mods, error);
    if (!ExtractRom(rom, *layout, pendingArchive)) {
        Complain("Could not extract that ROM. The log has what ZAPD complained about.");
        return;
    }

    pfd::message("Majora's Mask assets",
                 INSTALLED_ARCHIVE_NAME " is ready. Unbound will restart now so the mods waiting for it can load.",
                 pfd::choice::ok, pfd::icon::info)
        .result();
    RestartGame(pendingArchive, mods / INSTALLED_ARCHIVE_NAME);
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
    ExtractFromRom();
}
