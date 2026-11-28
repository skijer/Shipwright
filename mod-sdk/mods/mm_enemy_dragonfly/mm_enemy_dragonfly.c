// Majora's Mask's Dragonfly (En_Grasshopper, the regular kind) as an Unbound custom enemy, drawn from mm.o2r.
// Its soft limb shadow is replaced by the usual circle.
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

#define DRAGONFLY_KEY "mm.dragonfly"
#define DRAGONFLY_SOUL_ITEM_KEY "soul.mm.dragonfly"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_DRAGONFLY "__OTR__objects/object_grasshopper/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_FLOOR ((1 << 0) | (1 << 2))

#define MM_SE_EN_BATTA_FLY 0x38A0
#define MM_SE_EN_BATTA_ATTACK 0x38A1
#define MM_SE_EN_BATTA_DAMAGE 0x38A2
#define MM_SE_EN_BATTA_DEAD 0x38A3
#define MM_SE_EN_COMMON_EXTINCT_LEV 0x3A1F
#define MM_SE_EV_ICE_BROKEN 0x28CB

#define DRAGONFLY_HEALTH 2
#define DRAGONFLY_NOTICE_RANGE 200.0f
#define DRAGONFLY_FLY_HEIGHT 90.0f
#define DRAGONFLY_SPARK_COUNT 40
#define DRAGONFLY_LIGHTNING_FRAMES 8

typedef enum {
    DRAGONFLY_LIMB_NONE,
    DRAGONFLY_LIMB_ROOT,
    DRAGONFLY_LIMB_TAIL_SEGMENT_1,
    DRAGONFLY_LIMB_TAIL_SEGMENT_2,
    DRAGONFLY_LIMB_TAIL_SEGMENT_3,
    DRAGONFLY_LIMB_TAIL_SEGMENT_4,
    DRAGONFLY_LIMB_TAIL_TIP,
    DRAGONFLY_LIMB_THORAX,
    DRAGONFLY_LIMB_MAX = 0x18
} DragonflyLimb;

typedef enum {
    DRAGONFLY_DMGEFF_IMMUNE,
    DRAGONFLY_DMGEFF_FIRE = 0x2,
    DRAGONFLY_DMGEFF_FREEZE,
    DRAGONFLY_DMGEFF_LIGHT,
    DRAGONFLY_DMGEFF_HOOK = 0xE,
    DRAGONFLY_DMGEFF_NONE
} DragonflyDamageEffect;

typedef struct {
    bool isEnabled;
    Vec3f offset;
    f32 yaw;
    f32 scale;
    s16 timer;
    s16 frame;
} DragonflySpark;

struct EnDragonfly;

typedef void (*EnDragonflyActionFunc)(struct EnDragonfly*, PlayState*);

typedef struct EnDragonfly {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[DRAGONFLY_LIMB_MAX];
    Vec3s morphTable[DRAGONFLY_LIMB_MAX];
    EnDragonflyActionFunc actionFunc;
    bool isAttackChosen;
    bool isFrozen;
    s16 timer;
    s16 waitTimer;
    s16 bobPhase;
    s16 turnYaw;
    s16 bankRoll;
    f32 animEndFrame;
    f32 approachSpeed;
    f32 angularVelocity;
    f32 baseFlyHeight;
    Vec3f flyingHomePos;
    Vec3f approachPos;
    Vec3f tailTipPos;
    Vec3f sparkOrigin;
    Vec3f damagedVelocity;
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[2];
    DragonflySpark sparks[DRAGONFLY_SPARK_COUNT];
} EnDragonfly;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sDragonflySkel[] = OBJ_DRAGONFLY "gDragonflySkel";
static const ALIGN_ASSET(2) char sDragonflyRaiseTailAnim[] = OBJ_DRAGONFLY "gDragonflyRaiseTailAnim";
static const ALIGN_ASSET(2) char sDragonflyLowerTailAnim[] = OBJ_DRAGONFLY "gDragonflyLowerTailAnim";
static const ALIGN_ASSET(2) char sDragonflyFlyAnim[] = OBJ_DRAGONFLY "gDragonflyFlyAnim";
static const ALIGN_ASSET(2) char sDragonflyAttackAnim[] = OBJ_DRAGONFLY "gDragonflyAttackAnim";
static const ALIGN_ASSET(2) char sDragonflyHoverAnim[] = OBJ_DRAGONFLY "gDragonflyHoverAnim";
static const ALIGN_ASSET(2) char sDragonflyDamageAnim[] = OBJ_DRAGONFLY "gDragonflyDamageAnim";
static const ALIGN_ASSET(2) char sDragonflyDeadAnim[] = OBJ_DRAGONFLY "gDragonflyDeadAnim";
static const ALIGN_ASSET(2) char sDragonflyFallAnim[] = OBJ_DRAGONFLY "gDragonflyFallAnim";

static const char* const sLightningTextures[DRAGONFLY_LIGHTNING_FRAMES] = {
    gEffLightning1Tex, gEffLightning2Tex, gEffLightning3Tex, gEffLightning4Tex,
    gEffLightning5Tex, gEffLightning6Tex, gEffLightning7Tex, gEffLightning8Tex,
};

static ColliderJntSphElementInit sElementsInit[2] = {
    {
        { ELEMTYPE_UNK3,
          { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x00 },
          { 0xFFCFFFFF, 0x00, 0x00 },
          TOUCH_ON | TOUCH_SFX_NORMAL,
          BUMP_ON | BUMP_HOOKABLE,
          OCELEM_ON },
        { DRAGONFLY_LIMB_THORAX, { { 0, 0, 0 }, 1 }, 2200 },
    },
    {
        { ELEMTYPE_UNK2,
          { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_ELECTRIC, 0x04 },
          { 0xFFCFFFFF, 0x00, 0x00 },
          TOUCH_ON | TOUCH_SFX_NORMAL,
          BUMP_ON,
          OCELEM_NONE },
        { DRAGONFLY_LIMB_TAIL_TIP, { { 1000, 0, 0 }, 1 }, 1600 },
    },
};

static ColliderJntSphInit sColliderInit = {
    { COLTYPE_HIT2, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_1, OC2_TYPE_1, COLSHAPE_JNTSPH },
    2,
    sElementsInit,
};

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(1, DRAGONFLY_DMGEFF_NONE),
    /* Deku stick    */ DMG_ENTRY(2, DRAGONFLY_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, DRAGONFLY_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, DRAGONFLY_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, DRAGONFLY_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, DRAGONFLY_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, DRAGONFLY_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, DRAGONFLY_DMGEFF_HOOK),
    /* Kokiri sword  */ DMG_ENTRY(1, DRAGONFLY_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, DRAGONFLY_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, DRAGONFLY_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(4, DRAGONFLY_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, DRAGONFLY_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(4, DRAGONFLY_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, DRAGONFLY_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, DRAGONFLY_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, DRAGONFLY_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(2, DRAGONFLY_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(4, DRAGONFLY_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(4, DRAGONFLY_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, DRAGONFLY_DMGEFF_IMMUNE),
    /* Mirror Ray    */ DMG_ENTRY(0, DRAGONFLY_DMGEFF_IMMUNE),
    /* Kokiri spin   */ DMG_ENTRY(2, DRAGONFLY_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(8, DRAGONFLY_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(4, DRAGONFLY_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, DRAGONFLY_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, DRAGONFLY_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, DRAGONFLY_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, DRAGONFLY_DMGEFF_IMMUNE),
    /* Unblockable   */ DMG_ENTRY(0, DRAGONFLY_DMGEFF_IMMUNE),
    /* Hammer jump   */ DMG_ENTRY(0, DRAGONFLY_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, DRAGONFLY_DMGEFF_IMMUNE),
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sSparkOffset = { 500.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

static void Dragonfly_Fly(EnDragonfly* this, PlayState* play);
static void Dragonfly_DecideAction(EnDragonfly* this, PlayState* play);
static void Dragonfly_ApproachPlayer(EnDragonfly* this, PlayState* play);
static void Dragonfly_Attack(EnDragonfly* this, PlayState* play);
static void Dragonfly_WaitAfterAttack(EnDragonfly* this, PlayState* play);
static void Dragonfly_Damaged(EnDragonfly* this, PlayState* play);
static void Dragonfly_Dead(EnDragonfly* this, PlayState* play);
static void Dragonfly_Fall(EnDragonfly* this, PlayState* play);

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

static void Dragonfly_ChangeAnim(EnDragonfly* this, const char* anim, u8 mode, f32 morphFrames) {
    this->animEndFrame = Animation_GetLastFrame((void*)anim);
    Animation_Change(&this->skelAnime, (AnimationHeader*)anim, 1.0f, 0.0f, this->animEndFrame, mode, morphFrames);
}

static bool Dragonfly_IsAnimDone(EnDragonfly* this) {
    return this->skelAnime.curFrame >= this->animEndFrame;
}

static void Dragonfly_Buzz(EnDragonfly* this) {
    PlaySfx(&this->actor, MM_SE_EN_BATTA_FLY, NA_SE_EN_FFLY_FLY);
}

static void Dragonfly_SetupFly(EnDragonfly* this) {
    Dragonfly_ChangeAnim(this, sDragonflyFlyAnim, ANIMMODE_LOOP, 0.0f);
    this->baseFlyHeight = Rand_CenteredFloat(50.0f) + this->flyingHomePos.y;
    this->actionFunc = Dragonfly_Fly;
}

static void Dragonfly_SetupApproachPlayer(EnDragonfly* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->timer = 50;
    this->approachSpeed = 0.0f;
    this->approachPos.x = Math_SinS(player->actor.shape.rot.y) * 130.0f + player->actor.world.pos.x;
    this->approachPos.z = Math_CosS(player->actor.shape.rot.y) * 130.0f + player->actor.world.pos.z;
    this->actionFunc = Dragonfly_ApproachPlayer;
}

static void Dragonfly_RaiseTail(EnDragonfly* this) {
    Dragonfly_ChangeAnim(this, sDragonflyRaiseTailAnim, ANIMMODE_ONCE, 0.0f);
    this->isAttackChosen = false;
    this->actionFunc = Dragonfly_DecideAction;
}

static void Dragonfly_LowerTail(EnDragonfly* this) {
    Dragonfly_ChangeAnim(this, sDragonflyLowerTailAnim, ANIMMODE_ONCE, 0.0f);
    this->isAttackChosen = true;
    this->actionFunc = Dragonfly_DecideAction;
}

static void Dragonfly_SetupAttack(EnDragonfly* this) {
    Dragonfly_ChangeAnim(this, sDragonflyAttackAnim, ANIMMODE_ONCE, -3.0f);
    this->approachSpeed = 0.0f;
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xFA0, 0xA);
    this->actor.speedXZ = 3.0f;
    this->baseFlyHeight = this->actor.world.pos.y;
    this->collider.elements[0].info.toucherFlags &= ~TOUCH_ON;
    this->collider.elements[1].info.toucherFlags |= TOUCH_ON;
    PlaySfx(&this->actor, MM_SE_EN_BATTA_ATTACK, NA_SE_EN_FFLY_ATTACK);
    this->actionFunc = Dragonfly_Attack;
}

static void Dragonfly_DecideAction(EnDragonfly* this, PlayState* play) {
    if (!Dragonfly_IsAnimDone(this)) {
        return;
    }
    if (this->isAttackChosen) {
        Dragonfly_SetupAttack(this);
        return;
    }
    this->flyingHomePos = this->actor.world.pos;
    this->flyingHomePos.y = this->actor.floorHeight + DRAGONFLY_FLY_HEIGHT;
    Dragonfly_ChangeAnim(this, sDragonflyFlyAnim, ANIMMODE_LOOP, 0.0f);
    Dragonfly_SetupApproachPlayer(this, play);
}

// Wanders around its home with sudden stops, turning back before walls, until Link comes near.
static void Dragonfly_Fly(EnDragonfly* this, PlayState* play) {
    f32 toHomeX = this->flyingHomePos.x - this->actor.world.pos.x;
    f32 toHomeZ = this->flyingHomePos.z - this->actor.world.pos.z;
    Vec3f ahead = { Math_SinS(this->actor.shape.rot.y) * 100.0f + this->actor.world.pos.x, this->actor.world.pos.y,
                    Math_CosS(this->actor.shape.rot.y) * 100.0f + this->actor.world.pos.z };
    bool isBlocked = BgCheck_SphVsFirstPoly(&play->colCtx, &ahead, 10.0f) ||
                     (this->collider.elements[0].info.ocElemFlags & OCELEM_HIT);

    Dragonfly_Buzz(this);
    this->bobPhase += 0xAF0;
    Math_ApproachF(&this->actor.world.pos.y, Math_SinS(this->bobPhase) * 10.0f + this->baseFlyHeight, 0.1f, 10.0f);
    if (isBlocked) {
        this->baseFlyHeight = Rand_CenteredFloat(50.0f) + this->flyingHomePos.y;
        this->turnYaw = Math_Atan2S(toHomeX, toHomeZ);
        this->timer = Rand_S16Offset(30, 30);
    }
    if (!isBlocked && this->actor.xzDistToPlayer < DRAGONFLY_NOTICE_RANGE) {
        Dragonfly_SetupApproachPlayer(this, play);
        return;
    }
    Math_SmoothStepToS(&this->actor.world.rot.z, this->bankRoll, 5, 0x3E8, 5);
    this->bankRoll *= 0.8f;
    if (this->waitTimer != 0) {
        Math_ApproachZeroF(&this->actor.speedXZ, 0.2f, 0.5f);
        return;
    }
    this->bankRoll = (this->actor.world.rot.y - this->turnYaw) * 0.2f;
    Math_ApproachF(&this->actor.speedXZ, 4.0f, 0.4f, 0.7f);
    Math_ApproachF(&this->angularVelocity, 2000.0f, 1.0f, 50.0f);
    Math_SmoothStepToS(&this->actor.world.rot.y, this->turnYaw, 5, this->angularVelocity, 5);
    if (this->timer == 0) {
        if (Rand_ZeroFloat(1.0f) < 0.3f) {
            this->waitTimer = Rand_S16Offset(10, 10);
            this->angularVelocity = 0.0f;
        }
        this->turnYaw = Math_Atan2S(toHomeX, toHomeZ);
        this->timer = Rand_S16Offset(30, 70);
        this->baseFlyHeight = Rand_CenteredFloat(50.0f) + this->flyingHomePos.y;
    }
}

// Flies to a spot in front of Link and lowers its tail over him.
static void Dragonfly_ApproachPlayer(EnDragonfly* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Dragonfly_Buzz(this);
    this->bobPhase += 0xAF0;
    this->approachPos.y = Math_SinS(this->bobPhase) * 10.0f + player->actor.world.pos.y + 120.0f;
    Math_SmoothStepToS(&this->actor.world.rot.z, 0, 5, 0x3E8, 5);
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.shape.rot.y, 0xA, 0xFA0, 0xA);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xFA0, 0xA);
    Math_ApproachF(&this->actor.world.pos.x, this->approachPos.x, 0.3f, this->approachSpeed);
    Math_ApproachF(&this->actor.world.pos.y, this->approachPos.y, 0.1f, this->approachSpeed);
    Math_ApproachF(&this->actor.world.pos.z, this->approachPos.z, 0.3f, this->approachSpeed);
    Math_ApproachF(&this->approachSpeed, 7.0f, 0.3f, 1.0f);

    bool hasArrived = fabsf(this->approachPos.x - this->actor.world.pos.x) <= 10.0f &&
                      fabsf(this->approachPos.y - this->actor.world.pos.y) <= 20.0f &&
                      fabsf(this->approachPos.z - this->actor.world.pos.z) <= 10.0f;

    if (this->timer == 0 || hasArrived) {
        Dragonfly_LowerTail(this);
    }
}

static void Dragonfly_SpawnSpark(EnDragonfly* this) {
    for (s32 i = 0; i < DRAGONFLY_SPARK_COUNT; i++) {
        DragonflySpark* spark = &this->sparks[i];

        if (spark->isEnabled) {
            continue;
        }
        spark->isEnabled = true;
        spark->timer = 10;
        spark->scale = 0.0f;
        spark->offset.x = Rand_CenteredFloat(20.0f);
        spark->offset.y = Rand_CenteredFloat(20.0f);
        spark->offset.z = Rand_CenteredFloat(20.0f);
        spark->yaw = Rand_CenteredFloat(30000.0f);
        return;
    }
}

static void Dragonfly_SetupWaitAfterAttack(EnDragonfly* this) {
    Dragonfly_ChangeAnim(this, sDragonflyHoverAnim, ANIMMODE_LOOP, -3.0f);
    this->waitTimer = 20;
    this->actor.speedXZ = 0.0f;
    this->collider.elements[1].info.toucherFlags &= ~TOUCH_ON;
    this->actionFunc = Dragonfly_WaitAfterAttack;
}

// The charged tail crackles as it stabs; a shield turns the sting aside.
static void Dragonfly_Attack(EnDragonfly* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    for (s32 i = 0; i < 6; i++) {
        Dragonfly_SpawnSpark(this);
    }
    this->bobPhase += 0xAF0;
    this->approachPos.y = Math_SinS(this->bobPhase) * 10.0f + player->actor.world.pos.y + 60.0f;
    if (this->collider.base.atFlags & AT_BOUNCED) {
        this->collider.elements[1].info.toucherFlags &= ~TOUCH_ON;
    }
    Math_ApproachF(&this->actor.world.pos.y, this->approachPos.y, 0.1f, this->approachSpeed);
    Math_ApproachF(&this->approachSpeed, 10.0f, 0.1f, 1.0f);
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xFA0, 0xA);
    if (Dragonfly_IsAnimDone(this)) {
        Dragonfly_SetupWaitAfterAttack(this);
    }
}

static void Dragonfly_WaitAfterAttack(EnDragonfly* this, PlayState* play) {
    this->bobPhase += 0xAF0;
    Math_ApproachF(&this->actor.world.pos.y, Math_SinS(this->bobPhase) * 10.0f + this->baseFlyHeight, 0.1f, 10.0f);
    if (this->waitTimer == 0) {
        this->collider.elements[0].info.toucherFlags |= TOUCH_ON;
        Dragonfly_RaiseTail(this);
    }
}

static void Dragonfly_Thaw(EnDragonfly* this, PlayState* play) {
    if (!this->isFrozen) {
        return;
    }
    this->isFrozen = false;
    PlaySfx(&this->actor, MM_SE_EV_ICE_BROKEN, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < 4; i++) {
        Vec3f pos = this->actor.world.pos;

        pos.x += Rand_CenteredFloat(30.0f);
        pos.y += Rand_CenteredFloat(20.0f);
        pos.z += Rand_CenteredFloat(30.0f);
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &pos, 150, 150, 150, 250, 235, 245, 255, 0.8f);
    }
}

static void Dragonfly_SetupDamaged(EnDragonfly* this, PlayState* play) {
    Dragonfly_ChangeAnim(this, sDragonflyDamageAnim, ANIMMODE_ONCE, -3.0f);
    this->actor.speedXZ = 0.0f;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->approachSpeed = 0.0f;
    this->collider.elements[1].info.toucherFlags &= ~TOUCH_ON;
    this->damagedVelocity.x = -20.0f * Math_SinS(this->actor.yawTowardsPlayer);
    this->damagedVelocity.y = 0.0f;
    this->damagedVelocity.z = -20.0f * Math_CosS(this->actor.yawTowardsPlayer);
    Dragonfly_Thaw(this, play);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    this->actionFunc = Dragonfly_Damaged;
}

static void Dragonfly_Damaged(EnDragonfly* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.world.rot.z, 0, 5, 0x3E8, 5);
    if (this->actor.colorFilterTimer == 0) {
        Dragonfly_RaiseTail(this);
    }
}

static void Dragonfly_SetupDead(EnDragonfly* this, PlayState* play) {
    Dragonfly_ChangeAnim(this, sDragonflyDeadAnim, ANIMMODE_ONCE, 0.0f);
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.speedXZ = 0.0f;
    this->approachSpeed = 0.0f;
    this->actor.velocity.y = 5.0f;
    this->actor.gravity = -0.5f;
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 25);
    Enemy_StartFinishingBlow(play, &this->actor);
    PlaySfx(&this->actor, MM_SE_EN_BATTA_DEAD, NA_SE_EN_FFLY_DEAD);
    this->actionFunc = Dragonfly_Dead;
}

static void Dragonfly_Dead(EnDragonfly* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.world.rot.z, 0, 5, 0x3E8, 5);
    if (Dragonfly_IsAnimDone(this)) {
        Dragonfly_Thaw(this, play);
        Dragonfly_ChangeAnim(this, sDragonflyFallAnim, ANIMMODE_ONCE, 0.0f);
        this->actionFunc = Dragonfly_Fall;
    }
}

static void Dragonfly_Burn(EnDragonfly* this, PlayState* play, f32 groundY) {
    static Vec3f sSpread[] = {
        { 1.0f, 0.0f, 0.5f },   { 1.0f, 0.0f, -0.5f },  { -1.0f, 0.0f, 0.5f },
        { -1.0f, 0.0f, -0.5f }, { 0.5f, 0.0f, 1.0f },   { -0.5f, 0.0f, 1.0f },
        { 0.5f, 0.0f, -1.0f },  { -0.5f, 0.0f, -1.0f }, { 0.0f, 0.0f, 0.0f },
    };

    for (s32 i = 0; i < ARRAY_COUNT(sSpread); i++) {
        Vec3f pos = { this->actor.world.pos.x + Rand_CenteredFloat(30.0f), groundY,
                      this->actor.world.pos.z + Rand_CenteredFloat(30.0f) };

        EffectSsDeadDb_Spawn(play, &pos, &sSpread[i], &sSpread[i], 100, 0, 255, 255, 255, 255, 0, 0, 255, 1, 9, false);
    }
    PlaySfx(&this->actor, MM_SE_EN_COMMON_EXTINCT_LEV, NA_SE_EN_EXTINCT);
    Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos, 0x60);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

// Spins down and bursts into blue flames where its tail touches ground or water.
static void Dragonfly_Fall(EnDragonfly* this, PlayState* play) {
    WaterBox* waterBox;
    f32 waterSurface;

    this->actor.shape.rot.y += 0x1388;
    if (this->actor.floorHeight <= BGCHECK_Y_MIN) {
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
        Actor_Kill(&this->actor);
        return;
    }
    bool hasWater = WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z,
                                         &waterSurface, &waterBox);

    if (hasWater && this->actor.world.pos.y < waterSurface) {
        Vec3f splash = this->actor.world.pos;

        splash.y = waterSurface;
        EffectSsGSplash_Spawn(play, &splash, NULL, NULL, 0, 400);
        Dragonfly_Burn(this, play, waterSurface);
    } else if (BgCheck_SphVsFirstPoly(&play->colCtx, &this->tailTipPos, 10.0f)) {
        Dragonfly_Burn(this, play, this->actor.floorHeight);
    }
}

static bool Dragonfly_IsVulnerable(EnDragonfly* this) {
    return this->actionFunc != Dragonfly_Damaged && this->actionFunc != Dragonfly_Dead &&
           this->actionFunc != Dragonfly_Fall;
}

static void Dragonfly_CheckDamage(EnDragonfly* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    if (!Dragonfly_IsVulnerable(this)) {
        return;
    }
    u8 effect = this->actor.colChkInfo.damageEffect;

    if (effect == DRAGONFLY_DMGEFF_IMMUNE || effect == DRAGONFLY_DMGEFF_HOOK) {
        return;
    }
    if (effect == DRAGONFLY_DMGEFF_FREEZE) {
        this->isFrozen = true;
        Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 80);
    }
    Actor_ApplyDamage(&this->actor);
    if (this->actor.colChkInfo.health > 0) {
        PlaySfx(&this->actor, MM_SE_EN_BATTA_DAMAGE, NA_SE_EN_FFLY_ATTACK);
        Dragonfly_SetupDamaged(this, play);
    } else {
        Dragonfly_SetupDead(this, play);
    }
}

static void Dragonfly_UpdateSparks(EnDragonfly* this) {
    for (s32 i = 0; i < DRAGONFLY_SPARK_COUNT; i++) {
        DragonflySpark* spark = &this->sparks[i];

        if (!spark->isEnabled) {
            continue;
        }
        if (spark->timer != 0) {
            spark->timer--;
            spark->frame = (spark->frame + 1) % DRAGONFLY_LIGHTNING_FRAMES;
            Math_ApproachF(&spark->scale, 0.1f, 0.1f, 0.05f);
        } else {
            Math_ApproachZeroF(&spark->scale, 0.1f, 0.1f);
            spark->isEnabled = spark->scale >= 0.02f;
        }
    }
}

static void Dragonfly_ApplyKnockback(EnDragonfly* this) {
    this->actor.world.pos.x += this->damagedVelocity.x;
    this->actor.world.pos.y += this->damagedVelocity.y;
    this->actor.world.pos.z += this->damagedVelocity.z;
    Math_ApproachZeroF(&this->damagedVelocity.x, 1.0f, 2.0f);
    Math_ApproachZeroF(&this->damagedVelocity.y, 1.0f, 2.0f);
    Math_ApproachZeroF(&this->damagedVelocity.z, 1.0f, 2.0f);
}

static void EnDragonfly_Init(Actor* thisx, PlayState* play) {
    EnDragonfly* this = (EnDragonfly*)thisx;

    this->actor.targetMode = 4;
    this->actor.colChkInfo.mass = 60;
    this->actor.colChkInfo.health = DRAGONFLY_HEALTH;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sColliderInit, this->colliderElements);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sDragonflySkel, (AnimationHeader*)sDragonflyFlyAnim,
                   this->jointTable, this->morphTable, DRAGONFLY_LIMB_MAX);
    Actor_SetScale(&this->actor, 0.01f);
    this->flyingHomePos = this->actor.home.pos;
    Dragonfly_SetupFly(this);
}

static void EnDragonfly_Destroy(Actor* thisx, PlayState* play) {
    EnDragonfly* this = (EnDragonfly*)thisx;

    Collider_DestroyJntSph(play, &this->collider);
}

static bool Dragonfly_IsStinging(EnDragonfly* this) {
    return this->actionFunc == Dragonfly_ApproachPlayer || this->actionFunc == Dragonfly_Attack;
}

static void EnDragonfly_Update(Actor* thisx, PlayState* play) {
    EnDragonfly* this = (EnDragonfly*)thisx;

    if (this->isFrozen && this->actor.colorFilterTimer != 0 && Dragonfly_IsVulnerable(this)) {
        Dragonfly_CheckDamage(this, play);
        return;
    }
    Dragonfly_Thaw(this, play);
    SkelAnime_Update(&this->skelAnime);
    Dragonfly_CheckDamage(this, play);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    Actor_SetFocus(&this->actor, 0.0f);
    DECR(this->timer);
    DECR(this->waitTimer);
    Actor_MoveXZGravity(&this->actor);
    Dragonfly_ApplyKnockback(this);
    if (this->actionFunc != Dragonfly_Fall) {
        Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 60.0f, 60.0f, BGCHECK_UPDATE_WALL_FLOOR);
    }
    this->actor.shape.rot.z = this->actor.world.rot.z;
    if (this->actionFunc != Dragonfly_Fall && this->actionFunc != Dragonfly_ApproachPlayer) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.world.rot.y, 5, 0x3E8, 5);
    }
    Dragonfly_UpdateSparks(this);
    if (this->actionFunc == Dragonfly_Dead || this->actionFunc == Dragonfly_Fall) {
        return;
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    if (Dragonfly_IsStinging(this)) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}

static void Dragonfly_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnDragonfly* this = (EnDragonfly*)thisx;

    Collider_UpdateSpheres(limbIndex, &this->collider);
    if (limbIndex == DRAGONFLY_LIMB_TAIL_TIP) {
        Matrix_MultVec3f(&sZeroVec, &this->tailTipPos);
        Matrix_MultVec3f(&sSparkOffset, &this->sparkOrigin);
    }
}

static void Dragonfly_DrawSparks(EnDragonfly* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    for (s32 i = 0; i < DRAGONFLY_SPARK_COUNT; i++) {
        DragonflySpark* spark = &this->sparks[i];

        if (!spark->isEnabled) {
            continue;
        }
        Matrix_Translate(this->sparkOrigin.x + spark->offset.x, this->sparkOrigin.y + spark->offset.y,
                         this->sparkOrigin.z + spark->offset.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_RotateZ(BINANG_TO_RAD((s16)spark->yaw), MTXMODE_APPLY);
        Matrix_Scale(spark->scale, spark->scale, spark->scale, MTXMODE_APPLY);
        Gfx_SetupDL_61Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sLightningTextures[spark->frame]);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 200, 255, 255, 255);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffLightningDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnDragonfly_Draw(Actor* thisx, PlayState* play) {
    EnDragonfly* this = (EnDragonfly*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, NULL, Dragonfly_PostLimbDraw, this);
    Dragonfly_DrawSparks(this, play);
}

// MM's CanKillEnemy(ACTOR_EN_GRASSHOPPER): a sword, explosives, Deku Nuts or a form attack.
static bool Dragonfly_CanKillInLogic(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicHasCapability("HasExplosives") ||
           sApi->LogicCanUseItem(RG_NUTS);
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
    static Vec3s sSoulJointTable[DRAGONFLY_LIMB_MAX];
    static Vec3s sSoulMorphTable[DRAGONFLY_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_Init(play, &sSoulSkelAnime, (SkeletonHeader*)sDragonflySkel, (AnimationHeader*)sDragonflyFlyAnim,
                       sSoulJointTable, sSoulMorphTable, DRAGONFLY_LIMB_MAX);
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
    SkelAnime_DrawOpa(play, sSoulSkelAnime.skeleton, sSoulSkelAnime.jointTable, NULL, NULL, NULL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterSoul(void) {
    static const SOHCustomItemRandomizer sSoulRandomizer = {
        .structSize = sizeof(SOHCustomItemRandomizer),
        .flags = SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT,
        .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
    };
    SOHCustomItemDefinition soul = Z64Items_Define(DRAGONFLY_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Dragonflies";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Dragonflies%w!&Dragonflies can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " DRAGONFLY_SOUL_ITEM_KEY);
    }
}

static void RegisterDragonfly(void) {
    SOHActorDefinition dragonfly = { 0 };

    dragonfly.structSize = sizeof(dragonfly);
    dragonfly.key = DRAGONFLY_KEY;
    dragonfly.description = "Dragonfly";
    dragonfly.category = ACTORCAT_ENEMY;
    dragonfly.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR;
    dragonfly.objectId = OBJECT_GAMEPLAY_KEEP;
    dragonfly.instanceSize = sizeof(EnDragonfly);
    dragonfly.init = EnDragonfly_Init;
    dragonfly.destroy = EnDragonfly_Destroy;
    dragonfly.update = EnDragonfly_Update;
    dragonfly.draw = EnDragonfly_Draw;
    dragonfly.enemyFlags = SOH_ACTOR_ENEMY;
    dragonfly.enemySpawnHeight = 90;
    dragonfly.naviHint = "Dragonfly&%cIts tail is charged with electricity! Strike it while it hovers above you.%w";
    dragonfly.soulKey = DRAGONFLY_KEY;
    dragonfly.logicKill = Dragonfly_CanKillInLogic;
    if (sApi->RegisterActor(&dragonfly) < 0) {
        sApi->Log("Could not register " DRAGONFLY_KEY);
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
    RegisterDragonfly();
}
