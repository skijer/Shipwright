#ifndef STATIC_STORY_GANON_H
#define STATIC_STORY_GANON_H

#include <stdbool.h>
#include <stdint.h>

#include "static_story_actor.h"

typedef enum {
    STATIC_STORY_GANON_SEGMENT_NONE,
    STATIC_STORY_GANON_SEGMENT_NULL_DL,
    STATIC_STORY_GANON_SEGMENT_NORMAL_EYE,
} StaticStoryGanonSegment08;

typedef struct {
    int16_t modelObjectId;
    int16_t animationObjectId;
    const char* skeletonPath;
    const char* animationPath;
    const char* eyePath;
    const char* eyesDisplayListPath;
    StaticStoryGanonSegment08 segment08;
    bool tracking;
    bool spawnHorse;
    bool spawnDynamicCape;
    bool usesBossActions;
    float shapeYOffset;
} StaticStoryGanonPresentation;

const StaticStoryGanonPresentation* StaticStoryGanon_GetPresentation(StaticStoryActorType type);

#endif
