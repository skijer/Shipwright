// Majora's Mask's fighter pirate (En_Kaizoku) as an Unbound custom enemy, drawn from mm.o2r.
// No fortress, cutscenes or exits: she taunts in place, draws her swords once Link comes near and fights there;
// beaten, she throws her flash and is gone.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/effects/ovl_Effect_Ss_HitMark/z_eff_ss_hitmark.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define PIRATE_KEY "mm.pirate"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
#define MM_SFX_STOP_SERVICE "mm.sfx.stop"

#define OBJ_KZ "__OTR__objects/object_kz/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_FLOOR_WATER 0x1D

#define MM_SE_EN_PIRATE_ATTACK 0x3998
#define MM_SE_EN_PIRATE_DAMAGE 0x3999
#define MM_SE_EN_PIRATE_DEAD 0x399A
#define MM_SE_EN_PIRATE_BREATH 0x39B7
#define MM_SE_EN_PIRATE_DAMM_BREATH 0x3A73
#define MM_SE_EN_PIRATE_SHOUT 0x3A74
#define MM_SE_EN_BOSU_SWORD 0x3A4D
#define MM_SE_EN_GERUDOFT_WALK 0x39A0
#define MM_SE_EN_GERUDOFT_DOWN 0x387A
#define MM_SE_EN_TEKU_JUMP 0x386C
#define MM_SE_EN_COMMON_FREEZE 0x389E
#define MM_SE_IT_SHIELD_BOUND 0x1806
#define MM_SE_EV_ICE_BROKEN 0x28CB

#define PIRATE_HEALTH 8
#define PIRATE_SCALE 0.0125f
#define PIRATE_SWORD_DAMAGE 4
#define PIRATE_SPIN_EXTRA_DAMAGE 0xC
#define PIRATE_CHALLENGE_DISTANCE 200.0f
#define PIRATE_BODYPART_MAX 15
#define PIRATE_COLOR_COUNT 3
#define PIRATE_FLASH_FRAME 18

typedef enum {
    PIRATE_LIMB_NONE,
    PIRATE_LIMB_ROOT,
    PIRATE_LIMB_TORSO,
    PIRATE_LIMB_NECK,
    PIRATE_LIMB_PONYTAIL,
    PIRATE_LIMB_VEIL,
    PIRATE_LIMB_HEAD,
    PIRATE_LIMB_RIGHT_UPPER_ARM,
    PIRATE_LIMB_RIGHT_FOREARM,
    PIRATE_LIMB_RIGHT_WRIST,
    PIRATE_LIMB_RIGHT_HAND,
    PIRATE_LIMB_RIGHT_SWORD,
    PIRATE_LIMB_LEFT_UPPER_ARM,
    PIRATE_LIMB_LEFT_FOREARM,
    PIRATE_LIMB_LEFT_WRIST,
    PIRATE_LIMB_LEFT_HAND,
    PIRATE_LIMB_LEFT_SWORD,
    PIRATE_LIMB_LEFT_THIGH,
    PIRATE_LIMB_LEFT_SHIN,
    PIRATE_LIMB_LEFT_FOOT,
    PIRATE_LIMB_RIGHT_THIGH,
    PIRATE_LIMB_RIGHT_SHIN,
    PIRATE_LIMB_RIGHT_FOOT,
    PIRATE_LIMB_WAIST,
    PIRATE_LIMB_MAX
} PirateLimb;

typedef enum {
    PIRATE_ANIM_FIGHTING_IDLE,
    PIRATE_ANIM_SIDESTEP,
    PIRATE_ANIM_WALK,
    PIRATE_ANIM_DAMAGE,
    PIRATE_ANIM_BLOCK,
    PIRATE_ANIM_FLIP,
    PIRATE_ANIM_SLASH,
    PIRATE_ANIM_SPIN_ATTACK,
    PIRATE_ANIM_CHALLENGE,
    PIRATE_ANIM_UNSHEATHE,
    PIRATE_ANIM_DEFEAT,
    PIRATE_ANIM_DEFEAT_IDLE,
    PIRATE_ANIM_THROW_FLASH,
    PIRATE_ANIM_MAX
} PirateAnimation;

// Order matters: everything below DAMAGED can still be hurt, as in MM.
typedef enum {
    PIRATE_ACTION_LURK,
    PIRATE_ACTION_UNSHEATHE,
    PIRATE_ACTION_READY,
    PIRATE_ACTION_SPIN_DODGE,
    PIRATE_ACTION_CIRCLE,
    PIRATE_ACTION_ADVANCE,
    PIRATE_ACTION_ROLL_FORWARD,
    PIRATE_ACTION_JUMP,
    PIRATE_ACTION_ROLL_BACK,
    PIRATE_ACTION_SLASH,
    PIRATE_ACTION_PIVOT,
    PIRATE_ACTION_SPIN_ATTACK,
    PIRATE_ACTION_BLOCK,
    PIRATE_ACTION_STUNNED,
    PIRATE_ACTION_DAMAGED,
    PIRATE_ACTION_KNOCK_DOWN,
    PIRATE_ACTION_VANISH
} PirateAction;

typedef enum {
    PIRATE_DMGEFF_NONE,
    PIRATE_DMGEFF_STUN,
    PIRATE_DMGEFF_FIRE,
    PIRATE_DMGEFF_FREEZE,
    PIRATE_DMGEFF_LIGHT,
    PIRATE_DMGEFF_STUNNED_ONLY = 0xD,
    PIRATE_DMGEFF_ALWAYS_HIT,
    PIRATE_DMGEFF_IFRAME_PROTECTED
} PirateDamageEffect;

typedef enum { PIRATE_AURA_NONE, PIRATE_AURA_FIRE, PIRATE_AURA_FROZEN, PIRATE_AURA_LIGHT } PirateAura;

typedef enum { PIRATE_VANISH_CATCH_BREATH, PIRATE_VANISH_THROW_FLASH, PIRATE_VANISH_FADE } PirateVanishStep;

struct EnPirate;

typedef void (*EnPirateActionFunc)(struct EnPirate*, PlayState*);

typedef struct EnPirate {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[PIRATE_LIMB_MAX];
    Vec3s morphTable[PIRATE_LIMB_MAX];
    EnPirateActionFunc actionFunc;
    PirateAction action;
    PirateAura aura;
    PirateVanishStep vanishStep;
    s16 combatTimer;
    s16 lookTimer;
    s16 iceTimer;
    s16 auraTimer;
    s16 eyeIndex;
    s16 blinkTimer;
    s16 swordState;
    s16 flashTimer;
    s16 hideTimer;
    s16 bodyPartIndex;
    s32 colorType;
    s32 blureIndex;
    bool animationsDisabled;
    bool isBreathing;
    bool isFightStarted;
    bool isFlashing;
    bool isSpinBounced;
    f32 animEndFrame;
    f32 circlingRate;
    f32 swordScaleRight;
    f32 swordScaleLeft;
    f32 flashAlpha;
    Vec3f knockbackVelocity;
    Vec3f rightFootPos;
    Vec3f leftFootPos;
    Vec3s headRot;
    Vec3f bodyPartsPos[PIRATE_BODYPART_MAX];
    ColliderCylinder bodyCollider;
    ColliderQuad swordCollider;
} EnPirate;

typedef void (*TransformLimbDraw)(PlayState* play, s32 limbIndex, void* arg);

typedef struct {
    PlayState* play;
    void** skeleton;
    Vec3s* jointTable;
    OverrideLimbDrawOpa overrideLimbDraw;
    PostLimbDrawOpa postLimbDraw;
    TransformLimbDraw transformLimbDraw;
    void* arg;
    Mtx* mtx;
} TransformFlexDraw;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);
typedef void (*MmSfxStopFunc)(uint16_t sfxId);

static const ALIGN_ASSET(2) char sPirateSkel[] = OBJ_KZ "gFighterPirateSkel";
static const ALIGN_ASSET(2) char sFightingIdleAnim[] = OBJ_KZ "gFighterPirateFightingIdleAnim";
static const ALIGN_ASSET(2) char sSidestepAnim[] = OBJ_KZ "gFighterPirateSidestepAnim";
static const ALIGN_ASSET(2) char sWalkAnim[] = OBJ_KZ "gFighterPirateWalkAnim";
static const ALIGN_ASSET(2) char sDamageAnim[] = OBJ_KZ "gFighterPirateDamageAnim";
static const ALIGN_ASSET(2) char sBlockAnim[] = OBJ_KZ "gFighterPirateBlockAnim";
static const ALIGN_ASSET(2) char sFlipAnim[] = OBJ_KZ "gFighterPirateFlipAnim";
static const ALIGN_ASSET(2) char sSlashAnim[] = OBJ_KZ "gFighterPirateSlashAnim";
static const ALIGN_ASSET(2) char sSpinAttackAnim[] = OBJ_KZ "gFighterPirateSpinAttackAnim";
static const ALIGN_ASSET(2) char sChallengeAnim[] = OBJ_KZ "gFighterPirateLandTalkAnim";
static const ALIGN_ASSET(2) char sUnsheatheAnim[] = OBJ_KZ "gFighterPirateUnsheatheAnim";
static const ALIGN_ASSET(2) char sDefeatAnim[] = OBJ_KZ "gFighterPirateDefeatAnim";
static const ALIGN_ASSET(2) char sDefeatIdleAnim[] = OBJ_KZ "gFighterPirateDefeatIdleAnim";
static const ALIGN_ASSET(2) char sThrowFlashAnim[] = OBJ_KZ "gFighterPirateThrowFlashAnim";
static const ALIGN_ASSET(2) char sEyeOpenTex[] = OBJ_KZ "gFighterPirateEyeOpenTex";
static const ALIGN_ASSET(2) char sEyeHalfTex[] = OBJ_KZ "gFighterPirateEyeHalfTex";
static const ALIGN_ASSET(2) char sEyeClosedTex[] = OBJ_KZ "gFighterPirateEyeClosedTex";

static const char* const sAnimations[PIRATE_ANIM_MAX] = {
    sFightingIdleAnim, sSidestepAnim,  sWalkAnim,      sDamageAnim, sBlockAnim,      sFlipAnim,       sSlashAnim,
    sSpinAttackAnim,   sChallengeAnim, sUnsheatheAnim, sDefeatAnim, sDefeatIdleAnim, sThrowFlashAnim,
};

static const u8 sAnimationModes[PIRATE_ANIM_MAX] = {
    ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_ONCE,
    ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE,
};

static const char* const sEyeTextures[] = { sEyeOpenTex, sEyeHalfTex, sEyeClosedTex, sEyeHalfTex };

static const Color_RGBA8 sLipstickColors[PIRATE_COLOR_COUNT] = {
    { 255, 255, 90, 255 },
    { 55, 25, 80, 255 },
    { 5, 195, 110, 255 },
};

static const Color_RGBA8 sOutfitColors[PIRATE_COLOR_COUNT] = {
    { 255, 130, 10, 255 },
    { 185, 130, 210, 255 },
    { 135, 195, 80, 255 },
};

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, PIRATE_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, PIRATE_DMGEFF_IFRAME_PROTECTED),
    /* Slingshot     */ DMG_ENTRY(1, PIRATE_DMGEFF_ALWAYS_HIT),
    /* Explosive     */ DMG_ENTRY(2, PIRATE_DMGEFF_IFRAME_PROTECTED),
    /* Boomerang     */ DMG_ENTRY(0, PIRATE_DMGEFF_STUN),
    /* Normal arrow  */ DMG_ENTRY(2, PIRATE_DMGEFF_IFRAME_PROTECTED),
    /* Hammer swing  */ DMG_ENTRY(2, PIRATE_DMGEFF_ALWAYS_HIT),
    /* Hookshot      */ DMG_ENTRY(0, PIRATE_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, PIRATE_DMGEFF_ALWAYS_HIT),
    /* Master sword  */ DMG_ENTRY(2, PIRATE_DMGEFF_ALWAYS_HIT),
    /* Giant's Knife */ DMG_ENTRY(4, PIRATE_DMGEFF_ALWAYS_HIT),
    /* Fire arrow    */ DMG_ENTRY(4, PIRATE_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, PIRATE_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(4, PIRATE_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, PIRATE_DMGEFF_IFRAME_PROTECTED),
    /* Unk arrow 2   */ DMG_ENTRY(2, PIRATE_DMGEFF_IFRAME_PROTECTED),
    /* Unk arrow 3   */ DMG_ENTRY(2, PIRATE_DMGEFF_IFRAME_PROTECTED),
    /* Fire magic    */ DMG_ENTRY(4, PIRATE_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(4, PIRATE_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(4, PIRATE_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, PIRATE_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, PIRATE_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(1, PIRATE_DMGEFF_STUNNED_ONLY),
    /* Giant spin    */ DMG_ENTRY(4, PIRATE_DMGEFF_STUNNED_ONLY),
    /* Master spin   */ DMG_ENTRY(2, PIRATE_DMGEFF_STUNNED_ONLY),
    /* Kokiri jump   */ DMG_ENTRY(2, PIRATE_DMGEFF_ALWAYS_HIT),
    /* Giant jump    */ DMG_ENTRY(8, PIRATE_DMGEFF_ALWAYS_HIT),
    /* Master jump   */ DMG_ENTRY(4, PIRATE_DMGEFF_ALWAYS_HIT),
    /* Unknown 1     */ DMG_ENTRY(0, PIRATE_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, PIRATE_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(0, PIRATE_DMGEFF_STUNNED_ONLY),
    /* Unknown 2     */ DMG_ENTRY(0, PIRATE_DMGEFF_NONE),
};

static ColliderCylinderInit sBodyCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK4,
      { 0x00000000, 0x00, 0x00 },
      { 0xF7CFFFFF, 0x00, 0x00 },
      TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_ON | BUMP_HOOKABLE,
      OCELEM_ON },
    { 20, 65, 0, { 0, 0, 0 } },
};

static ColliderQuadInit sSwordQuadInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_QUAD },
    { ELEMTYPE_UNK4,
      { 0xF7CFFFFF, 0x00, PIRATE_SWORD_DAMAGE },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
      BUMP_NONE,
      OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sFootOffset = { 300.0f, 0.0f, 0.0f };
static Vec3f sSwordTipOffset = { 0.0f, -3000.0f, 0.0f };
static Vec3f sSwordHiltOffset = { 400.0f, 0.0f, 0.0f };
static Vec3f sSwordQuadOffsets[4] = {
    { -3000.0f, -2000.0f, 1300.0f },
    { 1600.0f, -4000.0f, 0.0f },
    { 1000.0f, 1000.0f, 0.0f },
    { -3000.0f, -2000.0f, -1300.0f },
};

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static MmSfxStopFunc sStopMmSfx;
static bool sAreMmSfxSearched;

static void Pirate_Lurk(EnPirate* this, PlayState* play);
static void Pirate_Unsheathe(EnPirate* this, PlayState* play);
static void Pirate_Ready(EnPirate* this, PlayState* play);
static void Pirate_SpinDodge(EnPirate* this, PlayState* play);
static void Pirate_Block(EnPirate* this, PlayState* play);
static void Pirate_Jump(EnPirate* this, PlayState* play);
static void Pirate_RollBack(EnPirate* this, PlayState* play);
static void Pirate_Slash(EnPirate* this, PlayState* play);
static void Pirate_RollForward(EnPirate* this, PlayState* play);
static void Pirate_Advance(EnPirate* this, PlayState* play);
static void Pirate_Pivot(EnPirate* this, PlayState* play);
static void Pirate_SpinAttack(EnPirate* this, PlayState* play);
static void Pirate_Circle(EnPirate* this, PlayState* play);
static void Pirate_Stunned(EnPirate* this, PlayState* play);
static void Pirate_Damaged(EnPirate* this, PlayState* play);
static void Pirate_DefeatKnockdown(EnPirate* this, PlayState* play);
static void Pirate_Vanish(EnPirate* this, PlayState* play);
static void Pirate_SetupReady(EnPirate* this);
static void Pirate_SetupSpinDodge(EnPirate* this, PlayState* play);
static void Pirate_SetupBlock(EnPirate* this);
static void Pirate_SetupJump(EnPirate* this);
static void Pirate_SetupRollBack(EnPirate* this);
static void Pirate_SetupSlash(EnPirate* this);
static void Pirate_SetupRollForward(EnPirate* this);
static void Pirate_SetupAdvance(EnPirate* this);
static void Pirate_SetupPivot(EnPirate* this);
static void Pirate_SetupSpinAttack(EnPirate* this);
static void Pirate_SetupCircle(EnPirate* this);

static void FindMmSfxServices(void) {
    if (sAreMmSfxSearched || !SOH_MOD_API_HAS(sApi, FindService)) {
        return;
    }
    sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
    sStopMmSfx = (MmSfxStopFunc)sApi->FindService(MM_SFX_STOP_SERVICE);
    sAreMmSfxSearched = true;
}

// Falls back to the closest OoT sound when the mm_assets sound service is missing.
static void PlaySfx(Actor* actor, u16 mmSfx, u16 ootSfx) {
    FindMmSfxServices();
    if (sPlayMmSfx != NULL && sPlayMmSfx(mmSfx, &actor->projectedPos)) {
        return;
    }
    Audio_PlayActorSound2(actor, ootSfx);
}

static void StopBreathing(Actor* actor) {
    FindMmSfxServices();
    if (sStopMmSfx != NULL) {
        sStopMmSfx(MM_SE_EN_PIRATE_BREATH);
    }
    Audio_StopSfxByPosAndId(&actor->projectedPos, NA_SE_EN_GERUDOFT_BREATH);
}

static void PlayBreath(EnPirate* this, PlayState* play) {
    if (!(play->gameplayFrames & 0x5F)) {
        PlaySfx(&this->actor, MM_SE_EN_PIRATE_BREATH, NA_SE_EN_GERUDOFT_BREATH);
    }
}

static void PlayStepOnFrames(EnPirate* this, s32 secondStep) {
    s32 before = this->skelAnime.curFrame - this->skelAnime.playSpeed;
    s32 after = this->skelAnime.curFrame + this->skelAnime.playSpeed;
    bool isStepping = (before < 0 && after > 0) || (before < secondStep && after > secondStep);

    if (this->skelAnime.curFrame != 0.0f && isStepping) {
        PlaySfx(&this->actor, MM_SE_EN_GERUDOFT_WALK, NA_SE_EN_MUSI_LAND);
    }
}

static void DrawTransformFlexLimb(TransformFlexDraw* draw, s32 limbIndex) {
    StandardLimb* limb = SEGMENTED_TO_VIRTUAL(draw->skeleton[limbIndex]);
    s32 drawnIndex = limbIndex + 1;
    Gfx* limbDList = limb->dList;
    Gfx* newDList = limbDList;
    Vec3s rot = draw->jointTable[drawnIndex];
    Vec3s* origin = limbIndex == 0 ? &draw->jointTable[0] : &limb->jointPos;
    Vec3f pos = { origin->x, origin->y, origin->z };

    OPEN_DISPS(draw->play->state.gfxCtx);
    Matrix_Push();
    bool isOverridden = draw->overrideLimbDraw != NULL &&
                        draw->overrideLimbDraw(draw->play, drawnIndex, &newDList, &pos, &rot, draw->arg);

    if (!isOverridden) {
        Matrix_TranslateRotateZYX(&pos, &rot);
        Matrix_Push();
        draw->transformLimbDraw(draw->play, drawnIndex, draw->arg);
        if (newDList != NULL) {
            MATRIX_TOMTX(draw->mtx);
            gSPMatrix(POLY_OPA_DISP++, draw->mtx, G_MTX_LOAD);
            gSPDisplayList(POLY_OPA_DISP++, newDList);
            draw->mtx++;
        } else if (limbDList != NULL) {
            MATRIX_TOMTX(draw->mtx);
            draw->mtx++;
        }
        Matrix_Pop();
    }
    if (draw->postLimbDraw != NULL) {
        draw->postLimbDraw(draw->play, drawnIndex, &limbDList, &rot, draw->arg);
    }
    if (limb->child != LIMB_DONE) {
        DrawTransformFlexLimb(draw, limb->child);
    }
    Matrix_Pop();
    if (limb->sibling != LIMB_DONE) {
        DrawTransformFlexLimb(draw, limb->sibling);
    }
    CLOSE_DISPS(draw->play->state.gfxCtx);
}

// MM's SkelAnime_DrawTransformFlexOpa: the transform scales one limb's own mesh, never its children.
static void DrawTransformFlexOpa(PlayState* play, SkelAnime* skelAnime, OverrideLimbDrawOpa overrideLimbDraw,
                                 PostLimbDrawOpa postLimbDraw, TransformLimbDraw transformLimbDraw, void* arg) {
    TransformFlexDraw draw = {
        play,
        skelAnime->skeleton,
        skelAnime->jointTable,
        overrideLimbDraw,
        postLimbDraw,
        transformLimbDraw,
        arg,
        Graph_Alloc(play->state.gfxCtx, skelAnime->dListCount * sizeof(Mtx)),
    };

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0D, (uintptr_t)draw.mtx);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawTransformFlexLimb(&draw, 0);
}

static void Pirate_ChangeAnim(EnPirate* this, PirateAnimation animIndex) {
    AnimationHeader* animation = (AnimationHeader*)sAnimations[animIndex];

    this->animEndFrame = Animation_GetLastFrame(animation);
    Animation_Change(&this->skelAnime, animation, 1.0f, 0.0f, this->animEndFrame, sAnimationModes[animIndex], 0.0f);
}

static bool Pirate_IsAnimDone(EnPirate* this) {
    return this->skelAnime.curFrame >= this->animEndFrame;
}

static bool Pirate_IsFrozen(EnPirate* this) {
    return this->aura == PIRATE_AURA_FROZEN && this->auraTimer != 0;
}

static void Pirate_SetAura(EnPirate* this, PirateAura aura, s16 duration) {
    this->aura = aura;
    this->auraTimer = duration;
}

static void Pirate_ShatterIce(EnPirate* this, PlayState* play) {
    PlaySfx(&this->actor, MM_SE_EV_ICE_BROKEN, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < PIRATE_BODYPART_MAX; i++) {
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &this->bodyPartsPos[i], 150, 150, 150, 250, 235, 245, 255,
                                       0.7f);
    }
    Pirate_SetAura(this, PIRATE_AURA_NONE, 0);
    this->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
}

static void Pirate_SetHittable(EnPirate* this) {
    this->bodyCollider.info.elemType = ELEMTYPE_UNK1;
    this->bodyCollider.base.colType = COLTYPE_HIT3;
}

static void Pirate_SetIntangible(EnPirate* this) {
    this->bodyCollider.info.elemType = ELEMTYPE_UNK4;
    this->bodyCollider.base.colType = COLTYPE_NONE;
    this->swordCollider.info.elemType = ELEMTYPE_UNK4;
}

static void Pirate_StartMinibossMusic(EnPirate* this) {
    if (this->isFightStarted) {
        return;
    }
    this->isFightStarted = true;
    func_800F5ACC(NA_BGM_MINI_BOSS);
}

static void Pirate_StopMinibossMusic(EnPirate* this) {
    if (!this->isFightStarted) {
        return;
    }
    this->isFightStarted = false;
    func_800F5B58();
}

static void Pirate_StartFight(EnPirate* this) {
    this->swordScaleRight = 1.0f;
    this->swordScaleLeft = 1.0f;
    this->actor.flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    Pirate_StartMinibossMusic(this);
}

static void Pirate_StopFlash(EnPirate* this, PlayState* play) {
    if (!this->isFlashing) {
        return;
    }
    this->isFlashing = false;
    play->envCtx.fillScreen = false;
    play->envCtx.screenFillColor[0] = 0;
    play->envCtx.screenFillColor[1] = 0;
    play->envCtx.screenFillColor[2] = 0;
    play->envCtx.screenFillColor[3] = 0;
}

static void Pirate_SetFlash(EnPirate* this, PlayState* play) {
    this->isFlashing = true;
    play->envCtx.fillScreen = true;
    play->envCtx.screenFillColor[0] = 255;
    play->envCtx.screenFillColor[1] = 255;
    play->envCtx.screenFillColor[2] = 255;
    play->envCtx.screenFillColor[3] = (this->flashAlpha / 60.0f) * 255.0f;
}

// An arrow or hookshot on its way makes her leap aside, unless the hookshot is reeling her in while stunned.
static bool Pirate_DodgeRanged(EnPirate* this, PlayState* play) {
    Actor* projectile = Actor_GetProjectileActor(play, &this->actor, 800.0f);

    if (projectile == NULL || this->action == PIRATE_ACTION_JUMP) {
        return false;
    }
    if (projectile->id == ACTOR_ARMS_HOOK && this->action == PIRATE_ACTION_STUNNED) {
        return false;
    }
    Pirate_SetupJump(this);
    return true;
}

static void Pirate_TurnHead(EnPirate* this) {
    if (this->action == PIRATE_ACTION_READY) {
        this->headRot.y = Math_SinS(this->lookTimer * 4200) * 8920;
        return;
    }
    if (this->action == PIRATE_ACTION_STUNNED) {
        return;
    }
    if (this->action != PIRATE_ACTION_SLASH && this->action != PIRATE_ACTION_SPIN_ATTACK) {
        this->headRot.y = 0;
        return;
    }
    Math_SmoothStepToS(&this->headRot.y, this->actor.yawTowardsPlayer - this->actor.shape.rot.y, 1, 500, 0);
    this->headRot.y = CLAMP(this->headRot.y, -0x256F, 0x256F);
}

static void Pirate_FacePlayer(EnPirate* this) {
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
}

static bool Pirate_ReactToBomb(EnPirate* this, PlayState* play, s16 angleToWall) {
    Actor* bomb = Actor_FindNearby(play, &this->actor, -1, ACTORCAT_EXPLOSIVE, 80.0f);

    if (bomb == NULL) {
        return false;
    }
    Pirate_FacePlayer(this);
    bool isCornered = (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) && angleToWall < 0x2EE0;

    if (!isCornered && bomb->id != ACTOR_EN_BOM_CHU) {
        if (this->action != PIRATE_ACTION_ROLL_BACK) {
            Pirate_SetupRollBack(this);
        }
        return true;
    }
    bool isChuComingAtHer = bomb->id == ACTOR_EN_BOM_CHU && Actor_WorldDistXYZToActor(&this->actor, bomb) < 80.0f &&
                            (s16)(this->actor.shape.rot.y - bomb->world.rot.y + 0x8000) < 0x4000;

    if (!isChuComingAtHer) {
        Pirate_SetupBlock(this);
    } else if (this->action != PIRATE_ACTION_JUMP) {
        Pirate_SetupJump(this);
    }
    return true;
}

static void Pirate_EngageAtRange(EnPirate* this, PlayState* play, s16 angleToPlayer) {
    Player* player = GET_PLAYER(play);

    if (angleToPlayer >= 0x2710) {
        Pirate_SetupBlock(this);
        return;
    }
    s16 yawDiff = player->actor.shape.rot.y - this->actor.shape.rot.y;
    bool canSlash =
        this->actor.xzDistToPlayer <= 65.0f && !Actor_OtherIsTargeted(play, &this->actor) && ABS(yawDiff) < 0x5000;

    if (canSlash) {
        if (this->action != PIRATE_ACTION_SLASH) {
            Pirate_SetupSlash(this);
        }
    } else if (this->action != PIRATE_ACTION_CIRCLE) {
        Pirate_SetupCircle(this);
    }
}

// MM's guard: blocks or spin-dodges a swing, flips over a cornered blow, shies away from bombs.
static bool Pirate_ReactToPlayer(EnPirate* this, PlayState* play, bool isForcedToEngage) {
    Player* player = GET_PLAYER(play);
    s16 angleToWall = ABS((s16)(this->actor.wallYaw - this->actor.shape.rot.y));
    s16 angleToPlayer = ABS((s16)(this->actor.yawTowardsPlayer - this->actor.shape.rot.y));
    bool isJumpSlashing = player->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_START;

    if (func_800354B4(play, &this->actor, 100.0f, 0x2710, 0x4000, this->actor.shape.rot.y)) {
        if (!isJumpSlashing) {
            Pirate_SetupBlock(this);
        } else if (this->action != PIRATE_ACTION_SPIN_DODGE) {
            Pirate_SetupSpinDodge(this, play);
        }
        return true;
    }
    if (func_800354B4(play, &this->actor, 100.0f, 0x5DC0, 0x2AA8, this->actor.shape.rot.y)) {
        bool isCornered =
            (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) && angleToWall < 0x2EE0 && this->actor.xzDistToPlayer < 90.0f;

        Pirate_FacePlayer(this);
        if (isCornered) {
            if (this->action != PIRATE_ACTION_JUMP) {
                Pirate_SetupJump(this);
            }
        } else if (isJumpSlashing) {
            if (this->action != PIRATE_ACTION_SPIN_DODGE) {
                Pirate_SetupSpinDodge(this, play);
            }
        } else if (this->actor.xzDistToPlayer < 180.0f) {
            Pirate_SetupBlock(this);
        } else if (this->action != PIRATE_ACTION_ROLL_BACK) {
            Pirate_SetupRollBack(this);
        }
        return true;
    }
    if (Pirate_ReactToBomb(this, play, angleToWall)) {
        return true;
    }
    if (isForcedToEngage) {
        Pirate_EngageAtRange(this, play, angleToPlayer);
    }
    return isForcedToEngage;
}

static void Pirate_SetupLurk(EnPirate* this) {
    Pirate_ChangeAnim(this, PIRATE_ANIM_CHALLENGE);
    this->action = PIRATE_ACTION_LURK;
    this->actionFunc = Pirate_Lurk;
}

// Taunts in place, turning to Link, until he steps within reach.
static void Pirate_Lurk(EnPirate* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, 0x400, 0x40);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (this->actor.xzDistToPlayer >= PIRATE_CHALLENGE_DISTANCE) {
        return;
    }
    Pirate_FacePlayer(this);
    Pirate_ChangeAnim(this, PIRATE_ANIM_UNSHEATHE);
    this->action = PIRATE_ACTION_UNSHEATHE;
    this->actionFunc = Pirate_Unsheathe;
}

static void Pirate_Unsheathe(EnPirate* this, PlayState* play) {
    f32 curFrame = this->skelAnime.curFrame;

    if (Animation_OnFrame(&this->skelAnime, 20.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_BOSU_SWORD, NA_SE_IT_SWORD_PICKOUT);
    }
    if (curFrame >= 6.0f) {
        Math_ApproachF(&this->swordScaleRight, 1.0f, 0.3f, 0.3f);
    }
    if (curFrame >= 22.0f) {
        Math_ApproachF(&this->swordScaleLeft, 1.0f, 0.3f, 0.3f);
    }
    if (Pirate_IsAnimDone(this)) {
        Pirate_StartFight(this);
        Pirate_SetupReady(this);
    }
}

static void Pirate_SetupReady(EnPirate* this) {
    this->actor.speedXZ = 0.0f;
    Pirate_ChangeAnim(this, PIRATE_ANIM_FIGHTING_IDLE);
    this->action = PIRATE_ACTION_READY;
    this->actionFunc = Pirate_Ready;
    this->actor.shape.shadowScale = 90.0f;
}

static void Pirate_Ready(EnPirate* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->lookTimer != 0 || Pirate_DodgeRanged(this, play) || Pirate_ReactToPlayer(this, play, false)) {
        return;
    }
    s16 angleToPlayer = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
    bool isSwingingFromSide =
        this->actor.xzDistToPlayer < 100.0f && player->meleeWeaponState != 0 && angleToPlayer >= 0x1F40;

    if (isSwingingFromSide) {
        Pirate_FacePlayer(this);
        Pirate_SetupCircle(this);
        return;
    }
    if (!Actor_IsFacingPlayer(&this->actor, 0xBB8)) {
        Pirate_SetupPivot(this);
        return;
    }
    bool isAtLungeRange = this->actor.xzDistToPlayer < 400.0f && this->actor.xzDistToPlayer > 150.0f;

    if (isAtLungeRange && Rand_ZeroOne() < 0.7f) {
        if (Rand_ZeroOne() > 0.5f || ABS(angleToPlayer) < 0x3000) {
            Pirate_SetupRollForward(this);
        } else {
            Pirate_SetupSpinAttack(this);
        }
    } else if (Rand_ZeroOne() > 0.1f) {
        Pirate_SetupAdvance(this);
    } else {
        Pirate_SetupCircle(this);
    }
}

static void Pirate_SetupSpinDodge(EnPirate* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 side = Math_SinS(player->actor.shape.rot.y - this->actor.shape.rot.y);

    Pirate_ChangeAnim(this, PIRATE_ANIM_SIDESTEP);
    if (side > 0.0f) {
        this->actor.speedXZ = -10.0f;
    } else if (side < 0.0f || Rand_ZeroOne() > 0.5f) {
        this->actor.speedXZ = 10.0f;
    } else {
        this->actor.speedXZ = -10.0f;
    }
    this->skelAnime.playSpeed = 1.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->combatTimer = 6;
    this->circlingRate = 0.0f;
    this->action = PIRATE_ACTION_SPIN_DODGE;
    this->actionFunc = Pirate_SpinDodge;
}

// Sidestepping into a wall or off a ledge turns the move around, or into a flip when there is no way out.
static bool Pirate_IsSidestepBlocked(EnPirate* this, PlayState* play, bool isDodge) {
    bool hasWall = (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) != 0;
    bool hasFloor =
        Actor_TestFloorInDirection(&this->actor, play, this->actor.speedXZ, this->actor.shape.rot.y + 0x4000);

    if (!hasWall && hasFloor) {
        return false;
    }
    s16 wallAngle = 0;

    if (hasWall) {
        s16 stepYaw = this->actor.shape.rot.y + (this->actor.speedXZ >= 0.0f ? 0x4000 : -0x4000);

        wallAngle = this->actor.wallYaw - stepYaw;
    } else {
        this->actor.speedXZ *= -0.8f;
    }
    if (ABS(wallAngle) <= 0x4000) {
        return false;
    }
    if (isDodge) {
        Pirate_SetupJump(this);
        return true;
    }
    this->actor.speedXZ *= -0.8f;
    this->actor.speedXZ += this->actor.speedXZ < 0.0f ? -0.5f : 0.5f;
    return false;
}

static void Pirate_KeepCirclingDistance(EnPirate* this, s16 yaw) {
    if (this->actor.xzDistToPlayer <= 65.0f) {
        Math_ApproachF(&this->circlingRate, -4.0f, 1.0f, 1.5f);
    } else if (this->actor.xzDistToPlayer > 40.0f) {
        Math_ApproachF(&this->circlingRate, 4.0f, 1.0f, 1.5f);
    } else {
        Math_ApproachZeroF(&this->circlingRate, 1.0f, 6.65f);
    }
    if (this->circlingRate != 0.0f) {
        this->actor.world.pos.x += Math_SinS(yaw) * this->circlingRate;
        this->actor.world.pos.z += Math_CosS(yaw) * this->circlingRate;
    }
}

static void Pirate_SpinDodge(EnPirate* this, PlayState* play) {
    this->actor.world.rot.y = this->actor.yawTowardsPlayer + 0x3A98;
    if (Pirate_IsSidestepBlocked(this, play, true)) {
        return;
    }
    Pirate_KeepCirclingDistance(this, this->actor.yawTowardsPlayer);
    this->skelAnime.playSpeed = 1.0f;
    PlayStepOnFrames(this, 5);
    PlayBreath(this, play);
    if (this->combatTimer != 0) {
        this->actor.shape.rot.y += this->actor.speedXZ >= 0.0f ? 0x4000 : -0x4000;
        return;
    }
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    if (Pirate_DodgeRanged(this, play)) {
        return;
    }
    if (this->actor.xzDistToPlayer <= 70.0f) {
        Pirate_SetupSlash(this);
    } else {
        Pirate_SetupRollBack(this);
    }
}

static void Pirate_SetupBlock(EnPirate* this) {
    Pirate_ChangeAnim(this, PIRATE_ANIM_BLOCK);
    if (this->swordState != 0) {
        this->swordState = -1;
    }
    this->animationsDisabled = false;
    this->actor.speedXZ = 0.0f;
    this->combatTimer = Rand_S16Offset(10, 10);
    this->bodyCollider.base.acFlags |= AC_HARD;
    this->lookTimer = 12;
    this->action = PIRATE_ACTION_BLOCK;
    this->actionFunc = Pirate_Block;
}

static void Pirate_LowerGuard(EnPirate* this) {
    this->bodyCollider.base.acFlags &= ~AC_HARD;
}

static void Pirate_CounterFromGuard(EnPirate* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 angleToPlayer = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
    bool isPlayerClose =
        ABS(angleToPlayer) <= 0x4000 && this->actor.xzDistToPlayer < 40.0f && fabsf(this->actor.yDistToPlayer) < 50.0f;

    if (!isPlayerClose) {
        Pirate_LowerGuard(this);
        Pirate_SetupCircle(this);
        return;
    }
    if (func_800354B4(play, &this->actor, 100.0f, 10000, 0x4000, this->actor.shape.rot.y)) {
        if (player->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_START) {
            Pirate_LowerGuard(this);
            Pirate_SetupSpinDodge(this, play);
        } else if (play->gameplayFrames % 2 == 0) {
            Pirate_SetupBlock(this);
        } else {
            Pirate_LowerGuard(this);
            Pirate_SetupRollBack(this);
        }
        return;
    }
    s16 facing = player->actor.shape.rot.y - this->actor.shape.rot.y;

    Pirate_LowerGuard(this);
    if (ABS(facing) < 0x4000) {
        Pirate_SetupSlash(this);
    } else {
        Pirate_SetupCircle(this);
    }
}

static void Pirate_Block(EnPirate* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->animationsDisabled = true;
    if (this->combatTimer == 0) {
        this->skelAnime.playSpeed = 1.0f;
    }
    if (this->lookTimer == 0) {
        this->animationsDisabled = false;
        Pirate_CounterFromGuard(this, play);
        return;
    }
    bool isAttacked =
        this->combatTimer == 0 && func_800354B4(play, &this->actor, 100.0f, 10000, 0x4000, this->actor.shape.rot.y);

    if (!isAttacked) {
        return;
    }
    if (player->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_START) {
        Pirate_LowerGuard(this);
        Pirate_SetupSpinDodge(this, play);
    } else if (Pirate_DodgeRanged(this, play)) {
        return;
    } else if (play->gameplayFrames & 1) {
        Pirate_SetupBlock(this);
    } else {
        Pirate_LowerGuard(this);
        if (this->actor.xzDistToPlayer < 100.0f && Rand_ZeroOne() > 0.7f) {
            Pirate_SetupJump(this);
        } else {
            Pirate_SetupRollBack(this);
        }
    }
}

static void Pirate_SetupJump(EnPirate* this) {
    Pirate_ChangeAnim(this, PIRATE_ANIM_FLIP);
    this->actor.speedXZ = 6.5f;
    this->actor.velocity.y = 15.0f;
    PlaySfx(&this->actor, MM_SE_EN_TEKU_JUMP, NA_SE_EN_STAL_JUMP);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    Pirate_SetIntangible(this);
    this->action = PIRATE_ACTION_JUMP;
    this->actionFunc = Pirate_Jump;
}

static void Pirate_SpawnFootDust(PlayState* play, Vec3f* pos) {
    static Color_RGBA8 sDustPrimColor = { 200, 160, 120, 255 };
    static Color_RGBA8 sDustEnvColor = { 130, 90, 50, 255 };
    static Vec3f sDustVelocity = { 0.0f, -1.5f, 0.0f };
    static Vec3f sDustAccel = { 0.0f, -0.2f, 0.0f };

    EffectSsKiraKira_SpawnSmall(play, pos, &sDustVelocity, &sDustAccel, &sDustPrimColor, &sDustEnvColor);
}

static void Pirate_Jump(EnPirate* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA0, 1);
    if (this->actor.velocity.y >= 5.0f) {
        Pirate_SpawnFootDust(play, &this->leftFootPos);
        Pirate_SpawnFootDust(play, &this->rightFootPos);
    }
    this->animationsDisabled = false;
    bool hasLanded = this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH);

    if (!Pirate_IsAnimDone(this) || !hasLanded) {
        return;
    }
    Pirate_SetHittable(this);
    this->swordCollider.info.elemType = ELEMTYPE_UNK2;
    this->actor.shape.rot.x = 0;
    Pirate_FacePlayer(this);
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.world.pos.y = this->actor.floorHeight;
    Pirate_SetupSlash(this);
}

static void Pirate_SetupRollBack(EnPirate* this) {
    Pirate_ChangeAnim(this, PIRATE_ANIM_FLIP);
    this->actor.speedXZ = -8.0f;
    PlaySfx(&this->actor, MM_SE_EN_TEKU_JUMP, NA_SE_EN_STAL_JUMP);
    Pirate_SetIntangible(this);
    this->action = PIRATE_ACTION_ROLL_BACK;
    this->actionFunc = Pirate_RollBack;
    Pirate_FacePlayer(this);
}

static void Pirate_RollBack(EnPirate* this, PlayState* play) {
    this->animationsDisabled = false;
    if (Pirate_IsAnimDone(this)) {
        bool isAtSpinRange = this->actor.xzDistToPlayer < 170.0f && this->actor.xzDistToPlayer > 140.0f;

        if (isAtSpinRange && Rand_ZeroOne() < 0.2f) {
            Pirate_SetupSpinAttack(this);
        } else {
            Pirate_SetupBlock(this);
        }
    }
    PlayBreath(this, play);
}

static void Pirate_SetupSlash(EnPirate* this) {
    Pirate_ChangeAnim(this, PIRATE_ANIM_SLASH);
    this->swordCollider.base.atFlags &= ~AT_BOUNCED;
    this->actor.speedXZ = 0.0f;
    StopBreathing(&this->actor);
    this->action = PIRATE_ACTION_SLASH;
    this->actionFunc = Pirate_Slash;
}

// After a swing she resets, guards, circles or strikes again depending on where Link stands.
static void Pirate_FollowUpAttack(EnPirate* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 playerFacing = ABS((s16)(player->actor.shape.rot.y - this->actor.shape.rot.y));
    s16 angleToPlayer = ABS((s16)(this->actor.yawTowardsPlayer - this->actor.shape.rot.y));

    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    if (0.4f < Rand_ZeroOne()) {
        Pirate_SetupBlock(this);
    } else if (playerFacing > 0x2710) {
        Pirate_SetupCircle(this);
    } else if (angleToPlayer > 0x4000) {
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        Pirate_SetupCircle(this);
    } else {
        Pirate_ReactToPlayer(this, play, true);
    }
}

static void Pirate_Slash(EnPirate* this, PlayState* play) {
    s16 angleToPlayer = ABS((s16)(this->actor.yawTowardsPlayer - this->actor.shape.rot.y));

    this->actor.speedXZ = 0.0f;
    if (Animation_OnFrame(&this->skelAnime, 1.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_PIRATE_ATTACK, NA_SE_EN_GERUDOFT_ATTACK);
        this->swordState = 1;
    } else if (Animation_OnFrame(&this->skelAnime, 6.0f)) {
        this->swordState = -1;
    }
    if (this->swordCollider.base.atFlags & AT_BOUNCED) {
        this->swordState = -1;
        this->swordCollider.base.atFlags &= ~(AT_BOUNCED | AT_HIT);
        Pirate_SetupRollBack(this);
        return;
    }
    this->animationsDisabled = false;
    if (!Pirate_IsAnimDone(this)) {
        return;
    }
    if (!Actor_IsFacingPlayer(&this->actor, 0x1554)) {
        Pirate_SetupReady(this);
        if (angleToPlayer > 0x4000) {
            this->lookTimer = 20;
        }
    } else if (Rand_ZeroOne() > 0.7f || this->actor.xzDistToPlayer >= 120.0f) {
        Pirate_SetupReady(this);
    } else {
        Pirate_FollowUpAttack(this, play);
    }
}

static void Pirate_SetupRollForward(EnPirate* this) {
    Pirate_ChangeAnim(this, PIRATE_ANIM_FLIP);
    this->combatTimer = 0;
    this->actor.speedXZ = 10.0f;
    Pirate_FacePlayer(this);
    PlaySfx(&this->actor, MM_SE_EN_TEKU_JUMP, NA_SE_EN_STAL_JUMP);
    this->action = PIRATE_ACTION_ROLL_FORWARD;
    this->actionFunc = Pirate_RollForward;
}

static void Pirate_RollForward(EnPirate* this, PlayState* play) {
    this->animationsDisabled = false;
    if (Pirate_IsAnimDone(this)) {
        this->actor.speedXZ = 0.0f;
        if (!Actor_IsFacingPlayer(&this->actor, 0x1554)) {
            Pirate_SetupReady(this);
            this->combatTimer = Rand_ZeroOne() * 5.0f + 5.0f;
        } else {
            Pirate_SetupSlash(this);
        }
    }
    PlayBreath(this, play);
}

static void Pirate_SetupAdvance(EnPirate* this) {
    Pirate_ChangeAnim(this, PIRATE_ANIM_WALK);
    this->action = PIRATE_ACTION_ADVANCE;
    this->actionFunc = Pirate_Advance;
}

static void Pirate_ApproachSlashRange(EnPirate* this) {
    if (this->actor.xzDistToPlayer <= 40.0f) {
        Math_ApproachF(&this->actor.speedXZ, -8.0f, 1.0f, 1.5f);
    } else if (this->actor.xzDistToPlayer > 55.0f) {
        Math_ApproachF(&this->actor.speedXZ, 8.0f, 1.0f, 1.5f);
    } else {
        Math_ApproachZeroF(&this->actor.speedXZ, 2.0f, 6.65f);
    }
}

static void Pirate_ChooseAdvanceMove(EnPirate* this, PlayState* play, s16 playerFacing) {
    if (!Actor_IsFacingPlayer(&this->actor, 0x11C7)) {
        if (Rand_ZeroOne() > 0.5f) {
            Pirate_SetupCircle(this);
        } else {
            Pirate_SetupReady(this);
        }
    } else if (this->actor.xzDistToPlayer < 90.0f) {
        if (Rand_ZeroOne() > 0.03f || (this->actor.xzDistToPlayer <= 65.0f && playerFacing < 0x4000)) {
            Pirate_SetupSlash(this);
        } else if (Actor_OtherIsTargeted(play, &this->actor) && Rand_ZeroOne() > 0.5f) {
            Pirate_SetupRollBack(this);
        } else {
            Pirate_SetupCircle(this);
        }
    }
}

static void Pirate_Advance(EnPirate* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (Pirate_DodgeRanged(this, play)) {
        return;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0x2EE, 0);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    Pirate_ApproachSlashRange(this);
    this->skelAnime.playSpeed = 1.0f;
    s16 playerFacing = ABS((s16)(player->actor.shape.rot.y - this->actor.shape.rot.y));
    bool isSwingingNearby =
        this->actor.xzDistToPlayer < 150.0f && player->meleeWeaponState != 0 && playerFacing >= 0x2000;

    if (isSwingingNearby) {
        Pirate_FacePlayer(this);
        if (Rand_ZeroOne() > 0.7f) {
            Pirate_SetupCircle(this);
            return;
        }
    }
    Pirate_ChooseAdvanceMove(this, play, playerFacing);
    if (Pirate_ReactToPlayer(this, play, false)) {
        return;
    }
    bool isAtLungeRange = this->actor.xzDistToPlayer < 210.0f && this->actor.xzDistToPlayer > 150.0f &&
                          Actor_IsFacingPlayer(&this->actor, 0x1388);

    if (isAtLungeRange) {
        if (!Actor_IsTargeted(play, &this->actor)) {
            Pirate_SetupCircle(this);
            return;
        }
        if (Rand_ZeroOne() > 0.5f) {
            Pirate_SetupRollForward(this);
        } else {
            Pirate_SetupSpinAttack(this);
        }
    }
    PlayBreath(this, play);
    PlayStepOnFrames(this, 4);
}

static void Pirate_SetupPivot(EnPirate* this) {
    Pirate_ChangeAnim(this, PIRATE_ANIM_SIDESTEP);
    this->action = PIRATE_ACTION_PIVOT;
    this->actionFunc = Pirate_Pivot;
}

static void Pirate_Pivot(EnPirate* this, PlayState* play) {
    if (Pirate_DodgeRanged(this, play) || Pirate_ReactToPlayer(this, play, false)) {
        return;
    }
    s16 angleToPlayer = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
    s16 turnRate = (s16)(angleToPlayer * 0.25f) + (angleToPlayer > 0 ? 0x7D0 : -0x7D0);

    this->actor.shape.rot.y += turnRate;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->skelAnime.playSpeed = 1.0f;
    if (Actor_IsFacingPlayer(&this->actor, 0x1388)) {
        if (Rand_ZeroOne() > 0.8f) {
            Pirate_SetupCircle(this);
        } else {
            Pirate_SetupAdvance(this);
        }
    }
    PlayBreath(this, play);
}

static void Pirate_SetupSpinAttack(EnPirate* this) {
    Pirate_ChangeAnim(this, PIRATE_ANIM_SPIN_ATTACK);
    this->swordCollider.base.atFlags &= ~(AT_BOUNCED | AT_HIT);
    this->isSpinBounced = false;
    this->action = PIRATE_ACTION_SPIN_ATTACK;
    this->actionFunc = Pirate_SpinAttack;
    this->actor.speedXZ = 0.0f;
}

// Lunges forward spinning with both blades; a shield bounce cuts it short into a flip back.
static void Pirate_SpinAttack(EnPirate* this, PlayState* play) {
    if (this->swordCollider.base.atFlags & AT_BOUNCED) {
        this->swordCollider.base.atFlags &= ~(AT_BOUNCED | AT_HIT);
        this->isSpinBounced = true;
        this->skelAnime.playSpeed = 1.0f;
    }
    if (this->skelAnime.curFrame <= 8.0f) {
        Pirate_FacePlayer(this);
    } else if (Animation_OnFrame(&this->skelAnime, 13.0f)) {
        Actor_SpawnFloorDustRing(play, &this->actor, &this->leftFootPos, 3.0f, 2, 2.0f, 0, 0, false);
        Actor_SpawnFloorDustRing(play, &this->actor, &this->rightFootPos, 3.0f, 2, 2.0f, 0, 0, false);
        this->swordState = 1;
        this->actor.speedXZ = 10.0f;
        PlaySfx(&this->actor, MM_SE_EN_PIRATE_ATTACK, NA_SE_EN_GERUDOFT_ATTACK);
    } else if (Animation_OnFrame(&this->skelAnime, 21.0f)) {
        this->actor.speedXZ = 0.0f;
    } else if (Animation_OnFrame(&this->skelAnime, 24.0f)) {
        this->swordState = -1;
    }
    this->animationsDisabled = false;
    if (!Pirate_IsAnimDone(this)) {
        return;
    }
    if (!Actor_IsFacingPlayer(&this->actor, 0x1554)) {
        Pirate_SetupReady(this);
        this->combatTimer = Rand_ZeroOne() * 5.0f + 5.0f;
        this->lookTimer = 46;
    } else if (this->isSpinBounced) {
        Pirate_SetupRollBack(this);
    } else if (Rand_ZeroOne() > 0.7f || this->actor.xzDistToPlayer >= 120.0f) {
        Pirate_SetupReady(this);
        this->combatTimer = Rand_ZeroOne() * 5.0f + 5.0f;
    } else {
        Pirate_FollowUpAttack(this, play);
    }
}

static void Pirate_SetupCircle(EnPirate* this) {
    Pirate_ChangeAnim(this, PIRATE_ANIM_SIDESTEP);
    this->actor.speedXZ = Rand_CenteredFloat(12.0f);
    this->skelAnime.playSpeed = 1.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->combatTimer = Rand_ZeroOne() * 30.0f + 30.0f;
    this->action = PIRATE_ACTION_CIRCLE;
    this->actionFunc = Pirate_Circle;
    this->circlingRate = 0.0f;
}

// Strafes around Link toward his back, keeping slash distance.
static void Pirate_Circle(EnPirate* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA0, 1);
    if (Pirate_DodgeRanged(this, play) || Pirate_ReactToPlayer(this, play, false)) {
        return;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x4000;
    s16 angleBehindPlayer = player->actor.shape.rot.y + 0x8000;
    f32 side = Math_SinS(angleBehindPlayer - this->actor.shape.rot.y);

    if (side >= 0.0f) {
        this->actor.speedXZ = MAX(this->actor.speedXZ - 0.25f, -8.0f);
    } else {
        this->actor.speedXZ = MIN(this->actor.speedXZ + 0.25f, 8.0f);
    }
    Pirate_IsSidestepBlocked(this, play, false);
    Pirate_KeepCirclingDistance(this, this->actor.shape.rot.y);
    PlayStepOnFrames(this, 5);
    PlayBreath(this, play);
    if (this->actor.xzDistToPlayer <= 65.0f) {
        Pirate_SetupSlash(this);
    } else if (this->combatTimer == 0) {
        if (Actor_OtherIsTargeted(play, &this->actor) && Rand_ZeroOne() > 0.5f) {
            Pirate_SetupRollBack(this);
        } else {
            Pirate_SetupReady(this);
        }
    }
}

static void Pirate_SetupStunned(EnPirate* this) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        this->actor.speedXZ = 0.0f;
    }
    if (this->action == PIRATE_ACTION_SPIN_ATTACK) {
        Pirate_ChangeAnim(this, PIRATE_ANIM_DAMAGE);
    }
    PlaySfx(&this->actor, MM_SE_EN_COMMON_FREEZE, NA_SE_EN_GOMA_JR_FREEZE);
    this->animationsDisabled = true;
    if (!Pirate_IsFrozen(this)) {
        this->iceTimer = 40;
    }
    this->action = PIRATE_ACTION_STUNNED;
    this->actionFunc = Pirate_Stunned;
}

static void Pirate_Stunned(EnPirate* this, PlayState* play) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        this->actor.speedXZ = 0.0f;
    }
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && this->actor.speedXZ < 0.0f) {
        this->actor.speedXZ += 0.05f;
    }
    bool isHooked = CHECK_FLAG_ALL(this->actor.flags, ACTOR_FLAG_HOOKSHOT_ATTACHED);
    bool hasRecovered =
        this->iceTimer == 0 && this->auraTimer == 0 && !isHooked && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);

    if (!hasRecovered) {
        return;
    }
    this->animationsDisabled = false;
    if (this->aura == PIRATE_AURA_FROZEN) {
        Pirate_ShatterIce(this, play);
    }
    Pirate_ReactToPlayer(this, play, true);
}

static void Pirate_PushAwayFromPlayer(EnPirate* this) {
    this->knockbackVelocity.x = Math_SinS(this->actor.yawTowardsPlayer) * -10.0f;
    this->knockbackVelocity.y = 0.0f;
    this->knockbackVelocity.z = Math_CosS(this->actor.yawTowardsPlayer) * -10.0f;
}

static void Pirate_SetupDamaged(EnPirate* this, PlayState* play) {
    Pirate_PushAwayFromPlayer(this);
    this->lookTimer = 0;
    this->animationsDisabled = false;
    this->actor.speedXZ = 0.0f;
    Pirate_ChangeAnim(this, PIRATE_ANIM_DAMAGE);
    if (Pirate_IsFrozen(this)) {
        Pirate_ShatterIce(this, play);
    }
    PlaySfx(&this->actor, MM_SE_EN_PIRATE_DAMAGE, NA_SE_EN_GERUDOFT_DAMAGE);
    this->action = PIRATE_ACTION_DAMAGED;
    this->actionFunc = Pirate_Damaged;
}

static void Pirate_Damaged(EnPirate* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0x1194, 0);
    if (Pirate_DodgeRanged(this, play) || Pirate_ReactToPlayer(this, play, false)) {
        return;
    }
    if (this->knockbackVelocity.x >= 1.0f || this->knockbackVelocity.z >= 1.0f) {
        return;
    }
    s16 angleToWall = this->actor.wallYaw - this->actor.shape.rot.y;
    bool isCornered = (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) && ABS(angleToWall) < 0x3000 &&
                      this->actor.xzDistToPlayer < 90.0f;

    if (isCornered) {
        Pirate_SetupJump(this);
    } else if (this->actor.xzDistToPlayer <= 65.0f && play->gameplayFrames % 8 != 0) {
        this->swordCollider.info.elemType = ELEMTYPE_UNK2;
        Pirate_SetupSlash(this);
    } else {
        Pirate_SetupRollBack(this);
    }
}

static void Pirate_SetupDefeatKnockdown(EnPirate* this, PlayState* play) {
    Pirate_PushAwayFromPlayer(this);
    Pirate_ChangeAnim(this, PIRATE_ANIM_DEFEAT);
    this->actor.speedXZ = 0.0f;
    this->animationsDisabled = true;
    Enemy_StartFinishingBlow(play, &this->actor);
    PlaySfx(&this->actor, MM_SE_EN_PIRATE_DEAD, NA_SE_EN_GERUDOFT_DEAD);
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER);
    this->action = PIRATE_ACTION_KNOCK_DOWN;
    this->actionFunc = Pirate_DefeatKnockdown;
}

static void Pirate_SetupVanish(EnPirate* this) {
    Pirate_ChangeAnim(this, PIRATE_ANIM_DEFEAT_IDLE);
    this->animationsDisabled = false;
    this->isBreathing = false;
    this->hideTimer = 40;
    this->vanishStep = PIRATE_VANISH_CATCH_BREATH;
    this->action = PIRATE_ACTION_VANISH;
    this->actionFunc = Pirate_Vanish;
}

static void Pirate_DefeatKnockdown(EnPirate* this, PlayState* play) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        this->actor.speedXZ = 0.0f;
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Math_SmoothStepToF(&this->actor.speedXZ, 0.0f, 1.0f, 0.5f, 0.0f);
    }
    if (Pirate_IsFrozen(this)) {
        Pirate_ShatterIce(this, play);
    }
    if (this->skelAnime.curFrame >= 25.0f) {
        this->knockbackVelocity = sZeroVec;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA0, 1);
    SkelAnime_Update(&this->skelAnime);
    if (Pirate_IsAnimDone(this)) {
        Pirate_SetupVanish(this);
    } else if (Animation_OnFrame(&this->skelAnime, 10.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_GERUDOFT_DOWN, NA_SE_EN_RIZA_DOWN);
    }
}

static void Pirate_CatchBreath(EnPirate* this) {
    if (Pirate_IsAnimDone(this)) {
        if (!this->isBreathing) {
            PlaySfx(&this->actor, MM_SE_EN_PIRATE_DAMM_BREATH, NA_SE_EN_GERUDOFT_BREATH);
            this->isBreathing = true;
        } else {
            PlaySfx(&this->actor, MM_SE_EN_PIRATE_BREATH, NA_SE_EN_GERUDOFT_BREATH);
        }
    }
    if (this->hideTimer == 0) {
        Pirate_ChangeAnim(this, PIRATE_ANIM_THROW_FLASH);
        this->flashTimer = 0;
        this->vanishStep = PIRATE_VANISH_THROW_FLASH;
    }
}

// MM's escape: a thrown Deku Nut whites out the screen and she is gone when it clears.
static void Pirate_ThrowFlash(EnPirate* this, PlayState* play) {
    this->flashTimer++;
    if (this->skelAnime.curFrame >= 1.0f) {
        Math_ApproachZeroF(&this->swordScaleRight, 0.3f, 0.3f);
        Math_ApproachZeroF(&this->swordScaleLeft, 0.3f, 0.3f);
    }
    if (this->flashTimer == PIRATE_FLASH_FRAME) {
        PlaySfx(&this->actor, MM_SE_EN_PIRATE_SHOUT, NA_SE_IT_DEKU);
    }
    if (this->flashTimer >= PIRATE_FLASH_FRAME) {
        Math_ApproachF(&this->flashAlpha, 60.0f, 1.0f, 20.0f);
        Pirate_SetFlash(this, play);
    }
    if (Pirate_IsAnimDone(this) && this->flashTimer >= 40) {
        this->actor.draw = NULL;
        this->hideTimer = 10;
        this->vanishStep = PIRATE_VANISH_FADE;
    }
}

static void Pirate_FadeFlash(EnPirate* this, PlayState* play) {
    if (this->hideTimer != 0) {
        return;
    }
    Math_ApproachZeroF(&this->flashAlpha, 0.5f, 10.0f);
    Pirate_SetFlash(this, play);
    if (play->envCtx.screenFillColor[3] >= 10) {
        return;
    }
    Pirate_StopFlash(this, play);
    Pirate_StopMinibossMusic(this);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

static void Pirate_Vanish(EnPirate* this, PlayState* play) {
    switch (this->vanishStep) {
        case PIRATE_VANISH_CATCH_BREATH:
            Pirate_CatchBreath(this);
            break;
        case PIRATE_VANISH_THROW_FLASH:
            Pirate_ThrowFlash(this, play);
            break;
        case PIRATE_VANISH_FADE:
            Pirate_FadeFlash(this, play);
            break;
        default:
            this->vanishStep = PIRATE_VANISH_FADE;
            break;
    }
}

static bool Pirate_IsRolling(EnPirate* this) {
    return this->action == PIRATE_ACTION_ROLL_FORWARD || this->action == PIRATE_ACTION_ROLL_BACK;
}

static bool Pirate_IsOpenToBlows(EnPirate* this) {
    return !Pirate_IsRolling(this) && this->action < PIRATE_ACTION_DAMAGED;
}

static bool Pirate_IsGuardingHits(EnPirate* this) {
    switch (this->action) {
        case PIRATE_ACTION_JUMP:
        case PIRATE_ACTION_BLOCK:
        case PIRATE_ACTION_DAMAGED:
        case PIRATE_ACTION_KNOCK_DOWN:
        case PIRATE_ACTION_VANISH:
            return true;
        default:
            return false;
    }
}

static void Pirate_Stun(EnPirate* this) {
    if (Pirate_IsFrozen(this)) {
        return;
    }
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 120, COLORFILTER_OPA, 40);
    Pirate_SetHittable(this);
    this->swordCollider.info.elemType = ELEMTYPE_UNK2;
    Pirate_SetupStunned(this);
}

static void Pirate_Freeze(EnPirate* this, PlayState* play) {
    if (Pirate_IsFrozen(this)) {
        return;
    }
    Actor_ApplyDamage(&this->actor);
    Pirate_SetHittable(this);
    this->swordCollider.info.elemType = ELEMTYPE_UNK4;
    Pirate_SetAura(this, PIRATE_AURA_FROZEN, 80);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 80);
    this->actor.flags &= ~ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    if (this->actor.colChkInfo.health == 0) {
        Pirate_SetupDefeatKnockdown(this, play);
    } else {
        Pirate_SetupStunned(this);
    }
}

static bool Pirate_DoesBlowLand(EnPirate* this, u8 effect) {
    switch (effect) {
        case PIRATE_DMGEFF_IFRAME_PROTECTED:
            return Pirate_IsOpenToBlows(this);
        case PIRATE_DMGEFF_ALWAYS_HIT:
            return true;
        case PIRATE_DMGEFF_STUNNED_ONLY:
            if (this->action == PIRATE_ACTION_STUNNED) {
                return true;
            }
            Pirate_SetupJump(this);
            return false;
        case PIRATE_DMGEFF_FIRE:
            if (!Pirate_IsOpenToBlows(this)) {
                return false;
            }
            Pirate_SetAura(this, PIRATE_AURA_FIRE, 40);
            return true;
        case PIRATE_DMGEFF_LIGHT:
            if (Pirate_IsFrozen(this)) {
                return false;
            }
            Pirate_SetAura(this, PIRATE_AURA_LIGHT, 20);
            return true;
        default:
            return false;
    }
}

static void Pirate_TakeBlow(EnPirate* this, PlayState* play) {
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    if (Actor_ApplyDamage(&this->actor) == 0) {
        Pirate_SetupDefeatKnockdown(this, play);
        return;
    }
    Pirate_SetHittable(this);
    this->swordCollider.info.elemType = ELEMTYPE_UNK4;
    Vec3f bloodPos = this->actor.focus.pos;

    for (s32 i = 0; i < 3; i++) {
        bloodPos.y += Rand_ZeroFloat(20.0f);
        CollisionCheck_BlueBlood(play, NULL, &bloodPos);
    }
    Pirate_SetupDamaged(this, play);
}

static void Pirate_ClangOffGuard(EnPirate* this, PlayState* play) {
    Vec3f hitPos = { this->bodyCollider.info.bumper.hitPos.x, this->bodyCollider.info.bumper.hitPos.y,
                     this->bodyCollider.info.bumper.hitPos.z };

    this->bodyCollider.base.acFlags &= ~AC_HIT;
    PlaySfx(&this->actor, MM_SE_IT_SHIELD_BOUND, NA_SE_IT_SHIELD_BOUND);
    EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_METAL, &hitPos);
    CollisionCheck_SpawnShieldParticlesMetal(play, &hitPos);
}

// A drawn pirate under attack: her guard clangs blows away, the rest stun, freeze or wound her.
static void Pirate_UpdateDamage(EnPirate* this, PlayState* play) {
    bool isSwordHittingDynaWall =
        (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) && this->actor.wallBgId != BGCHECK_SCENE;
    bool isWandering = this->action == PIRATE_ACTION_SPIN_DODGE || this->action == PIRATE_ACTION_CIRCLE ||
                       this->action == PIRATE_ACTION_ADVANCE || this->action == PIRATE_ACTION_READY;

    bool hasCutPlayer = (this->swordCollider.base.atFlags & AT_HIT) && !(this->swordCollider.base.atFlags & AT_BOUNCED);

    if (hasCutPlayer) {
        this->swordCollider.base.atFlags &= ~AT_HIT;
        if (this->action == PIRATE_ACTION_SPIN_ATTACK && this->swordCollider.base.at == &GET_PLAYER(play)->actor) {
            Actor_SetPlayerKnockbackLargeNoDamage(play, &this->actor, 3.0f, this->actor.yawTowardsPlayer, 1.0f);
            Health_ChangeBy(play, -PIRATE_SPIN_EXTRA_DAMAGE);
        }
    }
    if (isSwordHittingDynaWall && isWandering) {
        Pirate_SetupSlash(this);
    }
    if (!(this->bodyCollider.base.acFlags & AC_HIT)) {
        return;
    }
    if (Pirate_IsGuardingHits(this)) {
        if (this->action == PIRATE_ACTION_BLOCK) {
            Pirate_ClangOffGuard(this, play);
        }
        return;
    }
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    if (this->action <= PIRATE_ACTION_UNSHEATHE) {
        Pirate_StartFight(this);
    }
    Actor_SetDropFlag(&this->actor, &this->bodyCollider.info, false);
    StopBreathing(&this->actor);
    u8 effect = this->actor.colChkInfo.damageEffect;

    if (effect == PIRATE_DMGEFF_STUN) {
        Pirate_Stun(this);
    } else if (effect == PIRATE_DMGEFF_FREEZE) {
        Pirate_Freeze(this, play);
    } else if (Pirate_DoesBlowLand(this, effect)) {
        Pirate_TakeBlow(this, play);
    }
}

static void Pirate_UpdateBlink(EnPirate* this) {
    if (this->blinkTimer != 0) {
        return;
    }
    this->eyeIndex++;
    if (this->eyeIndex >= ARRAY_COUNT(sEyeTextures)) {
        this->eyeIndex = 0;
        this->blinkTimer = Rand_S16Offset(20, 60);
    }
}

static void EnPirate_Init(Actor* thisx, PlayState* play) {
    EnPirate* this = (EnPirate*)thisx;
    EffectBlureInit1 blureInit = { 0 };

    this->actor.targetMode = 3;
    this->actor.colChkInfo.mass = 80;
    this->actor.colChkInfo.health = PIRATE_HEALTH;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    this->actor.gravity = -2.0f;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawFeet, 90.0f);
    bool hasColorParam = this->actor.params >= 0 && this->actor.params < PIRATE_COLOR_COUNT;

    this->colorType = hasColorParam ? this->actor.params : (s32)Rand_ZeroFloat(PIRATE_COLOR_COUNT);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sPirateSkel, (AnimationHeader*)sWalkAnim,
                       this->jointTable, this->morphTable, PIRATE_LIMB_MAX);
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyCylinderInit);
    Collider_InitQuad(play, &this->swordCollider);
    Collider_SetQuad(play, &this->swordCollider, &this->actor, &sSwordQuadInit);
    for (s32 i = 0; i < 4; i++) {
        blureInit.p1StartColor[i] = 255;
        blureInit.p2StartColor[i] = i == 3 ? 64 : 255;
        blureInit.p1EndColor[i] = i == 3 ? 0 : 255;
        blureInit.p2EndColor[i] = i == 3 ? 0 : 255;
    }
    blureInit.elemDuration = 8;
    blureInit.calcMode = 2;
    Effect_Add(play, &this->blureIndex, EFFECT_BLURE1, 0, 0, &blureInit);
    Actor_SetScale(&this->actor, PIRATE_SCALE);
    this->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    Pirate_SetupLurk(this);
}

static void EnPirate_Destroy(Actor* thisx, PlayState* play) {
    EnPirate* this = (EnPirate*)thisx;

    Effect_Delete(play, this->blureIndex);
    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyQuad(play, &this->swordCollider);
    Pirate_StopFlash(this, play);
    Pirate_StopMinibossMusic(this);
}

static void EnPirate_Update(Actor* thisx, PlayState* play) {
    EnPirate* this = (EnPirate*)thisx;
    bool isVanishing = this->action == PIRATE_ACTION_VANISH;

    if (!this->animationsDisabled) {
        SkelAnime_Update(&this->skelAnime);
    }
    Pirate_TurnHead(this);
    DECR(this->blinkTimer);
    DECR(this->combatTimer);
    DECR(this->lookTimer);
    DECR(this->iceTimer);
    DECR(this->hideTimer);
    DECR(this->auraTimer);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    Actor_MoveXZGravity(&this->actor);
    if (!isVanishing) {
        Pirate_UpdateDamage(this, play);
    }
    Actor_SetFocus(&this->actor, 60.0f);
    Actor_SetScale(&this->actor, PIRATE_SCALE);
    Pirate_UpdateBlink(this);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        this->actor.world.pos.x += this->knockbackVelocity.x;
        this->actor.world.pos.z += this->knockbackVelocity.z;
        Math_ApproachZeroF(&this->knockbackVelocity.x, 1.0f, 2.0f);
        Math_ApproachZeroF(&this->knockbackVelocity.z, 1.0f, 2.0f);
    }
    Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 40.0f, 35.0f, BGCHECK_UPDATE_WALL_FLOOR_WATER);
    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    if (!isVanishing) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    if (this->swordState > 0) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->swordCollider.base);
    }
}

static s32 Pirate_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnPirate* this = (EnPirate*)thisx;
    const Color_RGBA8* color =
        limbIndex == PIRATE_LIMB_HEAD ? &sLipstickColors[this->colorType] : &sOutfitColors[this->colorType];

    if (limbIndex == PIRATE_LIMB_VEIL) {
        *dList = NULL;
    }
    if (limbIndex == PIRATE_LIMB_NECK) {
        rot->z += this->headRot.x;
        rot->x += this->headRot.y;
        rot->y += this->headRot.z;
        return false;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, color->r, color->g, color->b, 255);
    CLOSE_DISPS(play->state.gfxCtx);
    return false;
}

// The sword quad is rebuilt from locals: the setter copies into the very slots it would read from.
static void Pirate_UpdateSword(EnPirate* this) {
    Vec3f quad[4];
    Vec3f tip;
    Vec3f hilt;

    for (s32 i = 0; i < 4; i++) {
        Matrix_MultVec3f(&sSwordQuadOffsets[i], &quad[i]);
    }
    Collider_SetQuadVertices(&this->swordCollider, &quad[0], &quad[1], &quad[2], &quad[3]);
    Matrix_MultVec3f(&sSwordTipOffset, &tip);
    Matrix_MultVec3f(&sSwordHiltOffset, &hilt);
    bool isSwinging = this->action == PIRATE_ACTION_SLASH || this->action == PIRATE_ACTION_SPIN_ATTACK;

    if (this->swordState == 1 && isSwinging) {
        EffectBlure_AddVertex(Effect_GetByIndex(this->blureIndex), &tip, &hilt);
    } else if (this->swordState != 0) {
        EffectBlure_AddSpace(Effect_GetByIndex(this->blureIndex));
        this->swordState = 0;
    }
}

static bool IsPirateBodyPart(s32 limbIndex) {
    switch (limbIndex) {
        case PIRATE_LIMB_TORSO:
        case PIRATE_LIMB_HEAD:
        case PIRATE_LIMB_RIGHT_UPPER_ARM:
        case PIRATE_LIMB_RIGHT_FOREARM:
        case PIRATE_LIMB_RIGHT_HAND:
        case PIRATE_LIMB_LEFT_UPPER_ARM:
        case PIRATE_LIMB_LEFT_FOREARM:
        case PIRATE_LIMB_LEFT_HAND:
        case PIRATE_LIMB_LEFT_THIGH:
        case PIRATE_LIMB_LEFT_SHIN:
        case PIRATE_LIMB_LEFT_FOOT:
        case PIRATE_LIMB_RIGHT_THIGH:
        case PIRATE_LIMB_RIGHT_SHIN:
        case PIRATE_LIMB_RIGHT_FOOT:
        case PIRATE_LIMB_WAIST:
            return true;
        default:
            return false;
    }
}

static void Pirate_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnPirate* this = (EnPirate*)thisx;

    if (limbIndex == PIRATE_LIMB_RIGHT_SWORD) {
        Pirate_UpdateSword(this);
    } else {
        Actor_SetFeetPos(&this->actor, limbIndex, PIRATE_LIMB_LEFT_FOOT, &sFootOffset, PIRATE_LIMB_RIGHT_FOOT,
                         &sFootOffset);
    }
    if (limbIndex == PIRATE_LIMB_LEFT_FOOT) {
        Matrix_MultVec3f(&sFootOffset, &this->leftFootPos);
    } else if (limbIndex == PIRATE_LIMB_RIGHT_FOOT) {
        Matrix_MultVec3f(&sFootOffset, &this->rightFootPos);
    }
    if (IsPirateBodyPart(limbIndex)) {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[this->bodyPartIndex]);
        this->bodyPartIndex = (this->bodyPartIndex + 1) % PIRATE_BODYPART_MAX;
    }
}

static void Pirate_TransformLimbDraw(PlayState* play, s32 limbIndex, void* thisx) {
    EnPirate* this = (EnPirate*)thisx;

    if (limbIndex == PIRATE_LIMB_RIGHT_SWORD) {
        Matrix_Scale(this->swordScaleRight, this->swordScaleRight, this->swordScaleRight, MTXMODE_APPLY);
    } else if (limbIndex == PIRATE_LIMB_LEFT_SWORD) {
        Matrix_Scale(this->swordScaleLeft, this->swordScaleLeft, this->swordScaleLeft, MTXMODE_APPLY);
    }
}

// MM's fire and light damage effects become OoT's flame billboard tinted per element.
static void Pirate_DrawAura(EnPirate* this, PlayState* play) {
    bool isLight = this->aura == PIRATE_AURA_LIGHT;
    u8 alpha = CLAMP_MAX(this->auraTimer * 0.05f, 1.0f) * 255.0f;
    MtxF billboard;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_RotateY(BINANG_TO_RAD(BINANG_ROT180(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)))), MTXMODE_NEW);
    Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
    Matrix_Get(&billboard);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                           (play->gameplayFrames * -20) % 512, 0x20, 0x80));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, isLight ? 255 : 0, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, isLight ? 255 : 10, isLight ? 100 : 0, 255);
    for (s32 i = 0; i < PIRATE_BODYPART_MAX; i++) {
        billboard.xw = this->bodyPartsPos[i].x;
        billboard.yw = this->bodyPartsPos[i].y;
        billboard.zw = this->bodyPartsPos[i].z;
        Matrix_Put(&billboard);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnPirate_Draw(Actor* thisx, PlayState* play) {
    EnPirate* this = (EnPirate*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyeTextures[this->eyeIndex]);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawTransformFlexOpa(play, &this->skelAnime, Pirate_OverrideLimbDraw, Pirate_PostLimbDraw, Pirate_TransformLimbDraw,
                         this);
    bool hasFlames = this->aura == PIRATE_AURA_FIRE || this->aura == PIRATE_AURA_LIGHT;

    if (hasFlames && this->auraTimer != 0) {
        Pirate_DrawAura(this, play);
    }
}

// MM's CanKillEnemy(ACTOR_EN_KAIZOKU): a blade or Goron fists; the hammer stands in for the fists.
static bool Pirate_CanKillInLogic(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RG_MEGATON_HAMMER);
}

static void RegisterPirate(void) {
    SOHActorDefinition pirate = { 0 };

    pirate.structSize = sizeof(pirate);
    pirate.key = PIRATE_KEY;
    pirate.description = "Pirate";
    pirate.category = ACTORCAT_ENEMY;
    pirate.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    pirate.objectId = OBJECT_GAMEPLAY_KEEP;
    pirate.instanceSize = sizeof(EnPirate);
    pirate.init = EnPirate_Init;
    pirate.destroy = EnPirate_Destroy;
    pirate.update = EnPirate_Update;
    pirate.draw = EnPirate_Draw;
    pirate.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_MINIBOSS | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    pirate.enemyParams = -1;
    pirate.naviHint = "Pirate&%cShe blocks and dodges everything! Strike right after she slashes.%w";
    // Pirates are MM's Gerudo Fighters: they share OoT's soul, which Fleet pairs with MM's Soul of Pirates.
    pirate.soulKey = "oot.gerudo_fighter";
    pirate.logicKill = Pirate_CanKillInLogic;
    if (sApi->RegisterActor(&pirate) < 0) {
        sApi->Log("Could not register " PIRATE_KEY);
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
    RegisterPirate();
}
