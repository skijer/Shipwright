#include "soh/Enhancements/randomizer/NeiUsedMagicPresentation.h"
#include "../helpers/ice_trail.h"
#include "soh/Enhancements/randomizer/NeiHeldPresentation.h"
/**
 * object_icerod.c - Ice Rod 3D model and draw functions
 *
 * Draws the cyan/blue glowing rod and ice projectiles.
 * USED projectiles use item-local faceted ice geometry.
 */

#include "z64.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "../helpers/rod_visual.h"
#include "../logic/item_rod_ice.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "soh/frame_interpolation.h"
#include <math.h>

// Ice Rod model from ice_rodDL folder
#include "ice_rodDL/header.h"

// Public display list reference for draw.cpp (give item)
Gfx* gIceRodGiveDL = g_ice_rod_dl;

static Vec3f IceRod_DrawHeadTrail(PlayState* play, const Vec3f* history, const Vec3f* head, unsigned index, f32 scale) {
    NeiIceTrailPoint center[6], reconstructed[6];
    Vec3f wake[6];
    for (s32 i = 0; i < 6; ++i)
        center[i] = (NeiIceTrailPoint){ history[i].x, history[i].y, history[i].z };
    NeiIceTrail_Reconstruct(center, 6, (NeiIceTrailPoint){ head->x, head->y, head->z }, index, reconstructed);
    for (s32 i = 0; i < 6; ++i)
        wake[i] = (Vec3f){ reconstructed[i].x, reconstructed[i].y, reconstructed[i].z };
    NeiUsedMagic_DrawTrail(play, 1, wake, 6, scale);
    // Remote state has center history but no per-head velocity. Retain its
    // actual heading even when the newest stopped samples are duplicates.
    for (s32 i = 1; i < 6; ++i) {
        Vec3f direction = { wake[0].x - wake[i].x, wake[0].y - wake[i].y, wake[0].z - wake[i].z };
        if (direction.x != 0.0f || direction.y != 0.0f || direction.z != 0.0f)
            return direction;
    }
    return (Vec3f){ 0.0f, 0.0f, 0.0f };
}

// ============================================================================
// DRAW FUNCTION - Draws Ice Rod and active projectiles
// Uses ice crystal/spark effects for visual
// ============================================================================

void CustomItems_DrawIceRod(Player* player, PlayState* play) {
    if (!iceRodActive)
        return;

    OPEN_DISPS(play->state.gfxCtx);

    // Keep the aiming view clear; projectile and trail rendering continues below.
    if (player != GET_PLAYER(play) || !iceRodFirstPerson) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);

        // The exported shaft is 32 degrees from model +Y. Rotate its exact
        // author axis onto native left-hand weapon +X, preserving the wrist roll
        // that forearm-to-hand positions cannot recover.
        static const ItemHandPose wristPose = { 0, 0, 0, 0, 0, 0, 1 };
        u8 rodDrawn = 0;
        Matrix_Push();
        if (ItemEquip_ApplyLeftHandPose(player, &wristPose)) {
            // Palm sockets measured from the native adult sword hilt and the
            // child Kokiri Sword grip. The wrist itself is below the fist.
            f32 gripY = LINK_IS_CHILD ? 216.22f : 328.0f;
            f32 gripZ = LINK_IS_CHILD ? 4.5f : -77.0f;
            Matrix_Translate(0.0f, gripY * player->actor.scale.x, gripZ * player->actor.scale.x, MTXMODE_APPLY);
            Matrix_RotateZ(DEG_TO_RAD(-122.0f), MTXMODE_APPLY);
            Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
            rodDrawn = NeiHeld_DrawRod(play, 1);
        }
        Matrix_Pop();

        if (!rodDrawn) {
            // Get forearm and hand positions to calculate hand direction
            Vec3f forearmPos = player->bodyPartsPos[PLAYER_BODYPART_L_FOREARM];
            Vec3f handPos = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];

            // Calculate direction vector from forearm to hand
            f32 dx = handPos.x - forearmPos.x;
            f32 dy = handPos.y - forearmPos.y;
            f32 dz = handPos.z - forearmPos.z;

            // Calculate yaw and pitch from direction
            f32 handYaw = atan2f(dx, dz);
            f32 horizDist = sqrtf(dx * dx + dz * dz);
            f32 handPitch = atan2f(dy, horizDist);

            // Position at hand
            Matrix_Translate(handPos.x, handPos.y, handPos.z, MTXMODE_NEW);

            // Apply hand rotation
            Matrix_RotateY(handYaw, MTXMODE_APPLY);
            Matrix_RotateX(-handPitch, MTXMODE_APPLY);
            Matrix_RotateY(BINANG_TO_RAD(0x4000), MTXMODE_APPLY);

            // Slight offset in local X and Z
            Matrix_Translate(-0.5f, 0.0f, 0.5f, MTXMODE_APPLY);

            Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);

            gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, g_ice_rod_dl);

            // Draw transparent parts (ice crystal) with same matrix
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, g_ice_rod_xlu_dl);
        }
    }

    // Item-local USED meshes. Local and synchronized remote positions retain
    // their original dispatch; gameplay updates own every position and timer.
    u8 hasLocalSets = IceRod_HasAnyActiveSet();
    u8 hasRemoteSync =
        !hasLocalSets && iceRodProjActive && iceRodProjScale > 0.001f && gCustomItemState.iceRodProjCount > 0;
    if (hasLocalSets) {
        RodProjSet* sets = IceRod_GetProjSets();
        for (s32 s = 0; s < ROD_MAX_PROJ_SETS; ++s) {
            RodProjSet* set = &sets[s];
            if (!set->active)
                continue;
            FrameInterpolation_RecordOpenChild(set, set->drawEpoch);
            for (s32 p = 0; p < set->count && p < 3; ++p) {
                FrameInterpolation_RecordOpenChild(&set->pos[p], 1);
                IceRod_DrawHeadTrail(play, set->trail, &set->pos[p], p, set->scale);
                FrameInterpolation_RecordCloseChild();
            }
            for (s32 p = 0; p < set->count && p < 3; ++p) {
                FrameInterpolation_RecordOpenChild(&set->pos[p], 0);
                Vec3f direction =
                    RodVisual_Heading(set->vel[p], set->yaw, set->pitch, ICE_ROD_SLASH_SPREAD * (0x10000 / 360), p);
                NeiUsedMagic_DrawProjectile(play, 1, &set->pos[p], &direction, set->scale, s * 19 + p * 7);
                FrameInterpolation_RecordCloseChild();
            }
            FrameInterpolation_RecordCloseChild();
        }
    } else if (hasRemoteSync) {
        Vec3f remotePos[3] = { iceRodProjPos, gCustomItemState.iceRodProjPos2, gCustomItemState.iceRodProjPos3 };
        Vec3f directions[3];
        for (s32 p = 0; p < gCustomItemState.iceRodProjCount && p < 3; ++p) {
            directions[p] = IceRod_DrawHeadTrail(play, iceRodProjTrail, &remotePos[p], p, iceRodProjScale);
        }
        for (s32 p = 0; p < gCustomItemState.iceRodProjCount && p < 3; ++p) {
            NeiUsedMagic_DrawProjectile(play, 1, &remotePos[p], &directions[p], iceRodProjScale, p * 7);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
