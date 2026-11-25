// Majora's Mask's Bad Bat (En_Bat) as an Unbound custom enemy, drawn from mm.o2r.
// One bat per spawn: MM's group spawning and Graveyard switch flag belong to its own scenes.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define BAD_BAT_KEY "mm.bad_bat"
#define BAD_BAT_SOUL_ITEM_KEY "soul.mm.bad_bat"

#define OBJ_BAT "__OTR__objects/object_bat/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL (1 << 0)
#define BGCHECK_UPDATE_FLOOR (1 << 2)

#define BAD_BAT_FLAP_FRAME 5
#define BAD_BAT_WING_FRAMES 9
#define BAD_BAT_MAX_ATTACKING 3
#define BAD_BAT_DIVE_RANGE 300.0f
#define BAD_BAT_ROOST_RADIUS 300.0f
#define BAD_BAT_BODY_HEIGHT 2000.0f

typedef enum {
    BAD_BAT_DMGEFF_NONE,
    BAD_BAT_DMGEFF_STUN,
    BAD_BAT_DMGEFF_FIRE,
    BAD_BAT_DMGEFF_ICE,
    BAD_BAT_DMGEFF_LIGHT
} BadBatDamageEffect;

struct EnBadBat;

typedef void (*EnBadBatActionFunc)(struct EnBadBat*, PlayState*);

typedef struct EnBadBat {
    Actor actor;
    EnBadBatActionFunc actionFunc;
    s16 timer;
    s16 pitchTarget;
    s16 yawTarget;
    s16 wingFrame;
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[1];
} EnBadBat;

static const ALIGN_ASSET(2) char sBadBatSetupDL[] = OBJ_BAT "gBadBatSetupDL";
static const ALIGN_ASSET(2) char sBadBatBodyDL[] = OBJ_BAT "gBadBatBodyDL";
static const ALIGN_ASSET(2) char sBadBatWings0DL[] = OBJ_BAT "gBadBatWingsFrame0DL";
static const ALIGN_ASSET(2) char sBadBatWings1DL[] = OBJ_BAT "gBadBatWingsFrame1DL";
static const ALIGN_ASSET(2) char sBadBatWings2DL[] = OBJ_BAT "gBadBatWingsFrame2DL";
static const ALIGN_ASSET(2) char sBadBatWings3DL[] = OBJ_BAT "gBadBatWingsFrame3DL";
static const ALIGN_ASSET(2) char sBadBatWings4DL[] = OBJ_BAT "gBadBatWingsFrame4DL";
static const ALIGN_ASSET(2) char sBadBatWings5DL[] = OBJ_BAT "gBadBatWingsFrame5DL";
static const ALIGN_ASSET(2) char sBadBatWings6DL[] = OBJ_BAT "gBadBatWingsFrame6DL";
static const ALIGN_ASSET(2) char sBadBatWings7DL[] = OBJ_BAT "gBadBatWingsFrame7DL";
static const ALIGN_ASSET(2) char sBadBatWings8DL[] = OBJ_BAT "gBadBatWingsFrame8DL";

static const char* const sWingsDLs[BAD_BAT_WING_FRAMES] = {
    sBadBatWings0DL, sBadBatWings1DL, sBadBatWings2DL, sBadBatWings3DL, sBadBatWings4DL,
    sBadBatWings5DL, sBadBatWings6DL, sBadBatWings7DL, sBadBatWings8DL,
};

static ColliderJntSphElementInit sElementsInit[1] = {
    {
        { ELEMTYPE_UNK0,
          { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x04 },
          { 0xFFCFFFFF, 0x00, 0x00 },
          TOUCH_ON | TOUCH_SFX_HARD,
          BUMP_ON,
          OCELEM_ON },
        { 0, { { 0, 0, 0 }, 15 }, 100 },
    },
};

static ColliderJntSphInit sColliderInit = {
    { COLTYPE_HIT3, AT_NONE | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    1,
    sElementsInit,
};

// MM entries multiply the weapon's own damage; OoT's are absolute. One hit of anything kills it either way.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, BAD_BAT_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, BAD_BAT_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, BAD_BAT_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, BAD_BAT_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, BAD_BAT_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, BAD_BAT_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, BAD_BAT_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(2, BAD_BAT_DMGEFF_NONE),
    /* Kokiri sword  */ DMG_ENTRY(1, BAD_BAT_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, BAD_BAT_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, BAD_BAT_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(4, BAD_BAT_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, BAD_BAT_DMGEFF_ICE),
    /* Light arrow   */ DMG_ENTRY(4, BAD_BAT_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, BAD_BAT_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, BAD_BAT_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, BAD_BAT_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(2, BAD_BAT_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(2, BAD_BAT_DMGEFF_ICE),
    /* Light magic   */ DMG_ENTRY(2, BAD_BAT_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, BAD_BAT_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, BAD_BAT_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(2, BAD_BAT_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(8, BAD_BAT_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(4, BAD_BAT_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, BAD_BAT_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, BAD_BAT_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, BAD_BAT_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, BAD_BAT_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, BAD_BAT_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, BAD_BAT_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, BAD_BAT_DMGEFF_NONE),
};

static CollisionCheckInfoInit sColChkInfoInit = { 1, 15, 30, 10 };

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static s32 sNumberAttacking;

static void BadBat_FlyIdle(EnBadBat* this, PlayState* play);
static void BadBat_DiveAttack(EnBadBat* this, PlayState* play);
static void BadBat_Die(EnBadBat* this, PlayState* play);
static void BadBat_Stunned(EnBadBat* this, PlayState* play);

static void BadBat_StepWings(EnBadBat* this, s32 frameStep) {
    s32 previousFrame = this->wingFrame;

    this->wingFrame = (this->wingFrame + frameStep) % BAD_BAT_WING_FRAMES;
    if (previousFrame < BAD_BAT_FLAP_FRAME && this->wingFrame >= BAD_BAT_FLAP_FRAME) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_FFLY_FLY);
    }
}

static void BadBat_SetupFlyIdle(EnBadBat* this) {
    this->timer = 100;
    this->collider.base.acFlags |= AC_ON;
    this->actor.speedXZ = 3.5f;
    this->actionFunc = BadBat_FlyIdle;
}

static void BadBat_SetupDiveAttack(EnBadBat* this) {
    this->collider.base.atFlags |= AT_ON;
    this->timer = 300;
    this->actor.speedXZ = 4.0f;
    sNumberAttacking++;
    this->actionFunc = BadBat_DiveAttack;
}

static void BadBat_StopDiving(EnBadBat* this) {
    this->collider.base.atFlags &= ~AT_ON;
    sNumberAttacking--;
}

static void BadBat_WanderYaw(EnBadBat* this) {
    bool isTurnDone = Math_ScaledStepToS(&this->actor.shape.rot.y, this->yawTarget, 0x300);

    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->actor.bgCheckFlags &= ~BGCHECKFLAG_WALL;
        this->yawTarget = this->actor.wallYaw;
    } else if (Math3D_Dist2DSq(this->actor.world.pos.x, this->actor.world.pos.z, this->actor.home.pos.x,
                               this->actor.home.pos.z) > SQ(BAD_BAT_ROOST_RADIUS)) {
        this->yawTarget = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
    } else if (isTurnDone && Rand_ZeroOne() < 0.015f) {
        s16 turn = (s32)(0x1000 * Rand_ZeroOne()) + 0x1000;

        this->yawTarget = this->actor.shape.rot.y + (Rand_ZeroOne() < 0.5f ? -turn : turn);
    }
}

// Stays within 100 units of its roost height and pulls up off the floor and water.
static void BadBat_WanderPitch(EnBadBat* this) {
    bool isTiltDone = Math_ScaledStepToS(&this->actor.shape.rot.x, this->pitchTarget, 0x100);

    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || this->actor.yDistToWater > -40.0f) {
        this->pitchTarget = -0x1000;
    } else if (this->actor.world.pos.y < this->actor.home.pos.y - 100.0f) {
        this->pitchTarget = -((s32)(0x800 * Rand_ZeroOne()) + 0x800);
    } else if (this->actor.world.pos.y > this->actor.home.pos.y + 100.0f) {
        this->pitchTarget = (s32)(0x800 * Rand_ZeroOne()) + 0x800;
    } else if (isTiltDone && Rand_ZeroOne() < 0.015f) {
        s16 tilt = (s32)(0x400 * Rand_ZeroOne()) + 0x400;

        this->pitchTarget += Rand_ZeroOne() < 0.5f ? -tilt : tilt;
        this->pitchTarget = CLAMP(this->pitchTarget, -0x1000, 0x1000);
    }
}

static void BadBat_FlyIdle(EnBadBat* this, PlayState* play) {
    BadBat_StepWings(this, 1);
    BadBat_WanderYaw(this);
    BadBat_WanderPitch(this);
    if (this->timer != 0) {
        this->timer--;
    }
    bool canDive =
        this->actor.xzDistToPlayer < BAD_BAT_DIVE_RANGE && this->timer == 0 && sNumberAttacking < BAD_BAT_MAX_ATTACKING;

    if (canDive) {
        BadBat_SetupDiveAttack(this);
    }
}

static bool BadBat_ShouldAbortDive(EnBadBat* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    return this->timer == 0 || (this->collider.base.atFlags & AT_HIT) ||
           (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) ||
           this->actor.yDistToWater > -40.0f;
}

static void BadBat_DiveAttack(EnBadBat* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    bool isFacingPlayer = Actor_IsFacingPlayer(&this->actor, 0x2800);

    BadBat_StepWings(this, 2);
    if (isFacingPlayer) {
        Vec3f prey = { player->actor.world.pos.x, player->actor.world.pos.y + 20.0f, player->actor.world.pos.z };
        s16 pitchTarget = CLAMP(Actor_WorldPitchTowardPoint(&this->actor, &prey), -0x3000, 0x3000);

        Math_SmoothStepToS(&this->actor.shape.rot.x, pitchTarget, 2, 0x400, 0x40);
    } else {
        Math_SmoothStepToS(&this->actor.shape.rot.x, -0x800, 2, 0x100, 0x10);
    }
    if (isFacingPlayer || this->actor.xzDistToPlayer > 80.0f) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0xC00, 0xC0);
    }
    this->timer--;
    if (BadBat_ShouldAbortDive(this, play)) {
        if (this->collider.base.atFlags & AT_HIT) {
            this->collider.base.atFlags &= ~AT_HIT;
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_FFLY_ATTACK);
        }
        BadBat_StopDiving(this);
        BadBat_SetupFlyIdle(this);
        return;
    }
    s16 wallOffPlayer = this->actor.wallYaw - this->actor.yawTowardsPlayer;

    if ((this->actor.bgCheckFlags & BGCHECKFLAG_WALL) && ABS(wallOffPlayer) > 0x6800) {
        BadBat_StopDiving(this);
        this->actor.bgCheckFlags &= ~BGCHECKFLAG_WALL;
        this->yawTarget = this->actor.wallYaw;
        BadBat_SetupFlyIdle(this);
    }
}

static void BadBat_SetupDie(EnBadBat* this, PlayState* play) {
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Enemy_StartFinishingBlow(play, &this->actor);
    this->actor.speedXZ *= Math_CosS(this->actor.world.rot.x);
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    this->actor.velocity.y = 0.0f;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_FFLY_DEAD);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 40);
    if (this->actor.flags & ACTOR_FLAG_ATTACHED_TO_ARROW) {
        this->actor.speedXZ = 0.0f;
    }
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    this->actionFunc = BadBat_Die;
}

static void BadBat_Vanish(EnBadBat* this, PlayState* play) {
    EffectSsDeadDb_Spawn(play, &this->actor.world.pos, &sZeroVec, &sZeroVec, 100, 0, 255, 255, 255, 255, 255, 0, 0, 1,
                         9, true);
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xE0);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

// An arrow that pierced it carries it along; it only tumbles down once it is free.
static void BadBat_Die(EnBadBat* this, PlayState* play) {
    Math_StepToF(&this->actor.speedXZ, 0.0f, 0.5f);
    this->actor.colorFilterTimer = 40;
    if (this->actor.flags & ACTOR_FLAG_ATTACHED_TO_ARROW) {
        return;
    }
    Math_ScaledStepToS(&this->actor.shape.rot.x, 0x4000, 0x200);
    this->actor.shape.rot.z += 0x1780;
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || this->actor.floorHeight == BGCHECK_Y_MIN) {
        BadBat_Vanish(this, play);
    }
}

static void BadBat_SetupStunned(EnBadBat* this) {
    if (this->actionFunc != BadBat_Stunned) {
        this->actor.shape.yOffset = 700.0f;
        this->actor.velocity.y = 0.0f;
        this->actor.speedXZ = 0.0f;
        this->actor.world.pos.y += 13.0f;
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, this->timer);
    this->actionFunc = BadBat_Stunned;
}

static void BadBat_Stunned(EnBadBat* this, PlayState* play) {
    Math_ScaledStepToS(&this->actor.shape.rot.x, 0, 0x100);
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && this->actor.floorHeight != BGCHECK_Y_MIN) {
        this->actor.colorFilterTimer = 40;
        return;
    }
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 0) {
        BadBat_SetupFlyIdle(this);
    }
}

static void BadBat_UpdateDamage(EnBadBat* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Actor_SetDropFlag(&this->actor, &this->collider.elements[0].info, false);
    if (this->actionFunc == BadBat_DiveAttack) {
        BadBat_StopDiving(this);
    }
    this->collider.base.atFlags &= ~AT_ON;
    if (this->actor.colChkInfo.damageEffect == BAD_BAT_DMGEFF_STUN) {
        this->timer = 40;
        BadBat_SetupStunned(this);
        return;
    }
    Actor_ApplyDamage(&this->actor);
    BadBat_SetupDie(this, play);
}

// MM's Actor_MoveWithoutGravity: flies along its pitch as well as its yaw.
static void BadBat_Fly(EnBadBat* this) {
    f32 speedXZ = Math_CosS(this->actor.world.rot.x) * this->actor.speedXZ;

    this->actor.velocity.x = Math_SinS(this->actor.world.rot.y) * speedXZ;
    this->actor.velocity.y = Math_SinS(this->actor.world.rot.x) * this->actor.speedXZ;
    this->actor.velocity.z = Math_CosS(this->actor.world.rot.y) * speedXZ;
    Actor_UpdatePos(&this->actor);
}

static void BadBat_Move(EnBadBat* this, PlayState* play) {
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.world.rot.x = -this->actor.shape.rot.x;
    if (this->actor.colChkInfo.health != 0 && this->actionFunc != BadBat_Stunned) {
        BadBat_Fly(this);
    } else {
        Actor_MoveXZGravity(&this->actor);
    }
    // MM skips the wall check while wandering; placed in any room, the bat must not drift through walls.
    Actor_UpdateBgCheckInfo(play, &this->actor, 12.0f, 15.0f, 50.0f, BGCHECK_UPDATE_WALL | BGCHECK_UPDATE_FLOOR);
    Actor_SetFocus(&this->actor, this->actor.shape.yOffset * 0.01f);
    this->collider.elements[0].dim.worldSphere.center = this->actor.focus.pos;
}

static void EnBadBat_Init(Actor* thisx, PlayState* play) {
    EnBadBat* this = (EnBadBat*)thisx;

    this->actor.gravity = -0.5f;
    this->actor.targetArrowOffset = 2000.0f;
    this->actor.uncullZoneForward = 3000.0f;
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sColliderInit, this->colliderElements);
    this->collider.elements[0].dim.worldSphere.radius = sElementsInit[0].dim.modelSphere.radius;
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    ActorShape_Init(&this->actor.shape, BAD_BAT_BODY_HEIGHT, ActorShadow_DrawCircle, 25.0f);
    this->wingFrame = Rand_ZeroOne() * BAD_BAT_WING_FRAMES;
    this->actor.yDistToWater = BGCHECK_Y_MIN;
    Actor_SetFocus(&this->actor, 20.0f);
    BadBat_SetupFlyIdle(this);
}

static void EnBadBat_Destroy(Actor* thisx, PlayState* play) {
    EnBadBat* this = (EnBadBat*)thisx;

    if (this->actionFunc == BadBat_DiveAttack) {
        sNumberAttacking--;
    }
    Collider_DestroyJntSph(play, &this->collider);
}

static void EnBadBat_Update(Actor* thisx, PlayState* play) {
    EnBadBat* this = (EnBadBat*)thisx;

    BadBat_UpdateDamage(this, play);
    this->actionFunc(this, play);
    if (this->actionFunc != BadBat_Stunned) {
        Math_StepToF(&this->actor.shape.yOffset, BAD_BAT_BODY_HEIGHT, 200.0f);
    }
    BadBat_Move(this, play);
    if (this->collider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->actionFunc == BadBat_DiveAttack) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

static void EnBadBat_Draw(Actor* thisx, PlayState* play) {
    EnBadBat* this = (EnBadBat*)thisx;

    if (this->actor.projectedPos.z <= 0.0f) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBadBatSetupDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBadBatBodyDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sWingsDLs[this->wingFrame]);
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's CanKillEnemy(ACTOR_EN_BAT): any sword, hookshot, bow, explosive or form fist; one HP falls to anything.
static bool BadBat_CanKillInLogic(void* userData) {
    return sApi->LogicHasCapability("CanAttack");
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
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBadBatSetupDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBadBatBodyDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sWingsDLs[play->state.frames % BAD_BAT_WING_FRAMES]);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterSoul(void) {
    static const SOHCustomItemRandomizer sSoulRandomizer = {
        .structSize = sizeof(SOHCustomItemRandomizer),
        .flags = SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT,
        .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
    };
    SOHCustomItemDefinition soul = Z64Items_Define(BAD_BAT_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Bad Bats";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Bad Bats%w!&Bad Bats can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " BAD_BAT_SOUL_ITEM_KEY);
    }
}

static void RegisterBadBat(void) {
    SOHActorDefinition badBat = { 0 };

    badBat.structSize = sizeof(badBat);
    badBat.key = BAD_BAT_KEY;
    badBat.description = "Bad Bat";
    badBat.category = ACTORCAT_ENEMY;
    badBat.actorFlags =
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_IGNORE_QUAKE | ACTOR_FLAG_CAN_ATTACH_TO_ARROW;
    badBat.objectId = OBJECT_GAMEPLAY_KEEP;
    badBat.instanceSize = sizeof(EnBadBat);
    badBat.init = EnBadBat_Init;
    badBat.destroy = EnBadBat_Destroy;
    badBat.update = EnBadBat_Update;
    badBat.draw = EnBadBat_Draw;
    badBat.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    badBat.enemySpawnHeight = 80;
    badBat.naviHint = "Bad Bat&%cIt swoops down on you from the dark. Strike it as it dives!%w";
    badBat.soulKey = BAD_BAT_KEY;
    badBat.logicKill = BadBat_CanKillInLogic;
    if (sApi->RegisterActor(&badBat) < 0) {
        sApi->Log("Could not register " BAD_BAT_KEY);
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
    RegisterBadBat();
}
