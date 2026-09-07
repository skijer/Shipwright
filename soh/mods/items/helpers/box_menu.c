/**
 * box_menu.c — generic hold-button "boxed icon row" selector (Skijer's NEI).
 *
 * The BotW rune-wheel interaction, as a reusable widget: while the player HOLDS a button the world
 * fully pauses (Minish Kaleido pattern: pauseCtx->state set with a custom update/draw pair wired in
 * z_play.c) and a centered row of boxed icons appears over a dimmed screen. The stick steps the
 * selection left/right (gray borders, the selected box grows and pulses white); RELEASING the hold
 * button confirms and unpauses; B cancels without confirming.
 *
 * Deliberately generic — entries are just OTR icon paths, the caller decides what selection means
 * via the confirm callback. The Sheikah Slate rune wheel is the first client; anything that wants a
 * "hold to choose between N things" (masks, spells, ammo, forms) can reuse it as-is.
 *
 * NO HEADER ON PURPOSE: both repos glob mods/*.h with CONFIGURE_DEPENDS, so a new header there
 * forces a full CMake regeneration on the next build. The public API is declared here and repeated
 * as local externs at the two call sites (z_play.c, item_sheikah_slate.c), which are in this same
 * unity translation unit or right next to it.
 *
 * Pause model: pauseCtx->state makes Play_Update skip the whole actor/player pass while rendering
 * continues, and z_play.c routes the pause update/draw here instead of to the kaleido. Nothing else
 * can reach the input while we are up, so the hold button needs no vanilla-action suppression once
 * the menu is open (only the OPENING press does, which is the caller's business).
 *
 * The hold button is read from cur.button, never press/rel: L and R are consumed by the shield /
 * Z-target handlers before item code runs, so an edge derived from press.button is unreliable. We
 * only need "is it still down", and the release edge comes from our own previous-state byte.
 */

#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"

// Raised from 10 for the Trirod echo wheel (~50 learned echoes). Anything past
// BOXM_PER_ROW wraps into further rows and the stick gains vertical steps; a
// single-row menu (the slate's four runes) renders exactly as it always did.
#define BOX_MENU_MAX_ENTRIES 64
#define BOXM_PER_ROW 10

typedef struct {
    const char* iconPath; // __OTR__ path of an RGBA32 texture
    u8 iconSize;          // source texture size in px (24 or 32)
    u8 enabled;           // 0 = drawn grayed and skipped by the cursor
} BoxMenuEntry;

// index = the confirmed entry. Runs AFTER the menu closed and the game unpaused.
typedef void (*BoxMenuConfirmFn)(s32 index);

u8 BoxMenu_IsOpen(void);
// Opens the menu (copies `entries`). Returns 0 if it cannot open right now (already paused,
// mid-transition, game over) — the caller just tries again next frame if the hold continues.
// holdButton == 0 opens LATCHED: the menu stays up until A confirms or B cancels.
u8 BoxMenu_Open(PlayState* play, const BoxMenuEntry* entries, s32 count, s32 selected, u16 holdButton,
                BoxMenuConfirmFn onConfirm);
// The z_play.c custom-pause pair (Minish Kaleido idiom).
void BoxMenu_Update(PlayState* play);
void BoxMenu_Draw(PlayState* play);

// Optional: on a SINGLE-ROW menu the vertical stick is otherwise dead, so it is offered to the
// caller as a per-entry sub-choice (dir -1 up, +1 down). Pair it with BoxMenu_SetEntryIcon to show
// the change. Registered per Open; a caller that never sets it is unaffected.
typedef void (*BoxMenuSubStepFn)(s32 index, s32 dir);
void BoxMenu_SetSubStep(BoxMenuSubStepFn onSubStep);
void BoxMenu_SetEntryIcon(s32 index, const char* iconPath);

// ── Layout (screen pixels, 320x240 virtual) ─────────────────────────────────
// Uniform boxes, BotW-style: the icon FILLS its box and state is shown by colour, never by size.
#define BOXM_BOX 32        // box side == icon source size, so the art fills it 1:1
#define BOXM_GAP 6         // space between boxes
#define BOXM_CY 76         // row center Y — high like BotW's rune row
#define BOXM_BORDER 1      // hairline box outline
#define BOXM_BRACKET_LEN 9 // arm length of the cursor's corner brackets
#define BOXM_BRACKET_TH 2  // bracket thickness
#define BOXM_STICK_DEAD 30 // stick deflection needed to step

static u8 sBoxMOpen = 0;
static BoxMenuEntry sBoxMEntries[BOX_MENU_MAX_ENTRIES];
static s32 sBoxMCount = 0;
static s32 sBoxMCursor = 0; // where the player is pointing right now (yellow brackets)
static s32 sBoxMActive = 0; // what is equipped/active going in (blue plate) — the caller's `selected`
static u16 sBoxMHoldButton = 0;
static BoxMenuConfirmFn sBoxMOnConfirm = NULL;
static u8 sBoxMStickHeld = 0;
static u8 sBoxMHoldSeen = 0; // the hold button has been observed down at least once
static s16 sBoxMPulse = 0;
static BoxMenuSubStepFn sBoxMOnSubStep = NULL;

u8 BoxMenu_IsOpen(void) {
    return sBoxMOpen;
}

void BoxMenu_SetSubStep(BoxMenuSubStepFn onSubStep) {
    sBoxMOnSubStep = onSubStep;
}

void BoxMenu_SetEntryIcon(s32 index, const char* iconPath) {
    if ((index >= 0) && (index < sBoxMCount) && (iconPath != NULL)) {
        sBoxMEntries[index].iconPath = iconPath;
    }
}

static void BoxMenu_PlaySfx(u16 sfx) {
    Audio_PlaySoundGeneral(sfx, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

// Next selectable entry in `dir`, wrapping, skipping disabled ones. Returns `from` if there is
// nowhere else to go.
static s32 BoxMenu_Step(s32 from, s32 dir) {
    for (s32 i = 1; i <= sBoxMCount; i++) {
        s32 probe = (from + (dir > 0 ? i : sBoxMCount - i)) % sBoxMCount;
        if (sBoxMEntries[probe].enabled) {
            return probe;
        }
    }
    return from;
}

// ── Grid geometry ────────────────────────────────────────────────────────────
// Entries wrap into rows of BOXM_PER_ROW; each row is centered on its own, so a
// partial last row sits centered under the full ones instead of hanging left.

static s32 BoxMenu_RowCount(void) {
    return (sBoxMCount + BOXM_PER_ROW - 1) / BOXM_PER_ROW;
}

// Top-left corner of entry i's box.
static void BoxMenu_EntryXY(s32 i, s32* outX, s32* outY) {
    s32 rows = BoxMenu_RowCount();
    s32 row = i / BOXM_PER_ROW;
    s32 col = i % BOXM_PER_ROW;
    s32 inRow = sBoxMCount - row * BOXM_PER_ROW;
    s32 rowStep = BOXM_BOX + BOXM_GAP + 2; // extra 2px so the cursor brackets of stacked rows never touch
    s32 firstCY;
    s32 rowW;

    if (inRow > BOXM_PER_ROW) {
        inRow = BOXM_PER_ROW;
    }
    // One row keeps the historic BotW-high position; a grid centers on the screen
    // instead, because five rows hanging from y=76 would run off the bottom.
    if (rows == 1) {
        firstCY = BOXM_CY;
    } else {
        s32 gridH = rows * BOXM_BOX + (rows - 1) * (BOXM_GAP + 2);

        firstCY = (SCREEN_HEIGHT / 2) - (gridH / 2) + (BOXM_BOX / 2);
        if (firstCY < 30) {
            firstCY = 30; // never under the HUD hearts
        }
    }

    rowW = inRow * BOXM_BOX + (inRow - 1) * BOXM_GAP;
    *outX = (SCREEN_WIDTH - rowW) / 2 + col * (BOXM_BOX + BOXM_GAP);
    *outY = firstCY - (BOXM_BOX / 2) + row * rowStep;
}

// Vertical step: land on the same column one row up/down, then slide to the
// nearest enabled entry. Distinct from BoxMenu_Step so a locked box in the next
// row does not send the cursor wrapping around the whole list.
static s32 BoxMenu_StepRow(s32 from, s32 dir) {
    s32 target = from + dir * BOXM_PER_ROW;

    if ((target < 0) || (target >= sBoxMCount)) {
        return from; // no row there
    }
    for (s32 span = 0; span < BOXM_PER_ROW; span++) {
        s32 a = target - span;
        s32 b = target + span;

        if ((a >= 0) && sBoxMEntries[a].enabled && (a / BOXM_PER_ROW == target / BOXM_PER_ROW)) {
            return a;
        }
        if ((b < sBoxMCount) && sBoxMEntries[b].enabled && (b / BOXM_PER_ROW == target / BOXM_PER_ROW)) {
            return b;
        }
    }
    return from; // whole row locked
}

u8 BoxMenu_Open(PlayState* play, const BoxMenuEntry* entries, s32 count, s32 selected, u16 holdButton,
                BoxMenuConfirmFn onConfirm) {
    if (sBoxMOpen) {
        return 1; // already ours
    }
    if (entries == NULL || count <= 0) {
        return 0;
    }
    // Never steal a frame the engine is already using for something modal.
    if (play->pauseCtx.state != 0 || play->pauseCtx.debugState != 0) {
        return 0;
    }
    if (play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF) {
        return 0;
    }
    if (play->gameOverCtx.state != GAMEOVER_INACTIVE) {
        return 0;
    }

    // Cleared per open: a caller that wants it calls BoxMenu_SetSubStep right after, so the last
    // client's callback can never leak into the next menu.
    sBoxMOnSubStep = NULL;

    if (count > BOX_MENU_MAX_ENTRIES) {
        count = BOX_MENU_MAX_ENTRIES;
    }
    for (s32 i = 0; i < count; i++) {
        sBoxMEntries[i] = entries[i];
    }
    sBoxMCount = count;
    sBoxMCursor = (selected >= 0 && selected < count) ? selected : 0;
    if (!sBoxMEntries[sBoxMCursor].enabled) {
        sBoxMCursor = BoxMenu_Step(sBoxMCursor, 1);
    }
    sBoxMActive = sBoxMCursor; // the blue plate stays on what was equipped while you browse
    sBoxMHoldButton = holdButton;
    sBoxMOnConfirm = onConfirm;
    sBoxMStickHeld = 0;
    sBoxMHoldSeen = 0;
    sBoxMPulse = 0;
    sBoxMOpen = 1;

    play->pauseCtx.state = 1; // freezes Play_Update; drawing continues
    BoxMenu_PlaySfx(NA_SE_SY_WIN_OPEN);
    return 1;
}

// Tear down and hand the game back. `confirm` runs the callback with the chosen index.
static void BoxMenu_Close(PlayState* play, u8 confirm) {
    s32 chosen = sBoxMCursor;
    BoxMenuConfirmFn fn = sBoxMOnConfirm;

    sBoxMOpen = 0;
    sBoxMOnConfirm = NULL;
    sBoxMCount = 0;
    play->pauseCtx.state = 0;

    if (confirm && fn != NULL) {
        fn(chosen);
    }
}

void BoxMenu_Update(PlayState* play) {
    Input* input = &play->state.input[0];
    u16 held = input->cur.button;

    if (!sBoxMOpen) {
        return;
    }

    sBoxMPulse++;

    // holdButton == 0 is the LATCHED mode: the menu stays up on its own and A confirms, for callers
    // that open on a press rather than a hold (the Rod of Seasons' change-season prompt).
    if (sBoxMHoldButton == 0) {
        if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
            BoxMenu_PlaySfx(NA_SE_SY_DECIDE);
            BoxMenu_Close(play, 1);
            return;
        }
    } else if (held & sBoxMHoldButton) {
        // Release of the hold button confirms. The button is usually still down on the frame we
        // open, but if the caller opened on a press that had already been consumed we must not
        // close on the very first frame — wait until we have actually SEEN it down.
        sBoxMHoldSeen = 1;
    } else if (sBoxMHoldSeen) {
        BoxMenu_PlaySfx(NA_SE_SY_DECIDE);
        BoxMenu_Close(play, 1);
        return;
    }

    // B cancels outright (selection discarded).
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        BoxMenu_PlaySfx(NA_SE_SY_CANCEL);
        BoxMenu_Close(play, 0);
        return;
    }

    // Stick steps one box per flick; on a multi-row grid a vertical flick steps a
    // whole row. Whichever axis is deflected further wins the frame, so diagonal
    // input never double-steps.
    {
        s16 stickX = input->rel.stick_x;
        s16 stickY = input->rel.stick_y;
        s32 wantX = (stickX > BOXM_STICK_DEAD) ? 1 : (stickX < -BOXM_STICK_DEAD) ? -1 : 0;
        s32 wantY = (stickY > BOXM_STICK_DEAD) ? -1 : (stickY < -BOXM_STICK_DEAD) ? 1 : 0; // up = previous row

        s32 singleRow = (BoxMenu_RowCount() <= 1);

        if (singleRow && (sBoxMOnSubStep == NULL)) {
            wantY = 0;
        }
        if (wantX != 0 || wantY != 0) {
            if (!sBoxMStickHeld) {
                s32 next = sBoxMCursor;

                if ((wantY != 0) && (ABS(stickY) >= ABS(stickX))) {
                    if (singleRow) {
                        sBoxMOnSubStep(sBoxMCursor, wantY);
                        BoxMenu_PlaySfx(NA_SE_SY_CURSOR);
                    } else {
                        next = BoxMenu_StepRow(sBoxMCursor, wantY);
                    }
                } else {
                    next = BoxMenu_Step(sBoxMCursor, wantX >= 0 ? 1 : -1);
                }
                if (next != sBoxMCursor) {
                    sBoxMCursor = next;
                    BoxMenu_PlaySfx(NA_SE_SY_CURSOR);
                }
                sBoxMStickHeld = 1;
            }
        } else {
            sBoxMStickHeld = 0;
        }
    }
}

// One translucent solid rectangle. Everything the menu draws (plates, outlines, cursor brackets,
// stick arrows) goes through here: the FILL cycle can only pack 1-bit alpha into its fill colour,
// and these need real transparency to sit over live gameplay. Coordinates are pixels; the texture
// rectangle wants 10.2 fixed point, hence the <<2.
#define BOXM_RECT(x1, y1, x2, y2, r, g, b, a)                  \
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, (r), (g), (b), (a)); \
    gSPWideTextureRectangle(OVERLAY_DISP++, (x1) << 2, (y1) << 2, (x2) << 2, (y2) << 2, G_TX_RENDERTILE, 0, 0, 0, 0)

// A triangle pointing left (dir < 0) or right (dir > 0), built from a staircase of rows — the RDP
// fills rectangles, not triangles, and at this size the stairs read as a clean arrow.
#define BOXM_ARROW_W 5
#define BOXM_ARROW_H 9

static void BoxMenu_DrawArrow(GraphicsContext* gfxCtx, s32 tipX, s32 cy, s32 dir, u8 r, u8 g, u8 b, u8 a) {
    OPEN_DISPS(gfxCtx);
    for (s32 i = 0; i < BOXM_ARROW_W; i++) {
        s32 half = (BOXM_ARROW_H / 2) - (BOXM_ARROW_H / 2) * i / BOXM_ARROW_W;
        s32 x = tipX + dir * i;
        s32 x1 = (dir > 0) ? x : x - 1;

        BOXM_RECT(x1, cy - half, x1 + 1, cy + half + 1, r, g, b, a);
    }
    CLOSE_DISPS(gfxCtx);
}

void BoxMenu_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 cx1;
    s32 cx2;
    s32 cy1;
    s32 cy2;
    u8 curR;
    u8 curG;
    u8 curB;

    if (!sBoxMOpen || sBoxMCount <= 0) {
        return;
    }

    OPEN_DISPS(gfxCtx);

    // No full-screen dim: the row floats over untouched gameplay, exactly like BotW's.
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetOtherMode(OVERLAY_DISP++,
                    G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PRIM | G_RM_CLD_SURF | G_RM_CLD_SURF2);

    // ---- 1. Plates + outline. The ACTIVE entry (what is equipped) gets the blue plate. ----
    for (s32 i = 0; i < sBoxMCount; i++) {
        s32 x1;
        s32 y1;
        s32 x2;
        s32 y2;

        BoxMenu_EntryXY(i, &x1, &y1);
        x2 = x1 + BOXM_BOX;
        y2 = y1 + BOXM_BOX;

        if (i == sBoxMActive) {
            BOXM_RECT(x1, y1, x2, y2, 45, 105, 175, 225); // sheikah blue
        } else if (sBoxMEntries[i].enabled) {
            BOXM_RECT(x1, y1, x2, y2, 16, 20, 28, 205);
        } else {
            BOXM_RECT(x1, y1, x2, y2, 16, 20, 28, 140); // locked: fainter plate
        }

        // Hairline outline so neighbouring boxes stay separate over busy scenery.
        BOXM_RECT(x1, y1, x2, y1 + BOXM_BORDER, 105, 115, 130, 200);
        BOXM_RECT(x1, y2 - BOXM_BORDER, x2, y2, 105, 115, 130, 200);
        BOXM_RECT(x1, y1, x1 + BOXM_BORDER, y2, 105, 115, 130, 200);
        BOXM_RECT(x2 - BOXM_BORDER, y1, x2, y2, 105, 115, 130, 200);
    }

    // ---- 2. Icons, filling their box ----
    Gfx_SetupDL_39Overlay(gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (s32 i = 0; i < sBoxMCount; i++) {
        s32 bx;
        s32 y1;
        s32 src = (sBoxMEntries[i].iconSize != 0) ? sBoxMEntries[i].iconSize : 32;

        BoxMenu_EntryXY(i, &bx, &y1);
        // Texel step for a src -> BOXM_BOX scale. NOT doubled: dsdx is (source << 10) / dest, and
        // shifting it once more samples the texture twice as fast — which drew each icon at half
        // size in the corner of its box.
        s32 dd = (src << 10) / BOXM_BOX;
        u8 a = sBoxMEntries[i].enabled ? 255 : 100;
        u8 tint = sBoxMEntries[i].enabled ? 255 : 130; // locked entries read as grayed art

        if (sBoxMEntries[i].iconPath != NULL) {
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, tint, tint, tint, a);
            gDPLoadTextureBlock(OVERLAY_DISP++, sBoxMEntries[i].iconPath, G_IM_FMT_RGBA, G_IM_SIZ_32b, src, src, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gSPWideTextureRectangle(OVERLAY_DISP++, bx << 2, y1 << 2, (bx + BOXM_BOX) << 2, (y1 + BOXM_BOX) << 2,
                                    G_TX_RENDERTILE, 0, 0, dd, dd);
        }
    }

    // ---- 3. Cursor: yellow corner brackets ----
    // Back to the untextured blend the plates used — section 2 left a texture-sampling mode set,
    // and the brackets/arrows are solid prim-coloured rectangles.
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetOtherMode(OVERLAY_DISP++,
                    G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PRIM | G_RM_CLD_SURF | G_RM_CLD_SURF2);

    {
        s32 ex;
        s32 ey;

        BoxMenu_EntryXY(sBoxMCursor, &ex, &ey);
        cx1 = ex - 2;
        cx2 = cx1 + BOXM_BOX + 4;
        cy1 = ey - 2;
        cy2 = cy1 + BOXM_BOX + 4;
    }
    {
        // Pulse only the brackets, so the icon underneath is never tinted.
        s32 p = (sBoxMPulse % 40 < 20) ? (sBoxMPulse % 40) : (40 - sBoxMPulse % 40);
        s32 L = BOXM_BRACKET_LEN;
        s32 T = BOXM_BRACKET_TH;

        curR = 255;
        curG = (u8)(225 + p);
        curB = (u8)(120 + p * 3);

        BOXM_RECT(cx1, cy1, cx1 + L, cy1 + T, curR, curG, curB, 255); // top-left
        BOXM_RECT(cx1, cy1, cx1 + T, cy1 + L, curR, curG, curB, 255);
        BOXM_RECT(cx2 - L, cy1, cx2, cy1 + T, curR, curG, curB, 255); // top-right
        BOXM_RECT(cx2 - T, cy1, cx2, cy1 + L, curR, curG, curB, 255);
        BOXM_RECT(cx1, cy2 - T, cx1 + L, cy2, curR, curG, curB, 255); // bottom-left
        BOXM_RECT(cx1, cy2 - L, cx1 + T, cy2, curR, curG, curB, 255);
        BOXM_RECT(cx2 - L, cy2 - T, cx2, cy2, curR, curG, curB, 255); // bottom-right
        BOXM_RECT(cx2 - T, cy2 - L, cx2, cy2, curR, curG, curB, 255);
    }
    gDPPipeSync(OVERLAY_DISP++);

    CLOSE_DISPS(gfxCtx);

    // ---- 4. Stick hint under the cursor: the left/right arrows. Drawn after CLOSE_DISPS because
    // each arrow opens its own display-list block — nesting OPEN_DISPS would shadow this one. ----
    if (sBoxMCount > 1) {
        s32 ay = cy2 + 7;
        s32 mid = (cx1 + cx2) / 2;

        BoxMenu_DrawArrow(gfxCtx, mid - 5, ay, -1, curR, curG, curB, 235);
        BoxMenu_DrawArrow(gfxCtx, mid + 5, ay, 1, curR, curG, curB, 235);
    }
}
