// Majora's Mask's Big Poe (En_Bigpo, the Beneath the Well kind) as an Unbound custom enemy, drawn from mm.o2r.
// The three-flame entrance plays without its cutscene camera, and the lantern leaves drops instead of a soul to bottle.
#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define BIG_POE_KEY "mm.big_poe"
#define POE_FAMILY_KEY "mm.poe"
#define POE_SOUL_ITEM_KEY "soul.mm.poe"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_BIG_POE "__OTR__objects/object_bigpo/"

#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL (1 << 0)
#define BGCHECK_UPDATE_FLOOR (1 << 2)

#define MM_SE_EN_STALKIDS_APPEAR 0x3873
#define MM_SE_EN_WIZ_DISAPPEAR 0x3877
#define MM_SE_EN_COMMON_EXTINCT_LEV 0x3A1F
#define MM_SE_EV_METAL_BOX_BOUND 0x28E0
#define MM_SE_EN_PO_FLY 0x3871
#define MM_SE_EN_PO_DISAPPEAR 0x3874
#define MM_SE_EN_PO_DAMAGE 0x3875
#define MM_SE_EN_PO_DEAD 0x3876
#define MM_SE_EN_PO_LAUGH 0x38EC
#define MM_SE_EN_PO_ROLL 0x38EE

#define BIG_POE_HEALTH 10
#define BIG_POE_WAKE_RANGE 400.0f
#define BIG_POE_LEASH_RADIUS 300.0f
#define BIG_POE_MAX_FLOOR_DROP 600.0f
#define BIG_POE_WARP_TRIES 8
#define BIG_POE_SWOOP_FRAMES 60
#define BIG_POE_SCALE 0.014f
#define BIG_POE_FIRE_COUNT 3
#define BIG_POE_LOCK_ON_PATIENCE 40
#define BIG_POE_DMGEFF_HOOKSHOT 0xF
#define BIG_POE_DMGEFF_LIGHT 0x4

typedef enum {
    BIG_POE_LIMB_NONE,
    BIG_POE_LIMB_FACE,
    BIG_POE_LIMB_LEFT_UPPER_ARM,
    BIG_POE_LIMB_LEFT_FOREARM,
    BIG_POE_LIMB_RIGHT_UPPER_ARM,
    BIG_POE_LIMB_RIGHT_FOREARM,
    BIG_POE_LIMB_RIGHT_HAND,
    BIG_POE_LIMB_LANTERN,
    BIG_POE_LIMB_HAT_AND_CLOAK,
    BIG_POE_LIMB_LOWER_ROBE,
    BIG_POE_LIMB_MAX
} BigPoeLimb;

struct EnBigPoe;

typedef void (*EnBigPoeActionFunc)(struct EnBigPoe*, PlayState*);

typedef struct {
    Vec3f pos;
    LightNode* light;
    LightInfo info;
} BigPoeFire;

typedef struct EnBigPoe {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[BIG_POE_LIMB_MAX];
    Vec3s morphTable[BIG_POE_LIMB_MAX];
    EnBigPoeActionFunc actionFunc;
    bool isFightStarted;
    s16 timer;
    s16 targetYaw;
    s16 angularVelocity;
    s16 lockOnFrames;
    s16 hoverCycle;
    f32 fireRadius;
    f32 hoverHeight;
    Color_RGBA8 mainColor;
    Color_RGBA8 lanternColor;
    ColliderCylinder collider;
    MtxF lanternMtx;
    BigPoeFire fires[BIG_POE_FIRE_COUNT];
} EnBigPoe;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sBigPoeSkel[] = OBJ_BIG_POE "gBigPoeSkel";
static const ALIGN_ASSET(2) char sBigPoeFloatAnim[] = OBJ_BIG_POE "gBigPoeFloatAnim";
static const ALIGN_ASSET(2) char sBigPoeAwakenStretchAnim[] = OBJ_BIG_POE "gBigPoeAwakenStretchAnim";
static const ALIGN_ASSET(2) char sBigPoeShockAnim[] = OBJ_BIG_POE "gBigPoeShockAnim";
static const ALIGN_ASSET(2) char sBigPoeCrispyBodyDL[] = OBJ_BIG_POE "gBigPoeCrispyBodyDL";
static const ALIGN_ASSET(2) char sBigPoeLanternMainDL[] = OBJ_BIG_POE "gBigPoeLanternMainDL";
static const ALIGN_ASSET(2) char sBigPoeLanternPurpleTopDL[] = OBJ_BIG_POE "gBigPoeLanternPurpleTopDL";
static const ALIGN_ASSET(2) char sBigPoeLanternFallingDL[] = OBJ_BIG_POE "gBigPoeLanternFallingDL";

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_HIT3, AT_NONE | AT_TYPE_ENEMY, AC_NONE | AC_TYPE_PLAYER, OC1_NONE | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x10 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_ON | BUMP_HOOKABLE,
      OCELEM_ON },
    { 35, 100, 10, { 0, 0, 0 } },
};

static CollisionCheckInfoInit sColChkInfoInit = { BIG_POE_HEALTH, 35, 100, 50 };

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, 0x0),
    /* Deku stick    */ DMG_ENTRY(2, 0x0),
    /* Slingshot     */ DMG_ENTRY(1, 0x0),
    /* Explosive     */ DMG_ENTRY(2, 0x0),
    /* Boomerang     */ DMG_ENTRY(1, 0x0),
    /* Normal arrow  */ DMG_ENTRY(2, 0x0),
    /* Hammer swing  */ DMG_ENTRY(2, 0x0),
    /* Hookshot      */ DMG_ENTRY(0, BIG_POE_DMGEFF_HOOKSHOT),
    /* Kokiri sword  */ DMG_ENTRY(1, 0x0),
    /* Master sword  */ DMG_ENTRY(2, 0x0),
    /* Giant's Knife */ DMG_ENTRY(4, 0x0),
    /* Fire arrow    */ DMG_ENTRY(2, 0x0),
    /* Ice arrow     */ DMG_ENTRY(2, 0x0),
    /* Light arrow   */ DMG_ENTRY(4, BIG_POE_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, 0x0),
    /* Unk arrow 2   */ DMG_ENTRY(2, 0x0),
    /* Unk arrow 3   */ DMG_ENTRY(2, 0x0),
    /* Fire magic    */ DMG_ENTRY(2, 0x0),
    /* Ice magic     */ DMG_ENTRY(2, 0x0),
    /* Light magic   */ DMG_ENTRY(4, BIG_POE_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, 0x0),
    /* Mirror Ray    */ DMG_ENTRY(0, 0x0),
    /* Kokiri spin   */ DMG_ENTRY(2, 0x0),
    /* Giant spin    */ DMG_ENTRY(8, 0x0),
    /* Master spin   */ DMG_ENTRY(4, 0x0),
    /* Kokiri jump   */ DMG_ENTRY(2, 0x0),
    /* Giant jump    */ DMG_ENTRY(8, 0x0),
    /* Master jump   */ DMG_ENTRY(4, 0x0),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(0, 0x0),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

#define DMG_KNOCKS_ALONG_SHOT (DMG_ARROW_NORMAL | DMG_ARROW_FIRE | DMG_ARROW_ICE | DMG_ARROW_LIGHT | DMG_SLINGSHOT)

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sBurnRise = { 0.0f, 3.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

static void BigPoe_AppearStage2(EnBigPoe* this, PlayState* play);
static void BigPoe_AppearStage4(EnBigPoe* this, PlayState* play);
static void BigPoe_AppearStage6(EnBigPoe* this, PlayState* play);
static void BigPoe_AppearStage8(EnBigPoe* this, PlayState* play);
static void BigPoe_WaitForProximity(EnBigPoe* this, PlayState* play);
static void BigPoe_WarpingOut(EnBigPoe* this, PlayState* play);
static void BigPoe_WarpingIn(EnBigPoe* this, PlayState* play);
static void BigPoe_IdleFlying(EnBigPoe* this, PlayState* play);
static void BigPoe_SpinningUp(EnBigPoe* this, PlayState* play);
static void BigPoe_SpinAttack(EnBigPoe* this, PlayState* play);
static void BigPoe_SpinningDown(EnBigPoe* this, PlayState* play);
static void BigPoe_CheckHealth(EnBigPoe* this, PlayState* play);
static void BigPoe_BurnAway(EnBigPoe* this, PlayState* play);
static void BigPoe_LanternFalling(EnBigPoe* this, PlayState* play);
static void BigPoe_SetupIdleFlying(EnBigPoe* this);
static void BigPoe_SetupWarpOut(EnBigPoe* this);
static void BigPoe_DrawMain(Actor* thisx, PlayState* play);
static void BigPoe_DrawLantern(Actor* thisx, PlayState* play);
static void BigPoe_DrawCircleFlames(Actor* thisx, PlayState* play);

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

static void BigPoe_StartMinibossMusic(EnBigPoe* this) {
    if (this->isFightStarted) {
        return;
    }
    this->isFightStarted = true;
    func_800F5ACC(NA_BGM_MINI_BOSS);
}

static void BigPoe_StopMinibossMusic(EnBigPoe* this) {
    if (!this->isFightStarted) {
        return;
    }
    this->isFightStarted = false;
    func_800F5B58();
}

static void BigPoe_RotateAppearFires(EnBigPoe* this) {
    for (s32 i = 0; i < BIG_POE_FIRE_COUNT; i++) {
        BigPoeFire* fire = &this->fires[i];

        fire->pos.x = Math_SinS(this->actor.shape.rot.y) * this->fireRadius + this->actor.world.pos.x;
        fire->pos.z = Math_CosS(this->actor.shape.rot.y) * this->fireRadius + this->actor.world.pos.z;
        this->actor.shape.rot.y += 0x5555;
    }
}

static void BigPoe_RaiseAppearFires(EnBigPoe* this) {
    for (s32 i = 0; i < BIG_POE_FIRE_COUNT; i++) {
        this->fires[i].pos.y += this->actor.velocity.y;
    }
}

static void BigPoe_UpdateSpin(EnBigPoe* this) {
    s16 previousYaw = this->actor.shape.rot.y;

    this->actor.shape.rot.y += this->angularVelocity;
    if (previousYaw < 0 && this->actor.shape.rot.y > 0) {
        PlaySfx(&this->actor, MM_SE_EN_PO_ROLL, NA_SE_EN_PO_ROLL);
    }
}

static void BigPoe_SetupWaitForProximity(EnBigPoe* this) {
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->fireRadius = 200.0f;
    this->actionFunc = BigPoe_WaitForProximity;
}

static void BigPoe_AppearStage1(EnBigPoe* this) {
    this->actor.draw = BigPoe_DrawCircleFlames;
    this->actor.shape.rot.y = BINANG_ROT180(this->actor.yawTowardsPlayer);
    BigPoe_RotateAppearFires(this);
    for (s32 i = 0; i < BIG_POE_FIRE_COUNT; i++) {
        this->fires[i].pos.y = this->actor.world.pos.y;
    }
    this->actor.scale.x = 0.0f;
    this->actor.scale.y = 0.015f;
    this->actor.scale.z = 0.0f;
    this->actionFunc = BigPoe_AppearStage2;
}

static void BigPoe_WaitForProximity(EnBigPoe* this, PlayState* play) {
    if (this->actor.xzDistToPlayer < BIG_POE_WAKE_RANGE) {
        BigPoe_AppearStage1(this);
    }
}

static void BigPoe_AppearStage3(EnBigPoe* this) {
    this->angularVelocity = 0x1000;
    this->fireRadius = 200.0f;
    this->actor.velocity.y = 0.0f;
    this->actionFunc = BigPoe_AppearStage4;
}

static void BigPoe_AppearStage2(EnBigPoe* this, PlayState* play) {
    if (Math_StepToF(&this->actor.scale.x, 0.01f, 0.001f)) {
        BigPoe_AppearStage3(this);
    }
    this->actor.scale.z = this->actor.scale.x;
    this->actor.scale.y = (0.01f - this->actor.scale.x) * 0.5f + 0.01f;
}

static void BigPoe_AppearStage5(EnBigPoe* this) {
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sBigPoeAwakenStretchAnim);
    this->actor.draw = BigPoe_DrawMain;
    Actor_SetScale(&this->actor, BIG_POE_SCALE);
    PlaySfx(&this->actor, MM_SE_EN_STALKIDS_APPEAR, NA_SE_EN_PO_APPEAR);
    this->actionFunc = BigPoe_AppearStage6;
}

// The three flames circle in, touch and rise.
static void BigPoe_AppearStage4(EnBigPoe* this, PlayState* play) {
    if (Math_StepToF(&this->fireRadius, 30.0f, 5.0f)) {
        this->angularVelocity += 0x80;
        this->actor.velocity.y += 0.25f;
    }
    this->actor.shape.rot.y += this->angularVelocity;
    BigPoe_RotateAppearFires(this);
    BigPoe_RaiseAppearFires(this);
    this->actor.world.pos.y += this->actor.velocity.y;
    if (this->actor.velocity.y >= 4.0f) {
        BigPoe_AppearStage5(this);
    }
}

static void BigPoe_AppearStage7(EnBigPoe* this) {
    this->timer = 15;
    BigPoe_StartMinibossMusic(this);
    this->actionFunc = BigPoe_AppearStage8;
}

// The Big Poe fades in inside the flames while they slow down and fade out.
static void BigPoe_AppearStage6(EnBigPoe* this, PlayState* play) {
    s32 alpha = this->mainColor.a + 10;

    SkelAnime_Update(&this->skelAnime);
    this->actor.shape.rot.y += this->angularVelocity;
    BigPoe_RotateAppearFires(this);
    if (alpha >= 90) {
        this->angularVelocity -= 0x80;
        this->actor.velocity.y -= 0.25f;
        if (alpha >= 180) {
            Math_ScaledStepToS(&this->actor.world.rot.y, 0, 0x180);
        }
    }
    this->actor.world.pos.y += this->actor.velocity.y;
    BigPoe_RaiseAppearFires(this);
    if (alpha >= 255) {
        this->mainColor.a = 255;
        BigPoe_AppearStage7(this);
    } else {
        this->mainColor.a = alpha;
    }
}

static void BigPoe_AppearStage8(EnBigPoe* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->timer--;
    if (this->timer == 0) {
        BigPoe_SetupIdleFlying(this);
    }
}

static void BigPoe_SetupWarpOut(EnBigPoe* this) {
    this->collider.base.acFlags &= ~AC_ON;
    this->collider.base.ocFlags1 &= ~OC1_ON;
    this->angularVelocity = 0x2000;
    this->timer = 32;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.speedXZ = 0.0f;
    PlaySfx(&this->actor, MM_SE_EN_PO_DISAPPEAR, NA_SE_EN_PO_DISAPPEAR);
    this->actionFunc = BigPoe_WarpingOut;
}

// MM's arenas are closed rooms; anywhere else it must keep a floor in reach below it.
static bool BigPoe_HasFloorBelow(PlayState* play, Vec3f* pos) {
    CollisionPoly* poly;
    Vec3f probe = { pos->x, pos->y + 50.0f, pos->z };
    f32 floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);

    return floorY > BGCHECK_Y_MIN && pos->y - floorY < BIG_POE_MAX_FLOOR_DROP;
}

// Reappears at a random spot around Link, no closer than 200 units, over floor and within its leash; else at home.
static void BigPoe_SetupWarpIn(EnBigPoe* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 distance = MAX(this->actor.xzDistToPlayer, 200.0f);
    Vec3f spot = this->actor.home.pos;

    spot.y = this->actor.world.pos.y;
    for (s32 i = 0; i < BIG_POE_WARP_TRIES; i++) {
        s16 yaw = ((s32)Rand_Next() >> 0x14) + this->actor.yawTowardsPlayer;
        Vec3f candidate = { Math_SinS(yaw) * distance + player->actor.world.pos.x, this->actor.world.pos.y,
                            Math_CosS(yaw) * distance + player->actor.world.pos.z };
        bool isNearHome = Math_Vec3f_DistXZ(&candidate, &this->actor.home.pos) <= BIG_POE_LEASH_RADIUS;

        if (isNearHome && BigPoe_HasFloorBelow(play, &candidate)) {
            spot = candidate;
            break;
        }
    }
    PlaySfx(&this->actor, MM_SE_EN_STALKIDS_APPEAR, NA_SE_EN_PO_APPEAR);
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sBigPoeAwakenStretchAnim);
    this->angularVelocity = 0x2000;
    this->actor.world.pos.x = spot.x;
    this->actor.world.pos.z = spot.z;
    this->actionFunc = BigPoe_WarpingIn;
}

static void BigPoe_WarpingOut(EnBigPoe* this, PlayState* play) {
    DECR(this->timer);
    this->actor.shape.rot.y += this->angularVelocity;
    if (this->timer < 16) {
        Math_ScaledStepToS(&this->angularVelocity, 0, 0x200);
    }
    this->mainColor.a = this->timer * (255.0f / 32.0f);
    if (this->timer == 0) {
        this->mainColor.a = 0;
        BigPoe_SetupWarpIn(this, play);
    }
}

static void BigPoe_WarpingIn(EnBigPoe* this, PlayState* play) {
    this->timer++;
    this->actor.shape.rot.y -= this->angularVelocity;
    if (this->timer >= 16) {
        Math_ScaledStepToS(&this->angularVelocity, 0, 0x200);
    }
    this->mainColor.a = this->timer * (255.0f / 32.0f);
    if (this->timer == 32) {
        this->mainColor.a = 255;
        BigPoe_StartMinibossMusic(this);
        BigPoe_SetupIdleFlying(this);
    }
}

// After a missed swoop it hovers for four seconds; after a reappearance it attacks at once.
static void BigPoe_SetupIdleFlying(EnBigPoe* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sBigPoeFloatAnim, -5.0f);
    this->timer = this->actionFunc == BigPoe_SpinningDown ? 80 : 0;
    this->hoverCycle = 40;
    this->actor.velocity.y = 0.0f;
    this->hoverHeight = this->actor.world.pos.y;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->collider.base.acFlags |= AC_ON;
    this->collider.base.ocFlags1 |= OC1_ON;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = BigPoe_IdleFlying;
}

static void BigPoe_SetupSpinUp(EnBigPoe* this) {
    this->collider.base.colType = COLTYPE_METAL;
    this->collider.base.acFlags |= AC_HARD;
    this->collider.base.atFlags |= AT_ON;
    this->angularVelocity = 0x800;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = BigPoe_SpinningUp;
}

static void BigPoe_IdleFlying(EnBigPoe* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);
    DECR(this->timer);
    this->hoverCycle = this->hoverCycle == 0 ? 40 : this->hoverCycle - 1;
    Math_StepToF(&this->hoverHeight, player->actor.world.pos.y + 100.0f, 1.5f);
    this->actor.world.pos.y = Math_SinF(this->hoverCycle * (M_PI / 20)) * 10.0f + this->hoverHeight;
    Math_StepToF(&this->actor.speedXZ, 3.0f, 0.2f);
    PlaySfx(&this->actor, MM_SE_EN_PO_FLY - SFX_FLAG, NA_SE_EN_PO_FLY - SFX_FLAG);
    if (Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > BIG_POE_LEASH_RADIUS) {
        this->targetYaw = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
    }
    if (Math_ScaledStepToS(&this->actor.shape.rot.y, this->targetYaw, 0x200) && Rand_ZeroOne() < 0.075f) {
        s16 turn = (Rand_Next() >> 0x14) + 0x1000;

        this->targetYaw += Rand_ZeroOne() < 0.5f ? -turn : turn;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (this->timer == 0) {
        BigPoe_SetupSpinUp(this);
    }
}

static void BigPoe_SetupSpinAttack(EnBigPoe* this) {
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->timer = BIG_POE_SWOOP_FRAMES;
    this->actionFunc = BigPoe_SpinAttack;
}

static void BigPoe_SpinningUp(EnBigPoe* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->angularVelocity += 0x200;
    BigPoe_UpdateSpin(this);
    if (this->angularVelocity >= 0x3C00) {
        BigPoe_SetupSpinAttack(this);
    }
}

static void BigPoe_SetupSpinDown(EnBigPoe* this) {
    this->collider.base.atFlags &= ~AT_ON;
    this->actionFunc = BigPoe_SpinningDown;
}

// The swoop ends when it hits Link or has flown past him; a wall or a pit edge that holds it back ends it on a timer.
static void BigPoe_SpinAttack(EnBigPoe* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    DECR(this->timer);
    SkelAnime_Update(&this->skelAnime);
    Math_StepToF(&this->actor.speedXZ, 10.0f, 1.0f);
    Math_SmoothStepToF(&this->actor.world.pos.y, player->actor.world.pos.y, 0.3f, 7.5f, 1.0f);
    BigPoe_UpdateSpin(this);

    s16 yawOffPlayer = this->actor.yawTowardsPlayer - this->actor.world.rot.y;
    bool hasPassedPlayer = ABS(yawOffPlayer) > 0x4000 && this->actor.xzDistToPlayer > 50.0f;

    if ((this->collider.base.atFlags & AT_HIT) || hasPassedPlayer || this->timer == 0) {
        BigPoe_SetupSpinDown(this);
    }
}

static void BigPoe_SpinningDown(EnBigPoe* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToF(&this->actor.world.pos.y, player->actor.world.pos.y + 100.0f, 0.3f, 5.0f, 1.0f);
    Math_StepToF(&this->actor.speedXZ, 0.0f, 0.2f);
    if (Math_ScaledStepToS(&this->angularVelocity, 0, 0x200)) {
        this->collider.base.colType = COLTYPE_HIT3;
        this->collider.base.acFlags &= ~AC_HARD;
        BigPoe_SetupIdleFlying(this);
    }
    BigPoe_UpdateSpin(this);
}

static void BigPoe_SetupHitStun(EnBigPoe* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sBigPoeShockAnim, -6.0f);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 16);
    this->collider.base.acFlags &= ~AC_ON;

    Actor* attacker = this->collider.base.ac;
    ColliderInfo* hitBy = this->collider.info.acHitInfo;

    if (attacker != NULL && hitBy != NULL && (hitBy->toucher.dmgFlags & DMG_KNOCKS_ALONG_SHOT)) {
        this->actor.world.rot.y = attacker->shape.rot.y;
    } else if (attacker != NULL) {
        this->actor.world.rot.y = Actor_WorldYawTowardActor(attacker, &this->actor);
    }
    this->actor.speedXZ = 5.0f;
    this->actionFunc = BigPoe_CheckHealth;
}

static void BigPoe_SetupBurnAway(EnBigPoe* this) {
    this->timer = 0;
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->collider.base.ocFlags1 &= ~OC1_ON;
    this->actionFunc = BigPoe_BurnAway;
}

static void BigPoe_CheckHealth(EnBigPoe* this, PlayState* play) {
    Math_StepToF(&this->actor.speedXZ, 0.0f, 0.5f);
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (this->actor.colChkInfo.health == 0) {
        BigPoe_SetupBurnAway(this);
    } else {
        BigPoe_SetupWarpOut(this);
    }
}

static void BigPoe_SpawnBurnFlame(PlayState* play, Vec3f* pos, s16 scale) {
    EffectSsDeadDb_Spawn(play, pos, &sBurnRise, &sZeroVec, scale, 0, 255, 255, 255, 255, 0, 0, 255, 1, 9, false);
}

// Three blue flames rise on the camera-facing sides of the ghost while it burns.
static void BigPoe_SpawnBurnFlames(EnBigPoe* this, PlayState* play) {
    s16 cameraYaw = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x4800;
    f32 scaleFactor = BIG_POE_SCALE * 100.0f;
    Vec3f pos;

    if (this->timer < 5) {
        s16 arc = this->timer * 0x1000 - 0x4000;
        f32 radius = Math_CosS(arc) * 32.2f;

        pos.y = (Math_SinS(arc) * 23.0f + 40.0f) * scaleFactor + this->actor.world.pos.y;
        pos.x = Math_SinS(cameraYaw) * radius + this->actor.world.pos.x;
        pos.z = Math_CosS(cameraYaw) * radius + this->actor.world.pos.z;
    } else {
        pos.y = this->actor.world.pos.y + (40.0f + 15.0f * (this->timer - 5)) * scaleFactor;
        pos.x = Math_SinS(cameraYaw) * 32.2f + this->actor.world.pos.x;
        pos.z = Math_CosS(cameraYaw) * 32.2f + this->actor.world.pos.z;
    }
    s16 scale = (this->timer * 10 + 80) * scaleFactor;

    BigPoe_SpawnBurnFlame(play, &pos, scale);
    pos.x = 2.0f * this->actor.world.pos.x - pos.x;
    pos.z = 2.0f * this->actor.world.pos.z - pos.z;
    BigPoe_SpawnBurnFlame(play, &pos, scale);
    pos.x = this->actor.world.pos.x;
    pos.z = this->actor.world.pos.z;
    BigPoe_SpawnBurnFlame(play, &pos, scale);
}

static void BigPoe_SetupLanternDrop(EnBigPoe* this, PlayState* play) {
    this->actor.draw = BigPoe_DrawLantern;
    this->actor.shape.shadowDraw = NULL;
    this->actor.world.pos.x = this->lanternMtx.xw;
    this->actor.world.pos.y = this->lanternMtx.yw - 15.0f;
    this->actor.world.pos.z = this->lanternMtx.zw;
    Actor_SetScale(&this->actor, BIG_POE_SCALE);
    this->actor.gravity = -1.0f;
    this->actor.shape.yOffset = 1500.0f;
    this->actor.shape.rot.x = -0x8000;
    this->actor.velocity.y = 0.0f;
    Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_MISC);
    this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    this->actionFunc = BigPoe_LanternFalling;
}

static void BigPoe_BurnAway(EnBigPoe* this, PlayState* play) {
    this->timer++;
    if (this->timer < 8) {
        BigPoe_SpawnBurnFlames(this, play);
    } else if (this->timer >= 28) {
        BigPoe_SetupLanternDrop(this, play);
    } else if (this->timer >= 19) {
        Actor_SetScale(&this->actor, (28 - this->timer) * BIG_POE_SCALE * 0.1f);
        this->actor.world.pos.y += 5.0f;
    }
    if (this->timer < 18) {
        PlaySfx(&this->actor, MM_SE_EN_COMMON_EXTINCT_LEV, NA_SE_EN_EXTINCT);
    } else if (this->timer == 18) {
        PlaySfx(&this->actor, MM_SE_EN_WIZ_DISAPPEAR, NA_SE_EN_EXTINCT);
    }
}

static void BigPoe_Vanish(EnBigPoe* this, PlayState* play) {
    EffectSsHahen_SpawnBurst(play, &this->actor.world.pos, 6.0f, 0, 1, 1, 15, -1, 10, (Gfx*)sBigPoeLanternFallingDL);
    PlaySfx(&this->actor, MM_SE_EV_METAL_BOX_BOUND, NA_SE_EV_METAL_BOX_BOUND);
    Item_DropCollectible(play, &this->actor.world.pos, ITEM00_RUPEE_PURPLE);
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x80);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

static void BigPoe_LanternFalling(EnBigPoe* this, PlayState* play) {
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || this->actor.floorHeight == BGCHECK_Y_MIN) {
        BigPoe_Vanish(this, play);
    }
}

static void BigPoe_UpdateColor(EnBigPoe* this) {
    if (this->actionFunc == BigPoe_CheckHealth) {
        bool isFlashOff = this->actor.colorFilterTimer & 2;

        this->mainColor.r = isFlashOff ? 0 : 80;
        this->mainColor.g = isFlashOff ? 0 : 255;
        this->mainColor.b = isFlashOff ? 0 : 225;
        return;
    }
    this->mainColor.r = CLAMP_MAX(this->mainColor.r + 5, 255);
    this->mainColor.g = CLAMP_MAX(this->mainColor.g + 5, 255);
    if (this->mainColor.b > 210) {
        this->mainColor.b = MAX(this->mainColor.b - 5, 210);
    } else {
        this->mainColor.b = MIN(this->mainColor.b + 5, 210);
    }
}

static void BigPoe_FlickerLantern(EnBigPoe* this) {
    f32 flicker = Rand_ZeroOne();

    this->lanternColor.r = (s32)(flicker * 30.0f) + 225;
    this->lanternColor.g = (s32)(flicker * 100.0f) + 155;
    this->lanternColor.b = (s32)(flicker * 160.0f) + 95;
    this->lanternColor.a = (s32)(flicker * 30.0f) + 220;
}

static void BigPoe_TakeHit(EnBigPoe* this, PlayState* play) {
    if (Actor_ApplyDamage(&this->actor) == 0) {
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        PlaySfx(&this->actor, MM_SE_EN_PO_DEAD, NA_SE_EN_PO_DEAD);
        Enemy_StartFinishingBlow(play, &this->actor);
        BigPoe_StopMinibossMusic(this);
    } else {
        PlaySfx(&this->actor, MM_SE_EN_PO_DAMAGE, NA_SE_EN_PO_DAMAGE);
    }
    BigPoe_SetupHitStun(this);
}

// A hookshot only reels it in; the hard collider while spinning bounces every blow.
static bool BigPoe_CheckDamage(EnBigPoe* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return false;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    if (this->collider.base.acFlags & AC_HARD) {
        return false;
    }
    if (this->actor.colChkInfo.damageEffect != BIG_POE_DMGEFF_HOOKSHOT) {
        BigPoe_TakeHit(this, play);
    }
    return true;
}

static bool BigPoe_CanLoseInterest(EnBigPoe* this) {
    return this->actor.isTargeted && this->actionFunc != BigPoe_WarpingOut &&
           !(this->collider.base.acFlags & AC_HARD) && this->actor.category == ACTORCAT_ENEMY;
}

// Locked on for two seconds without being hit, it laughs and warps away.
static void BigPoe_UpdateLockOnPatience(EnBigPoe* this) {
    this->lockOnFrames = BigPoe_CanLoseInterest(this) ? this->lockOnFrames + 1 : 0;
    if (this->lockOnFrames == BIG_POE_LOCK_ON_PATIENCE) {
        PlaySfx(&this->actor, MM_SE_EN_PO_LAUGH, NA_SE_EN_PO_LAUGH);
        BigPoe_SetupWarpOut(this);
    }
}

static bool BigPoe_IsRisingWithFlames(EnBigPoe* this) {
    return this->actionFunc == BigPoe_AppearStage4 || this->actionFunc == BigPoe_AppearStage6;
}

// Walls stop it, and a step out over a pit is taken back and turned toward home.
static void BigPoe_KeepOverFloor(EnBigPoe* this, PlayState* play) {
    Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 35.0f, 60.0f, BGCHECK_UPDATE_WALL);
    if (BigPoe_HasFloorBelow(play, &this->actor.world.pos)) {
        return;
    }
    this->actor.world.pos.x = this->actor.prevPos.x;
    this->actor.world.pos.z = this->actor.prevPos.z;
    this->targetYaw = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
}

static void BigPoe_SubmitCollider(EnBigPoe* this, PlayState* play) {
    Collider_UpdateCylinder(&this->actor, &this->collider);
    if (this->collider.base.ocFlags1 & OC1_ON) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

static void EnBigPoe_Init(Actor* thisx, PlayState* play) {
    EnBigPoe* this = (EnBigPoe*)thisx;

    this->actor.targetArrowOffset = 3200.0f;
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sBigPoeSkel, (AnimationHeader*)sBigPoeFloatAnim,
                   this->jointTable, this->morphTable, BIG_POE_LIMB_MAX);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    for (s32 i = 0; i < BIG_POE_FIRE_COUNT; i++) {
        BigPoeFire* fire = &this->fires[i];

        fire->light = LightContext_InsertLight(play, &play->lightCtx, &fire->info);
        Lights_PointNoGlowSetInfo(&fire->info, this->actor.home.pos.x, this->actor.home.pos.y, this->actor.home.pos.z,
                                  255, 255, 255, 0);
    }
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 45.0f);
    this->hoverHeight = this->actor.home.pos.y + 100.0f;
    this->mainColor.r = 255;
    this->mainColor.g = 255;
    this->mainColor.b = 210;
    this->mainColor.a = 0;
    this->actor.draw = NULL;
    BigPoe_SetupWaitForProximity(this);
}

static void EnBigPoe_Destroy(Actor* thisx, PlayState* play) {
    EnBigPoe* this = (EnBigPoe*)thisx;

    BigPoe_StopMinibossMusic(this);
    for (s32 i = 0; i < BIG_POE_FIRE_COUNT; i++) {
        LightContext_RemoveLight(play, &play->lightCtx, this->fires[i].light);
    }
    Collider_DestroyCylinder(play, &this->collider);
}

static void EnBigPoe_Update(Actor* thisx, PlayState* play) {
    EnBigPoe* this = (EnBigPoe*)thisx;

    if (CHECK_FLAG_ALL(this->actor.flags, ACTOR_FLAG_HOOKSHOT_ATTACHED)) {
        this->hoverCycle = 0;
        this->hoverHeight = this->actor.world.pos.y;
    }
    if (!BigPoe_CheckDamage(this, play)) {
        BigPoe_UpdateLockOnPatience(this);
    }
    this->actionFunc(this, play);
    if (!BigPoe_IsRisingWithFlames(this)) {
        Actor_MoveXZGravity(&this->actor);
    }
    if (this->actionFunc == BigPoe_LanternFalling) {
        Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 27.0f, 60.0f, BGCHECK_UPDATE_FLOOR);
    } else {
        BigPoe_KeepOverFloor(this, play);
    }
    Actor_SetFocus(&this->actor, 42.0f);
    BigPoe_UpdateColor(this);
    BigPoe_FlickerLantern(this);
    this->actor.shape.shadowAlpha = this->mainColor.a;
    BigPoe_SubmitCollider(this, play);
}

static bool BigPoe_IsBurning(EnBigPoe* this) {
    return this->actionFunc == BigPoe_BurnAway && this->timer >= 2;
}

static s32 BigPoe_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx,
                                   Gfx** gfx) {
    EnBigPoe* this = (EnBigPoe*)thisx;

    if (this->mainColor.a == 0 || limbIndex == BIG_POE_LIMB_LANTERN || BigPoe_IsBurning(this)) {
        *dList = NULL;
    }
    return false;
}

static void BigPoe_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx) {
    EnBigPoe* this = (EnBigPoe*)thisx;

    if (BigPoe_IsBurning(this) && limbIndex == BIG_POE_LIMB_HAT_AND_CLOAK) {
        gSPMatrix((*gfx)++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList((*gfx)++, (Gfx*)sBigPoeCrispyBodyDL);
    }
    if (limbIndex != BIG_POE_LIMB_LANTERN) {
        return;
    }
    // The lantern keeps its size while the shrinking ghost carries it up.
    if (this->actionFunc == BigPoe_BurnAway && this->timer > 18 && this->actor.scale.x != 0.0f) {
        f32 keepSize = BIG_POE_SCALE / this->actor.scale.x;

        Matrix_Scale(keepSize, keepSize, keepSize, MTXMODE_APPLY);
    }
    Matrix_Get(&this->lanternMtx);
}

static bool BigPoe_IsDrawnOpaque(EnBigPoe* this) {
    return this->mainColor.a == 255 || this->mainColor.a == 0;
}

// MM's Scene_SetRenderModeXlu tables for segment 0x0C: object_bigpo calls entries 0 and 2.
static Gfx sRenderModeOpaDL[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};
static Gfx sRenderModeXluDL[] = {
    gsDPSetRenderMode(AA_EN | Z_CMP | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                          GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1),
                      G_RM_AA_ZB_XLU_SURF2),
    gsSPEndDisplayList(),
    gsDPSetRenderMode(AA_EN | Z_CMP | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                          GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_IN, G_BL_1MA),
                      G_RM_AA_ZB_XLU_SURF2),
    gsSPEndDisplayList(),
};

static void BigPoe_DrawMain(Actor* thisx, PlayState* play) {
    EnBigPoe* this = (EnBigPoe*)thisx;
    Color_RGBA8 color = this->mainColor;

    OPEN_DISPS(play->state.gfxCtx);
    if (BigPoe_IsDrawnOpaque(this)) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
        gSPSegment(POLY_OPA_DISP++, 0x08,
                   (uintptr_t)Gfx_EnvColor(play->state.gfxCtx, color.r, color.g, color.b, color.a));
        POLY_OPA_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                       BigPoe_OverrideLimbDraw, BigPoe_PostLimbDraw, this, POLY_OPA_DISP);
    } else {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)sRenderModeXluDL);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   (uintptr_t)Gfx_EnvColor(play->state.gfxCtx, color.r, color.g, color.b, color.a));
        POLY_XLU_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                       BigPoe_OverrideLimbDraw, BigPoe_PostLimbDraw, this, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Put(&this->lanternMtx);
    BigPoe_DrawLantern(&this->actor, play);
    if (this->actionFunc == BigPoe_AppearStage6) {
        BigPoe_DrawCircleFlames(&this->actor, play);
    }
}

static void BigPoe_LightLantern(EnBigPoe* this, PlayState* play) {
    Camera* camera = GET_ACTIVE_CAM(play);
    Vec3f towardCamera = sZeroVec;
    Vec3f lamp;
    Vec3f lampOffset = { 0.0f, 1400.0f, 0.0f };

    if (camera != NULL) {
        Math_Vec3f_Diff(&camera->eye, &camera->at, &towardCamera);
        f32 distance = Math3D_Vec3fMagnitude(&towardCamera);

        Math_Vec3f_Scale(&towardCamera, distance > 1.0f ? 20.0f / distance : 20.0f);
    }
    Matrix_MultVec3f(&lampOffset, &lamp);
    Lights_PointGlowSetInfo(&this->fires[0].info, lamp.x + towardCamera.x, lamp.y + towardCamera.y,
                            lamp.z + towardCamera.z, this->lanternColor.r, this->lanternColor.g, this->lanternColor.b,
                            this->lanternColor.a);
}

static void BigPoe_DrawLantern(Actor* thisx, PlayState* play) {
    EnBigPoe* this = (EnBigPoe*)thisx;
    bool isOpaque = BigPoe_IsDrawnOpaque(this);

    BigPoe_LightLantern(this, play);
    OPEN_DISPS(play->state.gfxCtx);
    if (isOpaque) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
    } else {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    }
    Gfx** disp = isOpaque ? &POLY_OPA_DISP : &POLY_XLU_DISP;

    gSPSegment((*disp)++, 0x0C, (uintptr_t)(isOpaque ? sRenderModeOpaDL : sRenderModeXluDL));
    gSPSegment((*disp)++, 0x0A, (uintptr_t)Gfx_EnvColor(play->state.gfxCtx, 160, 0, 255, this->mainColor.a));
    gDPSetEnvColor((*disp)++, this->lanternColor.r, this->lanternColor.g, this->lanternColor.b, this->mainColor.a);
    gSPMatrix((*disp)++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList((*disp)++, (Gfx*)sBigPoeLanternMainDL);
    gSPDisplayList((*disp)++, (Gfx*)sBigPoeLanternPurpleTopDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void BigPoe_DrawCircleFlames(Actor* thisx, PlayState* play) {
    EnBigPoe* this = (EnBigPoe*)thisx;
    bool isFadingIn = this->actionFunc == BigPoe_AppearStage6;
    s16 lightRadius = isFadingIn ? 500 : (s16)(this->actor.scale.x * 500.0f * 100.0f);
    MtxF billboard;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_RotateY(BINANG_TO_RAD(BINANG_ROT180(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)))), MTXMODE_NEW);
    if (isFadingIn) {
        Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    } else {
        Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    }
    Matrix_Get(&billboard);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                           (play->gameplayFrames * -20) % 512, 0x20, 0x80));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 170, 255, 255, 255 - this->mainColor.a);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 255, 255);
    for (s32 i = 0; i < BIG_POE_FIRE_COUNT; i++) {
        BigPoeFire* fire = &this->fires[i];

        Lights_PointNoGlowSetInfo(&fire->info, fire->pos.x, fire->pos.y, fire->pos.z, 170, 255, 255, lightRadius);
        billboard.xw = fire->pos.x;
        billboard.yw = fire->pos.y;
        billboard.zw = fire->pos.z;
        Matrix_Put(&billboard);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's CanKillEnemy(ACTOR_EN_BIGPO) asks for the bow: it only comes in reach while hovering above Link.
static bool BigPoe_CanKillInLogic(void* userData) {
    return sApi->LogicCanUseItem(RG_FAIRY_BOW) || sApi->LogicCanUseItem(RG_FAIRY_SLINGSHOT);
}

// MM's Poe family soul, which any MM Poe mod shares: all define it alike, so whichever loads first registers it.
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

static s32 Soul_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* data) {
    if (limbIndex == BIG_POE_LIMB_LANTERN) {
        *dList = NULL;
    }
    return false;
}

// The lantern is drawn apart from the skeleton, lit as the awake Big Poe's.
static void Soul_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* data) {
    if (limbIndex != BIG_POE_LIMB_LANTERN) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)Gfx_EnvColor(play->state.gfxCtx, 160, 0, 255, 255));
    gDPSetEnvColor(POLY_OPA_DISP++, 240, 205, 175, 255);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBigPoeLanternMainDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBigPoeLanternPurpleTopDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The soul shows its enemy in an idle loop, as Majora's Mask souls do.
static void DrawSoul(PlayState* play, GetItemEntry* getItemEntry) {
    static SkelAnime sSoulSkelAnime;
    static Vec3s sSoulJointTable[BIG_POE_LIMB_MAX];
    static Vec3s sSoulMorphTable[BIG_POE_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_Init(play, &sSoulSkelAnime, (SkeletonHeader*)sBigPoeSkel, (AnimationHeader*)sBigPoeFloatAnim,
                       sSoulJointTable, sSoulMorphTable, BIG_POE_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-sSoulJointTable[0].x, -sSoulJointTable[0].y, -sSoulJointTable[0].z, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)Gfx_EnvColor(play->state.gfxCtx, 255, 255, 210, 255));
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0B, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
    SkelAnime_DrawOpa(play, sSoulSkelAnime.skeleton, sSoulSkelAnime.jointTable, Soul_OverrideLimbDraw,
                      Soul_PostLimbDraw, NULL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterSoul(void) {
    static const SOHCustomItemRandomizer sSoulRandomizer = {
        .structSize = sizeof(SOHCustomItemRandomizer),
        .flags = SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT,
        .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
    };
    SOHCustomItemDefinition soul = Z64Items_Define(POE_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Poes (MM)";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Poes (MM)%w!&Majora's Mask's Poes can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_RegisterShared(sApi, &soul)) {
        sApi->Log("Could not register " POE_SOUL_ITEM_KEY);
    }
}

static void RegisterBigPoe(void) {
    SOHActorDefinition bigPoe = { 0 };

    bigPoe.structSize = sizeof(bigPoe);
    bigPoe.key = BIG_POE_KEY;
    bigPoe.description = "Big Poe (Majora's Mask)";
    bigPoe.category = ACTORCAT_ENEMY;
    bigPoe.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                        ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR | ACTOR_FLAG_IGNORE_QUAKE;
    bigPoe.objectId = OBJECT_GAMEPLAY_KEEP;
    bigPoe.instanceSize = sizeof(EnBigPoe);
    bigPoe.init = EnBigPoe_Init;
    bigPoe.destroy = EnBigPoe_Destroy;
    bigPoe.update = EnBigPoe_Update;
    bigPoe.draw = BigPoe_DrawMain;
    bigPoe.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_MINIBOSS | SOH_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS;
    bigPoe.naviHint = "Big Poe&%cIt vanishes when you stare at it too long. Shoot it with %rarrows%c while it "
                      "hovers, before it spins at you!%w";
    bigPoe.soulKey = POE_FAMILY_KEY;
    bigPoe.logicKill = BigPoe_CanKillInLogic;
    if (sApi->RegisterActor(&bigPoe) < 0) {
        sApi->Log("Could not register " BIG_POE_KEY);
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
    RegisterBigPoe();
}
