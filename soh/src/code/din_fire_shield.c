#include "global.h"
#include "align_asset_macro.h"
#include "din_fire_shield.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "mods/transformation_masks/transformation_masks.h"
#include <libultraship/bridge/resourcebridge.h>
#include <math.h>

#define FIRE_ROOT "objects/din_fire_shield/poc1/"
static const char sSurfacePath[] = FIRE_ROOT "SurfaceDL";
static const char sRimPath[] = FIRE_ROOT "RimDL";
static const char sTexturePath[] = FIRE_ROOT "FlameTex";
static const char sFlowPath[] = FIRE_ROOT "FlowTex";
static const char sSurfaceVertices[] = FIRE_ROOT "SurfaceVertices";
static const char sRimVertices[] = FIRE_ROOT "RimVertices";
static const char sGIBracer[] = FIRE_ROOT "GIBracerDL";
static const char sGIVertices[] = FIRE_ROOT "GIBracerVertices";
static const char sIcon[] = FIRE_ROOT "IconTex";
// Give the interpreter named resources so it retains HD dimensions/flags.
// Its OTR signature check requires even-aligned addresses.
static const ALIGN_ASSET(2) char sFlameTextureRef[] = "__OTR__" FIRE_ROOT "FlameTex";
static const ALIGN_ASSET(2) char sFlowTextureRef[] = "__OTR__" FIRE_ROOT "FlowTex";
static const ALIGN_ASSET(2) char sIconTextureRef[] = "__OTR__" FIRE_ROOT "IconTex";
static const char sChildBracer[] = "objects/object_link_child/DinSleekEquipmentPOC1_OOT_Child/BracerDL";
static const char sAdultBracer[] = "objects/object_link_boy/DinSleekEquipmentPOC1_OOT_Adult/BracerDL";

static struct {
    PlayState* play;
    Player* player;
    u32 lastFrame;
    u16 phase;
    s16 scene;
    s32 age;
    f32 opacity;
} sFire;

typedef struct {
    Gfx* surface;
    Gfx* rim;
    void* flame;
    void* flow;
} FireResources;

static s32 DinFireShield_OptionEnabled(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("DinFireShield"), 0) && CVarGetInteger(CVAR_SETTING("AltAssets"), 1);
}

static s32 DinFireShield_Load(FireResources* resources) {
    const char* paths[] = { sSurfacePath, sRimPath, sTexturePath, sFlowPath, sSurfaceVertices, sRimVertices };
    for (size_t i = 0; i < ARRAY_COUNT(paths); ++i) {
        if (!ResourceMgr_FileExists(paths[i]))
            return false;
    }
    // Resolve manager-owned resources every draw; this module retains no raw
    // pointers across frames. Hashed lists require their vertex dependencies to
    // reload with them (the engine caches those pointers inside loaded lists).
    resources->surface = ResourceMgr_LoadGfxByName(sSurfacePath);
    resources->rim = ResourceMgr_LoadGfxByName(sRimPath);
    resources->flame = ResourceGetDataByName(sTexturePath);
    resources->flow = ResourceGetDataByName(sFlowPath);
    return resources->surface != NULL && resources->rim != NULL && resources->flame != NULL &&
           resources->flow != NULL && ResourceGetDataByName(sSurfaceVertices) != NULL &&
           ResourceGetDataByName(sRimVertices) != NULL;
}

static s32 DinFireShield_ItemEnabled(u16 itemId) {
    if (itemId != ITEM_SHIELD_DEKU && itemId != ITEM_SHIELD_HYLIAN)
        return false;
    return DinFireShield_OptionEnabled() &&
           ResourceMgr_FileExists(itemId == ITEM_SHIELD_DEKU ? sChildBracer : sAdultBracer);
}

void* DinFireShield_ItemIcon(u16 itemId) {
    if (!DinFireShield_ItemEnabled(itemId) || !ResourceMgr_FileExists(sIcon))
        return NULL;
    return ResourceGetDataByName(sIcon) != NULL ? (void*)sIconTextureRef : NULL;
}

// The option and the matching bracer pack are both required. This effect owns
// no equipment or collision state and never substitutes for owning a shield.
static s32 DinFireShield_Eligible(PlayState* play, Player* player) {
    if (play == NULL || player == NULL || player != GET_PLAYER(play) || !DinFireShield_OptionEnabled() ||
        TransformMasks_IsTransformedAny() || GameInteractor_InvisibleLinkActive() || player->actor.scale.y <= 0.0f ||
        (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_WATER)) ||
        (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) || player->csAction != 0 ||
        play->transitionTrigger != TRANS_TRIGGER_OFF) {
        return false;
    }
    if (player->currentShield != (LINK_IS_ADULT ? PLAYER_SHIELD_HYLIAN : PLAYER_SHIELD_DEKU)) {
        return false;
    }
    return ResourceMgr_FileExists(LINK_IS_ADULT ? sAdultBracer : sChildBracer) &&
           ResourceMgr_FileExists(sSurfacePath) && ResourceMgr_FileExists(sRimPath) &&
           ResourceMgr_FileExists(sTexturePath) && ResourceMgr_FileExists(sFlowPath) &&
           ResourceMgr_FileExists(sSurfaceVertices) && ResourceMgr_FileExists(sRimVertices);
}

static s32 DinFireShield_SameContext(PlayState* play, Player* player) {
    return sFire.play == play && sFire.player == player && sFire.scene == play->sceneNum &&
           sFire.age == gSaveContext.linkAge;
}

void DinFireShield_Reset(void) {
    sFire.play = NULL;
    sFire.player = NULL;
    sFire.opacity = 0.0f;
    sFire.phase = 0;
    sFire.lastFrame = UINT32_MAX;
}

void DinFireShield_Update(PlayState* play, Player* player) {
    if (play == NULL || player == NULL || player != GET_PLAYER(play)) {
        return;
    }
    if (!DinFireShield_Eligible(play, player)) {
        DinFireShield_Reset();
        return;
    }
    if (!DinFireShield_SameContext(play, player) ||
        (sFire.lastFrame != UINT32_MAX && play->gameplayFrames < sFire.lastFrame)) {
        DinFireShield_Reset();
        sFire.play = play;
        sFire.player = player;
        sFire.scene = play->sceneNum;
        sFire.age = gSaveContext.linkAge;
    }
    if (play->pauseCtx.state != 0 || play->pauseCtx.debugState != 0 || sFire.lastFrame == play->gameplayFrames) {
        return;
    }
    sFire.lastFrame = play->gameplayFrames;
    sFire.phase = (sFire.phase + 1) & 1023;
    const s32 guarding =
        (player->stateFlags1 & PLAYER_STATE1_SHIELDING) && player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD;
    sFire.opacity = CLAMP(sFire.opacity + (guarding ? 0.25f : -0.18f), 0.0f, 1.0f);
    if (guarding && CVarGetInteger(CVAR_ENHANCEMENT("DinFireShieldSfx"), 0)) {
        FireResources resources;
        if (DinFireShield_Load(&resources)) {
            // Fire Arrow's sustained flame sound. Direct positional playback
            // leaves the player's own actor.sfx slot and audio flags intact.
            // Without SFX_FLAG it expires when guarding stops refreshing it.
            Audio_PlayActorSound2(&player->actor, NA_SE_PL_ARROW_CHARGE_FIRE - SFX_FLAG);
        }
    }
}

// Both the held shield and get-item model use this material/animation path.
// The caller supplies a local matrix and owns its push/pop.
static void DinFireShield_DrawFlames(PlayState* play, const FireResources* resources, u16 phase, f32 opacity) {
    const f32 pulse = 0.96f + 0.04f * sinf(phase * 0.71f);
    const u8 alpha = (u8)(opacity * pulse * 255.0f);
    const s32 surfaceScroll = (phase * 3) & 127;
    const s32 rimScroll = (phase * 7) & 127;
    const Color_RGB8 core =
        CVarGetColor24(CVAR_COSMETIC("Custom.DinFireShieldCore.Value"), (Color_RGB8){ 255, 225, 122 });
    const Color_RGB8 outer =
        CVarGetColor24(CVAR_COSMETIC("Custom.DinFireShieldOuter.Value"), (Color_RGB8){ 255, 43, 3 });

    OPEN_DISPS(play->state.gfxCtx);
    Mtx* mtx = MATRIX_NEWMTX(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_2CYCLE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
    gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
    gDPSetTextureFilter(POLY_XLU_DISP++, G_TF_BILERP);
    gDPSetAlphaCompare(POLY_XLU_DISP++, G_AC_NONE);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCombineLERP(POLY_XLU_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, SHADE, 0, 0, 0, 0,
                      COMBINED, COMBINED, 0, PRIMITIVE, 0);
    gDPLoadTextureBlock(POLY_XLU_DISP++, sFlowTextureRef, G_IM_FMT_I, G_IM_SIZ_8b, 64, 32, 0, G_TX_WRAP, G_TX_WRAP, 6,
                        5, G_TX_NOLOD, G_TX_NOLOD);
    // Emit tile offsets inline, not through an eye/mouth/scene texture segment.
    gDPSetTileSize(POLY_XLU_DISP++, 0, 0, surfaceScroll, 63 << 2, surfaceScroll + (31 << 2));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, core.r, core.g, core.b, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, outer.r, outer.g, outer.b, 255);
    gSPDisplayList(POLY_XLU_DISP++, resources->surface);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPLoadTextureBlock(POLY_XLU_DISP++, sFlameTextureRef, G_IM_FMT_I, G_IM_SIZ_8b, 64, 32, 0, G_TX_WRAP, G_TX_WRAP, 6,
                        5, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetTileSize(POLY_XLU_DISP++, 0, 0, rimScroll, 63 << 2, rimScroll + (31 << 2));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, core.r, core.g, core.b, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, outer.r, outer.g, outer.b, 255);
    gSPDisplayList(POLY_XLU_DISP++, resources->rim);
    gDPPipeSync(POLY_XLU_DISP++);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
}

void DinFireShield_Draw(PlayState* play, Player* player) {
    if (play == NULL || player == NULL || player != GET_PLAYER(play))
        return;
    // Reflection runs first; skip it without erasing the real player's state.
    if (player->actor.scale.y < 0.0f)
        return;
    // Recheck at draw time: toggles can change while paused.
    if (!DinFireShield_Eligible(play, player)) {
        DinFireShield_Reset();
        return;
    }
    if (!DinFireShield_SameContext(play, player) || sFire.opacity <= 0.0f)
        return;
    FireResources resources;
    if (!DinFireShield_Load(&resources)) {
        DinFireShield_Reset();
        return;
    }
    const f32 bloom = 0.25f + 0.75f * sFire.opacity;
    Matrix_Push();
    // Native shield quad is centered at (-1500,0), z=-600 in this hand's frame.
    Matrix_Translate(-1500.0f, 0.0f, -850.0f, MTXMODE_APPLY);
    Matrix_Scale(2.4f * bloom, 2.7f * bloom, 1.0f, MTXMODE_APPLY);
    DinFireShield_DrawFlames(play, &resources, sFire.phase, sFire.opacity);
    Matrix_Pop();
}

int DinFireShield_DrawItem(PlayState* play, s16 drawId) {
    if (play == NULL || (drawId != GID_SHIELD_DEKU && drawId != GID_SHIELD_HYLIAN))
        return false;
    const u16 itemId = drawId == GID_SHIELD_DEKU ? ITEM_SHIELD_DEKU : ITEM_SHIELD_HYLIAN;
    if (!DinFireShield_ItemEnabled(itemId) || !ResourceMgr_FileExists(sGIBracer) ||
        !ResourceMgr_FileExists(sGIVertices))
        return false;
    FireResources resources;
    if (!DinFireShield_Load(&resources) || ResourceGetDataByName(sGIVertices) == NULL)
        return false;
    Gfx* bracer = ResourceMgr_LoadGfxByName(sGIBracer);
    if (bracer == NULL)
        return false;

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(0.65f, 0.65f, 0.65f, MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gSPTexture(POLY_OPA_DISP++, 0, 0, 0, 0, G_OFF);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_SHADE, G_CC_SHADE);
    gSPDisplayList(POLY_OPA_DISP++, bracer);
    gDPPipeSync(POLY_OPA_DISP++);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);

    Matrix_Translate(0.0f, 18.0f, -45.0f, MTXMODE_APPLY);
    Matrix_Scale(0.035f, 0.043f, 0.015f, MTXMODE_APPLY);
    DinFireShield_DrawFlames(play, &resources, play->gameplayFrames & 1023, 1.0f);
    Matrix_Pop();
    return true;
}
