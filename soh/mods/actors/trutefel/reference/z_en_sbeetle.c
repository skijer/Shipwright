/*
 * File: z_en_sbeetle.c
 * Overlay: Ovl_En_Sbeetle
 * Description: Scissors Beetle comparable to the Scissors Beetles from The Minish Cap
 * Authors: @syeo501 (Model) @trueffel (Code)
 * Note: This enemy code was mostly written by @trueffel but contains some AI code mostly for mathematical operations
 *       related to the pincer attack.
 */

#include "z_en_sbeetle.h"

// z-targetable, unfriendly actor, update outside uncull zone, hookshottable, navi dialogue
#define FLAGS (ACTOR_FLAG_0 | ACTOR_FLAG_2 | ACTOR_FLAG_4 | ACTOR_FLAG_9 | ACTOR_FLAG_18)

void EnSbeetle_Init(Actor* thisx, PlayState* play);
void EnSbeetle_Destroy(Actor* thisx, PlayState* play);
void EnSbeetle_Update(Actor* thisx, PlayState* play);
void EnSbeetle_Draw(Actor* thisx, PlayState* play);

void EnSbeetle_WorldToCurrentMatrixLocal(Vec3f* worldPos, Vec3f* localPos);
void EnSbeetle_GetPincerPath(Vec3f* start, Vec3f* end, f32 progress, f32 side, Vec3f* result);
void EnSbeetle_StartPincerReturn(EnSbeetle* this);
void EnSbeetle_StartPincerFastReturn(EnSbeetle* this);
void EnSbeetle_UpdatePincers(EnSbeetle* this, PlayState* play);

s32 EnSbeetle_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx);
void EnSbeetle_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx);

void EnSbeetle_CheckHurt(EnSbeetle* this, PlayState* play);
void EnSbeetle_UpdateBgCheck(EnSbeetle* this, PlayState* play);
s32 EnSbeetle_HasLostPlayer(EnSbeetle* this, PlayState* play);
s32 EnSbeetle_CheckPlayerNear(EnSbeetle* this, PlayState* play);
void EnSbeetle_SetupDoNothing(EnSbeetle* this, PlayState* play);
void EnSbeetle_DoNothing(EnSbeetle* this, PlayState* play);
void EnSbeetle_IdleActionWalk(EnSbeetle* this, PlayState* play);
void EnSbeetle_IdleActionIdle2(EnSbeetle* this, PlayState* play);
void EnSbeetle_IdleActionIdle3(EnSbeetle* this, PlayState* play);
void EnSbeetle_SetupHopWithPlayerRot(EnSbeetle* this, PlayState* play);
void EnSbeetle_HopWithPlayerRot(EnSbeetle* this, PlayState* play);
void EnSbeetle_SetupThreatPlayer(EnSbeetle* this, PlayState* play);
void EnSbeetle_ThreatPlayer(EnSbeetle* this, PlayState* play);
void EnSbeetle_SetupAttack(EnSbeetle* this, PlayState* play);
void EnSbeetle_Attack(EnSbeetle* this, PlayState* play);
void EnSbeetle_SetupSwingAttack(EnSbeetle* this, PlayState* play);
void EnSbeetle_SwingAttack(EnSbeetle* this, PlayState* play);
void EnSbeetle_SetupHopAwayFromOrTowardsPlayer(EnSbeetle* this, PlayState* play);
void EnSbeetle_HopAwayFromOrTowardsPlayer(EnSbeetle* this, PlayState* play);
void EnSbeetle_SetupStunned(EnSbeetle* this, PlayState* play);
void EnSbeetle_Stunned(EnSbeetle* this, PlayState* play);
void EnSbeetle_SetupHurt(EnSbeetle* this, PlayState* play);
void EnSbeetle_Hurt(EnSbeetle* this, PlayState* play);
void EnSbeetle_SetupDie(EnSbeetle* this, PlayState* play);
void EnSbeetle_Die(EnSbeetle* this, PlayState* play);

#define ENSBEETLE_PINCER_THROW_FRAME 12.0f
#define ENSBEETLE_PINCER_OUT_TIME 18
#define ENSBEETLE_PINCER_RETURN_TIME 20
#define ENSBEETLE_PINCER_CURVE 55.0f
#define ENSBEETLE_PINCER_ARC_HEIGHT 25.0f
#define ENSBEETLE_PINCER_SPIN_SPEED 0x2800
#define ENSBEETLE_PINCER_FAST_RETURN_TIME 6

ActorInit En_Sbeetle_InitVars = {
    ACTOR_EN_SBEETLE,  ACTORCAT_ENEMY,   FLAGS,          OBJECT_SBEETLE, sizeof(EnSbeetle), EnSbeetle_Init,
    EnSbeetle_Destroy, EnSbeetle_Update, EnSbeetle_Draw,
};

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_HARD,
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
    { 40, 45, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sPincerLCylinderInit = { {
                                                         COLTYPE_NONE,
                                                         AT_ON | AT_TYPE_ENEMY,
                                                         AC_NONE,
                                                         OC1_NONE,
                                                         OC2_NONE,
                                                         COLSHAPE_CYLINDER,
                                                     },
                                                     {
                                                         ELEMTYPE_UNK0,
                                                         { DMG_SLASH, 0x00, 0x8 },
                                                         { 0x00000000, 0x00, 0x00 },
                                                         TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
                                                         BUMP_NONE,
                                                         OCELEM_NONE,
                                                     },
                                                     { 35, 30, 0, { 0, 0, 0 } } };

static ColliderCylinderInit sPincerRCylinderInit = { {
                                                         COLTYPE_NONE,
                                                         AT_ON | AT_TYPE_ENEMY,
                                                         AC_NONE,
                                                         OC1_NONE,
                                                         OC2_NONE,
                                                         COLSHAPE_CYLINDER,
                                                     },
                                                     {
                                                         ELEMTYPE_UNK0,
                                                         { 0x20000000, 0x00, 0x8 },
                                                         { 0x00000000, 0x00, 0x00 },
                                                         TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
                                                         BUMP_NONE,
                                                         OCELEM_NONE,
                                                     },
                                                     { 35, 30, 0, { 0, 0, 0 } } };

typedef enum {
    /*  0 */ ENSBEETLE_DMGEFF_NONE,
    /*  1 */ ENSBEETLE_DMGEFF_STUN,
    /*  6 */ ENSBEETLE_DMGEFF_ICE_MAGIC = 6,
    /* 13 */ ENSBEETLE_DMGEFF_LIGHT_MAGIC = 13,
    /* 14 */ ENSBEETLE_DMGEFF_FIRE,
} EnSbeetleDamageEffect;

static DamageTable sDamageTable[] = {
    /* Deku nut      */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, ENSBEETLE_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_STUN),
    /* Normal arrow  */ DMG_ENTRY(1, ENSBEETLE_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, ENSBEETLE_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, ENSBEETLE_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(3, ENSBEETLE_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_ICE_MAGIC),
    /* Light arrow   */ DMG_ENTRY(4, ENSBEETLE_DMGEFF_NONE),
    /* Unk arrow 1   */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(3, ENSBEETLE_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_ICE_MAGIC),
    /* Light magic   */ DMG_ENTRY(4, ENSBEETLE_DMGEFF_LIGHT_MAGIC),
    /* Shield        */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(4, ENSBEETLE_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(3, ENSBEETLE_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, ENSBEETLE_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, ENSBEETLE_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, ENSBEETLE_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
};

static CollisionCheckInfoInit2 sColChkInit = {
    .health = 5, .mass = MASS_HEAVY, .cylHeight = 35.0f, .cylRadius = 25.0f
};

typedef enum {
    /* 0 */ SCISSORSBEETLE_ANIMATION_IDLE1,
    /* 1 */ SCISSORSBEETLE_ANIMATION_IDLE2,
    /* 2 */ SCISSORSBEETLE_ANIMATION_IDLE3,
    /* 3 */ SCISSORSBEETLE_ANIMATION_WALK,
    /* 4 */ SCISSORSBEETLE_ANIMATION_HOP,
    /* 5 */ SCISSORSBEETLE_ANIMATION_ATTACK,
    /* 6 */ SCISSORSBEETLE_ANIMATION_SWING,
    /* 7 */ SCISSORSBEETLE_ANIMATION_HURT,
    /* 8 */ SCISSORSBEETLE_ANIMATION_DIE,
} EnSbeetleAnimation;

static AnimationInfo sAnimationInfo[] = {
    { &gScissorsBeetleSkelIdle1Anim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, 3.0f },
    { &gScissorsBeetleSkelIdle2Anim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelIdle3Anim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelWalkAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, 3.0f },
    { &gScissorsBeetleSkelHopAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelAttackAnim, 1.5f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelSwingAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelHurtAnim, 1.5f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelDieAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
};

void EnSbeetle_ChangeAnimation(EnSbeetle* this, s32 index) {
    Animation_ChangeByInfo(&this->skelAnime, sAnimationInfo, index);
}

/*
 * Prepares pincer colliders and body collider
 */
void EnSbeetle_InitAndSetCollision(EnSbeetle* this, PlayState* play) {
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, sDamageTable, &sColChkInit);

    Collider_InitCylinder(play, &this->pincerLCollider);
    Collider_SetCylinder(play, &this->pincerLCollider, &this->actor, &sPincerLCylinderInit);

    Collider_InitCylinder(play, &this->pincerRCollider);
    Collider_SetCylinder(play, &this->pincerRCollider, &this->actor, &sPincerRCylinderInit);
}

/*  --- This function was written by AI ---
 *
 */
void EnSbeetle_InitPincers(EnSbeetle* this, PlayState* play) {
    this->pincerState = ENSBEETLE_PINCER_ATTACHED;
    this->pincerFlightTimer = 0;
    this->pincerLSpin = 0;
    this->pincerRSpin = 0;

    this->pincerLWorldPos = this->actor.world.pos;
    this->pincerRWorldPos = this->actor.world.pos;

    this->pincerLHomePos = this->actor.world.pos;
    this->pincerRHomePos = this->actor.world.pos;

    this->pincerLReturnStart = this->actor.world.pos;
    this->pincerRReturnStart = this->actor.world.pos;

    this->pincerTargetPos = this->actor.world.pos;
}

void EnSbeetle_Init(Actor* thisx, PlayState* play) {
    EnSbeetle* this = (EnSbeetle*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 10.0f);
    Actor_SetScale(&this->actor, 0.1f);
    this->actor.naviEnemyId = NAVI_ENEMY_SCISSORS_BEETLE;
    thisx->gravity = -1.0f;
    this->nextIdleTimer = 0;
    this->attackTimer = 0;
    EnSbeetle_InitAndSetCollision(this, play);
    SkelAnime_InitFlex(play, &this->skelAnime, &gScissorsBeetleSkel, &gScissorsBeetleSkelIdle1Anim, this->jointTable,
                       this->morphTable, GSCISSORSBEETLESKEL_NUM_LIMBS);
    EnSbeetle_SetupDoNothing(this, play);
}

void EnSbeetle_Destroy(Actor* thisx, PlayState* play) {
    EnSbeetle* this = (EnSbeetle*)thisx;

    SkelAnime_Free(&this->skelAnime, play);
    Collider_DestroyCylinder(play, &this->collider);
    Collider_DestroyCylinder(play, &this->pincerLCollider);
    Collider_DestroyCylinder(play, &this->pincerRCollider);
}

void EnSbeetle_Update(Actor* thisx, PlayState* play) {
    EnSbeetle* this = (EnSbeetle*)thisx;

    EnSbeetle_CheckHurt(this, play);
    this->actionFunc(this, play);

    EnSbeetle_UpdatePincers(this, play);

    Actor_MoveXZGravity(thisx);
    EnSbeetle_UpdateBgCheck(this, play);

    Collider_UpdateCylinder(thisx, &this->collider);

    if ((this->pincerState == ENSBEETLE_PINCER_OUTBOUND) || (this->pincerState == ENSBEETLE_PINCER_RETURN) ||
        (this->pincerState == ENSBEETLE_PINCER_FAST_RETURN)) { // Update pincer collider positions

        Collider_UpdateCylinder(&this->actor, &this->pincerLCollider);

        Collider_UpdateCylinder(&this->actor, &this->pincerRCollider);

        this->pincerLCollider.dim.pos.x = this->pincerLWorldPos.x;
        this->pincerLCollider.dim.pos.y = this->pincerLWorldPos.y;
        this->pincerLCollider.dim.pos.z = this->pincerLWorldPos.z;

        this->pincerRCollider.dim.pos.x = this->pincerRWorldPos.x;
        this->pincerRCollider.dim.pos.y = this->pincerRWorldPos.y;
        this->pincerRCollider.dim.pos.z = this->pincerRWorldPos.z;

        if (this->pincerState != ENSBEETLE_PINCER_FAST_RETURN) {
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->pincerLCollider.base);
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->pincerRCollider.base);
        }
    }

    if (this->actionFunc != EnSbeetle_Die) {    // Enemy can't take more damage after death
        if (DECR(this->hurtboxCooldown) == 0) { // Player is not able to spam the sword
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }

        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
}

// Relative positions to spawn ice chunks when tektite is frozen
static Vec3f sIceChunks[12] = {
    { 20.0f, 20.0f, 0.0f },   { 10.0f, 40.0f, 10.0f },   { -10.0f, 40.0f, 10.0f }, { -20.0f, 20.0f, 0.0f },
    { 10.0f, 40.0f, -10.0f }, { -10.0f, 40.0f, -10.0f }, { 0.0f, 20.0f, -20.0f },  { 10.0f, 0.0f, 10.0f },
    { 10.0f, 0.0f, -10.0f },  { 0.0f, 20.0f, 20.0f },    { -10.0f, 0.0f, 10.0f },  { -10.0f, 0.0f, -10.0f },
};

static Vec3f sFlames[12] = {
    { 20.0f, 20.0f, 0.0f },   { 10.0f, 40.0f, 10.0f },   { -10.0f, 40.0f, 10.0f }, { -20.0f, 20.0f, 0.0f },
    { 10.0f, 40.0f, -10.0f }, { -10.0f, 40.0f, -10.0f }, { 0.0f, 20.0f, -20.0f },  { 10.0f, 0.0f, 10.0f },
    { 10.0f, 0.0f, -10.0f },  { 0.0f, 20.0f, 20.0f },    { -10.0f, 0.0f, 10.0f },  { -10.0f, 0.0f, -10.0f },
};

void EnSbeetle_Draw(Actor* thisx, PlayState* play) {
    EnSbeetle* this = (EnSbeetle*)thisx;

    if (this->spawnIceTimer != 0) {
        // Spawn chunks of ice all over the Goomba's body
        thisx->colorFilterTimer++;
        this->spawnIceTimer--;
        if ((this->spawnIceTimer & 3) == 0) {
            Vec3f iceChunk;
            s32 idx = this->spawnIceTimer >> 2;

            iceChunk.x = thisx->world.pos.x + sIceChunks[idx].x;
            iceChunk.y = thisx->world.pos.y + sIceChunks[idx].y;
            iceChunk.z = thisx->world.pos.z + sIceChunks[idx].z;
            EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &iceChunk, 150, 150, 150, 250, 235, 245, 255, 2.0f);
        }
    }

    if (this->fireTimer != 0) {
        thisx->colorFilterTimer++;
        this->fireTimer--;
        if ((this->fireTimer & 3) == 0) {
            Vec3f firePos = this->actor.world.pos;
            Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
            Vec3f effectVel = { 0.0f, 4.0f, 0.0f };

            s32 idx = this->fireTimer >> 2;

            firePos.x = thisx->world.pos.x + sFlames[idx].x;
            firePos.y = thisx->world.pos.y + sFlames[idx].y;
            firePos.z = thisx->world.pos.z + sFlames[idx].z;

            EffectSsDeadDb_Spawn(play, &firePos, &effectVel, &zeroVec, 90, 0, 200, 135, 50, 255, 200, 80, 50, 1, 9,
                                 true);
        }
    }

    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->jointTable, this->skelAnime.dListCount,
                          EnSbeetle_OverrideLimbDraw, EnSbeetle_PostLimbDraw, this);
}

/*  --- This function was written by AI ---
 *  Converts a world-space position into the local coordinate space of the currently active matrix
 * This is needed for bones as their position coordinates are bound to the actor
 */
void EnSbeetle_WorldToCurrentMatrixLocal(Vec3f* worldPos, Vec3f* localPos) {
    MtxF mtx;
    Vec3f delta;
    f32 scaleSqX;
    f32 scaleSqY;
    f32 scaleSqZ;

    Matrix_Get(&mtx);

    delta.x = worldPos->x - mtx.xw;
    delta.y = worldPos->y - mtx.yw;
    delta.z = worldPos->z - mtx.zw;

    scaleSqX = SQ(mtx.xx) + SQ(mtx.yx) + SQ(mtx.zx);
    scaleSqY = SQ(mtx.xy) + SQ(mtx.yy) + SQ(mtx.zy);
    scaleSqZ = SQ(mtx.xz) + SQ(mtx.yz) + SQ(mtx.zz);

    if (scaleSqX > 0.000001f) {
        localPos->x = ((delta.x * mtx.xx) + (delta.y * mtx.yx) + (delta.z * mtx.zx)) / scaleSqX;
    } else {
        localPos->x = 0.0f;
    }

    if (scaleSqY > 0.000001f) {
        localPos->y = ((delta.x * mtx.xy) + (delta.y * mtx.yy) + (delta.z * mtx.zy)) / scaleSqY;
    } else {
        localPos->y = 0.0f;
    }

    if (scaleSqZ > 0.000001f) {
        localPos->z = ((delta.x * mtx.xz) + (delta.y * mtx.yz) + (delta.z * mtx.zz)) / scaleSqZ;
    } else {
        localPos->z = 0.0f;
    }
}

/*  --- This function was written by AI ---
 *  Calculates a curved boomerang-like flight path between a start and end position
 */
void EnSbeetle_GetPincerPath(Vec3f* start, Vec3f* end, f32 progress, f32 side, Vec3f* result) {
    f32 dx;
    f32 dz;
    f32 length;
    f32 perpendicularX;
    f32 perpendicularZ;
    f32 curve;
    s16 curveAngle;

    result->x = start->x + ((end->x - start->x) * progress);
    result->y = start->y + ((end->y - start->y) * progress);
    result->z = start->z + ((end->z - start->z) * progress);

    dx = end->x - start->x;
    dz = end->z - start->z;
    length = sqrtf(SQ(dx) + SQ(dz));

    if (length > 0.001f) {
        perpendicularX = -dz / length;
        perpendicularZ = dx / length;
    } else {
        perpendicularX = 0.0f;
        perpendicularZ = 0.0f;
    }

    curveAngle = (s16)(progress * 0x7FFF);
    curve = Math_SinS(curveAngle);

    result->x += perpendicularX * curve * ENSBEETLE_PINCER_CURVE * side;
    result->z += perpendicularZ * curve * ENSBEETLE_PINCER_CURVE * side;
    result->y += curve * ENSBEETLE_PINCER_ARC_HEIGHT;
}

/*  --- This function was written by AI ---
 *  pincers will start flying back
 */
void EnSbeetle_StartPincerReturn(EnSbeetle* this) {
    this->pincerLReturnStart = this->pincerLWorldPos;
    this->pincerRReturnStart = this->pincerRWorldPos;

    this->pincerState = ENSBEETLE_PINCER_RETURN;
    this->pincerFlightTimer = 0;
}

/*  --- This function was written by AI ---
 *  If the scissors beetle takes damage while pincers are flying
 *  They fly back immediately stopping the attack
 */
void EnSbeetle_StartPincerFastReturn(EnSbeetle* this) {
    if ((this->pincerState != ENSBEETLE_PINCER_OUTBOUND) && (this->pincerState != ENSBEETLE_PINCER_RETURN)) {
        return;
    }

    this->pincerLReturnStart = this->pincerLWorldPos;
    this->pincerRReturnStart = this->pincerRWorldPos;

    this->pincerFlightTimer = 0;
    this->pincerState = ENSBEETLE_PINCER_FAST_RETURN;

    this->pincerLCollider.base.atFlags &= ~AT_HIT;
    this->pincerRCollider.base.atFlags &= ~AT_HIT;
}

/*  --- This function was written by AI ---
 *  Commands for the pincers on how to behave
 *  depending on state
 */
void EnSbeetle_UpdatePincers(EnSbeetle* this, PlayState* play) {
    f32 progress;
    s32 pincerHit;

    switch (this->pincerState) {
        case ENSBEETLE_PINCER_OUTBOUND:
            this->pincerLSpin += ENSBEETLE_PINCER_SPIN_SPEED;
            this->pincerRSpin -= ENSBEETLE_PINCER_SPIN_SPEED;

            pincerHit = (this->pincerLCollider.base.atFlags & AT_HIT) || (this->pincerRCollider.base.atFlags & AT_HIT);

            if (pincerHit) {
                this->pincerLCollider.base.atFlags &= ~AT_HIT;
                this->pincerRCollider.base.atFlags &= ~AT_HIT;

                EnSbeetle_StartPincerReturn(this);
                break;
            }

            progress = (f32)this->pincerFlightTimer / (f32)ENSBEETLE_PINCER_OUT_TIME;

            if (progress > 1.0f) {
                progress = 1.0f;
            }

            EnSbeetle_GetPincerPath(&this->pincerLHomePos, &this->pincerTargetPos, progress, -1.0f,
                                    &this->pincerLWorldPos);

            EnSbeetle_GetPincerPath(&this->pincerRHomePos, &this->pincerTargetPos, progress, 1.0f,
                                    &this->pincerRWorldPos);

            this->pincerFlightTimer++;

            if (this->pincerFlightTimer >= ENSBEETLE_PINCER_OUT_TIME) {
                EnSbeetle_StartPincerReturn(this);
            }
            break;

        case ENSBEETLE_PINCER_RETURN:
            this->pincerLSpin += ENSBEETLE_PINCER_SPIN_SPEED;
            this->pincerRSpin -= ENSBEETLE_PINCER_SPIN_SPEED;

            progress = (f32)this->pincerFlightTimer / (f32)ENSBEETLE_PINCER_RETURN_TIME;

            if (progress > 1.0f) {
                progress = 1.0f;
            }

            EnSbeetle_GetPincerPath(&this->pincerLReturnStart, &this->pincerLHomePos, progress, 1.0f,
                                    &this->pincerLWorldPos);

            EnSbeetle_GetPincerPath(&this->pincerRReturnStart, &this->pincerRHomePos, progress, -1.0f,
                                    &this->pincerRWorldPos);

            this->pincerFlightTimer++;

            if (this->pincerFlightTimer >= ENSBEETLE_PINCER_RETURN_TIME) {
                this->pincerState = ENSBEETLE_PINCER_ATTACHED;
                this->pincerFlightTimer = 0;
                this->pincerLSpin = 0;
                this->pincerRSpin = 0;
            }
            break;

        case ENSBEETLE_PINCER_FAST_RETURN:
            this->pincerLSpin += ENSBEETLE_PINCER_SPIN_SPEED * 2;
            this->pincerRSpin -= ENSBEETLE_PINCER_SPIN_SPEED * 2;

            progress = (f32)this->pincerFlightTimer / (f32)ENSBEETLE_PINCER_FAST_RETURN_TIME;

            if (progress > 1.0f) {
                progress = 1.0f;
            }

            EnSbeetle_GetPincerPath(&this->pincerLReturnStart, &this->pincerLHomePos, progress, 0.2f,
                                    &this->pincerLWorldPos);

            EnSbeetle_GetPincerPath(&this->pincerRReturnStart, &this->pincerRHomePos, progress, -0.2f,
                                    &this->pincerRWorldPos);

            this->pincerFlightTimer++;

            if (this->pincerFlightTimer >= ENSBEETLE_PINCER_FAST_RETURN_TIME) {
                this->pincerState = ENSBEETLE_PINCER_ATTACHED;
                this->pincerFlightTimer = 0;
                this->pincerLSpin = 0;
                this->pincerRSpin = 0;

                this->pincerLCollider.base.atFlags &= ~AT_HIT;
                this->pincerRCollider.base.atFlags &= ~AT_HIT;
            }
            break;

        default:
            break;
    }
}

/*  --- This function was written by AI ---
 *  Visual work of the pincers flying towards link and back
 *
 */
s32 EnSbeetle_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnSbeetle* this = (EnSbeetle*)thisx;
    Vec3f originalPos;

    switch (limbIndex) {
        case GSCISSORSBEETLESKEL_PINCER_L_LIMB:
            originalPos = *pos;
            Matrix_MultVec3f(&originalPos, &this->pincerLHomePos);

            if ((this->pincerState == ENSBEETLE_PINCER_OUTBOUND) || (this->pincerState == ENSBEETLE_PINCER_RETURN) ||
                (this->pincerState == ENSBEETLE_PINCER_FAST_RETURN)) {

                if ((this->pincerState == ENSBEETLE_PINCER_OUTBOUND) && (this->pincerFlightTimer <= 1)) {
                    this->pincerLWorldPos = this->pincerLHomePos;
                }

                EnSbeetle_WorldToCurrentMatrixLocal(&this->pincerLWorldPos, pos);

                rot->y += this->pincerLSpin;
            }
            break;

        case GSCISSORSBEETLESKEL_PINCER_R_LIMB:
            originalPos = *pos;
            Matrix_MultVec3f(&originalPos, &this->pincerRHomePos);

            if ((this->pincerState == ENSBEETLE_PINCER_OUTBOUND) || (this->pincerState == ENSBEETLE_PINCER_RETURN) ||
                (this->pincerState == ENSBEETLE_PINCER_FAST_RETURN)) {

                if ((this->pincerState == ENSBEETLE_PINCER_OUTBOUND) && (this->pincerFlightTimer <= 1)) {
                    this->pincerRWorldPos = this->pincerRHomePos;
                }

                EnSbeetle_WorldToCurrentMatrixLocal(&this->pincerRWorldPos, pos);

                rot->y += this->pincerRSpin;
            }
            break;
    }

    return false;
}

/*
 *  Sync actor focus position to the body and pincer colliders to the pincer bones
 *
 */
void EnSbeetle_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
    EnSbeetle* this = (EnSbeetle*)thisx;
    MtxF mtx;

    Matrix_Get(&mtx);

    switch (limbIndex) {
        case GSCISSORSBEETLESKEL_BODYFRONT_LIMB:
            Matrix_MultVec3f(&sZeroVec, &this->actor.focus.pos);
            break;

        case GSCISSORSBEETLESKEL_PINCER_L_LIMB:
            if ((this->pincerState != ENSBEETLE_PINCER_OUTBOUND) && (this->pincerState != ENSBEETLE_PINCER_RETURN)) {
                this->pincerLCollider.dim.pos.x = mtx.xw;
                this->pincerLCollider.dim.pos.y = mtx.yw;
                this->pincerLCollider.dim.pos.z = mtx.zw;
            }
            break;

        case GSCISSORSBEETLESKEL_PINCER_R_LIMB:
            if ((this->pincerState != ENSBEETLE_PINCER_OUTBOUND) && (this->pincerState != ENSBEETLE_PINCER_RETURN)) {
                this->pincerRCollider.dim.pos.x = mtx.xw;
                this->pincerRCollider.dim.pos.y = mtx.yw;
                this->pincerRCollider.dim.pos.z = mtx.zw;
            }
            break;
    }
}

/*
 *  Checks whether the beetle was hit and transitions it into the appropriate hurt, stunned, or death state.
 *
 */
void EnSbeetle_CheckHurt(EnSbeetle* this, PlayState* play) {
    static Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        Actor_SetDropFlag(&this->actor, &this->collider.info, true);
        this->actor.speed = 0.0f;

        switch (this->actor.colChkInfo.damageEffect) {
            case ENSBEETLE_DMGEFF_STUN:
                // Stunning effect because of e.g. a deku nut
                Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
                Actor_ApplyDamage(&this->actor);
                EnSbeetle_SetupStunned(this, play);
                break;
            case ENSBEETLE_DMGEFF_ICE_MAGIC:
                Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 255, COLORFILTER_BUFFLAG_OPA, 80);
                this->spawnIceTimer = 48;
                this->frozen = true;
                EnSbeetle_SetupStunned(this, play);
                break;
            case ENSBEETLE_DMGEFF_FIRE:
                Actor_PlaySfx(&this->actor, NA_SE_EV_FLAME_OF_FIRE);
                Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 80);
                this->fireTimer = 80;
                EnSbeetle_SetupDie(this, play);
                break;
            case ENSBEETLE_DMGEFF_NONE:
            default:
                if (this->actionFunc != EnSbeetle_ThreatPlayer) {
                    EnSbeetle_SetupHurt(this, play);
                }
                break;
        }
        if (this->actor.colChkInfo.health == 0) {
            EnSbeetle_SetupDie(this, play);
        }
    }
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_WATER) && this->actionFunc != EnSbeetle_Die) {
        // This enemy is not supposed to be in water so it dies immediately when in deep water
        EnSbeetle_SetupDie(this, play);
    }
}

/*
 *  Updates the beetle's collision state with the environment, including the ground, walls, ceilings, and water.
 *
 */
void EnSbeetle_UpdateBgCheck(EnSbeetle* this, PlayState* play) {
    Actor_UpdateBgCheckInfo(
        play, &this->actor, this->actor.colChkInfo.cylHeight, this->actor.colChkInfo.cylRadius,
        this->actor.colChkInfo.cylHeight,
        (UPDBGCHECKINFO_FLAG_0 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_3 | UPDBGCHECKINFO_FLAG_4));
}

#define ENSBEETLE_FORGET_DISTANCE 460.0f
#define ENSBEETLE_FORGET_HEIGHT 140.0f
#define ENSBEETLE_FORGET_TIME 60

/*
 *  Checks whether the player has moved far enough away or changed elevation enough for the beetle to lose track of
 * them.
 *
 */
s32 EnSbeetle_HasLostPlayer(EnSbeetle* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 yDist;

    yDist = player->actor.world.pos.y - this->actor.world.pos.y;

    if (this->actor.xzDistToPlayer > ENSBEETLE_FORGET_DISTANCE) {
        return true;
    }

    if (fabsf(yDist) > ENSBEETLE_FORGET_HEIGHT) {
        return true;
    }

    return false;
}

#define ENSBEETLE_HEARING_DISTANCE 200.0f
#define ENSBEETLE_FRONT_DISTANCE 460.0f
#define ENSBEETLE_FRONT_ANGLE 0x2000
#define ENSBEETLE_SIDE_DISTANCE 300.0f
#define ENSBEETLE_SIDE_ANGLE 0x5000
#define ENSBEETLE_DETECT_HEIGHT 80.0f

/*
 * Checks whether the player is close enough and within the beetle's hearing or field-of-view range to be detected.
 *
 */
s32 EnSbeetle_CheckPlayerNear(EnSbeetle* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 yDist;
    f32 xzDist;
    s16 yawToPlayer;
    s16 yawDiff;
    s16 absYawDiff;

    xzDist = this->actor.xzDistToPlayer;

    if (xzDist > ENSBEETLE_FRONT_DISTANCE) {
        return false;
    }

    yDist = player->actor.world.pos.y - this->actor.world.pos.y;

    if (fabsf(yDist) > ENSBEETLE_DETECT_HEIGHT) {
        return false;
    }

    if (xzDist <= ENSBEETLE_HEARING_DISTANCE) {
        return true;
    }

    yawToPlayer = Math_Vec3f_Yaw(&this->actor.world.pos, &player->actor.world.pos);

    yawDiff = yawToPlayer - this->actor.shape.rot.y;
    absYawDiff = ABS(yawDiff);

    if ((absYawDiff <= ENSBEETLE_FRONT_ANGLE) && (xzDist <= ENSBEETLE_FRONT_DISTANCE)) {
        return true;
    }

    if ((absYawDiff <= ENSBEETLE_SIDE_ANGLE) && (xzDist <= ENSBEETLE_SIDE_DISTANCE)) {
        return true;
    }

    return false;
}

/*
 * Enemy has nothing to do. Setup for idling around
 *
 */
void EnSbeetle_SetupDoNothing(EnSbeetle* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->nextIdleTimer = 100;
    EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_IDLE1);
    this->actionFunc = EnSbeetle_DoNothing;
}

/*
 * Enemy has nothing to do.
 * Constantly checking for the player
 * Random idle animations
 * Random walking
 */
void EnSbeetle_DoNothing(EnSbeetle* this, PlayState* play) {
    if (EnSbeetle_CheckPlayerNear(this, play) == true) {
        this->idleAction = NULL;
        EnSbeetle_SetupThreatPlayer(this, play);
    }

    if (this->idleAction == NULL) {
        SkelAnime_Update(&this->skelAnime);
        if (DECR(this->nextIdleTimer) == 0) {
            this->nextIdleTimer = Rand_S16Offset(40, 40); // Between 2 and 4 seconds
            if (Rand_ZeroOne() > 0.4f) {                  // 50% chance of a random idle action
                f32 randomIdle = Rand_ZeroOne();
                if (randomIdle <= 0.3f) { // 30% chance of idle2
                    this->afterAnimTimer = 10;
                    EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_IDLE2);
                    this->idleAction = EnSbeetle_IdleActionIdle2;
                } else if (randomIdle >= 0.4f && randomIdle <= 0.7f) { // 30% chance of idle3
                    this->afterAnimTimer = 10;
                    EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_IDLE3);
                    this->idleAction = EnSbeetle_IdleActionIdle3;
                } else { // 30% chance of walking
                    this->actor.speed = 1.0f;
                    this->randomWalkTimer = Rand_S16Offset(60, 40); // Between 3 and 5 seconds
                    EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_WALK);
                    this->idleAction = EnSbeetle_IdleActionWalk;
                }
            }
        }
    } else {
        this->idleAction(this, play); // This is like a sub actionFunc
    }
}

/*
 * Enemy starts walking randomly.
 *
 */
void EnSbeetle_IdleActionWalk(EnSbeetle* this, PlayState* play) {
    f32 distToHome = Math_Vec3f_DistXZ(&this->actor.world.pos, &this->actor.home.pos);

    SkelAnime_Update(&this->skelAnime);

    if (distToHome > 300.0f) { // this way, the scissors beetle doesn't move off too much from the spawn position
        Math_ApproachS(&this->actor.world.rot.y, Math_Vec3f_Yaw(&this->actor.world.pos, &this->actor.home.pos), 3,
                       4000);
    } else {
        Math_ApproachS(&this->actor.world.rot.y, Rand_S16Offset(this->actor.world.rot.y, 0x400), 3, 4000);
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 6000);

    if (Animation_OnFrame(&this->skelAnime, 10.0f) || Animation_OnFrame(&this->skelAnime, 17.0f)) {
        // foot touches the ground
        Actor_PlaySfx(&this->actor, NA_SE_EN_TEKU_WALK);
    }

    if (DECR(this->randomWalkTimer) == 0) { // Back to doing nothing
        this->actor.speed = 0.0f;
        EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_IDLE1);
        this->idleAction = NULL;
    }
}

/*
 * Enemy Idle2 Animation.
 * Rattling
 */
void EnSbeetle_IdleActionIdle2(EnSbeetle* this, PlayState* play) {
    if (Animation_OnFrame(&this->skelAnime, 7.0f)) { // Rattling sound
        Actor_PlaySfx(&this->actor, NA_SE_EN_TUBOOCK_FLY);
    }
    if (Animation_OnFrame(&this->skelAnime, 30.0f)) { // this sound effect must be stopped manually
        Audio_StopSfxById(NA_SE_EN_TUBOOCK_FLY);
    }

    if (SkelAnime_Update(&this->skelAnime)) {
        if (DECR(this->afterAnimTimer) == 0) { // Back to doing nothing
            EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_IDLE1);
            this->idleAction = NULL;
        }
    }
}

/*
 * Enemy Idle3 Animation.
 * Looking around
 */
void EnSbeetle_IdleActionIdle3(EnSbeetle* this, PlayState* play) {
    if (Animation_OnFrame(&this->skelAnime, 7.0f) || Animation_OnFrame(&this->skelAnime, 39.0f)) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_TEKU_WALK);
    }

    if (SkelAnime_Update(&this->skelAnime)) {
        if (DECR(this->afterAnimTimer) == 0) { // Back to doing nothing
            EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_IDLE1);
            this->idleAction = NULL;
        }
    }
}

/*
 * Ends up being unused.
 * Setup for a jump - see EnSbeetle_HopWithPlayerRot.
 *
 */
void EnSbeetle_SetupHopWithPlayerRot(EnSbeetle* this, PlayState* play) {
    this->actor.speed = 0.0f;
    EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_HOP);
    this->actionFunc = EnSbeetle_HopWithPlayerRot;
}

/*
 * Ends up being unused.
 * Jump and rotate towards the player midair.
 *
 */
void EnSbeetle_HopWithPlayerRot(EnSbeetle* this, PlayState* play) {
    if (this->skelAnime.curFrame >= 7.0f) {
        Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 4000);
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 6000);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnSbeetle_SetupThreatPlayer(this, play);
    }
}

void EnSbeetle_SetupThreatPlayer(EnSbeetle* this, PlayState* play) {
    EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_IDLE1);
    this->attackTimer = 40;
    this->playerLostTimer = ENSBEETLE_FORGET_TIME;
    this->actionFunc = EnSbeetle_ThreatPlayer;
}

void EnSbeetle_ThreatPlayer(EnSbeetle* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    if (!EnSbeetle_HasLostPlayer(this, play)) { // Always reset the timer if player is in sight
        this->playerLostTimer = ENSBEETLE_FORGET_TIME;
    } else if (DECR(this->playerLostTimer) == 0) { // Player lost
        this->actor.speed = 0.0f;
        EnSbeetle_SetupDoNothing(this, play);
        return;
    }

    // Rotate towards player
    Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 3000);
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 5000);

    if (DECR(this->attackTimer) == 0) {
        if (Rand_ZeroOne() < 0.6f) {     // 50% chance
            if (Rand_ZeroOne() < 0.6f) { // 50% chance for normal attack
                EnSbeetle_SetupAttack(this, play);
            } else { // 50% change for swinging the pincers
                EnSbeetle_SetupSwingAttack(this, play);
            }
            return;
        }
        this->attackTimer = 40; // 2 seconds
    }
}

void EnSbeetle_SetupAttack(EnSbeetle* this, PlayState* play) {
    EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_ATTACK);
    this->actor.speed = 0.0f;
    this->pincerLCollider.info.toucher.damage = 0x10;
    this->audioPlayed = false;
    this->actionFunc = EnSbeetle_Attack;
}

void EnSbeetle_Attack(EnSbeetle* this, PlayState* play) {
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->pincerLCollider.base);

    if (this->skelAnime.curFrame < 20.0f) { // Rotate towards player
        Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 3000);
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 5000);
    }

    if (Animation_OnFrame(&this->skelAnime, 24.0f) ||
        Animation_OnFrame(&this->skelAnime, 25.0f)) { // Dash towards player
        this->actor.speed = this->actor.xzDistToPlayer / 2;
        if (!this->audioPlayed) {
            Actor_PlaySfx(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
            this->audioPlayed = true;
        }
    } else {
        this->actor.speed = 0.0f;
    }

    if (SkelAnime_Update(&this->skelAnime)) { // Animation finished
        EnSbeetle_SetupHopAwayFromOrTowardsPlayer(this, play);
    }
}

/* --- This function was written by AI ---
 * Setup for the pincers
 *
 */
void EnSbeetle_SetupSwingAttack(EnSbeetle* this, PlayState* play) {
    EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_SWING);

    this->actor.speed = 0.0f;
    this->pincerLCollider.info.toucher.damage = 0x08;

    this->pincerState = ENSBEETLE_PINCER_WINDUP;
    this->pincerFlightTimer = 0;

    this->pincerLSpin = 0;
    this->pincerRSpin = 0;

    this->pincerLCollider.base.atFlags &= ~AT_HIT;
    this->pincerRCollider.base.atFlags &= ~AT_HIT;

    this->actionFunc = EnSbeetle_SwingAttack;
}

/* --- This function was written by AI ---
 *
 */
void EnSbeetle_ThrowPincers(EnSbeetle* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->pincerLWorldPos = this->pincerLHomePos;
    this->pincerRWorldPos = this->pincerRHomePos;

    this->pincerTargetPos = player->actor.world.pos;
    this->pincerTargetPos.y += 30.0f;

    this->pincerFlightTimer = 0;
    this->pincerLSpin = 0;
    this->pincerRSpin = 0;
    this->pincerState = ENSBEETLE_PINCER_OUTBOUND;

    this->pincerLCollider.base.atFlags &= ~AT_HIT;
    this->pincerRCollider.base.atFlags &= ~AT_HIT;

    Actor_PlaySfx(&this->actor, NA_SE_IT_BOOMERANG_THROW);
}

/* --- This function was written by AI ---
 * Setup for the pincers
 *
 */
void EnSbeetle_SwingAttack(EnSbeetle* this, PlayState* play) {
    s32 animationFinished;

    this->actor.speed = 0.0f;

    if (this->pincerState == ENSBEETLE_PINCER_WINDUP) { // Rotate towards player
        Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 3000);

        Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 5000);
    }

    animationFinished = SkelAnime_Update(&this->skelAnime);

    if ((this->pincerState == ENSBEETLE_PINCER_WINDUP) &&
        Animation_OnFrame(&this->skelAnime, ENSBEETLE_PINCER_THROW_FRAME)) {
        EnSbeetle_ThrowPincers(this, play);
    }

    if (animationFinished && (this->pincerState == ENSBEETLE_PINCER_ATTACHED)) {
        EnSbeetle_SetupHopAwayFromOrTowardsPlayer(this, play);
    }
}

void EnSbeetle_SetupHopAwayFromOrTowardsPlayer(EnSbeetle* this, PlayState* play) {
    EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_HOP);
    this->audioPlayed = false;
    this->playerDistAtSetup = this->actor.xzDistToPlayer;
    this->actionFunc = EnSbeetle_HopAwayFromOrTowardsPlayer;
}

void EnSbeetle_HopAwayFromOrTowardsPlayer(EnSbeetle* this, PlayState* play) {
    if (this->skelAnime.curFrame <= 6.0f || this->skelAnime.curFrame >= 15.0f) { // Rotate towards player before jumping
        this->actor.speed = 0.0f;
        Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 4000);
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 6000);
    } else {
        if (!this->audioPlayed) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_JUMP);
            this->audioPlayed = true;
        }
        if (this->playerDistAtSetup > 300.0f) { // Either jump towards the player
            this->actor.speed = 12.0f;
        } else {
            this->actor.speed = -12.0f; // Or away from the player
        }
    }

    if (SkelAnime_Update(&this->skelAnime)) {
        if (EnSbeetle_HasLostPlayer(this, play)) {
            EnSbeetle_SetupDoNothing(this, play);
        } else {
            EnSbeetle_SetupThreatPlayer(this, play);
        }
    }
}

void EnSbeetle_SetupStunned(EnSbeetle* this, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(this);
    this->actor.speed = 0.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Animation_PlayOnceSetSpeed(&this->skelAnime, &gScissorsBeetleSkelIdle1Anim, 0.0f);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
    this->actionFunc = EnSbeetle_Stunned;
}

void EnSbeetle_Stunned(EnSbeetle* this, PlayState* play) {
    if (this->spawnIceTimer == 0) {
        if (this->actor.colorFilterTimer == 0) {
            EnSbeetle_SetupHopAwayFromOrTowardsPlayer(this, play);
            if (this->frozen) {
                Actor_PlaySfx(&this->actor, NA_SE_EV_ICE_BROKEN);
                this->frozen = false;
            }
        }
    }
}

void EnSbeetle_SetupHurt(EnSbeetle* this, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(this);
    this->damageTimer = 2;
    this->hurtboxCooldown = 40;
    EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_HURT);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
    Actor_ApplyDamage(&this->actor);
    Actor_PlaySfx(&this->actor, NA_SE_EN_BUBLEWALK_AIM);
    this->actionFunc = EnSbeetle_Hurt;
}

void EnSbeetle_Hurt(EnSbeetle* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        if (DECR(this->damageTimer) == 0) { // timer for seeing the Sbeetle taking damage
            EnSbeetle_SetupHopAwayFromOrTowardsPlayer(this, play);
        }
    }
}

void EnSbeetle_SetupDie(EnSbeetle* this, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(this);
    this->deathFreeze = 12;
    this->actor.speed = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_0; // Sbeetle not targetable anymore
    this->actor.shape.shadowAlpha = 0;
    Actor_PlaySfx(&this->actor, NA_SE_EN_BUBLEWALK_DEAD);
    Enemy_StartFinishingBlow(play, &this->actor);
    EnSbeetle_ChangeAnimation(this, SCISSORSBEETLE_ANIMATION_DIE);
    this->actionFunc = EnSbeetle_Die;
}

void EnSbeetle_Die(EnSbeetle* this, PlayState* play) {
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    Vec3f effectVel = { 0.0f, 4.0f, 0.0f };
    Vec3f effectPos = this->actor.world.pos;

    if (SkelAnime_Update(&this->skelAnime)) {
        if (DECR(this->deathFreeze) == 0) {
            Math_StepToF(&this->actor.scale.x, 0.0f, 0.0084f); // Sbeetle shrinks in his scale while dying
            this->actor.scale.y = this->actor.scale.z = this->actor.scale.x;
            if (this->actor.scale.x <= 0.001f) { // Enemy not visible anymore
                effectPos.y += 10.0f;
                EffectSsDeadDb_Spawn(play, &effectPos, &effectVel, &zeroVec, 90, 0, 255, 255, 255, 255, 0, 0, 255, 1, 9,
                                     true);
                Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos,
                                           0xE0); // The Sbeetle might drop some random collectibles
                Actor_Kill(&this->actor);
            }
        }
    }
}
