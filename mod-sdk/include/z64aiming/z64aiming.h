#ifndef UNBOUND_Z64AIMING_H
#define UNBOUND_Z64AIMING_H

#include "soh/ModApi/ModApi.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "functions.h"
#include "macros.h"
#ifdef __cplusplus
}
#endif

s32 func_8083AD4C(PlayState* play, Player* player);
s16 func_8084ABD8(PlayState* play, Player* player, s32 isAimingWeapon, s16 yawOffset);
void Player_ZeroSpeedXZ(Player* player);

static bool sZ64Aiming = false;

#define Z64AIMING_CHARGE_FRAMES 14
#define Z64AIMING_RETICLE_MATRIX_SCALE 0.01f

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
    Camera_ChangeMode(Play_GetCamera(play, MAIN_CAM), CAM_MODE_NORMAL);
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

static inline void Z64Aiming_DrawReticle(PlayState* play, Player* player, f32 range) {
    if (play == NULL || player == NULL) {
        return;
    }
    Matrix_Translate(player->actor.focus.pos.x, player->actor.focus.pos.y, player->actor.focus.pos.z, MTXMODE_NEW);
    Matrix_RotateY((f32)BINANG_TO_RAD(player->actor.focus.rot.y), MTXMODE_APPLY);
    Matrix_RotateX((f32)BINANG_TO_RAD(player->actor.focus.rot.x), MTXMODE_APPLY);
    Matrix_Scale(Z64AIMING_RETICLE_MATRIX_SCALE, Z64AIMING_RETICLE_MATRIX_SCALE, Z64AIMING_RETICLE_MATRIX_SCALE,
                 MTXMODE_APPLY);
    Player_DrawHookshotReticle(play, player, range / Z64AIMING_RETICLE_MATRIX_SCALE);
}

#endif
