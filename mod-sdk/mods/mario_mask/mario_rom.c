#include "mario_rom.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libultraship/bridge/consolevariablebridge.h>

#include "mario_menu.h"

#define MARIO_ROM_TITLE "Mario Mask"
#define MARIO_ROM_FILE "sm64.z64"
#define MARIO_ROM_PROMPT_CVAR "gMods.MarioMask.RomPromptShown"
#define MARIO_ROM_PATTERNS "*.z64 *.n64 *.v64"
#define MARIO_ROM_EXTENSIONS ".z64;.n64;.v64"

// libsm64 reads fixed offsets of the USA release, so no other dump can work.
#define MARIO_ROM_SIZE 0x800000u
#define MARIO_ROM_CRC32 0x3CE60709u

#define ROM_MAGIC_BIG_ENDIAN 0x80371240u
#define ROM_MAGIC_BYTE_SWAPPED 0x37804012u
#define ROM_MAGIC_LITTLE_ENDIAN 0x40123780u

typedef struct MarioRomSearch {
    uint8_t* rom;
    bool isFound;
} MarioRomSearch;

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
    for (uint32_t offset = 0; offset < MARIO_ROM_SIZE; offset += wordSize) {
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
    return size == MARIO_ROM_SIZE && ConvertToBigEndian(rom) && CalculateCrc32(rom, MARIO_ROM_SIZE) == MARIO_ROM_CRC32;
}

static bool KeepSearchingModsFolder(const char* path, const uint8_t* data, uint64_t size, void* userData) {
    MarioRomSearch* search = userData;

    if (size != MARIO_ROM_SIZE) {
        return true;
    }
    memcpy(search->rom, data, MARIO_ROM_SIZE);
    search->isFound = IsSupportedRom(search->rom, size);
    return !search->isFound;
}

static bool LoadRomFromModsFolder(void) {
    MarioRomSearch search = { malloc(MARIO_ROM_SIZE), false };

    if (search.rom == NULL) {
        return false;
    }
    sApi->ReadModsFolderFiles(MARIO_ROM_EXTENSIONS, KeepSearchingModsFolder, &search);
    if (!search.isFound) {
        free(search.rom);
        return false;
    }
    sRom = search.rom;
    return true;
}

static void CopyPickedRomToModsFolder(void) {
    SOHUserFile file = { sizeof(SOHUserFile) };

    if (!sApi->PickUserFile("Select your Super Mario 64 (USA) ROM", "Nintendo 64 ROM", MARIO_ROM_PATTERNS, &file)) {
        return;
    }
    if (!IsSupportedRom(file.data, file.size)) {
        sApi->TellPlayer(MARIO_ROM_TITLE, "That file is not an original Super Mario 64 (USA) ROM.");
    } else if (!sApi->SaveToModsFolder(MARIO_ROM_FILE, file.data, MARIO_ROM_SIZE) || !LoadRomFromModsFolder()) {
        sApi->TellPlayer(MARIO_ROM_TITLE, "The ROM could not be copied to the mods folder. See the game log.");
    }
    sApi->FreeUserFile(&file);
}

static void DrawRomStatus(void) {
    if (sRom != NULL) {
        ModUI_Text("Super Mario 64 ROM: ready");
        return;
    }
    ModUI_Text("Super Mario 64 ROM: missing");
    if (ModUI_Button("Select Super Mario 64 ROM", 0.0f, 0.0f)) {
        CopyPickedRomToModsFolder();
    }
}

void MarioRom_RegisterMenu(const SOHModApi* api) {
    sApi = api;
    MarioMenu_AddDrawn(api, MARIO_COLUMN_CONFIGURATION, "mario_mask.rom_status", DrawRomStatus);
}

void MarioRom_LoadOrAskOnce(void) {
    if (LoadRomFromModsFolder() || CVarGetInteger(MARIO_ROM_PROMPT_CVAR, 0)) {
        return;
    }
    CVarSetInteger(MARIO_ROM_PROMPT_CVAR, 1);
    CVarSave();
    if (sApi->AskPlayer(MARIO_ROM_TITLE, "Mario Mask needs your own Super Mario 64 (USA) ROM. Select it now? "
                                         "You can also place it in the mods folder.")) {
        CopyPickedRomToModsFolder();
    }
}

const uint8_t* MarioRom_Get(void) {
    return sRom;
}
