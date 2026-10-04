#include "soh/Enhancements/randomizer/NeiHeldPresentation.h"
#include "soh/Enhancements/randomizer/NeiUsedMagicPresentation.h"
/**
 * object_firerod.c - Fire Rod 3D model and draw functions
 *
 * Draws the red glowing rod and fire projectiles.
 * Uses item-local flame meshes for projectiles.
 */

#include "z64.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "../helpers/rod_visual.h"
#include "../logic/item_rod_fire.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "soh/frame_interpolation.h"
#include <math.h>

// Fire Rod model from fire_rodDL folder
#include "fire_rodDL/header.h"

// Public display list reference for draw.cpp (give item)
Gfx* gFireRodGiveDL = g_fire_rod_dl;

// ============================================================================
// DRAW FUNCTION - Draws Fire Rod and active projectile
// Projectile surfaces are private nei_rod_attack resources; shared torch assets are untouched.
// ============================================================================

void CustomItems_DrawFireRod(Player* player, PlayState* play) {
    if (!fireRodActive)
        return;

    OPEN_DISPS(play->state.gfxCtx);

    // Keep the aiming view clear; projectile and trail rendering continues below.
    if (player != GET_PLAYER(play) || !fireRodFirstPerson) {
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
            rodDrawn = NeiHeld_DrawRod(play, 0);
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
            gSPDisplayList(POLY_OPA_DISP++, g_fire_rod_dl);
        }
    }

    // Item-local USED meshes. Local and synchronized remote positions retain
    // their original dispatch; gameplay updates own every position and timer.
    u8 hasLocalSets = FireRod_HasAnyActiveSet();
    u8 hasRemoteSync =
        !hasLocalSets && fireRodProjActive && fireRodProjScale > 0.001f && gCustomItemState.fireRodProjCount > 0;
    if (hasLocalSets) {
        RodProjSet* sets = FireRod_GetProjSets();
        for (s32 s = 0; s < ROD_MAX_PROJ_SETS; ++s) {
            RodProjSet* set = &sets[s];
            if (!set->active)
                continue;
            FrameInterpolation_RecordOpenChild(set, set->drawEpoch);
            FrameInterpolation_RecordOpenChild(set->trail, 0);
            NeiUsedMagic_DrawTrail(play, 0, set->trail, 6, set->scale);
            FrameInterpolation_RecordCloseChild();
            for (s32 p = 0; p < set->count && p < 3; ++p) {
                FrameInterpolation_RecordOpenChild(&set->pos[p], 0);
                Vec3f direction =
                    RodVisual_Heading(set->vel[p], set->yaw, set->pitch, FIRE_ROD_SLASH_SPREAD * (0x10000 / 360), p);
                NeiUsedMagic_DrawProjectile(play, 0, &set->pos[p], &direction, set->scale, s * 19 + p * 7);
                FrameInterpolation_RecordCloseChild();
            }
            FrameInterpolation_RecordCloseChild();
        }
    } else if (hasRemoteSync) {
        Vec3f remotePos[3] = { fireRodProjPos, gCustomItemState.fireRodProjPos2, gCustomItemState.fireRodProjPos3 };
        Vec3f velocity = { fireRodProjTrail[0].x - fireRodProjTrail[1].x, fireRodProjTrail[0].y - fireRodProjTrail[1].y,
                           fireRodProjTrail[0].z - fireRodProjTrail[1].z };
        NeiUsedMagic_DrawTrail(play, 0, fireRodProjTrail, 6, fireRodProjScale);
        for (s32 p = 0; p < gCustomItemState.fireRodProjCount && p < 3; ++p) {
            NeiUsedMagic_DrawProjectile(play, 0, &remotePos[p], &velocity, fireRodProjScale, p * 7);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
