// Scissors Beetle from The Minish Cap: it throws its pincers like boomerangs.
// Model @syeo501, code @trueffel; ported from NEI as a custom enemy the enemy randomizer can pick.

#include <math.h>

#include "soh/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#include "object_sbeetle_assets.h"
#include "object_sbeetle_assets.inc.c"

#define SBEETLE_KEY "trutefel.enemy.sbeetle"

// Unbound leaves the bgCheckFlags bits unnamed.
#define BGCHECK_WATER (1 << 5)
#define BGCHECK_UPDATE_ALL ((1 << 0) | (1 << 2) | (1 << 3) | (1 << 4))

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000

#define SBEETLE_PINCER_THROW_FRAME 12.0f
#define SBEETLE_PINCER_OUT_TIME 18
#define SBEETLE_PINCER_RETURN_TIME 20
#define SBEETLE_PINCER_FAST_RETURN_TIME 6
#define SBEETLE_PINCER_CURVE 55.0f
#define SBEETLE_PINCER_ARC_HEIGHT 25.0f
#define SBEETLE_PINCER_SPIN_SPEED 0x2800

#define SBEETLE_FORGET_DISTANCE 460.0f
#define SBEETLE_FORGET_HEIGHT 140.0f
#define SBEETLE_FORGET_TIME 60
#define SBEETLE_HEARING_DISTANCE 200.0f
#define SBEETLE_FRONT_DISTANCE 460.0f
#define SBEETLE_FRONT_ANGLE 0x2000
#define SBEETLE_SIDE_DISTANCE 300.0f
#define SBEETLE_SIDE_ANGLE 0x5000
#define SBEETLE_DETECT_HEIGHT 80.0f
#define SBEETLE_WANDER_RADIUS 300.0f
#define SBEETLE_CHARGE_DISTANCE 300.0f

typedef enum {
    SBEETLE_PINCER_ATTACHED,
    SBEETLE_PINCER_WINDUP,
    SBEETLE_PINCER_OUTBOUND,
    SBEETLE_PINCER_RETURN,
    SBEETLE_PINCER_FAST_RETURN,
} EnSbeetlePincerState;

typedef enum {
    SBEETLE_ANIMATION_IDLE1,
    SBEETLE_ANIMATION_IDLE2,
    SBEETLE_ANIMATION_IDLE3,
    SBEETLE_ANIMATION_WALK,
    SBEETLE_ANIMATION_HOP,
    SBEETLE_ANIMATION_ATTACK,
    SBEETLE_ANIMATION_SWING,
    SBEETLE_ANIMATION_HURT,
    SBEETLE_ANIMATION_DIE,
} EnSbeetleAnimation;

typedef enum {
    SBEETLE_DMGEFF_NONE,
    SBEETLE_DMGEFF_STUN,
    SBEETLE_DMGEFF_ICE_MAGIC = 6,
    SBEETLE_DMGEFF_LIGHT_MAGIC = 13,
    SBEETLE_DMGEFF_FIRE,
} EnSbeetleDamageEffect;

struct EnSbeetle;

typedef void (*EnSbeetleActionFunc)(struct EnSbeetle*, PlayState*);

typedef struct {
    Vec3f worldPos;
    Vec3f homePos;
    Vec3f returnStart;
    s16 spin;
} EnSbeetlePincer;

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
    EnSbeetlePincer pincerL;
    EnSbeetlePincer pincerR;
    Vec3f pincerTargetPos;
    s16 pincerState;
    s16 pincerFlightTimer;
} EnSbeetle;

static const SOHModApi* sApi;

static void EnSbeetle_SetupDoNothing(EnSbeetle* this, PlayState* play);
static void EnSbeetle_SetupThreatPlayer(EnSbeetle* this, PlayState* play);
static void EnSbeetle_ThreatPlayer(EnSbeetle* this, PlayState* play);
static void EnSbeetle_SetupHopAwayFromOrTowardsPlayer(EnSbeetle* this, PlayState* play);
static void EnSbeetle_SetupStunned(EnSbeetle* this, PlayState* play);
static void EnSbeetle_SetupHurt(EnSbeetle* this, PlayState* play);
static void EnSbeetle_SetupDie(EnSbeetle* this, PlayState* play);
static void EnSbeetle_Die(EnSbeetle* this, PlayState* play);

static ColliderCylinderInit sBodyCylinderInit = {
    {
        COLTYPE_HARD,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK1,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON | BUMP_HOOKABLE,
        OCELEM_ON,
    },
    { 40, 45, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sPincerLCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { DMG_SLASH, 0x00, 0x8 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 35, 30, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sPincerRCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x20000000, 0x00, 0x8 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 35, 30, 0, { 0, 0, 0 } },
};

static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, SBEETLE_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, SBEETLE_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, SBEETLE_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, SBEETLE_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(0, SBEETLE_DMGEFF_STUN),
    /* Normal arrow  */ DMG_ENTRY(1, SBEETLE_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, SBEETLE_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, SBEETLE_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, SBEETLE_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, SBEETLE_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, SBEETLE_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(3, SBEETLE_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(2, SBEETLE_DMGEFF_ICE_MAGIC),
    /* Light arrow   */ DMG_ENTRY(4, SBEETLE_DMGEFF_NONE),
    /* Unk arrow 1   */ DMG_ENTRY(2, SBEETLE_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, SBEETLE_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, SBEETLE_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(3, SBEETLE_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(2, SBEETLE_DMGEFF_ICE_MAGIC),
    /* Light magic   */ DMG_ENTRY(4, SBEETLE_DMGEFF_LIGHT_MAGIC),
    /* Shield        */ DMG_ENTRY(0, SBEETLE_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, SBEETLE_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(2, SBEETLE_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(4, SBEETLE_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(3, SBEETLE_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, SBEETLE_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, SBEETLE_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, SBEETLE_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, SBEETLE_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, SBEETLE_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, SBEETLE_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, SBEETLE_DMGEFF_NONE),
};

static CollisionCheckInfoInit2 sColChkInit = { 5, 25, 35, 0, MASS_HEAVY };

static AnimationInfo sAnimationInfo[] = {
    { &gScissorsBeetleSkelIdle1Anim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, 3.0f },
    { &gScissorsBeetleSkelIdle2Anim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelIdle3Anim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelWalkAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, 3.0f },
    { &gScissorsBeetleSkelHopAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelAttackAnim, 1.5f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelSwingAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelHurtAnim, 1.5f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelDieAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
};

// Offsets around the body for the ice shards and flames of an elemental hit, one every four frames.
static Vec3f sBodyEffectOffsets[12] = {
    { 20.0f, 20.0f, 0.0f },   { 10.0f, 40.0f, 10.0f },   { -10.0f, 40.0f, 10.0f }, { -20.0f, 20.0f, 0.0f },
    { 10.0f, 40.0f, -10.0f }, { -10.0f, 40.0f, -10.0f }, { 0.0f, 20.0f, -20.0f },  { 10.0f, 0.0f, 10.0f },
    { 10.0f, 0.0f, -10.0f },  { 0.0f, 20.0f, 20.0f },    { -10.0f, 0.0f, 10.0f },  { -10.0f, 0.0f, -10.0f },
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static void EnSbeetle_ChangeAnimation(EnSbeetle* this, s32 index) {
    Animation_ChangeByInfo(&this->skelAnime, sAnimationInfo, index);
}

static bool EnSbeetle_ArePincersFlying(EnSbeetle* this) {
    return this->pincerState == SBEETLE_PINCER_OUTBOUND || this->pincerState == SBEETLE_PINCER_RETURN ||
           this->pincerState == SBEETLE_PINCER_FAST_RETURN;
}

static void EnSbeetle_FacePlayer(EnSbeetle* this, s16 step, s16 shapeStep) {
    Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, step);
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, shapeStep);
}

// Converts a world position into the space of the current limb matrix, so a flying pincer can be drawn on its bone.
static void EnSbeetle_WorldToCurrentMatrixLocal(Vec3f* worldPos, Vec3f* localPos) {
    MtxF mtx;

    Matrix_Get(&mtx);
    Vec3f delta = { worldPos->x - mtx.xw, worldPos->y - mtx.yw, worldPos->z - mtx.zw };
    f32 scaleSqX = SQ(mtx.xx) + SQ(mtx.yx) + SQ(mtx.zx);
    f32 scaleSqY = SQ(mtx.xy) + SQ(mtx.yy) + SQ(mtx.zy);
    f32 scaleSqZ = SQ(mtx.xz) + SQ(mtx.yz) + SQ(mtx.zz);

    localPos->x = scaleSqX > 0.000001f ? (delta.x * mtx.xx + delta.y * mtx.yx + delta.z * mtx.zx) / scaleSqX : 0.0f;
    localPos->y = scaleSqY > 0.000001f ? (delta.x * mtx.xy + delta.y * mtx.yy + delta.z * mtx.zy) / scaleSqY : 0.0f;
    localPos->z = scaleSqZ > 0.000001f ? (delta.x * mtx.xz + delta.y * mtx.yz + delta.z * mtx.zz) / scaleSqZ : 0.0f;
}

// A boomerang arc: straight interpolation, bowed sideways and upwards by a half sine over the flight.
static void EnSbeetle_GetPincerPath(Vec3f* start, Vec3f* end, f32 progress, f32 side, Vec3f* result) {
    f32 dx = end->x - start->x;
    f32 dz = end->z - start->z;
    f32 length = sqrtf(SQ(dx) + SQ(dz));
    f32 perpendicularX = length > 0.001f ? -dz / length : 0.0f;
    f32 perpendicularZ = length > 0.001f ? dx / length : 0.0f;
    f32 curve = Math_SinS((s16)(progress * 0x7FFF));

    result->x = start->x + (end->x - start->x) * progress + perpendicularX * curve * SBEETLE_PINCER_CURVE * side;
    result->y = start->y + (end->y - start->y) * progress + curve * SBEETLE_PINCER_ARC_HEIGHT;
    result->z = start->z + (end->z - start->z) * progress + perpendicularZ * curve * SBEETLE_PINCER_CURVE * side;
}

static void EnSbeetle_ClearPincerHits(EnSbeetle* this) {
    this->pincerLCollider.base.atFlags &= ~AT_HIT;
    this->pincerRCollider.base.atFlags &= ~AT_HIT;
}

static void EnSbeetle_ResetPincerFlight(EnSbeetle* this) {
    this->pincerFlightTimer = 0;
    this->pincerL.spin = 0;
    this->pincerR.spin = 0;
}

static void EnSbeetle_StartPincerReturn(EnSbeetle* this, s16 state) {
    this->pincerL.returnStart = this->pincerL.worldPos;
    this->pincerR.returnStart = this->pincerR.worldPos;
    this->pincerState = state;
    this->pincerFlightTimer = 0;
}

// Getting hit while the pincers are out calls them straight back and cancels the attack.
static void EnSbeetle_StartPincerFastReturn(EnSbeetle* this) {
    if (this->pincerState != SBEETLE_PINCER_OUTBOUND && this->pincerState != SBEETLE_PINCER_RETURN) {
        return;
    }
    EnSbeetle_StartPincerReturn(this, SBEETLE_PINCER_FAST_RETURN);
    EnSbeetle_ClearPincerHits(this);
}

static f32 EnSbeetle_GetFlightProgress(EnSbeetle* this, s16 duration) {
    return CLAMP_MAX((f32)this->pincerFlightTimer / (f32)duration, 1.0f);
}

static void EnSbeetle_SpinPincers(EnSbeetle* this, s16 speed) {
    this->pincerL.spin += speed;
    this->pincerR.spin -= speed;
}

static void EnSbeetle_MovePincers(EnSbeetle* this, bool isOutbound, f32 progress, f32 side) {
    Vec3f* leftEnd = isOutbound ? &this->pincerTargetPos : &this->pincerL.homePos;
    Vec3f* rightEnd = isOutbound ? &this->pincerTargetPos : &this->pincerR.homePos;
    Vec3f* leftStart = isOutbound ? &this->pincerL.homePos : &this->pincerL.returnStart;
    Vec3f* rightStart = isOutbound ? &this->pincerR.homePos : &this->pincerR.returnStart;

    EnSbeetle_GetPincerPath(leftStart, leftEnd, progress, -side, &this->pincerL.worldPos);
    EnSbeetle_GetPincerPath(rightStart, rightEnd, progress, side, &this->pincerR.worldPos);
}

static void EnSbeetle_LandPincers(EnSbeetle* this) {
    this->pincerState = SBEETLE_PINCER_ATTACHED;
    EnSbeetle_ResetPincerFlight(this);
}

static void EnSbeetle_UpdatePincers(EnSbeetle* this, PlayState* play) {
    switch (this->pincerState) {
        case SBEETLE_PINCER_OUTBOUND:
            EnSbeetle_SpinPincers(this, SBEETLE_PINCER_SPIN_SPEED);
            if ((this->pincerLCollider.base.atFlags & AT_HIT) || (this->pincerRCollider.base.atFlags & AT_HIT)) {
                EnSbeetle_ClearPincerHits(this);
                EnSbeetle_StartPincerReturn(this, SBEETLE_PINCER_RETURN);
                break;
            }
            EnSbeetle_MovePincers(this, true, EnSbeetle_GetFlightProgress(this, SBEETLE_PINCER_OUT_TIME), 1.0f);
            if (++this->pincerFlightTimer >= SBEETLE_PINCER_OUT_TIME) {
                EnSbeetle_StartPincerReturn(this, SBEETLE_PINCER_RETURN);
            }
            break;
        case SBEETLE_PINCER_RETURN:
            EnSbeetle_SpinPincers(this, SBEETLE_PINCER_SPIN_SPEED);
            EnSbeetle_MovePincers(this, false, EnSbeetle_GetFlightProgress(this, SBEETLE_PINCER_RETURN_TIME), -1.0f);
            if (++this->pincerFlightTimer >= SBEETLE_PINCER_RETURN_TIME) {
                EnSbeetle_LandPincers(this);
            }
            break;
        case SBEETLE_PINCER_FAST_RETURN:
            EnSbeetle_SpinPincers(this, SBEETLE_PINCER_SPIN_SPEED * 2);
            EnSbeetle_MovePincers(this, false, EnSbeetle_GetFlightProgress(this, SBEETLE_PINCER_FAST_RETURN_TIME),
                                  -0.2f);
            if (++this->pincerFlightTimer >= SBEETLE_PINCER_FAST_RETURN_TIME) {
                EnSbeetle_LandPincers(this);
                EnSbeetle_ClearPincerHits(this);
            }
            break;
        default:
            break;
    }
}

static void EnSbeetle_UpdateBgCheck(EnSbeetle* this, PlayState* play) {
    Actor_UpdateBgCheckInfo(play, &this->actor, this->actor.colChkInfo.cylHeight, this->actor.colChkInfo.cylRadius,
                            this->actor.colChkInfo.cylHeight, BGCHECK_UPDATE_ALL);
}

static bool EnSbeetle_HasLostPlayer(EnSbeetle* this, PlayState* play) {
    f32 yDist = GET_PLAYER(play)->actor.world.pos.y - this->actor.world.pos.y;

    return this->actor.xzDistToPlayer > SBEETLE_FORGET_DISTANCE || fabsf(yDist) > SBEETLE_FORGET_HEIGHT;
}

// It hears the player up close, and otherwise sees a narrow cone far ahead and a wider one nearer.
static bool EnSbeetle_CanSensePlayer(EnSbeetle* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 xzDist = this->actor.xzDistToPlayer;

    if (xzDist > SBEETLE_FRONT_DISTANCE ||
        fabsf(player->actor.world.pos.y - this->actor.world.pos.y) > SBEETLE_DETECT_HEIGHT) {
        return false;
    }
    if (xzDist <= SBEETLE_HEARING_DISTANCE) {
        return true;
    }
    s16 yawDiff = Math_Vec3f_Yaw(&this->actor.world.pos, &player->actor.world.pos) - this->actor.shape.rot.y;
    s16 absYawDiff = ABS(yawDiff);

    return absYawDiff <= SBEETLE_FRONT_ANGLE || (absYawDiff <= SBEETLE_SIDE_ANGLE && xzDist <= SBEETLE_SIDE_DISTANCE);
}

static void EnSbeetle_ReturnToIdle(EnSbeetle* this) {
    EnSbeetle_ChangeAnimation(this, SBEETLE_ANIMATION_IDLE1);
    this->idleAction = NULL;
}

static void EnSbeetle_IdleActionWalk(EnSbeetle* this, PlayState* play) {
    f32 distToHome = Math_Vec3f_DistXZ(&this->actor.world.pos, &this->actor.home.pos);

    SkelAnime_Update(&this->skelAnime);
    if (distToHome > SBEETLE_WANDER_RADIUS) {
        Math_ApproachS(&this->actor.world.rot.y, Math_Vec3f_Yaw(&this->actor.world.pos, &this->actor.home.pos), 3,
                       4000);
    } else {
        Math_ApproachS(&this->actor.world.rot.y, Rand_S16Offset(this->actor.world.rot.y, 0x400), 3, 4000);
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 6000);

    if (Animation_OnFrame(&this->skelAnime, 10.0f) || Animation_OnFrame(&this->skelAnime, 17.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_TEKU_WALK);
    }
    if (DECR(this->randomWalkTimer) == 0) {
        this->actor.speedXZ = 0.0f;
        EnSbeetle_ReturnToIdle(this);
    }
}

static void EnSbeetle_FinishIdleAnimation(EnSbeetle* this) {
    if (SkelAnime_Update(&this->skelAnime) && DECR(this->afterAnimTimer) == 0) {
        EnSbeetle_ReturnToIdle(this);
    }
}

static void EnSbeetle_IdleActionRattle(EnSbeetle* this, PlayState* play) {
    if (Animation_OnFrame(&this->skelAnime, 7.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_TUBOOCK_FLY);
    }
    // This sound loops until stopped by id.
    if (Animation_OnFrame(&this->skelAnime, 30.0f)) {
        Audio_StopSfxById(NA_SE_EN_TUBOOCK_FLY);
    }
    EnSbeetle_FinishIdleAnimation(this);
}

static void EnSbeetle_IdleActionLookAround(EnSbeetle* this, PlayState* play) {
    if (Animation_OnFrame(&this->skelAnime, 7.0f) || Animation_OnFrame(&this->skelAnime, 39.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_TEKU_WALK);
    }
    EnSbeetle_FinishIdleAnimation(this);
}

static void EnSbeetle_PickIdleAction(EnSbeetle* this) {
    f32 roll = Rand_ZeroOne();

    if (roll <= 0.3f) {
        this->afterAnimTimer = 10;
        EnSbeetle_ChangeAnimation(this, SBEETLE_ANIMATION_IDLE2);
        this->idleAction = EnSbeetle_IdleActionRattle;
    } else if (roll >= 0.4f && roll <= 0.7f) {
        this->afterAnimTimer = 10;
        EnSbeetle_ChangeAnimation(this, SBEETLE_ANIMATION_IDLE3);
        this->idleAction = EnSbeetle_IdleActionLookAround;
    } else {
        this->actor.speedXZ = 1.0f;
        this->randomWalkTimer = Rand_S16Offset(60, 40);
        EnSbeetle_ChangeAnimation(this, SBEETLE_ANIMATION_WALK);
        this->idleAction = EnSbeetle_IdleActionWalk;
    }
}

static void EnSbeetle_DoNothing(EnSbeetle* this, PlayState* play) {
    if (EnSbeetle_CanSensePlayer(this, play)) {
        this->idleAction = NULL;
        EnSbeetle_SetupThreatPlayer(this, play);
        return;
    }
    if (this->idleAction != NULL) {
        this->idleAction(this, play);
        return;
    }
    SkelAnime_Update(&this->skelAnime);
    if (DECR(this->nextIdleTimer) != 0) {
        return;
    }
    this->nextIdleTimer = Rand_S16Offset(40, 40);
    if (Rand_ZeroOne() > 0.4f) {
        EnSbeetle_PickIdleAction(this);
    }
}

static void EnSbeetle_SetupDoNothing(EnSbeetle* this, PlayState* play) {
    this->actor.speedXZ = 0.0f;
    this->nextIdleTimer = 100;
    EnSbeetle_ChangeAnimation(this, SBEETLE_ANIMATION_IDLE1);
    this->actionFunc = EnSbeetle_DoNothing;
}

static void EnSbeetle_Attack(EnSbeetle* this, PlayState* play) {
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->pincerLCollider.base);

    if (this->skelAnime.curFrame < 20.0f) {
        EnSbeetle_FacePlayer(this, 3000, 5000);
    }
    // The lunge: two frames of a dash that covers half the gap to the player.
    if (Animation_OnFrame(&this->skelAnime, 24.0f) || Animation_OnFrame(&this->skelAnime, 25.0f)) {
        this->actor.speedXZ = this->actor.xzDistToPlayer / 2;
        if (!this->audioPlayed) {
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
            this->audioPlayed = true;
        }
    } else {
        this->actor.speedXZ = 0.0f;
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnSbeetle_SetupHopAwayFromOrTowardsPlayer(this, play);
    }
}

static void EnSbeetle_SetupAttack(EnSbeetle* this, PlayState* play) {
    EnSbeetle_ChangeAnimation(this, SBEETLE_ANIMATION_ATTACK);
    this->actor.speedXZ = 0.0f;
    this->pincerLCollider.info.toucher.damage = 0x10;
    this->audioPlayed = false;
    this->actionFunc = EnSbeetle_Attack;
}

static void EnSbeetle_ThrowPincers(EnSbeetle* this, PlayState* play) {
    this->pincerL.worldPos = this->pincerL.homePos;
    this->pincerR.worldPos = this->pincerR.homePos;
    this->pincerTargetPos = GET_PLAYER(play)->actor.world.pos;
    this->pincerTargetPos.y += 30.0f;
    EnSbeetle_ResetPincerFlight(this);
    this->pincerState = SBEETLE_PINCER_OUTBOUND;
    EnSbeetle_ClearPincerHits(this);
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_BOOMERANG_THROW);
}

static void EnSbeetle_SwingAttack(EnSbeetle* this, PlayState* play) {
    this->actor.speedXZ = 0.0f;
    if (this->pincerState == SBEETLE_PINCER_WINDUP) {
        EnSbeetle_FacePlayer(this, 3000, 5000);
    }
    bool isAnimationDone = SkelAnime_Update(&this->skelAnime);

    if (this->pincerState == SBEETLE_PINCER_WINDUP && Animation_OnFrame(&this->skelAnime, SBEETLE_PINCER_THROW_FRAME)) {
        EnSbeetle_ThrowPincers(this, play);
    }
    if (isAnimationDone && this->pincerState == SBEETLE_PINCER_ATTACHED) {
        EnSbeetle_SetupHopAwayFromOrTowardsPlayer(this, play);
    }
}

static void EnSbeetle_SetupSwingAttack(EnSbeetle* this, PlayState* play) {
    EnSbeetle_ChangeAnimation(this, SBEETLE_ANIMATION_SWING);
    this->actor.speedXZ = 0.0f;
    this->pincerLCollider.info.toucher.damage = 0x08;
    this->pincerState = SBEETLE_PINCER_WINDUP;
    EnSbeetle_ResetPincerFlight(this);
    EnSbeetle_ClearPincerHits(this);
    this->actionFunc = EnSbeetle_SwingAttack;
}

static void EnSbeetle_ThreatPlayer(EnSbeetle* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    if (!EnSbeetle_HasLostPlayer(this, play)) {
        this->playerLostTimer = SBEETLE_FORGET_TIME;
    } else if (DECR(this->playerLostTimer) == 0) {
        EnSbeetle_SetupDoNothing(this, play);
        return;
    }
    EnSbeetle_FacePlayer(this, 3000, 5000);

    if (DECR(this->attackTimer) != 0) {
        return;
    }
    if (Rand_ZeroOne() >= 0.6f) {
        this->attackTimer = 40;
    } else if (Rand_ZeroOne() < 0.6f) {
        EnSbeetle_SetupAttack(this, play);
    } else {
        EnSbeetle_SetupSwingAttack(this, play);
    }
}

static void EnSbeetle_SetupThreatPlayer(EnSbeetle* this, PlayState* play) {
    EnSbeetle_ChangeAnimation(this, SBEETLE_ANIMATION_IDLE1);
    this->attackTimer = 40;
    this->playerLostTimer = SBEETLE_FORGET_TIME;
    this->actionFunc = EnSbeetle_ThreatPlayer;
}

// Jumps at a far player and away from a near one; it turns on the ground before and after the jump.
static void EnSbeetle_HopAwayFromOrTowardsPlayer(EnSbeetle* this, PlayState* play) {
    if (this->skelAnime.curFrame <= 6.0f || this->skelAnime.curFrame >= 15.0f) {
        this->actor.speedXZ = 0.0f;
        EnSbeetle_FacePlayer(this, 4000, 6000);
    } else {
        if (!this->audioPlayed) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_RIZA_JUMP);
            this->audioPlayed = true;
        }
        this->actor.speedXZ = this->playerDistAtSetup > SBEETLE_CHARGE_DISTANCE ? 12.0f : -12.0f;
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (EnSbeetle_HasLostPlayer(this, play)) {
        EnSbeetle_SetupDoNothing(this, play);
    } else {
        EnSbeetle_SetupThreatPlayer(this, play);
    }
}

static void EnSbeetle_SetupHopAwayFromOrTowardsPlayer(EnSbeetle* this, PlayState* play) {
    EnSbeetle_ChangeAnimation(this, SBEETLE_ANIMATION_HOP);
    this->audioPlayed = false;
    this->playerDistAtSetup = this->actor.xzDistToPlayer;
    this->actionFunc = EnSbeetle_HopAwayFromOrTowardsPlayer;
}

static void EnSbeetle_Stunned(EnSbeetle* this, PlayState* play) {
    if (this->spawnIceTimer != 0 || this->actor.colorFilterTimer != 0) {
        return;
    }
    EnSbeetle_SetupHopAwayFromOrTowardsPlayer(this, play);
    if (this->frozen) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_BROKEN);
        this->frozen = false;
    }
}

static void EnSbeetle_SetupStunned(EnSbeetle* this, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(this);
    this->actor.speedXZ = 0.0f;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Animation_PlayOnceSetSpeed(&this->skelAnime, &gScissorsBeetleSkelIdle1Anim, 0.0f);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 120, COLORFILTER_OPA, 60);
    this->actionFunc = EnSbeetle_Stunned;
}

static void EnSbeetle_Hurt(EnSbeetle* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime) && DECR(this->damageTimer) == 0) {
        EnSbeetle_SetupHopAwayFromOrTowardsPlayer(this, play);
    }
}

static void EnSbeetle_SetupHurt(EnSbeetle* this, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(this);
    this->damageTimer = 2;
    this->hurtboxCooldown = 40;
    EnSbeetle_ChangeAnimation(this, SBEETLE_ANIMATION_HURT);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    Actor_ApplyDamage(&this->actor);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_BUBLEWALK_AIM);
    this->actionFunc = EnSbeetle_Hurt;
}

static void EnSbeetle_SpawnPuff(EnSbeetle* this, PlayState* play, Vec3f* pos, Color_RGBA8 prim, Color_RGBA8 env) {
    Vec3f velocity = { 0.0f, 4.0f, 0.0f };

    EffectSsDeadDb_Spawn(play, pos, &velocity, &sZeroVec, 90, 0, prim.r, prim.g, prim.b, prim.a, env.r, env.g, env.b, 1,
                         9, true);
}

static void EnSbeetle_Die(EnSbeetle* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime) || DECR(this->deathFreeze) != 0) {
        return;
    }
    Math_StepToF(&this->actor.scale.x, 0.0f, 0.0084f);
    this->actor.scale.y = this->actor.scale.x;
    this->actor.scale.z = this->actor.scale.x;
    if (this->actor.scale.x > 0.001f) {
        return;
    }
    Vec3f puffPos = this->actor.world.pos;

    puffPos.y += 10.0f;
    EnSbeetle_SpawnPuff(this, play, &puffPos, (Color_RGBA8){ 255, 255, 255, 255 }, (Color_RGBA8){ 0, 0, 255, 0 });
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xE0);
    Actor_Kill(&this->actor);
}

static void EnSbeetle_SetupDie(EnSbeetle* this, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(this);
    this->deathFreeze = 12;
    this->actor.speedXZ = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.shape.shadowAlpha = 0;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_BUBLEWALK_DEAD);
    Enemy_StartFinishingBlow(play, &this->actor);
    EnSbeetle_ChangeAnimation(this, SBEETLE_ANIMATION_DIE);
    this->actionFunc = EnSbeetle_Die;
}

static void EnSbeetle_ApplyHit(EnSbeetle* this, PlayState* play) {
    switch (this->actor.colChkInfo.damageEffect) {
        case SBEETLE_DMGEFF_STUN:
            Actor_ApplyDamage(&this->actor);
            EnSbeetle_SetupStunned(this, play);
            break;
        case SBEETLE_DMGEFF_ICE_MAGIC:
            Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 80);
            this->spawnIceTimer = 48;
            this->frozen = true;
            EnSbeetle_SetupStunned(this, play);
            break;
        case SBEETLE_DMGEFF_FIRE:
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_FLAME_OF_FIRE);
            Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 80);
            this->fireTimer = 80;
            EnSbeetle_SetupDie(this, play);
            break;
        default:
            // A hit while it stares the player down bounces off its guard.
            if (this->actionFunc != EnSbeetle_ThreatPlayer) {
                EnSbeetle_SetupHurt(this, play);
            }
            break;
    }
}

static void EnSbeetle_CheckHurt(EnSbeetle* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        Actor_SetDropFlag(&this->actor, &this->collider.info, true);
        this->actor.speedXZ = 0.0f;
        EnSbeetle_ApplyHit(this, play);
        if (this->actor.colChkInfo.health == 0) {
            EnSbeetle_SetupDie(this, play);
        }
    }
    if ((this->actor.bgCheckFlags & BGCHECK_WATER) && this->actionFunc != EnSbeetle_Die) {
        EnSbeetle_SetupDie(this, play);
    }
}

static void EnSbeetle_UpdatePincerColliders(EnSbeetle* this, PlayState* play) {
    if (!EnSbeetle_ArePincersFlying(this)) {
        return;
    }
    Collider_UpdateCylinder(&this->actor, &this->pincerLCollider);
    Collider_UpdateCylinder(&this->actor, &this->pincerRCollider);
    this->pincerLCollider.dim.pos = this->pincerL.worldPos;
    this->pincerRCollider.dim.pos = this->pincerR.worldPos;
    if (this->pincerState != SBEETLE_PINCER_FAST_RETURN) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->pincerLCollider.base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->pincerRCollider.base);
    }
}

static void EnSbeetle_Init(Actor* thisx, PlayState* play) {
    EnSbeetle* this = (EnSbeetle*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 10.0f);
    Actor_SetScale(&this->actor, 0.1f);
    this->actor.gravity = -1.0f;

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sBodyCylinderInit);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, &sDamageTable, &sColChkInit);
    Collider_InitCylinder(play, &this->pincerLCollider);
    Collider_SetCylinder(play, &this->pincerLCollider, &this->actor, &sPincerLCylinderInit);
    Collider_InitCylinder(play, &this->pincerRCollider);
    Collider_SetCylinder(play, &this->pincerRCollider, &this->actor, &sPincerRCylinderInit);

    SkelAnime_InitFlex(play, &this->skelAnime, &gScissorsBeetleSkel, &gScissorsBeetleSkelIdle1Anim, this->jointTable,
                       this->morphTable, GSCISSORSBEETLESKEL_NUM_LIMBS);
    EnSbeetle_SetupDoNothing(this, play);
}

// No SkelAnime_Free: the joint tables live inside the actor, and freeing them would corrupt the arena.
static void EnSbeetle_Destroy(Actor* thisx, PlayState* play) {
    EnSbeetle* this = (EnSbeetle*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    Collider_DestroyCylinder(play, &this->pincerLCollider);
    Collider_DestroyCylinder(play, &this->pincerRCollider);
}

static void EnSbeetle_Update(Actor* thisx, PlayState* play) {
    EnSbeetle* this = (EnSbeetle*)thisx;

    EnSbeetle_CheckHurt(this, play);
    this->actionFunc(this, play);
    EnSbeetle_UpdatePincers(this, play);
    Actor_MoveXZGravity(&this->actor);
    EnSbeetle_UpdateBgCheck(this, play);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    EnSbeetle_UpdatePincerColliders(this, play);

    if (this->actionFunc == EnSbeetle_Die) {
        return;
    }
    if (DECR(this->hurtboxCooldown) == 0) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

// A flying pincer is still drawn by its own limb, moved to where the pincer is in the world.
static void EnSbeetle_DrawPincerLimb(EnSbeetle* this, EnSbeetlePincer* pincer, Vec3f* pos, Vec3s* rot) {
    Vec3f bindPos = *pos;

    Matrix_MultVec3f(&bindPos, &pincer->homePos);
    if (!EnSbeetle_ArePincersFlying(this)) {
        return;
    }
    if (this->pincerState == SBEETLE_PINCER_OUTBOUND && this->pincerFlightTimer <= 1) {
        pincer->worldPos = pincer->homePos;
    }
    EnSbeetle_WorldToCurrentMatrixLocal(&pincer->worldPos, pos);
    rot->y += pincer->spin;
}

static s32 EnSbeetle_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                      void* thisx) {
    EnSbeetle* this = (EnSbeetle*)thisx;

    if (limbIndex == GSCISSORSBEETLESKEL_PINCER_L_LIMB) {
        EnSbeetle_DrawPincerLimb(this, &this->pincerL, pos, rot);
    } else if (limbIndex == GSCISSORSBEETLESKEL_PINCER_R_LIMB) {
        EnSbeetle_DrawPincerLimb(this, &this->pincerR, pos, rot);
    }
    return false;
}

static void EnSbeetle_AttachPincerCollider(EnSbeetle* this, ColliderCylinder* pincer, MtxF* mtx) {
    if (this->pincerState == SBEETLE_PINCER_OUTBOUND || this->pincerState == SBEETLE_PINCER_RETURN) {
        return;
    }
    pincer->dim.pos.x = mtx->xw;
    pincer->dim.pos.y = mtx->yw;
    pincer->dim.pos.z = mtx->zw;
}

static void EnSbeetle_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnSbeetle* this = (EnSbeetle*)thisx;
    MtxF mtx;

    Matrix_Get(&mtx);
    switch (limbIndex) {
        case GSCISSORSBEETLESKEL_BODYFRONT_LIMB:
            Matrix_MultVec3f(&sZeroVec, &this->actor.focus.pos);
            break;
        case GSCISSORSBEETLESKEL_PINCER_L_LIMB:
            EnSbeetle_AttachPincerCollider(this, &this->pincerLCollider, &mtx);
            break;
        case GSCISSORSBEETLESKEL_PINCER_R_LIMB:
            EnSbeetle_AttachPincerCollider(this, &this->pincerRCollider, &mtx);
            break;
        default:
            break;
    }
}

// Both timers also hold the colour filter on while their effects play.
static void EnSbeetle_SpawnElementEffects(EnSbeetle* this, PlayState* play) {
    if (this->spawnIceTimer != 0) {
        this->actor.colorFilterTimer++;
        this->spawnIceTimer--;
        if ((this->spawnIceTimer & 3) == 0) {
            Vec3f shardPos;

            Math_Vec3f_Sum(&this->actor.world.pos, &sBodyEffectOffsets[this->spawnIceTimer >> 2], &shardPos);
            EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &shardPos, 150, 150, 150, 250, 235, 245, 255, 2.0f);
        }
    }
    if (this->fireTimer != 0) {
        this->actor.colorFilterTimer++;
        this->fireTimer--;
        if ((this->fireTimer & 3) == 0) {
            Vec3f flamePos;

            Math_Vec3f_Sum(&this->actor.world.pos, &sBodyEffectOffsets[this->fireTimer >> 2], &flamePos);
            EnSbeetle_SpawnPuff(this, play, &flamePos, (Color_RGBA8){ 200, 135, 50, 255 },
                                (Color_RGBA8){ 200, 80, 50, 0 });
        }
    }
}

static void EnSbeetle_Draw(Actor* thisx, PlayState* play) {
    EnSbeetle* this = (EnSbeetle*)thisx;

    EnSbeetle_SpawnElementEffects(this, play);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->jointTable, this->skelAnime.dListCount,
                          EnSbeetle_OverrideLimbDraw, EnSbeetle_PostLimbDraw, this);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterActor)) {
        return;
    }
    SOHActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = SBEETLE_KEY;
    definition.description = "Scissors Beetle";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                            ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnSbeetle);
    definition.init = EnSbeetle_Init;
    definition.destroy = EnSbeetle_Destroy;
    definition.update = EnSbeetle_Update;
    definition.draw = EnSbeetle_Draw;
    definition.enemyFlags = SOH_ACTOR_ENEMY;
    definition.naviHint = "Scissors Beetle&%cIt attacks by throwing its pincers like boomerangs! Keep moving, then "
                          "strike when they return!%w";

    if (sApi->RegisterActor(&definition) < 0) {
        sApi->Log("Could not register " SBEETLE_KEY);
    }
}
