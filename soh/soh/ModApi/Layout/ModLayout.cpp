#include "ModLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <libultraship/libultraship.h>
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/enhancementTypes.h"

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "src/overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h"
#include "textures/parameter_static/parameter_static.h"
bool CanMaskSelect(void);
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

namespace {

constexpr int MaxCells = 48;
constexpr const char* NativePage = "vanilla";
constexpr const char* WindowCvar = CVAR_WINDOW("ModsItemsLayout");
constexpr std::array<int, SOH_LAYOUT_COUNT> PausePages = { PAUSE_ITEM, PAUSE_EQUIP, PAUSE_QUEST };
constexpr std::array<const char*, SOH_LAYOUT_COUNT> KindNames = { "Items", "Equipment", "Collectables" };

struct Page {
    std::string key;
    std::string title;
    uint8_t kind;
    int rows;
    int columns;
    std::vector<SOHLayoutCell> cells;
};

struct Placement {
    std::string page;
    int cell;
};

struct Entry {
    std::string key;
    std::string label;
    const char* icon = nullptr;
    const char* name = nullptr;
    uint8_t kind = SOH_LAYOUT_ITEMS;
    uint8_t sourceKind = SOH_LAYOUT_ITEMS;
    int source = -1;
    uint16_t item = ITEM_NONE;
    const SOHCustomItemDefinition* custom = nullptr;
    const SOHCustomEquipDefinition* equipment = nullptr;
    Placement placement;
    bool owned = false;
};

std::map<std::string, Page> sTemplates;
std::map<std::string, Placement> sDefaults;
struct VanillaReference {
    std::string key;
    std::string page;
    uint8_t kind;
    uint8_t source;
    uint8_t cell;
};
std::vector<VanillaReference> sVanillaReferences;
std::array<std::string, SOH_LAYOUT_COUNT> sShown = { NativePage, NativePage, NativePage };
std::array<std::string, SOH_LAYOUT_COUNT> sEdited = { NativePage, NativePage, NativePage };
std::array<int, SOH_LAYOUT_COUNT> sCursor{};
std::map<std::string, std::string> sSelected;
std::array<bool, SOH_LAYOUT_COUNT> sCycling{};
std::array<uint32_t, SOH_LAYOUT_COUNT> sInputFrame{};
std::array<int, SOH_LAYOUT_COUNT> sRepeat{};
std::array<int, SOH_LAYOUT_COUNT> sLastDirection{};
std::array<bool, SOH_LAYOUT_COUNT> sActive{};
bool sPreviewingSong = false;
std::string sAnimatingItem;

std::string Setting(const std::string& key, const char* field) {
    std::string result = CVAR_ENHANCEMENT("ModLayout.");
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char byte : key) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result + "." + field;
}

void SaveLayout() {
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

std::string NativeKey(uint8_t kind, int source) {
    return "vanilla." + std::to_string(kind) + "." + std::to_string(source);
}

Placement GetPlacement(const std::string& key, Placement fallback) {
    auto registered = sDefaults.find(key);
    if (registered != sDefaults.end()) {
        fallback = registered->second;
    }
    return { CVarGetString(Setting(key, "Page").c_str(), fallback.page.c_str()),
             std::clamp(CVarGetInteger(Setting(key, "Cell").c_str(), fallback.cell), 0, MaxCells - 1) };
}

void SetPlacement(const Entry& entry, const Placement& placement) {
    CVarSetString(Setting(entry.key, "Page").c_str(), placement.page.c_str());
    CVarSetInteger(Setting(entry.key, "Cell").c_str(), placement.cell);
    CVarSetInteger(Setting(std::to_string(entry.kind), "Edited").c_str(), 1);
    SaveLayout();
}

void ApplyVanillaItemGridSpacing(Page& page) {
    for (int cell = 0; cell < 24; ++cell) {
        page.cells[cell] = { static_cast<int16_t>(-94 + cell % 6 * 32), static_cast<int16_t>(56 - cell / 6 * 32), 28,
                             28 };
    }
}

void ApplyVanillaEquipmentGridSpacing(Page& page) {
    static const int16_t sEquipColumnX[] = { -114, 12, 44, 76 };
    for (int cell = 0; cell < 16; ++cell) {
        page.cells[cell] = { static_cast<int16_t>(sEquipColumnX[cell % 4] + 2),
                             static_cast<int16_t>(56 - cell / 4 * 32), 28, 28 };
    }
}

Page MakeGrid(uint8_t kind, const std::string& key, const std::string& title, int rows, int columns) {
    Page page{ key, title, kind, rows, columns, {} };
    const int stepX = 216 / columns;
    const int stepY = 128 / rows;
    const int size = std::min({ 32, stepX - 3, stepY - 3 });
    for (int cell = 0; cell < rows * columns; ++cell) {
        page.cells.push_back({ static_cast<int16_t>(-108 + cell % columns * stepX),
                               static_cast<int16_t>(58 - cell / columns * stepY), static_cast<uint16_t>(size),
                               static_cast<uint16_t>(size) });
    }
    if (kind == SOH_LAYOUT_ITEMS && rows == 4 && columns == 6) {
        ApplyVanillaItemGridSpacing(page);
    }
    if (kind == SOH_LAYOUT_EQUIPMENT && rows == 4 && columns == 4) {
        ApplyVanillaEquipmentGridSpacing(page);
    }
    return page;
}

Page NativeLayout(uint8_t kind) {
    Page page = MakeGrid(kind, NativePage, "Vanilla", 4, kind == SOH_LAYOUT_EQUIPMENT ? 4 : 6);
    if (kind == SOH_LAYOUT_COLLECTABLES) {
        page.rows = 5;
        page.columns = 6;
        page.cells.resize(25);
        const int x[] = { 74,  74,  46,  18,  18,  46, -108, -90, -72,  -54, -36,  -18, -108,
                          -90, -72, -54, -36, -18, 20, 46,   72,  -110, -86, -110, -54 };
        const int y[] = { 38, 6, -12, 6, 38, 56,  -20, -20, -20, -20, -20, -20, 2,
                          2,  2, 2,   2, 2,  -46, -46, -46, 58,  58,  34,  58 };
        for (int i = 0; i < 25; ++i) {
            page.cells[i] = { static_cast<int16_t>(x[i]), static_cast<int16_t>(y[i]), 24, 24 };
            if (i >= 6 && i < 18) {
                page.cells[i].width = 16;
            }
        }
        page.cells[24].width = page.cells[24].height = 48;
    }
    return page;
}

std::string ItemPage(uint16_t page) {
    if (page == SOH_ITEM_PAGE_VANILLA || page == SOH_ITEM_PAGE_QUEST_VANILLA) {
        return NativePage;
    }
    return "page." +
           std::to_string(page >= SOH_ITEM_PAGE_QUEST_VANILLA ? page - SOH_ITEM_PAGE_QUEST_VANILLA - 1 : page);
}

std::vector<Entry> NativeEntries(uint8_t kind) {
    std::vector<Entry> entries;
    const char* inventoryNames[] = { "Deku Sticks", "Deku Nuts",     "Bombs",        "Bow",           "Fire Arrows",
                                     "Din's Fire",  "Slingshot",     "Ocarina",      "Bombchus",      "Hookshot",
                                     "Ice Arrows",  "Farore's Wind", "Boomerang",    "Lens of Truth", "Magic Beans",
                                     "Hammer",      "Light Arrows",  "Nayru's Love", "Bottle 1",      "Bottle 2",
                                     "Bottle 3",    "Bottle 4",      "Adult Trade",  "Child Trade" };
    const int inventoryIcons[] = { ITEM_STICK,     ITEM_NUT,          ITEM_BOMB,         ITEM_BOW,      ITEM_ARROW_FIRE,
                                   ITEM_DINS_FIRE, ITEM_SLINGSHOT,    ITEM_OCARINA_TIME, ITEM_BOMBCHU,  ITEM_HOOKSHOT,
                                   ITEM_ARROW_ICE, ITEM_FARORES_WIND, ITEM_BOOMERANG,    ITEM_LENS,     ITEM_BEAN,
                                   ITEM_HAMMER,    ITEM_ARROW_LIGHT,  ITEM_NAYRUS_LOVE,  ITEM_BOTTLE,   ITEM_BOTTLE,
                                   ITEM_BOTTLE,    ITEM_BOTTLE,       ITEM_POCKET_EGG,   ITEM_WEIRD_EGG };
    const char* equipNames[] = {
        "Quiver / Bullet Bag", "Kokiri Sword",  "Master Sword",  "Biggoron Sword", "Bomb Bag",
        "Deku Shield",         "Hylian Shield", "Mirror Shield", "Strength",       "Kokiri Tunic",
        "Goron Tunic",         "Zora Tunic",    "Scale",         "Kokiri Boots",   "Iron Boots",
        "Hover Boots"
    };
    const char* questNames[] = { "Forest Medallion",  "Fire Medallion",    "Water Medallion",    "Spirit Medallion",
                                 "Shadow Medallion",  "Light Medallion",   "Minuet of Forest",   "Bolero of Fire",
                                 "Serenade of Water", "Requiem of Spirit", "Nocturne of Shadow", "Prelude of Light",
                                 "Zelda's Lullaby",   "Epona's Song",      "Saria's Song",       "Sun's Song",
                                 "Song of Time",      "Song of Storms",    "Kokiri Emerald",     "Goron Ruby",
                                 "Zora Sapphire",     "Stone of Agony",    "Gerudo Card",        "Gold Skulltulas",
                                 "Heart Pieces" };
    const int count = kind == SOH_LAYOUT_ITEMS ? 24 : kind == SOH_LAYOUT_EQUIPMENT ? 16 : 25;
    for (int i = 0; i < count; ++i) {
        if (kind == SOH_LAYOUT_ITEMS && (i == SLOT_TRADE_ADULT || i == SLOT_TRADE_CHILD)) {
            continue;
        }
        Entry entry;
        entry.kind = kind;
        entry.sourceKind = kind;
        entry.source = i;
        entry.key = NativeKey(kind, i);
        if (kind == SOH_LAYOUT_ITEMS) {
            entry.label = inventoryNames[i];
            entry.item = gSaveContext.inventory.items[i];
            entry.owned = entry.item != ITEM_NONE;
            const int icon = entry.owned ? entry.item : inventoryIcons[i];
            entry.icon = static_cast<const char*>(gItemIcons[icon]);
        } else if (kind == SOH_LAYOUT_EQUIPMENT) {
            entry.label = equipNames[i];
            const int row = i / 4;
            const int column = i % 4;
            if (column != 0) {
                entry.item = ITEM_SWORD_KOKIRI + row * 3 + column - 1;
                entry.owned = (gSaveContext.inventory.equipment & (1 << (row * 4 + column - 1))) != 0;
            } else {
                const int upgrades[] = { LINK_IS_ADULT ? UPG_QUIVER : UPG_BULLET_BAG, UPG_BOMB_BAG, UPG_STRENGTH,
                                         UPG_SCALE };
                const int bases[] = { LINK_IS_ADULT ? ITEM_QUIVER_30 : ITEM_BULLET_BAG_30, ITEM_BOMB_BAG_20,
                                      ITEM_BRACELET, ITEM_SCALE_SILVER };
                const int level = CUR_UPG_VALUE(upgrades[row]);
                entry.item = bases[row] + std::max(0, level - 1);
                entry.owned = level > 0;
            }
            entry.icon = static_cast<const char*>(gItemIcons[entry.item]);
        } else {
            entry.label = questNames[i];
            entry.item = i < 6    ? ITEM_MEDALLION_FOREST + i
                         : i < 18 ? ITEM_SONG_MINUET + i - 6
                         : i < 24 ? ITEM_KOKIRI_EMERALD + i - 18
                                  : ITEM_HEART_PIECE;
            entry.owned = i == 24 ? (gSaveContext.inventory.questItems >> 28) != 0 : CHECK_QUEST_ITEM(i);
            entry.icon = static_cast<const char*>(gItemIcons[entry.item]);
        }
        entry.placement = GetPlacement(entry.key, { NativePage, i });
        entries.push_back(std::move(entry));
    }
    if (kind == SOH_LAYOUT_ITEMS) {
        const char* childNames[] = { "Weird Egg",  "Chicken",     "Zelda's Letter", "Keaton Mask",
                                     "Skull Mask", "Spooky Mask", "Bunny Hood",     "Goron Mask",
                                     "Zora Mask",  "Gerudo Mask", "Mask of Truth" };
        const char* adultNames[] = {
            "Pocket Egg",           "Pocket Cucco", "Cojiro",       "Odd Mushroom", "Odd Potion", "Poacher's Saw",
            "Broken Goron's Sword", "Prescription", "Eyeball Frog", "Eye Drops",    "Claim Check"
        };
        for (int item = ITEM_WEIRD_EGG; item <= ITEM_CLAIM_CHECK; ++item) {
            if (item == ITEM_SOLD_OUT || VanillaItems_IsVanillaSuppressed(item))
                continue;
            const bool child = item <= ITEM_MASK_TRUTH;
            const int first = child ? ITEM_WEIRD_EGG : ITEM_POCKET_EGG;
            const int slot = child ? SLOT_TRADE_CHILD : SLOT_TRADE_ADULT;
            Entry entry;
            entry.key = "vanilla.trade." + std::to_string(item);
            entry.label = child ? childNames[item - first] : adultNames[item - first];
            entry.source = slot;
            entry.item = item;
            entry.icon = static_cast<const char*>(gItemIcons[item]);
            entry.owned = gSaveContext.inventory.items[slot] == item;
            if (IS_RANDO) {
                entry.owned = entry.owned ||
                              Flags_GetRandomizerInf(static_cast<RandomizerInf>(
                                  (child ? RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG : RAND_INF_ADULT_TRADES_HAS_POCKET_EGG) +
                                  item - first)) != 0;
            } else if (item >= ITEM_MASK_KEATON && item <= ITEM_MASK_TRUTH && CanMaskSelect()) {
                entry.owned = true;
            }
            entry.placement = GetPlacement(entry.key, { NativePage, slot });
            entries.push_back(std::move(entry));
        }
    }
    return entries;
}

std::vector<Entry> Entries(uint8_t kind) {
    auto entries = NativeEntries(kind);
    for (const auto& reference : sVanillaReferences) {
        auto page = sTemplates.find(reference.page);
        if (page == sTemplates.end() || page->second.kind != kind) {
            continue;
        }
        auto sources = NativeEntries(reference.kind);
        auto source = std::find_if(sources.begin(), sources.end(),
                                   [&](const Entry& entry) { return entry.source == reference.source && entry.owned; });
        if (source == sources.end()) {
            source = std::find_if(sources.begin(), sources.end(),
                                  [&](const Entry& entry) { return entry.source == reference.source; });
        }
        if (source == sources.end())
            continue;
        Entry entry = *source;
        entry.key = reference.key;
        entry.kind = kind;
        entry.placement = GetPlacement(entry.key, { reference.page, reference.cell });
        entries.push_back(std::move(entry));
    }
    if (kind == SOH_LAYOUT_EQUIPMENT) {
        for (uint32_t i = 0; i < CustomEquipRegistry_GetCount(); ++i) {
            const auto* definition = CustomEquipRegistry_GetAt(i);
            if (definition == nullptr) {
                continue;
            }
            Entry entry;
            entry.key = definition->key;
            entry.label = definition->key;
            entry.icon = definition->iconPath;
            entry.name = definition->namePath;
            entry.kind = kind;
            entry.sourceKind = kind;
            entry.equipment = definition;
            entry.owned = CustomEquipRegistry_IsOwned(definition->key);
            const int row = definition->slot == SOH_EQUIP_SLOT_UPGRADE ? definition->row : definition->slot;
            entry.placement = GetPlacement(
                entry.key,
                { definition->page == SOH_EQUIP_PAGE_VANILLA ? NativePage : "page." + std::to_string(definition->page),
                  row * 4 + definition->column });
            entries.push_back(std::move(entry));
        }
        return entries;
    }
    for (uint32_t i = 0; i < CustomItemRegistry_GetCount(); ++i) {
        const auto* definition = CustomItemRegistry_GetAt(i);
        if (definition == nullptr || !CustomItemRegistry_IsEditorVisible(definition->key) ||
            CustomItemRegistry_IsVanillaUpgrade(definition)) {
            continue;
        }
        SOHCustomItemPlacement placement;
        KaleidoItemManager_GetPlacement(definition->key, &placement);
        uint8_t itemKind = CustomItemRegistry_IsQuestPage(placement.page) ? SOH_LAYOUT_COLLECTABLES : SOH_LAYOUT_ITEMS;
        const auto defaultPlacement = sDefaults.find(definition->key);
        if (defaultPlacement != sDefaults.end()) {
            auto page = sTemplates.find(defaultPlacement->second.page);
            if (page != sTemplates.end()) {
                itemKind = page->second.kind;
            }
        }
        if (itemKind != kind) {
            continue;
        }
        Entry entry;
        entry.key = definition->key;
        entry.label = definition->key;
        entry.icon = CustomItemRegistry_ResolveTexture(definition->key, SOH_ITEM_ICON_INVENTORY);
        entry.name = definition->namePath;
        entry.custom = definition;
        entry.kind = kind;
        entry.sourceKind = kind;
        entry.item = ITEM_CUSTOM;
        entry.owned = CustomItemRegistry_IsOwned(definition->key);
        entry.placement = GetPlacement(entry.key, { ItemPage(placement.page), placement.slot });
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<Page> Pages(uint8_t kind, const std::vector<Entry>& entries) {
    std::vector<Page> pages{ NativeLayout(kind) };
    const auto nativeSettings = std::to_string(kind) + "." + NativePage;
    const int nativeRows = CVarGetInteger(Setting(nativeSettings, "Rows").c_str(), 0);
    if (kind != SOH_LAYOUT_COLLECTABLES && nativeRows > 0) {
        const int rows = std::clamp(nativeRows, 1, 8);
        const int columns =
            std::clamp(CVarGetInteger(Setting(nativeSettings, "Columns").c_str(), 6), 1, MaxCells / rows);
        pages.front() = MakeGrid(kind, NativePage, "Vanilla", rows, columns);
    }
    std::set<std::string> keys{ NativePage };
    for (const auto& [key, page] : sTemplates) {
        if (page.kind == kind) {
            pages.push_back(page);
            keys.insert(key);
        }
    }
    const int userCount = std::clamp(CVarGetInteger(Setting(std::to_string(kind), "Pages").c_str(), 0), 0, 128);
    for (int index = 0; index < userCount; ++index) {
        const auto key = "user." + std::to_string(index);
        if (keys.insert(key).second) {
            const auto settingsKey = std::to_string(kind) + "." + key;
            const int rows = std::clamp(CVarGetInteger(Setting(settingsKey, "Rows").c_str(), 4), 1, 8);
            const int columns = std::clamp(
                CVarGetInteger(Setting(settingsKey, "Columns").c_str(), kind == SOH_LAYOUT_EQUIPMENT ? 4 : 6), 1,
                MaxCells / rows);
            pages.push_back(MakeGrid(kind, key, "Custom " + std::to_string(index + 1), rows, columns));
        }
    }
    for (const auto& entry : entries) {
        if (keys.insert(entry.placement.page).second) {
            pages.push_back(MakeGrid(kind, entry.placement.page,
                                     "Page " + entry.placement.page.substr(entry.placement.page.find('.') + 1), 4,
                                     kind == SOH_LAYOUT_EQUIPMENT ? 4 : 6));
        }
    }
    return pages;
}

const Page& FindPage(const std::vector<Page>& pages, const std::string& key) {
    auto found = std::find_if(pages.begin(), pages.end(), [&](const Page& page) { return page.key == key; });
    return found == pages.end() ? pages.front() : *found;
}

std::vector<const Entry*> CellEntries(const std::vector<Entry>& entries, const Page& page, int cell, bool ownedOnly) {
    std::vector<const Entry*> result;
    for (const auto& entry : entries) {
        if (entry.placement.page == page.key && entry.placement.cell == cell && (!ownedOnly || entry.owned)) {
            result.push_back(&entry);
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const Entry* left, const Entry* right) {
        return (left->custom ? left->custom->priority : 0) > (right->custom ? right->custom->priority : 0);
    });
    return result;
}

std::string CellKey(const Page& page, int cell) {
    return std::to_string(page.kind) + "." + page.key + "." + std::to_string(cell);
}

const Entry* SelectedEntry(const std::vector<const Entry*>& entries, const Page& page, int cell) {
    if (entries.empty()) {
        return nullptr;
    }
    const auto selected = sSelected.find(CellKey(page, cell));
    if (selected != sSelected.end()) {
        for (const auto* entry : entries) {
            if (entry->key == selected->second) {
                return entry;
            }
        }
    }
    return entries.front();
}

void DrawIcon(const Entry& entry, float size) {
    auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
    if (entry.icon == nullptr) {
        ImGui::Dummy(ImVec2(size, size));
        return;
    }
    const std::string texture = "ModLayout." + std::string(entry.icon);
    if (!gui->HasTextureByName(texture)) {
        gui->LoadGuiTexture(texture, entry.icon, ImVec4(1, 1, 1, 1));
    }
    ImGui::Image(gui->GetTextureByName(texture), ImVec2(size, size));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", entry.label.c_str());
    }
}

void DrawCellEditor(const Page& page, int cell, const std::vector<Entry>& entries) {
    ImGui::Text("Cell %d", cell + 1);
    for (const auto* entry : CellEntries(entries, page, cell, false)) {
        ImGui::BulletText("%s", entry->label.c_str());
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Add / move an entry to this cell:");
    static ImGuiTextFilter filter;
    filter.Draw("Search", 240);
    ImGui::BeginChild("entries", ImVec2(360, 280), ImGuiChildFlags_Borders);
    for (const auto& entry : entries) {
        if (!filter.PassFilter(entry.label.c_str())) {
            continue;
        }
        const bool here = entry.placement.page == page.key && entry.placement.cell == cell;
        ImGui::PushID(entry.key.c_str());
        DrawIcon(entry, 24);
        ImGui::SameLine();
        if (ImGui::Selectable(entry.label.c_str(), here, ImGuiSelectableFlags_DontClosePopups)) {
            SetPlacement(entry, { page.key, cell });
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void DrawGrid(const Page& page, const std::vector<Entry>& entries) {
    const float width = std::max(280.0f, ImGui::GetContentRegionAvail().x - 8);
    const float scale = std::min(width / 256.0f, 3.0f);
    const auto origin = ImGui::GetCursorScreenPos();
    for (size_t cell = 0; cell < page.cells.size(); ++cell) {
        const auto& bounds = page.cells[cell];
        const ImVec2 pos(origin.x + (bounds.x + 120) * scale, origin.y + (70 - bounds.y) * scale);
        const ImVec2 size(std::max(18.0f, bounds.width * scale), std::max(18.0f, bounds.height * scale));
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushID(static_cast<int>(cell));
        if (ImGui::InvisibleButton("cell", size)) {
            ImGui::OpenPopup("Edit cell");
        }
        auto draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::GetColorU32(ImGuiCol_FrameBg), 4);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::GetColorU32(ImGuiCol_Border), 4);
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MOD_LAYOUT_ENTRY")) {
                const auto* key = static_cast<const char*>(payload->Data);
                auto found =
                    std::find_if(entries.begin(), entries.end(), [&](const Entry& entry) { return entry.key == key; });
                if (found != entries.end()) {
                    SetPlacement(*found, { page.key, static_cast<int>(cell) });
                }
            }
            ImGui::EndDragDropTarget();
        }
        const auto contents = CellEntries(entries, page, cell, false);
        if (!contents.empty()) {
            ImGui::SetCursorScreenPos(ImVec2(pos.x + 2, pos.y + 2));
            DrawIcon(*contents.front(), std::max(14.0f, std::min(size.x, size.y) - 4));
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                const auto& key = contents.front()->key;
                ImGui::SetDragDropPayload("MOD_LAYOUT_ENTRY", key.c_str(), key.size() + 1);
                ImGui::TextUnformatted(contents.front()->label.c_str());
                ImGui::EndDragDropSource();
            }
            if (contents.size() > 1) {
                draw->AddText(ImVec2(pos.x + size.x - 18, pos.y + 2), IM_COL32(255, 220, 80, 255),
                              std::to_string(contents.size()).c_str());
            }
        } else {
            draw->AddText(ImVec2(pos.x + 3, pos.y + 3), ImGui::GetColorU32(ImGuiCol_TextDisabled), "+");
        }
        if (ImGui::BeginPopup("Edit cell")) {
            DrawCellEditor(page, static_cast<int>(cell), entries);
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 160 * scale));
    ImGui::Dummy(ImVec2(width, 1));
}

class LayoutWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {
    }
    void UpdateElement() override {
    }
    void DrawElement() override {
        if (!ModLayout_HasMods()) {
            return;
        }
        ImGui::BeginDisabled(CVarGetInteger(CVAR_SETTING("DisableChanges"), 0));
        if (ImGui::BeginTabBar("Layouts")) {
            for (uint8_t kind = 0; kind < SOH_LAYOUT_COUNT; ++kind) {
                if (ImGui::BeginTabItem(KindNames[kind])) {
                    ModLayout_DrawEditor(kind);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
        ImGui::EndDisabled();
    }
};

void PlaySound(uint16_t sound) {
    Audio_PlaySoundGeneral(sound, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

void ResolveName(PlayState* play, uint16_t item, const char** name) {
    if (item != ITEM_CUSTOM) {
        return;
    }
    for (uint8_t kind = 0; kind < SOH_LAYOUT_COUNT; ++kind) {
        if (play->pauseCtx.pageIndex != PausePages[kind] || !sActive[kind]) {
            continue;
        }
        const auto entries = Entries(kind);
        const auto pages = Pages(kind, entries);
        const auto& page = FindPage(pages, sShown[kind]);
        const auto* selected = SelectedEntry(CellEntries(entries, page, sCursor[kind], true), page, sCursor[kind]);
        if (selected != nullptr) {
            const char* resolved = selected->custom != nullptr
                                       ? CustomItemRegistry_ResolveTexture(selected->key.c_str(), SOH_ITEM_NAME_TEXTURE)
                                       : selected->name;
            if (resolved != nullptr) {
                *name = resolved;
            }
        }
    }
}

int ReadDirection(PlayState* play, uint8_t kind) {
    auto& pause = play->pauseCtx;
    const auto buttons = play->state.input[0].press.button;
    const bool dpad =
        CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) != 0 && !CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0);
    int direction = pause.stickRelX > 30    ? 1
                    : pause.stickRelX < -30 ? -1
                    : pause.stickRelY > 30  ? -2
                    : pause.stickRelY < -30 ? 2
                                            : 0;
    if (dpad) {
        if (buttons & BTN_DRIGHT) {
            direction = 1;
        }
        if (buttons & BTN_DLEFT) {
            direction = -1;
        }
        if (buttons & BTN_DUP) {
            direction = -2;
        }
        if (buttons & BTN_DDOWN) {
            direction = 2;
        }
    }
    if (direction == 0) {
        sLastDirection[kind] = 0;
        sRepeat[kind] = 0;
        return 0;
    }
    if (sLastDirection[kind] == direction && sRepeat[kind]-- > 0) {
        return 0;
    }
    sRepeat[kind] = sLastDirection[kind] == direction ? 3 : 10;
    sLastDirection[kind] = direction;
    return direction;
}

void MoveCursor(PlayState* play, const Page& page, const std::vector<Entry>& entries, int direction) {
    auto& pause = play->pauseCtx;
    if (pause.cursorSpecialPos != 0) {
        if ((pause.cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT && direction == 1) ||
            (pause.cursorSpecialPos == PAUSE_CURSOR_PAGE_RIGHT && direction == -1)) {
            pause.cursorSpecialPos = 0;
        }
        return;
    }
    const auto& origin = page.cells[sCursor[page.kind]];
    float score = 1e9f;
    int next = -1;
    for (size_t i = 0; i < page.cells.size(); ++i) {
        if (CellEntries(entries, page, i, true).empty()) {
            continue;
        }
        const auto& target = page.cells[i];
        const float dx = target.x + target.width / 2.0f - origin.x - origin.width / 2.0f;
        const float dy = target.y - target.height / 2.0f - origin.y + origin.height / 2.0f;
        const float forward = direction == 1 ? dx : direction == -1 ? -dx : direction == -2 ? dy : -dy;
        const float lateral = std::abs(direction == 1 || direction == -1 ? dy : dx);
        if (forward > 1 && forward + lateral * 3 < score) {
            score = forward + lateral * 3;
            next = i;
        }
    }
    if (next >= 0) {
        sCursor[page.kind] = next;
        pause.namedItem = PAUSE_ITEM_NONE;
        PlaySound(NA_SE_SY_CURSOR);
    } else if (direction == -1 || direction == 1) {
        KaleidoScope_MoveCursorToSpecialPos(play, direction == -1 ? PAUSE_CURSOR_PAGE_LEFT : PAUSE_CURSOR_PAGE_RIGHT);
    }
}

bool IsAgeAllowed(const Entry& entry) {
    if (entry.custom != nullptr) {
        return CustomItemRegistry_IsAgeAllowed(entry.key.c_str());
    }
    if (entry.equipment != nullptr) {
        return CustomEquipRegistry_IsAgeAllowed(entry.key.c_str());
    }
    if (entry.sourceKind == SOH_LAYOUT_ITEMS) {
        return CHECK_AGE_REQ_ITEM(entry.item);
    }
    if (entry.sourceKind == SOH_LAYOUT_EQUIPMENT) {
        return CHECK_AGE_REQ_EQUIP(entry.source / 4, entry.source % 4);
    }
    return true;
}

void ActivateEquipment(PlayState* play, const Entry& entry) {
    if (!IsAgeAllowed(entry)) {
        PlaySound(NA_SE_SY_ERROR);
        return;
    }
    if (entry.equipment != nullptr) {
        const auto slot = entry.equipment->slot;
        if (slot == SOH_EQUIP_SLOT_UPGRADE) {
            CustomEquipRegistry_SetToggle(entry.key.c_str(), !CustomEquipRegistry_IsToggleOn(entry.key.c_str()));
        } else if (CustomEquipRegistry_IsWorn(entry.key.c_str())) {
            CustomEquipRegistry_TakeOff(slot);
        } else {
            CustomEquipRegistry_Wear(entry.key.c_str());
        }
    } else {
        const int slot = entry.source / 4;
        int value = entry.source % 4;
        if (value == 0) {
            return;
        }
        CustomEquipRegistry_TakeOff(slot);
        if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentCanBeRemoved"), 0) && CUR_EQUIP_VALUE(slot) == value) {
            if (slot == EQUIP_TYPE_SWORD) {
                const int toggle = CVarGetInteger(CVAR_ENHANCEMENT("SwordToggle"), SWORD_TOGGLE_NONE);
                if (toggle == SWORD_TOGGLE_BOTH_AGES || (toggle == SWORD_TOGGLE_CHILD && LINK_IS_CHILD)) {
                    value = EQUIP_VALUE_SWORD_NONE;
                } else if (value == EQUIP_VALUE_SWORD_BIGGORON &&
                           CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER)) {
                    value = EQUIP_VALUE_SWORD_MASTER;
                }
            } else {
                value = slot == EQUIP_TYPE_SHIELD ? 0 : 1;
            }
        }
        Inventory_ChangeEquipment(slot, value);
        if (slot == EQUIP_TYPE_SWORD) {
            gSaveContext.equips.buttonItems[0] = value == 0 ? ITEM_NONE : ITEM_SWORD_KOKIRI + value - 1;
            if (value == EQUIP_VALUE_SWORD_BIGGORON) {
                if (gSaveContext.bgsFlag) {
                    gSaveContext.swordHealth = 8;
                } else if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
                    gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KNIFE;
                }
            }
            if (value == 0) {
                Flags_SetInfTable(INFTABLE_SWORDLESS);
            } else {
                gSaveContext.infTable[29] = 0;
                Flags_UnsetInfTable(INFTABLE_SWORDLESS);
            }
            Interface_LoadItemIcon1(play, 0);
        }
        Player_SetEquipmentData(play, GET_PLAYER(play));
    }
    PlaySound(NA_SE_SY_DECIDE);
}

bool IsTradeEntry(const Entry& entry) {
    return entry.custom == nullptr && entry.sourceKind == SOH_LAYOUT_ITEMS &&
           (entry.source == SLOT_TRADE_CHILD || entry.source == SLOT_TRADE_ADULT);
}

void SelectTradeItem(PlayState* play, const Entry& entry) {
    gSaveContext.inventory.items[entry.source] = entry.item;
    for (int button = 1; button < ARRAY_COUNT(gSaveContext.equips.buttonItems); ++button) {
        if (gSaveContext.equips.cButtonSlots[button - 1] == entry.source &&
            CustomItemRegistry_GetEquippedKey(button) == nullptr) {
            gSaveContext.equips.buttonItems[button] = entry.item;
            Interface_LoadItemIcon1(play, button);
        }
    }
}

void EquipEntry(PlayState* play, const Entry& entry, const SOHLayoutCell& cell) {
    const uint16_t buttons[] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };
    for (int i = 0; i < 7; ++i) {
        if (!(play->state.input[0].press.button & buttons[i]) ||
            (i >= 3 && !CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0))) {
            continue;
        }
        if (!IsAgeAllowed(entry)) {
            PlaySound(NA_SE_SY_ERROR);
        } else if (entry.custom != nullptr) {
            if (!ModLayout_BeginCustomEquip(play, entry.key.c_str(), i + 1, cell.x * 10, cell.y * 10)) {
                PlaySound(NA_SE_SY_ERROR);
            }
        } else if (entry.sourceKind == SOH_LAYOUT_ITEMS) {
            if (IsTradeEntry(entry)) {
                SelectTradeItem(play, entry);
            }
            KaleidoScope_SetupItemEquip(play, entry.item, entry.source, cell.x * 10, cell.y * 10);
        }
        return;
    }
}

void TurnCell(PlayState* play, const Page& page, const std::vector<const Entry*>& entries, int direction) {
    const auto* previous = SelectedEntry(entries, page, sCursor[page.kind]);
    const int index = std::find(entries.begin(), entries.end(), previous) - entries.begin();
    const auto* next = entries[(index + (direction > 0 ? 1 : entries.size() - 1)) % entries.size()];
    sSelected[CellKey(page, sCursor[page.kind])] = next->key;
    if (IsTradeEntry(*next) && IsTradeEntry(*previous) && next->source == previous->source) {
        SelectTradeItem(play, *next);
    }
    if (previous->custom != nullptr && next->custom != nullptr) {
        for (uint8_t button = 0; button < 8; ++button) {
            const char* key = CustomItemRegistry_GetEquippedKey(button);
            if (key != nullptr && previous->key == key) {
                CustomItemRegistry_Equip(button, next->key.c_str());
            }
        }
    }
    play->pauseCtx.namedItem = PAUSE_ITEM_NONE;
    PlaySound(NA_SE_SY_CURSOR);
}

void HandleInput(PlayState* play, const Page& page, const std::vector<Entry>& entries) {
    const auto kind = page.kind;
    auto& pause = play->pauseCtx;
    if (pause.pageIndex != PausePages[kind] || pause.state != 6 || pause.unk_1E4 != 0 ||
        sInputFrame[kind] == play->state.frames) {
        return;
    }
    sInputFrame[kind] = play->state.frames;
    const auto contents = CellEntries(entries, page, sCursor[kind], true);
    const auto* selected = SelectedEntry(contents, page, sCursor[kind]);
    const auto buttons = play->state.input[0].press.button;
    const int direction = ReadDirection(play, kind);
    if (pause.cursorSpecialPos == 0 && contents.size() > 1 && (buttons & BTN_A)) {
        sCycling[kind] = !sCycling[kind];
        if (!sCycling[kind] && kind == SOH_LAYOUT_EQUIPMENT && selected != nullptr) {
            ActivateEquipment(play, *selected);
        }
        PlaySound(NA_SE_SY_DECIDE);
    } else if (pause.cursorSpecialPos == 0 && selected != nullptr && (buttons & BTN_A)) {
        if (kind == SOH_LAYOUT_EQUIPMENT) {
            ActivateEquipment(play, *selected);
        } else if (selected->sourceKind == SOH_LAYOUT_COLLECTABLES && selected->custom == nullptr &&
                   selected->source >= 6 && selected->source < 18) {
            Audio_OcaSetInstrument(1);
            Audio_OcaSetSongPlayback(gOcarinaSongItemMap[selected->source - 6] + 1, 1);
            sPreviewingSong = true;
        }
    }
    if (direction != 0) {
        if (sPreviewingSong) {
            Audio_OcaSetInstrument(0);
            sPreviewingSong = false;
        }
        if (sCycling[kind] && contents.size() > 1 && pause.cursorSpecialPos == 0) {
            TurnCell(play, page, contents, direction);
        } else {
            sCycling[kind] = false;
            MoveCursor(play, page, entries, direction);
        }
    }
    if (direction == 0 && selected != nullptr && pause.cursorSpecialPos == 0) {
        EquipEntry(play, *selected, page.cells[sCursor[kind]]);
        if (selected->custom != nullptr && (buttons & BTN_CUP)) {
            const char* text = selected->custom->pauseText;
            if (text != nullptr) {
                CustomItemRegistry_ShowTextbox(play, text, true);
                pause.unk_1E4 = 10;
            }
        }
    }
}

bool IsEntryEquipped(const Entry& entry) {
    if (entry.equipment != nullptr) {
        return entry.equipment->slot != SOH_EQUIP_SLOT_UPGRADE && CustomEquipRegistry_IsWorn(entry.key.c_str());
    }
    if (entry.sourceKind != SOH_LAYOUT_EQUIPMENT || entry.custom != nullptr || entry.source % 4 == 0) {
        return false;
    }
    const int slot = entry.source / 4;
    return CUR_EQUIP_VALUE(slot) == entry.source % 4 && CustomEquipRegistry_GetWorn(slot) == nullptr;
}

void FillQuad(Vtx* vertices, const SOHLayoutCell& cell, int textureWidth, int textureHeight, int offsetY) {
    for (int i = 0; i < 4; ++i) {
        vertices[i] = {};
        vertices[i].v.ob[0] = cell.x + (i % 2 ? cell.width : 0);
        vertices[i].v.ob[1] = cell.y + offsetY - (i / 2 ? cell.height : 0);
        vertices[i].v.tc[0] = i % 2 ? textureWidth << 5 : 0;
        vertices[i].v.tc[1] = i / 2 ? textureHeight << 5 : 0;
        for (int channel = 0; channel < 4; ++channel) {
            vertices[i].v.cn[channel] = 255;
        }
    }
}

} // namespace

static void DrawEquippedOutlines(PlayState* play, const Page& page, const std::vector<Entry>& entries) {
    auto& pause = play->pauseCtx;
    auto* vertices = static_cast<Vtx*>(Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * page.cells.size() * 4));

    if (vertices == nullptr) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, ZREG(39), ZREG(40), ZREG(41), pause.alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, ZREG(43), ZREG(44), ZREG(45), 0);
    for (size_t cell = 0; cell < page.cells.size(); ++cell) {
        const auto contents = CellEntries(entries, page, cell, true);
        const auto* selected = SelectedEntry(contents, page, cell);

        if (selected == nullptr || !IsEntryEquipped(*selected)) {
            continue;
        }
        FillQuad(&vertices[cell * 4], page.cells[cell], 32, 32, pause.offsetY);
        gDPPipeSync(POLY_OPA_DISP++);
        gSPVertex(POLY_OPA_DISP++, reinterpret_cast<uintptr_t>(&vertices[cell * 4]), 4, 0);
        POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, (void*)gEquippedItemOutlineTex, 32, 32, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool IsItemEntryEquipped(const Entry& entry) {
    if (entry.custom != nullptr) {
        for (uint8_t button = 1; button < 8; ++button) {
            const char* key = CustomItemRegistry_GetEquippedKey(button);
            if (key != nullptr && entry.key == key) {
                return true;
            }
        }
        return false;
    }
    if (entry.sourceKind != SOH_LAYOUT_ITEMS || entry.source < 0 || entry.item == ITEM_NONE) {
        return false;
    }
    for (int button = 0; button < ARRAY_COUNT(gSaveContext.equips.cButtonSlots); ++button) {
        if (gSaveContext.equips.cButtonSlots[button] == entry.source &&
            gSaveContext.equips.buttonItems[button + 1] == entry.item) {
            return true;
        }
    }
    return false;
}

static void DrawItemEquippedOutlines(PlayState* play, const Page& page, const std::vector<Entry>& entries) {
    auto& pause = play->pauseCtx;
    auto* vertices = static_cast<Vtx*>(Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * page.cells.size() * 4));
    if (vertices == nullptr) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pause.alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 0);
    for (size_t cell = 0; cell < page.cells.size(); ++cell) {
        const auto contents = CellEntries(entries, page, cell, true);
        if (std::none_of(contents.begin(), contents.end(),
                         [](const Entry* entry) { return IsItemEntryEquipped(*entry); })) {
            continue;
        }
        auto bounds = page.cells[cell];
        bounds.x -= 2;
        bounds.y += 2;
        bounds.width += 4;
        bounds.height += 4;
        FillQuad(&vertices[cell * 4], bounds, 32, 32, pause.offsetY);
        gDPPipeSync(POLY_OPA_DISP++);
        gSPVertex(POLY_OPA_DISP++, reinterpret_cast<uintptr_t>(&vertices[cell * 4]), 4, 0);
        POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, (void*)gEquippedItemOutlineTex, 32, 32, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawItemAmmoCounts(PlayState* play, const Page& page, const std::vector<Entry>& entries) {
    auto& pause = play->pauseCtx;
    auto* vertices = static_cast<Vtx*>(Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * page.cells.size() * 8));
    if (vertices == nullptr) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    CLOSE_DISPS(play->state.gfxCtx);
    for (size_t cell = 0; cell < page.cells.size(); ++cell) {
        const auto* selected = SelectedEntry(CellEntries(entries, page, cell, true), page, cell);
        if (selected == nullptr || selected->sourceKind != SOH_LAYOUT_ITEMS) {
            continue;
        }
        const bool isVanillaAmmo =
            selected->custom == nullptr && selected->source >= 0 && ItemInSlotUsesAmmo(selected->source);
        const int32_t customAmmo = selected->custom != nullptr && selected->custom->getAmmo != nullptr
                                       ? selected->custom->getAmmo(selected->key.c_str())
                                       : -1;
        if (!isVanillaAmmo && customAmmo < 0) {
            continue;
        }
        const auto& bounds = page.cells[cell];
        Vtx* digits = &vertices[cell * 8];
        FillQuad(digits, { bounds.x, static_cast<int16_t>(bounds.y - 22), 8, 8 }, 8, 8, pause.offsetY);
        FillQuad(&digits[4], { static_cast<int16_t>(bounds.x + 6), static_cast<int16_t>(bounds.y - 22), 8, 8 }, 8, 8,
                 pause.offsetY);
        if (isVanillaAmmo) {
            KaleidoScope_DrawAmmoCountAt(&pause, play->state.gfxCtx, selected->item, digits);
            continue;
        }
        const uint8_t shade = !IsAgeAllowed(*selected) ? 100 : customAmmo == 0 ? 130 : 255;
        OPEN_DISPS(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, shade, shade, shade, pause.alpha);
        CLOSE_DISPS(play->state.gfxCtx);
        KaleidoScope_DrawAmmoDigits(play->state.gfxCtx, static_cast<s16>(std::min(customAmmo, 99)), digits);
    }
}

static void DrawItemCellWheels(PlayState* play, const Page& page, const std::vector<Entry>& entries) {
    const auto& pause = play->pauseCtx;
    for (size_t cell = 0; cell < page.cells.size(); ++cell) {
        const auto contents = CellEntries(entries, page, cell, true);
        if (contents.size() < 2) {
            continue;
        }
        const auto* selected = SelectedEntry(contents, page, cell);
        const auto index = std::find(contents.begin(), contents.end(), selected) - contents.begin();
        const auto count = static_cast<std::ptrdiff_t>(contents.size());
        const auto* previous = contents[(index + count - 1) % count];
        const auto* next = contents[(index + 1) % count];
        const KaleidoCycleIcon left = { const_cast<char*>(previous->icon), IsAgeAllowed(*previous) };
        const KaleidoCycleIcon right = previous == next
                                           ? KaleidoCycleIcon{ nullptr, true }
                                           : KaleidoCycleIcon{ const_cast<char*>(next->icon), IsAgeAllowed(*next) };
        const auto& bounds = page.cells[cell];
        const bool hovered = pause.pageIndex == PAUSE_ITEM && pause.cursorSpecialPos == 0 && pause.unk_1E4 == 0 &&
                             cell == sCursor[SOH_LAYOUT_ITEMS];
        KaleidoScope_DrawCycleIconsAt(play, static_cast<uint8_t>(cell), left, right, bounds.x + bounds.width / 2,
                                      bounds.y + pause.offsetY - bounds.height / 2,
                                      hovered && sCycling[SOH_LAYOUT_ITEMS], hovered);
    }
}

bool ModLayout_IsActive(uint8_t kind) {
    return kind < SOH_LAYOUT_COUNT && sActive[kind];
}

bool ModLayout_BeginCustomEquip(PlayState* play, const char* key, uint8_t button, int16_t x, int16_t y) {
    const auto* definition = CustomItemRegistry_Find(key);
    if (play == nullptr || definition == nullptr || button < 1 || button > 7 || !CustomItemRegistry_IsOwned(key) ||
        !CustomItemRegistry_IsAgeAllowed(key) || CustomItemRegistry_IsVanillaUpgrade(definition) ||
        !(definition->flags & SOH_CUSTOM_ITEM_EQUIPPABLE) ||
        !(definition->flags & (button <= 3 ? SOH_CUSTOM_ITEM_C_BUTTON : SOH_CUSTOM_ITEM_DPAD))) {
        return false;
    }
    sAnimatingItem = key;
    KaleidoScope_SetupItemEquip(play, ITEM_CUSTOM, SLOT_CUSTOM, x, y);
    play->pauseCtx.equipTargetCBtn = button - 1;
    return true;
}

const char* ModLayout_GetEquipIcon(void) {
    const char* icon = CustomItemRegistry_ResolveTexture(sAnimatingItem.c_str(), SOH_ITEM_ICON_INVENTORY);
    return icon != nullptr ? icon : static_cast<const char*>(gItemIcons[ITEM_SOLD_OUT]);
}

void ModLayout_CompleteCustomEquip(PlayState* play) {
    if (!sAnimatingItem.empty() &&
        !CustomItemRegistry_Equip(play->pauseCtx.equipTargetCBtn + 1, sAnimatingItem.c_str())) {
        PlaySound(NA_SE_SY_ERROR);
    }
    sAnimatingItem.clear();
}

bool ModLayout_CyclePage(PlayState* play, uint8_t kind) {
    if (play == nullptr || kind >= SOH_LAYOUT_COUNT || play->pauseCtx.state != 6 ||
        (play->pauseCtx.unk_1E4 != 0 && play->pauseCtx.unk_1E4 != 8)) {
        return false;
    }
    const auto entries = Entries(kind);
    const auto pages = Pages(kind, entries);
    std::vector<std::string> visible{ NativePage };
    for (const auto& page : pages) {
        if (page.key == NativePage) {
            continue;
        }
        visible.push_back(page.key);
    }
    if (visible.size() == 1) {
        return false;
    }
    auto current = std::find(visible.begin(), visible.end(), sShown[kind]);
    sShown[kind] = current == visible.end() || ++current == visible.end() ? visible.front() : *current;
    const auto& page = FindPage(pages, sShown[kind]);
    sCursor[kind] = 0;
    for (size_t cell = 0; cell < page.cells.size(); ++cell) {
        if (!CellEntries(entries, page, cell, true).empty()) {
            sCursor[kind] = cell;
            break;
        }
    }
    sCycling[kind] = false;
    play->pauseCtx.unk_1E4 = 0;
    play->pauseCtx.cursorSpecialPos = 0;
    Audio_OcaSetInstrument(0);
    if (sPreviewingSong) {
        Audio_OcaSetInstrument(0);
        sPreviewingSong = false;
    }
    play->pauseCtx.namedItem = PAUSE_ITEM_NONE;
    PlaySound(NA_SE_SY_WIN_SCROLL_RIGHT);
    return true;
}

bool ModLayout_DrawPage(PlayState* play, uint8_t kind) {
    if (play == nullptr || kind >= SOH_LAYOUT_COUNT) {
        return false;
    }
    const auto entries = Entries(kind);
    const auto pages = Pages(kind, entries);
    const auto& page = FindPage(pages, sShown[kind]);
    sShown[kind] = page.key;
    const bool movedVanilla = std::any_of(entries.begin(), entries.end(), [](const Entry& entry) {
        return entry.key.rfind("vanilla.", 0) == 0 &&
               (entry.placement.page != NativePage || entry.placement.cell != entry.source);
    });
    const bool customOnNative =
        kind == SOH_LAYOUT_ITEMS && std::any_of(entries.begin(), entries.end(), [](const Entry& entry) {
            return entry.custom != nullptr && entry.owned && entry.placement.page == NativePage;
        });
    if (page.key == NativePage && !movedVanilla && !customOnNative &&
        !CVarGetInteger(Setting(std::to_string(kind), "Edited").c_str(), 0)) {
        sActive[kind] = false;
        return false;
    }
    sActive[kind] = true;
    sCursor[kind] = std::clamp(sCursor[kind], 0, static_cast<int>(page.cells.size()) - 1);
    HandleInput(play, page, entries);
    auto& pause = play->pauseCtx;
    auto* vertices = static_cast<Vtx*>(Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * page.cells.size() * 4));
    if (vertices == nullptr) {
        return true;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_42Opa(play->state.gfxCtx);
    if (kind == SOH_LAYOUT_EQUIPMENT) {
        DrawEquippedOutlines(play, page, entries);
    }
    if (kind == SOH_LAYOUT_ITEMS) {
        DrawItemEquippedOutlines(play, page, entries);
    }
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    for (size_t cell = 0; cell < page.cells.size(); ++cell) {
        const auto contents = CellEntries(entries, page, cell, true);
        const auto* selected = SelectedEntry(contents, page, cell);
        const bool quest =
            selected != nullptr && selected->custom == nullptr && selected->sourceKind == SOH_LAYOUT_COLLECTABLES;
        const bool song = quest && selected->source >= 6 && selected->source < 18;
        const bool heart = quest && selected->source == 24;
        const int textureWidth = heart ? 48 : song ? 16 : quest ? 24 : 32;
        const int textureHeight = song ? 24 : textureWidth;
        auto bounds = page.cells[cell];
        if (kind == SOH_LAYOUT_ITEMS && pause.pageIndex == PAUSE_ITEM && pause.cursorSpecialPos == 0 &&
            pause.unk_1E4 == 0 && cell == sCursor[kind] && selected != nullptr && IsAgeAllowed(*selected)) {
            bounds.x -= 2;
            bounds.y += 2;
            bounds.width += 4;
            bounds.height += 4;
        }
        FillQuad(&vertices[cell * 4], bounds, textureWidth, textureHeight, pause.offsetY);
        if (selected == nullptr || selected->icon == nullptr) {
            continue;
        }
        gDPPipeSync(POLY_OPA_DISP++);
        const bool allowed = IsAgeAllowed(*selected);
        const bool isPassiveOff = selected->equipment != nullptr &&
                                  selected->equipment->slot == SOH_EQUIP_SLOT_UPGRADE &&
                                  !CustomEquipRegistry_IsToggleOn(selected->key.c_str());
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, allowed ? 255 : 100, allowed ? 255 : 100, allowed ? 255 : 100,
                        isPassiveOff ? pause.alpha / 2 : pause.alpha);
        gSPVertex(POLY_OPA_DISP++, reinterpret_cast<uintptr_t>(&vertices[cell * 4]), 4, 0);
        if (song || heart) {
            void* texture = heart ? gItemIcons[0x79 + ((gSaveContext.inventory.questItems >> 28) & 0xF)]
                                  : const_cast<char*>(selected->icon);
            POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, texture, textureWidth, textureHeight, 0);
        } else {
            KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, const_cast<char*>(selected->icon), textureWidth,
                                               textureHeight, 0);
        }
    }
    if (kind == SOH_LAYOUT_ITEMS) {
        DrawItemAmmoCounts(play, page, entries);
    }
    if (pause.pageIndex == PausePages[kind] && pause.cursorSpecialPos == 0) {
        const auto contents = CellEntries(entries, page, sCursor[kind], true);
        const auto* selected = SelectedEntry(contents, page, sCursor[kind]);
        pause.cursorPoint[PausePages[kind]] = sCursor[kind];
        pause.cursorX[PausePages[kind]] =
            selected != nullptr && selected->sourceKind == SOH_LAYOUT_EQUIPMENT && selected->source >= 0
                ? selected->source % 4
                : 0;
        pause.cursorY[PausePages[kind]] =
            selected != nullptr && selected->sourceKind == SOH_LAYOUT_EQUIPMENT && selected->source >= 0
                ? selected->source / 4
                : 0;
        pause.cursorSlot[PausePages[kind]] =
            selected != nullptr && selected->source >= 0 ? selected->source : SLOT_CUSTOM;
        pause.cursorItem[PausePages[kind]] = selected == nullptr              ? PAUSE_ITEM_NONE
                                             : selected->equipment != nullptr ? ITEM_CUSTOM
                                                                              : selected->item;
        pause.cursorColorSet = sCycling[kind] ? 8 : 0;
        pause.nameColorSet = selected != nullptr && !IsAgeAllowed(*selected) ? 1 : 0;
        if (selected != nullptr && selected->custom != nullptr) {
            CustomItemRegistry_SelectPauseItem(selected->key.c_str());
        }
        KaleidoScope_SetCursorVtx(&pause, sCursor[kind] * 4, vertices);
        KaleidoScope_DrawCursor(play, PausePages[kind]);
    }
    if (kind == SOH_LAYOUT_ITEMS) {
        DrawItemCellWheels(play, page, entries);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

bool ModLayout_HasMods(void) {
    return CustomItemRegistry_GetCount() != 0 || CustomEquipRegistry_GetCount() != 0 || !sTemplates.empty();
}

bool ModLayout_RegisterPage(const SOHLayoutPage* definition) {
    if (definition == nullptr || definition->structSize < sizeof(SOHLayoutPage) || definition->key == nullptr ||
        definition->key[0] == '\0' || definition->kind >= SOH_LAYOUT_COUNT || definition->rows == 0 ||
        definition->columns == 0 || definition->rows * definition->columns > MaxCells ||
        definition->cellCount > MaxCells || std::string(definition->key) == NativePage ||
        std::string(definition->key).rfind("user.", 0) == 0 || std::string(definition->key).rfind("page.", 0) == 0 ||
        sTemplates.count(definition->key)) {
        return false;
    }
    auto page = MakeGrid(definition->kind, definition->key, definition->title ? definition->title : definition->key,
                         definition->rows, definition->columns);
    if (definition->cells != nullptr && definition->cellCount != 0) {
        for (int i = 0; i < definition->cellCount; ++i) {
            const auto& cell = definition->cells[i];
            if (cell.width == 0 || cell.height == 0 || cell.width > 128 || cell.height > 128 || cell.x < -128 ||
                cell.x + cell.width > 128 || cell.y > 80 || cell.y - cell.height < -88) {
                return false;
            }
        }
        page.cells.assign(definition->cells, definition->cells + definition->cellCount);
    }
    sTemplates.emplace(page.key, std::move(page));
    return true;
}

bool ModLayout_PlaceItem(const char* itemKey, const char* pageKey, uint8_t cell) {
    if (itemKey == nullptr || pageKey == nullptr) {
        return false;
    }
    const bool isNativeEntry = std::string(itemKey).rfind("vanilla.", 0) == 0;
    if (std::string(pageKey) == NativePage) {
        if (cell >= MaxCells || (!isNativeEntry && CustomItemRegistry_Find(itemKey) == nullptr &&
                                 CustomEquipRegistry_Find(itemKey) == nullptr)) {
            return false;
        }
        sDefaults[itemKey] = { pageKey, cell };
        return true;
    }
    auto found = sTemplates.find(pageKey);
    if (found == sTemplates.end() || cell >= found->second.cells.size()) {
        return false;
    }
    if (isNativeEntry) {
        sDefaults[itemKey] = { pageKey, cell };
        return true;
    }
    const auto* item = CustomItemRegistry_Find(itemKey);
    const auto* equipment = CustomEquipRegistry_Find(itemKey);
    if ((found->second.kind == SOH_LAYOUT_EQUIPMENT && equipment == nullptr) ||
        (found->second.kind != SOH_LAYOUT_EQUIPMENT && item == nullptr)) {
        return false;
    }
    sDefaults[itemKey] = { pageKey, cell };
    return true;
}

bool ModLayout_RegisterVanilla(const SOHLayoutVanilla* definition) {
    if (definition == nullptr || definition->key == nullptr || definition->key[0] == '\0' ||
        definition->pageKey == nullptr || definition->sourceKind >= SOH_LAYOUT_COUNT ||
        definition->source >= (definition->sourceKind == SOH_LAYOUT_EQUIPMENT ? 16
                               : definition->sourceKind == SOH_LAYOUT_ITEMS   ? 24
                                                                              : 25)) {
        return false;
    }
    auto found = sTemplates.find(definition->pageKey);
    if (found == sTemplates.end() || definition->cell >= found->second.cells.size() ||
        std::any_of(sVanillaReferences.begin(), sVanillaReferences.end(),
                    [&](const VanillaReference& ref) { return ref.key == definition->key; })) {
        return false;
    }
    sVanillaReferences.push_back(
        { definition->key, definition->pageKey, definition->sourceKind, definition->source, definition->cell });
    return true;
}

void ModLayout_DrawEditor(uint8_t kind) {
    if (kind >= SOH_LAYOUT_COUNT) {
        return;
    }
    ImGui::PushID(kind);
    const auto entries = Entries(kind);
    const auto pages = Pages(kind, entries);
    const auto& page = FindPage(pages, sEdited[kind]);
    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("Page", page.title.c_str())) {
        for (const auto& candidate : pages) {
            if (ImGui::Selectable(candidate.title.c_str(), candidate.key == page.key)) {
                sEdited[kind] = candidate.key;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    const auto pageCountKey = Setting(std::to_string(kind), "Pages");
    const int count = std::clamp(CVarGetInteger(pageCountKey.c_str(), 0), 0, 128);
    ImGui::BeginDisabled(count >= 128);
    if (ImGui::Button("New page")) {
        sEdited[kind] = "user." + std::to_string(count);
        CVarSetInteger(pageCountKey.c_str(), count + 1);
        SaveLayout();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reset layout")) {
        ImGui::OpenPopup("Reset layout?");
    }
    if (ImGui::BeginPopup("Reset layout?")) {
        ImGui::TextUnformatted("Restore placements provided by the game and installed mods?");
        if (ImGui::Button("Restore defaults")) {
            for (const auto& entry : entries) {
                CVarClear(Setting(entry.key, "Page").c_str());
                CVarClear(Setting(entry.key, "Cell").c_str());
            }
            CVarClear(Setting(std::to_string(kind), "Edited").c_str());
            CVarClear(Setting(std::to_string(kind) + "." + NativePage, "Rows").c_str());
            CVarClear(Setting(std::to_string(kind) + "." + NativePage, "Columns").c_str());
            for (int index = 0; index < count; ++index) {
                const auto key = std::to_string(kind) + ".user." + std::to_string(index);
                CVarClear(Setting(key, "Rows").c_str());
                CVarClear(Setting(key, "Columns").c_str());
            }
            CVarClear(pageCountKey.c_str());
            sEdited[kind] = NativePage;
            sShown[kind] = NativePage;
            SaveLayout();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::TextWrapped("Click a cell to add or move entries. Multiple entries share a wheel. Layout changes do not "
                       "grant or remove items.");
    if (page.key.rfind("user.", 0) == 0 || (page.key == NativePage && kind != SOH_LAYOUT_COLLECTABLES)) {
        int rows = page.rows;
        int columns = page.columns;
        ImGui::SetNextItemWidth(100);
        bool changed = ImGui::InputInt("Rows", &rows);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        changed |= ImGui::InputInt("Columns", &columns);
        if (changed) {
            rows = std::clamp(rows, 1, 8);
            columns = std::clamp(columns, 1, MaxCells / rows);
            const auto key = std::to_string(kind) + "." + page.key;
            CVarSetInteger(Setting(key, "Rows").c_str(), rows);
            CVarSetInteger(Setting(key, "Columns").c_str(), columns);
            CVarSetInteger(Setting(std::to_string(kind), "Edited").c_str(), 1);
            for (const auto& entry : entries) {
                if (entry.placement.page == page.key && entry.placement.cell >= rows * columns) {
                    SetPlacement(entry, { page.key, rows * columns - 1 });
                }
            }
            SaveLayout();
        }
    } else {
        ImGui::Text("%d rows x %d columns (game / mod template)", page.rows, page.columns);
    }
    ImGui::TextUnformatted("In pause: A opens/closes a cell wheel; stick selects an entry; C buttons equip.");
    DrawGrid(page, entries);
    ImGui::PopID();
}

void ModLayout_DrawWindowButton() {
    if (ModLayout_HasMods() && ImGui::Button("Mods Items Layout")) {
        auto window = Ship::Context::GetInstance()->GetWindow()->GetGui()->GetGuiWindow("Mods Items Layout");
        if (window != nullptr) {
            window->ToggleVisibility();
        }
    }
}

void ModLayout_Init(void) {
    auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
    gui->AddGuiWindow(std::make_shared<LayoutWindow>(WindowCvar, "Mods Items Layout", ImVec2(760, 640)));
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoResolveName>(ResolveName);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() {
        if (sPreviewingSong &&
            (gPlayState == nullptr || gPlayState->pauseCtx.state != 6 ||
             gPlayState->pauseCtx.pageIndex != PAUSE_QUEST || gPlayState->pauseCtx.cursorSpecialPos != 0)) {
            Audio_OcaSetInstrument(0);
            sPreviewingSong = false;
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) {
        sShown.fill(NativePage);
        sCursor.fill(0);
        sCycling.fill(false);
        sActive.fill(false);
        sSelected.clear();
        sAnimatingItem.clear();
    });
}
