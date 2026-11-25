// Majora's Mask's Nejiron (En_Baguo) as an Unbound custom enemy, drawn from mm.o2r.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define NEJIRON_KEY "mm.nejiron"
#define NEJIRON_SOUL_ITEM_KEY "soul.mm.nejiron"
#define GORON_MASK_KEY "nei.goron_form_mask"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_GMO "__OTR__objects/object_gmo/"

#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_FLOOR ((1 << 0) | (1 << 2))

#define MM_SE_EN_BAKUO_ROLL 0x3805
#define MM_SE_EN_BAKUO_VOICE 0x38FB
#define MM_SE_EN_BAKUO_DEAD 0x39C8
#define MM_SE_EN_BAKUO_APPEAR 0x39C9

#define NEJIRON_WAKE_RANGE 200.0f
#define NEJIRON_ROAM_RADIUS 240.0f
#define NEJIRON_BURIED_OFFSET -3000.0f
#define NEJIRON_RISEN_OFFSET 2700.0f
#define NEJIRON_EYE_FRAMES 3
#define NEJIRON_LIMB_MAX 3

typedef enum { NEJIRON_DMGEFF_NONE, NEJIRON_DMGEFF_KILL = 0xE, NEJIRON_DMGEFF_RECOIL } NejironDamageEffect;

struct EnNejiron;

typedef void (*EnNejironActionFunc)(struct EnNejiron*, PlayState*);

typedef struct EnNejiron {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[NEJIRON_LIMB_MAX];
    Vec3s morphTable[NEJIRON_LIMB_MAX];
    EnNejironActionFunc actionFunc;
    bool isRollingLeft;
    bool hasBounced;
    s16 eyeIndex;
    s16 blinkTimer;
    s16 timer;
    Vec3f rollSpeed;
    Vec3f targetRollSpeed;
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[1];
} EnNejiron;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sNejironSkel[] = OBJ_GMO "gNejironSkel";
static const ALIGN_ASSET(2) char sNejironIdleAnim[] = OBJ_GMO "gNejironIdleAnim";
static const ALIGN_ASSET(2) char sNejironEyeOpenTex[] = OBJ_GMO "gNejironEyeOpenTex";
static const ALIGN_ASSET(2) char sNejironEyeHalfTex[] = OBJ_GMO "gNejironEyeHalfTex";
static const ALIGN_ASSET(2) char sNejironEyeClosedTex[] = OBJ_GMO "gNejironEyeClosedTex";

static const char* const sEyeTextures[NEJIRON_EYE_FRAMES] = {
    sNejironEyeOpenTex,
    sNejironEyeHalfTex,
    sNejironEyeClosedTex,
};

static ColliderJntSphElementInit sElementsInit[1] = {
    {
        { ELEMTYPE_UNK0,
          { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x04 },
          { 0xFFCFFFFF, 0x00, 0x00 },
          TOUCH_ON | TOUCH_SFX_NORMAL,
          BUMP_ON,
          OCELEM_ON },
        { 1, { { 80, 80, 0 }, 30 }, 100 },
    },
};

static ColliderJntSphInit sColliderInit = {
    { COLTYPE_HARD, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_HARD | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    1,
    sElementsInit,
};

// Swords, blunt blows, the hookshot and explosives set it off; arrows, nuts and seeds only bounce off.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, NEJIRON_DMGEFF_RECOIL),
    /* Deku stick    */ DMG_ENTRY(0, NEJIRON_DMGEFF_RECOIL),
    /* Slingshot     */ DMG_ENTRY(0, NEJIRON_DMGEFF_RECOIL),
    /* Explosive     */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Boomerang     */ DMG_ENTRY(3, NEJIRON_DMGEFF_KILL),
    /* Normal arrow  */ DMG_ENTRY(0, NEJIRON_DMGEFF_RECOIL),
    /* Hammer swing  */ DMG_ENTRY(2, NEJIRON_DMGEFF_KILL),
    /* Hookshot      */ DMG_ENTRY(3, NEJIRON_DMGEFF_KILL),
    /* Kokiri sword  */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Master sword  */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Giant's Knife */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Fire arrow    */ DMG_ENTRY(0, NEJIRON_DMGEFF_RECOIL),
    /* Ice arrow     */ DMG_ENTRY(0, NEJIRON_DMGEFF_RECOIL),
    /* Light arrow   */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Unk arrow 1   */ DMG_ENTRY(0, NEJIRON_DMGEFF_RECOIL),
    /* Unk arrow 2   */ DMG_ENTRY(0, NEJIRON_DMGEFF_RECOIL),
    /* Unk arrow 3   */ DMG_ENTRY(0, NEJIRON_DMGEFF_RECOIL),
    /* Fire magic    */ DMG_ENTRY(0, NEJIRON_DMGEFF_RECOIL),
    /* Ice magic     */ DMG_ENTRY(0, NEJIRON_DMGEFF_RECOIL),
    /* Light magic   */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Shield        */ DMG_ENTRY(0, NEJIRON_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, NEJIRON_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Giant spin    */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Master spin   */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Kokiri jump   */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Giant jump    */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Master jump   */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Unknown 1     */ DMG_ENTRY(0, NEJIRON_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, NEJIRON_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(1, NEJIRON_DMGEFF_KILL),
    /* Unknown 2     */ DMG_ENTRY(0, NEJIRON_DMGEFF_NONE),
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

static void Nejiron_UndergroundIdle(EnNejiron* this, PlayState* play);
static void Nejiron_Emerge(EnNejiron* this, PlayState* play);
static void Nejiron_Idle(EnNejiron* this, PlayState* play);
static void Nejiron_Roll(EnNejiron* this, PlayState* play);
static void Nejiron_Retreat(EnNejiron* this, PlayState* play);
static void Nejiron_AfterBlast(EnNejiron* this, PlayState* play);
static void EnNejiron_Draw(Actor* thisx, PlayState* play);

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

static void Nejiron_SpawnDust(EnNejiron* this, PlayState* play) {
    if ((play->gameplayFrames % 8) == 0) {
        Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, this->actor.shape.shadowScale - 20.0f, 10,
                                 8.0f, 500, 10, true);
    }
}

static bool Nejiron_IsActive(EnNejiron* this) {
    return this->actionFunc != Nejiron_UndergroundIdle && this->actionFunc != Nejiron_AfterBlast;
}

static void Nejiron_UndergroundIdle(EnNejiron* this, PlayState* play) {
    this->actor.shape.rot.y = this->actor.world.rot.y;
    if (this->actor.xzDistToPlayer >= NEJIRON_WAKE_RANGE) {
        return;
    }
    this->actor.draw = EnNejiron_Draw;
    PlaySfx(&this->actor, MM_SE_EN_BAKUO_APPEAR, NA_SE_EN_DODO_M_UP);
    this->actor.world.rot.x = 0;
    this->actor.world.rot.z = 0;
    this->actor.flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = Nejiron_Emerge;
}

static void Nejiron_Emerge(EnNejiron* this, PlayState* play) {
    this->actor.world.rot.y += 0x1518;
    this->actor.shape.rot.y = this->actor.world.rot.y;
    Nejiron_SpawnDust(this, play);
    Math_ApproachF(&this->actor.shape.shadowScale, 50.0f, 0.3f, 5.0f);
    Math_ApproachF(&this->actor.shape.yOffset, NEJIRON_RISEN_OFFSET, 100.0f, 500.0f);
    if (this->actor.shape.yOffset > NEJIRON_RISEN_OFFSET - 50.0f) {
        this->actor.shape.yOffset = NEJIRON_RISEN_OFFSET;
        this->timer = 60;
        this->actionFunc = Nejiron_Idle;
    }
}

static void Nejiron_SetupRoll(EnNejiron* this) {
    s16 offPlayer = this->actor.yawTowardsPlayer - this->actor.world.rot.y;

    this->targetRollSpeed = sZeroVec;
    this->rollSpeed = sZeroVec;
    if (ABS(offPlayer) < 0x2000) {
        this->targetRollSpeed.x = 2000.0f;
    } else {
        this->targetRollSpeed.z = 2000.0f;
        this->isRollingLeft = offPlayer > 0;
    }
    this->timer = 38;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->hasBounced = false;
    this->actionFunc = Nejiron_Roll;
}

// Turns to face Link in short grinding twists, then rolls at him.
static void Nejiron_Idle(EnNejiron* this, PlayState* play) {
    if (this->timer == 0) {
        Nejiron_SetupRoll(this);
        return;
    }
    Math_SmoothStepToS(&this->actor.world.rot.x, 0, 10, 0x64, 0x3E8);
    Math_SmoothStepToS(&this->actor.world.rot.z, 0, 10, 0x64, 0x3E8);
    bool isFacingAway = ABS(this->actor.world.rot.y - this->actor.yawTowardsPlayer) > 200;

    if ((this->timer & 8) != 0 && isFacingAway) {
        Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 30, 0x12C, 0x3E8);
        if ((play->gameplayFrames % 8) == 0) {
            Nejiron_SpawnDust(this, play);
            PlaySfx(&this->actor, MM_SE_EN_BAKUO_VOICE, NA_SE_EN_DODO_M_CRY);
        }
    }
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

static void Nejiron_SetupRetreat(EnNejiron* this) {
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Nejiron_Retreat;
}

static void Nejiron_Roll(EnNejiron* this, PlayState* play) {
    if (Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > NEJIRON_ROAM_RADIUS) {
        Nejiron_SetupRetreat(this);
        return;
    }
    if (this->timer == 0) {
        this->timer = 100;
        this->actor.world.rot.y = this->actor.shape.rot.y;
        this->actor.speedXZ = 0.0f;
        this->actionFunc = Nejiron_Idle;
        return;
    }
    if (!this->hasBounced && (this->collider.base.atFlags & AT_BOUNCED)) {
        this->isRollingLeft = !this->isRollingLeft;
        this->hasBounced = true;
        this->actor.speedXZ = -7.0f;
    }
    Math_ApproachF(&this->rollSpeed.x, this->targetRollSpeed.x, 0.2f, 1000.0f);
    Math_ApproachF(&this->rollSpeed.z, this->targetRollSpeed.z, 0.2f, 1000.0f);
    Math_ApproachF(&this->actor.speedXZ, 5.0f, 0.3f, 0.5f);
    this->actor.world.rot.x += (s16)this->rollSpeed.x;
    this->actor.world.rot.z += this->isRollingLeft ? -(s16)this->rollSpeed.z : (s16)this->rollSpeed.z;
    PlaySfx(&this->actor, MM_SE_EN_BAKUO_ROLL, NA_SE_EN_DODO_M_MOVE);
}

// Sinks back home when it rolls too far from its burrow.
static void Nejiron_Retreat(EnNejiron* this, PlayState* play) {
    this->actor.world.rot.y -= 0x1518;
    this->actor.shape.rot.y = this->actor.world.rot.y;
    Nejiron_SpawnDust(this, play);
    Math_ApproachF(&this->actor.shape.yOffset, NEJIRON_BURIED_OFFSET, 100.0f, 500.0f);
    Math_ApproachZeroF(&this->actor.shape.shadowScale, 0.3f, 5.0f);
    if (this->actor.shape.yOffset >= NEJIRON_BURIED_OFFSET + 30.0f) {
        return;
    }
    this->actor.shape.yOffset = NEJIRON_BURIED_OFFSET;
    this->actor.world.pos = this->actor.home.pos;
    PlaySfx(&this->actor, MM_SE_EN_BAKUO_APPEAR, NA_SE_EN_DODO_M_UP);
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = Nejiron_UndergroundIdle;
}

static void Nejiron_AfterBlast(EnNejiron* this, PlayState* play) {
    if (this->timer >= 26) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->timer == 0) {
        Actor_Kill(&this->actor);
    }
}

// Blows up into rock shards; the blast keeps hurting for a few frames.
static void Nejiron_Detonate(EnNejiron* this, PlayState* play) {
    static Vec3f sDebrisAccel = { 0.0f, -1.0f, 0.0f };

    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    this->actor.speedXZ = 0.0f;
    this->actor.shape.shadowScale = 0.0f;
    for (s32 i = 0; i < 20; i++) {
        Vec3f velocity = { Rand_CenteredFloat(14.0f), Rand_ZeroOne() * 20.0f, Rand_CenteredFloat(14.0f) };

        EffectSsHahen_Spawn(play, &this->actor.focus.pos, &velocity, &sDebrisAccel, 0, Rand_S16Offset(30, 30), -1, 20,
                            NULL);
    }
    EffectSsBomb2_SpawnLayered(play, &this->actor.world.pos, &sZeroVec, &sZeroVec, 100, 19);
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_BOMB_EXPLOSION);
    PlaySfx(&this->actor, MM_SE_EN_BAKUO_DEAD, NA_SE_EN_DODO_M_DEAD);
    this->timer = 30;
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Actor_SetScale(&this->actor, 0.0f);
    this->collider.elements[0].dim.scale = 3.0f;
    this->collider.elements[0].info.toucher.damage = 8;
    Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos, 0xB0);
    Enemy_StartFinishingBlow(play, &this->actor);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    this->actionFunc = Nejiron_AfterBlast;
}

static bool Nejiron_HasFallenOrSunk(EnNejiron* this) {
    bool hasFallen =
        !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && this->actor.world.pos.y < this->actor.home.pos.y - 100.0f;
    bool hasSunk =
        (this->actor.bgCheckFlags & (BGCHECKFLAG_WATER | BGCHECKFLAG_WATER_TOUCH)) && this->actor.yDistToWater >= 40.0f;

    return hasFallen || hasSunk;
}

static void Nejiron_CheckDetonation(EnNejiron* this, PlayState* play) {
    if (!Nejiron_IsActive(this) || this->actionFunc == Nejiron_Retreat) {
        return;
    }
    bool isHit = (this->collider.base.acFlags & AC_HIT) != 0;
    bool isKillingHit = isHit && this->actor.colChkInfo.damageEffect == NEJIRON_DMGEFF_KILL;

    this->collider.base.acFlags &= ~AC_HIT;
    if (isKillingHit || Nejiron_HasFallenOrSunk(this)) {
        Nejiron_Detonate(this, play);
    }
}

static void Nejiron_Blink(EnNejiron* this) {
    if (this->blinkTimer != 0) {
        return;
    }
    this->eyeIndex++;
    if (this->eyeIndex >= NEJIRON_EYE_FRAMES) {
        this->eyeIndex = 0;
        this->blinkTimer = Rand_ZeroFloat(60.0f) + 20.0f;
    }
}

static void EnNejiron_Init(Actor* thisx, PlayState* play) {
    EnNejiron* this = (EnNejiron*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 0.0f);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sNejironSkel, NULL, this->jointTable, this->morphTable,
                   NEJIRON_LIMB_MAX);
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.targetMode = 2;
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sColliderInit, this->colliderElements);
    this->actor.shape.yOffset = NEJIRON_BURIED_OFFSET;
    this->actor.gravity = -3.0f;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.draw = NULL;
    this->actionFunc = Nejiron_UndergroundIdle;
}

static void EnNejiron_Destroy(Actor* thisx, PlayState* play) {
    EnNejiron* this = (EnNejiron*)thisx;

    Collider_DestroyJntSph(play, &this->collider);
}

static void EnNejiron_Update(Actor* thisx, PlayState* play) {
    EnNejiron* this = (EnNejiron*)thisx;
    bool isAwake = this->actionFunc != Nejiron_UndergroundIdle;

    Actor_SetFocus(&this->actor, 30.0f);
    Nejiron_CheckDetonation(this, play);
    this->actionFunc(this, play);
    DECR(this->blinkTimer);
    DECR(this->timer);
    if (this->actionFunc == Nejiron_AfterBlast) {
        return;
    }
    if (isAwake) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    this->actor.shape.rot.x = this->actor.world.rot.x;
    this->actor.shape.rot.z = this->actor.world.rot.z;
    Nejiron_Blink(this);
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 60.0f, BGCHECK_UPDATE_WALL_FLOOR);
    if (isAwake) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void Nejiron_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    Collider_UpdateSpheres(limbIndex, &((EnNejiron*)thisx)->collider);
}

static void EnNejiron_Draw(Actor* thisx, PlayState* play) {
    EnNejiron* this = (EnNejiron*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyeTextures[this->eyeIndex]);
    CLOSE_DISPS(play->state.gfxCtx);
    SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, NULL, Nejiron_PostLimbDraw, this);
}

// MM's CanKillEnemy(ACTOR_EN_BAGUO): a sword, the hookshot or a form punch sets it off.
static bool Nejiron_CanKillInLogic(void* userData) {
    if (sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RG_HOOKSHOT) ||
        sApi->LogicCanUseItem(RG_MEGATON_HAMMER) || sApi->LogicCanUseItem(RG_BOOMERANG)) {
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
    static Vec3s sSoulJointTable[NEJIRON_LIMB_MAX];
    static Vec3s sSoulMorphTable[NEJIRON_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_Init(play, &sSoulSkelAnime, (SkeletonHeader*)sNejironSkel, (AnimationHeader*)sNejironIdleAnim,
                       sSoulJointTable, sSoulMorphTable, NEJIRON_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-sSoulJointTable[0].x, -sSoulJointTable[0].y, -sSoulJointTable[0].z, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyeTextures[0]);
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
    SOHCustomItemDefinition soul = Z64Items_Define(NEJIRON_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Nejirons";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Nejirons%w!&Nejirons can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " NEJIRON_SOUL_ITEM_KEY);
    }
}

static void RegisterNejiron(void) {
    SOHActorDefinition nejiron = { 0 };

    nejiron.structSize = sizeof(nejiron);
    nejiron.key = NEJIRON_KEY;
    nejiron.description = "Nejiron";
    nejiron.category = ACTORCAT_ENEMY;
    nejiron.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
    nejiron.objectId = OBJECT_GAMEPLAY_KEEP;
    nejiron.instanceSize = sizeof(EnNejiron);
    nejiron.init = EnNejiron_Init;
    nejiron.destroy = EnNejiron_Destroy;
    nejiron.update = EnNejiron_Update;
    nejiron.draw = EnNejiron_Draw;
    nejiron.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    nejiron.naviHint = "Nejiron&%cA living boulder! It explodes when struck, so hit it with your %rsword%c and back "
                       "away!%w";
    nejiron.soulKey = NEJIRON_KEY;
    nejiron.logicKill = Nejiron_CanKillInLogic;
    if (sApi->RegisterActor(&nejiron) < 0) {
        sApi->Log("Could not register " NEJIRON_KEY);
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
    RegisterNejiron();
}
