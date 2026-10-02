#include "CustomItemRegistry.h"
#include "soh/ModApi/Layout/ModLayout.h"

#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/SohGui/SohMenu.h"
#include <array>
#include <deque>
#include <algorithm>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "src/overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h"
extern PlayState* gPlayState;
extern s8 gCurrentItemCyclingSlot;
}

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;
}

namespace {

#define CVAR_CUSTOM_ITEM_MANAGER(name) CVAR_ENHANCEMENT("CustomItemManager." name)

struct PoolEntry {
    uint16_t vanillaItem;
    std::string customKey;
};

struct ItemPool {
    std::string key;
    std::vector<PoolEntry> entries;
};

std::deque<ItemPool> sPools;

const ItemPool* FindPool(const char* key) {
    if (key == nullptr) {
        return nullptr;
    }
    for (const auto& pool : sPools) {
        if (pool.key == key) {
            return &pool;
        }
    }
    return nullptr;
}

std::string PlacementCvar(const char* key, const char* field) {
    std::string result = CVAR_CUSTOM_ITEM_MANAGER("Layout.");
    constexpr char digits[] = "0123456789abcdef";
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(key); *cursor != 0; ++cursor) {
        result += digits[*cursor >> 4];
        result += digits[*cursor & 15];
    }
    return result + "." + field;
}

std::string VanillaCellCvar(uint8_t slot) {
    return std::string(CVAR_CUSTOM_ITEM_MANAGER("Layout.vanilla.")) + std::to_string(slot) + ".Cell";
}

void DrawLayoutEditor(SaveContext*) {
    if (!ImGui::BeginTabItem("Item Layout")) {
        return;
    }
    ImGui::TextUnformatted("Page 0 is the first extra item page; slots are 1-24. Items sharing a slot form a wheel.");
    for (uint32_t index = 0; index < CustomItemRegistry_GetCount(); ++index) {
        const auto* definition = CustomItemRegistry_GetAt(index);
        if (definition == nullptr || !CustomItemRegistry_IsEditorVisible(definition->key) ||
            CustomItemRegistry_IsVanillaUpgrade(definition)) {
            continue;
        }
        SOHCustomItemPlacement placement;
        KaleidoItemManager_GetPlacement(definition->key, &placement);
        ImGui::PushID(definition->key);
        ImGui::TextUnformatted(definition->key);
        int page = placement.page;
        int slot = placement.slot + 1;
        bool changed = ImGui::InputInt("Page", &page);
        changed |= ImGui::SliderInt("Slot", &slot, 1, 24);
        if (changed && page >= 0 && page <= UINT16_MAX) {
            placement = { static_cast<uint16_t>(page), static_cast<uint8_t>(slot - 1) };
            KaleidoItemManager_SetPlacement(definition->key, &placement);
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
        ImGui::PopID();
    }
    if (ImGui::TreeNode("Ocarina of Time items")) {
        ImGui::TextUnformatted("Which cell of the vanilla grid each vanilla slot is drawn in. The inventory itself is "
                               "not touched.");
        for (uint8_t slot = 0; slot < 24; ++slot) {
            ImGui::PushID(slot);
            int cell = CVarGetInteger(VanillaCellCvar(slot).c_str(), slot) + 1;
            if (ImGui::SliderInt(std::to_string(slot + 1).c_str(), &cell, 1, 24)) {
                CVarSetInteger(VanillaCellCvar(slot).c_str(), cell - 1);
                Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    for (const auto& pool : sPools) {
        if (ImGui::TreeNode(pool.key.c_str())) {
            for (const auto& entry : pool.entries) {
                if (entry.customKey.empty()) {
                    ImGui::Text("Vanilla item: %u", entry.vanillaItem);
                } else {
                    if (CustomItemRegistry_IsEditorVisible(entry.customKey.c_str())) {
                        ImGui::TextUnformatted(entry.customKey.c_str());
                    }
                }
            }
            ImGui::TreePop();
        }
    }
    ImGui::EndTabItem();
}

std::string sDescribedItem;

const SOHCustomItemDefinition* GetCursorItemDefinition(PlayState* play) {
    uint16_t cursorItem = play->pauseCtx.cursorItem[PAUSE_ITEM];
    if (cursorItem == ITEM_CUSTOM) {
        return CustomItemRegistry_GetPauseItem();
    }
    return CustomItemRegistry_FindReplacement(cursorItem);
}

bool IsCUpOnItemPage(PlayState* play, Input* input) {
    return CHECK_BTN_ALL(input->press.button, BTN_CUP) && play->pauseCtx.pageIndex == PAUSE_ITEM &&
           play->pauseCtx.cursorSpecialPos == 0;
}

bool IsCUpEquipModifier() {
    return CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) && CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0);
}

void OpenPauseTextbox(PlayState* play, uint16_t textId) {
    Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    Message_StartTextbox(play, textId, nullptr);
    play->pauseCtx.unk_1E4 = 10;
}

void HandleCUpOnItem(PlayState* play, Input* input, bool* handled) {
    if (*handled || IsCUpEquipModifier() || !IsCUpOnItemPage(play, input)) {
        return;
    }
    const SOHCustomItemDefinition* definition = GetCursorItemDefinition(play);
    if (definition == nullptr) {
        return;
    }
    if (definition->pauseText != nullptr) {
        sDescribedItem = definition->key;
        OpenPauseTextbox(play, TEXT_MOD_ITEM_PAUSE_DESCRIPTION);
        *handled = true;
        return;
    }
    if (!CVarGetInteger(CVAR_CUSTOM_ITEM_MANAGER("RepeatGetItemMessage"), 0) || definition->getItemEntry.textId == 0) {
        return;
    }
    CustomItemRegistry_PreparePauseMessage();
    OpenPauseTextbox(play, definition->getItemEntry.textId);
    *handled = true;
}

void BuildPauseDescription(uint16_t* textId, bool* loadFromMessageTable) {
    const SOHCustomItemDefinition* definition = CustomItemRegistry_Find(sDescribedItem.c_str());
    sDescribedItem.clear();
    if (definition == nullptr || definition->pauseText == nullptr) {
        return;
    }
    CustomMessage message(definition->pauseText);
    message.AutoFormat();
    message.LoadIntoFont();
    *loadFromMessageTable = false;
}

constexpr uint8_t SlotCount = 24;
constexpr uint8_t ButtonCount = 8;
constexpr uint8_t GridColumns = 6;

using WheelItems = std::vector<const SOHCustomItemDefinition*>;

struct ItemPage {
    uint16_t page;
    std::array<WheelItems, SlotCount> slots;
};

std::vector<ItemPage> sVisiblePages;
ItemPage sVanillaClaims = { SOH_ITEM_PAGE_VANILLA, {} };
std::array<uint8_t, SlotCount> sCellToVanillaSlot;
std::optional<uint32_t> sLayoutFrame;
std::optional<uint16_t> sShownPage;
std::map<std::pair<uint16_t, uint8_t>, std::string> sWheelSelection;

void PlayPauseSfx(uint16_t sfxId) {
    Audio_PlaySoundGeneral(sfxId, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

const ItemPage* FindVisiblePage(uint16_t page) {
    for (const auto& visible : sVisiblePages) {
        if (visible.page == page) {
            return &visible;
        }
    }
    return nullptr;
}

void BuildVanillaCells() {
    sCellToVanillaSlot.fill(UINT8_MAX);
    std::array<uint8_t, SlotCount> wanted;
    for (uint8_t slot = 0; slot < SlotCount; ++slot) {
        int cell = CVarGetInteger(VanillaCellCvar(slot).c_str(), slot);
        wanted[slot] = cell >= 0 && cell < SlotCount ? static_cast<uint8_t>(cell) : slot;
    }
    for (uint8_t slot = 0; slot < SlotCount; ++slot) {
        if (sCellToVanillaSlot[wanted[slot]] == UINT8_MAX) {
            sCellToVanillaSlot[wanted[slot]] = slot;
        } else if (sCellToVanillaSlot[slot] == UINT8_MAX) {
            sCellToVanillaSlot[slot] = slot;
        }
    }
}

void BuildVisiblePages() {
    BuildVanillaCells();
    std::map<uint16_t, std::array<WheelItems, SlotCount>> pages;
    for (uint32_t index = 0; index < CustomItemRegistry_GetCount(); ++index) {
        const SOHCustomItemDefinition* definition = CustomItemRegistry_GetAt(index);
        SOHCustomItemPlacement placement;
        if (definition == nullptr || CustomItemRegistry_IsVanillaUpgrade(definition) ||
            !CustomItemRegistry_IsOwned(definition->key) ||
            !KaleidoItemManager_GetPlacement(definition->key, &placement)) {
            continue;
        }
        if (CustomItemRegistry_IsQuestPage(placement.page)) {
            continue;
        }
        pages[placement.page][placement.slot].push_back(definition);
    }

    auto sortByPriority = [](std::array<WheelItems, SlotCount>& slots) {
        for (auto& items : slots) {
            std::stable_sort(items.begin(), items.end(),
                             [](const SOHCustomItemDefinition* a, const SOHCustomItemDefinition* b) {
                                 return a->priority > b->priority;
                             });
        }
    };

    for (auto& items : sVanillaClaims.slots) {
        items.clear();
    }
    auto claims = pages.find(SOH_ITEM_PAGE_VANILLA);
    if (claims != pages.end()) {
        sVanillaClaims.slots = std::move(claims->second);
        sortByPriority(sVanillaClaims.slots);
        pages.erase(claims);
    }

    sVisiblePages.clear();
    for (auto& [page, slots] : pages) {
        sortByPriority(slots);
        sVisiblePages.push_back({ page, std::move(slots) });
    }
    if (sShownPage.has_value() && FindVisiblePage(*sShownPage) == nullptr) {
        sShownPage.reset();
    }
}

void RefreshVisiblePages() {
    uint32_t frame = gPlayState != nullptr ? gPlayState->state.frames : 0;
    if (sLayoutFrame == frame) {
        return;
    }
    sLayoutFrame = frame;
    BuildVisiblePages();
}

const ItemPage* GetShownPage() {
    RefreshVisiblePages();
    return sShownPage.has_value() ? FindVisiblePage(*sShownPage) : nullptr;
}

const SOHCustomItemDefinition* GetWheelSelection(const ItemPage& page, uint8_t slot) {
    const WheelItems& items = page.slots[slot];
    if (items.empty()) {
        return nullptr;
    }
    const auto selection = sWheelSelection.find({ page.page, slot });
    if (selection != sWheelSelection.end()) {
        for (const auto* item : items) {
            if (selection->second == item->key) {
                return item;
            }
        }
    }
    return items.front();
}

const ItemPage* GetShownOrVanillaPage() {
    const ItemPage* page = GetShownPage();
    return page != nullptr ? page : &sVanillaClaims;
}

const SOHCustomItemDefinition* GetShownSlotItem(uint8_t slot) {
    return slot < SlotCount ? GetWheelSelection(*GetShownOrVanillaPage(), slot) : nullptr;
}

void ShowPauseItem(PlayState* play, const SOHCustomItemDefinition* definition) {
    if (definition == nullptr || definition == CustomItemRegistry_GetPauseItem()) {
        return;
    }
    CustomItemRegistry_SelectPauseItem(definition->key);
    play->pauseCtx.namedItem = PAUSE_ITEM_NONE;
}

void SelectCursorItem(PlayState* play, uint16_t* item, uint16_t* slot) {
    if (*item != ITEM_CUSTOM) {
        return;
    }
    ShowPauseItem(play, GetShownSlotItem(static_cast<uint8_t>(*slot)));
    *slot = SLOT_CUSTOM;
}

void EquipCursorItem(PlayState* play, uint8_t button, uint16_t item, bool* handled) {
    if (item != ITEM_CUSTOM) {
        return;
    }
    const SOHCustomItemDefinition* definition = CustomItemRegistry_GetPauseItem();
    const int cell = std::clamp<int>(play->pauseCtx.cursorPoint[PAUSE_ITEM], 0, SlotCount - 1);
    const Vtx* vertex = &play->pauseCtx.itemVtx[cell * 4];
    bool equipped = definition != nullptr && ModLayout_BeginCustomEquip(play, definition->key, button,
                                                                        vertex->v.ob[0] * 10, vertex->v.ob[1] * 10);
    if (!equipped) {
        PlayPauseSfx(NA_SE_SY_ERROR);
    }
    *handled = true;
}

void MoveCursorToFirstItem(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    for (uint8_t slot = 0; slot < SlotCount; ++slot) {
        uint16_t item = KaleidoItemManager_GetSlotItem(slot);
        if (item == ITEM_NONE) {
            continue;
        }
        pauseCtx->cursorPoint[PAUSE_ITEM] = slot;
        pauseCtx->cursorX[PAUSE_ITEM] = slot % GridColumns;
        pauseCtx->cursorY[PAUSE_ITEM] = slot / GridColumns;
        pauseCtx->cursorSlot[PAUSE_ITEM] = slot;
        pauseCtx->cursorItem[PAUSE_ITEM] = item;
        pauseCtx->namedItem = PAUSE_ITEM_NONE;
        pauseCtx->nameDisplayTimer = 0;
        return;
    }
    pauseCtx->cursorItem[PAUSE_ITEM] = PAUSE_ITEM_NONE;
}

void EquipOnSameButtons(const char* previousKey, const char* nextKey) {
    for (uint8_t button = 0; button < ButtonCount; ++button) {
        const char* equipped = CustomItemRegistry_GetEquippedKey(button);
        if (equipped == nullptr || std::strcmp(equipped, previousKey) != 0) {
            continue;
        }
        if (!CustomItemRegistry_Equip(button, nextKey)) {
            CustomItemRegistry_Unequip(button);
        }
    }
}

int32_t ReadWheelStep(PauseContext* pauseCtx, Input* input) {
    bool dpad = CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) && !CHECK_BTN_ALL(input->cur.button, BTN_CUP);
    if (pauseCtx->stickRelX > 30 || pauseCtx->stickRelY > 30 ||
        (dpad && CHECK_BTN_ANY(input->press.button, BTN_DRIGHT | BTN_DUP))) {
        return 1;
    }
    if (pauseCtx->stickRelX < -30 || pauseCtx->stickRelY < -30 ||
        (dpad && CHECK_BTN_ANY(input->press.button, BTN_DLEFT | BTN_DDOWN))) {
        return -1;
    }
    return 0;
}

void TurnWheel(PlayState* play, const ItemPage& page, uint8_t slot, int32_t step) {
    const WheelItems& items = page.slots[slot];
    const SOHCustomItemDefinition* previous = GetWheelSelection(page, slot);
    const auto position = std::find(items.begin(), items.end(), previous) - items.begin();
    const auto count = static_cast<std::ptrdiff_t>(items.size());
    const SOHCustomItemDefinition* next = items[(position + step + count) % count];
    sWheelSelection[{ page.page, slot }] = next->key;
    EquipOnSameButtons(previous->key, next->key);
    ShowPauseItem(play, next);
}

void* ResolveInventoryIcon(const SOHCustomItemDefinition* definition) {
    const char* icon =
        definition != nullptr ? CustomItemRegistry_ResolveTexture(definition->key, SOH_ITEM_ICON_INVENTORY) : nullptr;
    return icon != nullptr ? static_cast<void*>(const_cast<char*>(icon)) : gItemIcons[ITEM_SOLD_OUT];
}

KaleidoCycleIcon GetWheelIcon(const SOHCustomItemDefinition* definition) {
    return { ResolveInventoryIcon(definition), CustomItemRegistry_IsAgeAllowed(definition->key) };
}

void ResetShownPage() {
    sShownPage.reset();
    sLayoutFrame.reset();
    sWheelSelection.clear();
}

void RegisterHooks() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoItemCursor>(SelectCursorItem);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoItemEquip>(EquipCursorItem);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) { ResetShownPage(); });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoInput>(HandleCUpOnItem);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(TEXT_MOD_ITEM_PAUSE_DESCRIPTION,
                                                                                BuildPauseDescription);
}

void RegisterMenu() {
    WidgetPath path = { "Enhancements", "Quality of Life", SECTION_COLUMN_2 };
    if (!SohGui::mSohMenu->HasModWidgetPath(path)) {
        SPDLOG_ERROR("[CustomItemManager] Menu destination unavailable: {}/{} column {}", path.sectionName,
                     path.sidebarName, static_cast<int>(path.column));
        return;
    }
    SohGui::mSohMenu->AddWidget(path, "Repeat custom get-item message with C-Up", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_CUSTOM_ITEM_MANAGER("RepeatGetItemMessage"))
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Press C-Up over a custom item to replay its get-item textbox without granting it again."));
    SohGui::mSohMenu->AddWidget(path, "Mods Items Layout", WIDGET_CUSTOM)
        .PreFunc([](WidgetInfo& info) { info.isHidden = !ModLayout_HasMods(); })
        .CustomFunction([](WidgetInfo&) { ModLayout_DrawWindowButton(); });
}

RegisterMenuInitFunc sMenuInit(RegisterMenu);

} // namespace

void KaleidoItemManager_Init() {
    RegisterHooks();
}

bool KaleidoItemManager_RegisterPool(const char* key, const SOHItemPoolEntry* entries, uint32_t count) {
    if (key == nullptr || key[0] == '\0' || FindPool(key) != nullptr || (count != 0 && entries == nullptr)) {
        return false;
    }
    ItemPool pool;
    pool.key = key;
    for (uint32_t index = 0; index < count; ++index) {
        const auto& entry = entries[index];
        if (entry.customKey != nullptr && entry.customKey[0] != '\0') {
            pool.entries.push_back({ ITEM_NONE, entry.customKey });
        } else if (entry.vanillaItem < ITEM_CUSTOM) {
            pool.entries.push_back({ entry.vanillaItem, "" });
        } else {
            return false;
        }
    }
    sPools.push_back(std::move(pool));
    return true;
}

uint32_t KaleidoItemManager_GetPoolCount(void) {
    return static_cast<uint32_t>(sPools.size());
}

const char* KaleidoItemManager_GetPoolKey(uint32_t index) {
    return index < sPools.size() ? sPools[index].key.c_str() : nullptr;
}

uint32_t KaleidoItemManager_GetPoolSize(const char* key) {
    const auto* pool = FindPool(key);
    return pool == nullptr ? 0 : static_cast<uint32_t>(pool->entries.size());
}

bool KaleidoItemManager_GetPoolEntry(const char* key, uint32_t index, SOHItemPoolEntry* entry) {
    const auto* pool = FindPool(key);
    if (pool == nullptr || entry == nullptr || index >= pool->entries.size()) {
        return false;
    }
    const auto& stored = pool->entries[index];
    *entry = { stored.vanillaItem, stored.customKey.empty() ? nullptr : stored.customKey.c_str() };
    return true;
}

bool KaleidoItemManager_GetPlacement(const char* key, SOHCustomItemPlacement* placement) {
    const auto* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr || placement == nullptr ||
        (definition->presentationFlags & SOH_ITEM_HIDE_FROM_KALEIDO) != 0) {
        return false;
    }
    int page = CVarGetInteger(PlacementCvar(key, "Page").c_str(), definition->preferredPage);
    int slot = CVarGetInteger(PlacementCvar(key, "Slot").c_str(), definition->preferredSlot);
    placement->page = static_cast<uint16_t>(std::clamp(page, 0, static_cast<int>(UINT16_MAX)));
    placement->slot = static_cast<uint8_t>(std::clamp(slot, 0, 23));
    return true;
}

uint32_t KaleidoItemManager_GetSlotItemCount(uint16_t page, uint8_t slot) {
    RefreshVisiblePages();
    const ItemPage* visible = FindVisiblePage(page);
    return visible != nullptr && slot < SlotCount ? static_cast<uint32_t>(visible->slots[slot].size()) : 0;
}

const char* KaleidoItemManager_GetSlotItemAt(uint16_t page, uint8_t slot, uint32_t index) {
    if (index >= KaleidoItemManager_GetSlotItemCount(page, slot)) {
        return nullptr;
    }
    return FindVisiblePage(page)->slots[slot][index]->key;
}

bool KaleidoItemManager_IsVanillaPageShown(void) {
    return GetShownPage() == nullptr;
}

uint16_t KaleidoItemManager_GetSlotItem(uint8_t slot) {
    if (slot >= SlotCount) {
        return ITEM_NONE;
    }
    if (GetShownSlotItem(slot) != nullptr) {
        return ITEM_CUSTOM;
    }
    if (!KaleidoItemManager_IsVanillaPageShown()) {
        return ITEM_NONE;
    }
    const uint8_t vanillaSlot = KaleidoItemManager_GetVanillaSlot(slot);
    return vanillaSlot < SlotCount ? gSaveContext.inventory.items[vanillaSlot] : ITEM_NONE;
}

uint8_t KaleidoItemManager_GetVanillaSlot(uint8_t cell) {
    if (cell >= SlotCount) {
        return cell;
    }
    RefreshVisiblePages();
    return sCellToVanillaSlot[cell];
}

void* KaleidoItemManager_GetSlotIcon(uint8_t slot) {
    return ResolveInventoryIcon(GetShownSlotItem(slot));
}

bool KaleidoItemManager_IsSlotAgeAllowed(uint8_t slot) {
    const SOHCustomItemDefinition* definition = GetShownSlotItem(slot);
    return definition != nullptr && CustomItemRegistry_IsAgeAllowed(definition->key);
}

bool KaleidoItemManager_CycleItemPage(PlayState* play) {
    RefreshVisiblePages();
    if (play == nullptr || sVisiblePages.empty() || gCurrentItemCyclingSlot != -1) {
        return false;
    }
    auto next = sVisiblePages.begin();
    if (sShownPage.has_value()) {
        next = std::find_if(sVisiblePages.begin(), sVisiblePages.end(),
                            [](const ItemPage& visible) { return visible.page == *sShownPage; });
        next = next == sVisiblePages.end() ? sVisiblePages.end() : next + 1;
    }
    if (next == sVisiblePages.end()) {
        sShownPage.reset();
    } else {
        sShownPage = next->page;
    }
    KaleidoScope_ResetItemCycling();
    PlayPauseSfx(NA_SE_SY_WIN_SCROLL_RIGHT);
    if (play->pauseCtx.cursorSpecialPos == 0) {
        MoveCursorToFirstItem(play);
    }
    return true;
}

void KaleidoItemManager_HandleWheel(PlayState* play) {
    const ItemPage* page = GetShownOrVanillaPage();
    PauseContext* pauseCtx = &play->pauseCtx;
    uint8_t slot = static_cast<uint8_t>(pauseCtx->cursorPoint[PAUSE_ITEM]);
    if (page == nullptr || slot >= SlotCount || page->slots[slot].size() < 2) {
        return;
    }
    Input* input = &play->state.input[0];
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        PlayPauseSfx(NA_SE_SY_DECIDE);
        gCurrentItemCyclingSlot = gCurrentItemCyclingSlot == slot ? -1 : slot;
    }
    if (gCurrentItemCyclingSlot != slot) {
        return;
    }
    pauseCtx->cursorColorSet = 8;
    int32_t step = ReadWheelStep(pauseCtx, input);
    if (step == 0) {
        return;
    }
    PlayPauseSfx(NA_SE_SY_CURSOR);
    TurnWheel(play, *page, slot, step);
}

void KaleidoItemManager_DrawWheels(PlayState* play) {
    const ItemPage* page = GetShownOrVanillaPage();
    if (page == nullptr) {
        return;
    }
    const KaleidoCycleIcon hidden = { nullptr, true };
    for (uint8_t slot = 0; slot < SlotCount; ++slot) {
        const WheelItems& items = page->slots[slot];
        if (items.size() < 2) {
            KaleidoScope_DrawCycleIcons(play, slot, hidden, hidden);
            continue;
        }
        const auto count = static_cast<std::ptrdiff_t>(items.size());
        const auto position = std::find(items.begin(), items.end(), GetWheelSelection(*page, slot)) - items.begin();
        const SOHCustomItemDefinition* left = items[(position + count - 1) % count];
        const SOHCustomItemDefinition* right = items[(position + 1) % count];
        KaleidoScope_DrawCycleIcons(play, slot, GetWheelIcon(left), left == right ? hidden : GetWheelIcon(right));
    }
}

bool KaleidoItemManager_SetPlacement(const char* key, const SOHCustomItemPlacement* placement) {
    if (placement == nullptr || placement->slot >= 24 || CustomItemRegistry_Find(key) == nullptr) {
        return false;
    }
    CVarSetInteger(PlacementCvar(key, "Page").c_str(), placement->page);
    CVarSetInteger(PlacementCvar(key, "Slot").c_str(), placement->slot);
    return true;
}
