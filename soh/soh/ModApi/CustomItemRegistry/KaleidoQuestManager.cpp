#include "CustomItemRegistry.h"
#include "soh/ModApi/Layout/ModLayout.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
#include "src/overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h"
extern SaveContext gSaveContext;
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

namespace {

constexpr uint8_t QuestPointCount = 0x18;
constexpr uint8_t FirstCButtonIndex = 1;
constexpr uint8_t CButtonCount = 3;

std::optional<uint16_t> sShownExtraPage;

uint16_t ShownPage() {
    return sShownExtraPage.value_or(SOH_ITEM_PAGE_QUEST_VANILLA);
}

const SOHCustomItemDefinition* FindClaimOnPage(uint16_t page, uint8_t point) {
    if (point >= QuestPointCount) {
        return nullptr;
    }
    for (uint32_t index = 0; index < CustomItemRegistry_GetCount(); ++index) {
        const SOHCustomItemDefinition* definition = CustomItemRegistry_GetAt(index);
        SOHCustomItemPlacement placement;
        if (definition == nullptr || !CustomItemRegistry_IsOwned(definition->key) ||
            !KaleidoItemManager_GetPlacement(definition->key, &placement)) {
            continue;
        }
        if (placement.page == page && placement.slot == point) {
            return definition;
        }
    }
    return nullptr;
}

std::vector<uint16_t> VisibleExtraPages() {
    std::vector<uint16_t> pages;
    for (uint32_t index = 0; index < CustomItemRegistry_GetCount(); ++index) {
        const SOHCustomItemDefinition* definition = CustomItemRegistry_GetAt(index);
        SOHCustomItemPlacement placement;
        if (definition == nullptr || !CustomItemRegistry_IsOwned(definition->key) ||
            !KaleidoItemManager_GetPlacement(definition->key, &placement)) {
            continue;
        }
        if (!CustomItemRegistry_IsQuestPage(placement.page) || placement.page == SOH_ITEM_PAGE_QUEST_VANILLA) {
            continue;
        }
        if (std::find(pages.begin(), pages.end(), placement.page) == pages.end()) {
            pages.push_back(placement.page);
        }
    }
    std::sort(pages.begin(), pages.end());
    return pages;
}

void PlayPauseSfx(uint16_t sfxId) {
    Audio_PlaySoundGeneral(sfxId, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

void ResolvePointName(PlayState* play, uint16_t item, const char** namePath) {
    if (item != ITEM_CUSTOM || play->pauseCtx.pageIndex != PAUSE_QUEST) {
        return;
    }
    const SOHCustomItemDefinition* claim =
        FindClaimOnPage(ShownPage(), static_cast<uint8_t>(play->pauseCtx.cursorPoint[PAUSE_QUEST]));
    if (claim != nullptr && claim->namePath != nullptr) {
        *namePath = claim->namePath;
    }
}

bool IsDrawnByVanilla(uint8_t point) {
    return KaleidoQuestManager_IsVanillaPageShown() && CHECK_QUEST_ITEM(point);
}

} // namespace

bool KaleidoQuestManager_IsVanillaPageShown(void) {
    if (sShownExtraPage.has_value()) {
        const std::vector<uint16_t> pages = VisibleExtraPages();
        if (std::find(pages.begin(), pages.end(), *sShownExtraPage) == pages.end()) {
            sShownExtraPage.reset();
        }
    }
    return !sShownExtraPage.has_value();
}

bool KaleidoQuestManager_IsPointClaimed(uint8_t point) {
    return FindClaimOnPage(ShownPage(), point) != nullptr;
}

bool KaleidoQuestManager_CycleQuestPage(PlayState* play) {
    const std::vector<uint16_t> pages = VisibleExtraPages();
    if (play == nullptr || pages.empty()) {
        return false;
    }

    auto next = pages.begin();
    if (sShownExtraPage.has_value()) {
        next = std::find(pages.begin(), pages.end(), *sShownExtraPage);
        next = next == pages.end() ? pages.end() : next + 1;
    }
    sShownExtraPage = next == pages.end() ? std::optional<uint16_t>() : std::optional<uint16_t>(*next);
    PlayPauseSfx(NA_SE_SY_WIN_SCROLL_RIGHT);
    return true;
}

bool KaleidoQuestManager_EquipPoint(PlayState* play) {
    Input* input = &play->state.input[0];
    static const uint16_t buttons[] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

    const SOHCustomItemDefinition* claim =
        FindClaimOnPage(ShownPage(), static_cast<uint8_t>(play->pauseCtx.cursorPoint[PAUSE_QUEST]));
    if (claim == nullptr || play->pauseCtx.cursorSpecialPos != 0) {
        return false;
    }

    const bool dpadAllowed = CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) != 0;
    for (uint8_t index = 0; index < ARRAY_COUNT(buttons); ++index) {
        if (!CHECK_BTN_ALL(input->press.button, buttons[index]) || (index >= CButtonCount && !dpadAllowed)) {
            continue;
        }
        const int point = play->pauseCtx.cursorPoint[PAUSE_QUEST];
        const Vtx* vertex = &play->pauseCtx.questVtx[point * 4];
        const bool equipped = ModLayout_BeginCustomEquip(play, claim->key, FirstCButtonIndex + index,
                                                         vertex->v.ob[0] * 10, vertex->v.ob[1] * 10);
        if (!equipped) {
            PlayPauseSfx(NA_SE_SY_ERROR);
        }
        return true;
    }
    return false;
}

void KaleidoQuestManager_DrawPoints(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    PauseContext* pauseCtx = &play->pauseCtx;

    OPEN_DISPS(gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);

    for (uint8_t point = 0; point < QuestPointCount; ++point) {
        const SOHCustomItemDefinition* claim = FindClaimOnPage(ShownPage(), point);
        if (claim == nullptr || claim->iconPath == nullptr || IsDrawnByVanilla(point)) {
            continue;
        }
        const char* icon = CustomItemRegistry_ResolveTexture(claim->key, SOH_ITEM_ICON_INVENTORY);
        if (icon == nullptr) {
            continue;
        }
        gSPVertex(POLY_OPA_DISP++, (uintptr_t)&pauseCtx->questVtx[point * 4], 4, 0);
        KaleidoScope_DrawQuadTextureRGBA32(gfxCtx, const_cast<char*>(icon), 24, 24, 0);
    }

    CLOSE_DISPS(gfxCtx);
}

void KaleidoQuestManager_Init(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoResolveName>(ResolvePointName);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) { sShownExtraPage.reset(); });
}
