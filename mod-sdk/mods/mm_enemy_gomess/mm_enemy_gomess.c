// Majora's Mask's Gomess (En_Death + its bat swarm, En_Minideath) as an Unbound custom enemy, drawn from mm.o2r.
// The fight starts at once, as after MM's intro: no cutscenes, no light dimming, and it dies where it stands.
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

#define GOMESS_KEY "mm.gomess"
#define GOMESS_SOUL_ITEM_KEY "soul.mm.gomess"
#define GOMESS_BAT_KEY "mm.gomess.bat"
#define GORON_MASK_KEY "nei.goron_form_mask"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_DEATH "__OTR__objects/object_death/"

#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_CEILING_FLOOR 0x07

#define MM_SE_EV_BAT_FLY 0x29A7
#define MM_SE_EN_COMMON_EXTINCT_LEV 0x3A1F
#define MM_SE_EN_SHARP_FLOAT 0x3A60
#define MM_SE_EN_DEATH_SCYTHE 0x3AAC
#define MM_SE_EN_DEATH_ROLL 0x3AAD
#define MM_SE_EN_DEATH_SCYTHE_LEV 0x3AAE
#define MM_SE_EN_DEATH_SCYTHE_ONGND 0x3AAF
#define MM_SE_EN_DEATH_VOICE 0x3AB0
#define MM_SE_EN_DEATH_DAMAGE 0x3AB1
#define MM_SE_EN_DEATH_DEAD 0x3AB2
#define MM_SE_EN_DEATH_ATTACK 0x3AB3
#define MM_SE_EN_DEATH_HEARTBREAK 0x3AB5

#define GOMESS_HEALTH 20
#define GOMESS_SCALE 0.01f
#define GOMESS_BAT_COUNT 20
#define GOMESS_GUARD_BAT_COUNT 5
#define GOMESS_FLAME_COUNT 56
#define GOMESS_BODYPART_COUNT 14
#define GOMESS_CORE_COLOR_PERIOD 24
#define GOMESS_CORE_RADIUS 22
#define BAT_EFFECT_COUNT 3
#define BAT_ANIM_LENGTH 9
#define BAT_SCALE 0.007f

typedef enum {
    GOMESS_LIMB_NONE,
    GOMESS_LIMB_1,
    GOMESS_LIMB_CORE_POS,
    GOMESS_LIMB_LOWER_BODY,
    GOMESS_LIMB_TORSO,
    GOMESS_LIMB_COLLAR,
    GOMESS_LIMB_HEAD_LOWER,
    GOMESS_LIMB_HEAD_UPPER,
    GOMESS_LIMB_8,
    GOMESS_LIMB_9,
    GOMESS_LIMB_10,
    GOMESS_LIMB_11,
    GOMESS_LIMB_CLOAK_UPPER,
    GOMESS_LIMB_CLOAK_LOWER,
    GOMESS_LIMB_UPPER_ARM_LEFT,
    GOMESS_LIMB_FOREARM_LEFT,
    GOMESS_LIMB_HAND_LEFT,
    GOMESS_LIMB_UPPER_ARM_RIGHT,
    GOMESS_LIMB_FOREARM_RIGHT,
    GOMESS_LIMB_HAND_RIGHT,
    GOMESS_LIMB_SCYTHE_HANDLE,
    GOMESS_LIMB_SCYTHE_BLADE,
    GOMESS_LIMB_MAX
} GomessLimb;

typedef enum {
    GOMESS_DMGEFF_NONE,
    GOMESS_DMGEFF_LIGHT_ARROW = 4,
    GOMESS_DMGEFF_IMMUNE = 0xE,
    GOMESS_DMGEFF_EXPLOSIVES = 0xF
} GomessDamageEffect;

typedef enum { BAT_DMGEFF_NONE, BAT_DMGEFF_STUN, BAT_DMGEFF_LIGHT_ARROW = 4, BAT_DMGEFF_IMMUNE = 0xE } BatDamageEffect;

// Orders Gomess leaves in a bat's params; the bat consumes them on its next update.
typedef enum {
    BAT_ORDER_NONE,
    BAT_ORDER_SCATTER,
    BAT_ORDER_RETURN,
    BAT_ORDER_START_SWARM,
    BAT_ORDER_SWARM,
    BAT_ORDER_DIE,
    BAT_ORDER_FLEE
} BatOrder;

typedef enum { BAT_EFFECT_FLYING, BAT_EFFECT_FALLING, BAT_EFFECT_DOWN } BatEffectState;

typedef struct {
    u8 animFrame;
    u8 state;
    Vec3f pos;
    Vec3f vel;
    Vec3s angle;
    s16 timer;
} BatEffect;

struct EnGomess;
struct EnGomessBat;

typedef void (*EnGomessActionFunc)(struct EnGomess*, PlayState*);
typedef void (*EnGomessBatActionFunc)(struct EnGomessBat*, PlayState*);

typedef struct EnGomessBat {
    Actor actor;
    EnGomessBatActionFunc actionFunc;
    u8 number;
    bool isCrowding;
    s16 timer;
    s16 spawnShapeYaw;
    s16 attackTimer;
    Vec3s moveDirection;
    f32 heightAboveParent;
    Actor* targetActor;
    BatEffect effects[BAT_EFFECT_COUNT];
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[BAT_EFFECT_COUNT];
} EnGomessBat;

typedef struct EnGomess {
    Actor actor;
    SkelAnime skelAnime;
    EnGomessActionFunc actionFunc;
    Vec3s jointTable[GOMESS_LIMB_MAX];
    Vec3s morphTable[GOMESS_LIMB_MAX];
    bool isWeaponArming;
    bool isGuardBatFalling;
    bool holdsScythe;
    bool isDeathLooping;
    bool isFightStarted;
    bool noDrawLimbs[GOMESS_LIMB_MAX];
    s8 scytheAfterimageCount;
    u8 flameAlphas[GOMESS_FLAME_COUNT];
    s16 crowdingBats;
    s16 floatTimer;
    s16 weaponColliderLastUpdateTime;
    s16 actionTimer;
    s16 cloakUpperRot;
    s16 cloakLowerRot;
    s16 guardBatRot;
    s16 explosiveDamageTimer;
    s32 effectIndex;
    f32 scytheScale;
    f32 guardBatVelocity;
    f32 lightOrbScale;
    f32 lightOrbAlpha;
    Vec3f guardBatPos;
    Vec3f scytheWorldPos;
    Vec3f scytheScreenPos;
    Vec3f bodyPartsPos[GOMESS_BODYPART_COUNT];
    Vec3f flamePositions[GOMESS_FLAME_COUNT];
    MtxF scytheMtxF;
    EnGomessBat* bats[GOMESS_BAT_COUNT];
    ColliderCylinder bodyCollider;
    ColliderQuad weaponCollider;
    ColliderJntSph coreCollider;
    ColliderJntSphElement coreElements[1];
    ColliderTris spinCollider;
    ColliderTrisElement spinElements[2];
} EnGomess;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sGomessSkel[] = OBJ_DEATH "gGomessSkel";
static const ALIGN_ASSET(2) char sFloatAnim[] = OBJ_DEATH "gGomessFloatAnim";
static const ALIGN_ASSET(2) char sScytheSwingAnim[] = OBJ_DEATH "gGomessScytheSwingAnim";
static const ALIGN_ASSET(2) char sScytheSpinAnim[] = OBJ_DEATH "gGomessScytheSpinAnim";
static const ALIGN_ASSET(2) char sDamagedAnim[] = OBJ_DEATH "gGomessDamagedAnim";
static const ALIGN_ASSET(2) char sBeginDeathAnim[] = OBJ_DEATH "gGomessBeginDeathAnim";
static const ALIGN_ASSET(2) char sDeathAnim[] = OBJ_DEATH "gGomessDeathAnim";
static const ALIGN_ASSET(2) char sBatSwarmStartAnim[] = OBJ_DEATH "gGomessBatSwarmStartAnim";
static const ALIGN_ASSET(2) char sBatSwarmAnim[] = OBJ_DEATH "gGomessBatSwarmAnim";
static const ALIGN_ASSET(2) char sScytheHandleDL[] = OBJ_DEATH "gGomessScytheHandleDL";
static const ALIGN_ASSET(2) char sScytheBladeDL[] = OBJ_DEATH "gGomessScytheBladeDL";
static const ALIGN_ASSET(2) char sCoreDL[] = OBJ_DEATH "gGomessCoreDL";
static const ALIGN_ASSET(2) char sBatsGuardingCoreDL[] = OBJ_DEATH "gGomessBatsGuardingCoreDL";
static const ALIGN_ASSET(2) char sBatMaterialDL[] = OBJ_DEATH "gGomessBatMaterialDL";
static const ALIGN_ASSET(2) char sBatFrame1DL[] = OBJ_DEATH "gGomessBatFrame1DL";
static const ALIGN_ASSET(2) char sBatFrame2DL[] = OBJ_DEATH "gGomessBatFrame2DL";
static const ALIGN_ASSET(2) char sBatFrame3DL[] = OBJ_DEATH "gGomessBatFrame3DL";
static const ALIGN_ASSET(2) char sBatFrame4DL[] = OBJ_DEATH "gGomessBatFrame4DL";
static const ALIGN_ASSET(2) char sBatFrame5DL[] = OBJ_DEATH "gGomessBatFrame5DL";
static const ALIGN_ASSET(2) char sBatFrame6DL[] = OBJ_DEATH "gGomessBatFrame6DL";
static const ALIGN_ASSET(2) char sBatFrame7DL[] = OBJ_DEATH "gGomessBatFrame7DL";
static const ALIGN_ASSET(2) char sBatFrame8DL[] = OBJ_DEATH "gGomessBatFrame8DL";
static const ALIGN_ASSET(2) char sBatFrame9DL[] = OBJ_DEATH "gGomessBatFrame9DL";

static const char* const sBatFrameDLs[BAT_ANIM_LENGTH] = {
    sBatFrame1DL, sBatFrame2DL, sBatFrame3DL, sBatFrame4DL, sBatFrame5DL,
    sBatFrame6DL, sBatFrame7DL, sBatFrame8DL, sBatFrame9DL,
};

// gGomessCoreMatAnim is empty in Ship's mm.o2r; its color keys (frames 0, 11, 23 of 24) come from 2Ship's mm.o2r.
static const u16 sCoreColorKeyFrames[3] = { 0, 11, 23 };
static const Color_RGBA8 sCorePrimKeys[3] = { { 235, 255, 125, 255 }, { 85, 155, 30, 255 }, { 235, 255, 125, 255 } };
static const Color_RGBA8 sCoreEnvKeys[3] = { { 30, 70, 0, 255 }, { 0, 0, 0, 255 }, { 30, 70, 0, 255 } };

static ColliderJntSphElementInit sCoreElementsInit[1] = {
    { { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL,
        BUMP_ON, OCELEM_NONE },
      { 1, { { 0, 0, 0 }, GOMESS_CORE_RADIUS }, 100 } },
};

static ColliderJntSphInit sCoreJntSphInit = {
    { COLTYPE_HIT3, AT_NONE, AC_NONE | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_1, COLSHAPE_JNTSPH },
    ARRAY_COUNT(sCoreElementsInit),
    sCoreElementsInit,
};

static ColliderCylinderInit sBodyCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_ON },
    { 35, 90, 20, { 0, 0, 0 } },
};

static ColliderTrisElementInit sSpinElementsInit[2] = {
    { { ELEMTYPE_UNK2, { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x20 }, { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_ON, OCELEM_NONE },
      { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } } },
    { { ELEMTYPE_UNK2, { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x20 }, { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_ON, OCELEM_NONE },
      { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } } },
};

static ColliderTrisInit sSpinTrisInit = {
    { COLTYPE_METAL, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_HARD | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_1, COLSHAPE_TRIS },
    ARRAY_COUNT(sSpinElementsInit),
    sSpinElementsInit,
};

static ColliderQuadInit sWeaponQuadInit = {
    { COLTYPE_NONE, AT_NONE | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_TYPE_2, COLSHAPE_QUAD },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x20 }, { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7, BUMP_NONE, OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static ColliderJntSphElementInit sBatElementsInit[BAT_EFFECT_COUNT] = {
    { { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x04 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_HARD, BUMP_ON,
        OCELEM_ON },
      { 1, { { 0, 0, 0 }, 15 }, 100 } },
    { { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x04 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_HARD, BUMP_ON,
        OCELEM_ON },
      { 1, { { 0, 0, 0 }, 15 }, 100 } },
    { { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x00, 0x04 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_HARD, BUMP_ON,
        OCELEM_ON },
      { 1, { { 0, 0, 0 }, 15 }, 100 } },
};

static ColliderJntSphInit sBatJntSphInit = {
    { COLTYPE_NONE, AT_NONE | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_NONE | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    ARRAY_COUNT(sBatElementsInit),
    sBatElementsInit,
};

// MM entries multiply the weapon's own damage; OoT's are absolute.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, GOMESS_DMGEFF_IMMUNE),
    /* Deku stick    */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, GOMESS_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, GOMESS_DMGEFF_EXPLOSIVES),
    /* Boomerang     */ DMG_ENTRY(1, GOMESS_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Kokiri sword  */ DMG_ENTRY(1, GOMESS_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, GOMESS_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Ice arrow     */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Light arrow   */ DMG_ENTRY(4, GOMESS_DMGEFF_LIGHT_ARROW),
    /* Unk arrow 1   */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Ice magic     */ DMG_ENTRY(0, GOMESS_DMGEFF_IMMUNE),
    /* Light magic   */ DMG_ENTRY(0, GOMESS_DMGEFF_IMMUNE),
    /* Shield        */ DMG_ENTRY(0, GOMESS_DMGEFF_IMMUNE),
    /* Mirror Ray    */ DMG_ENTRY(0, GOMESS_DMGEFF_IMMUNE),
    /* Kokiri spin   */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(4, GOMESS_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(4, GOMESS_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, GOMESS_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, GOMESS_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, GOMESS_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, GOMESS_DMGEFF_IMMUNE),
    /* Unblockable   */ DMG_ENTRY(0, GOMESS_DMGEFF_IMMUNE),
    /* Hammer jump   */ DMG_ENTRY(0, GOMESS_DMGEFF_IMMUNE),
    /* Unknown 2     */ DMG_ENTRY(0, GOMESS_DMGEFF_IMMUNE),
};

static DamageTable sBatDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, BAT_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Kokiri sword  */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Ice arrow     */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Light arrow   */ DMG_ENTRY(2, BAT_DMGEFF_LIGHT_ARROW),
    /* Unk arrow 1   */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Ice magic     */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Light magic   */ DMG_ENTRY(1, BAT_DMGEFF_LIGHT_ARROW),
    /* Shield        */ DMG_ENTRY(0, BAT_DMGEFF_IMMUNE),
    /* Mirror Ray    */ DMG_ENTRY(0, BAT_DMGEFF_IMMUNE),
    /* Kokiri spin   */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, BAT_DMGEFF_IMMUNE),
    /* Unblockable   */ DMG_ENTRY(0, BAT_DMGEFF_IMMUNE),
    /* Hammer jump   */ DMG_ENTRY(1, BAT_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, BAT_DMGEFF_IMMUNE),
};

static CollisionCheckInfoInit2 sColChkInfoInit = { GOMESS_HEALTH, 28, 90, 20, 100 };

static EffectBlureInit2 sBlureInit = {
    0, 8, 0, { 100, 50, 100, 200 }, { 100, 0, 0, 64 }, { 100, 0, 0, 20 }, { 50, 0, 0, 0 }, 8,
    0, 2, 0, { 0, 0, 0, 0 },        { 0, 0, 0, 0 },
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;
static int16_t sBatActorId = -1;
static s32 sBatItemDropTimer;
static s32 sSwarmHitCount;
static s32 sBatScatterTimer;
static bool sHasPlayedBatDeathSfx;

static void Gomess_SetupApproachPlayer(EnGomess* this);
static void Gomess_ApproachPlayer(EnGomess* this, PlayState* play);
static void Gomess_EndSwingAttack(EnGomess* this, PlayState* play);
static void Gomess_BlockProjectile(EnGomess* this, PlayState* play);
static void Gomess_SpinAttack(EnGomess* this, PlayState* play);
static void Gomess_Damaged(EnGomess* this, PlayState* play);
static void Gomess_DeathPart1(EnGomess* this, PlayState* play);
static void Gomess_DeathPart3(EnGomess* this, PlayState* play);
static void Gomess_DeathPart4(EnGomess* this, PlayState* play);
static void Gomess_Dead(EnGomess* this, PlayState* play);
static void Gomess_StartBatSwarm(EnGomess* this, PlayState* play);
static void Bat_CrowdParent(EnGomessBat* this, PlayState* play);
static void Bat_Scatter(EnGomessBat* this, PlayState* play);
static void Bat_Gather(EnGomessBat* this, PlayState* play);
static void Bat_StartSwarm(EnGomessBat* this, PlayState* play);
static void Bat_SwarmFollower(EnGomessBat* this, PlayState* play);
static void Bat_SwarmLeader(EnGomessBat* this, PlayState* play);
static void Bat_Die(EnGomessBat* this, PlayState* play);
static void Bat_Dead(EnGomessBat* this, PlayState* play);
static void Bat_SetupScatter(EnGomessBat* this);

static void FindMmSfxService(void) {
    if (!sIsMmSfxSearched && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
        sIsMmSfxSearched = true;
    }
}

// Falls back to the closest OoT sound when the mm_assets sound service is missing.
static void PlaySfx(Actor* actor, u16 mmSfx, u16 ootSfx) {
    FindMmSfxService();
    if (sPlayMmSfx != NULL && sPlayMmSfx(mmSfx, &actor->projectedPos)) {
        return;
    }
    Audio_PlayActorSound2(actor, ootSfx);
}

// The position must stay valid while the sound plays: it is projected (screen) space, like actor->projectedPos.
static void PlaySfxAt(Vec3f* projectedPos, u16 mmSfx, u16 ootSfx) {
    FindMmSfxService();
    if (sPlayMmSfx != NULL && sPlayMmSfx(mmSfx, projectedPos)) {
        return;
    }
    Audio_PlaySfxGeneral(ootSfx, projectedPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
}

static bool Gomess_IsCoreGuarded(EnGomess* this) {
    return this->crowdingBats >= GOMESS_GUARD_BAT_COUNT;
}

static void Gomess_OrderBats(EnGomess* this, s16 order) {
    for (s32 i = 0; i < GOMESS_BAT_COUNT; i++) {
        if (this->bats[i] != NULL) {
            this->bats[i]->actor.params = order;
        }
    }
}

static void Gomess_StartMinibossMusic(EnGomess* this) {
    if (this->isFightStarted) {
        return;
    }
    this->isFightStarted = true;
    func_800F5ACC(NA_BGM_MINI_BOSS);
}

static void Gomess_StopMinibossMusic(EnGomess* this) {
    if (!this->isFightStarted) {
        return;
    }
    this->isFightStarted = false;
    func_800F5B58();
}

static void Gomess_Float(EnGomess* this) {
    PlaySfx(&this->actor, MM_SE_EN_SHARP_FLOAT - SFX_FLAG, NA_SE_EN_FANTOM_FLOAT - SFX_FLAG);
    if (this->floatTimer == 0) {
        this->floatTimer = 40;
    }
    this->floatTimer--;
    this->actor.world.pos.y = this->actor.home.pos.y + (1.0f - Math_CosS(this->floatTimer * 0x666)) * 7.5f;
}

static bool Gomess_IsProjectileApproaching(EnGomess* this, PlayState* play) {
    Actor* projectile = Actor_GetProjectileActor(play, &this->actor, 80.0f);

    if (projectile == NULL) {
        return false;
    }
    s16 yawOffset = Actor_WorldYawTowardActor(&this->actor, projectile) - this->actor.shape.rot.y;

    if (ABS(yawOffset) >= 0x2000) {
        return false;
    }
    s16 pitchOffset = Actor_WorldPitchTowardPoint(projectile, &this->actor.focus.pos) - projectile->world.rot.x;

    return ABS(pitchOffset) < 0x3000;
}

static void Gomess_UpdateSpinTris(EnGomess* this) {
    f32 sinYaw = Math_SinS(this->actor.shape.rot.y);
    f32 cosYaw = Math_CosS(this->actor.shape.rot.y);
    Vec3f p1 = { this->actor.world.pos.x - 70.0f * cosYaw + 75.0f * sinYaw, this->actor.world.pos.y - 10.0f,
                 this->actor.world.pos.z + 70.0f * sinYaw + 75.0f * cosYaw };
    Vec3f p2 = { this->actor.world.pos.x + 70.0f * cosYaw + 65.0f * sinYaw, p1.y + 150.0f,
                 this->actor.world.pos.z - 70.0f * sinYaw + 65.0f * cosYaw };
    Vec3f p3 = { p1.x - 10.0f * sinYaw, p2.y, p1.z - 10.0f * cosYaw };

    Collider_SetTrisVertices(&this->spinCollider, 0, &p1, &p2, &p3);
    p3.x = p2.x + 10.0f * sinYaw;
    p3.y = p1.y;
    p3.z = p2.z + 10.0f * cosYaw;
    Collider_SetTrisVertices(&this->spinCollider, 1, &p1, &p3, &p2);
}

static void Gomess_SetupApproachPlayer(EnGomess* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sFloatAnim, 10.0f);
    this->actor.speedXZ = 1.5f;
    bool isAfterAttack = this->actionFunc == Gomess_EndSwingAttack || this->actionFunc == Gomess_SpinAttack;

    this->actionTimer = isAfterAttack ? 140 : 100;
    this->actionFunc = Gomess_ApproachPlayer;
}

static void Gomess_SwingAttack(EnGomess* this, PlayState* play);

static void Gomess_SetupSwingAttack(EnGomess* this) {
    Animation_Change(&this->skelAnime, (AnimationHeader*)sScytheSwingAnim, 1.0f, 0.0f, 10.0f, ANIMMODE_ONCE, -3.0f);
    this->actionTimer = 0;
    this->floatTimer = 0;
    this->actor.speedXZ = 8.0f;
    PlaySfx(&this->actor, MM_SE_EN_DEATH_ATTACK, NA_SE_EN_PO_LAUGH);
    this->actionFunc = Gomess_SwingAttack;
}

static void Gomess_SetupEndSwingAttack(EnGomess* this) {
    this->skelAnime.endFrame = Animation_GetLastFrame((AnimationHeader*)sScytheSwingAnim);
    this->actionTimer = 0;
    this->weaponColliderLastUpdateTime = -1;
    this->isWeaponArming = true;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Gomess_EndSwingAttack;
}

// Lunges at Link and, once in reach, swings the scythe down; MM tilts its whole body into the blow.
static void Gomess_SwingAttack(EnGomess* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x1000, 0x200);
    Math_StepToF(&this->actor.world.pos.y, this->actor.home.pos.y, 3.0f);
    s32 tilt = (s32)((100.0f - this->actor.xzDistToPlayer) * 0.01f * 0x400);

    this->actor.shape.rot.x = CLAMP_MIN(tilt, 0);
    if (this->actionTimer > 0) {
        this->actionTimer++;
        if (this->actionTimer == 3) {
            Gomess_SetupEndSwingAttack(this);
        }
        return;
    }
    if (SkelAnime_Update(&this->skelAnime) && this->actor.xzDistToPlayer < 100.0f) {
        PlaySfxAt(&this->scytheScreenPos, MM_SE_EN_DEATH_SCYTHE, NA_SE_IT_SWORD_SWING_HARD);
        this->actionTimer++;
        this->actor.speedXZ = 0.0f;
    }
}

static void Gomess_EndSwingAttack(EnGomess* this, PlayState* play) {
    this->actionTimer++;
    if (this->skelAnime.curFrame > 20.0f) {
        this->isWeaponArming = false;
        this->weaponCollider.base.atFlags &= ~AT_ON;
        Math_ScaledStepToS(&this->actor.shape.rot.x, 0, 0x200);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        this->actor.shape.rot.x = 0;
        Gomess_SetupApproachPlayer(this);
    }
}

static void Gomess_SetupBlockProjectile(EnGomess* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sScytheSpinAnim, -3.0f);
    this->weaponColliderLastUpdateTime = 31;
    this->actionTimer = 30;
    this->actor.speedXZ = 0.0f;
    Gomess_UpdateSpinTris(this);
    this->isWeaponArming = true;
    this->scytheAfterimageCount = -3;
    this->actionFunc = Gomess_BlockProjectile;
}

static void Gomess_SetupSpinAttack(EnGomess* this) {
    if (this->actionFunc == Gomess_BlockProjectile) {
        this->actionTimer = 10;
        this->weaponColliderLastUpdateTime = 9;
        this->actor.speedXZ = 10.0f;
    } else {
        Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sScytheSpinAnim, -3.0f);
        this->scytheAfterimageCount = -3;
        this->actionTimer = 0;
        this->weaponColliderLastUpdateTime = -1;
        this->isWeaponArming = true;
        this->actor.speedXZ = 0.0f;
    }
    Gomess_UpdateSpinTris(this);
    this->actionFunc = Gomess_SpinAttack;
}

static void Gomess_SetupStartBatSwarm(EnGomess* this) {
    PlaySfx(&this->actor, MM_SE_EN_DEATH_VOICE, NA_SE_EN_PO_LAUGH);
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sBatSwarmStartAnim, 5.0f);
    this->actor.speedXZ = 0.0f;
    Gomess_OrderBats(this, BAT_ORDER_START_SWARM);
    this->floatTimer = 0;
    this->actionFunc = Gomess_StartBatSwarm;
}

// Drifts toward Link, blocks shots with a spin while guarded, then swings, spins across the room or sends the swarm.
static void Gomess_ApproachPlayer(EnGomess* this, PlayState* play) {
    Gomess_Float(this);
    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 8, 0x1000, 0x100);
    if (this->actionTimer > 0) {
        this->actionTimer--;
    }
    bool isGuarded = Gomess_IsCoreGuarded(this);

    if (!isGuarded && Rand_ZeroOne() < 0.4f && Gomess_IsProjectileApproaching(this, play)) {
        Gomess_SetupBlockProjectile(this);
    } else if (this->actionTimer < 100 && this->actor.xzDistToPlayer < 200.0f) {
        Gomess_SetupSwingAttack(this);
    } else if (this->actionTimer == 0 && isGuarded) {
        Gomess_SetupStartBatSwarm(this);
    } else if (this->actionTimer == 0) {
        Gomess_SetupSpinAttack(this);
    }
}

static void Gomess_BlockProjectile(EnGomess* this, PlayState* play) {
    Gomess_Float(this);
    this->actionTimer--;
    if (this->scytheAfterimageCount < 7) {
        this->scytheAfterimageCount++;
    }
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
        PlaySfxAt(&this->scytheScreenPos, MM_SE_EN_DEATH_ROLL, NA_SE_EN_PO_ROLL);
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 8, 0x1000, 0x100);
    Gomess_UpdateSpinTris(this);
    if (this->actionTimer != 0) {
        return;
    }
    if (this->actor.xzDistToPlayer > 200.0f) {
        Gomess_SetupSpinAttack(this);
    } else {
        Gomess_SetupApproachPlayer(this);
    }
}

// Spins the scythe in front of it and charges in a straight line until it hits Link, a shield or a wall.
static void Gomess_SpinAttack(EnGomess* this, PlayState* play) {
    Gomess_Float(this);
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
        PlaySfxAt(&this->scytheScreenPos, MM_SE_EN_DEATH_ROLL, NA_SE_EN_PO_ROLL);
    }
    if (this->scytheAfterimageCount < 7) {
        this->scytheAfterimageCount++;
    }
    this->actionTimer++;
    if (this->actionTimer < 10) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 8, 0x1000, 0x100);
    } else if (this->actionTimer == 10) {
        this->actor.speedXZ = 10.0f;
    }
    Gomess_UpdateSpinTris(this);
    Math_ScaledStepToS(&this->cloakUpperRot, (s32)(Math_SinS(this->actionTimer * 0x2000) * 0x800) + 0x3000, 0x1000);
    Math_ScaledStepToS(&this->cloakLowerRot, (s32)(Math_SinS(this->actionTimer * 0x2000 - 0x8000) * 0x800), 0x1000);
    bool isStopped = (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) || (this->weaponCollider.base.atFlags & AT_HIT) ||
                     (this->spinCollider.base.atFlags & AT_HIT);

    if (!isStopped) {
        return;
    }
    this->isWeaponArming = false;
    this->weaponCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    this->spinCollider.base.atFlags &= ~AT_HIT;
    Gomess_SetupApproachPlayer(this);
}

static void Gomess_SetupDamaged(EnGomess* this) {
    Actor* attacker = this->coreCollider.base.ac;

    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sDamagedAnim);
    this->actor.speedXZ = 10.0f;
    this->actor.world.rot.y = attacker != NULL ? Actor_WorldYawTowardActor(attacker, &this->actor)
                                               : BINANG_ROT180(this->actor.yawTowardsPlayer);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 15);
    PlaySfx(&this->actor, MM_SE_EN_DEATH_DAMAGE, NA_SE_EN_PO_DAMAGE);
    this->actionFunc = Gomess_Damaged;
}

static void Gomess_Damaged(EnGomess* this, PlayState* play) {
    Gomess_Float(this);
    Math_StepToF(&this->actor.speedXZ, 0.0f, 0.5f);
    if (SkelAnime_Update(&this->skelAnime)) {
        this->coreCollider.base.acFlags |= AC_ON;
        Gomess_SetupSpinAttack(this);
    }
}

static void Gomess_SetupDeath(EnGomess* this) {
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sBeginDeathAnim);
    Gomess_OrderBats(this, BAT_ORDER_FLEE);
    this->isGuardBatFalling = false;
    this->isDeathLooping = false;
    this->coreCollider.base.acFlags &= ~AC_ON;
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 30);
    this->actionTimer = 35;
    this->actor.world.pos.y = this->actor.home.pos.y;
    this->actor.speedXZ = 0.0f;
    this->actor.shape.rot.y += 0x2000;
    PlaySfx(&this->actor, MM_SE_EN_DEATH_DEAD, NA_SE_EN_PO_DEAD);
    this->actionFunc = Gomess_DeathPart1;
}

static void Gomess_DeathPart2(EnGomess* this, PlayState* play);

static void Gomess_SetupDeathPart2(EnGomess* this) {
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sDeathAnim);
    this->isDeathLooping = true;
    this->actionTimer = 30;
    this->actor.world.pos.y = this->actor.home.pos.y + 15.0f;
    this->actionFunc = Gomess_DeathPart2;
}

// Whirls while it loses its grip on the scythe; MM holds this pose until its camera has settled.
static void Gomess_DeathPart1(EnGomess* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        this->actor.shape.rot.y += 0x2000;
        Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sDeathAnim);
        this->isDeathLooping = true;
    } else if (!this->isDeathLooping) {
        this->actor.shape.rot.y += 0x2000;
        if (this->skelAnime.curFrame > 3.0f) {
            Matrix_Put(&this->scytheMtxF);
            Matrix_RotateX(BINANG_TO_RAD(-0x1000), MTXMODE_APPLY);
            Matrix_Get(&this->scytheMtxF);
            this->scytheMtxF.yw += 18.0f;
            if (this->holdsScythe) {
                PlaySfxAt(&this->scytheScreenPos, MM_SE_EN_DEATH_SCYTHE, NA_SE_IT_SWORD_SWING_HARD);
            }
            this->holdsScythe = false;
        }
    }
    if (this->actionTimer == 35) {
        PlaySfxAt(&this->scytheScreenPos, MM_SE_EN_DEATH_SCYTHE_LEV, NA_SE_EN_FANTOM_FLOAT);
    }
    this->actionTimer--;
    if (this->actionTimer == 0) {
        Gomess_SetupDeathPart2(this);
    }
}

// The dropped scythe falls beside it and sticks into the ground.
static void Gomess_DeathPart2(EnGomess* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->actionTimer--;
    if (this->actionTimer >= 0 && this->actionTimer < 3) {
        s16 camYaw = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000;
        f32 sinYaw = Math_SinS(camYaw);
        f32 cosYaw = Math_CosS(camYaw);

        Matrix_Translate(this->actor.world.pos.x + (83.0f * sinYaw) + (-38.0f * cosYaw),
                         this->actor.world.pos.y + 53.0f + 15.0f * this->actionTimer,
                         this->actor.world.pos.z + (83.0f * cosYaw) - (-38.0f * sinYaw), MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD((s16)(camYaw - 0x3300)), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD((s16)(0x1100 - this->actionTimer * 0x1800)), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(-0xA00), MTXMODE_APPLY);
        Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
        Matrix_Get(&this->scytheMtxF);
        if (this->actionTimer == 0) {
            Camera_AddQuake(GET_ACTIVE_CAM(play), 2, 4, 6);
            Rumble_Request(this->actor.xyzDistToPlayerSq, 180, 20, 100);
            Audio_StopSfxByPos(&this->scytheScreenPos);
            PlaySfxAt(&this->scytheScreenPos, MM_SE_EN_DEATH_SCYTHE_ONGND, NA_SE_EV_STONE_BOUND);
        }
    }
    if (this->actionTimer < -25) {
        Gomess_OrderBats(this, BAT_ORDER_DIE);
        this->actionTimer = 0;
        this->actionFunc = Gomess_DeathPart3;
    }
}

static void Gomess_SpawnDissolveFlames(EnGomess* this, PlayState* play) {
    static Vec3f sSparkleVel = { 0.0f, -1.5f, 0.0f };
    static Vec3f sSparkleAccel = { 0.0f, -0.2f, 0.0f };
    static Color_RGBA8 sSparklePrimColor = { 0, 0, 0, 0 };
    static Color_RGBA8 sSparkleEnvColor = { 40, 40, 40, 0 };
    s32 first = (this->actionTimer * 7) % GOMESS_FLAME_COUNT;
    s16 camYaw = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000;
    f32 sinYaw = Math_SinS(camYaw);
    f32 cosYaw = Math_CosS(camYaw);

    for (s32 j = 0; j < 7; j++) {
        Vec3f* flamePos = &this->flamePositions[first + j];
        f32 side = Rand_CenteredFloat(100.0f) - 20.0f;

        flamePos->x = this->actor.world.pos.x + 30.0f * sinYaw + side * cosYaw;
        flamePos->y = Rand_ZeroFloat(20.0f) + this->actor.world.pos.y + 4.0f * this->actionTimer - 30.0f;
        flamePos->z = this->actor.world.pos.z + 30.0f * cosYaw - side * sinYaw;
        this->flameAlphas[first + j] = 255 - j * 4;
        EffectSsKiraKira_SpawnSmall(play, flamePos, &sSparkleVel, &sSparkleAccel, &sSparklePrimColor,
                                    &sSparkleEnvColor);
    }
    PlaySfx(&this->actor, MM_SE_EN_COMMON_EXTINCT_LEV - SFX_FLAG, NA_SE_EN_EXTINCT);
}

// Burns away from the bottom up in green flames, limb by limb.
static void Gomess_DeathPart3(EnGomess* this, PlayState* play) {
    static const s16 sLimbVanishFrames[GOMESS_LIMB_MAX] = {
        30, 30, 10, 2, 12, 17, 17, 17, 17, 17, 17, 17, 14, 10, 12, 9, 9, 12, 9, 9, 30, 30,
    };

    for (s32 i = 0; i < GOMESS_FLAME_COUNT; i++) {
        this->flamePositions[i].y += 1.7f;
        this->flameAlphas[i] = CLAMP_MIN(this->flameAlphas[i] - 30, 0);
    }
    this->actor.shape.shadowAlpha = CLAMP_MIN(252 - this->actionTimer * 14, 0);
    if (this->actionTimer < 18) {
        Gomess_SpawnDissolveFlames(this, play);
    }
    this->actionTimer++;
    for (s32 i = 0; i < GOMESS_LIMB_MAX; i++) {
        if (this->actionTimer == sLimbVanishFrames[i]) {
            this->noDrawLimbs[i] = true;
        }
    }
    if (this->actionTimer != 26) {
        return;
    }
    for (s32 i = 0; i < GOMESS_FLAME_COUNT; i++) {
        this->flameAlphas[i] = 0;
    }
    this->actor.world.pos = this->actor.focus.pos;
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    this->actionTimer = 0;
    this->actor.gravity = -1.0f;
    this->actor.shape.yOffset = 0.0f;
    this->actionFunc = Gomess_DeathPart4;
}

// The bare core drops to the floor and shatters.
static void Gomess_DeathPart4(EnGomess* this, PlayState* play) {
    static Vec3f sSparkleAccel = { 0.0f, -0.2f, 0.0f };
    static Color_RGBA8 sSparklePrimColor = { 0, 200, 100, 0 };
    static Color_RGBA8 sSparkleEnvColor = { 0, 100, 0, 0 };

    if (this->actionTimer != 0) {
        this->actionTimer++;
        if (this->actionTimer == 20) {
            this->actionTimer = 255;
            this->actionFunc = Gomess_Dead;
        }
        return;
    }
    bool hasLanded = (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || this->actor.floorHeight == BGCHECK_Y_MIN;

    if (!hasLanded) {
        return;
    }
    Actor_SetScale(&this->actor, 0.0f);
    this->actionTimer++;
    for (s32 i = 0; i < 40; i++) {
        Vec3f sparkleVel = { Rand_CenteredFloat(4.0f), Rand_ZeroFloat(2.0f) + 3.0f, Rand_CenteredFloat(4.0f) };

        EffectSsKiraKira_SpawnSmall(play, &this->actor.world.pos, &sparkleVel, &sSparkleAccel, &sSparklePrimColor,
                                    &sSparkleEnvColor);
    }
    PlaySfx(&this->actor, MM_SE_EN_DEATH_HEARTBREAK, NA_SE_EN_PO_SISTER_DEAD);
}

static void Gomess_Dead(EnGomess* this, PlayState* play) {
    this->actionTimer -= 5;
    if (this->actionTimer > 0) {
        return;
    }
    this->actionTimer = 0;
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

static void Gomess_BatSwarm(EnGomess* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 8, 0x1000, 0x100);
    SkelAnime_Update(&this->skelAnime);
    if (Gomess_IsCoreGuarded(this)) {
        Gomess_SetupApproachPlayer(this);
    }
}

static void Gomess_StartBatSwarm(EnGomess* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 8, 0x1000, 0x100);
    Math_StepToF(&this->actor.world.pos.y, this->actor.home.pos.y, 5.0f);
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sBatSwarmAnim);
    Gomess_OrderBats(this, BAT_ORDER_SWARM);
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Gomess_BatSwarm;
}

static void Gomess_BeginFight(EnGomess* this, PlayState* play) {
    if (Play_InCsMode(play)) {
        return;
    }
    this->coreCollider.base.acFlags |= AC_ON;
    Gomess_StartMinibossMusic(this);
    Gomess_SetupApproachPlayer(this);
}

static void Gomess_UpdateGuardBat(EnGomess* this, PlayState* play) {
    if (!this->isGuardBatFalling) {
        return;
    }
    this->guardBatPos.y += this->guardBatVelocity;
    this->guardBatVelocity -= 1.0f;
    this->guardBatRot += 0x1800;
    if (this->guardBatPos.y >= this->actor.floorHeight) {
        return;
    }
    this->guardBatPos.y = this->actor.floorHeight;
    EffectSsDeadDb_Spawn(play, &this->guardBatPos, &sZeroVec, &sZeroVec, 100, 0, 255, 255, 255, 255, 255, 0, 0, 1, 9,
                         false);
    SoundSource_PlaySfxAtFixedWorldPos(play, &this->guardBatPos, 11, NA_SE_EN_EXTINCT);
    this->isGuardBatFalling = false;
}

// A guarded core costs a guarding bat instead; a light arrow scatters the whole swarm.
static void Gomess_HitGuardedCore(EnGomess* this, PlayState* play) {
    u8 damageEffect = this->actor.colChkInfo.damageEffect;

    this->isGuardBatFalling = true;
    this->guardBatVelocity = -1.0f;
    this->guardBatRot = this->actor.shape.rot.y;
    this->guardBatPos.x = this->coreCollider.elements[0].dim.worldSphere.center.x;
    this->guardBatPos.y = this->coreCollider.elements[0].dim.worldSphere.center.y;
    this->guardBatPos.z = this->coreCollider.elements[0].dim.worldSphere.center.z;
    SoundSource_PlaySfxAtFixedWorldPos(play, &this->guardBatPos, 30, NA_SE_EN_FFLY_DEAD);
    if (damageEffect == GOMESS_DMGEFF_LIGHT_ARROW && this->actionFunc != Gomess_StartBatSwarm) {
        Gomess_OrderBats(this, BAT_ORDER_SCATTER);
        if (this->actionFunc == Gomess_ApproachPlayer) {
            this->actionTimer = 100;
        }
    } else if (damageEffect == GOMESS_DMGEFF_EXPLOSIVES) {
        this->explosiveDamageTimer = 10;
    }
}

static void Gomess_HitCore(EnGomess* this, PlayState* play) {
    this->isWeaponArming = false;
    this->weaponCollider.base.atFlags &= ~AT_ON;
    this->coreCollider.base.acFlags &= ~AC_ON;
    if (this->actor.colChkInfo.damageEffect == GOMESS_DMGEFF_LIGHT_ARROW) {
        this->lightOrbAlpha = 3.0f;
        this->lightOrbScale = 0.8f;
    }
    this->actor.shape.rot.x = 0;
    if (Actor_ApplyDamage(&this->actor) == 0) {
        Enemy_StartFinishingBlow(play, &this->actor);
        Gomess_StopMinibossMusic(this);
        Gomess_SetupDeath(this);
        return;
    }
    Gomess_OrderBats(this, BAT_ORDER_RETURN);
    Gomess_SetupDamaged(this);
}

static void Gomess_UpdateDamage(EnGomess* this, PlayState* play) {
    if (this->explosiveDamageTimer > 0) {
        this->explosiveDamageTimer--;
    }
    if (!(this->coreCollider.base.acFlags & AC_HIT)) {
        return;
    }
    this->coreCollider.base.acFlags &= ~AC_HIT;
    u8 damageEffect = this->actor.colChkInfo.damageEffect;

    if (damageEffect == GOMESS_DMGEFF_IMMUNE) {
        return;
    }
    if (Gomess_IsCoreGuarded(this)) {
        Gomess_HitGuardedCore(this, play);
    } else if (damageEffect != GOMESS_DMGEFF_EXPLOSIVES || this->explosiveDamageTimer == 0) {
        Gomess_HitCore(this, play);
    }
}

static void Gomess_SubmitColliders(EnGomess* this, PlayState* play) {
    if (this->actionFunc != Gomess_Dead) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    if (this->coreCollider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->coreCollider.base);
    }
    if (this->weaponCollider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->weaponCollider.base);
    }
    if (this->actionFunc == Gomess_BlockProjectile || this->actionFunc == Gomess_SpinAttack) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->spinCollider.base);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->spinCollider.base);
    }
}

static void Bat_RandomizeEffects(EnGomessBat* this) {
    for (s32 i = 0; i < BAT_EFFECT_COUNT; i++) {
        BatEffect* effect = &this->effects[i];

        effect->vel.x = Rand_CenteredFloat(80.0f);
        effect->vel.y = Rand_ZeroFloat(40.0f);
        effect->vel.z = Rand_CenteredFloat(80.0f);
        effect->animFrame = (s32)Rand_ZeroFloat(9.0f) % BAT_ANIM_LENGTH;
        effect->angle.x = (s32)Rand_Next() >> 17;
        effect->angle.y = (s32)Rand_Next() >> 16;
        effect->state = BAT_EFFECT_FLYING;
        effect->timer = 0;
    }
}

// Spawns the 20 bat actors crowding the core; each carries three of the 60 bats.
static bool Gomess_SpawnBats(EnGomess* this, PlayState* play) {
    f32 yOffset = 15.0f;
    s16 yaw = 0;

    for (s32 i = 0; i < GOMESS_BAT_COUNT; i++) {
        EnGomessBat* bat =
            (EnGomessBat*)Actor_Spawn(&play->actorCtx, play, sBatActorId, this->actor.world.pos.x,
                                      this->actor.world.pos.y + yOffset, this->actor.world.pos.z, 0, yaw, 0, i);

        if (bat == NULL) {
            return false;
        }
        // OoT runs Init inside Actor_Spawn, before a parent could be set, so the bat is attached here.
        bat->actor.parent = &this->actor;
        bat->heightAboveParent = bat->actor.world.pos.y - this->actor.world.pos.y;
        bat->actor.shape.rot.y = this->actor.shape.rot.y;
        bat->actor.world.rot.y = this->actor.shape.rot.y;
        bat->collider.base.atFlags |= AT_ON;
        bat->isCrowding = true;
        Bat_CrowdParent(bat, play);
        for (s32 j = 0; j < BAT_EFFECT_COUNT; j++) {
            Math_Vec3f_Sum(&bat->actor.world.pos, &bat->effects[j].vel, &bat->effects[j].pos);
        }
        this->bats[i] = bat;
        yaw += 0x3A00;
        yOffset += 4.0f;
    }
    for (s32 i = 0; i < GOMESS_BAT_COUNT - 1; i++) {
        this->bats[i]->actor.child = &this->bats[i + 1]->actor;
    }
    return true;
}

static void EnGomess_Init(Actor* thisx, PlayState* play) {
    EnGomess* this = (EnGomess*)thisx;

    ActorShape_Init(&this->actor.shape, 5500.0f, ActorShadow_DrawCircle, 80.0f);
    this->actor.targetArrowOffset = 6000.0f;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sGomessSkel, (AnimationHeader*)sFloatAnim,
                       this->jointTable, this->morphTable, GOMESS_LIMB_MAX);
    Collider_InitJntSph(play, &this->coreCollider);
    Collider_SetJntSph(play, &this->coreCollider, &this->actor, &sCoreJntSphInit, this->coreElements);
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyCylinderInit);
    Collider_InitQuad(play, &this->weaponCollider);
    Collider_SetQuad(play, &this->weaponCollider, &this->actor, &sWeaponQuadInit);
    Collider_InitTris(play, &this->spinCollider);
    Collider_SetTris(play, &this->spinCollider, &this->actor, &sSpinTrisInit, this->spinElements);
    this->coreCollider.elements[0].dim.worldSphere.radius = GOMESS_CORE_RADIUS;
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    Effect_Add(play, &this->effectIndex, EFFECT_BLURE2, 0, 0, &sBlureInit);
    Actor_SetScale(&this->actor, GOMESS_SCALE);
    this->holdsScythe = true;
    this->scytheScale = 1.0f;
    this->crowdingBats = GOMESS_BAT_COUNT;
    this->actionFunc = Gomess_BeginFight;
    if (!Gomess_SpawnBats(this, play)) {
        Actor_Kill(&this->actor);
    }
}

static void EnGomess_Destroy(Actor* thisx, PlayState* play) {
    EnGomess* this = (EnGomess*)thisx;

    for (s32 i = 0; i < GOMESS_BAT_COUNT; i++) {
        if (this->bats[i] != NULL) {
            this->bats[i]->actor.parent = NULL;
            Actor_Kill(&this->bats[i]->actor);
        }
    }
    Gomess_StopMinibossMusic(this);
    Collider_DestroyJntSph(play, &this->coreCollider);
    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyQuad(play, &this->weaponCollider);
    Collider_DestroyTris(play, &this->spinCollider);
    Audio_StopSfxByPos(&this->scytheScreenPos);
    Effect_Delete(play, this->effectIndex);
}

static void EnGomess_Update(Actor* thisx, PlayState* play) {
    EnGomess* this = (EnGomess*)thisx;

    Gomess_UpdateDamage(this, play);
    Gomess_UpdateGuardBat(this, play);
    if (this->actionFunc != Gomess_SpinAttack) {
        Math_ScaledStepToS(&this->cloakUpperRot, 0, 0x800);
        Math_ScaledStepToS(&this->cloakLowerRot, 0, 0x800);
    }
    this->actionFunc(this, play);
    if (this->actionFunc != Gomess_Damaged) {
        this->actor.world.rot.y = this->actor.shape.rot.y;
    }
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 40.0f, this->actionFunc == Gomess_SpinAttack ? 50.0f : 100.0f, 40.0f,
                            BGCHECK_UPDATE_WALL_CEILING_FLOOR);
    this->bodyCollider.dim.pos.x = this->actor.world.pos.x + Math_SinS(this->actor.shape.rot.y) * 3.0f;
    this->bodyCollider.dim.pos.y = this->actor.world.pos.y;
    this->bodyCollider.dim.pos.z = this->actor.world.pos.z + Math_CosS(this->actor.shape.rot.y) * 3.0f;
    this->coreCollider.elements[0].dim.worldSphere.radius =
        Gomess_IsCoreGuarded(this) ? GOMESS_CORE_RADIUS : GOMESS_CORE_RADIUS + 5;
    Gomess_SubmitColliders(this, play);
    if (this->lightOrbAlpha > 0.0f) {
        Math_StepToF(&this->lightOrbAlpha, 0.0f, 0.05f);
        this->lightOrbScale = CLAMP_MAX((this->lightOrbAlpha + 1.0f) * 0.4f, 0.8f);
    }
}

static Gfx* Gomess_MakeColorDL(PlayState* play, u8 lodFrac, Color_RGBA8 prim, const Color_RGBA8* env) {
    Gfx* colorDL = Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Gfx));
    Gfx* cursor = colorDL;

    gDPSetPrimColor(cursor++, 0, lodFrac, prim.r, prim.g, prim.b, prim.a);
    if (env != NULL) {
        gDPSetEnvColor(cursor++, env->r, env->g, env->b, env->a);
    }
    gSPEndDisplayList(cursor++);
    return colorDL;
}

static u8 LerpChannel(u8 from, u8 to, f32 t) {
    return (s32)((to - from) * t) + from;
}

// MM's ColorChangeLERP over three keys, looping every 24 frames.
static Gfx* Gomess_MakeCoreColorDL(PlayState* play) {
    s32 frame = play->gameplayFrames % GOMESS_CORE_COLOR_PERIOD;
    s32 key = frame < sCoreColorKeyFrames[1] ? 1 : 2;
    f32 t =
        (f32)(frame - sCoreColorKeyFrames[key - 1]) / (f32)(sCoreColorKeyFrames[key] - sCoreColorKeyFrames[key - 1]);
    const Color_RGBA8* primFrom = &sCorePrimKeys[key - 1];
    const Color_RGBA8* primTo = &sCorePrimKeys[key];
    const Color_RGBA8* envFrom = &sCoreEnvKeys[key - 1];
    const Color_RGBA8* envTo = &sCoreEnvKeys[key];
    Color_RGBA8 prim = { LerpChannel(primFrom->r, primTo->r, t), LerpChannel(primFrom->g, primTo->g, t),
                         LerpChannel(primFrom->b, primTo->b, t), 255 };
    Color_RGBA8 env = { LerpChannel(envFrom->r, envTo->r, t), LerpChannel(envFrom->g, envTo->g, t),
                        LerpChannel(envFrom->b, envTo->b, t), 255 };

    return Gomess_MakeColorDL(play, 127, prim, &env);
}

// gGomessBodyMatAnim and gGomessCoreMatAnim are empty in Ship's mm.o2r: segments 08-0B are rebuilt by hand.
static void Gomess_SetMaterialSegments(PlayState* play) {
    static const Color_RGBA8 sBodyPrimColor = { 255, 255, 255, 255 };
    u32 frames = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)Gomess_MakeColorDL(play, 255, sBodyPrimColor, NULL));
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)Gomess_MakeCoreColorDL(play));
    gSPSegment(POLY_OPA_DISP++, 0x0A,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, -frames, -frames, 16, 16, 1, frames * 3, frames * 3,
                                           16, 16));
    gSPSegment(POLY_OPA_DISP++, 0x0B,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, frames, frames, 16, 16, 1, 0, 0, 16, 16));
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Gomess_DrawCore(EnGomess* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sCoreDL);
    if (Gomess_IsCoreGuarded(this)) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBatsGuardingCoreDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 Gomess_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnGomess* this = (EnGomess*)thisx;

    if (this->noDrawLimbs[limbIndex]) {
        *dList = NULL;
    } else if (limbIndex == GOMESS_LIMB_CLOAK_LOWER && this->actionFunc == Gomess_DeathPart3 && this->actionTimer > 5) {
        f32 scale = 1.0f - 0.1f * (this->actionTimer - 5);

        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    }
    if (limbIndex == GOMESS_LIMB_SCYTHE_HANDLE || limbIndex == GOMESS_LIMB_SCYTHE_BLADE) {
        if (!this->holdsScythe) {
            *dList = NULL;
        }
    } else if (limbIndex == GOMESS_LIMB_CLOAK_UPPER) {
        rot->z += this->cloakUpperRot;
    } else if (limbIndex == GOMESS_LIMB_CLOAK_LOWER) {
        rot->z += this->cloakLowerRot;
    }
    return false;
}

// Stretches the swing quad from last frame's blade edge to this frame's and feeds the blur trail.
static void Gomess_TrackBlade(EnGomess* this) {
    static Vec3f sSwingEndOffset = { -1000.0f, 12000.0f, 0.0f };
    static Vec3f sSpinBaseOffset = { -2000.0f, -2500.0f, 0.0f };
    static Vec3f sSwingTipOffset = { 6000.0f, 0.0f, 0.0f };
    static Vec3f sSpinTipOffset = { 0.0f, 5000.0f, 0.0f };
    bool isSwinging = this->actionFunc == Gomess_EndSwingAttack;
    Vec3f edgeA;
    Vec3f edgeB;

    Matrix_MultVec3f(&sZeroVec, &this->scytheWorldPos);
    if (isSwinging) {
        Matrix_MultVec3f(&sSwingTipOffset, &edgeA);
        Matrix_MultVec3f(&sSwingEndOffset, &edgeB);
    } else {
        Matrix_MultVec3f(&sSpinBaseOffset, &edgeA);
        Matrix_MultVec3f(&sSpinTipOffset, &edgeB);
    }
    if ((this->weaponCollider.base.atFlags & AT_ON) && this->weaponColliderLastUpdateTime != this->actionTimer) {
        Vec3f prevEdge0 = this->weaponCollider.dim.quad[0];
        Vec3f prevEdge1 = this->weaponCollider.dim.quad[1];

        Collider_SetQuadVertices(&this->weaponCollider, &edgeB, &edgeA, &prevEdge0, &prevEdge1);
        EffectBlure_AddVertex(Effect_GetByIndex(this->effectIndex), &edgeA, &edgeB);
        this->weaponColliderLastUpdateTime = this->actionTimer;
    } else if (this->isWeaponArming) {
        this->weaponCollider.dim.quad[0] = edgeB;
        this->weaponCollider.dim.quad[1] = edgeA;
        this->isWeaponArming = false;
        this->weaponCollider.base.atFlags |= AT_ON;
        this->weaponColliderLastUpdateTime = this->actionTimer;
    }
}

static void Gomess_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sLightOrbOffsets[5] = {
        { 1000.0f, 2000.0f, 0.0f },    { 1500.0f, 1000.0f, 4000.0f },  { 1500.0f, 1000.0f, -4000.0f },
        { 4000.0f, 4000.0f, 2000.0f }, { 4000.0f, 4000.0f, -2000.0f },
    };
    static Vec3f sHeadTopOffset = { 0.0f, -2000.0f, 0.0f };
    static const s8 sLimbToBodyPart[GOMESS_LIMB_MAX] = {
        -1, -1, -1, 12, -1, 0, -1, -1, -1, -1, -1, -1, -1, 7, 1, 2, 3, 4, 5, 6, -1, -1,
    };
    EnGomess* this = (EnGomess*)thisx;

    if (limbIndex == GOMESS_LIMB_SCYTHE_BLADE) {
        Gomess_TrackBlade(this);
    } else if (limbIndex == GOMESS_LIMB_CORE_POS && this->actionFunc != Gomess_Dead) {
        Matrix_Push();
        Gomess_DrawCore(this, play);
        Matrix_Pop();
        Matrix_MultVec3f(&sZeroVec, &this->actor.focus.pos);
        this->coreCollider.elements[0].dim.worldSphere.center.x = this->actor.focus.pos.x;
        this->coreCollider.elements[0].dim.worldSphere.center.y = this->actor.focus.pos.y;
        this->coreCollider.elements[0].dim.worldSphere.center.z = this->actor.focus.pos.z;
        if (!Gomess_IsCoreGuarded(this)) {
            this->coreCollider.elements[0].dim.worldSphere.center.y += 5;
        }
    } else if (limbIndex == GOMESS_LIMB_SCYTHE_HANDLE) {
        bool isSpinning = this->actionFunc == Gomess_SpinAttack || this->actionFunc == Gomess_BlockProjectile;
        bool isLosingScythe = this->actionFunc == Gomess_DeathPart1 && this->holdsScythe;

        if (isSpinning || isLosingScythe) {
            Matrix_Get(&this->scytheMtxF);
        }
    }
    s8 bodyPart = sLimbToBodyPart[limbIndex];

    if (bodyPart < 0) {
        return;
    }
    if (bodyPart < 7) {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[bodyPart]);
    } else if (bodyPart == 7) {
        for (s32 i = 0; i < ARRAY_COUNT(sLightOrbOffsets); i++) {
            Matrix_MultVec3f(&sLightOrbOffsets[i], &this->bodyPartsPos[bodyPart + i]);
        }
    } else {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[bodyPart]);
        Matrix_MultVec3f(&sHeadTopOffset, &this->bodyPartsPos[bodyPart + 1]);
    }
}

static void Gomess_DrawScytheAt(PlayState* play, Gfx** gfx, MtxF* scytheMtxF, f32 scale) {
    Matrix_Put(scytheMtxF);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix((*gfx)++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList((*gfx)++, (Gfx*)sScytheHandleDL);
    Matrix_Translate(0.0f, -1084.0f, 7012.0f, MTXMODE_APPLY);
    Matrix_RotateZYX(-0x4000, 0, -0x4000, MTXMODE_APPLY);
    gSPMatrix((*gfx)++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList((*gfx)++, (Gfx*)sScytheBladeDL);
}

static void Gomess_DrawSpinAfterimages(EnGomess* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    for (s32 i = 1; i < this->scytheAfterimageCount; i++) {
        MtxF afterimageMtxF;

        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetEnvColor(POLY_XLU_DISP++, 30, 30, 0, 255 - i * 35);
        Matrix_Put(&this->scytheMtxF);
        Matrix_RotateX(BINANG_TO_RAD((s16)(i * 0x2100)), MTXMODE_APPLY);
        Matrix_Get(&afterimageMtxF);
        Gomess_DrawScytheAt(play, &POLY_XLU_DISP, &afterimageMtxF, 1.0f);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Gomess_DrawDroppedScythe(EnGomess* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    if (this->actionFunc == Gomess_Dead) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetEnvColor(POLY_XLU_DISP++, 30, 30, 0, this->actionTimer);
        Gomess_DrawScytheAt(play, &POLY_XLU_DISP, &this->scytheMtxF, this->scytheScale);
    } else {
        Gomess_DrawScytheAt(play, &POLY_OPA_DISP, &this->scytheMtxF, this->scytheScale);
    }
    Matrix_MultVec3f(&sZeroVec, &this->scytheWorldPos);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Gomess_DrawBatAt(PlayState* play, MtxF* batMtxF, Vec3f* pos, u8 animFrame) {
    OPEN_DISPS(play->state.gfxCtx);
    batMtxF->xw = pos->x;
    batMtxF->yw = pos->y;
    batMtxF->zw = pos->z;
    Matrix_Put(batMtxF);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBatFrameDLs[animFrame]);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Flying bats share their actor's heading; falling ones tumble on their own.
static void Gomess_DrawBats(EnGomess* this, PlayState* play) {
    f32 flyingScale = BAT_SCALE;
    MtxF batMtxF;

    if (this->actionFunc == Gomess_Dead) {
        flyingScale = CLAMP_MAX(this->actionTimer * 0.000035f, BAT_SCALE);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBatMaterialDL);
    CLOSE_DISPS(play->state.gfxCtx);
    for (s32 i = 0; i < GOMESS_BAT_COUNT; i++) {
        EnGomessBat* bat = this->bats[i];

        if (bat == NULL) {
            continue;
        }
        Matrix_RotateZYX(bat->actor.shape.rot.x, bat->actor.shape.rot.y, 0, MTXMODE_NEW);
        Matrix_Scale(flyingScale, flyingScale, flyingScale, MTXMODE_APPLY);
        Matrix_Get(&batMtxF);
        for (s32 j = 0; j < BAT_EFFECT_COUNT; j++) {
            if (bat->effects[j].state == BAT_EFFECT_FLYING) {
                Gomess_DrawBatAt(play, &batMtxF, &bat->effects[j].pos, bat->effects[j].animFrame);
            }
        }
        for (s32 j = 0; j < BAT_EFFECT_COUNT; j++) {
            BatEffect* effect = &bat->effects[j];

            if (effect->state != BAT_EFFECT_FALLING) {
                continue;
            }
            Matrix_RotateZYX(0x4000, effect->angle.y, 0, MTXMODE_NEW);
            Matrix_Scale(BAT_SCALE, BAT_SCALE, BAT_SCALE, MTXMODE_APPLY);
            Matrix_Get(&batMtxF);
            Gomess_DrawBatAt(play, &batMtxF, &effect->pos, effect->animFrame);
        }
    }
    if (this->isGuardBatFalling) {
        Matrix_RotateZYX(0x4000, this->guardBatRot, 0, MTXMODE_NEW);
        Matrix_Scale(BAT_SCALE, BAT_SCALE, BAT_SCALE, MTXMODE_APPLY);
        Matrix_Get(&batMtxF);
        Gomess_DrawBatAt(play, &batMtxF, &this->guardBatPos, 0);
    }
}

static void Gomess_DrawFlameAt(PlayState* play, MtxF* flameMtxF, Vec3f* pos, u8 alpha, s32 scrollSeed) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 165, 255, 215, alpha);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0,
                                           ((play->gameplayFrames + scrollSeed * 10) * -20) & 0x1FF, 32, 128));
    flameMtxF->xw = pos->x;
    flameMtxF->yw = pos->y;
    flameMtxF->zw = pos->z;
    Matrix_Put(flameMtxF);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Gomess_DrawFlames(EnGomess* this, PlayState* play) {
    s32 flameCount = CLAMP_MAX(this->actionTimer * 7, GOMESS_FLAME_COUNT);
    u8 batAlpha = this->actionFunc == Gomess_Dead ? this->actionTimer : 255;
    MtxF flameMtxF;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 230, 135, 25, 0);
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Put(&play->billboardMtxF);
    Matrix_Scale(0.00405f, 0.003f, 0.00405f, MTXMODE_APPLY);
    Matrix_Get(&flameMtxF);
    for (s32 i = 0; i < flameCount; i++) {
        if (this->flameAlphas[i] > 0) {
            Gomess_DrawFlameAt(play, &flameMtxF, &this->flamePositions[i], this->flameAlphas[i], i);
        }
    }
    for (s32 i = 0; i < GOMESS_BAT_COUNT; i++) {
        if (this->bats[i] == NULL) {
            continue;
        }
        for (s32 j = 0; j < BAT_EFFECT_COUNT; j++) {
            Vec3f flamePos = this->bats[i]->effects[j].pos;

            flamePos.y -= 12.0f;
            Gomess_DrawFlameAt(play, &flameMtxF, &flamePos, batAlpha, i + j);
        }
    }
}

// MM's light-orb damage effect becomes OoT's flame billboard tinted white.
static void Gomess_DrawLightOrbs(EnGomess* this, PlayState* play) {
    u8 alpha = CLAMP_MAX(this->lightOrbAlpha, 1.0f) * 255.0f;
    f32 scale = this->lightOrbScale * 0.01f;
    MtxF billboard;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_RotateY(BINANG_TO_RAD(BINANG_ROT180(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)))), MTXMODE_NEW);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_Get(&billboard);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                           (play->gameplayFrames * -20) % 512, 0x20, 0x80));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 100, 255);
    for (s32 i = 0; i < GOMESS_BODYPART_COUNT; i++) {
        billboard.xw = this->bodyPartsPos[i].x;
        billboard.yw = this->bodyPartsPos[i].y;
        billboard.zw = this->bodyPartsPos[i].z;
        Matrix_Put(&billboard);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's Scene_SetRenderModeXlu tables for segment 0x0C: the scythe DLs call entries 0 and 2.
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

static void EnGomess_Draw(Actor* thisx, PlayState* play) {
    EnGomess* this = (EnGomess*)thisx;
    bool isBodyGone = this->actionFunc == Gomess_Dead || this->actionFunc == Gomess_DeathPart4;
    bool isSpinning = this->actionFunc == Gomess_SpinAttack || this->actionFunc == Gomess_BlockProjectile;
    bool isBurning = isBodyGone || (this->actionFunc == Gomess_DeathPart3 && this->actionTimer > 0);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gomess_SetMaterialSegments(play);
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
    gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)sRenderModeXluDL);
    gDPSetEnvColor(POLY_OPA_DISP++, 30, 30, 0, 255);
    CLOSE_DISPS(play->state.gfxCtx);
    if (!isBodyGone) {
        SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                              Gomess_OverrideLimbDraw, Gomess_PostLimbDraw, this);
    }
    if (this->actionFunc == Gomess_DeathPart4) {
        Gomess_DrawCore(this, play);
    }
    if (!this->holdsScythe) {
        Gomess_DrawDroppedScythe(this, play);
    }
    if (this->lightOrbAlpha > 0.0f) {
        Gomess_DrawLightOrbs(this, play);
    }
    if (isSpinning) {
        Gomess_DrawSpinAfterimages(this, play);
    }
    Gomess_DrawBats(this, play);
    if (isBurning) {
        Gomess_DrawFlames(this, play);
    }
    SkinMatrix_Vec3fMtxFMultXYZ(&play->viewProjectionMtxF, &this->scytheWorldPos, &this->scytheScreenPos);
}

static EnGomess* Bat_GetParent(EnGomessBat* this) {
    return (EnGomess*)this->actor.parent;
}

static void Bat_DropCollectible(PlayState* play, Vec3f* pos) {
    if (sBatItemDropTimer != 0 || Rand_ZeroOne() >= 0.3f) {
        return;
    }
    Item_DropCollectible(play, pos, Rand_ZeroOne() < 0.5f ? ITEM00_ARROWS_MEDIUM : ITEM00_MAGIC_LARGE);
    sBatItemDropTimer = 800;
}

// Gomess counts how many bat actors crowd its core; five or more guard it.
static void Bat_SetCrowding(EnGomessBat* this, bool isCrowding) {
    if (this->isCrowding == isCrowding) {
        return;
    }
    this->isCrowding = isCrowding;
    Bat_GetParent(this)->crowdingBats += isCrowding ? 1 : -1;
}

static void Bat_SteerAwayFromBounds(BatEffect* effect, f32 maxRadius) {
    bool isOutside =
        effect->vel.y < 0.0f || maxRadius < effect->vel.y || SQ(maxRadius) < SQ(effect->vel.x) + SQ(effect->vel.z);

    if (!isOutside) {
        return;
    }
    s16 pitch = Math_Atan2S(sqrtf(SQ(effect->vel.x) + SQ(effect->vel.z)), effect->vel.y);

    effect->angle.x = -pitch + ((s32)Rand_Next() >> 19);
    effect->angle.y = 0x8000 + Math_Atan2S(effect->vel.z, effect->vel.x) + ((s32)Rand_Next() >> 19);
}

// Each bat flutters inside a small bubble around its actor; downed bats regrow once the actor is back on the core.
static void Bat_UpdateEffects(EnGomessBat* this, PlayState* play) {
    EnGomess* parent = Bat_GetParent(this);
    bool isTight = this->actionFunc == Bat_StartSwarm || this->actionFunc == Bat_SwarmLeader;
    f32 maxRadius = isTight ? 20.0f : 40.0f;
    s32 flyingCount = 0;
    bool hasRegrown = false;

    for (s32 i = 0; i < BAT_EFFECT_COUNT; i++) {
        BatEffect* effect = &this->effects[i];

        if (effect->state == BAT_EFFECT_FLYING) {
            effect->vel.x += 3.0f * Math_CosS(effect->angle.x) * Math_SinS(effect->angle.y);
            effect->vel.y += 3.0f * Math_SinS(effect->angle.x);
            effect->vel.z += 3.0f * Math_CosS(effect->angle.x) * Math_CosS(effect->angle.y);
            Bat_SteerAwayFromBounds(effect, maxRadius);
            flyingCount++;
        } else if (effect->state == BAT_EFFECT_FALLING) {
            effect->pos.y += effect->vel.y;
            effect->angle.y += 0x1800;
            effect->vel.y -= 1.0f;
            if (effect->pos.y < parent->actor.home.pos.y) {
                effect->pos.y = parent->actor.home.pos.y;
                EffectSsDeadDb_Spawn(play, &effect->pos, &sZeroVec, &sZeroVec, 100, 0, 255, 255, 255, 255, 255, 0, 0, 1,
                                     9, false);
                SoundSource_PlaySfxAtFixedWorldPos(play, &effect->pos, 11, NA_SE_EN_EXTINCT);
                Bat_DropCollectible(play, &effect->pos);
                effect->state = BAT_EFFECT_DOWN;
            }
        } else if (effect->timer > 0) {
            effect->timer--;
        } else if (this->actionFunc == Bat_CrowdParent) {
            Math_Vec3f_Diff(&parent->actor.focus.pos, &this->actor.world.pos, &effect->vel);
            effect->state = BAT_EFFECT_FLYING;
            this->collider.elements[i].info.bumperFlags |= BUMP_ON;
            this->collider.elements[i].info.toucherFlags |= TOUCH_ON;
            hasRegrown = true;
            flyingCount++;
        }
    }
    if (hasRegrown && flyingCount > 1) {
        Bat_SetCrowding(this, true);
    }
}

static void Bat_CrowdParent(EnGomessBat* this, PlayState* play) {
    Actor* parent = this->actor.parent;
    s16 yaw = this->spawnShapeYaw + parent->shape.rot.y;

    this->actor.world.pos.x = parent->world.pos.x + Math_SinS(yaw) * 60.0f;
    this->actor.world.pos.z = parent->world.pos.z + Math_CosS(yaw) * 60.0f;
    this->actor.world.pos.y = parent->world.pos.y + this->heightAboveParent;
    this->actor.shape.rot.y = parent->shape.rot.y;
}

static void Bat_SetupCrowdParent(EnGomessBat* this) {
    this->actor.shape.rot.x = 0;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Bat_CrowdParent;
}

static void Bat_FlyAwayFromWalls(EnGomessBat* this) {
    bool isTurned = Math_ScaledStepToS(&this->actor.shape.rot.y, this->moveDirection.y, 0x480);

    if (isTurned && (this->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
        this->moveDirection.y = ((s32)Rand_Next() >> 18) + this->actor.wallYaw;
    }
}

// Keeps a scattered bat between 50 and 200 units above Gomess's home.
static void Bat_KeepFlightBand(EnGomessBat* this) {
    if (!Math_ScaledStepToS(&this->actor.shape.rot.x, this->moveDirection.x, 0x480)) {
        return;
    }
    f32 homeY = this->actor.parent->home.pos.y;

    if (this->actor.world.pos.y < homeY + 50.0f) {
        this->moveDirection.x = -0x800 - (Rand_Next() >> 20);
    } else if (this->actor.world.pos.y > homeY + 200.0f) {
        this->moveDirection.x = 0x800 + (Rand_Next() >> 20);
    }
}

static void Bat_SetupScatter(EnGomessBat* this) {
    Vec3f* parentPos = &this->actor.parent->world.pos;
    f32 dx = this->actor.world.pos.x - parentPos->x;
    f32 dz = this->actor.world.pos.z - parentPos->z;

    this->actor.params = BAT_ORDER_NONE;
    this->actor.speedXZ = 8.0f;
    this->moveDirection.y = Math_Atan2S(dz, dx);
    this->moveDirection.x = Math_Atan2S(sqrtf(SQ(dx) + SQ(dz)), parentPos->y - this->actor.world.pos.y);
    this->timer = sBatScatterTimer != 0 ? sBatScatterTimer : 200;
    Bat_SetCrowding(this, false);
    this->actionFunc = Bat_Scatter;
}

static void Bat_SetupGather(EnGomessBat* this);

static void Bat_Scatter(EnGomessBat* this, PlayState* play) {
    Math_StepToF(&this->actor.speedXZ, 6.0f, 0.5f);
    Bat_FlyAwayFromWalls(this);
    Bat_KeepFlightBand(this);
    if (this->timer == 0) {
        Bat_SetupGather(this);
    } else {
        this->timer--;
    }
}

static void Bat_Return(EnGomessBat* this, PlayState* play);

static void Bat_SetupReturn(EnGomessBat* this) {
    this->actor.params = BAT_ORDER_NONE;
    this->actionFunc = Bat_Return;
}

static void Bat_Return(EnGomessBat* this, PlayState* play) {
    Vec3f* core = &this->actor.parent->focus.pos;
    f32 distToCore = Actor_WorldDistXYZToPoint(&this->actor, core);

    Math_StepToF(&this->actor.speedXZ, 6.0f * CLAMP_MAX(distToCore * (1.0f / 60.0f), 1.0f), 0.5f);
    Math_ScaledStepToS(&this->actor.shape.rot.y, Actor_WorldYawTowardPoint(&this->actor, core), 0x800);
    Math_ScaledStepToS(&this->actor.shape.rot.x, Actor_WorldPitchTowardPoint(&this->actor, core), 0x800);
    if (distToCore < 30.0f) {
        Bat_SetupGather(this);
    }
}

static void Bat_SetupGather(EnGomessBat* this) {
    this->actor.speedXZ = 0.0f;
    Bat_SetCrowding(this, true);
    for (s32 i = 0; i < BAT_EFFECT_COUNT; i++) {
        this->effects[i].timer = 0;
    }
    this->actionFunc = Bat_Gather;
}

static f32 StepVec3fTo(Vec3f* pos, Vec3f* target, f32 step) {
    Vec3f diff;

    Math_Vec3f_Diff(target, pos, &diff);
    f32 dist = sqrtf(SQ(diff.x) + SQ(diff.y) + SQ(diff.z));

    if (dist <= step) {
        *pos = *target;
        return 0.0f;
    }
    pos->x += diff.x * (step / dist);
    pos->y += diff.y * (step / dist);
    pos->z += diff.z * (step / dist);
    return dist - step;
}

// Settles back into its slot around the core.
static void Bat_Gather(EnGomessBat* this, PlayState* play) {
    Actor* parent = this->actor.parent;
    s16 slotYaw = this->spawnShapeYaw + parent->shape.rot.y;
    bool isFacingSlot = Math_ScaledStepToS(&this->actor.shape.rot.y, slotYaw, 0x800);
    Vec3f slot = { parent->world.pos.x + Math_SinS(slotYaw) * 60.0f, parent->world.pos.y + this->heightAboveParent,
                   parent->world.pos.z + Math_CosS(slotYaw) * 60.0f };

    Math_ScaledStepToS(&this->actor.shape.rot.x, 0, 0x800);
    if (isFacingSlot && StepVec3fTo(&this->actor.world.pos, &slot, 10.0f) < 20.0f) {
        Bat_SetupCrowdParent(this);
    }
}

static void Bat_StartSwarm(EnGomessBat* this, PlayState* play) {
    StepVec3fTo(&this->actor.world.pos, &this->actor.parent->focus.pos, 5.0f);
}

// Two leaders (0 and 10) circle the core; the rest string out in two snakes that chase Link, each bat its child.
static void Bat_SetupSwarmFollower(EnGomessBat* this, PlayState* play) {
    Actor* parent = this->actor.parent;
    bool isSecondSnake = this->number >= 10;
    s32 rank = isSecondSnake ? 19 - this->number : 9 - this->number;
    s32 side = isSecondSnake ? 1 : -1;
    s32 pitchBias = isSecondSnake ? 0 : 0x200;

    this->actor.params = BAT_ORDER_NONE;
    this->actor.speedXZ = 8.0f;
    this->actor.shape.rot.y = parent->shape.rot.y + 0x4000 * side + 0x2000 * rank * side;
    this->actor.shape.rot.x = 0xC00 + pitchBias - 0x400 * rank;
    this->actor.world.pos.x = parent->focus.pos.x + Math_SinS(this->actor.shape.rot.y) * 60.0f;
    this->actor.world.pos.z = parent->focus.pos.z + Math_CosS(this->actor.shape.rot.y) * 60.0f;
    this->actor.world.pos.y = parent->focus.pos.y - Math_SinS(this->actor.shape.rot.x) * 60.0f;
    Bat_RandomizeEffects(this);
    this->timer = 100;
    Bat_SetCrowding(this, false);
    sSwarmHitCount = 0;
    bool isHead = this->actor.child == NULL || this->number == 9;

    this->targetActor = isHead ? &GET_PLAYER(play)->actor : this->actor.child;
    this->actionFunc = Bat_SwarmFollower;
}

static bool Bat_IsBat(Actor* actor) {
    return actor->id == sBatActorId;
}

static void Bat_SwarmFollower(EnGomessBat* this, PlayState* play) {
    Actor* parent = this->actor.parent;
    Vec3f target;
    s16 yawOffset = 0;

    Math_StepToF(&this->actor.speedXZ, 6.0f, 0.2f);
    if (this->timer > 0) {
        this->timer--;
    }
    if (!Bat_IsBat(this->targetActor)) {
        if (this->timer == 0) {
            this->targetActor = parent;
        }
        target = this->targetActor->focus.pos;
        if (Actor_WorldDistXYZToPoint(&this->actor, &target) > 200.0f) {
            yawOffset = this->number < 10 ? -0x1C00 : 0x1C00;
            target.y += 50.0f;
        }
    } else if (((EnGomessBat*)this->targetActor)->actionFunc != Bat_SwarmFollower) {
        target = parent->focus.pos;
    } else {
        target = this->targetActor->focus.pos;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, Actor_WorldYawTowardPoint(&this->actor, &target) + yawOffset, 5, 0xC00,
                       0x80);
    Math_SmoothStepToS(&this->actor.shape.rot.x, Actor_WorldPitchTowardPoint(&this->actor, &target), 5, 0xC00, 0x80);
    if ((this->collider.base.atFlags & AT_HIT) && this->timer > 0) {
        sSwarmHitCount++;
    }
    if (this->timer == 0 && Actor_WorldDistXYZToPoint(&this->actor, &parent->focus.pos) < 50.0f) {
        Bat_SetupGather(this);
    }
}

static void Bat_SetupSwarmLeader(EnGomessBat* this) {
    this->actor.params = BAT_ORDER_NONE;
    this->timer = 10;
    Bat_RandomizeEffects(this);
    this->actionFunc = Bat_SwarmLeader;
}

static void Bat_SwarmLeader(EnGomessBat* this, PlayState* play) {
    Actor* parent = this->actor.parent;
    Vec3f target = { Math_SinS(parent->shape.rot.y) * 20.0f + parent->focus.pos.x, parent->focus.pos.y,
                     Math_CosS(parent->shape.rot.y) * 20.0f + parent->focus.pos.z };

    if (this->timer > 0) {
        this->timer--;
    }
    StepVec3fTo(&this->actor.world.pos, &target, 5.0f);
    bool isSnakeBack = this->actor.child == NULL || ((EnGomessBat*)this->actor.child)->actionFunc != Bat_SwarmFollower;

    if (this->timer == 0 && isSnakeBack) {
        Bat_SetupCrowdParent(this);
    }
}

static void Bat_Flee(EnGomessBat* this, PlayState* play) {
    Math_StepToF(&this->actor.speedXZ, 5.0f, 0.2f);
    Bat_FlyAwayFromWalls(this);
    Bat_KeepFlightBand(this);
}

static void Bat_SetupFlee(EnGomessBat* this) {
    this->actor.params = BAT_ORDER_NONE;
    Bat_SetCrowding(this, false);
    this->collider.base.atFlags &= ~AT_ON;
    for (s32 i = 0; i < BAT_EFFECT_COUNT; i++) {
        this->effects[i].state = BAT_EFFECT_FLYING;
    }
    this->moveDirection.y = this->actor.shape.rot.y;
    this->moveDirection.x = this->actor.shape.rot.x;
    this->actionFunc = Bat_Flee;
}

static void Bat_SetupDie(EnGomessBat* this) {
    this->actor.params = BAT_ORDER_NONE;
    this->moveDirection.x = 0x4000;
    this->actor.gravity = -0.5f;
    this->actionFunc = Bat_Die;
}

static void Bat_Die(EnGomessBat* this, PlayState* play) {
    Math_StepToF(&this->actor.speedXZ, 0.0f, 0.2f);
    Math_ScaledStepToS(&this->actor.shape.rot.x, this->moveDirection.x, 0x480);
    PlaySfx(&this->actor, MM_SE_EN_COMMON_EXTINCT_LEV - SFX_FLAG, NA_SE_EN_EXTINCT);
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return;
    }
    this->actor.speedXZ = 0.0f;
    s32 settledCount = 0;

    for (s32 i = 0; i < BAT_EFFECT_COUNT; i++) {
        if (Math_StepToF(&this->effects[i].vel.y, 0.0f, 7.0f)) {
            settledCount++;
        }
    }
    if (settledCount == BAT_EFFECT_COUNT) {
        this->actionFunc = Bat_Dead;
    }
}

static void Bat_Dead(EnGomessBat* this, PlayState* play) {
    PlaySfx(&this->actor, MM_SE_EN_COMMON_EXTINCT_LEV - SFX_FLAG, NA_SE_EN_EXTINCT);
}

static void Bat_FollowOrder(EnGomessBat* this, PlayState* play) {
    switch (this->actor.params) {
        case BAT_ORDER_NONE:
            break;
        case BAT_ORDER_FLEE:
            Bat_SetupFlee(this);
            break;
        case BAT_ORDER_RETURN:
            Bat_SetupReturn(this);
            break;
        case BAT_ORDER_START_SWARM:
            this->actor.params = BAT_ORDER_NONE;
            this->actionFunc = Bat_StartSwarm;
            break;
        case BAT_ORDER_SWARM:
            if (this->number == 0 || this->number == 10) {
                Bat_SetupSwarmLeader(this);
            } else {
                Bat_SetupSwarmFollower(this, play);
            }
            break;
        case BAT_ORDER_SCATTER:
            Bat_SetupScatter(this);
            break;
        case BAT_ORDER_DIE:
            Bat_SetupDie(this);
            break;
        default:
            this->actor.params = BAT_ORDER_NONE;
            break;
    }
}

// A nut stuns the crowd into a brief scatter or makes a swarm bat break off toward Link.
static void Bat_Stun(EnGomessBat* this, PlayState* play) {
    if (this->actionFunc == Bat_CrowdParent || this->actionFunc == Bat_Gather) {
        sBatScatterTimer = 5;
        return;
    }
    if (this->actionFunc != Bat_SwarmFollower) {
        return;
    }
    EnGomessBat** bats = Bat_GetParent(this)->bats;

    for (s32 i = this->number - 1; i >= 0; i--) {
        if (bats[i] != NULL && bats[i]->targetActor == &this->actor) {
            bats[i]->targetActor = this->targetActor;
            break;
        }
    }
    this->timer = 0;
    this->targetActor = &GET_PLAYER(play)->actor;
}

static void Bat_KnockDown(EnGomessBat* this, PlayState* play) {
    bool isGuarding = this->actionFunc == Bat_CrowdParent || this->actionFunc == Bat_Gather;
    s32 downCount = 0;

    for (s32 i = 0; i < BAT_EFFECT_COUNT; i++) {
        ColliderInfo* info = &this->collider.elements[i].info;

        if (info->bumperFlags & BUMP_HIT) {
            info->bumperFlags &= ~(BUMP_ON | BUMP_HIT);
            info->toucherFlags &= ~(TOUCH_ON | TOUCH_HIT);
            this->effects[i].vel.y = -1.0f;
            this->effects[i].state = BAT_EFFECT_FALLING;
            this->effects[i].angle.y = this->actor.shape.rot.y;
            this->effects[i].timer = 60;
            if (isGuarding && this->actor.colChkInfo.damageEffect == BAT_DMGEFF_LIGHT_ARROW) {
                sBatScatterTimer = 200;
            }
        }
        if (this->effects[i].state != BAT_EFFECT_FLYING) {
            downCount++;
        }
    }
    if (downCount > 1) {
        Bat_SetCrowding(this, false);
    }
    if (!sHasPlayedBatDeathSfx) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_FFLY_DEAD);
        sHasPlayedBatDeathSfx = true;
    }
}

static void Bat_UpdateDamage(EnGomessBat* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    switch (this->actor.colChkInfo.damageEffect) {
        case BAT_DMGEFF_IMMUNE:
            break;
        case BAT_DMGEFF_STUN:
            Bat_Stun(this, play);
            break;
        default:
            Bat_KnockDown(this, play);
            break;
    }
}

// Bat 19 runs last each frame and applies the swarm-wide outcomes: a scatter, or an early recall after 5 hits.
static void Bat_CoordinateSwarm(EnGomessBat* this) {
    EnGomessBat** bats = Bat_GetParent(this)->bats;

    sHasPlayedBatDeathSfx = false;
    if (sBatScatterTimer != 0) {
        for (s32 i = 0; i < GOMESS_BAT_COUNT; i++) {
            if (bats[i] != NULL) {
                Bat_SetupScatter(bats[i]);
            }
        }
        sBatScatterTimer = 0;
    } else if (sSwarmHitCount >= 5) {
        for (s32 i = 0; i < GOMESS_BAT_COUNT; i++) {
            if (bats[i] != NULL) {
                bats[i]->timer = 0;
            }
        }
        sSwarmHitCount = 0;
    }
}

static void Bat_UpdateEffectPositions(EnGomessBat* this) {
    for (s32 i = 0; i < BAT_EFFECT_COUNT; i++) {
        BatEffect* effect = &this->effects[i];

        if (effect->state != BAT_EFFECT_FLYING) {
            continue;
        }
        Math_Vec3f_Sum(&this->actor.world.pos, &effect->vel, &effect->pos);
        if (this->actionFunc != Bat_Die) {
            effect->animFrame = (effect->animFrame + 1) % BAT_ANIM_LENGTH;
        }
        this->collider.elements[i].dim.worldSphere.center.x = effect->pos.x;
        this->collider.elements[i].dim.worldSphere.center.y = effect->pos.y;
        this->collider.elements[i].dim.worldSphere.center.z = effect->pos.z;
    }
}

// Crowding bats take turns, a quarter of them per frame, to spare the collision checks.
static void Bat_SubmitColliders(EnGomessBat* this, PlayState* play) {
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        this->attackTimer = 40;
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_FFLY_ATTACK);
    }
    if (this->collider.base.atFlags & AT_ON) {
        s32 turn = this->actionFunc == Bat_CrowdParent ? play->gameplayFrames % 4 : 4;
        bool isMyTurn = turn == 4 || turn == (this->number & 3);

        if (isMyTurn && this->attackTimer == 0 && this->actor.parent->shape.rot.x == 0) {
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        }
        if (isMyTurn && this->actionFunc != Bat_StartSwarm) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }
    }
    if (this->attackTimer > 0) {
        this->attackTimer--;
    }
}

static void EnGomessBat_Init(Actor* thisx, PlayState* play) {
    EnGomessBat* this = (EnGomessBat*)thisx;

    this->spawnShapeYaw = this->actor.shape.rot.y;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sBatJntSphInit, this->colliderElements);
    this->actor.colChkInfo.damageTable = &sBatDamageTable;
    this->actor.colChkInfo.health = 1;
    for (s32 i = 0; i < BAT_EFFECT_COUNT; i++) {
        this->collider.elements[i].dim.worldSphere.radius = this->collider.elements[i].dim.modelSphere.radius;
    }
    Bat_RandomizeEffects(this);
    this->number = this->actor.params;
    this->actor.params = BAT_ORDER_NONE;
    this->actionFunc = Bat_CrowdParent;
}

static void EnGomessBat_Destroy(Actor* thisx, PlayState* play) {
    EnGomessBat* this = (EnGomessBat*)thisx;
    EnGomess* parent = Bat_GetParent(this);

    if (parent != NULL) {
        parent->bats[this->number] = NULL;
    }
    Collider_DestroyJntSph(play, &this->collider);
}

static void EnGomessBat_Update(Actor* thisx, PlayState* play) {
    EnGomessBat* this = (EnGomessBat*)thisx;

    // Not attached yet (still inside Gomess's Init) or Gomess is gone: OoT never clears a dead parent pointer.
    if (this->actor.parent == NULL || this->actor.parent->update == NULL) {
        Actor_Kill(&this->actor);
        return;
    }
    if (this->number == 0 && sBatItemDropTimer > 0) {
        sBatItemDropTimer--;
    }
    if (this->actionFunc != Bat_Dead) {
        for (s32 i = 0; i < BAT_EFFECT_COUNT; i++) {
            if (this->effects[i].state == BAT_EFFECT_FLYING) {
                PlaySfx(&this->actor, MM_SE_EV_BAT_FLY - SFX_FLAG, NA_SE_EN_FFLY_FLY - SFX_FLAG);
                break;
            }
        }
    }
    Bat_UpdateDamage(this, play);
    Bat_FollowOrder(this, play);
    this->actionFunc(this, play);
    if (this->actionFunc == Bat_Dead) {
        return;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.world.rot.x = -this->actor.shape.rot.x;
    if (this->actionFunc == Bat_Die) {
        Actor_MoveXZGravity(&this->actor);
    } else {
        Actor_MoveXYZ(&this->actor);
    }
    if (this->actionFunc != Bat_CrowdParent && this->actionFunc != Bat_Gather) {
        Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 50.0f, 80.0f, BGCHECK_UPDATE_WALL_CEILING_FLOOR);
    }
    if (this->actionFunc != Bat_Die) {
        Bat_UpdateEffects(this, play);
    }
    Actor_SetFocus(&this->actor, 0.0f);
    Bat_UpdateEffectPositions(this);
    Bat_SubmitColliders(this, play);
    if (this->number == GOMESS_BAT_COUNT - 1) {
        Bat_CoordinateSwarm(this);
    }
}

static bool CanBeGoron(void) {
    int32_t goronMask = sApi->GetRandoItem(GORON_MASK_KEY);

    return goronMask >= 0 && sApi->LogicHasItem(goronMask);
}

// MM's CanKillEnemy(ACTOR_EN_DEATH): a sword, bow, hookshot or Goron fists; the hammer stands in for the fists.
static bool Gomess_CanKillInLogic(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RG_FAIRY_BOW) ||
           sApi->LogicCanUseItem(RG_HOOKSHOT) || sApi->LogicCanUseItem(RG_MEGATON_HAMMER) || CanBeGoron();
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
    static Vec3s sSoulJointTable[GOMESS_LIMB_MAX];
    static Vec3s sSoulMorphTable[GOMESS_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;
    static const Color_RGBA8 sSoulBodyPrimColor = { 255, 255, 255, 255 };

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sGomessSkel, (AnimationHeader*)sFloatAnim,
                           sSoulJointTable, sSoulMorphTable, GOMESS_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-sSoulJointTable[0].x, -sSoulJointTable[0].y, -sSoulJointTable[0].z, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)Gomess_MakeColorDL(play, 255, sSoulBodyPrimColor, NULL));
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)Gomess_MakeCoreColorDL(play));
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
    SOHCustomItemDefinition soul = Z64Items_Define(GOMESS_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Gomess";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Gomess%w!&Gomess can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " GOMESS_SOUL_ITEM_KEY);
    }
}

static void RegisterBat(void) {
    SOHActorDefinition bat = { 0 };

    bat.structSize = sizeof(bat);
    bat.key = GOMESS_BAT_KEY;
    bat.description = "Gomess Bats";
    bat.category = ACTORCAT_ENEMY;
    bat.actorFlags = ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    bat.objectId = OBJECT_GAMEPLAY_KEEP;
    bat.instanceSize = sizeof(EnGomessBat);
    bat.init = EnGomessBat_Init;
    bat.destroy = EnGomessBat_Destroy;
    bat.update = EnGomessBat_Update;
    sBatActorId = sApi->RegisterActor(&bat);
}

static void RegisterGomess(void) {
    SOHActorDefinition gomess = { 0 };

    gomess.structSize = sizeof(gomess);
    gomess.key = GOMESS_KEY;
    gomess.description = "Gomess";
    gomess.category = ACTORCAT_ENEMY;
    gomess.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                        ACTOR_FLAG_DRAW_CULLING_DISABLED;
    gomess.objectId = OBJECT_GAMEPLAY_KEEP;
    gomess.instanceSize = sizeof(EnGomess);
    gomess.init = EnGomess_Init;
    gomess.destroy = EnGomess_Destroy;
    gomess.update = EnGomess_Update;
    gomess.draw = EnGomess_Draw;
    gomess.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_MINIBOSS | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    gomess.naviHint = "Gomess&%cIts bats shield the core. Scatter them with a %ylight arrow%c or cut them down, "
                      "then strike!%w";
    gomess.soulKey = GOMESS_KEY;
    gomess.logicKill = Gomess_CanKillInLogic;
    if (sApi->RegisterActor(&gomess) < 0) {
        sApi->Log("Could not register " GOMESS_KEY);
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
    RegisterBat();
    if (sBatActorId < 0) {
        sApi->Log("Could not register " GOMESS_BAT_KEY);
        return;
    }
    RegisterSoul();
    RegisterGomess();
}
