#include "CustomEquipRegistry.h"

#include <optional>
#include <string>
#include <vector>

#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
extern SaveContext gSaveContext;
}

namespace {

constexpr uint8_t RowCount = 4;
constexpr uint8_t LevelColumnCount = 3;
constexpr uint8_t PassiveColumn = 0;

std::optional<uint8_t> sShownPage;

bool IsOwnedOnPage(const SOHCustomEquipDefinition* definition, uint8_t page) {
    return definition != nullptr && definition->page == page && CustomEquipRegistry_IsOwned(definition->key);
}

uint8_t ShownPage() {
    return sShownPage.value_or(SOH_EQUIP_PAGE_VANILLA);
}

const SOHCustomEquipDefinition* FindClaimOnPage(uint8_t page, uint8_t row, uint8_t column) {
    if (row >= RowCount || column > LevelColumnCount) {
        return nullptr;
    }
    for (uint32_t index = 0; index < CustomEquipRegistry_GetCount(); ++index) {
        const SOHCustomEquipDefinition* definition = CustomEquipRegistry_GetAt(index);
        if (!IsOwnedOnPage(definition, page)) {
            continue;
        }
        const bool matches = column == PassiveColumn
                                 ? definition->slot == SOH_EQUIP_SLOT_UPGRADE && definition->row == row
                                 : definition->slot == row && definition->column == column;
        if (matches) {
            return definition;
        }
    }
    return nullptr;
}

const SOHCustomEquipDefinition* FindClaim(uint8_t row, uint8_t column) {
    const bool isVanillaUpgradeColumn = ShownPage() == SOH_EQUIP_PAGE_VANILLA && column == PassiveColumn;
    if (isVanillaUpgradeColumn) {
        return nullptr;
    }
    return FindClaimOnPage(ShownPage(), row, column);
}

std::vector<uint8_t> VisiblePages() {
    std::vector<uint8_t> pages;
    for (uint32_t index = 0; index < CustomEquipRegistry_GetCount(); ++index) {
        const SOHCustomEquipDefinition* definition = CustomEquipRegistry_GetAt(index);
        if (definition == nullptr || definition->page == SOH_EQUIP_PAGE_VANILLA ||
            !CustomEquipRegistry_IsOwned(definition->key)) {
            continue;
        }
        if (std::find(pages.begin(), pages.end(), definition->page) == pages.end()) {
            pages.push_back(definition->page);
        }
    }
    std::sort(pages.begin(), pages.end());
    return pages;
}

void MoveCursorToFirstPiece(PlayState* play) {
    for (uint8_t row = 0; row < RowCount; ++row) {
        for (uint8_t column = 0; column <= LevelColumnCount; ++column) {
            if (FindClaim(row, column) == nullptr) {
                continue;
            }
            play->pauseCtx.cursorX[PAUSE_EQUIP] = column;
            play->pauseCtx.cursorY[PAUSE_EQUIP] = row;
            play->pauseCtx.cursorPoint[PAUSE_EQUIP] = column + row * 4;
            return;
        }
    }
}

void ResolveCellName(PlayState* play, uint16_t item, const char** namePath) {
    if (item != ITEM_CUSTOM || play->pauseCtx.pageIndex != PAUSE_EQUIP) {
        return;
    }
    const SOHCustomEquipDefinition* claim = FindClaim(static_cast<uint8_t>(play->pauseCtx.cursorY[PAUSE_EQUIP]),
                                                      static_cast<uint8_t>(play->pauseCtx.cursorX[PAUSE_EQUIP]));
    if (claim != nullptr && claim->namePath != nullptr) {
        *namePath = claim->namePath;
    }
}

void PlayPauseSfx(uint16_t sfxId) {
    Audio_PlaySoundGeneral(sfxId, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

void DescribeCellOnCUp(PlayState* play, Input* input, bool* handled) {
    if (*handled || !CHECK_BTN_ALL(input->press.button, BTN_CUP) || play->pauseCtx.pageIndex != PAUSE_EQUIP ||
        play->pauseCtx.cursorSpecialPos != 0) {
        return;
    }
    const SOHCustomEquipDefinition* claim = FindClaim(static_cast<uint8_t>(play->pauseCtx.cursorY[PAUSE_EQUIP]),
                                                      static_cast<uint8_t>(play->pauseCtx.cursorX[PAUSE_EQUIP]));
    if (claim == nullptr || claim->pauseText == nullptr) {
        return;
    }
    PlayPauseSfx(NA_SE_SY_DECIDE);
    CustomItemRegistry_ShowTextbox(play, claim->pauseText, true);
    play->pauseCtx.unk_1E4 = 10;
    *handled = true;
}

} // namespace

bool KaleidoEquipManager_IsVanillaPageShown(void) {
    if (sShownPage.has_value()) {
        const std::vector<uint8_t> pages = VisiblePages();
        if (std::find(pages.begin(), pages.end(), *sShownPage) == pages.end()) {
            sShownPage.reset();
        }
    }
    return !sShownPage.has_value();
}

bool KaleidoEquipManager_IsCellClaimed(uint8_t row, uint8_t column) {
    return FindClaim(row, column) != nullptr;
}

void* KaleidoEquipManager_GetCellIcon(uint8_t row, uint8_t column) {
    const SOHCustomEquipDefinition* claim = FindClaim(row, column);
    if (claim == nullptr || claim->iconPath == nullptr) {
        return gItemIcons[ITEM_SOLD_OUT];
    }
    return static_cast<void*>(const_cast<char*>(claim->iconPath));
}

bool KaleidoEquipManager_IsCellAgeAllowed(uint8_t row, uint8_t column) {
    const SOHCustomEquipDefinition* claim = FindClaim(row, column);
    return claim != nullptr && CustomEquipRegistry_IsAgeAllowed(claim->key);
}

bool KaleidoEquipManager_IsCellEquipped(uint8_t row, uint8_t column) {
    const SOHCustomEquipDefinition* claim = FindClaim(row, column);
    if (claim == nullptr) {
        return false;
    }
    return claim->slot == SOH_EQUIP_SLOT_UPGRADE ? CustomEquipRegistry_IsToggleOn(claim->key)
                                                 : CustomEquipRegistry_IsWorn(claim->key);
}

bool KaleidoEquipManager_EquipCell(PlayState* play, uint8_t row, uint8_t column) {
    const SOHCustomEquipDefinition* claim = FindClaim(row, column);
    if (claim == nullptr) {
        return false;
    }
    if (!CustomEquipRegistry_IsAgeAllowed(claim->key)) {
        PlayPauseSfx(NA_SE_SY_ERROR);
        return true;
    }

    const bool wasOn = KaleidoEquipManager_IsCellEquipped(row, column);
    if (claim->slot == SOH_EQUIP_SLOT_UPGRADE) {
        CustomEquipRegistry_SetToggle(claim->key, !wasOn);
    } else if (wasOn) {
        CustomEquipRegistry_TakeOff(claim->slot);
    } else {
        CustomEquipRegistry_Wear(claim->key);
    }
    PlayPauseSfx(NA_SE_SY_DECIDE);
    return true;
}

bool KaleidoEquipManager_CycleEquipPage(PlayState* play) {
    const std::vector<uint8_t> pages = VisiblePages();
    if (play == nullptr || pages.empty()) {
        return false;
    }

    auto next = pages.begin();
    if (sShownPage.has_value()) {
        next = std::find(pages.begin(), pages.end(), *sShownPage);
        next = next == pages.end() ? pages.end() : next + 1;
    }
    if (next == pages.end()) {
        sShownPage.reset();
    } else {
        sShownPage = *next;
        MoveCursorToFirstPiece(play);
    }
    PlayPauseSfx(NA_SE_SY_WIN_SCROLL_RIGHT);
    return true;
}

void KaleidoEquipManager_Init(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoResolveName>(ResolveCellName);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoInput>(DescribeCellOnCUp);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) { sShownPage.reset(); });
}
