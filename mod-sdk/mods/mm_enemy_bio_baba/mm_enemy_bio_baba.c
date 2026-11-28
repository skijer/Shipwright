// Majora's Mask's Bio Deku Baba (Boss_05) as an Unbound custom enemy, drawn from mm.o2r.
// It raises its own lily pad on the nearest water and hangs under it; this actor stays the head all its life,
// so a cut stem drops it to walk the bottom while the pad (mm.bio_baba.lily_pad) floats on.
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

#define BIO_BABA_KEY "mm.bio_baba"
#define DEKU_BABA_FAMILY_KEY "mm.deku_baba"
#define DEKU_BABA_SOUL_ITEM_KEY "soul.mm.deku_baba"
#define BIO_BABA_LILY_PAD_KEY "mm.bio_baba.lily_pad"
#define BIO_BABA_FRAGMENT_KEY "mm.bio_baba.fragment"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_BOSS05 "__OTR__objects/object_boss05/"
#define OVL_BOSS05 "__OTR__overlays/ovl_Boss_05/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_FLOOR ((1 << 0) | (1 << 2))
#define BGCHECK_UPDATE_FLOOR (1 << 2)

#define MM_SE_EN_MIZUBABA1_ATTACK 0x3861
#define MM_SE_EN_MIZUBABA1_DAMAGE 0x381E
#define MM_SE_EN_MIZUBABA1_MOUTH 0x3860
#define MM_SE_EN_MIZUBABA2_ATTACK 0x3837
#define MM_SE_EN_MIZUBABA2_DAMAGE 0x3807
#define MM_SE_EN_MIZUBABA2_VOICE 0x3806
#define MM_SE_EN_MIZUBABA2_WALK 0x3827
#define MM_SE_EN_MIZUBABA_DEAD 0x381F
#define MM_SE_EN_MIZUBABA_TRANSFORM 0x3825
#define MM_SE_EV_OUT_OF_WATER 0x2808

#define BIO_BABA_HEALTH 2
#define BIO_BABA_MIN_DEPTH 100.0f
#define BIO_BABA_HEAD_DROP 85.0f
#define WATER_SEARCH_STEP 100.0f
#define WATER_SEARCH_RADIUS 800.0f
#define WATER_SEARCH_ANGLES 12
#define EYE_FLASH_PERIOD 7
#define FRAGMENT_COUNT 14
#define HANGING_FRAGMENT_COUNT 2

typedef enum {
    LILY_PAD_LIMB_NONE,
    LILY_PAD_LIMB_ROOTS,
    LILY_PAD_LIMB_UPPER_STEM,
    LILY_PAD_LIMB_MIDDLE_STEM,
    LILY_PAD_LIMB_LOWER_STEM,
    LILY_PAD_LIMB_LEFT_UPPER_ARM,
    LILY_PAD_LIMB_LEFT_LOWER_ARM,
    LILY_PAD_LIMB_RIGHT_UPPER_ARM,
    LILY_PAD_LIMB_RIGHT_LOWER_ARM,
    LILY_PAD_LIMB_LEAF,
    LILY_PAD_LIMB_MAX
} BioBabaLilyPadLimb;

typedef enum {
    HEAD_LIMB_NONE,
    HEAD_LIMB_BODY,
    HEAD_LIMB_LOWER_JAW,
    HEAD_LIMB_UPPER_JAW,
    HEAD_LIMB_BACK_UPPER_LEG,
    HEAD_LIMB_BACK_LOWER_LEG,
    HEAD_LIMB_BACK_FOOT,
    HEAD_LIMB_LEFT_UPPER_LEG,
    HEAD_LIMB_LEFT_LOWER_LEG,
    HEAD_LIMB_LEFT_FOOT,
    HEAD_LIMB_LEFT_LOWER_EYESTALK,
    HEAD_LIMB_LEFT_UPPER_EYESTALK,
    HEAD_LIMB_LEFT_EYE,
    HEAD_LIMB_RIGHT_LOWER_EYESTALK,
    HEAD_LIMB_RIGHT_UPPER_EYESTALK,
    HEAD_LIMB_RIGHT_EYE,
    HEAD_LIMB_RIGHT_UPPER_LEG,
    HEAD_LIMB_RIGHT_LOWER_LEG,
    HEAD_LIMB_RIGHT_FOOT,
    HEAD_LIMB_LEAVES,
    HEAD_LIMB_MAX
} BioBabaHeadLimb;

typedef enum {
    STEM_ROT_UPPER_STEM,
    STEM_ROT_MIDDLE_STEM,
    STEM_ROT_LOWER_STEM,
    STEM_ROT_LEFT_UPPER_ARM,
    STEM_ROT_LEFT_LOWER_ARM,
    STEM_ROT_RIGHT_UPPER_ARM,
    STEM_ROT_RIGHT_LOWER_ARM,
    STEM_ROT_MAX
} BioBabaStemRot;

typedef enum { BIO_BABA_FORM_HANGING, BIO_BABA_FORM_FALLING, BIO_BABA_FORM_WALKING } BioBabaForm;

typedef enum {
    BIO_BABA_ATTACK_IDLE,
    BIO_BABA_ATTACK_WIND_UP,
    BIO_BABA_ATTACK_LUNGE,
    BIO_BABA_ATTACK_FLIP
} BioBabaAttackState;

typedef enum {
    BIO_BABA_DMGEFF_IMMUNE,
    BIO_BABA_DMGEFF_STUN,
    BIO_BABA_DMGEFF_FIRE,
    BIO_BABA_DMGEFF_FREEZE,
    BIO_BABA_DMGEFF_LIGHT,
    BIO_BABA_DMGEFF_DAMAGE = 0xF
} BioBabaDamageEffect;

typedef enum { BIO_BABA_AURA_NONE, BIO_BABA_AURA_FIRE, BIO_BABA_AURA_FROZEN, BIO_BABA_AURA_LIGHT } BioBabaAura;

typedef enum { BIO_BABA_HIT_NONE, BIO_BABA_HIT_DEAD, BIO_BABA_HIT_STEM_CUT } BioBabaHangingHit;

typedef enum {
    FRAGMENT_STATE_SPAWNED,
    FRAGMENT_STATE_UNDERWATER,
    FRAGMENT_STATE_ABOVE_WATER,
    FRAGMENT_STATE_FLYING
} BioBabaFragmentState;

struct EnBioBaba;

typedef void (*EnBioBabaActionFunc)(struct EnBioBaba*, PlayState*);

typedef struct EnBioBaba {
    DynaPolyActor dyna;
    EnBioBabaActionFunc actionFunc;
    BioBabaForm form;
    BioBabaAttackState attackState;
    BioBabaAura aura;
    bool hasLilyPadCollision;
    s16 flipAttackFrames;
    s16 frameCounter;
    s16 actionTimer;
    s16 bubbleTimer;
    s16 damagedTimer;
    s16 damagedFlashTimer;
    s16 auraTimer;
    s16 stemRotX;
    Vec3s stemRot[STEM_ROT_MAX];
    f32 fallingStemScale;
    f32 knockbackMagnitude;
    s16 knockbackAngle;
    Vec3f walkTargetPos;
    f32 walkAngularVelocity;
    f32 lowerJawScaleXZ;
    f32 headBodyScale;
    f32 headLimbScale;
    f32 animEndFrame;
    Vec3f headPos;
    Vec3s headRot;
    Vec3f auraPos;
    ColliderJntSph lilyPadCollider;
    ColliderJntSphElement lilyPadColliderElements[2];
    SkelAnime lilyPadSkelAnime;
    Vec3s lilyPadJointTable[LILY_PAD_LIMB_MAX];
    Vec3s lilyPadMorphTable[LILY_PAD_LIMB_MAX];
    ColliderJntSph headCollider;
    ColliderJntSphElement headColliderElements[1];
    SkelAnime headSkelAnime;
    Vec3s headJointTable[HEAD_LIMB_MAX];
    Vec3s headMorphTable[HEAD_LIMB_MAX];
} EnBioBaba;

typedef struct {
    DynaPolyActor dyna;
    SkelAnime skelAnime;
    Vec3s jointTable[LILY_PAD_LIMB_MAX];
    Vec3s morphTable[LILY_PAD_LIMB_MAX];
    s16 frameCounter;
    s16 tiltYaw;
    s16 tilt;
} EnBioBabaLilyPad;

typedef struct {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[HEAD_LIMB_MAX];
    Vec3s morphTable[HEAD_LIMB_MAX];
    BioBabaFragmentState state;
    s16 frameCounter;
    s16 timer;
    Vec3s angularVelocity;
    Vec3f pos;
} EnBioBabaFragment;

typedef struct {
    s16 frequency[3];
    s16 shift[3];
    s16 amplitude[3];
    s16 fraction;
    s16 step;
} StemSway;

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

static const ALIGN_ASSET(2) char sLilyPadSkel[] = OBJ_BOSS05 "gBioDekuBabaLilyPadSkel";
static const ALIGN_ASSET(2) char sLilyPadIdleAnim[] = OBJ_BOSS05 "gBioDekuBabaLilyPadIdleAnim";
static const ALIGN_ASSET(2) char sHeadSkel[] = OBJ_BOSS05 "gBioDekuBabaHeadSkel";
static const ALIGN_ASSET(2) char sHeadChompAnim[] = OBJ_BOSS05 "gBioDekuBabaHeadChompAnim";
static const ALIGN_ASSET(2) char sHeadTransformAnim[] = OBJ_BOSS05 "gBioDekuBabaHeadTransformAnim";
static const ALIGN_ASSET(2) char sHeadIdleAnim[] = OBJ_BOSS05 "gBioDekuBabaHeadIdleAnim";
static const ALIGN_ASSET(2) char sHeadWalkAnim[] = OBJ_BOSS05 "gBioDekuBabaHeadWalkAnim";
static const ALIGN_ASSET(2) char sHeadSpotAnim[] = OBJ_BOSS05 "gBioDekuBabaHeadSpotAnim";
static const ALIGN_ASSET(2) char sHeadChargeAnim[] = OBJ_BOSS05 "gBioDekuBabaHeadChargeAnim";
static const ALIGN_ASSET(2) char sHeadAttackAnim[] = OBJ_BOSS05 "gBioDekuBabaHeadAttackAnim";
static const ALIGN_ASSET(2) char sHeadDamagedAnim[] = OBJ_BOSS05 "gBioDekuBabaHeadDamagedAnim";
static const ALIGN_ASSET(2) char sLilyPadCol[] = OVL_BOSS05 "sBioBabaLilypadCol";

#define BIO_BABA_ELEMENT(bumperFlags, radius)                                                                         \
    {                                                                                                                 \
        { ELEMTYPE_UNK3, { 0xF7CFFFFF, 0x00, 0x08 }, { bumperFlags, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_HARD, BUMP_ON, \
          OCELEM_ON },                                                                                                \
            { 0, { { 0, 0, 0 }, radius }, 100 },                                                                      \
    }

static ColliderJntSphElementInit sLilyPadElementsInit[] = {
    BIO_BABA_ELEMENT(0xF7CFFFFF, 15),
    BIO_BABA_ELEMENT(0xF7CFFFFF, 15),
};

static ColliderJntSphElementInit sHangingHeadElementsInit[] = {
    BIO_BABA_ELEMENT(0xF7CFFFFF, 20),
};

static ColliderJntSphElementInit sWalkingHeadElementsInit[] = {
    BIO_BABA_ELEMENT(0xF7FFFFFF, 15),
};

static ColliderJntSphInit sLilyPadJntSphInit = {
    { COLTYPE_HIT3, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_PLAYER, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    ARRAY_COUNT(sLilyPadElementsInit),
    sLilyPadElementsInit,
};

static ColliderJntSphInit sHangingHeadJntSphInit = {
    { COLTYPE_HIT3, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_PLAYER, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    ARRAY_COUNT(sHangingHeadElementsInit),
    sHangingHeadElementsInit,
};

static ColliderJntSphInit sWalkingHeadJntSphInit = {
    { COLTYPE_HIT3, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_PLAYER, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    ARRAY_COUNT(sWalkingHeadElementsInit),
    sWalkingHeadElementsInit,
};

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sHangingDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, BIO_BABA_DMGEFF_IMMUNE),
    /* Deku stick    */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Slingshot     */ DMG_ENTRY(3, BIO_BABA_DMGEFF_DAMAGE),
    /* Explosive     */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Boomerang     */ DMG_ENTRY(1, BIO_BABA_DMGEFF_DAMAGE),
    /* Normal arrow  */ DMG_ENTRY(6, BIO_BABA_DMGEFF_DAMAGE),
    /* Hammer swing  */ DMG_ENTRY(4, BIO_BABA_DMGEFF_DAMAGE),
    /* Hookshot      */ DMG_ENTRY(6, BIO_BABA_DMGEFF_DAMAGE),
    /* Kokiri sword  */ DMG_ENTRY(1, BIO_BABA_DMGEFF_DAMAGE),
    /* Master sword  */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Giant's Knife */ DMG_ENTRY(4, BIO_BABA_DMGEFF_DAMAGE),
    /* Fire arrow    */ DMG_ENTRY(2, BIO_BABA_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(2, BIO_BABA_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(6, BIO_BABA_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Unk arrow 2   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Unk arrow 3   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Fire magic    */ DMG_ENTRY(2, BIO_BABA_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(2, BIO_BABA_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, BIO_BABA_DMGEFF_IMMUNE),
    /* Mirror Ray    */ DMG_ENTRY(0, BIO_BABA_DMGEFF_IMMUNE),
    /* Kokiri spin   */ DMG_ENTRY(1, BIO_BABA_DMGEFF_DAMAGE),
    /* Giant spin    */ DMG_ENTRY(4, BIO_BABA_DMGEFF_DAMAGE),
    /* Master spin   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Kokiri jump   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Giant jump    */ DMG_ENTRY(8, BIO_BABA_DMGEFF_DAMAGE),
    /* Master jump   */ DMG_ENTRY(4, BIO_BABA_DMGEFF_DAMAGE),
    /* Unknown 1     */ DMG_ENTRY(0, BIO_BABA_DMGEFF_IMMUNE),
    /* Unblockable   */ DMG_ENTRY(0, BIO_BABA_DMGEFF_IMMUNE),
    /* Hammer jump   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Unknown 2     */ DMG_ENTRY(0, BIO_BABA_DMGEFF_IMMUNE),
};

static DamageTable sWalkingDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, BIO_BABA_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(6, BIO_BABA_DMGEFF_DAMAGE),
    /* Slingshot     */ DMG_ENTRY(1, BIO_BABA_DMGEFF_DAMAGE),
    /* Explosive     */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Boomerang     */ DMG_ENTRY(1, BIO_BABA_DMGEFF_DAMAGE),
    /* Normal arrow  */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Hammer swing  */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Hookshot      */ DMG_ENTRY(0, BIO_BABA_DMGEFF_STUN),
    /* Kokiri sword  */ DMG_ENTRY(1, BIO_BABA_DMGEFF_DAMAGE),
    /* Master sword  */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Giant's Knife */ DMG_ENTRY(4, BIO_BABA_DMGEFF_DAMAGE),
    /* Fire arrow    */ DMG_ENTRY(4, BIO_BABA_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, BIO_BABA_DMGEFF_FREEZE),
    /* Light arrow   */ DMG_ENTRY(4, BIO_BABA_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Unk arrow 2   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Unk arrow 3   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Fire magic    */ DMG_ENTRY(4, BIO_BABA_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(4, BIO_BABA_DMGEFF_FREEZE),
    /* Light magic   */ DMG_ENTRY(4, BIO_BABA_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, BIO_BABA_DMGEFF_IMMUNE),
    /* Mirror Ray    */ DMG_ENTRY(0, BIO_BABA_DMGEFF_IMMUNE),
    /* Kokiri spin   */ DMG_ENTRY(1, BIO_BABA_DMGEFF_DAMAGE),
    /* Giant spin    */ DMG_ENTRY(4, BIO_BABA_DMGEFF_DAMAGE),
    /* Master spin   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Kokiri jump   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Giant jump    */ DMG_ENTRY(8, BIO_BABA_DMGEFF_DAMAGE),
    /* Master jump   */ DMG_ENTRY(4, BIO_BABA_DMGEFF_DAMAGE),
    /* Unknown 1     */ DMG_ENTRY(0, BIO_BABA_DMGEFF_IMMUNE),
    /* Unblockable   */ DMG_ENTRY(0, BIO_BABA_DMGEFF_IMMUNE),
    /* Hammer jump   */ DMG_ENTRY(2, BIO_BABA_DMGEFF_DAMAGE),
    /* Unknown 2     */ DMG_ENTRY(0, BIO_BABA_DMGEFF_IMMUNE),
};

static const StemSway sIdleSway = {
    { 0x3E8, 0x5DC, 0x514 }, { 0x3E80, 0x1770, 0x5208 }, { 0x7D0, 0xBB8, 0xDAC }, 0xA, 0x300,
};

static const StemSway sHurtSway = {
    { 0x1B58, 0x5DC, 0x1C84 }, { 0x3E80, 0x1770, 0x5208 }, { 0x1770, 0x1770, 0x1964 }, 1, 0x1000,
};

static const StemSway sFlipSway = {
    { 0x1B58, 0x5DC, 0x1C84 }, { 0x3E80, 0x2328, 0x5208 }, { 0x1388, 0x1388, 0x157C }, 1, 0x1000,
};

static const Vec3s sWindUpStemRot[STEM_ROT_MAX] = {
    { 0x3200, 0, 0 }, { -0x1E00, 0, 0 }, { -0x1400, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
};

static const Vec3s sLungeStemRot[STEM_ROT_MAX] = {
    { -0x3200, 0, 0 }, { 0, 0, 0 }, { 0x1E00, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
};

static const s8 sLimbToStemRot[LILY_PAD_LIMB_MAX] = {
    -1,
    -1,
    STEM_ROT_UPPER_STEM,
    STEM_ROT_MIDDLE_STEM,
    STEM_ROT_LOWER_STEM,
    STEM_ROT_LEFT_UPPER_ARM,
    STEM_ROT_LEFT_LOWER_ARM,
    STEM_ROT_RIGHT_UPPER_ARM,
    STEM_ROT_RIGHT_LOWER_ARM,
    -1,
};

static const u8 sFragmentLimbs[FRAGMENT_COUNT] = {
    HEAD_LIMB_LOWER_JAW,
    HEAD_LIMB_UPPER_JAW,
    HEAD_LIMB_BODY,
    HEAD_LIMB_BACK_UPPER_LEG,
    HEAD_LIMB_BACK_LOWER_LEG,
    HEAD_LIMB_LEFT_UPPER_LEG,
    HEAD_LIMB_LEFT_LOWER_LEG,
    HEAD_LIMB_LEFT_LOWER_EYESTALK,
    HEAD_LIMB_LEFT_UPPER_EYESTALK,
    HEAD_LIMB_RIGHT_LOWER_EYESTALK,
    HEAD_LIMB_RIGHT_UPPER_EYESTALK,
    HEAD_LIMB_RIGHT_UPPER_LEG,
    HEAD_LIMB_RIGHT_LOWER_LEG,
    HEAD_LIMB_LEAVES,
};

// gBioDekuBabaHeadEyeFlashTexAnim is empty in Ship's mm.o2r: a 7-frame prim lerp taken from 2Ship's.
static const u16 sEyeFlashKeyFrames[3] = { 0, 3, 6 };
static const u8 sEyeFlashShades[3] = { 255, 150, 255 };

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sLilyPadHeadOffset = { 0.0f, -1400.0f, 600.0f };
static Vec3f sMiddleStemColliderOffset = { 0.0f, -500.0f, 0.0f };
static Vec3f sMiddleStemFocusOffset = { 0.0f, 1500.0f, 0.0f };
static Vec3f sHeadColliderOffset = { 1600.0f, -300.0f, 0.0f };
static Vec3f sHeadCenterOffset = { 700.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;
static s16 sLilyPadActorId = -1;
static s16 sFragmentActorId = -1;
static Vec3f sDeathSfxPos;

static void BioBaba_Hang(EnBioBaba* this, PlayState* play);
static void BioBaba_Fall(EnBioBaba* this, PlayState* play);
static void BioBaba_Transform(EnBioBaba* this, PlayState* play);
static void BioBaba_Idle(EnBioBaba* this, PlayState* play);
static void BioBaba_Walk(EnBioBaba* this, PlayState* play);
static void BioBaba_SpottedPlayer(EnBioBaba* this, PlayState* play);
static void BioBaba_Charge(EnBioBaba* this, PlayState* play);
static void BioBaba_Attack(EnBioBaba* this, PlayState* play);
static void BioBaba_Damaged(EnBioBaba* this, PlayState* play);
static void BioBaba_Stunned(EnBioBaba* this, PlayState* play);

// Falls back to the closest OoT sound when the mm_assets sound service is missing.
static void PlaySfxAt(Vec3f* pos, Actor* actor, u16 mmSfx, u16 ootSfx) {
    if (!sIsMmSfxSearched && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
        sIsMmSfxSearched = true;
    }
    if (sPlayMmSfx != NULL && sPlayMmSfx(mmSfx, pos)) {
        return;
    }
    Audio_PlayActorSound2(actor, ootSfx);
}

static void PlaySfx(Actor* actor, u16 mmSfx, u16 ootSfx) {
    PlaySfxAt(&actor->projectedPos, actor, mmSfx, ootSfx);
}

// The sound system keeps reading the position: a dying actor's must outlive it.
static void PlayDeathSfx(Actor* actor, u16 mmSfx, u16 ootSfx) {
    sDeathSfxPos = actor->projectedPos;
    PlaySfxAt(&sDeathSfxPos, actor, mmSfx, ootSfx);
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

static u8 LerpChannel(u8 from, u8 to, f32 t) {
    return (s32)((to - from) * t) + from;
}

static void BioBaba_SetEyeFlashSegment(PlayState* play) {
    s32 frame = play->gameplayFrames % EYE_FLASH_PERIOD;
    s32 key = frame < sEyeFlashKeyFrames[1] ? 1 : 2;
    f32 t = (f32)(frame - sEyeFlashKeyFrames[key - 1]) / (sEyeFlashKeyFrames[key] - sEyeFlashKeyFrames[key - 1]);
    u8 shade = LerpChannel(sEyeFlashShades[key - 1], sEyeFlashShades[key], CLAMP_MAX(t, 1.0f));
    Gfx* eyeFlash = Graph_Alloc(play->state.gfxCtx, 2 * sizeof(Gfx));

    gDPSetPrimColor(&eyeFlash[0], 0, 255, shade, shade, shade, 255);
    gSPEndDisplayList(&eyeFlash[1]);
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)eyeFlash);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void SetSphere(ColliderJntSph* collider, s32 index, Vec3f* center) {
    ColliderJntSphElement* element = &collider->elements[index];

    element->dim.worldSphere.center = *center;
    element->dim.worldSphere.radius = element->dim.modelSphere.radius * element->dim.scale;
}

static void InitJntSph(PlayState* play, ColliderJntSph* collider, Actor* actor, ColliderJntSphInit* init,
                       ColliderJntSphElement* elements) {
    Collider_InitJntSph(play, collider);
    Collider_SetJntSph(play, collider, actor, init, elements);
}

// The first floor below the probe must lie under the surface, or the water box belongs to a lower floor.
static bool FindDeepWaterSurface(PlayState* play, f32 x, f32 z, f32 originY, Vec3f* surfacePos) {
    f32 surface;
    WaterBox* waterBox;

    if (!WaterBox_GetSurface1(play, &play->colCtx, x, z, &surface, &waterBox)) {
        return false;
    }
    CollisionPoly* poly;
    Vec3f probe = { x, MAX(surface, originY) + 50.0f, z };
    f32 floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);

    if (floorY <= BGCHECK_Y_MIN || surface - floorY < BIO_BABA_MIN_DEPTH) {
        return false;
    }
    surfacePos->x = x;
    surfacePos->y = surface;
    surfacePos->z = z;
    return true;
}

static bool IsWallBetween(PlayState* play, Vec3f* from, Vec3f* to) {
    Vec3f a = { from->x, MAX(from->y, to->y) + 40.0f, from->z };
    Vec3f b = { to->x, a.y, to->z };
    Vec3f hit;
    CollisionPoly* poly;
    s32 bgId;

    return BgCheck_EntityLineTest1(&play->colCtx, &a, &b, &hit, &poly, true, false, false, true, &bgId);
}

// Rings outward from the spawn for the closest deep enough water surface to float the lily pad on.
static bool BioBaba_FindLilyPadSpot(PlayState* play, Vec3f* origin, Vec3f* spot) {
    if (FindDeepWaterSurface(play, origin->x, origin->z, origin->y, spot)) {
        return true;
    }
    for (f32 radius = WATER_SEARCH_STEP; radius <= WATER_SEARCH_RADIUS; radius += WATER_SEARCH_STEP) {
        for (s32 step = 0; step < WATER_SEARCH_ANGLES; step++) {
            s16 angle = step * (0x10000 / WATER_SEARCH_ANGLES);
            f32 x = origin->x + Math_SinS(angle) * radius;
            f32 z = origin->z + Math_CosS(angle) * radius;

            if (FindDeepWaterSurface(play, x, z, origin->y, spot) && !IsWallBetween(play, origin, spot)) {
                return true;
            }
        }
    }
    return false;
}

static bool BioBaba_IsIgnoredHit(EnBioBaba* this) {
    return this->dyna.actor.colChkInfo.damage == 0 &&
           this->dyna.actor.colChkInfo.damageEffect == BIO_BABA_DMGEFF_IMMUNE;
}

static void BioBaba_SetAura(EnBioBaba* this, BioBabaAura aura) {
    this->aura = aura;
    this->auraTimer = aura == BIO_BABA_AURA_NONE ? 0 : 80;
    if (aura == BIO_BABA_AURA_FROZEN) {
        Actor_SetColorFilter(&this->dyna.actor, COLORFILTER_BLUE, 120, COLORFILTER_OPA, this->auraTimer);
    }
}

static BioBabaAura BioBaba_GetAuraOfEffect(u8 effect) {
    switch (effect) {
        case BIO_BABA_DMGEFF_FIRE:
            return BIO_BABA_AURA_FIRE;
        case BIO_BABA_DMGEFF_FREEZE:
            return BIO_BABA_AURA_FROZEN;
        case BIO_BABA_DMGEFF_LIGHT:
            return BIO_BABA_AURA_LIGHT;
        default:
            return BIO_BABA_AURA_NONE;
    }
}

static void BioBaba_Thaw(EnBioBaba* this, PlayState* play) {
    static Color_RGBA8 sIcePrimColor = { 170, 255, 255, 255 };
    static Color_RGBA8 sIceEnvColor = { 200, 200, 255, 255 };
    static Vec3f sIceAccel = { 0.0f, -1.0f, 0.0f };

    SoundSource_PlaySfxAtFixedWorldPos(play, &this->auraPos, 30, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < 8; i++) {
        Vec3f velocity = { Rand_CenteredFloat(7.0f), Rand_ZeroFloat(6.0f) + 4.0f, Rand_CenteredFloat(7.0f) };
        Vec3f pos = { this->auraPos.x + velocity.x, this->auraPos.y + velocity.y, this->auraPos.z + velocity.z };

        EffectSsEnIce_Spawn(play, &pos, Rand_ZeroFloat(0.5f) + 0.7f, &velocity, &sIceAccel, &sIcePrimColor,
                            &sIceEnvColor, 30);
    }
    BioBaba_SetAura(this, BIO_BABA_AURA_NONE);
}

static void BioBaba_UpdateAura(EnBioBaba* this, PlayState* play) {
    if (this->auraTimer != 0) {
        return;
    }
    if (this->aura == BIO_BABA_AURA_FROZEN) {
        BioBaba_Thaw(this, play);
    }
    this->aura = BIO_BABA_AURA_NONE;
}

static void BioBaba_SpawnBubbles(PlayState* play, Vec3f* center, s32 count) {
    for (s32 i = 0; i < count; i++) {
        Vec3f pos = { Rand_CenteredFloat(40.0f) + center->x, Rand_CenteredFloat(40.0f) + center->y,
                      Rand_CenteredFloat(40.0f) + center->z };

        EffectSsBubble_Spawn(play, &pos, 20.0f, 10.0f, 20.0f, 0.13f);
    }
}

static void BioBaba_SpawnFragment(EnBioBaba* this, PlayState* play, Vec3f* pos, Vec3s* rot, s32 index, s16 timer) {
    EnBioBabaFragment* fragment = (EnBioBabaFragment*)Actor_Spawn(&play->actorCtx, play, sFragmentActorId, pos->x,
                                                                  pos->y, pos->z, rot->x, rot->y, rot->z, index);

    if (fragment == NULL) {
        return;
    }
    for (s32 i = 0; i < HEAD_LIMB_MAX; i++) {
        fragment->jointTable[i] = this->headJointTable[i];
    }
    if (timer > 0) {
        fragment->timer = timer;
    }
}

// The pad cannot change hands while Link may stand on it: the head drops its collision as the new pad raises its own.
static void BioBaba_LeaveLilyPad(EnBioBaba* this, PlayState* play) {
    Actor* actor = &this->dyna.actor;

    if (!this->hasLilyPadCollision) {
        return;
    }
    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    this->dyna.bgId = BGACTOR_NEG_ONE;
    this->hasLilyPadCollision = false;
    Actor_Spawn(&play->actorCtx, play, sLilyPadActorId, actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, 0,
                actor->shape.rot.y, 0, 0);
}

static void BioBaba_Defeat(EnBioBaba* this) {
    GameInteractor_ExecuteOnEnemyDefeat(&this->dyna.actor);
    Actor_Kill(&this->dyna.actor);
}

static void BioBaba_SetupFall(EnBioBaba* this, BioBabaAura aura) {
    this->form = BIO_BABA_FORM_FALLING;
    this->fallingStemScale = 1.0f;
    this->dyna.actor.velocity.y = 0.0f;
    ActorShape_Init(&this->dyna.actor.shape, 0.0f, ActorShadow_DrawCircle, 30.0f);
    BioBaba_SetAura(this, aura);
    this->actionFunc = BioBaba_Fall;
}

static void BioBaba_SetupTransform(EnBioBaba* this) {
    Animation_MorphToPlayOnce(&this->headSkelAnime, (AnimationHeader*)sHeadTransformAnim, -5.0f);
    this->actionFunc = BioBaba_Transform;
}

// The walking head starts afresh, as MM's newly spawned one does: full health, no aura, limbs still to grow.
static void BioBaba_BecomeWalkingHead(EnBioBaba* this, PlayState* play) {
    Actor* actor = &this->dyna.actor;

    this->form = BIO_BABA_FORM_WALKING;
    actor->colChkInfo.mass = 90;
    actor->colChkInfo.health = BIO_BABA_HEALTH;
    actor->colChkInfo.damageTable = &sWalkingDamageTable;
    actor->gravity = -0.3f;
    actor->minVelocityY = -20.0f;
    actor->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    Collider_DestroyJntSph(play, &this->headCollider);
    InitJntSph(play, &this->headCollider, actor, &sWalkingHeadJntSphInit, this->headColliderElements);
    ActorShape_Init(&actor->shape, 0.0f, ActorShadow_DrawCircle, 30.0f);
    this->headBodyScale = 0.0f;
    this->headLimbScale = 0.0f;
    this->lowerJawScaleXZ = 1.0f;
    this->damagedTimer = 0;
    this->damagedFlashTimer = 0;
    this->knockbackMagnitude = 0.0f;
    BioBaba_SetAura(this, BIO_BABA_AURA_NONE);
    BioBaba_SetupTransform(this);
}

static BioBabaHangingHit BioBaba_CheckHangingHit(EnBioBaba* this, PlayState* play) {
    if (this->damagedFlashTimer != 0 || BioBaba_IsIgnoredHit(this)) {
        return BIO_BABA_HIT_NONE;
    }
    for (s32 i = 0; i < ARRAY_COUNT(this->lilyPadColliderElements); i++) {
        if (this->lilyPadCollider.elements[i].info.bumperFlags & BUMP_HIT) {
            return BIO_BABA_HIT_STEM_CUT;
        }
    }
    if (!(this->headCollider.elements[0].info.bumperFlags & BUMP_HIT)) {
        return BIO_BABA_HIT_NONE;
    }
    if (Actor_ApplyDamage(&this->dyna.actor) == 0) {
        Enemy_StartFinishingBlow(play, &this->dyna.actor);
        return BIO_BABA_HIT_DEAD;
    }
    PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA2_DAMAGE, NA_SE_EN_DEKU_DAMAGE);
    this->damagedFlashTimer = 15;
    this->attackState = BIO_BABA_ATTACK_IDLE;
    this->actionTimer = 30;
    return BIO_BABA_HIT_NONE;
}

static void BioBaba_ReactToHangingHit(EnBioBaba* this, PlayState* play) {
    BioBabaHangingHit hit = BioBaba_CheckHangingHit(this, play);

    if (hit == BIO_BABA_HIT_STEM_CUT) {
        PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA1_DAMAGE, NA_SE_EN_DEKU_DAMAGE);
        BioBaba_LeaveLilyPad(this, play);
        BioBaba_SetupFall(this, BioBaba_GetAuraOfEffect(this->dyna.actor.colChkInfo.damageEffect));
    } else if (hit == BIO_BABA_HIT_DEAD) {
        for (s32 i = 0; i < HANGING_FRAGMENT_COUNT; i++) {
            BioBaba_SpawnFragment(this, play, &this->headPos, &this->headRot, i, Rand_ZeroFloat(20.0f) + 20.0f);
        }
        PlayDeathSfx(&this->dyna.actor, MM_SE_EN_MIZUBABA_DEAD, NA_SE_EN_DEKU_DEAD);
        BioBaba_LeaveLilyPad(this, play);
        BioBaba_Defeat(this);
    }
}

static void BioBaba_GetStemPose(EnBioBaba* this, Vec3s* target, s16* fraction, s16* step) {
    const Vec3s* pose = this->attackState == BIO_BABA_ATTACK_WIND_UP ? sWindUpStemRot : sLungeStemRot;
    bool isSwaying = this->attackState == BIO_BABA_ATTACK_IDLE || this->attackState == BIO_BABA_ATTACK_FLIP;

    if (!isSwaying) {
        *fraction = this->attackState == BIO_BABA_ATTACK_WIND_UP ? 5 : 1;
        *step = this->attackState == BIO_BABA_ATTACK_WIND_UP ? 0x1000 : 0x2000;
        for (s32 i = 0; i < STEM_ROT_MAX; i++) {
            target[i] = pose[i];
        }
        return;
    }
    const StemSway* sway = this->attackState == BIO_BABA_ATTACK_FLIP ? &sFlipSway
                           : this->damagedFlashTimer != 0            ? &sHurtSway
                                                                     : &sIdleSway;
    s32 frame = this->frameCounter;

    *fraction = sway->fraction;
    *step = sway->step;
    for (s32 i = 0; i < STEM_ROT_MAX; i++) {
        bool isStem = i <= STEM_ROT_LOWER_STEM;
        bool isLowerArm = i == STEM_ROT_LEFT_LOWER_ARM || i == STEM_ROT_RIGHT_LOWER_ARM;
        s16 swayX = Math_SinS(frame * sway->frequency[0] + i * sway->shift[0]) * sway->amplitude[0];
        s16 swayY = Math_SinS(frame * sway->frequency[1] + i * sway->shift[1]) * sway->amplitude[1];

        target[i].x = isStem ? swayX : 0;
        target[i].y = isStem ? 0 : swayY;
        target[i].z = Math_SinS(frame * sway->frequency[2] + i * sway->shift[2]) * sway->amplitude[2] *
                      (isLowerArm ? 2.0f : 1.0f);
    }
}

static void BioBaba_SwayStem(EnBioBaba* this, f32 playerHeightAbovePad) {
    Vec3s target[STEM_ROT_MAX];
    s16 fraction;
    s16 step;

    BioBaba_GetStemPose(this, target, &fraction, &step);
    for (s32 i = 0; i < STEM_ROT_MAX; i++) {
        Math_ApproachS(&this->stemRot[i].x, target[i].x, fraction, step);
        Math_ApproachS(&this->stemRot[i].y, target[i].y, fraction, step);
        Math_ApproachS(&this->stemRot[i].z, target[i].z, fraction, step);
    }
    if (playerHeightAbovePad < 0.0f) {
        Math_ApproachS(&this->dyna.actor.shape.rot.y, this->dyna.actor.yawTowardsPlayer, fraction, step);
    }
}

// Snaps at Link from below, but flips its pad over on whoever stands on it for a second.
static bool BioBaba_UpdateHangingAttack(EnBioBaba* this, PlayState* play, f32 playerHeightAbovePad) {
    Player* player = GET_PLAYER(play);
    f32 distToPlayer = sqrtf(this->dyna.actor.xyzDistToPlayerSq);
    bool isPlayerAbove = playerHeightAbovePad >= 0.0f;

    switch (this->attackState) {
        case BIO_BABA_ATTACK_IDLE: {
            f32 attackRange = player->actor.speedXZ > 10.0f ? 220.0f : 150.0f;

            this->headSkelAnime.playSpeed = 1.0f;
            if (!isPlayerAbove) {
                if (this->actionTimer == 0 && distToPlayer <= attackRange) {
                    this->attackState = BIO_BABA_ATTACK_WIND_UP;
                    this->actionTimer = 10;
                }
                this->flipAttackFrames = 0;
            } else if (distToPlayer <= 40.0f && ++this->flipAttackFrames > 30) {
                this->attackState = BIO_BABA_ATTACK_FLIP;
                this->actionTimer = 30;
                PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA1_ATTACK, NA_SE_EN_DEKU_JR_ATTACK);
            } else if (distToPlayer > 40.0f) {
                this->flipAttackFrames = 0;
            }
            if (this->frameCounter % 4 == 0 && Rand_ZeroOne() < 0.5f) {
                PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA1_MOUTH, NA_SE_EN_DEKU_JR_MOUTH);
            }
            return isPlayerAbove;
        }
        case BIO_BABA_ATTACK_FLIP:
            if (this->frameCounter % 2 == 0) {
                PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA1_MOUTH, NA_SE_EN_DEKU_JR_MOUTH);
            }
            this->stemRotX = (this->actionTimer & 1) * 0x200;
            this->headSkelAnime.playSpeed = 4.0f;
            Math_ApproachS(&this->dyna.actor.shape.rot.x, -0x8000, 2, 0x2000);
            Math_ApproachS(&this->dyna.actor.shape.rot.y, this->dyna.actor.yawTowardsPlayer, 2, 0x2000);
            if (this->actionTimer == 27) {
                Vec3f splashPos = this->dyna.actor.world.pos;

                splashPos.y += 40.0f;
                EffectSsGSplash_Spawn(play, &splashPos, NULL, NULL, 1, 2000);
                PlaySfx(&this->dyna.actor, MM_SE_EV_OUT_OF_WATER, NA_SE_EV_OUT_OF_WATER);
            }
            if (this->actionTimer == 0) {
                this->flipAttackFrames = 0;
                this->attackState = BIO_BABA_ATTACK_IDLE;
                this->actionTimer = 100;
            }
            return false;
        case BIO_BABA_ATTACK_WIND_UP:
            if (Animation_OnFrame(&this->headSkelAnime, this->animEndFrame)) {
                this->headSkelAnime.playSpeed = 0.0f;
            }
            if (this->actionTimer == 0) {
                this->attackState = BIO_BABA_ATTACK_LUNGE;
                this->actionTimer = 20;
                PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA1_ATTACK, NA_SE_EN_DEKU_JR_ATTACK);
            }
            return false;
        case BIO_BABA_ATTACK_LUNGE:
            if (this->frameCounter % 2 == 0) {
                PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA1_MOUTH, NA_SE_EN_DEKU_JR_MOUTH);
            }
            this->stemRotX = (this->actionTimer & 1) * 0x200;
            this->headSkelAnime.playSpeed = 4.0f;
            Math_ApproachF(&this->lowerJawScaleXZ, 1.5f, 1.0f, 0.7f);
            if (this->actionTimer == 0) {
                this->attackState = BIO_BABA_ATTACK_IDLE;
                this->actionTimer = 30;
            }
            return false;
        default:
            this->attackState = BIO_BABA_ATTACK_IDLE;
            return false;
    }
}

static void BioBaba_Hang(EnBioBaba* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 playerHeightAbovePad = player->actor.world.pos.y - this->dyna.actor.world.pos.y + 10.0f;

    BioBaba_SwayStem(this, playerHeightAbovePad);
    SkelAnime_Update(&this->headSkelAnime);
    Math_ApproachF(&this->lowerJawScaleXZ, 1.0f, 0.1f, 0.1f);
    if (this->attackState != BIO_BABA_ATTACK_FLIP) {
        Math_ApproachS(&this->dyna.actor.shape.rot.x, 0, 0x14, 0x800);
    }
    bool isHarmless = BioBaba_UpdateHangingAttack(this, play, playerHeightAbovePad);

    BioBaba_ReactToHangingHit(this, play);
    if (this->actionFunc != BioBaba_Hang || this->dyna.actor.update == NULL) {
        return;
    }
    if (!isHarmless) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->lilyPadCollider.base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->headCollider.base);
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->lilyPadCollider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->headCollider.base);
}

// Collision is checked 85 units down, where the head hangs, so it lands head first.
static void BioBaba_Fall(EnBioBaba* this, PlayState* play) {
    Actor* actor = &this->dyna.actor;

    Actor_MoveXZGravity(actor);
    actor->world.pos.y -= BIO_BABA_HEAD_DROP;
    actor->prevPos.y -= BIO_BABA_HEAD_DROP;
    Actor_UpdateBgCheckInfo(play, actor, 20.0f, 50.0f, 40.0f, BGCHECK_UPDATE_FLOOR);
    actor->world.pos.y += BIO_BABA_HEAD_DROP;
    actor->prevPos.y += BIO_BABA_HEAD_DROP;
    bool isInWater = (actor->bgCheckFlags & BGCHECKFLAG_WATER) != 0;

    actor->gravity = isInWater ? -0.3f : -2.0f;
    actor->minVelocityY = isInWater ? -5.0f : -20.0f;
    if (actor->bgCheckFlags & BGCHECKFLAG_WATER_TOUCH) {
        actor->velocity.y = -3.0f;
        actor->gravity = -0.3f;
        actor->minVelocityY = -5.0f;
        EffectSsGSplash_Spawn(play, &actor->world.pos, NULL, NULL, 1, 1000);
        this->bubbleTimer = 20;
    }
    if (this->bubbleTimer != 0) {
        BioBaba_SpawnBubbles(play, &actor->world.pos, 3);
    }
    if (!(actor->bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return;
    }
    if (this->aura == BIO_BABA_AURA_FROZEN) {
        this->auraTimer = 0;
    }
    Math_ApproachZeroF(&this->fallingStemScale, 1.0f, 0.05f);
    if (this->fallingStemScale != 0.0f) {
        return;
    }
    actor->world.pos = this->headPos;
    actor->prevPos = this->headPos;
    actor->world.rot = this->headRot;
    actor->shape.rot = this->headRot;
    actor->velocity.y = 0.0f;
    BioBaba_BecomeWalkingHead(this, play);
    PlaySfx(actor, MM_SE_EN_MIZUBABA_TRANSFORM, NA_SE_EN_DEKU_WAKEUP);
}

static bool BioBaba_IsLookingAtPlayer(EnBioBaba* this) {
    s16 yawDiff = this->dyna.actor.yawTowardsPlayer - this->dyna.actor.shape.rot.y;

    return ABS(yawDiff) < 0x3000;
}

static void BioBaba_SetupSpottedPlayer(EnBioBaba* this) {
    Animation_MorphToPlayOnce(&this->headSkelAnime, (AnimationHeader*)sHeadSpotAnim, 0.0f);
    this->actionTimer = 20;
    this->actionFunc = BioBaba_SpottedPlayer;
}

static void BioBaba_TrySpottingPlayer(EnBioBaba* this) {
    bool isPlayerNear =
        this->dyna.actor.xyzDistToPlayerSq <= SQ(200.0f) && fabsf(this->dyna.actor.yDistToPlayer) < 70.0f;

    if (BioBaba_IsLookingAtPlayer(this) && isPlayerNear) {
        BioBaba_SetupSpottedPlayer(this);
    }
}

static void BioBaba_SetupIdle(EnBioBaba* this) {
    Animation_MorphToLoop(&this->headSkelAnime, (AnimationHeader*)sHeadIdleAnim, -10.0f);
    this->actionTimer = Rand_ZeroFloat(25.0f) + 25.0f;
    PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA1_MOUTH, NA_SE_EN_DEKU_JR_MOUTH);
    this->actionFunc = BioBaba_Idle;
}

// Grows its legs and eyestalks back to full size before it gets up.
static void BioBaba_Transform(EnBioBaba* this, PlayState* play) {
    SkelAnime_Update(&this->headSkelAnime);
    Math_ApproachS(&this->dyna.actor.shape.rot.x, 0, 2, 0x400);
    Math_ApproachS(&this->dyna.actor.shape.rot.z, 0, 2, 0x400);
    Math_ApproachF(&this->headBodyScale, 1.0f, 1.0f, 0.5f);
    Math_ApproachF(&this->headLimbScale, 1.0f, 1.0f, 0.14f);
    if (this->headLimbScale == 1.0f) {
        BioBaba_SetupIdle(this);
    }
}

static void BioBaba_SetupWalk(EnBioBaba* this) {
    Animation_MorphToLoop(&this->headSkelAnime, (AnimationHeader*)sHeadWalkAnim, 0.0f);
    this->actionTimer = Rand_ZeroFloat(80.0f) + 60.0f;
    this->walkTargetPos.x = Rand_CenteredFloat(400.0f) + this->dyna.actor.world.pos.x;
    this->walkTargetPos.z = Rand_CenteredFloat(400.0f) + this->dyna.actor.world.pos.z;
    this->walkAngularVelocity = 0.0f;
    this->actionFunc = BioBaba_Walk;
}

static void BioBaba_Idle(EnBioBaba* this, PlayState* play) {
    SkelAnime_Update(&this->headSkelAnime);
    Math_ApproachZeroF(&this->dyna.actor.speedXZ, 1.0f, 2.5f);
    Math_ApproachS(&this->dyna.actor.shape.rot.x, 0, 2, 0x400);
    Math_ApproachS(&this->dyna.actor.shape.rot.z, 0, 2, 0x400);
    if (this->actionTimer == 0) {
        BioBaba_SetupWalk(this);
    }
    BioBaba_TrySpottingPlayer(this);
}

static void BioBaba_Walk(EnBioBaba* this, PlayState* play) {
    f32 diffX = this->walkTargetPos.x - this->dyna.actor.world.pos.x;
    f32 diffZ = this->walkTargetPos.z - this->dyna.actor.world.pos.z;

    PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA2_WALK - SFX_FLAG, NA_SE_EN_DEKU_SCRAPE);
    SkelAnime_Update(&this->headSkelAnime);
    Math_ApproachF(&this->dyna.actor.speedXZ, 5.0f, 1.0f, 2.0f);
    Math_ApproachS(&this->dyna.actor.world.rot.y, Math_Atan2S(diffZ, diffX), 5, this->walkAngularVelocity);
    Math_ApproachF(&this->walkAngularVelocity, 2000.0f, 1.0f, 100.0f);
    if (this->actionTimer == 0 || SQ(diffX) + SQ(diffZ) < SQ(50.0f)) {
        BioBaba_SetupIdle(this);
    }
    BioBaba_TrySpottingPlayer(this);
}

static void BioBaba_SetupCharge(EnBioBaba* this) {
    Animation_MorphToLoop(&this->headSkelAnime, (AnimationHeader*)sHeadChargeAnim, 0.0f);
    this->actionTimer = 60;
    this->walkAngularVelocity = 0.0f;
    this->actionFunc = BioBaba_Charge;
}

static void BioBaba_SpottedPlayer(EnBioBaba* this, PlayState* play) {
    SkelAnime_Update(&this->headSkelAnime);
    PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA2_VOICE - SFX_FLAG, NA_SE_EN_DEKU_JR_MOUTH);
    Math_ApproachZeroF(&this->dyna.actor.speedXZ, 1.0f, 2.5f);
    Math_ApproachS(&this->dyna.actor.world.rot.y, this->dyna.actor.yawTowardsPlayer, 5, 0x1000);
    if (this->actionTimer == 0) {
        BioBaba_SetupCharge(this);
    }
}

static void BioBaba_SetupAttack(EnBioBaba* this) {
    AnimationHeader* attack = (AnimationHeader*)sHeadAttackAnim;

    Animation_MorphToPlayOnce(&this->headSkelAnime, attack, 0.0f);
    this->animEndFrame = Animation_GetLastFrame(attack);
    PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA2_ATTACK, NA_SE_EN_DEKU_ATTACK);
    this->actionFunc = BioBaba_Attack;
}

static void BioBaba_Charge(EnBioBaba* this, PlayState* play) {
    PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA2_VOICE - SFX_FLAG, NA_SE_EN_DEKU_JR_MOUTH);
    SkelAnime_Update(&this->headSkelAnime);
    Math_ApproachF(&this->dyna.actor.speedXZ, 8.0f, 1.0f, 4.0f);
    Math_ApproachS(&this->dyna.actor.world.rot.y, this->dyna.actor.yawTowardsPlayer, 5, this->walkAngularVelocity);
    Math_ApproachF(&this->walkAngularVelocity, 4000.0f, 1.0f, 400.0f);
    if (this->actionTimer == 0 || this->dyna.actor.xyzDistToPlayerSq <= SQ(150.0f)) {
        BioBaba_SetupAttack(this);
    }
}

static void BioBaba_Attack(EnBioBaba* this, PlayState* play) {
    SkelAnime_Update(&this->headSkelAnime);
    if (Animation_OnFrame(&this->headSkelAnime, this->animEndFrame)) {
        BioBaba_SetupIdle(this);
    }
}

static void BioBaba_SetupDamaged(EnBioBaba* this) {
    Animation_MorphToPlayOnce(&this->headSkelAnime, (AnimationHeader*)sHeadDamagedAnim, 0.0f);
    this->animEndFrame = Animation_GetLastFrame((AnimationHeader*)sHeadAttackAnim);
    Actor_SetColorFilter(&this->dyna.actor, COLORFILTER_RED, 120, COLORFILTER_OPA, 30);
    this->actionFunc = BioBaba_Damaged;
}

// Bursts into its fourteen limbs in a cloud of bubbles 22 frames into the hurt animation.
static void BioBaba_BreakApart(EnBioBaba* this, PlayState* play) {
    Actor* actor = &this->dyna.actor;

    for (s32 i = 0; i < FRAGMENT_COUNT; i++) {
        BioBaba_SpawnFragment(this, play, &actor->world.pos, &actor->shape.rot, i, 0);
    }
    BioBaba_SpawnBubbles(play, &actor->world.pos, 20);
    PlayDeathSfx(actor, MM_SE_EN_MIZUBABA_DEAD, NA_SE_EN_DEKU_DEAD);
    Item_DropCollectibleRandom(play, NULL, &actor->world.pos, 0x60);
    BioBaba_Defeat(this);
}

static void BioBaba_Damaged(EnBioBaba* this, PlayState* play) {
    SkelAnime_Update(&this->headSkelAnime);
    if ((s8)this->dyna.actor.colChkInfo.health <= 0) {
        if (Animation_OnFrame(&this->headSkelAnime, 22.0f)) {
            BioBaba_BreakApart(this, play);
        }
    } else if (Animation_OnFrame(&this->headSkelAnime, this->animEndFrame)) {
        BioBaba_SetupSpottedPlayer(this);
    }
}

static void BioBaba_SetupStunned(EnBioBaba* this, s16 duration) {
    PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA2_DAMAGE, NA_SE_EN_DEKU_DAMAGE);
    this->actionTimer = duration;
    Actor_SetColorFilter(&this->dyna.actor, COLORFILTER_BLUE, 120, COLORFILTER_OPA, duration);
    this->actionFunc = BioBaba_Stunned;
}

static void BioBaba_Stunned(EnBioBaba* this, PlayState* play) {
    Math_ApproachZeroF(&this->dyna.actor.speedXZ, 1.0f, 2.5f);
    if (this->actionTimer == 0) {
        BioBaba_SetupIdle(this);
    }
}

static void BioBaba_TakeWalkingDamage(EnBioBaba* this, PlayState* play) {
    bool isFrozenStiff = this->actionFunc == BioBaba_Stunned && this->aura == BIO_BABA_AURA_FROZEN;

    if (isFrozenStiff) {
        BioBaba_Thaw(this, play);
    }
    if (Actor_ApplyDamage(&this->dyna.actor) == 0) {
        Enemy_StartFinishingBlow(play, &this->dyna.actor);
    }
    BioBaba_SetupDamaged(this);
    this->damagedFlashTimer = 15;
}

// A shield bash only shoves it back; the rest knocks it away from Link and hurts, stuns or freezes it.
static void BioBaba_UpdateWalkingDamage(EnBioBaba* this, PlayState* play) {
    ColliderJntSphElement* head = &this->headCollider.elements[0];

    if (this->damagedTimer != 0 || !(head->info.bumperFlags & BUMP_HIT)) {
        return;
    }
    head->info.bumperFlags &= ~BUMP_HIT;
    if (head->info.acHitInfo != NULL && (head->info.acHitInfo->toucher.dmgFlags & (DMG_SHIELD | DMG_MIR_RAY))) {
        this->knockbackMagnitude = -12.0f;
        this->knockbackAngle = this->dyna.actor.yawTowardsPlayer;
        this->damagedTimer = 6;
        return;
    }
    if (BioBaba_IsIgnoredHit(this)) {
        return;
    }
    this->damagedTimer = 10;
    this->dyna.actor.speedXZ = 0.0f;
    this->knockbackMagnitude = -20.0f;
    this->knockbackAngle = this->dyna.actor.yawTowardsPlayer;
    PlaySfx(&this->dyna.actor, MM_SE_EN_MIZUBABA2_DAMAGE, NA_SE_EN_DEKU_DAMAGE);
    switch (this->dyna.actor.colChkInfo.damageEffect) {
        case BIO_BABA_DMGEFF_STUN:
            BioBaba_SetupStunned(this, 40);
            break;
        case BIO_BABA_DMGEFF_FREEZE:
            BioBaba_SetupStunned(this, 80);
            BioBaba_SetAura(this, BIO_BABA_AURA_FROZEN);
            break;
        case BIO_BABA_DMGEFF_FIRE:
        case BIO_BABA_DMGEFF_LIGHT:
            BioBaba_SetAura(this, BioBaba_GetAuraOfEffect(this->dyna.actor.colChkInfo.damageEffect));
            BioBaba_TakeWalkingDamage(this, play);
            break;
        default:
            BioBaba_TakeWalkingDamage(this, play);
            break;
    }
}

static void BioBaba_UpdateWalkingHead(EnBioBaba* this, PlayState* play) {
    Actor* actor = &this->dyna.actor;

    Actor_MoveXZGravity(actor);
    actor->world.pos.x += Math_SinS(this->knockbackAngle) * this->knockbackMagnitude;
    actor->world.pos.z += Math_CosS(this->knockbackAngle) * this->knockbackMagnitude;
    Math_ApproachZeroF(&this->knockbackMagnitude, 1.0f, 1.0f);
    Actor_UpdateBgCheckInfo(play, actor, 20.0f, 50.0f, 40.0f, BGCHECK_UPDATE_WALL_FLOOR);
    BioBaba_UpdateWalkingDamage(this, play);
    if (actor->update == NULL) {
        return;
    }
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->headCollider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->headCollider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->headCollider.base);
    actor->shape.rot.y = actor->world.rot.y;
}

static void EnBioBaba_Init(Actor* thisx, PlayState* play) {
    EnBioBaba* this = (EnBioBaba*)thisx;
    Vec3f padSpot;

    this->dyna.actor.targetMode = 3;
    this->dyna.actor.colChkInfo.mass = MASS_HEAVY;
    this->dyna.actor.colChkInfo.health = BIO_BABA_HEALTH;
    this->dyna.actor.gravity = -0.3f;
    this->frameCounter = Rand_ZeroFloat(1000.0f);
    this->lowerJawScaleXZ = 1.0f;
    Actor_SetScale(&this->dyna.actor, 0.01f);
    SkelAnime_InitFlex(play, &this->lilyPadSkelAnime, (FlexSkeletonHeader*)sLilyPadSkel,
                       (AnimationHeader*)sLilyPadIdleAnim, this->lilyPadJointTable, this->lilyPadMorphTable,
                       LILY_PAD_LIMB_MAX);
    SkelAnime_InitFlex(play, &this->headSkelAnime, (FlexSkeletonHeader*)sHeadSkel, (AnimationHeader*)sHeadChompAnim,
                       this->headJointTable, this->headMorphTable, HEAD_LIMB_MAX);
    this->animEndFrame = Animation_GetLastFrame((AnimationHeader*)sHeadChompAnim);
    InitJntSph(play, &this->lilyPadCollider, &this->dyna.actor, &sLilyPadJntSphInit, this->lilyPadColliderElements);
    InitJntSph(play, &this->headCollider, &this->dyna.actor, &sHangingHeadJntSphInit, this->headColliderElements);
    if (!BioBaba_FindLilyPadSpot(play, &this->dyna.actor.world.pos, &padSpot)) {
        BioBaba_BecomeWalkingHead(this, play);
        return;
    }
    CollisionHeader* colHeader = NULL;

    this->dyna.actor.world.pos = padSpot;
    this->dyna.actor.home.pos = padSpot;
    this->dyna.actor.shape.rot.z = 0;
    this->dyna.actor.world.rot.z = 0;
    this->dyna.actor.colChkInfo.damageTable = &sHangingDamageTable;
    DynaPolyActor_Init(&this->dyna, 0);
    CollisionHeader_GetVirtual((void*)sLilyPadCol, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);
    this->hasLilyPadCollision = true;
    this->form = BIO_BABA_FORM_HANGING;
    this->actionFunc = BioBaba_Hang;
}

static void EnBioBaba_Destroy(Actor* thisx, PlayState* play) {
    EnBioBaba* this = (EnBioBaba*)thisx;

    if (this->hasLilyPadCollision) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    }
    Collider_DestroyJntSph(play, &this->lilyPadCollider);
    Collider_DestroyJntSph(play, &this->headCollider);
}

static void EnBioBaba_Update(Actor* thisx, PlayState* play) {
    EnBioBaba* this = (EnBioBaba*)thisx;

    this->frameCounter++;
    DECR(this->actionTimer);
    DECR(this->bubbleTimer);
    DECR(this->damagedTimer);
    DECR(this->damagedFlashTimer);
    DECR(this->auraTimer);
    this->actionFunc(this, play);
    if (this->dyna.actor.update == NULL) {
        return;
    }
    if (this->form == BIO_BABA_FORM_WALKING) {
        BioBaba_UpdateWalkingHead(this, play);
    }
    BioBaba_UpdateAura(this, play);
}

static s32 BioBaba_HangingStem_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                                void* thisx) {
    EnBioBaba* this = (EnBioBaba*)thisx;

    if (limbIndex == LILY_PAD_LIMB_UPPER_STEM) {
        rot->x += this->stemRotX;
    }
    s8 stemRotIndex = sLimbToStemRot[limbIndex];

    if (stemRotIndex >= 0) {
        rot->x += this->stemRot[stemRotIndex].x;
        rot->y += this->stemRot[stemRotIndex].y;
        rot->z += this->stemRot[stemRotIndex].z;
    }
    return false;
}

static void BioBaba_Stem_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnBioBaba* this = (EnBioBaba*)thisx;

    if (limbIndex == LILY_PAD_LIMB_LOWER_STEM) {
        MtxF stemMtx;

        Matrix_MultVec3f(&sLilyPadHeadOffset, &this->headPos);
        if (this->form == BIO_BABA_FORM_FALLING) {
            this->dyna.actor.focus.pos = this->headPos;
        }
        Matrix_Get(&stemMtx);
        Matrix_MtxFToYXZRotS(&stemMtx, &this->headRot, 0);
        this->headRot.x += 0xF00;
    } else if (limbIndex == LILY_PAD_LIMB_UPPER_STEM) {
        Vec3f upperStemPos;

        Matrix_MultVec3f(&sZeroVec, &upperStemPos);
        SetSphere(&this->lilyPadCollider, 0, &upperStemPos);
    } else if (limbIndex == LILY_PAD_LIMB_MIDDLE_STEM) {
        Vec3f middleStemPos;

        Matrix_MultVec3f(&sMiddleStemColliderOffset, &middleStemPos);
        SetSphere(&this->lilyPadCollider, 1, &middleStemPos);
        if (this->form == BIO_BABA_FORM_HANGING) {
            Matrix_MultVec3f(&sMiddleStemFocusOffset, &this->dyna.actor.focus.pos);
        }
    }
}

static void BioBaba_Head_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnBioBaba* this = (EnBioBaba*)thisx;

    if (limbIndex != HEAD_LIMB_BODY) {
        return;
    }
    Vec3f colliderPos;

    Matrix_MultVec3f(&sHeadColliderOffset, &colliderPos);
    SetSphere(&this->headCollider, 0, &colliderPos);
    if (this->form == BIO_BABA_FORM_WALKING) {
        Matrix_MultVec3f(&sHeadCenterOffset, &this->dyna.actor.focus.pos);
    }
    Matrix_MultVec3f(&sHeadCenterOffset, &this->auraPos);
    if (this->aura == BIO_BABA_AURA_FIRE) {
        this->auraPos.y -= 15.0f;
    }
}

static void BioBaba_Head_TransformLimbDraw(PlayState* play, s32 limbIndex, void* thisx) {
    EnBioBaba* this = (EnBioBaba*)thisx;

    if (limbIndex == HEAD_LIMB_LOWER_JAW || limbIndex == HEAD_LIMB_UPPER_JAW) {
        Matrix_Scale(this->lowerJawScaleXZ, 1.0f, this->lowerJawScaleXZ, MTXMODE_APPLY);
    }
    if (limbIndex == HEAD_LIMB_BODY) {
        Matrix_Scale(this->headBodyScale, this->headBodyScale, this->headBodyScale, MTXMODE_APPLY);
    }
    if (limbIndex >= HEAD_LIMB_BACK_UPPER_LEG && limbIndex <= HEAD_LIMB_LEAVES) {
        Matrix_Scale(this->headLimbScale, this->headLimbScale, this->headLimbScale, MTXMODE_APPLY);
    }
}

static void BioBaba_NoTransformLimbDraw(PlayState* play, s32 limbIndex, void* thisx) {
}

static s32 BioBaba_FallingStem_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                                void* thisx) {
    if (limbIndex == LILY_PAD_LIMB_ROOTS || limbIndex == LILY_PAD_LIMB_UPPER_STEM || limbIndex == LILY_PAD_LIMB_LEAF) {
        *dList = NULL;
    }
    return false;
}

static void BioBaba_FallingStem_TransformLimbDraw(PlayState* play, s32 limbIndex, void* thisx) {
    EnBioBaba* this = (EnBioBaba*)thisx;

    if (limbIndex >= LILY_PAD_LIMB_MIDDLE_STEM && limbIndex <= LILY_PAD_LIMB_RIGHT_LOWER_ARM) {
        Matrix_Scale(this->fallingStemScale, this->fallingStemScale, this->fallingStemScale, MTXMODE_APPLY);
    }
}

static void BioBaba_DrawHead(EnBioBaba* this, PlayState* play) {
    DrawTransformFlexOpa(play, &this->headSkelAnime, NULL, BioBaba_Head_PostLimbDraw, BioBaba_Head_TransformLimbDraw,
                         this);
}

static void BioBaba_DrawHangingHead(EnBioBaba* this, PlayState* play) {
    Matrix_Translate(this->headPos.x, this->headPos.y, this->headPos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(this->headRot.y), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(this->headRot.x), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(this->headRot.z), MTXMODE_APPLY);
    Matrix_Scale(this->dyna.actor.scale.x, this->dyna.actor.scale.y, this->dyna.actor.scale.z, MTXMODE_APPLY);
    BioBaba_DrawHead(this, play);
}

// MM's fire and light damage effects become OoT's flame billboard tinted per element.
static void BioBaba_DrawAura(EnBioBaba* this, PlayState* play) {
    bool isLight = this->aura == BIO_BABA_AURA_LIGHT;
    u8 alpha = CLAMP_MAX(this->auraTimer * 0.05f, 1.0f) * 255.0f;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(this->auraPos.x, this->auraPos.y, this->auraPos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(BINANG_ROT180(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)))), MTXMODE_APPLY);
    Matrix_Scale(0.012f, 0.012f, 0.012f, MTXMODE_APPLY);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                           (play->gameplayFrames * -20) % 512, 0x20, 0x80));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, isLight ? 255 : 0, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, isLight ? 255 : 10, isLight ? 100 : 0, 255);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void BioBaba_SetHurtFog(EnBioBaba* this, PlayState* play) {
    if (this->damagedFlashTimer % 2 == 0) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    POLY_OPA_DISP = Gfx_SetFog(POLY_OPA_DISP, 255, 0, 0, 255, 900, 1099);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnBioBaba_Draw(Actor* thisx, PlayState* play) {
    EnBioBaba* this = (EnBioBaba*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    BioBaba_SetEyeFlashSegment(play);
    switch (this->form) {
        case BIO_BABA_FORM_HANGING:
            DrawTransformFlexOpa(play, &this->lilyPadSkelAnime, BioBaba_HangingStem_OverrideLimbDraw,
                                 BioBaba_Stem_PostLimbDraw, BioBaba_NoTransformLimbDraw, this);
            BioBaba_SetHurtFog(this, play);
            BioBaba_DrawHangingHead(this, play);
            break;
        case BIO_BABA_FORM_FALLING:
            DrawTransformFlexOpa(play, &this->lilyPadSkelAnime, BioBaba_FallingStem_OverrideLimbDraw,
                                 BioBaba_Stem_PostLimbDraw, BioBaba_FallingStem_TransformLimbDraw, this);
            BioBaba_DrawHangingHead(this, play);
            break;
        case BIO_BABA_FORM_WALKING:
            BioBaba_SetHurtFog(this, play);
            BioBaba_DrawHead(this, play);
            break;
        default:
            break;
    }
    OPEN_DISPS(play->state.gfxCtx);
    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
    bool hasFlames = this->aura == BIO_BABA_AURA_FIRE || this->aura == BIO_BABA_AURA_LIGHT;

    if (hasFlames && this->auraTimer != 0) {
        BioBaba_DrawAura(this, play);
    }
}

static void EnBioBabaLilyPad_Init(Actor* thisx, PlayState* play) {
    EnBioBabaLilyPad* this = (EnBioBabaLilyPad*)thisx;
    CollisionHeader* colHeader = NULL;

    Actor_SetScale(&this->dyna.actor, 0.01f);
    DynaPolyActor_Init(&this->dyna, 0);
    CollisionHeader_GetVirtual((void*)sLilyPadCol, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sLilyPadSkel, (AnimationHeader*)sLilyPadIdleAnim,
                       this->jointTable, this->morphTable, LILY_PAD_LIMB_MAX);
    this->frameCounter = Rand_ZeroFloat(1000.0f);
}

static void EnBioBabaLilyPad_Destroy(Actor* thisx, PlayState* play) {
    EnBioBabaLilyPad* this = (EnBioBabaLilyPad*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

// Dips toward Link and sends out ripples while he stands on it.
static void EnBioBabaLilyPad_Update(Actor* thisx, PlayState* play) {
    EnBioBabaLilyPad* this = (EnBioBabaLilyPad*)thisx;
    Player* player = GET_PLAYER(play);
    f32 distToPlayer = sqrtf(this->dyna.actor.xyzDistToPlayerSq);

    this->frameCounter++;
    if (distToPlayer >= 50.0f || !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        Math_ApproachS(&this->tilt, 0, 0x14, 0x100);
        return;
    }
    s16 targetTilt = CLAMP_MAX((s16)(distToPlayer * 80.0f), 0x7D0);

    Math_ApproachS(&this->tilt, targetTilt, 0x14, 0x7D0);
    Math_ApproachS(&this->tiltYaw, this->dyna.actor.yawTowardsPlayer, 0x14, 0xFA0);
    if (this->frameCounter % 16 == 0) {
        EffectSsGRipple_Spawn(play, &this->dyna.actor.world.pos, 500, 1000, 0);
    }
}

static s32 BioBabaLilyPad_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                           void* thisx) {
    if (limbIndex >= LILY_PAD_LIMB_MIDDLE_STEM && limbIndex <= LILY_PAD_LIMB_RIGHT_LOWER_ARM) {
        *dList = NULL;
    }
    return false;
}

static void EnBioBabaLilyPad_Draw(Actor* thisx, PlayState* play) {
    EnBioBabaLilyPad* this = (EnBioBabaLilyPad*)thisx;
    Actor* actor = &this->dyna.actor;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(this->tiltYaw), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(this->tilt), MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD((s16)(actor->shape.rot.y - this->tiltYaw)), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(actor->shape.rot.x), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(actor->shape.rot.z), MTXMODE_APPLY);
    Matrix_Scale(actor->scale.x, actor->scale.y, actor->scale.z, MTXMODE_APPLY);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          BioBabaLilyPad_OverrideLimbDraw, NULL, this);
}

static void EnBioBabaFragment_Init(Actor* thisx, PlayState* play) {
    EnBioBabaFragment* this = (EnBioBabaFragment*)thisx;

    Actor_SetScale(&this->actor, 0.01f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sHeadSkel, (AnimationHeader*)sHeadChompAnim,
                       this->jointTable, this->morphTable, HEAD_LIMB_MAX);
    this->actor.params = CLAMP(this->actor.params, 0, FRAGMENT_COUNT - 1);
    this->actor.gravity = 0.0f;
    this->actor.world.rot.y = Rand_ZeroFloat(0x8000);
    this->actor.speedXZ = Rand_ZeroFloat(3.0f) + 3.0f;
    this->actor.velocity.y = Rand_ZeroFloat(1.5f) + 1.5f;
    this->angularVelocity.x = Rand_CenteredFloat(700.0f);
    this->angularVelocity.y = Rand_CenteredFloat(1500.0f);
    this->timer = Rand_ZeroFloat(30.0f) + 50.0f;
    this->pos = this->actor.world.pos;
}

static void EnBioBabaFragment_Destroy(Actor* thisx, PlayState* play) {
}

// Underwater pieces drift up and fade; above water they fly off until they drop out of sight.
static void EnBioBabaFragment_Update(Actor* thisx, PlayState* play) {
    EnBioBabaFragment* this = (EnBioBabaFragment*)thisx;

    this->frameCounter++;
    DECR(this->timer);
    Actor_MoveXZGravity(&this->actor);
    switch (this->state) {
        case FRAGMENT_STATE_SPAWNED:
            Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 50.0f, 40.0f, BGCHECK_UPDATE_FLOOR);
            this->state =
                (this->actor.bgCheckFlags & BGCHECKFLAG_WATER) ? FRAGMENT_STATE_UNDERWATER : FRAGMENT_STATE_ABOVE_WATER;
            break;
        case FRAGMENT_STATE_UNDERWATER:
            Math_ApproachF(&this->actor.velocity.y, 1.0f, 1.0f, 0.1f);
            Math_ApproachZeroF(&this->actor.speedXZ, 0.5f, 0.5f);
            this->actor.shape.rot.x += this->angularVelocity.x;
            this->actor.shape.rot.y += this->angularVelocity.y;
            if (this->timer == 0) {
                Actor_Kill(&this->actor);
            }
            break;
        case FRAGMENT_STATE_ABOVE_WATER:
            this->actor.velocity.y = Rand_ZeroFloat(3.0f) + 3.0f;
            this->actor.speedXZ = Rand_CenteredFloat(5.0f) + 5.0f;
            this->actor.world.rot.y = Rand_ZeroFloat(0x10000);
            this->actor.gravity = -1.0f;
            this->state = FRAGMENT_STATE_FLYING;
            break;
        case FRAGMENT_STATE_FLYING:
            Actor_MoveXZGravity(&this->actor);
            if (this->pos.y < this->actor.floorHeight - 30.0f) {
                Actor_Kill(&this->actor);
            }
            break;
        default:
            Actor_Kill(&this->actor);
            break;
    }
}

static s32 BioBabaFragment_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            void* thisx) {
    EnBioBabaFragment* this = (EnBioBabaFragment*)thisx;

    if (limbIndex != sFragmentLimbs[this->actor.params]) {
        *dList = NULL;
    } else if (this->state >= FRAGMENT_STATE_ABOVE_WATER) {
        rot->x += this->frameCounter * 0x3000;
        rot->y += this->frameCounter * 0x1A00;
        rot->z += this->frameCounter * 0x2000;
    }
    return false;
}

static void BioBabaFragment_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnBioBabaFragment* this = (EnBioBabaFragment*)thisx;

    if (limbIndex == sFragmentLimbs[this->actor.params]) {
        Matrix_MultVec3f(&sZeroVec, &this->pos);
    }
}

static void EnBioBabaFragment_Draw(Actor* thisx, PlayState* play) {
    EnBioBabaFragment* this = (EnBioBabaFragment*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    BioBaba_SetEyeFlashSegment(play);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          BioBabaFragment_OverrideLimbDraw, BioBabaFragment_PostLimbDraw, this);
}

// MM's CanKillEnemy(ACTOR_BOSS_05) needs Zora fins; OoT cuts the stem by hookshot and fights the head in Iron Boots.
static bool BioBaba_CanKillInLogic(void* userData) {
    return sApi->LogicCanUseItem(RG_IRON_BOOTS) && sApi->LogicCanUseItem(RG_HOOKSHOT) &&
           sApi->LogicHasCapability("CanUseSword");
}

// MM's Deku Baba family soul, which any MM Deku Baba mod shares: all define it alike, so whichever loads first
// registers it.
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
    static Vec3s sSoulJointTable[HEAD_LIMB_MAX];
    static Vec3s sSoulMorphTable[HEAD_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sHeadSkel, (AnimationHeader*)sHeadIdleAnim,
                           sSoulJointTable, sSoulMorphTable, HEAD_LIMB_MAX);
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
    SOHCustomItemDefinition soul = Z64Items_Define(DEKU_BABA_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Deku Babas (MM)";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul,
                        "You got the %rSoul of Deku Babas (MM)%w!&Majora's Mask's Deku Babas can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_RegisterShared(sApi, &soul)) {
        sApi->Log("Could not register " DEKU_BABA_SOUL_ITEM_KEY);
    }
}

static s16 RegisterHelper(const char* key, const char* description, u8 category, u32 instanceSize, SOHActorFunc init,
                          SOHActorFunc destroy, SOHActorFunc update, SOHActorFunc draw) {
    SOHActorDefinition helper = { 0 };

    helper.structSize = sizeof(helper);
    helper.key = key;
    helper.description = description;
    helper.category = category;
    helper.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    helper.objectId = OBJECT_GAMEPLAY_KEEP;
    helper.instanceSize = instanceSize;
    helper.init = init;
    helper.destroy = destroy;
    helper.update = update;
    helper.draw = draw;
    s16 actorId = sApi->RegisterActor(&helper);

    if (actorId < 0) {
        sApi->Log("Could not register a Bio Deku Baba helper actor");
    }
    return actorId;
}

static void RegisterBioBaba(void) {
    SOHActorDefinition bioBaba = { 0 };

    bioBaba.structSize = sizeof(bioBaba);
    bioBaba.key = BIO_BABA_KEY;
    bioBaba.description = "Bio Deku Baba";
    bioBaba.category = ACTORCAT_ENEMY;
    bioBaba.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
    bioBaba.objectId = OBJECT_GAMEPLAY_KEEP;
    bioBaba.instanceSize = sizeof(EnBioBaba);
    bioBaba.init = EnBioBaba_Init;
    bioBaba.destroy = EnBioBaba_Destroy;
    bioBaba.update = EnBioBaba_Update;
    bioBaba.draw = EnBioBaba_Draw;
    bioBaba.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NEEDS_WATER | SOH_ACTOR_ENEMY_UNDERWATER;
    bioBaba.naviHint = "Bio Deku Baba&%cIt dangles under a lily pad. Cut its stem, then finish the head off before "
                       "it grows legs!%w";
    bioBaba.soulKey = DEKU_BABA_FAMILY_KEY;
    bioBaba.logicKill = BioBaba_CanKillInLogic;
    if (sApi->RegisterActor(&bioBaba) < 0) {
        sApi->Log("Could not register " BIO_BABA_KEY);
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
    sLilyPadActorId =
        RegisterHelper(BIO_BABA_LILY_PAD_KEY, "Bio Deku Baba Lily Pad", ACTORCAT_BG, sizeof(EnBioBabaLilyPad),
                       EnBioBabaLilyPad_Init, EnBioBabaLilyPad_Destroy, EnBioBabaLilyPad_Update, EnBioBabaLilyPad_Draw);
    sFragmentActorId = RegisterHelper(BIO_BABA_FRAGMENT_KEY, "Bio Deku Baba Fragment", ACTORCAT_MISC,
                                      sizeof(EnBioBabaFragment), EnBioBabaFragment_Init, EnBioBabaFragment_Destroy,
                                      EnBioBabaFragment_Update, EnBioBabaFragment_Draw);
    RegisterSoul();
    RegisterBioBaba();
}
