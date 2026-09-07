#ifndef Z_EN_HAMMERGEIST_H
#define Z_EN_HAMMERGEIST_H

/**
 * z_en_hammergeist.h - Molmauk / Hammergeist (trueffel/syeo501 custom enemy), SoH port.
 * Struct is 1:1 with the reference (mods/actors/trutefel/reference/z_en_hammergeist.h);
 * only the asset include changed. Types come from the host TU (z64.h included first).
 */

#include "../assets/object_hammergeist_assets.h"

struct EnHammergeist;

typedef void (*EnHammergeistActionFunc)(struct EnHammergeist*, PlayState*);

typedef struct EnHammergeist {
    Actor actor;
    Vec3s firePos[10]; // Fire effect spawn positions (one per burning body part)
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
