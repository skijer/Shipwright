#include "z_en_m_thunder.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "mods/transformation_masks/transformation_masks.h"
#include "mods/extended_equipment.h"
#include "overlays/effects/ovl_Effect_Ss_Blast/z_eff_ss_blast.h"

#define FLAGS 0

// Sword beam params: bit 7 in lower byte signals sword beam mode
#define EN_M_THUNDER_SWORD_BEAM_FLAG 0x80

void EnMThunder_Init(Actor* thisx, PlayState* play);
void EnMThunder_Destroy(Actor* thisx, PlayState* play);
void EnMThunder_Update(Actor* thisx, PlayState* play);
void EnMThunder_Draw(Actor* thisx, PlayState* play);

void EnMThunder_AdjustEnvLights(PlayState* play, f32 intensity);
void EnMThunder_ChargingSpinAttack(EnMThunder* this, PlayState* play);
void EnMThunder_SpinAttacking(EnMThunder* this, PlayState* play);
static void EnMThunder_SwordBeamAction(EnMThunder* this, PlayState* play);

const ActorInit En_M_Thunder_InitVars = {
    ACTOR_EN_M_THUNDER,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(EnMThunder),
    (ActorFunc)EnMThunder_Init,
    (ActorFunc)EnMThunder_Destroy,
    (ActorFunc)EnMThunder_Update,
    (ActorFunc)EnMThunder_Draw,
    NULL,
};

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000001, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 200, 200, 0, { 0, 0, 0 } },
};

// ---------------------------------------------------------------------------
// Gerudo Dual Blades: her charge release is not the ring around Link, it is a THIRD
// of that cylinder thrown forward out of the blades. En_M_Thunder's spin DLs are a
// cylinder textured on its outer wall only (the caps are never drawn), so this is the
// same surface, cut to 120 degrees and rebuilt here: outer faces, one texture, 1.5x
// the radius of a normal charge. The geometry is procedural — a strip written into the
// frame's graphics arena, so it costs no asset and no object. It lives in the actor's
// local space, which every EnMThunder DL enters through Matrix_Scale(0.02), so the
// actor's own scale ramp (0 -> targetScale) grows it for free. Skijer's NEI
// ---------------------------------------------------------------------------
#define GERUDO_WEDGE_UNIT 0.02f // the Matrix_Scale every EnMThunder DL applies
// Vanilla's ring sits at a world radius of scale*25, i.e. 1250 local. x1.5 = 1875.
#define GERUDO_WEDGE_RADIUS 1875.0f
#define GERUDO_WEDGE_HEIGHT 900.0f // wall height in local units
#define GERUDO_WEDGE_ARC 0x5555    // a third of a turn (0x10000 / 3)
#define GERUDO_WEDGE_SEGMENTS 10
#define GERUDO_WEDGE_ARM_CAP 24 // frames the throw will wait for its 2/3 mark, at most
#define GERUDO_WEDGE_WORLD_RADIUS(scale) ((GERUDO_WEDGE_RADIUS * GERUDO_WEDGE_UNIT) * (scale))

static u32 sSpinAttackDmgFlags[] = { 0x01000000, 0x00400000, 0x00800000 };
static u32 sJumpAttackDmgFlags[] = { 0x08000000, 0x02000000, 0x04000000 };

static u16 sSfxIds[] = {
    NA_SE_IT_ROLLING_CUT_LV2,
    NA_SE_IT_ROLLING_CUT_LV1,
    NA_SE_IT_ROLLING_CUT_LV2,
    NA_SE_IT_ROLLING_CUT_LV1,
};

void EnMThunder_SetupAction(EnMThunder* this, EnMThunderActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void EnMThunder_Init(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    EnMThunder* this = (EnMThunder*)thisx;
    Player* player = GET_PLAYER(play);

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);

    // Sword beam mode: spawned by FD Z-target + B attack.
    // From MM z_en_m_thunder.c EnMThunder_Init lines 169-184.
    // Uses OOT struct fields mapped to MM fields:
    //   spinAttackTimer    = lightColorFrac (lifetime: 1.0 → 0.0 at 0.05/frame = 20 frames)
    //   spinAttackAlpha    = alphaFrac (alpha for draw: derived from lightColorFrac)
    //   spinTrailTexScroll = scroll (texture animation counter)
    //   dimmingIntensity   = scaleTarget (12.0 in MM, beam grows to this)
    if (this->actor.params & EN_M_THUNDER_SWORD_BEAM_FLAG) {
        // MM EnMThunder_Init line 124: shape.rot.y = player.shape.rot.y + 0x8000.
        // Without this, movement formula (-80 * sin(shape.rot.y)) fires beam backward.
        this->actor.shape.rot.y = player->actor.shape.rot.y + 0x8000;
        // MM EnMThunder_Init line 125: shape.rot.x = -world.rot.x. The spawn passed the
        // pitch as rotX so BOTH world.rot.x AND shape.rot.x = +beamPitch — drawing the
        // crescent disc tilted edge-on, which reads as a "cone". Negate it so the flat
        // disc faces the travel direction like MM (movement still uses world.rot.x).
        this->actor.shape.rot.x = -this->actor.world.rot.x;
        this->attackStrength = 2; // Sword beam type
        this->isUsingMagic = 0;   // No magic tracking
        // MM: scale starts at 0, ramps to scaleTarget (12) via Math_SmoothStepToF
        Actor_SetScale(&this->actor, 0.0f);
        this->dimmingIntensity = 12.0f;  // scaleTarget (MM line 173)
        this->spinAttackTimer = 1.0f;    // lightColorFrac starts at 1.0 (MM line 184)
        this->spinAttackAlpha = 1.0f;    // alphaFrac starts at 1.0
        this->spinTrailTexScroll = 0.0f; // scroll counter
        this->followPlayerTimer = 1;     // timer (MM line 172)
        // Collider: DMG_SWORD_BEAM (0x02000000). Damage = 4 to match MM's FD sword beam (sDamages
        // max). Enemies that read CollisionCheck_GetSwordDamage get the FD value there; this
        // toucher.damage covers the few that read it directly.
        this->collider.info.toucher.dmgFlags = 0x02000000; // DMG_SWORD_BEAM
        this->collider.info.toucher.damage = 4;
        this->collider.dim.height = 60;
        this->collider.dim.yShift = -30;
        this->actor.room = -1;
        // Light (MM line 184: lightColorFrac = 1.0)
        Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                                  this->actor.world.pos.z, 255, 255, 100, 800);
        this->lightNode = LightContext_InsertLight(play, &play->lightCtx, &this->lightInfo);
        // SFX (MM line 181)
        Audio_PlaySoundGeneral(NA_SE_IT_ROLLING_CUT_LV1, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        EnMThunder_SetupAction(this, EnMThunder_SwordBeamAction);
        return;
    }

    this->swordType = (this->actor.params & 0xFF) - 1;
    Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                              this->actor.world.pos.z, 255, 255, 255, 0);
    this->lightNode = LightContext_InsertLight(play, &play->lightCtx, &this->lightInfo);
    this->collider.dim.radius = 0;
    this->collider.dim.height = 40;
    this->collider.dim.yShift = -20;
    this->followPlayerTimer = 8;
    this->spinTrailTexScroll = 0.0f;
    this->actor.world.pos = player->bodyPartsPos[0];
    this->spinAttackTimer = 0.0f;
    this->dimmingIntensity = 0.0f;
    this->actor.shape.rot.y = player->actor.shape.rot.y + 0x8000;
    this->actor.room = -1;
    Actor_SetScale(&this->actor, 0.1f);
    this->isUsingMagic = 0;

    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) {
        if (!gSaveContext.isMagicAcquired || (gSaveContext.magicState != MAGIC_STATE_IDLE) ||
            (((this->actor.params & 0xFF00) >> 8) &&
             !(Magic_RequestChange(play, (this->actor.params & 0xFF00) >> 8, MAGIC_CONSUME_NOW)))) {
            Audio_PlaySoundGeneral(NA_SE_IT_ROLLING_CUT, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            Audio_PlaySoundGeneral(NA_SE_IT_SWORD_SWING_HARD, &player->actor.projectedPos, 4,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            Actor_Kill(&this->actor);
            return;
        }

        player->stateFlags2 &= ~PLAYER_STATE2_SPIN_ATTACKING;
        this->isUsingMagic = 1;
        this->collider.info.toucher.dmgFlags = sSpinAttackDmgFlags[this->swordType];
        this->attackStrength = 1;
        this->targetScale = ((this->swordType == 1) ? 2 : 4);
        EnMThunder_SetupAction(this, EnMThunder_SpinAttacking);
        this->followPlayerTimer = 8;
        Audio_PlaySoundGeneral(NA_SE_IT_ROLLING_CUT_LV1, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        this->spinAttackTimer = 1.0f;
    } else {
        EnMThunder_SetupAction(this, EnMThunder_ChargingSpinAttack);
    }
    this->actor.child = NULL;
}

void EnMThunder_Destroy(Actor* thisx, PlayState* play) {
    EnMThunder* this = (EnMThunder*)thisx;

    if (this->isUsingMagic != 0) {
        Magic_Reset(play);
    }

    Collider_DestroyCylinder(play, &this->collider);
    EnMThunder_AdjustEnvLights(play, 0.0f);
    LightContext_RemoveLight(play, &play->lightCtx, this->lightNode);
}

void EnMThunder_AdjustEnvLights(PlayState* play, f32 intensity) {
    Environment_AdjustLights(play, intensity, 850.0f, 0.2f, 0.0f);
}

void EnMThunder_EmptySpinAttack(EnMThunder* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) {
        if (player->meleeWeaponAnimation >= 0x18) {
            Audio_PlaySoundGeneral(NA_SE_IT_ROLLING_CUT, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            Audio_PlaySoundGeneral(NA_SE_IT_SWORD_SWING_HARD, &player->actor.projectedPos, 4,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }

        Actor_Kill(&this->actor);
        return;
    }

    if (!(player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
        Actor_Kill(&this->actor);
    }
}

void EnMThunder_ChargingSpinAttack(EnMThunder* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* child = this->actor.child;

    this->spinChargePercent = player->unk_858;
    this->actor.world.pos = player->bodyPartsPos[0];
    this->actor.shape.rot.y = player->actor.shape.rot.y + 0x8000;

    if (this->isUsingMagic == 0) {
        if (player->unk_858 >= 0.1f) {
            if ((gSaveContext.magicState != MAGIC_STATE_IDLE) ||
                (((this->actor.params & 0xFF00) >> 8) &&
                 !(Magic_RequestChange(play, (this->actor.params & 0xFF00) >> 8, MAGIC_CONSUME_WAIT_PREVIEW)))) {
                EnMThunder_EmptySpinAttack(this, play);
                EnMThunder_SetupAction(this, EnMThunder_EmptySpinAttack);
                this->chargeAlpha = 0;
                this->dimmingIntensity = 0.0;
                this->spinAttackTimer = 0.0f;
                return;
            }

            this->isUsingMagic = 1;
        }
    }

    if (player->unk_858 >= 0.1f) {
        Rumble_Request(0.0f, (s32)(player->unk_858 * 150.0f) & 0xFF, 2, (s32)(player->unk_858 * 150.0f) & 0xFF);
    }

    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) {
        if ((child != NULL) && (child->update != NULL)) {
            child->parent = NULL;
        }

        if (player->unk_858 <= 0.15f) {
            if ((player->unk_858 >= 0.1f) && (player->meleeWeaponAnimation >= 0x18)) {
                Audio_PlaySoundGeneral(NA_SE_IT_ROLLING_CUT, &player->actor.projectedPos, 4,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                Audio_PlaySoundGeneral(NA_SE_IT_SWORD_SWING_HARD, &player->actor.projectedPos, 4,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
            Actor_Kill(&this->actor);
            return;
        } else {
            player->stateFlags2 &= ~PLAYER_STATE2_SPIN_ATTACKING;
            // Gerudo Dual Blades: the charge sparks and their levels are hers, and so is
            // the release — but hers is not a ring around her body. It is a CONE thrown
            // forward out of the blades, so it keeps the level/damage machinery below and
            // only changes where the volume sits and what is drawn. Skijer's NEI
            if (GerudoMhr_UsesConeBurst(player)) {
                this->isGerudoCone = 1;
                this->coneYaw = player->actor.shape.rot.y;
            }
            if ((this->actor.params & 0xFF00) >> 8) {
                gSaveContext.magicState = MAGIC_STATE_CONSUME_SETUP;
            }
            if (player->unk_858 < 0.85f) {
                this->collider.info.toucher.dmgFlags = sSpinAttackDmgFlags[this->swordType];
                this->attackStrength = 1;
                this->targetScale = ((this->swordType == 1) ? 2 : 4);
            } else {
                this->collider.info.toucher.dmgFlags = sJumpAttackDmgFlags[this->swordType];
                this->attackStrength = 0;
                this->targetScale = ((this->swordType == 1) ? 4 : 8);
            }

            EnMThunder_SetupAction(this, EnMThunder_SpinAttacking);
            this->followPlayerTimer = 8;
            Audio_PlaySoundGeneral(sSfxIds[this->attackStrength], &player->actor.projectedPos, 4,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            this->spinAttackTimer = 1.0f;
            return;
        }
    }

    if (!(player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
        if (this->actor.child != NULL) {
            this->actor.child->parent = NULL;
        }
        Actor_Kill(&this->actor);
        return;
    }

    if (player->unk_858 > 0.15f) {
        this->chargeAlpha = 255;
        if (this->actor.child == NULL) {
            Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EFF_DUST, this->actor.world.pos.x,
                               this->actor.world.pos.y, this->actor.world.pos.z, 0, this->actor.shape.rot.y, 0,
                               this->swordType + 2);
        }
        this->dimmingIntensity += ((((player->unk_858 - 0.15f) * 1.5f) - this->dimmingIntensity) * 0.5f);

    } else if (player->unk_858 > .1f) {
        this->chargeAlpha = (s32)((player->unk_858 - .1f) * 255.0f * 20.0f);
        this->spinAttackTimer = (player->unk_858 - .1f) * 10.0f;
    } else {
        this->chargeAlpha = 0;
    }

    if (player->unk_858 > 0.85f) {
        func_800F4254(&player->actor.projectedPos, 2);
    } else if (player->unk_858 > 0.15f) {
        func_800F4254(&player->actor.projectedPos, 1);
    } else if (player->unk_858 > 0.1f) {
        func_800F4254(&player->actor.projectedPos, 0);
    }

    if (Play_InCsMode(play)) {
        Actor_Kill(&this->actor);
    }
}

void EnMThunder_UpdateSpinAttack(EnMThunder* this, PlayState* play) {
    if (this->followPlayerTimer < 2) {
        if (this->chargeAlpha < 40) {
            this->chargeAlpha = 0;
        } else {
            this->chargeAlpha -= 40;
        }
    }

    this->spinTrailTexScroll += 2.0f * this->spinAttackAlpha;

    if (this->dimmingIntensity < this->spinAttackTimer) {
        this->dimmingIntensity += ((this->spinAttackTimer - this->dimmingIntensity) * 0.1f);
    } else {
        this->dimmingIntensity = this->spinAttackTimer;
    }
}

void EnMThunder_SpinAttacking(EnMThunder* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    // Gerudo: the wedge is THROWN, not worn — it leaves the blades on one exact frame of
    // the release swing, not on the frame the button comes up. That frame is source frame
    // GMHR_CHARGE_FAST_BEG of ForwardTumbleDelayedCrossFinish, which the clip builder
    // translates into an installed frame for us (the release is built at three rates, so
    // source and installed frames are not proportional). Until then the actor is frozen:
    // no growth, no collider, no draw, and the lifetime does not start ticking. The frame
    // cap is a safety net so a swing cut short (damage, a cutscene) can never strand it
    // waiting forever. Skijer's NEI
    if (this->isGerudoCone && !this->coneArmed) {
        s16 summonFrame = GerudoMhr_ChargeSummonFrame();
        f32 last = player->skelAnime.endFrame;
        f32 mark = (summonFrame > 0) ? (f32)summonFrame : (last * (2.0f / 3.0f));

        this->actor.scale.x = 0.0f;
        Actor_SetScale(&this->actor, 0.0f);
        this->spinAttackAlpha = 0.0f;
        if ((this->coneWait++ >= GERUDO_WEDGE_ARM_CAP) || ((mark > 0.0f) && (player->skelAnime.curFrame >= mark))) {
            this->coneArmed = 1;
        }
        if (Play_InCsMode(play)) {
            Actor_Kill(&this->actor);
        }
        return;
    }

    if (Math_StepToF(&this->spinAttackTimer, 0.0f, 1 / 16.0f)) {
        Actor_Kill(&this->actor);
    } else {
        Math_SmoothStepToF(&this->actor.scale.x, (s32)this->targetScale, 0.6f, 0.8f, 0.0f);
        Actor_SetScale(&this->actor, this->actor.scale.x);

        if (this->isGerudoCone) {
            // Her volume is the forward third of the ring, not the whole ring. Same
            // growth curve; the cylinder that stands in for the wedge is pushed out
            // along her release facing and is 1.5x the vanilla radius.
            // AT_HIT must be read BEFORE CollisionCheck_SetAT — sATResetFuncs clears the
            // flag every frame (same trap as the sword beam further down).
            f32 wedgeR = GERUDO_WEDGE_WORLD_RADIUS(this->actor.scale.x);
            if (this->collider.base.atFlags & AT_HIT) {
                GerudoMhr_AddChargeRage(this->collider.base.at);
            }
            this->collider.dim.radius = (s16)(wedgeR * 0.62f);
            this->collider.dim.height = 70;
            this->collider.dim.yShift = -35;
            Collider_UpdateCylinder(&this->actor, &this->collider);
            this->collider.dim.pos.x += (s16)(Math_SinS(this->coneYaw) * wedgeR * 0.55f);
            this->collider.dim.pos.z += (s16)(Math_CosS(this->coneYaw) * wedgeR * 0.55f);
        } else {
            this->collider.dim.radius = (this->actor.scale.x * 25.0f);
            Collider_UpdateCylinder(&this->actor, &this->collider);
        }
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }

    if (this->followPlayerTimer > 0) {
        this->actor.world.pos.x = player->bodyPartsPos[0].x;
        this->actor.world.pos.z = player->bodyPartsPos[0].z;
        this->followPlayerTimer--;
    }

    if (this->spinAttackTimer > 0.6f) {
        this->spinAttackAlpha = 1.0f;
    } else {
        this->spinAttackAlpha = this->spinAttackTimer * (5.0f / 3.0f);
    }

    EnMThunder_UpdateSpinAttack(this, play);

    if (Play_InCsMode(play)) {
        Actor_Kill(&this->actor);
    }
}

// =============================================================================
// Fierce Deity Sword Beam (attackStrength == 2)
// =============================================================================
// Blue crescent energy disk fired when FD attacks while Z-targeting.
// Travels forward 80 units/frame for 20 frames, deals 4 damage on contact.
// DL loaded from mm.o2r (gSwordBeamDL in gameplay_keep).

// From MM z_en_m_thunder.c EnMThunder_SwordBeam_Attack (line 400-442).
// Beam grows from scale 0 to scaleTarget(12), fades via lightColorFrac 1.0→0.0.
// Movement: -80 * cos(pitch) along yaw, -80 * sin(pitch) vertically.
static void EnMThunder_SwordBeamAction(EnMThunder* this, PlayState* play) {
    // Alpha from lightColorFrac (MM lines 404-408)
    if (this->spinAttackTimer > (9.0f / 10.0f)) {
        this->spinAttackAlpha = 1.0f; // alphaFrac
    } else {
        this->spinAttackAlpha = this->spinAttackTimer * (10.0f / 9.0f);
    }

    // Lifetime: lightColorFrac steps toward 0 (MM line 410, rate 0.05 = ~20 frames)
    if (Math_StepToF(&this->spinAttackTimer, 0.0f, 0.05f)) {
        Actor_Kill(&this->actor);
        return;
    }

    // FD beam HOMING (SOH enhancement — MM's beam flies straight). The locked target
    // is stored at spawn (focusActor / arrowPointedActor). Each frame we curve the
    // beam's travel yaw/pitch toward the target's focus point so moving enemies still
    // get hit. shape.rot.y carries the +0x8000 the movement formula expects (see Init);
    // shape.rot.x is kept = -world.rot.x so the disc stays oriented to the new heading.
    if (this->homingTarget != NULL && this->homingTarget->update != NULL) {
        Vec3f* targetPos = &this->homingTarget->focus.pos;
        s16 targetYaw = Math_Vec3f_Yaw(&this->actor.world.pos, targetPos) + 0x8000;
        s16 targetPitch = Math_Vec3f_Pitch(&this->actor.world.pos, targetPos);
        Math_ScaledStepToS(&this->actor.shape.rot.y, targetYaw, 0x1000);
        Math_ScaledStepToS(&this->actor.world.rot.x, targetPitch, 0x1000);
        this->actor.shape.rot.x = -this->actor.world.rot.x;
    }

    // Movement: direct position update (MM lines 413-417)
    f32 sp2C = -80.0f * Math_CosS(this->actor.world.rot.x);
    this->actor.world.pos.x += sp2C * Math_SinS(this->actor.shape.rot.y);
    this->actor.world.pos.z += sp2C * Math_CosS(this->actor.shape.rot.y);
    this->actor.world.pos.y += -80.0f * Math_SinS(this->actor.world.rot.x);

    // Scale ramps up to scaleTarget (MM line 419-420)
    Math_SmoothStepToF(&this->actor.scale.x, this->dimmingIntensity, 0.6f, 2.0f, 0.0f);
    Actor_SetScale(&this->actor, this->actor.scale.x);

    // Scroll counter for texture animation
    this->spinTrailTexScroll += 1.0f;

    // Die on contact like an arrow: CHECK AT_HIT *BEFORE* CollisionCheck_SetAT. SetAT runs
    // sATResetFuncs which CLEARS atFlags AT_HIT every frame (z_collision_check.c:1186), so the
    // old order (SetAT then check) always read 0 → the beam never despawned on a hit (it only
    // expired by lifetime). Reading it first picks up last frame's registered hit, exactly how
    // EnArrow does it (z_en_arrow.c:386 checks AT_HIT before its own SetAT).
    if (this->collider.base.atFlags & AT_HIT) {
        Actor* hitActor = this->collider.base.at;
        if (hitActor != NULL) {
            Vec3f vel = { 0.0f, 0.0f, 0.0f };
            Vec3f accel = { 0.0f, 0.0f, 0.0f };
            EffectSsBlast_SpawnWhiteCustomScale(play, &hitActor->focus.pos, &vel, &accel, 100, 250, 8);
        }
        Actor_Kill(&this->actor);
        return;
    }

    // Collider: radius grows with scale (MM line 422)
    this->collider.dim.radius = (s16)(this->actor.scale.x * 5.0f);
    // Position offset forward from actor (MM lines 428-432)
    this->collider.dim.pos.x =
        (s32)((Math_SinS(this->actor.shape.rot.y) * -5.0f * this->actor.scale.x) + this->actor.world.pos.x);
    this->collider.dim.pos.y = (s32)this->actor.world.pos.y;
    this->collider.dim.pos.z =
        (s32)((Math_CosS(this->actor.shape.rot.y) * -5.0f * this->actor.scale.z) + this->actor.world.pos.z);

    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);

    // Timer (MM line 437-439)
    if (this->followPlayerTimer > 0) {
        this->followPlayerTimer--;
    }
}

void EnMThunder_Update(Actor* thisx, PlayState* play) {
    EnMThunder* this = (EnMThunder*)thisx;
    f32 blueRadius;
    s32 redGreen;

    this->actionFunc(this, play);

    // Sword beam: update light using lightColorFrac (MM EnMThunder_Update line 466-468)
    if (this->attackStrength == 2) {
        f32 lcf = this->spinAttackTimer; // lightColorFrac
        Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                                  this->actor.world.pos.z, (s32)(lcf * 255.0f), (s32)(lcf * 255.0f),
                                  (s32)(lcf * 100.0f), (s32)(lcf * 800.0f));
        return;
    }

    EnMThunder_AdjustEnvLights(play, this->dimmingIntensity);
    blueRadius = this->spinAttackTimer;
    redGreen = (u32)(blueRadius * 255.0f) & 0xFF;
    Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                              this->actor.world.pos.z, redGreen, redGreen, (u32)(blueRadius * 100.0f),
                              (s32)(blueRadius * 800.0f));
}

// Gerudo's charge wedge: the spin attack's own VFX, 1:1, with only the third of the
// cylinder that faces forward left standing.
//
// The look cannot be rebuilt by hand — gameplay_keep exports no symbol for the sheet the
// spin DLs use; it lives inside gSpinAttack1DL. So the real DLs are RUN, under a matrix
// scaled to zero: every triangle collapses to a point and nothing is rasterised, but the
// RDP is left exactly as the vanilla effect wants it — its texture in TMEM, its two
// tiles, its combiner, its render mode, its cycle type. Our arc is then drawn on top of
// that state without touching any of it, so it is the same material by construction.
//
// The geometry itself is a strip: two rings of vertices (bottom and top) across 120
// degrees, written into the frame's arena (Graph_Alloc), never a static buffer — the RDP
// reads them long after this returns. Vertex colours stay neutral white so they cannot
// tint whatever combiner the vanilla DL installed; the colour comes from PRIMITIVE, set
// here to the same values vanilla uses per level.
static void EnMThunder_DrawGerudoWedge(EnMThunder* this, PlayState* play, u8 alpha) {
    s32 vtxCount = (GERUDO_WEDGE_SEGMENTS + 1) * 2;
    Vtx* verts = (Vtx*)Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * vtxCount);
    s32 i;
    f32 scroll;

    if (verts == NULL) {
        return;
    }

    // tc is s10.5 and only holds about +-1024 texels, so the scroll MUST be wrapped into
    // one tile first: spinTrailTexScroll climbs forever and would overflow the coordinate.
    scroll = this->spinTrailTexScroll - (64.0f * floorf(this->spinTrailTexScroll / 64.0f));

    for (i = 0; i <= GERUDO_WEDGE_SEGMENTS; i++) {
        f32 t = (f32)i / (f32)GERUDO_WEDGE_SEGMENTS; // 0..1 across the arc
        s16 angle = (s16)(-(GERUDO_WEDGE_ARC / 2) + (s16)(GERUDO_WEDGE_ARC * t));
        s16 x = (s16)(Math_SinS(angle) * GERUDO_WEDGE_RADIUS);
        s16 z = (s16)(Math_CosS(angle) * GERUDO_WEDGE_RADIUS);
        // Two tiles across the arc, scrolling on the same counter the spin DLs use.
        s16 u = (s16)(((t * 2.0f * 64.0f) + scroll) * 32.0f);
        s32 j = i * 2;

        verts[j].v.ob[0] = x;
        verts[j].v.ob[1] = (s16)(-GERUDO_WEDGE_HEIGHT * 0.5f);
        verts[j].v.ob[2] = z;
        verts[j].v.flag = 0;
        verts[j].v.tc[0] = u;
        verts[j].v.tc[1] = (s16)(32 * 32); // the spin tile is 64x32 (see the scroll below)
        verts[j].v.cn[0] = 255;
        verts[j].v.cn[1] = 255;
        verts[j].v.cn[2] = 255;
        verts[j].v.cn[3] = 255;

        verts[j + 1].v.ob[0] = x;
        verts[j + 1].v.ob[1] = (s16)(GERUDO_WEDGE_HEIGHT * 0.5f);
        verts[j + 1].v.ob[2] = z;
        verts[j + 1].v.flag = 0;
        verts[j + 1].v.tc[0] = u;
        verts[j + 1].v.tc[1] = 0;
        verts[j + 1].v.cn[0] = 255;
        verts[j + 1].v.cn[1] = 255;
        verts[j + 1].v.cn[2] = 255;
        verts[j + 1].v.cn[3] = 255;
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    // Segment 0x08 is the animated tile scroll the spin DLs run through. It has to be
    // live before they do, and our vertices index the tiles it sets. Same call vanilla
    // makes for attackStrength 0/1.
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0xFF - ((u8)(s32)(this->spinTrailTexScroll * 30) & 0xFF), 0,
                                  0x40, 0x20, 1, 0xFF - ((u8)(s32)(this->spinTrailTexScroll * 20) & 0xFF), 0, 8, 8, -30,
                                  0, -20, 0));

    // Vanilla's own colours: yellow for the level-2 spin, cyan for the level-1 one.
    if (this->attackStrength == 0) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, alpha);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, alpha);
    }

    // Load the vanilla effect's state by running its DLs collapsed to a point.
    Matrix_Push();
    Matrix_Scale(0.0f, 0.0f, 0.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    if (this->attackStrength == 0) {
        gSPDisplayList(POLY_XLU_DISP++, gSpinAttack3DL);
        gSPDisplayList(POLY_XLU_DISP++, gSpinAttack4DL);
    } else {
        gSPDisplayList(POLY_XLU_DISP++, gSpinAttack1DL);
        gSPDisplayList(POLY_XLU_DISP++, gSpinAttack2DL);
    }
    Matrix_Pop();

    // Our third of that cylinder, in the real frame. The actor's shape.rot.y carries the
    // +0x8000 EnMThunder is spawned with, so local +Z points BEHIND her: half a turn puts
    // the arc where the blades are pointing.
    Matrix_Scale(GERUDO_WEDGE_UNIT, GERUDO_WEDGE_UNIT, GERUDO_WEDGE_UNIT, MTXMODE_APPLY);
    Matrix_RotateY(3.14159265f, MTXMODE_APPLY); // Matrix_RotateY takes RADIANS (sinf), not binangs
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    // The only state we insist on: no lighting (vertex colours are colours, not normals),
    // no generated UVs (ours would be thrown away), and both faces, since the arc is seen
    // from whichever side she happens to throw it.
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_CULL_BOTH);

    gSPVertex(POLY_XLU_DISP++, verts, vtxCount, 0);
    for (i = 0; i < GERUDO_WEDGE_SEGMENTS; i++) {
        s32 j = i * 2;
        gSP2Triangles(POLY_XLU_DISP++, j, j + 1, j + 3, 0, j, j + 3, j + 2, 0);
    }

    gSPSetGeometryMode(POLY_XLU_DISP++, G_CULL_BACK);
    CLOSE_DISPS(play->state.gfxCtx);
}

void EnMThunder_DrawChargeGlow(EnMThunder* thunder, PlayState* play, MtxF* handMtx) {
    static f32 sSpinChargeScale[] = { 0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.25f, 0.2f, 0.15f };
    f32 pulse;
    s32 scrollRate;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Mult(handMtx, MTXMODE_NEW);

    // The trident hides the sword and draws a lance with its own placement, so the glow has to be
    // built in the LANCE's frame or it sits off the weapon entirely.
    if (!ExtEquip_TridentThunderTransform()) {
        switch (thunder->swordType) {
            case 1:
                Matrix_Translate(0.0f, 220.0f, 0.0f, MTXMODE_APPLY);
                Matrix_Scale(-0.7f, -0.6f, -0.4f, MTXMODE_APPLY);
                Matrix_RotateX(16384.0f, MTXMODE_APPLY);
                break;
            case 0:
                Matrix_Translate(0.0f, 300.0f, -100.0f, MTXMODE_APPLY);
                Matrix_Scale(-1.2f, -1.0f, -0.7f, MTXMODE_APPLY);
                Matrix_RotateX(16384.0f, MTXMODE_APPLY);
                break;
            case 2:
                Matrix_Translate(200.0f, 350.0f, 0.0f, MTXMODE_APPLY);
                Matrix_Scale(-1.8f, -1.4f, -0.7f, MTXMODE_APPLY);
                Matrix_RotateX(16384.0f, MTXMODE_APPLY);
                break;
        }
    }

    if (thunder->spinChargePercent >= 0.85f) {
        pulse = (sSpinChargeScale[(play->gameplayFrames & 7)] * 6.0f) + 1.0f;
        if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level2Primary.Changed"), 0)) {
            Color_RGB8 color =
                CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level2Primary.Value"), (Color_RGB8){ 255, 255, 170 });
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, thunder->chargeAlpha);
        } else {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, thunder->chargeAlpha);
        }
        if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level2Secondary.Changed"), 0)) {
            Color_RGB8 color =
                CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level2Secondary.Value"), (Color_RGB8){ 255, 100, 0 });
            gDPSetEnvColor(POLY_XLU_DISP++, color.r, color.g, color.b, 128);
        } else {
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 100, 0, 128);
        }
        scrollRate = 0x28;
    } else {
        pulse = (sSpinChargeScale[play->gameplayFrames & 7] * 2.0f) + 1.0f;
        if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level1Primary.Changed"), 0)) {
            Color_RGB8 color =
                CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level1Primary.Value"), (Color_RGB8){ 170, 255, 255 });
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, thunder->chargeAlpha);
        } else {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, thunder->chargeAlpha);
        }
        if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level1Secondary.Changed"), 0)) {
            Color_RGB8 color =
                CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level1Secondary.Value"), (Color_RGB8){ 0, 100, 255 });
            gDPSetEnvColor(POLY_XLU_DISP++, color.r, color.g, color.b, 128);
        } else {
            gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
        }
        scrollRate = 0x14;
    }

    Matrix_Scale(1.0f, pulse, pulse, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    gSPSegment(POLY_XLU_DISP++, 0x09,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (play->gameplayFrames * 5) & 0xFF, 0, 0x20, 0x20, 1,
                                  (play->gameplayFrames * 20) & 0xFF, (play->gameplayFrames * scrollRate) & 0xFF, 8, 8,
                                  5, 0, 20, scrollRate));

    gSPDisplayList(POLY_XLU_DISP++, gSpinAttackChargingDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void EnMThunder_Draw(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    EnMThunder* this = (EnMThunder*)thisx;
    Player* player = GET_PLAYER(play);

    // Sword beam: draw blue crescent energy disk from mm.o2r.
    // From MM EnMThunder_Draw (lines 480-535): uses Matrix_Scale(0.02f), segment 0x08 TwoTexScroll,
    // alphaFrac for fade, prim/env colors matching ENMTHUNDER_SUBTYPE_SWORDBEAM_REGULAR.
    if (this->attackStrength == 2) {
        u16 alpha = (u16)(this->spinAttackAlpha * 255.0f); // alphaFrac
        Gfx* beamDL = TransformMasks_GetFDSwordBeamDL(play);

        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        // MM line 490: scale 0.02f applied to actor matrix (actor scale ramps to 12 → final 0.24f)
        Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        // Segment 0x08: animated texture scroll (MM lines 504-506)
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 16, 64, 1, 0,
                                    0x1FF - ((u16)(s32)(this->spinTrailTexScroll * 10.0f) & 0x1FF), 32, 128));
        // Colors: MM SWORDBEAM_REGULAR (lines 527-529)
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, alpha);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
        if (beamDL != NULL) {
            // MM DL from mm.o2r
            gSPDisplayList(POLY_XLU_DISP++, beamDL);
        } else {
            // Fallback: use OOT level-1 spin attack DL (same cyan color)
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0xFF - ((u8)(s32)(this->spinTrailTexScroll * 30) & 0xFF),
                                        0, 0x40, 0x20, 1, 0xFF - ((u8)(s32)(this->spinTrailTexScroll * 20) & 0xFF), 0,
                                        8, 8));
            gSPDisplayList(POLY_XLU_DISP++, gSpinAttack1DL);
            gSPDisplayList(POLY_XLU_DISP++, gSpinAttack2DL);
        }
        CLOSE_DISPS(play->state.gfxCtx);
        return;
    }

    // Gerudo: her release is the forward cone, so the two ring DLs below are replaced.
    // The charge glow on the blades further down still runs — it is a different thing.
    // Drawn before the shared Matrix_Scale so it owns its own local frame.
    if (this->isGerudoCone) {
        if (this->coneArmed) {
            EnMThunder_DrawGerudoWedge(this, play, (u8)(this->spinAttackAlpha * 255.0f));
        }
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    switch (this->isGerudoCone ? -1 : this->attackStrength) {
        case 0:
        case 1:
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScrollEx(
                           play->state.gfxCtx, 0, 0xFF - ((u8)(s32)(this->spinTrailTexScroll * 30) & 0xFF), 0, 0x40,
                           0x20, 1, 0xFF - ((u8)(s32)(this->spinTrailTexScroll * 20) & 0xFF), 0, 8, 8, -30, 0, -20, 0));
            break;
    }

    switch (this->isGerudoCone ? -1 : this->attackStrength) {
        case 0:
            if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level2Primary.Changed"), 0)) {
                Color_RGB8 color =
                    CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level2Primary.Value"), (Color_RGB8){ 255, 255, 170 });
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, (u8)(this->spinAttackAlpha * 255));
            } else {
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, (u8)(this->spinAttackAlpha * 255));
            }
            gSPDisplayList(POLY_XLU_DISP++, gSpinAttack3DL);
            gSPDisplayList(POLY_XLU_DISP++, gSpinAttack4DL);
            break;
        case 1:
            if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level1Primary.Changed"), 0)) {
                Color_RGB8 color =
                    CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level1Primary.Value"), (Color_RGB8){ 170, 255, 255 });
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, (u8)(this->spinAttackAlpha * 255));
            } else {
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, (u8)(this->spinAttackAlpha * 255));
            }
            gSPDisplayList(POLY_XLU_DISP++, gSpinAttack1DL);
            gSPDisplayList(POLY_XLU_DISP++, gSpinAttack2DL);
            break;
    }

    CLOSE_DISPS(play->state.gfxCtx);

    EnMThunder_DrawChargeGlow(this, play, &player->mf_9E0);

    // Gerudo Dual Blades: she charges with a blade in each hand, so the glow goes up twice. Her
    // right-hand frame is captured in MmForm_PostLimbDraw.
    if (GerudoMhr_UsesConeBurst(player)) {
        EnMThunder_DrawChargeGlow(this, play, &gGerudoRightHandMtx);
    }

    FourSwordClone_DrawChargeGlowAll(this, play);
}
