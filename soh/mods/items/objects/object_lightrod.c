#include "soh/Enhancements/randomizer/NeiUsedMagicPresentation.h"
#include "soh/Enhancements/randomizer/NeiHeldPresentation.h"
/**
 * object_lightrod.c - Light Rod 3D model and draw functions
 *
 * Draws the golden/yellow glowing rod and light projectiles.
 * USED projectiles use item-local radiant light geometry.
 */

#include "z64.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "../logic/item_rod_light.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include <math.h>

// Light Rod model from light_rodDL folder
#include "light_rodDL/header.h"

// Public display list reference for draw.cpp (give item)
Gfx* gLightRodGiveDL = g_light_rod_dl;

// Draw the held model and the USED projectile presentation.
void CustomItems_DrawLightRod(Player* player, PlayState* play) {
    if (!lightRodActive)
        return;

    OPEN_DISPS(play->state.gfxCtx);

    // Keep the aiming view clear; projectile and trail rendering continues below.
    if (player != GET_PLAYER(play) || !lightRodFirstPerson) {
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
            rodDrawn = NeiHeld_DrawRod(play, 2);
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

            // Slight offset in local X, Y and Z
            Matrix_Translate(-0.5f, 5.0f, 0.5f, MTXMODE_APPLY);

            Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);

            gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, g_light_rod_dl);

            // Draw transparent parts (light crystal) with same matrix
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, g_light_rod_xlu_dl);
        }
    }

    // Item-local USED meshes. Local and synchronized remote positions retain
    // their original dispatch; gameplay updates own every position and timer.
    u8 hasLocalSets = LightRod_HasAnyActiveSet();
    u8 hasRemoteSync = !hasLocalSets && lightRodProjActive && gCustomItemState.lightRodProjCount > 0;
    if (hasLocalSets || hasRemoteSync) {
        // The legacy energy-ball draw consumed one RNG value here. Retain
        // exactly that cadence without using gameplay RNG to animate visuals.
        (void)Rand_ZeroOne();
    }
    if (hasLocalSets) {
        RodProjSet* sets = LightRod_GetProjSets();
        for (s32 s = 0; s < ROD_MAX_PROJ_SETS; ++s) {
            RodProjSet* set = &sets[s];
            if (!set->active)
                continue;
            NeiUsedMagic_DrawTrail(play, 2, set->trail, 6, set->scale);
            for (s32 p = 0; p < set->count && p < 3; ++p) {
                NeiUsedMagic_DrawProjectile(play, 2, &set->pos[p], &set->vel[p], set->scale, s * 19 + p * 7);
            }
        }
    } else if (hasRemoteSync) {
        Vec3f remotePos[3] = { lightRodProjPos, gCustomItemState.lightRodProjPos2, gCustomItemState.lightRodProjPos3 };
        Vec3f velocity = { lightRodProjTrail[0].x - lightRodProjTrail[1].x,
                           lightRodProjTrail[0].y - lightRodProjTrail[1].y,
                           lightRodProjTrail[0].z - lightRodProjTrail[1].z };
        NeiUsedMagic_DrawTrail(play, 2, lightRodProjTrail, 6, lightRodProjScale);
        for (s32 p = 0; p < gCustomItemState.lightRodProjCount && p < 3; ++p) {
            NeiUsedMagic_DrawProjectile(play, 2, &remotePos[p], &velocity, lightRodProjScale, p * 7);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
