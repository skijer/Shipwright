#ifndef Z_EN_MINIBLIN_H
#define Z_EN_MINIBLIN_H

/**
 * z_en_miniblin.h - Miniblin (trueffel/syeo501 custom enemy), SoH port.
 * Struct is 1:1 with the reference (mods/actors/trutefel/reference/z_en_miniblin.h);
 * only the asset include changed (compiled trutefel assets instead of decomp objects).
 * Types (Actor/SkelAnime/Collider*) come from the host TU (trutefel_enemies.cpp
 * includes z64.h before this).
 */

#include "../assets/object_miniblin_assets.h"

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
