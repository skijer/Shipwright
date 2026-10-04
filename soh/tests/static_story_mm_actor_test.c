#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define REQUIRE(condition)  \
    do {                    \
        if (!(condition)) { \
            return 1;       \
        }                   \
    } while (0)

#include "../src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h"

int main(void) {
    /* Ordinary and scoped-player catalogue identities remain independent. */
    const StaticStoryActorType ordinary[] = { STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, STATIC_STORY_ACTOR_KEATON,
                                              STATIC_STORY_ACTOR_LULU };
    const unsigned counts[] = { 3, 3, 4 };
    const unsigned ids[] = { 9, 10, 12 };
    for (unsigned actor = 0; actor < 3; ++actor) {
        REQUIRE(StaticStoryActor_IsAvailable(ordinary[actor]));
        for (unsigned pose = 0; pose < counts[actor]; ++pose) {
            REQUIRE(StaticStoryActor_GetType(0x7E00 | (pose << 4) | ids[actor]) == ordinary[actor]);
            REQUIRE(StaticStoryMm_GetPresentation(ordinary[actor], pose) != NULL);
            REQUIRE(!StaticStoryActor_LocksRootTranslation(ordinary[actor], pose));
            REQUIRE((StaticStoryActor_ResolvePose(ordinary[actor], pose)->flags &
                     (STATIC_POSE_FLAG_VOCAL | STATIC_POSE_FLAG_OCARINA)) == 0);
        }
        REQUIRE(StaticStoryMm_GetPresentation(ordinary[actor], counts[actor]) == NULL);
    }
    REQUIRE(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_CHILD_KAFEI));
    REQUIRE(StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_CHILD_KAFEI, 0) != NULL);

    REQUIRE(StaticStoryActor_GetType(0x7E0B) == STATIC_STORY_ACTOR_CHILD_KAFEI);
    REQUIRE(StaticStoryActor_GetType(0x7E1B) == STATIC_STORY_ACTOR_CHILD_KAFEI);
    REQUIRE(StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_CHILD_KAFEI, 2) == NULL);
    for (unsigned pose = 0; pose < 2; ++pose) {
        const StaticStoryMmPresentation* p = StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_CHILD_KAFEI, pose);
        REQUIRE(p->kind == STATIC_STORY_MM_SCOPED_PLAYER_LOD && p->limbCount == 21 && p->matrixCount == 18);
        REQUIRE(p->frameCount == (pose == 0 ? 89 : 48) && p->eyeCount == 8 && p->mouthCount == 4);
        REQUIRE(!StaticStoryActor_LocksRootTranslation(STATIC_STORY_ACTOR_CHILD_KAFEI, pose));
        REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_CHILD_KAFEI, pose));
    }
    const StaticStoryActorDefinition* kd = StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_KAFEI);
    REQUIRE(kd->colliderRadius == 18 && kd->colliderHeight == 60 && kd->colliderYShift == 0 && kd->scale == 0.01f);

    const unsigned limbs[] = { 18, 20, 22 }, matrices[] = { 17, 20, 21 };
    const unsigned frames[3][4] = { { 29, 29, 29, 0 }, { 36, 36, 30, 0 }, { 30, 30, 72, 87 } };
    const int radius[] = { 22, 18, 22 }, height[] = { 70, 50, 70 };
    for (unsigned a = 0; a < 3; ++a) {
        const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(ordinary[a]);
        REQUIRE(definition->scale == 0.01f);
        REQUIRE(definition->colliderRadius == radius[a] && definition->colliderHeight == height[a]);
        REQUIRE(definition->colliderYShift == 0 && definition->blinkMin == 30 && definition->blinkRange == 30);
        for (unsigned pose = 0; pose < counts[a]; ++pose) {
            const StaticStoryMmPresentation* p = StaticStoryMm_GetPresentation(ordinary[a], pose);
            REQUIRE(p->kind == STATIC_STORY_MM_NORMAL_FLEX);
            REQUIRE(p->limbCount == limbs[a] && p->matrixCount == matrices[a] && p->frameCount == frames[a][pose]);
            REQUIRE(StaticStoryActor_CanTrack(ordinary[a], pose) == (a != 1 && pose == 0));
            REQUIRE(StaticStoryActor_ResolvePose(ordinary[a], pose)->playbackSpeed == 1.0f);
            REQUIRE(p->eyeCount == (a == 0 ? 1 : a == 1 ? 0 : 3));
            REQUIRE(p->mouthCount == (a == 0 ? 1 : a == 1 ? 0 : 2));
            for (unsigned f = 0; f < frames[a][pose]; ++f)
                for (unsigned blink = 0; blink < 3; ++blink) {
                    StaticStoryMmFace face = StaticStoryMm_ResolveFace(ordinary[a], pose, f, blink, false);
                    unsigned eye = blink, mouth = 0;
                    if (a == 0)
                        eye = 0;
                    if (a == 2) {
                        if (pose == 0 && blink == 0)
                            eye = 1;
                        if (pose == 2) {
                            eye = 0;
                            mouth = 1;
                        }
                        if (pose == 3) {
                            const unsigned sequence[] = { 1, 2, 1, 0, 1, 2, 1, 0 };
                            if (f < 43)
                                eye = 0;
                            else if (f <= 50)
                                eye = sequence[f - 43];
                            mouth = 1;
                        }
                    }
                    REQUIRE(face.eye == eye && face.mouth == mouth);
                }
        }
    }
    REQUIRE(StaticStoryMm_ResolveFace(STATIC_STORY_ACTOR_LULU, 0, 0, 0, true).eye == 0);
    REQUIRE(strcmp(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 0),
                   "objects/object_osn/gHappyMaskSalesmanEyeClosedHappyTex") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetMouthTexturePath(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 0),
                   "objects/object_osn/gHappyMaskSalesmanSmileTex") == 0);
    REQUIRE(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 1) == NULL);
    REQUIRE(StaticStoryMm_GetMouthTexturePath(STATIC_STORY_ACTOR_LULU, 2) == NULL);
    REQUIRE(StaticStoryMm_GetMouthTexturePath(STATIC_STORY_ACTOR_KEATON, 0) == NULL);
    const StaticStoryMmPresentation* idle =
        StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 0);
    const StaticStoryMmPresentation* sway =
        StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 1);
    const StaticStoryMmPresentation* fallback =
        StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 2);

    REQUIRE(idle != NULL);
    REQUIRE(strcmp(idle->skeletonPath, "objects/object_bg/gTreasureChestShopGalSkel") == 0);
    REQUIRE(strcmp(idle->animationPath, "objects/object_bg/object_bg_Anim_009890") == 0);
    REQUIRE(strcmp(sway->animationPath, "objects/object_bg/object_bg_Anim_001384") == 0);
    REQUIRE(strcmp(fallback->animationPath, "objects/object_bg/object_bg_Anim_009890") == 0);
    REQUIRE(idle->limbCount == 23);
    REQUIRE(idle->tracking == STATIC_STORY_MM_TRACKING_HEAD_TORSO);
    REQUIRE(sway->tracking == STATIC_STORY_MM_TRACKING_NONE);
    REQUIRE(fallback->tracking == STATIC_STORY_MM_TRACKING_HEAD_TORSO);
    REQUIRE(strcmp(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 0),
                   "objects/object_bg/gTreasureChestShopGalEyeOpenDownTex") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 1),
                   "objects/object_bg/gTreasureChestShopGalEyeHalfDownTex") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 2),
                   "objects/object_bg/gTreasureChestShopGalEyeClosedTex") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 3),
                   "objects/object_bg/gTreasureChestShopGalEyeHalfDownTex") == 0);
    REQUIRE(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 4) == NULL);
    REQUIRE(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_SKULL_KID, 0) == NULL);
    REQUIRE(!idle->requiresSecondarySkeleton);
    REQUIRE(StaticStoryMm_ResourcesComplete(idle, true, true, false));
    REQUIRE(!StaticStoryMm_ResourcesComplete(idle, false, true, false));
    REQUIRE(!StaticStoryMm_ResourcesComplete(idle, true, false, false));
    REQUIRE(StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 3) == NULL);
    REQUIRE(StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_ADULT_GANONDORF, 0) == NULL);

    const StaticStoryMmPresentation* reclining = StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_SKULL_KID, 0);
    const StaticStoryMmPresentation* upright = StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_SKULL_KID, 1);
    StaticStoryMmVec3f recliningAnchor = StaticStoryMm_GetTatlAnchor(0);
    StaticStoryMmVec3f uprightAnchor = StaticStoryMm_GetTatlAnchor(1);

    REQUIRE(reclining != NULL);
    REQUIRE(upright != NULL);
    REQUIRE(strcmp(reclining->skeletonPath, "objects/object_stk/gSkullKidSkel") == 0);
    REQUIRE(strcmp(reclining->animationPath, "objects/object_stk2/gSkullKidRecliningFloatAnim") == 0);
    REQUIRE(strcmp(upright->animationPath, "objects/object_stk2/gSkullKidFloatingArmsCrossedAnim") == 0);
    REQUIRE(strcmp(reclining->secondarySkeletonPath, "objects/gameplay_keep/gameplay_keep_Skel_02AF58") == 0);
    REQUIRE(strcmp(reclining->secondaryAnimationPath, "objects/gameplay_keep/gameplay_keep_Anim_029140") == 0);
    REQUIRE(strcmp(reclining->maskDisplayListPath, "objects/object_stk/gSkullKidMajorasMask1DL") == 0);
    REQUIRE(strcmp(reclining->headDisplayListPath, "objects/object_stk/gSkullKidNormalHeadDL") == 0);
    REQUIRE(strcmp(reclining->eyesDisplayListPath, "objects/object_stk/gSkullKidNormalEyesDL") == 0);
    REQUIRE(StaticStoryMm_GetSkullKidLimbDisplayListPath(1) == NULL);
    REQUIRE(strcmp(StaticStoryMm_GetSkullKidLimbDisplayListPath(2), "objects/object_stk/gSkullKidPelvisDL") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetSkullKidLimbDisplayListPath(9), "objects/object_stk/gSkullKidTorsoDL") == 0);
    REQUIRE(StaticStoryMm_GetSkullKidLimbDisplayListPath(17) == NULL);
    REQUIRE(strcmp(StaticStoryMm_GetSkullKidLimbDisplayListPath(21), "objects/object_stk/gSkullKidHatTopDL") == 0);
    REQUIRE(StaticStoryMm_GetSkullKidLimbDisplayListPath(22) == NULL);
    REQUIRE(strcmp(StaticStoryMm_GetTatlLimbPath(0), "objects/gameplay_keep/gameplay_keep_Standardlimb_02AEF8") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetTatlLimbPath(5), "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF34") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetTatlDListPath(0), "objects/gameplay_keep/gameplay_keep_DL_029990") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetTatlDListPath(5), "objects/gameplay_keep/gameplay_keep_DL_029CF0") == 0);
    REQUIRE(StaticStoryMm_GetTatlLimbPath(6) == NULL);
    REQUIRE(StaticStoryMm_GetTatlDListPath(6) == NULL);
    REQUIRE(reclining->requiresSecondarySkeleton);
    REQUIRE(upright->requiresSecondarySkeleton);
    REQUIRE(StaticStoryMm_UsesNativeFairyCompanion(STATIC_STORY_ACTOR_SKULL_KID));
    REQUIRE(!StaticStoryMm_UsesNativeFairyCompanion(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL));
    REQUIRE(reclining->tracking == STATIC_STORY_MM_TRACKING_BODY_YAW);
    REQUIRE(upright->tracking == STATIC_STORY_MM_TRACKING_BODY_YAW);
    REQUIRE(StaticStoryMm_ResourcesComplete(reclining, true, true, true));
    REQUIRE(!StaticStoryMm_ResourcesComplete(reclining, true, true, false));
    REQUIRE(fabsf(StaticStoryMm_GetHoverOffset(0)) < 0.001f);
    REQUIRE(fabsf(StaticStoryMm_GetHoverOffset(0x4000) - 10.0f) < 0.001f);
    REQUIRE(fabsf(StaticStoryMm_ComposeHoverY(123.0f, 0x4000) - 133.0f) < 0.001f);
    REQUIRE(recliningAnchor.x != uprightAnchor.x || recliningAnchor.y != uprightAnchor.y ||
            recliningAnchor.z != uprightAnchor.z);
    REQUIRE(StaticStoryMm_GetTatlOuterAlpha(0) >= 120);
    REQUIRE(StaticStoryMm_GetTatlOuterAlpha(0) <= 255);
    REQUIRE(StaticStoryMm_GetTatlOuterAlpha(0x8000) >= 120);
    REQUIRE(StaticStoryMm_GetTatlOuterAlpha(0x8000) <= 255);
    REQUIRE(StaticStoryMm_GetTatlScale(0) >= 0.8f);
    REQUIRE(StaticStoryMm_GetTatlScale(0) <= 1.2f);
    REQUIRE(StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_SKULL_KID, 2) == NULL);
    return 0;
}
