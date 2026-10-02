#ifndef UNBOUND_Z64WHEEL_H
#define UNBOUND_Z64WHEEL_H

/**
 * z64wheel — the in-game picker an item opens to retune itself: hold L with the item in hand and the stick
 * chooses, or, latched, the item opens a row of boxes that A confirms and B cancels.
 *
 * One wheel per mod, compiled into the mod. Nothing arbitrates between wheels because nothing has to: a
 * held wheel needs its own item in Link's hand, and a latched one stops the world while it is up.
 *
 * The mod registers it once and the library takes it from there; selection and unlocked entries live in the
 * mod's own ModStorage, so each save file keeps its own.
 */

#include <math.h>
#include <string.h>

#include "soh/ModApi/ModApi.h"

#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define Z64WHEEL_MAX_ENTRIES 16
#define Z64WHEEL_STORAGE_FIELD "z64wheel"

#define Z64WHEEL_CENTER_X (SCREEN_WIDTH / 2)
#define Z64WHEEL_CENTER_Y (SCREEN_HEIGHT / 2)
#define Z64WHEEL_RADIUS 52
#define Z64WHEEL_ICON_SIZE 24
#define Z64WHEEL_SELECTED_ICON_SIZE 32
#define Z64WHEEL_ICON_SOURCE_SIZE 32
#define Z64WHEEL_STICK_DEADZONE 20
#define Z64WHEEL_STICK_STEP_DEADZONE 30
#define Z64WHEEL_OPEN_FRAMES 6
#define Z64WHEEL_BLOCKED_BUTTONS (BTN_A | BTN_B | BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT | BTN_CUP | BTN_R)
#define Z64WHEEL_TIME_PRIORITY 1000
#define Z64WHEEL_TAU 6.28318531f

#define Z64WHEEL_BOX_SIDE 32
#define Z64WHEEL_BOX_GAP 6
#define Z64WHEEL_BOX_BORDER 1
#define Z64WHEEL_BOXES_PER_ROW 10
#define Z64WHEEL_BOX_ROW_CENTER_Y 76
#define Z64WHEEL_BOX_GRID_TOP 30
#define Z64WHEEL_BOX_ROW_STEP (Z64WHEEL_BOX_GAP + 2)
#define Z64WHEEL_BRACKET_ARM 9
#define Z64WHEEL_BRACKET_THICKNESS 2
#define Z64WHEEL_ARROW_WIDTH 5
#define Z64WHEEL_ARROW_HEIGHT 9

typedef struct {
    const char* iconPath;
    const char* label;
    const char* itemIconPath;
} Z64WheelEntry;

typedef struct {
    uint32_t unlocked;
    uint32_t selection;
} Z64WheelSaved;

typedef struct {
    const SOHModApi* api;
    const char* itemKey;
    const char* modName;
    const Z64WheelEntry* entries;
    uint32_t count;
    uint32_t unlocked;
    uint32_t selection;
    uint32_t highlighted;
    uint16_t pulse;
    uint8_t openProgress;
    bool isLatched;
    bool isOpen;
    bool isOpenPending;
    bool isStickHeld;
} Z64Wheel;

const char* CustomItemRegistry_GetHeldKey(void);
bool CustomItemRegistry_SetIconPath(const char* key, const char* path);

int16_t OTRGetRectDimensionFromLeftEdge(float v);
int16_t OTRGetRectDimensionFromRightEdge(float v);

static Z64Wheel sZ64Wheel;

static inline bool Z64Wheel_IsEntryUnlocked(uint32_t entry) {
    return entry < sZ64Wheel.count && (sZ64Wheel.unlocked & (1u << entry)) != 0;
}

static inline uint32_t Z64Wheel_CountUnlocked(void) {
    uint32_t count = 0;

    for (uint32_t entry = 0; entry < sZ64Wheel.count; entry++) {
        count += Z64Wheel_IsEntryUnlocked(entry) ? 1 : 0;
    }
    return count;
}

static inline uint32_t Z64Wheel_GetSelection(void) {
    return sZ64Wheel.selection;
}

static inline bool Z64Wheel_IsOpen(void) {
    return sZ64Wheel.isOpen;
}

static inline void Z64Wheel_PlaySfx(uint16_t sfxId) {
    Audio_PlaySoundGeneral(sfxId, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static inline void Z64Wheel_ApplySelectedIcon(void) {
    const Z64WheelEntry* entry;
    const char* icon;

    if (sZ64Wheel.selection >= sZ64Wheel.count) {
        return;
    }
    entry = &sZ64Wheel.entries[sZ64Wheel.selection];
    icon = entry->itemIconPath != NULL ? entry->itemIconPath : entry->iconPath;
    if (icon != NULL) {
        CustomItemRegistry_SetIconPath(sZ64Wheel.itemKey, icon);
    }
}

static inline void Z64Wheel_Store(void) {
    Z64WheelSaved saved = { sZ64Wheel.unlocked, sZ64Wheel.selection };

    sZ64Wheel.api->StorageSet(sZ64Wheel.modName, Z64WHEEL_STORAGE_FIELD, &saved, sizeof(saved));
}

static inline void Z64Wheel_Load(void) {
    Z64WheelSaved saved = { 1, 0 };

    sZ64Wheel.api->StorageGet(sZ64Wheel.modName, Z64WHEEL_STORAGE_FIELD, &saved, sizeof(saved));
    sZ64Wheel.unlocked = saved.unlocked | 1;
    sZ64Wheel.selection = saved.selection;
    if (!Z64Wheel_IsEntryUnlocked(sZ64Wheel.selection)) {
        sZ64Wheel.selection = 0;
    }
    sZ64Wheel.highlighted = sZ64Wheel.selection;
    Z64Wheel_ApplySelectedIcon();
}

static inline bool Z64Wheel_SetSelection(uint32_t entry) {
    if (!Z64Wheel_IsEntryUnlocked(entry) || sZ64Wheel.selection == entry) {
        return false;
    }
    sZ64Wheel.selection = entry;
    Z64Wheel_ApplySelectedIcon();
    Z64Wheel_Store();
    return true;
}

static inline bool Z64Wheel_SetEntryUnlocked(uint32_t entry, bool unlocked) {
    if (entry >= sZ64Wheel.count || Z64Wheel_IsEntryUnlocked(entry) == unlocked) {
        return false;
    }
    if (unlocked) {
        sZ64Wheel.unlocked |= 1u << entry;
    } else {
        sZ64Wheel.unlocked &= ~(1u << entry);
        if (sZ64Wheel.selection == entry) {
            sZ64Wheel.selection = 0;
            Z64Wheel_ApplySelectedIcon();
        }
    }
    Z64Wheel_Store();
    return true;
}

static inline void Z64Wheel_Close(void) {
    if (!sZ64Wheel.isOpen) {
        return;
    }
    if (sZ64Wheel.highlighted != sZ64Wheel.selection && Z64Wheel_IsEntryUnlocked(sZ64Wheel.highlighted)) {
        Z64Wheel_SetSelection(sZ64Wheel.highlighted);
        Z64Wheel_PlaySfx(NA_SE_SY_DECIDE);
    }
    sZ64Wheel.api->ReleasePlayerInput(sZ64Wheel.modName);
    sZ64Wheel.api->ReleaseTimeControl(sZ64Wheel.modName);
    sZ64Wheel.isOpen = false;
    sZ64Wheel.openProgress = 0;
    sZ64Wheel.pulse = 0;
    sZ64Wheel.isStickHeld = false;
}

static inline void Z64Wheel_Cancel(void) {
    sZ64Wheel.highlighted = sZ64Wheel.selection;
    Z64Wheel_PlaySfx(NA_SE_SY_CANCEL);
    Z64Wheel_Close();
}

static inline void Z64Wheel_Begin(void) {
    sZ64Wheel.isOpen = true;
    sZ64Wheel.highlighted = sZ64Wheel.selection;
    sZ64Wheel.openProgress = 0;
    sZ64Wheel.api->BlockPlayerInput(sZ64Wheel.modName, Z64WHEEL_BLOCKED_BUTTONS, true);
    if (sZ64Wheel.isLatched) {
        sZ64Wheel.api->RequestTimeControl(sZ64Wheel.modName, Z64WHEEL_TIME_PRIORITY, 0.0f, true);
    }
    Z64Wheel_PlaySfx(sZ64Wheel.isLatched ? NA_SE_SY_WIN_OPEN : NA_SE_SY_CAMERA_ZOOM_DOWN);
}

static inline bool Z64Wheel_Open(void) {
    if (!sZ64Wheel.isLatched || Z64Wheel_CountUnlocked() == 0 || sZ64Wheel.isOpen) {
        return false;
    }
    sZ64Wheel.isOpenPending = true;
    return true;
}

static inline bool Z64Wheel_IsPlayerBusy(Player* player, PlayState* play) {
    return player == NULL || play->msgCtx.msgMode != MSGMODE_NONE ||
           (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_IN_ITEM_CS |
                                   PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM)) != 0;
}

static inline void Z64Wheel_StepHighlight(int16_t stickX) {
    int32_t direction = stickX > Z64WHEEL_STICK_STEP_DEADZONE ? 1 : (stickX < -Z64WHEEL_STICK_STEP_DEADZONE ? -1 : 0);

    if (direction == 0) {
        sZ64Wheel.isStickHeld = false;
        return;
    }
    if (sZ64Wheel.isStickHeld) {
        return;
    }
    sZ64Wheel.isStickHeld = true;
    for (uint32_t step = 1; step <= sZ64Wheel.count; step++) {
        uint32_t probe = (sZ64Wheel.highlighted + (direction > 0 ? step : sZ64Wheel.count - step)) % sZ64Wheel.count;

        if (Z64Wheel_IsEntryUnlocked(probe)) {
            sZ64Wheel.highlighted = probe;
            Z64Wheel_PlaySfx(NA_SE_SY_CURSOR);
            return;
        }
    }
}

static inline uint32_t Z64Wheel_EntryUnderStick(int8_t stickX, int8_t stickY) {
    uint32_t unlocked[Z64WHEEL_MAX_ENTRIES];
    uint32_t unlockedCount = 0;
    float stickAngle;
    float step;
    int32_t slot;

    for (uint32_t entry = 0; entry < sZ64Wheel.count; entry++) {
        if (Z64Wheel_IsEntryUnlocked(entry)) {
            unlocked[unlockedCount++] = entry;
        }
    }
    if (unlockedCount == 0 || (ABS(stickX) < Z64WHEEL_STICK_DEADZONE && ABS(stickY) < Z64WHEEL_STICK_DEADZONE)) {
        return sZ64Wheel.highlighted;
    }
    stickAngle = atan2f((float)stickX, (float)stickY);
    step = Z64WHEEL_TAU / (float)unlockedCount;
    slot = (int32_t)lroundf(stickAngle / step) % (int32_t)unlockedCount;
    if (slot < 0) {
        slot += (int32_t)unlockedCount;
    }
    return unlocked[slot];
}

static inline void Z64Wheel_Update(void) {
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);
    const char* heldKey;
    uint32_t entry;

    if (gPlayState == NULL || Z64Wheel_IsPlayerBusy(player, gPlayState) || Z64Wheel_CountUnlocked() == 0) {
        Z64Wheel_Close();
        return;
    }
    if (sZ64Wheel.isLatched) {
        uint16_t pressed = gPlayState->state.input[0].press.button;

        if (!sZ64Wheel.isOpen) {
            if (!sZ64Wheel.isOpenPending) {
                return;
            }
            sZ64Wheel.isOpenPending = false;
            Z64Wheel_Begin();
        }
        if (pressed & BTN_B) {
            Z64Wheel_Cancel();
            return;
        }
        if (pressed & BTN_A) {
            Z64Wheel_Close();
            return;
        }
        sZ64Wheel.pulse++;
        Z64Wheel_StepHighlight(gPlayState->state.input[0].rel.stick_x);
        return;
    }

    heldKey = CustomItemRegistry_GetHeldKey();
    if (!(gPlayState->state.input[0].cur.button & BTN_L) || heldKey == NULL ||
        strcmp(heldKey, sZ64Wheel.itemKey) != 0) {
        Z64Wheel_Close();
        return;
    }
    if (!sZ64Wheel.isOpen) {
        Z64Wheel_Begin();
    }
    if (sZ64Wheel.openProgress < Z64WHEEL_OPEN_FRAMES) {
        sZ64Wheel.openProgress++;
    }
    entry = Z64Wheel_EntryUnderStick(gPlayState->state.input[0].cur.stick_x, gPlayState->state.input[0].cur.stick_y);
    if (entry != sZ64Wheel.highlighted) {
        sZ64Wheel.highlighted = entry;
        Z64Wheel_PlaySfx(NA_SE_SY_CURSOR);
    }
}

static inline void Z64Wheel_DrawIcon(PlayState* play, const char* iconPath, int16_t x1, int16_t y1, int16_t size,
                                     uint8_t tint, uint8_t alpha) {
    int16_t texelStep = (int16_t)((Z64WHEEL_ICON_SOURCE_SIZE << 10) / size);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, tint, tint, tint, alpha);
    gDPLoadTextureBlock(OVERLAY_DISP++, iconPath, G_IM_FMT_RGBA, G_IM_SIZ_32b, Z64WHEEL_ICON_SOURCE_SIZE,
                        Z64WHEEL_ICON_SOURCE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, x1 << 2, y1 << 2, (x1 + size) << 2, (y1 + size) << 2, G_TX_RENDERTILE, 0, 0,
                            texelStep, texelStep);
    CLOSE_DISPS(play->state.gfxCtx);
}

static inline void Z64Wheel_DimScreen(PlayState* play, uint8_t alpha) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gSPClearGeometryMode(OVERLAY_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN |
                                             G_TEXTURE_GEN_LINEAR | G_SHADING_SMOOTH | G_LOD);
    gDPSetOtherMode(OVERLAY_DISP++,
                    G_AD_DISABLE | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_1PRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_CLD_SURF | G_RM_CLD_SURF2);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, alpha);
    gDPFillWideRectangle(OVERLAY_DISP++, OTRGetRectDimensionFromLeftEdge(0), 0,
                         OTRGetRectDimensionFromRightEdge(SCREEN_WIDTH), SCREEN_HEIGHT);
    CLOSE_DISPS(play->state.gfxCtx);
}

static inline void Z64Wheel_SolidRect(PlayState* play, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r,
                                      uint8_t g, uint8_t b, uint8_t a) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, r, g, b, a);
    gSPWideTextureRectangle(OVERLAY_DISP++, x1 << 2, y1 << 2, x2 << 2, y2 << 2, G_TX_RENDERTILE, 0, 0, 0, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static inline void Z64Wheel_SetupSolidRects(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetOtherMode(OVERLAY_DISP++,
                    G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PRIM | G_RM_CLD_SURF | G_RM_CLD_SURF2);
    CLOSE_DISPS(play->state.gfxCtx);
}

static inline void Z64Wheel_DrawArrow(PlayState* play, int16_t tipX, int16_t centerY, int16_t direction, uint8_t r,
                                      uint8_t g, uint8_t b) {
    for (int16_t i = 0; i < Z64WHEEL_ARROW_WIDTH; i++) {
        int16_t half = (int16_t)(Z64WHEEL_ARROW_HEIGHT / 2 - (Z64WHEEL_ARROW_HEIGHT / 2) * i / Z64WHEEL_ARROW_WIDTH);
        int16_t x = (int16_t)(tipX + direction * i);
        int16_t left = direction > 0 ? x : (int16_t)(x - 1);

        Z64Wheel_SolidRect(play, left, (int16_t)(centerY - half), (int16_t)(left + 1), (int16_t)(centerY + half + 1), r,
                           g, b, 235);
    }
}

static inline void Z64Wheel_BoxCorner(uint32_t slot, uint32_t count, int16_t* outX, int16_t* outY) {
    uint32_t rows = (count + Z64WHEEL_BOXES_PER_ROW - 1) / Z64WHEEL_BOXES_PER_ROW;
    uint32_t row = slot / Z64WHEEL_BOXES_PER_ROW;
    uint32_t column = slot % Z64WHEEL_BOXES_PER_ROW;
    uint32_t left = count - row * Z64WHEEL_BOXES_PER_ROW;
    uint32_t inRow = left < Z64WHEEL_BOXES_PER_ROW ? left : Z64WHEEL_BOXES_PER_ROW;
    int16_t rowWidth = (int16_t)(inRow * Z64WHEEL_BOX_SIDE + (inRow - 1) * Z64WHEEL_BOX_GAP);
    int16_t firstCenterY = Z64WHEEL_BOX_ROW_CENTER_Y;

    if (rows > 1) {
        int16_t gridHeight = (int16_t)(rows * Z64WHEEL_BOX_SIDE + (rows - 1) * Z64WHEEL_BOX_ROW_STEP);

        firstCenterY = (int16_t)(SCREEN_HEIGHT / 2 - gridHeight / 2 + Z64WHEEL_BOX_SIDE / 2);
        if (firstCenterY < Z64WHEEL_BOX_GRID_TOP) {
            firstCenterY = Z64WHEEL_BOX_GRID_TOP;
        }
    }
    *outX = (int16_t)((SCREEN_WIDTH - rowWidth) / 2 + column * (Z64WHEEL_BOX_SIDE + Z64WHEEL_BOX_GAP));
    *outY = (int16_t)(firstCenterY - Z64WHEEL_BOX_SIDE / 2 + row * (Z64WHEEL_BOX_SIDE + Z64WHEEL_BOX_ROW_STEP));
}

static inline void Z64Wheel_DrawBoxRow(PlayState* play) {
    uint32_t count = sZ64Wheel.count;
    int16_t pulse;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    int16_t ex;
    int16_t ey;
    int16_t cx1;
    int16_t cy1;
    int16_t cx2;
    int16_t cy2;

    Z64Wheel_SetupSolidRects(play);
    for (uint32_t slot = 0; slot < count; slot++) {
        int16_t x1;
        int16_t y1;
        int16_t x2;
        int16_t y2;
        bool isUnlocked = Z64Wheel_IsEntryUnlocked(slot);

        Z64Wheel_BoxCorner(slot, count, &x1, &y1);
        x2 = (int16_t)(x1 + Z64WHEEL_BOX_SIDE);
        y2 = (int16_t)(y1 + Z64WHEEL_BOX_SIDE);
        if (slot == sZ64Wheel.selection) {
            Z64Wheel_SolidRect(play, x1, y1, x2, y2, 45, 105, 175, 225);
        } else {
            Z64Wheel_SolidRect(play, x1, y1, x2, y2, 16, 20, 28, isUnlocked ? 205 : 140);
        }
        Z64Wheel_SolidRect(play, x1, y1, x2, (int16_t)(y1 + Z64WHEEL_BOX_BORDER), 105, 115, 130, 200);
        Z64Wheel_SolidRect(play, x1, (int16_t)(y2 - Z64WHEEL_BOX_BORDER), x2, y2, 105, 115, 130, 200);
        Z64Wheel_SolidRect(play, x1, y1, (int16_t)(x1 + Z64WHEEL_BOX_BORDER), y2, 105, 115, 130, 200);
        Z64Wheel_SolidRect(play, (int16_t)(x2 - Z64WHEEL_BOX_BORDER), y1, x2, y2, 105, 115, 130, 200);
    }
    for (uint32_t slot = 0; slot < count; slot++) {
        int16_t x1;
        int16_t y1;
        bool isUnlocked = Z64Wheel_IsEntryUnlocked(slot);

        if (sZ64Wheel.entries[slot].iconPath == NULL) {
            continue;
        }
        Z64Wheel_BoxCorner(slot, count, &x1, &y1);
        Z64Wheel_DrawIcon(play, sZ64Wheel.entries[slot].iconPath, x1, y1, Z64WHEEL_BOX_SIDE, isUnlocked ? 255 : 130,
                          isUnlocked ? 255 : 100);
    }

    pulse = (int16_t)(sZ64Wheel.pulse % 40 < 20 ? sZ64Wheel.pulse % 40 : 40 - sZ64Wheel.pulse % 40);
    r = 255;
    g = (uint8_t)(225 + pulse);
    b = (uint8_t)(120 + pulse * 3);
    Z64Wheel_BoxCorner(sZ64Wheel.highlighted, count, &ex, &ey);
    cx1 = (int16_t)(ex - 2);
    cy1 = (int16_t)(ey - 2);
    cx2 = (int16_t)(cx1 + Z64WHEEL_BOX_SIDE + 4);
    cy2 = (int16_t)(cy1 + Z64WHEEL_BOX_SIDE + 4);

    Z64Wheel_SetupSolidRects(play);
    Z64Wheel_SolidRect(play, cx1, cy1, (int16_t)(cx1 + Z64WHEEL_BRACKET_ARM),
                       (int16_t)(cy1 + Z64WHEEL_BRACKET_THICKNESS), r, g, b, 255);
    Z64Wheel_SolidRect(play, cx1, cy1, (int16_t)(cx1 + Z64WHEEL_BRACKET_THICKNESS),
                       (int16_t)(cy1 + Z64WHEEL_BRACKET_ARM), r, g, b, 255);
    Z64Wheel_SolidRect(play, (int16_t)(cx2 - Z64WHEEL_BRACKET_ARM), cy1, cx2,
                       (int16_t)(cy1 + Z64WHEEL_BRACKET_THICKNESS), r, g, b, 255);
    Z64Wheel_SolidRect(play, (int16_t)(cx2 - Z64WHEEL_BRACKET_THICKNESS), cy1, cx2,
                       (int16_t)(cy1 + Z64WHEEL_BRACKET_ARM), r, g, b, 255);
    Z64Wheel_SolidRect(play, cx1, (int16_t)(cy2 - Z64WHEEL_BRACKET_THICKNESS), (int16_t)(cx1 + Z64WHEEL_BRACKET_ARM),
                       cy2, r, g, b, 255);
    Z64Wheel_SolidRect(play, cx1, (int16_t)(cy2 - Z64WHEEL_BRACKET_ARM), (int16_t)(cx1 + Z64WHEEL_BRACKET_THICKNESS),
                       cy2, r, g, b, 255);
    Z64Wheel_SolidRect(play, (int16_t)(cx2 - Z64WHEEL_BRACKET_ARM), (int16_t)(cy2 - Z64WHEEL_BRACKET_THICKNESS), cx2,
                       cy2, r, g, b, 255);
    Z64Wheel_SolidRect(play, (int16_t)(cx2 - Z64WHEEL_BRACKET_THICKNESS), (int16_t)(cy2 - Z64WHEEL_BRACKET_ARM), cx2,
                       cy2, r, g, b, 255);
    if (count > 1) {
        int16_t hintY = (int16_t)(cy2 + 7);
        int16_t middle = (int16_t)((cx1 + cx2) / 2);

        Z64Wheel_DrawArrow(play, (int16_t)(middle - 5), hintY, -1, r, g, b);
        Z64Wheel_DrawArrow(play, (int16_t)(middle + 5), hintY, 1, r, g, b);
    }
}

static inline void Z64Wheel_Draw(PlayState* play) {
    uint32_t unlocked[Z64WHEEL_MAX_ENTRIES];
    uint32_t unlockedCount = 0;
    float openScale;
    uint8_t alpha;

    if (!sZ64Wheel.isOpen || play == NULL) {
        return;
    }
    if (sZ64Wheel.isLatched) {
        Z64Wheel_DrawBoxRow(play);
        return;
    }
    for (uint32_t entry = 0; entry < sZ64Wheel.count; entry++) {
        if (Z64Wheel_IsEntryUnlocked(entry)) {
            unlocked[unlockedCount++] = entry;
        }
    }
    openScale = (float)sZ64Wheel.openProgress / (float)Z64WHEEL_OPEN_FRAMES;
    alpha = (uint8_t)(255.0f * openScale);

    Z64Wheel_DimScreen(play, (uint8_t)(110.0f * openScale));
    for (uint32_t slot = 0; slot < unlockedCount; slot++) {
        float angle = Z64WHEEL_TAU * (float)slot / (float)unlockedCount;
        int16_t radius = (int16_t)(Z64WHEEL_RADIUS * openScale);
        int16_t x = (int16_t)(Z64WHEEL_CENTER_X + sinf(angle) * radius);
        int16_t y = (int16_t)(Z64WHEEL_CENTER_Y - cosf(angle) * radius);
        bool isHighlighted = unlocked[slot] == sZ64Wheel.highlighted;
        int16_t size = isHighlighted ? Z64WHEEL_SELECTED_ICON_SIZE : Z64WHEEL_ICON_SIZE;

        Z64Wheel_DrawIcon(play, sZ64Wheel.entries[unlocked[slot]].iconPath, (int16_t)(x - size / 2),
                          (int16_t)(y - size / 2), size, 255, isHighlighted ? alpha : (uint8_t)(alpha * 0.65f));
    }
}

static inline void Z64Wheel_OnLoadFile(int32_t fileNum) {
    Z64Wheel_Load();
}

static inline void Z64Wheel_OnInterfaceDrawEnd(PlayState* play) {
    Z64Wheel_Draw(play);
}

static inline void Z64Wheel_OnPlayerUpdate(void) {
    Z64Wheel_Update();
}

/**
 * Registers the mod's one wheel and takes over its frame, its drawing and its save slot. A latched wheel is
 * opened by the item with Z64Wheel_Open; a plain one answers L while the item is in hand.
 */
static inline bool Z64Wheel_Register(const SOHModApi* api, const char* itemKey, const char* modName,
                                     const Z64WheelEntry* entries, uint32_t count, bool isLatched) {
    if (api == NULL || itemKey == NULL || modName == NULL || entries == NULL || count == 0 ||
        count > Z64WHEEL_MAX_ENTRIES) {
        return false;
    }
    sZ64Wheel.api = api;
    sZ64Wheel.itemKey = itemKey;
    sZ64Wheel.modName = modName;
    sZ64Wheel.entries = entries;
    sZ64Wheel.count = count;
    sZ64Wheel.isLatched = isLatched;
    Z64Wheel_Load();
    SOH_REGISTER_HOOK(api, OnPlayerUpdate, Z64Wheel_OnPlayerUpdate);
    SOH_REGISTER_HOOK(api, OnInterfaceDrawEnd, Z64Wheel_OnInterfaceDrawEnd);
    SOH_REGISTER_HOOK(api, OnLoadFile, Z64Wheel_OnLoadFile);
    return true;
}

#endif
