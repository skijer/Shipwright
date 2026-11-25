// Majora's Mask's Death Armos (En_Famos) as an Unbound custom enemy, drawn from mm.o2r.
// It hovers in place: MM's path index (params) points into its own scenes' path lists. World rot x sets its sight.
// The ground crater (En_Test) its slam leaves is left out.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define DEATH_ARMOS_KEY "mm.death_armos"
#define DEATH_ARMOS_SOUL_ITEM_KEY "soul.mm.death_armos"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_FAMOS "__OTR__objects/object_famos/"

#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_ALL 0x1F

#define MM_SE_EN_EYEGOLE_DEAD 0x3846
#define MM_SE_EN_EYEGOLE_DAMAGE 0x3847
#define MM_SE_EN_AMOS_VOICE 0x3848
#define MM_SE_EN_LAST1_FALL_OLD 0x39D7
#define MM_SE_EN_FAMOS_REVERSE1 0x3A91
#define MM_SE_EN_FAMOS_REVERSE2 0x3A92
#define MM_SE_EN_FAMOS_FLOAT 0x3A93
#define MM_SE_EN_FAMOS_FLOAT_REVERSE 0x3A94
#define MM_SE_EV_EXPLOSION 0x2802

#define DEATH_ARMOS_ROCK_COUNT 20
#define DEATH_ARMOS_FLIP_FRAMES 100
#define DEATH_ARMOS_DYING (-1)
#define DEATH_ARMOS_GLOW_PERIOD 12
#define FLOOR_PROPERTY_VOID_A 12
#define FLOOR_PROPERTY_VOID_B 13

typedef enum {
    DEATH_ARMOS_LIMB_NONE,
    DEATH_ARMOS_LIMB_BODY,
    DEATH_ARMOS_LIMB_EMBLEM,
    DEATH_ARMOS_LIMB_SWORD,
    DEATH_ARMOS_LIMB_SHIELD,
    DEATH_ARMOS_LIMB_HEAD,
    DEATH_ARMOS_LIMB_MAX
} DeathArmosLimb;

typedef struct {
    Vec3f pos;
    Vec3f velocity;
    Vec3s rot;
    f32 scale;
} DeathArmosRock;

struct EnDeathArmos;

typedef void (*EnDeathArmosActionFunc)(struct EnDeathArmos*, PlayState*);

typedef struct EnDeathArmos {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[DEATH_ARMOS_LIMB_MAX];
    Vec3s morphTable[DEATH_ARMOS_LIMB_MAX];
    EnDeathArmosActionFunc actionFunc;
    bool isFlipped;
    bool hasFinishedRotating;
    bool isCalm;
    s16 hoverTimer;
    s16 stateTimer;
    s16 debrisTimer;
    s16 flippedTimer;
    s16 targetYaw;
    s16 flipRot;
    f32 baseHeight;
    f32 aggroDistance;
    Vec3f targetDest;
    Vec3f calmPos;
    ColliderCylinder bodyCollider;
    ColliderCylinder shockwaveCollider;
    ColliderJntSph emblemCollider;
    ColliderJntSphElement emblemColliderElements[2];
    DeathArmosRock rocks[DEATH_ARMOS_ROCK_COUNT];
} EnDeathArmos;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sDeathArmosSkel[] = OBJ_FAMOS "gFamosSkel";
static const ALIGN_ASSET(2) char sDeathArmosIdleAnim[] = OBJ_FAMOS "gFamosIdleAnim";
static const ALIGN_ASSET(2) char sDeathArmosShakeAnim[] = OBJ_FAMOS "gFamosShakeAnim";
static const ALIGN_ASSET(2) char sRockDL[] = "__OTR__objects/gameplay_keep/gameplay_keep_DL_06AB30";

static ColliderCylinderInit sBodyCylinderInit = {
    { COLTYPE_METAL, AT_NONE | AT_TYPE_ENEMY, AC_ON | AC_HARD | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_UNBLOCKABLE, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x10 },
      { 0xF7CFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_ON,
      OCELEM_ON },
    { 20, 80, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sShockwaveCylinderInit = {
    { COLTYPE_NONE, AT_NONE | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xF7CFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x08 },
      { 0xF7CFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { 70, 10, 0, { 0, 0, 0 } },
};

// The emblem runs through the torso: one sphere on each face. Only light arrows reach it.
static ColliderJntSphElementInit sEmblemElementsInit[2] = {
    {
        { ELEMTYPE_UNK0,
          { 0x00000000, 0x00, 0x00 },
          { DMG_ARROW_LIGHT, 0x00, 0x00 },
          TOUCH_NONE,
          BUMP_ON,
          OCELEM_NONE },
        { DEATH_ARMOS_LIMB_EMBLEM, { { 2500, 0, 0 }, 20 }, 100 },
    },
    {
        { ELEMTYPE_UNK0,
          { 0x00000000, 0x00, 0x00 },
          { DMG_ARROW_LIGHT, 0x00, 0x00 },
          TOUCH_NONE,
          BUMP_ON,
          OCELEM_NONE },
        { DEATH_ARMOS_LIMB_EMBLEM, { { -1500, 0, 0 }, 20 }, 100 },
    },
};

static ColliderJntSphInit sEmblemColliderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_1, COLSHAPE_JNTSPH },
    2,
    sEmblemElementsInit,
};

// gFamos*GlowingEmblemTexAnim is empty in Ship's mm.o2r; its env-color keys (frames 0, 5, 11 of 12) live here.
static const Color_RGB8 sGlowKeys[2][3] = {
    { { 0, 0, 0 }, { 0, 70, 40 }, { 0, 0, 0 } },
    { { 0, 0, 0 }, { 255, 150, 0 }, { 0, 0, 0 } },
};
static const f32 sGlowKeyFrames[3] = { 0.0f, 5.0f, 11.0f };

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

static void DeathArmos_StillIdle(EnDeathArmos* this, PlayState* play);
static void DeathArmos_TurnHome(EnDeathArmos* this, PlayState* play);
static void DeathArmos_ReturnHome(EnDeathArmos* this, PlayState* play);
static void DeathArmos_Alert(EnDeathArmos* this, PlayState* play);
static void DeathArmos_Chase(EnDeathArmos* this, PlayState* play);
static void DeathArmos_AttackAim(EnDeathArmos* this, PlayState* play);
static void DeathArmos_Attack(EnDeathArmos* this, PlayState* play);
static void DeathArmos_FinishAttack(EnDeathArmos* this, PlayState* play);
static void DeathArmos_AttackRebound(EnDeathArmos* this, PlayState* play);
static void DeathArmos_ScanForLink(EnDeathArmos* this, PlayState* play);
static void DeathArmos_DeathSlam(EnDeathArmos* this, PlayState* play);
static void DeathArmos_DeathExplosion(EnDeathArmos* this, PlayState* play);
static void DeathArmos_DeathFade(EnDeathArmos* this, PlayState* play);

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

static bool DeathArmos_IsUpsideDown(EnDeathArmos* this) {
    return ABS(this->flipRot) > 0x4000;
}

static void DeathArmos_PlayHum(EnDeathArmos* this) {
    if (DeathArmos_IsUpsideDown(this)) {
        PlaySfx(&this->actor, MM_SE_EN_FAMOS_FLOAT_REVERSE - SFX_FLAG, NA_SE_EN_AMOS_WALK - SFX_FLAG);
    } else {
        PlaySfx(&this->actor, MM_SE_EN_FAMOS_FLOAT - SFX_FLAG, NA_SE_EN_AMOS_WALK - SFX_FLAG);
    }
}

static void DeathArmos_ScatterRocks(EnDeathArmos* this, bool isDeath) {
    this->debrisTimer = 40;
    for (s32 i = 0; i < DEATH_ARMOS_ROCK_COUNT; i++) {
        DeathArmosRock* rock = &this->rocks[i];
        s16 direction = (s32)Rand_Next() >> 0x10;
        s16 lift = isDeath ? (s16)(Rand_Next() >> 0x12) : Rand_S16Offset(0x1800, 0x2800);
        f32 speed = isDeath ? Rand_ZeroFloat(6.0f) + 7.0f : Rand_ZeroFloat(5.0f) + 5.0f;

        rock->velocity.x = speed * Math_CosS(lift) * Math_SinS(direction);
        rock->velocity.y = Math_SinS(lift) * speed + (isDeath ? 4.5f : 3.0f);
        rock->velocity.z = speed * Math_CosS(lift) * Math_CosS(direction);
        rock->rot.x = (s32)Rand_Next() >> 0x10;
        rock->rot.y = (s32)Rand_Next() >> 0x10;
        rock->rot.z = (s32)Rand_Next() >> 0x10;
        rock->pos.x = Math_SinS(direction) * 20.0f + this->actor.world.pos.x;
        rock->pos.y = isDeath ? Rand_CenteredFloat(60.0f) + this->actor.world.pos.y + 40.0f : this->actor.floorHeight;
        rock->pos.z = Math_CosS(direction) * 20.0f + this->actor.world.pos.z;
        rock->scale = isDeath ? Rand_ZeroFloat(0.002f) + 0.0025f : Rand_ZeroFloat(0.0015f) + 0.002f;
    }
}

static bool DeathArmos_IsLinkSeen(EnDeathArmos* this, PlayState* play) {
    return Actor_WorldDistXZToPoint(&GET_PLAYER(play)->actor, &this->calmPos) < this->aggroDistance &&
           Actor_IsFacingPlayer(&this->actor, 0x5000);
}

static void DeathArmos_Bob(EnDeathArmos* this) {
    if (this->hoverTimer == 0) {
        this->hoverTimer = 30;
    }
    this->hoverTimer--;
    this->actor.world.pos.y = Math_SinS(this->hoverTimer * 0x888) * 10.0f + this->baseHeight;
    DeathArmos_PlayHum(this);
}

static void DeathArmos_SetFlipped(EnDeathArmos* this, bool isFlipped) {
    if (this->isFlipped && !isFlipped) {
        PlaySfx(&this->actor, MM_SE_EN_FAMOS_REVERSE2, NA_SE_EN_AMOS_VOICE);
    }
    if (isFlipped) {
        this->flippedTimer = DEATH_ARMOS_FLIP_FRAMES;
        PlaySfx(&this->actor, MM_SE_EN_FAMOS_REVERSE1, NA_SE_EN_AMOS_VOICE);
        PlaySfx(&this->actor, MM_SE_EN_EYEGOLE_DAMAGE, NA_SE_EN_AMOS_DAMAGE);
    }
    this->isFlipped = isFlipped;
    this->hasFinishedRotating = false;
}

// A light arrow in the emblem turns it upside down for a while; another one rights it.
static void DeathArmos_UpdateFlip(EnDeathArmos* this) {
    if (this->emblemCollider.base.acFlags & AC_HIT) {
        this->emblemCollider.base.acFlags &= ~AC_HIT;
        if (this->hasFinishedRotating) {
            DeathArmos_SetFlipped(this, !this->isFlipped);
        }
    } else if (this->flippedTimer > 0) {
        this->flippedTimer--;
        if (this->flippedTimer == 0) {
            DeathArmos_SetFlipped(this, false);
        }
    }
    if (!this->hasFinishedRotating) {
        this->hasFinishedRotating = Math_ScaledStepToS(&this->flipRot, this->isFlipped ? -0x8000 : 0, 0x1000);
    }
}

static void DeathArmos_SetupStillIdle(EnDeathArmos* this) {
    this->actor.speedXZ = 0.0f;
    this->actionFunc = DeathArmos_StillIdle;
}

static void DeathArmos_SetupAlert(EnDeathArmos* this) {
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->stateTimer = 8;
    this->actor.speedXZ = 0.0f;
    if (this->isCalm) {
        this->isCalm = false;
        this->calmPos = this->actor.world.pos;
    }
    this->actionFunc = DeathArmos_Alert;
}

static void DeathArmos_StillIdle(EnDeathArmos* this, PlayState* play) {
    DeathArmos_Bob(this);
    if (this->isCalm) {
        this->calmPos = this->actor.world.pos;
    }
    if (DeathArmos_IsLinkSeen(this, play)) {
        DeathArmos_SetupAlert(this);
    }
}

static void DeathArmos_SetupReturnHome(EnDeathArmos* this) {
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.world.rot.x = -Actor_WorldPitchTowardPoint(&this->actor, &this->targetDest);
    this->actionFunc = DeathArmos_ReturnHome;
}

static void DeathArmos_TurnHome(EnDeathArmos* this, PlayState* play) {
    DeathArmos_Bob(this);
    if (DeathArmos_IsLinkSeen(this, play)) {
        DeathArmos_SetupAlert(this);
    } else if (Math_ScaledStepToS(&this->actor.shape.rot.y, this->targetYaw, 0x200)) {
        DeathArmos_SetupReturnHome(this);
    }
}

static void DeathArmos_ReturnHome(EnDeathArmos* this, PlayState* play) {
    f32 distanceToHome = Actor_WorldDistXZToPoint(&this->actor, &this->targetDest);

    this->actor.shape.rot.y = Actor_WorldYawTowardPoint(&this->actor, &this->targetDest);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    DeathArmos_Bob(this);
    if (this->isCalm) {
        this->calmPos = this->actor.world.pos;
    }
    if (DeathArmos_IsLinkSeen(this, play)) {
        DeathArmos_SetupAlert(this);
    } else if (distanceToHome < 20.0f) {
        this->isCalm = true;
        DeathArmos_SetupStillIdle(this);
    } else {
        Math_StepToF(&this->actor.speedXZ, distanceToHome < 40.0f ? 0.5f : 3.0f, 0.3f);
    }
}

static void DeathArmos_SetupChase(EnDeathArmos* this) {
    this->hoverTimer = 0;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = DeathArmos_Chase;
}

// Startled, it jolts up and down before giving chase.
static void DeathArmos_Alert(EnDeathArmos* this, PlayState* play) {
    DeathArmos_PlayHum(this);
    this->stateTimer--;
    if (this->stateTimer == 0) {
        this->actor.world.pos.y = this->baseHeight;
        DeathArmos_SetupChase(this);
    } else {
        this->actor.world.pos.y = Math_SinS(this->stateTimer * 0x1000) * 30.0f + this->baseHeight;
    }
}

static bool DeathArmos_IsOverVoid(EnDeathArmos* this, PlayState* play) {
    u32 floorProperty = func_80041EA4(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId);

    return floorProperty == FLOOR_PROPERTY_VOID_A || floorProperty == FLOOR_PROPERTY_VOID_B;
}

static void DeathArmos_SetupScanForLink(EnDeathArmos* this) {
    this->stateTimer = 60;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = DeathArmos_ScanForLink;
}

static void DeathArmos_SetupAttackAim(EnDeathArmos* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sDeathArmosShakeAnim);
    this->actor.speedXZ = 0.0f;
    PlaySfx(&this->actor, MM_SE_EN_AMOS_VOICE, NA_SE_EN_AMOS_VOICE);
    this->actionFunc = DeathArmos_AttackAim;
}

// Glides 100 above Link and drops on him once right overhead.
static void DeathArmos_Chase(EnDeathArmos* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f abovePlayer = { player->actor.world.pos.x, player->actor.world.pos.y + 100.0f, player->actor.world.pos.z };

    DeathArmos_Bob(this);
    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0x800);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.world.rot.x = -Actor_WorldPitchTowardPoint(&this->actor, &abovePlayer);
    Math_StepToF(&this->actor.speedXZ, 6.0f, 0.5f);
    bool canSlam = this->actor.xzDistToPlayer < 30.0f && this->actor.floorHeight > BGCHECK_Y_MIN &&
                   !DeathArmos_IsOverVoid(this, play);

    if (canSlam) {
        DeathArmos_SetupAttackAim(this);
    } else if (this->aggroDistance < Actor_WorldDistXZToPoint(&player->actor, &this->calmPos) ||
               !Actor_IsFacingPlayer(&this->actor, 0x6000)) {
        DeathArmos_SetupScanForLink(this);
    }
}

static void DeathArmos_AttackAim(EnDeathArmos* this, PlayState* play) {
    PlaySfx(&this->actor, MM_SE_EN_LAST1_FALL_OLD - SFX_FLAG, NA_SE_EN_AMOS_WALK - SFX_FLAG);
    if (SkelAnime_Update(&this->skelAnime)) {
        this->actor.world.rot.x = -0x4000;
        this->bodyCollider.base.atFlags |= AT_ON;
        this->stateTimer = 4;
        this->actionFunc = DeathArmos_Attack;
    }
}

static void DeathArmos_SetupDeathSlam(EnDeathArmos* this) {
    this->emblemCollider.base.acFlags &= ~AC_ON;
    this->stateTimer = 20;
    this->actor.speedXZ = 0.0f;
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 20);
    this->flippedTimer = DEATH_ARMOS_DYING;
    this->actor.world.pos.y = this->actor.floorHeight - 60.0f;
    PlaySfx(&this->actor, MM_SE_EN_EYEGOLE_DEAD, NA_SE_EN_AMOS_DEAD);
    this->actionFunc = DeathArmos_DeathSlam;
}

static void DeathArmos_SetupFinishAttack(EnDeathArmos* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sDeathArmosIdleAnim);
    SkelAnime_Update(&this->skelAnime);
    this->emblemCollider.base.acFlags |= AC_ON;
    this->stateTimer = 3;
    this->actor.speedXZ = 0.0f;
    PlaySfx(&this->actor, MM_SE_EV_EXPLOSION, NA_SE_EV_EXPLOSION);
    this->actionFunc = DeathArmos_FinishAttack;
}

static void DeathArmos_SetupAttackRebound(EnDeathArmos* this) {
    this->actor.world.rot.x = 0x4000;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = DeathArmos_AttackRebound;
}

// Upside down, the slam drives its head into the ground and it blows apart.
static void DeathArmos_Land(EnDeathArmos* this, PlayState* play) {
    Camera_AddQuake(GET_ACTIVE_CAM(play), 2, 15, 10);
    Rumble_Request(this->actor.xyzDistToPlayerSq, 180, 20, 100);
    DeathArmos_ScatterRocks(this, false);
    if (this->isFlipped) {
        DeathArmos_SetupDeathSlam(this);
    } else {
        DeathArmos_SetupFinishAttack(this);
    }
}

static void DeathArmos_Attack(EnDeathArmos* this, PlayState* play) {
    Math_StepToF(&this->actor.speedXZ, 20.0f, 2.0f);
    this->stateTimer--;
    if (this->stateTimer == 0) {
        this->emblemCollider.base.acFlags &= ~AC_ON;
    }
    bool hasHitFloor = (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;

    if (!hasHitFloor && this->actor.floorHeight != BGCHECK_Y_MIN && !DeathArmos_IsOverVoid(this, play)) {
        PlaySfx(&this->actor, MM_SE_EN_LAST1_FALL_OLD - SFX_FLAG, NA_SE_EN_AMOS_WALK - SFX_FLAG);
        return;
    }
    this->bodyCollider.base.atFlags &= ~AT_ON;
    this->shockwaveCollider.base.atFlags |= AT_ON;
    if (hasHitFloor) {
        DeathArmos_Land(this, play);
        return;
    }
    this->emblemCollider.base.acFlags |= AC_ON;
    DeathArmos_SetupAttackRebound(this);
}

static void DeathArmos_FinishAttack(EnDeathArmos* this, PlayState* play) {
    if (this->stateTimer == 0) {
        this->shockwaveCollider.base.atFlags &= ~AT_ON;
    }
    this->stateTimer--;
    if (SkelAnime_Update(&this->skelAnime)) {
        DeathArmos_SetupAttackRebound(this);
    }
}

static void DeathArmos_AttackRebound(EnDeathArmos* this, PlayState* play) {
    Math_StepToF(&this->actor.speedXZ, 5.0f, 0.3f);
    if (this->actor.speedXZ > 1.0f) {
        DeathArmos_PlayHum(this);
    }
    if (this->baseHeight < this->actor.world.pos.y || (this->actor.bgCheckFlags & BGCHECKFLAG_CEILING)) {
        this->actor.speedXZ = 0.0f;
        DeathArmos_SetupChase(this);
    }
}

static void DeathArmos_ScanForLink(EnDeathArmos* this, PlayState* play) {
    DeathArmos_Bob(this);
    this->stateTimer--;
    if (DeathArmos_IsLinkSeen(this, play)) {
        DeathArmos_SetupAlert(this);
    } else if (this->stateTimer == 0) {
        this->targetYaw = Actor_WorldYawTowardPoint(&this->actor, &this->calmPos);
        this->targetDest = this->calmPos;
        this->actor.speedXZ = 0.0f;
        this->actionFunc = DeathArmos_TurnHome;
    } else {
        this->actor.shape.rot.y = (s32)(Math_SinS(this->stateTimer * 0x888) * 8192.0f) + this->actor.world.rot.y;
    }
}

static void DeathArmos_DeathSlam(EnDeathArmos* this, PlayState* play) {
    if (this->stateTimer == 17) {
        this->shockwaveCollider.base.atFlags &= ~AT_ON;
    }
    if (this->stateTimer != 0) {
        this->stateTimer--;
        return;
    }
    this->actor.world.rot.x = 0x4000;
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 4);
    this->stateTimer = 25;
    this->targetDest = this->actor.world.pos;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    this->actionFunc = DeathArmos_DeathExplosion;
}

static void DeathArmos_Detonate(EnDeathArmos* this, PlayState* play) {
    EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, this->actor.world.pos.x,
                                      this->actor.world.pos.y + 40.0f, this->actor.world.pos.z, 0, 0, 0, BOMB_BODY);

    if (bomb != NULL) {
        bomb->timer = 0;
    }
}

// Rises shaking and flashing red, explodes and crumbles into rocks.
static void DeathArmos_DeathExplosion(EnDeathArmos* this, PlayState* play) {
    Math_StepToF(&this->actor.speedXZ, 3.0f, 0.3f);
    if (this->actor.colorFilterTimer == 0) {
        Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 4);
    }
    this->actor.world.pos.x = Rand_CenteredFloat(5.0f) + this->targetDest.x;
    this->actor.world.pos.z = Rand_CenteredFloat(5.0f) + this->targetDest.z;
    if (this->stateTimer == 1) {
        DeathArmos_Detonate(this, play);
    }
    if (this->stateTimer != 0) {
        this->stateTimer--;
        return;
    }
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xD << 4);
    DeathArmos_ScatterRocks(this, true);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.shape.shadowDraw = NULL;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = DeathArmos_DeathFade;
}

static void DeathArmos_DeathFade(EnDeathArmos* this, PlayState* play) {
    if (this->debrisTimer == 0) {
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
        Actor_Kill(&this->actor);
    }
}

static void DeathArmos_UpdateRocks(EnDeathArmos* this) {
    for (s32 i = 0; i < DEATH_ARMOS_ROCK_COUNT; i++) {
        DeathArmosRock* rock = &this->rocks[i];

        rock->velocity.y -= 1.0f;
        Math_Vec3f_Sum(&rock->pos, &rock->velocity, &rock->pos);
        rock->rot.x += (s16)((Rand_Next() >> 0x17) + 0x700);
        rock->rot.y += (s16)((Rand_Next() >> 0x17) + 0x900);
        rock->rot.z += (s16)((Rand_Next() >> 0x17) + 0xB00);
    }
}

static void DeathArmos_SubmitColliders(EnDeathArmos* this, PlayState* play) {
    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    if (this->bodyCollider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    if (this->emblemCollider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->emblemCollider.base);
    }
    if (this->shockwaveCollider.base.atFlags & AT_ON) {
        Collider_UpdateCylinder(&this->actor, &this->shockwaveCollider);
        this->shockwaveCollider.dim.pos.y = this->actor.floorHeight;
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->shockwaveCollider.base);
    }
}

static void EnDeathArmos_Init(Actor* thisx, PlayState* play) {
    EnDeathArmos* this = (EnDeathArmos*)thisx;

    this->actor.targetArrowOffset = 3500.0f;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 30.0f);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sDeathArmosSkel, (AnimationHeader*)sDeathArmosIdleAnim,
                   this->jointTable, this->morphTable, DEATH_ARMOS_LIMB_MAX);
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyCylinderInit);
    Collider_InitCylinder(play, &this->shockwaveCollider);
    Collider_SetCylinder(play, &this->shockwaveCollider, &this->actor, &sShockwaveCylinderInit);
    Collider_InitJntSph(play, &this->emblemCollider);
    Collider_SetJntSph(play, &this->emblemCollider, &this->actor, &sEmblemColliderInit, this->emblemColliderElements);
    this->actor.colChkInfo.mass = 250;
    this->baseHeight = this->actor.world.pos.y;
    this->aggroDistance = this->actor.shape.rot.x <= 0 ? 200.0f : this->actor.shape.rot.x * 40.0f * 0.1f;
    this->actor.shape.rot.x = 0;
    this->actor.world.rot.x = 0;
    this->hasFinishedRotating = true;
    this->isCalm = true;
    this->calmPos = this->actor.world.pos;
    DeathArmos_SetupStillIdle(this);
}

static void EnDeathArmos_Destroy(Actor* thisx, PlayState* play) {
    EnDeathArmos* this = (EnDeathArmos*)thisx;

    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyCylinder(play, &this->shockwaveCollider);
    Collider_DestroyJntSph(play, &this->emblemCollider);
}

static void EnDeathArmos_Update(Actor* thisx, PlayState* play) {
    EnDeathArmos* this = (EnDeathArmos*)thisx;

    if (this->debrisTimer > 0) {
        this->debrisTimer--;
        DeathArmos_UpdateRocks(this);
    }
    if (this->actionFunc == DeathArmos_DeathFade) {
        DeathArmos_DeathFade(this, play);
        return;
    }
    s16 previousHoverTimer = this->hoverTimer;

    DeathArmos_UpdateFlip(this);
    this->actionFunc(this, play);
    f32 previousHeight = this->actor.world.pos.y;

    Actor_MoveXYZ(&this->actor);
    if (previousHoverTimer != this->hoverTimer) {
        this->baseHeight += this->actor.world.pos.y - previousHeight;
    }
    if (this->flippedTimer >= 0) {
        Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 30.0f, 80.0f, BGCHECK_UPDATE_ALL);
        bool isHeadFirstLanding =
            this->actionFunc == DeathArmos_Attack && this->isFlipped && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);

        if (isHeadFirstLanding) {
            this->actor.world.pos.y -= 60.0f;
        }
    }
    this->actor.focus.rot.y = this->actor.shape.rot.y;
    DeathArmos_SubmitColliders(this, play);
}

// Pivots the body 4000 units up so a flip turns it over in place.
static s32 DeathArmos_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                       void* thisx) {
    EnDeathArmos* this = (EnDeathArmos*)thisx;

    if (limbIndex == DEATH_ARMOS_LIMB_BODY) {
        Matrix_Translate(0.0f, 4000.0f, 0.0f, MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(this->flipRot), MTXMODE_APPLY);
        Matrix_Translate(0.0f, -4000.0f, 0.0f, MTXMODE_APPLY);
        return false;
    }
    bool isShattered = this->flippedTimer == DEATH_ARMOS_DYING;
    bool isLooseLimb = limbIndex == DEATH_ARMOS_LIMB_SWORD || limbIndex == DEATH_ARMOS_LIMB_SHIELD ||
                       limbIndex == DEATH_ARMOS_LIMB_HEAD;

    if (isShattered && isLooseLimb) {
        *dList = NULL;
    }
    return false;
}

static void DeathArmos_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnDeathArmos* this = (EnDeathArmos*)thisx;

    if (limbIndex == DEATH_ARMOS_LIMB_EMBLEM) {
        Matrix_MultVec3f(&sZeroVec, &this->actor.focus.pos);
        Collider_UpdateSpheres(limbIndex, &this->emblemCollider);
    }
}

// MM's non-linear color animation: a quadratic through the three keys, looping every 12 frames.
static u8 DeathArmos_GlowChannel(u8 key0, u8 key1, u8 key2, f32 frame) {
    const f32* x = sGlowKeyFrames;
    f32 value = key0 * ((frame - x[1]) * (frame - x[2])) / ((x[0] - x[1]) * (x[0] - x[2])) +
                key1 * ((frame - x[0]) * (frame - x[2])) / ((x[1] - x[0]) * (x[1] - x[2])) +
                key2 * ((frame - x[0]) * (frame - x[1])) / ((x[2] - x[0]) * (x[2] - x[1]));

    return CLAMP(value, 0.0f, 255.0f);
}

static Gfx* DeathArmos_MakeGlowDL(EnDeathArmos* this, PlayState* play) {
    const Color_RGB8* keys = sGlowKeys[this->isFlipped ? 1 : 0];
    f32 frame = play->gameplayFrames % DEATH_ARMOS_GLOW_PERIOD;
    Gfx* glow = Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Gfx));
    Gfx* cursor = glow;

    gDPSetPrimColor(cursor++, 0, 255, 255, 255, 255, 255);
    gDPSetEnvColor(cursor++, DeathArmos_GlowChannel(keys[0].r, keys[1].r, keys[2].r, frame),
                   DeathArmos_GlowChannel(keys[0].g, keys[1].g, keys[2].g, frame),
                   DeathArmos_GlowChannel(keys[0].b, keys[1].b, keys[2].b, frame), 255);
    gSPEndDisplayList(cursor++);
    return glow;
}

static void DeathArmos_DrawRocks(EnDeathArmos* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    for (s32 i = 0; i < DEATH_ARMOS_ROCK_COUNT; i++) {
        DeathArmosRock* rock = &this->rocks[i];

        Matrix_SetTranslateRotateYXZ(rock->pos.x, rock->pos.y, rock->pos.z, &rock->rot);
        Matrix_Scale(rock->scale, rock->scale, rock->scale, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRockDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnDeathArmos_Draw(Actor* thisx, PlayState* play) {
    EnDeathArmos* this = (EnDeathArmos*)thisx;

    if (this->actionFunc != DeathArmos_DeathFade) {
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)DeathArmos_MakeGlowDL(this, play));
        CLOSE_DISPS(play->state.gfxCtx);
        SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, DeathArmos_OverrideLimbDraw,
                          DeathArmos_PostLimbDraw, this);
    }
    if (this->debrisTimer > 0) {
        DeathArmos_DrawRocks(this, play);
    }
}

// MM's CanKillEnemy(ACTOR_EN_FAMOS): only a light arrow flips it so its own slam breaks it.
static bool DeathArmos_CanKillInLogic(void* userData) {
    return sApi->LogicCanUseItem(RG_LIGHT_ARROWS);
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
    static Vec3s sSoulJointTable[DEATH_ARMOS_LIMB_MAX];
    static Vec3s sSoulMorphTable[DEATH_ARMOS_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_Init(play, &sSoulSkelAnime, (SkeletonHeader*)sDeathArmosSkel, (AnimationHeader*)sDeathArmosIdleAnim,
                       sSoulJointTable, sSoulMorphTable, DEATH_ARMOS_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.008f, 0.008f, 0.008f, MTXMODE_APPLY);
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
    SOHCustomItemDefinition soul = Z64Items_Define(DEATH_ARMOS_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Death Armos";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Death Armos%w!&Death Armos can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " DEATH_ARMOS_SOUL_ITEM_KEY);
    }
}

static void RegisterDeathArmos(void) {
    SOHActorDefinition deathArmos = { 0 };

    deathArmos.structSize = sizeof(deathArmos);
    deathArmos.key = DEATH_ARMOS_KEY;
    deathArmos.description = "Death Armos";
    deathArmos.category = ACTORCAT_ENEMY;
    deathArmos.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_IGNORE_QUAKE;
    deathArmos.objectId = OBJECT_GAMEPLAY_KEEP;
    deathArmos.instanceSize = sizeof(EnDeathArmos);
    deathArmos.init = EnDeathArmos_Init;
    deathArmos.destroy = EnDeathArmos_Destroy;
    deathArmos.update = EnDeathArmos_Update;
    deathArmos.draw = EnDeathArmos_Draw;
    deathArmos.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    deathArmos.enemyParams = 0xFF;
    deathArmos.enemySpawnHeight = 60;
    deathArmos.naviHint = "Death Armos&%cIt drops on you from above. Turn it upside down with a %ylight arrow%c to "
                          "its emblem and let it fall!%w";
    deathArmos.soulKey = DEATH_ARMOS_KEY;
    deathArmos.logicKill = DeathArmos_CanKillInLogic;
    if (sApi->RegisterActor(&deathArmos) < 0) {
        sApi->Log("Could not register " DEATH_ARMOS_KEY);
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
    RegisterDeathArmos();
}
