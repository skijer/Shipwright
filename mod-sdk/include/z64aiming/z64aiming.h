#ifndef UNBOUND_Z64AIMING_H
#define UNBOUND_Z64AIMING_H

#include "soh/ModApi/ModApi.h"
#include <libultraship/bridge/consolevariablebridge.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "functions.h"
#include "macros.h"
#include "objects/object_link_boy/object_link_boy.h"
#ifdef __cplusplus
}
#endif

s32 func_8083AD4C(PlayState* play, Player* player);
s16 func_8084ABD8(PlayState* play, Player* player, s32 isAimingWeapon, s16 yawOffset);
void Player_ZeroSpeedXZ(Player* player);

static bool sZ64Aiming = false;

#define Z64AIMING_CHARGE_FRAMES 14

typedef enum {
    Z64AIMING_TOGGLE_HOOKSHOT,
    Z64AIMING_TOGGLE_BOW,
    Z64AIMING_TOGGLE_BOOMERANG,
} Z64AimingReticleToggle;

typedef enum {
    Z64AIMING_TINT_LAUNCH,
    Z64AIMING_TINT_CONTROL,
    Z64AIMING_TINT_ATTRACT,
    Z64AIMING_TINT_MAX,
} Z64AimingReticleTint;

typedef struct {
    Z64AimingReticleToggle toggle;
    Z64AimingReticleTint tint;
} Z64AimingReticleStyle;

static const u8 sZ64AimingTintColors[Z64AIMING_TINT_MAX][3] = {
    { 255, 0, 0 },
    { 0, 255, 0 },
    { 0, 120, 255 },
};

static inline void Z64Aiming_HoldState(Player* player) {
    player->unk_6AD = 2;
    player->unk_834 = Z64AIMING_CHARGE_FRAMES;
    player->stateFlags1 |= PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_READY_TO_FIRE | PLAYER_STATE1_ITEM_IN_HAND;
    player->unk_6AE_rotFlags |= UNK6AE_ROT_FOCUS_X | UNK6AE_ROT_FOCUS_Y | UNK6AE_ROT_UPPER_X | UNK6AE_ROT_UPPER_Y;
}

static inline bool Z64Aiming_Request(Player* player, PlayState* play) {
    if (player == NULL || play == NULL) {
        return false;
    }
    sZ64Aiming = true;
    player->actor.focus.rot.x = 0;
    player->actor.focus.rot.y = player->actor.shape.rot.y;
    Z64Aiming_HoldState(player);
    Player_ZeroSpeedXZ(player);
    func_8083AD4C(play, player);
    Sfx_PlaySfxCentered(NA_SE_SY_CAMERA_ZOOM_UP);
    return true;
}

static inline void Z64Aiming_Update(Player* player, PlayState* play) {
    if (!sZ64Aiming || player == NULL || play == NULL) {
        return;
    }
    Z64Aiming_HoldState(player);
    func_8083AD4C(play, player);
    player->upperLimbRot.y = func_8084ABD8(play, player, 1, 0) - player->actor.shape.rot.y;
}

static inline void Z64Aiming_Release(Player* player, PlayState* play) {
    if (!sZ64Aiming) {
        return;
    }
    sZ64Aiming = false;
    if (player == NULL || play == NULL) {
        return;
    }
    player->unk_6AD = 0;
    player->unk_834 = 0;
    player->stateFlags1 &= ~(PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_READY_TO_FIRE | PLAYER_STATE1_ITEM_IN_HAND);
    Camera_RequestMode(Play_GetCamera(play, CAM_ID_MAIN), CAM_MODE_NORMAL);
    Sfx_PlaySfxCentered(NA_SE_SY_CAMERA_ZOOM_DOWN);
}

static inline void Z64Aiming_Drop(void) {
    sZ64Aiming = false;
}

static inline bool Z64Aiming_IsAiming(void) {
    return sZ64Aiming;
}

static inline void Z64Aiming_GetDirection(Player* player, s16* yaw, s16* pitch) {
    if (player == NULL) {
        return;
    }
    if (yaw != NULL) {
        *yaw = player->actor.focus.rot.y;
    }
    if (pitch != NULL) {
        *pitch = player->actor.focus.rot.x;
    }
}

// The hookshot's reticle has no toggle; the others follow vanilla's Bow and Boomerang Reticle options.
static inline bool Z64Aiming_IsReticleEnabled(Z64AimingReticleToggle toggle) {
    switch (toggle) {
        case Z64AIMING_TOGGLE_BOW:
            return CVarGetInteger("gEnhancements.BowReticle", 0) != 0;
        case Z64AIMING_TOGGLE_BOOMERANG:
            return CVarGetInteger("gEnhancements.BoomerangReticle", 0) != 0;
        case Z64AIMING_TOGGLE_HOOKSHOT:
        default:
            return true;
    }
}

// Player_DrawHookshotReticle's hit test and scale, with HookshotReticle.cpp's draw so the dot takes a tint.
static inline void Z64Aiming_DrawReticle(PlayState* play, Player* player, f32 range, Z64AimingReticleStyle style) {
    if (play == NULL || player == NULL || !Z64Aiming_IsReticleEnabled(style.toggle)) {
        return;
    }
    s16 yaw;
    s16 pitch;
    Z64Aiming_GetDirection(player, &yaw, &pitch);
    f32 cosPitch = Math_CosS(pitch);
    Vec3f start = player->actor.focus.pos;
    Vec3f end = { start.x + Math_SinS(yaw) * cosPitch * range, start.y - Math_SinS(pitch) * range,
                  start.z + Math_CosS(yaw) * cosPitch * range };
    Vec3f hit;
    CollisionPoly* poly;
    s32 bgId;
    if (!BgCheck_AnyLineTest3(&play->colCtx, &start, &end, &hit, &poly, 1, 1, 1, 1, &bgId)) {
        return;
    }
    Vec3f projected;
    f32 depth;
    SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &hit, &projected, &depth);
    f32 scale = (depth < 200.0f) ? 0.08f : (depth / 200.0f) * 0.08f;
    const u8* color = sZ64AimingTintColors[style.tint % Z64AIMING_TINT_MAX];

    OPEN_DISPS(play->state.gfxCtx);
    OVERLAY_DISP = Gfx_SetupDL(OVERLAY_DISP, 0x07);
    Matrix_Translate(hit.x, hit.y, hit.z, MTXMODE_NEW);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(OVERLAY_DISP++, SEG_ADDR(1, 0), G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPTexture(OVERLAY_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(OVERLAY_DISP++, gLinkAdultHookshotReticleTex, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, color[0], color[1], color[2], 255);
    gSPVertex(OVERLAY_DISP++, (uintptr_t)gLinkAdultHookshotReticleVtx, 3, 0);
    gSP1Triangle(OVERLAY_DISP++, 0, 1, 2, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

#endif
