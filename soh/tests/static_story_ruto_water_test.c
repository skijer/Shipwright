#include <stdbool.h>

#define REQUIRE(condition)  \
    do {                    \
        if (!(condition)) { \
            return 1;       \
        }                   \
    } while (0)
#define assert(condition) REQUIRE(condition)

#include "../src/overlays/actors/ovl_En_Viewer/static_story_ruto_water.h"

static StaticRutoWaterEvents ReachSurface(StaticRutoWaterState* state, bool playerNear) {
    StaticRutoWaterEvents events = STATIC_RUTO_WATER_EVENT_NONE;

    while (state->phase == STATIC_RUTO_PHASE_RISING) {
        events |= StaticRutoWater_Update(state, true, 100.0f, playerNear, false, 60);
    }
    return events;
}

int main(void) {
    StaticRutoWaterState state;
    StaticRutoWaterEvents events;
    float firstTreadY;
    int rippleSeen = 0;
    int fadeFrames = 0;
    StaticRutoWaterLegPose legPose;
    int i;

    /* A missing initial water query must not erase the placement's requested mode. */
    StaticRutoWater_Init(&state, STATIC_RUTO_SURFACE, false, 10.0f, 0.0f);
    assert(state.mode == STATIC_RUTO_SURFACE);
    assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);
    assert(state.currentY == 10.0f);
    assert(state.alpha == 0);
    events = StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_RISING);
    assert(state.velocityY == 4.0f);
    assert(state.alpha == 0);
    assert(events == STATIC_RUTO_WATER_EVENT_NONE);

    /* The persistent surface placement visibly ascends from authored Y once water resolves. */
    events = ReachSurface(&state, false);
    assert(state.phase == STATIC_RUTO_PHASE_SURFACED);
    /* Adult Ruto's taller rig needs a 10-unit lift over En_Zora's 54-unit anchor. */
    assert(state.currentY == 56.0f);
    assert(state.alpha == 255);
    assert((events & STATIC_RUTO_WATER_EVENT_EMERGED) != 0);

    /* Surfaced Ruto treads around the water line and emits periodic ripple requests. */
    firstTreadY = state.currentY;
    for (i = 0; i < 30; ++i) {
        events = StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
        if ((events & STATIC_RUTO_WATER_EVENT_RIPPLE) != 0) {
            rippleSeen = 1;
        }
    }
    assert(state.phase == STATIC_RUTO_PHASE_SURFACED);
    assert(state.currentY != firstTreadY);
    assert(state.treadPhase != 0);
    assert(state.currentY >= 54.5f && state.currentY <= 57.5f);
    assert(rippleSeen);
    assert(StaticRutoWater_CanTrack(&state));
    StaticRutoWater_GetTreadLegPose(0, &legPose);
    /* Adult Ruto's sagittal leg bend is on Z: flex the hips slightly and fold both shins behind her. */
    assert(legPose.leftHip.z <= -0xA00 && legPose.rightHip.z <= -0xA00);
    assert(legPose.leftKnee.z >= 0x1C00 && legPose.rightKnee.z >= 0x1C00);
    assert(legPose.leftHip.x == 0 && legPose.leftHip.y == 0);
    assert(legPose.leftKnee.x == 0 && legPose.leftKnee.y == 0);
    StaticRutoWater_GetTreadLegPose(0x300, &legPose);
    assert(legPose.leftHip.z != legPose.rightHip.z);
    assert(legPose.leftKnee.z != legPose.rightKnee.z);
    assert(StaticRutoWater_ShouldTurnBody(&state));

    /* Dive-loop Ruto stays hidden at authored depth until Link approaches. */
    StaticRutoWater_Init(&state, STATIC_RUTO_DIVE_LOOP, true, 10.0f, 100.0f);
    assert(state.mode == STATIC_RUTO_DIVE_LOOP);
    assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);
    assert(state.currentY == 10.0f);
    StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);
    StaticRutoWater_Update(&state, true, 100.0f, true, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_RISING);
    events = ReachSurface(&state, true);
    assert((events & STATIC_RUTO_WATER_EVENT_EMERGED) != 0);

    /* Leaving starts the supplied 40-79 frame grace period; returning cancels it. */
    StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_SURFACED);
    assert(state.phaseTimer == 60);
    StaticRutoWater_Update(&state, true, 100.0f, true, false, 60);
    assert(state.phaseTimer == 0);
    StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    events = STATIC_RUTO_WATER_EVENT_NONE;
    for (i = 0; i < 60; ++i) {
        events |= StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    }
    assert(state.phase == STATIC_RUTO_PHASE_DIVING);
    assert(state.velocityY == -4.0f);
    assert((events & STATIC_RUTO_WATER_EVENT_DIVE) != 0);
    assert(!StaticRutoWater_CanTrack(&state));
    assert(!StaticRutoWater_ShouldTurnBody(&state));
    /* The reverse swim and descent begin together; fading is visible throughout the downward motion. */
    events = StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_DIVING);
    assert(state.currentY > state.homeY);
    assert(state.velocityY == -4.0f);
    assert(state.alpha < 255 && state.alpha > 0);
    while (state.phase == STATIC_RUTO_PHASE_DIVING && fadeFrames < 100) {
        StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
        fadeFrames++;
    }
    assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);
    /* Once fully invisible, submerge returns the actor to its authored reset point. */
    assert(state.currentY == state.homeY);
    assert(fadeFrames >= 6);

    /* Losing a water box is recoverable and never changes the selected water mode. */
    StaticRutoWater_Update(&state, false, 0.0f, false, false, 60);
    assert(state.mode == STATIC_RUTO_DIVE_LOOP);
    assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);
    StaticRutoWater_Update(&state, true, 100.0f, true, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_RISING);

    StaticRutoWater_Init(&state, STATIC_RUTO_GROUNDED, true, 15.0f, 100.0f);
    StaticRutoWater_Update(&state, true, 120.0f, true, false, 60);
    assert(state.mode == STATIC_RUTO_GROUNDED);
    assert(state.phase == STATIC_RUTO_PHASE_GROUNDED);
    assert(state.currentY == 15.0f);

    return 0;
}
