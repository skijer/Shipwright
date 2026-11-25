// Majora's Mask's ChuChu (En_Slime) as an Unbound custom enemy, drawn from mm.o2r. Params pick the colour.
// It stays dead (MM revives it, which would keep clear rooms shut), and ice freezes it in place instead of into
// MM's pushable ice block.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define CHUCHU_KEY "mm.chuchu"
#define CHUCHU_SOUL_ITEM_KEY "soul.mm.chuchu"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_SLIME "__OTR__objects/object_slime/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_XLU 0x2000
#define BGCHECK_UPDATE_WALL_FLOOR ((1 << 0) | (1 << 2))

#define MM_SE_EN_SLIME_JUMP 0x38B9
#define MM_SE_EN_SLIME_DAMAGE 0x38BA
#define MM_SE_EN_SLIME_BREAK 0x38BB
#define MM_SE_EN_SLIME_DEAD 0x38BE
#define MM_SE_EN_SLIME_DEFENCE 0x38BF

#define CHUCHU_NOTICE_RANGE 280.0f
#define CHUCHU_WANDER_RADIUS 120.0f
#define CHUCHU_LEASH_RADIUS 300.0f
#define CHUCHU_EYE_FRAMES 4

typedef enum { CHUCHU_TYPE_BLUE, CHUCHU_TYPE_GREEN, CHUCHU_TYPE_YELLOW, CHUCHU_TYPE_RED, CHUCHU_TYPE_MAX } ChuchuType;

typedef enum {
    CHUCHU_DMGEFF_NORMAL,
    CHUCHU_DMGEFF_STUN,
    CHUCHU_DMGEFF_FREEZE,
    CHUCHU_DMGEFF_LIGHT,
    CHUCHU_DMGEFF_HOOKSHOT = 0xD,
    CHUCHU_DMGEFF_BLUNT = 0xF
} ChuchuDamageEffect;

struct EnChuchu;

typedef void (*EnChuchuActionFunc)(struct EnChuchu*, PlayState*);

typedef struct EnChuchu {
    Actor actor;
    EnChuchuActionFunc actionFunc;
    u8 eyeIndex;
    bool isFrozen;
    s16 timer;
    s16 wanderYaw;
    Vec3f wobbleRot;
    ColliderCylinder collider;
} EnChuchu;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sChuchuBodyDL[] = OBJ_SLIME "gChuchuBodyDL";
static const ALIGN_ASSET(2) char sChuchuEyesDL[] = OBJ_SLIME "gChuchuEyesDL";
static const ALIGN_ASSET(2) char sChuchuEyeOpenTex[] = OBJ_SLIME "gChuchuEyeOpenTex";
static const ALIGN_ASSET(2) char sChuchuEyeHalfTex[] = OBJ_SLIME "gChuchuEyeHalfTex";
static const ALIGN_ASSET(2) char sChuchuEyeClosedTex[] = OBJ_SLIME "gChuchuEyeClosedTex";

static const char* const sEyeTextures[CHUCHU_EYE_FRAMES] = {
    sChuchuEyeOpenTex,
    sChuchuEyeHalfTex,
    sChuchuEyeClosedTex,
    sChuchuEyeHalfTex,
};

static const Color_RGBA8 sPrimColors[CHUCHU_TYPE_MAX] = {
    { 255, 255, 255, 255 },
    { 255, 255, 0, 255 },
    { 255, 255, 200, 255 },
    { 225, 200, 255, 255 },
};

static const Color_RGBA8 sEnvColors[CHUCHU_TYPE_MAX] = {
    { 140, 255, 195, 255 },
    { 50, 255, 0, 255 },
    { 255, 180, 0, 255 },
    { 255, 50, 155, 255 },
};

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x04 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_HARD,
      BUMP_ON | BUMP_HOOKABLE,
      OCELEM_ON },
    { 22, 35, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit sColChkInfoInit = { 1, 22, 35, 60 };

// MM entries multiply the weapon's own damage; OoT's are absolute. Hammers bounce off it like MM's Goron fists.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, CHUCHU_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, CHUCHU_DMGEFF_NORMAL),
    /* Slingshot     */ DMG_ENTRY(1, CHUCHU_DMGEFF_NORMAL),
    /* Explosive     */ DMG_ENTRY(2, CHUCHU_DMGEFF_NORMAL),
    /* Boomerang     */ DMG_ENTRY(1, CHUCHU_DMGEFF_NORMAL),
    /* Normal arrow  */ DMG_ENTRY(2, CHUCHU_DMGEFF_NORMAL),
    /* Hammer swing  */ DMG_ENTRY(0, CHUCHU_DMGEFF_BLUNT),
    /* Hookshot      */ DMG_ENTRY(0, CHUCHU_DMGEFF_HOOKSHOT),
    /* Kokiri sword  */ DMG_ENTRY(1, CHUCHU_DMGEFF_NORMAL),
    /* Master sword  */ DMG_ENTRY(2, CHUCHU_DMGEFF_NORMAL),
    /* Giant's Knife */ DMG_ENTRY(4, CHUCHU_DMGEFF_NORMAL),
    /* Fire arrow    */ DMG_ENTRY(4, CHUCHU_DMGEFF_NORMAL),
    /* Ice arrow     */ DMG_ENTRY(4, CHUCHU_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(4, CHUCHU_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, CHUCHU_DMGEFF_NORMAL),
    /* Unk arrow 2   */ DMG_ENTRY(2, CHUCHU_DMGEFF_NORMAL),
    /* Unk arrow 3   */ DMG_ENTRY(2, CHUCHU_DMGEFF_NORMAL),
    /* Fire magic    */ DMG_ENTRY(2, CHUCHU_DMGEFF_NORMAL),
    /* Ice magic     */ DMG_ENTRY(4, CHUCHU_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(4, CHUCHU_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, CHUCHU_DMGEFF_NORMAL),
    /* Mirror Ray    */ DMG_ENTRY(0, CHUCHU_DMGEFF_NORMAL),
    /* Kokiri spin   */ DMG_ENTRY(2, CHUCHU_DMGEFF_NORMAL),
    /* Giant spin    */ DMG_ENTRY(8, CHUCHU_DMGEFF_NORMAL),
    /* Master spin   */ DMG_ENTRY(4, CHUCHU_DMGEFF_NORMAL),
    /* Kokiri jump   */ DMG_ENTRY(2, CHUCHU_DMGEFF_NORMAL),
    /* Giant jump    */ DMG_ENTRY(8, CHUCHU_DMGEFF_NORMAL),
    /* Master jump   */ DMG_ENTRY(4, CHUCHU_DMGEFF_NORMAL),
    /* Unknown 1     */ DMG_ENTRY(0, CHUCHU_DMGEFF_NORMAL),
    /* Unblockable   */ DMG_ENTRY(0, CHUCHU_DMGEFF_NORMAL),
    /* Hammer jump   */ DMG_ENTRY(0, CHUCHU_DMGEFF_BLUNT),
    /* Unknown 2     */ DMG_ENTRY(0, CHUCHU_DMGEFF_NORMAL),
};

// MM ignores these while frozen: only a strong blow shatters the ice.
#define CHUCHU_ICE_PROOF_DMG                                                                                           \
    (DMG_DEKU_NUT | DMG_DEKU_STICK | DMG_SLINGSHOT | DMG_BOOMERANG | DMG_ARROW_NORMAL | DMG_HOOKSHOT | DMG_ARROW_ICE | \
     DMG_ARROW_LIGHT | DMG_MAGIC_ICE)

static Color_RGBA8 sBubblePrim = { 255, 255, 255, 255 };
static Color_RGBA8 sBubbleEnv = { 150, 150, 150, 0 };
static Vec3f sBubbleAccel = { 0.0f, -0.8f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

static void Chuchu_WaitForGround(EnChuchu* this, PlayState* play);
static void Chuchu_Idle(EnChuchu* this, PlayState* play);
static void Chuchu_Wander(EnChuchu* this, PlayState* play);
static void Chuchu_ReturnHome(EnChuchu* this, PlayState* play);
static void Chuchu_TurnToPlayer(EnChuchu* this, PlayState* play);
static void Chuchu_Jump(EnChuchu* this, PlayState* play);
static void Chuchu_Land(EnChuchu* this, PlayState* play);
static void Chuchu_Bounce(EnChuchu* this, PlayState* play);
static void Chuchu_Damaged(EnChuchu* this, PlayState* play);
static void Chuchu_Melt(EnChuchu* this, PlayState* play);
static void Chuchu_Stun(EnChuchu* this, PlayState* play);
static void Chuchu_SetupTurnToPlayer(EnChuchu* this);
static void Chuchu_SetupDamaged(EnChuchu* this, PlayState* play, bool isKnockedBack);

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

static void Chuchu_Blink(EnChuchu* this) {
    if (this->eyeIndex != 0) {
        this->eyeIndex = (this->eyeIndex + 1) % CHUCHU_EYE_FRAMES;
    } else if (Rand_ZeroOne() < 0.05f) {
        this->eyeIndex = 1;
    }
}

static bool Chuchu_HasNoticedPlayer(EnChuchu* this) {
    return this->actor.xzDistToPlayer < CHUCHU_NOTICE_RANGE && Actor_IsFacingPlayer(&this->actor, 0x5000);
}

static void Chuchu_Jiggle(EnChuchu* this, f32 strength) {
    this->actor.shape.rot.x = Rand_CenteredFloat(0x200) * strength;
    this->actor.shape.rot.z = Rand_CenteredFloat(0x200) * strength;
}

static void Chuchu_Freeze(EnChuchu* this) {
    this->isFrozen = true;
    this->collider.base.colType = COLTYPE_HIT3;
    this->timer = 80;
    this->actor.flags &= ~ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_XLU, 80);
}

static void Chuchu_Thaw(EnChuchu* this, PlayState* play) {
    if (!this->isFrozen) {
        return;
    }
    this->isFrozen = false;
    this->collider.base.colType = COLTYPE_NONE;
    this->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR;
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < 5; i++) {
        Vec3f pos = this->actor.world.pos;

        pos.x += Rand_CenteredFloat(30.0f);
        pos.y += Rand_ZeroFloat(30.0f);
        pos.z += Rand_CenteredFloat(30.0f);
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &pos, 150, 150, 150, 250, 235, 245, 255, 0.8f);
    }
}

static void Chuchu_SetupIdle(EnChuchu* this) {
    this->timer = 25;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Chuchu_Idle;
}

static void Chuchu_WaitForGround(EnChuchu* this, PlayState* play) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Chuchu_SetupIdle(this);
    }
}

static void Chuchu_SetupWander(EnChuchu* this) {
    this->timer = 120;
    if (Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > CHUCHU_WANDER_RADIUS) {
        this->wanderYaw = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
    } else if (Rand_ZeroOne() < 0.7f) {
        this->wanderYaw = (s32)Rand_CenteredFloat(0x4000) + this->actor.shape.rot.y;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Chuchu_Wander;
}

static void Chuchu_Idle(EnChuchu* this, PlayState* play) {
    this->timer--;
    Chuchu_Blink(this);

    f32 settle = sqrtf(this->timer) * 0.2f;
    f32 scaleXZ = (Math_SinF(this->timer * (2.0f * M_PI / 5.0f)) * (0.13f * settle) + 1.0f) * 0.01f;

    this->actor.scale.x = scaleXZ;
    this->actor.scale.z = scaleXZ;
    if (this->timer < 24) {
        this->actor.scale.y = (Math_CosF(this->timer * (2.0f * M_PI / 5.0f)) * (0.05f * settle) + 1.0f) * 0.01f;
    }
    Chuchu_Jiggle(this, settle);
    if (Chuchu_HasNoticedPlayer(this)) {
        Chuchu_SetupTurnToPlayer(this);
    } else if (this->timer == 0) {
        Chuchu_SetupWander(this);
    }
}

// Squash-and-stretch crawl: the body stretches forward on each wave of the cycle.
static void Chuchu_Crawl(EnChuchu* this, s16 turnStep) {
    f32 wave = fabsf(Math_SinF(this->timer * (M_PI / 24)));

    Math_ApproachS(&this->actor.shape.rot.y, this->wanderYaw, 4, turnStep);
    Math_StepToF(&this->actor.scale.z, (0.15f * wave + 1.0f) * 0.01f, 0.0002f);
    Math_StepToF(&this->actor.scale.x, (1.0f - 0.2f * wave) * 0.01f, 0.0002f);
    Math_StepToF(&this->actor.scale.y, (1.0f - 0.1f * wave) * 0.01f, 0.0002f);
    this->actor.speedXZ = 0.8f * wave + 0.2f;
    this->actor.shape.rot.x = 0x800 * wave;
    this->actor.world.rot.y = this->actor.shape.rot.y;
}

static void Chuchu_SetupReturnHome(EnChuchu* this) {
    this->timer = 24;
    this->wanderYaw = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Chuchu_ReturnHome;
}

static bool Chuchu_IsTooFarFromHome(EnChuchu* this) {
    return Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > CHUCHU_LEASH_RADIUS;
}

static void Chuchu_Wander(EnChuchu* this, PlayState* play) {
    Chuchu_Blink(this);
    this->timer--;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->wanderYaw = this->actor.wallYaw;
    } else if (Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > CHUCHU_WANDER_RADIUS) {
        this->wanderYaw = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
    }
    Chuchu_Crawl(this, 0x100);
    if (Chuchu_IsTooFarFromHome(this)) {
        Chuchu_SetupReturnHome(this);
    } else if (Chuchu_HasNoticedPlayer(this)) {
        Chuchu_SetupTurnToPlayer(this);
    } else if (this->timer == 0) {
        Chuchu_SetupIdle(this);
    }
}

static void Chuchu_ReturnHome(EnChuchu* this, PlayState* play) {
    Chuchu_Blink(this);
    this->timer--;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->wanderYaw = this->actor.wallYaw;
    } else {
        this->wanderYaw = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
    }
    Chuchu_Crawl(this, 0x400);
    if (this->timer == 0) {
        this->timer = 24;
    }
    if (Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) < CHUCHU_LEASH_RADIUS * 0.8f) {
        Chuchu_SetupIdle(this);
    }
}

static void Chuchu_SetupTurnToPlayer(EnChuchu* this) {
    this->actor.shape.rot.x = 0;
    this->actor.shape.rot.z = 0;
    this->timer = 8;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Chuchu_TurnToPlayer;
}

static void Chuchu_SetupJump(EnChuchu* this) {
    bool isFar = this->actor.xzDistToPlayer > 120.0f;

    this->actor.velocity.y = isFar ? 11.0f : 18.0f;
    this->actor.speedXZ = isFar ? 5.0f : 7.0f;
    this->actor.gravity = isFar ? -2.0f : -3.5f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->timer = 12;
    this->eyeIndex = 0;
    Math_StepToF(&this->actor.scale.x, 0.008f, 0.0025f);
    Math_StepToF(&this->actor.scale.y, 0.011f, 0.0025f);
    this->actor.scale.z = this->actor.scale.x;
    PlaySfx(&this->actor, MM_SE_EN_SLIME_JUMP, NA_SE_EN_STAL_JUMP);
    this->actionFunc = Chuchu_Jump;
}

// Squats down while turning, then leaps.
static void Chuchu_TurnToPlayer(EnChuchu* this, PlayState* play) {
    this->timer--;
    Chuchu_Blink(this);
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x1000);
    if (this->timer >= 0) {
        f32 squat = 8 - this->timer;
        f32 scaleXZ = (squat * 0.04f + 1.0f) * 0.01f;

        this->actor.scale.x = scaleXZ;
        this->actor.scale.y = (1.0f - squat * 0.05f) * 0.01f;
        this->actor.scale.z = scaleXZ;
    }
    if (this->timer < -2) {
        Chuchu_SetupJump(this);
    }
}

static void Chuchu_SetupLand(EnChuchu* this) {
    this->timer = 15;
    this->actor.scale.x = 0.0132f;
    this->actor.scale.y = 0.00744f;
    this->actor.scale.z = 0.0132f;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Chuchu_Land;
}

// Hitting Link bounces it back off him.
static void Chuchu_Jump(EnChuchu* this, PlayState* play) {
    this->timer--;
    if ((this->collider.base.atFlags & AT_HIT) && this->actor.speedXZ > 0.0f) {
        this->actor.speedXZ *= -1.2f;
        this->collider.base.atFlags &= ~AT_HIT;
    }
    Math_StepToF(&this->actor.scale.x, 0.008f, 0.0025f);
    Math_StepToF(&this->actor.scale.y, 0.011f, 0.0025f);
    this->actor.scale.z = this->actor.scale.x;
    if (this->actor.velocity.y < 0.0f && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        Chuchu_SetupLand(this);
    }
}

static void Chuchu_Land(EnChuchu* this, PlayState* play) {
    Chuchu_Blink(this);
    this->timer--;

    f32 wobble = (this->timer / 5 + 1) * 1.6f;
    f32 scaleXZ = (Math_CosF(this->timer * (2.0f * M_PI / 5.0f)) * (0.05f * wobble) + 1.0f) * 0.01f;

    this->actor.scale.x = scaleXZ;
    this->actor.scale.z = scaleXZ;
    if (this->timer < 15) {
        this->actor.scale.y = (1.0f - Math_SinF(this->timer * (2.0f * M_PI / 5.0f)) * (0.04f * wobble)) * 0.01f;
    }
    Chuchu_Jiggle(this, sqrtf(this->timer) * 0.2f);
    if (Chuchu_IsTooFarFromHome(this)) {
        Chuchu_SetupReturnHome(this);
    } else if (this->actor.xzDistToPlayer < CHUCHU_NOTICE_RANGE && this->timer < 12) {
        Chuchu_SetupTurnToPlayer(this);
    } else if (this->timer == 0) {
        Chuchu_SetupIdle(this);
    }
}

static s16 Chuchu_GetYawAwayFromAttacker(EnChuchu* this) {
    Actor* attacker = this->collider.base.ac;

    if (attacker == NULL) {
        return this->actor.yawTowardsPlayer + 0x8000;
    }
    return Actor_WorldYawTowardActor(&this->actor, attacker) + 0x8000;
}

static void Chuchu_SetupBounce(EnChuchu* this) {
    this->actor.speedXZ = 10.0f;
    this->actor.velocity.y = MIN(this->actor.velocity.y, 0.0f);
    this->collider.base.acFlags &= ~AC_ON;
    this->timer = 30;
    PlaySfx(&this->actor, MM_SE_EN_SLIME_DEFENCE, NA_SE_EN_DODO_M_GND);
    this->actor.world.rot.y = Chuchu_GetYawAwayFromAttacker(this);
    this->actionFunc = Chuchu_Bounce;
}

// Blunt hits only squash it and send it sliding.
static void Chuchu_Bounce(EnChuchu* this, PlayState* play) {
    Chuchu_Blink(this);
    this->timer--;
    Math_StepToF(&this->actor.speedXZ, 0.0f, 1.0f);

    f32 strength = sqrtf(this->timer);
    f32 scaleXZ = (Math_CosF(this->timer * (2.0f * M_PI / 5.0f)) * (0.08f * strength) + 1.0f) * 0.01f;

    this->actor.scale.x = scaleXZ;
    this->actor.scale.z = scaleXZ;
    this->actor.scale.y = (Math_SinF(this->timer * (2.0f * M_PI / 5.0f)) * (0.07f * strength) + 1.0f) * 0.01f;
    Chuchu_Jiggle(this, strength);
    if (this->timer == 15) {
        this->collider.base.acFlags |= AC_ON;
    }
    if (this->timer != 0) {
        return;
    }
    if (Chuchu_IsTooFarFromHome(this)) {
        Chuchu_SetupReturnHome(this);
    } else if (this->actor.xzDistToPlayer < CHUCHU_NOTICE_RANGE) {
        Chuchu_SetupTurnToPlayer(this);
    } else {
        Chuchu_SetupIdle(this);
    }
}

static void Chuchu_SpawnBubbles(EnChuchu* this, PlayState* play, Vec3f* origin, s32 count, bool isSpraying) {
    for (s32 i = 0; i < count; i++) {
        Vec3f pos = *origin;
        Vec3f velocity;

        if (isSpraying) {
            f32 push = Rand_ZeroOne() * 3.5f + 1.0f;

            pos.x += Rand_CenteredFloat(40.0f);
            pos.y += Rand_CenteredFloat(10.0f) + 20.0f;
            pos.z += Rand_CenteredFloat(40.0f);
            velocity.x = push * -Math_SinS(this->actor.world.rot.y);
            velocity.z = push * -Math_CosS(this->actor.world.rot.y);
        } else {
            velocity.x = (Rand_ZeroOne() < 0.5f ? -1 : 1) * (Rand_ZeroOne() * 2.5f + 2.0f);
            velocity.z = (Rand_ZeroOne() < 0.5f ? -1 : 1) * (Rand_ZeroOne() * 2.5f + 2.0f);
        }
        velocity.y = Rand_ZeroOne() * 6.0f + 2.0f;
        EffectSsDtBubble_SpawnCustomColor(play, &pos, &velocity, &sBubbleAccel, &sBubblePrim, &sBubbleEnv,
                                          Rand_S16Offset(40, 20), 20, 0);
    }
}

static void Chuchu_SetupDamaged(EnChuchu* this, PlayState* play, bool isKnockedBack) {
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.velocity.y = MIN(this->actor.velocity.y, 0.0f);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_XLU, 20);
    this->timer = 20;
    this->actor.speedXZ = 10.0f;
    if (isKnockedBack) {
        this->actor.world.rot.y = Chuchu_GetYawAwayFromAttacker(this);
    }
    this->eyeIndex = 0;
    Actor_SetScale(&this->actor, 0.01f);
    this->wobbleRot.x = Rand_ZeroOne() * (M_PI * 2.0f);
    this->wobbleRot.z = Rand_ZeroOne() * (M_PI * 2.0f);

    Vec3f splash = { this->actor.world.pos.x + Math_SinS(this->actor.world.rot.y) * 10.0f,
                     this->actor.world.pos.y + 20.0f,
                     this->actor.world.pos.z + Math_CosS(this->actor.world.rot.y) * 10.0f };
    Vec3f bubbles = { splash.x, this->actor.world.pos.y, splash.z };

    EffectSsGSplash_Spawn(play, &splash, NULL, NULL, 1, 550);
    Chuchu_SpawnBubbles(this, play, &bubbles, 10, true);
    if (this->actor.colChkInfo.health == 0) {
        PlaySfx(&this->actor, MM_SE_EN_SLIME_DEAD, NA_SE_EN_STAL_DEAD);
    } else {
        PlaySfx(&this->actor, MM_SE_EN_SLIME_DAMAGE, NA_SE_EN_STAL_DAMAGE);
    }
    this->actionFunc = Chuchu_Damaged;
}

static void Chuchu_SetupMelt(EnChuchu* this) {
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.velocity.y = MIN(this->actor.velocity.y, 0.0f);
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Chuchu_Melt;
}

static void Chuchu_Damaged(EnChuchu* this, PlayState* play) {
    this->timer--;
    Math_StepToF(&this->actor.speedXZ, 0.0f, 1.0f);
    if ((this->timer % 5) == 0) {
        this->wobbleRot.x = Rand_ZeroOne() * (M_PI * 2.0f);
        this->wobbleRot.z = Rand_ZeroOne() * (M_PI * 2.0f);
    }
    if (this->timer != 0) {
        return;
    }
    if (this->actor.colChkInfo.health == 0) {
        PlaySfx(&this->actor, MM_SE_EN_SLIME_BREAK, NA_SE_EN_STAL_DEAD);
        Chuchu_SetupMelt(this);
        return;
    }
    this->collider.base.acFlags |= AC_ON;
    if (this->actor.xzDistToPlayer < CHUCHU_NOTICE_RANGE) {
        Chuchu_SetupTurnToPlayer(this);
    } else {
        Chuchu_SetupIdle(this);
    }
}

static void Chuchu_DropColourItem(EnChuchu* this, PlayState* play) {
    if (this->actor.params == CHUCHU_TYPE_YELLOW) {
        Item_DropCollectible(play, &this->actor.world.pos, LINK_IS_ADULT ? ITEM00_ARROWS_MEDIUM : ITEM00_SEEDS);
    } else if (this->actor.params == CHUCHU_TYPE_GREEN) {
        Item_DropCollectible(play, &this->actor.world.pos, ITEM00_MAGIC_SMALL);
    } else if (this->actor.params == CHUCHU_TYPE_RED) {
        Item_DropCollectible(play, &this->actor.world.pos, ITEM00_HEART);
    }
}

// Shrinks into a puddle of bubbles and leaves its colour's drop.
static void Chuchu_Melt(EnChuchu* this, PlayState* play) {
    if (Math_StepToF(&this->actor.scale.x, 0.0f, 0.002f)) {
        Vec3f puddle = { this->actor.world.pos.x, this->actor.world.pos.y + 20.0f, this->actor.world.pos.z };

        EffectSsGSplash_Spawn(play, &puddle, NULL, NULL, 1, 800);
        Chuchu_SpawnBubbles(this, play, &puddle, 15, false);
        Chuchu_DropColourItem(this, play);
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
        Actor_Kill(&this->actor);
        return;
    }
    this->actor.scale.y = this->actor.scale.x;
    this->actor.scale.z = this->actor.scale.x;
}

static void Chuchu_SetupStun(EnChuchu* this) {
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = MIN(this->actor.velocity.y, 0.0f);
    this->actionFunc = Chuchu_Stun;
}

static void Chuchu_Stun(EnChuchu* this, PlayState* play) {
    this->timer--;
    if (this->timer != 0) {
        return;
    }
    Chuchu_Thaw(this, play);
    if (this->actor.colChkInfo.health == 0) {
        Chuchu_SetupDamaged(this, play, false);
    } else {
        this->actor.world.rot.y = this->actor.shape.rot.y;
        Chuchu_SetupWander(this);
    }
}

static void Chuchu_ReactToHit(EnChuchu* this, PlayState* play, u8 effect) {
    if (effect == CHUCHU_DMGEFF_BLUNT) {
        Chuchu_SetupBounce(this);
        return;
    }
    if (effect == CHUCHU_DMGEFF_STUN) {
        this->timer = 40;
        Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_XLU, 40);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
        Chuchu_SetupStun(this);
        return;
    }
    if (effect == CHUCHU_DMGEFF_HOOKSHOT) {
        return;
    }
    if (effect == CHUCHU_DMGEFF_FREEZE) {
        Chuchu_Freeze(this);
        if (this->actor.colChkInfo.health == 0) {
            this->timer = 3;
            this->collider.base.acFlags &= ~AC_ON;
        }
        Chuchu_SetupStun(this);
        return;
    }
    Chuchu_SetupDamaged(this, play, true);
}

static void Chuchu_CheckDamage(EnChuchu* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;

    ColliderInfo* hitBy = this->collider.info.acHitInfo;

    if (this->isFrozen && hitBy != NULL && (hitBy->toucher.dmgFlags & CHUCHU_ICE_PROOF_DMG)) {
        return;
    }
    Chuchu_Thaw(this, play);
    if (Actor_ApplyDamage(&this->actor) == 0) {
        Actor_SetDropFlag(&this->actor, &this->collider.info, false);
        Enemy_StartFinishingBlow(play, &this->actor);
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    }
    Chuchu_ReactToHit(this, play, this->actor.colChkInfo.damageEffect);
}

static void Chuchu_UpdateCollider(EnChuchu* this, PlayState* play) {
    Collider_UpdateCylinder(&this->actor, &this->collider);
    this->collider.dim.radius = sCylinderInit.dim.radius * (100.0f * this->actor.scale.z);
    this->collider.dim.height = sCylinderInit.dim.height * (100.0f * this->actor.scale.y);
    if (this->actionFunc == Chuchu_Jump) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.ocFlags1 & OC1_ON) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
}

static void EnChuchu_Init(Actor* thisx, PlayState* play) {
    EnChuchu* this = (EnChuchu*)thisx;

    this->actor.params = CLAMP(this->actor.params & 0xFF, CHUCHU_TYPE_BLUE, CHUCHU_TYPE_RED);
    this->actor.gravity = -2.0f;
    this->actor.targetArrowOffset = 6000.0f;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 38.0f);
    this->actor.scale.x = 0.008f;
    this->actor.scale.z = 0.008f;
    this->actor.scale.y = 0.011f;
    this->actionFunc = Chuchu_WaitForGround;
}

static void EnChuchu_Destroy(Actor* thisx, PlayState* play) {
    EnChuchu* this = (EnChuchu*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void EnChuchu_Update(Actor* thisx, PlayState* play) {
    EnChuchu* this = (EnChuchu*)thisx;

    Chuchu_CheckDamage(this, play);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    if (this->actor.scale.y > 0.0001f) {
        this->actor.targetArrowOffset = 60.0f / this->actor.scale.y;
    }
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 35.0f, 40.0f, BGCHECK_UPDATE_WALL_FLOOR);
    Chuchu_UpdateCollider(this, play);
    Actor_SetFocus(&this->actor, 15.0f);
}

// The damage wobble squashes the jelly along a random tilted axis.
static void Chuchu_ApplyDamageWobble(EnChuchu* this) {
    f32 phase = this->timer * (M_PI / 2.0f);

    Matrix_RotateX(this->wobbleRot.x, MTXMODE_APPLY);
    Matrix_RotateZ(this->wobbleRot.z, MTXMODE_APPLY);
    Matrix_Scale(1.0f - Math_SinF(phase) * 0.3f, Math_SinF(phase) * 0.3f + 1.0f, 1.0f - Math_CosF(phase) * 0.3f,
                 MTXMODE_APPLY);
    Matrix_RotateZ(-this->wobbleRot.z, MTXMODE_APPLY);
    Matrix_RotateX(-this->wobbleRot.x, MTXMODE_APPLY);
}

// MM's opaque Scene_SetRenderModeXlu table for segment 0x0C: gChuchuEyesDL calls entries 0 and 2.
static Gfx sRenderModeOpaDL[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

// Segment 0x0A scrolls the slime texture (MM's texture animation).
static void EnChuchu_Draw(Actor* thisx, PlayState* play) {
    EnChuchu* this = (EnChuchu*)thisx;
    Color_RGBA8 prim = sPrimColors[this->actor.params];
    Color_RGBA8 env = sEnvColors[this->actor.params];

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x0A,
               (uintptr_t)Gfx_TexScroll(play->state.gfxCtx, 0, -(s32)play->gameplayFrames, 32, 32));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 100, prim.r, prim.g, prim.b, prim.a);
    gDPSetEnvColor(POLY_XLU_DISP++, env.r, env.g, env.b, 255);
    if (this->actionFunc == Chuchu_Damaged) {
        Chuchu_ApplyDamageWobble(this);
    }
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sChuchuBodyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sEyeTextures[this->eyeIndex]);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 30, 70, 255);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sChuchuEyesDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's CanKillEnemy(ACTOR_EN_SLIME): a sword, the bow, Deku Sticks or a form attack.
static bool Chuchu_CanKillInLogic(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RG_FAIRY_BOW) ||
           sApi->LogicCanUseItem(RG_STICKS);
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

// The soul shows its enemy, as Majora's Mask souls do.
static void DrawSoul(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSoulFlame(play);
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sEyeTextures[0]);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 30, 70, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sChuchuEyesDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x0A,
               (uintptr_t)Gfx_TexScroll(play->state.gfxCtx, 0, -(s32)play->gameplayFrames, 32, 32));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 100, sPrimColors[0].r, sPrimColors[0].g, sPrimColors[0].b, sPrimColors[0].a);
    gDPSetEnvColor(POLY_XLU_DISP++, sEnvColors[0].r, sEnvColors[0].g, sEnvColors[0].b, 255);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sChuchuBodyDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterSoul(void) {
    static const SOHCustomItemRandomizer sSoulRandomizer = {
        .structSize = sizeof(SOHCustomItemRandomizer),
        .flags = SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT,
        .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
    };
    SOHCustomItemDefinition soul = Z64Items_Define(CHUCHU_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of ChuChus";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of ChuChus%w!&ChuChus can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " CHUCHU_SOUL_ITEM_KEY);
    }
}

static void RegisterChuchu(void) {
    SOHActorDefinition chuchu = { 0 };

    chuchu.structSize = sizeof(chuchu);
    chuchu.key = CHUCHU_KEY;
    chuchu.description = "ChuChu";
    chuchu.category = ACTORCAT_ENEMY;
    chuchu.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    chuchu.objectId = OBJECT_GAMEPLAY_KEEP;
    chuchu.instanceSize = sizeof(EnChuchu);
    chuchu.init = EnChuchu_Init;
    chuchu.destroy = EnChuchu_Destroy;
    chuchu.update = EnChuchu_Update;
    chuchu.draw = EnChuchu_Draw;
    chuchu.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    chuchu.enemyParams = CHUCHU_TYPE_RED;
    chuchu.naviHint = "ChuChu&%cA jelly blob. Blunt blows bounce off it; use your %rsword%c!%w";
    chuchu.soulKey = CHUCHU_KEY;
    chuchu.logicKill = Chuchu_CanKillInLogic;
    if (sApi->RegisterActor(&chuchu) < 0) {
        sApi->Log("Could not register " CHUCHU_KEY);
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
    RegisterChuchu();
}
