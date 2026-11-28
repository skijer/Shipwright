// Majora's Mask's Eyegore (En_Egol + its falling stones, En_Estone) as an Unbound custom enemy, drawn from mm.o2r.
// MM walks it along a scene path; here it guards its spawn point, turning to face Link there. No death cutscene.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/effects/ovl_Effect_Ss_HitMark/z_eff_ss_hitmark.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define EYEGORE_KEY "mm.eyegore"
#define EYEGORE_STONE_KEY "mm.eyegore.stone"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_EG "__OTR__objects/object_eg/"
#define OBJ_KEEP "__OTR__objects/gameplay_keep/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_FLOOR_WATER 0x1D
#define BGCHECK_UPDATE_FLOOR_WATER 0x1C

#define MM_SE_EN_EYEGOLE_DEAD 0x3846
#define MM_SE_EN_EYEGOLE_DAMAGE 0x3847
#define MM_SE_EN_COMMON_FREEZE 0x389E
#define MM_SE_EN_EYEGOLE_BEAM 0x391B
#define MM_SE_EN_EYEGOLE_WALK 0x3A06
#define MM_SE_EN_EYEGOLE_STAND 0x3A07
#define MM_SE_EN_EYEGOLE_SIT 0x3A08
#define MM_SE_IT_SHIELD_BOUND 0x1806
#define MM_SE_IT_BOMB_EXPLOSION 0x180E
#define MM_SE_EV_EXPLOSION 0x2802
#define MM_SE_EV_WALL_BROKEN 0x2810

#define EYEGORE_HEALTH 8
#define EYEGORE_SCALE 0.015f
#define EYEGORE_EFFECT_COUNT 100
#define EYEGORE_BODYPART_MAX 12
#define EYEGORE_STONE_EFFECT_COUNT 10
#define EYEGORE_DEFAULT_RANGE 200.0f

typedef enum {
    EYEGORE_LIMB_NONE,
    EYEGORE_LIMB_ROOT,
    EYEGORE_LIMB_TORSO,
    EYEGORE_LIMB_HEAD,
    EYEGORE_LIMB_LEFT_SHOULDER,
    EYEGORE_LIMB_LEFT_ARM,
    EYEGORE_LIMB_LEFT_HAND,
    EYEGORE_LIMB_RIGHT_SHOULDER,
    EYEGORE_LIMB_RIGHT_ARM,
    EYEGORE_LIMB_RIGHT_HAND,
    EYEGORE_LIMB_UPPER_EYELID,
    EYEGORE_LIMB_LOWER_EYELID,
    EYEGORE_LIMB_PUPIL,
    EYEGORE_LIMB_LASER_ATTACH,
    EYEGORE_LIMB_EYE_COLLIDER,
    EYEGORE_LIMB_EYEBALL,
    EYEGORE_LIMB_HIPS,
    EYEGORE_LIMB_LEFT_THIGH,
    EYEGORE_LIMB_LEFT_SHIN,
    EYEGORE_LIMB_LEFT_FOOT,
    EYEGORE_LIMB_RIGHT_THIGH,
    EYEGORE_LIMB_RIGHT_SHIN,
    EYEGORE_LIMB_RIGHT_FOOT,
    EYEGORE_LIMB_FAULDS,
    EYEGORE_LIMB_MAX
} EyegoreLimb;

typedef enum {
    EYEGORE_ACTION_WAIT,
    EYEGORE_ACTION_STAND,
    EYEGORE_ACTION_WALK,
    EYEGORE_ACTION_RETREAT,
    EYEGORE_ACTION_LASER,
    EYEGORE_ACTION_SLAM,
    EYEGORE_ACTION_PUNCH,
    EYEGORE_ACTION_SLAM_WAIT,
    EYEGORE_ACTION_STUNNED,
    EYEGORE_ACTION_STUN_END,
    EYEGORE_ACTION_SLAM_END,
    EYEGORE_ACTION_DAMAGED,
    EYEGORE_ACTION_STOP,
    EYEGORE_ACTION_DYING,
    EYEGORE_ACTION_DEAD
} EyegoreAction;

typedef enum {
    EYEGORE_EFFECT_IMPACT,
    EYEGORE_EFFECT_PIECE_LARGE,
    EYEGORE_EFFECT_PIECE_SMALL,
    EYEGORE_EFFECT_DEBRIS
} EyegoreEffectType;

typedef enum {
    EYEGORE_LASER_OFF,
    EYEGORE_LASER_START,
    EYEGORE_LASER_CHARGING,
    EYEGORE_LASER_FIRE,
    EYEGORE_LASER_ON = 7
} EyegoreLaserState;

typedef enum {
    EYEGORE_ANIM_STAND,
    EYEGORE_ANIM_WALK,
    EYEGORE_ANIM_SLAM,
    EYEGORE_ANIM_SLAM_WAIT,
    EYEGORE_ANIM_SLAM_END,
    EYEGORE_ANIM_DAMAGED,
    EYEGORE_ANIM_DEATH,
    EYEGORE_ANIM_LASER,
    EYEGORE_ANIM_STUNNED,
    EYEGORE_ANIM_STUN_END,
    EYEGORE_ANIM_RETREAT,
    EYEGORE_ANIM_SIT,
    EYEGORE_ANIM_LEFT_PUNCH,
    EYEGORE_ANIM_RIGHT_PUNCH,
    EYEGORE_ANIM_MAX
} EyegoreAnimation;

typedef enum {
    EYEGORE_DMGEFF_IMMUNE,
    EYEGORE_DMGEFF_LIGHT_ARROW = 4,
    EYEGORE_DMGEFF_NONE = 0xE,
    EYEGORE_DMGEFF_DEFLECT = 0xF
} EyegoreDamageEffect;

typedef enum {
    EYEGORE_STONE_LARGE,
    EYEGORE_STONE_SMALL
} EyegoreStoneType;

typedef struct {
    bool isActive;
    Vec3f pos;
    s16 alpha;
    s16 timer;
    s16 type;
    Vec3f velocity;
    Vec3f accel;
    Vec3s rot;
    f32 scale;
} EyegoreEffect;

struct EnEyegore;

typedef void (*EnEyegoreActionFunc)(struct EnEyegore*, PlayState*);

typedef struct EnEyegore {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[EYEGORE_LIMB_MAX];
    Vec3s morphTable[EYEGORE_LIMB_MAX];
    EnEyegoreActionFunc actionFunc;
    s16 waypoint;
    s16 action;
    s16 waitTimer;
    s16 eyeShutTimer;
    s16 headRot;
    s16 dmgEffectTimer;
    s16 bodyPartIndex;
    s16 animIndex;
    s16 eyelidRot;
    s16 eyelidRotTarget;
    s16 actionTimer;
    s16 chargeLightRot;
    s16 laserState;
    s32 laserCount;
    s32 chargeLevel;
    s32 texScroll;
    bool isRetreating;
    bool isChargingLaser;
    bool hasHitPlayer;
    f32 animEndFrame;
    f32 wakeupRange;
    f32 minLaserRange;
    f32 laserLightScale;
    f32 chargeLightScale;
    Vec3f bodyPartsPos[EYEGORE_BODYPART_MAX];
    Vec3f eyePos;
    Vec3f laserBase;
    Vec3f laserScale;
    Vec3f laserScaleTarget;
    Vec3f leftFootPos;
    Vec3f rightFootPos;
    Vec3f leftHandPos;
    Vec3f rightHandPos;
    Vec3f waypointPos;
    Vec3s pupilRot;
    Vec3s laserRot;
    EyegoreEffect effects[EYEGORE_EFFECT_COUNT];
    ColliderJntSph bodyCollider;
    ColliderJntSphElement bodyElements[6];
    ColliderJntSph eyeCollider;
    ColliderJntSphElement eyeElements[1];
    ColliderQuad laserCollider;
} EnEyegore;

typedef struct {
    bool isActive;
    Vec3f pos;
    Vec3f velocity;
    Vec3f accel;
    Vec3s rot;
    f32 scale;
    s16 timer;
} EyegoreStoneChip;

typedef struct EnEyegoreStone {
    Actor actor;
    ColliderCylinder collider;
    Vec3f rot;
    Vec3f rotVel;
    f32 scale;
    s16 timer;
    bool isInactive;
    EyegoreStoneChip chips[EYEGORE_STONE_EFFECT_COUNT];
} EnEyegoreStone;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sEyegoreSkel[] = OBJ_EG "gEyegoreSkel";
static const ALIGN_ASSET(2) char sStandAnim[] = OBJ_EG "gEyegoreStandAnim";
static const ALIGN_ASSET(2) char sWalkAnim[] = OBJ_EG "gEyegoreWalkAnim";
static const ALIGN_ASSET(2) char sSlamAnim[] = OBJ_EG "gEyegoreSlamAnim";
static const ALIGN_ASSET(2) char sSlamWaitAnim[] = OBJ_EG "gEyegoreSlamWaitAnim";
static const ALIGN_ASSET(2) char sSlamEndAnim[] = OBJ_EG "gEyegoreSlamEndAnim";
static const ALIGN_ASSET(2) char sDamagedAnim[] = OBJ_EG "gEyegoreDamagedAnim";
static const ALIGN_ASSET(2) char sDeathAnim[] = OBJ_EG "gEyegoreDeathAnim";
static const ALIGN_ASSET(2) char sLaserAnim[] = OBJ_EG "gEyegoreLaserAnim";
static const ALIGN_ASSET(2) char sStunnedAnim[] = OBJ_EG "gEyegoreStunnedAnim";
static const ALIGN_ASSET(2) char sStunEndAnim[] = OBJ_EG "gEyegoreStunEndAnim";
static const ALIGN_ASSET(2) char sRetreatAnim[] = OBJ_EG "gEyegoreRetreatAnim";
static const ALIGN_ASSET(2) char sSitAnim[] = OBJ_EG "gEyegoreSitAnim";
static const ALIGN_ASSET(2) char sLeftPunchAnim[] = OBJ_EG "gEyegoreLeftPunchAnim";
static const ALIGN_ASSET(2) char sRightPunchAnim[] = OBJ_EG "gEyegoreRightPunchAnim";
static const ALIGN_ASSET(2) char sLaserDL[] = OBJ_EG "gEyegoreLaserDL";
static const ALIGN_ASSET(2) char sStoneDL[] = OBJ_EG "gEyegoreStoneDL";
static const ALIGN_ASSET(2) char sImpactDL[] = OBJ_EG "gEyegoreEffectImpactDL";
static const ALIGN_ASSET(2) char sLargePieceDL[] = OBJ_EG "gEyegoreEffectLargeBodyPieceDL";
static const ALIGN_ASSET(2) char sSmallPieceDL[] = OBJ_EG "gEyegoreEffectSmallBodyPieceDL";
static const ALIGN_ASSET(2) char sSolidDebrisDL[] = OBJ_EG "gEyegoreEffectSolidDebrisDL";
static const ALIGN_ASSET(2) char sLightOrbMaterialDL[] = OBJ_KEEP "gLightOrbMaterial1DL";
static const ALIGN_ASSET(2) char sLightOrbModelDL[] = OBJ_KEEP "gLightOrbModelDL";
static const ALIGN_ASSET(2) char sRockChipDL[] = OBJ_KEEP "gameplay_keep_DL_06AB30";

static const char* const sAnimations[EYEGORE_ANIM_MAX] = {
    sStandAnim,   sWalkAnim,    sSlamAnim,    sSlamWaitAnim,   sSlamEndAnim,  sDamagedAnim,   sDeathAnim,
    sLaserAnim,   sStunnedAnim, sStunEndAnim, sRetreatAnim,    sSitAnim,      sLeftPunchAnim, sRightPunchAnim,
};

static const u8 sAnimationModes[EYEGORE_ANIM_MAX] = {
    ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_ONCE,
    ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_ONCE,
};

static const Color_RGB8 sEyePrimColors[] = { { 175, 255, 255 }, { 195, 245, 10 }, { 255, 55, 205 } };
static const Color_RGB8 sEyeEnvColors[] = { { 255, 115, 155 }, { 40, 155, 155 }, { 215, 255, 255 } };
static const s16 sLaserAngles[3] = { 0x1F40, 0xBB8, 0x7D0 };

static ColliderJntSphElementInit sEyeElementsInit[1] = {
    { { ELEMTYPE_UNK2, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
      { EYEGORE_LIMB_EYE_COLLIDER, { { 500, 0, 0 }, 26 }, 100 } },
};

static ColliderJntSphElementInit sBodyElementsInit[6] = {
    { { ELEMTYPE_UNK2,
        { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x10 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_ON },
      { EYEGORE_LIMB_LEFT_HAND, { { 1800, 100, 0 }, 20 }, 100 } },
    { { ELEMTYPE_UNK2,
        { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x10 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_ON },
      { EYEGORE_LIMB_RIGHT_HAND, { { 1800, 100, 0 }, 20 }, 100 } },
    { { ELEMTYPE_UNK2, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
      { EYEGORE_LIMB_HEAD, { { 1000, -500, 0 }, 35 }, 100 } },
    { { ELEMTYPE_UNK2, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
      { EYEGORE_LIMB_LEFT_SHOULDER, { { 200, 200, 0 }, 25 }, 100 } },
    { { ELEMTYPE_UNK2, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
      { EYEGORE_LIMB_RIGHT_SHOULDER, { { 300, 200, 0 }, 25 }, 100 } },
    { { ELEMTYPE_UNK2, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
      { EYEGORE_LIMB_FAULDS, { { 2100, -300, 0 }, 37 }, 100 } },
};

static ColliderJntSphInit sBodyJntSphInit = {
    { COLTYPE_METAL, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    ARRAY_COUNT(sBodyElementsInit),
    sBodyElementsInit,
};

static ColliderJntSphInit sEyeJntSphInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_JNTSPH },
    ARRAY_COUNT(sEyeElementsInit),
    sEyeElementsInit,
};

static ColliderQuadInit sLaserQuadInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_QUAD },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_ELECTRIC, 0x10 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
      BUMP_NONE,
      OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static ColliderCylinderInit sStoneCylinderInit = {
    { COLTYPE_HARD, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x04 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_ON,
      OCELEM_NONE },
    { 30, 30, -10, { 0, 0, 0 } },
};

// MM entries multiply the weapon's own damage; OoT's are absolute. Only hits to the eye count.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Deku stick    */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Slingshot     */ DMG_ENTRY(1, EYEGORE_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(4, EYEGORE_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, EYEGORE_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, EYEGORE_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, EYEGORE_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(2, EYEGORE_DMGEFF_NONE),
    /* Kokiri sword  */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Master sword  */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Giant's Knife */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Fire arrow    */ DMG_ENTRY(4, EYEGORE_DMGEFF_NONE),
    /* Ice arrow     */ DMG_ENTRY(4, EYEGORE_DMGEFF_NONE),
    /* Light arrow   */ DMG_ENTRY(4, EYEGORE_DMGEFF_LIGHT_ARROW),
    /* Unk arrow 1   */ DMG_ENTRY(2, EYEGORE_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, EYEGORE_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, EYEGORE_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Ice magic     */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Light magic   */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Shield        */ DMG_ENTRY(0, EYEGORE_DMGEFF_IMMUNE),
    /* Mirror Ray    */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Kokiri spin   */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Giant spin    */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Master spin   */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Kokiri jump   */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Giant jump    */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Master jump   */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Unknown 1     */ DMG_ENTRY(0, EYEGORE_DMGEFF_IMMUNE),
    /* Unblockable   */ DMG_ENTRY(0, EYEGORE_DMGEFF_IMMUNE),
    /* Hammer jump   */ DMG_ENTRY(0, EYEGORE_DMGEFF_DEFLECT),
    /* Unknown 2     */ DMG_ENTRY(0, EYEGORE_DMGEFF_IMMUNE),
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3s sZeroRot = { 0, 0, 0 };
static Vec3f sFootOffset = { 1000.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;
static int16_t sStoneActorId = -1;

static void Eyegore_Wait(EnEyegore* this, PlayState* play);
static void Eyegore_SetupWalk(EnEyegore* this);
static void Eyegore_SetupStop(EnEyegore* this);
static void Eyegore_SetupSlam(EnEyegore* this);

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

static void Eyegore_ChangeAnim(EnEyegore* this, s32 animIndex) {
    AnimationHeader* animation = (AnimationHeader*)sAnimations[animIndex];

    this->animIndex = animIndex;
    this->animEndFrame = Animation_GetLastFrame(animation);
    Animation_Change(&this->skelAnime, animation, 1.0f, 0.0f, this->animEndFrame, sAnimationModes[animIndex], 0.0f);
}

static bool Eyegore_IsAnimDone(EnEyegore* this) {
    return this->skelAnime.curFrame >= this->animEndFrame;
}

static bool Eyegore_IsFacingPlayer(EnEyegore* this, s16 facing) {
    s16 angleToFacing = facing - this->actor.yawTowardsPlayer;

    return ABS(angleToFacing) < 0x3000;
}

static bool Eyegore_IsPlayerLevel(EnEyegore* this, PlayState* play) {
    return this->actor.world.pos.y - 50.0f <= GET_PLAYER(play)->actor.world.pos.y;
}

static void Eyegore_Quake(EnEyegore* this, PlayState* play) {
    f32 dyToPlayer = fabsf(GET_PLAYER(play)->actor.world.pos.y - this->actor.world.pos.y);
    s32 quakeYOffset = CLAMP(4 - (s32)(dyToPlayer * 0.02f), 1, 4);

    Actor_RequestQuakeAndRumble(&this->actor, play, quakeYOffset, 2);
}

static void Eyegore_SpawnEffect(EnEyegore* this, Vec3f* pos, Vec3s* rot, s16 lifetime, f32 scale, s16 type) {
    for (s32 i = 0; i < EYEGORE_EFFECT_COUNT; i++) {
        EyegoreEffect* effect = &this->effects[i];

        if (effect->isActive) {
            continue;
        }
        effect->isActive = true;
        effect->scale = scale;
        effect->pos = *pos;
        effect->timer = lifetime;
        effect->alpha = 255;
        effect->type = type;
        effect->rot.x = Rand_CenteredFloat(0x7530);
        effect->rot.y = Rand_CenteredFloat(0x7530);
        effect->rot.z = Rand_CenteredFloat(0x7530);
        effect->velocity = sZeroVec;
        effect->accel = sZeroVec;
        if (type == EYEGORE_EFFECT_PIECE_LARGE || type == EYEGORE_EFFECT_PIECE_SMALL) {
            effect->accel.y = -1.0f;
            effect->velocity.x = 4.0f * (Rand_ZeroOne() - 0.5f);
            effect->velocity.y = 10.0f + 10.0f * Rand_ZeroOne();
            effect->velocity.z = 4.0f * (Rand_ZeroOne() - 0.5f);
        } else if (type == EYEGORE_EFFECT_DEBRIS) {
            effect->accel.y = -1.0f;
            effect->velocity.x = 0.5f * (Rand_ZeroOne() - 0.5f);
            effect->velocity.y = 5.0f + 5.0f * Rand_ZeroOne();
            effect->velocity.z = 0.5f * (Rand_ZeroOne() - 0.5f);
            effect->timer = 20.0f + Rand_ZeroFloat(10.0f);
        } else {
            effect->rot.x = rot->x;
            effect->rot.y = 0;
            effect->rot.z = rot->z;
        }
        return;
    }
}

static void Eyegore_SpawnFootprint(EnEyegore* this, PlayState* play, Vec3f* footPos) {
    Vec3f pos = { footPos->x, this->actor.floorHeight + 2.0f, footPos->z };

    if (Eyegore_IsPlayerLevel(this, play)) {
        Eyegore_SpawnEffect(this, &pos, &sZeroRot, 100, 0.005f, EYEGORE_EFFECT_IMPACT);
    }
}

static void Eyegore_FootstepEffects(EnEyegore* this, PlayState* play, f32 leftFootFrame, f32 rightFootFrame) {
    bool isLeftStep = Animation_OnFrame(&this->skelAnime, leftFootFrame);

    if (!isLeftStep && !Animation_OnFrame(&this->skelAnime, rightFootFrame)) {
        return;
    }
    PlaySfx(&this->actor, MM_SE_EN_EYEGOLE_WALK, NA_SE_EN_AMOS_WALK);
    Eyegore_Quake(this, play);
    Vec3f dustPos = isLeftStep ? this->leftFootPos : this->rightFootPos;

    dustPos.y = this->actor.floorHeight;
    Actor_SpawnFloorDustRing(play, &this->actor, &dustPos, 0.0f, 10, 6.0f, 50, 30, true);
}

static void Eyegore_SpawnStones(EnEyegore* this, PlayState* play, Vec3f* pos, s32 count, s16 type) {
    for (s32 i = 0; i < count; i++) {
        Actor_Spawn(&play->actorCtx, play, sStoneActorId, pos->x, pos->y, pos->z, 0, this->actor.world.rot.y, 0, type);
    }
}

static void Eyegore_ResetLaser(EnEyegore* this) {
    this->isChargingLaser = false;
    this->chargeLevel = 0;
    this->actionTimer = 0;
    this->laserState = EYEGORE_LASER_OFF;
    this->laserScale = sZeroVec;
    this->laserScaleTarget = sZeroVec;
    for (s32 i = 0; i < 4; i++) {
        this->laserCollider.dim.quad[i] = this->laserBase;
    }
}

static bool Eyegore_IsAtPost(EnEyegore* this) {
    return Actor_WorldDistXZToPoint(&this->actor, &this->waypointPos) < 4.0f;
}

static void Eyegore_SetupWait(EnEyegore* this) {
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->action = EYEGORE_ACTION_WAIT;
    this->actionFunc = Eyegore_Wait;
}

static void Eyegore_Stand(EnEyegore* this, PlayState* play) {
    if (Eyegore_IsAnimDone(this)) {
        Eyegore_SetupWalk(this);
    }
}

static void Eyegore_Wait(EnEyegore* this, PlayState* play) {
    bool isWoken = Eyegore_IsAnimDone(this) && Eyegore_IsFacingPlayer(this, this->actor.shape.rot.y) &&
                   this->actor.xzDistToPlayer < this->wakeupRange;

    if (!isWoken) {
        return;
    }
    Eyegore_ChangeAnim(this, EYEGORE_ANIM_STAND);
    this->actor.flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;
    this->action = EYEGORE_ACTION_STAND;
    PlaySfx(&this->actor, MM_SE_EN_EYEGOLE_STAND, NA_SE_EN_AMOS_VOICE);
    this->actionFunc = Eyegore_Stand;
}

static void Eyegore_SetupLaser(EnEyegore* this);

// Stomps back to its post; once there it turns in place to keep Link in front of it.
static void Eyegore_Walk(EnEyegore* this, PlayState* play) {
    Math_SmoothStepToS(&this->headRot, 0, 1, 0x3E8, 0);
    if (Eyegore_IsAnimDone(this) && this->actor.xzDistToPlayer > this->minLaserRange) {
        Eyegore_SetupLaser(this);
        return;
    }
    if (Animation_OnFrame(&this->skelAnime, 24.0f)) {
        Eyegore_SpawnFootprint(this, play, &this->rightFootPos);
    }
    if (Animation_OnFrame(&this->skelAnime, 9.0f)) {
        Eyegore_SpawnFootprint(this, play, &this->leftFootPos);
    }
    if (Eyegore_IsFacingPlayer(this, this->actor.world.rot.y) && this->actor.xzDistToPlayer < 100.0f) {
        Eyegore_SetupSlam(this);
        return;
    }
    f32 frame = this->skelAnime.curFrame;
    bool isStepping = !(frame == 0.0f || (frame >= 9.0f && frame <= 15.0f) || (frame >= 24.0f && frame <= 29.0f));
    bool isAtPost = Eyegore_IsAtPost(this);

    if (isStepping) {
        Math_ApproachF(&this->actor.world.pos.x, this->waypointPos.x, 0.5f,
                       fabsf(Math_SinS(this->actor.world.rot.y) * 4.0f));
        Math_ApproachF(&this->actor.world.pos.z, this->waypointPos.z, 0.5f,
                       fabsf(Math_CosS(this->actor.world.rot.y) * 4.0f));
    }
    s16 targetYaw = isAtPost ? this->actor.yawTowardsPlayer : Math_Vec3f_Yaw(&this->actor.world.pos, &this->waypointPos);

    Math_SmoothStepToS(&this->actor.world.rot.y, targetYaw, 1, 0x7D0, 0);
    Eyegore_FootstepEffects(this, play, 9.0f, 24.0f);
    s16 angleBehind = this->actor.world.rot.y - this->actor.yawTowardsPlayer + 0x8000;

    if (ABS(angleBehind) < 0x2000 && !isAtPost) {
        Eyegore_SetupStop(this);
    }
}

static void Eyegore_SetupWalk(EnEyegore* this) {
    Eyegore_ChangeAnim(this, EYEGORE_ANIM_WALK);
    this->laserCount = 0;
    this->action = EYEGORE_ACTION_WALK;
    this->actionFunc = Eyegore_Walk;
}

static void Eyegore_Retreat(EnEyegore* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    bool isTrampling = Eyegore_IsFacingPlayer(this, this->actor.world.rot.y) &&
                       fabsf(this->actor.world.pos.y - player->actor.world.pos.y) < 50.0f &&
                       this->actor.xzDistToPlayer < 100.0f && player->invincibilityTimer == 0;

    if (isTrampling) {
        Actor_SetPlayerKnockbackLarge(play, &this->actor, 2.0f,
                                      (s32)Rand_CenteredFloat(0x2000) + this->actor.world.rot.y, 5.0f, 0x10);
    }
    if (Animation_OnFrame(&this->skelAnime, 5.0f)) {
        Eyegore_SpawnFootprint(this, play, &this->rightFootPos);
    }
    if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
        Eyegore_SpawnFootprint(this, play, &this->leftFootPos);
    }
    Math_ApproachF(&this->actor.world.pos.x, this->waypointPos.x, 0.5f,
                   fabsf(Math_SinS(this->actor.world.rot.y) * 10.0f));
    Math_ApproachF(&this->actor.world.pos.z, this->waypointPos.z, 0.5f,
                   fabsf(Math_CosS(this->actor.world.rot.y) * 10.0f));
    Math_SmoothStepToS(&this->actor.world.rot.y, Math_Vec3f_Yaw(&this->actor.world.pos, &this->waypointPos), 1, 0x7D0,
                       0);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.world.rot.y + 0x8000, 1, 0x7D0, 0);
    Eyegore_FootstepEffects(this, play, 5.0f, 0.0f);
    if (Eyegore_IsAtPost(this)) {
        Eyegore_SetupStop(this);
    }
}

static void Eyegore_SetupRetreat(EnEyegore* this) {
    Eyegore_ChangeAnim(this, EYEGORE_ANIM_RETREAT);
    this->laserCount = 0;
    this->actor.world.rot.y = Math_Vec3f_Yaw(&this->actor.world.pos, &this->waypointPos);
    this->actor.shape.rot.y = this->actor.world.rot.y + 0x8000;
    this->action = EYEGORE_ACTION_RETREAT;
    this->actionFunc = Eyegore_Retreat;
}

static void Eyegore_ChargeLaser(EnEyegore* this) {
    switch (this->chargeLevel) {
        case 0:
            this->waitTimer = 10;
            this->chargeLevel++;
            break;
        case 1:
            Math_ApproachF(&this->chargeLightScale, 8.0f, 0.5f, 2.5f);
            if (this->waitTimer == 0) {
                this->waitTimer = 10;
                this->chargeLevel++;
            }
            break;
        default:
            Math_ApproachF(&this->chargeLightScale, 1.0f, 0.5f, 1.0f);
            if (this->waitTimer == 0) {
                this->chargeLevel = 0;
                this->laserState = EYEGORE_LASER_FIRE;
                this->isChargingLaser = false;
                this->chargeLightScale = 0.0f;
            }
            break;
    }
    this->chargeLightRot += 0x7D0;
}

// The beam hits the floor: a shockwave, smoke and a few stones kicked up toward Link.
static void Eyegore_LaserImpact(EnEyegore* this, PlayState* play, Vec3f* hitPos, CollisionPoly* poly) {
    f32 nx = COLPOLY_GET_NORMAL(poly->normal.x);
    f32 ny = COLPOLY_GET_NORMAL(poly->normal.y);
    f32 nz = COLPOLY_GET_NORMAL(poly->normal.z);
    Vec3s rotToNormal = { RADF_TO_BINANG(-Math_FAtan2F(-nz * ny, 1.0f)), 0,
                          RADF_TO_BINANG(Math_FAtan2F(-nx * ny, 1.0f)) };
    bool isPlayerLevel = Eyegore_IsPlayerLevel(this, play);

    if (isPlayerLevel) {
        Eyegore_SpawnEffect(this, hitPos, &rotToNormal, 100, 0.02f, EYEGORE_EFFECT_IMPACT);
    }
    Eyegore_Quake(this, play);
    PlaySfx(&this->actor, MM_SE_EV_EXPLOSION, NA_SE_EV_EXPLOSION);
    EffectSsDeadDs_Spawn(play, hitPos, &sZeroVec, &sZeroVec, 40, -2, 255, 20);
    func_80033480(play, hitPos, 6.0f, 2, 120, 20, true);
    if (isPlayerLevel && this->actor.floorBgId == BGCHECK_SCENE) {
        Vec3f stonePos = { hitPos->x + Math_SinS(this->actor.world.rot.y) * 60.0f, hitPos->y,
                           hitPos->z + Math_CosS(this->actor.world.rot.y) * 60.0f };

        Eyegore_SpawnStones(this, play, &stonePos, 3, EYEGORE_STONE_SMALL);
    }
    if (isPlayerLevel) {
        for (s32 i = 0; i < 10; i++) {
            Eyegore_SpawnEffect(this, hitPos, &sZeroRot, 30, Rand_ZeroFloat(1.0f) * 0.1f + 0.2f,
                                EYEGORE_EFFECT_DEBRIS);
        }
    }
    this->actionTimer = 1;
}

static void Eyegore_FireLaser(EnEyegore* this, PlayState* play) {
    CollisionPoly* poly;
    Vec3f hitPos;
    s32 bgId;

    PlaySfx(&this->actor, MM_SE_EN_EYEGOLE_BEAM - SFX_FLAG, NA_SE_EN_BIMOS_LAZER - SFX_FLAG);
    bool hasHitFloor = this->actionTimer == 0 &&
                       BgCheck_EntityLineTest1(&play->colCtx, &this->laserCollider.dim.quad[3],
                                               &this->laserCollider.dim.quad[1], &hitPos, &poly, false, true, false,
                                               true, &bgId);

    if (hasHitFloor) {
        Eyegore_LaserImpact(this, play, &hitPos, poly);
    }
    if (this->actionTimer != 0) {
        this->actionTimer++;
        if (this->actionTimer >= 5) {
            Eyegore_ResetLaser(this);
        }
    }
    Math_ApproachF(&this->laserScale.x, this->laserScaleTarget.x, 0.5f, 0.5f);
    Math_ApproachF(&this->laserScale.y, this->laserScaleTarget.y, 0.5f, 0.5f);
    Math_ApproachF(&this->laserScale.z, this->laserScaleTarget.z, 0.5f, 0.5f);
}

static void Eyegore_StartNextBeam(EnEyegore* this) {
    if (this->laserCount >= 3) {
        Eyegore_SetupWalk(this);
        return;
    }
    this->laserRot.x = sLaserAngles[this->laserCount];
    this->laserScaleTarget.z = 0.0f;
    this->laserScale.z = 0.0f;
    if (this->laserCount == 0) {
        this->laserState = EYEGORE_LASER_START;
    } else {
        this->laserState = EYEGORE_LASER_FIRE;
        this->laserScaleTarget.x = 0.03f;
        this->laserScaleTarget.y = 0.03f;
    }
    this->laserCount++;
}

// Three sweeping beams, each landing closer; a charge glow precedes the first.
static void Eyegore_Laser(EnEyegore* this, PlayState* play) {
    if (Eyegore_IsFacingPlayer(this, this->actor.world.rot.y) && this->actor.xzDistToPlayer < 100.0f) {
        Eyegore_ResetLaser(this);
        Eyegore_SetupSlam(this);
        return;
    }
    if (this->actor.xzDistToPlayer < this->minLaserRange) {
        Eyegore_ResetLaser(this);
        Eyegore_SetupWalk(this);
        return;
    }
    if (this->isChargingLaser) {
        Eyegore_ChargeLaser(this);
    }
    s16 headTarget = (this->laserState != EYEGORE_LASER_OFF || this->laserCount != 0) ? -0x2710 : 0;

    Math_SmoothStepToS(&this->headRot, headTarget, 5, 0x1F4, 5);
    if (this->laserState == EYEGORE_LASER_OFF) {
        Eyegore_StartNextBeam(this);
    } else if (this->laserState >= EYEGORE_LASER_FIRE) {
        Eyegore_FireLaser(this, play);
    }
}

static void Eyegore_SetupLaser(EnEyegore* this) {
    Eyegore_ChangeAnim(this, EYEGORE_ANIM_LASER);
    this->actionTimer = 0;
    this->laserCount = 0;
    this->action = EYEGORE_ACTION_LASER;
    this->actionFunc = Eyegore_Laser;
}

static void Eyegore_Stop(EnEyegore* this, PlayState* play) {
    Eyegore_FootstepEffects(this, play, 28.0f, 13.0f);
    bool isSettled = true;

    if (this->isRetreating) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.home.rot.y, 5, 0xBB8, 5);
        this->actor.world.rot.y = this->actor.shape.rot.y;
        s16 angleToHome = this->actor.world.rot.y - this->actor.home.rot.y;

        isSettled = ABS(angleToHome) < 0x64;
    }
    if (!isSettled || !Eyegore_IsAnimDone(this)) {
        return;
    }
    this->isRetreating = !this->isRetreating;
    if (this->isRetreating) {
        Eyegore_SetupRetreat(this);
        return;
    }
    this->actor.world.rot.y = this->actor.home.rot.y;
    this->actor.shape.rot.y = this->actor.home.rot.y;
    Eyegore_ChangeAnim(this, EYEGORE_ANIM_SIT);
    PlaySfx(&this->actor, MM_SE_EN_EYEGOLE_SIT, NA_SE_EN_AMOS_VOICE);
    this->laserCount = 0;
    Eyegore_SetupWait(this);
}

static void Eyegore_SetupStop(EnEyegore* this) {
    Eyegore_ChangeAnim(this, this->isRetreating ? EYEGORE_ANIM_STUN_END : EYEGORE_ANIM_LASER);
    this->action = EYEGORE_ACTION_STOP;
    this->actionFunc = Eyegore_Stop;
}

static void Eyegore_SlamEnd(EnEyegore* this, PlayState* play) {
    if (Eyegore_IsAnimDone(this)) {
        Eyegore_SetupWalk(this);
    }
}

// Head down after a slam, the eye is open to attack for 20 frames.
static void Eyegore_SlamWait(EnEyegore* this, PlayState* play) {
    if (!Eyegore_IsAnimDone(this)) {
        return;
    }
    this->actionTimer++;
    if (this->actionTimer <= 20) {
        return;
    }
    this->eyeCollider.elements[0].info.elemType = ELEMTYPE_UNK2;
    Eyegore_ChangeAnim(this, EYEGORE_ANIM_SLAM_END);
    this->actionTimer = 0;
    this->action = EYEGORE_ACTION_SLAM_END;
    this->actionFunc = Eyegore_SlamEnd;
}

static void Eyegore_SlamImpact(EnEyegore* this, PlayState* play) {
    PlaySfx(&this->actor, MM_SE_EV_EXPLOSION, NA_SE_EV_EXPLOSION);
    Eyegore_Quake(this, play);
    if (this->actor.floorBgId == BGCHECK_SCENE) {
        Vec3f stonePos = { this->actor.world.pos.x + Math_SinS(this->actor.world.rot.y) * 60.0f,
                           this->actor.floorHeight,
                           this->actor.world.pos.z + Math_CosS(this->actor.world.rot.y) * 60.0f };
        Vec3f markPos = { this->actor.world.pos.x + Math_SinS(this->actor.world.rot.y) * 55.0f,
                          this->actor.floorHeight + 2.0f,
                          this->actor.world.pos.z + Math_CosS(this->actor.world.rot.y) * 55.0f };

        Eyegore_SpawnStones(this, play, &stonePos, Rand_S16Offset(1, 3), EYEGORE_STONE_LARGE);
        Actor_SpawnFloorDustRing(play, &this->actor, &stonePos, 30.0f, 30, 10.0f, 100, 30, true);
        Eyegore_SpawnEffect(this, &markPos, &sZeroRot, 100, 0.03f, EYEGORE_EFFECT_IMPACT);
    }
}

static void Eyegore_Slam(EnEyegore* this, PlayState* play) {
    Math_SmoothStepToS(&this->headRot, 0, 1, 0x3E8, 0);
    if (Animation_OnFrame(&this->skelAnime, 17.0f)) {
        Eyegore_SlamImpact(this, play);
    }
    if (Eyegore_IsAnimDone(this)) {
        Eyegore_ChangeAnim(this, EYEGORE_ANIM_SLAM_WAIT);
        this->actionTimer = 0;
        this->eyeCollider.elements[0].info.elemType = ELEMTYPE_UNK1;
        this->action = EYEGORE_ACTION_SLAM_WAIT;
        this->actionFunc = Eyegore_SlamWait;
    } else if (this->skelAnime.curFrame >= 10.0f && this->skelAnime.curFrame <= 17.0f) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->bodyCollider.base);
    }
}

static void Eyegore_SetupSlam(EnEyegore* this) {
    Eyegore_ChangeAnim(this, EYEGORE_ANIM_SLAM);
    this->action = EYEGORE_ACTION_SLAM;
    this->actionFunc = Eyegore_Slam;
}

static void Eyegore_SetHandRadius(EnEyegore* this, s16 radius) {
    this->bodyCollider.elements[0].dim.modelSphere.radius = radius;
    this->bodyCollider.elements[1].dim.modelSphere.radius = radius;
}

static void Eyegore_Punch(EnEyegore* this, PlayState* play) {
    Math_SmoothStepToS(&this->headRot, 0, 1, 0x3E8, 0);
    if (Eyegore_IsAnimDone(this)) {
        Eyegore_SetHandRadius(this, 20);
        Eyegore_SetupWalk(this);
        return;
    }
    if (this->hasHitPlayer) {
        return;
    }
    bool hasLanded = (this->bodyCollider.elements[0].info.toucherFlags & TOUCH_HIT) ||
                     (this->bodyCollider.elements[1].info.toucherFlags & TOUCH_HIT);

    if (hasLanded) {
        this->hasHitPlayer = true;
        Actor_SetPlayerKnockbackLarge(play, &this->actor, 10.0f, this->actor.home.rot.y, 10.0f, 0);
    }
    if (!(this->bodyCollider.base.atFlags & AT_BOUNCED)) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->bodyCollider.base);
    }
}

// Backhands Link when he slips around to its side.
static void Eyegore_TrySidePunch(EnEyegore* this) {
    bool isBusy = this->action == EYEGORE_ACTION_PUNCH || this->action == EYEGORE_ACTION_RETREAT ||
                  this->action >= EYEGORE_ACTION_DYING;

    if (isBusy) {
        return;
    }
    s16 angleToFacing = this->actor.shape.rot.y - this->actor.yawTowardsPlayer;
    s16 angleBehind = this->actor.world.rot.y - this->actor.yawTowardsPlayer + 0x8000;
    bool isBeside = ABS(angleToFacing) > 0x1888 && ABS(angleBehind) > 0x2000 && this->actor.xzDistToPlayer < 100.0f;

    if (!isBeside) {
        return;
    }
    s32 punchAnim = angleToFacing < 0 ? EYEGORE_ANIM_LEFT_PUNCH : EYEGORE_ANIM_RIGHT_PUNCH;

    if (this->animIndex != punchAnim) {
        Eyegore_ChangeAnim(this, punchAnim);
    }
    Eyegore_ResetLaser(this);
    Eyegore_SetHandRadius(this, 40);
    this->hasHitPlayer = false;
    this->action = EYEGORE_ACTION_PUNCH;
    this->actionFunc = Eyegore_Punch;
}

static void Eyegore_StunEnd(EnEyegore* this, PlayState* play) {
    if (Eyegore_IsAnimDone(this)) {
        Eyegore_SetupWalk(this);
    }
}

static void Eyegore_Stunned(EnEyegore* this, PlayState* play) {
    if (!Eyegore_IsAnimDone(this)) {
        return;
    }
    this->actionTimer++;
    if (this->actionTimer <= 80) {
        return;
    }
    this->eyeCollider.elements[0].info.elemType = ELEMTYPE_UNK2;
    Eyegore_SetHandRadius(this, 20);
    Eyegore_ChangeAnim(this, EYEGORE_ANIM_STUN_END);
    this->action = EYEGORE_ACTION_STUN_END;
    this->actionFunc = Eyegore_StunEnd;
}

static void Eyegore_SetupStunned(EnEyegore* this) {
    Eyegore_ChangeAnim(this, EYEGORE_ANIM_STUNNED);
    this->actionTimer = 0;
    this->eyeCollider.elements[0].info.elemType = ELEMTYPE_UNK1;
    Eyegore_SetHandRadius(this, 0);
    this->action = EYEGORE_ACTION_STUNNED;
    this->actionFunc = Eyegore_Stunned;
}

// Without MM's cutscene camera: it topples, then bursts into pieces and fades.
static void Eyegore_Death(EnEyegore* this, PlayState* play) {
    if (this->action == EYEGORE_ACTION_DEAD) {
        if (this->waitTimer == 1) {
            GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
            Actor_Kill(&this->actor);
        }
        return;
    }
    if (Animation_OnFrame(&this->skelAnime, 46.0f)) {
        PlaySfx(&this->actor, MM_SE_EV_EXPLOSION, NA_SE_EV_EXPLOSION);
        Actor_RequestQuakeAndRumble(&this->actor, play, 10, 5);
    }
    if (!Eyegore_IsAnimDone(this)) {
        return;
    }
    EffectSsBomb2_SpawnLayered(play, &this->actor.world.pos, &sZeroVec, &sZeroVec, 100, 19);
    this->waitTimer = 30;
    PlaySfx(&this->actor, MM_SE_IT_BOMB_EXPLOSION, NA_SE_IT_BOMB_EXPLOSION);
    this->action = EYEGORE_ACTION_DEAD;
    for (s32 i = 0; i < 20; i++) {
        Eyegore_SpawnEffect(this, &this->actor.world.pos, &sZeroRot, 10.0f + Rand_ZeroFloat(20.0f),
                            0.01f + 0.02f * Rand_ZeroOne(), (i & 1) + EYEGORE_EFFECT_PIECE_LARGE);
    }
}

static void Eyegore_Damaged(EnEyegore* this, PlayState* play) {
    if (!Eyegore_IsAnimDone(this)) {
        return;
    }
    if (this->actor.colChkInfo.health > 0) {
        Eyegore_SetupWalk(this);
        return;
    }
    Enemy_StartFinishingBlow(play, &this->actor);
    PlaySfx(&this->actor, MM_SE_EN_EYEGOLE_DEAD, NA_SE_EN_AMOS_DEAD);
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Eyegore_ChangeAnim(this, EYEGORE_ANIM_DEATH);
    this->action = EYEGORE_ACTION_DYING;
    this->actor.shape.shadowScale = 0.0f;
    this->actionFunc = Eyegore_Death;
}

static void Eyegore_SetupDamaged(EnEyegore* this) {
    Eyegore_ChangeAnim(this, EYEGORE_ANIM_DAMAGED);
    Eyegore_SetHandRadius(this, 20);
    this->action = EYEGORE_ACTION_DAMAGED;
    this->actionFunc = Eyegore_Damaged;
}

static bool Eyegore_IsEyeExposed(EnEyegore* this) {
    return this->action == EYEGORE_ACTION_SLAM_WAIT || this->action == EYEGORE_ACTION_STUNNED;
}

static void Eyegore_StunWithLight(EnEyegore* this) {
    Eyegore_ResetLaser(this);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 120, COLORFILTER_OPA, 40);
    PlaySfx(&this->actor, MM_SE_EN_COMMON_FREEZE, NA_SE_EN_GOMA_JR_FREEZE);
    Eyegore_SetupStunned(this);
}

static void Eyegore_TakeDamage(EnEyegore* this, PlayState* play) {
    Actor_ApplyDamage(&this->actor);
    for (s32 i = 0; i < 3; i++) {
        CollisionCheck_BlueBlood(play, NULL, &this->eyePos);
    }
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 25);
    PlaySfx(&this->actor, MM_SE_EN_EYEGOLE_DAMAGE, NA_SE_EN_AMOS_DAMAGE);
    Eyegore_SetupDamaged(this);
}

static void Eyegore_Deflect(EnEyegore* this, PlayState* play) {
    Vec3s* hit = &this->eyeCollider.elements[0].info.bumper.hitPos;
    Vec3f hitPos = { hit->x, hit->y, hit->z };

    PlaySfx(&this->actor, MM_SE_IT_SHIELD_BOUND, NA_SE_IT_SHIELD_BOUND);
    EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_METAL, &hitPos);
    CollisionCheck_SpawnShieldParticlesMetal(play, &hitPos);
}

// Only blows to the eye count: exposed it takes damage, a light arrow to its face stuns it, the rest glance off.
static void Eyegore_CheckEyeHit(EnEyegore* this, PlayState* play) {
    if ((this->action == EYEGORE_ACTION_LASER) && (this->laserCollider.base.atFlags & AT_BOUNCED)) {
        this->laserCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
        Eyegore_ResetLaser(this);
        Eyegore_SetupWalk(this);
    }
    if (!(this->eyeCollider.elements[0].info.bumperFlags & BUMP_HIT)) {
        return;
    }
    bool isFacing = Eyegore_IsFacingPlayer(this, this->actor.world.rot.y);
    bool canBeStunned = this->action >= EYEGORE_ACTION_WALK && this->action <= EYEGORE_ACTION_STUN_END &&
                        !this->isRetreating;

    switch (this->actor.colChkInfo.damageEffect) {
        case EYEGORE_DMGEFF_LIGHT_ARROW:
            if (isFacing && Eyegore_IsEyeExposed(this)) {
                this->dmgEffectTimer = 20;
                Eyegore_TakeDamage(this, play);
                return;
            }
            if (isFacing && canBeStunned) {
                Eyegore_StunWithLight(this);
                return;
            }
            break;
        case EYEGORE_DMGEFF_NONE:
            if (Eyegore_IsEyeExposed(this)) {
                Eyegore_TakeDamage(this, play);
                return;
            }
            break;
        default:
            break;
    }
    Eyegore_Deflect(this, play);
}

static void Eyegore_UpdatePupil(EnEyegore* this, PlayState* play) {
    switch (this->action) {
        case EYEGORE_ACTION_SLAM_WAIT:
        case EYEGORE_ACTION_WAIT:
        case EYEGORE_ACTION_RETREAT:
        case EYEGORE_ACTION_STAND:
        case EYEGORE_ACTION_LASER:
        case EYEGORE_ACTION_STUNNED:
        case EYEGORE_ACTION_DAMAGED:
        case EYEGORE_ACTION_DYING:
            this->pupilRot.x = 0;
            this->pupilRot.y = 0;
            return;
        default:
            break;
    }
    Player* player = GET_PLAYER(play);
    s16 pitch = Math_Vec3f_Pitch(&this->eyePos, &player->actor.world.pos);
    s16 yaw = this->actor.world.rot.y - Math_Vec3f_Yaw(&this->eyePos, &player->actor.world.pos);

    this->pupilRot.x = CLAMP(pitch, -0x7D0, 0x7D0);
    this->pupilRot.y = CLAMP(yaw, -0x1770, 0x1770);
}

// Shuts its eye against incoming shots, all but light arrows, and breaks off a laser to do so.
static void Eyegore_UpdateEyelid(EnEyegore* this, PlayState* play) {
    if (this->eyeShutTimer == 1) {
        this->eyelidRotTarget = 0;
    } else if (this->eyeShutTimer == 0) {
        bool isWatching = (this->action == EYEGORE_ACTION_WALK || this->action == EYEGORE_ACTION_LASER) &&
                          !this->isRetreating;
        Actor* projectile = isWatching ? Actor_GetProjectileActor(play, &this->actor, 80.0f) : NULL;
        bool isThreat = projectile != NULL && !(projectile->id == ACTOR_EN_ARROW && projectile->params == ARROW_LIGHT);

        this->eyelidRotTarget = 0;
        if (isThreat) {
            this->eyelidRotTarget = 0xFA0;
            this->eyeShutTimer = 20;
            if (this->action == EYEGORE_ACTION_LASER) {
                Eyegore_ResetLaser(this);
                Eyegore_SetupWalk(this);
            }
        }
    }
    Math_SmoothStepToS(&this->eyelidRot, this->eyelidRotTarget, 1, 0x7D0, 0);
}

static void Eyegore_UpdateEffects(EnEyegore* this) {
    for (s32 i = 0; i < EYEGORE_EFFECT_COUNT; i++) {
        EyegoreEffect* effect = &this->effects[i];

        if (!effect->isActive) {
            continue;
        }
        if (this->action == EYEGORE_ACTION_DYING) {
            effect->timer = 0;
        }
        if (effect->type != EYEGORE_EFFECT_IMPACT) {
            effect->rot.x += 0x100;
            effect->rot.z += 0x130;
            Math_Vec3f_Sum(&effect->pos, &effect->velocity, &effect->pos);
            effect->velocity.y += effect->accel.y;
        }
        if (effect->timer != 0) {
            effect->timer--;
        } else if (effect->type == EYEGORE_EFFECT_IMPACT) {
            effect->alpha -= 10;
            effect->isActive = effect->alpha >= 10;
        } else {
            effect->isActive = false;
        }
    }
}

static void Eyegore_SubmitColliders(EnEyegore* this, PlayState* play) {
    if (this->action == EYEGORE_ACTION_DEAD) {
        return;
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->eyeCollider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    if (this->laserState >= EYEGORE_LASER_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->laserCollider.base);
    }
}

static void EnEyegore_Init(Actor* thisx, PlayState* play) {
    EnEyegore* this = (EnEyegore*)thisx;

    this->actor.gravity = -2.0f;
    Actor_SetScale(&this->actor, EYEGORE_SCALE);
    this->actor.colChkInfo.damageTable = &sDamageTable;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 60.0f);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.colChkInfo.health = EYEGORE_HEALTH;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sEyegoreSkel, (AnimationHeader*)sStandAnim,
                       this->jointTable, this->morphTable, EYEGORE_LIMB_MAX);
    Collider_InitJntSph(play, &this->bodyCollider);
    Collider_SetJntSph(play, &this->bodyCollider, &this->actor, &sBodyJntSphInit, this->bodyElements);
    Collider_InitJntSph(play, &this->eyeCollider);
    Collider_SetJntSph(play, &this->eyeCollider, &this->actor, &sEyeJntSphInit, this->eyeElements);
    Collider_InitQuad(play, &this->laserCollider);
    Collider_SetQuad(play, &this->laserCollider, &this->actor, &sLaserQuadInit);
    this->wakeupRange = this->actor.world.rot.x > 0 ? this->actor.world.rot.x * 20.0f : EYEGORE_DEFAULT_RANGE;
    this->minLaserRange = this->actor.world.rot.z > 0 ? this->actor.world.rot.z * 20.0f : EYEGORE_DEFAULT_RANGE;
    this->actor.world.rot.x = 0;
    this->actor.world.rot.z = 0;
    this->waypointPos = this->actor.home.pos;
    Eyegore_SetupWait(this);
}

static void EnEyegore_Destroy(Actor* thisx, PlayState* play) {
    EnEyegore* this = (EnEyegore*)thisx;

    Collider_DestroyJntSph(play, &this->bodyCollider);
    Collider_DestroyJntSph(play, &this->eyeCollider);
    Collider_DestroyQuad(play, &this->laserCollider);
}

static void EnEyegore_Update(Actor* thisx, PlayState* play) {
    EnEyegore* this = (EnEyegore*)thisx;

    Eyegore_TrySidePunch(this);
    if (this->action != EYEGORE_ACTION_WAIT) {
        SkelAnime_Update(&this->skelAnime);
    }
    if (this->laserState == EYEGORE_LASER_OFF) {
        Math_ApproachZeroF(&this->laserLightScale, 0.5f, 0.5f);
    } else if (this->laserState >= EYEGORE_LASER_FIRE) {
        Math_ApproachF(&this->laserLightScale, 1.0f, 0.5f, 0.5f);
    }
    Actor_SetScale(&this->actor, EYEGORE_SCALE);
    if (this->action != EYEGORE_ACTION_STOP && this->action != EYEGORE_ACTION_RETREAT) {
        this->actor.shape.rot.y = this->actor.world.rot.y;
    }
    this->texScroll += 12;
    DECR(this->waitTimer);
    DECR(this->eyeShutTimer);
    DECR(this->dmgEffectTimer);
    Eyegore_UpdateEffects(this);
    this->actionFunc(this, play);
    this->actor.focus.pos = this->eyePos;
    this->actor.focus.rot = this->actor.world.rot;
    Eyegore_UpdatePupil(this, play);
    Eyegore_UpdateEyelid(this, play);
    Eyegore_CheckEyeHit(this, play);
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 50.0f, 50.0f, BGCHECK_UPDATE_WALL_FLOOR_WATER);
    Eyegore_SubmitColliders(this, play);
}

static s32 Eyegore_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                    void* thisx) {
    EnEyegore* this = (EnEyegore*)thisx;

    if (limbIndex == EYEGORE_LIMB_HEAD) {
        rot->z += this->headRot;
    } else if (limbIndex == EYEGORE_LIMB_UPPER_EYELID) {
        rot->z += this->eyelidRot;
    } else if (limbIndex == EYEGORE_LIMB_LOWER_EYELID) {
        rot->z -= this->eyelidRot;
    } else if (limbIndex == EYEGORE_LIMB_PUPIL) {
        rot->y += this->pupilRot.y;
        rot->z += this->pupilRot.x;
    } else if (limbIndex == EYEGORE_LIMB_EYEBALL) {
        s32 colorIndex = 0;

        if (this->action == EYEGORE_ACTION_DAMAGED || this->action == EYEGORE_ACTION_DYING) {
            colorIndex = 2;
        } else if (Eyegore_IsEyeExposed(this)) {
            colorIndex = 1;
        }
        OPEN_DISPS(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0xFF, sEyePrimColors[colorIndex].r, sEyePrimColors[colorIndex].g,
                        sEyePrimColors[colorIndex].b, 255);
        gDPSetEnvColor(POLY_OPA_DISP++, sEyeEnvColors[colorIndex].r, sEyeEnvColors[colorIndex].g,
                       sEyeEnvColors[colorIndex].b, 255);
        gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)Gfx_TexScroll(play->state.gfxCtx, 0, this->texScroll, 0, 0));
        CLOSE_DISPS(play->state.gfxCtx);
    }
    return false;
}

static void Eyegore_RotateLaser(EnEyegore* this, f32 length, Vec3f* dest) {
    Vec3f lengthZ = { 0.0f, 0.0f, length * 700.0f };

    Matrix_Push();
    Matrix_RotateY(BINANG_TO_RAD(this->actor.world.rot.y), MTXMODE_NEW);
    Matrix_RotateX(BINANG_TO_RAD(this->laserRot.x), MTXMODE_APPLY);
    Matrix_MultVec3f(&lengthZ, dest);
    Matrix_Pop();
}

// Grows the beam until its far end dips 20 units under the floor height, then rebuilds the laser quad.
static void Eyegore_AimLaser(EnEyegore* this) {
    Vec3f* quad = this->laserCollider.dim.quad;
    f32 width = this->laserScale.x * 77.0f;
    Vec3f laserVec;
    Vec3f nearLeft;
    Vec3f nearRight;
    Vec3f farLeft;
    Vec3f farRight;

    Matrix_MultVec3f(&sZeroVec, &this->laserBase);
    nearRight.x = this->laserBase.x + Math_SinS(this->actor.world.rot.y + 0x4000) * width;
    nearRight.y = this->laserBase.y;
    nearRight.z = this->laserBase.z + Math_CosS(this->actor.world.rot.y + 0x4000) * width;
    nearLeft.x = this->laserBase.x + Math_SinS(this->actor.world.rot.y + 0xC000) * width;
    nearLeft.y = this->laserBase.y;
    nearLeft.z = this->laserBase.z + Math_CosS(this->actor.world.rot.y + 0xC000) * width;
    f32 targetScale = this->laserScale.z;
    f32 targetHeight = nearRight.y;

    while (targetHeight + 20.0f > this->actor.floorHeight && targetScale < 10.0f) {
        Eyegore_RotateLaser(this, targetScale, &laserVec);
        targetHeight = nearRight.y + laserVec.y;
        targetScale += 0.01f;
    }
    this->laserScaleTarget.z = targetScale;
    Eyegore_RotateLaser(this, this->laserScale.z, &laserVec);
    Math_Vec3f_Sum(&nearRight, &laserVec, &farRight);
    Math_Vec3f_Sum(&nearLeft, &laserVec, &farLeft);
    quad[0] = farLeft;
    quad[1] = farRight;
    quad[2] = nearLeft;
    quad[3] = nearRight;
    Collider_SetQuadVertices(&this->laserCollider, &farLeft, &farRight, &nearLeft, &nearRight);
    this->laserState = EYEGORE_LASER_ON;
}

static bool IsEyegoreBodyPart(s32 limbIndex) {
    switch (limbIndex) {
        case EYEGORE_LIMB_HEAD:
        case EYEGORE_LIMB_LEFT_SHOULDER:
        case EYEGORE_LIMB_LEFT_ARM:
        case EYEGORE_LIMB_LEFT_HAND:
        case EYEGORE_LIMB_RIGHT_SHOULDER:
        case EYEGORE_LIMB_RIGHT_ARM:
        case EYEGORE_LIMB_RIGHT_HAND:
        case EYEGORE_LIMB_UPPER_EYELID:
        case EYEGORE_LIMB_LOWER_EYELID:
        case EYEGORE_LIMB_HIPS:
        case EYEGORE_LIMB_LEFT_SHIN:
        case EYEGORE_LIMB_RIGHT_SHIN:
            return true;
        default:
            return false;
    }
}

static void Eyegore_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnEyegore* this = (EnEyegore*)thisx;

    if (limbIndex == EYEGORE_LIMB_LEFT_HAND) {
        Matrix_MultVec3f(&sZeroVec, &this->leftHandPos);
    } else if (limbIndex == EYEGORE_LIMB_RIGHT_HAND) {
        Matrix_MultVec3f(&sZeroVec, &this->rightHandPos);
    } else if (limbIndex == EYEGORE_LIMB_LEFT_FOOT) {
        Matrix_MultVec3f(&sFootOffset, &this->leftFootPos);
    } else if (limbIndex == EYEGORE_LIMB_RIGHT_FOOT) {
        Matrix_MultVec3f(&sFootOffset, &this->rightFootPos);
    } else if (limbIndex == EYEGORE_LIMB_LASER_ATTACH) {
        Matrix_MultVec3f(&sZeroVec, &this->eyePos);
        if (this->laserState >= EYEGORE_LASER_FIRE) {
            Eyegore_AimLaser(this);
        } else if (this->laserState == EYEGORE_LASER_START) {
            this->isChargingLaser = true;
            this->laserState = EYEGORE_LASER_CHARGING;
            this->laserScaleTarget.x = 0.03f;
            this->laserScaleTarget.y = 0.03f;
        }
    }
    if (IsEyegoreBodyPart(limbIndex)) {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[this->bodyPartIndex]);
        this->bodyPartIndex = (this->bodyPartIndex + 1) % EYEGORE_BODYPART_MAX;
    }
    Collider_UpdateSpheres(limbIndex, &this->bodyCollider);
    Collider_UpdateSpheres(limbIndex, &this->eyeCollider);
}

static void Eyegore_DrawLaser(EnEyegore* this, PlayState* play) {
    this->laserRot.y = this->actor.world.rot.y;
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)Gfx_TexScroll(play->state.gfxCtx, 0, this->texScroll, 0, 0));
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    Matrix_Translate(this->laserBase.x, this->laserBase.y, this->laserBase.z, MTXMODE_NEW);
    Matrix_RotateZYX(this->laserRot.x, this->laserRot.y, 0, MTXMODE_APPLY);
    Matrix_Scale(this->laserScale.x, this->laserScale.y, this->laserScale.z, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sLaserDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Eyegore_DrawOrb(PlayState* play, f32 scale, u8 alpha, s16 roll) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_RotateZ(BINANG_TO_RAD(roll), MTXMODE_APPLY);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, alpha);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sLightOrbModelDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Eyegore_DrawEyeGlow(EnEyegore* this, PlayState* play) {
    f32 glowScale = this->isChargingLaser ? this->chargeLightScale : this->laserLightScale;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sLightOrbMaterialDL);
    gDPSetEnvColor(POLY_XLU_DISP++, 155, 255, 255, 128);
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Translate(this->eyePos.x, this->eyePos.y, this->eyePos.z, MTXMODE_NEW);
    Matrix_Scale(glowScale, glowScale, glowScale, MTXMODE_APPLY);
    if (this->isChargingLaser) {
        Eyegore_DrawOrb(play, 1.0f, 255, this->chargeLightRot);
        return;
    }
    Eyegore_DrawOrb(play, 10.0f, 80, 0);
    Eyegore_DrawOrb(play, 3.0f, 200, 0);
}

// MM's light-orb damage effect becomes OoT's flame billboard tinted white.
static void Eyegore_DrawLightAura(EnEyegore* this, PlayState* play) {
    u8 alpha = CLAMP_MAX(this->dmgEffectTimer * 0.05f, 1.0f) * 255.0f;
    MtxF billboard;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_RotateY(BINANG_TO_RAD(BINANG_ROT180(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)))), MTXMODE_NEW);
    Matrix_Scale(0.008f, 0.008f, 0.008f, MTXMODE_APPLY);
    Matrix_Get(&billboard);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                           (play->gameplayFrames * -20) % 512, 0x20, 0x80));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 100, 255);
    for (s32 i = 0; i < EYEGORE_BODYPART_MAX; i++) {
        billboard.xw = this->bodyPartsPos[i].x;
        billboard.yw = this->bodyPartsPos[i].y;
        billboard.zw = this->bodyPartsPos[i].z;
        Matrix_Put(&billboard);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static const char* Eyegore_GetEffectDL(EyegoreEffect* effect) {
    switch (effect->type) {
        case EYEGORE_EFFECT_IMPACT:
            return sImpactDL;
        case EYEGORE_EFFECT_PIECE_LARGE:
            return sLargePieceDL;
        case EYEGORE_EFFECT_PIECE_SMALL:
            return sSmallPieceDL;
        default:
            return sSolidDebrisDL;
    }
}

static void Eyegore_DrawEffects(EnEyegore* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    for (s32 i = 0; i < EYEGORE_EFFECT_COUNT; i++) {
        EyegoreEffect* effect = &this->effects[i];

        if (!effect->isActive) {
            continue;
        }
        bool isImpact = effect->type == EYEGORE_EFFECT_IMPACT;

        Matrix_Translate(effect->pos.x, effect->pos.y, effect->pos.z, MTXMODE_NEW);
        Matrix_RotateX(BINANG_TO_RAD(effect->rot.x), MTXMODE_APPLY);
        Matrix_RotateY(BINANG_TO_RAD(effect->rot.y), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(effect->rot.z), MTXMODE_APPLY);
        Matrix_Scale(effect->scale, effect->scale, effect->scale, MTXMODE_APPLY);
        if (isImpact) {
            Matrix_Translate(0.0f, 50.0f, 0.0f, MTXMODE_APPLY);
        }
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, isImpact ? 155 : 255, isImpact ? 155 : 255, isImpact ? 155 : 255,
                        255);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, effect->type == EYEGORE_EFFECT_DEBRIS ? 255 : effect->alpha);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)Eyegore_GetEffectDL(effect));
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnEyegore_Draw(Actor* thisx, PlayState* play) {
    EnEyegore* this = (EnEyegore*)thisx;

    if (this->action != EYEGORE_ACTION_DEAD) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                              Eyegore_OverrideLimbDraw, Eyegore_PostLimbDraw, this);
    }
    if (this->dmgEffectTimer != 0) {
        Eyegore_DrawLightAura(this, play);
    }
    if (this->laserState >= EYEGORE_LASER_FIRE) {
        Eyegore_DrawLaser(this, play);
    }
    bool isGlowing = (this->action == EYEGORE_ACTION_LASER && this->laserState >= EYEGORE_LASER_FIRE) ||
                     this->isChargingLaser;

    if (isGlowing) {
        Eyegore_DrawEyeGlow(this, play);
    }
    Eyegore_DrawEffects(this, play);
}

static void Stone_AddChip(EnEyegoreStone* this, Vec3f* velocity, Vec3f* accel, f32 scale, s16 lifetime) {
    for (s32 i = 0; i < EYEGORE_STONE_EFFECT_COUNT; i++) {
        EyegoreStoneChip* chip = &this->chips[i];

        if (chip->isActive) {
            continue;
        }
        chip->isActive = true;
        chip->pos = this->actor.world.pos;
        chip->velocity = *velocity;
        chip->accel = *accel;
        chip->scale = scale;
        chip->timer = lifetime;
        chip->rot.x = Rand_CenteredFloat(0x7530);
        chip->rot.y = Rand_CenteredFloat(0x7530);
        chip->rot.z = Rand_CenteredFloat(0x7530);
        return;
    }
}

static void Stone_ShedChips(EnEyegoreStone* this, s32 count, f32 spread, f32 baseScale, s16 lifetime) {
    for (s32 i = 0; i < count; i++) {
        Vec3f accel = { spread * 0.5f * (Rand_ZeroOne() - 0.5f), -1.0f, spread * 0.5f * (Rand_ZeroOne() - 0.5f) };
        Vec3f velocity = { spread * (Rand_ZeroOne() - 0.5f), 8.0f + Rand_ZeroOne() * 10.0f,
                           spread * (Rand_ZeroOne() - 0.5f) };

        Stone_AddChip(this, &velocity, &accel, baseScale + baseScale * Rand_ZeroOne(), lifetime);
    }
}

static void EnEyegoreStone_Init(Actor* thisx, PlayState* play) {
    EnEyegoreStone* this = (EnEyegoreStone*)thisx;
    bool isLarge = this->actor.params == EYEGORE_STONE_LARGE;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 15.0f);
    this->rotVel.x = Rand_CenteredFloat(1.0f) * 20.0f;
    this->rotVel.y = this->rotVel.x;
    this->rotVel.z = this->rotVel.x;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sStoneCylinderInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.world.rot.y += (s16)Rand_CenteredFloat(0x2710);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    this->actor.speedXZ = isLarge ? Rand_ZeroFloat(5.0f) + 2.0f : Rand_ZeroFloat(3.0f) + 1.0f;
    this->scale = isLarge ? Rand_ZeroOne() * 0.005f + 0.005f : Rand_ZeroOne() * 0.003f + 0.003f;
    this->actor.velocity.y = isLarge ? Rand_ZeroFloat(10.0f) + 15.0f : Rand_ZeroFloat(5.0f) + 7.0f;
    this->actor.gravity = isLarge ? -2.0f : -1.0f;
    this->collider.dim.radius = this->scale * 3000.0f;
    this->collider.dim.height = this->scale * 3000.0f;
    this->collider.dim.yShift = this->scale * -1300.0f;
    Stone_ShedChips(this, 7, 2.0f, 0.001f, 70);
    this->timer = 5;
    Actor_SetScale(&this->actor, 1.0f);
}

static void EnEyegoreStone_Destroy(Actor* thisx, PlayState* play) {
    EnEyegoreStone* this = (EnEyegoreStone*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

// The stone shatters on landing or against a shield and lingers 50 frames for its chips.
static void Stone_Land(EnEyegoreStone* this, PlayState* play) {
    Vec3f dustPos = { this->actor.world.pos.x, this->actor.floorHeight, this->actor.world.pos.z };

    this->collider.base.atFlags &= ~AT_BOUNCED;
    if (this->actor.params == EYEGORE_STONE_LARGE) {
        Stone_ShedChips(this, 2, 3.0f, 0.003f, 20);
    }
    Actor_SpawnFloorDustRing(play, &this->actor, &dustPos, 0.0f, 10, 6.0f, 50, 30, true);
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = 0.0f;
    this->actor.speedXZ *= 0.3f;
    this->actor.shape.shadowScale = 0.0f;
    this->isInactive = true;
    this->timer = 50;
    PlaySfx(&this->actor, MM_SE_EV_WALL_BROKEN, NA_SE_EV_WALL_BROKEN);
}

static void Stone_UpdateChips(EnEyegoreStone* this, PlayState* play) {
    for (s32 i = 0; i < EYEGORE_STONE_EFFECT_COUNT; i++) {
        EyegoreStoneChip* chip = &this->chips[i];

        if (!chip->isActive) {
            continue;
        }
        chip->rot.x += 0x100;
        chip->rot.z += 0x130;
        Math_Vec3f_Sum(&chip->pos, &chip->velocity, &chip->pos);
        chip->velocity.y += chip->accel.y;
        if (chip->pos.y < this->actor.floorHeight && chip->velocity.y < 0.0f) {
            chip->timer = 0;
            Actor_SpawnFloorDustRing(play, &this->actor, &chip->pos, 0.0f, 5, 3.0f, 30, 15, true);
        }
        if (chip->timer != 0) {
            chip->timer--;
        } else {
            chip->isActive = false;
        }
    }
}

static void EnEyegoreStone_Update(Actor* thisx, PlayState* play) {
    EnEyegoreStone* this = (EnEyegoreStone*)thisx;

    DECR(this->timer);
    if (this->isInactive) {
        if (this->timer == 0) {
            Actor_Kill(&this->actor);
            return;
        }
    } else if (this->actor.floorHeight == BGCHECK_Y_MIN ||
               GET_PLAYER(play)->actor.world.pos.y - 200.0f > this->actor.world.pos.y) {
        Actor_Kill(&this->actor);
        return;
    } else if (((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && this->actor.velocity.y < 0.0f) ||
               (this->collider.base.atFlags & AT_BOUNCED)) {
        Stone_Land(this, play);
    } else {
        this->actor.shape.shadowScale = 4500.0f * this->scale;
        Actor_SetScale(&this->actor, this->scale);
    }
    this->rot.x -= this->rotVel.x;
    this->rot.y -= this->rotVel.y;
    this->rot.z -= this->rotVel.z;
    Actor_MoveXZGravity(&this->actor);
    if (this->timer == 0 && !this->isInactive) {
        Actor_UpdateBgCheckInfo(play, &this->actor, 50.0f, 50.0f, 100.0f, BGCHECK_UPDATE_FLOOR_WATER);
    }
    if (!this->isInactive) {
        Collider_UpdateCylinder(&this->actor, &this->collider);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    Stone_UpdateChips(this, play);
}

static void EnEyegoreStone_Draw(Actor* thisx, PlayState* play) {
    EnEyegoreStone* this = (EnEyegoreStone*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    if (!this->isInactive) {
        Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
        Matrix_RotateX(DEG_TO_RAD(this->rot.x), MTXMODE_APPLY);
        Matrix_RotateY(DEG_TO_RAD(this->rot.y), MTXMODE_APPLY);
        Matrix_RotateZ(DEG_TO_RAD(this->rot.z), MTXMODE_APPLY);
        Matrix_Scale(this->scale, this->scale, this->scale, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sStoneDL);
    }
    for (s32 i = 0; i < EYEGORE_STONE_EFFECT_COUNT; i++) {
        EyegoreStoneChip* chip = &this->chips[i];

        if (!chip->isActive) {
            continue;
        }
        Matrix_Translate(chip->pos.x, chip->pos.y, chip->pos.z, MTXMODE_NEW);
        Matrix_RotateX(BINANG_TO_RAD(chip->rot.x), MTXMODE_APPLY);
        Matrix_RotateY(BINANG_TO_RAD(chip->rot.y), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(chip->rot.z), MTXMODE_APPLY);
        Matrix_Scale(chip->scale, chip->scale, chip->scale, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRockChipDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's CanKillEnemy(ACTOR_EN_EGOL): any projectile into the exposed eye.
static bool Eyegore_CanKillInLogic(void* userData) {
    return sApi->LogicCanUseItem(RG_FAIRY_BOW) || sApi->LogicCanUseItem(RG_FAIRY_SLINGSHOT) ||
           sApi->LogicCanUseItem(RG_HOOKSHOT) || sApi->LogicCanUseItem(RG_BOOMERANG);
}

static void RegisterStone(void) {
    SOHActorDefinition stone = { 0 };

    stone.structSize = sizeof(stone);
    stone.key = EYEGORE_STONE_KEY;
    stone.description = "Eyegore Stone";
    stone.category = ACTORCAT_PROP;
    stone.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    stone.objectId = OBJECT_GAMEPLAY_KEEP;
    stone.instanceSize = sizeof(EnEyegoreStone);
    stone.init = EnEyegoreStone_Init;
    stone.destroy = EnEyegoreStone_Destroy;
    stone.update = EnEyegoreStone_Update;
    stone.draw = EnEyegoreStone_Draw;
    sStoneActorId = sApi->RegisterActor(&stone);
}

static void RegisterEyegore(void) {
    SOHActorDefinition eyegore = { 0 };

    eyegore.structSize = sizeof(eyegore);
    eyegore.key = EYEGORE_KEY;
    eyegore.description = "Eyegore";
    eyegore.category = ACTORCAT_ENEMY;
    eyegore.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                         ACTOR_FLAG_DRAW_CULLING_DISABLED;
    eyegore.objectId = OBJECT_GAMEPLAY_KEEP;
    eyegore.instanceSize = sizeof(EnEyegore);
    eyegore.init = EnEyegore_Init;
    eyegore.destroy = EnEyegore_Destroy;
    eyegore.update = EnEyegore_Update;
    eyegore.draw = EnEyegore_Draw;
    eyegore.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_MINIBOSS | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    eyegore.naviHint = "Eyegore&%cIts eye opens after a slam. Shoot it then, or stun it with a %ylight arrow%c!%w";
    eyegore.soulKey = EYEGORE_KEY;
    eyegore.enemyCategory = SOH_ENEMY_CATEGORY_MINIBOSS;
    eyegore.soulModel.skeleton = sEyegoreSkel;
    eyegore.soulModel.idleAnimation = sStandAnim;
    eyegore.soulModel.isFlex = 1;
    eyegore.soulModel.limbCount = EYEGORE_LIMB_MAX;
    eyegore.soulModel.scale = 0.006f;
    eyegore.logicKill = Eyegore_CanKillInLogic;
    if (sApi->RegisterActor(&eyegore) < 0) {
        sApi->Log("Could not register " EYEGORE_KEY);
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
    RegisterStone();
    if (sStoneActorId < 0) {
        sApi->Log("Could not register " EYEGORE_STONE_KEY);
        return;
    }
    RegisterEyegore();
}
