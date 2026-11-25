// Majora's Mask's Wizrobe (En_Wiz + En_Wiz_Fire + En_Wiz_Brock) as an Unbound custom enemy, drawn from mm.o2r.
// It plays its arena fight without cutscenes and raises its own warp platforms around its spawn point.
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

#define WIZROBE_KEY "mm.wizrobe"
#define WIZROBE_SOUL_ITEM_KEY "soul.mm.wizrobe"
#define WIZROBE_MAGIC_KEY "mm.wizrobe.magic"
#define WIZROBE_PLATFORM_KEY "mm.wizrobe.platform"
#define GORON_MASK_KEY "nei.goron_form_mask"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_WIZ "__OTR__objects/object_wiz/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_CEILING_FLOOR_WATER 0x1F
#define BGCHECK_UPDATE_WALL_FLOOR_WATER 0x1D

#define MM_SE_EN_WIZ_UNARI 0x38A4
#define MM_SE_EN_WIZ_RUN 0x38A5
#define MM_SE_EN_WIZ_VOICE 0x38A6
#define MM_SE_EN_WIZ_LAUGH 0x38A7
#define MM_SE_EN_WIZ_ATTACK 0x38A8
#define MM_SE_EN_WIZ_DAMAGE 0x38A9
#define MM_SE_EN_WIZ_DEAD 0x38AA
#define MM_SE_EN_WIZ_EXP 0x38AB
#define MM_SE_EN_WIZ_LAUGH2 0x38B0
#define MM_SE_EN_WIZ_DISAPPEAR 0x3877

#define WIZROBE_PLATFORM_MAX 5
#define WIZROBE_PLATFORM_MIN 3
#define WIZROBE_PLATFORM_MAX_STEP 120.0f
#define WIZROBE_WAKE_DISTANCE 800.0f
#define WIZROBE_FULL_SCALE 0.015f
#define WIZROBE_FIRE_HEALTH 8
#define WIZROBE_ICE_HEALTH 6
#define WIZROBE_NO_PLATFORM (-1)
#define WIZROBE_TYPE_RANDOM 0xFF
#define WIZROBE_TRAIL_COUNT 10
#define WIZROBE_ICE_SMOKE_COUNT 200
#define WIZROBE_INVINCIBILITY_MAX 127

typedef enum {
    WIZROBE_LIMB_NONE,
    WIZROBE_LIMB_PELVIS,
    WIZROBE_LIMB_TORSO,
    WIZROBE_LIMB_LEFT_UPPER_ARM,
    WIZROBE_LIMB_LEFT_FOREARM,
    WIZROBE_LIMB_LEFT_HAND,
    WIZROBE_LIMB_RIGHT_UPPER_ARM,
    WIZROBE_LIMB_RIGHT_FOREARM,
    WIZROBE_LIMB_RIGHT_HAND,
    WIZROBE_LIMB_STAFF,
    WIZROBE_LIMB_NECK,
    WIZROBE_LIMB_HEAD,
    WIZROBE_LIMB_JAW,
    WIZROBE_LIMB_LOINCLOTH,
    WIZROBE_LIMB_LEFT_THIGH,
    WIZROBE_LIMB_LEFT_SHIN,
    WIZROBE_LIMB_LEFT_FOOT,
    WIZROBE_LIMB_RIGHT_THIGH,
    WIZROBE_LIMB_RIGHT_SHIN,
    WIZROBE_LIMB_RIGHT_FOOT,
    WIZROBE_LIMB_MAX
} WizrobeLimb;

#define WIZROBE_BODYPART_MAX 12

typedef enum {
    WIZROBE_TYPE_FIRE,
    WIZROBE_TYPE_ICE,
    WIZROBE_TYPE_FIRE_NO_BGM
} WizrobeType;

typedef enum {
    WIZROBE_ACTION_WAIT,
    WIZROBE_ACTION_APPEAR,
    WIZROBE_ACTION_DISAPPEAR = 3,
    WIZROBE_ACTION_DAMAGED,
    WIZROBE_ACTION_DEAD,
    WIZROBE_ACTION_BURST_INTO_FLAMES,
    WIZROBE_ACTION_RUN_IN_CIRCLES,
    WIZROBE_ACTION_ATTACK,
    WIZROBE_ACTION_DANCE
} WizrobeAction;

typedef enum {
    WIZROBE_FIGHT_FIRST_PHASE,
    WIZROBE_FIGHT_SECOND_PHASE_START,
    WIZROBE_FIGHT_GHOSTS_COPY_WIZROBE,
    WIZROBE_FIGHT_GHOSTS_RUN_AROUND
} WizrobeFightState;

typedef enum {
    WIZROBE_ANIM_IDLE,
    WIZROBE_ANIM_RUN,
    WIZROBE_ANIM_DANCE,
    WIZROBE_ANIM_WIND_UP,
    WIZROBE_ANIM_ATTACK,
    WIZROBE_ANIM_DAMAGE,
    WIZROBE_ANIM_MAX
} WizrobeAnimation;

typedef enum {
    WIZROBE_DMGEFF_IMMUNE,
    WIZROBE_DMGEFF_NUT,
    WIZROBE_DMGEFF_FIRE,
    WIZROBE_DMGEFF_FREEZE,
    WIZROBE_DMGEFF_LIGHT,
    WIZROBE_DMGEFF_NONE = 0xF
} WizrobeDamageEffect;

typedef enum {
    WIZROBE_AURA_NONE,
    WIZROBE_AURA_FIRE,
    WIZROBE_AURA_FROZEN,
    WIZROBE_AURA_LIGHT
} WizrobeAura;

typedef enum {
    MAGIC_TYPE_PROJECTILE,
    MAGIC_TYPE_ARCING_PROJECTILE,
    MAGIC_TYPE_SMALL_FLAME,
    MAGIC_TYPE_REFLECTED_PROJECTILE,
    MAGIC_TYPE_ICE_PROJECTILE
} WizrobeMagicType;

typedef enum {
    MAGIC_ACTION_MOVE,
    MAGIC_ACTION_SMALL_FLAME,
    MAGIC_ACTION_POOL,
    MAGIC_ACTION_FADE_TRAIL
} WizrobeMagicAction;

struct EnWizrobe;
struct EnWizrobeMagic;

typedef void (*EnWizrobeActionFunc)(struct EnWizrobe*, PlayState*);
typedef void (*EnWizrobeMagicActionFunc)(struct EnWizrobeMagic*, PlayState*);

typedef struct EnWizrobe {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[WIZROBE_LIMB_MAX];
    Vec3s morphTable[WIZROBE_LIMB_MAX];
    SkelAnime ghostSkelAnime;
    Vec3s ghostBaseJointTable[WIZROBE_LIMB_MAX];
    Vec3s ghostMorphTable[WIZROBE_LIMB_MAX];
    EnWizrobeActionFunc actionFunc;
    s16 action;
    s16 timer;
    s16 fightState;
    s16 staffFlameScroll;
    s16 angularVelocity;
    s16 alpha;
    s16 platformLightAlpha;
    s16 targetPlatformLightAlpha;
    s16 animLoopCounter;
    s16 type;
    s16 curPlatformIndex;
    s16 auraTimer;
    s16 aura;
    s16 bodyPartIndex;
    bool hasActiveProjectile;
    bool shouldStartTimer;
    bool isDead;
    bool isFightStarted;
    f32 animEndFrame;
    f32 scale;
    Vec3f staffFlamePos;
    Vec3f staffFlameScale;
    Vec3f staffTargetFlameScale;
    Vec3f platformLightPos;
    ColliderCylinder collider;
    s32 platformCount;
    s32 ghostCount;
    Vec3f platformPos[WIZROBE_PLATFORM_MAX];
    Vec3f bodyPartsPos[WIZROBE_BODYPART_MAX];
    s16 ghostAlpha[WIZROBE_PLATFORM_MAX];
    s16 ghostNextPlatformIndex[WIZROBE_PLATFORM_MAX];
    Vec3f ghostPos[WIZROBE_PLATFORM_MAX];
    s16 ghostYaw[WIZROBE_PLATFORM_MAX];
    Vec3s ghostJointTables[WIZROBE_PLATFORM_MAX][WIZROBE_LIMB_MAX];
} EnWizrobe;

typedef struct {
    bool isEnabled;
    u8 smokeScroll;
    s16 alpha;
    bool isFadingOut;
    f32 scale;
    Vec3f pos;
    Vec3f velocity;
    Vec3f accel;
} WizrobeIceSmoke;

typedef struct EnWizrobeMagic {
    Actor actor;
    EnWizrobeMagicActionFunc actionFunc;
    bool hitByIceArrow;
    bool hasSpawnedSmallFlame;
    bool shouldPoolFadeOut;
    bool isIceType;
    f32 scale;
    f32 targetScale;
    Vec3f scaleMod;
    s16 action;
    s16 type;
    s16 timer;
    s16 poolTimer;
    s16 trailFadeTimer;
    s16 steamSpawnTimer;
    s16 lowestUsedIndex;
    s16 smallFlameScroll;
    s16 wallCheckTimer;
    Vec3f trailPos[WIZROBE_TRAIL_COUNT];
    f32 poolScale;
    f32 bigFlameScale;
    f32 fireSmokeScale;
    f32 alpha;
    f32 blendScale;
    f32 blendScaleFrac;
    ColliderCylinder collider;
    WizrobeIceSmoke iceSmoke[WIZROBE_ICE_SMOKE_COUNT];
} EnWizrobeMagic;

typedef struct EnWizrobePlatform {
    DynaPolyActor dyna;
    s16 element;
    s16 vanishTimer;
    bool isVanishing;
    f32 alpha;
} EnWizrobePlatform;

typedef struct {
    s8 xStep;
    s8 yStep;
    u8 width;
    u8 height;
} WizrobeTexScroll;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sWizrobeSkel[] = OBJ_WIZ "gWizrobeSkel";
static const ALIGN_ASSET(2) char sWizrobeIdleAnim[] = OBJ_WIZ "gWizrobeIdleAnim";
static const ALIGN_ASSET(2) char sWizrobeRunAnim[] = OBJ_WIZ "gWizrobeRunAnim";
static const ALIGN_ASSET(2) char sWizrobeDanceAnim[] = OBJ_WIZ "gWizrobeDanceAnim";
static const ALIGN_ASSET(2) char sWizrobeWindUpAnim[] = OBJ_WIZ "gWizrobeWindUpAnim";
static const ALIGN_ASSET(2) char sWizrobeAttackAnim[] = OBJ_WIZ "gWizrobeAttackAnim";
static const ALIGN_ASSET(2) char sWizrobeDamageAnim[] = OBJ_WIZ "gWizrobeDamageAnim";
static const ALIGN_ASSET(2) char sPlatformLightDL[] = OBJ_WIZ "gWizrobePlatformLightDL";
static const ALIGN_ASSET(2) char sPlatformDL[] = OBJ_WIZ "gWizrobePlatformDL";
static const ALIGN_ASSET(2) char sPlatformCenterDL[] = OBJ_WIZ "gWizrobePlatformCenterDL";
static const ALIGN_ASSET(2) char sPlatformCol[] = OBJ_WIZ "gWizrobePlatformCol";
static const ALIGN_ASSET(2) char sMagicProjectileDL[] = OBJ_WIZ "gWizrobeMagicProjectileDL";
static const ALIGN_ASSET(2) char sIcePoolDL[] = OBJ_WIZ "gWizrobeIcePoolDL";
static const ALIGN_ASSET(2) char sIcePoolShineDL[] = OBJ_WIZ "gWizrobeIcePoolShineDL";
static const ALIGN_ASSET(2) char sFirePoolDL[] = OBJ_WIZ "gWizrobeFirePoolDL";
static const ALIGN_ASSET(2) char sFireSmokeDL[] = OBJ_WIZ "gWizrobeFireSmokeDL";
static const ALIGN_ASSET(2) char sBigFlameDL[] = OBJ_WIZ "gWizrobeBigFlameDL";
static const ALIGN_ASSET(2) char sSmallFlameDL[] = OBJ_WIZ "gWizrobeSmallFlameDL";
static const ALIGN_ASSET(2) char sIceSmokeMaterialDL[] = OBJ_WIZ "gWizrobeIceSmokeMaterialDL";
static const ALIGN_ASSET(2) char sIceSmokeModelDL[] = OBJ_WIZ "gWizrobeIceSmokeModelDL";

static const char* const sAnimations[WIZROBE_ANIM_MAX] = {
    sWizrobeIdleAnim, sWizrobeRunAnim, sWizrobeDanceAnim, sWizrobeWindUpAnim, sWizrobeAttackAnim, sWizrobeDamageAnim,
};

static const u8 sAnimationModes[WIZROBE_ANIM_MAX] = {
    ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE,
};

// gWizrobePlatform*TexAnim are empty in Ship's mm.o2r; these are their scroll layers from 2Ship's archive.
static const WizrobeTexScroll sPlatformLightRaysScroll[2] = { { 0, 0, 16, 32 }, { 2, 2, 32, 32 } };
static const WizrobeTexScroll sPlatformLightBaseScroll[2] = { { 1, 0, 32, 32 }, { -1, 4, 32, 32 } };
static const WizrobeTexScroll sPlatformCenterScroll[2] = { { 0, 0, 16, 32 }, { 2, 3, 32, 32 } };

static ColliderCylinderInit sBodyCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0xFFCFFFFF, 0x00, 0x04 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
    { 35, 130, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sMagicCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_FIRE, 0x10 },
      { DMG_ARROW_ICE | DMG_SWORD | DMG_DEKU_STICK, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_ON,
      OCELEM_NONE },
    { 0, 0, 0, { 0, 0, 0 } },
};

static DamageTable sFireWizrobeDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, WIZROBE_DMGEFF_NUT),
    /* Deku stick    */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, WIZROBE_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, WIZROBE_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Kokiri sword  */ DMG_ENTRY(1, WIZROBE_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, WIZROBE_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(2, WIZROBE_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, WIZROBE_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(4, WIZROBE_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(2, WIZROBE_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(4, WIZROBE_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(4, WIZROBE_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, WIZROBE_DMGEFF_IMMUNE),
    /* Mirror Ray    */ DMG_ENTRY(0, WIZROBE_DMGEFF_IMMUNE),
    /* Kokiri spin   */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(8, WIZROBE_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(4, WIZROBE_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, WIZROBE_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, WIZROBE_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, WIZROBE_DMGEFF_IMMUNE),
    /* Unblockable   */ DMG_ENTRY(0, WIZROBE_DMGEFF_IMMUNE),
    /* Hammer jump   */ DMG_ENTRY(6, WIZROBE_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, WIZROBE_DMGEFF_IMMUNE),
};

static DamageTable sIceWizrobeDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, WIZROBE_DMGEFF_NUT),
    /* Deku stick    */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, WIZROBE_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, WIZROBE_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(4, WIZROBE_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Kokiri sword  */ DMG_ENTRY(1, WIZROBE_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, WIZROBE_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(4, WIZROBE_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(2, WIZROBE_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(4, WIZROBE_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(4, WIZROBE_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(2, WIZROBE_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(4, WIZROBE_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, WIZROBE_DMGEFF_IMMUNE),
    /* Mirror Ray    */ DMG_ENTRY(0, WIZROBE_DMGEFF_IMMUNE),
    /* Kokiri spin   */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(8, WIZROBE_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(4, WIZROBE_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, WIZROBE_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, WIZROBE_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, WIZROBE_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, WIZROBE_DMGEFF_IMMUNE),
    /* Unblockable   */ DMG_ENTRY(0, WIZROBE_DMGEFF_IMMUNE),
    /* Hammer jump   */ DMG_ENTRY(6, WIZROBE_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, WIZROBE_DMGEFF_IMMUNE),
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Color_RGBA8 sDustPrimColor = { 250, 250, 250, 255 };
static Color_RGBA8 sDustEnvColor = { 180, 180, 180, 255 };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;
static int16_t sWizrobeActorId = -1;
static int16_t sMagicActorId = -1;
static int16_t sPlatformActorId = -1;
static bool sPoolHitByIceArrow;

static void Wizrobe_SetupAppear(EnWizrobe* this, PlayState* play);
static void Wizrobe_Appear(EnWizrobe* this, PlayState* play);
static void Wizrobe_Dance(EnWizrobe* this, PlayState* play);
static void Wizrobe_WindUp(EnWizrobe* this, PlayState* play);
static void Wizrobe_Attack(EnWizrobe* this, PlayState* play);
static void Wizrobe_Disappear(EnWizrobe* this, PlayState* play);
static void Wizrobe_Damaged(EnWizrobe* this, PlayState* play);
static void Wizrobe_Dead(EnWizrobe* this, PlayState* play);
static void Wizrobe_SetupDisappear(EnWizrobe* this);
static void Magic_MoveProjectile(EnWizrobeMagic* this, PlayState* play);
static void Magic_SmallFlame(EnWizrobeMagic* this, PlayState* play);
static void Magic_Pool(EnWizrobeMagic* this, PlayState* play);
static void Magic_FadeTrail(EnWizrobeMagic* this, PlayState* play);
static void Magic_DrawSmallFlame(Actor* thisx, PlayState* play);

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

static bool Wizrobe_IsFireType(EnWizrobe* this) {
    return this->type != WIZROBE_TYPE_ICE;
}

static Gfx* MakeTwoLayerScroll(PlayState* play, const WizrobeTexScroll* layers) {
    u32 frames = play->gameplayFrames;

    return Gfx_TwoTexScroll(play->state.gfxCtx, 0, layers[0].xStep * frames, -(layers[0].yStep * (s32)frames),
                            layers[0].width, layers[0].height, 1, layers[1].xStep * frames,
                            -(layers[1].yStep * (s32)frames), layers[1].width, layers[1].height);
}

static Gfx* MakePlatformCenterColor(PlayState* play) {
    Gfx* color = Graph_Alloc(play->state.gfxCtx, 2 * sizeof(Gfx));

    gDPSetPrimColor(&color[0], 0, 0x78, 255, 255, 255, 255);
    gSPEndDisplayList(&color[1]);
    return color;
}

static void Wizrobe_StartMinibossMusic(EnWizrobe* this) {
    if (this->isFightStarted || this->type == WIZROBE_TYPE_FIRE_NO_BGM) {
        return;
    }
    this->isFightStarted = true;
    func_800F5ACC(NA_BGM_MINI_BOSS);
}

static void Wizrobe_StopMinibossMusic(EnWizrobe* this) {
    if (!this->isFightStarted) {
        return;
    }
    this->isFightStarted = false;
    func_800F5B58();
}

static bool Wizrobe_FindPlatformSpot(PlayState* play, Vec3f* origin, s16 yaw, f32 radius, Vec3f* spot) {
    Vec3f from = { origin->x, origin->y + 40.0f, origin->z };
    Vec3f to = { origin->x + Math_SinS(yaw) * radius, from.y, origin->z + Math_CosS(yaw) * radius };
    Vec3f hitPos;
    CollisionPoly* poly;
    s32 bgId;

    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hitPos, &poly, true, false, false, true, &bgId)) {
        return false;
    }
    f32 floorY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &to);

    if (floorY == BGCHECK_Y_MIN || fabsf(floorY - origin->y) > WIZROBE_PLATFORM_MAX_STEP) {
        return false;
    }
    *spot = to;
    spot->y = floorY;
    return true;
}

static void Wizrobe_AddPlatform(EnWizrobe* this, PlayState* play, Vec3f* pos) {
    s16 element = Wizrobe_IsFireType(this) ? WIZROBE_TYPE_FIRE : WIZROBE_TYPE_ICE;

    this->platformPos[this->platformCount] = *pos;
    this->platformCount++;
    Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, sPlatformActorId, pos->x, pos->y, pos->z, 0, 0, 0,
                       element);
}

// MM places these in the arena; here one sits under the spawn point and the rest ring it where the floor allows.
static void Wizrobe_SpawnPlatforms(EnWizrobe* this, PlayState* play) {
    static const f32 sRingRadii[] = { 300.0f, 180.0f, 100.0f };
    Vec3f origin = this->actor.home.pos;
    CollisionPoly* poly;
    s32 bgId;
    Vec3f probe = { origin.x, origin.y + 50.0f, origin.z };
    f32 floorY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &probe);

    if (floorY != BGCHECK_Y_MIN) {
        origin.y = floorY;
    }
    Wizrobe_AddPlatform(this, play, &origin);
    for (s32 i = 0; i < WIZROBE_PLATFORM_MAX - 1; i++) {
        s16 yaw = this->actor.home.rot.y + i * 0x4000;
        Vec3f spot;

        for (s32 r = 0; r < ARRAY_COUNT(sRingRadii); r++) {
            if (Wizrobe_FindPlatformSpot(play, &origin, yaw, sRingRadii[r], &spot)) {
                Wizrobe_AddPlatform(this, play, &spot);
                break;
            }
        }
    }
    for (s32 i = 0; this->platformCount < WIZROBE_PLATFORM_MIN; i++) {
        Vec3f spot = { origin.x + Math_SinS(i * 0x8000) * 80.0f, origin.y, origin.z + Math_CosS(i * 0x8000) * 80.0f };

        Wizrobe_AddPlatform(this, play, &spot);
    }
}

static void Wizrobe_ReleasePlatforms(EnWizrobe* this, PlayState* play, bool shouldVanish) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].head;

    while (actor != NULL) {
        Actor* next = actor->next;

        if (actor->id == sPlatformActorId && actor->parent == &this->actor) {
            actor->parent = NULL;
            if (shouldVanish) {
                ((EnWizrobePlatform*)actor)->isVanishing = true;
            } else {
                Actor_Kill(actor);
            }
        }
        actor = next;
    }
}

static void Wizrobe_ReleaseMagic(EnWizrobe* this, PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        if (actor->id == sMagicActorId && actor->parent == &this->actor) {
            actor->parent = NULL;
        }
    }
}

static void Wizrobe_ChangeAnim(EnWizrobe* this, s32 animIndex, bool updateGhostAnim) {
    AnimationHeader* animation = (AnimationHeader*)sAnimations[animIndex];

    this->animEndFrame = Animation_GetLastFrame(animation);
    Animation_Change(&this->skelAnime, animation, 1.0f, 0.0f, this->animEndFrame, sAnimationModes[animIndex], -2.0f);
    if (updateGhostAnim) {
        Animation_Change(&this->ghostSkelAnime, animation, 1.0f, 0.0f, this->animEndFrame, sAnimationModes[animIndex],
                         -2.0f);
    }
}

static s16 Wizrobe_PickPlatform(EnWizrobe* this) {
    if (this->platformCount < 2) {
        return 0;
    }
    s16 index = Rand_ZeroFloat(this->platformCount);

    while (index == this->curPlatformIndex || index >= this->platformCount) {
        index = Rand_ZeroFloat(this->platformCount);
    }
    return index;
}

static void Wizrobe_PlaceGhosts(EnWizrobe* this) {
    for (s32 i = this->ghostCount - 1; i >= 0; i--) {
        if (i == this->curPlatformIndex) {
            this->ghostPos[i] = sZeroVec;
            continue;
        }
        this->ghostPos[i] = this->platformPos[i];
        this->ghostYaw[i] = this->actor.world.rot.y;
        this->ghostAlpha[i] = 100;
        this->ghostNextPlatformIndex[i] = i;
        for (s32 j = 0; j < WIZROBE_LIMB_MAX; j++) {
            this->ghostJointTables[i][j] = this->jointTable[j];
        }
    }
}

// Second phase: every platform but the real one shows a phantom.
static void Wizrobe_SelectPlatform(EnWizrobe* this) {
    this->ghostCount = this->platformCount;
    this->curPlatformIndex = Wizrobe_PickPlatform(this);
    if (this->fightState == WIZROBE_FIGHT_SECOND_PHASE_START) {
        this->actor.world.pos = this->platformPos[0];
        s16 ghostAlpha = 128;

        for (s32 i = 0; i < this->ghostCount; i++, ghostAlpha -= 10) {
            this->ghostPos[i] = this->actor.world.pos;
            this->ghostAlpha[i] = ghostAlpha;
        }
        return;
    }
    this->actor.world.pos = this->platformPos[this->curPlatformIndex];
    if (this->fightState != WIZROBE_FIGHT_FIRST_PHASE) {
        Wizrobe_PlaceGhosts(this);
    }
}

static void Wizrobe_MoveGhosts(EnWizrobe* this) {
    bool isAnyGhostRunning = false;

    for (s32 i = 0; i < this->ghostCount; i++) {
        if (this->ghostPos[i].x == 0.0f || this->ghostPos[i].z == 0.0f) {
            continue;
        }
        Vec3f* target = &this->platformPos[this->ghostNextPlatformIndex[i]];

        isAnyGhostRunning = true;
        if (sqrtf(SQ(target->x - this->ghostPos[i].x) + SQ(target->z - this->ghostPos[i].z)) < 30.0f) {
            this->ghostNextPlatformIndex[i]--;
            if (this->ghostNextPlatformIndex[i] < 0) {
                this->ghostNextPlatformIndex[i] = this->ghostCount - 1;
            }
            target = &this->platformPos[this->ghostNextPlatformIndex[i]];
        }
        Math_ApproachF(&this->ghostPos[i].x, target->x, 0.3f, 30.0f);
        Math_ApproachF(&this->ghostPos[i].y, target->y, 0.3f, 30.0f);
        Math_ApproachF(&this->ghostPos[i].z, target->z, 0.3f, 30.0f);
        this->ghostYaw[i] = Math_Vec3f_Yaw(&this->ghostPos[i], target);
    }
    if (isAnyGhostRunning) {
        PlaySfx(&this->actor, MM_SE_EN_WIZ_RUN - SFX_FLAG, NA_SE_EN_PO_FLY - SFX_FLAG);
    }
}

// Stands in for MM's player->unk_D57: an arrow or seed Link has just loosed.
static bool Wizrobe_IsShotJustFired(PlayState* play) {
    Player* player = GET_PLAYER(play);

    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; actor != NULL; actor = actor->next) {
        bool isFresh = actor->id == ACTOR_EN_ARROW && actor->parent == &player->actor && actor->speedXZ > 0.0f &&
                       actor->xyzDistToPlayerSq < SQ(150.0f);

        if (isFresh) {
            return true;
        }
    }
    return false;
}

static bool Wizrobe_IsUnderFire(EnWizrobe* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 facingLink = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
    s16 linkFacing = this->actor.yawTowardsPlayer - (s16)(player->actor.shape.rot.y + 0x8000);

    return ABS(facingLink) < 0x7D0 && ABS(linkFacing) < 0x7D0 && Wizrobe_IsShotJustFired(play);
}

static void Wizrobe_WaitForPlayer(EnWizrobe* this, PlayState* play) {
    if (this->platformCount == 0) {
        Wizrobe_SpawnPlatforms(this, play);
    }
    if (this->actor.xzDistToPlayer < WIZROBE_WAKE_DISTANCE) {
        Wizrobe_StartMinibossMusic(this);
        this->action = WIZROBE_ACTION_APPEAR;
        this->actionFunc = Wizrobe_SetupAppear;
    }
}

static void Wizrobe_SetupAppear(EnWizrobe* this, PlayState* play) {
    this->action = WIZROBE_ACTION_APPEAR;
    if (this->timer != 0) {
        return;
    }
    Player* player = GET_PLAYER(play);

    Wizrobe_SelectPlatform(this);
    this->actor.world.rot.y = Math_Vec3f_Yaw(&this->actor.world.pos, &player->actor.world.pos);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    for (s32 i = 0; i < this->ghostCount; i++) {
        this->ghostYaw[i] = Math_Vec3f_Yaw(&this->ghostPos[i], &player->actor.world.pos);
    }
    Wizrobe_ChangeAnim(this, WIZROBE_ANIM_IDLE, true);
    this->shouldStartTimer = false;
    this->targetPlatformLightAlpha = 255;
    this->platformLightPos = this->actor.world.pos;
    if (this->fightState == WIZROBE_FIGHT_FIRST_PHASE) {
        PlaySfx(&this->actor, MM_SE_EN_WIZ_UNARI, NA_SE_EN_PO_APPEAR);
    } else {
        PlaySfx(&this->actor, MM_SE_EN_WIZ_VOICE - SFX_FLAG, NA_SE_EN_PO_LAUGH);
    }
    this->actionFunc = Wizrobe_Appear;
}

static void Wizrobe_SetupDance(EnWizrobe* this) {
    Wizrobe_ChangeAnim(this, WIZROBE_ANIM_DANCE, false);
    Math_ApproachF(&this->scale, WIZROBE_FULL_SCALE, 0.05f, 0.001f);
    this->angularVelocity = 0;
    this->animLoopCounter = 0;
    this->action = WIZROBE_ACTION_DANCE;
    if (this->fightState >= WIZROBE_FIGHT_GHOSTS_COPY_WIZROBE) {
        AnimationHeader* run = (AnimationHeader*)sWizrobeRunAnim;

        Animation_Change(&this->ghostSkelAnime, run, 1.0f, 0.0f, Animation_GetLastFrame(run), ANIMMODE_LOOP, 0.0f);
        this->fightState = WIZROBE_FIGHT_GHOSTS_RUN_AROUND;
    }
    Math_SmoothStepToS(&this->alpha, 255, 1, 5, 0);
    this->actionFunc = Wizrobe_Dance;
}

// The second phase begins without MM's lap-of-the-room cutscene: the phantoms just start copying it.
static void Wizrobe_StartSecondPhase(EnWizrobe* this) {
    for (s32 i = 0; i < this->ghostCount; i++) {
        this->ghostPos[i] = sZeroVec;
    }
    this->ghostCount = 0;
    this->fightState = WIZROBE_FIGHT_GHOSTS_COPY_WIZROBE;
    this->timer = 0;
    Wizrobe_SetupDisappear(this);
}

static void Wizrobe_FinishAppearing(EnWizrobe* this) {
    this->action = WIZROBE_ACTION_RUN_IN_CIRCLES;
    this->actor.flags &= ~ACTOR_FLAG_LOCK_ON_DISABLED;
    this->staffTargetFlameScale.x = 0.006f;
    this->staffTargetFlameScale.y = 0.006f;
    this->staffTargetFlameScale.z = 0.006f;
    this->targetPlatformLightAlpha = 0;
    if (this->fightState == WIZROBE_FIGHT_SECOND_PHASE_START) {
        Wizrobe_StartSecondPhase(this);
    } else {
        Wizrobe_SetupDance(this);
    }
}

// Grows out of the platform; in the first phase it ducks away again if Link gets close or takes aim.
static void Wizrobe_Appear(EnWizrobe* this, PlayState* play) {
    bool isFirstPhase = this->fightState == WIZROBE_FIGHT_FIRST_PHASE;

    SkelAnime_Update(&this->skelAnime);
    if (isFirstPhase && (this->actor.xzDistToPlayer < 200.0f || Wizrobe_IsUnderFire(this, play))) {
        Wizrobe_SetupDisappear(this);
        return;
    }
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 0xA, 0xBB8, 0);
    if (isFirstPhase) {
        Math_SmoothStepToS(&this->platformLightAlpha, this->targetPlatformLightAlpha, 10, 10, 10);
        if (!this->shouldStartTimer) {
            this->timer = 20;
            this->shouldStartTimer = true;
        }
    } else {
        PlaySfx(&this->actor, MM_SE_EN_WIZ_VOICE - SFX_FLAG, NA_SE_EN_PO_FLY - SFX_FLAG);
    }
    if (this->timer == 0) {
        Math_ApproachF(&this->scale, WIZROBE_FULL_SCALE, 0.05f, 0.01f);
        Math_SmoothStepToS(&this->alpha, 255, 1, 5, 0);
    }
    if (this->scale >= 0.0138f) {
        Wizrobe_FinishAppearing(this);
    }
}

static void Wizrobe_SetupWindUp(EnWizrobe* this) {
    Wizrobe_ChangeAnim(this, WIZROBE_ANIM_WIND_UP, false);
    this->animLoopCounter = 0;
    this->actionFunc = Wizrobe_WindUp;
}

// Spins in place until its last spell is gone and three dance loops have played.
static void Wizrobe_Dance(EnWizrobe* this, PlayState* play) {
    Math_SmoothStepToS(&this->alpha, 255, 1, 5, 0);
    Math_ApproachF(&this->scale, WIZROBE_FULL_SCALE, 0.05f, 0.001f);
    PlaySfx(&this->actor, MM_SE_EN_WIZ_RUN - SFX_FLAG, NA_SE_EN_PO_FLY - SFX_FLAG);
    this->actor.world.rot.y += this->angularVelocity;
    if (this->fightState >= WIZROBE_FIGHT_GHOSTS_RUN_AROUND) {
        Wizrobe_MoveGhosts(this);
    } else {
        for (s32 i = 0; i < this->ghostCount; i++) {
            this->ghostYaw[i] += this->angularVelocity;
        }
    }
    Math_SmoothStepToS(&this->angularVelocity, 0x1388, 0x64, 0x3E8, 0x3E8);
    Math_SmoothStepToS(&this->platformLightAlpha, this->targetPlatformLightAlpha, 20, 50, 10);
    if (this->skelAnime.curFrame >= this->animEndFrame && this->animLoopCounter < 10) {
        this->animLoopCounter++;
    }
    if (this->animLoopCounter >= 3 && !this->hasActiveProjectile) {
        this->targetPlatformLightAlpha = 0;
        Wizrobe_SetupWindUp(this);
    }
}

static void Wizrobe_SetupAttack(EnWizrobe* this) {
    Wizrobe_ChangeAnim(this, WIZROBE_ANIM_ATTACK, false);
    this->timer = 0;
    this->shouldStartTimer = false;
    this->action = WIZROBE_ACTION_ATTACK;
    this->actionFunc = Wizrobe_Attack;
}

static void Wizrobe_WindUp(EnWizrobe* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 0xC8, 0x1F40, 0x1388);
    if (this->fightState >= WIZROBE_FIGHT_GHOSTS_RUN_AROUND) {
        Wizrobe_MoveGhosts(this);
    } else {
        for (s32 i = 0; i < this->ghostCount; i++) {
            Math_SmoothStepToS(&this->ghostYaw[i], this->actor.yawTowardsPlayer, 0xC8, 0x1F40, 0x1388);
        }
    }
    if (this->skelAnime.curFrame >= this->animEndFrame) {
        this->animLoopCounter++;
        if (this->animLoopCounter >= 2) {
            Wizrobe_SetupAttack(this);
        }
    }
    Math_SmoothStepToS(&this->platformLightAlpha, this->targetPlatformLightAlpha, 10, 10, 10);
}

static void Wizrobe_CastSpell(EnWizrobe* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f pos = this->actor.world.pos;
    s16 magicType = Wizrobe_IsFireType(this) ? MAGIC_TYPE_PROJECTILE : MAGIC_TYPE_ICE_PROJECTILE;

    pos.x += Math_SinS(this->actor.world.rot.y) * 40.0f;
    pos.y += 60.0f;
    pos.z += Math_CosS(this->actor.world.rot.y) * 40.0f;
    Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, sMagicActorId, pos.x, pos.y, pos.z,
                       Math_Vec3f_Pitch(&pos, &player->actor.world.pos),
                       Math_Vec3f_Yaw(&pos, &player->actor.world.pos), 0, magicType);
    this->hasActiveProjectile = true;
    PlaySfx(&this->actor, MM_SE_EN_WIZ_ATTACK, NA_SE_EN_FANTOM_MASIC1);
    Audio_PlayActorSound2(&this->actor, NA_SE_PL_MAGIC_FIRE);
}

static void Wizrobe_Attack(EnWizrobe* this, PlayState* play) {
    if (this->fightState >= WIZROBE_FIGHT_GHOSTS_RUN_AROUND) {
        Wizrobe_MoveGhosts(this);
    }
    if (this->timer != 0) {
        return;
    }
    if (Animation_OnFrame(&this->skelAnime, 6.0f) && !this->hasActiveProjectile) {
        Wizrobe_CastSpell(this, play);
    }
    if (this->skelAnime.curFrame >= 8.0f && !this->shouldStartTimer) {
        this->timer = 3;
        this->shouldStartTimer = true;
    }
    if (this->skelAnime.curFrame >= this->animEndFrame) {
        Wizrobe_SetupDisappear(this);
    }
}

static void Wizrobe_SetupDisappear(EnWizrobe* this) {
    this->angularVelocity = 0x2710;
    if (this->action != WIZROBE_ACTION_DAMAGED) {
        this->timer = 0;
        Wizrobe_ChangeAnim(this, WIZROBE_ANIM_IDLE, false);
        this->action = WIZROBE_ACTION_DISAPPEAR;
    } else {
        this->actor.world.rot.y += this->angularVelocity;
    }
    this->targetPlatformLightAlpha = 0;
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    PlaySfx(&this->actor, MM_SE_EN_WIZ_DISAPPEAR, NA_SE_EN_PO_DISAPPEAR);
    Math_SmoothStepToS(&this->angularVelocity, 0x1388, 0x64, 0x3E8, 0x3E8);
    this->actor.world.rot.y += this->angularVelocity;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = Wizrobe_Disappear;
}

// Spins down into its platform, then picks another one to rise from.
static void Wizrobe_Disappear(EnWizrobe* this, PlayState* play) {
    Math_SmoothStepToS(&this->angularVelocity, 0, 0xA, 0xBB8, 0x14);
    this->actor.world.rot.y += this->angularVelocity;
    if (this->fightState == WIZROBE_FIGHT_FIRST_PHASE || this->action == WIZROBE_ACTION_DAMAGED) {
        Math_ApproachZeroF(&this->scale, 0.3f, 0.01f);
        Math_SmoothStepToS(&this->platformLightAlpha, this->targetPlatformLightAlpha, 5, 50, 0);
    } else {
        Math_ApproachZeroF(&this->scale, 0.3f, 0.001f);
        Math_SmoothStepToS(&this->platformLightAlpha, this->targetPlatformLightAlpha, 10, 50, 0);
        for (s32 i = 0; i < this->ghostCount; i++) {
            this->ghostYaw[i] += this->angularVelocity;
        }
    }
    this->staffTargetFlameScale = sZeroVec;
    if (this->scale >= 0.001f) {
        return;
    }
    this->scale = 0.0f;
    this->alpha = 0;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = Wizrobe_SetupAppear;
}

static void Wizrobe_SetupDamaged(EnWizrobe* this, PlayState* play) {
    bool isAuraActive = this->auraTimer != 0;

    Wizrobe_ChangeAnim(this, WIZROBE_ANIM_DAMAGE, false);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    this->timer = 20;
    if (this->fightState != WIZROBE_FIGHT_FIRST_PHASE && this->actor.colChkInfo.health <= 0) {
        Enemy_StartFinishingBlow(play, &this->actor);
        PlaySfx(&this->actor, MM_SE_EN_WIZ_DEAD, NA_SE_EN_PO_DEAD);
        this->timer = 0;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    } else {
        PlaySfx(&this->actor, MM_SE_EN_WIZ_DAMAGE, NA_SE_EN_PO_DAMAGE);
    }
    this->scale = WIZROBE_FULL_SCALE;
    this->ghostCount = 0;
    this->targetPlatformLightAlpha = 0;
    if (isAuraActive && (this->aura == WIZROBE_AURA_FIRE || this->aura == WIZROBE_AURA_LIGHT)) {
        this->timer = 0;
    }
    this->angularVelocity = 0x4E20;
    if (isAuraActive && this->aura == WIZROBE_AURA_FROZEN) {
        this->angularVelocity = 0;
        this->timer = 0;
        Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 30);
    }
    this->action = WIZROBE_ACTION_DAMAGED;
    this->actionFunc = Wizrobe_Damaged;
}

static void Wizrobe_Launch(EnWizrobe* this) {
    this->actor.velocity.y = 30.0f;
    this->actor.gravity = -3.0f;
}

static void Wizrobe_ShatterIce(EnWizrobe* this, PlayState* play) {
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < WIZROBE_BODYPART_MAX; i++) {
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &this->bodyPartsPos[i], 150, 150, 150, 250, 235, 245, 255,
                                       0.7f);
    }
    this->auraTimer = 0;
    this->aura = WIZROBE_AURA_FIRE;
    this->angularVelocity = 0x4E20;
    Wizrobe_Launch(this);
}

static void Wizrobe_FinishDamage(EnWizrobe* this) {
    this->timer = 0;
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = 0.0f;
    this->auraTimer = 0;
    this->aura = WIZROBE_AURA_NONE;
    if (this->actor.colChkInfo.health > 0) {
        Wizrobe_SetupDisappear(this);
    } else if (this->fightState == WIZROBE_FIGHT_FIRST_PHASE) {
        this->fightState = WIZROBE_FIGHT_SECOND_PHASE_START;
        this->actor.colChkInfo.health = Wizrobe_IsFireType(this) ? WIZROBE_FIRE_HEALTH : WIZROBE_ICE_HEALTH;
        Wizrobe_SetupDisappear(this);
    } else {
        Wizrobe_ChangeAnim(this, WIZROBE_ANIM_DAMAGE, false);
        this->angularVelocity = 0x2710;
        this->action = WIZROBE_ACTION_DEAD;
        this->timer = 0;
        this->actionFunc = Wizrobe_Dead;
    }
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
}

// Spins from the blow; a burning, glowing or dying Wizrobe is flung up first and lands back on its platform.
static void Wizrobe_Damaged(EnWizrobe* this, PlayState* play) {
    bool isBlazing = this->aura == WIZROBE_AURA_FIRE || this->aura == WIZROBE_AURA_LIGHT;

    if (this->auraTimer < 50 && this->aura == WIZROBE_AURA_FROZEN) {
        Wizrobe_ShatterIce(this, play);
    }
    if (this->auraTimer != 0 && this->auraTimer < 30 && isBlazing) {
        Wizrobe_Launch(this);
        this->auraTimer = 0;
        this->aura = WIZROBE_AURA_FIRE;
    } else if (!this->isDead && this->fightState != WIZROBE_FIGHT_FIRST_PHASE &&
               this->actor.colChkInfo.health <= 0) {
        Wizrobe_Launch(this);
        this->isDead = true;
    }
    this->actor.world.rot.y += this->angularVelocity;
    Math_SmoothStepToS(&this->angularVelocity, 0, 0xA, 0xBB8, 0x14);
    for (s32 i = 0; i < this->ghostCount; i++) {
        this->ghostYaw[i] += this->angularVelocity;
    }
    f32 platformTop = this->platformPos[this->curPlatformIndex].y + 11.0f;
    bool hasLanded = this->actor.velocity.y < 0.0f && this->actor.world.pos.y < platformTop;

    if (this->timer == 1 || hasLanded) {
        Wizrobe_FinishDamage(this);
    }
    Math_SmoothStepToS(&this->platformLightAlpha, this->targetPlatformLightAlpha, 20, 50, 10);
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 40.0f, 40.0f, BGCHECK_UPDATE_WALL_CEILING_FLOOR_WATER);
}

// Stops spinning and fades out in flames, taking its platforms with it.
static void Wizrobe_Dead(EnWizrobe* this, PlayState* play) {
    this->actor.world.rot.y += this->angularVelocity;
    Math_SmoothStepToS(&this->angularVelocity, 0, 0xA, 0xBB8, 0x14);
    if (this->angularVelocity < 0x1E) {
        Math_SmoothStepToS(&this->alpha, 0, 10, 30, 20);
        for (s32 i = 0; i < this->ghostCount; i++) {
            Math_SmoothStepToS(&this->ghostAlpha[i], 0, 10, 30, 20);
        }
        this->action = WIZROBE_ACTION_BURST_INTO_FLAMES;
    }
    if (this->alpha >= 30) {
        return;
    }
    Wizrobe_ReleasePlatforms(this, play, true);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_EXTINCT);
    Wizrobe_StopMinibossMusic(this);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

// A Deku Nut flash scatters the phantoms, as MM's nut flash does.
static void Wizrobe_ScatterGhosts(EnWizrobe* this, PlayState* play) {
    if (this->ghostCount == 0 || this->fightState == WIZROBE_FIGHT_SECOND_PHASE_START) {
        return;
    }
    this->fightState = WIZROBE_FIGHT_GHOSTS_COPY_WIZROBE;
    for (s32 i = 0; i < this->ghostCount; i++) {
        if (this->ghostPos[i].x == 0.0f && this->ghostPos[i].z == 0.0f) {
            continue;
        }
        for (s32 j = 0; j < 9; j++) {
            Vec3f rise = { 0.0f, 1.0f, 0.0f };
            Vec3f pos = this->ghostPos[i];

            pos.x += Rand_S16Offset(20, 20) * (Rand_ZeroOne() < 0.5f ? -1.0f : 1.0f);
            pos.y += 70.0f + Rand_CenteredFloat(30.0f);
            pos.z += Rand_S16Offset(20, 20) * (Rand_ZeroOne() < 0.5f ? -1.0f : 1.0f);
            func_8002829C(play, &pos, &rise, &rise, &sDustPrimColor, &sDustEnvColor, Rand_S16Offset(350, 100),
                          Rand_S16Offset(20, 10));
        }
        this->ghostPos[i] = sZeroVec;
    }
    PlaySfx(&this->actor, MM_SE_EN_WIZ_LAUGH, NA_SE_EN_PO_LAUGH);
}

static void Wizrobe_UpdateDamage(EnWizrobe* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    if (this->action < WIZROBE_ACTION_RUN_IN_CIRCLES) {
        return;
    }
    bool isFrozen = this->auraTimer != 0 && this->aura == WIZROBE_AURA_FROZEN;
    bool dealsDamage = false;

    switch (this->actor.colChkInfo.damageEffect) {
        case WIZROBE_DMGEFF_NONE:
            dealsDamage = true;
            break;
        case WIZROBE_DMGEFF_FIRE:
            if (!Wizrobe_IsFireType(this)) {
                this->auraTimer = 40;
                this->aura = WIZROBE_AURA_FIRE;
            }
            dealsDamage = true;
            break;
        case WIZROBE_DMGEFF_FREEZE:
            if (Wizrobe_IsFireType(this)) {
                this->auraTimer = 80;
                this->aura = WIZROBE_AURA_FROZEN;
            }
            dealsDamage = true;
            break;
        case WIZROBE_DMGEFF_LIGHT:
            if (!isFrozen) {
                this->auraTimer = 40;
                this->aura = WIZROBE_AURA_LIGHT;
                dealsDamage = true;
            }
            break;
        case WIZROBE_DMGEFF_NUT:
            Wizrobe_ScatterGhosts(this, play);
            break;
        default:
            break;
    }
    this->alpha = 255;
    if (dealsDamage) {
        Actor_ApplyDamage(&this->actor);
        Wizrobe_SetupDamaged(this, play);
    }
}

static void Wizrobe_UpdateGhostPoses(EnWizrobe* this) {
    if (this->fightState == WIZROBE_FIGHT_FIRST_PHASE) {
        this->ghostCount = 0;
        return;
    }
    Vec3s* source = this->fightState == WIZROBE_FIGHT_GHOSTS_RUN_AROUND ? this->ghostBaseJointTable : this->jointTable;

    for (s32 i = 0; i < this->ghostCount; i++) {
        for (s32 j = 0; j < WIZROBE_LIMB_MAX; j++) {
            this->ghostJointTables[i][j] = source[j];
        }
    }
}

static void EnWizrobe_Init(Actor* thisx, PlayState* play) {
    EnWizrobe* this = (EnWizrobe*)thisx;
    s16 type = (this->actor.params >> 8) & 0xFF;

    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sWizrobeSkel, (AnimationHeader*)sWizrobeIdleAnim,
                       this->jointTable, this->morphTable, WIZROBE_LIMB_MAX);
    SkelAnime_InitFlex(play, &this->ghostSkelAnime, (FlexSkeletonHeader*)sWizrobeSkel,
                       (AnimationHeader*)sWizrobeIdleAnim, this->ghostBaseJointTable, this->ghostMorphTable,
                       WIZROBE_LIMB_MAX);
    Actor_SetScale(&this->actor, 0.0f);
    this->alpha = 255;
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.targetMode = 3;
    this->actor.shape.yOffset = 700.0f;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sBodyCylinderInit);
    this->staffFlameScroll = Rand_S16Offset(0, 7);
    this->type = type > WIZROBE_TYPE_FIRE_NO_BGM ? (Rand_ZeroOne() < 0.5f ? WIZROBE_TYPE_FIRE : WIZROBE_TYPE_ICE)
                                                 : type;
    if (Wizrobe_IsFireType(this)) {
        this->actor.colChkInfo.damageTable = &sFireWizrobeDamageTable;
        this->actor.colChkInfo.health = WIZROBE_FIRE_HEALTH;
    } else {
        this->actor.colChkInfo.damageTable = &sIceWizrobeDamageTable;
        this->actor.colChkInfo.health = WIZROBE_ICE_HEALTH;
    }
    this->curPlatformIndex = WIZROBE_NO_PLATFORM;
    this->action = WIZROBE_ACTION_WAIT;
    this->actionFunc = Wizrobe_WaitForPlayer;
}

static void EnWizrobe_Destroy(Actor* thisx, PlayState* play) {
    EnWizrobe* this = (EnWizrobe*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    Wizrobe_StopMinibossMusic(this);
    Wizrobe_ReleasePlatforms(this, play, false);
    Wizrobe_ReleaseMagic(this, play);
}

static void EnWizrobe_Update(Actor* thisx, PlayState* play) {
    EnWizrobe* this = (EnWizrobe*)thisx;

    if (this->action != WIZROBE_ACTION_APPEAR) {
        SkelAnime_Update(&this->skelAnime);
        SkelAnime_Update(&this->ghostSkelAnime);
    }
    Actor_SetFocus(&this->actor, 60.0f);
    Actor_SetScale(&this->actor, this->scale);
    Wizrobe_UpdateDamage(this, play);
    this->actionFunc(this, play);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    DECR(this->timer);
    DECR(this->auraTimer);
    if (this->action >= WIZROBE_ACTION_RUN_IN_CIRCLES) {
        Collider_UpdateCylinder(&this->actor, &this->collider);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
    Math_ApproachF(&this->staffFlameScale.x, this->staffTargetFlameScale.x, 0.3f, 0.002f);
    Math_ApproachF(&this->staffFlameScale.y, this->staffTargetFlameScale.y, 0.3f, 0.002f);
    Math_ApproachF(&this->staffFlameScale.z, this->staffTargetFlameScale.z, 0.3f, 0.002f);
    Wizrobe_UpdateGhostPoses(this);
}

static bool IsWizrobeBodyPart(s32 limbIndex) {
    switch (limbIndex) {
        case WIZROBE_LIMB_PELVIS:
        case WIZROBE_LIMB_TORSO:
        case WIZROBE_LIMB_LEFT_UPPER_ARM:
        case WIZROBE_LIMB_LEFT_FOREARM:
        case WIZROBE_LIMB_RIGHT_UPPER_ARM:
        case WIZROBE_LIMB_RIGHT_FOREARM:
        case WIZROBE_LIMB_NECK:
        case WIZROBE_LIMB_HEAD:
        case WIZROBE_LIMB_JAW:
        case WIZROBE_LIMB_LEFT_SHIN:
        case WIZROBE_LIMB_RIGHT_SHIN:
        case WIZROBE_LIMB_LOINCLOTH:
            return true;
        default:
            return false;
    }
}

static void Wizrobe_StoreLimbPoints(EnWizrobe* this, s32 limbIndex) {
    if (limbIndex == WIZROBE_LIMB_STAFF) {
        Vec3f staffTip = { 5300.0f, 0.0f, 0.0f };

        if (this->action == WIZROBE_ACTION_DANCE) {
            staffTip.x = 7300.0f;
            staffTip.y = -1500.0f;
        }
        Matrix_MultVec3f(&staffTip, &this->staffFlamePos);
    }
    if (IsWizrobeBodyPart(limbIndex)) {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[this->bodyPartIndex]);
        this->bodyPartIndex = (this->bodyPartIndex + 1) % WIZROBE_BODYPART_MAX;
    }
}

static void Wizrobe_PostLimbDrawOpa(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    Wizrobe_StoreLimbPoints((EnWizrobe*)thisx, limbIndex);
}

// While it burns away every limb sheds a flame each fourth frame.
static void Wizrobe_PostLimbDrawXlu(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx) {
    EnWizrobe* this = (EnWizrobe*)thisx;

    if (this->action != WIZROBE_ACTION_BURST_INTO_FLAMES) {
        Wizrobe_StoreLimbPoints(this, limbIndex);
        return;
    }
    if (this->timer == 0) {
        Vec3f flamePos;

        Matrix_MultVec3f(&sZeroVec, &flamePos);
        flamePos.x += Rand_CenteredFloat(4.0f);
        flamePos.y += Rand_CenteredFloat(7.0f);
        flamePos.z += Rand_CenteredFloat(5.0f);
        EffectSsDeadDb_Spawn(play, &flamePos, &sZeroVec, &sZeroVec, Rand_ZeroFloat(1.0f) * 50.0f + 70.0f, 10, 255, 255,
                             255, 255, 255, 0, 0, 1, 9, false);
        if (limbIndex >= WIZROBE_LIMB_RIGHT_FOOT) {
            this->timer = 4;
        }
    }
    if (IsWizrobeBodyPart(limbIndex)) {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[this->bodyPartIndex]);
        this->bodyPartIndex = (this->bodyPartIndex + 1) % WIZROBE_BODYPART_MAX;
    }
}

// MM's Scene_SetRenderModeXlu tables for segment 0x0C: object_wiz calls entries 0 and 2.
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

static void SetRenderModeSegments(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
    gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)sRenderModeXluDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Wizrobe_DrawBody(EnWizrobe* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    if (this->action == WIZROBE_ACTION_BURST_INTO_FLAMES || this->alpha != 255) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->alpha);
        POLY_XLU_DISP =
            SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                               NULL, Wizrobe_PostLimbDrawXlu, this, POLY_XLU_DISP);
    } else {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, this->alpha);
        SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                              NULL, Wizrobe_PostLimbDrawOpa, this);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Wizrobe_DrawGhosts(EnWizrobe* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    for (s32 i = 0; i < this->ghostCount; i++) {
        if (this->ghostPos[i].x == 0.0f || this->ghostPos[i].z == 0.0f) {
            continue;
        }
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        Matrix_Translate(this->ghostPos[i].x, this->ghostPos[i].y + 10.0f, this->ghostPos[i].z, MTXMODE_NEW);
        Matrix_Scale(this->scale, this->scale, this->scale, MTXMODE_APPLY);
        Matrix_RotateY(BINANG_TO_RAD(this->ghostYaw[i]), MTXMODE_APPLY);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->ghostAlpha[i]);
        POLY_XLU_DISP = SkelAnime_DrawFlex(play, this->ghostSkelAnime.skeleton, this->ghostJointTables[i],
                                           this->ghostSkelAnime.dListCount, NULL, NULL, this, POLY_XLU_DISP);
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's damage-effect fire and light orbs become OoT's flame billboard tinted per element.
static void Wizrobe_DrawAura(EnWizrobe* this, PlayState* play) {
    bool isLight = this->aura == WIZROBE_AURA_LIGHT;
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
    for (s32 i = 0; i < WIZROBE_BODYPART_MAX; i++) {
        billboard.xw = this->bodyPartsPos[i].x;
        billboard.yw = this->bodyPartsPos[i].y;
        billboard.zw = this->bodyPartsPos[i].z;
        Matrix_Put(&billboard);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Wizrobe_DrawPlatformLight(EnWizrobe* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)MakeTwoLayerScroll(play, sPlatformLightRaysScroll));
    gSPSegment(POLY_XLU_DISP++, 0x0A, (uintptr_t)MakeTwoLayerScroll(play, sPlatformLightBaseScroll));
    Matrix_Translate(this->platformLightPos.x, this->platformLightPos.y, this->platformLightPos.z, MTXMODE_NEW);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 255, this->platformLightAlpha);
    if (Wizrobe_IsFireType(this)) {
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 100, 255);
    } else {
        gDPSetEnvColor(POLY_XLU_DISP++, 50, 0, 255, 255);
    }
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sPlatformLightDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Wizrobe_DrawStaffFlame(EnWizrobe* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(this->staffFlamePos.x, this->staffFlamePos.y, this->staffFlamePos.z, MTXMODE_NEW);
    Matrix_Scale(this->staffFlameScale.x, this->staffFlameScale.y, this->staffFlameScale.z, MTXMODE_APPLY);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0,
                                           ((this->staffFlameScroll * 10) - (play->state.frames * 20)) % 512, 32,
                                           128));
    gDPPipeSync(POLY_XLU_DISP++);
    if (Wizrobe_IsFireType(this)) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 170, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 50, 0, 255);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 170, 255, 255, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 50, 255, 255);
    }
    Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnWizrobe_Draw(Actor* thisx, PlayState* play) {
    EnWizrobe* this = (EnWizrobe*)thisx;

    if (this->action == WIZROBE_ACTION_WAIT) {
        return;
    }
    SetRenderModeSegments(play);
    Wizrobe_DrawBody(this, play);
    bool isAuraBurning = this->auraTimer != 0 && this->aura != WIZROBE_AURA_FROZEN;

    if (isAuraBurning) {
        Wizrobe_DrawAura(this, play);
    }
    if (this->ghostCount > 0) {
        Wizrobe_DrawGhosts(this, play);
    }
    if (this->fightState == WIZROBE_FIGHT_FIRST_PHASE) {
        Wizrobe_DrawPlatformLight(this, play);
    }
    Wizrobe_DrawStaffFlame(this, play);
}

static EnWizrobe* Magic_GetCaster(EnWizrobeMagic* this) {
    Actor* parent = this->actor.parent;

    if (parent == NULL || parent->update == NULL || parent->id != sWizrobeActorId) {
        return NULL;
    }
    return (EnWizrobe*)parent;
}

static void Magic_ReleaseCaster(EnWizrobeMagic* this) {
    EnWizrobe* caster = Magic_GetCaster(this);

    if (caster != NULL) {
        caster->hasActiveProjectile = false;
    }
}

static void Magic_SetupMoveProjectile(EnWizrobeMagic* this) {
    Vec3f launch = { 0.0f, 0.0f, this->type == MAGIC_TYPE_PROJECTILE ? 12.0f : Rand_CenteredFloat(2.0f) + 8.0f };

    for (s32 i = 0; i < WIZROBE_TRAIL_COUNT; i++) {
        this->trailPos[i] = this->actor.world.pos;
    }
    this->lowestUsedIndex = 0;
    Matrix_Push();
    Matrix_RotateY(BINANG_TO_RAD(this->actor.world.rot.y), MTXMODE_NEW);
    Matrix_RotateX(BINANG_TO_RAD(this->actor.world.rot.x), MTXMODE_APPLY);
    Matrix_MultVec3f(&launch, &this->actor.velocity);
    Matrix_Pop();
    this->actor.world.rot.x = 0;
    this->actor.world.rot.y = 0;
    this->actor.world.rot.z = 0;
    if (this->type == MAGIC_TYPE_PROJECTILE) {
        this->targetScale = 0.02f;
        this->timer = 100;
    } else {
        this->timer = 50;
        this->actor.velocity.y = 10.0f;
        this->actor.gravity = -1.0f;
        this->targetScale = 0.01f;
    }
    this->action = MAGIC_ACTION_MOVE;
    this->actionFunc = Magic_MoveProjectile;
}

static void Magic_SetupFadeTrail(EnWizrobeMagic* this) {
    this->actor.velocity = sZeroVec;
    this->action = MAGIC_ACTION_FADE_TRAIL;
    this->trailFadeTimer = 0;
    this->actionFunc = Magic_FadeTrail;
}

// An arcing ember dies where it lands and leaves a small flame behind.
static void Magic_LandArcingProjectile(EnWizrobeMagic* this, PlayState* play) {
    if (!this->hasSpawnedSmallFlame) {
        Actor_Spawn(&play->actorCtx, play, sMagicActorId, this->actor.world.pos.x, this->actor.world.pos.y - 10.0f,
                    this->actor.world.pos.z, 0, 0, 0, MAGIC_TYPE_SMALL_FLAME);
        this->hasSpawnedSmallFlame = true;
    }
    this->timer = 0;
    this->scale = 0.0f;
    Magic_SetupFadeTrail(this);
}

// The main spell turns into a pool on scene floor; fire spells also burst into five arcing embers.
static void Magic_LandProjectile(EnWizrobeMagic* this, PlayState* play) {
    if (this->actor.floorBgId != BGCHECK_SCENE) {
        return;
    }
    if (!this->isIceType) {
        s16 emberYaw = 0;

        for (s32 i = 0; i < 5; i++) {
            Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, sMagicActorId, this->actor.world.pos.x,
                               this->actor.world.pos.y, this->actor.world.pos.z, 0, emberYaw, 0,
                               MAGIC_TYPE_ARCING_PROJECTILE);
            emberYaw += (s16)((s32)Rand_CenteredFloat(0x1000) + 0x10000 / 5);
        }
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_BOMB_EXPLOSION);
        this->poolTimer = Rand_S16Offset(70, 30);
        PlaySfx(&this->actor, MM_SE_EN_WIZ_EXP - SFX_FLAG, NA_SE_EV_BURN_OUT - SFX_FLAG);
    } else {
        this->poolTimer = 100;
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_FREEZE - SFX_FLAG);
    }
    this->actor.velocity = sZeroVec;
    this->timer = 0;
    this->action = MAGIC_ACTION_POOL;
    this->scale = 0.0f;
    this->actionFunc = Magic_Pool;
}

static void Magic_Reflect(EnWizrobeMagic* this) {
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_SHIELD_REFLECT_MG);
    this->collider.base.atFlags &= ~(AT_TYPE_ENEMY | AT_BOUNCED | AT_HIT);
    this->collider.base.atFlags |= AT_TYPE_PLAYER;
    this->collider.info.toucher.dmgFlags = DMG_ARROW_NORMAL;
    this->collider.info.toucher.damage = 2;
    this->timer = 100;
    this->type = MAGIC_TYPE_REFLECTED_PROJECTILE;
    this->actor.velocity.x *= -1.0f;
    this->actor.velocity.y *= -0.5f;
    this->actor.velocity.z *= -1.0f;
    Magic_ReleaseCaster(this);
}

static bool Magic_IsHitByIceArrow(EnWizrobeMagic* this) {
    return this->collider.info.acHitInfo != NULL && this->collider.info.acHitInfo->toucher.dmgFlags == DMG_ARROW_ICE;
}

static void Magic_MoveProjectile(EnWizrobeMagic* this, PlayState* play) {
    this->actor.world.rot.z += 5000;
    this->targetScale = this->type == MAGIC_TYPE_PROJECTILE ? 0.02f : 0.01f;
    if (this->timer == 0 && this->scale < 0.001f) {
        Magic_SetupFadeTrail(this);
        return;
    }
    if (this->timer == 0) {
        this->targetScale = 0.0f;
    }
    Math_ApproachF(&this->scale, this->targetScale, 0.2f, 0.01f);
    bool hasHitWall = this->wallCheckTimer == 0 && (this->actor.bgCheckFlags & BGCHECKFLAG_WALL);

    if (hasHitWall && this->type == MAGIC_TYPE_PROJECTILE && this->timer != 0) {
        sPoolHitByIceArrow = false;
        this->timer = 0;
        this->targetScale = 0.0f;
    }
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && this->poolTimer == 0) {
        if (this->type == MAGIC_TYPE_ARCING_PROJECTILE) {
            Magic_LandArcingProjectile(this, play);
            return;
        }
        if (this->type == MAGIC_TYPE_PROJECTILE && this->timer != 0) {
            Magic_LandProjectile(this, play);
            return;
        }
    }
    if (this->type == MAGIC_TYPE_REFLECTED_PROJECTILE || this->timer == 0) {
        return;
    }
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        if (Magic_IsHitByIceArrow(this)) {
            this->timer = 0;
            this->hitByIceArrow = true;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_MELT);
        }
    }
    if (Player_HasMirrorShieldEquipped(play) && (this->collider.base.atFlags & AT_BOUNCED)) {
        Magic_Reflect(this);
    }
}

static void Magic_SmallFlame(EnWizrobeMagic* this, PlayState* play) {
    if (this->timer <= 10) {
        Math_ApproachF(&this->scale, 2.0f * this->targetScale, 0.2f, 0.002f);
        Math_ApproachZeroF(&this->alpha, 1.0f, 35.0f);
        if (this->timer == 0 && this->alpha < 2.0f) {
            Actor_Kill(&this->actor);
        }
        return;
    }
    Math_ApproachF(&this->scale, this->targetScale, 0.3f, 0.01f);
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        this->timer -= 10;
        if (Magic_IsHitByIceArrow(this)) {
            this->timer = 0;
            this->hitByIceArrow = true;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_MELT);
        }
    }
    if (this->timer != 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_BURN_OUT - SFX_FLAG);
    }
}

static void Magic_AddIceSmoke(EnWizrobeMagic* this, Vec3f* pos, Vec3f* accel) {
    for (s32 i = 0; i < WIZROBE_ICE_SMOKE_COUNT; i++) {
        WizrobeIceSmoke* smoke = &this->iceSmoke[i];

        if (smoke->isEnabled) {
            continue;
        }
        smoke->isEnabled = true;
        smoke->velocity = sZeroVec;
        smoke->smokeScroll = Rand_ZeroFloat(100.0f);
        smoke->pos = *pos;
        smoke->accel = *accel;
        smoke->scale = (Rand_ZeroFloat(5.0f) + 20.0f) * 0.001f;
        smoke->isFadingOut = false;
        smoke->alpha = 0;
        return;
    }
}

static void Magic_GrowIcePool(EnWizrobeMagic* this) {
    Vec3f accel = { Rand_CenteredFloat(3.0f) / 10.0f, 0.23f, Rand_CenteredFloat(3.0f) / 10.0f };
    Vec3f pos = this->actor.world.pos;

    pos.x += Rand_CenteredFloat(150.0f);
    pos.z += Rand_CenteredFloat(150.0f);
    Math_ApproachF(&this->poolScale, 0.022f, 0.3f, 0.01f);
    this->collider.dim.radius = this->poolScale * 4300.0f;
    this->collider.dim.height = 30;
    this->collider.dim.yShift = 15;
    Magic_AddIceSmoke(this, &pos, &accel);
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_FREEZE - SFX_FLAG);
}

static void Magic_GrowFirePool(EnWizrobeMagic* this) {
    Math_ApproachF(&this->poolScale, 0.02f, 0.3f, 0.002f);
    Math_ApproachF(&this->fireSmokeScale, 0.02f, 0.3f, 0.002f);
    Math_ApproachF(&this->bigFlameScale, 0.02f, 0.3f, 0.2f);
    this->collider.dim.radius = this->poolScale * 4000.0f;
    this->collider.dim.height = MAX(this->bigFlameScale * 1850.0f, 2.0f);
    this->collider.dim.yShift = -15;
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        if (!sPoolHitByIceArrow && Magic_IsHitByIceArrow(this)) {
            sPoolHitByIceArrow = true;
            this->hitByIceArrow = true;
            this->poolTimer = 0;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_MELT);
        }
    }
    this->actor.world.pos.y = this->actor.floorHeight + 10.0f;
    Actor_SetFocus(&this->actor, 0.0f);
}

static void Magic_ShrinkIcePool(EnWizrobeMagic* this) {
    Math_ApproachZeroF(&this->poolScale, 0.046f, 0.001f);
    Audio_PlayActorSound2(&this->actor, NA_SE_EV_ICE_FREEZE - SFX_FLAG);
    if (!this->shouldPoolFadeOut && Magic_GetCaster(this) != NULL && this->poolScale < 0.05f) {
        this->collider.dim.yShift = -15;
        this->shouldPoolFadeOut = true;
        Magic_ReleaseCaster(this);
    }
    if (this->shouldPoolFadeOut && this->poolScale < 0.05f) {
        Math_ApproachZeroF(&this->alpha, 1.0f, 5.0f);
    }
    if (this->poolScale < 0.001f && this->blendScaleFrac < 0.001f) {
        sPoolHitByIceArrow = false;
        Actor_Kill(&this->actor);
    }
}

static void Magic_ShrinkFirePool(EnWizrobeMagic* this) {
    Math_ApproachZeroF(&this->bigFlameScale, 0.1f, 0.01f);
    if (this->bigFlameScale >= 0.01f) {
        return;
    }
    Math_ApproachZeroF(&this->alpha, 1.0f, 10.0f);
    if (this->alpha < 10.0f && this->blendScaleFrac < 0.001f) {
        sPoolHitByIceArrow = false;
        Magic_ReleaseCaster(this);
        Actor_Kill(&this->actor);
    }
}

static void Magic_Pool(EnWizrobeMagic* this, PlayState* play) {
    EnWizrobe* caster = Magic_GetCaster(this);

    if (caster != NULL && caster->actor.colChkInfo.health == 0) {
        this->poolTimer = 0;
    }
    if (this->lowestUsedIndex < WIZROBE_TRAIL_COUNT) {
        this->lowestUsedIndex++;
    }
    if (this->poolTimer != 0) {
        Math_ApproachF(&this->blendScale, 60.0f, 0.5f, 10.0f);
        if (this->isIceType) {
            Magic_GrowIcePool(this);
        } else {
            Magic_GrowFirePool(this);
        }
        return;
    }
    Math_ApproachZeroF(&this->blendScale, 0.2f, 3.0f);
    if (this->isIceType) {
        Magic_ShrinkIcePool(this);
    } else {
        Magic_ShrinkFirePool(this);
    }
}

static void Magic_FadeTrail(EnWizrobeMagic* this, PlayState* play) {
    if (this->trailFadeTimer != 0) {
        return;
    }
    this->trailFadeTimer = 2;
    this->lowestUsedIndex++;
    if (this->lowestUsedIndex < 6) {
        return;
    }
    if (this->type == MAGIC_TYPE_PROJECTILE && Magic_GetCaster(this) != NULL) {
        sPoolHitByIceArrow = false;
        Magic_ReleaseCaster(this);
    }
    Actor_Kill(&this->actor);
}

static void Magic_ClearLighting(PlayState* play) {
    for (s32 i = 0; i < 3; i++) {
        play->envCtx.adjAmbientColor[i] = 0;
        play->envCtx.adjLight1Color[i] = 0;
        play->envCtx.adjFogColor[i] = 0;
    }
    play->envCtx.adjFogNear = 0;
}

// The spell tints the room toward fire or frost while its pool burns; OoT has one diffuse adjust for both lights.
static void Magic_TintLighting(EnWizrobeMagic* this, PlayState* play) {
    static const u8 sAmbient[2][3] = { { 100, 40, 40 }, { 0, 0, 0 } };
    static const u8 sDiffuse[2][3] = { { 180, 120, 80 }, { 200, 250, 250 } };
    static const u8 sFog[2][3] = { { 125, 20, 0 }, { 225, 255, 235 } };
    s32 element = this->isIceType ? 1 : 0;
    EnvLightSettings* base = &play->envCtx.lightSettings;
    f32 frac = this->blendScaleFrac;
    f32 fogNear = this->isIceType ? 970.0f : 968.0f;

    play->envCtx.adjFogNear = (fogNear - base->fogNear) * frac;
    for (s32 i = 0; i < 3; i++) {
        play->envCtx.adjAmbientColor[i] = (sAmbient[element][i] - base->ambientColor[i]) * frac;
        play->envCtx.adjLight1Color[i] = (sDiffuse[element][i] - base->light1Color[i]) * frac;
        play->envCtx.adjFogColor[i] = (sFog[element][i] - base->fogColor[i]) * frac;
    }
}

static void Magic_UpdateIceSmoke(EnWizrobeMagic* this) {
    for (s32 i = 0; i < WIZROBE_ICE_SMOKE_COUNT; i++) {
        WizrobeIceSmoke* smoke = &this->iceSmoke[i];

        if (!smoke->isEnabled) {
            continue;
        }
        smoke->smokeScroll++;
        Math_Vec3f_Sum(&smoke->pos, &smoke->velocity, &smoke->pos);
        Math_Vec3f_Sum(&smoke->velocity, &smoke->accel, &smoke->velocity);
        if (!smoke->isFadingOut) {
            smoke->alpha += 10;
            smoke->isFadingOut = smoke->alpha >= 100;
            continue;
        }
        smoke->alpha -= 8;
        if (smoke->alpha <= 0) {
            smoke->alpha = 0;
            smoke->isEnabled = false;
        }
    }
}

static void Magic_SpawnSteam(EnWizrobeMagic* this, PlayState* play) {
    Vec3f rise = { 0.0f, 1.0f, 0.0f };
    s16 scaleStep = this->type == MAGIC_TYPE_PROJECTILE ? Rand_S16Offset(40, 20) : Rand_S16Offset(20, 10);

    this->steamSpawnTimer = Rand_S16Offset(2, 2);
    for (s32 i = 0; i < 2; i++) {
        Vec3f pos;

        pos.x = (Rand_ZeroOne() < 0.5f ? -1.0f : 1.0f) * Rand_S16Offset(20, 20) + this->actor.world.pos.x;
        pos.y = Rand_ZeroOne() * 20.0f + this->actor.floorHeight;
        pos.z = (Rand_ZeroOne() < 0.5f ? -1.0f : 1.0f) * Rand_S16Offset(20, 20) + this->actor.world.pos.z;
        func_8002829C(play, &pos, &rise, &rise, &sDustPrimColor, &sDustEnvColor, Rand_S16Offset(350, 100),
                      scaleStep);
    }
}

static void Magic_SizeCollider(EnWizrobeMagic* this) {
    if (this->type == MAGIC_TYPE_SMALL_FLAME) {
        this->collider.dim.radius = 10;
        this->collider.dim.height = this->scale * 5000.0f;
        this->collider.dim.yShift = 0;
    } else if (this->action < MAGIC_ACTION_POOL) {
        this->collider.dim.radius = this->scale * 15.0f + 25.0f;
        this->collider.dim.height = this->scale * 15.0f + 25.0f;
        this->collider.dim.yShift = this->scale * -0.75f - 5.0f;
    }
}

// A spell that lands on Link keeps him invulnerable longer, as MM does; OoT's timer is an s8.
static void Magic_ExtendInvincibility(EnWizrobeMagic* this, Player* player) {
    s32 extension = this->isIceType ? 90 : 40;

    if (player->invincibilityTimer > 0) {
        player->invincibilityTimer = MIN(player->invincibilityTimer + extension, WIZROBE_INVINCIBILITY_MAX);
    }
}

static void Magic_SubmitCollider(EnWizrobeMagic* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        if (this->type == MAGIC_TYPE_PROJECTILE) {
            PlaySfx(&this->actor, MM_SE_EN_WIZ_LAUGH2, NA_SE_EN_PO_LAUGH2);
            Magic_ExtendInvincibility(this, player);
        }
    }
    bool isQuenched = this->hitByIceArrow || sPoolHitByIceArrow;
    bool isSolid = this->type != MAGIC_TYPE_PROJECTILE || this->alpha > 200.0f;

    if (isQuenched || !isSolid) {
        return;
    }
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    if (player->invincibilityTimer == 0) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}

static void EnWizrobeMagic_Init(Actor* thisx, PlayState* play) {
    EnWizrobeMagic* this = (EnWizrobeMagic*)thisx;

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sMagicCylinderInit);
    this->type = this->actor.params;
    this->actor.targetMode = 3;
    this->wallCheckTimer = 10;
    this->alpha = 255.0f;
    if (!Player_HasMirrorShieldEquipped(play)) {
        this->collider.info.toucher.dmgFlags = DMG_UNBLOCKABLE;
    }
    switch (this->type) {
        case MAGIC_TYPE_ICE_PROJECTILE:
            this->isIceType = true;
            this->collider.info.toucher.damage = 8;
            this->collider.info.toucher.effect = HIT_SPECIAL_EFFECT_ICE;
            this->type = MAGIC_TYPE_PROJECTILE;
            Magic_SetupMoveProjectile(this);
            break;
        case MAGIC_TYPE_PROJECTILE:
        case MAGIC_TYPE_ARCING_PROJECTILE:
        case MAGIC_TYPE_REFLECTED_PROJECTILE:
            Magic_SetupMoveProjectile(this);
            break;
        case MAGIC_TYPE_SMALL_FLAME:
            this->actor.draw = Magic_DrawSmallFlame;
            this->smallFlameScroll = Rand_S16Offset(0, 10000);
            this->action = MAGIC_ACTION_SMALL_FLAME;
            this->collider.info.toucher.damage = 2;
            this->targetScale = 0.02f;
            this->timer = Rand_S16Offset(50, 50);
            this->scaleMod.x = Rand_CenteredFloat(1.0f) * 0.007f;
            this->scaleMod.y = Rand_CenteredFloat(1.0f) * 0.005f;
            this->scaleMod.z = Rand_CenteredFloat(1.0f) * 0.007f;
            this->actionFunc = Magic_SmallFlame;
            break;
        default:
            Actor_Kill(&this->actor);
            break;
    }
}

static void EnWizrobeMagic_Destroy(Actor* thisx, PlayState* play) {
    EnWizrobeMagic* this = (EnWizrobeMagic*)thisx;

    if (this->type == MAGIC_TYPE_PROJECTILE) {
        Magic_ClearLighting(play);
    }
    Collider_DestroyCylinder(play, &this->collider);
}

static void EnWizrobeMagic_Update(Actor* thisx, PlayState* play) {
    EnWizrobeMagic* this = (EnWizrobeMagic*)thisx;
    EnWizrobe* caster = Magic_GetCaster(this);

    Actor_SetScale(&this->actor, this->scale);
    Magic_UpdateIceSmoke(this);
    this->blendScaleFrac = this->blendScale / 60.0f;
    if (this->type == MAGIC_TYPE_PROJECTILE && caster != NULL && caster->type != WIZROBE_TYPE_FIRE_NO_BGM) {
        Magic_TintLighting(this, play);
    }
    this->smallFlameScroll++;
    this->actionFunc(this, play);
    this->actor.shape.yOffset = 10.0f;
    Actor_UpdatePos(&this->actor);
    for (s32 i = WIZROBE_TRAIL_COUNT - 2; i >= 0; i--) {
        this->trailPos[i + 1] = this->trailPos[i];
    }
    this->trailPos[0] = this->actor.world.pos;
    this->actor.velocity.y += this->actor.gravity;
    DECR(this->wallCheckTimer);
    DECR(this->timer);
    DECR(this->steamSpawnTimer);
    DECR(this->poolTimer);
    DECR(this->trailFadeTimer);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 5.0f, 10.0f, BGCHECK_UPDATE_WALL_FLOOR_WATER);
    if ((this->hitByIceArrow || sPoolHitByIceArrow) && this->steamSpawnTimer == 0) {
        Magic_SpawnSteam(this, play);
    }
    Magic_SizeCollider(this);
    Magic_SubmitCollider(this, play);
}

static void Magic_DrawTrail(EnWizrobeMagic* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    for (s32 i = WIZROBE_TRAIL_COUNT - 1; i >= this->lowestUsedIndex; i--) {
        f32 scale = this->actor.scale.x + i * 0.0019f;

        if (scale <= 0.0f) {
            continue;
        }
        if (!this->isIceType) {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255 - i * 25, 0, 255 - i * 25);
            gDPSetEnvColor(POLY_XLU_DISP++, 255 - i * 25, 0, 0, 0);
        } else {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255 - i * 7, 255 - i * 3, 255 - i * 3, 255 - i * 25);
            gDPSetEnvColor(POLY_XLU_DISP++, 220, 255, 235, 0);
        }
        Matrix_Translate(this->trailPos[i].x, this->trailPos[i].y + this->actor.shape.yOffset, this->trailPos[i].z,
                         MTXMODE_NEW);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_RotateZ(BINANG_TO_RAD(this->actor.world.rot.z), MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sMagicProjectileDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Magic_DrawIceSmoke(EnWizrobeMagic* this, PlayState* play) {
    bool isMaterialSet = false;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    for (s32 i = 0; i < WIZROBE_ICE_SMOKE_COUNT; i++) {
        WizrobeIceSmoke* smoke = &this->iceSmoke[i];

        if (!smoke->isEnabled) {
            continue;
        }
        if (!isMaterialSet) {
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sIceSmokeMaterialDL);
            isMaterialSet = true;
        }
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 195, 225, 235, smoke->alpha);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, (smoke->smokeScroll * 3) & 0x7F,
                                               (smoke->smokeScroll * 0xF) & 0xFF, 32, 64, 1, 0, 0, 32, 32));
        Matrix_Translate(smoke->pos.x, smoke->pos.y, smoke->pos.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(smoke->scale, smoke->scale, 1.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPSetRenderMode(POLY_XLU_DISP++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
        gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BACK | G_FOG);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sIceSmokeModelDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Magic_DrawIcePool(EnWizrobeMagic* this, PlayState* play) {
    u32 frames = play->state.frames;
    u8 alpha = this->alpha;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(this->actor.world.pos.x, this->actor.floorHeight, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_Scale(this->poolScale, this->poolScale, this->poolScale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 40, 30, 80);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sIcePoolDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 215, 215, 215, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 128);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, -frames & 0x7F, -frames & 0x7F, 32, 64, 1,
                                           frames & 0xFF, frames & 0xFF, 16, 16));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sIcePoolShineDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Magic_DrawBillboardLayer(PlayState* play, Vec3f* pos, f32 scale, Gfx* scroll, const char* dl) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)scroll);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)dl);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Magic_DrawFirePool(EnWizrobeMagic* this, PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 frames = play->state.frames;
    u8 alpha = this->alpha;
    Vec3f base = { this->actor.world.pos.x, this->actor.floorHeight, this->actor.world.pos.z };

    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Xlu(gfxCtx);
    Matrix_Translate(base.x, base.y, base.z, MTXMODE_NEW);
    Matrix_Scale(this->poolScale, this->poolScale, this->poolScale, MTXMODE_APPLY);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(gfxCtx, 0, -frames % 128, 0, 0x20, 0x20, 1, (frames * 2) % 128, 0, 0x20,
                                           0x20));
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 100, 40, 0, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 245, 255, 128);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sFirePoolDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 80, 0, 0, alpha);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, 100);
    CLOSE_DISPS(gfxCtx);
    Magic_DrawBillboardLayer(play, &base, this->fireSmokeScale,
                             Gfx_TwoTexScroll(gfxCtx, 0, frames % 128, (-frames * 6) % 256, 0x20, 0x40, 1,
                                              (frames * 2) % 128, (-frames * 6) % 256, 0x20, 0x40),
                             sFireSmokeDL);
    OPEN_DISPS(gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 100, 50, 0, 255);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetEnvColor(POLY_XLU_DISP++, 200, 235, 240, 128);
    CLOSE_DISPS(gfxCtx);
    Magic_DrawBillboardLayer(play, &base, this->bigFlameScale,
                             Gfx_TwoTexScroll(gfxCtx, 0, (-frames * 3) % 128, 0, 0x20, 0x20, 1, 0,
                                              (-frames * 10) % 256, 0x20, 0x40),
                             sBigFlameDL);
}

static void EnWizrobeMagic_Draw(Actor* thisx, PlayState* play) {
    EnWizrobeMagic* this = (EnWizrobeMagic*)thisx;
    bool isPool = this->type == MAGIC_TYPE_PROJECTILE && this->action == MAGIC_ACTION_POOL;

    Magic_DrawTrail(this, play);
    if (!isPool) {
        return;
    }
    if (this->isIceType) {
        Magic_DrawIcePool(this, play);
        Magic_DrawIceSmoke(this, play);
    } else {
        Magic_DrawFirePool(this, play);
    }
}

static void Magic_DrawSmallFlame(Actor* thisx, PlayState* play) {
    EnWizrobeMagic* this = (EnWizrobeMagic*)thisx;
    s16 scroll = this->smallFlameScroll;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(this->actor.world.pos.x, this->actor.floorHeight + 20.0f, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_Scale(this->scale + this->scaleMod.x, this->scale + this->scaleMod.y, this->scale + this->scaleMod.z,
                 MTXMODE_APPLY);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 32, 32, 1, scroll & 0x7F,
                                           (-scroll * 10) & 0x7F, 32, 32));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 100, 50, 0, (u8)this->alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 200, 235, 245, 255);
    Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSmallFlameDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnWizrobePlatform_Init(Actor* thisx, PlayState* play) {
    EnWizrobePlatform* this = (EnWizrobePlatform*)thisx;
    CollisionHeader* header = NULL;

    DynaPolyActor_Init(&this->dyna, 0);
    CollisionHeader_GetVirtual((void*)sPlatformCol, &header);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, header);
    this->dyna.actor.colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(&this->dyna.actor, 0.01f);
    this->element = this->dyna.actor.params;
    this->alpha = 255.0f;
}

static void EnWizrobePlatform_Destroy(Actor* thisx, PlayState* play) {
    EnWizrobePlatform* this = (EnWizrobePlatform*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

// Once its Wizrobe dies the platform waits a second, then flattens out and fades.
static void EnWizrobePlatform_Update(Actor* thisx, PlayState* play) {
    EnWizrobePlatform* this = (EnWizrobePlatform*)thisx;

    if (!this->isVanishing) {
        return;
    }
    this->vanishTimer++;
    if (this->vanishTimer <= 30) {
        return;
    }
    Math_ApproachZeroF(&this->dyna.actor.scale.y, 0.3f, 0.003f);
    Math_ApproachZeroF(&this->alpha, 1.0f, 35.0f);
    Math_ApproachF(&this->dyna.actor.scale.x, 0.02f, 0.2f, 0.002f);
    this->dyna.actor.scale.z = this->dyna.actor.scale.x;
    if (this->dyna.actor.scale.y < 0.001f) {
        Actor_Kill(&this->dyna.actor);
    }
}

static void EnWizrobePlatform_Draw(Actor* thisx, PlayState* play) {
    EnWizrobePlatform* this = (EnWizrobePlatform*)thisx;
    u8 alpha = this->alpha;

    SetRenderModeSegments(play);
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(this->dyna.actor.world.pos.x, this->dyna.actor.world.pos.y, this->dyna.actor.world.pos.z,
                     MTXMODE_NEW);
    Matrix_Scale(this->dyna.actor.scale.x, this->dyna.actor.scale.y, this->dyna.actor.scale.z, MTXMODE_APPLY);
    if (!this->isVanishing) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sPlatformDL);
    } else {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, alpha);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sPlatformDL);
    }
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)MakePlatformCenterColor(play));
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)MakeTwoLayerScroll(play, sPlatformCenterScroll));
    gDPPipeSync(POLY_XLU_DISP++);
    if (this->element == WIZROBE_TYPE_FIRE) {
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 100, alpha);
    } else {
        gDPSetEnvColor(POLY_XLU_DISP++, 50, 0, 255, alpha);
    }
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sPlatformCenterDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool CanBeGoron(void) {
    int32_t goronMask = sApi->GetRandoItem(GORON_MASK_KEY);

    return goronMask >= 0 && sApi->LogicHasItem(goronMask);
}

// MM's CanKillEnemy(ACTOR_EN_WIZ): bow, hookshot, a sword or Goron fists; the hammer stands in for the fists.
static bool Wizrobe_CanKillInLogic(void* userData) {
    return sApi->LogicCanUseItem(RG_FAIRY_BOW) || sApi->LogicCanUseItem(RG_HOOKSHOT) ||
           sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RG_MEGATON_HAMMER) || CanBeGoron();
}

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
    static Vec3s sSoulJointTable[WIZROBE_LIMB_MAX];
    static Vec3s sSoulMorphTable[WIZROBE_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sWizrobeSkel, (AnimationHeader*)sWizrobeIdleAnim,
                           sSoulJointTable, sSoulMorphTable, WIZROBE_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
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
    SOHCustomItemDefinition soul = Z64Items_Define(WIZROBE_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Wizrobes";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Wizrobes%w!&Wizrobes can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " WIZROBE_SOUL_ITEM_KEY);
    }
}

static void RegisterMagic(void) {
    SOHActorDefinition magic = { 0 };

    magic.structSize = sizeof(magic);
    magic.key = WIZROBE_MAGIC_KEY;
    magic.description = "Wizrobe Magic";
    magic.category = ACTORCAT_MISC;
    magic.actorFlags = ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_LOCK_ON_DISABLED;
    magic.objectId = OBJECT_GAMEPLAY_KEEP;
    magic.instanceSize = sizeof(EnWizrobeMagic);
    magic.init = EnWizrobeMagic_Init;
    magic.destroy = EnWizrobeMagic_Destroy;
    magic.update = EnWizrobeMagic_Update;
    magic.draw = EnWizrobeMagic_Draw;
    sMagicActorId = sApi->RegisterActor(&magic);
}

static void RegisterPlatform(void) {
    SOHActorDefinition platform = { 0 };

    platform.structSize = sizeof(platform);
    platform.key = WIZROBE_PLATFORM_KEY;
    platform.description = "Wizrobe Warp Platform";
    platform.category = ACTORCAT_BG;
    platform.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_LOCK_ON_DISABLED;
    platform.objectId = OBJECT_GAMEPLAY_KEEP;
    platform.instanceSize = sizeof(EnWizrobePlatform);
    platform.init = EnWizrobePlatform_Init;
    platform.destroy = EnWizrobePlatform_Destroy;
    platform.update = EnWizrobePlatform_Update;
    platform.draw = EnWizrobePlatform_Draw;
    sPlatformActorId = sApi->RegisterActor(&platform);
}

static bool RegisterHelpers(void) {
    RegisterMagic();
    RegisterPlatform();
    return sMagicActorId >= 0 && sPlatformActorId >= 0;
}

static void RegisterWizrobe(void) {
    SOHActorDefinition wizrobe = { 0 };

    wizrobe.structSize = sizeof(wizrobe);
    wizrobe.key = WIZROBE_KEY;
    wizrobe.description = "Wizrobe";
    wizrobe.category = ACTORCAT_ENEMY;
    wizrobe.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                         ACTOR_FLAG_DRAW_CULLING_DISABLED | ACTOR_FLAG_IGNORE_QUAKE | ACTOR_FLAG_LOCK_ON_DISABLED;
    wizrobe.objectId = OBJECT_GAMEPLAY_KEEP;
    wizrobe.instanceSize = sizeof(EnWizrobe);
    wizrobe.init = EnWizrobe_Init;
    wizrobe.destroy = EnWizrobe_Destroy;
    wizrobe.update = EnWizrobe_Update;
    wizrobe.draw = EnWizrobe_Draw;
    wizrobe.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_MINIBOSS | SOH_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS |
                         SOH_ACTOR_ENEMY_NOT_IN_WATER;
    wizrobe.enemyParams = WIZROBE_TYPE_RANDOM << 8;
    wizrobe.naviHint = "Wizrobe&%cIt warps between its platforms casting fire or ice. Strike while it dances, and "
                       "a %rDeku Nut%c clears away its phantoms!%w";
    wizrobe.soulKey = WIZROBE_KEY;
    wizrobe.logicKill = Wizrobe_CanKillInLogic;
    sWizrobeActorId = sApi->RegisterActor(&wizrobe);
    if (sWizrobeActorId < 0) {
        sApi->Log("Could not register " WIZROBE_KEY);
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
    if (!RegisterHelpers()) {
        sApi->Log("Could not register the Wizrobe's magic and platforms");
        return;
    }
    RegisterSoul();
    RegisterWizrobe();
}
