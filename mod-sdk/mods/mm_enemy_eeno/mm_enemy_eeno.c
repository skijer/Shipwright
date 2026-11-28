// Majora's Mask's small Eeno (En_Snowman) as an Unbound custom enemy, drawn from mm.o2r.
// The large Eeno that splits into three and recombines is left out: it needs its trio placed together.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define EENO_KEY "mm.eeno"
#define EENO_SOUL_ITEM_KEY "soul.mm.eeno"
#define EENO_SNOWBALL_KEY "mm.eeno.snowball"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_SNOWMAN "__OTR__objects/object_snowman/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_FLOOR ((1 << 0) | (1 << 2))
#define BGCHECK_UPDATE_WALL_CEILING_FLOOR ((1 << 0) | (1 << 1) | (1 << 2))

#define MM_SE_EV_SMALL_SNOWBALL_BROKEN 0x28FD
#define MM_SE_EN_YMAJIN_MINI_HOLD 0x3967
#define MM_SE_EN_YMAJIN_MINI_THROW 0x3968
#define MM_SE_EN_YMAJIN_MINI_MOVE 0x396A
#define MM_SE_EN_YMAJIN_SURFACE 0x396B
#define MM_SE_EN_YMAJIN_HIDE 0x396C
#define MM_SE_EN_YMAJIN_DEAD_BREAK 0x396F
#define MM_SE_EN_YMAJIN_MINI_DAMAGE 0x39F9
#define MM_SE_EN_COMMON_FREEZE 0x389E

#define EENO_HEALTH 2
#define EENO_ATTACK_RANGE 240.0f
#define EENO_ROAM_RADIUS 200.0f
#define EENO_SNOWBALLS_PER_VOLLEY 3
#define EENO_HOLD_FRAME 6.0f
#define EENO_THROW_FRAME 15.0f

typedef enum {
    EENO_LIMB_NONE,
    EENO_LIMB_ROOT,
    EENO_LIMB_BODY_TOP,
    EENO_LIMB_LEFT_EYE,
    EENO_LIMB_RIGHT_EYE,
    EENO_LIMB_LEFT_UPPER_ARM,
    EENO_LIMB_LEFT_FOREARM,
    EENO_LIMB_LEFT_HAND,
    EENO_LIMB_RIGHT_UPPER_ARM,
    EENO_LIMB_RIGHT_FOREARM,
    EENO_LIMB_RIGHT_HAND,
    EENO_LIMB_BODY_BOTTOM,
    EENO_LIMB_MAX
} EenoLimb;

#define EENO_SNOW_PILE_LIMB_MAX 3

typedef enum { EENO_DMGEFF_NONE, EENO_DMGEFF_STUN, EENO_DMGEFF_MELT, EENO_DMGEFF_LIGHT = 0x4 } EenoDamageEffect;

struct EnEeno;

typedef void (*EnEenoActionFunc)(struct EnEeno*, PlayState*);

typedef struct EnEeno {
    Actor actor;
    SkelAnime skelAnime;
    SkelAnime snowPileSkelAnime;
    Vec3s jointTable[EENO_LIMB_MAX];
    Vec3s morphTable[EENO_LIMB_MAX];
    Vec3s snowPileJointTable[EENO_SNOW_PILE_LIMB_MAX];
    Vec3s snowPileMorphTable[EENO_SNOW_PILE_LIMB_MAX];
    EnEenoActionFunc actionFunc;
    EnEenoActionFunc actionBeforeStun;
    bool isHoldingSnowball;
    s16 timer;
    s16 snowballsLeft;
    s16 roamYaw;
    Vec3f snowballPos;
    ColliderCylinder collider;
} EnEeno;

typedef struct EnEenoSnowball {
    Actor actor;
    s16 armTimer;
    ColliderCylinder collider;
} EnEenoSnowball;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sEenoSkel[] = OBJ_SNOWMAN "gEenoSkel";
static const ALIGN_ASSET(2) char sEenoSnowPileSkel[] = OBJ_SNOWMAN "gEenoSnowPileSkel";
static const ALIGN_ASSET(2) char sEenoEmergeAnim[] = OBJ_SNOWMAN "gEenoEmergeAnim";
static const ALIGN_ASSET(2) char sEenoSnowPileMoveAnim[] = OBJ_SNOWMAN "gEenoSnowPileMoveAnim";
static const ALIGN_ASSET(2) char sEenoSmallSnowballCreateAnim[] = OBJ_SNOWMAN "gEenoSmallSnowballCreateAnim";
static const ALIGN_ASSET(2) char sEenoSmallSnowballThrowAnim[] = OBJ_SNOWMAN "gEenoSmallSnowballThrowAnim";
static const ALIGN_ASSET(2) char sEenoIdleAnim[] = OBJ_SNOWMAN "gEenoIdleAnim";
static const ALIGN_ASSET(2) char sEenoDamageAnim[] = OBJ_SNOWMAN "gEenoDamageAnim";
static const ALIGN_ASSET(2) char sEenoSmallSnowballDL[] = OBJ_SNOWMAN "gEenoSmallSnowballDL";
static const ALIGN_ASSET(2) char sEenoSnowballFragment1DL[] = OBJ_SNOWMAN "gEenoSnowballFragment1DL";
static const ALIGN_ASSET(2) char sEenoSnowballFragment2DL[] = OBJ_SNOWMAN "gEenoSnowballFragment2DL";
static const ALIGN_ASSET(2) char sEenoSnowballFragment3DL[] = OBJ_SNOWMAN "gEenoSnowballFragment3DL";

static const char* const sSnowballFragmentDLs[] = {
    sEenoSnowballFragment1DL,
    sEenoSnowballFragment2DL,
    sEenoSnowballFragment3DL,
};

static ColliderCylinderInit sEenoCylinderInit = {
    { COLTYPE_HIT4, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0x00000000, HIT_SPECIAL_EFFECT_NONE, 0x00 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_ON | BUMP_HOOKABLE,
      OCELEM_ON },
    { 60, 80, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sSnowballCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_NONE | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x04 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NONE,
      BUMP_ON,
      OCELEM_ON },
    { 8, 12, -6, { 0, 0, 0 } },
};

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, EENO_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, EENO_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, EENO_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(4, EENO_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Kokiri sword  */ DMG_ENTRY(1, EENO_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, EENO_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(4, EENO_DMGEFF_MELT),
    /* Ice arrow     */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Light arrow   */ DMG_ENTRY(4, EENO_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(2, EENO_DMGEFF_MELT),
    /* Ice magic     */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Light magic   */ DMG_ENTRY(4, EENO_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, EENO_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, EENO_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(8, EENO_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(4, EENO_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, EENO_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, EENO_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, EENO_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, EENO_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, EENO_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, EENO_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, EENO_DMGEFF_NONE),
};

static CollisionCheckInfoInit sColChkInfoInit = { EENO_HEALTH, 60, 80, 150 };

static Color_RGBA8 sDustPrim = { 250, 250, 250, 255 };
static Color_RGBA8 sDustEnv = { 180, 180, 180, 255 };
static Vec3f sDustVelocity = { 0.0f, 1.5f, 0.0f };
static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;
static s16 sSnowballActorId = -1;

static void Eeno_MoveSnowPile(EnEeno* this, PlayState* play);
static void Eeno_Emerge(EnEeno* this, PlayState* play);
static void Eeno_ReadySnowball(EnEeno* this, PlayState* play);
static void Eeno_ThrowSnowball(EnEeno* this, PlayState* play);
static void Eeno_Idle(EnEeno* this, PlayState* play);
static void Eeno_Submerge(EnEeno* this, PlayState* play);
static void Eeno_Melt(EnEeno* this, PlayState* play);
static void Eeno_Stun(EnEeno* this, PlayState* play);
static void Eeno_Damaged(EnEeno* this, PlayState* play);
static void EnEeno_Draw(Actor* thisx, PlayState* play);
static void Eeno_DrawSnowPile(Actor* thisx, PlayState* play);

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

static void Eeno_SpawnSnowDust(PlayState* play, Vec3f* pos, Vec3f* velocity, s16 scale, s16 scaleStep) {
    func_8002829C(play, pos, velocity, &sZeroVec, &sDustPrim, &sDustEnv, scale, scaleStep);
}

static void Eeno_SpawnDustRing(EnEeno* this, PlayState* play) {
    s16 angle = 0;
    Vec3f pos;

    pos.y = Rand_ZeroFloat(10.0f) + this->actor.world.pos.y;
    for (s32 i = 0; i < 16; i++) {
        f32 offset = Rand_ZeroFloat(10.0f) + 20.0f;

        pos.x = Math_SinS(angle) * offset + this->actor.world.pos.x;
        pos.z = Math_CosS(angle) * offset + this->actor.world.pos.z;
        Eeno_SpawnSnowDust(play, &pos, &sDustVelocity, 400, 10);
        angle += 0x1000;
    }
}

static void Eeno_SpawnSnowFragments(PlayState* play, Vec3f* origin, f32 spread, s16 baseScale) {
    static Vec3f sAccel = { 0.0f, -0.5f, 0.0f };

    for (s32 i = 0; i < 15; i++) {
        s16 altitude = Rand_S16Offset(0x1000, 0x3000);
        s16 azimuth = Rand_Next() >> 0x10;
        f32 speed = Rand_ZeroFloat(2.0f) + 4.0f;
        Vec3f velocity = { speed * Math_CosS(altitude) * Math_SinS(azimuth), speed * Math_SinS(altitude),
                           speed * Math_CosS(altitude) * Math_CosS(azimuth) };
        Vec3f pos = { Rand_ZeroFloat(spread) * velocity.x + origin->x,
                      Rand_ZeroFloat(spread * 0.5f) * velocity.y + origin->y,
                      Rand_ZeroFloat(spread) * velocity.z + origin->z };
        s16 scale = Rand_S16Offset((i % 3) * baseScale + baseScale, (i % 3) * (baseScale / 2) + baseScale / 2);

        EffectSsHahen_Spawn(play, &pos, &velocity, &sAccel, 0, scale, -1, 20, (Gfx*)sSnowballFragmentDLs[i % 3]);
    }
}

static void Eeno_SetupMoveSnowPile(EnEeno* this) {
    Animation_PlayLoop(&this->snowPileSkelAnime, (AnimationHeader*)sEenoSnowPileMoveAnim);
    this->actor.scale.y = this->actor.scale.x;
    this->actor.speedXZ = 2.0f;
    this->actor.draw = Eeno_DrawSnowPile;
    this->timer = 40;
    this->collider.dim.radius = 30;
    this->collider.dim.height = 10;
    this->actionFunc = Eeno_MoveSnowPile;
}

static void Eeno_SetupEmerge(EnEeno* this, PlayState* play) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sEenoEmergeAnim);
    PlaySfx(&this->actor, MM_SE_EN_YMAJIN_SURFACE, NA_SE_EN_FREEZAD_BREATH);
    this->collider.dim.radius = 40;
    this->collider.dim.height = 25;
    this->actor.draw = EnEeno_Draw;
    this->actor.scale.y = this->actor.scale.x * 0.4f;
    this->actor.speedXZ = 0.0f;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    Eeno_SpawnDustRing(this, play);
    this->collider.base.acFlags &= ~AC_ON;
    this->actionFunc = Eeno_Emerge;
}

static bool Eeno_IsPlayerInReach(EnEeno* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    return fabsf(this->actor.yDistToPlayer) < 60.0f && this->actor.xzDistToPlayer < EENO_ATTACK_RANGE &&
           !(player->stateFlags1 & PLAYER_STATE1_ON_HORSE);
}

static void Eeno_RoamAsSnowPile(EnEeno* this) {
    if (this->roamYaw != this->actor.shape.rot.y) {
        Math_ScaledStepToS(&this->actor.shape.rot.y, this->roamYaw, 0x100);
        this->actor.world.rot.y = this->actor.shape.rot.y;
    } else if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->roamYaw = this->actor.wallYaw;
    } else if (Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > EENO_ROAM_RADIUS) {
        this->roamYaw = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos) + ((s32)Rand_Next() >> 0x14);
    } else if (Rand_ZeroOne() < 0.02f) {
        s16 turn = (Rand_Next() >> 0x13) + 0x1000;

        this->roamYaw += Rand_ZeroOne() < 0.5f ? -turn : turn;
    }
}

// Slides around as a mound of snow and pops out when Link comes within reach.
static void Eeno_MoveSnowPile(EnEeno* this, PlayState* play) {
    SkelAnime_Update(&this->snowPileSkelAnime);
    PlaySfx(&this->actor, MM_SE_EN_YMAJIN_MINI_MOVE, NA_SE_EN_DODO_M_MOVE);
    if (this->timer > 0) {
        this->timer--;
    }
    if (this->timer == 0) {
        this->collider.base.acFlags |= AC_ON;
    }
    if (this->timer == 0 && Eeno_IsPlayerInReach(this, play)) {
        Eeno_SetupEmerge(this, play);
        return;
    }
    Eeno_RoamAsSnowPile(this);
}

static void Eeno_SetupReadySnowball(EnEeno* this) {
    this->actor.scale.y = this->actor.scale.x;
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sEenoSmallSnowballCreateAnim);
    this->actionFunc = Eeno_ReadySnowball;
}

static void Eeno_SetupIdle(EnEeno* this) {
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sEenoIdleAnim);
    this->timer = 60;
    this->actionFunc = Eeno_Idle;
}

static f32 Eeno_GetEmergeStep(EnEeno* this) {
    return (this->actor.scale.x * 0.6f) / Animation_GetLastFrame((void*)sEenoEmergeAnim);
}

static void Eeno_Emerge(EnEeno* this, PlayState* play) {
    Math_StepToF(&this->actor.scale.y, this->actor.scale.x, Eeno_GetEmergeStep(this));
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0x1000);
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    this->collider.base.acFlags |= AC_ON;
    this->snowballsLeft = EENO_SNOWBALLS_PER_VOLLEY;
    Eeno_SetupReadySnowball(this);
}

static void Eeno_SetupThrowSnowball(EnEeno* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sEenoSmallSnowballThrowAnim);
    this->snowballsLeft--;
    this->actionFunc = Eeno_ThrowSnowball;
}

static void Eeno_ReadySnowball(EnEeno* this, PlayState* play) {
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0x1000);
    if (this->isHoldingSnowball && (play->gameplayFrames % 2) != 0) {
        Vec3f pos = { Rand_CenteredFloat(10.0f) + this->snowballPos.x, Rand_CenteredFloat(10.0f) + this->snowballPos.y,
                      Rand_CenteredFloat(10.0f) + this->snowballPos.z };

        Eeno_SpawnSnowDust(play, &pos, &sDustVelocity, 500, 30);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        Eeno_SetupThrowSnowball(this);
    } else if (Animation_OnFrame(&this->skelAnime, EENO_HOLD_FRAME)) {
        PlaySfx(&this->actor, MM_SE_EN_YMAJIN_MINI_HOLD, NA_SE_EN_FREEZAD_BREATH);
        this->isHoldingSnowball = true;
    }
}

static void Eeno_ThrowSnowball(EnEeno* this, PlayState* play) {
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0x1000);
    if (SkelAnime_Update(&this->skelAnime)) {
        if (this->snowballsLeft != 0) {
            Eeno_SetupReadySnowball(this);
        } else {
            Eeno_SetupIdle(this);
        }
        return;
    }
    if (!Animation_OnFrame(&this->skelAnime, EENO_THROW_FRAME)) {
        return;
    }
    this->isHoldingSnowball = false;
    PlaySfx(&this->actor, MM_SE_EN_YMAJIN_MINI_THROW, NA_SE_IT_SWORD_SWING);
    Actor_Spawn(&play->actorCtx, play, sSnowballActorId, this->snowballPos.x, this->snowballPos.y, this->snowballPos.z,
                0, this->actor.yawTowardsPlayer, 0, 0);
}

static void Eeno_SetupSubmerge(EnEeno* this, PlayState* play) {
    AnimationHeader* emerge = (AnimationHeader*)sEenoEmergeAnim;

    Animation_Change(&this->skelAnime, emerge, -1.0f, Animation_GetLastFrame(emerge), 0.0f, ANIMMODE_ONCE, -3.0f);
    Eeno_SpawnDustRing(this, play);
    PlaySfx(&this->actor, MM_SE_EN_YMAJIN_HIDE, NA_SE_EN_FREEZAD_BREATH);
    this->actionFunc = Eeno_Submerge;
}

static void Eeno_Idle(EnEeno* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer > 0) {
        this->timer--;
    } else {
        Eeno_SetupSubmerge(this, play);
    }
}

static void Eeno_Submerge(EnEeno* this, PlayState* play) {
    Math_StepToF(&this->actor.scale.y, this->actor.scale.x * 0.4f, Eeno_GetEmergeStep(this));
    if (SkelAnime_Update(&this->skelAnime)) {
        Eeno_SetupMoveSnowPile(this);
    }
}

static void Eeno_Vanish(EnEeno* this, PlayState* play) {
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x60);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

static void Eeno_SetupMelt(EnEeno* this) {
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 50);
    this->collider.base.acFlags &= ~AC_ON;
    this->timer = 50;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    this->actor.scale.y = this->actor.scale.x;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Eeno_Melt;
}

// Fire melts it into a steaming puddle.
static void Eeno_Melt(EnEeno* this, PlayState* play) {
    this->timer--;
    if (this->timer >= 38 && !(this->timer & 1)) {
        Vec3f velocity;
        f32 rise = (this->timer - 38) * (1.0f / 12.0f);

        velocity.x = Rand_CenteredFloat(1.5f) * rise;
        velocity.z = Rand_CenteredFloat(1.5f) * rise;
        velocity.y = rise + 0.8f;

        Vec3f pos = { (velocity.x >= 0.0f ? 1.0f : -1.0f) * Rand_ZeroFloat(20.0f) + this->actor.world.pos.x,
                      this->actor.world.pos.y + 3.0f,
                      (velocity.z >= 0.0f ? 1.0f : -1.0f) * Rand_ZeroFloat(20.0f) + this->actor.world.pos.z };

        EffectSsIceSmoke_Spawn(play, &pos, &velocity, &sZeroVec, 300);
    }
    if (this->timer == 0) {
        Eeno_Vanish(this, play);
        return;
    }
    this->actor.scale.y = this->timer * 0.0002f;
    this->actor.scale.x = 0.014f - 0.4f * this->actor.scale.y;
    this->actor.scale.z = this->actor.scale.x;
}

static void Eeno_SetupStun(EnEeno* this) {
    if (this->actionFunc != Eeno_Stun) {
        this->actionBeforeStun = this->actionFunc;
    }
    this->actionFunc = Eeno_Stun;
}

static void Eeno_Stun(EnEeno* this, PlayState* play) {
    if (this->actor.colorFilterTimer == 0) {
        this->actionFunc = this->actionBeforeStun;
    }
}

static void Eeno_SetupDamaged(EnEeno* this) {
    Actor* attacker = this->collider.base.ac;

    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sEenoDamageAnim);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 20);
    this->collider.base.acFlags &= ~AC_ON;
    this->timer = 20;
    this->actor.draw = EnEeno_Draw;
    this->actor.scale.y = this->actor.scale.x;
    this->actor.speedXZ = 10.0f;
    if (attacker != NULL) {
        this->actor.world.rot.y = Actor_WorldYawTowardActor(attacker, &this->actor);
    }
    PlaySfx(&this->actor, MM_SE_EN_YMAJIN_MINI_DAMAGE, NA_SE_EN_FREEZAD_DAMAGE);
    this->actionFunc = Eeno_Damaged;
}

static void Eeno_Shatter(EnEeno* this, PlayState* play) {
    Eeno_SpawnSnowFragments(play, &this->actor.world.pos, 6.0f, 20);
    Eeno_SpawnSnowDust(play, &this->actor.world.pos, &sZeroVec, 500, 30);
    PlaySfx(&this->actor, MM_SE_EN_YMAJIN_DEAD_BREAK, NA_SE_EN_FREEZAD_DEAD);
    Eeno_Vanish(this, play);
}

// Spins away from the blow, then dives back into the snow or shatters.
static void Eeno_Damaged(EnEeno* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->actor.shape.rot.y += CLAMP_MAX(this->timer, 10) * 0x300;
    Math_StepToF(&this->actor.speedXZ, 0.0f, 0.5f);
    if (this->timer > 0) {
        this->timer--;
        return;
    }
    if (this->actor.colChkInfo.health == 0) {
        Eeno_Shatter(this, play);
        return;
    }
    this->collider.base.acFlags |= AC_ON;
    Eeno_SetupSubmerge(this, play);
}

static void Eeno_CheckDamage(EnEeno* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Actor_SetDropFlag(&this->actor, &this->collider.info, false);

    u8 effect = this->actor.colChkInfo.damageEffect;

    if (effect == EENO_DMGEFF_MELT) {
        Enemy_StartFinishingBlow(play, &this->actor);
        Actor_ApplyDamage(&this->actor);
        Eeno_SetupMelt(this);
    } else if (this->actionFunc == Eeno_MoveSnowPile) {
        Eeno_SetupEmerge(this, play);
    } else if (effect == EENO_DMGEFF_STUN) {
        Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 40);
        PlaySfx(&this->actor, MM_SE_EN_COMMON_FREEZE, NA_SE_EN_GOMA_JR_FREEZE);
        Eeno_SetupStun(this);
    } else {
        if (Actor_ApplyDamage(&this->actor) == 0) {
            Enemy_StartFinishingBlow(play, &this->actor);
        }
        this->isHoldingSnowball = false;
        Eeno_SetupDamaged(this);
    }
}

static void EnEeno_Init(Actor* thisx, PlayState* play) {
    EnEeno* this = (EnEeno*)thisx;

    this->actor.targetArrowOffset = 3000.0f;
    this->actor.gravity = -1.0f;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sEenoSkel, (AnimationHeader*)sEenoEmergeAnim,
                       this->jointTable, this->morphTable, EENO_LIMB_MAX);
    SkelAnime_InitFlex(play, &this->snowPileSkelAnime, (FlexSkeletonHeader*)sEenoSnowPileSkel,
                       (AnimationHeader*)sEenoSnowPileMoveAnim, this->snowPileJointTable, this->snowPileMorphTable,
                       EENO_SNOW_PILE_LIMB_MAX);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sEenoCylinderInit);
    this->roamYaw = this->actor.shape.rot.y;
    Eeno_SetupMoveSnowPile(this);
}

static void EnEeno_Destroy(Actor* thisx, PlayState* play) {
    EnEeno* this = (EnEeno*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void EnEeno_Update(Actor* thisx, PlayState* play) {
    EnEeno* this = (EnEeno*)thisx;

    Eeno_CheckDamage(this, play);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, this->collider.dim.radius, 0.0f, BGCHECK_UPDATE_WALL_FLOOR);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    Actor_SetFocus(&this->actor, this->actor.scale.y * (this->actor.draw == EnEeno_Draw ? 1800.0f : 720.0f));
}

static void Eeno_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnEeno* this = (EnEeno*)thisx;

    if (limbIndex != EENO_LIMB_RIGHT_HAND || !this->isHoldingSnowball) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(800.0f, -600.0f, 0.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sEenoSmallSnowballDL);
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_MultVec3f(&sZeroVec, &this->snowballPos);
}

static void EnEeno_Draw(Actor* thisx, PlayState* play) {
    EnEeno* this = (EnEeno*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          Eeno_PostLimbDraw, this);
}

static void Eeno_DrawSnowPile(Actor* thisx, PlayState* play) {
    EnEeno* this = (EnEeno*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->snowPileSkelAnime.skeleton, this->snowPileSkelAnime.jointTable,
                          this->snowPileSkelAnime.dListCount, NULL, NULL, this);
}

// Lobbed at Link: the arc is aimed from his distance, as MM does.
static void EnEenoSnowball_Init(Actor* thisx, PlayState* play) {
    EnEenoSnowball* this = (EnEenoSnowball*)thisx;
    Player* player = GET_PLAYER(play);

    this->actor.gravity = -1.0f;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sSnowballCylinderInit);
    this->actor.world.rot.y = Actor_WorldYawTowardActor(&this->actor, &player->actor);
    this->actor.velocity.y = CLAMP_MAX(Actor_WorldDistXZToActor(&this->actor, &player->actor) * 0.035f - 5.0f, 3.5f);
    this->actor.speedXZ = 15.0f;
    this->actor.world.pos.x += this->actor.speedXZ * Math_SinS(this->actor.world.rot.y);
    this->actor.world.pos.y += this->actor.velocity.y;
    this->actor.world.pos.z += this->actor.speedXZ * Math_CosS(this->actor.world.rot.y);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 10.0f);
    this->armTimer = 5;
}

static void EnEenoSnowball_Destroy(Actor* thisx, PlayState* play) {
    EnEenoSnowball* this = (EnEenoSnowball*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static bool EnEenoSnowball_HasHitSomething(EnEenoSnowball* this) {
    u16 bgHits = BGCHECKFLAG_WALL | BGCHECKFLAG_GROUND | BGCHECKFLAG_CEILING;

    return (this->actor.bgCheckFlags & bgHits) || (this->collider.base.atFlags & AT_HIT) ||
           (this->collider.base.acFlags & AC_HIT) || (this->collider.base.ocFlags1 & OC1_HIT);
}

static void EnEenoSnowball_Burst(EnEenoSnowball* this, PlayState* play) {
    s16 scale = 10;

    for (s32 i = 0; i < 3; i++) {
        EffectSsHahen_SpawnBurst(play, &this->actor.world.pos, 5.0f, 0, scale, scale >> 1, 3, -1, 20,
                                 (Gfx*)sSnowballFragmentDLs[i]);
        scale *= 2;
    }
    Eeno_SpawnSnowDust(play, &this->actor.world.pos, &sZeroVec, 500, 30);
    PlaySfx(&this->actor, MM_SE_EV_SMALL_SNOWBALL_BROKEN, NA_SE_IT_REFLECTION_WOOD);
    Actor_Kill(&this->actor);
}

static void EnEenoSnowball_Update(Actor* thisx, PlayState* play) {
    EnEenoSnowball* this = (EnEenoSnowball*)thisx;

    if (this->armTimer > 0) {
        this->armTimer--;
    } else {
        this->collider.base.ocFlags1 |= OC1_ON;
    }
    if (EnEenoSnowball_HasHitSomething(this)) {
        EnEenoSnowball_Burst(this, play);
        return;
    }
    this->actor.shape.rot.x += 0xF00;
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, this->collider.dim.radius * 0.6f,
                            this->collider.dim.height - this->collider.dim.yShift, BGCHECK_UPDATE_WALL_CEILING_FLOOR);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void EnEenoSnowball_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, (Gfx*)sEenoSmallSnowballDL);
}

// MM's CanKillEnemy(ACTOR_EN_SNOWMAN): nearly any weapon; fire melts it outright.
static bool Eeno_CanKillInLogic(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RG_FAIRY_BOW) ||
           sApi->LogicCanUseItem(RG_STICKS) || sApi->LogicCanUseItem(RG_HOOKSHOT) ||
           sApi->LogicCanUseItem(RG_MEGATON_HAMMER) || sApi->LogicCanUseItem(RG_DINS_FIRE);
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
    static Vec3s sSoulJointTable[EENO_LIMB_MAX];
    static Vec3s sSoulMorphTable[EENO_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sEenoSkel, (AnimationHeader*)sEenoIdleAnim,
                           sSoulJointTable, sSoulMorphTable, EENO_LIMB_MAX);
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
    SOHCustomItemDefinition soul = Z64Items_Define(EENO_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Eenos";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Eenos%w!&Eenos can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " EENO_SOUL_ITEM_KEY);
    }
}

static void RegisterSnowball(void) {
    SOHActorDefinition snowball = { 0 };

    snowball.structSize = sizeof(snowball);
    snowball.key = EENO_SNOWBALL_KEY;
    snowball.description = "Eeno snowball";
    snowball.category = ACTORCAT_PROP;
    snowball.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    snowball.objectId = OBJECT_GAMEPLAY_KEEP;
    snowball.instanceSize = sizeof(EnEenoSnowball);
    snowball.init = EnEenoSnowball_Init;
    snowball.destroy = EnEenoSnowball_Destroy;
    snowball.update = EnEenoSnowball_Update;
    snowball.draw = EnEenoSnowball_Draw;
    sSnowballActorId = sApi->RegisterActor(&snowball);
}

static void RegisterEeno(void) {
    SOHActorDefinition eeno = { 0 };

    eeno.structSize = sizeof(eeno);
    eeno.key = EENO_KEY;
    eeno.description = "Eeno";
    eeno.category = ACTORCAT_ENEMY;
    eeno.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
    eeno.objectId = OBJECT_GAMEPLAY_KEEP;
    eeno.instanceSize = sizeof(EnEeno);
    eeno.init = EnEeno_Init;
    eeno.destroy = EnEeno_Destroy;
    eeno.update = EnEeno_Update;
    eeno.draw = Eeno_DrawSnowPile;
    eeno.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    eeno.naviHint = "Eeno&%cA snowman that hides in the snow and throws snowballs. %rFire%c melts it at once!%w";
    eeno.soulKey = EENO_KEY;
    eeno.logicKill = Eeno_CanKillInLogic;
    if (sApi->RegisterActor(&eeno) < 0) {
        sApi->Log("Could not register " EENO_KEY);
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
    RegisterSnowball();
    if (sSnowballActorId < 0) {
        sApi->Log("Could not register " EENO_SNOWBALL_KEY);
        return;
    }
    RegisterSoul();
    RegisterEeno();
}
