#ifndef Z_EN_SBEETLE_H
#define Z_EN_SBEETLE_H

#include "ultra64.h"
#include "global.h"
#include "assets/objects/object_sbeetle/object_sbeetle.h"

typedef enum {
    ENSBEETLE_PINCER_ATTACHED,
    ENSBEETLE_PINCER_WINDUP,
    ENSBEETLE_PINCER_OUTBOUND,
    ENSBEETLE_PINCER_RETURN,
    ENSBEETLE_PINCER_FAST_RETURN,
} EnSbeetlePincerState;

struct EnSbeetle;

typedef void (*EnSbeetleActionFunc)(struct EnSbeetle*, PlayState*);

typedef struct EnSbeetle {
    Actor actor;
    Vec3s jointTable[GSCISSORSBEETLESKEL_NUM_LIMBS];
    Vec3s morphTable[GSCISSORSBEETLESKEL_NUM_LIMBS];
    SkelAnime skelAnime;
    ColliderCylinder collider;
    ColliderCylinder pincerLCollider;
    ColliderCylinder pincerRCollider;
    EnSbeetleActionFunc actionFunc;
    EnSbeetleActionFunc idleAction;
    f32 playerDistAtSetup;
    s16 nextIdleTimer;
    s16 afterAnimTimer;
    s16 attackTimer;
    s16 hurtboxCooldown;
    s16 damageTimer;
    s16 deathFreeze;
    s16 randomWalkTimer;
    s16 playerLostTimer;
    s16 spawnIceTimer;
    s16 fireTimer;
    u8 frozen;
    u8 audioPlayed;

    Vec3f pincerLWorldPos;
    Vec3f pincerRWorldPos;
    Vec3f pincerLHomePos;
    Vec3f pincerRHomePos;
    Vec3f pincerLReturnStart;
    Vec3f pincerRReturnStart;
    Vec3f pincerTargetPos;
    s16 pincerState;
    s16 pincerFlightTimer;
    s16 pincerLSpin;
    s16 pincerRSpin;

} EnSbeetle;

#endif