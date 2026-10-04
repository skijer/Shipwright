#include <stddef.h>
#include <stdint.h>

#define REQUIRE(condition)  \
    do {                    \
        if (!(condition)) { \
            return 1;       \
        }                   \
    } while (0)
#include "z64object.h"
#include "../src/overlays/actors/ovl_En_Viewer/static_story_actor.h"

int main(void) {
    REQUIRE(StaticStoryActor_GetType(0x7E1D) == STATIC_STORY_ACTOR_ANJU);
    REQUIRE(StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_ANJU, StaticStoryActor_GetPose(0x7E1D)) == 1);
    REQUIRE(StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_ANJU, 2) == 0);
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ANJU, 1));
    REQUIRE(!StaticStoryActor_LocksRootTranslation(STATIC_STORY_ACTOR_ANJU, 1));
    const StaticStoryActorDefinition* definition;
    StaticStoryObjectRequirements objects;
    StaticStoryProgression early = { 0 };
    StaticStoryProgression complete = {
        .metZelda = true,
        .forestComplete = true,
        .waterComplete = true,
        .eponaComplete = true,
    };
    int16_t placementPitch = 0x2000;
    int16_t placementYaw = -0x3456;
    int16_t placementRoll = 0x1000;

    for (unsigned bits = 0; bits < 16; ++bits) {
        StaticStoryProgression progression = {
            .metZelda = (bits & 1) != 0,
            .forestComplete = (bits & 2) != 0,
            .waterComplete = (bits & 4) != 0,
            .eponaComplete = (bits & 8) != 0,
        };

        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_IMPA, &progression) ==
                (progression.metZelda ? 0x708E : 0x708D));
        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_ZELDA, &progression) ==
                (progression.metZelda ? 0x70FE : 0x70FF));
        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SHEIK, &progression) ==
                (progression.waterComplete ? 0x7010 : 0x700F));
        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_MALON, &progression) ==
                (progression.eponaComplete ? 0x204A : 0x2041));
        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SARIA, &progression) ==
                (progression.forestComplete ? 0x10AD : 0x1001));
        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_RUTO, &progression) ==
                (progression.waterComplete ? 0x403E : 0x402C));
        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_KOKIRI_GIRL, &progression) ==
                (progression.forestComplete ? 0x10DA : 0x1004));
        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_FADO, &progression) ==
                (progression.forestComplete ? 0x10D9 : 0x1005));
        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_MALON, &progression) ==
                (progression.eponaComplete ? 0x2056 : 0x204C));
        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_DARUNIA, &progression) ==
                (progression.forestComplete ? 0x301E : 0x301A));
        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_NABOORU, &progression) ==
                (progression.forestComplete ? 0x6012 : 0x600C));
        REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, &progression) ==
                (progression.waterComplete ? 0x403E : 0x402C));
    }

    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_IMPA, NULL) == 0x708D);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_ZELDA, NULL) == 0x70FF);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_PHANTOM_GANON, NULL) == 0);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, NULL) == 0x8F20);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SKULL_KID, NULL) == 0x8F21);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_KEATON, NULL) == 0x8F22);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, NULL) == 0x8F23);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_KAFEI, NULL) == 0x8F24);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_LULU, NULL) == 0x8F25);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_NONE, NULL) == 0);
    REQUIRE(StaticStoryActor_SelectTextId((StaticStoryActorType)0x7FFF, NULL) == 0);
    REQUIRE(!StaticStoryActor_CanTalk(STATIC_STORY_ACTOR_NONE));
    REQUIRE(!StaticStoryActor_CanTalk(STATIC_STORY_ACTOR_PHANTOM_GANON));
    REQUIRE(!StaticStoryActor_CanTalk((StaticStoryActorType)0x7FFF));
    REQUIRE(StaticStoryActor_CanTalk(STATIC_STORY_ACTOR_SKULL_KID));

    REQUIRE(STATIC_STORY_ACTOR_KEATON == 20);
    REQUIRE(STATIC_STORY_ACTOR_CHILD_KAFEI == 21);
    REQUIRE(STATIC_STORY_ACTOR_LULU == 22);
    REQUIRE(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_KEATON));
    REQUIRE(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_CHILD_KAFEI));
    REQUIRE(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_LULU));
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_KEATON)->objectId == OBJECT_INVALID);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_KAFEI)->objectId == OBJECT_INVALID);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_LULU)->objectId == OBJECT_INVALID);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_KEATON)->adapter == STATIC_ADAPTER_MM_KEATON);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_KAFEI)->adapter == STATIC_ADAPTER_MM_KAFEI);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_LULU)->adapter == STATIC_ADAPTER_MM_LULU);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E0A) == STATIC_STORY_ACTOR_KEATON);

    REQUIRE(StaticStoryActor_IsParam((int16_t)0x7F01));
    REQUIRE(StaticStoryActor_GetType(0x7F01) == STATIC_STORY_ACTOR_IMPA);
    REQUIRE(StaticStoryActor_GetType(0x7F02) == STATIC_STORY_ACTOR_CHILD_MALON);
    REQUIRE(StaticStoryActor_GetType(0x7F03) == STATIC_STORY_ACTOR_SARIA);
    REQUIRE(StaticStoryActor_GetType(0x7F04) == STATIC_STORY_ACTOR_ADULT_ZELDA);
    REQUIRE(StaticStoryActor_GetType(0x7F05) == STATIC_STORY_ACTOR_SHEIK);
    REQUIRE(StaticStoryActor_GetType(0x7F06) == STATIC_STORY_ACTOR_ADULT_RUTO);
    REQUIRE(StaticStoryActor_GetType(0x7F07) == STATIC_STORY_ACTOR_CHILD_RUTO);
    REQUIRE(StaticStoryActor_GetType(0x7F0A) == STATIC_STORY_ACTOR_ADULT_MALON);
    REQUIRE(StaticStoryActor_GetType(0x7F59) == STATIC_STORY_ACTOR_FADO);
    REQUIRE(StaticStoryActor_GetPose(0x7F59) == 5);
    REQUIRE(StaticStoryActor_IsParam((int16_t)0x7E01));
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E01) == STATIC_STORY_ACTOR_DARUNIA);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E02) == STATIC_STORY_ACTOR_NABOORU);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E03) == STATIC_STORY_ACTOR_ADULT_RUTO_WATER);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E04) == STATIC_STORY_ACTOR_GREAT_FAIRY);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E05) == STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E15) == STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E25) == STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E06) == STATIC_STORY_ACTOR_ADULT_GANONDORF);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E07) == STATIC_STORY_ACTOR_PHANTOM_GANON);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E08) == STATIC_STORY_ACTOR_SKULL_KID);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E18) == STATIC_STORY_ACTOR_SKULL_KID);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E09) == STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E19) == STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E29) == STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN);
    REQUIRE(StaticStoryActor_GetPose((int16_t)0x7E11) == 1);
    REQUIRE(StaticStoryActor_GetPose((int16_t)0x7E23) == 2);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E00) == STATIC_STORY_ACTOR_NONE);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7E0A) == STATIC_STORY_ACTOR_KEATON);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7D01) == STATIC_STORY_ACTOR_NONE);
    REQUIRE(StaticStoryActor_GetType((int16_t)0x7F06) == STATIC_STORY_ACTOR_ADULT_RUTO);
    REQUIRE(StaticStoryActor_GetType(0x7F00) == 0);
    REQUIRE(StaticStoryActor_GetType(0x7F0B) == 0);
    REQUIRE(StaticStoryActor_GetType(0x0101) == 0);
    REQUIRE(StaticStoryActor_GetType(-1) == 0);
    REQUIRE(StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_IMPA, 15) == 0);
    REQUIRE(StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_FADO, 5) == 5);
    REQUIRE(StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_FADO, 6) == 0);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_CHILD_MALON, 0)->animation == STATIC_ANIM_MALON_IDLE);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_CHILD_MALON, 1)->animation == STATIC_ANIM_MALON_SING);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SARIA, 1)->animation == STATIC_ANIM_SARIA_HANDS_BEHIND);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SARIA, 2)->animation == STATIC_ANIM_SARIA_OCARINA);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SARIA, 3)->animation == STATIC_ANIM_SARIA_SEATED);
    REQUIRE(StaticStoryActor_LocksRootTranslation(STATIC_STORY_ACTOR_SARIA, 3));
    REQUIRE(!StaticStoryActor_LocksRootTranslation(STATIC_STORY_ACTOR_SARIA, 0));
    REQUIRE(!StaticStoryActor_LocksRootTranslation(STATIC_STORY_ACTOR_KOKIRI_GIRL, 3));
    StaticStoryActor_NormalizePlacementRotation(&placementPitch, &placementYaw, &placementRoll);
    REQUIRE(placementPitch == 0);
    REQUIRE(placementYaw == -0x3456);
    REQUIRE(placementRoll == 0);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_IMPA, 0)->playbackSpeed == 1.0f);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_SHEIK)->objectId == OBJECT_XC);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SHEIK, 2)->animation == STATIC_ANIM_SHEIK_HARP);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->objectId == OBJECT_RU2);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO, 1)->animation ==
            STATIC_ANIM_ADULT_RUTO_HANDS_HIPS);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 0)->animation ==
            STATIC_ANIM_ADULT_RUTO_IDLE);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 0)->waterMode == STATIC_RUTO_GROUNDED);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 1)->waterMode == STATIC_RUTO_SURFACE);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 2)->waterMode == STATIC_RUTO_DIVE_LOOP);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, &complete) == 0x403E);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_GREAT_FAIRY)->objectId == OBJECT_DY_OBJ);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_GREAT_FAIRY)->adapter == STATIC_ADAPTER_GREAT_FAIRY);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_GREAT_FAIRY)->scale == 0.035f);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_GREAT_FAIRY)->focusHeight == 262.5f);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_GREAT_FAIRY, 0)->animation ==
            STATIC_ANIM_GREAT_FAIRY_SITTING);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_GREAT_FAIRY, 1)->animation ==
            STATIC_ANIM_GREAT_FAIRY_LAYING);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_GREAT_FAIRY, 2)->animation ==
            STATIC_ANIM_GREAT_FAIRY_AFTER_SPELL);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_GREAT_FAIRY, &early) == 0x00DB);
    REQUIRE(StaticStoryActor_ShouldCloseEventMessage(true, true));
    REQUIRE(!StaticStoryActor_ShouldCloseEventMessage(true, false));
    REQUIRE(!StaticStoryActor_ShouldCloseEventMessage(false, true));
    REQUIRE(StaticStoryActor_GetResourceSource(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL) ==
            STATIC_STORY_RESOURCE_MM_ARCHIVE);
    REQUIRE(StaticStoryActor_GetResourceSource(STATIC_STORY_ACTOR_SKULL_KID) == STATIC_STORY_RESOURCE_MM_ARCHIVE);
    REQUIRE(StaticStoryActor_GetResourceSource(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN) ==
            STATIC_STORY_RESOURCE_MM_ARCHIVE);
    REQUIRE(StaticStoryActor_GetResourceSource(STATIC_STORY_ACTOR_PHANTOM_GANON) == STATIC_STORY_RESOURCE_OOT_OBJECT);
    REQUIRE(StaticStoryActor_GetResourceSource(STATIC_STORY_ACTOR_ADULT_GANONDORF) == STATIC_STORY_RESOURCE_OOT_OBJECT);
    REQUIRE(!StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_ADULT_GANONDORF));
    REQUIRE(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN));
    REQUIRE(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL));
    REQUIRE(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_SKULL_KID));
    REQUIRE(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_PHANTOM_GANON));
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 0)->animation ==
            STATIC_ANIM_TREASURE_CHEST_SHOP_GAL_IDLE);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 1)->animation ==
            STATIC_ANIM_TREASURE_CHEST_SHOP_GAL_SWAY);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 2)->animation ==
            STATIC_ANIM_TREASURE_CHEST_SHOP_GAL_IDLE);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SKULL_KID, 0)->animation ==
            STATIC_ANIM_SKULL_KID_RECLINING_FLOAT);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SKULL_KID, 1)->animation ==
            STATIC_ANIM_SKULL_KID_ARMS_CROSSED_FLOAT);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_PHANTOM_GANON, 0)->animation ==
            STATIC_ANIM_PHANTOM_GANON_NEUTRAL);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_GANONDORF, 0)->animation ==
            STATIC_ANIM_ADULT_GANONDORF_STAND);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 0)->animation ==
            STATIC_ANIM_HAPPY_MASK_SALESMAN_IDLE);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 1)->animation ==
            STATIC_ANIM_HAPPY_MASK_SALESMAN_HANDS_CLASPED);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 2)->animation ==
            STATIC_ANIM_HAPPY_MASK_SALESMAN_ARMS_OUT);
    REQUIRE(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 0));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 1));
    REQUIRE(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 2));
    REQUIRE(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SKULL_KID, 0));
    REQUIRE(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SKULL_KID, 1));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_PHANTOM_GANON, 0));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_GANONDORF, 0));
    REQUIRE(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 0));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 1));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 2));
    REQUIRE(StaticStoryActor_GetTrackingMode(STATIC_STORY_ACTOR_GREAT_FAIRY, 0) == STATIC_TRACKING_MODE_FULL);
    REQUIRE(StaticStoryActor_GetTrackingMode(STATIC_STORY_ACTOR_GREAT_FAIRY, 1) == STATIC_TRACKING_MODE_HEAD_ONLY);
    REQUIRE(StaticStoryActor_GetTrackingMode(STATIC_STORY_ACTOR_GREAT_FAIRY, 2) == STATIC_TRACKING_MODE_HEAD_ONLY);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_DARUNIA)->objectId == OBJECT_DU);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_DARUNIA)->adapter == STATIC_ADAPTER_DARUNIA);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_DARUNIA)->drawContract == STATIC_DRAW_CONTRACT_NPC_FLEX);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_DARUNIA, 0)->animation == STATIC_ANIM_DARUNIA_IDLE);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_DARUNIA, 1)->animation == STATIC_ANIM_DARUNIA_DANCE_1);
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_DARUNIA, 1));
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_DARUNIA, &complete) != 0);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_NABOORU)->objectId == OBJECT_NB);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_NABOORU)->adapter == STATIC_ADAPTER_NABOORU);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_NABOORU)->drawContract ==
            STATIC_DRAW_CONTRACT_STANDARD_OPA);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_NABOORU, 0)->animation == STATIC_ANIM_NABOORU_IDLE);
    REQUIRE(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_NABOORU, 0));
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_NABOORU, &complete) != 0);
    REQUIRE(StaticStoryActor_GetAnimationObjectId(STATIC_STORY_ACTOR_ADULT_ZELDA) == OBJECT_ZL2_ANIME2);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_ZELDA, 0)->animation == STATIC_ANIM_ADULT_ZELDA_IDLE);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->objectId == OBJECT_RU1);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_CHILD_RUTO, 2)->animation ==
            STATIC_ANIM_CHILD_RUTO_SITTING);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_ZELDA)->objectId == OBJECT_ZL2);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_ZELDA)->drawContract ==
            STATIC_DRAW_CONTRACT_FACE_FLEX);
    objects = StaticStoryActor_GetObjectRequirements(STATIC_STORY_ACTOR_ADULT_RUTO_WATER);
    REQUIRE(objects.modelObjectId == OBJECT_RU2);
    REQUIRE(objects.animationObjectId == OBJECT_RU2);
    objects = StaticStoryActor_GetObjectRequirements(STATIC_STORY_ACTOR_NABOORU);
    REQUIRE(objects.modelObjectId == OBJECT_NB);
    REQUIRE(objects.animationObjectId == OBJECT_NB);
    objects = StaticStoryActor_GetObjectRequirements(STATIC_STORY_ACTOR_ADULT_ZELDA);
    REQUIRE(objects.modelObjectId == OBJECT_ZL2);
    REQUIRE(objects.animationObjectId == OBJECT_ZL2_ANIME2);
    objects = StaticStoryActor_GetObjectRequirements(STATIC_STORY_ACTOR_DARUNIA);
    REQUIRE(objects.modelObjectId == OBJECT_DU);
    REQUIRE(objects.animationObjectId == OBJECT_DU);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_ZELDA, 0) != NULL);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_ZELDA, 1) != NULL);
    REQUIRE(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_ZELDA, 1)->skeletonFamily ==
            STATIC_SKELETON_ADULT_ZELDA);

    for (int type = STATIC_STORY_ACTOR_IMPA; type < STATIC_STORY_ACTOR_MAX; ++type) {
        definition = StaticStoryActor_GetDefinition(type);
        REQUIRE(definition != NULL);
        REQUIRE(definition->scale > 0.0f);
        REQUIRE(definition->focusHeight > 0.0f);
        REQUIRE(definition->colliderRadius > 0);
        REQUIRE(definition->colliderHeight > definition->colliderRadius);
        REQUIRE(!StaticStoryActor_CanTalk(type) || definition->talkDistance > 0.0f);
        if (definition->available) {
            REQUIRE(definition->adapter != STATIC_ADAPTER_NONE);
            for (uint8_t pose = 0; pose <= definition->maxPose; ++pose) {
                const StaticStoryPoseDescriptor* poseDescriptor =
                    StaticStoryActor_ResolvePose((StaticStoryActorType)type, pose);
                REQUIRE(poseDescriptor != NULL);
                REQUIRE(poseDescriptor->animation != STATIC_ANIM_NONE);
                REQUIRE(poseDescriptor->skeletonFamily != STATIC_SKELETON_NONE);
                REQUIRE(StaticStoryActor_CanTrack((StaticStoryActorType)type, pose) ==
                        ((definition->trackingAdapter != STATIC_TRACKING_NONE) &&
                         !(poseDescriptor->flags & STATIC_POSE_FLAG_NO_TRACKING)));
            }
        }
        REQUIRE(StaticStoryActor_SelectTextId((StaticStoryActorType)type, &early) != 0 ||
                !StaticStoryActor_CanTalk(type));
        REQUIRE(StaticStoryActor_SelectTextId((StaticStoryActorType)type, &complete) != 0 ||
                !StaticStoryActor_CanTalk(type));
    }

    REQUIRE(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SHEIK, 0));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SHEIK, 2));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_FADO, 3));
    REQUIRE(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_ZELDA, 0));
    REQUIRE(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 2));
    REQUIRE(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_RUTO, 0));
    REQUIRE(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_RUTO, 1));
    REQUIRE(StaticStoryActor_GetAdultRutoTrackingLimb(9) == STATIC_RUTO_TRACKING_LIMB_NONE);
    REQUIRE(StaticStoryActor_GetAdultRutoTrackingLimb(10) == STATIC_RUTO_TRACKING_LIMB_TORSO);
    REQUIRE(StaticStoryActor_GetAdultRutoTrackingLimb(20) == STATIC_RUTO_TRACKING_LIMB_NONE);
    REQUIRE(StaticStoryActor_GetAdultRutoTrackingLimb(21) == STATIC_RUTO_TRACKING_LIMB_HEAD);
    REQUIRE(StaticStoryActor_GetGreatFairyTrackingLimb(8) == STATIC_GREAT_FAIRY_TRACKING_LIMB_TORSO);
    REQUIRE(StaticStoryActor_GetGreatFairyTrackingLimb(15) == STATIC_GREAT_FAIRY_TRACKING_LIMB_HEAD);
    REQUIRE(StaticStoryActor_GetGreatFairyTrackingLimb(14) == STATIC_GREAT_FAIRY_TRACKING_LIMB_NONE);
    REQUIRE(StaticStoryActor_ClampGreatFairyHeadRotation(0x2000) == 0x1000);
    REQUIRE(StaticStoryActor_ClampGreatFairyHeadRotation(-0x2000) == -0x1000);
    REQUIRE(StaticStoryActor_ClampGreatFairyHeadRotation(0x0800) == 0x0800);
    REQUIRE(StaticStoryActor_GetGreatFairyHoverAmplitude(0) == 5.0f);
    REQUIRE(StaticStoryActor_GetGreatFairyHoverAmplitude(1) == 3.0f);
    REQUIRE(StaticStoryActor_GetGreatFairyHoverAmplitude(2) == 5.0f);
    REQUIRE(StaticStoryActor_GetFixedEyeIndex(STATIC_STORY_ACTOR_SHEIK, 2) == 2);
    REQUIRE(StaticStoryActor_GetFixedEyeIndex(STATIC_STORY_ACTOR_SHEIK, 0) == -1);
    REQUIRE(StaticStoryActor_GetFaceProfile(STATIC_STORY_ACTOR_IMPA) == STATIC_FACE_PROFILE_IMPA);
    REQUIRE(StaticStoryActor_GetFaceProfile(STATIC_STORY_ACTOR_ADULT_RUTO) == STATIC_FACE_PROFILE_ADULT_RUTO);
    REQUIRE(StaticStoryActor_GetFaceProfile(STATIC_STORY_ACTOR_ADULT_RUTO_WATER) == STATIC_FACE_PROFILE_ADULT_RUTO);
    REQUIRE(StaticStoryActor_ResolveEyeIndex(STATIC_STORY_ACTOR_IMPA, 2, false) == 2);
    REQUIRE(StaticStoryActor_ResolveEyeIndex(STATIC_STORY_ACTOR_IMPA, 2, true) == 0);
    REQUIRE(StaticStoryActor_ResolveEyeIndex(STATIC_STORY_ACTOR_ADULT_RUTO, 1, true) == 0);
    REQUIRE(StaticStoryActor_ResolveEyeIndex(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 2, true) == 0);
    REQUIRE(StaticStoryActor_ResolveEyeIndex(STATIC_STORY_ACTOR_SARIA, 1, true) == 1);
    REQUIRE(StaticStoryActor_ResolveEyeIndex(STATIC_STORY_ACTOR_IMPA, 9, false) == 0);
    /* An alternate Impa skeleton owns its complete head/material graph. */
    REQUIRE(!StaticStoryActor_ShouldOverrideImpaHead(true));
    REQUIRE(StaticStoryActor_ShouldOverrideImpaHead(false));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_CHILD_MALON, 1));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SARIA, 2));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SARIA, 3));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_CHILD_RUTO, 2));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_KOKIRI_GIRL, 3));
    REQUIRE(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_MALON, 2));
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_IMPA)->trackingAdapter == STATIC_TRACKING_IMPA);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_MALON)->trackingAdapter ==
            STATIC_TRACKING_CHILD_MALON);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_SARIA)->trackingAdapter == STATIC_TRACKING_SARIA);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_SHEIK)->trackingAdapter == STATIC_TRACKING_SHEIK);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->trackingAdapter ==
            STATIC_TRACKING_CHILD_RUTO);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_KOKIRI_GIRL)->trackingAdapter == STATIC_TRACKING_KOKIRI);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_FADO)->trackingAdapter == STATIC_TRACKING_KOKIRI);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_MALON)->trackingAdapter ==
            STATIC_TRACKING_ADULT_MALON);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_ZELDA)->trackingAdapter ==
            STATIC_TRACKING_ADULT_ZELDA);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->trackingAdapter ==
            STATIC_TRACKING_ADULT_RUTO);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_IMPA)->trackingPreset == 12);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_MALON)->trackingPreset == 0);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_SARIA)->trackingPreset == 2);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->trackingPreset == 12);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->trackingPreset == 12);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_KOKIRI_GIRL)->trackingPreset == 2);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_MALON)->trackingPreset == 0);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_MALON)->trackingYOffset == 0.0f);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_MALON)->trackingTargetYOffset == 10.0f);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_MALON, &early) !=
            StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_MALON, &complete));
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SARIA, &early) !=
            StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SARIA, &complete));
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_IMPA, &early) !=
            StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_IMPA, &complete));
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_ZELDA, &early) !=
            StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_ZELDA, &complete));
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SHEIK, &early) !=
            StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SHEIK, &complete));
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_RUTO, &early) !=
            StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_RUTO, &complete));
    /* Static Child Ruto must never enter the 0x404C-0x404E Jabu dialogue chain. */
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_RUTO, &early) == 0x402C);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_RUTO, &complete) == 0x402C);
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_KOKIRI_GIRL, &early) !=
            StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_KOKIRI_GIRL, &complete));
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_FADO, &early) !=
            StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_FADO, &complete));
    REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_MALON, &early) !=
            StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_MALON, &complete));

    REQUIRE(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_ADULT_ZELDA));
    REQUIRE(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_ADULT_MALON));
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->colliderRadius >
            StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->colliderRadius);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->colliderHeight >
            StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->colliderHeight);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->blinkMin == 60);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->blinkRange == 60);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->blinkMin == 60);
    REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->blinkRange == 60);
    return 0;
}
