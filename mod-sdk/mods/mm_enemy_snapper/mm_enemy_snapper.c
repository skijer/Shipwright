// Majora's Mask's Snapper (En_Kame) as an Unbound custom enemy, drawn from mm.o2r.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define SNAPPER_KEY "mm.snapper"
#define SNAPPER_SOUL_ITEM_KEY "soul.mm.snapper"
#define GORON_MASK_KEY "nei.goron_form_mask"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_SNAPPER "__OTR__objects/object_tl/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_CEILING_FLOOR ((1 << 0) | (1 << 1) | (1 << 2))

#define MM_SE_EN_HIPLOOP_LAND 0x38DD
#define MM_SE_EN_PAMET_VOICE 0x38F3
#define MM_SE_EN_PAMET_ROLL 0x38F4
#define MM_SE_EN_PAMET_WALK 0x38F5
#define MM_SE_EN_PAMET_ROAR 0x38F6
#define MM_SE_EN_PAMET_WAKEUP 0x38F7
#define MM_SE_EN_PAMET_REVERSE 0x38F8
#define MM_SE_EN_PAMET_DAMAGE 0x38F9
#define MM_SE_EN_PAMET_DEAD 0x38FA
#define MM_SE_EN_PAMET_CUTTER_ON 0x392B
#define MM_SE_EN_PAMET_CUTTER_OFF 0x392D
#define MM_SE_EN_COMMON_EXTINCT_LEV 0x3A1F
#define MM_SE_EV_ICE_BROKEN 0x28CB
#define MM_SE_EN_COMMON_FREEZE 0x389E

#define SNAPPER_HEALTH 3
#define SNAPPER_NOTICE_RANGE 240.0f
#define SNAPPER_CHARGE_DISTANCE 360.0f
#define SNAPPER_SPIN_SPEED 0x3B00
#define SNAPPER_STRUGGLE_FRAMES 200
#define HAMMER_POUND_RANGE 200.0f

typedef enum {
    SNAPPER_LIMB_NONE,
    SNAPPER_LIMB_BODY,
    SNAPPER_LIMB_HEAD,
    SNAPPER_LIMB_JAW,
    SNAPPER_LIMB_EYES,
    SNAPPER_LIMB_FRONT_LEFT_LEG,
    SNAPPER_LIMB_FRONT_LEFT_FOOT,
    SNAPPER_LIMB_FRONT_RIGHT_LEG,
    SNAPPER_LIMB_FRONT_RIGHT_FOOT,
    SNAPPER_LIMB_BACK_LEFT_LEG,
    SNAPPER_LIMB_BACK_LEFT_FOOT,
    SNAPPER_LIMB_BACK_RIGHT_LEG,
    SNAPPER_LIMB_BACK_RIGHT_FOOT,
    SNAPPER_LIMB_MAX
} SnapperLimb;

typedef enum {
    SPIKED_SNAPPER_LIMB_NONE,
    SPIKED_SNAPPER_LIMB_BODY,
    SPIKED_SNAPPER_LIMB_SHELL,
    SPIKED_SNAPPER_LIMB_SPIKES,
    SPIKED_SNAPPER_LIMB_MAX
} SpikedSnapperLimb;

typedef enum {
    SNAPPER_DMGEFF_NONE,
    SNAPPER_DMGEFF_STUN,
    SNAPPER_DMGEFF_FIRE,
    SNAPPER_DMGEFF_FREEZE,
    SNAPPER_DMGEFF_LIGHT,
    SNAPPER_DMGEFF_HOOKSHOT = 0xD,
    SNAPPER_DMGEFF_FLIP_OVER = 0xF
} SnapperDamageEffect;

struct EnSnapper;

typedef void (*EnSnapperActionFunc)(struct EnSnapper*, PlayState*);

typedef struct EnSnapper {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[SNAPPER_LIMB_MAX];
    Vec3s morphTable[SNAPPER_LIMB_MAX];
    SkelAnime shellSkelAnime;
    Vec3s shellJointTable[SPIKED_SNAPPER_LIMB_MAX];
    Vec3s shellMorphTable[SPIKED_SNAPPER_LIMB_MAX];
    EnSnapperActionFunc actionFunc;
    u8 eyeIndex;
    bool isFrozen;
    bool isChargingAtPlayer;
    bool isFlippedUpsideDown;
    s16 timer;
    s16 targetSpin;
    s16 voiceTimer;
    s16 stunTimer;
    s16 targetRotY;
    s16 spin;
    f32 limbScaleXZ;
    f32 limbScaleY;
    f32 spikesScale;
    Vec3f targetPos;
    ColliderCylinder collider;
} EnSnapper;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sSnapperSkel[] = OBJ_SNAPPER "gSnapperSkel";
static const ALIGN_ASSET(2) char sSpikedSnapperSkel[] = OBJ_SNAPPER "gSpikedSnapperSkel";
static const ALIGN_ASSET(2) char sSnapperIdleAnim[] = OBJ_SNAPPER "gSnapperIdleAnim";
static const ALIGN_ASSET(2) char sSpikedSnapperIdleAnim[] = OBJ_SNAPPER "gSpikedSnapperIdleAnim";
static const ALIGN_ASSET(2) char sSnapperWalkAnim[] = OBJ_SNAPPER "gSnapperWalkAnim";
static const ALIGN_ASSET(2) char sSnapperRetreatIntoShellAnim[] = OBJ_SNAPPER "gSnapperRetreatIntoShellAnim";
static const ALIGN_ASSET(2) char sSnapperEmergeFromShellAnim[] = OBJ_SNAPPER "gSnapperEmergeFromShellAnim";
static const ALIGN_ASSET(2) char sSnapperBouncedUprightAnim[] = OBJ_SNAPPER "gSnapperBouncedUprightAnim";
static const ALIGN_ASSET(2) char sSnapperFlipOverAnim[] = OBJ_SNAPPER "gSnapperFlipOverAnim";
static const ALIGN_ASSET(2) char sSnapperWiggleLegsAnim[] = OBJ_SNAPPER "gSnapperWiggleLegsAnim";
static const ALIGN_ASSET(2) char sSnapperFailToFlipUprightAnim[] = OBJ_SNAPPER "gSnapperFailToFlipUprightAnim";
static const ALIGN_ASSET(2) char sSnapperFlipUprightAnim[] = OBJ_SNAPPER "gSnapperFlipUprightAnim";
static const ALIGN_ASSET(2) char sSnapperDamageAnim[] = OBJ_SNAPPER "gSnapperDamageAnim";
static const ALIGN_ASSET(2) char sSnapperDeathAnim[] = OBJ_SNAPPER "gSnapperDeathAnim";
static const ALIGN_ASSET(2) char sSnapperEyeOpenTex[] = OBJ_SNAPPER "gSnapperEyeOpenTex";
static const ALIGN_ASSET(2) char sSnapperEyeHalfTex[] = OBJ_SNAPPER "gSnapperEyeHalfTex";
static const ALIGN_ASSET(2) char sSnapperEyeClosedTex[] = OBJ_SNAPPER "gSnapperEyeClosedTex";

static const char* const sEyeTextures[] = {
    sSnapperEyeOpenTex,
    sSnapperEyeHalfTex,
    sSnapperEyeClosedTex,
    sSnapperEyeHalfTex,
};

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_HARD, AT_NONE | AT_TYPE_ENEMY, AC_ON | AC_HARD | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x04 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_ON | BUMP_HOOKABLE,
      OCELEM_ON },
    { 35, 40, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit sColChkInfoInit = { SNAPPER_HEALTH, 15, 30, 80 };

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, SNAPPER_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, SNAPPER_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, SNAPPER_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, SNAPPER_DMGEFF_FLIP_OVER),
    /* Boomerang     */ DMG_ENTRY(1, SNAPPER_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, SNAPPER_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, SNAPPER_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, SNAPPER_DMGEFF_HOOKSHOT),
    /* Kokiri sword  */ DMG_ENTRY(1, SNAPPER_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, SNAPPER_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, SNAPPER_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(4, SNAPPER_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, SNAPPER_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(4, SNAPPER_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, SNAPPER_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, SNAPPER_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, SNAPPER_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(2, SNAPPER_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(4, SNAPPER_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(4, SNAPPER_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, SNAPPER_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, SNAPPER_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(2, SNAPPER_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(8, SNAPPER_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(4, SNAPPER_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, SNAPPER_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, SNAPPER_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, SNAPPER_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, SNAPPER_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, SNAPPER_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(0, SNAPPER_DMGEFF_FLIP_OVER),
    /* Unknown 2     */ DMG_ENTRY(0, SNAPPER_DMGEFF_NONE),
};

// MM ignores these while frozen: only a strong blow shatters the ice.
#define SNAPPER_ICE_PROOF_DMG                                                                                          \
    (DMG_DEKU_NUT | DMG_DEKU_STICK | DMG_SLINGSHOT | DMG_BOOMERANG | DMG_ARROW_NORMAL | DMG_HOOKSHOT | DMG_ARROW_ICE | \
     DMG_ARROW_LIGHT | DMG_MAGIC_ICE)

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sDustVelocity = { 0.0f, 0.75f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

static void Snapper_Idle(EnSnapper* this, PlayState* play);
static void Snapper_Walk(EnSnapper* this, PlayState* play);
static void Snapper_RetreatIntoShell(EnSnapper* this, PlayState* play);
static void Snapper_PrepareToAttack(EnSnapper* this, PlayState* play);
static void Snapper_Attack(EnSnapper* this, PlayState* play);
static void Snapper_EmergeFromShell(EnSnapper* this, PlayState* play);
static void Snapper_Flip(EnSnapper* this, PlayState* play);
static void Snapper_Struggle(EnSnapper* this, PlayState* play);
static void Snapper_FlipUpright(EnSnapper* this, PlayState* play);
static void Snapper_Stunned(EnSnapper* this, PlayState* play);
static void Snapper_Damaged(EnSnapper* this, PlayState* play);
static void Snapper_Dead(EnSnapper* this, PlayState* play);
static void Snapper_BurstIntoFlames(EnSnapper* this, PlayState* play);
static void Snapper_SetupWalk(EnSnapper* this);
static void Snapper_SetupRetreatIntoShell(EnSnapper* this);
static void Snapper_SetupPrepareToAttack(EnSnapper* this);
static void Snapper_SetupEmergeFromShell(EnSnapper* this);
static void Snapper_SetupStruggle(EnSnapper* this);
static void EnSnapper_Draw(Actor* thisx, PlayState* play);
static void Snapper_DrawShell(Actor* thisx, PlayState* play);

// Falls back to the closest OoT sound when the mm_assets sound service is missing.
static void PlaySfx(Actor* actor, u16 mmSfx, u16 ootSfx) {
    if (!sIsMmSfxSearched && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
        sIsMmSfxSearched = true;
    }
    if (sPlayMmSfx != NULL && sPlayMmSfx(mmSfx, &actor->projectedPos)) {
        return;
    }
    Audio_PlayActorSound2(actor, ootSfx);
}

static void Snapper_Blink(EnSnapper* this) {
    if (this->eyeIndex != 0) {
        this->eyeIndex = (this->eyeIndex + 1) % ARRAY_COUNT(sEyeTextures);
    } else if (Rand_ZeroOne() < 0.05f) {
        this->eyeIndex = 1;
    }
}

static void Snapper_Freeze(EnSnapper* this) {
    this->isFrozen = true;
    this->collider.base.colType = COLTYPE_HIT3;
    this->stunTimer = 80;
    this->actor.flags &= ~ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 80);
}

static void Snapper_Thaw(EnSnapper* this, PlayState* play) {
    if (!this->isFrozen) {
        return;
    }
    this->isFrozen = false;
    this->collider.base.colType = COLTYPE_HIT6;
    this->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    PlaySfx(&this->actor, MM_SE_EV_ICE_BROKEN, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < 6; i++) {
        Vec3f pos = this->actor.world.pos;

        pos.x += Rand_CenteredFloat(50.0f);
        pos.y += Rand_ZeroFloat(30.0f);
        pos.z += Rand_CenteredFloat(50.0f);
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &pos, 150, 150, 150, 250, 235, 245, 255, 1.0f);
    }
}

static bool Snapper_HasNoticedPlayer(EnSnapper* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    return !(player->stateFlags1 & PLAYER_STATE1_ON_HORSE) && this->actor.xzDistToPlayer < SNAPPER_NOTICE_RANGE;
}

static void Snapper_SetupIdle(EnSnapper* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperIdleAnim, -5.0f);
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Snapper_Idle;
}

static void Snapper_Idle(EnSnapper* this, PlayState* play) {
    if (Animation_OnFrame(&this->skelAnime, 10.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_PAMET_VOICE, NA_SE_EN_DODO_M_CRY);
        this->voiceTimer = 40;
    }
    if (Snapper_HasNoticedPlayer(this, play)) {
        Snapper_SetupRetreatIntoShell(this);
    } else if (SkelAnime_Update(&this->skelAnime)) {
        Snapper_SetupWalk(this);
    }
}

static void Snapper_SetupWalk(EnSnapper* this) {
    AnimationHeader* walk = (AnimationHeader*)sSnapperWalkAnim;

    Animation_MorphToLoop(&this->skelAnime, walk, -5.0f);
    this->actor.speedXZ = 0.5f;
    this->timer = Animation_GetLastFrame(walk) * ((s32)Rand_ZeroFloat(5.0f) + 3);
    this->targetRotY = this->actor.shape.rot.y;
    this->collider.base.acFlags |= AC_HARD | AC_ON;
    this->collider.base.colType = COLTYPE_HARD;
    this->actionFunc = Snapper_Walk;
}

// Ambles around its home until Link comes close, then hides in its shell to attack.
static void Snapper_Walk(EnSnapper* this, PlayState* play) {
    if (Snapper_HasNoticedPlayer(this, play)) {
        Snapper_SetupRetreatIntoShell(this);
        return;
    }
    SkelAnime_Update(&this->skelAnime);
    if (this->targetRotY != this->actor.shape.rot.y) {
        Math_ScaledStepToS(&this->actor.shape.rot.y, this->targetRotY, 0x100);
        this->actor.world.rot.y = this->actor.shape.rot.y;
    } else if (Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > 40.0f) {
        this->targetRotY = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos) + ((s32)Rand_Next() >> 0x14);
    }
    this->timer--;
    if (this->timer == 0) {
        Snapper_SetupIdle(this);
    } else if (Animation_OnFrame(&this->skelAnime, 0.0f) || Animation_OnFrame(&this->skelAnime, 15.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_PAMET_WALK, NA_SE_EN_DODO_M_MOVE);
    }
}

static void Snapper_SetupRetreatIntoShell(EnSnapper* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperRetreatIntoShellAnim, -3.0f);
    this->timer = 0;
    this->limbScaleY = 1.0f;
    this->limbScaleXZ = 1.0f;
    this->actor.speedXZ = 0.0f;
    if (this->voiceTimer == 0) {
        PlaySfx(&this->actor, MM_SE_EN_PAMET_VOICE, NA_SE_EN_DODO_M_CRY);
    }
    this->actionFunc = Snapper_RetreatIntoShell;
}

static void Snapper_RetreatIntoShell(EnSnapper* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        bool isPlayerInReach = this->timer == 0 || this->actor.xzDistToPlayer < 120.0f;

        if (Snapper_HasNoticedPlayer(this, play) && isPlayerInReach) {
            Snapper_SetupPrepareToAttack(this);
        } else if (--this->timer == 0) {
            Snapper_SetupEmergeFromShell(this);
        }
        return;
    }
    f32 frame = this->skelAnime.curFrame;

    if (frame > 2.0f) {
        this->limbScaleY = 1.5f - (frame - 2.0f) * (7.0f / 30);
        this->limbScaleXZ = 1.5f - (frame - 2.0f) * (1.0f / 12);
    } else {
        this->limbScaleY = 0.25f * frame + 1.0f;
        this->limbScaleXZ = this->limbScaleY;
    }
}

// Speed follows the spin, so the shell winds up before it rolls home.
static void Snapper_UpdateAttackSpeed(EnSnapper* this) {
    this->actor.speedXZ = this->spin * (5.0f / 0x1D80);
    this->actor.shape.rot.z = this->spin * (13.0f / 118.0f);
}

static void Snapper_SpawnAttackEffects(EnSnapper* this, PlayState* play) {
    bool isRollingFast = (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && this->actor.speedXZ >= 3.0f;

    if (isRollingFast && (play->gameplayFrames % 2) == 0) {
        func_800286CC(play, &this->actor.world.pos, &sDustVelocity, &sZeroVec, 550, 100);
    }
    if (this->spin > 0x1200) {
        PlaySfx(&this->actor, MM_SE_EN_PAMET_ROLL - SFX_FLAG, NA_SE_EN_DODO_M_MOVE);
    }
}

// The attack charges 360 units at Link, then rolls back home; isChargingAtPlayer tells the two legs apart.
static void Snapper_SetupPrepareToAttack(EnSnapper* this) {
    if (this->actor.draw == EnSnapper_Draw) {
        this->actor.draw = Snapper_DrawShell;
        this->spin = SNAPPER_SPIN_SPEED;
        this->spikesScale = 0.5f;
        Snapper_UpdateAttackSpeed(this);
        this->timer = 15;
        this->actor.speedXZ = 0.0f;
        PlaySfx(&this->actor, MM_SE_EN_PAMET_CUTTER_ON, NA_SE_IT_SWORD_PICKOUT);
        this->isChargingAtPlayer = true;
    } else {
        this->actor.world.rot.y = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
        this->targetPos = this->actor.home.pos;
        this->isChargingAtPlayer = false;
        this->timer = 0;
    }
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    this->actionFunc = Snapper_PrepareToAttack;
}

static void Snapper_SetupAttack(EnSnapper* this) {
    this->targetSpin = -1;
    this->actionFunc = Snapper_Attack;
}

static void Snapper_PrepareToAttack(EnSnapper* this, PlayState* play) {
    if (this->timer == 15) {
        this->spikesScale += 0.2f;
        if (this->spikesScale > 1.1f) {
            this->timer--;
            this->collider.base.atFlags |= AT_ON;
            this->spikesScale = 1.0f;
        }
    } else if (this->timer > 0) {
        this->timer--;
    } else if (Math_ScaledStepToS(&this->spin, SNAPPER_SPIN_SPEED, (s32)(this->spin * 0.09f) + 45)) {
        if (this->isChargingAtPlayer) {
            this->actor.world.rot.y = this->actor.yawTowardsPlayer;
            this->targetPos.x = Math_SinS(this->actor.world.rot.y) * SNAPPER_CHARGE_DISTANCE + this->actor.world.pos.x;
            this->targetPos.z = Math_CosS(this->actor.world.rot.y) * SNAPPER_CHARGE_DISTANCE + this->actor.world.pos.z;
        }
        Snapper_SetupAttack(this);
    }
    this->actor.shape.rot.y += this->spin;
    if (!this->isChargingAtPlayer || this->actionFunc != Snapper_PrepareToAttack) {
        Snapper_UpdateAttackSpeed(this);
    }
    Snapper_SpawnAttackEffects(this, play);
}

static bool Snapper_HasReachedTarget(EnSnapper* this) {
    s16 offTarget = Actor_WorldYawTowardPoint(&this->actor, &this->targetPos) - this->actor.world.rot.y;

    return (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) || ABS(offTarget) > 0x3000 ||
           Actor_WorldDistXZToPoint(&this->actor, &this->targetPos) < 50.0f;
}

static void Snapper_Attack(EnSnapper* this, PlayState* play) {
    this->actor.shape.rot.y += this->spin;
    Snapper_SpawnAttackEffects(this, play);
    if (this->targetSpin == -1) {
        if (Snapper_HasReachedTarget(this)) {
            this->targetSpin = this->isChargingAtPlayer ? 0x300 : 0;
        }
        return;
    }
    if (Math_ScaledStepToS(&this->spin, this->targetSpin, (s32)(this->spin * 0.09f) + 45)) {
        if (this->targetSpin != 0) {
            Snapper_SetupPrepareToAttack(this);
        } else {
            if (this->spikesScale >= 1.0f) {
                PlaySfx(&this->actor, MM_SE_EN_PAMET_CUTTER_OFF, NA_SE_IT_SWORD_PUTAWAY);
            }
            this->spikesScale -= 0.1f;
            this->collider.base.atFlags &= ~AT_ON;
            if (this->spikesScale < 0.5f) {
                this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
                Snapper_SetupEmergeFromShell(this);
            }
        }
    }
    Snapper_UpdateAttackSpeed(this);
}

static void Snapper_SetupEmergeFromShell(EnSnapper* this) {
    this->actor.draw = EnSnapper_Draw;
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperEmergeFromShellAnim, -3.0f);
    this->actor.speedXZ = 0.0f;
    this->limbScaleY = 0.1f;
    this->limbScaleXZ = 1.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Snapper_EmergeFromShell;
}

static void Snapper_EmergeFromShell(EnSnapper* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        Snapper_SetupWalk(this);
        return;
    }
    f32 frame = this->skelAnime.curFrame;

    if (frame > 7.0f) {
        this->limbScaleY = 1.5f - (frame - 7.0f) * (1.0f / 6);
        this->limbScaleXZ = this->limbScaleY;
    } else {
        this->limbScaleY = 0.2f * frame + 0.1f;
        this->limbScaleXZ = (1.0f / 14) * frame + 1.0f;
    }
}

// A second flip while it struggles on its back bounces it upright again.
static void Snapper_SetupFlip(EnSnapper* this) {
    if (this->actionFunc == Snapper_Struggle) {
        Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperBouncedUprightAnim, -3.0f);
        this->isFlippedUpsideDown = false;
    } else {
        Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperFlipOverAnim, -3.0f);
        this->isFlippedUpsideDown = true;
    }
    this->actor.draw = EnSnapper_Draw;
    this->actor.speedXZ = 0.0f;
    this->collider.base.acFlags &= ~AC_ON;
    this->collider.base.atFlags &= ~(AT_ON | AT_BOUNCED | AT_HIT);
    this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    this->actor.shape.rot.z = 0;
    PlaySfx(&this->actor, MM_SE_EN_PAMET_REVERSE, NA_SE_EN_DODO_M_DOWN);
    this->actionFunc = Snapper_Flip;
}

static void Snapper_Flip(EnSnapper* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (!this->isFlippedUpsideDown) {
        Snapper_SetupWalk(this);
        return;
    }
    this->timer = SNAPPER_STRUGGLE_FRAMES;
    Snapper_SetupStruggle(this);
}

static void Snapper_SetupStruggle(EnSnapper* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperWiggleLegsAnim, -3.0f);
    this->collider.base.acFlags |= AC_ON;
    this->collider.base.acFlags &= ~AC_HARD;
    this->collider.base.colType = COLTYPE_HIT6;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Snapper_Struggle;
}

static void Snapper_SetupFlipUpright(EnSnapper* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperFlipUprightAnim, -3.0f);
    PlaySfx(&this->actor, MM_SE_EN_PAMET_WAKEUP, NA_SE_EN_DODO_M_UP);
    this->actionFunc = Snapper_FlipUpright;
}

static void Snapper_Struggle(EnSnapper* this, PlayState* play) {
    if (this->timer <= 0) {
        Snapper_SetupFlipUpright(this);
        return;
    }
    this->timer--;
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (Rand_ZeroOne() > 0.5f) {
        Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperWiggleLegsAnim);
    } else {
        Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperFailToFlipUprightAnim);
        PlaySfx(&this->actor, MM_SE_EN_PAMET_ROAR, NA_SE_EN_DODO_M_CRY);
    }
}

static void Snapper_FlipUpright(EnSnapper* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        Snapper_SetupWalk(this);
    } else if (this->skelAnime.curFrame >= 10.0f) {
        this->collider.base.acFlags &= ~AC_ON;
    }
}

static void Snapper_FaceAwayFromAttacker(EnSnapper* this) {
    if (this->collider.base.ac != NULL) {
        this->actor.world.rot.y = Actor_WorldYawTowardActor(this->collider.base.ac, &this->actor);
    }
}

static void Snapper_SetupStunned(EnSnapper* this) {
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = MIN(this->actor.velocity.y, 0.0f);
    Snapper_FaceAwayFromAttacker(this);
    this->actionFunc = Snapper_Stunned;
}

static void Snapper_SetupDead(EnSnapper* this, PlayState* play, bool isFinishingBlow);

static void Snapper_Stunned(EnSnapper* this, PlayState* play) {
    if (this->stunTimer != 0) {
        this->stunTimer--;
    }
    if (this->stunTimer != 0) {
        return;
    }
    Snapper_Thaw(this, play);
    if (this->actor.colChkInfo.health == 0) {
        Snapper_SetupDead(this, play, false);
    } else {
        this->actor.world.rot.y = this->actor.shape.rot.y;
        Snapper_SetupStruggle(this);
    }
}

static void Snapper_SetupDamaged(EnSnapper* this) {
    AnimationHeader* damage = (AnimationHeader*)sSnapperDamageAnim;
    s16 endFrame = Animation_GetLastFrame(damage);

    Animation_Change(&this->skelAnime, damage, 1.0f, 0.0f, endFrame, ANIMMODE_ONCE, -3.0f);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, endFrame);
    PlaySfx(&this->actor, MM_SE_EN_PAMET_DAMAGE, NA_SE_EN_DODO_M_DOWN);
    this->collider.base.acFlags &= ~AC_ON;
    this->actionFunc = Snapper_Damaged;
}

static void Snapper_Damaged(EnSnapper* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        Snapper_SetupStruggle(this);
    }
}

static void Snapper_SetupDead(EnSnapper* this, PlayState* play, bool isFinishingBlow) {
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sSnapperDeathAnim);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 20);
    this->collider.base.acFlags &= ~AC_ON;
    this->collider.base.atFlags &= ~(AT_ON | AT_HIT);
    this->actor.velocity.y = 15.0f;
    this->actor.speedXZ = 1.5f;
    if (isFinishingBlow) {
        Enemy_StartFinishingBlow(play, &this->actor);
        if (this->actor.draw == Snapper_DrawShell) {
            this->actor.draw = EnSnapper_Draw;
        } else {
            Snapper_FaceAwayFromAttacker(this);
        }
    }
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    PlaySfx(&this->actor, MM_SE_EN_PAMET_DEAD, NA_SE_EN_DODO_M_DEAD);
    this->actionFunc = Snapper_Dead;
}

static void Snapper_SetupBurstIntoFlames(EnSnapper* this) {
    this->timer = 20;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Snapper_BurstIntoFlames;
}

static void Snapper_Dead(EnSnapper* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && this->actor.velocity.y < 0.0f) {
        PlaySfx(&this->actor, MM_SE_EN_HIPLOOP_LAND, NA_SE_EN_DODO_M_GND);
        Snapper_SetupBurstIntoFlames(this);
        return;
    }
    this->actor.colorFilterTimer = 100;
    this->actor.shape.rot.x += Rand_S16Offset(0x700, 0x1400);
    this->actor.shape.rot.y += (s16)Rand_ZeroFloat(0x1400);
    this->actor.shape.rot.z += Rand_S16Offset(0x700, 0x1400);
}

static void Snapper_Vanish(EnSnapper* this, PlayState* play) {
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x60);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

// Shrinks away in blue flames once it lands.
static void Snapper_BurstIntoFlames(EnSnapper* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->actor.colorFilterTimer = 100;
    if (this->timer > 0) {
        this->timer--;
        return;
    }
    PlaySfx(&this->actor, MM_SE_EN_COMMON_EXTINCT_LEV - SFX_FLAG, NA_SE_EN_EXTINCT);
    this->actor.scale.x -= 0.001f;
    if (this->actor.scale.x <= 0.0f) {
        Snapper_Vanish(this, play);
        return;
    }
    this->actor.scale.y = this->actor.scale.x;
    this->actor.scale.z = this->actor.scale.x;

    Vec3f flamePos = { Rand_CenteredFloat(40.0f) + this->actor.world.pos.x, this->actor.world.pos.y + 15.0f,
                       Rand_CenteredFloat(40.0f) + this->actor.world.pos.z };

    EffectSsDeadDb_Spawn(play, &flamePos, &sZeroVec, &sZeroVec, 100, 0, 255, 255, 255, 255, 0, 0, 255, 1, 9, false);
}

static void Snapper_ApplyHit(EnSnapper* this, PlayState* play, u8 effect) {
    if (effect == SNAPPER_DMGEFF_STUN) {
        this->stunTimer = 40;
        Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 40);
        PlaySfx(&this->actor, MM_SE_EN_COMMON_FREEZE, NA_SE_EN_GOMA_JR_FREEZE);
        Snapper_SetupStunned(this);
        return;
    }
    if (effect == SNAPPER_DMGEFF_FREEZE) {
        Snapper_Freeze(this);
        if (Actor_ApplyDamage(&this->actor) == 0) {
            this->stunTimer = 3;
            this->collider.base.acFlags &= ~AC_ON;
        }
        Snapper_SetupStunned(this);
        return;
    }
    if (Actor_ApplyDamage(&this->actor) == 0) {
        Snapper_SetupDead(this, play, true);
    } else {
        Snapper_SetupDamaged(this);
    }
}

static bool Snapper_IsHardShell(EnSnapper* this) {
    return (this->collider.base.acFlags & AC_HARD) != 0;
}

static void Snapper_CheckDamage(EnSnapper* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Actor_SetDropFlag(&this->actor, &this->collider.info, false);

    ColliderInfo* hitBy = this->collider.info.acHitInfo;

    if (this->isFrozen && hitBy != NULL && (hitBy->toucher.dmgFlags & SNAPPER_ICE_PROOF_DMG)) {
        return;
    }
    Snapper_Thaw(this, play);

    u8 effect = this->actor.colChkInfo.damageEffect;

    if (effect == SNAPPER_DMGEFF_HOOKSHOT) {
        return;
    }
    if (effect == SNAPPER_DMGEFF_FLIP_OVER) {
        if (Snapper_IsHardShell(this)) {
            Snapper_SetupFlip(this);
        } else {
            Snapper_ApplyHit(this, play, SNAPPER_DMGEFF_NONE);
        }
    } else if (this->actionFunc == Snapper_Idle || this->actionFunc == Snapper_Walk) {
        Snapper_SetupRetreatIntoShell(this);
        this->timer = 21;
    } else if (!Snapper_IsHardShell(this)) {
        Snapper_ApplyHit(this, play, effect);
    }
}

// OoT's hammer shockwave flips it like MM's Goron pound.
static void Snapper_CheckHammerPound(EnSnapper* this, PlayState* play) {
    bool isPounded = play->actorCtx.unk_02 != 0 && this->actor.xyzDistToPlayerSq < SQ(HAMMER_POUND_RANGE);

    if (isPounded && (this->collider.base.acFlags & AC_ON)) {
        Snapper_Thaw(this, play);
        Snapper_SetupFlip(this);
    }
}

// A spinning shell that bounces off a shield heads straight back home.
static void Snapper_CheckBounced(EnSnapper* this) {
    if (!(this->collider.base.atFlags & AT_HIT) || !(this->collider.base.atFlags & AT_BOUNCED)) {
        return;
    }
    this->collider.base.atFlags &= ~(AT_BOUNCED | AT_HIT);
    Snapper_SetupPrepareToAttack(this);
    if (Actor_WorldDistXZToPoint(&this->actor, &this->targetPos) < 50.0f) {
        this->collider.base.atFlags &= ~AT_ON;
    }
    this->spin = SNAPPER_SPIN_SPEED;
    Snapper_UpdateAttackSpeed(this);
}

static void EnSnapper_Init(Actor* thisx, PlayState* play) {
    EnSnapper* this = (EnSnapper*)thisx;

    this->actor.gravity = -1.0f;
    this->actor.targetArrowOffset = 3500.0f;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSnapperSkel, (AnimationHeader*)sSnapperIdleAnim,
                       this->jointTable, this->morphTable, SNAPPER_LIMB_MAX);
    SkelAnime_InitFlex(play, &this->shellSkelAnime, (FlexSkeletonHeader*)sSpikedSnapperSkel,
                       (AnimationHeader*)sSpikedSnapperIdleAnim, this->shellJointTable, this->shellMorphTable,
                       SPIKED_SNAPPER_LIMB_MAX);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 55.0f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    this->spikesScale = 1.0f;
    Snapper_SetupIdle(this);
}

static void EnSnapper_Destroy(Actor* thisx, PlayState* play) {
    EnSnapper* this = (EnSnapper*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void EnSnapper_Update(Actor* thisx, PlayState* play) {
    EnSnapper* this = (EnSnapper*)thisx;

    Snapper_Blink(this);
    DECR(this->voiceTimer);
    Snapper_CheckDamage(this, play);
    Snapper_CheckHammerPound(this, play);
    Snapper_CheckBounced(this);
    this->actionFunc(this, play);
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 40.0f, 60.0f, 40.0f, BGCHECK_UPDATE_WALL_CEILING_FLOOR);
    Actor_SetFocus(&this->actor, 25.0f);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    if (this->collider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static bool IsSnapperLeg(s32 limbIndex) {
    return limbIndex == SNAPPER_LIMB_BACK_RIGHT_LEG || limbIndex == SNAPPER_LIMB_BACK_LEFT_LEG ||
           limbIndex == SNAPPER_LIMB_FRONT_RIGHT_LEG || limbIndex == SNAPPER_LIMB_FRONT_LEFT_LEG;
}

static s32 Snapper_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnSnapper* this = (EnSnapper*)thisx;
    bool isShellChanging = this->actionFunc == Snapper_RetreatIntoShell || this->actionFunc == Snapper_EmergeFromShell;

    if (!isShellChanging) {
        return false;
    }
    if (limbIndex == SNAPPER_LIMB_HEAD) {
        Matrix_Scale(this->limbScaleXZ, this->limbScaleY, this->limbScaleXZ, MTXMODE_APPLY);
    } else if (IsSnapperLeg(limbIndex)) {
        Matrix_Scale(this->limbScaleXZ, this->limbScaleY, this->limbScaleY, MTXMODE_APPLY);
    }
    return false;
}

static void EnSnapper_Draw(Actor* thisx, PlayState* play) {
    EnSnapper* this = (EnSnapper*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyeTextures[this->eyeIndex]);
    CLOSE_DISPS(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          Snapper_OverrideLimbDraw, NULL, this);
}

static s32 Snapper_OverrideShellLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                         void* thisx) {
    EnSnapper* this = (EnSnapper*)thisx;

    if (limbIndex == SPIKED_SNAPPER_LIMB_BODY) {
        pos->y -= 700.0f;
    }
    if (this->spikesScale != 1.0f && limbIndex == SPIKED_SNAPPER_LIMB_SPIKES) {
        Matrix_Scale(1.0f, this->spikesScale, this->spikesScale, MTXMODE_APPLY);
    }
    return false;
}

static void Snapper_DrawShell(Actor* thisx, PlayState* play) {
    EnSnapper* this = (EnSnapper*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->shellSkelAnime.skeleton, this->shellSkelAnime.jointTable,
                          this->shellSkelAnime.dListCount, Snapper_OverrideShellLimbDraw, NULL, this);
}

// MM's CanKillEnemy(ACTOR_EN_KAME): explosives or the Goron form flip it; the hammer pound does the same here.
static bool Snapper_CanKillInLogic(void* userData) {
    if (sApi->LogicHasCapability("HasExplosives") || sApi->LogicCanUseItem(RG_MEGATON_HAMMER)) {
        return true;
    }
    int32_t goronMask = sApi->GetRandoItem(GORON_MASK_KEY);

    return goronMask >= 0 && sApi->LogicHasItem(goronMask);
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
    static Vec3s sSoulJointTable[SNAPPER_LIMB_MAX];
    static Vec3s sSoulMorphTable[SNAPPER_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sSnapperSkel, (AnimationHeader*)sSnapperIdleAnim,
                           sSoulJointTable, sSoulMorphTable, SNAPPER_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-sSoulJointTable[0].x, -sSoulJointTable[0].y, -sSoulJointTable[0].z, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyeTextures[0]);
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
    SOHCustomItemDefinition soul = Z64Items_Define(SNAPPER_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Snappers";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Snappers%w!&Snappers can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " SNAPPER_SOUL_ITEM_KEY);
    }
}

static void RegisterSnapper(void) {
    SOHActorDefinition snapper = { 0 };

    snapper.structSize = sizeof(snapper);
    snapper.key = SNAPPER_KEY;
    snapper.description = "Snapper";
    snapper.category = ACTORCAT_ENEMY;
    snapper.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    snapper.objectId = OBJECT_GAMEPLAY_KEEP;
    snapper.instanceSize = sizeof(EnSnapper);
    snapper.init = EnSnapper_Init;
    snapper.destroy = EnSnapper_Destroy;
    snapper.update = EnSnapper_Update;
    snapper.draw = EnSnapper_Draw;
    snapper.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    snapper.naviHint = "Snapper&%cIts shell is too hard! Flip it over with %rexplosives%c or a %rhammer pound%c, "
                       "then strike its belly.%w";
    snapper.soulKey = SNAPPER_KEY;
    snapper.logicKill = Snapper_CanKillInLogic;
    if (sApi->RegisterActor(&snapper) < 0) {
        sApi->Log("Could not register " SNAPPER_KEY);
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
    RegisterSnapper();
}
