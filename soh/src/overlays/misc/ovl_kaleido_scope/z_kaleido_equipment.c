#include "z_kaleido_scope.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "assets/soh_assets.h"          // gItemIconReservedSlotTex (left-column reserved cells)
#include "soh/ResourceManagerHelpers.h" // ResourceMgr_LoadTexOrDListByName (missing-icon probe)
#include "textures/parameter_static/parameter_static.h"
#include "soh/Enhancements/cosmetics/cosmeticsTypes.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "mods/extended_inventory.h"
#include "mods/extended_equipment.h"
#include "mods/broken_items/broken_items.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

// Left column of the equipment page (Skijer, latest 2026-07-29 — mirror of 2ship):
//   row 0 = MAGIC CAPE          (A toggles the cloth visibility; its half-cost passive is always on)
//   row 1 = PENDANT OF MEMORIES (A toggles its whole moveset)
//   row 2 = RESERVED ext slot 1 } freed when quiver/bullet bag, bomb bag, STRENGTH and SWIM all moved
//   row 3 = RESERVED ext slot 2 } to the quest page. Placeholder icon, hoverable, A error-beeps.
// Rows 0/1 draw solid = toggle ON, half-transparent = OFF (the spiritual-stones visual from
// z_kaleido_collect.c).
#define EQUIP_UPGRADE_ROW_RESERVED_1 2
#define EQUIP_UPGRADE_ROW_RESERVED_2 3

static s32 KaleidoEquip_UpgradeCellAvailable(s16 cursorY) {
    if (cursorY == 0) {
        return ExtEquip_CapeOwned();
    }
    if (cursorY == 1) {
        return ExtEquip_PendantOwned();
    }
    return true; // reserved: always drawn/hoverable until the item that lands here is decided
}

static u8 sEquipmentItemOffsets[] = {
    0x00, 0x00, 0x01, 0x02, 0x00, 0x03, 0x04, 0x05, 0x00, 0x06, 0x07, 0x08, 0x00, 0x09, 0x0A, 0x0B,
};

// (The strength A-button-indicator vertices moved to z_kaleido_collect.c with the strength cell.)

static s16 sEquipTimer = 0;

extern int gPauseLinkFrameBuffer;

void KaleidoScope_DrawEquipmentImage(PlayState* play, void* source, u32 width, u32 height) {
    PauseContext* pauseCtx = &play->pauseCtx;
    u8* curTexture;
    s32 vtxIndex;
    s32 textureCount;
    s32 textureHeight;
    s32 remainingSize;
    s32 textureSize;
    s32 pad;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_POINT);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    curTexture = source;
    remainingSize = width * height * 2;
    textureHeight = 4096 / (width * 2);
    textureSize = width * textureHeight * 2;
    textureCount = remainingSize / textureSize;
    if ((remainingSize % textureSize) != 0) {
        textureCount += 1;
    }

    vtxIndex = 80;

    gDPSetTileCustom(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, textureHeight, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                     G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    remainingSize -= textureSize;

    textureCount = 1;

    // VERTEX Y EXTEND
    pauseCtx->equipVtx[vtxIndex + 2].v.ob[1] -= 80;
    pauseCtx->equipVtx[vtxIndex + 3].v.ob[1] -= 80;

    for (i = 0; i < textureCount; i++) {
        gSPVertex(POLY_OPA_DISP++, &pauseCtx->equipVtx[vtxIndex], 4, 0);

        gDPSetTextureImage(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, curTexture);

        gDPLoadSync(POLY_OPA_DISP++);
        gDPLoadTile(POLY_OPA_DISP++, G_TX_LOADTILE, 0, 0, (width - 1) << 2, (textureHeight - 1) << 2);

        gDPSetTextureImageFB(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, gPauseLinkFrameBuffer);
        gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);

        curTexture += textureSize;

        if ((remainingSize - textureSize) < 0) {
            if (remainingSize > 0) {
                textureHeight = remainingSize / (s32)(width * 2);
                remainingSize -= textureSize;

                gDPSetTileCustom(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, textureHeight, 0,
                                 G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                 G_TX_NOLOD, G_TX_NOLOD);
            }
        } else {
            remainingSize -= textureSize;
        }

        vtxIndex += 4;
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void KaleidoScope_DrawAButton(PlayState* play, Vtx* vtx, int16_t xTranslate, int16_t yTranslate) {
    PauseContext* pauseCtx = &play->pauseCtx;
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();

    Matrix_Translate(xTranslate, yTranslate, 0, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Color_RGB8 aButtonColor = { 0, 100, 255 };
    if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.Changed"), 0)) {
        aButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.AButton.Value"), aButtonColor);
    } else if (CVarGetInteger(CVAR_COSMETIC("DefaultColorScheme"), COLORSCHEME_N64) == COLORSCHEME_GAMECUBE) {
        aButtonColor = (Color_RGB8){ 0, 255, 100 };
    }

    gSPVertex(POLY_OPA_DISP++, vtx, 4, 0);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, aButtonColor.r, aButtonColor.g, aButtonColor.b, pauseCtx->alpha);
    gDPLoadTextureBlock(POLY_OPA_DISP++, gABtnSymbolTex, G_IM_FMT_IA, G_IM_SIZ_8b, 24, 16, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 4, 4, G_TX_NOLOD, G_TX_NOLOD);
    gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
    Matrix_Pop();
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Crossover Items: in Mario mode the equipment doll is a real libsm64 Mario, not Link.
// Defined in expansions/sm64/sm64_mario.c; returns 1 when it drew Mario into the
// pause framebuffer (then we skip the Link draw). All the work lives out of src/.
u8 Sm64Kaleido_DrawForm(PlayState* play);

void KaleidoScope_DrawPlayerWork(PlayState* play) {
    // Mario-mode hook: hand the pause doll to libsm64 Mario. 0 → fall through to Link.
    if (Sm64Kaleido_DrawForm(play)) {
        return;
    }
    PauseContext* pauseCtx = &play->pauseCtx;
    Vec3f pos;
    Vec3s rot;
    f32 scale;

    if (LINK_AGE_IN_YEARS == YEARS_CHILD) {
        pos.x = 2.0f;
        pos.y = -130.0f;
        pos.z = -150.0f;
        scale = 0.046f;
    } else if (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_MASTER &&
               !CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
        pos.x = 25.0f;
        pos.y = -228.0f;
        pos.z = 60.0f;
        scale = 0.056f;
    } else {
        pos.x = 20.0f;
        pos.y = -180.0f;
        pos.z = -40.0f;
        scale = 0.047f;
    }

    // SOH [Port] Draw the pause Link on a separate framebuffer starting in the work buffer
    OPEN_DISPS(play->state.gfxCtx);
    gsSPSetFB(WORK_DISP++, gPauseLinkFrameBuffer);

    rot.y = 32300;
    rot.x = rot.z = 0;
    Player_DrawPause(play, pauseCtx->playerSegment, &pauseCtx->playerSkelAnime, &pos, &rot, scale,
                     SWORD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD)),
                     TUNIC_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC)),
                     SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)),
                     BOOTS_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS)));

    gsSPResetFB(WORK_DISP++);
    CLOSE_DISPS(play->state.gfxCtx);
}

// =============================================================================
// Transform page (B1): a 3rd equipment page that shows the Crossover-Items form
// selector (Link / Mario / Pikachu) IN the equipment grid — the form icons sit
// where the swords/shields go and the form's item NAME shows where the equipment
// item name normally appears. Reuses broken_items.c for the form data + toggle.
// =============================================================================
static s16 sBrokenTransformPage = 0; // 1 = equipment screen is showing the form selector
static s16 sTransformCursor = 0;     // highlighted form index (0..count-1)
static s16 sTransformCooldown = 0;   // stick-move debounce

// Per-frame input on the transform page: stick/D-pad moves the cursor over the
// forms, A equips the highlighted one, the freed shoulder button exits back to
// the normal equipment page. Also drives the name + cursor position so the shared
// kaleido draw shows the form's item name + the green selection cursor.
static void KaleidoEquip_TransformInput(PlayState* play, Input* input) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 count = BrokenItems_FormCount();
    bool dpad = CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0);
    bool ngc = CVarGetInteger(CVAR_ENHANCEMENT("NGCKaleidoSwitcher"), 0) != 0;
    s16 freedBtn = ngc ? BTN_Z : BTN_L;
    s16 sx = pauseCtx->stickRelX;

    if (sTransformCooldown > 0) {
        sTransformCooldown--;
    }
    if (sTransformCursor >= count) {
        sTransformCursor = count - 1;
    }

    // Freed shoulder button → leave the transform page (back to equipment).
    if (CHECK_BTN_ALL(input->press.button, freedBtn)) {
        sBrokenTransformPage = 0;
        sTransformCooldown = 6;
        Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }

    // Stick / D-pad left-right over the forms.
    s32 left = (sx < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT));
    s32 right = (sx > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DRIGHT));
    if ((sTransformCooldown == 0) && (left || right)) {
        if (left && (sTransformCursor > 0)) {
            sTransformCursor--;
            sTransformCooldown = 8;
            Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        } else if (right && (sTransformCursor < count - 1)) {
            sTransformCursor++;
            sTransformCooldown = 8;
            Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    }
    if (ABS(sx) < 10) {
        sTransformCooldown = 0;
    }

    // A → equip the highlighted form (sets gSm64Mario / gPikachuMode).
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        BrokenItems_EquipForm(play, sTransformCursor);
    }

    // Name (item-name texture) + green cursor on the form. Forms occupy row 0
    // cols 1..count (the same slots the swords use), so slot = cursor + 1.
    pauseCtx->cursorItem[PAUSE_EQUIP] = BrokenItems_FormItem(sTransformCursor);
    pauseCtx->cursorSlot[PAUSE_EQUIP] = sTransformCursor + 1;
    pauseCtx->cursorColorSet = 8;
    KaleidoScope_SetCursorVtx(pauseCtx, (sTransformCursor + 1) * 4, pauseCtx->equipVtx);
}

void KaleidoScope_DrawEquipment(PlayState* play) {
    // Suppress ext equipment icon overrides so vanilla sword/shield icons show on equip screen
    gExtEquipSuppressIconOverride = 1;

    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    u16 i;
    u16 j;
    u16 k;
    u16 bit;
    u16 temp;
    u16 point;
    u16 rowStart;
    u16 pad;
    s16 cursorMoveResult;
    u16 cursorItem;
    u16 cursorSlot = 0;
    s16 cursorPoint;
    s16 cursorX;
    s16 cursorY;
    s16 oldCursorPoint;
    u8 extEquipPage;

    // #B1 Transform page: the equipment screen shows the Crossover-Items form selector
    // (Link / Mario / Pikachu) in the grid where the swords go, with the form's item
    // name in the usual name spot. Fully self-contained — its OWN OPEN/CLOSE_DISPS +
    // early return, so the normal equipment path below is completely untouched (and
    // there's exactly one OPEN_DISPS/CLOSE_DISPS pair per code path). The 3rd-page
    // toggle lives in the freed-button handler further down.
    if (sBrokenTransformPage) {
        OPEN_DISPS(play->state.gfxCtx);
        if ((pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0) && (pauseCtx->pageIndex == PAUSE_EQUIP)) {
            KaleidoEquip_TransformInput(play, input);
        }

        // Form icons in the top grid row (slots 1..count = where the swords sit).
        Gfx_SetupDL_42Opa(play->state.gfxCtx);
        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gSPVertex(POLY_OPA_DISP++, &pauseCtx->equipVtx[0], 16, 0);
        for (i = 0; i < BrokenItems_FormCount(); i++) {
            void* formTex = BrokenItems_FormIconTex(i);
            if (formTex != NULL) {
                u8 locked = !BrokenItems_FormUnlocked(i);
                gDPPipeSync(POLY_OPA_DISP++);
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, locked ? 90 : 255, locked ? 90 : 255, locked ? 90 : 255,
                                locked ? (pauseCtx->alpha * 2 / 3) : pauseCtx->alpha);
                KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, formTex, 32, 32, (i + 1) * 4);
            }
        }

        // Render the spinning player (B2 will make it the chosen form) + blit it and
        // bind the name segment so the form's item-name texture shows.
        KaleidoScope_DrawPlayerWork(play);
        gSPInvalidateTexCache(POLY_OPA_DISP++, pauseCtx->iconItemSegment);
        gSPInvalidateTexCache(POLY_OPA_DISP++, pauseCtx->nameSegment);
        gSPSegment(POLY_OPA_DISP++, 0x08, pauseCtx->iconItemSegment);
        gSPSegment(POLY_OPA_DISP++, 0x09, pauseCtx->iconItem24Segment);
        gSPSegment(POLY_OPA_DISP++, 0x0A, pauseCtx->nameSegment);
        gSPSegment(POLY_OPA_DISP++, 0x0B, play->interfaceCtx.mapSegment);
        Gfx_SetupDL_42Opa(play->state.gfxCtx);
        KaleidoScope_DrawEquipmentImage(play, pauseCtx->playerSegment, PAUSE_EQUIP_PLAYER_WIDTH,
                                        PAUSE_EQUIP_PLAYER_HEIGHT);

        CLOSE_DISPS(play->state.gfxCtx);
        gExtEquipSuppressIconOverride = 0;
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    extEquipPage = (ExtEquip_GetPage() == 1);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, ZREG(39), ZREG(40), ZREG(41), pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, ZREG(43), ZREG(44), ZREG(45), 0);

    if (extEquipPage) {
        // Extended equipment page: draw equipped outline for ext-equipped items
        for (i = 0; i < 4; i++) {
            u8 extIdx = ExtEquip_GetCurrent(i);
            if (extIdx > 0 && extIdx <= 3) {
                // Vertex index: row * 16 + col * 4, where col = extIdx (1-3)
                u16 vtxIdx = i * 16 + extIdx * 4;
                gDPPipeSync(POLY_OPA_DISP++);
                gSPVertex(POLY_OPA_DISP++, &pauseCtx->equipVtx[vtxIdx], 4, 0);
                POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, gEquippedItemOutlineTex, 32, 32, 0);
            }
        }
    } else {
        for (i = 0, j = 64; i < 4; i++, j += 4) {
            if (CUR_EQUIP_VALUE(i) != 0) {
                gDPPipeSync(POLY_OPA_DISP++);
                gSPVertex(POLY_OPA_DISP++, &pauseCtx->equipVtx[j], 4, 0);
                POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, gEquippedItemOutlineTex, 32, 32, 0);
            }
        }
    }

    if ((pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0) && (pauseCtx->pageIndex == PAUSE_EQUIP)) {
        bool dpad = (CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) && !CHECK_BTN_ALL(input->cur.button, BTN_CUP));
        bool pauseAnyCursor =
            (CVarGetInteger(CVAR_ENHANCEMENT("PauseAnyCursor"), 0) == PAUSE_ANY_CURSOR_RANDO_ONLY && IS_RANDO) ||
            (CVarGetInteger(CVAR_ENHANCEMENT("PauseAnyCursor"), 0) == PAUSE_ANY_CURSOR_ALWAYS_ON);

        // Tick ExtEquip cooldown here (Player_Update doesn't run during pause)
        if (gExtEquipState.pageSwitchTimer > 0) {
            gExtEquipState.pageSwitchTimer--;
        }

        // Toggle the extended equipment page with the shoulder button NOT bound to
        // kaleido tab switching (the freed button — see NGCKaleidoSwitcher).
        bool ngcModeEq = CVarGetInteger(CVAR_ENHANCEMENT("NGCKaleidoSwitcher"), 0) != 0;
        s16 freedBtnEq = ngcModeEq ? BTN_Z : BTN_L;
        // Page cycle with the freed shoulder button: vanilla → ext (if ExtEquip on)
        // → transform (if Crossover Items on) → vanilla. Works even when ExtEquip is
        // off (then it's just vanilla ↔ transform). Entering the transform page is
        // handled here; leaving it is handled in KaleidoEquip_TransformInput.
        if (CHECK_BTN_ALL(input->press.button, freedBtnEq) && ExtEquip_CanSwitch()) {
            bool switched = false;
            if (ExtEquip_IsEnabled() && ExtEquip_GetPage() == 0) {
                ExtEquip_SwitchPage(); // vanilla → ext
                extEquipPage = (ExtEquip_GetPage() == 1);
                switched = true;
            } else if (BrokenItems_Enabled()) {
                if (ExtEquip_IsEnabled()) {
                    ExtEquip_SwitchPage(); // drop ext back to vanilla underneath the transform page
                    extEquipPage = (ExtEquip_GetPage() == 1);
                }
                sBrokenTransformPage = 1;
                sTransformCursor = BrokenItems_CurrentForm();
                switched = true;
            } else if (ExtEquip_IsEnabled()) {
                ExtEquip_SwitchPage(); // ext → vanilla (no transform page available)
                extEquipPage = (ExtEquip_GetPage() == 1);
                switched = true;
            }
            if (switched) {
                Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
        }

        // On extended page, cursor can land on any equipment slot (all owned)
        if (extEquipPage) {
            pauseAnyCursor = true;
        }

        oldCursorPoint = pauseCtx->cursorPoint[PAUSE_EQUIP];
        pauseCtx->cursorColorSet = 0;

        if (pauseCtx->cursorSpecialPos == 0) {
            pauseCtx->nameColorSet = 0;

            cursorItem = pauseCtx->cursorItem[PAUSE_EQUIP];
            if ((cursorItem >= ITEM_SWORD_KOKIRI) && (cursorItem <= ITEM_BOOTS_HOVER)) {
                pauseCtx->cursorColorSet = 8;
            }
            // Extended equipment items also get green cursor
            if (extEquipPage && (cursorItem >= ITEM_EXT_SWORD_1) && (cursorItem <= ITEM_EXT_BOOTS_3)) {
                pauseCtx->cursorColorSet = 8;
            }

            cursorPoint = pauseCtx->cursorPoint[PAUSE_EQUIP];
            cursorX = pauseCtx->cursorX[PAUSE_EQUIP];
            cursorY = pauseCtx->cursorY[PAUSE_EQUIP];

            cursorMoveResult = 0;
            while (cursorMoveResult == 0) {
                if ((pauseCtx->stickRelX < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT))) {
                    if (pauseCtx->cursorX[PAUSE_EQUIP] != 0) {
                        pauseCtx->cursorX[PAUSE_EQUIP] -= 1;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] -= 1;

                        if (pauseCtx->cursorX[PAUSE_EQUIP] == 0) {
                            if (KaleidoEquip_UpgradeCellAvailable(pauseCtx->cursorY[PAUSE_EQUIP])) {
                                cursorMoveResult = 1;
                            }
                        } else if ((gBitFlags[pauseCtx->cursorPoint[PAUSE_EQUIP] - 1] &
                                    gSaveContext.inventory.equipment) ||
                                   pauseAnyCursor) {
                            cursorMoveResult = 2;
                        }
                    } else {
                        pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                        pauseCtx->cursorY[PAUSE_EQUIP] += 1;

                        if (pauseCtx->cursorY[PAUSE_EQUIP] >= 4) {
                            pauseCtx->cursorY[PAUSE_EQUIP] = 0;
                        }

                        pauseCtx->cursorPoint[PAUSE_EQUIP] =
                            pauseCtx->cursorX[PAUSE_EQUIP] + (pauseCtx->cursorY[PAUSE_EQUIP] * 4);

                        if (pauseCtx->cursorPoint[PAUSE_EQUIP] >= 16) {
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = pauseCtx->cursorX[PAUSE_EQUIP];
                        }

                        if (cursorY == pauseCtx->cursorY[PAUSE_EQUIP]) {
                            pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
                            cursorMoveResult = 3;
                        }
                    }
                } else if ((pauseCtx->stickRelX > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DRIGHT))) {
                    if (pauseCtx->cursorX[PAUSE_EQUIP] < 3) {
                        pauseCtx->cursorX[PAUSE_EQUIP] += 1;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] += 1;

                        if (pauseCtx->cursorX[PAUSE_EQUIP] == 0) {
                            if (KaleidoEquip_UpgradeCellAvailable(pauseCtx->cursorY[PAUSE_EQUIP])) {
                                cursorMoveResult = 1;
                            }
                        } else if ((gBitFlags[pauseCtx->cursorPoint[PAUSE_EQUIP] - 1] &
                                    gSaveContext.inventory.equipment) ||
                                   pauseAnyCursor) {
                            cursorMoveResult = 2;
                        }
                    } else {
                        pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                        pauseCtx->cursorY[PAUSE_EQUIP] += 1;

                        if (pauseCtx->cursorY[PAUSE_EQUIP] >= 4) {
                            pauseCtx->cursorY[PAUSE_EQUIP] = 0;
                        }

                        pauseCtx->cursorPoint[PAUSE_EQUIP] =
                            pauseCtx->cursorX[PAUSE_EQUIP] + (pauseCtx->cursorY[PAUSE_EQUIP] * 4);

                        if (pauseCtx->cursorPoint[PAUSE_EQUIP] >= 16) {
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = pauseCtx->cursorX[PAUSE_EQUIP];
                        }

                        if (cursorY == pauseCtx->cursorY[PAUSE_EQUIP]) {
                            pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
                            cursorMoveResult = 3;
                        }
                    }
                } else {
                    cursorMoveResult = 4;
                }
            }

            cursorPoint = pauseCtx->cursorPoint[PAUSE_EQUIP];
            cursorY = pauseCtx->cursorY[PAUSE_EQUIP];

            if (cursorMoveResult) {}

            cursorMoveResult = 0;
            while (cursorMoveResult == 0) {
                if ((pauseCtx->stickRelY > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DUP))) {
                    if (pauseCtx->cursorY[PAUSE_EQUIP] != 0) {
                        pauseCtx->cursorY[PAUSE_EQUIP] -= 1;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] -= 4;

                        if (pauseCtx->cursorX[PAUSE_EQUIP] == 0) {
                            if (KaleidoEquip_UpgradeCellAvailable(pauseCtx->cursorY[PAUSE_EQUIP])) {
                                cursorMoveResult = 1;
                            }
                        } else if ((gBitFlags[pauseCtx->cursorPoint[PAUSE_EQUIP] - 1] &
                                    gSaveContext.inventory.equipment) ||
                                   pauseAnyCursor) {
                            cursorMoveResult = 2;
                        }
                    } else {
                        pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                        cursorMoveResult = 3;
                    }
                } else if ((pauseCtx->stickRelY < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DDOWN))) {
                    if (pauseCtx->cursorY[PAUSE_EQUIP] < 3) {
                        pauseCtx->cursorY[PAUSE_EQUIP] += 1;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] += 4;

                        if (pauseCtx->cursorX[PAUSE_EQUIP] == 0) {
                            if (KaleidoEquip_UpgradeCellAvailable(pauseCtx->cursorY[PAUSE_EQUIP])) {
                                cursorMoveResult = 1;
                            }
                        } else if ((gBitFlags[pauseCtx->cursorPoint[PAUSE_EQUIP] - 1] &
                                    gSaveContext.inventory.equipment) ||
                                   pauseAnyCursor) {
                            cursorMoveResult = 2;
                        }
                    } else {
                        pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                        cursorMoveResult = 3;
                    }
                } else {
                    cursorMoveResult = 4;
                }
            }
        } else if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
            if ((pauseCtx->stickRelX > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DRIGHT))) {
                pauseCtx->nameDisplayTimer = 0;
                pauseCtx->cursorSpecialPos = 0;

                Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

                cursorPoint = cursorX = cursorY = 0;
                while (true) {
                    if (cursorX == 0) {
                        // The left column is Cape / Pendant / 2 reserved slots now — one landability
                        // rule for all four rows (Skijer 2026-07-29).
                        if (KaleidoEquip_UpgradeCellAvailable(cursorY)) {
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                            pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                            pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                            break;
                        }
                    } else if ((gBitFlags[cursorPoint - 1] & gSaveContext.inventory.equipment) || extEquipPage) {
                        pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                        pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                        pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                        break;
                    }

                    cursorY = cursorY + 1;
                    cursorPoint = cursorPoint + 4;
                    if (cursorY < 4) {
                        continue;
                    }

                    cursorY = 0;
                    cursorPoint = cursorX + 1;
                    cursorX = cursorPoint;
                    if (cursorX < 4) {
                        continue;
                    }

                    KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
                    break;
                }
            }
        } else {
            if ((pauseCtx->stickRelX < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT))) {
                pauseCtx->nameDisplayTimer = 0;
                pauseCtx->cursorSpecialPos = 0;
                Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

                cursorPoint = cursorX = 3;
                cursorY = 0;
                while (true) {
                    if (cursorX == 0) {
                        if (KaleidoEquip_UpgradeCellAvailable(cursorY)) {
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                            pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                            pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                            break;
                        }
                    } else if ((gBitFlags[cursorPoint - 1] & gSaveContext.inventory.equipment) || extEquipPage) {
                        pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                        pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                        pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                        break;
                    }

                    cursorY = cursorY + 1;
                    cursorPoint = cursorPoint + 4;
                    if (cursorY < 4) {
                        continue;
                    }

                    cursorY = 0;
                    cursorPoint = cursorX - 1;
                    cursorX = cursorPoint;
                    if (cursorX >= 0) {
                        continue;
                    }

                    KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
                    break;
                }
            }
        }

        gExtEquipGridNameContext = false; // re-armed below only while naming a page-2 grid cell

        if (pauseCtx->cursorX[PAUSE_EQUIP] == 0) {
            pauseCtx->cursorColorSet = 0;

            if (pauseCtx->cursorY[PAUSE_EQUIP] <= 1) {
                // Rows 0/1 = Magic Cape / Pendant of Memories. They no longer borrow the ext TUNIC-1 /
                // BOOTS-2 ids for their label — those slots are the Champion's Tunic and the Climb
                // Boots now, so the name box would lie. Left unnamed to match 2ship exactly until both
                // games get dedicated Cape/Pendant name textures wired.
                cursorItem = PAUSE_ITEM_NONE;
            } else {
                // Rows 2/3 = the reserved slots — nothing to name yet (Skijer 2026-07-29).
                cursorItem = PAUSE_ITEM_NONE;
            }
        } else {
            if (extEquipPage) {
                // Extended equipment page: map cursor position to ext item ID. The name resolver runs
                // later (z_kaleido_scope_PAL.c), so flag that the id it gets is a GRID slot — the one
                // shared id (0xEA) means Climb Boots here, Pendant of Memories everywhere else.
                gExtEquipGridNameContext = true;
                cursorItem = ExtEquip_GetItemId(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP]);
            } else {
                cursorItem = ITEM_SWORD_KOKIRI + sEquipmentItemOffsets[pauseCtx->cursorPoint[PAUSE_EQUIP]];
            }
            osSyncPrintf("ccc=%d\n", cursorItem);

            if (pauseCtx->cursorSpecialPos == 0) {
                pauseCtx->cursorColorSet = 8;
            }
        }

        if (!extEquipPage && (pauseCtx->cursorY[PAUSE_EQUIP] == 0) && (pauseCtx->cursorX[PAUSE_EQUIP] == 3)) {
            if (gSaveContext.bgsFlag != 0) {
                cursorItem = ITEM_HEART_PIECE_2;
            } else if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
                cursorItem = ITEM_SWORD_KNIFE;
            }
        }

        cursorSlot = pauseCtx->cursorPoint[PAUSE_EQUIP];

        pauseCtx->cursorItem[PAUSE_EQUIP] = cursorItem;
        pauseCtx->cursorSlot[PAUSE_EQUIP] = cursorSlot;

        osSyncPrintf("kscope->select_name[Display_Equipment] = %d\n", pauseCtx->cursorItem[PAUSE_EQUIP]);

        if (!extEquipPage && !(CHECK_AGE_REQ_EQUIP(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP]))) {
            pauseCtx->nameColorSet = 1;
        }
        if (extEquipPage && !ExtEquip_CheckAgeReq(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP])) {
            pauseCtx->nameColorSet = 1;
        }

        // (The age-based greying for the strength/quiver capacity labels moved to the quest page with
        // those cells — the left column holds the Cape, the Pendant and 2 reserved slots now.)

        KaleidoScope_SetCursorVtx(pauseCtx, cursorSlot * 4, pauseCtx->equipVtx);

        // (The "A toggles Strength" interaction moved to the quest page with the strength cell —
        // see z_kaleido_collect.c. Skijer 2026-07-29)

        // Skijer 2026-07-15: A on upgrade row 0 = toggle Magic Cape VISIBILITY (its magic refund is
        // always active once owned); A on row 1 = toggle the Pendant of Memories moveset on/off.
        // Same interaction as the strength toggle above; solid vs half-transparent icon shows state.
        if ((pauseCtx->cursorSpecialPos == 0) && (pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0) &&
            CHECK_BTN_ALL(input->press.button, BTN_A) && (pauseCtx->cursorX[PAUSE_EQUIP] == 0)) {
            u8 upgradeToggled = false;

            if ((pauseCtx->cursorY[PAUSE_EQUIP] == 0) && ExtEquip_CapeOwned()) {
                ExtEquip_ToggleCapeVisibility();
                upgradeToggled = true;
            } else if ((pauseCtx->cursorY[PAUSE_EQUIP] == 1) && ExtEquip_PendantOwned()) {
                ExtEquip_TogglePendantEffect();
                upgradeToggled = true;
            }

            if (upgradeToggled) {
                Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                pauseCtx->unk_1E4 = 7;
                sEquipTimer = 10;
            }
        }

        u16 buttonsToCheck = BTN_A | BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT;
        if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) &&
            (!CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) || CHECK_BTN_ALL(input->cur.button, BTN_CUP))) {
            buttonsToCheck |= BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT;
        }

        if ((pauseCtx->cursorSpecialPos == 0) && (cursorItem != PAUSE_ITEM_NONE) && (pauseCtx->state == 6) &&
            (pauseCtx->unk_1E4 == 0) && CHECK_BTN_ANY(input->press.button, buttonsToCheck) &&
            (pauseCtx->cursorX[PAUSE_EQUIP] != 0)) {

            // Extended equipment page: A = equip on body, C buttons = assign to C button for toggle
            if (extEquipPage) {
                u8 extAgeOk = ExtEquip_CheckAgeReq(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP]);
                if (!extAgeOk) {
                    Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                } else if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
                    ExtEquip_Equip(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP]);
                    Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    pauseCtx->unk_1E4 = 7;
                    sEquipTimer = 10;
                } else if (CHECK_BTN_ANY(input->press.button, BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT) &&
                           (pauseCtx->cursorY[PAUSE_EQUIP] != EQUIP_TYPE_BOOTS)) {
                    // Assign ext equipment to C button for toggle on/off during gameplay. The BOOTS row
                    // is excluded: all three are real boots (A equips them), and its middle id (0xEA)
                    // belongs to the Pendant of Memories in the C-button id space. Skijer 2026-07-29
                    u16 extItemId = ExtEquip_GetItemId(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP]);
                    // Determine which C button was pressed (1=CLeft, 2=CDown, 3=CRight)
                    s32 cBtn = CHECK_BTN_ALL(input->press.button, BTN_CLEFT)   ? 0
                               : CHECK_BTN_ALL(input->press.button, BTN_CDOWN) ? 1
                                                                               : 2;
                    s32 buttonIndex = cBtn + 1; // buttonItems[1]=CLeft, [2]=CDown, [3]=CRight

                    // Toggle: if same item already on this button, remove it
                    if (gSaveContext.equips.buttonItems[buttonIndex] == extItemId) {
                        gSaveContext.equips.buttonItems[buttonIndex] = ITEM_NONE;
                        gSaveContext.equips.cButtonSlots[cBtn] = SLOT_NONE;
                    } else {
                        gSaveContext.equips.buttonItems[buttonIndex] = extItemId;
                        gSaveContext.equips.cButtonSlots[cBtn] = SLOT_NONE; // No inventory slot
                    }
                    Interface_LoadItemIcon2(play, buttonIndex);
                    Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    pauseCtx->unk_1E4 = 7;
                    sEquipTimer = 10;
                }
            } else if (CHECK_AGE_REQ_EQUIP(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP])) {
                if (CHECK_BTN_ALL(input->press.button, BTN_A)) {

                    // #Region SoH [Enhancements]
                    // Allow Link to remove his equipment from the equipment subscreen by toggling on/off
                    // Shields will be un-equipped entirely, and tunics/boots will revert to Kokiri Tunic/Kokiri Boots
                    // Only BGS/Giant's Knife is affected, and it will revert to Master Sword.

                    // If we have the feature toggled on
                    if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentCanBeRemoved"), 0)) {

                        if (CVarGetInteger(CVAR_ENHANCEMENT("SwordToggle"), SWORD_TOGGLE_NONE) ==
                                SWORD_TOGGLE_BOTH_AGES ||
                            (CVarGetInteger(CVAR_ENHANCEMENT("SwordToggle"), SWORD_TOGGLE_NONE) ==
                             SWORD_TOGGLE_CHILD) &&
                                LINK_IS_CHILD) {
                            // If we're on the "swords" section of the equipment screen AND we're on a
                            // currently-equipped sword
                            if (pauseCtx->cursorY[PAUSE_EQUIP] == 0 &&
                                pauseCtx->cursorX[PAUSE_EQUIP] == CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD)) {
                                Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
                                gSaveContext.equips.buttonItems[0] = ITEM_NONE;
                                Flags_SetInfTable(INFTABLE_SWORDLESS);
                                goto RESUME_EQUIPMENT_SWORD; // Skip to here so we don't re-equip it
                            }
                        } else {
                            // If we're on the "swords" section of the equipment screen AND we're on a
                            // currently-equipped BGS/Giant's Knife
                            if (pauseCtx->cursorY[PAUSE_EQUIP] == 0 && pauseCtx->cursorX[PAUSE_EQUIP] == 3 &&
                                CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_BIGGORON &&
                                CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD,
                                                  EQUIP_INV_SWORD_MASTER)) { // And we have the Master Sword
                                Inventory_ChangeEquipment(
                                    EQUIP_TYPE_SWORD,
                                    EQUIP_VALUE_SWORD_MASTER); // "Unequip" it by equipping Master Sword
                                gSaveContext.equips.buttonItems[0] = ITEM_SWORD_MASTER;
                                Flags_UnsetInfTable(INFTABLE_SWORDLESS);
                                goto RESUME_EQUIPMENT_SWORD; // Skip to here so we don't re-equip it
                            }
                        }

                        // If we're on the "shields" section of the equipment screen AND we're on a currently-equipped
                        // shield
                        if (pauseCtx->cursorY[PAUSE_EQUIP] == 1 &&
                            pauseCtx->cursorX[PAUSE_EQUIP] == CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)) {
                            Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE); // Unequip it
                            goto RESUME_EQUIPMENT; // Skip to here so we don't re-equip it
                        }

                        // If we're on the "tunics" section of the equipment screen AND we're on a currently-equipped
                        // tunic
                        if (pauseCtx->cursorY[PAUSE_EQUIP] == 2 &&
                            pauseCtx->cursorX[PAUSE_EQUIP] == CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC)) {
                            Inventory_ChangeEquipment(
                                EQUIP_TYPE_TUNIC, EQUIP_VALUE_TUNIC_KOKIRI); // "Unequip" it (by equipping Kokiri Tunic)
                            goto RESUME_EQUIPMENT;                           // Skip to here so we don't re-equip it
                        }

                        // If we're on the "boots" section of the equipment screen AND we're on currently-equipped boots
                        if (pauseCtx->cursorY[PAUSE_EQUIP] == 3 &&
                            pauseCtx->cursorX[PAUSE_EQUIP] == CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS)) {
                            Inventory_ChangeEquipment(
                                EQUIP_TYPE_BOOTS, EQUIP_VALUE_BOOTS_KOKIRI); // "Unequip" it (by equipping Kokiri Boots)
                            goto RESUME_EQUIPMENT;                           // Skip to here so we don't re-equip it
                        }
                    }

                    if (CHECK_OWNED_EQUIP(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP] - 1)) {
                        // The Trident tolerates only the Divine Shield or the Mirror.
                        if ((pauseCtx->cursorY[PAUSE_EQUIP] == EQUIP_TYPE_SHIELD) &&
                            (ExtEquip_GetCurrent(EQUIP_TYPE_SWORD) == 3) &&
                            !ExtEquip_TridentAllowsShield(0, pauseCtx->cursorX[PAUSE_EQUIP])) {
                            goto EQUIP_FAIL;
                        }
                        // The ext piece of this type comes off FIRST (synchronous cleanup, slot left
                        // bare/Kokiri), then the vanilla value goes on top — the old order let the
                        // ext piece's deferred cleanup overwrite the sword just equipped. All four
                        // types: the ext boots are real boots now, exclusive with Iron/Hover.
                        ExtEquip_Unequip(pauseCtx->cursorY[PAUSE_EQUIP]);
                        Inventory_ChangeEquipment(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP]);
                    } else {
                        goto EQUIP_FAIL;
                    }

                RESUME_EQUIPMENT:
                    if (pauseCtx->cursorY[PAUSE_EQUIP] == 0) {
                        gSaveContext.infTable[29] = 0;
                        gSaveContext.equips.buttonItems[0] = cursorItem;

                        if ((pauseCtx->cursorX[PAUSE_EQUIP] == 3) && (gSaveContext.bgsFlag != 0)) {
                            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
                            gSaveContext.swordHealth = 8;
                        } else {
                            if (gSaveContext.equips.buttonItems[0] == ITEM_HEART_PIECE_2) {
                                gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
                            }
                            if ((gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS) && (gSaveContext.bgsFlag == 0) &&
                                CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
                                gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KNIFE;
                            }
                        }
                    RESUME_EQUIPMENT_SWORD:
                        Interface_LoadItemIcon1(play, 0);
                    }

                    Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    pauseCtx->unk_1E4 = 7;
                    sEquipTimer = 10;
                } else if (CVarGetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"), 0) != 0) {
                    // Only allow assigning shield, tunic and boots to c-buttons
                    if (pauseCtx->cursorY[PAUSE_EQUIP] > 0) {
                        if (CHECK_OWNED_EQUIP(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP] - 1)) {
                            u16 slot = 0;
                            switch (cursorItem) {
                                case ITEM_TUNIC_KOKIRI:
                                    slot = SLOT_TUNIC_KOKIRI;
                                    break;
                                case ITEM_TUNIC_GORON:
                                    slot = SLOT_TUNIC_GORON;
                                    break;
                                case ITEM_TUNIC_ZORA:
                                    slot = SLOT_TUNIC_ZORA;
                                    break;
                                case ITEM_BOOTS_KOKIRI:
                                    slot = SLOT_BOOTS_KOKIRI;
                                    break;
                                case ITEM_BOOTS_IRON:
                                    slot = SLOT_BOOTS_IRON;
                                    break;
                                case ITEM_BOOTS_HOVER:
                                    slot = SLOT_BOOTS_HOVER;
                                    break;
                                case ITEM_SHIELD_DEKU:
                                    slot = SLOT_SHIELD_DEKU;
                                    break;
                                case ITEM_SHIELD_HYLIAN:
                                    slot = SLOT_SHIELD_HYLIAN;
                                    break;
                                case ITEM_SHIELD_MIRROR:
                                    slot = SLOT_SHIELD_MIRROR;
                                    break;
                                default:
                                    break;
                            }
                            if (GameInteractor_Should(VB_EQUIP_ITEM_TO_C_BUTTON, true, play, slot, cursorItem)) {
                                KaleidoScope_SetupItemEquip(play, cursorItem, slot,
                                                            pauseCtx->equipVtx[cursorSlot * 4].v.ob[0] * 10,
                                                            pauseCtx->equipVtx[cursorSlot * 4].v.ob[1] * 10);
                            }
                        } else {
                            Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                        }
                    }
                }
            } else {
            EQUIP_FAIL:
                if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
                    Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                } else if ((CVarGetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"), 0) != 0) &&
                           (pauseCtx->cursorY[PAUSE_EQUIP] > 1)) {
                    Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                }
            }
        }

        if (oldCursorPoint != pauseCtx->cursorPoint[PAUSE_EQUIP]) {
            Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    } else if ((pauseCtx->unk_1E4 == 7) && (pauseCtx->pageIndex == PAUSE_EQUIP)) {
        KaleidoScope_SetCursorVtx(pauseCtx, pauseCtx->cursorSlot[PAUSE_EQUIP] * 4, pauseCtx->equipVtx);
        pauseCtx->cursorColorSet = 8;

        sEquipTimer--;
        if (sEquipTimer == 0) {
            pauseCtx->unk_1E4 = 0;
        }
    }

    // Grey Out Strength Upgrade Name when Disabled
    // Do not Grey Out Strength Upgrade Name when Enabled
    // This needs to be outside the previous block since otherwise the nameColorSet is cleared to 0 by other menu pages
    // when toggling
    if ((pauseCtx->pageIndex == PAUSE_EQUIP) && (pauseCtx->cursorX[PAUSE_EQUIP] == 0) &&
        (pauseCtx->cursorY[PAUSE_EQUIP] == 2) && CVarGetInteger(CVAR_ENHANCEMENT("ToggleStrength"), 0)) {
        if (CVarGetInteger(CVAR_ENHANCEMENT("StrengthDisabled"), 0)) {
            pauseCtx->nameColorSet = 1;
        } else {
            pauseCtx->nameColorSet = 0;
        }
    }

    for (rowStart = 0, i = 0, point = 4; i < 4; i++, rowStart += 4, point += 16) {

        for (k = 0, temp = rowStart + 1, bit = rowStart, j = point; k < 3; k++, bit++, j += 4, temp++) {

            if (((gBitFlags[bit] & gSaveContext.inventory.equipment) || extEquipPage) &&
                (pauseCtx->cursorSpecialPos == 0)) {
                if ((extEquipPage && ExtEquip_CheckAgeReq(i, k + 1)) ||
                    (!extEquipPage && CHECK_AGE_REQ_EQUIP(i, k + 1))) {
                    if (temp == cursorSlot) {
                        pauseCtx->equipVtx[j].v.ob[0] = pauseCtx->equipVtx[j + 2].v.ob[0] =
                            pauseCtx->equipVtx[j].v.ob[0] - 2;
                        pauseCtx->equipVtx[j + 1].v.ob[0] = pauseCtx->equipVtx[j + 3].v.ob[0] =
                            pauseCtx->equipVtx[j + 1].v.ob[0] + 4;
                        pauseCtx->equipVtx[j].v.ob[1] = pauseCtx->equipVtx[j + 1].v.ob[1] =
                            pauseCtx->equipVtx[j].v.ob[1] + 2;
                        pauseCtx->equipVtx[j + 2].v.ob[1] = pauseCtx->equipVtx[j + 3].v.ob[1] =
                            pauseCtx->equipVtx[j + 2].v.ob[1] - 4;
                    }
                }
            }
        }
    }

    // (The strength cell's hover-zoom moved to the quest page with the cell itself.)

    Gfx_SetupDL_42Opa(play->state.gfxCtx);

    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    for (rowStart = 0, j = 0, temp = 0, i = 0; i < 4; i++, rowStart += 4, j += 16) {
        gSPVertex(POLY_OPA_DISP++, &pauseCtx->equipVtx[j], 16, 0);
        if (i == 0 || i == 1) {
            // Skijer 2026-07-15: upgrade rows 0/1 = MAGIC CAPE / PENDANT OF MEMORIES (replacing the
            // quiver/bullet-bag and bomb-bag capacity icons). Solid = toggle ON, half-transparent =
            // OFF — the spiritual-stones visual (prim alpha halved, z_kaleido_collect.c pattern).
            u8 upgOwned = (i == 0) ? ExtEquip_CapeOwned() : ExtEquip_PendantOwned();

            if (upgOwned) {
                u8 upgOn = (i == 0) ? ExtEquip_CapeVisible() : ExtEquip_PendantActive();
                void* upgIcon = (i == 0) ? ExtEquip_GetCapeIcon() : ExtEquip_GetPendantIcon();

                if (upgIcon != NULL) {
                    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255,
                                    upgOn ? pauseCtx->alpha : (pauseCtx->alpha >> 1));
                    KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, upgIcon, 32, 32, 0);
                    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
                }
            }
        } else {
            // Rows 2/3 = the two RESERVED slots (Skijer 2026-07-29). The quiver/bullet-bag, bomb-bag,
            // strength and swim capacities all live on the quest page now, so nothing here is
            // age-dependent any more — just the placeholder art.
            KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, (void*)gItemIconReservedSlotTex, 32, 32, 0);
        }
        // Draw inventory screen icons
        for (k = 0, bit = rowStart, point = 4; k < 3; k++, point += 4, temp++, bit++) {

            if (extEquipPage) {
                // Extended equipment page: only draw owned items in LIVE slots (retired slots —
                // Cape/Pendant on the upgrade column, deleted Dragon Scale — stay empty)
                if (ExtEquip_HasItem(i, k + 1) && !ExtEquip_SlotRetired(i, k + 1)) {
                    void* extIcon = ExtEquip_GetIcon(i, k + 1); // i=row(0-3), k+1=col(1-3)
                    // A path that isn't in the archive does NOT draw nothing: gDPLoadTextureBlock
                    // leaves whatever was last in TMEM, so the cell shows the PREVIOUS cell's icon and
                    // the grid reads as if its rows were scrambled. Probe first. Skijer 2026-07-29
                    if ((extIcon != NULL) && (ResourceMgr_LoadTexOrDListByName((const char*)extIcon) == NULL)) {
                        extIcon = NULL;
                    }
                    if (extIcon) {
                        bool extAgeRestricted = !ExtEquip_CheckAgeReq(i, k + 1);
                        if (extAgeRestricted) {
                            gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
                            gSPGrayscale(POLY_OPA_DISP++, true);
                        }
                        KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, extIcon, 32, 32, point);
                        if (extAgeRestricted) {
                            gSPGrayscale(POLY_OPA_DISP++, false);
                        }
                    }
                }
            } else {
                int itemId = ITEM_SWORD_KOKIRI + temp;
                bool age_restricted = !CHECK_AGE_REQ_ITEM(itemId);
                if (age_restricted) {
                    gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
                    gSPGrayscale(POLY_OPA_DISP++, true);
                }
                if (((u32)i == 0) && (k == 2) && (gSaveContext.bgsFlag != 0)) {
                    // ExtInv_GetItemIcon so the Great Fairy's Sword upgrade icon shows here too
                    // (falls back to the vanilla Biggoron Sword icon). Skijer's NEI
                    KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, ExtInv_GetItemIcon(itemId), 32, 32, point);
                } else if ((i == 0) && (k == 2) && (gBitFlags[bit + 1] & gSaveContext.inventory.equipment)) {
                    KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, gItemIconBrokenGiantsKnifeTex, 32, 32,
                                                       point);
                } else if (gBitFlags[bit] & gSaveContext.inventory.equipment) {
                    KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, ExtInv_GetItemIcon(itemId), 32, 32, point);
                }
                gSPGrayscale(POLY_OPA_DISP++, false);
            }
        }
    }

    // (The strength A-button indicator moved to the quest page with the strength cell.)

    KaleidoScope_DrawPlayerWork(play);

    // if ((pauseCtx->unk_1E4 == 7) && (sEquipTimer == 10)) {
    // KaleidoScope_SetupPlayerPreRender(play);
    //}

    if ((pauseCtx->unk_1E4 == 7) && (sEquipTimer == 9)) {
        //! @bug: This function shouldn't take any arguments
        // KaleidoScope_ProcessPlayerPreRender(play);
    }

    // gSPInvalidateTexCache(POLY_OPA_DISP++, 0);
    gSPInvalidateTexCache(POLY_OPA_DISP++, pauseCtx->iconItemSegment);
    // gSPInvalidateTexCache(POLY_OPA_DISP++, pauseCtx->iconItem24Segment);
    gSPInvalidateTexCache(POLY_OPA_DISP++, pauseCtx->nameSegment);

    // gSPSegment(POLY_OPA_DISP++, 0x07, pauseCtx->playerSegment);
    gSPSegment(POLY_OPA_DISP++, 0x08, pauseCtx->iconItemSegment);
    gSPSegment(POLY_OPA_DISP++, 0x09, pauseCtx->iconItem24Segment);
    gSPSegment(POLY_OPA_DISP++, 0x0A, pauseCtx->nameSegment);
    gSPSegment(POLY_OPA_DISP++, 0x0B, play->interfaceCtx.mapSegment);
    // gSPSegment(POLY_OPA_DISP++, 0x0C, pauseCtx->iconItemAltSegment);

    Gfx_SetupDL_42Opa(play->state.gfxCtx);
    KaleidoScope_DrawEquipmentImage(play, pauseCtx->playerSegment, PAUSE_EQUIP_PLAYER_WIDTH, PAUSE_EQUIP_PLAYER_HEIGHT);

    if (gUpgradeMasks[0]) {}

    CLOSE_DISPS(play->state.gfxCtx);

    // Restore ext equipment icon overrides
    gExtEquipSuppressIconOverride = 0;
}
