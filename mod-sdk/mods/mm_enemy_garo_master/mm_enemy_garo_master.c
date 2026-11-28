// Majora's Mask's Garo Master (En_Jso2, the field type without cutscenes) as an Unbound custom enemy, drawn from mm.o2r.
// It springs up once Link comes near; En_Col_Man's held bomb becomes an OoT bomb pinned to its hand.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define GARO_MASTER_KEY "mm.garo_master"
#define GARO_FAMILY_KEY "mm.garo"
#define GARO_SOUL_ITEM_KEY "soul.mm.garo"
#define GORON_MASK_KEY "nei.goron_form_mask"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_JSO "__OTR__objects/object_jso/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_FLOOR_WATER 0x1D

#define MM_SE_EN_ANSATSUSYA_ENTRY 0x3812
#define MM_SE_EN_ANSATSUSYA_MOVING 0x39BD
#define MM_SE_EN_ANSATSUSYA_DASH_2 0x39BE
#define MM_SE_EN_ANSATSUSYA_CRYING 0x39C0
#define MM_SE_EN_ANSATSUSYA_SKIP 0x39C1
#define MM_SE_EN_ANSATSUSYA_JUMP 0x39C3
#define MM_SE_EN_ANSATSUSYA_FALL 0x39C4
#define MM_SE_EN_ANSATSUSYA_DAMAGE 0x39C5
#define MM_SE_EN_ANSATSUSYA_DEAD 0x39C6
#define MM_SE_EN_ANSATSUSYA_SWORD 0x39C7
#define MM_SE_IT_SHIELD_BOUND 0x1806
#define MM_SE_IT_SWORD_SWING_HARD 0x1818
#define MM_SE_EV_FLAME_IGNITION 0x2822
#define MM_SE_EV_ICE_BROKEN 0x28CB
#define MM_SE_EN_COMMON_FREEZE 0x389E

#define GARO_MASTER_HEALTH 14
#define GARO_MASTER_SCALE 0.042f
#define GARO_MASTER_WAKE_DISTANCE 400.0f
#define GARO_MASTER_AFTERIMAGE_COUNT 20
#define GARO_MASTER_BODYPART_MAX 12
#define GARO_MASTER_FLAME_COUNT 6
#define GARO_MASTER_BOMB_FUSE 200

typedef enum {
    GARO_MASTER_LIMB_NONE,
    GARO_MASTER_LIMB_ROOT,
    GARO_MASTER_LIMB_TORSO,
    GARO_MASTER_LIMB_LEFT_ARM,
    GARO_MASTER_LIMB_LEFT_SWORD,
    GARO_MASTER_LIMB_RIGHT_ARM,
    GARO_MASTER_LIMB_RIGHT_SWORD,
    GARO_MASTER_LIMB_ROBE_TOP,
    GARO_MASTER_LIMB_ROBE_BACK,
    GARO_MASTER_LIMB_ROBE_LEFT,
    GARO_MASTER_LIMB_ROBE_RIGHT,
    GARO_MASTER_LIMB_ROBE_FRONT,
    GARO_MASTER_LIMB_HEAD,
    GARO_MASTER_LIMB_LOWER_BODY_ROOT,
    GARO_MASTER_LIMB_RIGHT_THIGH,
    GARO_MASTER_LIMB_RIGHT_SHIN,
    GARO_MASTER_LIMB_RIGHT_FOOT,
    GARO_MASTER_LIMB_LEFT_THIGH,
    GARO_MASTER_LIMB_LEFT_SHIN,
    GARO_MASTER_LIMB_LEFT_FOOT,
    GARO_MASTER_LIMB_MAX
} GaroMasterLimb;

typedef enum {
    GARO_MASTER_ACTION_WAIT,
    GARO_MASTER_ACTION_APPEAR,
    GARO_MASTER_ACTION_CIRCLE_PLAYER = 3,
    GARO_MASTER_ACTION_GUARD,
    GARO_MASTER_ACTION_SPIN_BEFORE_ATTACK,
    GARO_MASTER_ACTION_DASH_ATTACK,
    GARO_MASTER_ACTION_SLASH,
    GARO_MASTER_ACTION_SPIN_ATTACK,
    GARO_MASTER_ACTION_WAIT_AFTER_SLASH,
    GARO_MASTER_ACTION_STUNNED,
    GARO_MASTER_ACTION_DAMAGED,
    GARO_MASTER_ACTION_JUMP_BACK,
    GARO_MASTER_ACTION_DEAD,
    GARO_MASTER_ACTION_BLOW_UP,
    GARO_MASTER_ACTION_TELEPORT,
    GARO_MASTER_ACTION_FALL_FROM_TELEPORT
} GaroMasterAction;

typedef enum {
    GARO_MASTER_ANIM_DASH_ATTACK,
    GARO_MASTER_ANIM_SLASH_START,
    GARO_MASTER_ANIM_SLASH_LOOP,
    GARO_MASTER_ANIM_JUMP_BACK,
    GARO_MASTER_ANIM_DAMAGED,
    GARO_MASTER_ANIM_GUARD,
    GARO_MASTER_ANIM_BOUNCE,
    GARO_MASTER_ANIM_LOOK_AROUND,
    GARO_MASTER_ANIM_APPEAR_AND_DRAW_SWORDS,
    GARO_MASTER_ANIM_SPIN_ATTACK,
    GARO_MASTER_ANIM_COLLAPSE,
    GARO_MASTER_ANIM_TREMBLE,
    GARO_MASTER_ANIM_TAKE_OUT_BOMB,
    GARO_MASTER_ANIM_MAX
} GaroMasterAnimation;

typedef enum {
    GARO_MASTER_DMGEFF_IMMUNE,
    GARO_MASTER_DMGEFF_STUN,
    GARO_MASTER_DMGEFF_FIRE,
    GARO_MASTER_DMGEFF_FREEZE,
    GARO_MASTER_DMGEFF_LIGHT,
    GARO_MASTER_DMGEFF_NONE = 0xF
} GaroMasterDamageEffect;

typedef enum {
    GARO_MASTER_AURA_NONE,
    GARO_MASTER_AURA_FIRE,
    GARO_MASTER_AURA_FROZEN,
    GARO_MASTER_AURA_LIGHT
} GaroMasterAura;

struct EnGaroMaster;

typedef void (*EnGaroMasterActionFunc)(struct EnGaroMaster*, PlayState*);

typedef struct EnGaroMaster {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[GARO_MASTER_LIMB_MAX];
    Vec3s morphTable[GARO_MASTER_LIMB_MAX];
    EnGaroMasterActionFunc actionFunc;
    s16 action;
    s16 animIndex;
    s16 circlingAngle;
    s16 circlingAngularVelocity;
    s16 attackMovementTimer;
    s16 attackTimer;
    s16 timer;
    s16 auraTimer;
    s16 aura;
    s16 alpha;
    s16 blowUpCounter;
    s16 bodyPartIndex;
    s16 afterimageIndex;
    s16 flameScroll;
    s32 afterimageCount;
    bool areSwordsDrawn;
    bool disableBlure;
    bool slashHitSomething;
    bool isFadingAway;
    bool isFightStarted;
    f32 animEndFrame;
    f32 scale;
    EnBom* bomb;
    Vec3f bombPos;
    Vec3f bodyPartsPos[GARO_MASTER_BODYPART_MAX];
    Vec3f knockbackVelocity;
    Vec3f flamePos[GARO_MASTER_FLAME_COUNT];
    f32 flameScale[GARO_MASTER_FLAME_COUNT];
    ColliderCylinder bodyCollider;
    ColliderQuad rightSwordCollider;
    ColliderQuad leftSwordCollider;
    s32 rightSwordBlureIndex;
    s32 leftSwordBlureIndex;
    Vec3f afterimagePos[GARO_MASTER_AFTERIMAGE_COUNT];
    Vec3s afterimageRot[GARO_MASTER_AFTERIMAGE_COUNT];
    Vec3s afterimageJointTable[GARO_MASTER_AFTERIMAGE_COUNT][GARO_MASTER_LIMB_MAX];
} EnGaroMaster;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sGaroMasterSkel[] = OBJ_JSO "gGaroMasterSkel";
static const ALIGN_ASSET(2) char sGaroMasterEyesDL[] = OBJ_JSO "gGaroMasterEyesDL";
static const ALIGN_ASSET(2) char sDashAttackAnim[] = OBJ_JSO "gGaroDashAttackAnim";
static const ALIGN_ASSET(2) char sSlashStartAnim[] = OBJ_JSO "gGaroSlashStartAnim";
static const ALIGN_ASSET(2) char sSlashLoopAnim[] = OBJ_JSO "gGaroSlashLoopAnim";
static const ALIGN_ASSET(2) char sJumpBackAnim[] = OBJ_JSO "gGaroJumpBackAnim";
static const ALIGN_ASSET(2) char sDamagedAnim[] = OBJ_JSO "gGaroDamagedAnim";
static const ALIGN_ASSET(2) char sGuardAnim[] = OBJ_JSO "gGaroGuardAnim";
static const ALIGN_ASSET(2) char sBounceAnim[] = OBJ_JSO "gGaroBounceAnim";
static const ALIGN_ASSET(2) char sLookAroundAnim[] = OBJ_JSO "gGaroLookAroundAnim";
static const ALIGN_ASSET(2) char sAppearAndDrawSwordsAnim[] = OBJ_JSO "gGaroAppearAndDrawSwordsAnim";
static const ALIGN_ASSET(2) char sSpinAttackAnim[] = OBJ_JSO "gGaroSpinAttackAnim";
static const ALIGN_ASSET(2) char sCollapseAnim[] = OBJ_JSO "gGaroCollapseAnim";
static const ALIGN_ASSET(2) char sTrembleAnim[] = OBJ_JSO "gGaroTrembleAnim";
static const ALIGN_ASSET(2) char sTakeOutBombAnim[] = OBJ_JSO "gGaroTakeOutBombAnim";

static const char* const sAnimations[GARO_MASTER_ANIM_MAX] = {
    sDashAttackAnim,  sSlashStartAnim,          sSlashLoopAnim,  sJumpBackAnim,  sDamagedAnim,
    sGuardAnim,       sBounceAnim,              sLookAroundAnim, sAppearAndDrawSwordsAnim,
    sSpinAttackAnim,  sCollapseAnim,            sTrembleAnim,    sTakeOutBombAnim,
};

static const u8 sAnimationModes[GARO_MASTER_ANIM_MAX] = {
    ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP,
    ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE,
};

static const s16 sAfterimageAlpha[GARO_MASTER_AFTERIMAGE_COUNT] = {
    128, 0, 0, 0, 0, 128, 0, 0, 0, 0, 128, 0, 0, 0, 0, 128, 0, 0, 0, 0,
};

static const f32 sTargetFlameScales[GARO_MASTER_FLAME_COUNT] = { 0.003f, 0.002f, 0.001f, 0.003f, 0.002f, 0.001f };

static Vec3f sFlameOffsets[GARO_MASTER_FLAME_COUNT] = {
    { 800.0f, -20.0f, -50.0f }, { 500.0f, -20.0f, -70.0f }, { 300.0f, -20.0f, -90.0f },
    { 800.0f, -20.0f, 50.0f },  { 500.0f, -20.0f, 70.0f },  { 300.0f, -20.0f, 90.0f },
};

static Vec3f sSlashFlameOffsets[GARO_MASTER_FLAME_COUNT] = {
    { 600.0f, -100.0f, 100.0f },  { 300.0f, -100.0f, 80.0f },  { 100.0f, -100.0f, 60.0f },
    { 600.0f, -100.0f, -100.0f }, { 300.0f, -100.0f, -80.0f }, { 100.0f, -100.0f, -60.0f },
};

static ColliderCylinderInit sBodyCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_HARD | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x04 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
    { 22, 70, 0, { 0, 0, 0 } },
};

static ColliderQuadInit sSwordQuadInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_QUAD },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_FIRE, 0x10 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
      BUMP_NONE,
      OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, GARO_MASTER_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, GARO_MASTER_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, GARO_MASTER_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, GARO_MASTER_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, GARO_MASTER_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, GARO_MASTER_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, GARO_MASTER_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, GARO_MASTER_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, GARO_MASTER_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, GARO_MASTER_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, GARO_MASTER_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(4, GARO_MASTER_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, GARO_MASTER_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(4, GARO_MASTER_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, GARO_MASTER_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, GARO_MASTER_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, GARO_MASTER_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(4, GARO_MASTER_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(4, GARO_MASTER_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(4, GARO_MASTER_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, GARO_MASTER_DMGEFF_IMMUNE),
    /* Mirror Ray    */ DMG_ENTRY(0, GARO_MASTER_DMGEFF_IMMUNE),
    /* Kokiri spin   */ DMG_ENTRY(1, GARO_MASTER_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(4, GARO_MASTER_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(2, GARO_MASTER_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, GARO_MASTER_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, GARO_MASTER_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, GARO_MASTER_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, GARO_MASTER_DMGEFF_IMMUNE),
    /* Unblockable   */ DMG_ENTRY(0, GARO_MASTER_DMGEFF_IMMUNE),
    /* Hammer jump   */ DMG_ENTRY(2, GARO_MASTER_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, GARO_MASTER_DMGEFF_IMMUNE),
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sSwordTipOffset = { 1600.0f, 0.0f, 0.0f };
static Vec3f sSwordTipQuadOffset = { 1700.0f, 0.0f, 0.0f };
static Vec3f sBombOffset = { 900.0f, 50.0f, -330.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

// Exported by the host but missing from OoT's functions.h.
void CollisionCheck_GreenBlood(PlayState* play, Collider* collider, Vec3f* v);

static void GaroMaster_CirclePlayer(EnGaroMaster* this, PlayState* play);
static void GaroMaster_DashAttack(EnGaroMaster* this, PlayState* play);
static void GaroMaster_SetupJumpBack(EnGaroMaster* this);
static void GaroMaster_SetupSlash(EnGaroMaster* this, PlayState* play);

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

static void GaroMaster_ChangeAnim(EnGaroMaster* this, s32 animIndex) {
    AnimationHeader* animation = (AnimationHeader*)sAnimations[animIndex];

    this->animEndFrame = Animation_GetLastFrame(animation);
    this->animIndex = animIndex;
    Animation_Change(&this->skelAnime, animation, animIndex == GARO_MASTER_ANIM_BOUNCE ? 2.0f : 1.0f, 0.0f,
                     this->animEndFrame, sAnimationModes[animIndex], -2.0f);
}

static bool GaroMaster_IsAnimDone(EnGaroMaster* this) {
    return this->skelAnime.curFrame >= this->animEndFrame;
}

static bool GaroMaster_IsGrounded(EnGaroMaster* this) {
    return (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static void GaroMaster_SpawnDustRing(EnGaroMaster* this, PlayState* play) {
    Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, this->actor.shape.shadowScale, 1, 8.0f, 500,
                             10, true);
}

static void GaroMaster_PushAwayFromPlayer(EnGaroMaster* this, f32 speed) {
    this->knockbackVelocity.x = Math_SinS(this->actor.yawTowardsPlayer) * -speed;
    this->knockbackVelocity.y = 0.0f;
    this->knockbackVelocity.z = Math_CosS(this->actor.yawTowardsPlayer) * -speed;
}

static bool GaroMaster_IsFrozen(EnGaroMaster* this) {
    return this->aura == GARO_MASTER_AURA_FROZEN && this->auraTimer != 0;
}

static void GaroMaster_ShatterIce(EnGaroMaster* this, PlayState* play) {
    PlaySfx(&this->actor, MM_SE_EV_ICE_BROKEN, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < GARO_MASTER_BODYPART_MAX; i++) {
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &this->bodyPartsPos[i], 150, 150, 150, 250, 235, 245, 255,
                                       0.7f);
    }
    this->auraTimer = 0;
    this->aura = GARO_MASTER_AURA_NONE;
}

static void GaroMaster_ClearSwordFlags(EnGaroMaster* this) {
    this->rightSwordCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    this->leftSwordCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
}

static bool GaroMaster_WasBlocked(EnGaroMaster* this) {
    return (this->rightSwordCollider.base.atFlags & AT_BOUNCED) || (this->leftSwordCollider.base.atFlags & AT_BOUNCED);
}

static bool GaroMaster_HasHit(EnGaroMaster* this) {
    return (this->rightSwordCollider.base.atFlags & AT_HIT) || (this->leftSwordCollider.base.atFlags & AT_HIT);
}

static void GaroMaster_StartMinibossMusic(EnGaroMaster* this) {
    if (this->isFightStarted) {
        return;
    }
    this->isFightStarted = true;
    func_800F5ACC(NA_BGM_MINI_BOSS);
}

static void GaroMaster_StopMinibossMusic(EnGaroMaster* this) {
    if (!this->isFightStarted) {
        return;
    }
    this->isFightStarted = false;
    func_800F5B58();
}

static void GaroMaster_SetupCirclePlayer(EnGaroMaster* this, PlayState* play) {
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_BOUNCE);
    this->circlingAngle = GET_PLAYER(play)->actor.shape.rot.y;
    this->circlingAngularVelocity = 0x258;
    this->action = GARO_MASTER_ACTION_CIRCLE_PLAYER;
    this->bodyCollider.base.acFlags |= AC_HARD;
    this->actionFunc = GaroMaster_CirclePlayer;
}

static void GaroMaster_GrowFlames(EnGaroMaster* this) {
    for (s32 i = 0; i < GARO_MASTER_FLAME_COUNT; i++) {
        Math_ApproachF(&this->flameScale[i], sTargetFlameScales[i], 0.3f, 0.0005f);
    }
}

// Jumps out of the ground, draws both blades at frame 45 and lights them.
static void GaroMaster_Appear(EnGaroMaster* this, PlayState* play) {
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    if (Animation_OnFrame(&this->skelAnime, 18.0f)) {
        GaroMaster_SpawnDustRing(this, play);
    }
    Math_ApproachF(&this->actor.shape.shadowScale, 17.0f, 0.4f, 4.0f);
    if (Animation_OnFrame(&this->skelAnime, 45.0f)) {
        GaroMaster_StartMinibossMusic(this);
        PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_SWORD, NA_SE_IT_SWORD_PICKOUT);
        PlaySfx(&this->actor, MM_SE_EV_FLAME_IGNITION, NA_SE_EV_FLAME_IGNITION);
        this->areSwordsDrawn = true;
    }
    if (this->areSwordsDrawn) {
        GaroMaster_GrowFlames(this);
    }
    if (GaroMaster_IsAnimDone(this)) {
        this->actor.flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        GaroMaster_SetupCirclePlayer(this, play);
    }
}

static void GaroMaster_WaitForPlayer(EnGaroMaster* this, PlayState* play) {
    if (this->actor.xzDistToPlayer > GARO_MASTER_WAKE_DISTANCE) {
        return;
    }
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_APPEAR_AND_DRAW_SWORDS);
    GaroMaster_SpawnDustRing(this, play);
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_ENTRY, NA_SE_EN_STAL_REBORN);
    this->action = GARO_MASTER_ACTION_APPEAR;
    this->actionFunc = GaroMaster_Appear;
}

static void GaroMaster_SpinBeforeAttack(EnGaroMaster* this, PlayState* play) {
    this->actor.world.rot.x += 0x1770;
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xFA0, 0x14);
    if (this->actor.velocity.y > 0.0f || !GaroMaster_IsGrounded(this)) {
        return;
    }
    this->actor.world.rot.x = 0;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->attackMovementTimer = 40;
    this->bodyCollider.base.acFlags |= AC_HARD;
    this->actor.speedXZ = 15.0f;
    this->actor.velocity.y = 13.0f;
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_ENTRY, NA_SE_EN_STAL_REBORN);
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_DASH_2, NA_SE_EN_STALWALL_DASH);
    this->action = GARO_MASTER_ACTION_DASH_ATTACK;
    this->actionFunc = GaroMaster_DashAttack;
}

static void GaroMaster_SetupSpinBeforeAttack(EnGaroMaster* this) {
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_DASH_ATTACK);
    this->actor.world.rot.y = -this->actor.yawTowardsPlayer;
    this->bodyCollider.base.acFlags |= AC_HARD;
    this->action = GARO_MASTER_ACTION_SPIN_BEFORE_ATTACK;
    this->actor.speedXZ = 10.0f;
    this->actor.velocity.y = 20.0f;
    this->actionFunc = GaroMaster_SpinBeforeAttack;
}

static void GaroMaster_CirclePlayer(EnGaroMaster* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_MOVING - SFX_FLAG, NA_SE_EN_STAL_WALK);
    if (!GaroMaster_IsAnimDone(this) || GaroMaster_IsGrounded(this)) {
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
    if (this->attackTimer == 0) {
        this->actor.speedXZ = 0.0f;
        GaroMaster_SetupSpinBeforeAttack(this);
        return;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xFA0, 0x14);
    Math_ApproachF(&this->actor.speedXZ, 5.0f, 0.3f, 2.0f);
    this->circlingAngle += this->circlingAngularVelocity;
    Vec3f target = { Math_SinS(this->circlingAngle) * 200.0f + player->actor.world.pos.x, this->actor.world.pos.y,
                     Math_CosS(this->circlingAngle) * 200.0f + player->actor.world.pos.z };

    Math_SmoothStepToS(&this->actor.world.rot.y, Math_Vec3f_Yaw(&this->actor.world.pos, &target), 0xA, 0xFA0, 0x14);
}

static void GaroMaster_Guard(EnGaroMaster* this, PlayState* play) {
    if (GaroMaster_IsAnimDone(this)) {
        GaroMaster_SetupCirclePlayer(this, play);
    }
}

static void GaroMaster_SetupGuard(EnGaroMaster* this) {
    this->actor.speedXZ = 0.0f;
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_GUARD);
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    PlaySfx(&this->actor, MM_SE_IT_SHIELD_BOUND, NA_SE_IT_SHIELD_BOUND);
    this->bodyCollider.base.acFlags |= AC_HARD;
    this->action = GARO_MASTER_ACTION_GUARD;
    this->actionFunc = GaroMaster_Guard;
}

// Lands where Link stood when it vanished, blades first.
static void GaroMaster_FallFromTeleport(EnGaroMaster* this, PlayState* play) {
    Math_ApproachF(&this->scale, GARO_MASTER_SCALE, 0.3f, 0.03f);
    Math_ApproachF(&this->actor.shape.shadowScale, 17.0f, 0.4f, 4.0f);
    if (!GaroMaster_IsGrounded(this)) {
        return;
    }
    this->scale = GARO_MASTER_SCALE;
    this->actor.shape.shadowScale = 17.0f;
    this->actor.flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;
    GaroMaster_SetupSlash(this, play);
}

static void GaroMaster_Teleport(EnGaroMaster* this, PlayState* play) {
    this->actor.shape.rot.y -= 0x1D4C;
    Math_ApproachZeroF(&this->scale, 0.3f, 0.01f);
    Math_ApproachZeroF(&this->actor.shape.shadowScale, 0.3f, 3.0f);
    if (this->attackMovementTimer != 0) {
        return;
    }
    this->actor.world.pos = GET_PLAYER(play)->actor.world.pos;
    this->actor.world.pos.y += 300.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -3.0f;
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_FALL, NA_SE_EN_STAL_JUMP);
    this->action = GARO_MASTER_ACTION_FALL_FROM_TELEPORT;
    this->actionFunc = GaroMaster_FallFromTeleport;
}

static void GaroMaster_SetupTeleport(EnGaroMaster* this) {
    this->disableBlure = true;
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_DASH_ATTACK);
    this->attackMovementTimer = 20;
    this->actor.speedXZ = 0.0f;
    this->actor.gravity = 0.0f;
    this->actor.velocity.y = 10.0f;
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_JUMP, NA_SE_EN_STAL_JUMP);
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->action = GARO_MASTER_ACTION_TELEPORT;
    this->actionFunc = GaroMaster_Teleport;
}

// A shielded dash bounces it back; each bounce on the floor has a 30% chance to vanish and strike from above.
static void GaroMaster_DashAttack(EnGaroMaster* this, PlayState* play) {
    if (GaroMaster_WasBlocked(this)) {
        GaroMaster_ClearSwordFlags(this);
        GaroMaster_PushAwayFromPlayer(this, 10.0f);
        this->disableBlure = true;
        this->attackMovementTimer = 0;
        GaroMaster_SetupJumpBack(this);
        return;
    }
    if (this->actor.velocity.y < 0.0f && GaroMaster_IsGrounded(this)) {
        if (Rand_ZeroOne() >= 0.7f) {
            GaroMaster_SetupTeleport(this);
            return;
        }
        this->actor.velocity.y = 13.0f;
    }
    if (!GaroMaster_IsAnimDone(this)) {
        return;
    }
    s16 yawDiff = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;

    if (this->attackMovementTimer == 0 || this->actor.xzDistToPlayer < 120.0f || ABS(yawDiff) > 0x4300) {
        Math_ApproachZeroF(&this->actor.speedXZ, 0.3f, 3.0f);
        GaroMaster_SetupSlash(this, play);
    }
}

static void GaroMaster_WaitAfterSlash(EnGaroMaster* this, PlayState* play) {
    if (this->timer != 0) {
        return;
    }
    this->attackTimer = Rand_S16Offset(30, 30);
    this->slashHitSomething = false;
    this->bodyCollider.base.acFlags |= AC_HARD;
    GaroMaster_SetupCirclePlayer(this, play);
}

static void GaroMaster_Slash(EnGaroMaster* this, PlayState* play) {
    Math_ApproachZeroF(&this->actor.speedXZ, 0.5f, 5.0f);
    if ((play->gameplayFrames % 8) == 0) {
        GaroMaster_SpawnDustRing(this, play);
    }
    if (GaroMaster_HasHit(this)) {
        this->slashHitSomething = true;
        this->rightSwordCollider.base.atFlags &= ~AT_HIT;
        this->leftSwordCollider.base.atFlags &= ~AT_HIT;
    }
    if (GaroMaster_WasBlocked(this)) {
        GaroMaster_ClearSwordFlags(this);
        GaroMaster_PushAwayFromPlayer(this, 10.0f);
        this->disableBlure = true;
        GaroMaster_SetupJumpBack(this);
        return;
    }
    if (!GaroMaster_IsAnimDone(this)) {
        return;
    }
    this->actor.speedXZ = 0.0f;
    this->disableBlure = true;
    GaroMaster_ChangeAnim(this, this->slashHitSomething ? GARO_MASTER_ANIM_SLASH_LOOP : GARO_MASTER_ANIM_LOOK_AROUND);
    this->timer = this->slashHitSomething ? 20 : 40;
    this->bodyCollider.base.acFlags &= ~AC_HARD;
    this->action = GARO_MASTER_ACTION_WAIT_AFTER_SLASH;
    this->actionFunc = GaroMaster_WaitAfterSlash;
}

static void GaroMaster_SetupSlash(EnGaroMaster* this, PlayState* play) {
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_SLASH_START);
    GaroMaster_SpawnDustRing(this, play);
    Math_ApproachZeroF(&this->actor.speedXZ, 0.3f, 3.0f);
    this->slashHitSomething = false;
    PlaySfx(&this->actor, MM_SE_IT_SWORD_SWING_HARD, NA_SE_IT_SWORD_SWING_HARD);
    this->action = GARO_MASTER_ACTION_SLASH;
    this->bodyCollider.base.acFlags &= ~AC_HARD;
    this->actionFunc = GaroMaster_Slash;
}

// Counter after being hurt: a 15-frame whirl that ends on any contact.
static void GaroMaster_SpinAttack(EnGaroMaster* this, PlayState* play) {
    this->actor.shape.rot.y -= 0x1770;
    Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, this->actor.shape.shadowScale, 1, 4.0f, 300,
                             5, true);
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA0, 0x14);
    if (this->attackTimer == 0 || GaroMaster_HasHit(this) || GaroMaster_WasBlocked(this)) {
        GaroMaster_ClearSwordFlags(this);
        GaroMaster_SetupJumpBack(this);
    }
}

static void GaroMaster_Damaged(EnGaroMaster* this, PlayState* play) {
    bool hasLanded = this->actor.velocity.y <= 0.0f && GaroMaster_IsGrounded(this);

    if (!hasLanded || this->actor.colorFilterTimer != 0) {
        return;
    }
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_SPIN_ATTACK);
    this->slashHitSomething = false;
    PlaySfx(&this->actor, MM_SE_IT_SWORD_SWING_HARD, NA_SE_IT_SWORD_SWING_HARD);
    this->disableBlure = false;
    this->attackTimer = 15;
    this->action = GARO_MASTER_ACTION_SPIN_ATTACK;
    this->actor.speedXZ = 12.0f;
    this->actionFunc = GaroMaster_SpinAttack;
}

static void GaroMaster_SetupDamaged(EnGaroMaster* this, PlayState* play) {
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_DAMAGED);
    this->slashHitSomething = false;
    this->actor.velocity.y = 10.0f;
    this->actor.speedXZ = 0.0f;
    GaroMaster_PushAwayFromPlayer(this, 20.0f);
    if (GaroMaster_IsFrozen(this)) {
        GaroMaster_ShatterIce(this, play);
    }
    for (s32 i = 0; i < 3; i++) {
        CollisionCheck_GreenBlood(play, NULL, &this->actor.focus.pos);
    }
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_DAMAGE, NA_SE_EN_GERUDOFT_DAMAGE);
    this->action = GARO_MASTER_ACTION_DAMAGED;
    this->actionFunc = GaroMaster_Damaged;
}

static void GaroMaster_JumpBack(EnGaroMaster* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xBB8, 0x14);
    if (!GaroMaster_IsGrounded(this)) {
        return;
    }
    this->actor.speedXZ = 0.0f;
    if (!GaroMaster_IsAnimDone(this)) {
        return;
    }
    this->actor.world.rot.x = 0;
    this->actor.velocity.y = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->attackTimer = Rand_S16Offset(10, 10);
    GaroMaster_SetupCirclePlayer(this, play);
}

static void GaroMaster_SetupJumpBack(EnGaroMaster* this) {
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_JUMP_BACK);
    this->actor.world.rot.y = -this->actor.world.rot.y;
    this->action = GARO_MASTER_ACTION_JUMP_BACK;
    this->actor.speedXZ = 7.0f;
    this->actor.velocity.y = 20.0f;
    this->actionFunc = GaroMaster_JumpBack;
}

static void GaroMaster_Stunned(EnGaroMaster* this, PlayState* play) {
    if (this->timer != 0 || GaroMaster_IsFrozen(this)) {
        return;
    }
    if (this->aura == GARO_MASTER_AURA_FROZEN) {
        GaroMaster_ShatterIce(this, play);
    }
    GaroMaster_SetupJumpBack(this);
}

static void GaroMaster_SetupStunned(EnGaroMaster* this) {
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_DAMAGED);
    this->timer = GaroMaster_IsFrozen(this) ? 30 : 40;
    this->bodyCollider.base.acFlags &= ~AC_HARD;
    this->actor.speedXZ = 0.0f;
    GaroMaster_PushAwayFromPlayer(this, 10.0f);
    this->action = GARO_MASTER_ACTION_STUNNED;
    this->actionFunc = GaroMaster_Stunned;
}

static void GaroMaster_FadeAway(EnGaroMaster* this, PlayState* play) {
    Math_SmoothStepToS(&this->alpha, 0, 1, 15, 50);
    Math_ApproachZeroF(&this->actor.shape.shadowScale, 0.3f, 3.0f);
    if (this->alpha < 2) {
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
        Actor_Kill(&this->actor);
    }
}

// The bomb can blow up early on contact with an enemy or when its fuse runs out, and may be freed while this actor is
// frozen, so the pointer is trusted only while the bomb is still listed and alive.
static bool GaroMaster_IsBombAlive(EnGaroMaster* this, PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_EXPLOSIVE].head; actor != NULL; actor = actor->next) {
        if (actor == &this->bomb->actor) {
            return actor->id == ACTOR_EN_BOM && actor->update != NULL;
        }
    }
    return false;
}

static void GaroMaster_HoldBomb(EnGaroMaster* this) {
    if (this->bomb == NULL) {
        return;
    }
    this->bomb->actor.world.pos = this->bombPos;
    this->bomb->actor.velocity = sZeroVec;
    this->bomb->actor.gravity = 0.0f;
}

// Collapses, trembles twice, takes out a bomb and sets it off in its own hand; the blast reaches Link too.
static void GaroMaster_BlowUp(EnGaroMaster* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xFA0, 0x14);
    bool isAnimDone = GaroMaster_IsAnimDone(this);

    if (this->bomb != NULL && !GaroMaster_IsBombAlive(this, play)) {
        this->bomb = NULL;
    }
    if (this->animIndex == GARO_MASTER_ANIM_COLLAPSE && isAnimDone) {
        this->blowUpCounter = 0;
        GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_TREMBLE);
    } else if (this->animIndex == GARO_MASTER_ANIM_TREMBLE && isAnimDone) {
        this->blowUpCounter++;
        if (this->blowUpCounter >= 2) {
            this->blowUpCounter = 0;
            GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_TAKE_OUT_BOMB);
        }
    } else if (this->animIndex == GARO_MASTER_ANIM_TAKE_OUT_BOMB && isAnimDone) {
        if (this->bomb == NULL) {
            this->bomb = (EnBom*)Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_BOM, this->bombPos.x,
                                                     this->bombPos.y, this->bombPos.z, 0, 0, 0, BOMB_BODY);
            if (this->bomb != NULL) {
                this->bomb->timer = GARO_MASTER_BOMB_FUSE;
            }
        } else if (this->blowUpCounter >= 10) {
            this->bomb->timer = 0;
            this->bomb->actor.parent = NULL;
            this->bomb = NULL;
            GaroMaster_StopMinibossMusic(this);
            this->isFadingAway = true;
            this->actionFunc = GaroMaster_FadeAway;
            return;
        } else {
            this->blowUpCounter++;
        }
    }
    GaroMaster_HoldBomb(this);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
}

static void GaroMaster_Dead(EnGaroMaster* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xFA0, 0x14);
    if (this->aura == GARO_MASTER_AURA_FROZEN) {
        GaroMaster_ShatterIce(this, play);
    }
    if (this->timer != 0) {
        return;
    }
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_COLLAPSE);
    this->action = GARO_MASTER_ACTION_BLOW_UP;
    this->actionFunc = GaroMaster_BlowUp;
}

static void GaroMaster_SetupDead(EnGaroMaster* this, PlayState* play) {
    GaroMaster_ChangeAnim(this, GARO_MASTER_ANIM_DAMAGED);
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    this->actor.speedXZ = 0.0f;
    this->disableBlure = true;
    this->timer = 30;
    this->areSwordsDrawn = false;
    this->actor.world.rot.x = 0;
    this->actor.shape.rot.x = 0;
    this->actor.world.rot.z = 0;
    this->actor.shape.rot.z = 0;
    Enemy_StartFinishingBlow(play, &this->actor);
    PlaySfx(&this->actor, MM_SE_EN_ANSATSUSYA_DEAD, NA_SE_EN_GERUDOFT_DEAD);
    this->knockbackVelocity = sZeroVec;
    this->action = GARO_MASTER_ACTION_DEAD;
    this->actionFunc = GaroMaster_Dead;
}

static void GaroMaster_Stun(EnGaroMaster* this) {
    this->actor.world.rot.x = 0;
    this->actor.shape.rot.x = 0;
    if (GaroMaster_IsFrozen(this)) {
        return;
    }
    PlaySfx(&this->actor, MM_SE_EN_COMMON_FREEZE, NA_SE_EN_GOMA_JR_FREEZE);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 40);
    GaroMaster_SetupStunned(this);
}

static void GaroMaster_Freeze(EnGaroMaster* this, PlayState* play) {
    if (!GaroMaster_IsFrozen(this)) {
        Actor_ApplyDamage(&this->actor);
        this->auraTimer = 80;
        this->aura = GARO_MASTER_AURA_FROZEN;
        Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 80);
    }
    if (this->actor.colChkInfo.health <= 0) {
        GaroMaster_SetupDead(this, play);
    } else {
        GaroMaster_SetupStunned(this);
    }
}

static bool GaroMaster_IsVulnerable(EnGaroMaster* this) {
    return this->action == GARO_MASTER_ACTION_SLASH || this->action == GARO_MASTER_ACTION_WAIT_AFTER_SLASH ||
           this->action == GARO_MASTER_ACTION_STUNNED;
}

static bool GaroMaster_IsIgnoringHits(EnGaroMaster* this) {
    return this->action == GARO_MASTER_ACTION_DAMAGED || this->action == GARO_MASTER_ACTION_JUMP_BACK ||
           this->action == GARO_MASTER_ACTION_DEAD || this->action == GARO_MASTER_ACTION_BLOW_UP;
}

// Parries while circling; only a slashing, recovering or stunned Garo Master can be hurt. A blow parried once its
// attack timer has run out is answered with the attack, so steady pressure cannot pin it in its guard.
static void GaroMaster_UpdateDamage(EnGaroMaster* this, PlayState* play) {
    if (GaroMaster_IsIgnoringHits(this) || !(this->bodyCollider.base.acFlags & AC_HIT)) {
        return;
    }
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    u8 effect = this->actor.colChkInfo.damageEffect;

    if (effect == GARO_MASTER_DMGEFF_STUN) {
        GaroMaster_Stun(this);
        return;
    }
    if (this->action == GARO_MASTER_ACTION_CIRCLE_PLAYER || this->action == GARO_MASTER_ACTION_GUARD) {
        if (this->attackTimer == 0) {
            this->actor.speedXZ = 0.0f;
            GaroMaster_SetupSpinBeforeAttack(this);
        } else {
            GaroMaster_SetupGuard(this);
        }
        return;
    }
    if (!GaroMaster_IsVulnerable(this)) {
        return;
    }
    switch (effect) {
        case GARO_MASTER_DMGEFF_FIRE:
            this->auraTimer = 40;
            this->aura = GARO_MASTER_AURA_FIRE;
            break;
        case GARO_MASTER_DMGEFF_LIGHT:
            if (GaroMaster_IsFrozen(this)) {
                return;
            }
            this->auraTimer = 20;
            this->aura = GARO_MASTER_AURA_LIGHT;
            break;
        case GARO_MASTER_DMGEFF_FREEZE:
            GaroMaster_Freeze(this, play);
            return;
        case GARO_MASTER_DMGEFF_NONE:
            break;
        default:
            return;
    }
    Actor_ApplyDamage(&this->actor);
    if (this->actor.colChkInfo.health > 0) {
        GaroMaster_SetupDamaged(this, play);
    } else {
        GaroMaster_SetupDead(this, play);
    }
}

static bool GaroMaster_IsLeavingAfterimages(EnGaroMaster* this) {
    switch (this->action) {
        case GARO_MASTER_ACTION_SPIN_BEFORE_ATTACK:
        case GARO_MASTER_ACTION_DASH_ATTACK:
        case GARO_MASTER_ACTION_TELEPORT:
        case GARO_MASTER_ACTION_FALL_FROM_TELEPORT:
            return true;
        default:
            return false;
    }
}

static void GaroMaster_RecordAfterimage(EnGaroMaster* this) {
    if (!GaroMaster_IsLeavingAfterimages(this)) {
        this->afterimageCount = 0;
        return;
    }
    this->afterimageIndex = (this->afterimageIndex + 1) % GARO_MASTER_AFTERIMAGE_COUNT;
    if (this->afterimageCount < GARO_MASTER_AFTERIMAGE_COUNT - 1) {
        this->afterimageCount++;
    }
    this->afterimagePos[this->afterimageIndex] = this->actor.world.pos;
    this->afterimagePos[this->afterimageIndex].y += 40.0f;
    this->afterimageRot[this->afterimageIndex] = this->actor.world.rot;
    for (s32 i = 0; i < GARO_MASTER_LIMB_MAX; i++) {
        this->afterimageJointTable[this->afterimageIndex][i] = this->jointTable[i];
    }
}

static bool GaroMaster_IsShapeYawFree(EnGaroMaster* this) {
    switch (this->action) {
        case GARO_MASTER_ACTION_CIRCLE_PLAYER:
        case GARO_MASTER_ACTION_SPIN_BEFORE_ATTACK:
        case GARO_MASTER_ACTION_DAMAGED:
        case GARO_MASTER_ACTION_SPIN_ATTACK:
        case GARO_MASTER_ACTION_TELEPORT:
        case GARO_MASTER_ACTION_JUMP_BACK:
            return true;
        default:
            return false;
    }
}

static bool GaroMaster_AreBladesLive(EnGaroMaster* this) {
    bool isStriking = this->action == GARO_MASTER_ACTION_SLASH ||
                      this->action == GARO_MASTER_ACTION_FALL_FROM_TELEPORT ||
                      this->action == GARO_MASTER_ACTION_DASH_ATTACK || this->action == GARO_MASTER_ACTION_SPIN_ATTACK;

    return isStriking && !this->slashHitSomething && this->areSwordsDrawn;
}

static void GaroMaster_SubmitColliders(EnGaroMaster* this, PlayState* play) {
    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    bool isIntangible = this->action == GARO_MASTER_ACTION_APPEAR ||
                        this->action == GARO_MASTER_ACTION_SPIN_ATTACK || this->action == GARO_MASTER_ACTION_TELEPORT;

    if (!isIntangible) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    if (GaroMaster_AreBladesLive(this)) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->rightSwordCollider.base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->leftSwordCollider.base);
    }
}

static void EnGaroMaster_Init(Actor* thisx, PlayState* play) {
    EnGaroMaster* this = (EnGaroMaster*)thisx;
    EffectBlureInit1 blureInit = { 0 };

    this->actor.targetMode = 5;
    this->actor.colChkInfo.mass = 80;
    this->actor.colChkInfo.health = GARO_MASTER_HEALTH;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 0.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sGaroMasterSkel,
                       (AnimationHeader*)sAppearAndDrawSwordsAnim, this->jointTable, this->morphTable,
                       GARO_MASTER_LIMB_MAX);
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyCylinderInit);
    Collider_InitQuad(play, &this->rightSwordCollider);
    Collider_SetQuad(play, &this->rightSwordCollider, &this->actor, &sSwordQuadInit);
    Collider_InitQuad(play, &this->leftSwordCollider);
    Collider_SetQuad(play, &this->leftSwordCollider, &this->actor, &sSwordQuadInit);
    for (s32 i = 0; i < 3; i++) {
        blureInit.p1StartColor[i] = i == 0 ? 255 : 0;
        blureInit.p2StartColor[i] = i == 0 ? 255 : 0;
        blureInit.p1EndColor[i] = i == 0 ? 255 : 0;
        blureInit.p2EndColor[i] = i == 1 ? 0 : 255;
    }
    blureInit.p1StartColor[3] = 128;
    blureInit.p2StartColor[3] = 64;
    blureInit.elemDuration = 8;
    blureInit.calcMode = 2;
    Effect_Add(play, &this->rightSwordBlureIndex, EFFECT_BLURE1, 0, 0, &blureInit);
    Effect_Add(play, &this->leftSwordBlureIndex, EFFECT_BLURE1, 0, 0, &blureInit);
    this->scale = GARO_MASTER_SCALE;
    this->flameScroll = Rand_S16Offset(0, 7);
    this->alpha = 255;
    this->actor.gravity = -3.0f;
    this->actor.shape.yOffset = 960.0f;
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->bodyCollider.base.acFlags |= AC_HARD;
    this->action = GARO_MASTER_ACTION_WAIT;
    this->actionFunc = GaroMaster_WaitForPlayer;
}

static void EnGaroMaster_Destroy(Actor* thisx, PlayState* play) {
    EnGaroMaster* this = (EnGaroMaster*)thisx;

    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyQuad(play, &this->rightSwordCollider);
    Collider_DestroyQuad(play, &this->leftSwordCollider);
    Effect_Delete(play, this->rightSwordBlureIndex);
    Effect_Delete(play, this->leftSwordBlureIndex);
    if (this->bomb != NULL && GaroMaster_IsBombAlive(this, play)) {
        this->bomb->actor.parent = NULL;
    }
    GaroMaster_StopMinibossMusic(this);
}

static void EnGaroMaster_Update(Actor* thisx, PlayState* play) {
    EnGaroMaster* this = (EnGaroMaster*)thisx;

    if (this->action == GARO_MASTER_ACTION_WAIT) {
        this->actionFunc(this, play);
        return;
    }
    if (this->action != GARO_MASTER_ACTION_CIRCLE_PLAYER) {
        SkelAnime_Update(&this->skelAnime);
    }
    DECR(this->attackMovementTimer);
    DECR(this->attackTimer);
    DECR(this->timer);
    DECR(this->auraTimer);
    GaroMaster_UpdateDamage(this, play);
    Actor_SetScale(&this->actor, this->scale);
    this->actionFunc(this, play);
    Actor_SetFocus(&this->actor, 80.0f);
    Actor_MoveXZGravity(&this->actor);
    if (GaroMaster_IsGrounded(this)) {
        this->actor.world.pos.x += this->knockbackVelocity.x;
        this->actor.world.pos.z += this->knockbackVelocity.z;
        Math_ApproachZeroF(&this->knockbackVelocity.x, 1.0f, 2.0f);
        Math_ApproachZeroF(&this->knockbackVelocity.z, 1.0f, 2.0f);
    }
    Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 60.0f, 60.0f, BGCHECK_UPDATE_WALL_FLOOR_WATER);
    GaroMaster_RecordAfterimage(this);
    if (!GaroMaster_IsShapeYawFree(this)) {
        this->actor.shape.rot.y = this->actor.world.rot.y;
    }
    this->actor.shape.rot.x = this->actor.world.rot.x;
    GaroMaster_SubmitColliders(this, play);
}

static s32 GaroMaster_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                       void* thisx) {
    EnGaroMaster* this = (EnGaroMaster*)thisx;

    if (this->areSwordsDrawn) {
        return false;
    }
    bool isLeaningOnSword = limbIndex == GARO_MASTER_LIMB_LEFT_SWORD && this->action == GARO_MASTER_ACTION_BLOW_UP;

    if ((limbIndex == GARO_MASTER_LIMB_LEFT_SWORD && !isLeaningOnSword) || limbIndex == GARO_MASTER_LIMB_RIGHT_SWORD) {
        *dList = NULL;
    }
    return false;
}

static bool GaroMaster_IsSwinging(EnGaroMaster* this) {
    return this->action == GARO_MASTER_ACTION_SLASH || this->action == GARO_MASTER_ACTION_SPIN_ATTACK ||
           this->action == GARO_MASTER_ACTION_DASH_ATTACK || this->action == GARO_MASTER_ACTION_FALL_FROM_TELEPORT;
}

// The sword quad trails from last frame's edge to this frame's; copies first, the setter overwrites its sources.
static void GaroMaster_UpdateSword(EnGaroMaster* this, ColliderQuad* collider, s32 blureIndex, s32 firstFlame) {
    bool isHolding = this->action == GARO_MASTER_ACTION_SLASH || this->action == GARO_MASTER_ACTION_WAIT_AFTER_SLASH;
    Vec3f* flameOffsets = isHolding ? sSlashFlameOffsets : sFlameOffsets;
    Vec3f previousBase = collider->dim.quad[0];
    Vec3f previousTip = collider->dim.quad[1];
    Vec3f quadTip;
    Vec3f base;
    Vec3f tip;

    Matrix_MultVec3f(&sSwordTipQuadOffset, &quadTip);
    Matrix_MultVec3f(&sZeroVec, &base);
    Collider_SetQuadVertices(collider, &base, &quadTip, &previousBase, &previousTip);
    Matrix_MultVec3f(&sSwordTipOffset, &tip);
    for (s32 i = firstFlame; i < firstFlame + GARO_MASTER_FLAME_COUNT / 2; i++) {
        Matrix_MultVec3f(&flameOffsets[i], &this->flamePos[i]);
    }
    if (GaroMaster_IsSwinging(this) && !this->disableBlure) {
        EffectBlure_AddVertex(Effect_GetByIndex(blureIndex), &tip, &base);
    } else if (this->disableBlure) {
        EffectBlure_AddSpace(Effect_GetByIndex(blureIndex));
    }
}

static bool IsGaroMasterBodyPart(s32 limbIndex) {
    switch (limbIndex) {
        case GARO_MASTER_LIMB_LEFT_SWORD:
        case GARO_MASTER_LIMB_RIGHT_SWORD:
        case GARO_MASTER_LIMB_ROBE_TOP:
        case GARO_MASTER_LIMB_ROBE_BACK:
        case GARO_MASTER_LIMB_ROBE_LEFT:
        case GARO_MASTER_LIMB_ROBE_RIGHT:
        case GARO_MASTER_LIMB_ROBE_FRONT:
        case GARO_MASTER_LIMB_HEAD:
        case GARO_MASTER_LIMB_RIGHT_THIGH:
        case GARO_MASTER_LIMB_RIGHT_FOOT:
        case GARO_MASTER_LIMB_LEFT_THIGH:
        case GARO_MASTER_LIMB_LEFT_FOOT:
            return true;
        default:
            return false;
    }
}

static void GaroMaster_DrawEyes(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGaroMasterEyesDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void GaroMaster_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnGaroMaster* this = (EnGaroMaster*)thisx;

    if (limbIndex == GARO_MASTER_LIMB_LEFT_SWORD) {
        GaroMaster_UpdateSword(this, &this->leftSwordCollider, this->leftSwordBlureIndex, 0);
    } else if (limbIndex == GARO_MASTER_LIMB_RIGHT_SWORD) {
        GaroMaster_UpdateSword(this, &this->rightSwordCollider, this->rightSwordBlureIndex, 3);
        this->disableBlure = false;
    } else if (limbIndex == GARO_MASTER_LIMB_ROBE_RIGHT) {
        Matrix_MultVec3f(&sBombOffset, &this->bombPos);
    } else if (limbIndex == GARO_MASTER_LIMB_HEAD) {
        GaroMaster_DrawEyes(play);
    }
    if (this->action != GARO_MASTER_ACTION_BLOW_UP && IsGaroMasterBodyPart(limbIndex)) {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[this->bodyPartIndex]);
        this->bodyPartIndex = (this->bodyPartIndex + 1) % GARO_MASTER_BODYPART_MAX;
    }
}

// MM's Scene_SetRenderModeXlu tables for segment 0x0C: object_jso calls entries 0 and 2.
static Gfx sRenderModeOpaDL[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};
static Gfx sRenderModeXluDL[] = {
    gsDPSetRenderMode(AA_EN | Z_CMP | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                          GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1),
                      G_RM_AA_ZB_XLU_SURF2),
    gsSPEndDisplayList(),
    gsDPSetRenderMode(AA_EN | Z_CMP | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                          GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_IN, G_BL_1MA),
                      G_RM_AA_ZB_XLU_SURF2),
    gsSPEndDisplayList(),
};

static void GaroMaster_DrawAfterimages(EnGaroMaster* this, PlayState* play) {
    s32 index = this->afterimageIndex;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    for (s32 i = 0; i < this->afterimageCount;
         i++, index = (index + GARO_MASTER_AFTERIMAGE_COUNT - 1) % GARO_MASTER_AFTERIMAGE_COUNT) {
        if (sAfterimageAlpha[i] == 0) {
            continue;
        }
        Vec3s* afterimageRot = &this->afterimageRot[index];

        Matrix_Translate(this->afterimagePos[index].x, this->afterimagePos[index].y, this->afterimagePos[index].z,
                         MTXMODE_NEW);
        Matrix_Scale(this->scale, this->scale, this->scale, MTXMODE_APPLY);
        Matrix_RotateY(BINANG_TO_RAD(afterimageRot->y), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(afterimageRot->x), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(afterimageRot->z), MTXMODE_APPLY);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, sAfterimageAlpha[i]);
        gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)sRenderModeXluDL);
        POLY_XLU_DISP = SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->afterimageJointTable[index],
                                           this->skelAnime.dListCount, NULL, NULL, this, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void GaroMaster_DrawSwordFlames(EnGaroMaster* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    for (s32 i = 0; i < GARO_MASTER_FLAME_COUNT; i++) {
        Matrix_Translate(this->flamePos[i].x, this->flamePos[i].y, this->flamePos[i].z, MTXMODE_NEW);
        Matrix_Scale(this->flameScale[i], this->flameScale[i], this->flameScale[i], MTXMODE_APPLY);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0,
                                               ((this->flameScroll * 10) - (play->state.frames * 20)) & 0x1FF, 32,
                                               128));
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 170, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 50, 0, 255);
        Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's damage-effect fire and light orbs become OoT's flame billboard tinted per element.
static void GaroMaster_DrawAura(EnGaroMaster* this, PlayState* play) {
    bool isLight = this->aura == GARO_MASTER_AURA_LIGHT;
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
    for (s32 i = 0; i < GARO_MASTER_BODYPART_MAX; i++) {
        billboard.xw = this->bodyPartsPos[i].x;
        billboard.yw = this->bodyPartsPos[i].y;
        billboard.zw = this->bodyPartsPos[i].z;
        Matrix_Put(&billboard);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void GaroMaster_DrawBody(EnGaroMaster* this, PlayState* play) {
    if (!this->isFadingAway) {
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
        CLOSE_DISPS(play->state.gfxCtx);
        SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                              GaroMaster_OverrideLimbDraw, GaroMaster_PostLimbDraw, this);
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->alpha);
    gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)sRenderModeXluDL);
    POLY_XLU_DISP = SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                       this->skelAnime.dListCount, NULL, NULL, this, POLY_XLU_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnGaroMaster_Draw(Actor* thisx, PlayState* play) {
    EnGaroMaster* this = (EnGaroMaster*)thisx;

    if (this->action == GARO_MASTER_ACTION_WAIT) {
        return;
    }
    GaroMaster_DrawBody(this, play);
    if (this->afterimageCount > 0) {
        GaroMaster_DrawAfterimages(this, play);
    }
    bool areFlamesLit = this->action < GARO_MASTER_ACTION_TELEPORT && this->areSwordsDrawn &&
                        !GaroMaster_IsFrozen(this);

    if (areFlamesLit) {
        GaroMaster_DrawSwordFlames(this, play);
    }
    if (this->auraTimer != 0 && (this->aura == GARO_MASTER_AURA_FIRE || this->aura == GARO_MASTER_AURA_LIGHT)) {
        GaroMaster_DrawAura(this, play);
    }
}

static bool CanBeGoron(void) {
    int32_t goronMask = sApi->GetRandoItem(GORON_MASK_KEY);

    return goronMask >= 0 && sApi->LogicHasItem(goronMask);
}

// MM's CanKillEnemy(ACTOR_EN_JSO2): bow, a sword or Goron fists; the hammer stands in for the fists.
static bool GaroMaster_CanKillInLogic(void* userData) {
    return sApi->LogicCanUseItem(RG_FAIRY_BOW) || sApi->LogicHasCapability("CanUseSword") ||
           sApi->LogicCanUseItem(RG_MEGATON_HAMMER) || CanBeGoron();
}

// The Garo family soul, shared with mm_enemy_garo: both define it alike, so either can register it.
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
    static Vec3s sSoulJointTable[GARO_MASTER_LIMB_MAX];
    static Vec3s sSoulMorphTable[GARO_MASTER_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sGaroMasterSkel,
                           (AnimationHeader*)sLookAroundAnim, sSoulJointTable, sSoulMorphTable, GARO_MASTER_LIMB_MAX);
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

static void RegisterGaroMaster(void) {
    SOHActorDefinition garoMaster = { 0 };

    garoMaster.structSize = sizeof(garoMaster);
    garoMaster.key = GARO_MASTER_KEY;
    garoMaster.description = "Garo Master";
    garoMaster.category = ACTORCAT_ENEMY;
    garoMaster.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    garoMaster.objectId = OBJECT_GAMEPLAY_KEEP;
    garoMaster.instanceSize = sizeof(EnGaroMaster);
    garoMaster.init = EnGaroMaster_Init;
    garoMaster.destroy = EnGaroMaster_Destroy;
    garoMaster.update = EnGaroMaster_Update;
    garoMaster.draw = EnGaroMaster_Draw;
    garoMaster.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_MINIBOSS | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    garoMaster.naviHint = "Garo Master&%cIts flaming blades parry everything while it circles you. Strike right "
                          "after it slashes!%w";
    garoMaster.soulKey = GARO_FAMILY_KEY;
    garoMaster.logicKill = GaroMaster_CanKillInLogic;
    if (sApi->RegisterActor(&garoMaster) < 0) {
        sApi->Log("Could not register " GARO_MASTER_KEY);
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
    RegisterGaroMaster();
}
