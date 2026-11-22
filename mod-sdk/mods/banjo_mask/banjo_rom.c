#include "banjo_rom.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libultraship/bridge/consolevariablebridge.h>

#include "banjo_menu.h"
#include "bk_assets.h"

#define BANJO_ROM_TITLE "Banjo Mask"
#define BANJO_ROM_FILE "banjo.z64"
#define BANJO_ROM_PROMPT_CVAR "gMods.BanjoMask.RomPromptShown"
#define BANJO_ROM_PATTERNS "*.z64 *.n64 *.v64"
#define BANJO_ROM_EXTENSIONS ".z64;.n64;.v64"

// The asset table is read at fixed offsets of the USA 1.0 release, so no other dump can work.
#define BANJO_ROM_CRC32 0xAD429961u

#define ROM_MAGIC_BIG_ENDIAN 0x80371240u
#define ROM_MAGIC_BYTE_SWAPPED 0x37804012u
#define ROM_MAGIC_LITTLE_ENDIAN 0x40123780u

typedef struct BanjoRomSearch {
    uint8_t* rom;
    bool isFound;
} BanjoRomSearch;

static const SOHModApi* sApi;
static uint8_t* sRom;

static uint32_t CalculateCrc32(const uint8_t* data, uint32_t size) {
    uint32_t table[256];

    for (uint32_t index = 0; index < ARRAY_COUNT(table); index++) {
        uint32_t value = index;

        for (int32_t bit = 0; bit < 8; bit++) {
            value = (value & 1) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
        }
        table[index] = value;
    }

    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t offset = 0; offset < size; offset++) {
        crc = table[(crc ^ data[offset]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

static void ReverseRomWords(uint8_t* rom, uint32_t wordSize) {
    for (uint32_t offset = 0; offset < BANJO_ROM_SIZE; offset += wordSize) {
        for (uint32_t low = 0; low < wordSize / 2; low++) {
            const uint32_t high = wordSize - 1 - low;
            const uint8_t saved = rom[offset + low];

            rom[offset + low] = rom[offset + high];
            rom[offset + high] = saved;
        }
    }
}

static bool ConvertToBigEndian(uint8_t* rom) {
    const uint32_t magic = ((uint32_t)rom[0] << 24) | (rom[1] << 16) | (rom[2] << 8) | rom[3];

    switch (magic) {
        case ROM_MAGIC_BIG_ENDIAN:
            return true;
        case ROM_MAGIC_BYTE_SWAPPED:
            ReverseRomWords(rom, 2);
            return true;
        case ROM_MAGIC_LITTLE_ENDIAN:
            ReverseRomWords(rom, 4);
            return true;
        default:
            return false;
    }
}

static bool IsSupportedRom(uint8_t* rom, uint64_t size) {
    return size == BANJO_ROM_SIZE && ConvertToBigEndian(rom) && CalculateCrc32(rom, BANJO_ROM_SIZE) == BANJO_ROM_CRC32;
}

static bool KeepSearchingModsFolder(const char* path, const uint8_t* data, uint64_t size, void* userData) {
    BanjoRomSearch* search = userData;

    if (size != BANJO_ROM_SIZE) {
        return true;
    }
    memcpy(search->rom, data, BANJO_ROM_SIZE);
    search->isFound = IsSupportedRom(search->rom, size);
    return !search->isFound;
}

static bool LoadRomFromModsFolder(void) {
    BanjoRomSearch search = { malloc(BANJO_ROM_SIZE), false };

    if (search.rom == NULL) {
        return false;
    }
    sApi->ReadModsFolderFiles(BANJO_ROM_EXTENSIONS, KeepSearchingModsFolder, &search);
    if (!search.isFound) {
        free(search.rom);
        return false;
    }
    sRom = search.rom;
    BkAssets_SetRom(sRom, BANJO_ROM_SIZE);
    return true;
}

static void CopyPickedRomToModsFolder(void) {
    SOHUserFile file = { sizeof(SOHUserFile) };

    if (!sApi->PickUserFile("Select your Banjo-Kazooie (USA 1.0) ROM", "Nintendo 64 ROM", BANJO_ROM_PATTERNS, &file)) {
        return;
    }
    if (!IsSupportedRom(file.data, file.size)) {
        sApi->TellPlayer(BANJO_ROM_TITLE, "That file is not an original Banjo-Kazooie (USA 1.0) ROM.");
    } else if (!sApi->SaveToModsFolder(BANJO_ROM_FILE, file.data, BANJO_ROM_SIZE) || !LoadRomFromModsFolder()) {
        sApi->TellPlayer(BANJO_ROM_TITLE, "The ROM could not be copied to the mods folder. See the game log.");
    }
    sApi->FreeUserFile(&file);
}

static void DrawRomStatus(void) {
    if (sRom != NULL) {
        ModUI_Text("Banjo-Kazooie ROM: ready");
        return;
    }
    ModUI_Text("Banjo-Kazooie ROM: missing");
    if (ModUI_Button("Select Banjo-Kazooie ROM", 0.0f, 0.0f)) {
        CopyPickedRomToModsFolder();
    }
}

void BanjoRom_RegisterMenu(const SOHModApi* api) {
    sApi = api;
    BanjoMenu_AddDrawn(api, BANJO_COLUMN_CONFIGURATION, "banjo_mask.rom_status", DrawRomStatus);
}

void BanjoRom_LoadOrAskOnce(void) {
    if (LoadRomFromModsFolder() || CVarGetInteger(BANJO_ROM_PROMPT_CVAR, 0)) {
        return;
    }
    CVarSetInteger(BANJO_ROM_PROMPT_CVAR, 1);
    CVarSave();
    if (sApi->AskPlayer(BANJO_ROM_TITLE, "Banjo Mask needs your own Banjo-Kazooie (USA 1.0) ROM. Select it now? "
                                         "You can also place it in the mods folder.")) {
        CopyPickedRomToModsFolder();
    }
}

const uint8_t* BanjoRom_Get(void) {
    return sRom;
}
