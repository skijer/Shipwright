#ifndef Z_EN_HAMMERGEIST_H
#define Z_EN_HAMMERGEIST_H

#include "ultra64.h"
#include "global.h"
#include "assets/objects/object_hammergeist/object_hammergeist.h"

struct EnHammergeist;

typedef void (*EnHammergeistActionFunc)(struct EnHammergeist*, PlayState*);

typedef struct EnHammergeist {
    Actor actor;
    Vec3s firePos[10]; // Because of the fire effect spawn function, it's necessary that firePos is exactly at this
                       // offset (0x014C)
    Vec3s jointTable[GHAMMERGEISTSKEL_NUM_LIMBS];
    Vec3s morphTable[GHAMMERGEISTSKEL_NUM_LIMBS];
    Vec3s headRot;
    Vec3s upperBodyRot;
    SkelAnime skelAnime;
    ColliderCylinder collider;
    ColliderCylinder hammerLeftCollider;
    ColliderCylinder hammerRightCollider;
    ColliderJntSph explosionCollider;
    ColliderJntSphElement explosionColliderItems[1];
    s16 faceIndex;
    s16 fireHammerIndex;
    s16 iceHammerIndex;
    s16 hurtboxCooldown;
    s16 explosionTimer;
    s16 infuseTimer;
    s16 slamTimer;
    s16 heavySlamTimer;
    s16 heavySlamCooldown;
    s16 genericAnimationTimer;
    s16 fireTimer;
    s16 alpha;
    u8 explosionRadiusIncrease;
    u8 leftHammerInfused;  // Ice
    u8 rightHammerInfused; // Fire
    u8 playerHit;
    u8 noHitAgain;
    EnHammergeistActionFunc actionFunc;
} EnHammergeist;

#endif