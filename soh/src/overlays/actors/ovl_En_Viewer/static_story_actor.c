#include "static_story_actor.h"
#include "static_story_dialogue.h"

#include <stddef.h>

#include "z64object.h"

#define STATIC_POSE(animation, flags, skeleton) \
    { animation, 1.0f, flags, skeleton, STATIC_RUTO_GROUNDED }
#define STATIC_WATER_POSE(animation, flags, skeleton, waterMode) \
    { animation, 1.0f, flags, skeleton, waterMode }

static const StaticStoryActorDefinition sDefinitions[STATIC_STORY_ACTOR_MAX] = {
    [STATIC_STORY_ACTOR_IMPA] = { 0, 1, OBJECT_IM, STATIC_ADAPTER_IMPA, 0.01f, 50.0f, 18, 46, 0, 30, 30, 80.0f,
                                  STATIC_TRACKING_IMPA, 12, 4.0f },
    [STATIC_STORY_ACTOR_CHILD_MALON] = { 2, 1, OBJECT_MA1, STATIC_ADAPTER_MALON, 0.01f, 42.0f, 18, 46, 0, 30, 30, 70.0f,
                                         STATIC_TRACKING_CHILD_MALON, 0, 0.0f, 10.0f },
    [STATIC_STORY_ACTOR_SARIA] = { 3, 1, OBJECT_SA, STATIC_ADAPTER_SARIA, 0.01f, 40.0f, 8, 46, 0, 30, 30, 70.0f,
                                   STATIC_TRACKING_SARIA, 2, 4.0f },
    [STATIC_STORY_ACTOR_ADULT_ZELDA] = { 1, 1, OBJECT_ZL2, STATIC_ADAPTER_ADULT_ZELDA, 0.01f, 60.0f, 25, 80, 0, 30, 30,
                                         90.0f, STATIC_TRACKING_ADULT_ZELDA, 12, -3.0f, 0.0f,
                                         STATIC_DRAW_CONTRACT_FACE_FLEX },
    [STATIC_STORY_ACTOR_SHEIK] = { 2, 1, OBJECT_XC, STATIC_ADAPTER_SHEIK, 0.01f, 52.0f, 25, 80, 0, 60, 60, 90.0f,
                                   STATIC_TRACKING_SHEIK, 12, -3.0f },
    [STATIC_STORY_ACTOR_ADULT_RUTO] = { 2, 1, OBJECT_RU2, STATIC_ADAPTER_ADULT_RUTO, 0.01f, 54.0f, 30, 100, 0, 60, 60,
                                        90.0f, STATIC_TRACKING_ADULT_RUTO, 12, -3.0f },
    [STATIC_STORY_ACTOR_CHILD_RUTO] = { 2, 1, OBJECT_RU1, STATIC_ADAPTER_CHILD_RUTO, 0.01f, 42.0f, 25, 80, 0, 60, 60,
                                        80.0f, STATIC_TRACKING_CHILD_RUTO, 12, -3.0f },
    [STATIC_STORY_ACTOR_KOKIRI_GIRL] = { 5, 1, OBJECT_KW1, STATIC_ADAPTER_KOKIRI_GIRL, 0.01f, 40.0f, 20, 46, 0, 30, 30,
                                         70.0f, STATIC_TRACKING_KOKIRI, 2, 0.0f },
    [STATIC_STORY_ACTOR_FADO] = { 5, 1, OBJECT_FA, STATIC_ADAPTER_FADO, 0.01f, 40.0f, 20, 46, 0, 30, 30, 70.0f,
                                  STATIC_TRACKING_KOKIRI, 2, 0.0f },
    [STATIC_STORY_ACTOR_ADULT_MALON] = { 3, 1, OBJECT_MA2, STATIC_ADAPTER_ADULT_MALON, 0.01f, 52.0f, 18, 46, 0, 30, 30,
                                         80.0f, STATIC_TRACKING_ADULT_MALON, 0, 0.0f },
    [STATIC_STORY_ACTOR_DARUNIA] = { 1, 1, OBJECT_DU, STATIC_ADAPTER_DARUNIA, 0.01f, 60.0f, 28, 70, 0, 30, 30, 120.0f,
                                     STATIC_TRACKING_DARUNIA, 12, 0.0f, 0.0f, STATIC_DRAW_CONTRACT_NPC_FLEX },
    [STATIC_STORY_ACTOR_NABOORU] = { 0, 1, OBJECT_NB, STATIC_ADAPTER_NABOORU, 0.01f, 60.0f, 25, 80, 0, 30, 30, 110.0f,
                                     STATIC_TRACKING_NABOORU, 12, 0.0f },
    [STATIC_STORY_ACTOR_ADULT_RUTO_WATER] = { 2, 1, OBJECT_RU2, STATIC_ADAPTER_ADULT_RUTO, 0.01f, 54.0f, 30, 100, 0, 60,
                                              60, 90.0f, STATIC_TRACKING_ADULT_RUTO, 12, -3.0f },
    [STATIC_STORY_ACTOR_GREAT_FAIRY] = { 2, 1, OBJECT_DY_OBJ, STATIC_ADAPTER_GREAT_FAIRY, 0.035f, 262.5f, 45, 220, 0,
                                         20, 60, 180.0f, STATIC_TRACKING_GREAT_FAIRY, 12, 0.0f },
    [STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL] = { 2, 1, OBJECT_INVALID, STATIC_ADAPTER_MM_TREASURE_CHEST_SHOP_GAL,
                                                     0.01f, 52.0f, 20, 60, 0, 30, 30, 90.0f,
                                                     STATIC_TRACKING_TREASURE_CHEST_SHOP_GAL, 12, 0.0f },
    [STATIC_STORY_ACTOR_ADULT_GANONDORF] = { 0, 0, OBJECT_GANON, STATIC_ADAPTER_ADULT_GANONDORF, 0.01f, 90.0f, 35, 110,
                                             0, 30, 30, 120.0f, STATIC_TRACKING_NONE, 0, 0.0f },
    [STATIC_STORY_ACTOR_PHANTOM_GANON] = { 0, 1, OBJECT_GND, STATIC_ADAPTER_PHANTOM_GANON, 0.01f, 80.0f, 35, 100, 0, 30,
                                           30, 120.0f, STATIC_TRACKING_NONE, 0, 0.0f },
    [STATIC_STORY_ACTOR_SKULL_KID] = { 1, 1, OBJECT_INVALID, STATIC_ADAPTER_MM_SKULL_KID, 0.01f, 55.0f, 20, 60, 0, 30,
                                       30, 90.0f, STATIC_TRACKING_SKULL_KID, 0, 0.0f },
    [STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN] = { 2, 1, OBJECT_INVALID, STATIC_ADAPTER_MM_HAPPY_MASK_SALESMAN, 0.01f,
                                                 60.0f, 22, 70, 0, 30, 30, 100.0f, STATIC_TRACKING_HAPPY_MASK_SALESMAN,
                                                 12, 0.0f },
    [STATIC_STORY_ACTOR_KEATON] = { 2, 1, OBJECT_INVALID, STATIC_ADAPTER_MM_KEATON, 0.01f, 35.0f, 18, 50, 0, 30, 30,
                                    70.0f, STATIC_TRACKING_NONE, 0, 0.0f },
    [STATIC_STORY_ACTOR_CHILD_KAFEI] = { 1, 1, OBJECT_INVALID, STATIC_ADAPTER_MM_KAFEI, 0.01f, 45.0f, 18, 60, 0, 30, 30,
                                         80.0f, STATIC_TRACKING_NONE, 0, 0.0f },
    [STATIC_STORY_ACTOR_LULU] = { 3, 1, OBJECT_INVALID, STATIC_ADAPTER_MM_LULU, 0.01f, 60.0f, 22, 70, 0, 30, 30, 100.0f,
                                  STATIC_TRACKING_LULU, 12, 0.0f },
    [STATIC_STORY_ACTOR_ANJU] = { 1, 1, OBJECT_INVALID, STATIC_ADAPTER_MM_ANJU, 0.01f, 40.0f, 20, 50, 0, 30, 30, 90.0f,
                                  STATIC_TRACKING_NONE, 0, 0.0f },
};

static const StaticStoryPoseDescriptor sPoses[STATIC_STORY_ACTOR_MAX][STATIC_STORY_ACTOR_POSE_COUNT] = {
    [STATIC_STORY_ACTOR_IMPA] = { STATIC_POSE(STATIC_ANIM_IMPA_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_IMPA) },
    [STATIC_STORY_ACTOR_CHILD_MALON] = {
        STATIC_POSE(STATIC_ANIM_MALON_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_MALON_CHILD),
        STATIC_POSE(STATIC_ANIM_MALON_SING, STATIC_POSE_FLAG_VOCAL | STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_MALON_CHILD),
        STATIC_POSE(STATIC_ANIM_MALON_SING, STATIC_POSE_FLAG_VOCAL | STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_MALON_CHILD),
    },
    [STATIC_STORY_ACTOR_SARIA] = {
        STATIC_POSE(STATIC_ANIM_SARIA_ARMS_TO_SIDE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_SARIA),
        STATIC_POSE(STATIC_ANIM_SARIA_HANDS_BEHIND, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_SARIA),
        STATIC_POSE(STATIC_ANIM_SARIA_OCARINA, STATIC_POSE_FLAG_OCARINA | STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_SARIA),
        STATIC_POSE(STATIC_ANIM_SARIA_SEATED,
                    STATIC_POSE_FLAG_NO_TRACKING | STATIC_POSE_FLAG_LOCK_ROOT_TRANSLATION, STATIC_SKELETON_SARIA),
    },
    [STATIC_STORY_ACTOR_ADULT_ZELDA] = {
        STATIC_POSE(STATIC_ANIM_ADULT_ZELDA_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_ZELDA),
        STATIC_POSE(STATIC_ANIM_ADULT_ZELDA_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_ZELDA),
    },
    [STATIC_STORY_ACTOR_SHEIK] = {
        STATIC_POSE(STATIC_ANIM_SHEIK_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_SHEIK),
        STATIC_POSE(STATIC_ANIM_SHEIK_ARMS_CROSSED, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_SHEIK),
        STATIC_POSE(STATIC_ANIM_SHEIK_HARP, STATIC_POSE_FLAG_NO_TRACKING | STATIC_POSE_FLAG_CLOSED_EYES,
                    STATIC_SKELETON_SHEIK),
    },
    [STATIC_STORY_ACTOR_ADULT_RUTO] = {
        STATIC_POSE(STATIC_ANIM_ADULT_RUTO_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_RUTO),
        STATIC_POSE(STATIC_ANIM_ADULT_RUTO_HANDS_HIPS, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_RUTO),
        STATIC_POSE(STATIC_ANIM_ADULT_RUTO_LOOK_DOWN_LEFT, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_RUTO),
    },
    [STATIC_STORY_ACTOR_CHILD_RUTO] = {
        STATIC_POSE(STATIC_ANIM_CHILD_RUTO_HANDS_BEHIND, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_CHILD_RUTO),
        STATIC_POSE(STATIC_ANIM_CHILD_RUTO_HANDS_HIPS, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_CHILD_RUTO),
        STATIC_POSE(STATIC_ANIM_CHILD_RUTO_SITTING, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_CHILD_RUTO),
    },
    [STATIC_STORY_ACTOR_KOKIRI_GIRL] = {
        STATIC_POSE(STATIC_ANIM_KOKIRI_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_ARMS_BEHIND, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_HANDS_HIPS, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_HEAD_HAND, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_CROSSED_LEGS, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_CROSSED_ARMS_LEGS, STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_KOKIRI),
    },
    [STATIC_STORY_ACTOR_FADO] = {
        STATIC_POSE(STATIC_ANIM_KOKIRI_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_ARMS_BEHIND, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_HANDS_HIPS, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_HEAD_HAND, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_CROSSED_LEGS, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_CROSSED_ARMS_LEGS, STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_KOKIRI),
    },
    [STATIC_STORY_ACTOR_ADULT_MALON] = {
        STATIC_POSE(STATIC_ANIM_ADULT_MALON_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_MALON_ADULT),
        STATIC_POSE(STATIC_ANIM_ADULT_MALON_BASKET, STATIC_POSE_FLAG_BASKET, STATIC_SKELETON_MALON_ADULT),
        STATIC_POSE(STATIC_ANIM_ADULT_MALON_SING, STATIC_POSE_FLAG_VOCAL | STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_MALON_ADULT),
        STATIC_POSE(STATIC_ANIM_ADULT_MALON_SING, STATIC_POSE_FLAG_VOCAL | STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_MALON_ADULT),
    },
    [STATIC_STORY_ACTOR_ADULT_RUTO_WATER] = {
        STATIC_WATER_POSE(STATIC_ANIM_ADULT_RUTO_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_RUTO,
                          STATIC_RUTO_GROUNDED),
        STATIC_WATER_POSE(STATIC_ANIM_ADULT_RUTO_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_RUTO,
                          STATIC_RUTO_SURFACE),
        STATIC_WATER_POSE(STATIC_ANIM_ADULT_RUTO_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_RUTO,
                          STATIC_RUTO_DIVE_LOOP),
    },
    [STATIC_STORY_ACTOR_DARUNIA] = {
        STATIC_POSE(STATIC_ANIM_DARUNIA_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_DARUNIA),
        STATIC_POSE(STATIC_ANIM_DARUNIA_DANCE_1, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_DARUNIA),
    },
    [STATIC_STORY_ACTOR_NABOORU] = {
        STATIC_POSE(STATIC_ANIM_NABOORU_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_NABOORU),
    },
    [STATIC_STORY_ACTOR_GREAT_FAIRY] = {
        STATIC_POSE(STATIC_ANIM_GREAT_FAIRY_SITTING, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_GREAT_FAIRY),
        STATIC_POSE(STATIC_ANIM_GREAT_FAIRY_LAYING, STATIC_POSE_FLAG_HEAD_ONLY_TRACKING,
                    STATIC_SKELETON_GREAT_FAIRY),
        STATIC_POSE(STATIC_ANIM_GREAT_FAIRY_AFTER_SPELL, STATIC_POSE_FLAG_HEAD_ONLY_TRACKING,
                    STATIC_SKELETON_GREAT_FAIRY),
    },
    [STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL] = {
        STATIC_POSE(STATIC_ANIM_TREASURE_CHEST_SHOP_GAL_IDLE, STATIC_POSE_FLAG_NONE,
                    STATIC_SKELETON_TREASURE_CHEST_SHOP_GAL),
        STATIC_POSE(STATIC_ANIM_TREASURE_CHEST_SHOP_GAL_SWAY, STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_TREASURE_CHEST_SHOP_GAL),
        STATIC_POSE(STATIC_ANIM_TREASURE_CHEST_SHOP_GAL_IDLE, STATIC_POSE_FLAG_NONE,
                    STATIC_SKELETON_TREASURE_CHEST_SHOP_GAL),
    },
    [STATIC_STORY_ACTOR_ADULT_GANONDORF] = {
        STATIC_POSE(STATIC_ANIM_ADULT_GANONDORF_STAND, STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_ADULT_GANONDORF),
    },
    [STATIC_STORY_ACTOR_PHANTOM_GANON] = {
        STATIC_POSE(STATIC_ANIM_PHANTOM_GANON_NEUTRAL, STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_PHANTOM_GANON),
    },
    [STATIC_STORY_ACTOR_SKULL_KID] = {
        STATIC_POSE(STATIC_ANIM_SKULL_KID_RECLINING_FLOAT, STATIC_POSE_FLAG_NONE,
                    STATIC_SKELETON_SKULL_KID),
        STATIC_POSE(STATIC_ANIM_SKULL_KID_ARMS_CROSSED_FLOAT, STATIC_POSE_FLAG_NONE,
                    STATIC_SKELETON_SKULL_KID),
    },
    [STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN] = {
        STATIC_POSE(STATIC_ANIM_HAPPY_MASK_SALESMAN_IDLE, STATIC_POSE_FLAG_NONE,
                    STATIC_SKELETON_HAPPY_MASK_SALESMAN),
        STATIC_POSE(STATIC_ANIM_HAPPY_MASK_SALESMAN_HANDS_CLASPED, STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_HAPPY_MASK_SALESMAN),
        STATIC_POSE(STATIC_ANIM_HAPPY_MASK_SALESMAN_ARMS_OUT, STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_HAPPY_MASK_SALESMAN),
    },
    [STATIC_STORY_ACTOR_KEATON] = {
        STATIC_POSE(STATIC_ANIM_KEATON_IDLE, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KEATON),
        STATIC_POSE(STATIC_ANIM_KEATON_CHUCKLE, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KEATON),
        STATIC_POSE(STATIC_ANIM_KEATON_CELEBRATE, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KEATON),
    },
    [STATIC_STORY_ACTOR_CHILD_KAFEI] = {
        STATIC_POSE(STATIC_ANIM_KAFEI_IDLE, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KAFEI),
        STATIC_POSE(STATIC_ANIM_KAFEI_GESTURE, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KAFEI),
    },
    [STATIC_STORY_ACTOR_LULU] = {
        STATIC_POSE(STATIC_ANIM_LULU_LOOK_DOWN, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_LULU),
        STATIC_POSE(STATIC_ANIM_LULU_LOOK_LEFT, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_LULU),
        STATIC_POSE(STATIC_ANIM_LULU_SING, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_LULU),
        STATIC_POSE(STATIC_ANIM_LULU_LOOK_AROUND, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_LULU),
    },
    [STATIC_STORY_ACTOR_ANJU] = {
        /* Keep the native seated root motion; the placement itself never moves. */
        STATIC_POSE(STATIC_ANIM_ANJU_UMBRELLA_CRY, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_ANJU),
        STATIC_POSE(STATIC_ANIM_ANJU_UMBRELLA_IDLE, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_ANJU),
    },
};

_Static_assert(sizeof(sDefinitions) / sizeof(sDefinitions[0]) == STATIC_STORY_ACTOR_MAX,
               "Every static story actor type needs a definition");
_Static_assert(sizeof(sPoses) / sizeof(sPoses[0]) == STATIC_STORY_ACTOR_MAX,
               "Every static story actor type needs a pose row");
enum { STATIC_STORY_DEFINITION_COUNT = 23, STATIC_STORY_POSE_ROW_COUNT = 23 };
_Static_assert(STATIC_STORY_DEFINITION_COUNT == STATIC_STORY_ACTOR_MAX - 1,
               "Definition count must change with the actor registry");
_Static_assert(STATIC_STORY_POSE_ROW_COUNT == STATIC_STORY_ACTOR_MAX - 1,
               "Pose-row count must change with the actor registry");

int StaticStoryActor_IsParam(int16_t params) {
    uint16_t prefix = (uint16_t)params & 0xFF00;

    return prefix == STATIC_STORY_ACTOR_LEGACY_PARAM_PREFIX || prefix == STATIC_STORY_ACTOR_EXPANDED_PARAM_PREFIX;
}

StaticStoryActorType StaticStoryActor_GetType(int16_t params) {
    uint16_t prefix = (uint16_t)params & 0xFF00;
    uint16_t id = (uint16_t)params & 0x000F;
    StaticStoryActorType type;

    if (prefix == STATIC_STORY_ACTOR_EXPANDED_PARAM_PREFIX) {
        switch (id) {
            case 1:
                return STATIC_STORY_ACTOR_DARUNIA;
            case 2:
                return STATIC_STORY_ACTOR_NABOORU;
            case 3:
                return STATIC_STORY_ACTOR_ADULT_RUTO_WATER;
            case 4:
                return STATIC_STORY_ACTOR_GREAT_FAIRY;
            case 5:
                return STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL;
            case 6:
                return STATIC_STORY_ACTOR_ADULT_GANONDORF;
            case 7:
                return STATIC_STORY_ACTOR_PHANTOM_GANON;
            case 8:
                return STATIC_STORY_ACTOR_SKULL_KID;
            case 9:
                return STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN;
            case 10:
                return STATIC_STORY_ACTOR_KEATON;
            case 11:
                return STATIC_STORY_ACTOR_CHILD_KAFEI;
            case 12:
                return STATIC_STORY_ACTOR_LULU;
            case 13:
                return STATIC_STORY_ACTOR_ANJU;
            default:
                return STATIC_STORY_ACTOR_NONE;
        }
    }

    type = (StaticStoryActorType)id;

    return prefix == STATIC_STORY_ACTOR_LEGACY_PARAM_PREFIX && type > STATIC_STORY_ACTOR_NONE &&
                   type <= STATIC_STORY_ACTOR_ADULT_MALON
               ? type
               : STATIC_STORY_ACTOR_NONE;
}

uint8_t StaticStoryActor_GetPose(int16_t params) {
    return StaticStoryActor_IsParam(params) ? (((uint16_t)params >> 4) & 0x0F) : 0;
}

const StaticStoryActorDefinition* StaticStoryActor_GetDefinition(StaticStoryActorType type) {
    return type > STATIC_STORY_ACTOR_NONE && type < STATIC_STORY_ACTOR_MAX ? &sDefinitions[type] : NULL;
}

int16_t StaticStoryActor_GetAnimationObjectId(StaticStoryActorType type) {
    const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(type);

    if (definition == NULL) {
        return OBJECT_INVALID;
    }
    if (type == STATIC_STORY_ACTOR_ADULT_ZELDA) {
        return OBJECT_ZL2_ANIME2;
    }
    if (type == STATIC_STORY_ACTOR_ADULT_GANONDORF) {
        return OBJECT_GANON_ANIME2;
    }
    return definition->objectId;
}

StaticStoryObjectRequirements StaticStoryActor_GetObjectRequirements(StaticStoryActorType type) {
    const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(type);
    StaticStoryObjectRequirements requirements = { OBJECT_INVALID, OBJECT_INVALID };

    if (definition != NULL) {
        requirements.modelObjectId = definition->objectId;
        requirements.animationObjectId = StaticStoryActor_GetAnimationObjectId(type);
    }
    return requirements;
}

int StaticStoryActor_IsAvailable(StaticStoryActorType type) {
    const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(type);

    return definition != NULL && definition->available;
}

uint8_t StaticStoryActor_SanitizePose(StaticStoryActorType type, uint8_t pose) {
    const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(type);

    if (definition == NULL || pose > definition->maxPose || sPoses[type][pose].animation == STATIC_ANIM_NONE) {
        return 0;
    }
    return pose;
}

const StaticStoryPoseDescriptor* StaticStoryActor_ResolvePose(StaticStoryActorType type, uint8_t pose) {
    if (StaticStoryActor_GetDefinition(type) == NULL) {
        return NULL;
    }
    return &sPoses[type][StaticStoryActor_SanitizePose(type, pose)];
}

uint16_t StaticStoryActor_SelectTextId(StaticStoryActorType type, const StaticStoryProgression* progression) {
    /*
     * These are ordinary actor-offered messages.  The static path only assigns
     * the ID and lets the message system close it; it intentionally never
     * dispatches the source actor's follow-up action/update function.
     */
    StaticStoryProgression empty = { 0 };

    if (progression == NULL) {
        progression = &empty;
    }

    switch (type) {
        case STATIC_STORY_ACTOR_IMPA:
            return progression->metZelda ? 0x708E : 0x708D;
        case STATIC_STORY_ACTOR_CHILD_MALON:
            return progression->eponaComplete ? 0x204A : 0x2041;
        case STATIC_STORY_ACTOR_SARIA:
            return progression->forestComplete ? 0x10AD : 0x1001;
        case STATIC_STORY_ACTOR_ADULT_ZELDA:
            return progression->metZelda ? 0x70FE : 0x70FF;
        case STATIC_STORY_ACTOR_SHEIK:
            return progression->waterComplete ? 0x7010 : 0x700F;
        case STATIC_STORY_ACTOR_ADULT_RUTO:
            return progression->waterComplete ? 0x403E : 0x402C;
        case STATIC_STORY_ACTOR_CHILD_RUTO:
            /* Post-Jabu throne-room text: closes normally without En_Ru1's Jabu action chain. */
            return 0x402C;
        case STATIC_STORY_ACTOR_KOKIRI_GIRL:
            return progression->forestComplete ? 0x10DA : 0x1004;
        case STATIC_STORY_ACTOR_FADO:
            return progression->forestComplete ? 0x10D9 : 0x1005;
        case STATIC_STORY_ACTOR_ADULT_MALON:
            return progression->eponaComplete ? 0x2056 : 0x204C;
        case STATIC_STORY_ACTOR_DARUNIA:
            return progression->forestComplete ? 0x301E : 0x301A;
        case STATIC_STORY_ACTOR_NABOORU:
            return progression->forestComplete ? 0x6012 : 0x600C;
        case STATIC_STORY_ACTOR_ADULT_RUTO_WATER:
            return progression->waterComplete ? 0x403E : 0x402C;
        case STATIC_STORY_ACTOR_GREAT_FAIRY:
            return 0x00DB;
        case STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL:
            return STATIC_STORY_TEXT_TREASURE_CHEST_SHOP_GAL;
        case STATIC_STORY_ACTOR_SKULL_KID:
            return STATIC_STORY_TEXT_SKULL_KID;
        case STATIC_STORY_ACTOR_KEATON:
            return STATIC_STORY_TEXT_KEATON;
        case STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN:
            return STATIC_STORY_TEXT_HAPPY_MASK_SALESMAN;
        case STATIC_STORY_ACTOR_CHILD_KAFEI:
            return STATIC_STORY_TEXT_CHILD_KAFEI;
        case STATIC_STORY_ACTOR_LULU:
            return STATIC_STORY_TEXT_LULU;
        case STATIC_STORY_ACTOR_ANJU:
            return STATIC_STORY_TEXT_ANJU;
        case STATIC_STORY_ACTOR_ADULT_GANONDORF:
            return 0x00DB;
        case STATIC_STORY_ACTOR_PHANTOM_GANON:
            return 0;
        default:
            return 0;
    }
}

bool StaticStoryActor_CanTalk(StaticStoryActorType type) {
    return StaticStoryActor_GetDefinition(type) != NULL && type != STATIC_STORY_ACTOR_PHANTOM_GANON;
}

void StaticStoryActor_NormalizePlacementRotation(int16_t* pitch, int16_t* yaw, int16_t* roll) {
    (void)yaw;
    *pitch = 0;
    *roll = 0;
}

int StaticStoryActor_LocksRootTranslation(StaticStoryActorType type, uint8_t pose) {
    const StaticStoryPoseDescriptor* descriptor = StaticStoryActor_ResolvePose(type, pose);

    return descriptor != NULL && (descriptor->flags & STATIC_POSE_FLAG_LOCK_ROOT_TRANSLATION) != 0;
}

int StaticStoryActor_CanTrack(StaticStoryActorType type, uint8_t pose) {
    return StaticStoryActor_GetTrackingMode(type, pose) != STATIC_TRACKING_MODE_NONE;
}

bool StaticStoryActor_ShouldCloseEventMessage(bool isEventState, bool shouldAdvance) {
    return isEventState && shouldAdvance;
}

StaticStoryTrackingMode StaticStoryActor_GetTrackingMode(StaticStoryActorType type, uint8_t pose) {
    const StaticStoryPoseDescriptor* poseDescriptor = StaticStoryActor_ResolvePose(type, pose);
    const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(type);

    if (definition == NULL || poseDescriptor == NULL || (poseDescriptor->flags & STATIC_POSE_FLAG_NO_TRACKING)) {
        return STATIC_TRACKING_MODE_NONE;
    }
    if (definition->trackingAdapter == STATIC_TRACKING_NONE) {
        return STATIC_TRACKING_MODE_NONE;
    }
    if (definition->trackingAdapter == STATIC_TRACKING_SKULL_KID) {
        return STATIC_TRACKING_MODE_BODY_YAW;
    }
    return poseDescriptor->flags & STATIC_POSE_FLAG_HEAD_ONLY_TRACKING ? STATIC_TRACKING_MODE_HEAD_ONLY
                                                                       : STATIC_TRACKING_MODE_FULL;
}

StaticStoryAdultRutoTrackingLimb StaticStoryActor_GetAdultRutoTrackingLimb(int limbIndex) {
    /* Flex callbacks number object_ru2's root as limb 1. */
    if (limbIndex == 10) {
        return STATIC_RUTO_TRACKING_LIMB_TORSO;
    }
    if (limbIndex == 21) {
        return STATIC_RUTO_TRACKING_LIMB_HEAD;
    }
    return STATIC_RUTO_TRACKING_LIMB_NONE;
}

StaticStoryGreatFairyTrackingLimb StaticStoryActor_GetGreatFairyTrackingLimb(int limbIndex) {
    if (limbIndex == 8) {
        return STATIC_GREAT_FAIRY_TRACKING_LIMB_TORSO;
    }
    if (limbIndex == 15) {
        return STATIC_GREAT_FAIRY_TRACKING_LIMB_HEAD;
    }
    return STATIC_GREAT_FAIRY_TRACKING_LIMB_NONE;
}

int16_t StaticStoryActor_ClampGreatFairyHeadRotation(int16_t rotation) {
    if (rotation > 0x1000) {
        return 0x1000;
    }
    if (rotation < -0x1000) {
        return -0x1000;
    }
    return rotation;
}

float StaticStoryActor_GetGreatFairyHoverAmplitude(uint8_t pose) {
    return StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_GREAT_FAIRY, pose) == 1 ? 3.0f : 5.0f;
}

int8_t StaticStoryActor_GetFixedEyeIndex(StaticStoryActorType type, uint8_t pose) {
    if (type == STATIC_STORY_ACTOR_ANJU)
        return 0; /* The sole loaded eye is the native crying expression. */
    const StaticStoryPoseDescriptor* descriptor = StaticStoryActor_ResolvePose(type, pose);
    return descriptor != NULL && (descriptor->flags & (STATIC_POSE_FLAG_OCARINA | STATIC_POSE_FLAG_CLOSED_EYES)) ? 2
                                                                                                                 : -1;
}

StaticStoryFaceProfile StaticStoryActor_GetFaceProfile(StaticStoryActorType type) {
    if (type == STATIC_STORY_ACTOR_IMPA) {
        return STATIC_FACE_PROFILE_IMPA;
    }
    if (type == STATIC_STORY_ACTOR_ADULT_RUTO || type == STATIC_STORY_ACTOR_ADULT_RUTO_WATER) {
        return STATIC_FACE_PROFILE_ADULT_RUTO;
    }
    if (type == STATIC_STORY_ACTOR_GREAT_FAIRY) {
        return STATIC_FACE_PROFILE_GREAT_FAIRY;
    }
    return STATIC_FACE_PROFILE_STANDARD;
}

uint8_t StaticStoryActor_ResolveEyeIndex(StaticStoryActorType type, uint8_t requestedEyeIndex, bool hasAlternateHead) {
    if (requestedEyeIndex >= 3) {
        return 0;
    }
    if (hasAlternateHead && (type == STATIC_STORY_ACTOR_IMPA || type == STATIC_STORY_ACTOR_ADULT_RUTO ||
                             type == STATIC_STORY_ACTOR_ADULT_RUTO_WATER)) {
        return 0;
    }
    return requestedEyeIndex;
}

bool StaticStoryActor_ShouldOverrideImpaHead(bool hasAlternateSkeleton) {
    return !hasAlternateSkeleton;
}

StaticStoryResourceSource StaticStoryActor_GetResourceSource(StaticStoryActorType type) {
    switch (type) {
        case STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL:
        case STATIC_STORY_ACTOR_SKULL_KID:
        case STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN:
        case STATIC_STORY_ACTOR_KEATON:
        case STATIC_STORY_ACTOR_LULU:
        case STATIC_STORY_ACTOR_CHILD_KAFEI:
        case STATIC_STORY_ACTOR_ANJU:
            return STATIC_STORY_RESOURCE_MM_ARCHIVE;
        default:
            return STATIC_STORY_RESOURCE_OOT_OBJECT;
    }
}
