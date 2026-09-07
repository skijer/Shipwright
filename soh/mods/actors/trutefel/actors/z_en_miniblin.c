/*
 * File: z_en_miniblin.c (SoH port)
 * Description: Miniblin, similiar to the bokoblins in The Wind Waker. Tries stealing a red rupee from the player
 * Authors: @syeo501 (Model) @trueffel (Code) — ported to SoH (Skijer's NEI)
 *
 * ---- Port notes (modern OoT decomp -> SoH) --------------------------------------------
 * - Unity-#included into trutefel_enemies.cpp inside extern "C" (compiled as C++):
 *   `this` renamed to `self`, file-scope statics prefixed sMiniblin* (three actors share
 *   one TU), asset symbols cast explicitly.
 * - Actor_PlaySfx -> Audio_PlayActorSound2 | Audio_PlaySfxGeneral -> Audio_PlaySoundGeneral
 * - actor.speed -> actor.speedXZ | Actor_MoveXZGravity exists in SoH as-is.
 * - ACTOR_FLAG_0 -> ACTOR_FLAG_ATTENTION_ENABLED (same bit 1<<0).
 * - UPDBGCHECKINFO_FLAG_* / COLORFILTER_* don't exist in SoH -> local guarded defines.
 * - ActorInit/ACTOR_EN_MINIBLIN/OBJECT_MINIBLIN deleted: ActorDB registration
 *   (trutefel_actor_reg.cpp) supplies category/flags/objectId/instanceSize.
 * - gRupeeDL/gRupeeRedTex are OTR path strings in SoH's gameplay_keep.h; SEGMENTED_TO_VIRTUAL
 *   is a no-op and the gSPSegment/gSPDisplayList wrappers resolve the paths at draw time.
 * - Eye textures keep their original symbol names but are now const char[] OTR paths
 *   (object_miniblin_assets.inc.c) — passed straight to gSPSegment like vanilla SoH actors.
 */

#include "z_en_miniblin.h"

// Flag combos the registration (.cpp) also needs live there; in-file we only clear bit 0.

#ifndef UPDBGCHECKINFO_FLAG_0
#define UPDBGCHECKINFO_FLAG_0 (1 << 0) // check wall
#define UPDBGCHECKINFO_FLAG_2 (1 << 2) // check floor
#define UPDBGCHECKINFO_FLAG_3 (1 << 3) // check ceiling
#define UPDBGCHECKINFO_FLAG_4 (1 << 4) // check water
#endif

#ifndef COLORFILTER_COLORFLAG_RED
#define COLORFILTER_COLORFLAG_GRAY 0x8000
#define COLORFILTER_COLORFLAG_RED 0x4000
#define COLORFILTER_COLORFLAG_BLUE 0x0000
#define COLORFILTER_BUFFLAG_XLU 0x2000
#define COLORFILTER_BUFFLAG_OPA 0x0000
#endif

void EnMiniblin_Init(Actor* thisx, PlayState* play);
void EnMiniblin_Destroy(Actor* thisx, PlayState* play);
void EnMiniblin_Update(Actor* thisx, PlayState* play);
void EnMiniblin_Draw(Actor* thisx, PlayState* play);

s32 EnMiniblin_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx);
void EnMiniblin_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx);

void EnMiniblin_CheckDamage(EnMiniblin* self, PlayState* play);
void EnMiniblin_UpdateBgCheck(EnMiniblin* self, PlayState* play);
void EnMiniblin_SetupDoNothing(EnMiniblin* self, PlayState* play);
void EnMiniblin_DoNothing(EnMiniblin* self, PlayState* play);
void EnMiniblin_SetupApproachPlayer(EnMiniblin* self, PlayState* play);
void EnMiniblin_ApproachPlayer(EnMiniblin* self, PlayState* play);
void EnMiniblin_SetupTailAttack(EnMiniblin* self, PlayState* play);
void EnMiniblin_TailAttack(EnMiniblin* self, PlayState* play);
void EnMiniblin_SetupStunned(EnMiniblin* self, PlayState* play);
void EnMiniblin_Stunned(EnMiniblin* self, PlayState* play);
void EnMiniblin_SetupFlee(EnMiniblin* self, PlayState* play);
void EnMiniblin_Flee(EnMiniblin* self, PlayState* play);
void EnMiniblin_SetupDamage(EnMiniblin* self, PlayState* play);
void EnMiniblin_Damage(EnMiniblin* self, PlayState* play);
void EnMiniblin_SetupLaugh(EnMiniblin* self, PlayState* play);
void EnMiniblin_Laugh(EnMiniblin* self, PlayState* play);
void EnMiniblin_SetupDisappear(EnMiniblin* self, PlayState* play);
void EnMiniblin_Disappear(EnMiniblin* self, PlayState* play);
void EnMiniblin_SetupDie(EnMiniblin* self, PlayState* play);
void EnMiniblin_Die(EnMiniblin* self, PlayState* play);

// Runtime ActorDB id — filled by Trutefel_EnsureActorsRegistered(); -1 until then.
s16 gEnMiniblinId = -1;
// Instance size for the reg .cpp (struct only visible inside this TU).
size_t gEnMiniblinStructSize = sizeof(EnMiniblin);

static ColliderCylinderInit sMiniblinCylinderInit = {
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

static ColliderQuadInit sMiniblinQuadInit = {
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

typedef enum {
    /* 0 */ MINIBLIN_ANIMATION_IDLE,
    /* 1 */ MINIBLIN_ANIMATION_JUMP,
    /* 2 */ MINIBLIN_ANIMATION_TAILATTACK,
    /* 3 */ MINIBLIN_ANIMATION_DAMAGE,
    /* 4 */ MINIBLIN_ANIMATION_LAUGH,
    /* 5 */ MINIBLIN_ANIMATION_BOMBTHROW,
    /* 6 */ MINIBLIN_ANIMATION_DEATH,
} EnMiniblinAnimation;

static AnimationInfo sMiniblinAnimationInfo[] = {
    { &gMiniblinSkelIdleAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 3.0f },
    { &gMiniblinSkelJumpAnim, 4.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, 3.0f },
    { &gMiniblinSkelTailattackAnim, 1.5f, 0.0f, -1.0f, ANIMMODE_ONCE, 0.0f },
    { &gMiniblinSkelDamageAnim, 2.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 0.0f },
    { &gMiniblinSkelLaughAnim, 1.5f, 0.0f, -1.0f, ANIMMODE_ONCE, 0.0f },
    { &gMiniblinSkelBombthrowAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 0.0f },
    { &gMiniblinSkelDeathAnim, 3.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 0.0f },
};

typedef enum {
    /* 0 */ MINIBLIN_EYES_NORMAL,
    /* 1 */ MINIBLIN_EYES_HALFCLOSED,
    /* 2 */ MINIBLIN_EYES_CLOSED,
    /* 3 */ MINIBLIN_EYES_LAUGH,
    /* 4 */ MINIBLIN_EYES_HIT,
} EnMiniblinEyeList;

// OTR texture paths (const char[]) — the gSPSegment wrapper resolves them at draw time.
static void* sMiniblinEyeTextures[] = {
    (void*)gMiniblinSkel_eye_normal_rgba16, (void*)gMiniblinSkel_eye_halfclosed_rgba16,
    (void*)gMiniblinSkel_eye_closed_rgba16, (void*)gMiniblinSkel_eye_laugh_rgba16,
    (void*)gMiniblinSkel_eye_hit_rgba16,
};

typedef enum {
    /*  0 */ ENMINIBLIN_DMGEFF_NONE,
    /*  1 */ ENMINIBLIN_DMGEFF_STUN,
    /*  6 */ ENMINIBLIN_DMGEFF_ICE_MAGIC = 6,
    /* 13 */ ENMINIBLIN_DMGEFF_LIGHT_MAGIC = 13,
    /* 14 */ ENMINIBLIN_DMGEFF_FIRE,
} EnMiniblinDamageEffect;

static DamageTable sMiniblinDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, ENMINIBLIN_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, ENMINIBLIN_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(0, ENMINIBLIN_DMGEFF_STUN),
    /* Normal arrow  */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, ENMINIBLIN_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, ENMINIBLIN_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, ENMINIBLIN_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Ice arrow     */ DMG_ENTRY(4, ENMINIBLIN_DMGEFF_NONE),
    /* Light arrow   */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Unk arrow 1   */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(0, ENMINIBLIN_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(3, ENMINIBLIN_DMGEFF_ICE_MAGIC),
    /* Light magic   */ DMG_ENTRY(4, ENMINIBLIN_DMGEFF_LIGHT_MAGIC),
    /* Shield        */ DMG_ENTRY(0, ENMINIBLIN_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, ENMINIBLIN_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(1, ENMINIBLIN_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(4, ENMINIBLIN_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, ENMINIBLIN_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, ENMINIBLIN_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, ENMINIBLIN_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, ENMINIBLIN_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, ENMINIBLIN_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, ENMINIBLIN_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, ENMINIBLIN_DMGEFF_NONE),
};

// SoH CollisionCheckInfoInit2 = { health, cylRadius(s16), cylHeight(s16), cylYShift, mass }
// (positional: designated initializers out of order don't fly in this C++ TU)
static CollisionCheckInfoInit2 sMiniblinColChkInit = { 4, 25, 35, 0, MASS_HEAVY };

void EnMiniblin_SetupAction(EnMiniblin* self, EnMiniblinActionFunc actionFunc) {
    self->actionFunc = actionFunc;
}

void EnMiniblin_ChangeAnimation(EnMiniblin* self, s32 index) {
    Animation_ChangeByInfo(&self->skelAnime, sMiniblinAnimationInfo, index);
}

void EnMiniblin_ChangeEyes(EnMiniblin* self, s16 eyeIndex) {
    self->eyeIndex = eyeIndex;
}

void EnMiniblin_UpdateEyes(EnMiniblin* self) {
    // Eye blinking logic
    if (self->eyeIndex <= MINIBLIN_EYES_CLOSED) {
        if (DECR(self->blinkTimer) == 0) {
            self->eyeIndex++;
            if (self->eyeIndex >= 2) {
                self->blinkTimer = Rand_S16Offset(30, 30);
                self->eyeIndex = 0;
            }
        }
    }
}

void EnMiniblin_InitAndSetCollision(EnMiniblin* self, PlayState* play) {
    Collider_InitCylinder(play, &self->collider);
    Collider_SetCylinder(play, &self->collider, &self->actor, &sMiniblinCylinderInit);
    Collider_InitQuad(play, &self->quad);
    Collider_SetQuad(play, &self->quad, &self->actor, &sMiniblinQuadInit);
    CollisionCheck_SetInfo2(&self->actor.colChkInfo, &sMiniblinDamageTable, &sMiniblinColChkInit);
}

void EnMiniblin_Init(Actor* thisx, PlayState* play) {
    EnMiniblin* self = (EnMiniblin*)thisx;

    ActorShape_Init(&self->actor.shape, 0.0f, ActorShadow_DrawCircle, 100.0f);
    Actor_SetScale(&self->actor, 0.0035f);
    EnMiniblin_ChangeEyes(self, MINIBLIN_EYES_NORMAL);
    thisx->targetMode = 3;
    thisx->gravity = -1.0f;

    EnMiniblin_InitAndSetCollision(self, play);
    SkelAnime_InitFlex(play, &self->skelAnime, &gMiniblinSkel, NULL, self->jointTable, self->morphTable,
                       GMINIBLINSKEL_NUM_LIMBS);
    EnMiniblin_ChangeAnimation(self, MINIBLIN_ANIMATION_IDLE);
    EnMiniblin_SetupDoNothing(self, play);
}

void EnMiniblin_Destroy(Actor* thisx, PlayState* play) {
    EnMiniblin* self = (EnMiniblin*)thisx;

    Collider_DestroyCylinder(play, &self->collider);
    Collider_DestroyQuad(play, &self->quad);
}

void EnMiniblin_Update(Actor* thisx, PlayState* play) {
    EnMiniblin* self = (EnMiniblin*)thisx;

    EnMiniblin_CheckDamage(self, play);
    self->actionFunc(self, play);

    Actor_MoveXZGravity(&self->actor);
    EnMiniblin_UpdateBgCheck(self, play);
    EnMiniblin_UpdateEyes(self);

    if (self->actionFunc != EnMiniblin_Die) { // No need for colliders if the Miniblin is dead
        Collider_UpdateCylinder(&self->actor, &self->collider);

        if (DECR(self->hurtboxCooldown) == 0 && self->actionFunc != EnMiniblin_TailAttack &&
            self->actionFunc != EnMiniblin_Laugh && self->actionFunc != EnMiniblin_Disappear) {
            // Miniblin can only take damage by the player if not already hit or doing specific animations
            CollisionCheck_SetAC(play, &play->colChkCtx, &self->collider.base);
        }

        CollisionCheck_SetOC(play, &play->colChkCtx, &self->collider.base);
    }

    if (self->actionFunc == EnMiniblin_TailAttack) {
        // Miniblin can only damage the player when in attack mode
        CollisionCheck_SetAT(play, &play->colChkCtx, &self->quad.base);
    }
}

void EnMiniblin_Draw(Actor* thisx, PlayState* play) {
    EnMiniblin* self = (EnMiniblin*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x08,
               (uintptr_t)SEGMENTED_TO_VIRTUAL(sMiniblinEyeTextures[self->eyeIndex])); // Different eye textures

    SkelAnime_DrawFlexOpa(play, self->skelAnime.skeleton, self->skelAnime.jointTable, self->skelAnime.dListCount,
                          EnMiniblin_OverrideLimbDraw, EnMiniblin_PostLimbDraw, self);

    CLOSE_DISPS(play->state.gfxCtx);
}

s32 EnMiniblin_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    return false;
}

static Vec3f sMiniblinTailQuadVertex[4] = {
    { 0.0f, 0.0f, 0.0f },
    { 0.0f, 8000.0f, 0.0f },
    { 0.0f, 0.0f, 5000.0f },
    { 0.0f, 8000.0f, 5000.0f },
};

static Vec3f sMiniblinZeroVec = { 0.0f, 0.0f, 0.0f };

void EnMiniblin_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnMiniblin* self = (EnMiniblin*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    switch (limbIndex) {
        case GMINIBLINSKEL_TAILEND_LIMB: // The tail of the Miniblin can attack the player
            Matrix_MultVec3f(&sMiniblinTailQuadVertex[0], &self->quad.dim.quad[0]);
            Matrix_MultVec3f(&sMiniblinTailQuadVertex[1], &self->quad.dim.quad[1]);
            Matrix_MultVec3f(&sMiniblinTailQuadVertex[2], &self->quad.dim.quad[2]);
            Matrix_MultVec3f(&sMiniblinTailQuadVertex[3], &self->quad.dim.quad[3]);
            Collider_SetQuadVertices(&self->quad, &self->quad.dim.quad[0], &self->quad.dim.quad[1],
                                     &self->quad.dim.quad[2], &self->quad.dim.quad[3]);

            if (self->aboutToSteal == true) {
                // The miniblin stole a rupee of the player. Display the rupee on his tail

                Matrix_Push();

                Matrix_Scale(3.0f, 3.0f, 3.0f, MTXMODE_APPLY);
                Matrix_RotateX(2.0f, MTXMODE_APPLY);
                Matrix_RotateY(1.4f, MTXMODE_APPLY);
                Matrix_RotateZ(3.0f, MTXMODE_APPLY);
                Matrix_Translate(-500.0f, -700.0f, 450.0f, MTXMODE_APPLY);

                gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)"../z_en_miniblin.c", __LINE__),
                          G_MTX_MODELVIEW | G_MTX_LOAD);

                gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)SEGMENTED_TO_VIRTUAL((void*)gRupeeRedTex));
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gRupeeDL);
                Matrix_Pop();
            }
            break;
        case GMINIBLINSKEL_HAND_L_LIMB: // If the miniblin stole a rupee, he runs away with it in his left hand
            if (self->rupeeStolen == true) {
                Matrix_Push();

                Matrix_Scale(3.0f, 3.0f, 3.0f, MTXMODE_APPLY);
                Matrix_RotateX(2.0f, MTXMODE_APPLY);
                Matrix_RotateY(1.4f, MTXMODE_APPLY);
                Matrix_RotateZ(3.0f, MTXMODE_APPLY);
                Matrix_Translate(-500.0f, 200.0f, 100.0f, MTXMODE_APPLY);

                gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)"../z_en_miniblin.c", __LINE__),
                          G_MTX_MODELVIEW | G_MTX_LOAD);

                gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)SEGMENTED_TO_VIRTUAL((void*)gRupeeRedTex));
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gRupeeDL);
                Matrix_Pop();
            }
            break;
        case GMINIBLINSKEL_BODY_LIMB: // This is just for fixing the navi target position
            Matrix_MultVec3f(&sMiniblinZeroVec, &self->actor.focus.pos);
            break;
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void EnMiniblin_CheckDamage(EnMiniblin* self, PlayState* play) {
    if (self->collider.base.acFlags & AC_HIT) {
        self->collider.base.acFlags &= ~AC_HIT;
        self->hurtboxCooldown = 20;
        self->actor.speedXZ = 0.0f;

        if (self->actor.colChkInfo.damageEffect != ENMINIBLIN_DMGEFF_STUN) {
            EnMiniblin_SetupDamage(self, play);
        } else {
            // Stunning effect because of e.g. a deku nut
            Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
            Actor_ApplyDamage(&self->actor);
            EnMiniblin_SetupStunned(self, play);
        }

        if (self->actor.colChkInfo.health == 0) {
            EnMiniblin_SetupDie(self, play);
        }
    }
    if ((self->actor.bgCheckFlags & BGCHECKFLAG_WATER) && self->actionFunc != EnMiniblin_Die) {
        // Currently, the miniblin dies if he falls into a water box
        EnMiniblin_SetupDie(self, play);
    }
}

void EnMiniblin_UpdateBgCheck(EnMiniblin* self, PlayState* play) {
    Actor_UpdateBgCheckInfo(
        play, &self->actor, self->actor.colChkInfo.cylHeight, self->actor.colChkInfo.cylRadius,
        self->actor.colChkInfo.cylHeight,
        (UPDBGCHECKINFO_FLAG_0 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_3 | UPDBGCHECKINFO_FLAG_4));
}

void EnMiniblin_SetupDoNothing(EnMiniblin* self, PlayState* play) {
    self->actor.speedXZ = 0.0f;
    EnMiniblin_ChangeAnimation(self, MINIBLIN_ANIMATION_IDLE);
    EnMiniblin_ChangeEyes(self, MINIBLIN_EYES_NORMAL);
    EnMiniblin_SetupAction(self, EnMiniblin_DoNothing);
}

void EnMiniblin_DoNothing(EnMiniblin* self, PlayState* play) {
    // Idling around
    SkelAnime_Update(&self->skelAnime);
    if (self->actor.xzDistToPlayer < 280.0f) {
        // Miniblin spots the player
        EnMiniblin_SetupApproachPlayer(self, play);
    }
}

void EnMiniblin_SetupApproachPlayer(EnMiniblin* self, PlayState* play) {
    EnMiniblin_ChangeAnimation(self, MINIBLIN_ANIMATION_JUMP);
    EnMiniblin_SetupAction(self, EnMiniblin_ApproachPlayer);
}

void EnMiniblin_ApproachPlayer(EnMiniblin* self, PlayState* play) {
    SkelAnime_Update(&self->skelAnime);
    if (Animation_OnFrame(&self->skelAnime, 17.0f)) {
        // Optimal frame for playing the sound effect as he touches the ground
        Audio_PlayActorSound2(&self->actor, NA_SE_EN_TEKU_WALK);
    }

    if (self->skelAnime.curFrame < 18.0f) {
        // The miniblin shouldn't rotate or move when the feet are clearly on the ground
        Math_ApproachF(&self->actor.speedXZ, 20.0f / 3.0f, 0.5f, 2.0f);
        Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer, 3, 2000);
        Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 3000);
    } else {
        self->actor.speedXZ = 0.0f;
    }

    if (self->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        // Since the miniblin jumps towards the player,
        // stopping his velocity as soon as he touches
        // the ground looks more natural
        self->actor.velocity.y = 0.0f;
    }

    if (self->actor.xzDistToPlayer < 35.0f) {
        // The tail can now hit the player
        EnMiniblin_SetupTailAttack(self, play);
    }

    if (self->actor.xzDistToPlayer > 280.0f) {
        // Player is too far away to still follow him
        EnMiniblin_SetupDoNothing(self, play);
    }
}

void EnMiniblin_SetupTailAttack(EnMiniblin* self, PlayState* play) {
    self->actor.speedXZ = 0.0f;
    self->timer = 3;
    EnMiniblin_ChangeAnimation(self, MINIBLIN_ANIMATION_TAILATTACK);
    EnMiniblin_SetupAction(self, EnMiniblin_TailAttack);
}

void EnMiniblin_TailAttack(EnMiniblin* self, PlayState* play) {
    if (self->quad.base.atFlags & AT_HIT) {
        Audio_PlayActorSound2(&self->actor, NA_SE_EV_NALE_MAGIC);
        if (gSaveContext.rupees >= 20 && self->rupeeStolen == false) {
            // Miniblin only steals rupees if the player has enough or if he didn't already steal one

            if (Rand_ZeroOne() < 0.4f) {
                // ~40% chance for the Miniblin to steal a rupee

                Rupees_ChangeBy(-20); // currently, the miniblin is setup to only steal a red rupee
                self->aboutToSteal = true;
            }
        }
    }
    if (SkelAnime_Update(&self->skelAnime)) {
        if (DECR(self->timer) == 0) {
            if (self->aboutToSteal == true) {
                self->rupeeStolen = true;
                self->aboutToSteal = false;
            }
            EnMiniblin_SetupFlee(self, play);
        }
    }
}

void EnMiniblin_SetupStunned(EnMiniblin* self, PlayState* play) {
    self->actor.speedXZ = 0.0f;
    Audio_PlayActorSound2(&self->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Animation_PlayOnceSetSpeed(&self->skelAnime, &gMiniblinSkelIdleAnim, 0.0f);
    Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
    EnMiniblin_SetupAction(self, EnMiniblin_Stunned);
}

void EnMiniblin_Stunned(EnMiniblin* self, PlayState* play) {
    if (self->actor.colorFilterTimer == 0) {
        if (self->rupeeStolen == true) {
            // Miniblin continues to try fleeing if he already has a rupee
            EnMiniblin_SetupFlee(self, play);
        } else {
            // Miniblin will still try to get a rupee of the player
            EnMiniblin_SetupDoNothing(self, play);
        }
    }
}

void EnMiniblin_SetupFlee(EnMiniblin* self, PlayState* play) {
    static f32 sFleePitch = 1.5f;
    self->timer = 100;
    EnMiniblin_ChangeAnimation(self, MINIBLIN_ANIMATION_JUMP);
    Audio_PlaySoundGeneral(NA_SE_VO_IN_LOST, &self->actor.world.pos, 4, &sFleePitch, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
    EnMiniblin_SetupAction(self, EnMiniblin_Flee);
}

void EnMiniblin_Flee(EnMiniblin* self, PlayState* play) {
    SkelAnime_Update(&self->skelAnime);
    if (Animation_OnFrame(&self->skelAnime, 17.0f)) {
        Audio_PlayActorSound2(&self->actor, NA_SE_EN_TEKU_WALK);
    }

    if (self->skelAnime.curFrame < 18.0f) {
        Math_ApproachF(&self->actor.speedXZ, 25.0f / 3.0f, 0.5f, 2.0f);
        Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer + 0x8000, 3,
                       2000); // opposite direction of the yaw towards player
        Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 3000);
    } else {
        self->actor.speedXZ = 0.0f;
    }

    if (self->rupeeStolen == true) {
        if (DECR(self->timer) == 0) {
            // The Miniblin had enough time fleeing
            EnMiniblin_SetupLaugh(self, play);
        }
    }

    if (self->actor.xzDistToPlayer > 150.0f || (self->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
        // If the miniblin didn't get a rupee, he will try getting back to the player in order to steal one
        if (self->rupeeStolen == false) {
            EnMiniblin_SetupDoNothing(self, play);
        }
    }
}

void EnMiniblin_SetupDamage(EnMiniblin* self, PlayState* play) {
    self->damageTimer = 3;
    EnMiniblin_ChangeAnimation(self, MINIBLIN_ANIMATION_DAMAGE);
    EnMiniblin_ChangeEyes(self, MINIBLIN_EYES_HIT);
    Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
    Actor_ApplyDamage(&self->actor);
    Audio_PlayActorSound2(&self->actor, NA_SE_EN_STALKID_DAMAGE);
    EnMiniblin_SetupAction(self, EnMiniblin_Damage);
}

void EnMiniblin_Damage(EnMiniblin* self, PlayState* play) {
    if (SkelAnime_Update(&self->skelAnime)) {
        if (DECR(self->damageTimer) == 0) { // timer for seeing the Miniblin taking damage
            if (self->rupeeStolen == true) {
                // Miniblin already has a rupee and continues fleeing
                EnMiniblin_SetupFlee(self, play);
            } else {
                // Miniblin will continue trying to get a rupee
                EnMiniblin_SetupDoNothing(self, play);
            }
        }
    }
}

void EnMiniblin_SetupLaugh(EnMiniblin* self, PlayState* play) {
    static f32 sLaughPitch = 3.5f;
    static f32 sVolumeScale = 9.0f;
    self->actor.speedXZ = 0.0f;
    self->actor.shape.rot.y = self->actor.yawTowardsPlayer; // Miniblin rotates to the player and laughs in his face
    EnMiniblin_ChangeAnimation(self, MINIBLIN_ANIMATION_LAUGH);
    EnMiniblin_ChangeEyes(self, MINIBLIN_EYES_LAUGH);
    Audio_PlaySoundGeneral(NA_SE_EN_STAL_WARAU, &self->actor.world.pos, 4, &sLaughPitch, &sVolumeScale,
                           &gSfxDefaultReverb);
    EnMiniblin_SetupAction(self, EnMiniblin_Laugh);
}

void EnMiniblin_Laugh(EnMiniblin* self, PlayState* play) {
    if (SkelAnime_Update(&self->skelAnime)) {
        // The Miniblin successfully stole a rupee and despawns
        EnMiniblin_SetupDisappear(self, play);
    }
}

void EnMiniblin_SetupDisappear(EnMiniblin* self, PlayState* play) {
    self->actor.speedXZ = 0.0f;
    self->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED; // Actor not targetable anymore
    self->timer = 12;
    EnMiniblin_SetupAction(self, EnMiniblin_Disappear);
}

void EnMiniblin_Disappear(EnMiniblin* self, PlayState* play) {
    Math_StepToF(&self->actor.scale.x, 0.0f, 0.00034f); // Miniblin shrinks in his scale while despawning
    self->actor.scale.y = self->actor.scale.z = self->actor.scale.x;
    if (DECR(self->timer) == 0) {
        Actor_Kill(&self->actor);
    }
}

void EnMiniblin_SetupDie(EnMiniblin* self, PlayState* play) {
    self->timer = 12;
    self->deathTimer = 12;
    self->actor.speedXZ = 0.0f;
    self->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED; // Miniblin not targetable anymore
    self->actor.shape.shadowAlpha = 0;
    Audio_PlayActorSound2(&self->actor, NA_SE_EN_STALKID_DEAD);
    Enemy_StartFinishingBlow(play, &self->actor);
    EnMiniblin_ChangeAnimation(self, MINIBLIN_ANIMATION_DEATH);
    EnMiniblin_ChangeEyes(self, MINIBLIN_EYES_CLOSED);
    EnMiniblin_SetupAction(self, EnMiniblin_Die);
}

void EnMiniblin_Die(EnMiniblin* self, PlayState* play) {
    if (SkelAnime_Update(&self->skelAnime)) {
        if (DECR(self->timer) == 0) {
            if (self->deathTimer != 0) {
                self->deathTimer--;
            }
            Math_StepToF(&self->actor.scale.x, 0.0f, 0.00034f); // Miniblin shrinks in his scale while dying
            self->actor.scale.y = self->actor.scale.z = self->actor.scale.x;
            if (self->deathTimer == 0) {
                if (self->rupeeStolen == true) {
                    // The player gets his rupee back if the Miniblin had one stolen
                    Item_DropCollectible(play, &self->actor.world.pos, ITEM00_RUPEE_RED);
                }
                // The Miniblin might also drop some random collectibles
                Item_DropCollectibleRandom(play, &self->actor, &self->actor.world.pos, 0xE0);
                Actor_Kill(&self->actor);
            }
        }
    }
}
