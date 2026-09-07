#ifndef Z_EN_MINIBLIN_H
#define Z_EN_MINIBLIN_H

#include "ultra64.h"
#include "global.h"
#include "assets/objects/object_miniblin/object_miniblin.h"

struct EnMiniblin;

typedef void (*EnMiniblinActionFunc)(struct EnMiniblin*, PlayState*);

typedef struct EnMiniblin {
    Actor actor;
    Vec3s jointTable[GMINIBLINSKEL_NUM_LIMBS];
    Vec3s morphTable[GMINIBLINSKEL_NUM_LIMBS];
    SkelAnime skelAnime;
    ColliderCylinder collider;
    ColliderQuad quad;
    EnMiniblinActionFunc actionFunc;
    // Actor* bombActor;
    s16 eyeIndex;
    s16 timer;
    s16 deathTimer;
    s16 damageTimer;
    s16 blinkTimer;
    s16 hurtboxCooldown;
    u8 rupeeStolen;
    u8 aboutToSteal;
} EnMiniblin;

#endif