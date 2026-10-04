#include "test_require.h"
#include "mods/pak_loader/pak_selection.h"
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

using s32 = int32_t;
using u8 = uint8_t;
using u32 = uint32_t;
struct PakModel {
    std::string pakPath;
    bool adultReady;
    bool childReady;
    bool isEquipmentOnly;
    std::map<u32, void*> adultEquipDLs;
    std::map<u32, void*> childEquipDLs;
};
static std::vector<PakModel> sModels;
static std::string sModsPath = "/game/mods";
static s32 sSelectedAdultIndex = -1, sSelectedChildIndex = -1, sSelectedEquipIndex = -1;
static constexpr int kSlotCount = 2;
static const struct {
    const char* cvarKey;
    u32 aliases[2];
} sSlotGroups[] = { { "Sword0", { 0x50C0, 0 } }, { "Shield0", { 0x5108, 0 } } };
static s32 sSlotMix[kSlotCount];
static u8 sSlotMixInitialized = 0;
static std::map<std::string, s32> integers;
static std::map<std::string, std::string> strings;
static int saves = 0, mounts = 0;
static s32 CVarGetInteger(const char* name, s32 fallback) {
    auto it = integers.find(name);
    return it != integers.end() ? it->second : fallback;
}
static const char* CVarGetString(const char* name, const char* fallback) {
    auto it = strings.find(name);
    return it != strings.end() ? it->second.c_str() : fallback;
}
static void CVarSetInteger(const char* name, s32 value) {
    integers[name] = value;
}
static void CVarSetString(const char* name, const char* value) {
    strings[name] = value;
}
namespace Ship {
struct Context {
    static Context* GetRawInstance() {
        static Context instance;
        return &instance;
    }
    Context* GetWindow() {
        return this;
    }
    Context* GetGui() {
        return this;
    }
    void SaveConsoleVariablesNextFrame() {
        ++saves;
    }
};
} // namespace Ship
static void O2rUpdateMounts() {
    ++mounts;
}
#define PAK_LOG(...) ((void)0)

// Actual production Save/Restore/Select functions, unchanged; filesystem/config
// storage and mount scheduling are the test boundaries, not the selection logic.
#include "pak_selection_runtime.inc"

static constexpr const char* adult = "gMods.PakLoader.AdultModel";
static constexpr const char* child = "gMods.PakLoader.ChildModel";
static constexpr const char* equipment = "gMods.PakLoader.Equipment";
static constexpr const char* sword = "gMods.PakLoader.SlotMix.Sword0";
static constexpr const char* shield = "gMods.PakLoader.SlotMix.Shield0";

int main() {
    sModels = {
        { "/game/mods/Din.pak", true, false, false, { { 0x50C0, nullptr } }, {} },
        { "/game/mods/Saria.zobj", false, true, false, {}, {} },
        { "/game/mods/Ruto.pak", false, false, true, { { 0x5108, nullptr } }, {} },
    };
    CVarSetInteger(adult, 0);
    CVarSetInteger(child, 0);     // Stale child index now points to adult-only Din.
    CVarSetInteger(equipment, 0); // Combined donors must survive reboot.
    CVarSetInteger(sword, 0);
    CVarSetInteger(shield, 2);
    EnsureSlotMixLoaded(); // Simulate an early read before boot restoration.
    RestoreSelections();
    REQUIRE(CVarGetInteger(child, 99) == -1);
    REQUIRE(strings[std::string(adult) + "Path"] == "Din.pak");
    REQUIRE(CVarGetInteger(equipment, -1) == 0);
    REQUIRE(sSlotMix[0] == 0 && sSlotMix[1] == 2);

    // Select a real child through the same persistence/runtime APIs as the menu.
    PakLoader_SaveSelection(child, 1);
    PakLoader_SelectChildModel(1);
    REQUIRE(sSelectedChildIndex == 1);
    PakLoader_SelectChildModel(-1); // Body checkbox off; saved choice must survive.
    REQUIRE(CVarGetInteger(child, -1) == 1);
    REQUIRE(strings[std::string(child) + "Path"] == "Saria.zobj");
    PakLoader_SelectChildModel(CVarGetInteger(child, -1));
    REQUIRE(sSelectedChildIndex == 1);

    sModels.insert(sModels.begin(), { "/game/mods/New.pak", true, false, false, {}, {} });
    RestoreSelections();
    REQUIRE(CVarGetInteger(adult, -1) == 1);
    REQUIRE(CVarGetInteger(child, -1) == 2);
    REQUIRE(CVarGetInteger(equipment, -1) == 1);
    REQUIRE(sSlotMix[0] == 1 && sSlotMix[1] == 3);
    const int previousSaves = saves;
    RestoreSelections();
    REQUIRE(saves == previousSaves); // Unchanged menus/startups must not keep writing config.

    PakLoader_SelectAdultModel(2);
    PakLoader_SelectChildModel(1);
    PakLoader_SelectEquipment(2);
    PakLoader_SetSlotMix(0, 3); // Shield-only donor is not valid for a sword slot.
    REQUIRE(sSelectedAdultIndex == -1 && sSelectedChildIndex == -1 && sSelectedEquipIndex == -1);
    REQUIRE(sSlotMix[0] == -1);
    PakLoader_SelectEquipment(1);
    REQUIRE(sSelectedEquipIndex == 1);
    PakLoader_SetSlotMix(0, 1);
    REQUIRE(sSlotMix[0] == 1);

    // Remove the selected Din file, leaving an unrelated adult at its old index.
    sModels.erase(sModels.begin() + 1);
    RestoreSelections();
    REQUIRE(CVarGetInteger(adult, 99) == -1);
    REQUIRE(CVarGetInteger(equipment, 99) == -1);
    REQUIRE(sSlotMix[0] == -1);
    REQUIRE(CVarGetInteger(child, -1) == 1);
    REQUIRE(sSlotMix[1] == 2);

    PakLoader_SaveSelection(shield, -1); // Reset all slots uses this API.
    RestoreSelections();
    REQUIRE(sSlotMix[1] == -1);
    REQUIRE(strings[std::string(shield) + "Path"].empty());
    REQUIRE(mounts > 0);
    puts("PASS production PAK config/runtime: migration, reorder, deletion, toggle, reset, category guards");
}
