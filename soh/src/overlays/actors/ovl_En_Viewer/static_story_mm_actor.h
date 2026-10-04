#ifndef STATIC_STORY_MM_ACTOR_H
#define STATIC_STORY_MM_ACTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "static_story_actor.h"

typedef enum {
    STATIC_STORY_MM_TRACKING_NONE,
    STATIC_STORY_MM_TRACKING_HEAD_TORSO,
    STATIC_STORY_MM_TRACKING_BODY_YAW,
} StaticStoryMmTracking;

typedef struct {
    float x;
    float y;
    float z;
} StaticStoryMmVec3f;

typedef enum {
    STATIC_STORY_MM_NORMAL_FLEX = 0,
    STATIC_STORY_MM_SCOPED_PLAYER_LOD = 1,
} StaticStoryMmPresentationKind;

typedef struct {
    uint8_t eye;
    uint8_t mouth;
} StaticStoryMmFace;

typedef struct {
    const char* skeletonPath;
    const char* animationPath;
    const char* secondarySkeletonPath;
    const char* secondaryAnimationPath;
    const char* maskDisplayListPath;
    const char* headDisplayListPath;
    const char* eyesDisplayListPath;
    uint8_t limbCount;
    StaticStoryMmTracking tracking;
    bool requiresSecondarySkeleton;
    /* Zero metadata preserves legacy validation. Ordinary rows use an exact contract. */
    StaticStoryMmPresentationKind kind;
    uint8_t matrixCount;
    uint16_t frameCount;
    uint8_t eyeCount;
    uint8_t mouthCount;
    uint8_t eyeSegment;
    uint8_t mouthSegment;
} StaticStoryMmPresentation;

const StaticStoryMmPresentation* StaticStoryMm_GetPresentation(StaticStoryActorType type, uint8_t pose);
const char* StaticStoryMm_GetEyeTexturePath(StaticStoryActorType type, uint8_t eyeIndex);
const char* StaticStoryMm_GetMouthTexturePath(StaticStoryActorType type, uint8_t mouthIndex);
StaticStoryMmFace StaticStoryMm_ResolveFace(StaticStoryActorType type, uint8_t pose, float frame,
                                           uint8_t blinkEye, bool tracking);
bool StaticStoryMm_ResourcesComplete(const StaticStoryMmPresentation* presentation, bool hasSkeleton,
                                     bool hasAnimation, bool hasSecondarySkeleton);
float StaticStoryMm_GetHoverOffset(uint16_t phase);
float StaticStoryMm_ComposeHoverY(float authoredY, uint16_t phase);
float StaticStoryMm_GetShapeYOffset(StaticStoryActorType type, uint8_t pose);
int16_t StaticStoryMm_GetModelYawOffset(StaticStoryActorType type, uint8_t pose);
StaticStoryMmVec3f StaticStoryMm_GetTatlAnchor(uint8_t pose);
uint8_t StaticStoryMm_GetTatlOuterAlpha(uint16_t phase);
float StaticStoryMm_GetTatlScale(uint16_t phase);
const char* StaticStoryMm_GetTatlLimbPath(uint8_t limb);
const char* StaticStoryMm_GetTatlDListPath(uint8_t limb);
const char* StaticStoryMm_GetSkullKidLimbDisplayListPath(uint8_t limb);
bool StaticStoryMm_UsesNativeFairyCompanion(StaticStoryActorType type);

bool StaticStoryMm_SampleKafei(const int16_t* data, uint16_t frames, float* cursor, float step,
                             void* joints, uint16_t* appearance);
StaticStoryMmFace StaticStoryMm_KafeiFace(uint16_t appearance);

#endif
