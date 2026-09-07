/**
 * garo_post_limb.cpp — make Garo behave like a real transformation.
 *
 * The previous Garo path in z_player.c skipped Player_DrawImpl entirely to
 * hide Link's sword/shield/belt/gauntlets. That also dropped
 * Player_PostLimbDrawGameplay, so Navi tracking, shadow updates,
 * leftHandPos/carried-actor sync, focus.pos at HEAD, and shieldMf all stopped
 * firing — Navi froze, picked-up pots snapped to the pre-transform position,
 * Z-target reticle anchored at the last human-form head, etc.
 *
 * Fix: run SkelAnime_DrawFlexLod over Link's normal skeleton with a custom
 * OverrideLimbDraw that nulls every limb's DL (suppressing the mesh) and a
 * PostLimbDraw that performs the six side-effect categories from
 * MmForm_PostLimbDraw (mm_player_form.cpp:11366-11550). Matrix walk still
 * happens, PostLimbDraw still fires for every limb, but no Link geometry
 * renders. The Garo body keeps rendering separately via GaroSkin_Draw.
 */

#include "garo_post_limb.h"
#include "mods/transformation_masks/transformation_masks.h" // gPlayerLimbToBodyPart
#include <cstring>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
}

// Limb→bodypart mapping is shared with mm_player_form.cpp via
// gPlayerLimbToBodyPart (declared in transformation_masks.h) — Garo uses
// Link's rig, so the same table applies. (Was a byte-for-byte local copy.)

// Returning 0 with *dList = NULL lets SkelAnime push the matrix and call
// PostLimbDraw, but skips the actual gSPDisplayList for this limb.
static s32 GaroForm_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg) {
    (void)play;
    (void)limbIndex;
    (void)pos;
    (void)rot;
    (void)arg;
    *dList = NULL;
    return 0;
}

static void GaroForm_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    (void)dList;
    (void)rot;
    Player* player = (Player*)thisx;
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };

    // === 1. bodyPartsPos[] per limb ===
    if (limbIndex > 0 && limbIndex < PLAYER_LIMB_MAX) {
        s8 bodyPart = gPlayerLimbToBodyPart[limbIndex];
        if (bodyPart >= 0) {
            Matrix_MultVec3f(&zeroVec, &player->bodyPartsPos[bodyPart]);
        }
    }

    // === 2. leftHandPos + carried-actor sync at L_HAND ===
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        Matrix_MultVec3f(&zeroVec, &player->leftHandPos);

        // NOTE: the sword trail is NOT fed from here any more. This skeleton
        // is Link's hidden rig — it has different proportions and a different
        // scale from the Garo body the player actually sees, so a streak built
        // off this hand floated away from the blades. Both trails are now fed
        // in garo_hybrid_render.cpp at the real L_SWORD / R_SWORD bones.

        if (player->actor.scale.y >= 0.0f) {
            Actor* heldActor = player->heldActor;

            if (!Player_HoldsHookshot(player) && (heldActor != NULL)) {
                if (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
                    MtxF carryMtx;
                    Vec3s carryRot;

                    Matrix_Get(&carryMtx);
                    Matrix_MtxFToYXZRotS(&carryMtx, &carryRot, 0);

                    if (heldActor->flags & ACTOR_FLAG_CARRY_X_ROT_INFLUENCE) {
                        heldActor->world.rot.x = heldActor->shape.rot.x = carryRot.x - player->unk_3BC.x;
                    } else {
                        heldActor->world.rot.y = heldActor->shape.rot.y = player->actor.shape.rot.y + player->unk_3BC.y;
                    }
                }
            } else {
                Matrix_Get(&player->mf_9E0);
                Matrix_MtxFToYXZRotS(&player->mf_9E0, &player->unk_3BC, 0);
            }
        }
    }

    // === 3. focus.pos at HEAD (Navi tracking, Z-targeting) ===
    // Rig is Link's, so the human-form offset {1100, -700, 0} from
    // mm_player_form.cpp:11437 applies directly.
    if (limbIndex == PLAYER_LIMB_HEAD) {
        Vec3f headOffset = { 1100.0f, -700.0f, 0.0f };
        Matrix_MultVec3f(&headOffset, &player->actor.focus.pos);
    }

    // === 4. Feet positions for ActorShadow_DrawFeet ===
    if (limbIndex == PLAYER_LIMB_L_FOOT || limbIndex == PLAYER_LIMB_R_FOOT) {
        Actor_SetFeetPos(&player->actor, limbIndex, PLAYER_LIMB_L_FOOT, &zeroVec, PLAYER_LIMB_R_FOOT, &zeroVec);
    }

    // === 5. shieldMf at R_HAND — push off-screen so Mir_Ray frustum test fails ===
    if (limbIndex == PLAYER_LIMB_R_HAND) {
        if (player->actor.scale.y >= 0.0f) {
            player->shieldMf.xw = 0.0f;
            player->shieldMf.yw = -32000.0f;
            player->shieldMf.zw = 0.0f;
        }
    }
}

extern "C" void GaroForm_DrawNullBody(PlayState* play, Player* player, s32 lod) {
    if (player->skelAnime.skeleton == NULL || player->skelAnime.jointTable == NULL) {
        return;
    }
    SkelAnime_DrawFlexLod(play, player->skelAnime.skeleton, player->skelAnime.jointTable, player->skelAnime.dListCount,
                          GaroForm_OverrideLimbDraw, GaroForm_PostLimbDraw, player, lod);
}
