#ifndef STATIC_STORY_RUTO_WATER_H
#define STATIC_STORY_RUTO_WATER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STATIC_RUTO_GROUNDED = 0,
    STATIC_RUTO_SURFACE = 1,
    STATIC_RUTO_DIVE_LOOP = 2,
} StaticRutoWaterMode;

typedef enum {
    STATIC_RUTO_PHASE_GROUNDED,
    STATIC_RUTO_PHASE_SURFACED,
    STATIC_RUTO_PHASE_PREPARING_DIVE,
    STATIC_RUTO_PHASE_DIVING,
    STATIC_RUTO_PHASE_SUBMERGED,
    STATIC_RUTO_PHASE_RISING,
} StaticRutoWaterPhase;

typedef enum {
    STATIC_RUTO_WATER_EVENT_NONE = 0,
    STATIC_RUTO_WATER_EVENT_EMERGED = 1 << 0,
    STATIC_RUTO_WATER_EVENT_SUBMERGED = 1 << 1,
    STATIC_RUTO_WATER_EVENT_RIPPLE = 1 << 2,
    STATIC_RUTO_WATER_EVENT_DIVE = 1 << 3,
} StaticRutoWaterEvents;

typedef struct {
    StaticRutoWaterMode mode;
    StaticRutoWaterPhase phase;
    float homeY;
    float surfaceY;
    float currentY;
    float velocityY;
    uint16_t phaseTimer;
    uint8_t alpha;
    uint8_t rippleTimer;
    uint16_t treadPhase;
} StaticRutoWaterState;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} StaticRutoWaterLimbRotation;

typedef struct {
    StaticRutoWaterLimbRotation leftHip;
    StaticRutoWaterLimbRotation leftKnee;
    StaticRutoWaterLimbRotation rightHip;
    StaticRutoWaterLimbRotation rightKnee;
} StaticRutoWaterLegPose;

void StaticRutoWater_Init(StaticRutoWaterState* state, StaticRutoWaterMode mode, bool hasWater, float homeY,
                          float surfaceY);
StaticRutoWaterEvents StaticRutoWater_Update(StaticRutoWaterState* state, bool hasWater, float surfaceY,
                                             bool playerNear, bool animationEnded, uint16_t diveDelay);
bool StaticRutoWater_CanTrack(const StaticRutoWaterState* state);
bool StaticRutoWater_ShouldTurnBody(const StaticRutoWaterState* state);
void StaticRutoWater_GetTreadLegPose(int16_t treadOffset, StaticRutoWaterLegPose* pose);

#endif
