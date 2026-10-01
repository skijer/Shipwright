// Miniblin, a Wind Waker style imp that tries to steal a red rupee with its tail.
// Model @syeo501, code @trueffel; ported from NEI as a custom enemy the enemy randomizer can pick.

#include "soh/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#include "object_miniblin_assets.h"
#include "object_miniblin_assets.inc.c"

#define MINIBLIN_KEY "trutefel.enemy.miniblin"

// Unbound leaves the bgCheckFlags bits unnamed.
#define BGCHECK_GROUND (1 << 0)
#define BGCHECK_WALL (1 << 3)
#define BGCHECK_WATER (1 << 5)
#define BGCHECK_UPDATE_ALL ((1 << 0) | (1 << 2) | (1 << 3) | (1 << 4))

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000

#define MINIBLIN_SIGHT_RANGE 280.0f
#define MINIBLIN_TAIL_RANGE 35.0f
#define MINIBLIN_GIVE_UP_RANGE 150.0f
#define MINIBLIN_STOLEN_RUPEES 20
#define MINIBLIN_STEAL_CHANCE 0.4f
#define MINIBLIN_SHRINK_STEP 0.00034f

struct EnMiniblin;

typedef void (*EnMiniblinActionFunc)(struct EnMiniblin*, PlayState*);

typedef struct EnMiniblin {
    Actor actor;
    Vec3s jointTable[GMINIBLINSKEL_NUM_LIMBS];
    Vec3s morphTable[GMINIBLINSKEL_NUM_LIMBS];
    SkelAnime skelAnime;
    ColliderCylinder collider;
    ColliderQuad quad;
    EnMiniblinActionFunc actionFunc;
    s16 eyeIndex;
    s16 timer;
    s16 deathTimer;
    s16 damageTimer;
    s16 blinkTimer;
    s16 hurtboxCooldown;
    u8 rupeeStolen;
    u8 aboutToSteal;
} EnMiniblin;

typedef enum {
    MINIBLIN_ANIMATION_IDLE,
    MINIBLIN_ANIMATION_JUMP,
    MINIBLIN_ANIMATION_TAILATTACK,
    MINIBLIN_ANIMATION_DAMAGE,
    MINIBLIN_ANIMATION_LAUGH,
    MINIBLIN_ANIMATION_BOMBTHROW,
    MINIBLIN_ANIMATION_DEATH,
} EnMiniblinAnimation;

typedef enum {
    MINIBLIN_EYES_NORMAL,
    MINIBLIN_EYES_HALFCLOSED,
    MINIBLIN_EYES_CLOSED,
    MINIBLIN_EYES_LAUGH,
    MINIBLIN_EYES_HIT,
} EnMiniblinEyes;

typedef enum {
    MINIBLIN_DMGEFF_NONE,
    MINIBLIN_DMGEFF_STUN,
    MINIBLIN_DMGEFF_ICE_MAGIC = 6,
    MINIBLIN_DMGEFF_LIGHT_MAGIC = 13,
    MINIBLIN_DMGEFF_FIRE,
} EnMiniblinDamageEffect;

static const SOHModApi* sApi;

static void EnMiniblin_SetupDoNothing(EnMiniblin* this, PlayState* play);
static void EnMiniblin_ApproachPlayer(EnMiniblin* this, PlayState* play);
static void EnMiniblin_SetupLaugh(EnMiniblin* this, PlayState* play);
static void EnMiniblin_TailAttack(EnMiniblin* this, PlayState* play);
static void EnMiniblin_Laugh(EnMiniblin* this, PlayState* play);
static void EnMiniblin_Disappear(EnMiniblin* this, PlayState* play);
static void EnMiniblin_Die(EnMiniblin* this, PlayState* play);

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_HIT5,
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
    { 20, 45, 0, { 0, 0, 0 } },
};

static ColliderQuadInit sTailQuadInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK0,
        { 0x20000000, 0x00, 0x8 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static AnimationInfo sAnimationInfo[] = {
    { &gMiniblinSkelIdleAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 3.0f },
    { &gMiniblinSkelJumpAnim, 4.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, 3.0f },
    { &gMiniblinSkelTailattackAnim, 1.5f, 0.0f, -1.0f, ANIMMODE_ONCE, 0.0f },
    { &gMiniblinSkelDamageAnim, 2.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 0.0f },
    { &gMiniblinSkelLaughAnim, 1.5f, 0.0f, -1.0f, ANIMMODE_ONCE, 0.0f },
    { &gMiniblinSkelBombthrowAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 0.0f },
    { &gMiniblinSkelDeathAnim, 3.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 0.0f },
};

static const char* sEyeTextures[] = {
    gMiniblinSkel_eye_normal_rgba16, gMiniblinSkel_eye_halfclosed_rgba16, gMiniblinSkel_eye_closed_rgba16,
    gMiniblinSkel_eye_laugh_rgba16,  gMiniblinSkel_eye_hit_rgba16,
};

static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, MINIBLIN_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, MINIBLIN_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(0, MINIBLIN_DMGEFF_STUN),
    /* Normal arrow  */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, MINIBLIN_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, MINIBLIN_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, MINIBLIN_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Ice arrow     */ DMG_ENTRY(4, MINIBLIN_DMGEFF_NONE),
    /* Light arrow   */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Unk arrow 1   */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(0, MINIBLIN_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(3, MINIBLIN_DMGEFF_ICE_MAGIC),
    /* Light magic   */ DMG_ENTRY(4, MINIBLIN_DMGEFF_LIGHT_MAGIC),
    /* Shield        */ DMG_ENTRY(0, MINIBLIN_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, MINIBLIN_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(1, MINIBLIN_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(4, MINIBLIN_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, MINIBLIN_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, MINIBLIN_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, MINIBLIN_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, MINIBLIN_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, MINIBLIN_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, MINIBLIN_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, MINIBLIN_DMGEFF_NONE),
};

static CollisionCheckInfoInit2 sColChkInit = { 4, 25, 35, 0, MASS_HEAVY };

static Vec3f sTailQuadVertices[4] = {
    { 0.0f, 0.0f, 0.0f },
    { 0.0f, 8000.0f, 0.0f },
    { 0.0f, 0.0f, 5000.0f },
    { 0.0f, 8000.0f, 5000.0f },
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static void EnMiniblin_SetupAction(EnMiniblin* this, EnMiniblinActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

static void EnMiniblin_ChangeAnimation(EnMiniblin* this, s32 index) {
    Animation_ChangeByInfo(&this->skelAnime, sAnimationInfo, index);
}

static void EnMiniblin_UpdateEyes(EnMiniblin* this) {
    if (this->eyeIndex > MINIBLIN_EYES_CLOSED || DECR(this->blinkTimer) != 0) {
        return;
    }
    this->eyeIndex++;
    if (this->eyeIndex >= MINIBLIN_EYES_CLOSED) {
        this->blinkTimer = Rand_S16Offset(30, 30);
        this->eyeIndex = MINIBLIN_EYES_NORMAL;
    }
}

static void EnMiniblin_UpdateBgCheck(EnMiniblin* this, PlayState* play) {
    Actor_UpdateBgCheckInfo(play, &this->actor, this->actor.colChkInfo.cylHeight, this->actor.colChkInfo.cylRadius,
                            this->actor.colChkInfo.cylHeight, BGCHECK_UPDATE_ALL);
}

static void EnMiniblin_Rotate(EnMiniblin* this, s16 targetYaw) {
    Math_ApproachS(&this->actor.world.rot.y, targetYaw, 3, 2000);
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 3000);
}

// The jump only carries it forward while its feet are off the ground.
static void EnMiniblin_Hop(EnMiniblin* this, f32 speed, s16 targetYaw) {
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 17.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_TEKU_WALK);
    }
    if (this->skelAnime.curFrame >= 18.0f) {
        this->actor.speedXZ = 0.0f;
        return;
    }
    Math_ApproachF(&this->actor.speedXZ, speed, 0.5f, 2.0f);
    EnMiniblin_Rotate(this, targetYaw);
}

static void EnMiniblin_DoNothing(EnMiniblin* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.xzDistToPlayer >= MINIBLIN_SIGHT_RANGE) {
        return;
    }
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_JUMP);
    EnMiniblin_SetupAction(this, EnMiniblin_ApproachPlayer);
}

static void EnMiniblin_SetupDoNothing(EnMiniblin* this, PlayState* play) {
    this->actor.speedXZ = 0.0f;
    this->eyeIndex = MINIBLIN_EYES_NORMAL;
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_IDLE);
    EnMiniblin_SetupAction(this, EnMiniblin_DoNothing);
}

static void EnMiniblin_SetupTailAttack(EnMiniblin* this, PlayState* play) {
    this->actor.speedXZ = 0.0f;
    this->timer = 3;
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_TAILATTACK);
    EnMiniblin_SetupAction(this, EnMiniblin_TailAttack);
}

static void EnMiniblin_ApproachPlayer(EnMiniblin* this, PlayState* play) {
    EnMiniblin_Hop(this, 20.0f / 3.0f, this->actor.yawTowardsPlayer);

    // Landing kills the jump's momentum, so it does not skid past the player.
    if (this->actor.bgCheckFlags & BGCHECK_GROUND) {
        this->actor.velocity.y = 0.0f;
    }
    if (this->actor.xzDistToPlayer < MINIBLIN_TAIL_RANGE) {
        EnMiniblin_SetupTailAttack(this, play);
    } else if (this->actor.xzDistToPlayer > MINIBLIN_SIGHT_RANGE) {
        EnMiniblin_SetupDoNothing(this, play);
    }
}

static void EnMiniblin_Flee(EnMiniblin* this, PlayState* play) {
    EnMiniblin_Hop(this, 25.0f / 3.0f, (s16)(this->actor.yawTowardsPlayer + 0x8000));

    if (this->rupeeStolen) {
        if (DECR(this->timer) == 0) {
            EnMiniblin_SetupLaugh(this, play);
        }
        return;
    }
    if (this->actor.xzDistToPlayer > MINIBLIN_GIVE_UP_RANGE || (this->actor.bgCheckFlags & BGCHECK_WALL)) {
        EnMiniblin_SetupDoNothing(this, play);
    }
}

static void EnMiniblin_SetupFlee(EnMiniblin* this, PlayState* play) {
    static f32 sFleePitch = 1.5f;

    this->timer = 100;
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_JUMP);
    Audio_PlaySoundGeneral(NA_SE_VO_IN_LOST, &this->actor.world.pos, 4, &sFleePitch, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
    EnMiniblin_SetupAction(this, EnMiniblin_Flee);
}

static void EnMiniblin_ResumeAfterHit(EnMiniblin* this, PlayState* play) {
    if (this->rupeeStolen) {
        EnMiniblin_SetupFlee(this, play);
    } else {
        EnMiniblin_SetupDoNothing(this, play);
    }
}

static void EnMiniblin_TryStealRupee(EnMiniblin* this) {
    if (gSaveContext.rupees < MINIBLIN_STOLEN_RUPEES || this->rupeeStolen || this->aboutToSteal) {
        return;
    }
    if (Rand_ZeroOne() < MINIBLIN_STEAL_CHANCE) {
        Rupees_ChangeBy(-MINIBLIN_STOLEN_RUPEES);
        this->aboutToSteal = true;
    }
}

static void EnMiniblin_TailAttack(EnMiniblin* this, PlayState* play) {
    if (this->quad.base.atFlags & AT_HIT) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_NALE_MAGIC);
        EnMiniblin_TryStealRupee(this);
    }
    if (!SkelAnime_Update(&this->skelAnime) || DECR(this->timer) != 0) {
        return;
    }
    if (this->aboutToSteal) {
        this->rupeeStolen = true;
        this->aboutToSteal = false;
    }
    EnMiniblin_SetupFlee(this, play);
}

static void EnMiniblin_Stunned(EnMiniblin* this, PlayState* play) {
    if (this->actor.colorFilterTimer == 0) {
        EnMiniblin_ResumeAfterHit(this, play);
    }
}

static void EnMiniblin_SetupStunned(EnMiniblin* this, PlayState* play) {
    this->actor.speedXZ = 0.0f;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Animation_PlayOnceSetSpeed(&this->skelAnime, &gMiniblinSkelIdleAnim, 0.0f);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 120, COLORFILTER_OPA, 60);
    EnMiniblin_SetupAction(this, EnMiniblin_Stunned);
}

static void EnMiniblin_Damage(EnMiniblin* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime) && DECR(this->damageTimer) == 0) {
        EnMiniblin_ResumeAfterHit(this, play);
    }
}

static void EnMiniblin_SetupDamage(EnMiniblin* this, PlayState* play) {
    this->damageTimer = 3;
    this->eyeIndex = MINIBLIN_EYES_HIT;
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_DAMAGE);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    Actor_ApplyDamage(&this->actor);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_STALKID_DAMAGE);
    EnMiniblin_SetupAction(this, EnMiniblin_Damage);
}

static void EnMiniblin_SetupLaugh(EnMiniblin* this, PlayState* play) {
    static f32 sLaughPitch = 3.5f;
    static f32 sLaughVolume = 9.0f;

    this->actor.speedXZ = 0.0f;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->eyeIndex = MINIBLIN_EYES_LAUGH;
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_LAUGH);
    Audio_PlaySoundGeneral(NA_SE_EN_STAL_WARAU, &this->actor.world.pos, 4, &sLaughPitch, &sLaughVolume,
                           &gSfxDefaultReverb);
    EnMiniblin_SetupAction(this, EnMiniblin_Laugh);
}

static void EnMiniblin_Laugh(EnMiniblin* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    this->actor.speedXZ = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->timer = 12;
    EnMiniblin_SetupAction(this, EnMiniblin_Disappear);
}

static void EnMiniblin_Shrink(EnMiniblin* this) {
    Math_StepToF(&this->actor.scale.x, 0.0f, MINIBLIN_SHRINK_STEP);
    this->actor.scale.y = this->actor.scale.x;
    this->actor.scale.z = this->actor.scale.x;
}

static void EnMiniblin_Disappear(EnMiniblin* this, PlayState* play) {
    EnMiniblin_Shrink(this);
    if (DECR(this->timer) == 0) {
        Actor_Kill(&this->actor);
    }
}

static void EnMiniblin_SetupDie(EnMiniblin* this, PlayState* play) {
    this->timer = 12;
    this->deathTimer = 12;
    this->actor.speedXZ = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.shape.shadowAlpha = 0;
    this->eyeIndex = MINIBLIN_EYES_CLOSED;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_STALKID_DEAD);
    Enemy_StartFinishingBlow(play, &this->actor);
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_DEATH);
    EnMiniblin_SetupAction(this, EnMiniblin_Die);
}

static void EnMiniblin_Die(EnMiniblin* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime) || DECR(this->timer) != 0) {
        return;
    }
    if (this->deathTimer != 0) {
        this->deathTimer--;
    }
    EnMiniblin_Shrink(this);
    if (this->deathTimer != 0) {
        return;
    }
    if (this->rupeeStolen) {
        Item_DropCollectible(play, &this->actor.world.pos, ITEM00_RUPEE_RED);
    }
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xE0);
    Actor_Kill(&this->actor);
}

static void EnMiniblin_CheckDamage(EnMiniblin* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        this->hurtboxCooldown = 20;
        this->actor.speedXZ = 0.0f;

        if (this->actor.colChkInfo.damageEffect == MINIBLIN_DMGEFF_STUN) {
            Actor_ApplyDamage(&this->actor);
            EnMiniblin_SetupStunned(this, play);
        } else {
            EnMiniblin_SetupDamage(this, play);
        }
        if (this->actor.colChkInfo.health == 0) {
            EnMiniblin_SetupDie(this, play);
        }
    }
    if ((this->actor.bgCheckFlags & BGCHECK_WATER) && this->actionFunc != EnMiniblin_Die) {
        EnMiniblin_SetupDie(this, play);
    }
}

static bool EnMiniblin_CanBeHit(EnMiniblin* this) {
    return this->actionFunc != EnMiniblin_TailAttack && this->actionFunc != EnMiniblin_Laugh &&
           this->actionFunc != EnMiniblin_Disappear;
}

static void EnMiniblin_UpdateCollision(EnMiniblin* this, PlayState* play) {
    if (this->actionFunc == EnMiniblin_TailAttack) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->quad.base);
    }
    if (this->actionFunc == EnMiniblin_Die) {
        return;
    }
    Collider_UpdateCylinder(&this->actor, &this->collider);
    if (DECR(this->hurtboxCooldown) == 0 && EnMiniblin_CanBeHit(this)) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void EnMiniblin_Init(Actor* thisx, PlayState* play) {
    EnMiniblin* this = (EnMiniblin*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 100.0f);
    Actor_SetScale(&this->actor, 0.0035f);
    this->actor.targetMode = 3;
    this->actor.gravity = -1.0f;

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    Collider_InitQuad(play, &this->quad);
    Collider_SetQuad(play, &this->quad, &this->actor, &sTailQuadInit);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, &sDamageTable, &sColChkInit);

    SkelAnime_InitFlex(play, &this->skelAnime, &gMiniblinSkel, NULL, this->jointTable, this->morphTable,
                       GMINIBLINSKEL_NUM_LIMBS);
    EnMiniblin_SetupDoNothing(this, play);
}

static void EnMiniblin_Destroy(Actor* thisx, PlayState* play) {
    EnMiniblin* this = (EnMiniblin*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    Collider_DestroyQuad(play, &this->quad);
}

static void EnMiniblin_Update(Actor* thisx, PlayState* play) {
    EnMiniblin* this = (EnMiniblin*)thisx;

    EnMiniblin_CheckDamage(this, play);
    this->actionFunc(this, play);
    Actor_MoveXZGravity(&this->actor);
    EnMiniblin_UpdateBgCheck(this, play);
    EnMiniblin_UpdateEyes(this);
    EnMiniblin_UpdateCollision(this, play);
}

static void EnMiniblin_DrawRupee(PlayState* play, f32 offsetY, f32 offsetZ) {
    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Matrix_Scale(3.0f, 3.0f, 3.0f, MTXMODE_APPLY);
    Matrix_RotateX(2.0f, MTXMODE_APPLY);
    Matrix_RotateY(1.4f, MTXMODE_APPLY);
    Matrix_RotateZ(3.0f, MTXMODE_APPLY);
    Matrix_Translate(-500.0f, offsetY, offsetZ, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gRupeeRedTex);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gRupeeDL);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnMiniblin_UpdateTailQuad(EnMiniblin* this) {
    for (s32 i = 0; i < ARRAY_COUNT(sTailQuadVertices); i++) {
        Matrix_MultVec3f(&sTailQuadVertices[i], &this->quad.dim.quad[i]);
    }
    Collider_SetQuadVertices(&this->quad, &this->quad.dim.quad[0], &this->quad.dim.quad[1], &this->quad.dim.quad[2],
                             &this->quad.dim.quad[3]);
}

static void EnMiniblin_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnMiniblin* this = (EnMiniblin*)thisx;

    switch (limbIndex) {
        case GMINIBLINSKEL_TAILEND_LIMB:
            EnMiniblin_UpdateTailQuad(this);
            if (this->aboutToSteal) {
                EnMiniblin_DrawRupee(play, -700.0f, 450.0f);
            }
            break;
        case GMINIBLINSKEL_HAND_L_LIMB:
            if (this->rupeeStolen) {
                EnMiniblin_DrawRupee(play, 200.0f, 100.0f);
            }
            break;
        case GMINIBLINSKEL_BODY_LIMB:
            Matrix_MultVec3f(&sZeroVec, &this->actor.focus.pos);
            break;
        default:
            break;
    }
}

static void EnMiniblin_Draw(Actor* thisx, PlayState* play) {
    EnMiniblin* this = (EnMiniblin*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyeTextures[this->eyeIndex]);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          EnMiniblin_PostLimbDraw, this);

    CLOSE_DISPS(play->state.gfxCtx);
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
    definition.key = MINIBLIN_KEY;
    definition.description = "Miniblin";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                            ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnMiniblin);
    definition.init = EnMiniblin_Init;
    definition.destroy = EnMiniblin_Destroy;
    definition.update = EnMiniblin_Update;
    definition.draw = EnMiniblin_Draw;
    definition.enemyFlags = SOH_ACTOR_ENEMY;
    definition.naviHint = "Miniblin&%cIt steals %wRupees%c with its tail! Beat it quickly to get them back!%w";

    if (sApi->RegisterActor(&definition) < 0) {
        sApi->Log("Could not register " MINIBLIN_KEY);
    }
}
