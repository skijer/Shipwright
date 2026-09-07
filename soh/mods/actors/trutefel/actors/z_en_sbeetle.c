/*
 * File: z_en_sbeetle.c (SoH port)
 * Description: Scissors Beetle comparable to the Scissors Beetles from The Minish Cap
 * Authors: @syeo501 (Model) @trueffel (Code) — ported to SoH (Skijer's NEI)
 * Note: This enemy code was mostly written by @trueffel but contains some AI code mostly for mathematical operations
 *       related to the pincer attack.
 *
 * ---- Port notes (modern OoT decomp -> SoH) --------------------------------------------
 * - Unity-#included into trutefel_enemies.cpp inside extern "C" (C++ TU): `this` -> `self`,
 *   file-scope statics prefixed sSbeetle*.
 * - Actor_PlaySfx -> Audio_PlayActorSound2 | actor.speed -> actor.speedXZ
 * - NAVI_ENEMY_SCISSORS_BEETLE has no SoH enum entry: naviEnemyId 0x5D -> Navi C-up text
 *   0x065D (z_player.c: textId = naviEnemyId + 0x600). The message itself is a CustomMessage
 *   registered on the OnOpenText hook for 0x065D (trutefel_actor_reg.cpp) — English only,
 *   the reference's german/french placeholders were dropped.
 * - ACTOR_FLAG_18 (navi/C-up dialogue) -> ACTOR_FLAG_TALK_WITH_C_UP (same bit 1<<18), set
 *   with the rest of the flags at registration.
 */

#include "z_en_sbeetle.h"

// Navi C-up dialogue: textId = 0x600 + naviEnemyId -> 0x065D (see trutefel_actor_reg.cpp)
#define NAVI_ENEMY_SCISSORS_BEETLE 0x5D

#ifndef UPDBGCHECKINFO_FLAG_0
#define UPDBGCHECKINFO_FLAG_0 (1 << 0)
#define UPDBGCHECKINFO_FLAG_2 (1 << 2)
#define UPDBGCHECKINFO_FLAG_3 (1 << 3)
#define UPDBGCHECKINFO_FLAG_4 (1 << 4)
#endif

#ifndef COLORFILTER_COLORFLAG_RED
#define COLORFILTER_COLORFLAG_GRAY 0x8000
#define COLORFILTER_COLORFLAG_RED 0x4000
#define COLORFILTER_COLORFLAG_BLUE 0x0000
#define COLORFILTER_BUFFLAG_XLU 0x2000
#define COLORFILTER_BUFFLAG_OPA 0x0000
#endif

void EnSbeetle_Init(Actor* thisx, PlayState* play);
void EnSbeetle_Destroy(Actor* thisx, PlayState* play);
void EnSbeetle_Update(Actor* thisx, PlayState* play);
void EnSbeetle_Draw(Actor* thisx, PlayState* play);

void EnSbeetle_WorldToCurrentMatrixLocal(Vec3f* worldPos, Vec3f* localPos);
void EnSbeetle_GetPincerPath(Vec3f* start, Vec3f* end, f32 progress, f32 side, Vec3f* result);
void EnSbeetle_StartPincerReturn(EnSbeetle* self);
void EnSbeetle_StartPincerFastReturn(EnSbeetle* self);
void EnSbeetle_UpdatePincers(EnSbeetle* self, PlayState* play);

s32 EnSbeetle_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx);
void EnSbeetle_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx);

void EnSbeetle_CheckHurt(EnSbeetle* self, PlayState* play);
void EnSbeetle_UpdateBgCheck(EnSbeetle* self, PlayState* play);
s32 EnSbeetle_HasLostPlayer(EnSbeetle* self, PlayState* play);
s32 EnSbeetle_CheckPlayerNear(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupDoNothing(EnSbeetle* self, PlayState* play);
void EnSbeetle_DoNothing(EnSbeetle* self, PlayState* play);
void EnSbeetle_IdleActionWalk(EnSbeetle* self, PlayState* play);
void EnSbeetle_IdleActionIdle2(EnSbeetle* self, PlayState* play);
void EnSbeetle_IdleActionIdle3(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupHopWithPlayerRot(EnSbeetle* self, PlayState* play);
void EnSbeetle_HopWithPlayerRot(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupThreatPlayer(EnSbeetle* self, PlayState* play);
void EnSbeetle_ThreatPlayer(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupAttack(EnSbeetle* self, PlayState* play);
void EnSbeetle_Attack(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupSwingAttack(EnSbeetle* self, PlayState* play);
void EnSbeetle_SwingAttack(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupHopAwayFromOrTowardsPlayer(EnSbeetle* self, PlayState* play);
void EnSbeetle_HopAwayFromOrTowardsPlayer(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupStunned(EnSbeetle* self, PlayState* play);
void EnSbeetle_Stunned(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupHurt(EnSbeetle* self, PlayState* play);
void EnSbeetle_Hurt(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupDie(EnSbeetle* self, PlayState* play);
void EnSbeetle_Die(EnSbeetle* self, PlayState* play);

#define ENSBEETLE_PINCER_THROW_FRAME 12.0f
#define ENSBEETLE_PINCER_OUT_TIME 18
#define ENSBEETLE_PINCER_RETURN_TIME 20
#define ENSBEETLE_PINCER_CURVE 55.0f
#define ENSBEETLE_PINCER_ARC_HEIGHT 25.0f
#define ENSBEETLE_PINCER_SPIN_SPEED 0x2800
#define ENSBEETLE_PINCER_FAST_RETURN_TIME 6

// Runtime ActorDB id — filled by Trutefel_EnsureActorsRegistered(); -1 until then.
s16 gEnSbeetleId = -1;
size_t gEnSbeetleStructSize = sizeof(EnSbeetle);

static ColliderCylinderInit sSbeetleCylinderInit = {
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

static ColliderCylinderInit sSbeetlePincerLCylinderInit = {
    {
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
    { 35, 30, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sSbeetlePincerRCylinderInit = {
    {
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
    { 35, 30, 0, { 0, 0, 0 } },
};

typedef enum {
    /*  0 */ ENSBEETLE_DMGEFF_NONE,
    /*  1 */ ENSBEETLE_DMGEFF_STUN,
    /*  6 */ ENSBEETLE_DMGEFF_ICE_MAGIC = 6,
    /* 13 */ ENSBEETLE_DMGEFF_LIGHT_MAGIC = 13,
    /* 14 */ ENSBEETLE_DMGEFF_FIRE,
} EnSbeetleDamageEffect;

static DamageTable sSbeetleDamageTable = {
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

// { health, cylRadius, cylHeight, cylYShift, mass } (positional, see z_en_miniblin.c)
static CollisionCheckInfoInit2 sSbeetleColChkInit = { 5, 25, 35, 0, MASS_HEAVY };

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

static AnimationInfo sSbeetleAnimationInfo[] = {
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

void EnSbeetle_ChangeAnimation(EnSbeetle* self, s32 index) {
    Animation_ChangeByInfo(&self->skelAnime, sSbeetleAnimationInfo, index);
}

/*
 * Prepares pincer colliders and body collider
 */
void EnSbeetle_InitAndSetCollision(EnSbeetle* self, PlayState* play) {
    Collider_InitCylinder(play, &self->collider);
    Collider_SetCylinder(play, &self->collider, &self->actor, &sSbeetleCylinderInit);
    CollisionCheck_SetInfo2(&self->actor.colChkInfo, &sSbeetleDamageTable, &sSbeetleColChkInit);

    Collider_InitCylinder(play, &self->pincerLCollider);
    Collider_SetCylinder(play, &self->pincerLCollider, &self->actor, &sSbeetlePincerLCylinderInit);

    Collider_InitCylinder(play, &self->pincerRCollider);
    Collider_SetCylinder(play, &self->pincerRCollider, &self->actor, &sSbeetlePincerRCylinderInit);
}

/*  --- This function was written by AI ---
 */
void EnSbeetle_InitPincers(EnSbeetle* self, PlayState* play) {
    self->pincerState = ENSBEETLE_PINCER_ATTACHED;
    self->pincerFlightTimer = 0;
    self->pincerLSpin = 0;
    self->pincerRSpin = 0;

    self->pincerLWorldPos = self->actor.world.pos;
    self->pincerRWorldPos = self->actor.world.pos;

    self->pincerLHomePos = self->actor.world.pos;
    self->pincerRHomePos = self->actor.world.pos;

    self->pincerLReturnStart = self->actor.world.pos;
    self->pincerRReturnStart = self->actor.world.pos;

    self->pincerTargetPos = self->actor.world.pos;
}

void EnSbeetle_Init(Actor* thisx, PlayState* play) {
    EnSbeetle* self = (EnSbeetle*)thisx;

    ActorShape_Init(&self->actor.shape, 0.0f, ActorShadow_DrawCircle, 10.0f);
    Actor_SetScale(&self->actor, 0.1f);
    self->actor.naviEnemyId = NAVI_ENEMY_SCISSORS_BEETLE;
    thisx->gravity = -1.0f;
    self->nextIdleTimer = 0;
    self->attackTimer = 0;
    EnSbeetle_InitAndSetCollision(self, play);
    SkelAnime_InitFlex(play, &self->skelAnime, &gScissorsBeetleSkel, &gScissorsBeetleSkelIdle1Anim, self->jointTable,
                       self->morphTable, GSCISSORSBEETLESKEL_NUM_LIMBS);
    EnSbeetle_SetupDoNothing(self, play);
}

void EnSbeetle_Destroy(Actor* thisx, PlayState* play) {
    EnSbeetle* self = (EnSbeetle*)thisx;

    // NOTE: the reference calls SkelAnime_Free here, but SoH's SkelAnime_Free
    // unconditionally ZeldaArena-frees jointTable/morphTable — ours live inside the actor
    // struct (passed to SkelAnime_InitFlex), so freeing them would corrupt the heap.
    Collider_DestroyCylinder(play, &self->collider);
    Collider_DestroyCylinder(play, &self->pincerLCollider);
    Collider_DestroyCylinder(play, &self->pincerRCollider);
}

void EnSbeetle_Update(Actor* thisx, PlayState* play) {
    EnSbeetle* self = (EnSbeetle*)thisx;

    EnSbeetle_CheckHurt(self, play);
    self->actionFunc(self, play);

    EnSbeetle_UpdatePincers(self, play);

    Actor_MoveXZGravity(thisx);
    EnSbeetle_UpdateBgCheck(self, play);

    Collider_UpdateCylinder(thisx, &self->collider);

    if ((self->pincerState == ENSBEETLE_PINCER_OUTBOUND) || (self->pincerState == ENSBEETLE_PINCER_RETURN) ||
        (self->pincerState == ENSBEETLE_PINCER_FAST_RETURN)) { // Update pincer collider positions

        Collider_UpdateCylinder(&self->actor, &self->pincerLCollider);

        Collider_UpdateCylinder(&self->actor, &self->pincerRCollider);

        self->pincerLCollider.dim.pos.x = self->pincerLWorldPos.x;
        self->pincerLCollider.dim.pos.y = self->pincerLWorldPos.y;
        self->pincerLCollider.dim.pos.z = self->pincerLWorldPos.z;

        self->pincerRCollider.dim.pos.x = self->pincerRWorldPos.x;
        self->pincerRCollider.dim.pos.y = self->pincerRWorldPos.y;
        self->pincerRCollider.dim.pos.z = self->pincerRWorldPos.z;

        if (self->pincerState != ENSBEETLE_PINCER_FAST_RETURN) {
            CollisionCheck_SetAT(play, &play->colChkCtx, &self->pincerLCollider.base);
            CollisionCheck_SetAT(play, &play->colChkCtx, &self->pincerRCollider.base);
        }
    }

    if (self->actionFunc != EnSbeetle_Die) {    // Enemy can't take more damage after death
        if (DECR(self->hurtboxCooldown) == 0) { // Player is not able to spam the sword
            CollisionCheck_SetAC(play, &play->colChkCtx, &self->collider.base);
        }

        CollisionCheck_SetOC(play, &play->colChkCtx, &self->collider.base);
    }
}

// Relative positions to spawn ice chunks when frozen
static Vec3f sSbeetleIceChunks[12] = {
    { 20.0f, 20.0f, 0.0f },   { 10.0f, 40.0f, 10.0f },   { -10.0f, 40.0f, 10.0f }, { -20.0f, 20.0f, 0.0f },
    { 10.0f, 40.0f, -10.0f }, { -10.0f, 40.0f, -10.0f }, { 0.0f, 20.0f, -20.0f },  { 10.0f, 0.0f, 10.0f },
    { 10.0f, 0.0f, -10.0f },  { 0.0f, 20.0f, 20.0f },    { -10.0f, 0.0f, 10.0f },  { -10.0f, 0.0f, -10.0f },
};

static Vec3f sSbeetleFlames[12] = {
    { 20.0f, 20.0f, 0.0f },   { 10.0f, 40.0f, 10.0f },   { -10.0f, 40.0f, 10.0f }, { -20.0f, 20.0f, 0.0f },
    { 10.0f, 40.0f, -10.0f }, { -10.0f, 40.0f, -10.0f }, { 0.0f, 20.0f, -20.0f },  { 10.0f, 0.0f, 10.0f },
    { 10.0f, 0.0f, -10.0f },  { 0.0f, 20.0f, 20.0f },    { -10.0f, 0.0f, 10.0f },  { -10.0f, 0.0f, -10.0f },
};

void EnSbeetle_Draw(Actor* thisx, PlayState* play) {
    EnSbeetle* self = (EnSbeetle*)thisx;

    if (self->spawnIceTimer != 0) {
        // Spawn chunks of ice all over the body
        thisx->colorFilterTimer++;
        self->spawnIceTimer--;
        if ((self->spawnIceTimer & 3) == 0) {
            Vec3f iceChunk;
            s32 idx = self->spawnIceTimer >> 2;

            iceChunk.x = thisx->world.pos.x + sSbeetleIceChunks[idx].x;
            iceChunk.y = thisx->world.pos.y + sSbeetleIceChunks[idx].y;
            iceChunk.z = thisx->world.pos.z + sSbeetleIceChunks[idx].z;
            EffectSsEnIce_SpawnFlyingVec3f(play, &self->actor, &iceChunk, 150, 150, 150, 250, 235, 245, 255, 2.0f);
        }
    }

    if (self->fireTimer != 0) {
        thisx->colorFilterTimer++;
        self->fireTimer--;
        if ((self->fireTimer & 3) == 0) {
            Vec3f firePos = self->actor.world.pos;
            Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
            Vec3f effectVel = { 0.0f, 4.0f, 0.0f };

            s32 idx = self->fireTimer >> 2;

            firePos.x = thisx->world.pos.x + sSbeetleFlames[idx].x;
            firePos.y = thisx->world.pos.y + sSbeetleFlames[idx].y;
            firePos.z = thisx->world.pos.z + sSbeetleFlames[idx].z;

            EffectSsDeadDb_Spawn(play, &firePos, &effectVel, &zeroVec, 90, 0, 200, 135, 50, 255, 200, 80, 50, 1, 9,
                                 true);
        }
    }

    SkelAnime_DrawFlexOpa(play, self->skelAnime.skeleton, self->jointTable, self->skelAnime.dListCount,
                          EnSbeetle_OverrideLimbDraw, EnSbeetle_PostLimbDraw, self);
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
void EnSbeetle_StartPincerReturn(EnSbeetle* self) {
    self->pincerLReturnStart = self->pincerLWorldPos;
    self->pincerRReturnStart = self->pincerRWorldPos;

    self->pincerState = ENSBEETLE_PINCER_RETURN;
    self->pincerFlightTimer = 0;
}

/*  --- This function was written by AI ---
 *  If the scissors beetle takes damage while pincers are flying
 *  They fly back immediately stopping the attack
 */
void EnSbeetle_StartPincerFastReturn(EnSbeetle* self) {
    if ((self->pincerState != ENSBEETLE_PINCER_OUTBOUND) && (self->pincerState != ENSBEETLE_PINCER_RETURN)) {
        return;
    }

    self->pincerLReturnStart = self->pincerLWorldPos;
    self->pincerRReturnStart = self->pincerRWorldPos;

    self->pincerFlightTimer = 0;
    self->pincerState = ENSBEETLE_PINCER_FAST_RETURN;

    self->pincerLCollider.base.atFlags &= ~AT_HIT;
    self->pincerRCollider.base.atFlags &= ~AT_HIT;
}

/*  --- This function was written by AI ---
 *  Commands for the pincers on how to behave
 *  depending on state
 */
void EnSbeetle_UpdatePincers(EnSbeetle* self, PlayState* play) {
    f32 progress;
    s32 pincerHit;

    switch (self->pincerState) {
        case ENSBEETLE_PINCER_OUTBOUND:
            self->pincerLSpin += ENSBEETLE_PINCER_SPIN_SPEED;
            self->pincerRSpin -= ENSBEETLE_PINCER_SPIN_SPEED;

            pincerHit = (self->pincerLCollider.base.atFlags & AT_HIT) || (self->pincerRCollider.base.atFlags & AT_HIT);

            if (pincerHit) {
                self->pincerLCollider.base.atFlags &= ~AT_HIT;
                self->pincerRCollider.base.atFlags &= ~AT_HIT;

                EnSbeetle_StartPincerReturn(self);
                break;
            }

            progress = (f32)self->pincerFlightTimer / (f32)ENSBEETLE_PINCER_OUT_TIME;

            if (progress > 1.0f) {
                progress = 1.0f;
            }

            EnSbeetle_GetPincerPath(&self->pincerLHomePos, &self->pincerTargetPos, progress, -1.0f,
                                    &self->pincerLWorldPos);

            EnSbeetle_GetPincerPath(&self->pincerRHomePos, &self->pincerTargetPos, progress, 1.0f,
                                    &self->pincerRWorldPos);

            self->pincerFlightTimer++;

            if (self->pincerFlightTimer >= ENSBEETLE_PINCER_OUT_TIME) {
                EnSbeetle_StartPincerReturn(self);
            }
            break;

        case ENSBEETLE_PINCER_RETURN:
            self->pincerLSpin += ENSBEETLE_PINCER_SPIN_SPEED;
            self->pincerRSpin -= ENSBEETLE_PINCER_SPIN_SPEED;

            progress = (f32)self->pincerFlightTimer / (f32)ENSBEETLE_PINCER_RETURN_TIME;

            if (progress > 1.0f) {
                progress = 1.0f;
            }

            EnSbeetle_GetPincerPath(&self->pincerLReturnStart, &self->pincerLHomePos, progress, 1.0f,
                                    &self->pincerLWorldPos);

            EnSbeetle_GetPincerPath(&self->pincerRReturnStart, &self->pincerRHomePos, progress, -1.0f,
                                    &self->pincerRWorldPos);

            self->pincerFlightTimer++;

            if (self->pincerFlightTimer >= ENSBEETLE_PINCER_RETURN_TIME) {
                self->pincerState = ENSBEETLE_PINCER_ATTACHED;
                self->pincerFlightTimer = 0;
                self->pincerLSpin = 0;
                self->pincerRSpin = 0;
            }
            break;

        case ENSBEETLE_PINCER_FAST_RETURN:
            self->pincerLSpin += ENSBEETLE_PINCER_SPIN_SPEED * 2;
            self->pincerRSpin -= ENSBEETLE_PINCER_SPIN_SPEED * 2;

            progress = (f32)self->pincerFlightTimer / (f32)ENSBEETLE_PINCER_FAST_RETURN_TIME;

            if (progress > 1.0f) {
                progress = 1.0f;
            }

            EnSbeetle_GetPincerPath(&self->pincerLReturnStart, &self->pincerLHomePos, progress, 0.2f,
                                    &self->pincerLWorldPos);

            EnSbeetle_GetPincerPath(&self->pincerRReturnStart, &self->pincerRHomePos, progress, -0.2f,
                                    &self->pincerRWorldPos);

            self->pincerFlightTimer++;

            if (self->pincerFlightTimer >= ENSBEETLE_PINCER_FAST_RETURN_TIME) {
                self->pincerState = ENSBEETLE_PINCER_ATTACHED;
                self->pincerFlightTimer = 0;
                self->pincerLSpin = 0;
                self->pincerRSpin = 0;

                self->pincerLCollider.base.atFlags &= ~AT_HIT;
                self->pincerRCollider.base.atFlags &= ~AT_HIT;
            }
            break;

        default:
            break;
    }
}

/*  --- This function was written by AI ---
 *  Visual work of the pincers flying towards link and back
 */
s32 EnSbeetle_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnSbeetle* self = (EnSbeetle*)thisx;
    Vec3f originalPos;

    switch (limbIndex) {
        case GSCISSORSBEETLESKEL_PINCER_L_LIMB:
            originalPos = *pos;
            Matrix_MultVec3f(&originalPos, &self->pincerLHomePos);

            if ((self->pincerState == ENSBEETLE_PINCER_OUTBOUND) || (self->pincerState == ENSBEETLE_PINCER_RETURN) ||
                (self->pincerState == ENSBEETLE_PINCER_FAST_RETURN)) {

                if ((self->pincerState == ENSBEETLE_PINCER_OUTBOUND) && (self->pincerFlightTimer <= 1)) {
                    self->pincerLWorldPos = self->pincerLHomePos;
                }

                EnSbeetle_WorldToCurrentMatrixLocal(&self->pincerLWorldPos, pos);

                rot->y += self->pincerLSpin;
            }
            break;

        case GSCISSORSBEETLESKEL_PINCER_R_LIMB:
            originalPos = *pos;
            Matrix_MultVec3f(&originalPos, &self->pincerRHomePos);

            if ((self->pincerState == ENSBEETLE_PINCER_OUTBOUND) || (self->pincerState == ENSBEETLE_PINCER_RETURN) ||
                (self->pincerState == ENSBEETLE_PINCER_FAST_RETURN)) {

                if ((self->pincerState == ENSBEETLE_PINCER_OUTBOUND) && (self->pincerFlightTimer <= 1)) {
                    self->pincerRWorldPos = self->pincerRHomePos;
                }

                EnSbeetle_WorldToCurrentMatrixLocal(&self->pincerRWorldPos, pos);

                rot->y += self->pincerRSpin;
            }
            break;
    }

    return false;
}

/*
 *  Sync actor focus position to the body and pincer colliders to the pincer bones
 */
void EnSbeetle_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sZeroVecSbeetle = { 0.0f, 0.0f, 0.0f };
    EnSbeetle* self = (EnSbeetle*)thisx;
    MtxF mtx;

    Matrix_Get(&mtx);

    switch (limbIndex) {
        case GSCISSORSBEETLESKEL_BODYFRONT_LIMB:
            Matrix_MultVec3f(&sZeroVecSbeetle, &self->actor.focus.pos);
            break;

        case GSCISSORSBEETLESKEL_PINCER_L_LIMB:
            if ((self->pincerState != ENSBEETLE_PINCER_OUTBOUND) && (self->pincerState != ENSBEETLE_PINCER_RETURN)) {
                self->pincerLCollider.dim.pos.x = mtx.xw;
                self->pincerLCollider.dim.pos.y = mtx.yw;
                self->pincerLCollider.dim.pos.z = mtx.zw;
            }
            break;

        case GSCISSORSBEETLESKEL_PINCER_R_LIMB:
            if ((self->pincerState != ENSBEETLE_PINCER_OUTBOUND) && (self->pincerState != ENSBEETLE_PINCER_RETURN)) {
                self->pincerRCollider.dim.pos.x = mtx.xw;
                self->pincerRCollider.dim.pos.y = mtx.yw;
                self->pincerRCollider.dim.pos.z = mtx.zw;
            }
            break;
    }
}

/*
 *  Checks whether the beetle was hit and transitions it into the appropriate hurt, stunned, or death state.
 */
void EnSbeetle_CheckHurt(EnSbeetle* self, PlayState* play) {
    if (self->collider.base.acFlags & AC_HIT) {
        self->collider.base.acFlags &= ~AC_HIT;
        Actor_SetDropFlag(&self->actor, &self->collider.info, true);
        self->actor.speedXZ = 0.0f;

        switch (self->actor.colChkInfo.damageEffect) {
            case ENSBEETLE_DMGEFF_STUN:
                // Stunning effect because of e.g. a deku nut
                Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
                Actor_ApplyDamage(&self->actor);
                EnSbeetle_SetupStunned(self, play);
                break;
            case ENSBEETLE_DMGEFF_ICE_MAGIC:
                Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_BLUE, 255, COLORFILTER_BUFFLAG_OPA, 80);
                self->spawnIceTimer = 48;
                self->frozen = true;
                EnSbeetle_SetupStunned(self, play);
                break;
            case ENSBEETLE_DMGEFF_FIRE:
                Audio_PlayActorSound2(&self->actor, NA_SE_EV_FLAME_OF_FIRE);
                Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 80);
                self->fireTimer = 80;
                EnSbeetle_SetupDie(self, play);
                break;
            case ENSBEETLE_DMGEFF_NONE:
            default:
                if (self->actionFunc != EnSbeetle_ThreatPlayer) {
                    EnSbeetle_SetupHurt(self, play);
                }
                break;
        }
        if (self->actor.colChkInfo.health == 0) {
            EnSbeetle_SetupDie(self, play);
        }
    }
    if ((self->actor.bgCheckFlags & BGCHECKFLAG_WATER) && self->actionFunc != EnSbeetle_Die) {
        // This enemy is not supposed to be in water so it dies immediately when in deep water
        EnSbeetle_SetupDie(self, play);
    }
}

/*
 *  Updates the beetle's collision state with the environment, including the ground, walls, ceilings, and water.
 */
void EnSbeetle_UpdateBgCheck(EnSbeetle* self, PlayState* play) {
    Actor_UpdateBgCheckInfo(
        play, &self->actor, self->actor.colChkInfo.cylHeight, self->actor.colChkInfo.cylRadius,
        self->actor.colChkInfo.cylHeight,
        (UPDBGCHECKINFO_FLAG_0 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_3 | UPDBGCHECKINFO_FLAG_4));
}

#define ENSBEETLE_FORGET_DISTANCE 460.0f
#define ENSBEETLE_FORGET_HEIGHT 140.0f
#define ENSBEETLE_FORGET_TIME 60

/*
 *  Checks whether the player has moved far enough away or changed elevation enough for the beetle to lose track of
 *  them.
 */
s32 EnSbeetle_HasLostPlayer(EnSbeetle* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 yDist;

    yDist = player->actor.world.pos.y - self->actor.world.pos.y;

    if (self->actor.xzDistToPlayer > ENSBEETLE_FORGET_DISTANCE) {
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
 */
s32 EnSbeetle_CheckPlayerNear(EnSbeetle* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 yDist;
    f32 xzDist;
    s16 yawToPlayer;
    s16 yawDiff;
    s16 absYawDiff;

    xzDist = self->actor.xzDistToPlayer;

    if (xzDist > ENSBEETLE_FRONT_DISTANCE) {
        return false;
    }

    yDist = player->actor.world.pos.y - self->actor.world.pos.y;

    if (fabsf(yDist) > ENSBEETLE_DETECT_HEIGHT) {
        return false;
    }

    if (xzDist <= ENSBEETLE_HEARING_DISTANCE) {
        return true;
    }

    yawToPlayer = Math_Vec3f_Yaw(&self->actor.world.pos, &player->actor.world.pos);

    yawDiff = yawToPlayer - self->actor.shape.rot.y;
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
 */
void EnSbeetle_SetupDoNothing(EnSbeetle* self, PlayState* play) {
    self->actor.speedXZ = 0.0f;
    self->nextIdleTimer = 100;
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE1);
    self->actionFunc = EnSbeetle_DoNothing;
}

/*
 * Enemy has nothing to do.
 * Constantly checking for the player
 * Random idle animations
 * Random walking
 */
void EnSbeetle_DoNothing(EnSbeetle* self, PlayState* play) {
    if (EnSbeetle_CheckPlayerNear(self, play) == true) {
        self->idleAction = NULL;
        EnSbeetle_SetupThreatPlayer(self, play);
    }

    if (self->idleAction == NULL) {
        SkelAnime_Update(&self->skelAnime);
        if (DECR(self->nextIdleTimer) == 0) {
            self->nextIdleTimer = Rand_S16Offset(40, 40); // Between 2 and 4 seconds
            if (Rand_ZeroOne() > 0.4f) {                  // ~50% chance of a random idle action
                f32 randomIdle = Rand_ZeroOne();
                if (randomIdle <= 0.3f) { // 30% chance of idle2
                    self->afterAnimTimer = 10;
                    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE2);
                    self->idleAction = EnSbeetle_IdleActionIdle2;
                } else if (randomIdle >= 0.4f && randomIdle <= 0.7f) { // 30% chance of idle3
                    self->afterAnimTimer = 10;
                    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE3);
                    self->idleAction = EnSbeetle_IdleActionIdle3;
                } else { // 30% chance of walking
                    self->actor.speedXZ = 1.0f;
                    self->randomWalkTimer = Rand_S16Offset(60, 40); // Between 3 and 5 seconds
                    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_WALK);
                    self->idleAction = EnSbeetle_IdleActionWalk;
                }
            }
        }
    } else {
        self->idleAction(self, play); // This is like a sub actionFunc
    }
}

/*
 * Enemy starts walking randomly.
 */
void EnSbeetle_IdleActionWalk(EnSbeetle* self, PlayState* play) {
    f32 distToHome = Math_Vec3f_DistXZ(&self->actor.world.pos, &self->actor.home.pos);

    SkelAnime_Update(&self->skelAnime);

    if (distToHome > 300.0f) { // this way, the scissors beetle doesn't move off too much from the spawn position
        Math_ApproachS(&self->actor.world.rot.y, Math_Vec3f_Yaw(&self->actor.world.pos, &self->actor.home.pos), 3,
                       4000);
    } else {
        Math_ApproachS(&self->actor.world.rot.y, Rand_S16Offset(self->actor.world.rot.y, 0x400), 3, 4000);
    }
    Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 6000);

    if (Animation_OnFrame(&self->skelAnime, 10.0f) || Animation_OnFrame(&self->skelAnime, 17.0f)) {
        // foot touches the ground
        Audio_PlayActorSound2(&self->actor, NA_SE_EN_TEKU_WALK);
    }

    if (DECR(self->randomWalkTimer) == 0) { // Back to doing nothing
        self->actor.speedXZ = 0.0f;
        EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE1);
        self->idleAction = NULL;
    }
}

/*
 * Enemy Idle2 Animation.
 * Rattling
 */
void EnSbeetle_IdleActionIdle2(EnSbeetle* self, PlayState* play) {
    if (Animation_OnFrame(&self->skelAnime, 7.0f)) { // Rattling sound
        Audio_PlayActorSound2(&self->actor, NA_SE_EN_TUBOOCK_FLY);
    }
    if (Animation_OnFrame(&self->skelAnime, 30.0f)) { // this sound effect must be stopped manually
        Audio_StopSfxById(NA_SE_EN_TUBOOCK_FLY);
    }

    if (SkelAnime_Update(&self->skelAnime)) {
        if (DECR(self->afterAnimTimer) == 0) { // Back to doing nothing
            EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE1);
            self->idleAction = NULL;
        }
    }
}

/*
 * Enemy Idle3 Animation.
 * Looking around
 */
void EnSbeetle_IdleActionIdle3(EnSbeetle* self, PlayState* play) {
    if (Animation_OnFrame(&self->skelAnime, 7.0f) || Animation_OnFrame(&self->skelAnime, 39.0f)) {
        Audio_PlayActorSound2(&self->actor, NA_SE_EN_TEKU_WALK);
    }

    if (SkelAnime_Update(&self->skelAnime)) {
        if (DECR(self->afterAnimTimer) == 0) { // Back to doing nothing
            EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE1);
            self->idleAction = NULL;
        }
    }
}

/*
 * Ends up being unused.
 * Setup for a jump - see EnSbeetle_HopWithPlayerRot.
 */
void EnSbeetle_SetupHopWithPlayerRot(EnSbeetle* self, PlayState* play) {
    self->actor.speedXZ = 0.0f;
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_HOP);
    self->actionFunc = EnSbeetle_HopWithPlayerRot;
}

/*
 * Ends up being unused.
 * Jump and rotate towards the player midair.
 */
void EnSbeetle_HopWithPlayerRot(EnSbeetle* self, PlayState* play) {
    if (self->skelAnime.curFrame >= 7.0f) {
        Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer, 3, 4000);
        Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 6000);
    }
    if (SkelAnime_Update(&self->skelAnime)) {
        EnSbeetle_SetupThreatPlayer(self, play);
    }
}

void EnSbeetle_SetupThreatPlayer(EnSbeetle* self, PlayState* play) {
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE1);
    self->attackTimer = 40;
    self->playerLostTimer = ENSBEETLE_FORGET_TIME;
    self->actionFunc = EnSbeetle_ThreatPlayer;
}

void EnSbeetle_ThreatPlayer(EnSbeetle* self, PlayState* play) {
    SkelAnime_Update(&self->skelAnime);

    if (!EnSbeetle_HasLostPlayer(self, play)) { // Always reset the timer if player is in sight
        self->playerLostTimer = ENSBEETLE_FORGET_TIME;
    } else if (DECR(self->playerLostTimer) == 0) { // Player lost
        self->actor.speedXZ = 0.0f;
        EnSbeetle_SetupDoNothing(self, play);
        return;
    }

    // Rotate towards player
    Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer, 3, 3000);
    Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 5000);

    if (DECR(self->attackTimer) == 0) {
        if (Rand_ZeroOne() < 0.6f) {
            if (Rand_ZeroOne() < 0.6f) { // chance for normal attack
                EnSbeetle_SetupAttack(self, play);
            } else { // chance for swinging the pincers
                EnSbeetle_SetupSwingAttack(self, play);
            }
            return;
        }
        self->attackTimer = 40; // 2 seconds
    }
}

void EnSbeetle_SetupAttack(EnSbeetle* self, PlayState* play) {
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_ATTACK);
    self->actor.speedXZ = 0.0f;
    self->pincerLCollider.info.toucher.damage = 0x10;
    self->audioPlayed = false;
    self->actionFunc = EnSbeetle_Attack;
}

void EnSbeetle_Attack(EnSbeetle* self, PlayState* play) {
    CollisionCheck_SetAT(play, &play->colChkCtx, &self->pincerLCollider.base);

    if (self->skelAnime.curFrame < 20.0f) { // Rotate towards player
        Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer, 3, 3000);
        Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 5000);
    }

    if (Animation_OnFrame(&self->skelAnime, 24.0f) || Animation_OnFrame(&self->skelAnime, 25.0f)) {
        // Dash towards player
        self->actor.speedXZ = self->actor.xzDistToPlayer / 2;
        if (!self->audioPlayed) {
            Audio_PlayActorSound2(&self->actor, NA_SE_IT_SWORD_SWING_HARD);
            self->audioPlayed = true;
        }
    } else {
        self->actor.speedXZ = 0.0f;
    }

    if (SkelAnime_Update(&self->skelAnime)) { // Animation finished
        EnSbeetle_SetupHopAwayFromOrTowardsPlayer(self, play);
    }
}

/* --- This function was written by AI ---
 * Setup for the pincers
 */
void EnSbeetle_SetupSwingAttack(EnSbeetle* self, PlayState* play) {
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_SWING);

    self->actor.speedXZ = 0.0f;
    self->pincerLCollider.info.toucher.damage = 0x08;

    self->pincerState = ENSBEETLE_PINCER_WINDUP;
    self->pincerFlightTimer = 0;

    self->pincerLSpin = 0;
    self->pincerRSpin = 0;

    self->pincerLCollider.base.atFlags &= ~AT_HIT;
    self->pincerRCollider.base.atFlags &= ~AT_HIT;

    self->actionFunc = EnSbeetle_SwingAttack;
}

/* --- This function was written by AI ---
 */
void EnSbeetle_ThrowPincers(EnSbeetle* self, PlayState* play) {
    Player* player = GET_PLAYER(play);

    self->pincerLWorldPos = self->pincerLHomePos;
    self->pincerRWorldPos = self->pincerRHomePos;

    self->pincerTargetPos = player->actor.world.pos;
    self->pincerTargetPos.y += 30.0f;

    self->pincerFlightTimer = 0;
    self->pincerLSpin = 0;
    self->pincerRSpin = 0;
    self->pincerState = ENSBEETLE_PINCER_OUTBOUND;

    self->pincerLCollider.base.atFlags &= ~AT_HIT;
    self->pincerRCollider.base.atFlags &= ~AT_HIT;

    Audio_PlayActorSound2(&self->actor, NA_SE_IT_BOOMERANG_THROW);
}

/* --- This function was written by AI ---
 * Setup for the pincers
 */
void EnSbeetle_SwingAttack(EnSbeetle* self, PlayState* play) {
    s32 animationFinished;

    self->actor.speedXZ = 0.0f;

    if (self->pincerState == ENSBEETLE_PINCER_WINDUP) { // Rotate towards player
        Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer, 3, 3000);

        Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 5000);
    }

    animationFinished = SkelAnime_Update(&self->skelAnime);

    if ((self->pincerState == ENSBEETLE_PINCER_WINDUP) &&
        Animation_OnFrame(&self->skelAnime, ENSBEETLE_PINCER_THROW_FRAME)) {
        EnSbeetle_ThrowPincers(self, play);
    }

    if (animationFinished && (self->pincerState == ENSBEETLE_PINCER_ATTACHED)) {
        EnSbeetle_SetupHopAwayFromOrTowardsPlayer(self, play);
    }
}

void EnSbeetle_SetupHopAwayFromOrTowardsPlayer(EnSbeetle* self, PlayState* play) {
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_HOP);
    self->audioPlayed = false;
    self->playerDistAtSetup = self->actor.xzDistToPlayer;
    self->actionFunc = EnSbeetle_HopAwayFromOrTowardsPlayer;
}

void EnSbeetle_HopAwayFromOrTowardsPlayer(EnSbeetle* self, PlayState* play) {
    if (self->skelAnime.curFrame <= 6.0f || self->skelAnime.curFrame >= 15.0f) {
        // Rotate towards player before jumping
        self->actor.speedXZ = 0.0f;
        Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer, 3, 4000);
        Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 6000);
    } else {
        if (!self->audioPlayed) {
            Audio_PlayActorSound2(&self->actor, NA_SE_EN_RIZA_JUMP);
            self->audioPlayed = true;
        }
        if (self->playerDistAtSetup > 300.0f) { // Either jump towards the player
            self->actor.speedXZ = 12.0f;
        } else {
            self->actor.speedXZ = -12.0f; // Or away from the player
        }
    }

    if (SkelAnime_Update(&self->skelAnime)) {
        if (EnSbeetle_HasLostPlayer(self, play)) {
            EnSbeetle_SetupDoNothing(self, play);
        } else {
            EnSbeetle_SetupThreatPlayer(self, play);
        }
    }
}

void EnSbeetle_SetupStunned(EnSbeetle* self, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(self);
    self->actor.speedXZ = 0.0f;
    Audio_PlayActorSound2(&self->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Animation_PlayOnceSetSpeed(&self->skelAnime, &gScissorsBeetleSkelIdle1Anim, 0.0f);
    Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
    self->actionFunc = EnSbeetle_Stunned;
}

void EnSbeetle_Stunned(EnSbeetle* self, PlayState* play) {
    if (self->spawnIceTimer == 0) {
        if (self->actor.colorFilterTimer == 0) {
            EnSbeetle_SetupHopAwayFromOrTowardsPlayer(self, play);
            if (self->frozen) {
                Audio_PlayActorSound2(&self->actor, NA_SE_EV_ICE_BROKEN);
                self->frozen = false;
            }
        }
    }
}

void EnSbeetle_SetupHurt(EnSbeetle* self, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(self);
    self->damageTimer = 2;
    self->hurtboxCooldown = 40;
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_HURT);
    Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
    Actor_ApplyDamage(&self->actor);
    Audio_PlayActorSound2(&self->actor, NA_SE_EN_BUBLEWALK_AIM);
    self->actionFunc = EnSbeetle_Hurt;
}

void EnSbeetle_Hurt(EnSbeetle* self, PlayState* play) {
    if (SkelAnime_Update(&self->skelAnime)) {
        if (DECR(self->damageTimer) == 0) { // timer for seeing the Sbeetle taking damage
            EnSbeetle_SetupHopAwayFromOrTowardsPlayer(self, play);
        }
    }
}

void EnSbeetle_SetupDie(EnSbeetle* self, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(self);
    self->deathFreeze = 12;
    self->actor.speedXZ = 0.0f;
    self->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED; // Sbeetle not targetable anymore
    self->actor.shape.shadowAlpha = 0;
    Audio_PlayActorSound2(&self->actor, NA_SE_EN_BUBLEWALK_DEAD);
    Enemy_StartFinishingBlow(play, &self->actor);
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_DIE);
    self->actionFunc = EnSbeetle_Die;
}

void EnSbeetle_Die(EnSbeetle* self, PlayState* play) {
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    Vec3f effectVel = { 0.0f, 4.0f, 0.0f };
    Vec3f effectPos = self->actor.world.pos;

    if (SkelAnime_Update(&self->skelAnime)) {
        if (DECR(self->deathFreeze) == 0) {
            Math_StepToF(&self->actor.scale.x, 0.0f, 0.0084f); // Sbeetle shrinks in his scale while dying
            self->actor.scale.y = self->actor.scale.z = self->actor.scale.x;
            if (self->actor.scale.x <= 0.001f) { // Enemy not visible anymore
                effectPos.y += 10.0f;
                EffectSsDeadDb_Spawn(play, &effectPos, &effectVel, &zeroVec, 90, 0, 255, 255, 255, 255, 0, 0, 255, 1, 9,
                                     true);
                Item_DropCollectibleRandom(play, &self->actor, &self->actor.world.pos,
                                           0xE0); // The Sbeetle might drop some random collectibles
                Actor_Kill(&self->actor);
            }
        }
    }
}
