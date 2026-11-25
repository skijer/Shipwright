// Majora's Mask's Gekko & Snapper (En_Pametfrog + En_Bigpamet) as an Unbound custom enemy, drawn from mm.o2r.
// No cutscenes and no minifrog; the wall and roll phases fall back to the floor when the arena has no walls.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "overlays/effects/ovl_Effect_Ss_Hahen/z_eff_ss_hahen.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define GEKKO_KEY "mm.gekko"
#define GEKKO_SOUL_ITEM_KEY "soul.mm.gekko"
#define SNAPPER_MOUNT_KEY "mm.gekko.snapper"
#define GORON_MASK_KEY "nei.goron_form_mask"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_GEKKO "__OTR__objects/object_bigslime/"
#define OBJ_SNAPPER "__OTR__objects/object_tl/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_CEILING_FLOOR ((1 << 0) | (1 << 1) | (1 << 2))

#define MM_SE_EV_WALK_WATER 0x2902
#define MM_SE_EV_ICE_BROKEN 0x28CB
#define MM_SE_EN_BOMCHU_WALK 0x3828
#define MM_SE_EN_COMMON_FREEZE 0x389E
#define MM_SE_EN_FROG_REAL 0x38D2
#define MM_SE_EN_FROG_DAMAGE 0x38D3
#define MM_SE_EN_FROG_DEAD 0x38D6
#define MM_SE_EN_HIPLOOP_LAND 0x38DD
#define MM_SE_EN_PAMET_CUTTER_ON 0x392B
#define MM_SE_EN_PAMET_CUTTER_OFF 0x392D
#define MM_SE_EN_B_PAMET_BREAK 0x393F
#define MM_SE_EN_FROG_JUMP 0x395B
#define MM_SE_EN_FROG_GREET 0x395C
#define MM_SE_EN_FROG_PUNCH1 0x3961
#define MM_SE_EN_FROG_KICK 0x3966
#define MM_SE_EN_B_PAMET_ROLL 0x399E
#define MM_SE_EN_FROG_VOICE1 0x399F
#define MM_SE_EN_FROG_VOICE2 0x39A1
#define MM_SE_EN_B_PAMET_VOICE 0x39A2
#define MM_SE_EN_B_PAMET_REVERSE 0x39A3
#define MM_SE_EN_FROG_RUNAWAY 0x39A9
#define MM_SE_EN_FROG_RUNAWAY2 0x39AC
#define MM_SE_EN_NPC_APPEAR 0x3A86
#define MM_SE_EN_NPC_FADEAWAY 0x3A87

#define GEKKO_FIRST_HEALTH 3
#define GEKKO_MOUNTED_HEALTH 6
#define GEKKO_WALL_SEARCH_FRAMES 60
#define GEKKO_WALL_SEARCH_RADIUS 700.0f
#define GEKKO_SCURRY_FRAMES 90
#define SNAPPER_ROLL_FRAMES 90
#define SNAPPER_ROLL_RADIUS 900.0f
#define SNAPPER_SPAWN_DISTANCE 300.0f
#define SNAPPER_DEBRIS_COUNT 20
#define HAMMER_POUND_RANGE 400.0f

typedef enum {
    GEKKO_LIMB_NONE,
    GEKKO_LIMB_ROOT,
    GEKKO_LIMB_WAIST,
    GEKKO_LIMB_LEFT_THIGH,
    GEKKO_LIMB_LEFT_SHIN,
    GEKKO_LIMB_LEFT_ANKLE,
    GEKKO_LIMB_LEFT_FOOT,
    GEKKO_LIMB_RIGHT_THIGH,
    GEKKO_LIMB_RIGHT_SHIN,
    GEKKO_LIMB_RIGHT_ANKLE,
    GEKKO_LIMB_RIGHT_FOOT,
    GEKKO_LIMB_TORSO,
    GEKKO_LIMB_LEFT_UPPER_ARM,
    GEKKO_LIMB_LEFT_FOREARM,
    GEKKO_LIMB_LEFT_WRIST,
    GEKKO_LIMB_LEFT_HAND,
    GEKKO_LIMB_RIGHT_UPPER_ARM,
    GEKKO_LIMB_RIGHT_FOREARM,
    GEKKO_LIMB_RIGHT_WRIST,
    GEKKO_LIMB_RIGHT_HAND,
    GEKKO_LIMB_HEAD,
    GEKKO_LIMB_JAW,
    GEKKO_LIMB_LEFT_EYE,
    GEKKO_LIMB_RIGHT_EYE,
    GEKKO_LIMB_MAX
} GekkoLimb;

typedef enum {
    GEKKO_BODYPART_NONE = -1,
    GEKKO_BODYPART_WAIST,
    GEKKO_BODYPART_LEFT_SHIN,
    GEKKO_BODYPART_LEFT_FOOT,
    GEKKO_BODYPART_RIGHT_SHIN,
    GEKKO_BODYPART_RIGHT_FOOT,
    GEKKO_BODYPART_LEFT_UPPER_ARM,
    GEKKO_BODYPART_LEFT_FOREARM,
    GEKKO_BODYPART_LEFT_HAND,
    GEKKO_BODYPART_RIGHT_UPPER_ARM,
    GEKKO_BODYPART_RIGHT_FOREARM,
    GEKKO_BODYPART_RIGHT_HAND,
    GEKKO_BODYPART_JAW,
    GEKKO_BODYPART_MAX
} GekkoBodyPart;

typedef enum {
    SNAPPER_LIMB_NONE,
    SNAPPER_LIMB_BODY,
    SNAPPER_LIMB_HEAD,
    SNAPPER_LIMB_JAW,
    SNAPPER_LIMB_EYES,
    SNAPPER_LIMB_FRONT_LEFT_LEG,
    SNAPPER_LIMB_FRONT_LEFT_FOOT,
    SNAPPER_LIMB_FRONT_RIGHT_LEG,
    SNAPPER_LIMB_FRONT_RIGHT_FOOT,
    SNAPPER_LIMB_BACK_LEFT_LEG,
    SNAPPER_LIMB_BACK_LEFT_FOOT,
    SNAPPER_LIMB_BACK_RIGHT_LEG,
    SNAPPER_LIMB_BACK_RIGHT_FOOT,
    SNAPPER_LIMB_MAX
} SnapperLimb;

typedef enum {
    SPIKED_SNAPPER_LIMB_NONE,
    SPIKED_SNAPPER_LIMB_BODY,
    SPIKED_SNAPPER_LIMB_SHELL,
    SPIKED_SNAPPER_LIMB_SPIKES,
    SPIKED_SNAPPER_LIMB_MAX
} SpikedSnapperLimb;

// The Snapper reads the Gekko's state to follow him, as MM's En_Bigpamet reads its parent's params.
typedef enum {
    GEKKO_STATE_PRE_SNAPPER,
    GEKKO_STATE_GET_SNAPPER,
    GEKKO_STATE_INIT_SNAPPER,
    GEKKO_STATE_ON_SNAPPER,
    GEKKO_STATE_REAR_ON_SNAPPER,
    GEKKO_STATE_KNOCKED_OFF,
    GEKKO_STATE_FALL_OFF_SNAPPER,
    GEKKO_STATE_RETURN_TO_SNAPPER,
    GEKKO_STATE_JUMP_ON_SNAPPER,
    GEKKO_STATE_DEFEAT
} GekkoState;

typedef enum {
    GEKKO_DMGEFF_NONE,
    GEKKO_DMGEFF_STUN,
    GEKKO_DMGEFF_FIRE,
    GEKKO_DMGEFF_ICE,
    GEKKO_DMGEFF_LIGHT
} GekkoDamageEffect;

#define SNAPPER_DMGEFF_FLIP 0xF

struct EnGekko;
struct EnSnapper;

typedef void (*EnGekkoActionFunc)(struct EnGekko*, PlayState*);
typedef void (*EnSnapperActionFunc)(struct EnSnapper*, PlayState*);

typedef struct EnGekko {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[GEKKO_LIMB_MAX];
    Vec3s morphTable[GEKKO_LIMB_MAX];
    EnGekkoActionFunc actionFunc;
    u8 state;
    u8 wallPausesLeft;
    bool isFrozen;
    bool isFightStarted;
    s16 timer;
    s16 spinYaw;
    s16 freezeTimer;
    f32 wallRotation;
    Vec3f surfaceForward;
    Vec3f surfaceNormal;
    Vec3f surfaceSide;
    Vec3f bodyPartsPos[GEKKO_BODYPART_MAX];
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[2];
} EnGekko;

typedef struct {
    Vec3f pos;
    Vec3f velocity;
    Vec3s rot;
    f32 scale;
} SnapperDebris;

typedef struct EnSnapper {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[SNAPPER_LIMB_MAX];
    Vec3s morphTable[SNAPPER_LIMB_MAX];
    SkelAnime shellSkelAnime;
    Vec3s shellJointTable[SPIKED_SNAPPER_LIMB_MAX];
    Vec3s shellMorphTable[SPIKED_SNAPPER_LIMB_MAX];
    EnSnapperActionFunc actionFunc;
    u8 blinkIndex;
    bool isUpright;
    s16 timer;
    s16 debrisTimer;
    s16 rollFrames;
    f32 limbScaleXZ;
    f32 limbScaleY;
    f32 bodyHeight;
    ColliderCylinder collider;
    SnapperDebris debris[SNAPPER_DEBRIS_COUNT];
} EnSnapper;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sGekkoSkel[] = OBJ_GEKKO "gGekkoSkel";
static const ALIGN_ASSET(2) char sGekkoBoxingStanceAnim[] = OBJ_GEKKO "gGekkoBoxingStanceAnim";
static const ALIGN_ASSET(2) char sGekkoCallAnim[] = OBJ_GEKKO "gGekkoCallAnim";
static const ALIGN_ASSET(2) char sGekkoCrawlAnim[] = OBJ_GEKKO "gGekkoCrawlAnim";
static const ALIGN_ASSET(2) char sGekkoFallInAirAnim[] = OBJ_GEKKO "gGekkoFallInAirAnim";
static const ALIGN_ASSET(2) char sGekkoFallOnGroundAnim[] = OBJ_GEKKO "gGekkoFallOnGroundAnim";
static const ALIGN_ASSET(2) char sGekkoFullFistPumpAnim[] = OBJ_GEKKO "gGekkoFullFistPumpAnim";
static const ALIGN_ASSET(2) char sGekkoHookPunchAnim[] = OBJ_GEKKO "gGekkoHookPunchAnim";
static const ALIGN_ASSET(2) char sGekkoJabPunchAnim[] = OBJ_GEKKO "gGekkoJabPunchAnim";
static const ALIGN_ASSET(2) char sGekkoJumpForwardAnim[] = OBJ_GEKKO "gGekkoJumpForwardAnim";
static const ALIGN_ASSET(2) char sGekkoJumpOnSnapperAnim[] = OBJ_GEKKO "gGekkoJumpOnSnapperAnim";
static const ALIGN_ASSET(2) char sGekkoKickAnim[] = OBJ_GEKKO "gGekkoKickAnim";
static const ALIGN_ASSET(2) char sGekkoKnockbackAnim[] = OBJ_GEKKO "gGekkoKnockbackAnim";
static const ALIGN_ASSET(2) char sGekkoLandOnSnapperAnim[] = OBJ_GEKKO "gGekkoLandOnSnapperAnim";
static const ALIGN_ASSET(2) char sGekkoLookAroundAnim[] = OBJ_GEKKO "gGekkoLookAroundAnim";
static const ALIGN_ASSET(2) char sGekkoQuickFistPumpAnim[] = OBJ_GEKKO "gGekkoQuickFistPumpAnim";
static const ALIGN_ASSET(2) char sGekkoRecoverAnim[] = OBJ_GEKKO "gGekkoRecoverAnim";
static const ALIGN_ASSET(2) char sGekkoSqueezeAnim[] = OBJ_GEKKO "gGekkoSqueezeAnim";
static const ALIGN_ASSET(2) char sGekkoStandingIdleAnim[] = OBJ_GEKKO "gGekkoStandingIdleAnim";
static const ALIGN_ASSET(2) char sGekkoWiggleAnim[] = OBJ_GEKKO "gGekkoWiggleAnim";
static const ALIGN_ASSET(2) char sGekkoWindupPunchAnim[] = OBJ_GEKKO "gGekkoWindupPunchAnim";

static const ALIGN_ASSET(2) char sSnapperSkel[] = OBJ_SNAPPER "gSnapperSkel";
static const ALIGN_ASSET(2) char sSpikedSnapperSkel[] = OBJ_SNAPPER "gSpikedSnapperSkel";
static const ALIGN_ASSET(2) char sSnapperIdleAnim[] = OBJ_SNAPPER "gSnapperIdleAnim";
static const ALIGN_ASSET(2) char sSpikedSnapperIdleAnim[] = OBJ_SNAPPER "gSpikedSnapperIdleAnim";
static const ALIGN_ASSET(2) char sSnapperWalkAnim[] = OBJ_SNAPPER "gSnapperWalkAnim";
static const ALIGN_ASSET(2) char sSnapperRetreatIntoShellAnim[] = OBJ_SNAPPER "gSnapperRetreatIntoShellAnim";
static const ALIGN_ASSET(2) char sSnapperEmergeFromShellAnim[] = OBJ_SNAPPER "gSnapperEmergeFromShellAnim";
static const ALIGN_ASSET(2) char sSnapperDeathAnim[] = OBJ_SNAPPER "gSnapperDeathAnim";
static const ALIGN_ASSET(2) char sSnapperRearUpAnim[] = OBJ_SNAPPER "gSnapperRearUpAnim";
static const ALIGN_ASSET(2) char sSnapperEyeOpenTex[] = OBJ_SNAPPER "gSnapperEyeOpenTex";
static const ALIGN_ASSET(2) char sSnapperEyeHalfTex[] = OBJ_SNAPPER "gSnapperEyeHalfTex";
static const ALIGN_ASSET(2) char sSnapperEyeClosedTex[] = OBJ_SNAPPER "gSnapperEyeClosedTex";
static const ALIGN_ASSET(2) char sRockFragmentDL[] = "__OTR__objects/gameplay_keep/gameplay_keep_DL_06AB30";

static const char* const sSnapperEyeTextures[] = {
    sSnapperEyeOpenTex,
    sSnapperEyeHalfTex,
    sSnapperEyeClosedTex,
    sSnapperEyeHalfTex,
};

static const char* const sGekkoAttackAnims[] = {
    sGekkoJabPunchAnim,
    sGekkoHookPunchAnim,
    sGekkoKickAnim,
    sGekkoWindupPunchAnim,
};

static const s8 sGekkoLimbToBodyPart[GEKKO_LIMB_MAX] = {
    [GEKKO_LIMB_NONE] = GEKKO_BODYPART_NONE,
    [GEKKO_LIMB_ROOT] = GEKKO_BODYPART_NONE,
    [GEKKO_LIMB_WAIST] = GEKKO_BODYPART_WAIST,
    [GEKKO_LIMB_LEFT_THIGH] = GEKKO_BODYPART_NONE,
    [GEKKO_LIMB_LEFT_SHIN] = GEKKO_BODYPART_LEFT_SHIN,
    [GEKKO_LIMB_LEFT_ANKLE] = GEKKO_BODYPART_NONE,
    [GEKKO_LIMB_LEFT_FOOT] = GEKKO_BODYPART_LEFT_FOOT,
    [GEKKO_LIMB_RIGHT_THIGH] = GEKKO_BODYPART_NONE,
    [GEKKO_LIMB_RIGHT_SHIN] = GEKKO_BODYPART_RIGHT_SHIN,
    [GEKKO_LIMB_RIGHT_ANKLE] = GEKKO_BODYPART_NONE,
    [GEKKO_LIMB_RIGHT_FOOT] = GEKKO_BODYPART_RIGHT_FOOT,
    [GEKKO_LIMB_TORSO] = GEKKO_BODYPART_NONE,
    [GEKKO_LIMB_LEFT_UPPER_ARM] = GEKKO_BODYPART_LEFT_UPPER_ARM,
    [GEKKO_LIMB_LEFT_FOREARM] = GEKKO_BODYPART_LEFT_FOREARM,
    [GEKKO_LIMB_LEFT_WRIST] = GEKKO_BODYPART_NONE,
    [GEKKO_LIMB_LEFT_HAND] = GEKKO_BODYPART_LEFT_HAND,
    [GEKKO_LIMB_RIGHT_UPPER_ARM] = GEKKO_BODYPART_RIGHT_UPPER_ARM,
    [GEKKO_LIMB_RIGHT_FOREARM] = GEKKO_BODYPART_RIGHT_FOREARM,
    [GEKKO_LIMB_RIGHT_WRIST] = GEKKO_BODYPART_NONE,
    [GEKKO_LIMB_RIGHT_HAND] = GEKKO_BODYPART_RIGHT_HAND,
    [GEKKO_LIMB_HEAD] = GEKKO_BODYPART_NONE,
    [GEKKO_LIMB_JAW] = GEKKO_BODYPART_JAW,
    [GEKKO_LIMB_LEFT_EYE] = GEKKO_BODYPART_NONE,
    [GEKKO_LIMB_RIGHT_EYE] = GEKKO_BODYPART_NONE,
};

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sGekkoDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, GEKKO_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, GEKKO_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, GEKKO_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, GEKKO_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, GEKKO_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, GEKKO_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, GEKKO_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, GEKKO_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, GEKKO_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, GEKKO_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, GEKKO_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(4, GEKKO_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, GEKKO_DMGEFF_ICE),
    /* Light arrow   */ DMG_ENTRY(4, GEKKO_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, GEKKO_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, GEKKO_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, GEKKO_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(2, GEKKO_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(4, GEKKO_DMGEFF_ICE),
    /* Light magic   */ DMG_ENTRY(4, GEKKO_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, GEKKO_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, GEKKO_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(2, GEKKO_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(8, GEKKO_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(4, GEKKO_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, GEKKO_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, GEKKO_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, GEKKO_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, GEKKO_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, GEKKO_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, GEKKO_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, GEKKO_DMGEFF_NONE),
};

static DamageTable sSnapperDamageTable = {
    /* Deku nut      */ DMG_ENTRY(1, 0x0),
    /* Deku stick    */ DMG_ENTRY(1, 0x0),
    /* Slingshot     */ DMG_ENTRY(1, 0x0),
    /* Explosive     */ DMG_ENTRY(1, SNAPPER_DMGEFF_FLIP),
    /* Boomerang     */ DMG_ENTRY(1, 0x0),
    /* Normal arrow  */ DMG_ENTRY(1, 0x0),
    /* Hammer swing  */ DMG_ENTRY(1, 0x0),
    /* Hookshot      */ DMG_ENTRY(1, 0x0),
    /* Kokiri sword  */ DMG_ENTRY(1, 0x0),
    /* Master sword  */ DMG_ENTRY(1, 0x0),
    /* Giant's Knife */ DMG_ENTRY(1, 0x0),
    /* Fire arrow    */ DMG_ENTRY(1, 0x0),
    /* Ice arrow     */ DMG_ENTRY(1, 0x0),
    /* Light arrow   */ DMG_ENTRY(1, 0x0),
    /* Unk arrow 1   */ DMG_ENTRY(1, 0x0),
    /* Unk arrow 2   */ DMG_ENTRY(1, 0x0),
    /* Unk arrow 3   */ DMG_ENTRY(1, 0x0),
    /* Fire magic    */ DMG_ENTRY(1, 0x0),
    /* Ice magic     */ DMG_ENTRY(1, 0x0),
    /* Light magic   */ DMG_ENTRY(1, 0x0),
    /* Shield        */ DMG_ENTRY(0, 0x0),
    /* Mirror Ray    */ DMG_ENTRY(0, 0x0),
    /* Kokiri spin   */ DMG_ENTRY(1, 0x0),
    /* Giant spin    */ DMG_ENTRY(1, 0x0),
    /* Master spin   */ DMG_ENTRY(1, 0x0),
    /* Kokiri jump   */ DMG_ENTRY(1, 0x0),
    /* Giant jump    */ DMG_ENTRY(1, 0x0),
    /* Master jump   */ DMG_ENTRY(1, 0x0),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(1, SNAPPER_DMGEFF_FLIP),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

// MM ignores these while Gekko is frozen: only a strong blow shatters the ice.
#define GEKKO_ICE_PROOF_DMG                                                                                            \
    (DMG_DEKU_NUT | DMG_DEKU_STICK | DMG_SLINGSHOT | DMG_BOOMERANG | DMG_ARROW_NORMAL | DMG_HOOKSHOT | DMG_ARROW_ICE | \
     DMG_ARROW_LIGHT | DMG_MAGIC_ICE)

static ColliderJntSphElementInit sGekkoElementsInit[2] = {
    {
        { ELEMTYPE_UNK1, { 0xFFCFFFFF, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
        { 1, { { 0, 0, 0 }, 32 }, 100 },
    },
    {
        { ELEMTYPE_UNK0,
          { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x04 },
          { 0xFFCFFFFF, 0x00, 0x00 },
          TOUCH_ON | TOUCH_SFX_WOOD,
          BUMP_NONE,
          OCELEM_NONE },
        { 1, { { 0, 0, 0 }, 25 }, 100 },
    },
};

static ColliderJntSphInit sGekkoColliderInit = {
    { COLTYPE_HIT6, AT_NONE | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    ARRAY_COUNT(sGekkoElementsInit),
    sGekkoElementsInit,
};

static CollisionCheckInfoInit sGekkoColChkInit = { GEKKO_FIRST_HEALTH, 30, 60, 50 };

static ColliderCylinderInit sSnapperColliderInit = {
    { COLTYPE_HARD, AT_NONE | AT_TYPE_ENEMY, AC_ON | AC_HARD | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_KNOCKBACK, 0x04 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_ON | BUMP_HOOKABLE,
      OCELEM_ON },
    { 53, 50, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit sSnapperColChkInit = { 1, 53, 60, 250 };

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sUpVec = { 0.0f, 1.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;
static s16 sSnapperActorId = -1;

static void Gekko_SetupJumpToLink(EnGekko* this);
static void Gekko_SetupMeleeAttack(EnGekko* this);
static void Gekko_SetupJumpToWall(EnGekko* this);
static void Gekko_SetupWallCrawl(EnGekko* this);
static void Gekko_SetupWallPause(EnGekko* this);
static void Gekko_SetupClimbDownWall(EnGekko* this);
static void Gekko_SetupRunToSnapper(EnGekko* this);
static void Gekko_SetupJumpOnSnapper(EnGekko* this);
static void Gekko_SetupScurry(EnGekko* this);
static void Gekko_SetupCallSnapper(EnGekko* this);
static void Gekko_StartDefeat(EnGekko* this);
static void Gekko_LookAround(EnGekko* this, PlayState* play);
static void Gekko_JumpToLink(EnGekko* this, PlayState* play);
static void Gekko_MeleeAttack(EnGekko* this, PlayState* play);
static void Gekko_Damage(EnGekko* this, PlayState* play);
static void Gekko_Stun(EnGekko* this, PlayState* play);
static void Gekko_CallSnapper(EnGekko* this, PlayState* play);
static void Gekko_SnapperSpawn(EnGekko* this, PlayState* play);
static void Gekko_WaitForSnapper(EnGekko* this, PlayState* play);
static void Gekko_RearOnSnapper(EnGekko* this, PlayState* play);
static void Gekko_RearOnSnapperWave(EnGekko* this, PlayState* play);
static void Gekko_RearOnSnapperRise(EnGekko* this, PlayState* play);
static void Gekko_FallOffSnapper(EnGekko* this, PlayState* play);
static void Gekko_JumpToWall(EnGekko* this, PlayState* play);
static void Gekko_Scurry(EnGekko* this, PlayState* play);
static void Gekko_WallCrawl(EnGekko* this, PlayState* play);
static void Gekko_WallPause(EnGekko* this, PlayState* play);
static void Gekko_ClimbDownWall(EnGekko* this, PlayState* play);
static void Gekko_RunToSnapper(EnGekko* this, PlayState* play);
static void Gekko_JumpOnSnapper(EnGekko* this, PlayState* play);
static void Gekko_LandOnSnapper(EnGekko* this, PlayState* play);
static void Gekko_FallInAir(EnGekko* this, PlayState* play);
static void Gekko_FallOnGround(EnGekko* this, PlayState* play);
static void Gekko_DefeatGekko(EnGekko* this, PlayState* play);
static void Gekko_Shrink(EnGekko* this, PlayState* play);
static void Snapper_SetupRearUp(EnSnapper* this);
static void Snapper_SetupRetreat(EnSnapper* this);
static void Snapper_SetupStopRoll(EnSnapper* this);
static void Snapper_SetupEmerge(EnSnapper* this);
static void Snapper_SetupVanish(EnSnapper* this);
static void Snapper_Buried(EnSnapper* this, PlayState* play);
static void Snapper_Rise(EnSnapper* this, PlayState* play);
static void Snapper_WaitForRider(EnSnapper* this, PlayState* play);
static void Snapper_Walk(EnSnapper* this, PlayState* play);
static void Snapper_Retreat(EnSnapper* this, PlayState* play);
static void Snapper_ExtendSpikes(EnSnapper* this, PlayState* play);
static void Snapper_SpinUp(EnSnapper* this, PlayState* play);
static void Snapper_Roll(EnSnapper* this, PlayState* play);
static void Snapper_RetractSpikes(EnSnapper* this, PlayState* play);
static void Snapper_Emerge(EnSnapper* this, PlayState* play);
static void Snapper_FlipInAir(EnSnapper* this, PlayState* play);
static void Snapper_Flipped(EnSnapper* this, PlayState* play);
static void Snapper_RearUp(EnSnapper* this, PlayState* play);
static void Snapper_Vanish(EnSnapper* this, PlayState* play);
static void Snapper_DrawBody(Actor* thisx, PlayState* play);
static void Snapper_DrawShell(Actor* thisx, PlayState* play);

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

static void StartMinibossMusic(EnGekko* this) {
    if (this->isFightStarted) {
        return;
    }
    this->isFightStarted = true;
    func_800F5ACC(NA_BGM_MINI_BOSS);
}

static void StopMinibossMusic(EnGekko* this) {
    if (!this->isFightStarted) {
        return;
    }
    this->isFightStarted = false;
    func_800F5B58();
}

static void RequestQuake(PlayState* play, s16 speed, s16 strength, s16 duration) {
    s16 quake = Quake_Add(GET_ACTIVE_CAM(play), 3);

    Quake_SetSpeed(quake, speed);
    Quake_SetQuakeValues(quake, strength, 0, 0, 0);
    Quake_SetCountdown(quake, duration);
}

static bool NormalizeVec3f(Vec3f* vec) {
    f32 magnitude = Math3D_Vec3fMagnitude(vec);

    if (magnitude < 0.0001f) {
        return false;
    }
    Math_Vec3f_Scale(vec, 1.0f / magnitude);
    return true;
}

// Right-handed rotation about a unit axis, the same as MM's Matrix_RotateAxisF followed by Matrix_MultVec3f.
static void RotateAboutAxis(Vec3f* vec, Vec3f* axis, f32 angle, Vec3f* dest) {
    f32 sin = sinf(angle);
    f32 cos = cosf(angle);
    f32 along = DOTXYZ((*axis), (*vec)) * (1.0f - cos);
    Vec3f cross;

    Math3D_Vec3f_Cross(axis, vec, &cross);
    dest->x = vec->x * cos + cross.x * sin + axis->x * along;
    dest->y = vec->y * cos + cross.y * sin + axis->y * along;
    dest->z = vec->z * cos + cross.z * sin + axis->z * along;
}

static void SpawnRipple(PlayState* play, Actor* actor, s16 radius, s16 radiusMax) {
    Vec3f pos = actor->world.pos;

    pos.y += actor->yDistToWater;
    EffectSsGRipple_Spawn(play, &pos, radius, radiusMax, 0);
}

static void Gekko_SpawnJumpWaterEffects(EnGekko* this, PlayState* play) {
    if (this->actor.yDistToWater <= 0.0f) {
        return;
    }
    Vec3f pos = this->actor.world.pos;

    pos.y += this->actor.yDistToWater;
    EffectSsGRipple_Spawn(play, &pos, 150, 550, 0);
    pos.y += 8.0f;
    EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, 550);
}

static void Gekko_SpawnIdleWaterEffects(EnGekko* this, PlayState* play) {
    if (this->actor.yDistToWater > 0.0f && (play->gameplayFrames % 14) == 0) {
        SpawnRipple(play, &this->actor, 150, 550);
    }
}

static void Gekko_Freeze(EnGekko* this) {
    this->isFrozen = true;
    this->collider.base.colType = COLTYPE_HIT3;
    this->collider.elements[0].info.elemType = ELEMTYPE_UNK0;
}

static void Gekko_Thaw(EnGekko* this, PlayState* play) {
    this->freezeTimer = 0;
    if (!this->isFrozen) {
        return;
    }
    this->isFrozen = false;
    this->collider.base.colType = COLTYPE_HIT6;
    this->collider.elements[0].info.elemType = ELEMTYPE_UNK1;
    this->actor.colorFilterTimer = 0;
    PlaySfx(&this->actor, MM_SE_EV_ICE_BROKEN, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < GEKKO_BODYPART_MAX; i++) {
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &this->bodyPartsPos[i], 150, 150, 150, 250, 235, 245, 255,
                                       1.0f);
    }
}

static void Gekko_ApplyStun(EnGekko* this) {
    this->freezeTimer = 40;
    PlaySfx(&this->actor, MM_SE_EN_COMMON_FREEZE, NA_SE_EN_GOMA_JR_FREEZE);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 40);
}

static void Gekko_ApplyIce(EnGekko* this) {
    Gekko_Freeze(this);
    this->freezeTimer = 80;
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 80);
}

static void Gekko_UpdateSurfaceRotation(EnGekko* this) {
    MtxF basis;

    basis.xx = this->surfaceSide.x;
    basis.yx = this->surfaceSide.y;
    basis.zx = this->surfaceSide.z;
    basis.xy = this->surfaceNormal.x;
    basis.yy = this->surfaceNormal.y;
    basis.zy = this->surfaceNormal.z;
    basis.xz = this->surfaceForward.x;
    basis.yz = this->surfaceForward.y;
    basis.zz = this->surfaceForward.z;
    Matrix_MtxFToYXZRotS(&basis, &this->actor.shape.rot, 0);
    this->actor.world.rot.x = -this->actor.shape.rot.x;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.world.rot.z = this->actor.shape.rot.z;
}

static bool Gekko_AlignToSurface(EnGekko* this, CollisionPoly* poly) {
    Vec3f normal = { COLPOLY_GET_NORMAL(poly->normal.x), COLPOLY_GET_NORMAL(poly->normal.y),
                     COLPOLY_GET_NORMAL(poly->normal.z) };
    f32 cosAngle = DOTXYZ(normal, this->surfaceNormal);

    this->actor.floorPoly = poly;
    if (fabsf(cosAngle) >= 1.0f) {
        return false;
    }
    f32 angle = Math_FAcosF(cosAngle);
    if (angle < 0.0001f) {
        return false;
    }
    Vec3f axis;
    Vec3f side;

    Math3D_Vec3f_Cross(&this->surfaceNormal, &normal, &axis);
    NormalizeVec3f(&axis);
    RotateAboutAxis(&this->surfaceSide, &axis, angle, &side);
    this->surfaceSide = side;
    Math3D_Vec3f_Cross(&this->surfaceSide, &normal, &this->surfaceForward);
    NormalizeVec3f(&this->surfaceForward);
    this->surfaceNormal = normal;
    return true;
}

static void Gekko_MoveAlongSurface(EnGekko* this) {
    f32 speedXZ = Math_CosS(this->actor.world.rot.x) * this->actor.speedXZ;

    this->actor.velocity.x = Math_SinS(this->actor.world.rot.y) * speedXZ;
    this->actor.velocity.y = Math_SinS(this->actor.world.rot.x) * this->actor.speedXZ;
    this->actor.velocity.z = Math_CosS(this->actor.world.rot.y) * speedXZ;
    Actor_UpdatePos(&this->actor);
}

static void Gekko_PlayJumpSounds(EnGekko* this, PlayState* play) {
    if (Animation_OnFrame(&this->skelAnime, 1.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_FROG_JUMP, NA_SE_EV_FROG_JUMP);
    } else if (Animation_OnFrame(&this->skelAnime, 11.0f)) {
        Gekko_SpawnJumpWaterEffects(this, play);
        PlaySfx(&this->actor, MM_SE_EV_WALK_WATER, NA_SE_PL_WALK_WATER1);
    }
}

static void Gekko_HopForward(EnGekko* this, f32 speed) {
    bool isMidHop = this->skelAnime.curFrame > 1.0f && this->skelAnime.curFrame < 12.0f;

    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || isMidHop) {
        this->actor.speedXZ = speed;
    } else {
        this->actor.speedXZ = 0.0f;
    }
}

static EnSnapper* Gekko_GetSnapper(EnGekko* this) {
    return (EnSnapper*)this->actor.child;
}

static void Gekko_PlaceSnapper(EnGekko* this, PlayState* play) {
    Actor* snapper = this->actor.child;
    Vec3f from = { this->actor.world.pos.x, this->actor.world.pos.y + 50.0f, this->actor.world.pos.z };
    Vec3f to = { from.x + Math_SinS(this->actor.shape.rot.y) * SNAPPER_SPAWN_DISTANCE, from.y,
                 from.z + Math_CosS(this->actor.shape.rot.y) * SNAPPER_SPAWN_DISTANCE };
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bgId;

    // A wall between Gekko and the spot pulls the Snapper back in front of it.
    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bgId)) {
        f32 reach = MAX(Math_Vec3f_DistXZ(&from, &hit) - 60.0f, 0.0f);

        to.x = from.x + Math_SinS(this->actor.shape.rot.y) * reach;
        to.z = from.z + Math_CosS(this->actor.shape.rot.y) * reach;
    }
    snapper->world.pos.x = to.x;
    snapper->world.pos.z = to.z;
    snapper->shape.rot.y = BINANG_ROT180(this->actor.shape.rot.y);

    Vec3f above = { to.x, snapper->world.pos.y + 50.0f, to.z };
    Vec3f below = { to.x, snapper->world.pos.y - 150.0f, to.z };

    if (BgCheck_EntityLineTest1(&play->colCtx, &above, &below, &hit, &poly, false, true, false, true, &bgId)) {
        snapper->floorHeight = hit.y;
    } else {
        snapper->floorHeight = this->actor.home.pos.y;
    }
    snapper->world.pos.y = snapper->floorHeight - 60.0f;
}

static void Gekko_SetupRearOnSnapper(EnGekko* this) {
    if (this->actionFunc == Gekko_RearOnSnapperRise) {
        Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoWiggleAnim);
    } else if (this->actionFunc == Gekko_RearOnSnapperWave) {
        Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoLandOnSnapperAnim);
    } else {
        Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoStandingIdleAnim);
    }
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->state = GEKKO_STATE_ON_SNAPPER;
    this->actionFunc = Gekko_RearOnSnapper;
}

static void Gekko_SetupRearOnSnapperWave(EnGekko* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoJumpOnSnapperAnim);
    this->timer = 15;
    this->actionFunc = Gekko_RearOnSnapperWave;
}

static void Gekko_SetupRearOnSnapperRise(EnGekko* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoSqueezeAnim);
    this->timer = 10;
    this->state = GEKKO_STATE_REAR_ON_SNAPPER;
    this->actor.shape.rot.x = 0;
    this->actor.shape.rot.z = 0;
    this->actor.shape.rot.y = this->actor.child->world.rot.y;
    this->actionFunc = Gekko_RearOnSnapperRise;
}

// Dodges an incoming arrow, hookshot or boomerang by waving or rising off the shell.
static void Gekko_RearOnSnapper(EnGekko* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        const char* idle = Rand_ZeroOne() < 0.5f ? sGekkoQuickFistPumpAnim : sGekkoStandingIdleAnim;

        Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)idle);
    }
    Actor* projectile = Actor_GetProjectileActor(play, &this->actor, 60.0f);
    if (projectile == NULL) {
        return;
    }
    Vec3f rearingPoint = { this->actor.world.pos.x, this->actor.world.pos.y + 10.0f, this->actor.world.pos.z };

    if (projectile->world.rot.x < Actor_WorldPitchTowardPoint(projectile, &rearingPoint)) {
        Gekko_SetupRearOnSnapperWave(this);
    } else {
        Gekko_SetupRearOnSnapperRise(this);
    }
}

static void Gekko_RearOnSnapperWave(EnGekko* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime) && this->timer > 0) {
        this->timer--;
        if (this->timer == 0) {
            Gekko_SetupRearOnSnapper(this);
        }
    }
}

static void Gekko_RearOnSnapperRise(EnGekko* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->timer--;
    if (this->timer == 0) {
        Gekko_SetupRearOnSnapper(this);
        return;
    }
    this->actor.world.pos.y = Math_SinS(this->timer * 0xCCC) * 100.0f + Gekko_GetSnapper(this)->bodyHeight + 46.0f;
}

static void Gekko_SetupFallOffSnapper(EnGekko* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoFallInAirAnim);
    this->state = GEKKO_STATE_FALL_OFF_SNAPPER;
    this->actor.speedXZ = 7.0f;
    this->actor.velocity.y = 15.0f;
    this->actor.world.rot.y = BINANG_ROT180(this->actor.child->world.rot.y);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->collider.base.ocFlags1 |= OC1_ON;
    PlaySfx(&this->actor, MM_SE_EN_FROG_DAMAGE, NA_SE_EN_RIZA_DAMAGE);
    this->actionFunc = Gekko_FallOffSnapper;
}

static void Gekko_FallOffSnapper(EnGekko* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->actor.shape.rot.x += 0x800;
    this->actor.shape.rot.z += 0x1000;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Gekko_SetupJumpToWall(this);
    }
}

static void Gekko_SetupJumpToWall(EnGekko* this) {
    Animation_Change(&this->skelAnime, (AnimationHeader*)sGekkoJumpForwardAnim, 2.0f, 0.0f, 0.0f, ANIMMODE_LOOP, -2.0f);
    this->actor.shape.rot.x = 0;
    this->actor.shape.rot.z = 0;
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_WALL;
    this->timer = GEKKO_WALL_SEARCH_FRAMES;
    PlaySfx(&this->actor, MM_SE_EN_FROG_VOICE2, NA_SE_EV_FROG_CRY_1);
    this->actionFunc = Gekko_JumpToWall;
}

static bool Gekko_IsOnClimbableWall(EnGekko* this) {
    return (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) &&
           this->actor.wallBgId == BGCHECK_SCENE && COLPOLY_GET_NORMAL(this->actor.wallPoly->normal.y) < 0.5f;
}

static void Gekko_JumpToWall(EnGekko* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Gekko_PlayJumpSounds(this, play);
    if (Gekko_IsOnClimbableWall(this)) {
        Gekko_SetupWallCrawl(this);
        return;
    }
    bool isOutOfReach = Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > GEKKO_WALL_SEARCH_RADIUS;

    if (DECR(this->timer) == 0 || isOutOfReach) {
        Gekko_SetupScurry(this);
        return;
    }
    Gekko_HopForward(this, 12.0f);
}

// Arena fallback: with no wall to climb, Gekko hops around in the open where a shot can still knock him over.
static void Gekko_SetupScurry(EnGekko* this) {
    Animation_Change(&this->skelAnime, (AnimationHeader*)sGekkoJumpForwardAnim, 2.0f, 0.0f, 0.0f, ANIMMODE_LOOP, -2.0f);
    this->collider.base.acFlags |= AC_ON;
    this->timer = GEKKO_SCURRY_FRAMES;
    PlaySfx(&this->actor, MM_SE_EN_FROG_RUNAWAY, NA_SE_EV_FROG_CRY_0);
    this->actionFunc = Gekko_Scurry;
}

static void Gekko_Scurry(EnGekko* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Gekko_PlayJumpSounds(this, play);
    if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
        s16 awayFromPlayer = BINANG_ROT180(this->actor.yawTowardsPlayer);
        s16 awayFromHome = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
        bool isFarFromHome = Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > 300.0f;

        this->actor.world.rot.y = (isFarFromHome ? awayFromHome : awayFromPlayer) + (s16)Rand_CenteredFloat(0x4000);
        this->actor.shape.rot.y = this->actor.world.rot.y;
    }
    Gekko_HopForward(this, 8.0f);
    if (DECR(this->timer) == 0) {
        this->collider.base.acFlags &= ~AC_ON;
        Gekko_SetupRunToSnapper(this);
    }
}

static void Gekko_SetupWallCrawl(EnGekko* this) {
    if (this->actionFunc == Gekko_JumpToWall) {
        Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sGekkoCrawlAnim);
        this->collider.base.acFlags |= AC_ON;
        this->actor.gravity = 0.0f;
        this->actor.world.pos.y = this->actor.focus.pos.y;
        this->surfaceForward = sUpVec;
        this->actor.colChkInfo.displacement = sZeroVec;
        this->actor.colChkInfo.mass = MASS_IMMOVABLE;
        this->surfaceNormal.x = COLPOLY_GET_NORMAL(this->actor.wallPoly->normal.x);
        this->surfaceNormal.y = COLPOLY_GET_NORMAL(this->actor.wallPoly->normal.y);
        this->surfaceNormal.z = COLPOLY_GET_NORMAL(this->actor.wallPoly->normal.z);
        Math3D_Vec3f_Cross(&this->surfaceNormal, &this->surfaceForward, &this->surfaceSide);
        NormalizeVec3f(&this->surfaceSide);
        Math3D_Vec3f_Cross(&this->surfaceSide, &this->surfaceNormal, &this->surfaceForward);
        NormalizeVec3f(&this->surfaceForward);
        Gekko_UpdateSurfaceRotation(this);
        this->actor.floorPoly = this->actor.wallPoly;
        this->wallPausesLeft = 10;
    } else {
        this->skelAnime.playSpeed = 1.0f;
    }
    PlaySfx(&this->actor, MM_SE_EN_FROG_RUNAWAY, NA_SE_EV_FROG_CRY_0);
    this->actor.speedXZ = 8.0f;
    this->timer = Rand_S16Offset(35, 15);
    this->actionFunc = Gekko_WallCrawl;
}

static bool Gekko_CrawlStep(EnGekko* this, PlayState* play) {
    f32 stride = this->actor.speedXZ * 2.0f;
    Vec3f surfacePoint;
    Vec3f aheadPoint;
    CollisionPoly* surfacePoly = NULL;
    CollisionPoly* aheadPoly = NULL;
    s32 surfaceBgId;
    s32 aheadBgId;
    Vec3f from = { this->actor.world.pos.x + this->surfaceNormal.x * 2.0f,
                   this->actor.world.pos.y + this->surfaceNormal.y * 2.0f,
                   this->actor.world.pos.z + this->surfaceNormal.z * 2.0f };
    Vec3f into = { this->actor.world.pos.x - this->surfaceNormal.x * 25.0f,
                   this->actor.world.pos.y - this->surfaceNormal.y * 25.0f,
                   this->actor.world.pos.z - this->surfaceNormal.z * 25.0f };

    if (!BgCheck_EntityLineTest1(&play->colCtx, &from, &into, &surfacePoint, &surfacePoly, true, true, true, true,
                                 &surfaceBgId)) {
        Gekko_SetupClimbDownWall(this);
        return false;
    }
    Vec3f ahead = { this->surfaceForward.x * stride + from.x, this->surfaceForward.y * stride + from.y,
                    this->surfaceForward.z * stride + from.z };

    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &ahead, &aheadPoint, &aheadPoly, true, true, true, true,
                                &aheadBgId)) {
        this->actor.world.pos = aheadPoint;
        this->actor.floorBgId = aheadBgId;
        this->actor.speedXZ = 0.0f;
        return Gekko_AlignToSurface(this, aheadPoly);
    }
    bool isRealigned = this->actor.floorPoly != surfacePoly && Gekko_AlignToSurface(this, surfacePoly);

    this->actor.world.pos = surfacePoint;
    this->actor.floorBgId = surfaceBgId;
    return isRealigned;
}

static void Gekko_WallCrawl(EnGekko* this, PlayState* play) {
    if (this->freezeTimer > 0) {
        this->freezeTimer--;
        return;
    }
    SkelAnime_Update(&this->skelAnime);
    this->timer--;
    this->actor.speedXZ = 8.0f;
    if (Gekko_CrawlStep(this, play)) {
        Gekko_UpdateSurfaceRotation(this);
    }
    if (this->actionFunc != Gekko_WallCrawl) {
        return;
    }
    if (Animation_OnFrame(&this->skelAnime, 15.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_BOMCHU_WALK, NA_SE_EN_DODO_J_WALK);
    }
    if ((play->gameplayFrames % 60) == 0 && Rand_ZeroOne() < 0.8f) {
        PlaySfx(&this->actor, MM_SE_EN_FROG_REAL, NA_SE_EV_FROG_CRY_0);
    }
    bool isHeadingDownLow = this->actor.world.pos.y < this->actor.home.pos.y + 200.0f && this->surfaceForward.y <= 0.0f;

    if (this->timer != 0 && !isHeadingDownLow) {
        return;
    }
    this->wallPausesLeft--;
    if (this->wallPausesLeft == 0) {
        Gekko_SetupClimbDownWall(this);
    } else {
        Gekko_SetupWallPause(this);
    }
}

static void Gekko_SetupWallPause(EnGekko* this) {
    this->actor.speedXZ = 0.0f;
    this->skelAnime.playSpeed = 1.5f;
    if (this->timer != 0) {
        this->wallRotation = this->surfaceSide.y > 0.0f ? (M_PI / 30) : (-M_PI / 30);
    } else {
        f32 turn = 0x1000 + Rand_ZeroFloat(0x2000);

        this->wallRotation = (Rand_ZeroOne() < 0.5f ? -1 : 1) * turn * (M_PI / (15 * 0x8000));
    }
    this->timer = 15;
    PlaySfx(&this->actor, MM_SE_EN_FROG_RUNAWAY2, NA_SE_EV_FROG_CRY_1);
    this->actionFunc = Gekko_WallPause;
}

static void Gekko_WallPause(EnGekko* this, PlayState* play) {
    if (this->freezeTimer > 0) {
        this->freezeTimer--;
        return;
    }
    Vec3f forward;

    SkelAnime_Update(&this->skelAnime);
    this->timer--;
    RotateAboutAxis(&this->surfaceForward, &this->surfaceNormal, this->wallRotation, &forward);
    this->surfaceForward = forward;
    Math3D_Vec3f_Cross(&this->surfaceNormal, &this->surfaceForward, &this->surfaceSide);
    Gekko_UpdateSurfaceRotation(this);
    if ((play->gameplayFrames % 60) == 0 && Rand_ZeroOne() < 0.8f) {
        PlaySfx(&this->actor, MM_SE_EN_FROG_REAL, NA_SE_EV_FROG_CRY_0);
    }
    if (this->timer == 0) {
        Gekko_SetupWallCrawl(this);
    }
}

static void Gekko_SetupClimbDownWall(EnGekko* this) {
    AnimationHeader* jump = (AnimationHeader*)sGekkoJumpForwardAnim;

    Animation_Change(&this->skelAnime, jump, 0.0f, 0.0f, Animation_GetLastFrame(jump), ANIMMODE_ONCE, 0.0f);
    this->actor.shape.rot.y = Actor_WorldYawTowardActor(&this->actor, this->actor.child);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.shape.rot.x = 0;
    this->actor.shape.rot.z = 0;
    this->actor.colChkInfo.mass = 50;
    this->actor.speedXZ = 5.0f;
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -1.0f;

    s16 towardHome = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);

    this->actor.world.pos.x += 30.0f * Math_SinS(towardHome);
    this->actor.world.pos.z += 30.0f * Math_CosS(towardHome);
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    this->state = GEKKO_STATE_RETURN_TO_SNAPPER;
    this->actionFunc = Gekko_ClimbDownWall;
}

static void Gekko_ClimbDownWall(EnGekko* this, PlayState* play) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Gekko_SetupRunToSnapper(this);
        return;
    }
    if (this->actor.floorHeight != BGCHECK_Y_MIN) {
        return;
    }
    s16 towardHome = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);

    this->actor.world.pos.x += 5.0f * Math_SinS(towardHome);
    this->actor.world.pos.z += 5.0f * Math_CosS(towardHome);
    if (this->actor.world.pos.y < this->actor.home.pos.y - 5.0f) {
        this->actor.world.pos.y = this->actor.home.pos.y;
    }
}

static void Gekko_SetupRunToSnapper(EnGekko* this) {
    Animation_Change(&this->skelAnime, (AnimationHeader*)sGekkoJumpForwardAnim, 2.0f, 0.0f, 0.0f, ANIMMODE_LOOP, -2.0f);
    this->state = GEKKO_STATE_RETURN_TO_SNAPPER;
    this->actionFunc = Gekko_RunToSnapper;
}

static void Gekko_RunToSnapper(EnGekko* this, PlayState* play) {
    EnSnapper* snapper = Gekko_GetSnapper(this);

    SkelAnime_Update(&this->skelAnime);
    Gekko_PlayJumpSounds(this, play);
    this->actor.shape.rot.y = Actor_WorldYawTowardActor(&this->actor, &snapper->actor);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    Gekko_HopForward(this, 12.0f);
    if (snapper->isUpright && Actor_WorldDistXZToActor(&this->actor, &snapper->actor) < 120.0f &&
        Animation_OnFrame(&this->skelAnime, 0.0f)) {
        Gekko_SetupJumpOnSnapper(this);
    }
}

static void Gekko_SetupJumpOnSnapper(EnGekko* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoStandingIdleAnim, 6.0f);
    this->timer = 6;
    this->collider.base.ocFlags1 &= ~OC1_ON;
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.shape.rot.y = Actor_WorldYawTowardActor(&this->actor, this->actor.child);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->state = GEKKO_STATE_JUMP_ON_SNAPPER;
    PlaySfx(&this->actor, MM_SE_EN_FROG_GREET, NA_SE_EV_FROG_CRY_0);
    this->actionFunc = Gekko_JumpOnSnapper;
}

static void Gekko_SetupLandOnSnapper(EnGekko* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoFullFistPumpAnim);
    this->actor.shape.rot.y = this->actor.child->shape.rot.y;
    this->state = GEKKO_STATE_ON_SNAPPER;
    this->actionFunc = Gekko_LandOnSnapper;
}

static void Gekko_JumpOnSnapper(EnGekko* this, PlayState* play) {
    EnSnapper* snapper = Gekko_GetSnapper(this);

    SkelAnime_Update(&this->skelAnime);
    this->timer--;
    if (this->timer == 0) {
        Gekko_SetupLandOnSnapper(this);
        return;
    }
    f32 lerp = 1.0f / this->timer;

    this->actor.world.pos.x -= (this->actor.world.pos.x - snapper->actor.world.pos.x) * lerp;
    this->actor.world.pos.z -= (this->actor.world.pos.z - snapper->actor.world.pos.z) * lerp;
    this->actor.world.pos.y = Math_SinS((-this->timer * 0x1000) + 0x6000) * 65.0f + snapper->bodyHeight;
}

static void Gekko_LandOnSnapper(EnGekko* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        Gekko_SetupRearOnSnapper(this);
    }
}

static void Gekko_SetupFallInAir(EnGekko* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoFallInAirAnim);
    if (this->actor.colChkInfo.health > 0) {
        this->state = GEKKO_STATE_RETURN_TO_SNAPPER;
        PlaySfx(&this->actor, MM_SE_EN_FROG_DAMAGE, NA_SE_EN_RIZA_DAMAGE);
    } else {
        PlaySfx(&this->actor, MM_SE_EN_FROG_DEAD, NA_SE_EN_RIZA_DEAD);
    }
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->collider.base.acFlags &= ~AC_ON;
    this->timer = 10;
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 16);

    s16 towardHome = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);

    this->actor.world.pos.x += 30.0f * Math_SinS(towardHome);
    this->actor.world.pos.z += 30.0f * Math_CosS(towardHome);
    this->actionFunc = Gekko_FallInAir;
}

static void Gekko_SetupFallOnGround(EnGekko* this, PlayState* play) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoFallOnGroundAnim);
    this->actor.shape.rot.x = 0;
    this->actor.shape.rot.y += this->spinYaw;
    this->actor.shape.rot.z = 0;
    this->spinYaw = 0;
    this->timer = 5;
    Gekko_Thaw(this, play);
    Gekko_SpawnJumpWaterEffects(this, play);
    PlaySfx(&this->actor, MM_SE_EV_WALK_WATER, NA_SE_PL_WALK_WATER1);
    this->actionFunc = Gekko_FallOnGround;
}

static void Gekko_FallInAir(EnGekko* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->actor.colorFilterTimer = 16;
    if (this->timer > 0) {
        this->timer--;
        if (this->timer == 0) {
            this->actor.gravity = -1.0f;
            this->actor.colChkInfo.mass = 50;
        }
        return;
    }
    this->spinYaw += 0xF00;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Gekko_SetupFallOnGround(this, play);
    }
}

static void Gekko_FallOnGround(EnGekko* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (this->skelAnime.animation != sGekkoFallOnGroundAnim) {
        Gekko_SetupRunToSnapper(this);
        return;
    }
    if (this->actor.colChkInfo.health > 0) {
        Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoRecoverAnim);
        return;
    }
    this->timer--;
    if (this->timer == 0) {
        Gekko_StartDefeat(this);
    }
}

static void Gekko_StartDefeat(EnGekko* this) {
    this->state = GEKKO_STATE_DEFEAT;
    this->timer = 38;
    this->actionFunc = Gekko_DefeatGekko;
}

static void Gekko_SetupShrink(EnGekko* this) {
    this->timer = 20;
    this->actionFunc = Gekko_Shrink;
}

static void Gekko_DefeatGekko(EnGekko* this, PlayState* play) {
    this->actor.colorFilterTimer = 16;
    if (DECR(this->timer) == 0) {
        Gekko_SetupShrink(this);
    }
}

static void Gekko_Vanish(EnGekko* this, PlayState* play) {
    static Vec3f sDebrisAccel = { 0.0f, -0.5f, 0.0f };
    static Color_RGBA8 sSmokePrim = { 250, 250, 250, 255 };
    static Color_RGBA8 sSmokeEnv = { 180, 180, 180, 255 };
    s16 towardCamera = BINANG_ROT180(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)));
    Vec3f smokePos = { Math_SinS(towardCamera) * 20.0f + this->actor.world.pos.x, this->actor.world.pos.y + 25.0f,
                       Math_CosS(towardCamera) * 20.0f + this->actor.world.pos.z };

    this->collider.base.ocFlags1 &= ~OC1_ON;
    func_8002829C(play, &smokePos, &sZeroVec, &sZeroVec, &sSmokePrim, &sSmokeEnv, 800, 50);
    PlaySfx(&this->actor, MM_SE_EN_NPC_APPEAR, NA_SE_EN_EXTINCT);
    for (s32 i = 0; i < 25; i++) {
        Vec3f velocity = { Rand_CenteredFloat(5.0f), Rand_ZeroFloat(3.0f) + 4.0f, Rand_CenteredFloat(5.0f) };

        EffectSsHahen_Spawn(play, &this->actor.world.pos, &velocity, &sDebrisAccel, 0, Rand_S16Offset(12, 3),
                            HAHEN_OBJECT_DEFAULT, 10, NULL);
    }
    Item_DropCollectible(play, &this->actor.world.pos, ITEM00_RUPEE_PURPLE);
    Item_DropCollectible(play, &this->actor.world.pos, ITEM00_HEART);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

static void Gekko_Shrink(EnGekko* this, PlayState* play) {
    this->timer--;
    Actor_SetScale(&this->actor, this->timer * 0.00035f);
    this->actor.colorFilterTimer = 16;
    if (this->timer == 0) {
        Gekko_Vanish(this, play);
    }
}

static void Gekko_SetupLookAround(EnGekko* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoLookAroundAnim);
    this->collider.base.atFlags &= ~AT_ON;
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Gekko_LookAround;
}

static void Gekko_LookAround(EnGekko* this, PlayState* play) {
    Gekko_SpawnIdleWaterEffects(this, play);
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 5, 0x400, 0x80);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    if (SkelAnime_Update(&this->skelAnime) && !Play_InCsMode(play)) {
        StartMinibossMusic(this);
        Gekko_SetupJumpToLink(this);
    }
}

static void Gekko_SetupJumpToLink(EnGekko* this) {
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sGekkoJumpForwardAnim);
    this->collider.base.acFlags |= AC_ON;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Gekko_JumpToLink;
}

static void Gekko_JumpToLink(EnGekko* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 5, 0x1000, 0x80);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    SkelAnime_Update(&this->skelAnime);
    Gekko_PlayJumpSounds(this, play);
    Gekko_HopForward(this, 8.0f);
    bool isTouchingPlayer = (this->collider.base.ocFlags1 & OC1_HIT) && (this->collider.base.ocFlags2 & OC2_HIT_PLAYER);
    bool isBetweenHops = this->skelAnime.curFrame <= 2.0f || this->skelAnime.curFrame >= 11.0f;

    if (isTouchingPlayer && isBetweenHops && Actor_IsFacingPlayer(&this->actor, 0x3000)) {
        Gekko_SetupMeleeAttack(this);
    }
}

static void Gekko_SetupMeleeAttack(EnGekko* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoBoxingStanceAnim);
    this->timer = 7;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Gekko_MeleeAttack;
}

static bool Gekko_IsPunchLanding(EnGekko* this) {
    const void* anim = this->skelAnime.animation;

    return (anim == sGekkoJabPunchAnim && Animation_OnFrame(&this->skelAnime, 2.0f)) ||
           (anim == sGekkoHookPunchAnim && Animation_OnFrame(&this->skelAnime, 9.0f)) ||
           (anim == sGekkoKickAnim && Animation_OnFrame(&this->skelAnime, 2.0f)) ||
           (anim == sGekkoWindupPunchAnim && Animation_OnFrame(&this->skelAnime, 27.0f));
}

static void Gekko_MeleeAttack(EnGekko* this, PlayState* play) {
    Gekko_SpawnIdleWaterEffects(this, play);
    if (SkelAnime_Update(&this->skelAnime)) {
        this->timer--;
        if (this->timer == 0) {
            Gekko_SetupLookAround(this);
        } else if (this->timer == 6) {
            Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoBoxingStanceAnim);
        } else {
            Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoAttackAnims[(s32)Rand_ZeroFloat(4.0f) % 4]);
        }
    }
    if (!Gekko_IsPunchLanding(this)) {
        this->collider.base.atFlags &= ~AT_ON;
        return;
    }
    this->collider.base.atFlags |= AT_ON;
    if (this->skelAnime.animation == sGekkoKickAnim) {
        PlaySfx(&this->actor, MM_SE_EN_FROG_KICK, NA_SE_EN_RIZA_ATTACK);
    } else {
        PlaySfx(&this->actor, MM_SE_EN_FROG_PUNCH1, NA_SE_EN_RIZA_ATTACK);
    }
}

static void Gekko_SetupDamage(EnGekko* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoKnockbackAnim, -3.0f);
    this->timer = 20;
    this->collider.base.atFlags &= ~AT_ON;
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.speedXZ = 10.0f;
    PlaySfx(&this->actor, MM_SE_EN_FROG_DAMAGE, NA_SE_EN_RIZA_DAMAGE);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 20);

    Actor* attacker = this->collider.base.ac;

    if (attacker != NULL) {
        this->actor.world.rot.y = Actor_WorldYawTowardActor(attacker, &this->actor);
    }
    this->actor.shape.rot.y = BINANG_ROT180(this->actor.world.rot.y);
    this->actionFunc = Gekko_Damage;
}

static void Gekko_Damage(EnGekko* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->timer--;
    Math_StepToF(&this->actor.speedXZ, 0.0f, 0.5f);
    if (this->timer != 0) {
        return;
    }
    if (this->actor.colChkInfo.health > 0) {
        Gekko_SetupJumpToLink(this);
    } else {
        Gekko_SetupCallSnapper(this);
    }
}

static void Gekko_SetupStun(EnGekko* this) {
    if (this->skelAnime.animation == sGekkoJumpForwardAnim) {
        this->skelAnime.curFrame = 0.0f;
        SkelAnime_Update(&this->skelAnime);
    }
    this->collider.base.atFlags &= ~AT_ON;
    this->actor.speedXZ = 0.0f;
    if (this->actor.colChkInfo.health == 0) {
        this->collider.base.acFlags &= ~AC_ON;
    }
    this->actionFunc = Gekko_Stun;
}

// MM always ends an ice freeze in its knock-off cutscene; here only a killing blow does.
static void Gekko_Stun(EnGekko* this, PlayState* play) {
    this->freezeTimer--;
    if (this->freezeTimer == 78 && this->actor.colChkInfo.health == 0) {
        Gekko_Thaw(this, play);
        Gekko_SetupCallSnapper(this);
    } else if (this->freezeTimer == 0) {
        Gekko_Thaw(this, play);
        Gekko_SetupJumpToLink(this);
    }
}

static void Gekko_SetupCallSnapper(EnGekko* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sGekkoCallAnim, 3.0f);
    PlaySfx(&this->actor, MM_SE_EN_FROG_GREET, NA_SE_EV_FROG_CRY_0);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.colChkInfo.health = GEKKO_MOUNTED_HEALTH;
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->collider.base.acFlags &= ~AC_ON;
    this->actionFunc = Gekko_CallSnapper;
}

static void Gekko_SetupSnapperSpawn(EnGekko* this, PlayState* play) {
    Gekko_PlaceSnapper(this, play);
    RequestQuake(play, 18000, 2, 15);
    Rumble_Request(this->actor.xyzDistToPlayerSq, 120, 20, 10);
    this->timer = 40;
    this->actionFunc = Gekko_SnapperSpawn;
}

static void Gekko_CallSnapper(EnGekko* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        Gekko_SetupSnapperSpawn(this, play);
    }
}

static void Gekko_SnapperSpawn(EnGekko* this, PlayState* play) {
    this->timer--;
    if (this->timer != 0) {
        Rumble_Request(this->actor.xyzDistToPlayerSq, 120, 20, 10);
        return;
    }
    this->state = GEKKO_STATE_GET_SNAPPER;
    RequestQuake(play, 20000, 17, 12);
    Rumble_Request(this->actor.xyzDistToPlayerSq, 255, 20, 150);
    this->actionFunc = Gekko_WaitForSnapper;
}

static void Gekko_WaitForSnapper(EnGekko* this, PlayState* play) {
    if (this->state != GEKKO_STATE_INIT_SNAPPER) {
        return;
    }
    this->isFightStarted = false;
    StartMinibossMusic(this);
    Gekko_SetupRunToSnapper(this);
}

static void Gekko_ApplyMountedDamage(EnGekko* this, u8 effect) {
    if (Actor_ApplyDamage(&this->actor) == 0) {
        this->collider.base.acFlags &= ~AC_ON;
        Enemy_StartFinishingBlow(gPlayState, &this->actor);
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        StopMinibossMusic(this);
        Gekko_SetupFallInAir(this);
        return;
    }
    if (effect == GEKKO_DMGEFF_STUN) {
        Gekko_ApplyStun(this);
        return;
    }
    if (effect == GEKKO_DMGEFF_ICE) {
        Gekko_Freeze(this);
    }
    Gekko_SetupFallInAir(this);
}

static void Gekko_ApplyFirstPhaseDamage(EnGekko* this, PlayState* play, u8 effect) {
    if (Actor_ApplyDamage(&this->actor) == 0) {
        StopMinibossMusic(this);
    }
    if (effect == GEKKO_DMGEFF_STUN) {
        Gekko_ApplyStun(this);
        Gekko_SetupStun(this);
        return;
    }
    if (effect == GEKKO_DMGEFF_ICE) {
        Gekko_ApplyIce(this);
        Gekko_SetupStun(this);
        return;
    }
    Gekko_Thaw(this, play);
    Gekko_SetupDamage(this);
}

static void Gekko_CheckDamage(EnGekko* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    ColliderInfo* hitBy = this->collider.elements[0].info.acHitInfo;

    if (this->isFrozen && hitBy != NULL && (hitBy->toucher.dmgFlags & GEKKO_ICE_PROOF_DMG)) {
        return;
    }
    u8 effect = this->actor.colChkInfo.damageEffect;

    if (this->state == GEKKO_STATE_PRE_SNAPPER) {
        Gekko_ApplyFirstPhaseDamage(this, play, effect);
    } else {
        Gekko_ApplyMountedDamage(this, effect);
    }
}

static void Gekko_SpawnSnapper(EnGekko* this, PlayState* play) {
    Actor* snapper = Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, sSnapperActorId, this->actor.world.pos.x,
                                        this->actor.world.pos.y, this->actor.world.pos.z, 0, 0, 0, 0);

    if (snapper == NULL) {
        Actor_Kill(&this->actor);
    }
}

static bool Gekko_IsMovingOnItsOwn(EnGekko* this) {
    return this->actionFunc != Gekko_JumpOnSnapper && this->actionFunc != Gekko_RearOnSnapperRise;
}

static void Gekko_Move(EnGekko* this, PlayState* play) {
    if (!Gekko_IsMovingOnItsOwn(this)) {
        return;
    }
    if (this->actor.gravity < -0.1f) {
        f32 wallCheckRadius = this->actionFunc == Gekko_FallInAir ? 3.0f : 15.0f;

        Actor_MoveXZGravity(&this->actor);
        Actor_UpdateBgCheckInfo(play, &this->actor, 25.0f, wallCheckRadius, 3.0f, BGCHECK_UPDATE_WALL_CEILING_FLOOR);
        return;
    }
    if (this->freezeTimer == 0) {
        Gekko_MoveAlongSurface(this);
        this->actor.floorHeight = this->actor.world.pos.y;
    }
}

static void Gekko_SubmitColliders(EnGekko* this, PlayState* play) {
    if (this->collider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.ocFlags1 & OC1_ON) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
}

static void EnGekko_Init(Actor* thisx, PlayState* play) {
    EnGekko* this = (EnGekko*)thisx;

    Actor_SetScale(&this->actor, 0.007f);
    this->actor.gravity = -1.0f;
    this->actor.targetArrowOffset = -13221.0f / 1000.0f;
    this->actor.targetMode = 5;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 55.0f);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sGekkoDamageTable, &sGekkoColChkInit);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sGekkoSkel,
                       (AnimationHeader*)sGekkoBoxingStanceAnim, this->jointTable, this->morphTable, GEKKO_LIMB_MAX);
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sGekkoColliderInit, this->colliderElements);
    for (s32 i = 0; i < 2; i++) {
        this->collider.elements[i].dim.worldSphere.radius = this->collider.elements[i].dim.modelSphere.radius;
    }
    this->state = GEKKO_STATE_PRE_SNAPPER;
    Gekko_SetupLookAround(this);
}

static void EnGekko_Destroy(Actor* thisx, PlayState* play) {
    EnGekko* this = (EnGekko*)thisx;

    StopMinibossMusic(this);
    Collider_DestroyJntSph(play, &this->collider);
}

// The Snapper spawns on the first update so that an enemy killed right after Init leaves no orphan behind.
static void EnGekko_Update(Actor* thisx, PlayState* play) {
    EnGekko* this = (EnGekko*)thisx;

    if (this->actor.child == NULL) {
        Gekko_SpawnSnapper(this, play);
        if (this->actor.update == NULL) {
            return;
        }
    }
    if (this->state == GEKKO_STATE_KNOCKED_OFF) {
        Gekko_SetupFallOffSnapper(this);
    } else {
        Gekko_CheckDamage(this, play);
    }
    this->actionFunc(this, play);
    Gekko_Move(this, play);
    Gekko_SubmitColliders(this, play);
}

static void EnGekko_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnGekko* this = (EnGekko*)thisx;

    if (limbIndex == GEKKO_LIMB_HEAD) {
        Vec3f fistOffset = { 0.0f, 2500.0f, 0.0f };
        Sphere16* fist = &this->collider.elements[1].dim.worldSphere;

        Matrix_MultVec3f(&sZeroVec, &this->actor.focus.pos);
        this->actor.focus.rot.y = this->actor.shape.rot.y;
        Matrix_MultVec3f(&fistOffset, &this->collider.elements[0].dim.worldSphere.center);
        fist->center.x = this->actor.focus.pos.x + Math_SinS(this->actor.shape.rot.y) * 35.0f;
        fist->center.y = this->actor.focus.pos.y - 10.0f;
        fist->center.z = this->actor.focus.pos.z + Math_CosS(this->actor.shape.rot.y) * 35.0f;
    }
    s8 bodyPart = sGekkoLimbToBodyPart[limbIndex];

    if (bodyPart != GEKKO_BODYPART_NONE) {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[bodyPart]);
    }
}

static void EnGekko_Draw(Actor* thisx, PlayState* play) {
    EnGekko* this = (EnGekko*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY(BINANG_TO_RAD(this->spinYaw), MTXMODE_APPLY);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          EnGekko_PostLimbDraw, this);
}

static void Snapper_UpdateBlink(EnSnapper* this) {
    if (this->blinkIndex != 0) {
        this->blinkIndex = (this->blinkIndex + 1) % ARRAY_COUNT(sSnapperEyeTextures);
    } else if (Rand_ZeroOne() < 0.05f) {
        this->blinkIndex = 1;
    }
}

static EnGekko* Snapper_GetRider(EnSnapper* this) {
    return (EnGekko*)this->actor.parent;
}

static void Snapper_CarryRider(EnSnapper* this) {
    EnGekko* rider = Snapper_GetRider(this);

    if (rider->state == GEKKO_STATE_ON_SNAPPER) {
        rider->actor.shape.rot.y = this->actor.shape.rot.y;
        rider->actor.shape.rot.z = this->actor.shape.rot.z;
        Matrix_SetTranslateRotateYXZ(this->actor.world.pos.x, this->bodyHeight, this->actor.world.pos.z,
                                     &this->actor.shape.rot);
        Vec3f seat = { 0.0f, 46.0f, 0.0f };

        Matrix_MultVec3f(&seat, &rider->actor.world.pos);
    } else if (rider->state == GEKKO_STATE_REAR_ON_SNAPPER) {
        rider->actor.world.pos.x = this->actor.world.pos.x;
        rider->actor.world.pos.z = this->actor.world.pos.z;
    }
}

static void Snapper_LaunchDebris(EnSnapper* this, SnapperDebris* debris, Vec3f* origin, s16 yaw, s16 pitch) {
    f32 speed = Rand_ZeroFloat(5.0f) + 7.0f;

    debris->velocity.x = speed * Math_CosS(pitch) * Math_SinS(yaw);
    debris->velocity.y = speed * Math_SinS(pitch) + 3.0f;
    debris->velocity.z = speed * Math_CosS(pitch) * Math_CosS(yaw);
    debris->rot.x = (s32)Rand_Next() >> 0x10;
    debris->rot.y = (s32)Rand_Next() >> 0x10;
    debris->rot.z = (s32)Rand_Next() >> 0x10;
    debris->pos = *origin;
    debris->scale = Rand_ZeroFloat(0.0025f) + 0.002f;
}

static void Snapper_BurstFromGround(EnSnapper* this) {
    this->debrisTimer = 40;
    for (s32 i = 0; i < SNAPPER_DEBRIS_COUNT; i++) {
        s16 yaw = (s32)Rand_Next() >> 0x10;
        Vec3f origin = { Math_SinS(yaw) * 40.0f + this->actor.world.pos.x, this->actor.floorHeight,
                         Math_CosS(yaw) * 40.0f + this->actor.world.pos.z };

        Snapper_LaunchDebris(this, &this->debris[i], &origin, yaw, Rand_S16Offset(0x1800, 0x2800));
    }
    PlaySfx(&this->actor, MM_SE_EN_B_PAMET_BREAK, NA_SE_EV_WALL_BROKEN);
}

static void Snapper_BurstFromWall(EnSnapper* this) {
    s16 awayFromWall = this->actor.wallYaw + 0x8000;
    Vec3f impact = { Math_SinS(awayFromWall) * 50.0f + this->actor.world.pos.x, this->actor.world.pos.y,
                     Math_CosS(awayFromWall) * 50.0f + this->actor.world.pos.z };

    this->debrisTimer = 40;
    for (s32 i = 0; i < SNAPPER_DEBRIS_COUNT; i++) {
        s16 yaw = (s32)Rand_CenteredFloat(0x5000) + this->actor.wallYaw;
        s16 pitch = Rand_S16Offset(0x1000, 0x3000);
        s16 sideways =
            (s16)(yaw - this->actor.wallYaw) > 0 ? this->actor.wallYaw + 0x4000 : this->actor.wallYaw - 0x4000;
        Vec3f origin = { Math_SinS(sideways) * (60.0f * Math_CosS(pitch)) + impact.x,
                         60.0f * Math_SinS(pitch) + impact.y,
                         Math_CosS(sideways) * (60.0f * Math_CosS(pitch)) + impact.z };

        Snapper_LaunchDebris(this, &this->debris[i], &origin, yaw, pitch);
    }
    PlaySfx(&this->actor, MM_SE_EN_B_PAMET_BREAK, NA_SE_EV_WALL_BROKEN);
}

static void Snapper_SpawnRiseDust(EnSnapper* this, PlayState* play) {
    f32 waterSurface = this->actor.yDistToWater + this->actor.world.pos.y;

    for (s32 i = 0; i < 2; i++) {
        f32 radius = Rand_ZeroFloat(30.0f) + 30.0f;
        s16 yaw = Rand_Next() >> 0x10;
        Vec3f pos = { Math_SinS(yaw) * radius + this->actor.world.pos.x,
                      Rand_ZeroFloat(10.0f) + this->actor.floorHeight + 8.0f,
                      Math_CosS(yaw) * radius + this->actor.world.pos.z };
        CollisionPoly* poly;
        s32 bgId;

        if (BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bgId, &this->actor, &pos) < waterSurface) {
            pos.y = waterSurface;
            EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, Rand_S16Offset(1000, 200));
        } else {
            func_800287AC(play, &pos, &sUpVec, &sZeroVec, Rand_S16Offset(950, 100), Rand_S16Offset(10, 10), 20);
        }
    }
}

static void Snapper_SpawnWallDust(EnSnapper* this, PlayState* play) {
    f32 alongX = Math_SinS(this->actor.wallYaw + 0x4000);
    f32 alongZ = Math_CosS(this->actor.wallYaw + 0x4000);
    Vec3f center = { Math_SinS(this->actor.wallYaw + 0x8000) * 50.0f + this->actor.world.pos.x, this->actor.world.pos.y,
                     Math_CosS(this->actor.wallYaw + 0x8000) * 50.0f + this->actor.world.pos.z };

    center.x += (this->actor.world.pos.x - center.x) * 0.3f;
    center.z += (this->actor.world.pos.z - center.z) * 0.3f;
    for (s32 i = 0; i < 4; i++) {
        f32 radius = Rand_ZeroFloat(60.0f) + 50.0f;
        s16 angle = Rand_Next() >> 0x11;
        Vec3f pos = { Math_CosS(angle) * radius * alongX + center.x, Math_SinS(angle) * radius + center.y,
                      Math_CosS(angle) * radius * alongZ + center.z };

        func_800287AC(play, &pos, &sZeroVec, &sZeroVec, Rand_S16Offset(950, 100), Rand_S16Offset(20, 10), 20);
    }
}

static void Snapper_SpawnSplash(EnSnapper* this, PlayState* play) {
    if (this->actor.yDistToWater <= 0.0f) {
        return;
    }
    Vec3f pos = this->actor.world.pos;

    pos.y += this->actor.yDistToWater;
    EffectSsGRipple_Spawn(play, &pos, 500, 900, 0);
    pos.y += 8.0f;
    if (this->actionFunc != Snapper_FlipInAir) {
        s16 yaw = (s32)Rand_CenteredFloat(0x8000) + this->actor.world.rot.y;

        pos.x -= 55.0f * Math_SinS(yaw);
        pos.z -= 55.0f * Math_CosS(yaw);
    }
    EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, Rand_S16Offset(1400, 200));
}

static void Snapper_SpawnIdleRipples(EnSnapper* this, PlayState* play) {
    if (this->actor.yDistToWater > 0.0f && (play->gameplayFrames % 14) == 0) {
        SpawnRipple(play, &this->actor, 500, 900);
    }
}

static void Snapper_SetupBuried(EnSnapper* this) {
    this->actor.draw = NULL;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = Snapper_Buried;
}

static void Snapper_SetupRise(EnSnapper* this) {
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sSnapperIdleAnim);
    this->actor.draw = Snapper_DrawBody;
    this->timer = 20;
    this->actionFunc = Snapper_Rise;
}

static void Snapper_Buried(EnSnapper* this, PlayState* play) {
    if (Snapper_GetRider(this)->state != GEKKO_STATE_GET_SNAPPER) {
        return;
    }
    WaterBox* waterBox;
    f32 waterSurface;

    if (WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &waterSurface,
                             &waterBox)) {
        this->actor.yDistToWater = waterSurface - this->actor.world.pos.y;
    } else {
        this->actor.yDistToWater = this->actor.world.pos.y;
    }
    Snapper_BurstFromGround(this);
    Snapper_SpawnRiseDust(this, play);
    Snapper_SetupRise(this);
}

static void Snapper_SetupWaitForRider(EnSnapper* this) {
    Snapper_GetRider(this)->state = GEKKO_STATE_INIT_SNAPPER;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Snapper_WaitForRider;
}

static void Snapper_Rise(EnSnapper* this, PlayState* play) {
    if (Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.floorHeight, 0.7f, 30.0f, 10.0f) >= 1.0f) {
        Snapper_SpawnRiseDust(this, play);
        return;
    }
    this->timer--;
    if (this->timer >= 16) {
        Snapper_SpawnRiseDust(this, play);
    }
    if (this->timer == 0) {
        Snapper_SetupWaitForRider(this);
    }
}

static void Snapper_WaitForRider(EnSnapper* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Snapper_GetRider(this)->state == GEKKO_STATE_ON_SNAPPER) {
        Snapper_SetupRearUp(this);
    }
}

static void Snapper_SetupWalk(EnSnapper* this) {
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sSnapperWalkAnim);
    this->actor.speedXZ = 1.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->isUpright = true;
    this->actionFunc = Snapper_Walk;
}

static void Snapper_Walk(EnSnapper* this, PlayState* play) {
    u8 riderState = Snapper_GetRider(this)->state;

    SkelAnime_Update(&this->skelAnime);
    if (riderState == GEKKO_STATE_RETURN_TO_SNAPPER) {
        Math_ScaledStepToS(&this->actor.shape.rot.y, Actor_WorldYawTowardActor(&this->actor, this->actor.parent),
                           0x400);
        this->actor.world.rot.y = this->actor.shape.rot.y;
    } else if (riderState == GEKKO_STATE_JUMP_ON_SNAPPER) {
        this->actor.speedXZ = 0.0f;
    } else if (riderState == GEKKO_STATE_ON_SNAPPER) {
        Snapper_SetupRearUp(this);
    }
}

static void Snapper_SetupRetreat(EnSnapper* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperRetreatIntoShellAnim, -3.0f);
    this->timer = 0;
    this->limbScaleY = 1.0f;
    this->limbScaleXZ = 1.0f;
    this->actor.speedXZ = 0.0f;
    PlaySfx(&this->actor, MM_SE_EN_B_PAMET_VOICE, NA_SE_EN_DODO_M_CRY);
    PlaySfx(this->actor.parent, MM_SE_EN_FROG_VOICE1, NA_SE_EV_FROG_CRY_1);
    this->actionFunc = Snapper_Retreat;
}

static void Snapper_SetupExtendSpikes(EnSnapper* this) {
    this->actor.draw = Snapper_DrawShell;
    this->limbScaleY = 0.5f;
    this->actor.speedXZ = 0.0f;
    PlaySfx(&this->actor, MM_SE_EN_PAMET_CUTTER_ON, NA_SE_IT_SWORD_PICKOUT);
    this->actionFunc = Snapper_ExtendSpikes;
}

static void Snapper_Retreat(EnSnapper* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        Snapper_SetupExtendSpikes(this);
        return;
    }
    f32 frame = this->skelAnime.curFrame;

    if (frame > 2.0f) {
        this->limbScaleY = 1.5f - (frame - 2.0f) * 0.23333333f;
        this->limbScaleXZ = 1.5f - (frame - 2.0f) * 0.083333336f;
    } else {
        this->limbScaleXZ = 0.25f * frame + 1.0f;
        this->limbScaleY = this->limbScaleXZ;
    }
}

static void Snapper_SetupSpinUp(EnSnapper* this) {
    this->collider.base.atFlags |= AT_ON;
    this->actor.shape.rot.z = 0x680;
    this->timer = 15;
    this->limbScaleY = 1.0f;
    this->actionFunc = Snapper_SpinUp;
}

static void Snapper_ExtendSpikes(EnSnapper* this, PlayState* play) {
    this->limbScaleY += 0.2f;
    if (this->limbScaleY > 1.1f) {
        Snapper_SetupSpinUp(this);
    }
}

static void Snapper_SetupRoll(EnSnapper* this) {
    this->actor.speedXZ = 15.0f;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        s16 offWall = this->actor.yawTowardsPlayer - this->actor.wallYaw;

        this->actor.bgCheckFlags &= ~BGCHECKFLAG_WALL;
        if (offWall > 0x3C00) {
            this->actor.world.rot.y = this->actor.wallYaw + 0x3C00;
        } else if (offWall < -0x3C00) {
            this->actor.world.rot.y = this->actor.wallYaw - 0x3C00;
        }
    }
    this->timer = 0;
    this->rollFrames = 0;
    this->actionFunc = Snapper_Roll;
}

static void Snapper_SpinUp(EnSnapper* this, PlayState* play) {
    this->timer--;
    this->actor.shape.rot.y += 0x3B00;
    PlaySfx(&this->actor, MM_SE_EN_B_PAMET_ROLL, NA_SE_EN_DODO_M_MOVE);
    if (this->timer == 0) {
        Snapper_SetupRoll(this);
    }
}

// Arena fallback: an open field has no wall to stop the roll, so it also ends by time and distance from home.
static bool Snapper_IsRollSpent(EnSnapper* this) {
    this->rollFrames++;
    return this->rollFrames > SNAPPER_ROLL_FRAMES ||
           Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > SNAPPER_ROLL_RADIUS;
}

static void Snapper_Roll(EnSnapper* this, PlayState* play) {
    this->actor.shape.rot.y += 0x3B00;
    PlaySfx(&this->actor, MM_SE_EN_B_PAMET_ROLL, NA_SE_EN_DODO_M_MOVE);
    this->timer = CLAMP_MAX(this->timer + 1, 20);
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        if ((s16)(this->actor.yawTowardsPlayer - this->actor.world.rot.y) > 0) {
            this->actor.world.rot.y -= 0x300;
        } else {
            this->actor.world.rot.y += 0x300;
        }
    }
    if (play->gameplayFrames & 1) {
        Snapper_SpawnSplash(this, play);
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->actor.velocity.y = this->timer * 0.375f;
        RequestQuake(play, 20000, 15, 10);
        Rumble_Request(this->actor.xyzDistToPlayerSq, 180, 20, 100);
        Snapper_BurstFromWall(this);
        Snapper_SpawnWallDust(this, play);
        Snapper_SetupStopRoll(this);
    } else if (Snapper_IsRollSpent(this)) {
        Snapper_SetupStopRoll(this);
    }
}

static void Snapper_SetupStopRoll(EnSnapper* this) {
    PlaySfx(&this->actor, MM_SE_EN_PAMET_CUTTER_OFF, NA_SE_IT_SWORD_PUTAWAY);
    this->actor.shape.rot.z = 0;
    this->collider.base.atFlags &= ~AT_ON;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Snapper_RetractSpikes;
}

static void Snapper_RetractSpikes(EnSnapper* this, PlayState* play) {
    this->limbScaleY -= 0.1f;
    if (this->limbScaleY < 0.5f) {
        Snapper_SetupEmerge(this);
    }
    Snapper_SpawnWallDust(this, play);
}

static void Snapper_SetupEmerge(EnSnapper* this) {
    this->actor.draw = Snapper_DrawBody;
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperEmergeFromShellAnim, -3.0f);
    this->limbScaleY = 0.1f;
    this->limbScaleXZ = 1.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Snapper_Emerge;
}

static bool Snapper_IsRiderReturning(EnSnapper* this) {
    u8 riderState = Snapper_GetRider(this)->state;

    return riderState == GEKKO_STATE_RETURN_TO_SNAPPER || riderState == GEKKO_STATE_JUMP_ON_SNAPPER;
}

static void Snapper_Emerge(EnSnapper* this, PlayState* play) {
    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0x600);
    if (SkelAnime_Update(&this->skelAnime)) {
        if (Snapper_IsRiderReturning(this)) {
            Snapper_SetupWalk(this);
        } else {
            Snapper_SetupRetreat(this);
        }
        return;
    }
    f32 frame = this->skelAnime.curFrame;

    if (frame > 7.0f) {
        this->limbScaleY = 1.5f - (frame - 7.0f) * (1.0f / 6.0f);
        this->limbScaleXZ = this->limbScaleY;
    } else {
        this->limbScaleY = 0.2f * frame + 0.1f;
        this->limbScaleXZ = (1.0f / 14.0f) * frame + 1.0f;
    }
}

static void Snapper_DropAmmo(EnSnapper* this, PlayState* play) {
    s16 ammo = LINK_IS_ADULT ? ITEM00_ARROWS_MEDIUM : ITEM00_SEEDS;
    EnItem00* drop = Item_DropCollectible(play, &this->actor.world.pos, ammo);

    if (drop == NULL) {
        return;
    }
    drop->actor.velocity.y = 15.0f;
    drop->actor.world.rot.y = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
}

static void Snapper_Flip(EnSnapper* this, PlayState* play) {
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sSnapperDeathAnim);
    this->collider.base.atFlags &= ~(AT_ON | AT_HIT | AT_BOUNCED);
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.velocity.y = 22.0f;
    this->actor.speedXZ = 5.0f;
    if (this->actor.draw == Snapper_DrawShell && this->actionFunc != Snapper_Flipped) {
        this->actor.draw = Snapper_DrawBody;
    } else if (this->collider.base.ac != NULL) {
        this->actor.world.rot.y = BINANG_ROT180(Actor_WorldYawTowardActor(&this->actor, this->collider.base.ac));
    }
    this->actor.shape.rot.y = this->actor.world.rot.y;
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->isUpright = false;

    EnGekko* rider = Snapper_GetRider(this);

    if (rider->state == GEKKO_STATE_ON_SNAPPER || rider->state == GEKKO_STATE_REAR_ON_SNAPPER ||
        rider->state == GEKKO_STATE_JUMP_ON_SNAPPER) {
        rider->state = GEKKO_STATE_KNOCKED_OFF;
    }
    Snapper_DropAmmo(this, play);
    PlaySfx(&this->actor, MM_SE_EN_B_PAMET_REVERSE, NA_SE_EN_DODO_M_DOWN);
    this->actionFunc = Snapper_FlipInAir;
}

static void Snapper_SetupFlipped(EnSnapper* this) {
    this->actor.draw = Snapper_DrawShell;
    this->actor.speedXZ = 0.0f;
    this->actor.shape.rot.x = 0;
    this->actor.shape.rot.z = 0;
    this->limbScaleY = 0.0f;
    this->collider.base.acFlags |= AC_ON;
    this->actionFunc = Snapper_Flipped;
}

static void Snapper_FlipInAir(EnSnapper* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        PlaySfx(&this->actor, MM_SE_EN_HIPLOOP_LAND, NA_SE_EN_DODO_M_GND);
        Snapper_SpawnSplash(this, play);
        Snapper_SetupFlipped(this);
    }
    this->actor.shape.rot.x += 0x800;
    this->actor.shape.rot.z += 0x1000;
}

static void Snapper_Flipped(EnSnapper* this, PlayState* play) {
    this->actor.shape.rot.x = (s32)Rand_Next() >> 0x16;
    this->actor.shape.rot.z = (s32)Rand_Next() >> 0x16;
    if (Snapper_IsRiderReturning(this)) {
        Snapper_SetupEmerge(this);
    }
}

static void Snapper_SetupRearUp(EnSnapper* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sSnapperRearUpAnim, -2.0f);
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.speedXZ = 0.0f;
    this->actionFunc = Snapper_RearUp;
}

static void Snapper_RearUp(EnSnapper* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        Snapper_SetupRetreat(this);
    }
}

static void Snapper_SetupVanish(EnSnapper* this) {
    this->actionFunc = Snapper_Vanish;
}

static void Snapper_Vanish(EnSnapper* this, PlayState* play) {
    static Color_RGBA8 sSmokePrim = { 250, 250, 250, 255 };
    static Color_RGBA8 sSmokeEnv = { 180, 180, 180, 255 };

    this->actor.shape.rot.x = (s32)Rand_Next() >> 0x16;
    this->actor.shape.rot.z = (s32)Rand_Next() >> 0x16;
    if (Math_StepToF(&this->actor.scale.x, 0.0f, 0.00075f)) {
        s16 towardCamera = BINANG_ROT180(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)));
        Vec3f smokePos = { Math_SinS(towardCamera) * 20.0f + this->actor.world.pos.x, this->actor.world.pos.y + 25.0f,
                           Math_CosS(towardCamera) * 20.0f + this->actor.world.pos.z };

        func_8002829C(play, &smokePos, &sZeroVec, &sZeroVec, &sSmokePrim, &sSmokeEnv, 800, 50);
        PlaySfx(&this->actor, MM_SE_EN_NPC_FADEAWAY, NA_SE_EN_EXTINCT);
        Actor_Kill(&this->actor);
    }
    this->actor.scale.y = this->actor.scale.x;
    this->actor.scale.z = this->actor.scale.x;
}

static bool Snapper_IsHitByHammerPound(EnSnapper* this, PlayState* play) {
    return play->actorCtx.unk_02 != 0 && this->actor.xzDistToPlayer <= HAMMER_POUND_RANGE &&
           (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);
}

static bool Snapper_IsAboveGround(EnSnapper* this) {
    return this->actionFunc != Snapper_Buried && this->actionFunc != Snapper_Rise;
}

static bool Snapper_CanFlip(EnSnapper* this) {
    return Snapper_GetRider(this)->state != GEKKO_STATE_JUMP_ON_SNAPPER && Snapper_IsAboveGround(this) &&
           this->actionFunc != Snapper_RearUp && this->actionFunc != Snapper_FlipInAir &&
           this->actionFunc != Snapper_Vanish;
}

// Explosives, a hammer jump or OoT's hammer shockwave flip it, as MM's damage effect 0xF does.
static void Snapper_CheckFlip(EnSnapper* this, PlayState* play) {
    bool isFlipHit = false;

    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        isFlipHit = this->actor.colChkInfo.damageEffect == SNAPPER_DMGEFF_FLIP;
    }
    if (!isFlipHit && !Snapper_IsHitByHammerPound(this, play)) {
        return;
    }
    if (Snapper_CanFlip(this)) {
        Snapper_Flip(this, play);
    }
}

static void Snapper_UpdateDebris(EnSnapper* this) {
    for (s32 i = 0; i < SNAPPER_DEBRIS_COUNT; i++) {
        SnapperDebris* debris = &this->debris[i];

        debris->velocity.y -= 1.0f;
        Math_Vec3f_Sum(&debris->pos, &debris->velocity, &debris->pos);
        debris->rot.x += (s16)((Rand_Next() >> 0x17) + 0x700);
        debris->rot.y += (s16)((Rand_Next() >> 0x17) + 0x900);
        debris->rot.z += (s16)((Rand_Next() >> 0x17) + 0xB00);
    }
}

static void EnSnapper_Init(Actor* thisx, PlayState* play) {
    EnSnapper* this = (EnSnapper*)thisx;

    Actor_SetScale(&this->actor, 0.015f);
    this->actor.gravity = -2.0f;
    this->actor.targetArrowOffset = 4333.0f;
    this->actor.targetMode = 5;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSnapperSkel, (AnimationHeader*)sSnapperIdleAnim,
                       this->jointTable, this->morphTable, SNAPPER_LIMB_MAX);
    SkelAnime_InitFlex(play, &this->shellSkelAnime, (FlexSkeletonHeader*)sSpikedSnapperSkel,
                       (AnimationHeader*)sSpikedSnapperIdleAnim, this->shellJointTable, this->shellMorphTable,
                       SPIKED_SNAPPER_LIMB_MAX);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 55.0f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sSnapperColliderInit);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sSnapperDamageTable, &sSnapperColChkInit);
    this->isUpright = true;
    Snapper_SetupBuried(this);
}

static void EnSnapper_Destroy(Actor* thisx, PlayState* play) {
    EnSnapper* this = (EnSnapper*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void Snapper_SubmitColliders(EnSnapper* this, PlayState* play) {
    Collider_UpdateCylinder(&this->actor, &this->collider);
    if (this->collider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

// The engine clears `parent` once the Gekko is gone; the Snapper never outlives its rider.
static void EnSnapper_Update(Actor* thisx, PlayState* play) {
    EnSnapper* this = (EnSnapper*)thisx;

    if (this->actor.parent == NULL) {
        Actor_Kill(&this->actor);
        return;
    }
    Snapper_UpdateBlink(this);
    Snapper_CheckFlip(this, play);
    if (Snapper_GetRider(this)->state == GEKKO_STATE_DEFEAT && this->actionFunc != Snapper_Vanish) {
        Snapper_SetupVanish(this);
    }
    this->actionFunc(this, play);
    if (this->debrisTimer > 0) {
        this->debrisTimer--;
        Snapper_UpdateDebris(this);
    }
    if (!Snapper_IsAboveGround(this)) {
        return;
    }
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 40.0f, 50.0f, 60.0f, BGCHECK_UPDATE_WALL_CEILING_FLOOR);
    Snapper_CarryRider(this);
    Actor_SetFocus(&this->actor, 25.0f);
    Snapper_SubmitColliders(this, play);
    if (this->actionFunc != Snapper_Roll) {
        Snapper_SpawnIdleRipples(this, play);
    }
}

static void Snapper_DrawDebris(EnSnapper* this, PlayState* play) {
    if (this->debrisTimer <= 0) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    for (s32 i = 0; i < SNAPPER_DEBRIS_COUNT; i++) {
        SnapperDebris* debris = &this->debris[i];

        Matrix_SetTranslateRotateYXZ(debris->pos.x, debris->pos.y, debris->pos.z, &debris->rot);
        Matrix_Scale(debris->scale, debris->scale, debris->scale, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRockFragmentDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool Snapper_IsShellChanging(EnSnapper* this) {
    return this->actionFunc == Snapper_Retreat || this->actionFunc == Snapper_Emerge;
}

static bool IsSnapperLeg(s32 limbIndex) {
    return limbIndex == SNAPPER_LIMB_BACK_RIGHT_LEG || limbIndex == SNAPPER_LIMB_BACK_LEFT_LEG ||
           limbIndex == SNAPPER_LIMB_FRONT_RIGHT_LEG || limbIndex == SNAPPER_LIMB_FRONT_LEFT_LEG;
}

static s32 Snapper_OverrideBodyLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                        void* thisx) {
    EnSnapper* this = (EnSnapper*)thisx;

    if (!Snapper_IsShellChanging(this)) {
        return false;
    }
    if (limbIndex == SNAPPER_LIMB_HEAD) {
        Matrix_Scale(this->limbScaleXZ, this->limbScaleY, this->limbScaleXZ, MTXMODE_APPLY);
    } else if (IsSnapperLeg(limbIndex)) {
        Matrix_Scale(this->limbScaleXZ, this->limbScaleY, this->limbScaleY, MTXMODE_APPLY);
    }
    return false;
}

static void Snapper_StoreBodyHeight(EnSnapper* this) {
    MtxF current;

    Matrix_Get(&current);
    this->bodyHeight = current.yw;
}

static void Snapper_PostBodyLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    if (limbIndex == SNAPPER_LIMB_BODY) {
        Snapper_StoreBodyHeight((EnSnapper*)thisx);
    }
}

static void Snapper_DrawBody(Actor* thisx, PlayState* play) {
    EnSnapper* this = (EnSnapper*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sSnapperEyeTextures[this->blinkIndex]);
    CLOSE_DISPS(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          Snapper_OverrideBodyLimbDraw, Snapper_PostBodyLimbDraw, this);
    Snapper_DrawDebris(this, play);
}

static s32 Snapper_OverrideShellLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                         void* thisx) {
    EnSnapper* this = (EnSnapper*)thisx;

    if (limbIndex == SPIKED_SNAPPER_LIMB_BODY) {
        pos->y -= this->actionFunc == Snapper_FlipInAir ? 2500.0f : 700.0f;
    }
    if (this->limbScaleY != 1.0f && limbIndex == SPIKED_SNAPPER_LIMB_SPIKES) {
        Matrix_Scale(1.0f, this->limbScaleY, this->limbScaleY, MTXMODE_APPLY);
    }
    return false;
}

static void Snapper_PostShellLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    if (limbIndex == SPIKED_SNAPPER_LIMB_BODY) {
        Snapper_StoreBodyHeight((EnSnapper*)thisx);
    }
}

static void Snapper_DrawShell(Actor* thisx, PlayState* play) {
    EnSnapper* this = (EnSnapper*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->shellSkelAnime.skeleton, this->shellSkelAnime.jointTable,
                          this->shellSkelAnime.dListCount, Snapper_OverrideShellLimbDraw, Snapper_PostShellLimbDraw,
                          this);
    Snapper_DrawDebris(this, play);
}

static bool CanFlipSnapper(void) {
    if (sApi->LogicHasCapability("HasExplosives") || sApi->LogicCanUseItem(RG_MEGATON_HAMMER)) {
        return true;
    }
    int32_t goronMask = sApi->GetRandoItem(GORON_MASK_KEY);

    return goronMask >= 0 && sApi->LogicHasItem(goronMask);
}

static bool CanShootGekko(void) {
    return sApi->LogicCanUseItem(RG_FAIRY_BOW) || sApi->LogicCanUseItem(RG_FAIRY_SLINGSHOT) ||
           sApi->LogicCanUseItem(RG_BOOMERANG);
}

// MM's CanKillEnemy(ACTOR_EN_PAMETFROG): a ranged hit on the wall Gekko after the Snapper is flipped.
static bool Gekko_CanKillInLogic(void* userData) {
    return CanShootGekko() && CanFlipSnapper();
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
    static Vec3s sSoulJointTable[GEKKO_LIMB_MAX];
    static Vec3s sSoulMorphTable[GEKKO_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sGekkoSkel,
                           (AnimationHeader*)sGekkoBoxingStanceAnim, sSoulJointTable, sSoulMorphTable, GEKKO_LIMB_MAX);
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
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sSoulEmptyDL);
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
    SOHCustomItemDefinition soul = Z64Items_Define(GEKKO_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Gekko";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Gekko%w!&Gekko and his Snapper can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " GEKKO_SOUL_ITEM_KEY);
    }
}

static void RegisterSnapperMount(void) {
    SOHActorDefinition snapper = { 0 };

    snapper.structSize = sizeof(snapper);
    snapper.key = SNAPPER_MOUNT_KEY;
    snapper.description = "Gekko's Snapper";
    snapper.category = ACTORCAT_ENEMY;
    snapper.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                         ACTOR_FLAG_DRAW_CULLING_DISABLED | ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    snapper.objectId = OBJECT_GAMEPLAY_KEEP;
    snapper.instanceSize = sizeof(EnSnapper);
    snapper.init = EnSnapper_Init;
    snapper.destroy = EnSnapper_Destroy;
    snapper.update = EnSnapper_Update;
    snapper.draw = Snapper_DrawBody;
    sSnapperActorId = sApi->RegisterActor(&snapper);
}

static void RegisterGekko(void) {
    SOHActorDefinition gekko = { 0 };

    gekko.structSize = sizeof(gekko);
    gekko.key = GEKKO_KEY;
    gekko.description = "Gekko & Snapper";
    gekko.category = ACTORCAT_ENEMY;
    gekko.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                       ACTOR_FLAG_DRAW_CULLING_DISABLED;
    gekko.objectId = OBJECT_GAMEPLAY_KEEP;
    gekko.instanceSize = sizeof(EnGekko);
    gekko.init = EnGekko_Init;
    gekko.destroy = EnGekko_Destroy;
    gekko.update = EnGekko_Update;
    gekko.draw = EnGekko_Draw;
    gekko.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_MINIBOSS | SOH_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS;
    gekko.naviHint = "Gekko&%cHe rides a Snapper! Flip it with %rexplosives%c or a %rhammer pound%c, then shoot "
                     "Gekko off the walls!%w";
    gekko.soulKey = GEKKO_KEY;
    gekko.logicKill = Gekko_CanKillInLogic;
    if (sApi->RegisterActor(&gekko) < 0) {
        sApi->Log("Could not register " GEKKO_KEY);
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
    RegisterSnapperMount();
    if (sSnapperActorId < 0) {
        sApi->Log("Could not register " SNAPPER_MOUNT_KEY);
        return;
    }
    RegisterSoul();
    RegisterGekko();
}
