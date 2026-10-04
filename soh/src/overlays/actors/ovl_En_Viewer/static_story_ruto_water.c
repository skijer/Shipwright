#include "static_story_ruto_water.h"

/* Adult Ruto's taller rig sits visibly lower than En_Zora at the same 54-unit anchor. */
static const float sSurfaceOffset = 44.0f;
static const float sDiveVelocity = -4.0f;
static const float sRiseVelocity = 4.0f;
static const float sTreadExtent = 1.5f;
static const float sTreadVelocity = 0.3f;
static const float sFadeDepth = 32.0f;

bool StaticRutoWater_CanTrack(const StaticRutoWaterState* state) {
    return state->phase == STATIC_RUTO_PHASE_GROUNDED || state->phase == STATIC_RUTO_PHASE_SURFACED;
}

bool StaticRutoWater_ShouldTurnBody(const StaticRutoWaterState* state) {
    return state->phase == STATIC_RUTO_PHASE_SURFACED;
}

void StaticRutoWater_GetTreadLegPose(int16_t treadOffset, StaticRutoWaterLegPose* pose) {
    pose->leftHip = (StaticRutoWaterLimbRotation){ 0, 0, -0xC00 - treadOffset };
    pose->leftKnee = (StaticRutoWaterLimbRotation){ 0, 0, 0x2000 + treadOffset };
    pose->rightHip = (StaticRutoWaterLimbRotation){ 0, 0, -0xC00 + treadOffset };
    pose->rightKnee = (StaticRutoWaterLimbRotation){ 0, 0, 0x2000 - treadOffset };
}

static uint8_t StaticRutoWater_AlphaForDepth(float currentY, float surfaceTarget) {
    float fadeStart = surfaceTarget - sFadeDepth;
    float alpha;

    if (currentY <= fadeStart) {
        return 0;
    }
    if (currentY >= surfaceTarget) {
        return 255;
    }
    alpha = ((currentY - fadeStart) / sFadeDepth) * 255.0f;
    return (uint8_t)alpha;
}

static void StaticRutoWater_Ground(StaticRutoWaterState* state) {
    state->phase = STATIC_RUTO_PHASE_GROUNDED;
    state->currentY = state->homeY;
    state->velocityY = 0.0f;
    state->phaseTimer = 0;
    state->alpha = 255;
    state->rippleTimer = 0;
    state->treadPhase = 0;
}

static void StaticRutoWater_Submerge(StaticRutoWaterState* state) {
    state->phase = STATIC_RUTO_PHASE_SUBMERGED;
    state->currentY = state->homeY;
    state->velocityY = 0.0f;
    state->phaseTimer = 0;
    state->alpha = 0;
    state->rippleTimer = 0;
    state->treadPhase = 0;
}

static void StaticRutoWater_StartRising(StaticRutoWaterState* state) {
    state->phase = STATIC_RUTO_PHASE_RISING;
    state->velocityY = sRiseVelocity;
    state->phaseTimer = 0;
    state->rippleTimer = 0;
    state->treadPhase = 0;
}

static void StaticRutoWater_Surface(StaticRutoWaterState* state, float surfaceTarget) {
    state->phase = STATIC_RUTO_PHASE_SURFACED;
    state->currentY = surfaceTarget;
    state->velocityY = sTreadVelocity;
    state->phaseTimer = 0;
    state->alpha = 255;
    state->rippleTimer = 0;
}

void StaticRutoWater_Init(StaticRutoWaterState* state, StaticRutoWaterMode mode, bool hasWater, float homeY,
                          float surfaceY) {
    state->mode = mode;
    state->homeY = homeY;
    state->surfaceY = surfaceY;

    if (mode == STATIC_RUTO_GROUNDED) {
        StaticRutoWater_Ground(state);
        return;
    }

    StaticRutoWater_Submerge(state);
    if (hasWater && mode == STATIC_RUTO_SURFACE) {
        StaticRutoWater_StartRising(state);
    }
}

StaticRutoWaterEvents StaticRutoWater_Update(StaticRutoWaterState* state, bool hasWater, float surfaceY,
                                             bool playerNear, bool animationEnded, uint16_t diveDelay) {
    StaticRutoWaterEvents events = STATIC_RUTO_WATER_EVENT_NONE;
    float surfaceTarget;

    (void)animationEnded;

    if (state->mode == STATIC_RUTO_GROUNDED) {
        StaticRutoWater_Ground(state);
        return events;
    }
    if (!hasWater) {
        StaticRutoWater_Submerge(state);
        return events;
    }

    state->surfaceY = surfaceY;
    surfaceTarget = surfaceY - sSurfaceOffset;

    switch (state->phase) {
        case STATIC_RUTO_PHASE_SUBMERGED:
            if (state->mode == STATIC_RUTO_SURFACE || playerNear) {
                StaticRutoWater_StartRising(state);
            }
            break;

        case STATIC_RUTO_PHASE_RISING:
            state->currentY += state->velocityY;
            state->alpha = StaticRutoWater_AlphaForDepth(state->currentY, surfaceTarget);
            if (state->currentY >= surfaceTarget) {
                StaticRutoWater_Surface(state, surfaceTarget);
                events |= STATIC_RUTO_WATER_EVENT_EMERGED;
            }
            break;

        case STATIC_RUTO_PHASE_SURFACED:
            state->currentY += state->velocityY;
            if (state->currentY >= surfaceTarget + sTreadExtent) {
                state->currentY = surfaceTarget + sTreadExtent;
                state->velocityY = -sTreadVelocity;
            } else if (state->currentY <= surfaceTarget - sTreadExtent) {
                state->currentY = surfaceTarget - sTreadExtent;
                state->velocityY = sTreadVelocity;
            }
            state->alpha = 255;
            state->treadPhase += 0x600;
            state->rippleTimer = (state->rippleTimer + 1) % 12;
            if (state->rippleTimer == 3 || state->rippleTimer == 6) {
                events |= STATIC_RUTO_WATER_EVENT_RIPPLE;
            }

            if (state->mode == STATIC_RUTO_DIVE_LOOP) {
                if (playerNear) {
                    state->phaseTimer = 0;
                } else if (state->phaseTimer == 0) {
                    state->phaseTimer = diveDelay;
                } else if (--state->phaseTimer == 0) {
                    state->phase = STATIC_RUTO_PHASE_DIVING;
                    state->velocityY = sDiveVelocity;
                    state->rippleTimer = 0;
                    events |= STATIC_RUTO_WATER_EVENT_DIVE;
                }
            }
            break;

        case STATIC_RUTO_PHASE_PREPARING_DIVE:
            state->velocityY = 0.0f;
            state->alpha = 255;
            if (animationEnded) {
                state->phase = STATIC_RUTO_PHASE_DIVING;
                state->velocityY = sDiveVelocity;
                events |= STATIC_RUTO_WATER_EVENT_DIVE;
            }
            break;

        case STATIC_RUTO_PHASE_DIVING:
            state->currentY += state->velocityY;
            state->alpha = StaticRutoWater_AlphaForDepth(state->currentY, surfaceTarget);
            if (state->alpha == 0) {
                StaticRutoWater_Submerge(state);
                events |= STATIC_RUTO_WATER_EVENT_SUBMERGED;
            }
            break;

        case STATIC_RUTO_PHASE_GROUNDED:
        default:
            StaticRutoWater_Submerge(state);
            break;
    }

    return events;
}
