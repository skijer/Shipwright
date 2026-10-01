// Where the primed element shows and is picked, as in NEI: the button icon wears the element's medallion,
// A on the bow or slingshot cell cycles it with the stick, and holding C on the cell opens a ring of all of them.

#include "z64items.h"

#include "sw97_mod.h"
#include "overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h"

#define ITEM_CELL_COUNT 24
#define ITEM_ICON_SIZE 32
#define STICK_STEP_THRESHOLD 30
#define STICK_RELEASE_THRESHOLD 20
#define WHEEL_HOLD_FRAMES 20
#define WHEEL_MAX_ENTRIES 8
#define CYCLING_COLOR_SET 8
#define NO_CELL -1
#define ICON_CUSTOM(name) "__OTR__textures/icon_item_custom/" name
// Every row starts even, so no path reads as a segmented address.
#define ICON_PATH_SIZE 72

typedef struct {
    bool isOpen;
    uint8_t cursor;
    uint8_t holdFrames;
    bool isStickHeld;
    s8 heldCButton;
} RingState;

extern HOST_DATA s8 gCurrentItemCyclingSlot;

static const ALIGN_ASSET(2) char sBowIcon[] = "__OTR__textures/icon_item_static/gItemIconBowTex";
static const ALIGN_ASSET(2) char sSlingshotIcon[] = "__OTR__textures/icon_item_static/gItemIconSlingshotTex";

static const ALIGN_ASSET(2) char sElementIcons[SW97_ELEM_COUNT][ICON_PATH_SIZE] = {
    "",
    ICON_CUSTOM("gItemIconMedallionFireTex"),
    ICON_CUSTOM("gItemIconMedallionWaterTex"),
    ICON_CUSTOM("gItemIconMedallionLightTex"),
    ICON_CUSTOM("gItemIconMedallionShadowTex"),
    ICON_CUSTOM("gItemIconMedallionSpiritTex"),
    ICON_CUSTOM("gItemIconMedallionForestTex"),
};

static const ALIGN_ASSET(2) char sBowElementIcons[SW97_ELEM_COUNT][ICON_PATH_SIZE] = {
    "",
    ICON_CUSTOM("gItemIconBowSw97FireTex"),
    ICON_CUSTOM("gItemIconBowSw97IceTex"),
    ICON_CUSTOM("gItemIconBowSw97LightTex"),
    ICON_CUSTOM("gItemIconBowSw97DarkTex"),
    ICON_CUSTOM("gItemIconBowSw97SoulTex"),
    ICON_CUSTOM("gItemIconBowSw97WindTex"),
};

static const ALIGN_ASSET(2) char sSlingshotElementIcons[SW97_ELEM_COUNT][ICON_PATH_SIZE] = {
    "",
    ICON_CUSTOM("gItemIconSlingshotSw97FireTex"),
    ICON_CUSTOM("gItemIconSlingshotSw97IceTex"),
    ICON_CUSTOM("gItemIconSlingshotSw97LightTex"),
    ICON_CUSTOM("gItemIconSlingshotSw97DarkTex"),
    ICON_CUSTOM("gItemIconSlingshotSw97SoulTex"),
    ICON_CUSTOM("gItemIconSlingshotSw97WindTex"),
};

// NEI's ring: up to eight entries clockwise from the top of the cell.
static const s16 sRingOffsetX[WHEEL_MAX_ENTRIES] = { 0, 17, 24, 17, 0, -17, -24, -17 };
static const s16 sRingOffsetY[WHEEL_MAX_ENTRIES] = { -24, -17, 0, 17, 24, 17, 0, -17 };

static bool sIsSelectorOpen;
static RingState sRing = { false, 0, 0, false, -1 };
static s8 sWeaponCell = NO_CELL;

static bool IsWeapon(uint16_t item) {
    return item == ITEM_BOW || item == ITEM_SLINGSHOT;
}

static const char* EntryIcon(bool isSling, Sw97Element element) {
    if (element == SW97_ELEM_NONE) {
        return isSling ? sSlingshotIcon : sBowIcon;
    }
    return sElementIcons[element];
}

static uint8_t CurrentEntryIndex(bool isSling) {
    Sw97Element element = Sw97_GetElement(isSling);
    uint8_t count = Sw97_ElementCount();

    for (uint8_t i = 0; i < count; i++) {
        if (Sw97_ElementAt(i) == element) {
            return i;
        }
    }
    return 0;
}

static void PlayMenuSfx(u16 sfxId) {
    Audio_PlaySoundGeneral(sfxId, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static void ResolveElementButtonIcon(PlayState* play, uint8_t button, uint16_t item, const char** iconPath) {
    bool isSling = item == ITEM_SLINGSHOT;
    Sw97Element element;

    if (!IsWeapon(item)) {
        return;
    }
    element = Sw97_GetElement(isSling);
    if (element != SW97_ELEM_NONE) {
        *iconPath = isSling ? sSlingshotElementIcons[element] : sBowElementIcons[element];
    }
}

static bool IsCursorInputLive(PauseContext* pauseCtx) {
    return pauseCtx->debugState == 0 && pauseCtx->state == 6 && pauseCtx->unk_1E4 == 0;
}

static void CloseSelector(void) {
    if (!sIsSelectorOpen) {
        return;
    }
    sIsSelectorOpen = false;
    gCurrentItemCyclingSlot = -1;
}

static void StepSelector(bool isSling, s32 direction) {
    uint8_t count = Sw97_ElementCount();
    uint8_t next = (uint8_t)((CurrentEntryIndex(isSling) + count + direction) % count);

    Sw97_SetElement(isSling, Sw97_ElementAt(next));
    PlayMenuSfx(NA_SE_SY_CURSOR);
}

// KaleidoWheel_Run from NEI: A opens and closes, the stick or the D-pad steps, and the cursor is locked meanwhile.
static void HandleSelector(PlayState* play, bool isSling, uint8_t cell) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    bool isDpadStepping = CVarGetInteger(SW97_CVAR_DPAD_ON_PAUSE, 0) && !CHECK_BTN_ALL(input->cur.button, BTN_CUP);

    if (Sw97_ElementCount() <= 1) {
        return;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        sIsSelectorOpen = !sIsSelectorOpen;
        gCurrentItemCyclingSlot = sIsSelectorOpen ? (s8)cell : -1;
        PlayMenuSfx(NA_SE_SY_DECIDE);
        return;
    }
    if (!sIsSelectorOpen) {
        return;
    }
    pauseCtx->cursorColorSet = CYCLING_COLOR_SET;
    gCurrentItemCyclingSlot = (s8)cell;
    if (pauseCtx->stickRelX > STICK_STEP_THRESHOLD || pauseCtx->stickRelY > STICK_STEP_THRESHOLD ||
        (isDpadStepping && CHECK_BTN_ANY(input->press.button, BTN_DRIGHT | BTN_DUP))) {
        StepSelector(isSling, 1);
    } else if (pauseCtx->stickRelX < -STICK_STEP_THRESHOLD || pauseCtx->stickRelY < -STICK_STEP_THRESHOLD ||
               (isDpadStepping && CHECK_BTN_ANY(input->press.button, BTN_DLEFT | BTN_DDOWN))) {
        StepSelector(isSling, -1);
    }
}

static s8 HeldCButton(Input* input) {
    if (input->cur.button & BTN_CLEFT) {
        return 0;
    }
    if (input->cur.button & BTN_CDOWN) {
        return 1;
    }
    return (input->cur.button & BTN_CRIGHT) ? 2 : -1;
}

static void CloseRing(void) {
    sRing.isOpen = false;
    sRing.holdFrames = 0;
}

static void MoveRingCursor(Input* input, uint8_t count) {
    if (input->rel.stick_x > STICK_STEP_THRESHOLD && !sRing.isStickHeld) {
        sRing.cursor = (uint8_t)((sRing.cursor + 1) % count);
        sRing.isStickHeld = true;
        PlayMenuSfx(NA_SE_SY_CURSOR);
    } else if (input->rel.stick_x < -STICK_STEP_THRESHOLD && !sRing.isStickHeld) {
        sRing.cursor = (uint8_t)((sRing.cursor + count - 1) % count);
        sRing.isStickHeld = true;
        PlayMenuSfx(NA_SE_SY_CURSOR);
    } else if (ABS(input->rel.stick_x) < STICK_RELEASE_THRESHOLD) {
        sRing.isStickHeld = false;
    }
}

// Pressing C equips the weapon as usual; keeping it held opens the ring, and letting go primes the choice.
static void HandleRing(PlayState* play, bool isSling) {
    Input* input = &play->state.input[0];
    uint8_t count = Sw97_ElementCount();
    s8 cButton = HeldCButton(input);

    if (count == 0) {
        CloseRing();
        return;
    }
    sRing.cursor = sRing.cursor >= count ? 0 : sRing.cursor;
    if (cButton < 0) {
        if (sRing.isOpen && sRing.heldCButton >= 0) {
            Sw97_SetElement(isSling, Sw97_ElementAt(sRing.cursor));
            PlayMenuSfx(NA_SE_SY_DECIDE);
        }
        CloseRing();
        return;
    }
    sRing.heldCButton = cButton;
    if (++sRing.holdFrames < WHEEL_HOLD_FRAMES) {
        return;
    }
    if (!sRing.isOpen) {
        PlayMenuSfx(NA_SE_SY_CAMERA_ZOOM_UP);
    }
    sRing.isOpen = true;
    MoveRingCursor(input, count);
}

static void HandleWeaponCell(PlayState* play, uint16_t* item, uint16_t* slot) {
    PauseContext* pauseCtx = &play->pauseCtx;
    uint8_t cell = (uint8_t)pauseCtx->cursorPoint[PAUSE_ITEM];
    bool isSling = *item == ITEM_SLINGSHOT;

    if (!IsCursorInputLive(pauseCtx)) {
        return;
    }
    if (!IsWeapon(*item)) {
        CloseSelector();
        CloseRing();
        return;
    }
    HandleRing(play, isSling);
    HandleSelector(play, isSling, cell);
}

static KaleidoCycleIcon CycleIcon(bool isSling, Sw97Element element) {
    KaleidoCycleIcon icon = { (void*)EntryIcon(isSling, element), true };

    return icon;
}

static void DrawSelectorPreviews(PlayState* play, bool isSling, uint8_t cell) {
    uint8_t count = Sw97_ElementCount();
    uint8_t current = CurrentEntryIndex(isSling);

    if (count <= 1) {
        return;
    }
    KaleidoScope_DrawCycleIcons(play, cell, CycleIcon(isSling, Sw97_ElementAt((uint8_t)((current + count - 1) % count))),
                                CycleIcon(isSling, Sw97_ElementAt((uint8_t)((current + 1) % count))));
}

static void DrawRing(PlayState* play, bool isSling, uint8_t cell) {
    PauseContext* pauseCtx = &play->pauseCtx;
    uint8_t count = MIN(Sw97_ElementCount(), WHEEL_MAX_ENTRIES);

    if (!sRing.isOpen) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    for (uint8_t i = 0; i < count; i++) {
        Vtx* vertices = (Vtx*)Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
        u8 alpha = i == sRing.cursor ? pauseCtx->alpha : (pauseCtx->alpha >> 1);

        for (uint8_t corner = 0; corner < 4; corner++) {
            vertices[corner] = pauseCtx->itemVtx[cell * 4 + corner];
            vertices[corner].v.ob[0] += sRingOffsetX[i];
            vertices[corner].v.ob[1] += sRingOffsetY[i];
        }
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, alpha);
        gSPVertex(POLY_OPA_DISP++, (uintptr_t)vertices, 4, 0);
        KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, (void*)EntryIcon(isSling, Sw97_ElementAt(i)),
                                           ITEM_ICON_SIZE, ITEM_ICON_SIZE, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// Drawn once every cell is down, so no later icon covers the previews that spill onto the neighbours.
static void DrawWeaponCell(PlayState* play, int32_t cell, int32_t item, Vtx* vertices) {
    PauseContext* pauseCtx = &play->pauseCtx;
    uint16_t cursorItem;

    if (cell == pauseCtx->cursorPoint[PAUSE_ITEM]) {
        sWeaponCell = IsWeapon((uint16_t)item) ? (s8)cell : NO_CELL;
    }
    if (cell != ITEM_CELL_COUNT - 1 || sWeaponCell == NO_CELL) {
        return;
    }
    cursorItem = pauseCtx->cursorItem[PAUSE_ITEM];
    if (!IsWeapon(cursorItem)) {
        return;
    }
    DrawSelectorPreviews(play, cursorItem == ITEM_SLINGSHOT, (uint8_t)sWeaponCell);
    DrawRing(play, cursorItem == ITEM_SLINGSHOT, (uint8_t)sWeaponCell);
}

void Sw97_RegisterArrowUi(void) {
    SOH_REGISTER_HOOK(gSw97Api, OnInterfaceResolveButtonIcon, ResolveElementButtonIcon);
    SOH_REGISTER_HOOK(gSw97Api, OnKaleidoItemCursor, HandleWeaponCell);
    SOH_REGISTER_HOOK(gSw97Api, OnKaleidoItemDraw, DrawWeaponCell);
}
