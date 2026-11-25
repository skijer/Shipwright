// Majora's Mask's Garo (En_Jso) as an Unbound custom enemy, drawn from mm.o2r.
// It appears on its own once Link comes near, without En_Encount3's mask check, cutscene or ring of fire,
// and vanishes in green flames when beaten instead of giving its Ikana hint.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define GARO_KEY "mm.garo"
#define GARO_SOUL_ITEM_KEY "soul.mm.garo"
#define GORON_MASK_KEY "nei.goron_form_mask"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_JSO "__OTR__objects/object_jso/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_CEILING_FLOOR_WATER 0x1F

#define MM_SE_EN_ANSATSUSYA_ENTRY 0x3812
#define MM_SE_EN_ANSATSUSYA_MOVING 0x39BD
#define MM_SE_EN_ANSATSUSYA_DASH_2 0x39BE
#define MM_SE_EN_ANSATSUSYA_CRYING 0x39C0
#define MM_SE_EN_ANSATSUSYA_SKIP 0x39C1
#define MM_SE_EN_ANSATSUSYA_DAMAGE 0x39C5
#define MM_SE_EN_ANSATSUSYA_DEAD 0x39C6
#define MM_SE_EN_ANSATSUSYA_SWORD 0x39C7
#define MM_SE_EN_COMMON_EXTINCT_LEV 0x3A1F

#define GARO_HEALTH 3
#define GARO_SCALE 0.035f
#define GARO_WAKE_DISTANCE 300.0f
#define GARO_AFTERIMAGE_COUNT 20
#define GARO_BODYPART_MAX 12

typedef enum {
    GARO_LIMB_NONE,
    GARO_LIMB_ROOT,
    GARO_LIMB_TORSO,
    GARO_LIMB_LEFT_ARM,
    GARO_LIMB_LEFT_SWORD,
    GARO_LIMB_RIGHT_ARM,
    GARO_LIMB_RIGHT_SWORD,
    GARO_LIMB_ROBE_TOP,
    GARO_LIMB_ROBE_BACK,
    GARO_LIMB_ROBE_LEFT,
    GARO_LIMB_ROBE_RIGHT,
    GARO_LIMB_ROBE_FRONT,
    GARO_LIMB_HEAD,
    GARO_LIMB_LOWER_BODY_ROOT,
    GARO_LIMB_RIGHT_THIGH,
    GARO_LIMB_RIGHT_SHIN,
    GARO_LIMB_RIGHT_FOOT,
    GARO_LIMB_LEFT_THIGH,
    GARO_LIMB_LEFT_SHIN,
    GARO_LIMB_LEFT_FOOT,
    GARO_LIMB_MAX
} GaroLimb;

typedef enum {
    GARO_ACTION_WAIT,
    GARO_ACTION_INTRO,
    GARO_ACTION_REAPPEAR,
    GARO_ACTION_CIRCLE_PLAYER,
    GARO_ACTION_GUARD,
    GARO_ACTION_SPIN_BEFORE_ATTACK,
    GARO_ACTION_DASH_ATTACK,
    GARO_ACTION_SLASH,
    GARO_ACTION_WAIT_AFTER_SLASH,
    GARO_ACTION_STUNNED,
    GARO_ACTION_KNOCKED_BACK,
    GARO_ACTION_COWER,
    GARO_ACTION_DAMAGED,
    GARO_ACTION_JUMP_BACK,
    GARO_ACTION_DEAD,
    GARO_ACTION_FALL_DOWN
} GaroAction;

typedef enum {
    GARO_INTRO_SPIN_UP_FROM_GROUND,
    GARO_INTRO_JUMP_OUT_FROM_GROUND,
    GARO_INTRO_LAND_FROM_ABOVE,
    GARO_INTRO_SCALE_UP
} GaroIntroType;

typedef enum {
    GARO_INTRO_STATE_APPEAR,
    GARO_INTRO_STATE_DRAW_RIGHT_SWORD,
    GARO_INTRO_STATE_DRAW_LEFT_SWORD,
    GARO_INTRO_STATE_ENDING
} GaroIntroState;

typedef enum {
    GARO_SWORDS_BOTH_DRAWN,
    GARO_SWORDS_KNOCKED_OUT,
    GARO_SWORDS_RIGHT_DRAWN,
    GARO_SWORDS_NONE_DRAWN
} GaroSwordState;

typedef enum {
    GARO_ANIM_APPEAR,
    GARO_ANIM_IDLE,
    GARO_ANIM_BOUNCE,
    GARO_ANIM_GUARD,
    GARO_ANIM_DASH_ATTACK,
    GARO_ANIM_SLASH_START,
    GARO_ANIM_SLASH_LOOP,
    GARO_ANIM_KNOCKED_BACK,
    GARO_ANIM_COWER,
    GARO_ANIM_DAMAGED,
    GARO_ANIM_JUMP_BACK,
    GARO_ANIM_FALL_DOWN,
    GARO_ANIM_MAX
} GaroAnimation;

typedef enum {
    GARO_DMGEFF_IMMUNE,
    GARO_DMGEFF_STUN,
    GARO_DMGEFF_FIRE,
    GARO_DMGEFF_FREEZE,
    GARO_DMGEFF_LIGHT,
    GARO_DMGEFF_NONE = 0xF
} GaroDamageEffect;

typedef enum {
    GARO_AURA_NONE,
    GARO_AURA_FIRE,
    GARO_AURA_FROZEN,
    GARO_AURA_LIGHT
} GaroAura;

struct EnGaro;

typedef void (*EnGaroActionFunc)(struct EnGaro*, PlayState*);

typedef struct EnGaro {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[GARO_LIMB_MAX];
    Vec3s morphTable[GARO_LIMB_MAX];
    EnGaroActionFunc actionFunc;
    s16 action;
    s16 circlingAngle;
    s16 circlingAngularVelocity;
    s16 attackMovementTimer;
    s16 attackTimer;
    s16 timer;
    s16 introType;
    s16 introState;
    s16 introTimer;
    s16 auraTimer;
    s16 aura;
    s16 bodyPartIndex;
    s16 afterimageIndex;
    s32 afterimageCount;
    bool isAttacking;
    bool disableBlure;
    bool slashHitSomething;
    u8 swordState;
    Vec3s robeRightRot;
    Vec3s targetRobeRightRot;
    Vec3s rightArmRot;
    Vec3s targetRightArmRot;
    f32 animEndFrame;
    f32 scale;
    Vec3f bodyPartsPos[GARO_BODYPART_MAX];
    Vec3f knockbackVelocity;
    ColliderCylinder bodyCollider;
    ColliderQuad rightSwordCollider;
    ColliderQuad leftSwordCollider;
    s32 rightSwordBlureIndex;
    s32 leftSwordBlureIndex;
    Vec3f afterimagePos[GARO_AFTERIMAGE_COUNT];
    s16 afterimageYaw[GARO_AFTERIMAGE_COUNT];
    Vec3s afterimageJointTable[GARO_AFTERIMAGE_COUNT][GARO_LIMB_MAX];
} EnGaro;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sGaroSkel[] = OBJ_JSO "gGaroSkel";
static const ALIGN_ASSET(2) char sGaroAppearAnim[] = OBJ_JSO "gGaroAppearAnim";
static const ALIGN_ASSET(2) char sGaroIdleAnim[] = OBJ_JSO "gGaroIdleAnim";
static const ALIGN_ASSET(2) char sGaroBounceAnim[] = OBJ_JSO "gGaroBounceAnim";
static const ALIGN_ASSET(2) char sGaroGuardAnim[] = OBJ_JSO "gGaroGuardAnim";
static const ALIGN_ASSET(2) char sGaroDashAttackAnim[] = OBJ_JSO "gGaroDashAttackAnim";
static const ALIGN_ASSET(2) char sGaroSlashStartAnim[] = OBJ_JSO "gGaroSlashStartAnim";
static const ALIGN_ASSET(2) char sGaroSlashLoopAnim[] = OBJ_JSO "gGaroSlashLoopAnim";
static const ALIGN_ASSET(2) char sGaroKnockedBackAnim[] = OBJ_JSO "gGaroKnockedBackAnim";
static const ALIGN_ASSET(2) char sGaroCowerAnim[] = OBJ_JSO "gGaroCowerAnim";
static const ALIGN_ASSET(2) char sGaroDamagedAnim[] = OBJ_JSO "gGaroDamagedAnim";
static const ALIGN_ASSET(2) char sGaroJumpBackAnim[] = OBJ_JSO "gGaroJumpBackAnim";
static const ALIGN_ASSET(2) char sGaroFallDownAnim[] = OBJ_JSO "gGaroFallDownAnim";

static const char* const sAnimations[GARO_ANIM_MAX] = {
    sGaroAppearAnim,      sGaroIdleAnim,   sGaroBounceAnim,  sGaroGuardAnim,
    sGaroDashAttackAnim,  sGaroSlashStartAnim, sGaroSlashLoopAnim, sGaroKnockedBackAnim,
    sGaroCowerAnim,       sGaroDamagedAnim, sGaroJumpBackAnim, sGaroFallDownAnim,
};

static const u8 sAnimationModes[GARO_ANIM_MAX] = {
    ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_ONCE,
    ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_ONCE,
};

static const s16 sAfterimageAlpha[GARO_AFTERIMAGE_COUNT] = {
    128, 0, 0, 0, 0, 128, 0, 0, 0, 0, 128, 0, 0, 0, 0, 128, 0, 0, 0, 0,
};

static ColliderCylinderInit sBodyCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_HARD | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x04 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
    { 22, 55, 0, { 0, 0, 0 } },
};

static ColliderQuadInit sSwordQuadInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_QUAD },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x08 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
      BUMP_NONE,
      OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, GARO_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, GARO_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, GARO_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, GARO_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, GARO_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, GARO_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, GARO_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, GARO_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, GARO_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, GARO_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, GARO_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(4, GARO_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, GARO_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(4, GARO_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, GARO_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, GARO_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, GARO_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(4, GARO_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(4, GARO_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(4, GARO_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, GARO_DMGEFF_IMMUNE),
    /* Mirror Ray    */ DMG_ENTRY(0, GARO_DMGEFF_IMMUNE),
    /* Kokiri spin   */ DMG_ENTRY(1, GARO_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(4, GARO_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(2, GARO_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, GARO_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, GARO_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, GARO_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, GARO_DMGEFF_IMMUNE),
    /* Unblockable   */ DMG_ENTRY(0, GARO_DMGEFF_IMMUNE),
    /* Hammer jump   */ DMG_ENTRY(2, GARO_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, GARO_DMGEFF_IMMUNE),
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sSwordTipOffset = { 1600.0f, 0.0f, 0.0f };
static Vec3f sSwordTipQuadOffset = { 1700.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;
// Shared by every Garo, as in MM: only one attacks at a time.
static bool sIsAnyGaroAttacking;

static void Garo_CirclePlayer(EnGaro* this, PlayState* play);
static void Garo_SetupJumpBack(EnGaro* this);
static void Garo_SetupFallDown(EnGaro* this, PlayState* play);

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

static void Garo_ChangeAnim(EnGaro* this, s32 animIndex) {
    AnimationHeader* animation = (AnimationHeader*)sAnimations[animIndex];
    bool isSnap = animIndex == GARO_ANIM_GUARD || animIndex == GARO_ANIM_DAMAGED;

    this->animEndFrame = Animation_GetLastFrame(animation);
    Animation_Change(&this->skelAnime, animation, 1.0f, 0.0f, this->animEndFrame, sAnimationModes[animIndex],
                     isSnap ? 0.0f : -4.0f);
}

static bool Garo_IsAnimDone(EnGaro* this) {
    return this->skelAnime.curFrame >= this->animEndFrame;
}

static void Garo_SpawnDustRing(EnGaro* this, PlayState* play) {
    Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, this->actor.shape.shadowScale, 1, 8.0f, 500,
                             10, true);
}

static void Garo_PushAwayFromPlayer(EnGaro* this, f32 speed) {
    this->knockbackVelocity.x = Math_SinS(this->actor.yawTowardsPlayer) * -speed;
    this->knockbackVelocity.y = 0.0f;
    this->knockbackVelocity.z = Math_CosS(this->actor.yawTowardsPlayer) * -speed;
}

static void Garo_StopAttacking(EnGaro* this) {
    if (this->isAttacking) {
        this->isAttacking = false;
        sIsAnyGaroAttacking = false;
    }
}

static bool Garo_IsFrozen(EnGaro* this) {
    return this->aura == GARO_AURA_FROZEN && this->auraTimer != 0;
}

static void Garo_ShatterIce(EnGaro* this, PlayState* play) {
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < GARO_BODYPART_MAX; i++) {
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &this->bodyPartsPos[i], 150, 150, 150, 250, 235, 245, 255,
                                       0.3f);
    }
    this->auraTimer = 0;
    this->aura = GARO_AURA_NONE;
}

static void Garo_WaitForPlayer(EnGaro* this, PlayState* play) {
    if (this->actor.xzDistToPlayer > GARO_WAKE_DISTANCE) {
        return;
    }
    Garo_ChangeAnim(this, GARO_ANIM_IDLE);
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_ENTRY, NA_SE_EN_STAL_REBORN);
    this->actor.shape.yOffset = 970.0f;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    switch (this->introType) {
        case GARO_INTRO_SPIN_UP_FROM_GROUND:
            this->actor.shape.yOffset = -300.0f;
            break;
        case GARO_INTRO_JUMP_OUT_FROM_GROUND:
            Garo_ChangeAnim(this, GARO_ANIM_APPEAR);
            Garo_SpawnDustRing(this, play);
            break;
        case GARO_INTRO_LAND_FROM_ABOVE:
            this->actor.world.pos.y = this->actor.home.pos.y + 200.0f;
            break;
        default:
            this->scale = 0.0f;
            break;
    }
    this->introState = GARO_INTRO_STATE_APPEAR;
    this->action = GARO_ACTION_INTRO;
}

static bool Garo_UpdateAppearance(EnGaro* this, PlayState* play) {
    Math_ApproachF(&this->actor.shape.shadowScale, 16.0f, 0.4f, 4.0f);
    switch (this->introType) {
        case GARO_INTRO_SPIN_UP_FROM_GROUND:
            Math_ApproachF(&this->actor.shape.yOffset, 970.0f, 0.5f, 100.0f);
            this->actor.shape.rot.y += 0x1770;
            if ((play->gameplayFrames % 4) == 0) {
                Garo_SpawnDustRing(this, play);
            }
            return this->actor.shape.yOffset >= 969.0f;
        case GARO_INTRO_JUMP_OUT_FROM_GROUND:
            return Garo_IsAnimDone(this);
        case GARO_INTRO_LAND_FROM_ABOVE:
            return (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
        default:
            Math_ApproachF(&this->scale, GARO_SCALE, 0.2f, 0.03f);
            return this->scale >= 0.034f;
    }
}

static void Garo_FinishIntro(EnGaro* this) {
    this->rightArmRot = (Vec3s){ 0, 0, 0 };
    this->robeRightRot = (Vec3s){ 0, 0, 0 };
    this->actor.flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    Garo_SetupJumpBack(this);
}

// MM's intro without its camera or lines: rise, raise the right arm and draw one blade, then the other.
static void Garo_Intro(EnGaro* this, PlayState* play) {
    this->introTimer++;
    switch (this->introState) {
        case GARO_INTRO_STATE_APPEAR:
            if (Garo_UpdateAppearance(this, play)) {
                Garo_SpawnDustRing(this, play);
                this->scale = GARO_SCALE;
                this->actor.shape.yOffset = 970.0f;
                this->targetRobeRightRot.x = -0x2710;
                this->targetRobeRightRot.z = -0x2710;
                this->introTimer = 0;
                this->introState++;
            }
            break;
        case GARO_INTRO_STATE_DRAW_RIGHT_SWORD:
            Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xBB8, 0);
            if (this->swordState == GARO_SWORDS_NONE_DRAWN &&
                ABS((s16)(this->targetRobeRightRot.x - this->robeRightRot.x)) < 0x64) {
                this->swordState = GARO_SWORDS_RIGHT_DRAWN;
                this->targetRightArmRot = (Vec3s){ -0x32C8, -0xBB8, -0x32C8 };
                this->rightArmRot.z = -0x32C8;
                PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_SWORD, NA_SE_IT_SWORD_PICKOUT);
            }
            if (this->introTimer >= 30) {
                this->targetRightArmRot = (Vec3s){ 0, 0, 0 };
                this->targetRobeRightRot = (Vec3s){ 0, 0, 0 };
                this->introTimer = 0;
                this->introState++;
            }
            break;
        case GARO_INTRO_STATE_DRAW_LEFT_SWORD:
            if (this->introTimer >= 10) {
                this->swordState = GARO_SWORDS_BOTH_DRAWN;
                PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_SWORD, NA_SE_IT_SWORD_PICKOUT);
                Garo_ChangeAnim(this, GARO_ANIM_IDLE);
                this->introTimer = 0;
                this->introState++;
            }
            break;
        default:
            if (this->introTimer >= 10) {
                Garo_FinishIntro(this);
                return;
            }
            break;
    }
    Math_SmoothStepToS(&this->rightArmRot.x, this->targetRightArmRot.x, 1, 0xBB8, 0);
    Math_SmoothStepToS(&this->rightArmRot.y, this->targetRightArmRot.y, 1, 0xBB8, 0);
    Math_SmoothStepToS(&this->rightArmRot.z, this->targetRightArmRot.z, 1, 0xBB8, 0);
    Math_SmoothStepToS(&this->robeRightRot.x, this->targetRobeRightRot.x, 1, 0xBB8, 0);
    Math_SmoothStepToS(&this->robeRightRot.y, this->targetRobeRightRot.y, 1, 0xBB8, 0);
    Math_SmoothStepToS(&this->robeRightRot.z, this->targetRobeRightRot.z, 1, 0xBB8, 0);
}

static void Garo_SetupCirclePlayer(EnGaro* this, PlayState* play) {
    Garo_ChangeAnim(this, GARO_ANIM_BOUNCE);
    this->circlingAngularVelocity = 0x258;
    this->action = GARO_ACTION_CIRCLE_PLAYER;
    this->circlingAngle = GET_PLAYER(play)->actor.shape.rot.y;
    this->actionFunc = Garo_CirclePlayer;
}

// Jumps back out of the ground at its spawn point whenever it ends up off Link's level.
static void Garo_Reappear(EnGaro* this, PlayState* play) {
    Math_ApproachF(&this->actor.shape.shadowScale, 16.0f, 0.4f, 4.0f);
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xBB8, 0x14);
    if ((play->gameplayFrames % 8) == 0) {
        Garo_SpawnDustRing(this, play);
    }
    if (!Garo_IsAnimDone(this)) {
        return;
    }
    this->actor.gravity = -3.0f;
    if (this->actor.colChkInfo.health > 0) {
        this->actor.flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;
        Garo_SetupCirclePlayer(this, play);
    } else {
        Garo_SetupFallDown(this, play);
    }
}

static void Garo_SetupReappear(EnGaro* this, PlayState* play) {
    Garo_ChangeAnim(this, GARO_ANIM_APPEAR);
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = 0.0f;
    Garo_StopAttacking(this);
    this->swordState = GARO_SWORDS_BOTH_DRAWN;
    this->actor.shape.rot.x = 0;
    this->actor.world.rot.x = 0;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.world.pos = this->actor.home.pos;
    this->actor.floorHeight = this->actor.home.pos.y;
    Garo_SpawnDustRing(this, play);
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_ENTRY, NA_SE_EN_STAL_REBORN);
    this->afterimageCount = 0;
    this->action = GARO_ACTION_REAPPEAR;
    this->actor.gravity = 0.0f;
    this->actionFunc = Garo_Reappear;
}

static void Garo_SpinBeforeAttack(EnGaro* this, PlayState* play) {
    this->actor.world.rot.x += 0x1770;
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xFA0, 0x14);
    if (this->actor.velocity.y > 0.0f || !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return;
    }
    this->actor.world.rot.x = 0;
    this->actor.velocity.y = 13.0f;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->action = GARO_ACTION_DASH_ATTACK;
    this->attackMovementTimer = 40;
    this->bodyCollider.base.colType = COLTYPE_HIT2;
    this->bodyCollider.base.acFlags &= ~AC_HARD;
    this->actor.speedXZ = 15.0f;
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_ENTRY, NA_SE_EN_STAL_REBORN);
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_DASH_2, NA_SE_EN_STALWALL_DASH);
}

static bool Garo_CanStartAttack(EnGaro* this) {
    return this->attackTimer == 0 && !sIsAnyGaroAttacking;
}

static void Garo_SetupSpinBeforeAttack(EnGaro* this) {
    sIsAnyGaroAttacking = true;
    this->isAttacking = true;
    this->action = GARO_ACTION_SPIN_BEFORE_ATTACK;
    Garo_ChangeAnim(this, GARO_ANIM_DASH_ATTACK);
    this->actor.world.rot.y = -this->actor.yawTowardsPlayer;
    this->actor.speedXZ = 10.0f;
    this->actor.velocity.y = 20.0f;
    this->actionFunc = Garo_SpinBeforeAttack;
}

// Bounces around Link on a 200-unit ring, sometimes turning back, until its attack timer runs out.
static void Garo_CirclePlayer(EnGaro* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_MOVING - SFX_FLAG, NA_SE_EN_STAL_WALK);
    if (fabsf(player->actor.world.pos.y - this->actor.world.pos.y) > 60.0f) {
        Garo_SetupReappear(this, play);
        return;
    }
    if (!Garo_IsAnimDone(this) || (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        SkelAnime_Update(&this->skelAnime);
    }
    if (Animation_OnFrame(&this->skelAnime, 6.0f)) {
        this->actor.velocity.y = 10.0f;
        if ((play->gameplayFrames % 2) == 0) {
            PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_CRYING, NA_SE_EN_GERUDOFT_BREATH);
        }
    }
    if (Animation_OnFrame(&this->skelAnime, 12.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_SKIP, NA_SE_EN_STAL_JUMP);
        this->actor.speedXZ = 0.0f;
        if (Rand_ZeroOne() < 0.5f) {
            this->circlingAngularVelocity = -this->circlingAngularVelocity;
        }
    }
    if (Garo_CanStartAttack(this)) {
        this->actor.speedXZ = 0.0f;
        Garo_SetupSpinBeforeAttack(this);
        return;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xFA0, 0x14);
    Math_ApproachF(&this->actor.speedXZ, 5.0f, 0.3f, 2.0f);
    this->circlingAngle += this->circlingAngularVelocity;
    Vec3f target = { Math_SinS(this->circlingAngle) * 200.0f + player->actor.world.pos.x, this->actor.world.pos.y,
                     Math_CosS(this->circlingAngle) * 200.0f + player->actor.world.pos.z };

    Math_SmoothStepToS(&this->actor.world.rot.y, Math_Vec3f_Yaw(&this->actor.world.pos, &target), 0xA, 0xFA0, 0x14);
}

static void Garo_Guard(EnGaro* this, PlayState* play) {
    if (Garo_IsAnimDone(this)) {
        Garo_StopAttacking(this);
        Garo_SetupCirclePlayer(this, play);
    }
}

static void Garo_SetupGuard(EnGaro* this) {
    this->actor.speedXZ = 0.0f;
    Garo_ChangeAnim(this, GARO_ANIM_GUARD);
    this->action = GARO_ACTION_GUARD;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_BOUND);
    this->bodyCollider.base.acFlags |= AC_HARD;
    this->actionFunc = Garo_Guard;
}

static void Garo_Cower(EnGaro* this, PlayState* play) {
    if (this->timer == 0) {
        Garo_SetupJumpBack(this);
    }
}

static void Garo_KnockedBack(EnGaro* this, PlayState* play) {
    if (!Garo_IsAnimDone(this)) {
        return;
    }
    Garo_ChangeAnim(this, GARO_ANIM_COWER);
    this->action = GARO_ACTION_COWER;
    this->timer = 30;
    this->actionFunc = Garo_Cower;
}

static bool Garo_WasBlocked(EnGaro* this) {
    return (this->rightSwordCollider.base.atFlags & AT_BOUNCED) || (this->leftSwordCollider.base.atFlags & AT_BOUNCED);
}

// A blade blocked by Link's shield knocks both swords from its hands.
static bool Garo_TryLoseSwords(EnGaro* this) {
    if (!Garo_WasBlocked(this)) {
        return false;
    }
    this->rightSwordCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    this->leftSwordCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    if (this->swordState != GARO_SWORDS_BOTH_DRAWN) {
        return false;
    }
    Garo_PushAwayFromPlayer(this, 10.0f);
    this->swordState = GARO_SWORDS_KNOCKED_OUT;
    this->attackMovementTimer = 0;
    this->disableBlure = true;
    Garo_ChangeAnim(this, GARO_ANIM_KNOCKED_BACK);
    this->bodyCollider.base.acFlags &= ~AC_HARD;
    this->timer = 30;
    this->action = GARO_ACTION_KNOCKED_BACK;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Garo_KnockedBack;
    return true;
}

static void Garo_WaitAfterSlash(EnGaro* this, PlayState* play) {
    if (this->timer != 0) {
        return;
    }
    this->attackTimer = Rand_S16Offset(30, 30);
    this->bodyCollider.base.colType = COLTYPE_NONE;
    this->bodyCollider.base.acFlags |= AC_HARD;
    this->slashHitSomething = false;
    Garo_StopAttacking(this);
    Garo_SetupCirclePlayer(this, play);
}

static void Garo_Slash(EnGaro* this, PlayState* play) {
    Math_ApproachZeroF(&this->actor.speedXZ, 0.5f, 5.0f);
    if ((play->gameplayFrames % 8) == 0) {
        Garo_SpawnDustRing(this, play);
    }
    if ((this->rightSwordCollider.base.atFlags & AT_HIT) || (this->leftSwordCollider.base.atFlags & AT_HIT)) {
        this->slashHitSomething = true;
        this->rightSwordCollider.base.atFlags &= ~AT_HIT;
        this->leftSwordCollider.base.atFlags &= ~AT_HIT;
    }
    if (Garo_TryLoseSwords(this) || !Garo_IsAnimDone(this)) {
        return;
    }
    this->actor.speedXZ = 0.0f;
    this->disableBlure = true;
    Garo_ChangeAnim(this, GARO_ANIM_SLASH_LOOP);
    this->timer = this->slashHitSomething ? 20 : 40;
    this->action = GARO_ACTION_WAIT_AFTER_SLASH;
    this->actionFunc = Garo_WaitAfterSlash;
}

static void Garo_SetupSlash(EnGaro* this, PlayState* play) {
    Garo_ChangeAnim(this, GARO_ANIM_SLASH_START);
    this->action = GARO_ACTION_SLASH;
    Garo_SpawnDustRing(this, play);
    Math_ApproachZeroF(&this->actor.speedXZ, 0.3f, 3.0f);
    this->slashHitSomething = false;
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_SWING_HARD);
    this->actionFunc = Garo_Slash;
}

// Dashes at Link and slashes once close, once it has run past him, or after 40 frames.
static void Garo_DashAttack(EnGaro* this, PlayState* play) {
    if (Garo_TryLoseSwords(this)) {
        return;
    }
    if (this->actor.velocity.y < 0.0f && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        this->actor.velocity.y = 13.0f;
    }
    if (!Garo_IsAnimDone(this)) {
        return;
    }
    s16 yawDiff = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;

    if (this->attackMovementTimer == 0 || this->actor.xzDistToPlayer < 100.0f || ABS(yawDiff) > 0x4300) {
        Garo_SetupSlash(this, play);
    }
}

static void Garo_JumpBack(EnGaro* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xBB8, 0x14);
    bool isGrounded = (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;

    if (isGrounded) {
        this->actor.speedXZ = 0.0f;
    }
    if (this->timer != 0 && !(Garo_IsAnimDone(this) && isGrounded)) {
        return;
    }
    this->actor.world.rot.x = 0;
    this->actor.velocity.y = 0.0f;
    this->actor.speedXZ = 0.0f;
    this->timer = 0;
    Garo_StopAttacking(this);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->attackTimer = Rand_S16Offset(10, 10);
    Garo_SetupCirclePlayer(this, play);
}

static void Garo_SetupJumpBack(EnGaro* this) {
    Garo_ChangeAnim(this, GARO_ANIM_JUMP_BACK);
    this->action = GARO_ACTION_JUMP_BACK;
    this->timer = 30;
    this->actor.speedXZ = 7.0f;
    this->actor.velocity.y = 20.0f;
    if (this->swordState != GARO_SWORDS_BOTH_DRAWN) {
        PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_SWORD, NA_SE_IT_SWORD_PICKOUT);
        this->swordState = GARO_SWORDS_BOTH_DRAWN;
    }
    this->actor.world.rot.y += 0x8000;
    this->actionFunc = Garo_JumpBack;
}

static void Garo_Stunned(EnGaro* this, PlayState* play) {
    if (this->timer != 0 || Garo_IsFrozen(this)) {
        return;
    }
    if (this->aura == GARO_AURA_FROZEN) {
        Garo_ShatterIce(this, play);
    }
    Garo_SetupJumpBack(this);
}

static void Garo_SetupStunned(EnGaro* this) {
    Garo_ChangeAnim(this, GARO_ANIM_DAMAGED);
    this->actor.speedXZ = 0.0f;
    this->timer = 40;
    Garo_PushAwayFromPlayer(this, 10.0f);
    this->action = GARO_ACTION_STUNNED;
    this->bodyCollider.base.acFlags &= ~AC_HARD;
    this->actionFunc = Garo_Stunned;
}

static void Garo_Damaged(EnGaro* this, PlayState* play) {
    bool hasLanded = this->actor.velocity.y <= 0.0f && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);

    if (hasLanded && this->actor.colorFilterTimer == 0) {
        Garo_SetupJumpBack(this);
    }
}

static void Garo_SetupDamaged(EnGaro* this, PlayState* play) {
    Garo_ChangeAnim(this, GARO_ANIM_DAMAGED);
    this->slashHitSomething = false;
    this->actor.velocity.y = 10.0f;
    this->actor.speedXZ = 0.0f;
    Garo_PushAwayFromPlayer(this, 20.0f);
    if (Garo_IsFrozen(this)) {
        Garo_ShatterIce(this, play);
    }
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_DAMAGE, NA_SE_EN_GERUDOFT_DAMAGE);
    this->action = GARO_ACTION_DAMAGED;
    this->actionFunc = Garo_Damaged;
}

// Bursts into nine green flames, MM's farewell once its hint is told.
static void Garo_BurstIntoFlames(EnGaro* this, PlayState* play) {
    static Vec3f sFireDrift[] = {
        { 1.0f, 0.0f, 0.5f },  { 1.0f, 0.0f, -0.5f }, { -1.0f, 0.0f, 0.5f },  { -1.0f, 0.0f, -0.5f }, { 0.5f, 0.0f, 1.0f },
        { -0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f, -1.0f }, { -0.5f, 0.0f, -1.0f }, { 0.0f, 0.0f, 0.0f },
    };

    if (!Garo_IsAnimDone(this) || this->timer != 0) {
        return;
    }
    for (s32 i = 0; i < ARRAY_COUNT(sFireDrift); i++) {
        Vec3f firePos = { this->actor.world.pos.x + Rand_CenteredFloat(30.0f), this->actor.floorHeight,
                          this->actor.world.pos.z + Rand_CenteredFloat(30.0f) };

        EffectSsDeadDb_Spawn(play, &firePos, &sFireDrift[i], &sFireDrift[i], Rand_ZeroOne() * 100.0f + 100.0f, 20,
                             255, 255, 255, 255, 0, 255, 0, 1, 9, false);
    }
    PlaySfx(&this->actor, MM_SE_EN_COMMON_EXTINCT_LEV, NA_SE_EN_EXTINCT);
    Garo_StopAttacking(this);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

static void Garo_SetupFallDown(EnGaro* this, PlayState* play) {
    Garo_ChangeAnim(this, GARO_ANIM_FALL_DOWN);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->timer = 20;
    this->action = GARO_ACTION_FALL_DOWN;
    this->actionFunc = Garo_BurstIntoFlames;
}

static void Garo_Dead(EnGaro* this, PlayState* play) {
    if (this->aura == GARO_AURA_FROZEN) {
        Garo_ShatterIce(this, play);
    }
    if (Garo_IsAnimDone(this) && this->timer == 0) {
        Garo_SetupFallDown(this, play);
    }
}

static void Garo_SetupDead(EnGaro* this, PlayState* play) {
    Garo_ChangeAnim(this, GARO_ANIM_DAMAGED);
    this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.speedXZ = 0.0f;
    this->disableBlure = true;
    this->timer = 30;
    Enemy_StartFinishingBlow(play, &this->actor);
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_DEAD, NA_SE_EN_GERUDOFT_DEAD);
    this->action = GARO_ACTION_DEAD;
    this->actionFunc = Garo_Dead;
}

static void Garo_Stun(EnGaro* this) {
    if (Garo_IsFrozen(this)) {
        return;
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 40);
    Garo_SetupStunned(this);
}

static void Garo_Freeze(EnGaro* this, PlayState* play) {
    if (!Garo_IsFrozen(this)) {
        Actor_ApplyDamage(&this->actor);
        this->auraTimer = 80;
        this->aura = GARO_AURA_FROZEN;
        Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 80);
    }
    if (this->actor.colChkInfo.health <= 0) {
        Garo_SetupDead(this, play);
    } else {
        Garo_SetupStunned(this);
    }
}

static bool Garo_IsVulnerable(EnGaro* this) {
    switch (this->action) {
        case GARO_ACTION_DASH_ATTACK:
        case GARO_ACTION_SLASH:
        case GARO_ACTION_WAIT_AFTER_SLASH:
        case GARO_ACTION_STUNNED:
        case GARO_ACTION_COWER:
            return true;
        default:
            return false;
    }
}

// Blocks blows while circling; only an attacking or reeling Garo can be hurt. A blow blocked once its attack timer
// has run out is answered with the dash, so steady pressure cannot pin it in its guard.
static void Garo_UpdateDamage(EnGaro* this, PlayState* play) {
    if (!(this->bodyCollider.base.acFlags & AC_HIT)) {
        return;
    }
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    u8 effect = this->actor.colChkInfo.damageEffect;

    if (effect == GARO_DMGEFF_STUN) {
        Garo_Stun(this);
        return;
    }
    if (this->action == GARO_ACTION_CIRCLE_PLAYER || this->action == GARO_ACTION_GUARD) {
        if (Garo_CanStartAttack(this)) {
            this->actor.speedXZ = 0.0f;
            Garo_SetupSpinBeforeAttack(this);
        } else {
            Garo_SetupGuard(this);
        }
        return;
    }
    if (!Garo_IsVulnerable(this)) {
        return;
    }
    switch (effect) {
        case GARO_DMGEFF_FIRE:
            this->auraTimer = 40;
            this->aura = GARO_AURA_FIRE;
            break;
        case GARO_DMGEFF_LIGHT:
            if (Garo_IsFrozen(this)) {
                return;
            }
            this->auraTimer = 20;
            this->aura = GARO_AURA_LIGHT;
            break;
        case GARO_DMGEFF_FREEZE:
            Garo_Freeze(this, play);
            return;
        case GARO_DMGEFF_NONE:
            break;
        default:
            return;
    }
    Actor_ApplyDamage(&this->actor);
    this->bodyCollider.base.colType = COLTYPE_NONE;
    this->bodyCollider.base.acFlags |= AC_HARD;
    if (this->actor.colChkInfo.health > 0) {
        Garo_SetupDamaged(this, play);
    } else {
        Garo_SetupDead(this, play);
    }
}

static void Garo_RecordAfterimage(EnGaro* this) {
    bool isDashing = this->action == GARO_ACTION_SPIN_BEFORE_ATTACK || this->action == GARO_ACTION_DASH_ATTACK;

    if (!isDashing) {
        this->afterimageCount = 0;
        return;
    }
    this->afterimageIndex = (this->afterimageIndex + 1) % GARO_AFTERIMAGE_COUNT;
    if (this->afterimageCount < GARO_AFTERIMAGE_COUNT - 1) {
        this->afterimageCount++;
    }
    this->afterimagePos[this->afterimageIndex] = this->actor.world.pos;
    this->afterimagePos[this->afterimageIndex].y += 40.0f;
    this->afterimageYaw[this->afterimageIndex] = this->actor.shape.rot.y;
    for (s32 i = 0; i < GARO_LIMB_MAX; i++) {
        this->afterimageJointTable[this->afterimageIndex][i] = this->jointTable[i];
    }
}

static bool Garo_IsTangible(EnGaro* this) {
    switch (this->action) {
        case GARO_ACTION_WAIT:
        case GARO_ACTION_INTRO:
        case GARO_ACTION_REAPPEAR:
        case GARO_ACTION_DAMAGED:
        case GARO_ACTION_SPIN_BEFORE_ATTACK:
        case GARO_ACTION_DEAD:
        case GARO_ACTION_FALL_DOWN:
            return false;
        default:
            return true;
    }
}

static bool Garo_IsShapeYawFree(EnGaro* this) {
    switch (this->action) {
        case GARO_ACTION_INTRO:
        case GARO_ACTION_CIRCLE_PLAYER:
        case GARO_ACTION_SPIN_BEFORE_ATTACK:
        case GARO_ACTION_DAMAGED:
        case GARO_ACTION_JUMP_BACK:
            return true;
        default:
            return false;
    }
}

static void Garo_SubmitColliders(EnGaro* this, PlayState* play) {
    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    if (!Garo_IsTangible(this)) {
        return;
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    bool isSlashing =
        this->action == GARO_ACTION_SLASH && !this->slashHitSomething && this->swordState == GARO_SWORDS_BOTH_DRAWN;

    if (isSlashing) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->rightSwordCollider.base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->leftSwordCollider.base);
    }
}

static void EnGaro_Init(Actor* thisx, PlayState* play) {
    EnGaro* this = (EnGaro*)thisx;
    EffectBlureInit1 blureInit = { 0 };

    this->actor.targetMode = 5;
    this->actor.colChkInfo.mass = 80;
    this->actor.colChkInfo.health = GARO_HEALTH;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 0.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sGaroSkel, (AnimationHeader*)sGaroAppearAnim,
                       this->jointTable, this->morphTable, GARO_LIMB_MAX);
    this->actor.focus.pos = this->actor.world.pos;
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyCylinderInit);
    Collider_InitQuad(play, &this->rightSwordCollider);
    Collider_SetQuad(play, &this->rightSwordCollider, &this->actor, &sSwordQuadInit);
    Collider_InitQuad(play, &this->leftSwordCollider);
    Collider_SetQuad(play, &this->leftSwordCollider, &this->actor, &sSwordQuadInit);
    for (s32 i = 0; i < 4; i++) {
        blureInit.p1StartColor[i] = 255;
        blureInit.p2StartColor[i] = 255;
        blureInit.p1EndColor[i] = 255;
        blureInit.p2EndColor[i] = 255;
    }
    blureInit.p2StartColor[3] = 64;
    blureInit.p1EndColor[3] = 0;
    blureInit.p2EndColor[3] = 0;
    blureInit.elemDuration = 8;
    blureInit.calcMode = 2;
    Effect_Add(play, &this->rightSwordBlureIndex, EFFECT_BLURE1, 0, 0, &blureInit);
    Effect_Add(play, &this->leftSwordBlureIndex, EFFECT_BLURE1, 0, 0, &blureInit);
    this->actor.gravity = -3.0f;
    this->scale = GARO_SCALE;
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->introType = this->actor.params & GARO_INTRO_SCALE_UP;
    this->swordState = GARO_SWORDS_NONE_DRAWN;
    this->action = GARO_ACTION_WAIT;
    this->actionFunc = Garo_Intro;
}

static void EnGaro_Destroy(Actor* thisx, PlayState* play) {
    EnGaro* this = (EnGaro*)thisx;

    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyQuad(play, &this->rightSwordCollider);
    Collider_DestroyQuad(play, &this->leftSwordCollider);
    Effect_Delete(play, this->rightSwordBlureIndex);
    Effect_Delete(play, this->leftSwordBlureIndex);
    Garo_StopAttacking(this);
}

static void EnGaro_Update(Actor* thisx, PlayState* play) {
    EnGaro* this = (EnGaro*)thisx;

    if (this->action == GARO_ACTION_WAIT) {
        Garo_WaitForPlayer(this, play);
        return;
    }
    if (this->action != GARO_ACTION_CIRCLE_PLAYER) {
        SkelAnime_Update(&this->skelAnime);
    }
    Garo_UpdateDamage(this, play);
    this->actionFunc(this, play);
    DECR(this->attackMovementTimer);
    DECR(this->attackTimer);
    DECR(this->timer);
    DECR(this->auraTimer);
    Actor_SetFocus(&this->actor, this->action == GARO_ACTION_FALL_DOWN ? 30.0f : 50.0f);
    Actor_SetScale(&this->actor, this->scale);
    Actor_MoveXZGravity(&this->actor);
    this->actor.world.pos.x += this->knockbackVelocity.x;
    this->actor.world.pos.z += this->knockbackVelocity.z;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Math_ApproachZeroF(&this->knockbackVelocity.x, 1.0f, 2.0f);
        Math_ApproachZeroF(&this->knockbackVelocity.z, 1.0f, 2.0f);
    }
    if (this->action != GARO_ACTION_REAPPEAR) {
        Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 40.0f, 40.0f, BGCHECK_UPDATE_WALL_CEILING_FLOOR_WATER);
    }
    Garo_RecordAfterimage(this);
    if (!Garo_IsShapeYawFree(this)) {
        this->actor.shape.rot.y = this->actor.world.rot.y;
    }
    this->actor.shape.rot.x = this->actor.world.rot.x;
    Garo_SubmitColliders(this, play);
}

static s32 Garo_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnGaro* this = (EnGaro*)thisx;

    if (limbIndex == GARO_LIMB_RIGHT_ARM) {
        rot->x += this->rightArmRot.x;
        rot->y += this->rightArmRot.y;
        rot->z += this->rightArmRot.z;
    }
    if (limbIndex == GARO_LIMB_ROBE_RIGHT) {
        rot->x += this->robeRightRot.x;
        rot->y += this->robeRightRot.y;
        rot->z += this->robeRightRot.z;
    }
    bool isHidden = (limbIndex == GARO_LIMB_LEFT_SWORD || limbIndex == GARO_LIMB_RIGHT_SWORD) &&
                    (this->swordState == GARO_SWORDS_NONE_DRAWN ||
                     (limbIndex == GARO_LIMB_LEFT_SWORD && this->swordState == GARO_SWORDS_RIGHT_DRAWN));

    if (isHidden) {
        *dList = NULL;
    }
    return false;
}

// The sword quad trails from last frame's edge to this frame's; copies first, the setter overwrites its sources.
static void Garo_UpdateSword(EnGaro* this, ColliderQuad* collider, s32 blureIndex) {
    Vec3f previousBase = collider->dim.quad[0];
    Vec3f previousTip = collider->dim.quad[1];
    Vec3f quadTip;
    Vec3f base;
    Vec3f tip;

    Matrix_MultVec3f(&sSwordTipQuadOffset, &quadTip);
    Matrix_MultVec3f(&sZeroVec, &base);
    Collider_SetQuadVertices(collider, &base, &quadTip, &previousBase, &previousTip);
    Matrix_MultVec3f(&sSwordTipOffset, &tip);
    bool isSwinging = this->action == GARO_ACTION_SLASH || this->action == GARO_ACTION_DASH_ATTACK;

    if (isSwinging && !this->disableBlure) {
        EffectBlure_AddVertex(Effect_GetByIndex(blureIndex), &tip, &base);
    } else if (this->disableBlure) {
        EffectBlure_AddSpace(Effect_GetByIndex(blureIndex));
    }
}

static bool IsGaroBodyPart(s32 limbIndex) {
    switch (limbIndex) {
        case GARO_LIMB_LEFT_SWORD:
        case GARO_LIMB_RIGHT_SWORD:
        case GARO_LIMB_ROBE_TOP:
        case GARO_LIMB_ROBE_BACK:
        case GARO_LIMB_ROBE_LEFT:
        case GARO_LIMB_ROBE_RIGHT:
        case GARO_LIMB_ROBE_FRONT:
        case GARO_LIMB_HEAD:
        case GARO_LIMB_RIGHT_THIGH:
        case GARO_LIMB_RIGHT_FOOT:
        case GARO_LIMB_LEFT_THIGH:
        case GARO_LIMB_LEFT_FOOT:
            return true;
        default:
            return false;
    }
}

static void Garo_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnGaro* this = (EnGaro*)thisx;

    if (limbIndex == GARO_LIMB_LEFT_SWORD) {
        Garo_UpdateSword(this, &this->leftSwordCollider, this->leftSwordBlureIndex);
    }
    if (limbIndex == GARO_LIMB_RIGHT_SWORD) {
        Garo_UpdateSword(this, &this->rightSwordCollider, this->rightSwordBlureIndex);
        this->disableBlure = false;
        if (this->swordState == GARO_SWORDS_KNOCKED_OUT) {
            this->swordState = GARO_SWORDS_NONE_DRAWN;
        }
    }
    if (IsGaroBodyPart(limbIndex)) {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[this->bodyPartIndex]);
        this->bodyPartIndex = (this->bodyPartIndex + 1) % GARO_BODYPART_MAX;
    }
}

static Gfx* Garo_MakeAfterimageRenderMode(GraphicsContext* gfxCtx) {
    Gfx* head = Graph_Alloc(gfxCtx, 2 * sizeof(Gfx));

    gDPSetRenderMode(&head[0],
                     AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                         G_RM_FOG_SHADE_A,
                     AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                         GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA));
    gSPEndDisplayList(&head[1]);
    return head;
}

static void Garo_DrawAfterimages(EnGaro* this, PlayState* play) {
    s32 index = this->afterimageIndex;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    for (s32 i = 0; i < this->afterimageCount; i++, index = (index + GARO_AFTERIMAGE_COUNT - 1) % GARO_AFTERIMAGE_COUNT) {
        if (sAfterimageAlpha[i] == 0) {
            continue;
        }
        Matrix_Translate(this->afterimagePos[index].x, this->afterimagePos[index].y, this->afterimagePos[index].z,
                         MTXMODE_NEW);
        Matrix_Scale(this->scale, this->scale, this->scale, MTXMODE_APPLY);
        Matrix_RotateY(BINANG_TO_RAD(this->afterimageYaw[index]), MTXMODE_APPLY);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, sAfterimageAlpha[i]);
        gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)Garo_MakeAfterimageRenderMode(play->state.gfxCtx));
        POLY_XLU_DISP = SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->afterimageJointTable[index],
                                           this->skelAnime.dListCount, NULL, NULL, this, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's damage-effect fire and light orbs become OoT's flame billboard tinted per element.
static void Garo_DrawAura(EnGaro* this, PlayState* play) {
    bool isLight = this->aura == GARO_AURA_LIGHT;
    u8 alpha = CLAMP_MAX(this->auraTimer * 0.05f, 1.0f) * 255.0f;
    MtxF billboard;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_RotateY(BINANG_TO_RAD(BINANG_ROT180(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)))), MTXMODE_NEW);
    Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);
    Matrix_Get(&billboard);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                           (play->gameplayFrames * -20) % 512, 0x20, 0x80));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, isLight ? 255 : 0, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, isLight ? 255 : 10, isLight ? 100 : 0, 255);
    for (s32 i = 0; i < GARO_BODYPART_MAX; i++) {
        billboard.xw = this->bodyPartsPos[i].x;
        billboard.yw = this->bodyPartsPos[i].y;
        billboard.zw = this->bodyPartsPos[i].z;
        Matrix_Put(&billboard);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's opaque Scene_SetRenderModeXlu table for segment 0x0C (object_jso calls entries 0 and 2).
static Gfx sRenderModeOpaDL[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

static void EnGaro_Draw(Actor* thisx, PlayState* play) {
    EnGaro* this = (EnGaro*)thisx;

    if (this->action == GARO_ACTION_WAIT) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
    CLOSE_DISPS(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          Garo_OverrideLimbDraw, Garo_PostLimbDraw, this);
    if (this->afterimageCount > 0) {
        Garo_DrawAfterimages(this, play);
    }
    if (this->auraTimer != 0 && this->aura != GARO_AURA_FROZEN && this->aura != GARO_AURA_NONE) {
        Garo_DrawAura(this, play);
    }
}

static bool CanBeGoron(void) {
    int32_t goronMask = sApi->GetRandoItem(GORON_MASK_KEY);

    return goronMask >= 0 && sApi->LogicHasItem(goronMask);
}

// MM's CanKillEnemy(ACTOR_EN_JSO) minus the Garo's Mask, which only summons it there.
static bool Garo_CanKillInLogic(void* userData) {
    return sApi->LogicCanUseItem(RG_FAIRY_BOW) || sApi->LogicHasCapability("CanUseSword") ||
           sApi->LogicCanUseItem(RG_MEGATON_HAMMER) || CanBeGoron();
}

// The Garo family soul, shared with mm_enemy_garo_master: both define it alike, so either can register it.
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
    static Vec3s sSoulJointTable[GARO_LIMB_MAX];
    static Vec3s sSoulMorphTable[GARO_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sGaroSkel, (AnimationHeader*)sGaroIdleAnim,
                           sSoulJointTable, sSoulMorphTable, GARO_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-sSoulJointTable[0].x, -sSoulJointTable[0].y, -sSoulJointTable[0].z, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0B, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
    SkelAnime_DrawFlexOpa(play, sSoulSkelAnime.skeleton, sSoulSkelAnime.jointTable, sSoulSkelAnime.dListCount, NULL,
                          NULL, NULL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterSoul(void) {
    static const SOHCustomItemRandomizer sSoulRandomizer = {
        .structSize = sizeof(SOHCustomItemRandomizer),
        .flags = SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT,
        .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
    };
    SOHCustomItemDefinition soul = Z64Items_Define(GARO_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Garos";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Garos%w!&Garos can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_RegisterShared(sApi, &soul)) {
        sApi->Log("Could not register " GARO_SOUL_ITEM_KEY);
    }
}

static void RegisterGaro(void) {
    SOHActorDefinition garo = { 0 };

    garo.structSize = sizeof(garo);
    garo.key = GARO_KEY;
    garo.description = "Garo";
    garo.category = ACTORCAT_ENEMY;
    garo.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    garo.objectId = OBJECT_GAMEPLAY_KEEP;
    garo.instanceSize = sizeof(EnGaro);
    garo.init = EnGaro_Init;
    garo.destroy = EnGaro_Destroy;
    garo.update = EnGaro_Update;
    garo.draw = EnGaro_Draw;
    garo.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    garo.enemyParams = GARO_INTRO_JUMP_OUT_FROM_GROUND;
    garo.naviHint = "Garo&%cIt parries while it circles you. Shield its dash to knock its blades away, then strike "
                    "back!%w";
    garo.soulKey = GARO_KEY;
    garo.logicKill = Garo_CanKillInLogic;
    if (sApi->RegisterActor(&garo) < 0) {
        sApi->Log("Could not register " GARO_KEY);
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
    RegisterGaro();
}
