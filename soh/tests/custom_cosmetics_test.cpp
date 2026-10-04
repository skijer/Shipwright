#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <tinyxml2.h>
#include <libultraship/libultra/gbi.h>

struct Color_RGBA8 {
    uint8_t r, g, b, a;
};
struct ImVec2 {
    float x, y;
    ImVec2(float x, float y) : x(x), y(y) {
    }
};
struct ImVec4 {
    float x = 0, y = 0, z = 0, w = 0;
    ImVec4() = default;
    ImVec4(float r, float g, float b, float a) : x(r), y(g), z(b), w(a) {
    }
};
static std::map<std::string, int> integers;
static std::map<std::string, Color_RGBA8> colors;
static int CVarGetInteger(const char* key, int fallback) {
    auto found = integers.find(key);
    return found == integers.end() ? fallback : found->second;
}
static void CVarSetInteger(const char* key, int value) {
    integers[key] = value;
}
static Color_RGBA8 CVarGetColor(const char* key, Color_RGBA8 fallback) {
    auto found = colors.find(key);
    return found == colors.end() ? fallback : found->second;
}
static void CVarSetColor(const char* key, Color_RGBA8 value) {
    colors[key] = value;
}
static void CVarClear(const char* key) {
    integers.erase(key);
    colors.erase(key);
}
namespace ShipInit {
static void Init(const char*) {
}
} // namespace ShipInit
#define CVAR_COSMETIC(name) "gCosmetics." name
#include "custom_cosmetic_declarations.inc"
std::map<std::string, CosmeticOption> cosmeticOptions;

enum {
    RANDOMIZE_OFF,
    RANDOMIZE_ON_NEW_SCENE,
    RANDOMIZE_ON_RANDO_GEN_ONLY,
    RANDOMIZE_ON_FILE_LOAD,
    RANDOMIZE_ON_FILE_LOAD_SEEDED
};
static bool isRando = false;
#define IS_RANDO isRando
struct {
    struct {
        struct {
            uint64_t fileCreatedAt = 987654;
        } stats;
    } ship;
} gSaveContext;
namespace Rando {
struct Context {
    static Context* GetInstance() {
        static Context context;
        return &context;
    }
    uint32_t GetSeed() {
        return 54321;
    }
};
} // namespace Rando
static bool default_init = true;
static uint64_t default_state = 12345;
static const uint64_t multiplier = 6364136223846793005ULL;
static const uint64_t increment = 11634580027462260723ULL;
namespace ShipUtils {
void RandInit(uint64_t seed, uint64_t* state = nullptr);
uint32_t next32(uint64_t* state = nullptr);
double RandomDouble(uint64_t* state = nullptr);
} // namespace ShipUtils
#include "custom_cosmetic_randomizer.inc"

namespace Ship {
struct Resource {
    virtual ~Resource() = default;
};
struct File {
    bool IsLoaded = true;
    std::shared_ptr<std::vector<char>> Buffer;
    explicit File(const std::string& text) : Buffer(std::make_shared<std::vector<char>>(text.begin(), text.end())) {
    }
};
struct Archive {
    std::string path = "/game/mods/cosmetics.o2r";
    std::map<std::string, std::shared_ptr<File>> files;
    const std::string& GetPath() {
        return path;
    }
    std::shared_ptr<File> LoadFile(const std::string& name) {
        auto it = files.find(name);
        return it == files.end() ? nullptr : it->second;
    }
};
struct ArchiveManager {
    std::shared_ptr<std::vector<std::shared_ptr<Archive>>> archives =
        std::make_shared<std::vector<std::shared_ptr<Archive>>>();
    auto GetArchives() {
        return archives;
    }
    std::map<std::string, std::shared_ptr<Archive>> fileOwners;
    std::shared_ptr<Archive> GetArchiveFromFile(const std::string& path) {
        // Match the real ArchiveManager's map[] insertion on a lookup miss.
        return fileOwners[path];
    }
    bool HasFile(const std::string& path) {
        return fileOwners.count(path) > 0;
    }
    std::shared_ptr<File> LoadFile(const std::string& path) {
        auto owner = GetArchiveFromFile(path);
        return owner ? owner->LoadFile(path) : nullptr;
    }
};
struct ResourceManager {
    std::shared_ptr<ArchiveManager> archives = std::make_shared<ArchiveManager>();
    std::map<std::string, std::shared_ptr<Resource>> resources;
    bool altAssets = false;
    size_t loads = 0;
    auto GetArchiveManager() {
        return archives;
    }
    bool IsAltAssetsEnabled() {
        return altAssets;
    }
    std::shared_ptr<Resource> LoadResource(const std::string& path) {
        ++loads;
        return resources[path];
    }
};
struct Gui {
    int saves = 0;
    void SaveConsoleVariablesNextFrame() {
        ++saves;
    }
};
struct Window {
    Gui gui;
    Gui* GetGui() {
        return &gui;
    }
};
struct Context {
    std::shared_ptr<ResourceManager> resources = std::make_shared<ResourceManager>();
    Window window;
    static Context* GetRawInstance() {
        static Context context;
        return &context;
    }
    auto GetResourceManager() {
        return resources;
    }
    Window* GetWindow() {
        return &window;
    }
};
} // namespace Ship
namespace Fast {
struct DisplayList : Ship::Resource {
    std::vector<Gfx> Instructions;
};
} // namespace Fast
static std::vector<std::string> drawnForms;
namespace ImGui {
static bool CollapsingHeader(const char* label) {
    drawnForms.emplace_back(label);
    return true;
}
static void PushID(const char*) {
}
static void PopID() {
}
static void SameLine(float = 0) {
}
static ImVec2 CalcTextSize(const char*) {
    return { 100, 20 };
}
static void Text(const char*, const char*) {
}
} // namespace ImGui
#define THEME_COLOR 0
namespace UIWidgets {
struct ButtonOptions {
    ButtonOptions& Size(ImVec2) {
        return *this;
    }
    ButtonOptions& Padding(ImVec2) {
        return *this;
    }
    ButtonOptions& Color(int) {
        return *this;
    }
};
struct CheckboxOptions {
    CheckboxOptions& Color(int) {
        return *this;
    }
};
static bool Button(const char*, const ButtonOptions&) {
    return false;
}
static bool CVarCheckbox(const char*, const char*, const CheckboxOptions&) {
    return false;
}
static bool CVarColorPicker(const char*, const char*, Color_RGBA8, bool, int, int) {
    return false;
}
static void Spacer() {
}
static void Separator(bool, bool, float, float) {
}
} // namespace UIWidgets
#include "custom_cosmetic_production.inc"
static void ApplyOrResetCustomGfxPatches() {
}
#include "custom_cosmetic_boundary.inc"

static int failures = 0;
static void Check(bool passed, const char* label) {
    if (!passed) {
        ++failures;
        std::cerr << "FAIL " << label << '\n';
    }
}
static uint32_t Rgba(Color_RGBA8 color, uint8_t alpha) {
    return uint32_t(color.r) << 24 | uint32_t(color.g) << 16 | uint32_t(color.b) << 8 | alpha;
}
static std::shared_ptr<Ship::Archive> ResetFixture() {
    customCosmeticEntries.clear();
    integers.clear();
    colors.clear();
    drawnForms.clear();
    auto context = Ship::Context::GetRawInstance();
    context->resources = std::make_shared<Ship::ResourceManager>();
    context->window.gui.saves = 0;
    auto archive = std::make_shared<Ship::Archive>();
    context->resources->archives->archives->push_back(archive);
    return archive;
}
static std::shared_ptr<Fast::DisplayList> Material(const std::shared_ptr<Ship::Archive>& archive,
                                                   const std::string& path, const std::string& xml,
                                                   std::vector<Gfx> commands) {
    archive->files[path] = std::make_shared<Ship::File>(xml);
    auto material = std::make_shared<Fast::DisplayList>();
    material->Instructions = std::move(commands);
    Ship::Context::GetRawInstance()->resources->resources[path] = material;
    Ship::Context::GetRawInstance()->resources->archives->fileOwners[path] = archive;
    return material;
}
static CustomCosmeticEntry* Entry(const std::string& label) {
    for (auto& entry : customCosmeticEntries)
        if (entry.option.label == label)
            return &entry;
    return nullptr;
}
static void ScannerAndSceneCases() {
    auto archive = ResetFixture();
    const std::string path = "alt/objects/object_link_zora/skin";
    archive->files["CosmeticEntries"] = std::make_shared<Ship::File>(
        "<CosmeticEntries><Entry CosmeticEntry='Zora Skin' CosmeticCategory='Body' CosmeticType='Prim' "
        "MaterialPath='objects/object_link_zora/skin'/><Entry CosmeticEntry='Zora Skin' CosmeticType='Env' "
        "MaterialPath='objects/object_link_zora/skin'/></CosmeticEntries>");
    auto material =
        Material(archive, path,
                 "<DisplayList><SetPrimColor R='10' G='20' B='30' A='40' M='7' L='9'/>"
                 "<SetPrimColor CosmeticEntry='Zora Skin' R='10' G='20' B='30' A='40' M='7' L='9'/>"
                 "<SetEnvColor R='90' G='80' B='70' A='60'/>"
                 "<SetEnvColor CosmeticEntry='Zora Skin' R='90' G='80' B='70' A='60'/></DisplayList>",
                 { gsDPSetPrimColor(7, 9, 10, 20, 30, 40), gsDPSetPrimColor(7, 9, 10, 20, 30, 40),
                   gsDPSetEnvColor(90, 80, 70, 60), gsDPSetEnvColor(90, 80, 70, 60), gsSPEndDisplayList() });
    const auto pristine = material->Instructions;
    ScanCustomCosmetics();
    Check(customCosmeticEntries.size() == 1, "manifest combines prim/env bindings using existing key");
    auto* entry = Entry("Zora Skin");
    if (!entry)
        return;
    Check(entry->baseCvar == "gCosmetics.Custom.ZoraSkin", "saved CVar identity preserved");
    Check(entry->bindings.size() == 2 && entry->bindings[0].commandIndex == 1 && entry->bindings[1].commandIndex == 3,
          "tagged commands mapped past identical untagged commands");
    Check(entry->option.cvar == entry->baseCvar.c_str() && entry->option.valuesCvar == entry->valuesCvar.c_str(),
          "option strings reference their final owning entry");
    CVarSetColor("gCosmetics.Custom.ZoraSkin.Value", { 120, 130, 140, 255 });
    CVarSetInteger("gCosmetics.Custom.ZoraSkin.Changed", 1);
    ApplyCustomCosmetics();
    Check(material->Instructions[0].words.w1 == pristine[0].words.w1 &&
              material->Instructions[2].words.w1 == pristine[2].words.w1,
          "untagged colors remain unchanged");
    Check(material->Instructions[1].words.w1 == 0x78828c28 && material->Instructions[3].words.w1 == 0x78828c3c,
          "hex color applied with each binding's original alpha");
    Check(material->Instructions[1].words.w0 == pristine[1].words.w0, "prim M/L remain unchanged");
    // Force every matching instruction away from its XML color, including the
    // untagged controls. A rescan must still find exactly the tagged locations.
    material->Instructions[0] = gsDPSetPrimColor(7, 9, 1, 2, 3, 40);
    material->Instructions[1] = gsDPSetPrimColor(7, 9, 4, 5, 6, 40);
    material->Instructions[2] = gsDPSetEnvColor(1, 2, 3, 60);
    material->Instructions[3] = gsDPSetEnvColor(4, 5, 6, 60);
    ScanCustomCosmetics();
    Check(customCosmeticEntries.size() == 1 && customCosmeticEntries[0].bindings.size() == 2,
          "rescan retains recolored tagged materials");

    CVarSetInteger(CVAR_COSMETIC("RandomizeCosmeticsGenModes"), RANDOMIZE_ON_NEW_SCENE);
    CosmeticsEditor_AutoRandomizeAll();
    const auto first = material->Instructions[1].words.w1;
    CosmeticsEditor_AutoRandomizeAll();
    Check(first != material->Instructions[1].words.w1, "scene reentry generates a fresh custom color");
    Check(material->Instructions[1].words.w1 == Rgba(colors.at("gCosmetics.Custom.ZoraSkin.Value"), 40),
          "scene color reaches loaded material");
    CVarSetInteger("gCosmetics.Custom.ZoraSkin.Locked", 1);
    const auto locked = material->Instructions[1].words.w1;
    CosmeticsEditor_AutoRandomizeAll();
    Check(locked == material->Instructions[1].words.w1, "scene randomization respects custom lock");
    CVarSetInteger("gCosmetics.Custom.ZoraSkin.Locked", 0);
    CVarSetInteger(CVAR_COSMETIC("RandomizeCosmeticsGenModes"), RANDOMIZE_ON_FILE_LOAD_SEEDED);
    CosmeticsEditor_AutoRandomizeAll();
    const auto seeded = material->Instructions[1].words.w1;
    CosmeticsEditor_AutoRandomizeAll();
    Check(seeded == material->Instructions[1].words.w1, "seeded custom mode remains repeatable");
    CosmeticsEditor_RandomizeAll();
    Check(seeded != material->Instructions[1].words.w1, "manual randomization remains fresh in seeded mode");
    auto copy = pristine;
    ApplyCustomCosmeticsToDisplayListCopy("__OTR__objects/object_link_zora/skin", copy.data(), copy.size());
    Check(copy[1].words.w1 == pristine[1].words.w1, "inactive Alt cosmetics do not tint base effect copies");
    Ship::Context::GetRawInstance()->resources->altAssets = true;
    ApplyCustomCosmeticsToDisplayListCopy("__OTR__objects/object_link_zora/skin", copy.data(), copy.size());
    Check(copy[1].words.w1 == material->Instructions[1].words.w1 &&
              copy[3].words.w1 == material->Instructions[3].words.w1,
          "cached effect copy receives current custom colors");
    Check(copy[0].words.w1 == pristine[0].words.w1 && copy[4].words.w0 == pristine[4].words.w0,
          "copy refresh preserves untagged and noncolor commands");
    copy = pristine;
    copy[1] = gsSPEndDisplayList();
    ApplyCustomCosmeticsToDisplayListCopy(path.c_str(), copy.data(), 2);
    Check(copy[1].words.w0 == (uintptr_t(G_ENDDL) << 24) && copy[3].words.w1 == pristine[3].words.w1,
          "copy refresh rejects wrong opcode and out-of-range binding");
}

static void FormGroupingCases() {
    auto archive = ResetFixture();
    std::string manifest = "<CosmeticEntries>";
    const std::vector<std::pair<std::string, std::string>> entries = {
        { "objects/object_link_boy/gLinkFierceDeityArmor", "Deity Armor" },
        { "objects/object_link_zora/body", "Zora Body" },
        { "objects/object_link_goron/body", "Goron Body" },
        { "objects/object_link_nuts/body", "Deku Body" },
        { "objects/object_link_boy/gLinkAdultTunic", "Adult Tunic" },
        { "objects/object_link_child/body", "Child Tunic" },
    };
    for (const auto& [path, label] : entries) {
        manifest += "<Entry CosmeticEntry='" + label +
                    "' CosmeticCategory='Body' CosmeticType='Prim' "
                    "MaterialPath='__OTR__alt/" +
                    path + "'/>";
        Material(archive, "alt/" + path,
                 "<DisplayList><SetPrimColor CosmeticEntry='" + label + "' R='11' G='22' B='33' A='99'/></DisplayList>",
                 { gsDPSetPrimColor(0, 0, 11, 22, 33, 99), gsSPEndDisplayList() });
    }
    manifest += "</CosmeticEntries>";
    archive->files["CosmeticEntries"] = std::make_shared<Ship::File>(manifest);
    ScanCustomCosmetics();
    Check(customCosmeticEntries.size() == 6, "OTR and alt prefixes resolve all six loaded forms");
    DrawCustomCosmetics();
    Check(drawnForms == std::vector<std::string>{ "Child", "Adult", "Deku", "Goron", "Zora", "Fierce Deity" },
          "menu groups loaded forms with Adult distinct from Fierce Deity");
    for (auto& entry : customCosmeticEntries) {
        Check(entry.option.cvar == entry.baseCvar.c_str() && entry.option.valuesCvar == entry.valuesCvar.c_str() &&
                  entry.option.rainbowCvar == entry.rainbowCvar.c_str() &&
                  entry.option.lockedCvar == entry.lockedCvar.c_str() &&
                  entry.option.changedCvar == entry.changedCvar.c_str(),
              "sorted option pointers retain their own CVar storage");
    }
    SetAllCustomCosmeticsLocked(true);
    Check(Ship::Context::GetRawInstance()->window.gui.saves > 0, "bulk lock schedules settings persistence");
    RandomizeAllCustomCosmetics(false);
    Check(colors.empty(), "lock all protects every custom entry from randomization");
    SetAllCustomCosmeticsLocked(false);
    const int savesBeforeRainbow = Ship::Context::GetRawInstance()->window.gui.saves;
    SetAllCustomCosmeticsRainbow(true);
    Check(Ship::Context::GetRawInstance()->window.gui.saves > savesBeforeRainbow,
          "bulk rainbow schedules settings persistence");
    int index = 0;
    UpdateCustomCosmeticsRainbow(3, 0.6f, index);
    ApplyCustomCosmetics();
    Check(colors.size() == 6, "rainbow all updates custom entries");
    CVarSetInteger("gCosmetics.Custom.ChildTunic.Locked", 1);
    CosmeticsEditor_ResetAll();
    Check(colors.size() == 1 && colors.count("gCosmetics.Custom.ChildTunic.Value"),
          "reset all clears unlocked colors and preserves locked entry");
    Check(CVarGetInteger("gCosmetics.Custom.AdultTunic.Rainbow", 0) == 0, "reset clears unlocked rainbow state");
    auto adult = std::dynamic_pointer_cast<Fast::DisplayList>(
        Ship::Context::GetRawInstance()->resources->resources["alt/objects/object_link_boy/gLinkAdultTunic"]);
    Check(adult->Instructions[0].words.w1 == 0x0b162163, "reset restores original material RGB and alpha");
}

static void ExpandedCommandCases() {
    // Payload high bytes can look like FA/FB. They are data, not color opcodes.
    const uint8_t expanded[] = { G_SETTIMG_OTR_HASH, G_DL_OTR_HASH, G_VTX_OTR_HASH,     G_BRANCH_Z_OTR,  G_MARKER,
                                 G_MTX_OTR,          G_MOVEMEM_OTR, G_VTX_OTR_FILEPATH, G_LOADBLOCK_WIDE };
    for (uint8_t opcode : expanded) {
        Fast::DisplayList material;
        material.Instructions = { { { uintptr_t(opcode) << 24, 0 } },
                                  gsDPSetPrimColor(0, 0, 1, 2, 3, 4),
                                  gsDPSetPrimColor(0, 0, 5, 6, 7, 8) };
        Check(FindDisplayListColorCommandIndex(material, true, 0) == 2,
              "scanner skips color-like payload in expanded commands");
    }
    for (uint8_t opcode : { G_SETTIMG_OTR_FILEPATH, G_DL_OTR_FILEPATH, G_MTX_OTR_FILEPATH }) {
        Fast::DisplayList material;
        material.Instructions = { { { uintptr_t(opcode) << 24, 0 } }, gsDPSetPrimColor(0, 0, 5, 6, 7, 8) };
        Check(FindDisplayListColorCommandIndex(material, true, 0) == 1,
              "scanner retains color immediately after single-slot filepath command");
    }
}

static void CustomDirectoryFormCases() {
    auto archive = ResetFixture();
    archive->files["CosmeticEntries"] = std::make_shared<Ship::File>(
        "<CustomCosmetics>"
        "<Entry CosmeticCategory='Goron Link' CosmeticEntry='Goron Tunic' CosmeticType='Prim' "
        "MaterialPath='objects/object_link_goy/mat_gLinkGoronSkel_GoronLinkBW_f3d_layerOpaque'/>"
        "<Entry CosmeticCategory='Zora Link' CosmeticEntry='Zora Tunic' CosmeticType='Prim' "
        "MaterialPath='objects/gameplay_keep/mat_gameplay_keep_DL_06FE20_WhiteAlphaGradient_f3d_layerOpaque'/>"
        "<Entry CosmeticCategory='Feirce Diety' CosmeticEntry='Feirce Diety Tunic' CosmeticType='Prim' "
        "MaterialPath='objects/object_link_boy/mat_gLinkFierceDeitySkel_FDLinkTunicColor_f3d_layerOpaque'/>"
        "</CustomCosmetics>");
    const std::vector<std::pair<std::string, std::string>> entries = {
        { "objects/object_link_goy/mat_gLinkGoronSkel_GoronLinkBW_f3d_layerOpaque", "Goron Tunic" },
        { "objects/gameplay_keep/mat_gameplay_keep_DL_06FE20_WhiteAlphaGradient_f3d_layerOpaque", "Zora Tunic" },
        { "objects/object_link_boy/mat_gLinkFierceDeitySkel_FDLinkTunicColor_f3d_layerOpaque", "Feirce Diety Tunic" },
    };
    for (const auto& [path, label] : entries) {
        Material(archive, "alt/" + path,
                 "<DisplayList><SetPrimColor CosmeticEntry='" + label + "' R='11' G='22' B='33' A='99'/></DisplayList>",
                 { gsDPSetPrimColor(0, 0, 11, 22, 33, 99), gsSPEndDisplayList() });
    }
    ScanCustomCosmetics();
    DrawCustomCosmetics();
    Check(drawnForms == std::vector<std::string>{ "Goron", "Zora", "Fierce Deity" },
          "real 3DS pack category/skeleton markers classify custom directories and shared effects");
    Check(Entry("Feirce Diety Tunic")->baseCvar == "gCosmetics.Custom.FeirceDietyTunic",
          "legacy manifest spelling remains the persisted CVar key");
}

static void AltOwnershipCase() {
    auto archive = ResetFixture();
    const std::string path = "objects/object_link_zora/skin";
    const std::string xml =
        "<DisplayList><SetPrimColor CosmeticEntry='Zora Skin' R='11' G='22' B='33' A='99'/></DisplayList>";
    archive->files["CosmeticEntries"] = std::make_shared<Ship::File>(
        "<Entries><Entry CosmeticEntry='Zora Skin' CosmeticType='Prim' MaterialPath='" + path + "'/></Entries>");
    auto native = std::make_shared<Ship::Archive>();
    native->path = "/game/mm.o2r";
    Ship::Context::GetRawInstance()->resources->archives->archives->insert(
        Ship::Context::GetRawInstance()->resources->archives->archives->begin(), native);
    auto base = Material(native, path, xml, { gsDPSetPrimColor(0, 0, 11, 22, 33, 99), gsSPEndDisplayList() });
    auto alt = Material(archive, "alt/" + path, xml, { gsDPSetPrimColor(0, 0, 11, 22, 33, 99), gsSPEndDisplayList() });
    Ship::Context::GetRawInstance()->resources->altAssets = true;
    ScanCustomCosmetics();
    CVarSetColor("gCosmetics.Custom.ZoraSkin.Value", { 120, 130, 140, 255 });
    CVarSetInteger("gCosmetics.Custom.ZoraSkin.Changed", 1);
    ApplyCustomCosmetics();
    Check(alt->Instructions[0].words.w1 == 0x78828c63 && base->Instructions[0].words.w1 == 0x0b162163,
          "Alt mod binding wins over a native resource at the canonical path");
}

static void BaseOnlyAltEnabledCase() {
    auto archive = ResetFixture();
    const std::string path = "objects/object_link_zora/skin";
    archive->files["CosmeticEntries"] = std::make_shared<Ship::File>(
        "<Entries><Entry CosmeticEntry='Zora Skin' CosmeticType='Prim' MaterialPath='" + path + "'/></Entries>");
    auto material =
        Material(archive, path,
                 "<DisplayList><SetPrimColor CosmeticEntry='Zora Skin' R='11' G='22' B='33' A='99'/></DisplayList>",
                 { gsDPSetPrimColor(0, 0, 11, 22, 33, 99), gsSPEndDisplayList() });
    auto resourceManager = Ship::Context::GetRawInstance()->resources;
    resourceManager->altAssets = true;
    ScanCustomCosmetics();
    Check(!resourceManager->archives->HasFile("alt/" + path), "scan does not create a phantom Alt resource");
    CVarSetColor("gCosmetics.Custom.ZoraSkin.Value", { 120, 130, 140, 255 });
    CVarSetInteger("gCosmetics.Custom.ZoraSkin.Changed", 1);
    auto copy = material->Instructions;
    ApplyCustomCosmeticsToDisplayListCopy(path.c_str(), copy.data(), copy.size());
    Check(copy[0].words.w1 == 0x78828c63, "base-only custom copy updates with Alt enabled");
    // Another resource consumer can already have inserted an empty owner.
    resourceManager->archives->GetArchiveFromFile("alt/" + path);
    copy = material->Instructions;
    ApplyCustomCosmeticsToDisplayListCopy(path.c_str(), copy.data(), copy.size());
    Check(copy[0].words.w1 == 0x78828c63, "null Alt owner cannot suppress the real base binding");
}

int main() {
    ScannerAndSceneCases();
    FormGroupingCases();
    ExpandedCommandCases();
    CustomDirectoryFormCases();
    AltOwnershipCase();
    BaseOnlyAltEnabledCase();
    if (failures)
        return 1;
    std::cout
        << "PASS custom scanner, form grouping, scene/seeded randomization, locks, rainbow, reset and copy refresh\n";
}
