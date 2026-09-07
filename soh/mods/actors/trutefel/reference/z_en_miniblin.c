/*
 * File: z_en_miniblin.c
 * Overlay: Ovl_En_Miniblin
 * Description: Miniblin, similiar to the bokoblins in The Wind Waker. Tries stealing a red rupee from the player
 * Authors: @syeo501 (Model) @trueffel (Code)
 */

#include "z_en_miniblin.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"

/** TODO:
 * Enemy should be able to throw bomb back at player (Old and unfunctional code still available)
 * Maybe the miniblin can try to steal different types of rupees, not only a red rupee
 */

#define FLAGS                                     \
    (ACTOR_FLAG_0 | ACTOR_FLAG_2 | ACTOR_FLAG_4 | \
     ACTOR_FLAG_9) // z-targetable, unfriendly actor, update outside uncull zone, hookshottable

void EnMiniblin_Init(Actor* thisx, PlayState* play);
void EnMiniblin_Destroy(Actor* thisx, PlayState* play);
void EnMiniblin_Update(Actor* thisx, PlayState* play);
void EnMiniblin_Draw(Actor* thisx, PlayState* play);

s32 EnMiniblin_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx);
void EnMiniblin_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx);

// Actor* EnMiniblin_FindBomb(EnMiniblin* this, PlayState* play);

void EnMiniblin_CheckDamage(EnMiniblin* this, PlayState* play);
void EnMiniblin_UpdateBgCheck(EnMiniblin* this, PlayState* play);
void EnMiniblin_SetupDoNothing(EnMiniblin* this, PlayState* play);
void EnMiniblin_DoNothing(EnMiniblin* this, PlayState* play);
void EnMiniblin_SetupApproachPlayer(EnMiniblin* this, PlayState* play);
void EnMiniblin_ApproachPlayer(EnMiniblin* this, PlayState* play);
void EnMiniblin_SetupTailAttack(EnMiniblin* this, PlayState* play);
void EnMiniblin_TailAttack(EnMiniblin* this, PlayState* play);
void EnMiniblin_SetupStunned(EnMiniblin* this, PlayState* play);
void EnMiniblin_Stunned(EnMiniblin* this, PlayState* play);
void EnMiniblin_SetupFlee(EnMiniblin* this, PlayState* play);
void EnMiniblin_Flee(EnMiniblin* this, PlayState* play);
void EnMiniblin_SetupDamage(EnMiniblin* this, PlayState* play);
void EnMiniblin_Damage(EnMiniblin* this, PlayState* play);
void EnMiniblin_SetupLaugh(EnMiniblin* this, PlayState* play);
void EnMiniblin_Laugh(EnMiniblin* this, PlayState* play);
void EnMiniblin_SetupDisappear(EnMiniblin* this, PlayState* play);
void EnMiniblin_Disappear(EnMiniblin* this, PlayState* play);
// void EnMiniblin_SetupMoveToBomb(EnMiniblin* this, PlayState* play);
// void EnMiniblin_MoveToBomb(EnMiniblin* this, PlayState* play);
// void EnMiniblin_SetupBombThrow(EnMiniblin* this, PlayState* play);
// void EnMiniblin_BombThrow(EnMiniblin* this, PlayState* play);
void EnMiniblin_SetupDie(EnMiniblin* this, PlayState* play);
void EnMiniblin_Die(EnMiniblin* this, PlayState* play);

ActorInit En_Miniblin_InitVars = {
    ACTOR_EN_MINIBLIN,  ACTORCAT_ENEMY,    FLAGS,           OBJECT_MINIBLIN, sizeof(EnMiniblin), EnMiniblin_Init,
    EnMiniblin_Destroy, EnMiniblin_Update, EnMiniblin_Draw,
};

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

static ColliderQuadInit sQuadInit = {
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

static AnimationInfo sAnimationInfo[] = {
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

static void* sEyeTextures[] = {
    gMiniblinSkel_eye_normal_rgba16, gMiniblinSkel_eye_halfclosed_rgba16, gMiniblinSkel_eye_closed_rgba16,
    gMiniblinSkel_eye_laugh_rgba16,  gMiniblinSkel_eye_hit_rgba16,
};

typedef enum {
    /*  0 */ ENMINIBLIN_DMGEFF_NONE,
    /*  1 */ ENMINIBLIN_DMGEFF_STUN,
    /*  6 */ ENMINIBLIN_DMGEFF_ICE_MAGIC = 6,
    /* 13 */ ENMINIBLIN_DMGEFF_LIGHT_MAGIC = 13,
    /* 14 */ ENMINIBLIN_DMGEFF_FIRE,
} EnMiniblinDamageEffect;

static DamageTable sDamageTable[] = {
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

static CollisionCheckInfoInit2 sColChkInit = {
    .health = 4, .mass = MASS_HEAVY, .cylHeight = 35.0f, .cylRadius = 25.0f
};

void EnMiniblin_SetupAction(EnMiniblin* this, EnMiniblinActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void EnMiniblin_ChangeAnimation(EnMiniblin* this, s32 index) {
    Animation_ChangeByInfo(&this->skelAnime, sAnimationInfo, index);
}

void EnMiniblin_ChangeEyes(EnMiniblin* this, s16 eyeIndex) {
    this->eyeIndex = eyeIndex;
}

void EnMiniblin_UpdateEyes(EnMiniblin* this) {
    // Eye blinking logic
    if (this->eyeIndex <= MINIBLIN_EYES_CLOSED) {
        if (DECR(this->blinkTimer) == 0) {
            this->eyeIndex++;
            if (this->eyeIndex >= 2) {
                this->blinkTimer = Rand_S16Offset(30, 30);
                this->eyeIndex = 0;
            }
        }
    }
}

void EnMiniblin_InitAndSetCollision(EnMiniblin* this, PlayState* play) {
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    Collider_InitQuad(play, &this->quad);
    Collider_SetQuad(play, &this->quad, &this->actor, &sQuadInit);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, sDamageTable, &sColChkInit);
}

void EnMiniblin_Init(Actor* thisx, PlayState* play) {
    EnMiniblin* this = (EnMiniblin*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 100.0f);
    Actor_SetScale(&this->actor, 0.0035f);
    EnMiniblin_ChangeEyes(this, MINIBLIN_EYES_NORMAL);
    thisx->targetMode = 3;
    thisx->gravity = -1.0f;

    // this->bombActor = NULL;

    EnMiniblin_InitAndSetCollision(this, play);
    SkelAnime_InitFlex(play, &this->skelAnime, &gMiniblinSkel, NULL, this->jointTable, this->morphTable,
                       GMINIBLINSKEL_NUM_LIMBS);
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_IDLE);
    EnMiniblin_SetupDoNothing(this, play);
}

void EnMiniblin_Destroy(Actor* thisx, PlayState* play) {
    EnMiniblin* this = (EnMiniblin*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    Collider_DestroyQuad(play, &this->quad);
}

void EnMiniblin_Update(Actor* thisx, PlayState* play) {
    EnMiniblin* this = (EnMiniblin*)thisx;
    // Actor* bomb;

    EnMiniblin_CheckDamage(this, play);
    this->actionFunc(this, play);

    Actor_MoveXZGravity(&this->actor);
    EnMiniblin_UpdateBgCheck(this, play);
    EnMiniblin_UpdateEyes(this);

    /*
    bomb = EnMiniblin_FindBomb(this, play);
    if (bomb != NULL) {
        this->bombActor = bomb;
    } else {
        this->bombActor = NULL;
        this->actor.child = NULL;
    }

    if (this->bombActor != NULL) {
        EnMiniblin_SetupMoveToBomb(this, play);
    }
    */

    if (this->actionFunc != EnMiniblin_Die) { // No need for colliders if the Miniblin is dead
        Collider_UpdateCylinder(&this->actor, &this->collider);

        if (DECR(this->hurtboxCooldown) == 0 && this->actionFunc != EnMiniblin_TailAttack &&
            this->actionFunc != EnMiniblin_Laugh && this->actionFunc != EnMiniblin_Disappear) {
            // Miniblin can only take damage by the player if not already hit or doing specific animations
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }

        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }

    if (this->actionFunc == EnMiniblin_TailAttack) {
        // Miniblin can only damage the player when in attack mode
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->quad.base);
    }
}

void EnMiniblin_Draw(Actor* thisx, PlayState* play) {
    EnMiniblin* this = (EnMiniblin*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[this->eyeIndex])); // Different eye textures

    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          EnMiniblin_OverrideLimbDraw, EnMiniblin_PostLimbDraw, this);

    CLOSE_DISPS(play->state.gfxCtx);
}

s32 EnMiniblin_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    return false;
}

static Vec3f sTailQuadVertex[4] = {
    { 0.0f, 0.0f, 0.0f },
    { 0.0f, 8000.0f, 0.0f },
    { 0.0f, 0.0f, 5000.0f },
    { 0.0f, 8000.0f, 5000.0f },
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

void EnMiniblin_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnMiniblin* this = (EnMiniblin*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    switch (limbIndex) {
        case GMINIBLINSKEL_TAILEND_LIMB: // The tail of the Miniblin can attack the player
            Matrix_MultVec3f(&sTailQuadVertex[0], &this->quad.dim.quad[0]);
            Matrix_MultVec3f(&sTailQuadVertex[1], &this->quad.dim.quad[1]);
            Matrix_MultVec3f(&sTailQuadVertex[2], &this->quad.dim.quad[2]);
            Matrix_MultVec3f(&sTailQuadVertex[3], &this->quad.dim.quad[3]);
            Collider_SetQuadVertices(&this->quad, &this->quad.dim.quad[0], &this->quad.dim.quad[1],
                                     &this->quad.dim.quad[2], &this->quad.dim.quad[3]);

            if (this->aboutToSteal == true) {
                // The miniblin stole a rupee of the player. Display the rupee on his tail

                Matrix_Push();

                Matrix_Scale(3.0f, 3.0f, 3.0f, MTXMODE_APPLY);
                Matrix_RotateX(2.0f, MTXMODE_APPLY);
                Matrix_RotateY(1.4f, MTXMODE_APPLY);
                Matrix_RotateZ(3.0f, MTXMODE_APPLY);
                Matrix_Translate(-500.0f, -700.0f, 450.0f, MTXMODE_APPLY);

                gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, "../z_en_miniblin.c", __LINE__),
                          G_MTX_MODELVIEW | G_MTX_LOAD);

                gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gRupeeRedTex));
                gSPDisplayList(POLY_OPA_DISP++, gRupeeDL);
                Matrix_Pop();
            }
            break;
        case GMINIBLINSKEL_HAND_L_LIMB: // If the miniblin stole a rupee, he runs away with it in his left hand
            if (this->rupeeStolen == true) {
                Matrix_Push();

                Matrix_Scale(3.0f, 3.0f, 3.0f, MTXMODE_APPLY);
                Matrix_RotateX(2.0f, MTXMODE_APPLY);
                Matrix_RotateY(1.4f, MTXMODE_APPLY);
                Matrix_RotateZ(3.0f, MTXMODE_APPLY);
                Matrix_Translate(-500.0f, 200.0f, 100.0f, MTXMODE_APPLY);

                gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, "../z_en_miniblin.c", __LINE__),
                          G_MTX_MODELVIEW | G_MTX_LOAD);

                gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gRupeeRedTex));
                gSPDisplayList(POLY_OPA_DISP++, gRupeeDL);
                Matrix_Pop();
            }
            break;
        case GMINIBLINSKEL_BODY_LIMB: // This is just for fixing the navi target position
            Matrix_MultVec3f(&sZeroVec, &this->actor.focus.pos);
            break;
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void EnMiniblin_CheckDamage(EnMiniblin* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        this->hurtboxCooldown = 20;
        this->actor.speed = 0.0f;

        if (this->actor.colChkInfo.damageEffect != ENMINIBLIN_DMGEFF_STUN) {
            EnMiniblin_SetupDamage(this, play);
        } else {
            // Stunning effect because of e.g. a deku nut
            Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
            Actor_ApplyDamage(&this->actor);
            EnMiniblin_SetupStunned(this, play);
        }

        if (this->actor.colChkInfo.health == 0) {
            EnMiniblin_SetupDie(this, play);
        }
    }
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_WATER) && this->actionFunc != EnMiniblin_Die) {
        // Currently, the miniblin dies if he falls into a water box
        EnMiniblin_SetupDie(this, play);
    }
}

void EnMiniblin_UpdateBgCheck(EnMiniblin* this, PlayState* play) {
    Actor_UpdateBgCheckInfo(
        play, &this->actor, this->actor.colChkInfo.cylHeight, this->actor.colChkInfo.cylRadius,
        this->actor.colChkInfo.cylHeight,
        (UPDBGCHECKINFO_FLAG_0 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_3 | UPDBGCHECKINFO_FLAG_4));
}

/*
Actor* EnMiniblin_FindBomb(EnMiniblin* this, PlayState* play) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_EXPLOSIVE].head;

    while (actor != NULL) {
        if (actor->params != 0 || actor->parent != NULL) {
            actor = actor->next;
            continue;
        }

        if (actor->id != ACTOR_EN_BOM) {
            actor = actor->next;
            continue;
        }

        if (Actor_WorldDistXYZToActor(&this->actor, actor) > 280.0f) {
            actor = actor->next;
            continue;
        }

        return actor;
    }
    return NULL;
}
*/

void EnMiniblin_SetupDoNothing(EnMiniblin* this, PlayState* play) {
    this->actor.speed = 0.0f;
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_IDLE);
    EnMiniblin_ChangeEyes(this, MINIBLIN_EYES_NORMAL);
    EnMiniblin_SetupAction(this, EnMiniblin_DoNothing);
}

void EnMiniblin_DoNothing(EnMiniblin* this, PlayState* play) {
    // Idling around
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.xzDistToPlayer < 280.0f) {
        // Miniblin spots the player
        EnMiniblin_SetupApproachPlayer(this, play);
    }
}

void EnMiniblin_SetupApproachPlayer(EnMiniblin* this, PlayState* play) {
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_JUMP);
    EnMiniblin_SetupAction(this, EnMiniblin_ApproachPlayer);
}

void EnMiniblin_ApproachPlayer(EnMiniblin* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 17.0f)) {
        // Optimal frame for playing the sound effect as he touches the ground
        Actor_PlaySfx(&this->actor, NA_SE_EN_TEKU_WALK);
    }

    if (this->skelAnime.curFrame < 18.0f) {
        // The miniblin shouldn't rotate or move when the feet are clearly on the ground
        Math_ApproachF(&this->actor.speed, 20.0f / 3.0f, 0.5f, 2.0f);
        Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 2000);
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 3000);
    } else {
        this->actor.speed = 0.0f;
    }

    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        // Since the miniblin jumps towards the player,
        // stopping his velocity as soon as he touches
        // the ground looks more natural
        this->actor.velocity.y = 0.0f;
    }

    if (this->actor.xzDistToPlayer < 35.0f) {
        // The tail can now hit the player
        EnMiniblin_SetupTailAttack(this, play);
    }

    if (this->actor.xzDistToPlayer > 280.0f) {
        // Player is too far away to still follow him
        EnMiniblin_SetupDoNothing(this, play);
    }
}

void EnMiniblin_SetupTailAttack(EnMiniblin* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->timer = 3;
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_TAILATTACK);
    EnMiniblin_SetupAction(this, EnMiniblin_TailAttack);
}

void EnMiniblin_TailAttack(EnMiniblin* this, PlayState* play) {
    if (this->quad.base.atFlags & AT_HIT) {
        Actor_PlaySfx(&this->actor, NA_SE_EV_NALE_MAGIC);
        if (gSaveContext.save.info.playerData.rupees >= 20 && this->rupeeStolen == false) {
            // Miniblin only steals rupees if the player has enough or if he didn't already steal one

            if (Rand_ZeroOne() < 0.4f) {
                // 30% chance for the Miniblin to steal a rupee

                Rupees_ChangeBy(
                    -20); // currently, the miniblin is setup to only steal a red rupee. This could be randomized
                this->aboutToSteal = true;
            }
        }
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        if (DECR(this->timer) == 0) {
            if (this->aboutToSteal == true) {
                this->rupeeStolen = true;
                this->aboutToSteal = false;
            }
            EnMiniblin_SetupFlee(this, play);
        }
    }
}

void EnMiniblin_SetupStunned(EnMiniblin* this, PlayState* play) {
    this->actor.speed = 0.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Animation_PlayOnceSetSpeed(&this->skelAnime, &gMiniblinSkelIdleAnim, 0.0f);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
    EnMiniblin_SetupAction(this, EnMiniblin_Stunned);
}

void EnMiniblin_Stunned(EnMiniblin* this, PlayState* play) {
    if (this->actor.colorFilterTimer == 0) {
        if (this->rupeeStolen == true) {
            // Miniblin continues to try fleeing if he already has a rupee
            EnMiniblin_SetupFlee(this, play);
        } else {
            // Miniblin will still try to get a rupee of the player
            // EnMiniblin_DoNothing will immediately switch to
            // EnMiniblin_ApproachPlayer if the player is in the near
            EnMiniblin_SetupDoNothing(this, play);
        }
    }
}

void EnMiniblin_SetupFlee(EnMiniblin* this, PlayState* play) {
    static f32 sFleePitch = 1.5f;
    this->timer = 100;
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_JUMP);
    Audio_PlaySfxGeneral(NA_SE_VO_IN_LOST, &this->actor.world.pos, 4, &sFleePitch, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
    EnMiniblin_SetupAction(this, EnMiniblin_Flee);
}

void EnMiniblin_Flee(EnMiniblin* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 17.0f)) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_TEKU_WALK);
    }

    if (this->skelAnime.curFrame < 18.0f) {
        Math_ApproachF(&this->actor.speed, 25.0f / 3.0f, 0.5f, 2.0f);
        Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer + 0x8000, 3,
                       2000); // opposite direction of the yaw towards player
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 3000);
    } else {
        this->actor.speed = 0.0f;
    }

    if (this->rupeeStolen == true) {
        if (DECR(this->timer) == 0) {
            // The Miniblin had enough time fleeing
            EnMiniblin_SetupLaugh(this, play);
        }
    }

    if (this->actor.xzDistToPlayer > 150.0f || (this->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
        // If the miniblin didn't get a rupee, he will try getting back to the player in order to steal one
        if (this->rupeeStolen == false) {
            EnMiniblin_SetupDoNothing(
                this,
                play); // this can also switch immediately to EnMiniblin_ApproachPlayer if the player is in the near
        }
    }
}

void EnMiniblin_SetupDamage(EnMiniblin* this, PlayState* play) {
    this->damageTimer = 3;
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_DAMAGE);
    EnMiniblin_ChangeEyes(this, MINIBLIN_EYES_HIT);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
    Actor_ApplyDamage(&this->actor);
    Actor_PlaySfx(&this->actor, NA_SE_EN_STALKID_DAMAGE);
    EnMiniblin_SetupAction(this, EnMiniblin_Damage);
}

void EnMiniblin_Damage(EnMiniblin* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        if (DECR(this->damageTimer) == 0) { // timer for seeing the Miniblin taking damage
            if (this->rupeeStolen == true) {
                // Miniblin already has a rupee and continues fleeing
                EnMiniblin_SetupFlee(this, play);
            } else {
                // Miniblin will continue trying to get a rupee
                EnMiniblin_SetupDoNothing(this, play);
            }
        }
    }
}

void EnMiniblin_SetupLaugh(EnMiniblin* this, PlayState* play) {
    static f32 sLaughPitch = 3.5f;
    static f32 sVolumeScale = 9.0f;
    this->actor.speed = 0.0f;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer; // Miniblin rotates to the player and laughs in his face
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_LAUGH);
    EnMiniblin_ChangeEyes(this, MINIBLIN_EYES_LAUGH);
    Audio_PlaySfxGeneral(NA_SE_EN_STAL_WARAU, &this->actor.world.pos, 4, &sLaughPitch, &sVolumeScale,
                         &gSfxDefaultReverb);
    EnMiniblin_SetupAction(this, EnMiniblin_Laugh);
}

void EnMiniblin_Laugh(EnMiniblin* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        // The Miniblin successfully stealed a rupee and despawns
        EnMiniblin_SetupDisappear(this, play);
    }
}

void EnMiniblin_SetupDisappear(EnMiniblin* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_0; // Actor not targetable anymore
    this->timer = 12;
    EnMiniblin_SetupAction(this, EnMiniblin_Disappear);
}

void EnMiniblin_Disappear(EnMiniblin* this, PlayState* play) {
    Math_StepToF(&this->actor.scale.x, 0.0f, 0.00034f); // Miniblin shrinks in his scale while despawning
    this->actor.scale.y = this->actor.scale.z = this->actor.scale.x;
    if (DECR(this->timer) == 0) {
        Actor_Kill(&this->actor);
    }
}

/*
void EnMiniblin_SetupMoveToBomb(EnMiniblin* this, PlayState* play) {
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_JUMP);
    EnMiniblin_ChangeEyes(this, MINIBLIN_EYES_NORMAL);
    EnMiniblin_SetupAction(this, EnMiniblin_MoveToBomb);
}

void EnMiniblin_MoveToBomb(EnMiniblin* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    if (this->skelAnime.curFrame < 18.0f) {
        Math_ApproachF(&this->actor.speed, 20.0f / 3.0f, 0.5f, 2.0f);
        Math_ApproachS(&this->actor.world.rot.y, Actor_WorldYawTowardActor(&this->actor, this->bombActor), 3, 2000);
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 3000);
    } else {
        this->actor.speed = 0.0f;
    }
    if (Actor_WorldDistXZToActor(&this->actor, this->bombActor) < 35.0f) {
        EnMiniblin_SetupBombThrow(this, play);
    }
    if (this->bombActor == NULL) {
        this->actor.child = NULL;
        EnMiniblin_SetupDoNothing(this, play);
    }
}

void EnMiniblin_SetupBombThrow(EnMiniblin* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->actor.child = this->bombActor;
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_BOMBTHROW);
    EnMiniblin_SetupAction(this, EnMiniblin_BombThrow); // big issues here
}

void EnMiniblin_BombThrow(EnMiniblin* this, PlayState* play) {
    f32 curFrame = this->skelAnime.curFrame;
    SkelAnime_Update(&this->skelAnime);

    if (curFrame > 10.0f && curFrame < 45.0f) {
        Math_Vec3f_Copy(&this->bombActor->world.pos, &this->actor.world.pos);
        Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 2000);
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 3000);
    } else if (curFrame == 45.0f) {
        this->actor.child = NULL;
    }

    if (this->bombActor == NULL) {
        EnMiniblin_SetupDoNothing(this, play);
    }
}
*/

void EnMiniblin_SetupDie(EnMiniblin* this, PlayState* play) {
    this->timer = 12;
    this->deathTimer = 12;
    this->actor.speed = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_0; // Miniblin not targetable anymore
    this->actor.shape.shadowAlpha = 0;
    Actor_PlaySfx(&this->actor, NA_SE_EN_STALKID_DEAD);
    Enemy_StartFinishingBlow(play, &this->actor);
    EnMiniblin_ChangeAnimation(this, MINIBLIN_ANIMATION_DEATH);
    EnMiniblin_ChangeEyes(this, MINIBLIN_EYES_CLOSED);
    EnMiniblin_SetupAction(this, EnMiniblin_Die);
}

void EnMiniblin_Die(EnMiniblin* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        if (DECR(this->timer) == 0) {
            if (this->deathTimer != 0) {
                this->deathTimer--;
            }
            Math_StepToF(&this->actor.scale.x, 0.0f, 0.00034f); // Miniblin shrinks in his scale while dying
            this->actor.scale.y = this->actor.scale.z = this->actor.scale.x;
            if (this->deathTimer == 0) {
                if (this->rupeeStolen == true) {
                    Item_DropCollectible(
                        play, &this->actor.world.pos,
                        ITEM00_RUPEE_RED); // The player gets his rupee back if the Miniblin had one stolen
                }
                Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos,
                                           0xE0); // The Miniblin might also drop some random collectibles
                Actor_Kill(&this->actor);
            }
        }
    }
}
