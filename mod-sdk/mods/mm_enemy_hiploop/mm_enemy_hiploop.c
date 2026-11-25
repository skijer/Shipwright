// Majora's Mask's Hiploop (En_Pp) as an Unbound custom enemy, drawn from mm.o2r.
// Params 0 masked, 1 bare; 2 is the body a knocked-off mask leaves behind, 7 and up its death fragments.
// Woodfall's straight-line charge (params bit 8) belongs to its platforms and is left out.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define HIPLOOP_KEY "mm.hiploop"
#define HIPLOOP_SOUL_ITEM_KEY "soul.mm.hiploop"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_PP "__OTR__objects/object_pp/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_GRAY 0x8000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_ALL 0x1F

#define MM_SE_EN_HIPLOOP_RUN 0x3979
#define MM_SE_EN_HIPLOOP_PAUSE 0x397A
#define MM_SE_EN_HIPLOOP_MASC_OFF 0x397B
#define MM_SE_EN_HIPLOOP_FOOT 0x397C
#define MM_SE_EN_HIPLOOP_DAMAGE 0x397D
#define MM_SE_EN_HIPLOOP_DEAD 0x397E
#define MM_SE_EN_HIPLOOP_FOOTSTEP 0x397F

#define HIPLOOP_SCALE 0.03f
#define HIPLOOP_ATTACK_RANGE 200.0f
#define HIPLOOP_MASK_FRONT_ARC 0x4A9C
#define HIPLOOP_FRAGMENT_COUNT 6
#define HIPLOOP_BODY_FLAME_COUNT 10

typedef enum {
    HIPLOOP_LIMB_NONE,
    HIPLOOP_LIMB_BODY,
    HIPLOOP_LIMB_FRONT_LEFT_UPPER_LEG,
    HIPLOOP_LIMB_FRONT_LEFT_LOWER_LEG,
    HIPLOOP_LIMB_FRONT_LEFT_FOOT,
    HIPLOOP_LIMB_FRONT_RIGHT_UPPER_LEG,
    HIPLOOP_LIMB_FRONT_RIGHT_LOWER_LEG,
    HIPLOOP_LIMB_FRONT_RIGHT_FOOT,
    HIPLOOP_LIMB_MASK,
    HIPLOOP_LIMB_LEFT_WING_BASE,
    HIPLOOP_LIMB_LEFT_WING_MIDDLE,
    HIPLOOP_LIMB_LEFT_WING_TIP,
    HIPLOOP_LIMB_BACK_RIGHT_UPPER_LEG,
    HIPLOOP_LIMB_BACK_RIGHT_LOWER_LEG,
    HIPLOOP_LIMB_BACK_RIGHT_FOOT,
    HIPLOOP_LIMB_RIGHT_WING_BASE,
    HIPLOOP_LIMB_RIGHT_WING_MIDDLE,
    HIPLOOP_LIMB_RIGHT_WING_TIP,
    HIPLOOP_LIMB_CENTER_WING_BASE,
    HIPLOOP_LIMB_CENTER_WING_MIDDLE,
    HIPLOOP_LIMB_CENTER_WING_TIP,
    HIPLOOP_LIMB_BACK_LEFT_UPPER_LEG,
    HIPLOOP_LIMB_BACK_LEFT_LOWER_LEG,
    HIPLOOP_LIMB_BACK_LEFT_FOOT,
    HIPLOOP_LIMB_RIGHT_EYE,
    HIPLOOP_LIMB_LEFT_EYE,
    HIPLOOP_LIMB_MAX
} HiploopLimb;

typedef enum {
    HIPLOOP_TYPE_MASKED,
    HIPLOOP_TYPE_NO_MASK,
    HIPLOOP_TYPE_UNMASKED,
    HIPLOOP_TYPE_FRAGMENT_BASE = 7,
    HIPLOOP_TYPE_FRAGMENT_BODY = HIPLOOP_LIMB_BODY + HIPLOOP_TYPE_FRAGMENT_BASE
} HiploopType;

typedef enum {
    HIPLOOP_ACTION_IDLE,
    HIPLOOP_ACTION_CHARGE,
    HIPLOOP_ACTION_ATTACK,
    HIPLOOP_ACTION_ROAR,
    HIPLOOP_ACTION_JUMP,
    HIPLOOP_ACTION_STUNNED,
    HIPLOOP_ACTION_MASK_DETACH,
    HIPLOOP_ACTION_DAMAGED,
    HIPLOOP_ACTION_DEAD,
    HIPLOOP_ACTION_MASK_DEAD,
    HIPLOOP_ACTION_FRAGMENT
} HiploopAction;

typedef enum {
    HIPLOOP_ANIM_IDLE,
    HIPLOOP_ANIM_WALK,
    HIPLOOP_ANIM_WIND_UP,
    HIPLOOP_ANIM_CHARGE,
    HIPLOOP_ANIM_ATTACK,
    HIPLOOP_ANIM_DAMAGE,
    HIPLOOP_ANIM_ROAR,
    HIPLOOP_ANIM_TURN_TO_PLAYER,
    HIPLOOP_ANIM_JUMP,
    HIPLOOP_ANIM_LAND,
    HIPLOOP_ANIM_MAX
} HiploopAnim;

typedef enum {
    HIPLOOP_DMGEFF_JUMP,
    HIPLOOP_DMGEFF_STUN,
    HIPLOOP_DMGEFF_FIRE,
    HIPLOOP_DMGEFF_FREEZE,
    HIPLOOP_DMGEFF_LIGHT,
    HIPLOOP_DMGEFF_KNOCK_OFF_MASK = 0xC,
    HIPLOOP_DMGEFF_HOOKSHOT,
    HIPLOOP_DMGEFF_POUND,
    HIPLOOP_DMGEFF_DAMAGE
} HiploopDamageEffect;

typedef enum { HIPLOOP_MASK_START, HIPLOOP_MASK_FALL, HIPLOOP_MASK_DIE } HiploopMaskState;

#define HIPLOOP_BODYPART_MAX 11

struct EnHiploop;

typedef void (*EnHiploopActionFunc)(struct EnHiploop*, PlayState*);

typedef struct EnHiploop {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[HIPLOOP_LIMB_MAX];
    Vec3s morphTable[HIPLOOP_LIMB_MAX];
    EnHiploopActionFunc actionFunc;
    s16 action;
    s16 animIndex;
    s16 fragmentIndex;
    s16 timer;
    s16 dustTimer;
    s16 targetRotY;
    s16 bounceTurnSpeed;
    s16 frozenTimer;
    s16 maskState;
    bool isCharging;
    bool hasDoneFirstRoar;
    bool hasLanded;
    bool isHornOn;
    bool hasBeenDamaged;
    bool isFrozen;
    f32 animEndFrame;
    f32 maskAccelY;
    s32 blureIndex;
    s32 bodyPartIndex;
    s32 flameCount;
    Vec3f damagedVelocity;
    Vec3f backFootPos[2];
    Vec3f maskFlamesBasePos;
    Vec3f maskPos;
    Vec3f maskRot;
    Vec3f maskVelocity;
    Vec3f fragmentPos;
    Vec3f flamePos[HIPLOOP_BODY_FLAME_COUNT];
    Vec3s fragmentSpin;
    Vec3f bodyPartsPos[HIPLOOP_BODYPART_MAX];
    ColliderJntSph maskCollider;
    ColliderJntSphElement maskColliderElements[1];
    ColliderJntSph bodyCollider;
    ColliderJntSphElement bodyColliderElements[1];
    ColliderQuad hornCollider;
} EnHiploop;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sHiploopSkel[] = OBJ_PP "gHiploopSkel";
static const ALIGN_ASSET(2) char sHiploopIdleAnim[] = OBJ_PP "gHiploopIdleAnim";
static const ALIGN_ASSET(2) char sHiploopWalkAnim[] = OBJ_PP "gHiploopWalkAnim";
static const ALIGN_ASSET(2) char sHiploopWindUpAnim[] = OBJ_PP "gHiploopWindUpAnim";
static const ALIGN_ASSET(2) char sHiploopChargeAnim[] = OBJ_PP "gHiploopChargeAnim";
static const ALIGN_ASSET(2) char sHiploopAttackAnim[] = OBJ_PP "gHiploopAttackAnim";
static const ALIGN_ASSET(2) char sHiploopDamageAnim[] = OBJ_PP "gHiploopDamageAnim";
static const ALIGN_ASSET(2) char sHiploopRoarAnim[] = OBJ_PP "gHiploopRoarAnim";

static const char* const sAnimations[HIPLOOP_ANIM_MAX] = {
    sHiploopIdleAnim,   sHiploopWalkAnim, sHiploopWindUpAnim, sHiploopChargeAnim, sHiploopAttackAnim,
    sHiploopDamageAnim, sHiploopRoarAnim, sHiploopWalkAnim,   sHiploopWindUpAnim, sHiploopWalkAnim,
};

static const u8 sAnimationModes[HIPLOOP_ANIM_MAX] = {
    ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE,
    ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_ONCE,
};

static const s16 sFragmentLimbs[HIPLOOP_FRAGMENT_COUNT] = {
    HIPLOOP_LIMB_BODY,
    HIPLOOP_LIMB_FRONT_LEFT_LOWER_LEG,
    HIPLOOP_LIMB_FRONT_RIGHT_LOWER_LEG,
    HIPLOOP_LIMB_LEFT_WING_MIDDLE,
    HIPLOOP_LIMB_RIGHT_WING_MIDDLE,
    HIPLOOP_LIMB_CENTER_WING_MIDDLE,
};

static ColliderJntSphElementInit sMaskElementsInit[1] = {
    {
        { ELEMTYPE_UNK2,
          { 0xF7CFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x10 },
          { 0xF7CFFFFF, 0x00, 0x00 },
          TOUCH_ON | TOUCH_SFX_NORMAL,
          BUMP_ON | BUMP_HOOKABLE,
          OCELEM_ON },
        { HIPLOOP_LIMB_BODY, { { 0, 0, 0 }, 0 }, 1 },
    },
};

static ColliderJntSphInit sMaskColliderInit = {
    { COLTYPE_HARD, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_JNTSPH },
    1,
    sMaskElementsInit,
};

static ColliderJntSphElementInit sBodyElementsInit[1] = {
    {
        { ELEMTYPE_UNK0,
          { 0xF7CFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x04 },
          { 0xF7CFFFFF, 0x00, 0x00 },
          TOUCH_ON | TOUCH_SFX_NORMAL,
          BUMP_ON | BUMP_HOOKABLE,
          OCELEM_ON },
        { HIPLOOP_LIMB_BODY, { { 0, 0, 0 }, 0 }, 1 },
    },
};

static ColliderJntSphInit sBodyColliderInit = {
    { COLTYPE_HARD, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_JNTSPH },
    1,
    sBodyElementsInit,
};

static ColliderQuadInit sHornInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_QUAD },
    { ELEMTYPE_UNK0,
      { 0xF7CFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x08 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
      BUMP_NONE,
      OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, HIPLOOP_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, HIPLOOP_DMGEFF_DAMAGE),
    /* Slingshot     */ DMG_ENTRY(1, HIPLOOP_DMGEFF_DAMAGE),
    /* Explosive     */ DMG_ENTRY(2, HIPLOOP_DMGEFF_KNOCK_OFF_MASK),
    /* Boomerang     */ DMG_ENTRY(1, HIPLOOP_DMGEFF_DAMAGE),
    /* Normal arrow  */ DMG_ENTRY(2, HIPLOOP_DMGEFF_DAMAGE),
    /* Hammer swing  */ DMG_ENTRY(2, HIPLOOP_DMGEFF_KNOCK_OFF_MASK),
    /* Hookshot      */ DMG_ENTRY(2, HIPLOOP_DMGEFF_HOOKSHOT),
    /* Kokiri sword  */ DMG_ENTRY(1, HIPLOOP_DMGEFF_DAMAGE),
    /* Master sword  */ DMG_ENTRY(2, HIPLOOP_DMGEFF_DAMAGE),
    /* Giant's Knife */ DMG_ENTRY(4, HIPLOOP_DMGEFF_DAMAGE),
    /* Fire arrow    */ DMG_ENTRY(2, HIPLOOP_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(2, HIPLOOP_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(4, HIPLOOP_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, HIPLOOP_DMGEFF_DAMAGE),
    /* Unk arrow 2   */ DMG_ENTRY(2, HIPLOOP_DMGEFF_DAMAGE),
    /* Unk arrow 3   */ DMG_ENTRY(2, HIPLOOP_DMGEFF_DAMAGE),
    /* Fire magic    */ DMG_ENTRY(2, HIPLOOP_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(2, HIPLOOP_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(4, HIPLOOP_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, HIPLOOP_DMGEFF_JUMP),
    /* Mirror Ray    */ DMG_ENTRY(0, HIPLOOP_DMGEFF_JUMP),
    /* Kokiri spin   */ DMG_ENTRY(2, HIPLOOP_DMGEFF_DAMAGE),
    /* Giant spin    */ DMG_ENTRY(8, HIPLOOP_DMGEFF_DAMAGE),
    /* Master spin   */ DMG_ENTRY(4, HIPLOOP_DMGEFF_DAMAGE),
    /* Kokiri jump   */ DMG_ENTRY(2, HIPLOOP_DMGEFF_DAMAGE),
    /* Giant jump    */ DMG_ENTRY(8, HIPLOOP_DMGEFF_DAMAGE),
    /* Master jump   */ DMG_ENTRY(4, HIPLOOP_DMGEFF_DAMAGE),
    /* Unknown 1     */ DMG_ENTRY(0, HIPLOOP_DMGEFF_JUMP),
    /* Unblockable   */ DMG_ENTRY(0, HIPLOOP_DMGEFF_JUMP),
    /* Hammer jump   */ DMG_ENTRY(0, HIPLOOP_DMGEFF_POUND),
    /* Unknown 2     */ DMG_ENTRY(0, HIPLOOP_DMGEFF_JUMP),
};

static Color_RGBA8 sDustPrimColor = { 60, 50, 20, 255 };
static Color_RGBA8 sDustEnvColor = { 40, 30, 30, 255 };
static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sHornTipOffset = { 1160.0f, -900.0f, 0.0f };
static Vec3f sHornBaseOffset = { 100.0f, 300.0f, 0.0f };
static Vec3f sDeathSfxPos;

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;
static int16_t sHiploopActorId = -1;
static s16 sNextFragmentIndex;

static void Hiploop_Idle(EnHiploop* this, PlayState* play);
static void Hiploop_Charge(EnHiploop* this, PlayState* play);
static void Hiploop_Attack(EnHiploop* this, PlayState* play);
static void Hiploop_Roar(EnHiploop* this, PlayState* play);
static void Hiploop_Jump(EnHiploop* this, PlayState* play);
static void Hiploop_Stunned(EnHiploop* this, PlayState* play);
static void Hiploop_Damaged(EnHiploop* this, PlayState* play);
static void Hiploop_Dead(EnHiploop* this, PlayState* play);
static void Hiploop_MaskDetach(EnHiploop* this, PlayState* play);
static void Hiploop_FragmentMove(EnHiploop* this, PlayState* play);

// Falls back to the closest OoT sound when the mm_assets sound service is missing.
static void PlaySfxAt(Actor* actor, Vec3f* pos, u16 mmSfx, u16 ootSfx) {
    if (!sIsMmSfxSearched && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
        sIsMmSfxSearched = true;
    }
    if (sPlayMmSfx != NULL && sPlayMmSfx(mmSfx, pos)) {
        return;
    }
    Audio_PlayActorSound2(actor, ootSfx);
}

static void PlaySfx(Actor* actor, u16 mmSfx, u16 ootSfx) {
    PlaySfxAt(actor, &actor->projectedPos, mmSfx, ootSfx);
}

// The actor dies before the sound ends: MM keeps reading the position, so it must not point into freed memory.
static void PlayDeathSfx(Actor* actor, u16 mmSfx, u16 ootSfx) {
    sDeathSfxPos = actor->projectedPos;
    PlaySfxAt(actor, &sDeathSfxPos, mmSfx, ootSfx);
}

static bool Hiploop_IsFragment(EnHiploop* this) {
    return this->actor.params >= HIPLOOP_TYPE_FRAGMENT_BASE;
}

static bool Hiploop_IsMasked(EnHiploop* this) {
    return this->actor.params == HIPLOOP_TYPE_MASKED;
}

static void Hiploop_ChangeAnim(EnHiploop* this, s32 animIndex) {
    AnimationHeader* animation = (AnimationHeader*)sAnimations[animIndex];
    f32 morphFrames = animIndex >= HIPLOOP_ANIM_WIND_UP ? 0.0f : -10.0f;
    f32 playSpeed = animIndex == HIPLOOP_ANIM_TURN_TO_PLAYER ? 2.0f : 1.0f;

    this->animIndex = animIndex;
    this->animEndFrame = Animation_GetLastFrame(animation);
    f32 startFrame = this->action == HIPLOOP_ACTION_FRAGMENT ? this->animEndFrame : 0.0f;

    Animation_Change(&this->skelAnime, animation, playSpeed, startFrame, this->animEndFrame, sAnimationModes[animIndex],
                     morphFrames);
}

static void Hiploop_PlayAnimSfx(EnHiploop* this) {
    if (this->action == HIPLOOP_ACTION_STUNNED) {
        return;
    }
    switch (this->animIndex) {
        case HIPLOOP_ANIM_WALK:
        case HIPLOOP_ANIM_TURN_TO_PLAYER:
            if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
                PlaySfx(&this->actor, MM_SE_EN_HIPLOOP_FOOTSTEP, NA_SE_EN_DODO_J_WALK);
            }
            break;
        case HIPLOOP_ANIM_WIND_UP:
            PlaySfx(&this->actor, MM_SE_EN_HIPLOOP_FOOT - SFX_FLAG, NA_SE_EN_MORIBLIN_DASH - SFX_FLAG);
            break;
        case HIPLOOP_ANIM_CHARGE:
            PlaySfx(&this->actor, MM_SE_EN_HIPLOOP_RUN - SFX_FLAG, NA_SE_EN_MORIBLIN_DASH - SFX_FLAG);
            break;
        case HIPLOOP_ANIM_ATTACK:
            if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
                PlaySfx(&this->actor, MM_SE_EN_HIPLOOP_PAUSE, NA_SE_EN_MORIBLIN_VOICE);
            }
            break;
        case HIPLOOP_ANIM_ROAR:
            if (Animation_OnFrame(&this->skelAnime, 6.0f)) {
                PlaySfx(&this->actor, MM_SE_EN_HIPLOOP_PAUSE, NA_SE_EN_MORIBLIN_VOICE);
            }
            break;
        default:
            break;
    }
}

static void Hiploop_SpawnDust(EnHiploop* this, PlayState* play) {
    for (s32 i = 0; i < ARRAY_COUNT(this->backFootPos); i++) {
        Vec3f velocity = { Rand_CenteredFloat(2.0f), Rand_ZeroFloat(2.0f) + 1.0f, Rand_CenteredFloat(2.0f) };
        Vec3f accel = { 0.0f, -0.1f, 0.0f };
        Vec3f pos = { this->backFootPos[i].x + Rand_CenteredFloat(10.0f),
                      Rand_ZeroFloat(3.0f) + this->actor.floorHeight + 1.0f,
                      this->backFootPos[i].z + Rand_CenteredFloat(10.0f) };

        func_8002836C(play, &pos, &velocity, &accel, &sDustPrimColor, &sDustEnvColor, Rand_ZeroFloat(50.0f) + 60.0f, 30,
                      Rand_ZeroFloat(5.0f) + 20.0f);
    }
}

static void Hiploop_KickUpDust(EnHiploop* this, PlayState* play) {
    if (this->dustTimer == 0) {
        Hiploop_SpawnDust(this, play);
        this->dustTimer = 3;
    }
}

// OoT has no wall-only sphere test: a hit 20 units up is a wall unless the floor rises steeply.
static bool Hiploop_IsPathBlocked(EnHiploop* this, PlayState* play, bool* isLedge) {
    static const s16 sLedgeCheckAngles[] = { 0x0000, 0x1000, -0x1000 };
    s16 angle = this->action == HIPLOOP_ACTION_DAMAGED ? this->targetRotY : this->actor.world.rot.y;

    *isLedge = false;
    for (s32 i = 0; i < ARRAY_COUNT(sLedgeCheckAngles); i++) {
        Vec3f ledgeCheck = { this->actor.world.pos.x + Math_SinS(sLedgeCheckAngles[i] + angle) * 70.0f,
                             this->actor.floorHeight - 10.0f,
                             this->actor.world.pos.z + Math_CosS(sLedgeCheckAngles[i] + angle) * 70.0f };

        if (!BgCheck_SphVsFirstPoly(&play->colCtx, &ledgeCheck, 20.0f)) {
            *isLedge = true;
            return true;
        }
    }
    Vec3f wallCheck = { this->actor.world.pos.x + Math_SinS(angle) * 40.0f, this->actor.world.pos.y + 20.0f,
                        this->actor.world.pos.z + Math_CosS(angle) * 40.0f };

    return BgCheck_SphVsFirstPoly(&play->colCtx, &wallCheck, 10.0f);
}

static bool Hiploop_IsAtLedge(EnHiploop* this, PlayState* play) {
    bool isLedge;

    Hiploop_IsPathBlocked(this, play, &isLedge);
    return isLedge;
}

static void Hiploop_Thaw(EnHiploop* this, PlayState* play) {
    if (!this->isFrozen) {
        return;
    }
    this->isFrozen = false;
    this->frozenTimer = 0;
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < HIPLOOP_BODYPART_MAX; i++) {
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &this->bodyPartsPos[i], 150, 150, 150, 250, 235, 245, 255,
                                       0.7f);
    }
}

static void Hiploop_SetupIdle(EnHiploop* this) {
    this->action = HIPLOOP_ACTION_IDLE;
    this->actionFunc = Hiploop_Idle;
}

static void Hiploop_LookAroundHome(EnHiploop* this) {
    Vec3f lookAt = this->actor.home.pos;

    this->dustTimer = Rand_ZeroFloat(20.0f) + 20.0f;
    lookAt.x += Rand_CenteredFloat(50.0f);
    lookAt.z += Rand_CenteredFloat(50.0f);
    this->targetRotY = Math_Vec3f_Yaw(&this->actor.world.pos, &lookAt);
    this->actor.speedXZ = 0.0f;
    if (this->animIndex != HIPLOOP_ANIM_IDLE) {
        Hiploop_ChangeAnim(this, HIPLOOP_ANIM_IDLE);
    }
}

static void Hiploop_SetupCharge(EnHiploop* this) {
    this->dustTimer = 0;
    this->timer = 0;
    this->isCharging = false;
    Hiploop_ChangeAnim(this, HIPLOOP_ANIM_TURN_TO_PLAYER);
    this->action = HIPLOOP_ACTION_CHARGE;
    this->actionFunc = Hiploop_Charge;
}

// Idles at home: stops, turns toward a random spot and ambles there, until Link comes close or hurts it.
static void Hiploop_Idle(EnHiploop* this, PlayState* play) {
    bool isLedge;

    SkelAnime_Update(&this->skelAnime);
    if (this->hasBeenDamaged || this->actor.xzDistToPlayer < HIPLOOP_ATTACK_RANGE) {
        Hiploop_SetupCharge(this);
        return;
    }
    if (this->timer == 0 && this->dustTimer == 0) {
        Hiploop_LookAroundHome(this);
    }
    if (this->dustTimer == 1) {
        this->timer = Rand_ZeroFloat(40.0f) + 40.0f;
    }
    if (Hiploop_IsPathBlocked(this, play, &isLedge)) {
        this->timer = 0;
        if (this->animIndex != HIPLOOP_ANIM_WALK) {
            Hiploop_ChangeAnim(this, HIPLOOP_ANIM_WALK);
        }
        Math_SmoothStepToS(&this->actor.world.rot.y, this->targetRotY, 1, 0x258, 0);
        return;
    }
    if (this->dustTimer != 0 || this->timer == 0) {
        return;
    }
    if (this->animIndex == HIPLOOP_ANIM_IDLE) {
        Hiploop_ChangeAnim(this, HIPLOOP_ANIM_WALK);
    }
    s16 yawLeft = this->actor.world.rot.y - this->targetRotY;

    if (this->bounceTurnSpeed < 0x64 && ABS(yawLeft) < 100) {
        Math_ApproachF(&this->actor.speedXZ, 1.0f, 0.3f, 1.0f);
    }
    Math_SmoothStepToS(&this->actor.world.rot.y, this->targetRotY, 1, this->bounceTurnSpeed + 0x258, 0);
    Math_SmoothStepToS(&this->bounceTurnSpeed, 0, 1, 0x1F4, 0);
}

static void Hiploop_SetupAttack(EnHiploop* this) {
    Hiploop_ChangeAnim(this, HIPLOOP_ANIM_ATTACK);
    this->isHornOn = true;
    this->action = HIPLOOP_ACTION_ATTACK;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Hiploop_Attack;
}

static void Hiploop_SetupRoar(EnHiploop* this) {
    this->hasDoneFirstRoar = false;
    this->timer = 20;
    this->dustTimer = 0;
    Hiploop_ChangeAnim(this, HIPLOOP_ANIM_ROAR);
    this->action = HIPLOOP_ACTION_ROAR;
    this->actionFunc = Hiploop_Roar;
}

static bool Hiploop_WasBounced(EnHiploop* this) {
    return (this->maskCollider.base.atFlags & AT_BOUNCED) || (this->bodyCollider.base.atFlags & AT_BOUNCED);
}

static bool Hiploop_HasHitSomething(EnHiploop* this) {
    return (this->maskCollider.base.atFlags & AT_HIT) || (this->bodyCollider.base.atFlags & AT_HIT);
}

// A shield bounce sends a masked Hiploop back half as hard as a bare one.
static void Hiploop_RunCharge(EnHiploop* this, PlayState* play) {
    bool isLedge;

    if (Hiploop_IsPathBlocked(this, play, &isLedge)) {
        this->actor.speedXZ = 0.0f;
        Hiploop_SetupRoar(this);
        return;
    }
    Math_ApproachF(&this->actor.speedXZ, 10.0f, 0.3f, 1.0f);
    if (this->timer == 0) {
        Hiploop_SetupRoar(this);
        return;
    }
    if (Hiploop_WasBounced(this)) {
        this->actor.speedXZ *= Hiploop_IsMasked(this) ? -0.5f : -1.0f;
        Hiploop_SetupRoar(this);
        return;
    }
    if (Hiploop_HasHitSomething(this)) {
        Hiploop_SetupAttack(this);
    }
}

static void Hiploop_Charge(EnHiploop* this, PlayState* play) {
    f32 curFrame = this->skelAnime.curFrame;

    SkelAnime_Update(&this->skelAnime);
    if (!this->isCharging || this->animIndex == HIPLOOP_ANIM_WIND_UP) {
        this->targetRotY = this->actor.yawTowardsPlayer;
        Math_SmoothStepToS(&this->actor.world.rot.y, this->targetRotY, 1, this->bounceTurnSpeed + 0x7D0, 0);
    }
    Math_SmoothStepToS(&this->bounceTurnSpeed, 0, 1, 0x1F4, 0);
    if (!this->isCharging) {
        s16 yawLeft = this->actor.world.rot.y - this->targetRotY;

        Math_ApproachZeroF(&this->actor.speedXZ, 0.5f, 1.0f);
        if (ABS(yawLeft) < 100) {
            this->isCharging = true;
        }
    } else if (this->animIndex == HIPLOOP_ANIM_TURN_TO_PLAYER) {
        Hiploop_ChangeAnim(this, HIPLOOP_ANIM_WIND_UP);
    } else if (this->animIndex == HIPLOOP_ANIM_WIND_UP) {
        if (curFrame >= this->animEndFrame) {
            this->timer = 20;
            Hiploop_ChangeAnim(this, HIPLOOP_ANIM_CHARGE);
        }
    } else if (this->animIndex == HIPLOOP_ANIM_CHARGE) {
        Hiploop_RunCharge(this, play);
        if (this->action != HIPLOOP_ACTION_CHARGE) {
            return;
        }
    }
    bool isRunning = this->animIndex == HIPLOOP_ANIM_WIND_UP || this->animIndex == HIPLOOP_ANIM_CHARGE;

    if (isRunning) {
        Hiploop_KickUpDust(this, play);
    }
}

static void Hiploop_Attack(EnHiploop* this, PlayState* play) {
    f32 curFrame = this->skelAnime.curFrame;

    SkelAnime_Update(&this->skelAnime);
    if (curFrame >= this->animEndFrame) {
        Hiploop_SetupCharge(this);
    }
}

static bool Hiploop_HasFallenFromHome(EnHiploop* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    return fabsf(this->actor.home.pos.y - this->actor.world.pos.y) > 100.0f &&
           fabsf(this->actor.floorHeight - player->actor.floorHeight) > 100.0f;
}

static void Hiploop_SetupDead(EnHiploop* this, PlayState* play);

// Skids to a halt roaring twice, then charges again; a fall far below its home and Link kills it.
static void Hiploop_Roar(EnHiploop* this, PlayState* play) {
    f32 curFrame = this->skelAnime.curFrame;

    SkelAnime_Update(&this->skelAnime);
    if (Hiploop_IsAtLedge(this, play)) {
        this->actor.speedXZ = 0.0f;
    }
    if (Hiploop_HasFallenFromHome(this, play)) {
        Hiploop_SetupDead(this, play);
        return;
    }
    Math_ApproachZeroF(&this->actor.speedXZ, 0.5f, 1.0f);
    if (this->actor.speedXZ > 0.3f) {
        Hiploop_KickUpDust(this, play);
    }
    if (!this->hasDoneFirstRoar && curFrame >= this->animEndFrame) {
        this->skelAnime.startFrame = 6.0f;
        this->hasDoneFirstRoar = true;
    }
    if (this->timer == 0) {
        Hiploop_SetupCharge(this);
    }
}

static void Hiploop_SetupJump(EnHiploop* this) {
    this->dustTimer = 0;
    this->timer = 0;
    this->hasLanded = false;
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = 20.0f;
    this->actor.gravity = -3.0f;
    Hiploop_ChangeAnim(this, HIPLOOP_ANIM_JUMP);
    this->action = HIPLOOP_ACTION_JUMP;
    this->actionFunc = Hiploop_Jump;
}

static void Hiploop_Jump(EnHiploop* this, PlayState* play) {
    f32 curFrame = this->skelAnime.curFrame;

    SkelAnime_Update(&this->skelAnime);
    this->targetRotY = this->actor.yawTowardsPlayer;
    Math_SmoothStepToS(&this->actor.world.rot.y, this->targetRotY, 1, 0x1388, 0);
    if (!this->hasLanded) {
        if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
            this->hasLanded = true;
            Hiploop_ChangeAnim(this, HIPLOOP_ANIM_LAND);
        }
    } else if (curFrame >= this->animEndFrame) {
        Hiploop_SetupIdle(this);
    }
}

static void Hiploop_SetupStunned(EnHiploop* this, s16 stunFrames) {
    this->dustTimer = stunFrames;
    this->actor.speedXZ = 0.0f;
    this->action = HIPLOOP_ACTION_STUNNED;
    this->actionFunc = Hiploop_Stunned;
}

static void Hiploop_Stunned(EnHiploop* this, PlayState* play) {
    if (this->dustTimer != 0 || this->frozenTimer != 0) {
        return;
    }
    Hiploop_Thaw(this, play);
    Hiploop_SetupCharge(this);
}

static void Hiploop_KnockBack(EnHiploop* this) {
    this->damagedVelocity.x = Math_SinS(this->actor.yawTowardsPlayer) * -30.0f;
    this->damagedVelocity.y = 0.0f;
    this->damagedVelocity.z = Math_CosS(this->actor.yawTowardsPlayer) * -30.0f;
}

static void Hiploop_StopAtLedge(EnHiploop* this, PlayState* play) {
    if (Hiploop_IsAtLedge(this, play)) {
        this->damagedVelocity.x = 0.0f;
        this->damagedVelocity.z = 0.0f;
        this->actor.speedXZ = 0.0f;
    }
}

static void Hiploop_SetupDamaged(EnHiploop* this, PlayState* play) {
    Hiploop_KnockBack(this);
    Hiploop_Thaw(this, play);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    this->dustTimer = 0;
    Hiploop_ChangeAnim(this, HIPLOOP_ANIM_DAMAGE);
    this->targetRotY = this->actor.yawTowardsPlayer + 0x8000;
    this->action = HIPLOOP_ACTION_DAMAGED;
    Hiploop_StopAtLedge(this, play);
    PlaySfx(&this->actor, MM_SE_EN_HIPLOOP_DAMAGE, NA_SE_EN_DODO_J_DAMAGE);
    this->actionFunc = Hiploop_Damaged;
}

// Spins backwards away from the blow until the knockback dies out.
static void Hiploop_Damaged(EnHiploop* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->actor.world.rot.y += 0x1000;
    Hiploop_KickUpDust(this, play);
    if (this->actor.colorFilterTimer == 0 && this->damagedVelocity.x < 1.0f && this->damagedVelocity.z < 1.0f) {
        Hiploop_SetupCharge(this);
    }
    Hiploop_StopAtLedge(this, play);
    if (Hiploop_HasFallenFromHome(this, play)) {
        Hiploop_SetupDead(this, play);
    }
}

static void Hiploop_SetupDead(EnHiploop* this, PlayState* play) {
    this->dustTimer = 0;
    Hiploop_KnockBack(this);
    this->bounceTurnSpeed = 0;
    this->isCharging = false;
    Hiploop_ChangeAnim(this, HIPLOOP_ANIM_DAMAGE);
    this->timer = 15;
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 25);
    Enemy_StartFinishingBlow(play, &this->actor);
    PlaySfx(&this->actor, MM_SE_EN_HIPLOOP_DEAD, NA_SE_EN_DODO_J_DEAD);
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->action = HIPLOOP_ACTION_DEAD;
    this->actionFunc = Hiploop_Dead;
}

static bool Hiploop_SplashIfInWater(EnHiploop* this, PlayState* play, s32 splashCount) {
    WaterBox* waterBox;
    f32 waterSurface;
    bool isOverWater = WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z,
                                            &waterSurface, &waterBox);

    if (!isOverWater || this->actor.world.pos.y >= waterSurface + 5.0f) {
        return false;
    }
    for (s32 i = 0; i < splashCount; i++) {
        Vec3f splashPos = this->actor.world.pos;

        splashPos.x += Rand_CenteredFloat(10 + (5 * i));
        splashPos.z += Rand_CenteredFloat(40 + (5 * i));
        EffectSsGSplash_Spawn(play, &splashPos, NULL, NULL, 0, (Rand_ZeroOne() * 100.0f) + 400.0f);
    }
    PlayDeathSfx(&this->actor, NA_SE_EV_BOMB_DROP_WATER, NA_SE_EV_BOMB_DROP_WATER);
    return true;
}

static void Hiploop_SpawnFragments(EnHiploop* this, PlayState* play) {
    for (s32 i = 0; i < HIPLOOP_FRAGMENT_COUNT; i++) {
        Actor_Spawn(&play->actorCtx, play, sHiploopActorId, this->actor.world.pos.x, this->actor.world.pos.y,
                    this->actor.world.pos.z, this->actor.world.rot.x, this->actor.world.rot.y, this->actor.world.rot.z,
                    sFragmentLimbs[i] + HIPLOOP_TYPE_FRAGMENT_BASE);
    }
}

// Spins on the spot and bursts into flaming fragments, sooner if it lands in water.
static void Hiploop_Dead(EnHiploop* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->actor.world.rot.y += 0x3000;
    if (this->isFrozen) {
        Hiploop_Thaw(this, play);
    }
    Hiploop_KickUpDust(this, play);
    bool isInWater = Hiploop_SplashIfInWater(this, play, 5);

    if (!isInWater && this->timer != 1) {
        return;
    }
    Hiploop_SpawnFragments(this, play);
    Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos,
                               this->actor.params == HIPLOOP_TYPE_UNMASKED ? 0x70 : 0xE0);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

static void Hiploop_SpawnUnmasked(EnHiploop* this, PlayState* play) {
    Actor_Spawn(&play->actorCtx, play, sHiploopActorId, this->actor.world.pos.x, this->actor.world.pos.y,
                this->actor.world.pos.z, this->actor.world.rot.x, this->actor.shape.rot.y, this->actor.world.rot.z,
                HIPLOOP_TYPE_UNMASKED);
}

// The masked actor becomes the flying mask; a bare Hiploop takes its place.
static void Hiploop_SetupMaskDetach(EnHiploop* this, PlayState* play) {
    Hiploop_SpawnUnmasked(this, play);
    this->actor.gravity = 0.0f;
    this->actor.speedXZ = 0.0f;
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    this->maskState = HIPLOOP_MASK_START;
    Hiploop_ChangeAnim(this, HIPLOOP_ANIM_IDLE);
    SkelAnime_Update(&this->skelAnime);
    this->action = HIPLOOP_ACTION_MASK_DETACH;
    this->actionFunc = Hiploop_MaskDetach;
}

static void Hiploop_BurnMask(EnHiploop* this, PlayState* play) {
    static Vec3f sFlameDrift[] = {
        { 1.0f, 0.0f, 0.5f },   { 1.0f, 0.0f, -0.5f },  { -1.0f, 0.0f, 0.5f },
        { -1.0f, 0.0f, -0.5f }, { 0.5f, 0.0f, 1.0f },   { -0.5f, 0.0f, 1.0f },
        { 0.5f, 0.0f, -1.0f },  { -0.5f, 0.0f, -1.0f }, { 0.0f, 0.0f, 0.0f },
    };

    for (s32 i = 0; i < ARRAY_COUNT(sFlameDrift); i++) {
        Vec3f flamePos = { this->maskFlamesBasePos.x + Rand_CenteredFloat(20.0f), this->actor.floorHeight,
                           this->maskFlamesBasePos.z + Rand_CenteredFloat(20.0f) };

        EffectSsDeadDb_Spawn(play, &flamePos, &sFlameDrift[i], &sFlameDrift[i], 70, 0, 255, 255, 255, 255, 0, 0, 255, 1,
                             9, false);
    }
    Actor_Kill(&this->actor);
}

// Flies off in a bouncing arc once the hookshot lets go, then bursts into blue flames.
static void Hiploop_MaskDetach(EnHiploop* this, PlayState* play) {
    bool isHeld = CHECK_FLAG_ALL(this->actor.flags, ACTOR_FLAG_HOOKSHOT_ATTACHED);

    if (isHeld && this->action != HIPLOOP_ACTION_MASK_DEAD) {
        return;
    }
    switch (this->maskState) {
        case HIPLOOP_MASK_START:
            this->action = HIPLOOP_ACTION_MASK_DEAD;
            this->maskAccelY = 50.0f;
            this->maskVelocity.y = -230.0f;
            this->maskVelocity.x = -150.0f;
            PlaySfx(&this->actor, MM_SE_EN_HIPLOOP_MASC_OFF, NA_SE_IT_SHIELD_BOUND);
            this->maskState = HIPLOOP_MASK_FALL;
            break;
        case HIPLOOP_MASK_FALL:
            if (this->maskPos.y > 800.0f) {
                this->maskAccelY = 50.0f;
                this->maskState = HIPLOOP_MASK_DIE;
                this->maskVelocity.y = -200.0f;
                this->maskVelocity.x = -150.0f;
            }
            break;
        case HIPLOOP_MASK_DIE:
            if (this->maskPos.y > 900.0f && this->maskVelocity.y > 0.0f) {
                Hiploop_BurnMask(this, play);
                return;
            }
            break;
        default:
            break;
    }
    this->maskVelocity.y += this->maskAccelY;
    this->maskRot.z += 0x2000;
    this->maskPos.x += this->maskVelocity.x;
    this->maskPos.y += this->maskVelocity.y;
    this->maskPos.z += this->maskVelocity.z;
}

static void Hiploop_SetupFragmentMove(EnHiploop* this) {
    this->action = HIPLOOP_ACTION_FRAGMENT;
    Hiploop_ChangeAnim(this, HIPLOOP_ANIM_DAMAGE);
    this->actor.velocity.y = Rand_ZeroFloat(5.0f) + 13.0f;
    this->actor.gravity = -2.0f;
    this->timer = Rand_S16Offset(30, 30);
    this->fragmentSpin.x = (this->fragmentIndex * 0x2E) + 0xFF00;
    this->fragmentSpin.z = (this->fragmentIndex * 0x2E) + 0xFF00;
    if (this->actor.params != HIPLOOP_TYPE_FRAGMENT_BODY) {
        this->actor.speedXZ = Rand_ZeroFloat(4.0f) + 4.0f;
        this->actor.world.rot.y = ((s32)Rand_CenteredFloat(223.0f) + 0x1999) * this->fragmentIndex;
    }
    this->actionFunc = Hiploop_FragmentMove;
}

// The body fragment burns in a ring of ten blue flames, every limb fragment in one.
static void Hiploop_FragmentMove(EnHiploop* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.params == HIPLOOP_TYPE_FRAGMENT_BODY) {
        this->flameCount = HIPLOOP_BODY_FLAME_COUNT;
        for (s32 i = 0; i < HIPLOOP_BODY_FLAME_COUNT; i++) {
            this->flamePos[i].x = this->fragmentPos.x + Math_SinS(0xCCC * i) * 15.0f;
            this->flamePos[i].y = this->fragmentPos.y - 5.0f;
            this->flamePos[i].z = this->fragmentPos.z + Math_CosS(0xCCC * i) * 15.0f;
        }
    } else {
        this->flameCount = 1;
        this->flamePos[0] = this->fragmentPos;
        this->actor.shape.rot.x += this->fragmentSpin.x;
        this->actor.shape.rot.z += this->fragmentSpin.z;
    }
    bool isBody = this->actor.params == HIPLOOP_TYPE_FRAGMENT_BODY;

    if (Hiploop_SplashIfInWater(this, play, isBody ? 6 : 1)) {
        this->timer = 0;
    }
    if (this->timer == 0 || (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        Actor_Kill(&this->actor);
    }
}

static void Hiploop_Freeze(EnHiploop* this, PlayState* play) {
    if (!this->isFrozen) {
        Actor_ApplyDamage(&this->actor);
        this->isFrozen = true;
        this->frozenTimer = 80;
        Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 80);
    }
    if (this->actor.colChkInfo.health == 0) {
        Hiploop_SetupDead(this, play);
    } else {
        Hiploop_SetupStunned(this, 0);
    }
}

static void Hiploop_Stun(EnHiploop* this) {
    if (this->isFrozen) {
        return;
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 40);
    Hiploop_SetupStunned(this, 40);
}

static void Hiploop_TakeDamage(EnHiploop* this, PlayState* play) {
    Actor_ApplyDamage(&this->actor);
    if (this->actor.colChkInfo.health > 0) {
        Hiploop_SetupDamaged(this, play);
    } else {
        Hiploop_SetupDead(this, play);
    }
}

// A bare body reacts to its damage effect; returns false when the blow glanced off.
static bool Hiploop_ReactToBodyHit(EnHiploop* this, PlayState* play) {
    if (this->action >= HIPLOOP_ACTION_DAMAGED || this->action == HIPLOOP_ACTION_JUMP) {
        return true;
    }
    switch (this->actor.colChkInfo.damageEffect) {
        case HIPLOOP_DMGEFF_STUN:
            Hiploop_Stun(this);
            return true;
        case HIPLOOP_DMGEFF_FREEZE:
            this->hasBeenDamaged = true;
            Hiploop_Freeze(this, play);
            return true;
        case HIPLOOP_DMGEFF_POUND:
        case HIPLOOP_DMGEFF_JUMP:
            Hiploop_SetupJump(this);
            return true;
        case HIPLOOP_DMGEFF_LIGHT:
            Actor_SetColorFilter(&this->actor, COLORFILTER_GRAY, 255, COLORFILTER_OPA, 25);
            break;
        default:
            break;
    }
    this->hasBeenDamaged = true;
    this->actor.speedXZ = 0.0f;
    Hiploop_TakeDamage(this, play);
    return true;
}

// The mask only gives way to the hookshot, explosives or a hammer, and only from the front.
static bool Hiploop_ReactToMaskHit(EnHiploop* this, PlayState* play, s16 yawFromFront) {
    if (yawFromFront >= HIPLOOP_MASK_FRONT_ARC) {
        return false;
    }
    switch (this->actor.colChkInfo.damageEffect) {
        case HIPLOOP_DMGEFF_HOOKSHOT:
            Hiploop_SetupMaskDetach(this, play);
            return true;
        case HIPLOOP_DMGEFF_POUND:
        case HIPLOOP_DMGEFF_KNOCK_OFF_MASK:
            Hiploop_SetupMaskDetach(this, play);
            this->action = HIPLOOP_ACTION_MASK_DEAD;
            return true;
        default:
            return true;
    }
}

static void Hiploop_GlanceOffMask(EnHiploop* this) {
    bool canTurn = this->action == HIPLOOP_ACTION_IDLE || this->action == HIPLOOP_ACTION_CHARGE ||
                   this->action == HIPLOOP_ACTION_ROAR;

    if (!canTurn) {
        return;
    }
    this->dustTimer = 0;
    this->timer = 10;
    this->bounceTurnSpeed = 0x20D0;
    this->actor.speedXZ = 0.0f;
    if (this->action == HIPLOOP_ACTION_CHARGE) {
        this->isCharging = false;
        Hiploop_ChangeAnim(this, HIPLOOP_ANIM_TURN_TO_PLAYER);
        this->bounceTurnSpeed = 0x1B58;
    }
    this->targetRotY = this->actor.yawTowardsPlayer;
    if (Hiploop_IsMasked(this)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_BOUND);
    }
}

static void Hiploop_UpdateDamage(EnHiploop* this, PlayState* play) {
    s16 yawDiff = this->actor.yawTowardsPlayer - this->actor.world.rot.y;
    s16 yawFromFront = ABS(yawDiff);
    bool isMasked = Hiploop_IsMasked(this);
    bool isMaskHit = (this->maskCollider.elements[0].info.bumperFlags & BUMP_HIT) != 0;
    bool isBodyHit = (this->bodyCollider.elements[0].info.bumperFlags & BUMP_HIT) != 0;
    bool hasGlanced = false;

    this->maskCollider.base.acFlags &= ~AC_HIT;
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    if (isMasked) {
        if (yawFromFront < HIPLOOP_MASK_FRONT_ARC) {
            this->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR;
        } else {
            this->actor.flags &= ~ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR;
        }
    }
    if (isMasked && isMaskHit && this->action < HIPLOOP_ACTION_MASK_DETACH) {
        hasGlanced = !Hiploop_ReactToMaskHit(this, play, yawFromFront);
    }
    if (isBodyHit) {
        hasGlanced |= isMasked || !Hiploop_ReactToBodyHit(this, play);
    }
    if (hasGlanced) {
        Hiploop_GlanceOffMask(this);
    }
}

static void Hiploop_ShapeColliders(EnHiploop* this) {
    ColliderJntSphElementDim* mask = &this->maskCollider.elements[0].dim;
    ColliderJntSphElementDim* body = &this->bodyCollider.elements[0].dim;
    bool isMasked = Hiploop_IsMasked(this);

    mask->modelSphere.radius = isMasked ? 25 : 42;
    mask->modelSphere.center.x = isMasked ? 780 : 400;
    mask->modelSphere.center.y = isMasked ? -500 : -400;
    mask->scale = 1.0f;
    body->modelSphere.radius = isMasked ? 25 : 20;
    body->modelSphere.center.x = 400;
    body->modelSphere.center.y = -400;
    body->scale = 1.0f;
}

static void Hiploop_InitColliders(EnHiploop* this, PlayState* play) {
    Collider_InitJntSph(play, &this->maskCollider);
    Collider_SetJntSph(play, &this->maskCollider, &this->actor, &sMaskColliderInit, this->maskColliderElements);
    Collider_InitJntSph(play, &this->bodyCollider);
    Collider_SetJntSph(play, &this->bodyCollider, &this->actor, &sBodyColliderInit, this->bodyColliderElements);
    Collider_InitQuad(play, &this->hornCollider);
    Collider_SetQuad(play, &this->hornCollider, &this->actor, &sHornInit);
    if (Hiploop_IsMasked(this)) {
        return;
    }
    ColliderInfo* mask = &this->maskColliderElements[0].info;

    mask->toucherFlags &= ~TOUCH_ON;
    mask->bumperFlags &= ~BUMP_ON;
    mask->ocElemFlags &= ~OCELEM_ON;
    this->maskCollider.base.colType = COLTYPE_HIT2;
}

static void Hiploop_InitFragment(EnHiploop* this, PlayState* play) {
    this->fragmentIndex = sNextFragmentIndex;
    sNextFragmentIndex++;
    this->actor.shape.rot.y = this->actor.world.rot.y;
    this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_PROP);
    Hiploop_SetupFragmentMove(this);
}

static void EnHiploop_Init(Actor* thisx, PlayState* play) {
    static EffectBlureInit2 sBlureInit = {
        .calcMode = 0,
        .flags = 8,
        .p1StartColor = { 255, 255, 255, 255 },
        .p2StartColor = { 255, 255, 255, 64 },
        .p1EndColor = { 255, 255, 255, 0 },
        .p2EndColor = { 255, 255, 255, 0 },
        .elemDuration = 8,
        .drawMode = 2,
    };
    EnHiploop* this = (EnHiploop*)thisx;

    if (this->actor.params < 0 || (this->actor.params > HIPLOOP_TYPE_UNMASKED && !Hiploop_IsFragment(this))) {
        this->actor.params = HIPLOOP_TYPE_MASKED;
    }
    this->actor.targetMode = 4;
    this->actor.colChkInfo.mass = 60;
    this->actor.colChkInfo.health = 3;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    this->blureIndex = -1;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sHiploopSkel, (AnimationHeader*)sHiploopIdleAnim,
                       this->jointTable, this->morphTable, HIPLOOP_LIMB_MAX);
    Actor_SetScale(&this->actor, HIPLOOP_SCALE);
    if (Hiploop_IsFragment(this)) {
        Hiploop_InitFragment(this, play);
        return;
    }
    Hiploop_InitColliders(this, play);
    Hiploop_ShapeColliders(this);
    Effect_Add(play, &this->blureIndex, EFFECT_BLURE2, 0, 0, &sBlureInit);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 17.0f);
    this->actor.gravity = -3.0f;
    if (this->actor.params == HIPLOOP_TYPE_UNMASKED) {
        Hiploop_SetupJump(this);
    } else {
        Hiploop_SetupIdle(this);
    }
}

static void EnHiploop_Destroy(Actor* thisx, PlayState* play) {
    EnHiploop* this = (EnHiploop*)thisx;

    if (Hiploop_IsFragment(this)) {
        return;
    }
    Collider_DestroyJntSph(play, &this->maskCollider);
    Collider_DestroyJntSph(play, &this->bodyCollider);
    Collider_DestroyQuad(play, &this->hornCollider);
    if (this->blureIndex >= 0) {
        Effect_Delete(play, this->blureIndex);
    }
}

static void Hiploop_TickTimers(EnHiploop* this) {
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->dustTimer != 0) {
        this->dustTimer--;
    }
    if (this->frozenTimer != 0) {
        this->frozenTimer--;
    }
}

static bool Hiploop_IsDrowning(EnHiploop* this, PlayState* play) {
    WaterBox* waterBox;
    f32 waterSurface;

    if (this->action >= HIPLOOP_ACTION_DEAD) {
        return false;
    }
    return WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &waterSurface,
                                &waterBox) &&
           this->actor.world.pos.y < waterSurface;
}

static void Hiploop_ApplyKnockback(EnHiploop* this) {
    bool isSliding = this->action == HIPLOOP_ACTION_DEAD || (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);

    if (!isSliding) {
        return;
    }
    this->actor.world.pos.x += this->damagedVelocity.x;
    this->actor.world.pos.z += this->damagedVelocity.z;
    Math_ApproachZeroF(&this->damagedVelocity.x, 1.0f, 2.0f);
    Math_ApproachZeroF(&this->damagedVelocity.z, 1.0f, 2.0f);
}

static void Hiploop_SubmitColliders(EnHiploop* this, PlayState* play) {
    if (this->action < HIPLOOP_ACTION_DEAD || this->action == HIPLOOP_ACTION_JUMP) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->maskCollider.base);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    if (this->action < HIPLOOP_ACTION_MASK_DETACH && this->action != HIPLOOP_ACTION_STUNNED) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->maskCollider.base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    if (this->isHornOn) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->hornCollider.base);
    }
}

static void EnHiploop_Update(Actor* thisx, PlayState* play) {
    EnHiploop* this = (EnHiploop*)thisx;

    Hiploop_TickTimers(this);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    Hiploop_PlayAnimSfx(this);
    Actor_MoveXZGravity(&this->actor);
    if (this->action == HIPLOOP_ACTION_FRAGMENT) {
        Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 40.0f, 40.0f, BGCHECK_UPDATE_ALL);
        return;
    }
    Hiploop_ShapeColliders(this);
    if (Hiploop_IsDrowning(this, play)) {
        Hiploop_SetupDead(this, play);
        return;
    }
    Hiploop_UpdateDamage(this, play);
    Actor_SetFocus(&this->actor, 40.0f);
    Hiploop_ApplyKnockback(this);
    Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 40.0f, 40.0f, BGCHECK_UPDATE_ALL);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    Hiploop_SubmitColliders(this, play);
}

static s32 Hiploop_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnHiploop* this = (EnHiploop*)thisx;

    if (this->action == HIPLOOP_ACTION_FRAGMENT) {
        if (limbIndex + HIPLOOP_TYPE_FRAGMENT_BASE != this->actor.params) {
            *dList = NULL;
        }
        return false;
    }
    bool isFlyingMask = this->action == HIPLOOP_ACTION_MASK_DETACH || this->action == HIPLOOP_ACTION_MASK_DEAD;

    if (limbIndex != HIPLOOP_LIMB_MASK) {
        if (isFlyingMask) {
            *dList = NULL;
        }
        return false;
    }
    if (!Hiploop_IsMasked(this)) {
        *dList = NULL;
    }
    rot->x += (s16)this->maskRot.x;
    rot->y += (s16)this->maskRot.y;
    rot->z += (s16)this->maskRot.z;
    pos->x += this->maskPos.x;
    pos->y += this->maskPos.y;
    pos->z += this->maskPos.z;
    return false;
}

// The horn swipe hits from the first to the tenth frame of the attack, trailing last frame's edge.
static void Hiploop_UpdateHorn(EnHiploop* this) {
    if (this->skelAnime.curFrame > 26.0f || this->action >= HIPLOOP_ACTION_MASK_DETACH) {
        this->isHornOn = false;
        EffectBlure_AddSpace(Effect_GetByIndex(this->blureIndex));
        return;
    }
    if (this->skelAnime.curFrame <= 0.0f || this->skelAnime.curFrame >= 10.0f) {
        return;
    }
    Vec3f previousBase = this->hornCollider.dim.quad[0];
    Vec3f previousTip = this->hornCollider.dim.quad[1];
    Vec3f tip;
    Vec3f base;

    Matrix_MultVec3f(&sHornTipOffset, &tip);
    Matrix_MultVec3f(&sHornBaseOffset, &base);
    Collider_SetQuadVertices(&this->hornCollider, &base, &tip, &previousBase, &previousTip);
    EffectBlure_AddVertex(Effect_GetByIndex(this->blureIndex), &tip, &base);
}

static bool Hiploop_IsBodyPartLimb(s32 limbIndex) {
    switch (limbIndex) {
        case HIPLOOP_LIMB_BODY:
        case HIPLOOP_LIMB_FRONT_LEFT_LOWER_LEG:
        case HIPLOOP_LIMB_FRONT_RIGHT_LOWER_LEG:
        case HIPLOOP_LIMB_LEFT_WING_MIDDLE:
        case HIPLOOP_LIMB_BACK_RIGHT_LOWER_LEG:
        case HIPLOOP_LIMB_RIGHT_WING_MIDDLE:
        case HIPLOOP_LIMB_CENTER_WING_BASE:
        case HIPLOOP_LIMB_CENTER_WING_MIDDLE:
        case HIPLOOP_LIMB_BACK_LEFT_LOWER_LEG:
        case HIPLOOP_LIMB_RIGHT_EYE:
        case HIPLOOP_LIMB_LEFT_EYE:
            return true;
        default:
            return false;
    }
}

static void Hiploop_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnHiploop* this = (EnHiploop*)thisx;

    if (this->action == HIPLOOP_ACTION_FRAGMENT) {
        if (limbIndex + HIPLOOP_TYPE_FRAGMENT_BASE == this->actor.params) {
            Matrix_MultVec3f(&sZeroVec, &this->fragmentPos);
        }
        return;
    }
    if (limbIndex == HIPLOOP_LIMB_BACK_RIGHT_FOOT) {
        Matrix_MultVec3f(&sZeroVec, &this->backFootPos[0]);
    } else if (limbIndex == HIPLOOP_LIMB_BACK_LEFT_FOOT) {
        Matrix_MultVec3f(&sZeroVec, &this->backFootPos[1]);
    } else if (limbIndex == HIPLOOP_LIMB_MASK && Hiploop_IsMasked(this)) {
        Matrix_MultVec3f(&sZeroVec, &this->maskFlamesBasePos);
        if (this->isHornOn) {
            Hiploop_UpdateHorn(this);
        }
    }
    Collider_UpdateSpheres(limbIndex, &this->maskCollider);
    Collider_UpdateSpheres(limbIndex, &this->bodyCollider);
    if (Hiploop_IsBodyPartLimb(limbIndex)) {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[this->bodyPartIndex]);
        this->bodyPartIndex = (this->bodyPartIndex + 1) % HIPLOOP_BODYPART_MAX;
    }
}

// MM draws these with its damage-effect fire; OoT's scrolling flame billboard tinted blue stands in.
static void Hiploop_DrawFlames(EnHiploop* this, PlayState* play) {
    f32 scale = this->actor.params == HIPLOOP_TYPE_FRAGMENT_BODY ? 0.006f : 0.004f;
    MtxF billboard;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_RotateY(BINANG_TO_RAD(BINANG_ROT180(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)))), MTXMODE_NEW);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_Get(&billboard);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                           (play->gameplayFrames * -20) % 512, 0x20, 0x80));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 170, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 255, 255);
    for (s32 i = 0; i < this->flameCount; i++) {
        billboard.xw = this->flamePos[i].x;
        billboard.yw = this->flamePos[i].y;
        billboard.zw = this->flamePos[i].z;
        Matrix_Put(&billboard);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnHiploop_Draw(Actor* thisx, PlayState* play) {
    EnHiploop* this = (EnHiploop*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          Hiploop_OverrideLimbDraw, Hiploop_PostLimbDraw, this);
    if (this->flameCount != 0) {
        Hiploop_DrawFlames(this, play);
    }
}

// MM's CanKillEnemy(ACTOR_EN_PP) counts on Woodfall's water; anywhere else the mask must come off first,
// and the hookshot, explosives or hammer that do it also finish the bare body.
static bool Hiploop_CanKillInLogic(void* userData) {
    return sApi->LogicCanUseItem(RG_HOOKSHOT) || sApi->LogicHasCapability("HasExplosives") ||
           sApi->LogicCanUseItem(RG_MEGATON_HAMMER);
}

#define SOUL_FLAME_DL "__OTR__objects/object_gi_fire/gGiBlueFireFlameDL"

// Ship's enemy soul flame, which its own enemies' souls burn in.
static void DrawSoulFlame(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 8,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 16, 32, 1, play->state.frames,
                                             -8 * play->state.frames, 16, 32, 0, 0, 1, -8));
    Matrix_Push();
    Matrix_Translate(0.0f, -70.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(5.0f, 5.0f, 5.0f, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, 200, 60, 60, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)SOUL_FLAME_DL);
    gSPGrayscale(POLY_XLU_DISP++, false);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// Segments the enemy's lists read but only its actor fills; four entries cover the indexed render-mode table.
static Gfx sSoulEmptyDL[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

// The soul shows its enemy in an idle loop, as Majora's Mask souls do.
static void DrawSoul(PlayState* play, GetItemEntry* getItemEntry) {
    static SkelAnime sSoulSkelAnime;
    static Vec3s sSoulJointTable[HIPLOOP_LIMB_MAX];
    static Vec3s sSoulMorphTable[HIPLOOP_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sHiploopSkel, (AnimationHeader*)sHiploopIdleAnim,
                           sSoulJointTable, sSoulMorphTable, HIPLOOP_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-sSoulJointTable[0].x, -sSoulJointTable[0].y, -sSoulJointTable[0].z, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0B, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sSoulEmptyDL);
    SkelAnime_DrawFlexOpa(play, sSoulSkelAnime.skeleton, sSoulSkelAnime.jointTable, sSoulSkelAnime.dListCount, NULL,
                          NULL, NULL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterSoul(void) {
    static const SOHCustomItemRandomizer sSoulRandomizer = {
        .structSize = sizeof(SOHCustomItemRandomizer),
        .flags = SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT,
        .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
    };
    SOHCustomItemDefinition soul = Z64Items_Define(HIPLOOP_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Hiploops";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Hiploops%w!&Hiploops can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " HIPLOOP_SOUL_ITEM_KEY);
    }
}

static void RegisterHiploop(void) {
    SOHActorDefinition hiploop = { 0 };

    hiploop.structSize = sizeof(hiploop);
    hiploop.key = HIPLOOP_KEY;
    hiploop.description = "Hiploop";
    hiploop.category = ACTORCAT_ENEMY;
    hiploop.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
    hiploop.objectId = OBJECT_GAMEPLAY_KEEP;
    hiploop.instanceSize = sizeof(EnHiploop);
    hiploop.init = EnHiploop_Init;
    hiploop.destroy = EnHiploop_Destroy;
    hiploop.update = EnHiploop_Update;
    hiploop.draw = EnHiploop_Draw;
    hiploop.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    hiploop.enemyParams = HIPLOOP_TYPE_MASKED;
    hiploop.naviHint = "Masked Hiploop&%cIts mask shrugs off any blow. Pull it off with the %rhookshot%c or blast it "
                       "away, then strike!%w";
    hiploop.soulKey = HIPLOOP_KEY;
    hiploop.logicKill = Hiploop_CanKillInLogic;
    sHiploopActorId = sApi->RegisterActor(&hiploop);
    if (sHiploopActorId < 0) {
        sApi->Log("Could not register " HIPLOOP_KEY);
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterActor) || !SOH_MOD_API_HAS(sApi, LogicHasCapability)) {
        return;
    }
    RegisterSoul();
    RegisterHiploop();
}
