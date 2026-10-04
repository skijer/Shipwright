#include "static_story_ganon.h"

#include <stddef.h>

#include "z64object.h"

static const StaticStoryGanonPresentation sPhantomGanon = {
    OBJECT_GND,
    OBJECT_GND,
    "__OTR__objects/object_gnd/gPhantomGanonSkel",
    "__OTR__objects/object_gnd/gPhantomGanonNeutralAnim",
    NULL,
    NULL,
    STATIC_STORY_GANON_SEGMENT_NULL_DL,
    false,
    false,
    false,
    false,
    1000.0f,
};

const StaticStoryGanonPresentation* StaticStoryGanon_GetPresentation(StaticStoryActorType type) {
    return type == STATIC_STORY_ACTOR_PHANTOM_GANON ? &sPhantomGanon : NULL;
}
