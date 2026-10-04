#include "CosmeticsEditor.h"

#include <string>
#include <functional>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <tinyxml2.h>
#include <fast/resource/type/DisplayList.h>
#include <ship/resource/archive/Archive.h>

#include "soh/SohGui/UIWidgets.hpp"
#include "soh/SohGui/SohGui.hpp"
#include "soh/OTRGlobals.h"

extern "C" {
#include "soh/cvar_prefixes.h"
}

static constexpr const char* CUSTOM_COSMETIC_GROUP = "Custom";
static constexpr const char* CUSTOM_CVAR_PREFIX = "gCosmetics.Custom.";

enum class CustomCosmeticForm { Child, Adult, Deku, Goron, Zora, FierceDeity, Other };

struct CustomCosmeticBinding {
    std::string materialPath;
    size_t commandIndex = 0;
    bool isPrimColor = true;
    uint8_t defaultA = 255;
    uint8_t primM = 0;
    uint8_t primL = 0;
};

struct CustomCosmeticEntry {
    CosmeticOption option;
    std::string baseCvar;
    std::string valuesCvar;
    std::string rainbowCvar;
    std::string lockedCvar;
    std::string changedCvar;
    std::string category;
    CustomCosmeticForm form = CustomCosmeticForm::Other;
    std::vector<CustomCosmeticBinding> bindings;
};

static std::vector<CustomCosmeticEntry> customCosmeticEntries;

static bool IsCustomArchive(const std::shared_ptr<Ship::Archive>& archive) {
    if (archive == nullptr) {
        return false;
    }

    const auto& archivePath = archive->GetPath();
    return archivePath.find("\\mods\\") != std::string::npos || archivePath.find("/mods/") != std::string::npos;
}

static std::shared_ptr<Ship::Archive> GetCustomMaterialArchive(Ship::ArchiveManager* archiveManager,
                                                               const std::string& path) {
    // GetArchiveFromFile inserts a null owner on a miss; HasFile alone also
    // considers those inserted keys present. Do not create new phantom paths.
    return archiveManager->HasFile(path) ? archiveManager->GetArchiveFromFile(path) : nullptr;
}

static std::string NormalizeCustomMaterialPath(std::string path, bool keepAlt = false) {
    bool isAlt = false;
    while (path.starts_with("__OTR__") || path.starts_with("alt/")) {
        if (path.starts_with("__OTR__")) {
            path.erase(0, 7);
        } else {
            isAlt = true;
            path.erase(0, 4);
        }
    }
    return keepAlt && isAlt ? "alt/" + path : path;
}

static CustomCosmeticForm GetCustomMaterialForm(const std::string& materialPath, const std::string& category,
                                                const std::string& label) {
    const std::string path = NormalizeCustomMaterialPath(materialPath);
    if (path.starts_with("objects/object_link_child/")) {
        return CustomCosmeticForm::Child;
    }
    if (path.starts_with("objects/object_link_nuts/")) {
        return CustomCosmeticForm::Deku;
    }
    if (path.starts_with("objects/object_link_goron/")) {
        return CustomCosmeticForm::Goron;
    }
    if (path.starts_with("objects/object_link_zora/")) {
        return CustomCosmeticForm::Zora;
    }

    // OoT Adult and MM Fierce Deity share object_link_boy. Use the actual
    // material/manifest identity instead of treating every adult mod as FD.
    std::string identity;
    for (unsigned char character : category + label + path) {
        if (std::isalnum(character)) {
            identity += static_cast<char>(std::tolower(character));
        }
    }
    if (identity.find("fiercedeity") != std::string::npos || identity.find("feircediety") != std::string::npos) {
        return CustomCosmeticForm::FierceDeity;
    }
    if (path.starts_with("objects/object_link_boy/")) {
        return CustomCosmeticForm::Adult;
    }

    // Packs may put form materials in custom folders (such as object_link_goy)
    // or shared gameplay_keep effects. Keep their declared form grouping.
    if (identity.find("goronlink") != std::string::npos || identity.find("glinkgoron") != std::string::npos) {
        return CustomCosmeticForm::Goron;
    }
    if (identity.find("dekulink") != std::string::npos || identity.find("glinkdeku") != std::string::npos) {
        return CustomCosmeticForm::Deku;
    }
    if (identity.find("zoralink") != std::string::npos || identity.find("glinkzora") != std::string::npos) {
        return CustomCosmeticForm::Zora;
    }
    if (identity.find("childlink") != std::string::npos || identity.find("humanlink") != std::string::npos ||
        identity.find("glinkchild") != std::string::npos || identity.find("glinkhuman") != std::string::npos) {
        return CustomCosmeticForm::Child;
    }
    if (identity.find("adultlink") != std::string::npos || identity.find("glinkadult") != std::string::npos) {
        return CustomCosmeticForm::Adult;
    }

    return CustomCosmeticForm::Other;
}

static void SanitizeCustomKey(std::string& value) {
    for (auto it = value.begin(); it != value.end();) {
        if (!std::isalnum(static_cast<unsigned char>(*it))) {
            it = value.erase(it);
        } else {
            ++it;
        }
    }
}

static bool TryLoadCustomDisplayListXml(Ship::ArchiveManager* archiveManager, Ship::ResourceManager* resourceManager,
                                        const std::string& materialPath, tinyxml2::XMLDocument& document,
                                        std::shared_ptr<Fast::DisplayList>& material, tinyxml2::XMLElement*& root) {
    auto file = archiveManager->LoadFile(materialPath);
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        return false;
    }

    document.Parse(file->Buffer->data(), file->Buffer->size());
    if (document.Error()) {
        return false;
    }

    root = document.FirstChildElement();
    if (root == nullptr || std::string(root->Name()) != "DisplayList") {
        return false;
    }

    material = std::dynamic_pointer_cast<Fast::DisplayList>(resourceManager->LoadResource(materialPath));
    return material != nullptr;
}

static size_t FindDisplayListColorCommandIndex(const Fast::DisplayList& displayList, bool isPrimColor,
                                               size_t searchStart) {
    const uint8_t opcode = isPrimColor ? G_SETPRIMCOLOR : G_SETENVCOLOR;
    for (size_t i = searchStart; i < displayList.Instructions.size(); i++) {
        const uint8_t currentOpcode = static_cast<uint8_t>(displayList.Instructions[i].words.w0 >> 24);
        if (currentOpcode == opcode) {
            return i;
        }
        // Skip data slots used by expanded GBI commands. A hash or coordinate
        // payload can have the same high byte as a color opcode.
        switch (currentOpcode) {
            case G_SETTIMG_OTR_HASH:
            case G_DL_OTR_HASH:
            case G_VTX_OTR_HASH:
            case G_BRANCH_Z_OTR:
            case G_MARKER:
            case G_MTX_OTR:
            case G_MOVEMEM_OTR:
            case G_VTX_OTR_FILEPATH:
            case G_LOADBLOCK_WIDE:
            case G_FILLWIDERECT:
                i++;
                break;
            case G_TEXRECT_WIDE:
                i += 2;
                break;
            case G_ENDDL:
                return SIZE_MAX;
        }
    }

    return SIZE_MAX;
}

static void RefreshCustomCosmeticOption(CustomCosmeticEntry& entry) {
    entry.option.cvar = entry.baseCvar.c_str();
    entry.option.valuesCvar = entry.valuesCvar.c_str();
    entry.option.rainbowCvar = entry.rainbowCvar.c_str();
    entry.option.lockedCvar = entry.lockedCvar.c_str();
    entry.option.changedCvar = entry.changedCvar.c_str();
}

static Color_RGBA8 GetCustomCosmeticColor(const CustomCosmeticEntry& entry) {
    if (CVarGetInteger(entry.option.changedCvar, 0)) {
        return CVarGetColor(entry.option.valuesCvar, entry.option.defaultColor);
    }

    return entry.option.defaultColor;
}

void ApplyCustomCosmetics() {
    auto resourceManager = Ship::Context::GetRawInstance()->GetResourceManager();
    auto archiveManager = resourceManager->GetArchiveManager();

    for (auto& entry : customCosmeticEntries) {
        Color_RGBA8 color = GetCustomCosmeticColor(entry);

        for (const auto& binding : entry.bindings) {
            if (!IsCustomArchive(GetCustomMaterialArchive(archiveManager.get(), binding.materialPath))) {
                continue;
            }

            auto material =
                std::dynamic_pointer_cast<Fast::DisplayList>(resourceManager->LoadResource(binding.materialPath));
            if (material == nullptr || binding.commandIndex >= material->Instructions.size()) {
                continue;
            }

            if (binding.isPrimColor) {
                material->Instructions[binding.commandIndex] =
                    gsDPSetPrimColor(binding.primM, binding.primL, color.r, color.g, color.b, binding.defaultA);
            } else {
                material->Instructions[binding.commandIndex] =
                    gsDPSetEnvColor(color.r, color.g, color.b, binding.defaultA);
            }
        }
    }
}

void ApplyCustomCosmeticsToDisplayListCopy(const char* materialPath, Gfx* instructions, size_t count) {
    if (materialPath == nullptr || instructions == nullptr) {
        return;
    }
    auto resourceManager = Ship::Context::GetRawInstance()->GetResourceManager();
    auto archiveManager = resourceManager->GetArchiveManager();
    const std::string path = NormalizeCustomMaterialPath(materialPath);
    const bool useAlt = NormalizeCustomMaterialPath(materialPath, true).starts_with("alt/") ||
                        (resourceManager->IsAltAssetsEnabled() &&
                         GetCustomMaterialArchive(archiveManager.get(), "alt/" + path) != nullptr);

    for (const auto& entry : customCosmeticEntries) {
        const Color_RGBA8 color = GetCustomCosmeticColor(entry);
        for (const auto& binding : entry.bindings) {
            if (NormalizeCustomMaterialPath(binding.materialPath) != path ||
                binding.materialPath.starts_with("alt/") != useAlt ||
                !IsCustomArchive(GetCustomMaterialArchive(archiveManager.get(), binding.materialPath)) ||
                binding.commandIndex >= count) {
                continue;
            }
            Gfx& command = instructions[binding.commandIndex];
            const uint8_t opcode = binding.isPrimColor ? G_SETPRIMCOLOR : G_SETENVCOLOR;
            if (static_cast<uint8_t>(command.words.w0 >> 24) != opcode) {
                continue;
            }
            if (binding.isPrimColor) {
                command = gsDPSetPrimColor(binding.primM, binding.primL, color.r, color.g, color.b, binding.defaultA);
            } else {
                command = gsDPSetEnvColor(color.r, color.g, color.b, binding.defaultA);
            }
        }
    }
}

static void ResetCustomCosmeticColor(CustomCosmeticEntry& entry) {
    ResetColor(entry.option);
    ShipInit::Init(entry.option.rainbowCvar);
    ShipInit::Init(entry.option.lockedCvar);
    ShipInit::Init(entry.option.changedCvar);
}

static void RandomizeCustomCosmeticColor(CustomCosmeticEntry& entry, bool manual) {
    RandomizeColor(entry.option, manual);
    ShipInit::Init(entry.option.valuesCvar);
    ShipInit::Init(entry.option.rainbowCvar);
    ShipInit::Init(entry.option.changedCvar);
}

static void DrawCustomCosmeticColorRow(const char* label, const char* cvar, Color_RGBA8 defaultColor,
                                       const char* rainbowCvar, const char* lockedCvar, const char* changedCvar,
                                       const std::function<void()>& onColorChanged,
                                       const std::function<void()>& onRandomize,
                                       const std::function<void()>& onRainbowToggle,
                                       const std::function<void()>& onReset) {
    if (UIWidgets::CVarColorPicker(label, cvar, defaultColor, false, 0, THEME_COLOR)) {
        onColorChanged();
    }

    ImGui::SameLine((ImGui::CalcTextSize("Message Light Blue (None No Shadow)").x * 1.0f) + 60.0f);
    if (UIWidgets::Button(
            ("Random##" + std::string(label)).c_str(),
            UIWidgets::ButtonOptions().Size(ImVec2(80, 31)).Padding(ImVec2(2.0f, 0.0f)).Color(THEME_COLOR))) {
        onRandomize();
    }

    ImGui::SameLine();
    if (UIWidgets::CVarCheckbox(("Rainbow##" + std::string(label)).c_str(), rainbowCvar,
                                UIWidgets::CheckboxOptions().Color(THEME_COLOR))) {
        onRainbowToggle();
    }

    ImGui::SameLine();
    UIWidgets::CVarCheckbox(("Locked##" + std::string(label)).c_str(), lockedCvar,
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR));

    if (CVarGetInteger(changedCvar, 0)) {
        ImGui::SameLine();
        if (UIWidgets::Button(("Reset##" + std::string(label)).c_str(),
                              UIWidgets::ButtonOptions().Size(ImVec2(80, 31)).Padding(ImVec2(2.0f, 0.0f)))) {
            onReset();
        }
    }
}

void ScanCustomCosmetics() {
    customCosmeticEntries.clear();

    auto resourceManager = Ship::Context::GetRawInstance()->GetResourceManager();
    auto archiveManager = resourceManager->GetArchiveManager();
    auto archives = archiveManager->GetArchives();
    std::unordered_map<std::string, size_t> entryIndicesByKey;

    for (const auto& archive : *archives) {
        if (!IsCustomArchive(archive)) {
            continue;
        }

        auto manifestFile = archive->LoadFile("CosmeticEntries");
        if (manifestFile == nullptr || !manifestFile->IsLoaded || manifestFile->Buffer == nullptr) {
            continue;
        }

        tinyxml2::XMLDocument manifestDocument;
        manifestDocument.Parse(manifestFile->Buffer->data(), manifestFile->Buffer->size());
        if (manifestDocument.Error()) {
            continue;
        }

        tinyxml2::XMLElement* manifestRoot = manifestDocument.FirstChildElement();
        if (manifestRoot == nullptr) {
            continue;
        }

        for (auto* manifestEntry = manifestRoot->FirstChildElement(); manifestEntry != nullptr;
             manifestEntry = manifestEntry->NextSiblingElement()) {
            const char* cosmeticEntry = manifestEntry->Attribute("CosmeticEntry");
            const char* materialPath = manifestEntry->Attribute("MaterialPath");

            std::string resolvedMaterialPath;
            if (materialPath != nullptr && materialPath[0] != '\0') {
                resolvedMaterialPath = NormalizeCustomMaterialPath(materialPath, true);
                if (!resolvedMaterialPath.starts_with("alt/") &&
                    IsCustomArchive(GetCustomMaterialArchive(archiveManager.get(), "alt/" + resolvedMaterialPath)) &&
                    (resourceManager->IsAltAssetsEnabled() ||
                     !IsCustomArchive(GetCustomMaterialArchive(archiveManager.get(), resolvedMaterialPath)))) {
                    resolvedMaterialPath = "alt/" + resolvedMaterialPath;
                }
                if (GetCustomMaterialArchive(archiveManager.get(), resolvedMaterialPath) == nullptr) {
                    if (!resolvedMaterialPath.starts_with("alt/") &&
                        GetCustomMaterialArchive(archiveManager.get(), "alt/" + resolvedMaterialPath) != nullptr) {
                        resolvedMaterialPath = "alt/" + resolvedMaterialPath;
                    } else {
                        resolvedMaterialPath.clear();
                    }
                }
            }

            const char* cosmeticType = manifestEntry->Attribute("CosmeticType");
            const bool isPrimColor = cosmeticType != nullptr && std::string(cosmeticType) == "Prim";
            const bool isEnvColor = cosmeticType != nullptr && std::string(cosmeticType) == "Env";

            if (cosmeticEntry == nullptr || cosmeticEntry[0] == '\0' || resolvedMaterialPath.empty() ||
                (!isPrimColor && !isEnvColor)) {
                continue;
            }

            std::string key = cosmeticEntry;
            SanitizeCustomKey(key);
            if (key.empty()) {
                continue;
            }

            tinyxml2::XMLDocument displayListDocument;
            std::shared_ptr<Fast::DisplayList> material;
            tinyxml2::XMLElement* displayListRoot = nullptr;
            if (!TryLoadCustomDisplayListXml(archiveManager.get(), resourceManager.get(), resolvedMaterialPath,
                                             displayListDocument, material, displayListRoot)) {
                continue;
            }

            size_t primSearchStart = 0;
            size_t envSearchStart = 0;
            for (auto* child = displayListRoot->FirstChildElement(); child != nullptr;
                 child = child->NextSiblingElement()) {
                const std::string childName = child->Name();
                const bool childIsPrimColor = childName == "SetPrimColor";
                if (!childIsPrimColor && childName != "SetEnvColor") {
                    continue;
                }

                // Advance for every color command, including untagged ones.
                // RGB words may already contain a saved, random or rainbow color.
                size_t& searchStart = childIsPrimColor ? primSearchStart : envSearchStart;
                const size_t commandIndex = FindDisplayListColorCommandIndex(*material, childIsPrimColor, searchStart);
                if (commandIndex == SIZE_MAX) {
                    continue;
                }
                searchStart = commandIndex + 1;

                if (childIsPrimColor != isPrimColor) {
                    continue;
                }
                const char* childCosmeticEntry = child->Attribute("CosmeticEntry");
                if (childCosmeticEntry == nullptr || std::string(childCosmeticEntry) != cosmeticEntry) {
                    continue;
                }

                size_t entryIndex = 0;
                if (auto it = entryIndicesByKey.find(key); it != entryIndicesByKey.end()) {
                    entryIndex = it->second;
                } else {
                    entryIndex = customCosmeticEntries.size();
                    entryIndicesByKey[key] = entryIndex;

                    const char* cosmeticCategory = manifestEntry->Attribute("CosmeticCategory");
                    if (cosmeticCategory == nullptr) {
                        cosmeticCategory = child->Attribute("CosmeticCategory");
                    }

                    CustomCosmeticEntry entry;
                    entry.category = (cosmeticCategory != nullptr) ? cosmeticCategory : "";
                    entry.baseCvar = std::string(CUSTOM_CVAR_PREFIX) + key;
                    entry.valuesCvar = entry.baseCvar + ".Value";
                    entry.rainbowCvar = entry.baseCvar + ".Rainbow";
                    entry.lockedCvar = entry.baseCvar + ".Locked";
                    entry.changedCvar = entry.baseCvar + ".Changed";
                    const Color_RGBA8 defaultColor = { static_cast<uint8_t>(child->IntAttribute("R")),
                                                       static_cast<uint8_t>(child->IntAttribute("G")),
                                                       static_cast<uint8_t>(child->IntAttribute("B")),
                                                       static_cast<uint8_t>(child->IntAttribute("A")) };
                    entry.option =
                        MakeCosmeticOption(entry.baseCvar.c_str(), entry.valuesCvar.c_str(), entry.rainbowCvar.c_str(),
                                           entry.lockedCvar.c_str(), entry.changedCvar.c_str(), cosmeticEntry,
                                           COSMETICS_GROUP_MAX, defaultColor, false, true, false);
                    customCosmeticEntries.push_back(std::move(entry));
                }

                CustomCosmeticBinding binding;
                binding.materialPath = resolvedMaterialPath;
                binding.commandIndex = commandIndex;
                binding.isPrimColor = isPrimColor;
                binding.defaultA = static_cast<uint8_t>(child->IntAttribute("A"));
                binding.primM = static_cast<uint8_t>(child->IntAttribute("M"));
                binding.primL = static_cast<uint8_t>(child->IntAttribute("L"));
                auto& entry = customCosmeticEntries[entryIndex];
                entry.form = std::min(entry.form,
                                      GetCustomMaterialForm(resolvedMaterialPath, entry.category, entry.option.label));
                entry.bindings.push_back(std::move(binding));
            }
        }
    }

    std::stable_sort(customCosmeticEntries.begin(), customCosmeticEntries.end(),
                     [](const CustomCosmeticEntry& lhs, const CustomCosmeticEntry& rhs) {
                         if (lhs.form != rhs.form) {
                             return lhs.form < rhs.form;
                         }

                         if (lhs.category.empty() != rhs.category.empty()) {
                             return !lhs.category.empty();
                         }

                         if (lhs.category != rhs.category) {
                             return lhs.category < rhs.category;
                         }

                         return lhs.option.label < rhs.option.label;
                     });

    for (auto& entry : customCosmeticEntries) {
        RefreshCustomCosmeticOption(entry);
    }

    ApplyCustomCosmetics();
}

static void DrawCustomCosmeticRow(CustomCosmeticEntry& entry) {
    const char* cvar = entry.option.cvar;

    DrawCustomCosmeticColorRow(
        entry.option.label.c_str(), cvar, entry.option.defaultColor, entry.option.rainbowCvar, entry.option.lockedCvar,
        entry.option.changedCvar,
        [&entry]() {
            CVarSetInteger(entry.option.rainbowCvar, 0);
            CVarSetInteger(entry.option.changedCvar, 1);
            ShipInit::Init(entry.option.rainbowCvar);
            ShipInit::Init(entry.option.changedCvar);
            ApplyCustomCosmetics();
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        },
        [&entry]() {
            RandomizeCustomCosmeticColor(entry, true);
            ApplyCustomCosmetics();
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        },
        [&entry]() {
            CVarSetInteger(entry.option.changedCvar, 1);
            ShipInit::Init(entry.option.changedCvar);
            ApplyCustomCosmetics();
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        },
        [&entry]() {
            ResetCustomCosmeticColor(entry);
            ApplyCustomCosmetics();
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        });
}

static void DrawCustomCosmeticCategory(const char* label, const std::vector<CustomCosmeticEntry*>& entries) {
    ImGui::Text("%s", label);
    ImGui::SameLine((ImGui::CalcTextSize("Message Light Blue (None No Shadow)").x * 1.0f) + 60.0f);
    if (UIWidgets::Button(
            ("Random##" + std::string(label)).c_str(),
            UIWidgets::ButtonOptions().Size(ImVec2(80, 31)).Padding(ImVec2(2.0f, 0.0f)).Color(THEME_COLOR))) {
        for (auto* entry : entries) {
            if (!CVarGetInteger(entry->option.lockedCvar, 0)) {
                RandomizeCustomCosmeticColor(*entry, true);
            }
        }
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        ApplyCustomCosmetics();
    }
    ImGui::SameLine();
    if (UIWidgets::Button(("Reset##" + std::string(label)).c_str(),
                          UIWidgets::ButtonOptions().Size(ImVec2(80, 31)).Padding(ImVec2(2.0f, 0.0f)))) {
        for (auto* entry : entries) {
            if (!CVarGetInteger(entry->option.lockedCvar, 0)) {
                ResetCustomCosmeticColor(*entry);
            }
        }
        ApplyCustomCosmetics();
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    UIWidgets::Spacer();
    for (auto* entry : entries) {
        DrawCustomCosmeticRow(*entry);
    }
    UIWidgets::Separator(true, true, 2.0f, 2.0f);
}

bool HasCustomCosmetics() {
    return !customCosmeticEntries.empty();
}

void DrawCustomCosmetics() {
    static constexpr const char* formLabels[] = { "Child", "Adult", "Deku", "Goron", "Zora", "Fierce Deity", "Other" };
    for (int formIndex = 0; formIndex <= static_cast<int>(CustomCosmeticForm::Other); ++formIndex) {
        const auto form = static_cast<CustomCosmeticForm>(formIndex);
        if (std::none_of(customCosmeticEntries.begin(), customCosmeticEntries.end(),
                         [form](const CustomCosmeticEntry& entry) { return entry.form == form; }) ||
            !ImGui::CollapsingHeader(formLabels[formIndex])) {
            continue;
        }
        ImGui::PushID(formLabels[formIndex]);
        std::vector<CustomCosmeticEntry*> currentEntries;
        std::string currentCategory;
        auto flushCategory = [&]() {
            if (!currentEntries.empty()) {
                const char* label = currentCategory.empty() ? CUSTOM_COSMETIC_GROUP : currentCategory.c_str();
                DrawCustomCosmeticCategory(label, currentEntries);
                currentEntries.clear();
            }
        };
        for (auto& entry : customCosmeticEntries) {
            if (entry.form != form) {
                continue;
            }
            if (entry.category != currentCategory) {
                flushCategory();
                currentCategory = entry.category;
            }
            currentEntries.push_back(&entry);
        }
        flushCategory();
        ImGui::PopID();
    }
}

void RandomizeAllCustomCosmetics(bool manual) {
    for (auto& entry : customCosmeticEntries) {
        if (!CVarGetInteger(entry.option.lockedCvar, 0)) {
            RandomizeCustomCosmeticColor(entry, manual);
        }
    }
    ApplyCustomCosmetics();
}

void ResetAllCustomCosmetics() {
    for (auto& entry : customCosmeticEntries) {
        if (!CVarGetInteger(entry.option.lockedCvar, 0)) {
            ResetCustomCosmeticColor(entry);
        }
    }
    ApplyCustomCosmetics();
}

void SetAllCustomCosmeticsLocked(bool locked) {
    for (const auto& entry : customCosmeticEntries) {
        CVarSetInteger(entry.option.lockedCvar, locked ? 1 : 0);
        ShipInit::Init(entry.option.lockedCvar);
    }
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void SetAllCustomCosmeticsRainbow(bool enabled) {
    for (const auto& entry : customCosmeticEntries) {
        if (!CVarGetInteger(entry.option.lockedCvar, 0)) {
            CVarSetInteger(entry.option.rainbowCvar, enabled ? 1 : 0);
            if (enabled) {
                CVarSetInteger(entry.option.changedCvar, 1);
            }
            ShipInit::Init(entry.option.rainbowCvar);
            ShipInit::Init(entry.option.changedCvar);
        }
    }
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void UpdateCustomCosmeticsRainbow(int hue, float rainbowSpeed, int& index) {
    for (const auto& entry : customCosmeticEntries) {
        if (CVarGetInteger(entry.option.rainbowCvar, 0)) {
            double frequency = 2 * M_PI / (360 * rainbowSpeed);
            Color_RGBA8 newColor;
            newColor.r = static_cast<uint8_t>(sin(frequency * (hue + index) + 0) * 127) + 128;
            newColor.g = static_cast<uint8_t>(sin(frequency * (hue + index) + (2 * M_PI / 3)) * 127) + 128;
            newColor.b = static_cast<uint8_t>(sin(frequency * (hue + index) + (4 * M_PI / 3)) * 127) + 128;
            newColor.a = 255;
            CVarSetColor(entry.option.valuesCvar, newColor);
        }
        if (!CVarGetInteger(CVAR_COSMETIC("RainbowSync"), 0)) {
            index += static_cast<int>(60 * rainbowSpeed);
        }
    }
}
