/*
 * File: z_en_hammergeist.c
 * Overlay: Ovl_En_Hammergeist
 * Description: Molmauk (formerly Hammergeist), an enemy with an ice hammer and a fire hammer
 * Authors: @syeo501 (Model) | @trueffel (Code)
 */

#include "z_en_hammergeist.h"

#define FLAGS (ACTOR_FLAG_0 | ACTOR_FLAG_2 | ACTOR_FLAG_4 | ACTOR_FLAG_5)

void EnHammergeist_Init(Actor* thisx, PlayState* play);
void EnHammergeist_Destroy(Actor* thisx, PlayState* play);
void EnHammergeist_Update(Actor* thisx, PlayState* play);
void EnHammergeist_Draw(Actor* thisx, PlayState* play);

s32 EnHammergeist_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx,
                                   Gfx** gfx);
void EnHammergeist_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx);
void EnHammergeist_DeadPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx,
                                    Gfx** gfx); // Sets body parts in fire and body transparency

void EnHammergeist_UpdateBgCheck(EnHammergeist* this, PlayState* play);
void EnHammergeist_Movement(EnHammergeist* this, PlayState* play);
void EnHammergeist_CheckDamage(EnHammergeist* this, PlayState* play);

void EnHammergeist_SetupDoNothing(EnHammergeist* this, PlayState* play);
void EnHammergeist_DoNothing(EnHammergeist* this, PlayState* play);
void EnHammergeist_SetupApproachPlayer(EnHammergeist* this, PlayState* play);
void EnHammergeist_ApproachPlayer(EnHammergeist* this, PlayState* play);
void EnHammergeist_SetupDamage(EnHammergeist* this, PlayState* play);
void EnHammergeist_Damage(EnHammergeist* this, PlayState* play);
void EnHammergeist_SetupStunned(EnHammergeist* this, PlayState* play);
void EnHammergeist_Stunned(EnHammergeist* this, PlayState* play);
void EnHammergeist_SetupDie(EnHammergeist* this, PlayState* play);
void EnHammergeist_Die(EnHammergeist* this, PlayState* play);
void EnHammergeist_SetupExplosion(EnHammergeist* this, PlayState* play);
void EnHammergeist_Explosion(EnHammergeist* this, PlayState* play); // 2 Heart Damage
void EnHammergeist_SetupInfuse(EnHammergeist* this, PlayState* play);
void EnHammergeist_Infuse(EnHammergeist* this, PlayState* play);
void EnHammergeist_SetupHeavySlam(EnHammergeist* this, PlayState* play);
void EnHammergeist_HeavySlam(EnHammergeist* this, PlayState* play); // 3 Heart Damage
void EnHammergeist_SetupSlamL(EnHammergeist* this, PlayState* play);
void EnHammergeist_SlamL(EnHammergeist* this, PlayState* play);
void EnHammergeist_SetupSlamR(EnHammergeist* this, PlayState* play);
void EnHammergeist_SlamR(EnHammergeist* this, PlayState* play); // 1 Heart Damage (1 1/2 if infused)
void EnHammergeist_SetupFlex(EnHammergeist* this, PlayState* play);
void EnHammergeist_Flex(EnHammergeist* this, PlayState* play);

ActorInit En_Hammergeist_InitVars = {
    ACTOR_EN_HAMMERGEIST,  ACTORCAT_ENEMY,        FLAGS,
    OBJECT_HAMMERGEIST,    sizeof(EnHammergeist), EnHammergeist_Init,
    EnHammergeist_Destroy, EnHammergeist_Update,  EnHammergeist_Draw,
};

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_METAL,
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
    { 40, 90, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sHammerLeftCylinderInit = {
    {
        COLTYPE_HIT5,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { DMG_HAMMER, 0x00, 0x10 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 40, 80, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sHammerRightCylinderInit = {
    {
        COLTYPE_HIT5,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { DMG_HAMMER, 0x00, 0x10 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 40, 80, 0, { 0, 0, 0 } },
};

static ColliderJntSphElementInit sJntSphElementsInit[1] = {
    {
        {
            ELEMTYPE_UNK0,
            { 0x00000008, 0x00, 0x20 },
            { 0x00000000, 0x00, 0x00 },
            TOUCH_ON | TOUCH_SFX_NONE,
            BUMP_ON,
            OCELEM_NONE,
        },
        { 0, { { 0, 0, 900 }, 0 }, 100 },
    },
};

// For the hammer explosion
static ColliderJntSphInit sJntSphInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ALL,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_JNTSPH,
    },
    1,
    sJntSphElementsInit,
};

typedef enum {
    /* 0 */ HAMMERGEIST_ANIMATION_IDLE,
    /* 1 */ HAMMERGEIST_ANIMATION_WALK,
    /* 2 */ HAMMERGEIST_ANIMATION_DAMAGE,
    /* 3 */ HAMMERGEIST_ANIMATION_DIE,
    /* 4 */ HAMMERGEIST_ANIMATION_EXPLOSION,
    /* 5 */ HAMMERGEIST_ANIMATION_INFUSE,
    /* 6 */ HAMMERGEIST_ANIMATION_SLAM_HEAVY,
    /* 7 */ HAMMERGEIST_ANIMATION_SLAM_L,
    /* 8 */ HAMMERGEIST_ANIMATION_SLAM_R,
    /* 9 */ HAMMERGEIST_ANIMATION_FLEX,
} EnHammergeistAnimation;

static AnimationInfo sAnimationInfo[] = {
    { &gHammergeistSkelIdleAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, 3.0f },
    { &gHammergeistSkelWalkAnim, 2.0f, 0.0f, -1.0f, ANIMMODE_LOOP_PARTIAL, 3.0f },
    { &gHammergeistSkelDamageAnim, 3.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelDieAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelExplosionAnim, 2.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelInfuseAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelSlamheavyAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelSlamlAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelSlamrAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
    { &gHammergeistSkelFlexAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 3.0f },
};

typedef enum {
    /* 0 */ HAMMERGEIST_FACE_NORMAL,
    /* 1 */ HAMMERGEIST_FACE_LAUGH,
    /* 2 */ HAMMERGEIST_FACE_MOUTH_OPEN,
} EnHammergeistFace;

typedef enum {
    /* 0 */ HAMMERGEIST_FIRE_HAMMER_NORMAL,
    /* 1 */ HAMMERGEIST_FIRE_HAMMER_FIRE_1,
    /* 2 */ HAMMERGEIST_FIRE_HAMMER_FIRE_2,
} EnHammergeistFireHammer;

typedef enum {
    /* 0 */ HAMMERGEIST_ICE_HAMMER_NORMAL,
    /* 1 */ HAMMERGEIST_ICE_HAMMER_ICE_1,
    /* 2 */ HAMMERGEIST_ICE_HAMMER_ICE_2,
} EnHammerGeistIceHammer;

static void* sFaceTextures[] = {
    gHammergeistSkel_normal_ci8,
    gHammergeistSkel_laugh_ci8,
    gHammergeistSkel_mouth_open_ci8,
};

// Very small texture differences so that the hammer doesn't just look the same the whole time
static void* sFireHammerTextures[] = {
    gHammergeistSkel_metal2_rgba16,
    gHammergeistSkel_hammerfire_1_rgba16,
    gHammergeistSkel_hammerfire_2_rgba16,
};

// Very small texture differences so that the hammer doesn't just look the same the whole time
static void* sIceHammerTextures[] = {
    gHammergeistSkel_metal2_rgba16,
    gHammergeistSkel_hammerice_1_rgba16,
    gHammergeistSkel_hammerice_2_rgba16,
};

typedef enum {
    /*  0 */ ENHAMMERGEIST_DMGEFF_NONE,
    /*  1 */ ENHAMMERGEIST_DMGEFF_STUN,
    /*  6 */ ENHAMMERGEIST_DMGEFF_ICE_MAGIC = 6,
    /* 13 */ ENHAMMERGEIST_DMGEFF_LIGHT_MAGIC = 13,
    /* 14 */ ENHAMMERGEIST_DMGEFF_FIRE,
} EnHammergeistDamageEffect;

static DamageTable sDamageTable[] = {
    /* Deku nut      */ DMG_ENTRY(0, ENHAMMERGEIST_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, ENHAMMERGEIST_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, ENHAMMERGEIST_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(0, ENHAMMERGEIST_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(0, ENHAMMERGEIST_DMGEFF_STUN),
    /* Normal arrow  */ DMG_ENTRY(2, ENHAMMERGEIST_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, ENHAMMERGEIST_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, ENHAMMERGEIST_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, ENHAMMERGEIST_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, ENHAMMERGEIST_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, ENHAMMERGEIST_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(2, ENHAMMERGEIST_DMGEFF_NONE),
    /* Ice arrow     */ DMG_ENTRY(4, ENHAMMERGEIST_DMGEFF_NONE),
    /* Light arrow   */ DMG_ENTRY(2, ENHAMMERGEIST_DMGEFF_NONE),
    /* Unk arrow 1   */ DMG_ENTRY(2, ENHAMMERGEIST_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, ENHAMMERGEIST_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, ENHAMMERGEIST_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(0, ENHAMMERGEIST_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(3, ENHAMMERGEIST_DMGEFF_ICE_MAGIC),
    /* Light magic   */ DMG_ENTRY(4, ENHAMMERGEIST_DMGEFF_LIGHT_MAGIC),
    /* Shield        */ DMG_ENTRY(0, ENHAMMERGEIST_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, ENHAMMERGEIST_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(1, ENHAMMERGEIST_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(5, ENHAMMERGEIST_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(3, ENHAMMERGEIST_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, ENHAMMERGEIST_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(6, ENHAMMERGEIST_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, ENHAMMERGEIST_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, ENHAMMERGEIST_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, ENHAMMERGEIST_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(3, ENHAMMERGEIST_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, ENHAMMERGEIST_DMGEFF_NONE),
};

static CollisionCheckInfoInit2 sColChkInit = {
    .health = 16, .mass = MASS_HEAVY, .cylHeight = 55.0f, .cylRadius = 35.0f
};

void EnHammergeist_SetupAction(EnHammergeist* this, EnHammergeistActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void EnHammergeist_ChangeAnimation(EnHammergeist* this, s32 index) {
    Animation_ChangeByInfo(&this->skelAnime, sAnimationInfo, index);
}

void EnHammergeist_ChangeFace(EnHammergeist* this, s16 faceIndex) {
    this->faceIndex = faceIndex;
}

// Very small texture differences so that the hammers don't just look the same the whole time
void EnHammergeist_HammerAppearance(EnHammergeist* this, PlayState* play) {
    if (this->rightHammerInfused) {
        if (this->fireHammerIndex == HAMMERGEIST_FIRE_HAMMER_NORMAL) {
            this->fireHammerIndex = HAMMERGEIST_FIRE_HAMMER_FIRE_1;
        }
        if (play->gameplayFrames % 16 == 0) {
            this->fireHammerIndex = this->fireHammerIndex == HAMMERGEIST_FIRE_HAMMER_FIRE_1
                                        ? HAMMERGEIST_FIRE_HAMMER_FIRE_2
                                        : HAMMERGEIST_FIRE_HAMMER_FIRE_1;
        }
    } else {
        if (this->fireHammerIndex != HAMMERGEIST_FIRE_HAMMER_NORMAL) {
            this->fireHammerIndex = HAMMERGEIST_FIRE_HAMMER_NORMAL;
        }
    }

    if (this->leftHammerInfused) {
        if (this->iceHammerIndex == HAMMERGEIST_ICE_HAMMER_NORMAL) {
            this->iceHammerIndex = HAMMERGEIST_ICE_HAMMER_ICE_1;
        }
        if (play->gameplayFrames % 16 == 0) {
            this->iceHammerIndex = this->iceHammerIndex == HAMMERGEIST_ICE_HAMMER_ICE_1 ? HAMMERGEIST_ICE_HAMMER_ICE_2
                                                                                        : HAMMERGEIST_ICE_HAMMER_ICE_1;
        }
    } else {
        if (this->iceHammerIndex != HAMMERGEIST_ICE_HAMMER_NORMAL) {
            this->iceHammerIndex = HAMMERGEIST_ICE_HAMMER_NORMAL;
        }
    }
}

void EnHammergeist_InitAndSetCollision(EnHammergeist* this, PlayState* play) {
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);

    Collider_InitCylinder(play, &this->hammerLeftCollider);
    Collider_SetCylinder(play, &this->hammerLeftCollider, &this->actor, &sHammerLeftCylinderInit);

    Collider_InitCylinder(play, &this->hammerRightCollider);
    Collider_SetCylinder(play, &this->hammerRightCollider, &this->actor, &sHammerRightCylinderInit);

    Collider_InitJntSph(play, &this->explosionCollider);
    Collider_SetJntSph(play, &this->explosionCollider, &this->actor, &sJntSphInit, &this->explosionColliderItems[0]);

    CollisionCheck_SetInfo2(&this->actor.colChkInfo, sDamageTable, &sColChkInit);
}

void EnHammergeist_UpdateCollision(EnHammergeist* this, PlayState* play) {
    if (DECR(this->hurtboxCooldown) == 0 && this->actionFunc != EnHammergeist_Die) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
}

void EnHammergeist_UpdateHammerCollider(EnHammergeist* this, PlayState* play) {
    if (this->leftHammerInfused) {                        // More damage and ice effect
        this->hammerLeftCollider.info.toucher.effect = 2; // Ice
        this->hammerLeftCollider.info.toucher.dmgFlags = (DMG_HAMMER | DMG_MAGIC_ICE);
        this->hammerLeftCollider.info.toucher.damage = 0x18;
    } else {
        this->hammerLeftCollider.info.toucher.effect = 0;
        this->hammerLeftCollider.info.toucher.dmgFlags = DMG_HAMMER;
        this->hammerLeftCollider.info.toucher.damage = 0x10;
    }

    if (this->rightHammerInfused) {                        // More damage and fire effect
        this->hammerRightCollider.info.toucher.effect = 1; // Fire
        this->hammerRightCollider.info.toucher.dmgFlags = (DMG_HAMMER | DMG_MAGIC_FIRE);
        this->hammerRightCollider.info.toucher.damage = 0x18;
    } else {
        this->hammerRightCollider.info.toucher.effect = 0;
        this->hammerRightCollider.info.toucher.dmgFlags = DMG_HAMMER;
        this->hammerRightCollider.info.toucher.damage = 0x10;
    }

    // If the hammers explode with ice and fire together, the explosion causes more damage
    if (this->leftHammerInfused && this->rightHammerInfused) {
        this->explosionColliderItems[0].info.toucher.damage = 0x40; // 4 Heart Damage
    } else {
        this->explosionColliderItems[0].info.toucher.damage = 0x20; // 2 Heart Damage
    }
}

void EnHammergeist_DefuseLeftHammer(EnHammergeist* this, PlayState* play) {
    s32 i;

    this->leftHammerInfused = false;

    for (i = 0; i <= 7; i++) { // The pushing ice energy gets visualized by ice fragments
        EffectSsEnIce_SpawnFlyingVec3s(play, &this->actor, &this->hammerLeftCollider.dim.pos, 150, 150, 150, 250, 235,
                                       245, 255, 4);
    }
}

void EnHammergeist_DefuseRightHammer(EnHammergeist* this, PlayState* play) {
    s32 i;

    this->rightHammerInfused = false;

    for (i = 0; i <= 7; i++) { // The pushing fire energy gets visualized as a big flame
        EffectSsEnFire_SpawnVec3s(play, &this->actor, &this->hammerRightCollider.dim.pos, 400, 0, 0, -1);
    }
}

void EnHammergeist_Init(Actor* thisx, PlayState* play) {
    EnHammergeist* this = (EnHammergeist*)thisx;

    EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_IDLE);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 80.0f);
    Actor_SetScale(&this->actor, 0.015f);

    thisx->gravity = -1.0f;
    this->explosionTimer = 20;
    this->infuseTimer = 20;
    this->slamTimer = 20;
    this->heavySlamTimer = 60;
    this->leftHammerInfused = false;
    this->rightHammerInfused = false;
    this->playerHit = false;
    this->alpha = 255;

    EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_NORMAL);

    EnHammergeist_InitAndSetCollision(this, play);
    SkelAnime_InitFlex(play, &this->skelAnime, &gHammergeistSkel, NULL, this->jointTable, this->morphTable,
                       GHAMMERGEISTSKEL_NUM_LIMBS);
    EnHammergeist_SetupDoNothing(this, play);
}

void EnHammergeist_Destroy(Actor* thisx, PlayState* play) {
    EnHammergeist* this = (EnHammergeist*)thisx;

    SkelAnime_Free(&this->skelAnime, play);
    Collider_DestroyCylinder(play, &this->collider);
    Collider_DestroyCylinder(play, &this->hammerLeftCollider);
    Collider_DestroyCylinder(play, &this->hammerRightCollider);
    Collider_DestroyJntSph(play, &this->explosionCollider);
}

void EnHammergeist_Update(Actor* thisx, PlayState* play) {
    EnHammergeist* this = (EnHammergeist*)thisx;
    this->actionFunc(this, play);

    Actor_MoveXZGravity(thisx);
    EnHammergeist_UpdateBgCheck(this, play);

    Collider_UpdateCylinder(&this->actor, &this->collider);
    Collider_UpdateCylinder(&this->actor, &this->hammerLeftCollider);
    Collider_UpdateCylinder(&this->actor, &this->hammerRightCollider);

    EnHammergeist_UpdateCollision(this, play);
    EnHammergeist_UpdateHammerCollider(this, play);
    EnHammergeist_HammerAppearance(this, play);

    Actor_TrackPlayer(play, &this->actor, &this->headRot, &this->upperBodyRot, this->actor.focus.pos);
}

void EnHammergeist_Draw(Actor* thisx, PlayState* play) {
    EnHammergeist* this = (EnHammergeist*)thisx;

    Collider_UpdateSpheres(0, &this->explosionCollider);

    OPEN_DISPS(play->state.gfxCtx);

    if (this->alpha == 255) { // Alive
        gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sFireHammerTextures[this->fireHammerIndex]));
        gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sIceHammerTextures[this->iceHammerIndex]));
        gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(sFaceTextures[this->faceIndex]));

        func_80034BA0(play, &this->skelAnime, EnHammergeist_OverrideLimbDraw, EnHammergeist_PostLimbDraw, thisx, 255);
    } else {                    // Dead
        if (this->alpha != 0) { // Molmauk loses his transparency over time
            gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sFireHammerTextures[this->fireHammerIndex]));
            gSPSegment(POLY_XLU_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sIceHammerTextures[this->iceHammerIndex]));
            gSPSegment(POLY_XLU_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(sFaceTextures[this->faceIndex]));
            func_80034CC4(play, &this->skelAnime, NULL, EnHammergeist_DeadPostLimbDraw, thisx, this->alpha);
        }

        if (this->fireTimer != 0) { // Molmauk is burning down when dying
            thisx->colorFilterTimer++;
            this->fireTimer--;
            if (this->fireTimer % 4 == 0) {
                EffectSsEnFire_SpawnVec3s(play, thisx, &this->firePos[this->fireTimer >> 2], 250, 0, 0,
                                          (this->fireTimer >> 2));
            }
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

s32 EnHammergeist_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx,
                                   Gfx** gfx) {
    EnHammergeist* this = (EnHammergeist*)thisx;

    switch (limbIndex) {
        // Rotate head towards player
        case GHAMMERGEISTSKEL_HEAD_LIMB:
            if (this->actionFunc == EnHammergeist_ApproachPlayer) {
                rot->z += this->headRot.y;
                rot->x += this->headRot.x;
            }

            break;
    }

    return false;
}

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

void EnHammergeist_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx) {
    static Vec3f fireEffPos;
    static Vec3f iceEffPos;
    static Vec3f effVelocity = { 0.0f, 0.0f, 0.0f };
    static Vec3f effAccel = { 0.0f, 0.0f, 0.0f };
    static Color_RGBA8 fireAuraPrimColor = { 255, 255, 100, 255 };
    static Color_RGBA8 fireAuraEnvColor = { 255, 50, 0, 0 };
    static Color_RGBA8 iceAuraPrimColor = { 100, 200, 255, 255 };
    static Color_RGBA8 iceAuraEnvColor = { 0, 0, 255, 0 };
    EnHammergeist* this = (EnHammergeist*)thisx;
    MtxF mtx;

    Matrix_Get(&mtx); // This is for positioning the hammer effects and AT colliders

    switch (limbIndex) {
        case GHAMMERGEISTSKEL_HEAD_LIMB:
            Matrix_MultVec3f(&sZeroVec, &this->actor.focus.pos);
            break;

        // Positioning code for the ice effect on the left hammer
        case GHAMMERGEISTSKEL_HAMMERL_LIMB:
            this->hammerLeftCollider.dim.pos.x = mtx.xw;
            this->hammerLeftCollider.dim.pos.y = (mtx.yw - 40.0f);
            this->hammerLeftCollider.dim.pos.z = mtx.zw;

            iceEffPos.x = mtx.xw;
            iceEffPos.y = mtx.yw + 30.0f;
            iceEffPos.z = mtx.zw;
            break;

        // Positioning code for the fire effect on the right hammer
        case GHAMMERGEISTSKEL_HAMMERR_LIMB:
            this->hammerRightCollider.dim.pos.x = mtx.xw;
            this->hammerRightCollider.dim.pos.y = (mtx.yw - 40.0f);
            this->hammerRightCollider.dim.pos.z = mtx.zw;

            fireEffPos.x = mtx.xw;
            fireEffPos.y = mtx.yw + 30.0f;
            fireEffPos.z = mtx.zw;
            break;
    }

    // Fire effect
    if (this->rightHammerInfused) {
        func_8002843C(play, &fireEffPos, &effVelocity, &effAccel, &fireAuraPrimColor, &fireAuraEnvColor, 500, 50, 10);
    }

    // Ice effect
    if (this->leftHammerInfused) {
        func_8002843C(play, &iceEffPos, &effVelocity, &effAccel, &iceAuraPrimColor, &iceAuraEnvColor, 500, 50, 10);
    }
}

// Flames on all his body parts when dying
void EnHammergeist_DeadPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx) {
    EnHammergeist* this = (EnHammergeist*)thisx;
    s32 idx = -1;
    Vec3f modifiedVec = { 300.0f, 0.0f, 0.0f };
    Vec3f destPos;

    if (this->fireTimer != 0) {
        switch (limbIndex) {
            case GHAMMERGEISTSKEL_HEAD_LIMB:
                idx = 0;
                break;

            case GHAMMERGEISTSKEL_HAMMERL_LIMB:
                idx = 1;
                break;

            case GHAMMERGEISTSKEL_HAMMERR_LIMB:
                idx = 2;
                break;

            case GHAMMERGEISTSKEL_BODY_LIMB:
                idx = 3;
                break;

            case GHAMMERGEISTSKEL_HAND_L_LIMB:
                idx = 4;
                break;

            case GHAMMERGEISTSKEL_HAND_R_LIMB:
                idx = 5;
                break;

            case GHAMMERGEISTSKEL_FOOT_L_LIMB:
                idx = 6;
                break;

            case GHAMMERGEISTSKEL_FOOT_R_LIMB:
                idx = 7;
                break;

            case GHAMMERGEISTSKEL_ARM_L_LIMB:
                idx = 8;
                break;

            case GHAMMERGEISTSKEL_ARM_R_LIMB:
                idx = 9;
                break;
        }
    }

    if (idx >= 0) { // this is straight off copied ReDead code
        Matrix_MultVec3f(&modifiedVec, &destPos);
        this->firePos[idx].x = destPos.x;
        this->firePos[idx].y = destPos.y;
        this->firePos[idx].z = destPos.z;
    }
}

void EnHammergeist_UpdateBgCheck(EnHammergeist* this, PlayState* play) {
    Actor_UpdateBgCheckInfo(
        play, &this->actor, this->actor.colChkInfo.cylHeight, this->actor.colChkInfo.cylRadius,
        this->actor.colChkInfo.cylHeight,
        (UPDBGCHECKINFO_FLAG_0 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_3 | UPDBGCHECKINFO_FLAG_4));
}

// Move towards Link, stand still if right infront of him
void EnHammergeist_Movement(EnHammergeist* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    if (this->actor.xzDistToPlayer <= 75.0f) {
        if (this->skelAnime.animation != &gHammergeistSkelIdleAnim) {
            EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_IDLE);
        }
        Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 2000);
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 3000);
        this->actor.speed = 0.0f;
    } else {
        if (this->skelAnime.animation != &gHammergeistSkelWalkAnim) {
            EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_WALK);
        }
        if ((this->skelAnime.curFrame >= 10.0f && this->skelAnime.curFrame <= 20.0f) ||
            (this->skelAnime.curFrame >= 38.0f && this->skelAnime.curFrame <= 45.0f)) {
            this->actor.speed = 0.0f;
            if (this->skelAnime.curFrame == 10.0f || this->skelAnime.curFrame == 38.0f) {
                Actor_PlaySfx(&this->actor, NA_SE_EN_AMOS_WALK);
            }
        } else {
            Math_ApproachF(&this->actor.speed, 5.0f / 1.5f, 0.5f, 1.5f);
            Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 2000);
            Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 3000);
        }
    }
}

void EnHammergeist_CheckDamage(EnHammergeist* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        this->hurtboxCooldown = 10;
        this->actor.speed = 0.0f;

        if (this->actor.colChkInfo.damageEffect != ENHAMMERGEIST_DMGEFF_STUN) {
            EnHammergeist_SetupDamage(this, play);
        } else {
            // Stunning effect because of e.g. a deku nut
            Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
            Actor_ApplyDamage(&this->actor);
            EnHammergeist_SetupStunned(this, play);
        }

        if (this->actor.colChkInfo.health == 0) {
            EnHammergeist_SetupDie(this, play);
        }
    }
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_WATER) && this->actionFunc != EnHammergeist_Die) {
        // Currently, the Hammergeist dies if he falls into a water box
        EnHammergeist_SetupDie(this, play);
    }
}

void EnHammergeist_SetupDoNothing(EnHammergeist* this, PlayState* play) {
    this->actor.speed = 0.0f;
    EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_IDLE);
    EnHammergeist_SetupAction(this, EnHammergeist_DoNothing);
}

void EnHammergeist_DoNothing(EnHammergeist* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    // Player noticed, get active
    if (this->actor.xzDistToPlayer < 800.0f) {
        EnHammergeist_SetupApproachPlayer(this, play);
    }
}

void EnHammergeist_SetupApproachPlayer(EnHammergeist* this, PlayState* play) {
    EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_WALK);
    EnHammergeist_SetupAction(this, EnHammergeist_ApproachPlayer);
}

void EnHammergeist_ApproachPlayer(EnHammergeist* this, PlayState* play) {
    EnHammergeist_Movement(this, play);

    if (this->actor.xzDistToPlayer > 1500.0f) {
        EnHammergeist_SetupDoNothing(this, play);
    }

    if (this->actor.xzDistToPlayer < 120.0f) {
        if (DECR(this->slamTimer) == 0) {
            this->slamTimer = 30;
            if (Rand_ZeroOne() < 0.6f) {
                if (play->gameplayFrames % 2 == 0) {
                    // Either hit with the left hammer
                    EnHammergeist_SetupSlamL(this, play);
                } else {
                    // Or the right hammer
                    EnHammergeist_SetupSlamR(this, play);
                }
            }
        }
    }

    if (!this->leftHammerInfused && !this->rightHammerInfused) {
        if (DECR(this->infuseTimer) == 0) {
            this->infuseTimer = 40;
            if (Rand_ZeroOne() < 0.2f) {
                // 10% chance
                EnHammergeist_SetupInfuse(this, play);
            }
        }
    }

    if (this->actor.xzDistToPlayer < 170.0f && this->actor.xzDistToPlayer > 60.0f) {
        if (DECR(this->explosionTimer) == 0) {
            this->explosionTimer = 20;
            if (Rand_ZeroOne() < 0.3f) {
                // 20% chance
                EnHammergeist_SetupExplosion(this, play);
            }
        }
    }
    if (DECR(this->heavySlamCooldown) == 0) {
        if (DECR(this->heavySlamTimer) == 0) {
            this->heavySlamTimer = 60;
            if (Rand_ZeroOne() < 0.2f) {
                // 10% chance
                EnHammergeist_SetupHeavySlam(this, play);
            }
        }
    }
}

void EnHammergeist_SetupDamage(EnHammergeist* this, PlayState* play) {
    static f32 sDamagePitch = 0.25f;
    this->genericAnimationTimer = 5;
    EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_NORMAL);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
    Actor_ApplyDamage(&this->actor);
    Audio_PlaySfxGeneral(NA_SE_EN_STALKID_DAMAGE, &this->actor.world.pos, 4, &sDamagePitch, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
    EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_DAMAGE);
    EnHammergeist_SetupAction(this, EnHammergeist_Damage);
}

void EnHammergeist_Damage(EnHammergeist* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        if (DECR(this->genericAnimationTimer) == 0) {
            // Molmauk might take revenge for getting hit
            if (Rand_ZeroOne() < 0.4f && this->noHitAgain == false) {
                // 30% chance
                this->noHitAgain = true;
                if (play->gameplayFrames % 2 == 0) {
                    // either left slam
                    EnHammergeist_SetupSlamL(this, play);
                } else {
                    // or right slam
                    EnHammergeist_SetupSlamR(this, play);
                }
            } else {
                this->noHitAgain = false;
                EnHammergeist_SetupDoNothing(this, play);
            }
        }
    }
}

void EnHammergeist_SetupStunned(EnHammergeist* this, PlayState* play) {
    this->actor.speed = 0.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Animation_PlayOnceSetSpeed(&this->skelAnime, &gHammergeistSkelIdleAnim, 0.0f);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
    EnHammergeist_SetupAction(this, EnHammergeist_Stunned);
}

void EnHammergeist_Stunned(EnHammergeist* this, PlayState* play) {
    EnHammergeist_CheckDamage(this, play);
    if (this->actor.colorFilterTimer == 0) {
        EnHammergeist_SetupDoNothing(this, play);
    }
}

void EnHammergeist_SetupDie(EnHammergeist* this, PlayState* play) {
    EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_NORMAL);
    this->actor.shape.shadowDraw = NULL;
    if (this->leftHammerInfused) {
        EnHammergeist_DefuseLeftHammer(this, play);
    }
    if (this->rightHammerInfused) {
        EnHammergeist_DefuseRightHammer(this, play);
    }
    this->actor.speed = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_0; // Molmauk not targetable anymore
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 80);
    this->fireTimer = 40;
    Enemy_StartFinishingBlow(play, &this->actor);
    Actor_PlaySfx(&this->actor, NA_SE_EN_ANUBIS_FIRE);
    EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_DIE);
    EnHammergeist_SetupAction(this, EnHammergeist_Die);
}

void EnHammergeist_Die(EnHammergeist* this, PlayState* play) {
    // Molmauk loses his transparency when dying
    if (this->alpha != 0) {
        if (play->gameplayFrames % 2 == 0) {
            this->alpha -= 5;
        }
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        if (DECR(this->fireTimer) == 0 && this->actor.colorFilterTimer == 0) {
            Actor_Kill(&this->actor);
        }
    }
}

void EnHammergeist_SetupExplosion(EnHammergeist* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->genericAnimationTimer = 33;
    this->explosionRadiusIncrease = false;
    this->playerHit = false;
    EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_EXPLOSION);
    EnHammergeist_SetupAction(this, EnHammergeist_Explosion);
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

void EnHammergeist_Explosion(EnHammergeist* this, PlayState* play) {
    Vec3f effPos = this->actor.world.pos;
    Vec3f effVel = { 0.0f, 0.0f, 0.0f };
    Vec3f effAcc = { 0.0f, 0.0f, 0.0f };

    if (SkelAnime_Update(&this->skelAnime)) {
        if (DECR(this->genericAnimationTimer) == 0) {
            if (this->playerHit == true) {
                // Player got hit, emote on him
                this->playerHit = false;
                EnHammergeist_SetupFlex(this, play);
            } else {
                EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_NORMAL);
                EnHammergeist_SetupDoNothing(this, play);
            }
        }
    }

    if (this->explosionCollider.base.atFlags & AT_HIT) {
        this->explosionCollider.base.atFlags &= ~AT_HIT;
        this->playerHit = true;
        Actor_SetPlayerKnockbackNoDamage(play, &this->actor, 10.0f, this->actor.shape.rot.y, 5.0f);
        Player_PlaySfx(GET_PLAYER(play), NA_SE_PL_BODY_HIT);
    }

    if (this->explosionRadiusIncrease == true) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->explosionCollider.base);
        this->explosionCollider.elements[0].dim.modelSphere.radius += 15;
        this->explosionCollider.elements[0].dim.worldSphere.radius =
            this->explosionCollider.elements[0].dim.modelSphere.radius;
        if (this->explosionCollider.elements[0].dim.worldSphere.radius >= 150) {
            this->explosionCollider.elements[0].dim.modelSphere.radius = 0;
            this->explosionCollider.elements[0].dim.worldSphere.radius = 0;
            this->explosionRadiusIncrease = false;
        }
    }

    if (this->skelAnime.curFrame == 30.0f) {
        this->explosionRadiusIncrease = true;
    }

    if (this->skelAnime.curFrame == 40.0f) {
        this->explosionRadiusIncrease = true;
        if (this->leftHammerInfused) {
            EnHammergeist_DefuseLeftHammer(this, play);
        }
        if (this->rightHammerInfused) {
            EnHammergeist_DefuseRightHammer(this, play);
        }
        EffectSsBomb2_SpawnLayered(play, &effPos, &effVel, &effAcc, 100, 30);
        Actor_PlaySfx(&this->actor, NA_SE_IT_BOMB_EXPLOSION);
        Camera_RequestQuake(&play->mainCamera, 2, 11, 8);
    }

    // Molmauk is attackable
    if (this->skelAnime.curFrame >= 41.0f) {
        EnHammergeist_CheckDamage(this, play);
    }
}

void EnHammergeist_SetupInfuse(EnHammergeist* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->genericAnimationTimer = 10;
    EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_INFUSE);
    EnHammergeist_SetupAction(this, EnHammergeist_Infuse);
}

void EnHammergeist_Infuse(EnHammergeist* this, PlayState* play) {
    s32 i;
    Vec3s newIcePos = this->hammerLeftCollider.dim.pos;
    newIcePos.y += 70; // ice fragments needed a better offset

    if (this->skelAnime.curFrame == 9.0f) {
        EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_MOUTH_OPEN);
    }

    if (this->skelAnime.curFrame == 15.0f) {
        this->rightHammerInfused = true;
        for (i = 0; i <= 7; i++) { // Big flame appears
            EffectSsEnFire_SpawnVec3s(play, &this->actor, &this->hammerRightCollider.dim.pos, 400, 0, 0, -1);
        }
        EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_NORMAL);
    }

    if (this->skelAnime.curFrame == 31.0f) {
        EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_MOUTH_OPEN);
    }

    if (this->skelAnime.curFrame == 37.0f) {
        this->leftHammerInfused = true;
        for (i = 0; i <= 7; i++) { // Ice fragments appear
            EffectSsEnIce_SpawnFlyingVec3s(play, &this->actor, &newIcePos, 150, 150, 150, 250, 235, 245, 255, 4);
        }
        EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_LAUGH);
    }

    if (SkelAnime_Update(&this->skelAnime)) {
        EnHammergeist_SetupDoNothing(this, play);
    }
}

void EnHammergeist_SetupHeavySlam(EnHammergeist* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->genericAnimationTimer = 10;
    EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_SLAM_HEAVY);
    EnHammergeist_SetupAction(this, EnHammergeist_HeavySlam);
}

void EnHammergeist_HeavySlam(EnHammergeist* this, PlayState* play) {
    f32 freqVolScale = 50.0f;

    if (SkelAnime_Update(&this->skelAnime)) {
        this->heavySlamCooldown = 600; // Heavy slam cooldown
        if (DECR(this->genericAnimationTimer) == 0) {
            EnHammergeist_SetupFlex(this, play);
        }
    }

    // Frame window right before the hit where the heavy slam can be prevented
    if (this->skelAnime.curFrame >= 38.0f && this->skelAnime.curFrame <= 45.0f) {
        EnHammergeist_CheckDamage(this, play);
    }

    // sound effect: NA_SE_EN_MONBLIN_HAM_LAND
    if (this->skelAnime.curFrame == 50.0f) {
        if (this->leftHammerInfused) {
            EnHammergeist_DefuseLeftHammer(this, play);
        }
        if (this->rightHammerInfused) {
            EnHammergeist_DefuseRightHammer(this, play);
        }

        Audio_PlaySfxGeneral(NA_SE_EV_WALL_BROKEN, &GET_PLAYER(play)->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        s32 i;
        for (i = 0; i < 10; i++) { // it just needed to be more powerful!
            Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, i * 100.0f, 4, 4.0f, i * 500, i * 110,
                                     true);
        }
        if (this->actor.xzDistToPlayer < 800.0f) { // The energy caused by the ground hit makes Link fly away
            Actor_SetPlayerKnockbackDamage(play, &this->actor, 20.0f, GET_PLAYER(play)->actor.world.rot.y + 0x8000,
                                           10.0f, 0x30);
        }
    }
}

void EnHammergeist_SetupSlamL(EnHammergeist* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.shape.rot.y = this->actor.world.rot.y;
    EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_SLAM_L);
    EnHammergeist_SetupAction(this, EnHammergeist_SlamL);
}

void EnHammergeist_SlamL(EnHammergeist* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (SkelAnime_Update(&this->skelAnime)) {
        EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_NORMAL);
        EnHammergeist_SetupDoNothing(this, play);
    }

    if (this->skelAnime.curFrame >= 20.0f) {
        EnHammergeist_CheckDamage(this, play);
    }

    // Sound of the hammer when hitting the floor
    if (this->skelAnime.curFrame == 15.0f) {
        Actor_PlaySfx(&this->actor, NA_SE_IT_HAMMER_HIT);
    }

    if (this->leftHammerInfused && this->skelAnime.curFrame == 17.0f) {
        EnHammergeist_DefuseLeftHammer(this, play);
    }

    if (this->hammerLeftCollider.base.atFlags & AT_HIT) {
        this->hammerLeftCollider.base.atFlags &= ~AT_HIT;
        if (this->leftHammerInfused) {
            EnHammergeist_DefuseLeftHammer(this, play);
        } else {
            Actor_SetPlayerKnockbackNoDamage(play, &this->actor, 0.0f, this->actor.shape.rot.y, 0.0f);
        }
        Player_PlaySfx(GET_PLAYER(play), NA_SE_PL_BODY_HIT);
    }

    // The frame window where the left hammer causes damage
    if (this->skelAnime.curFrame >= 10.0f && this->skelAnime.curFrame <= 18.0f) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->hammerLeftCollider.base);
    }
}

void EnHammergeist_SetupSlamR(EnHammergeist* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.shape.rot.y = this->actor.world.rot.y;
    EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_SLAM_R);
    EnHammergeist_SetupAction(this, EnHammergeist_SlamR);
}

void EnHammergeist_SlamR(EnHammergeist* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 i;

    if (SkelAnime_Update(&this->skelAnime)) {
        EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_NORMAL);
        EnHammergeist_SetupDoNothing(this, play);
    }

    if (this->skelAnime.curFrame >= 20.0f) {
        EnHammergeist_CheckDamage(this, play);
    }

    // Sound of the hammer when hitting the floor
    if (this->skelAnime.curFrame == 15.0f) {
        Actor_PlaySfx(&this->actor, NA_SE_IT_HAMMER_HIT);
    }

    if (this->rightHammerInfused && this->skelAnime.curFrame == 17.0f) {
        EnHammergeist_DefuseRightHammer(this, play);
    }

    if (this->hammerRightCollider.base.atFlags & AT_HIT) {
        this->hammerRightCollider.base.atFlags &= ~AT_HIT;
        if (this->rightHammerInfused) {
            EnHammergeist_DefuseRightHammer(this, play);
            Actor_SetPlayerKnockbackNoDamage(play, &this->actor, 0.0f, this->actor.shape.rot.y, 0.0f);
            if (player->isBurning == false) {
                for (i = 0; i < PLAYER_BODYPART_MAX; i++) {
                    player->flameTimers[i] = Rand_S16Offset(0, 200);
                }
                player->isBurning = true;
            }
        } else {
            Actor_SetPlayerKnockbackNoDamage(play, &this->actor, 0.0f, this->actor.shape.rot.y, 0.0f);
        }
        Player_PlaySfx(GET_PLAYER(play), NA_SE_PL_BODY_HIT);
    }

    // The frame window where the right hammer causes damage
    if (this->skelAnime.curFrame >= 10.0f && this->skelAnime.curFrame <= 18.0f) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->hammerRightCollider.base);
    }
}

void EnHammergeist_SetupFlex(EnHammergeist* this, PlayState* play) {
    static f32 sFlexPitch = 0.7f;
    this->actor.speed = 0.0f;
    this->genericAnimationTimer = 10;
    EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_LAUGH);
    // Flexing voice
    Audio_PlaySfxGeneral(NA_SE_EN_FANTOM_VOICE, &this->actor.world.pos, 4, &sFlexPitch, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
    EnHammergeist_ChangeAnimation(this, HAMMERGEIST_ANIMATION_FLEX);
    EnHammergeist_SetupAction(this, EnHammergeist_Flex);
}

// Molmauk is distracted when flexing and can be attacked
void EnHammergeist_Flex(EnHammergeist* this, PlayState* play) {
    EnHammergeist_CheckDamage(this, play);
    if (SkelAnime_Update(&this->skelAnime)) {
        if (DECR(this->genericAnimationTimer) == 0) {
            EnHammergeist_ChangeFace(this, HAMMERGEIST_FACE_NORMAL);
            EnHammergeist_SetupDoNothing(this, play);
        }
    }
}