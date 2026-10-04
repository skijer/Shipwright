#include "static_story_mm_actor.h"

#include <stddef.h>
#include <math.h>
#include <string.h>

#define STATIC_STORY_MM_TAU 6.28318530717958647692f
#define STATIC_STORY_MM_HOVER_AMPLITUDE 10.0f

static const StaticStoryMmPresentation sTreasureChestShopGalPresentations[] = {
    { "objects/object_bg/gTreasureChestShopGalSkel", "objects/object_bg/object_bg_Anim_009890", NULL, NULL, NULL, NULL,
      NULL, 23, STATIC_STORY_MM_TRACKING_HEAD_TORSO, false },
    { "objects/object_bg/gTreasureChestShopGalSkel", "objects/object_bg/object_bg_Anim_001384", NULL, NULL, NULL, NULL,
      NULL, 23, STATIC_STORY_MM_TRACKING_NONE, false },
    { "objects/object_bg/gTreasureChestShopGalSkel", "objects/object_bg/object_bg_Anim_009890", NULL, NULL, NULL, NULL,
      NULL, 23, STATIC_STORY_MM_TRACKING_HEAD_TORSO, false },
};

static const char* sTreasureChestShopGalEyeTexturePaths[] = {
    "objects/object_bg/gTreasureChestShopGalEyeOpenDownTex",
    "objects/object_bg/gTreasureChestShopGalEyeHalfDownTex",
    "objects/object_bg/gTreasureChestShopGalEyeClosedTex",
    "objects/object_bg/gTreasureChestShopGalEyeHalfDownTex",
};

static const StaticStoryMmPresentation sSkullKidPresentations[] = {
    { "objects/object_stk/gSkullKidSkel", "objects/object_stk2/gSkullKidRecliningFloatAnim",
      "objects/gameplay_keep/gameplay_keep_Skel_02AF58", "objects/gameplay_keep/gameplay_keep_Anim_029140",
      "objects/object_stk/gSkullKidMajorasMask1DL", "objects/object_stk/gSkullKidNormalHeadDL",
      "objects/object_stk/gSkullKidNormalEyesDL", 21, STATIC_STORY_MM_TRACKING_BODY_YAW, true },
    { "objects/object_stk/gSkullKidSkel", "objects/object_stk2/gSkullKidFloatingArmsCrossedAnim",
      "objects/gameplay_keep/gameplay_keep_Skel_02AF58", "objects/gameplay_keep/gameplay_keep_Anim_029140",
      "objects/object_stk/gSkullKidMajorasMask1DL", "objects/object_stk/gSkullKidNormalHeadDL",
      "objects/object_stk/gSkullKidNormalEyesDL", 21, STATIC_STORY_MM_TRACKING_BODY_YAW, true },
};

/* Counts and clip lengths verified against the unmodified MM donor archive. */
#define ORDINARY(object, skel, anim, limbs, matrices, frames, track, eyes, mouths, eyeSeg, mouthSeg)               \
    {                                                                                                              \
        "objects/" object "/" skel, "objects/" object "/" anim, NULL, NULL, NULL, NULL, NULL, limbs, track, false, \
            STATIC_STORY_MM_NORMAL_FLEX, matrices, frames, eyes, mouths, eyeSeg, mouthSeg                          \
    }
static const StaticStoryMmPresentation sHappyMaskSalesmanPresentations[] = {
    ORDINARY("object_osn", "gHappyMaskSalesmanSkel", "gHappyMaskSalesmanIdleAnim", 18, 17, 29,
             STATIC_STORY_MM_TRACKING_HEAD_TORSO, 1, 1, 8, 9),
    ORDINARY("object_osn", "gHappyMaskSalesmanSkel", "gHappyMaskSalesmanHandsClaspedAnim", 18, 17, 29,
             STATIC_STORY_MM_TRACKING_NONE, 1, 1, 8, 9),
    ORDINARY("object_osn", "gHappyMaskSalesmanSkel", "gHappyMaskSalesmanArmsOutAnim", 18, 17, 29,
             STATIC_STORY_MM_TRACKING_NONE, 1, 1, 8, 9),
};
static const StaticStoryMmPresentation sKeatonPresentations[] = {
    ORDINARY("object_kitan", "gKeatonSkel", "gKeatonIdleAnim", 20, 20, 36, STATIC_STORY_MM_TRACKING_NONE, 0, 0, 0, 0),
    ORDINARY("object_kitan", "gKeatonSkel", "gKeatonChuckleAnim", 20, 20, 36, STATIC_STORY_MM_TRACKING_NONE, 0, 0, 0,
             0),
    ORDINARY("object_kitan", "gKeatonSkel", "gKeatonCelebrateAnim", 20, 20, 30, STATIC_STORY_MM_TRACKING_NONE, 0, 0, 0,
             0),
};
static const StaticStoryMmPresentation sLuluPresentations[] = {
    ORDINARY("object_zov", "gLuluSkel", "gLuluLookDownAnim", 22, 21, 30, STATIC_STORY_MM_TRACKING_HEAD_TORSO, 3, 2, 9,
             8),
    ORDINARY("object_zov", "gLuluSkel", "gLuluLookLeftLoopAnim", 22, 21, 30, STATIC_STORY_MM_TRACKING_NONE, 3, 2, 9, 8),
    ORDINARY("object_zov", "gLuluSkel", "gLuluSingLoopAnim", 22, 21, 72, STATIC_STORY_MM_TRACKING_NONE, 3, 2, 9, 8),
    ORDINARY("object_zov", "gLuluSkel", "gLuluLookAroundAnim", 22, 21, 87, STATIC_STORY_MM_TRACKING_NONE, 3, 2, 9, 8),
};
#undef ORDINARY
static const StaticStoryMmPresentation sAnjuPresentations[] = {
    { "objects/object_an1/gAnju1Skel", "objects/object_an2/gAnju2UmbrellaCryAnim", NULL, NULL, NULL, NULL, NULL, 20,
      STATIC_STORY_MM_TRACKING_NONE, false, STATIC_STORY_MM_NORMAL_FLEX, 19, 43, 1, 1, 8, 9 },
    { "objects/object_an1/gAnju1Skel", "objects/object_an2/gAnju2UmbrellaIdleAnim", NULL, NULL, NULL, NULL, NULL, 20,
      STATIC_STORY_MM_TRACKING_NONE, false, STATIC_STORY_MM_NORMAL_FLEX, 19, 32, 1, 1, 8, 9 },
};
static const StaticStoryMmPresentation sKafeiPresentations[] = {
    { "objects/object_test3/gKafeiSkel", "objects/gameplay_keep/gPlayerAnim_link_normal_wait_free", NULL, NULL, NULL,
      NULL, NULL, 21, STATIC_STORY_MM_TRACKING_NONE, false, STATIC_STORY_MM_SCOPED_PLAYER_LOD, 18, 89, 8, 4, 8, 9 },
    { "objects/object_test3/gKafeiSkel", "objects/gameplay_keep/gPlayerAnim_al_yareyare", NULL, NULL, NULL, NULL, NULL,
      21, STATIC_STORY_MM_TRACKING_NONE, false, STATIC_STORY_MM_SCOPED_PLAYER_LOD, 18, 48, 8, 4, 8, 9 },
};

static const char* sTatlLimbPaths[] = {
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AEF8",
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF04",
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF10",
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF1C",
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF28",
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF34",
};

static const char* sTatlDListPaths[] = {
    "objects/gameplay_keep/gameplay_keep_DL_029990", "objects/gameplay_keep/gameplay_keep_DL_029A58",
    "objects/gameplay_keep/gameplay_keep_DL_029B20", "objects/gameplay_keep/gameplay_keep_DL_029BE8",
    "objects/gameplay_keep/gameplay_keep_DL_029CB0", "objects/gameplay_keep/gameplay_keep_DL_029CF0",
};

/* Keep Skull Kid's geometry and vertex buffers in one archive. The skeleton
 * factory leaves display-list names for the global renderer to resolve, which
 * can otherwise combine mm.o2r geometry with a same-named MM texture-pack
 * resource graph. Indexes match SkullKidLimb; head (17) is drawn separately. */
static const char* sSkullKidLimbDisplayListPaths[] = {
    NULL,
    NULL,
    "objects/object_stk/gSkullKidPelvisDL",
    "objects/object_stk/gSkullKidRightThighDL",
    "objects/object_stk/gSkullKidRightShinDL",
    "objects/object_stk/gSkullKidRightFootDL",
    "objects/object_stk/gSkullKidLeftThighDL",
    "objects/object_stk/gSkullKidLeftShinDL",
    "objects/object_stk/gSkullKidLeftFootDL",
    "objects/object_stk/gSkullKidTorsoDL",
    "objects/object_stk/gSkullKidLeftUpperArmDL",
    "objects/object_stk/gSkullKidLeftForearmDL",
    "objects/object_stk/gSkullKidLeftHandAndFluteDL",
    "objects/object_stk/gSkullKidRightUpperArmDL",
    "objects/object_stk/gSkullKidRightForearmDL",
    "objects/object_stk/gSkullKidRightHandDL",
    "objects/object_stk/gSkullKidNeckDL",
    NULL,
    "objects/object_stk/gSkullKidHatBrimDL",
    "objects/object_stk/gSkullKidHatRingsDL",
    "objects/object_stk/gSkullKidHatNarrowSectionDL",
    "objects/object_stk/gSkullKidHatTopDL",
};

const char* StaticStoryMm_GetSkullKidLimbDisplayListPath(uint8_t limb) {
    return limb < sizeof(sSkullKidLimbDisplayListPaths) / sizeof(sSkullKidLimbDisplayListPaths[0])
               ? sSkullKidLimbDisplayListPaths[limb]
               : NULL;
}

const StaticStoryMmPresentation* StaticStoryMm_GetPresentation(StaticStoryActorType type, uint8_t pose) {
    if (type == STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL && pose < 3) {
        return &sTreasureChestShopGalPresentations[pose];
    }
    if (type == STATIC_STORY_ACTOR_SKULL_KID && pose < 2) {
        return &sSkullKidPresentations[pose];
    }
    if (type == STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN && pose < 3)
        return &sHappyMaskSalesmanPresentations[pose];
    if (type == STATIC_STORY_ACTOR_KEATON && pose < 3)
        return &sKeatonPresentations[pose];
    if (type == STATIC_STORY_ACTOR_LULU && pose < 4)
        return &sLuluPresentations[pose];
    if (type == STATIC_STORY_ACTOR_CHILD_KAFEI && pose < 2)
        return &sKafeiPresentations[pose];
    if (type == STATIC_STORY_ACTOR_ANJU && pose < 2)
        return &sAnjuPresentations[pose];
    return NULL;
}

const char* StaticStoryMm_GetEyeTexturePath(StaticStoryActorType type, uint8_t eyeIndex) {
    if (type == STATIC_STORY_ACTOR_ANJU)
        return eyeIndex == 0 ? "objects/object_an1/gAnju1EyeSadTex" : NULL;
    static const char* kafei[] = {
        "objects/object_test3/gKafeiEyesOpenTex",   "objects/object_test3/gKafeiEyesHalfTex",
        "objects/object_test3/gKafeiEyesClosedTex", "objects/object_test3/gKafeiEyesRightTex",
        "objects/object_test3/gKafeiEyesLeftTex",   "objects/object_test3/gKafeiEyesUpTex",
        "objects/object_test3/gKafeiEyesDownTex",   "objects/object_test3/gKafeiEyesWincingTex"
    };
    if (type == STATIC_STORY_ACTOR_CHILD_KAFEI)
        return eyeIndex < 8 ? kafei[eyeIndex] : NULL;
    static const char* lulu[] = { "objects/object_zov/gLuluEyeOpenTex", "objects/object_zov/gLuluEyeHalfTex",
                                  "objects/object_zov/gLuluEyeClosedTex" };
    if (type == STATIC_STORY_ACTOR_LULU)
        return eyeIndex < 3 ? lulu[eyeIndex] : NULL;
    if (type == STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN)
        return eyeIndex == 0 ? "objects/object_osn/gHappyMaskSalesmanEyeClosedHappyTex" : NULL;
    if (type != STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL || eyeIndex >= 4) {
        return NULL;
    }
    return sTreasureChestShopGalEyeTexturePaths[eyeIndex];
}

float StaticStoryMm_GetHoverOffset(uint16_t phase) {
    return sinf((float)phase * (STATIC_STORY_MM_TAU / 65536.0f)) * STATIC_STORY_MM_HOVER_AMPLITUDE;
}

float StaticStoryMm_ComposeHoverY(float authoredY, uint16_t phase) {
    return authoredY + StaticStoryMm_GetHoverOffset(phase);
}

float StaticStoryMm_GetShapeYOffset(StaticStoryActorType type, uint8_t pose) {
    static const struct {
        bool anchorFeet;
        float lowestFootY;
    } anchors[] = {
        { false, 0.0f },
        /* Full native loop, frame 21: -2550.3546125 model units. The encoded
         * R2 3DS feet reach -2537.3810622, so the native bound covers both.
         * Reclining intentionally retains its original placement. */
        { true, -2550.3547f },
    };
    if (type != STATIC_STORY_ACTOR_SKULL_KID || pose >= sizeof(anchors) / sizeof(anchors[0]) ||
        !anchors[pose].anchorFeet) {
        return 0.0f;
    }
    const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(type);
    /* Shape offsets are model units; include the independent world-space
     * hover trough so feet stay above the authored Y through either loop. */
    return ceilf(fmaxf(0.0f, STATIC_STORY_MM_HOVER_AMPLITUDE / definition->scale - anchors[pose].lowestFootY));
}

int16_t StaticStoryMm_GetModelYawOffset(StaticStoryActorType type, uint8_t pose) {
    /* The look-left clip carries a cutscene body turn in its pelvis, not its
     * root yaw. Its hip axis has mean heading 19310.906 binary-angle units
     * (range 19308.091..19313.727); the other Lulu loops start at zero.
     * A fixed model-space correction keeps the animated head/torso gesture. */
    return type == STATIC_STORY_ACTOR_LULU && pose == 1 ? -19311 : 0;
}

StaticStoryMmVec3f StaticStoryMm_GetTatlAnchor(uint8_t pose) {
    static const StaticStoryMmVec3f sAnchors[] = {
        { -24.0f, 38.0f, -18.0f },
        { 30.0f, 46.0f, 4.0f },
    };

    return sAnchors[pose == 1 ? 1 : 0];
}

uint8_t StaticStoryMm_GetTatlOuterAlpha(uint16_t phase) {
    uint16_t halfPhase = phase & 0x7FFF;
    uint16_t triangle = halfPhase <= 0x4000 ? halfPhase : 0x8000 - halfPhase;

    return (uint8_t)(160 + (triangle * 80U) / 0x4000U);
}

float StaticStoryMm_GetTatlScale(uint16_t phase) {
    return 1.0f + sinf((float)phase * (STATIC_STORY_MM_TAU / 65536.0f)) * 0.08f;
}

const char* StaticStoryMm_GetTatlLimbPath(uint8_t limb) {
    return limb < 6 ? sTatlLimbPaths[limb] : NULL;
}

const char* StaticStoryMm_GetTatlDListPath(uint8_t limb) {
    return limb < 6 ? sTatlDListPaths[limb] : NULL;
}

bool StaticStoryMm_UsesNativeFairyCompanion(StaticStoryActorType type) {
    return type == STATIC_STORY_ACTOR_SKULL_KID;
}

bool StaticStoryMm_ResourcesComplete(const StaticStoryMmPresentation* presentation, bool hasSkeleton, bool hasAnimation,
                                     bool hasSecondarySkeleton) {
    return presentation != NULL && hasSkeleton && hasAnimation &&
           (!presentation->requiresSecondarySkeleton || hasSecondarySkeleton);
}

const char* StaticStoryMm_GetMouthTexturePath(StaticStoryActorType type, uint8_t mouthIndex) {
    if (type == STATIC_STORY_ACTOR_ANJU)
        return mouthIndex == 0 ? "objects/object_an1/gAnju1MouthClosedTex" : NULL;
    static const char* kafei[] = { "objects/object_test3/gKafeiMouthClosedTex",
                                   "objects/object_test3/gKafeiMouthHalfTex", "objects/object_test3/gKafeiMouthOpenTex",
                                   "objects/object_test3/gKafeiMouthSmileTex" };
    if (type == STATIC_STORY_ACTOR_CHILD_KAFEI)
        return mouthIndex < 4 ? kafei[mouthIndex] : NULL;
    static const char* lulu[] = { "objects/object_zov/gLuluMouthClosedTex", "objects/object_zov/gLuluMouthOpenTex" };
    if (type == STATIC_STORY_ACTOR_LULU)
        return mouthIndex < 2 ? lulu[mouthIndex] : NULL;
    if (type == STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN)
        return mouthIndex == 0 ? "objects/object_osn/gHappyMaskSalesmanSmileTex" : NULL;
    return NULL;
}

StaticStoryMmFace StaticStoryMm_ResolveFace(StaticStoryActorType type, uint8_t pose, float frame, uint8_t blinkEye,
                                            bool tracking) {
    StaticStoryMmFace face = { blinkEye < 3 ? blinkEye : 0, 0 };
    /* Native laundry-pool crying holds the sad eyes and closed mouth. */
    if (type == STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN || type == STATIC_STORY_ACTOR_ANJU)
        face.eye = 0;
    if (type == STATIC_STORY_ACTOR_LULU) {
        if (pose == 0 && face.eye == 0 && !tracking)
            face.eye = 1;
        if (pose == 2) {
            face.eye = 0;
            face.mouth = 1;
        }
        if (pose == 3) {
            /* En_Zov selects these four values twice, then resumes ordinary blinking. */
            static const uint8_t sequence[] = { 1, 2, 1, 0 };
            if (frame < 43.0f)
                face.eye = 0;
            else if (frame < 51.0f)
                face.eye = sequence[((unsigned)frame - 43) & 3];
            face.mouth = 1;
        }
    }
    return face;
}

/* Isolated full-loop sampling: the 67th word is appearance, never a joint. */
bool StaticStoryMm_SampleKafei(const int16_t* data, uint16_t frames, float* cursor, float step, void* joints,
                               uint16_t* appearance) {
    if (!data || !cursor || !joints || !appearance || (frames != 89 && frames != 48))
        return false;
    float next = *cursor + step;
    if (!isfinite(next))
        return false;
    next = fmodf(next, (float)frames);
    if (next < 0)
        next += frames;
    /* Float rounding can turn a tiny negative remainder into exactly frameCount. */
    if (next >= frames)
        next = 0;
    const int16_t* frame = data + (unsigned)next * 67;
    memcpy(joints, frame, 66 * sizeof(int16_t));
    *appearance = (uint16_t)frame[66];
    *cursor = next;
    return true;
}
StaticStoryMmFace StaticStoryMm_KafeiFace(uint16_t appearance) {
    int eye = (appearance & 15) - 1;
    int mouth = ((appearance >> 4) & 15) - 1;
    StaticStoryMmFace result = { eye >= 0 && eye < 8 ? eye : 0, mouth >= 0 && mouth < 4 ? mouth : 0 };
    return result;
}
