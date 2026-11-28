// Majora's Mask's Dinolfos (En_Dinofos) as an Unbound custom enemy, drawn from mm.o2r.
// It starts on the ground: the ceiling-drop intro needs a scene cutscene.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define DINOLFOS_KEY "mm.dinolfos"
#define DINOLFOS_SOUL_ITEM_KEY "soul.mm.dinolfos"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
#define GORON_MASK_KEY "nei.goron_form_mask"

#define OBJ_DINOLFOS "__OTR__objects/object_dinofos/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_FLOOR ((1 << 0) | (1 << 2))

#define MM_SE_EV_ICE_BROKEN 0x28CB
#define MM_SE_EN_DODO_J_FIRE 0x3802
#define MM_SE_EN_BOMCHU_WALK 0x3828
#define MM_SE_EN_RIZA_CRY 0x3829
#define MM_SE_EN_RIZA_ATTACK 0x382A
#define MM_SE_EN_RIZA_DAMAGE 0x382B
#define MM_SE_EN_RIZA_WARAU 0x382C
#define MM_SE_EN_RIZA_DEAD 0x382D
#define MM_SE_EN_RIZA_WALK 0x382E
#define MM_SE_EN_RIZA_JUMP 0x382F
#define MM_SE_EN_GERUDOFT_DOWN 0x387A
#define MM_SE_EN_COMMON_FREEZE 0x389E

#define DINOLFOS_HEALTH 4
#define DINOLFOS_FIRE_START_INDEX 6
#define DINOLFOS_COLLIDER_COUNT 9
#define DINOLFOS_FIRE_FRAMES 20
#define DINOLFOS_HEAD_SWEEP 0x2C00

typedef enum {
    DINOLFOS_LIMB_NONE,
    DINOLFOS_LIMB_LOWER_BODY,
    DINOLFOS_LIMB_LEFT_UPPER_LEG,
    DINOLFOS_LIMB_LEFT_LOWER_LEG,
    DINOLFOS_LIMB_LEFT_CLAWS,
    DINOLFOS_LIMB_RIGHT_UPPER_LEG,
    DINOLFOS_LIMB_RIGHT_LOWER_LEG,
    DINOLFOS_LIMB_RIGHT_CLAWS,
    DINOLFOS_LIMB_UPPER_BODY,
    DINOLFOS_LIMB_LEFT_UPPER_ARM,
    DINOLFOS_LIMB_LEFT_FOREARM,
    DINOLFOS_LIMB_LEFT_HAND,
    DINOLFOS_LIMB_RIGHT_UPPER_ARM,
    DINOLFOS_LIMB_RIGHT_FOREARM,
    DINOLFOS_LIMB_RIGHT_HAND,
    DINOLFOS_LIMB_NECK,
    DINOLFOS_LIMB_HEAD,
    DINOLFOS_LIMB_JAW,
    DINOLFOS_LIMB_UPPER_TAIL,
    DINOLFOS_LIMB_LOWER_TAIL,
    DINOLFOS_LIMB_KNIFE,
    DINOLFOS_LIMB_MAX
} DinolfosLimb;

typedef enum {
    DINOLFOS_BODYPART_NONE = -1,
    DINOLFOS_BODYPART_LEFT_UPPER_LEG,
    DINOLFOS_BODYPART_LEFT_CLAWS,
    DINOLFOS_BODYPART_RIGHT_UPPER_LEG,
    DINOLFOS_BODYPART_RIGHT_CLAWS,
    DINOLFOS_BODYPART_UPPER_BODY,
    DINOLFOS_BODYPART_LEFT_FOREARM,
    DINOLFOS_BODYPART_LEFT_HAND,
    DINOLFOS_BODYPART_RIGHT_FOREARM,
    DINOLFOS_BODYPART_RIGHT_HAND,
    DINOLFOS_BODYPART_HEAD,
    DINOLFOS_BODYPART_JAW,
    DINOLFOS_BODYPART_LOWER_TAIL,
    DINOLFOS_BODYPART_MAX
} DinolfosBodyPart;

typedef enum {
    DINOLFOS_JUMP_FORWARD,
    DINOLFOS_JUMP_IN_PLACE,
    DINOLFOS_JUMP_BACKWARD,
    DINOLFOS_JUMP_SLASH
} DinolfosJump;

typedef enum {
    DINOLFOS_DMGEFF_NONE,
    DINOLFOS_DMGEFF_STUN,
    DINOLFOS_DMGEFF_FIRE,
    DINOLFOS_DMGEFF_ICE,
    DINOLFOS_DMGEFF_LIGHT,
    DINOLFOS_DMGEFF_IMMUNE = 0xF
} DinolfosDamageEffect;

struct EnDinolfos;

typedef void (*EnDinolfosActionFunc)(struct EnDinolfos*, PlayState*);

typedef struct EnDinolfos {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[DINOLFOS_LIMB_MAX];
    Vec3s morphTable[DINOLFOS_LIMB_MAX];
    EnDinolfosActionFunc actionFunc;
    u8 envColorAlpha;
    u8 eyeIndex;
    u8 jumpType;
    bool isFrozen;
    bool isJumpingBackward;
    s16 targetRotY;
    s16 headRotY;
    s16 timer;
    s16 stepTimer;
    s16 lastKnifeFrame;
    s32 blureIndex;
    f32 circlingRate;
    Vec3f bodyPartsPos[DINOLFOS_BODYPART_MAX];
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[DINOLFOS_COLLIDER_COUNT];
    ColliderQuad knifeCollider;
} EnDinolfos;

static const ALIGN_ASSET(2) char sDinolfosSkel[] = OBJ_DINOLFOS "gDinolfosSkel";
static const ALIGN_ASSET(2) char sDinolfosIdleAnim[] = OBJ_DINOLFOS "gDinolfosIdleAnim";
static const ALIGN_ASSET(2) char sDinolfosWalkAnim[] = OBJ_DINOLFOS "gDinolfosWalkAnim";
static const ALIGN_ASSET(2) char sDinolfosSidestepAnim[] = OBJ_DINOLFOS "gDinolfosSidestepAnim";
static const ALIGN_ASSET(2) char sDinolfosJumpAnim[] = OBJ_DINOLFOS "gDinolfosJumpAnim";
static const ALIGN_ASSET(2) char sDinolfosLandAnim[] = OBJ_DINOLFOS "gDinolfosLandAnim";
static const ALIGN_ASSET(2) char sDinolfosAttackAnim[] = OBJ_DINOLFOS "gDinolfosAttackAnim";
static const ALIGN_ASSET(2) char sDinolfosHitAnim[] = OBJ_DINOLFOS "gDinolfosHitAnim";
static const ALIGN_ASSET(2) char sDinolfosFireStartAnim[] = OBJ_DINOLFOS "gDinolfosFireStartAnim";
static const ALIGN_ASSET(2) char sDinolfosFireLoopAnim[] = OBJ_DINOLFOS "gDinolfosFireLoopAnim";
static const ALIGN_ASSET(2) char sDinolfosFireEndAnim[] = OBJ_DINOLFOS "gDinolfosFireEndAnim";
static const ALIGN_ASSET(2) char sDinolfosDieAnim[] = OBJ_DINOLFOS "gDinolfosDieAnim";
static const ALIGN_ASSET(2) char sDinolfosEyeOpenTex[] = OBJ_DINOLFOS "gDinolfosEyeOpenTex";
static const ALIGN_ASSET(2) char sDinolfosEyeHalfTex[] = OBJ_DINOLFOS "gDinolfosEyeHalfTex";
static const ALIGN_ASSET(2) char sDinolfosEyeClosedTex[] = OBJ_DINOLFOS "gDinolfosEyeClosedTex";

static const char* const sEyeTextures[] = {
    sDinolfosEyeOpenTex,
    sDinolfosEyeHalfTex,
    sDinolfosEyeClosedTex,
    sDinolfosEyeHalfTex,
};

#define DINOLFOS_BODY_SPHERE(limb, x, y, z, radius)                              \
    {                                                                            \
        { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, \
          TOUCH_NONE,    BUMP_ON | BUMP_HOOKABLE,    OCELEM_ON },                \
        {                                                                        \
            limb, { { x, y, z }, radius }, 100                                   \
        }                                                                        \
    }

#define DINOLFOS_FIRE_SPHERE(z, y, radius)                    \
    {                                                         \
        { ELEMTYPE_UNK0,                                      \
          { DMG_UNBLOCKABLE, HIT_SPECIAL_EFFECT_FIRE, 0x08 }, \
          { 0x00000000, 0x00, 0x00 },                         \
          TOUCH_ON,                                           \
          BUMP_NONE,                                          \
          OCELEM_NONE },                                      \
        {                                                     \
            DINOLFOS_LIMB_MAX, { { 0, y, z }, radius }, 100   \
        }                                                     \
    }

static ColliderJntSphElementInit sElementsInit[DINOLFOS_COLLIDER_COUNT] = {
    DINOLFOS_BODY_SPHERE(DINOLFOS_LIMB_LOWER_BODY, 200, 300, 0, 19),
    DINOLFOS_BODY_SPHERE(DINOLFOS_LIMB_UPPER_BODY, 200, 200, 0, 17),
    DINOLFOS_BODY_SPHERE(DINOLFOS_LIMB_HEAD, 600, 200, 0, 15),
    DINOLFOS_BODY_SPHERE(DINOLFOS_LIMB_NECK, 700, 100, 0, 10),
    DINOLFOS_BODY_SPHERE(DINOLFOS_LIMB_LEFT_UPPER_LEG, 1300, 100, 0, 12),
    DINOLFOS_BODY_SPHERE(DINOLFOS_LIMB_RIGHT_UPPER_LEG, 1300, 100, 0, 12),
    DINOLFOS_FIRE_SPHERE(35, -10, 20),
    DINOLFOS_FIRE_SPHERE(70, -10, 28),
    DINOLFOS_FIRE_SPHERE(110, -5, 30),
};

static ColliderJntSphInit sColliderInit = {
    { COLTYPE_HIT0, AT_NONE | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    DINOLFOS_COLLIDER_COUNT,
    sElementsInit,
};

static ColliderQuadInit sKnifeInit = {
    { COLTYPE_NONE, AT_NONE | AT_TYPE_ENEMY, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_QUAD },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x04 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL | TOUCH_UNK7,
      BUMP_NONE,
      OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

// MM entries multiply the weapon's own damage; OoT's are absolute: MM's multiplier times OoT's usual weapon damage.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, DINOLFOS_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, DINOLFOS_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, DINOLFOS_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, DINOLFOS_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(1, DINOLFOS_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, DINOLFOS_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, DINOLFOS_DMGEFF_NONE),
    /* Hookshot      */ DMG_ENTRY(0, DINOLFOS_DMGEFF_IMMUNE),
    /* Kokiri sword  */ DMG_ENTRY(1, DINOLFOS_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, DINOLFOS_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, DINOLFOS_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(2, DINOLFOS_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(4, DINOLFOS_DMGEFF_ICE),
    /* Light arrow   */ DMG_ENTRY(4, DINOLFOS_DMGEFF_LIGHT),
    /* Unk arrow 1   */ DMG_ENTRY(2, DINOLFOS_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, DINOLFOS_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, DINOLFOS_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(2, DINOLFOS_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(4, DINOLFOS_DMGEFF_ICE),
    /* Light magic   */ DMG_ENTRY(4, DINOLFOS_DMGEFF_LIGHT),
    /* Shield        */ DMG_ENTRY(0, DINOLFOS_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, DINOLFOS_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(2, DINOLFOS_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(8, DINOLFOS_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(4, DINOLFOS_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, DINOLFOS_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, DINOLFOS_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, DINOLFOS_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, DINOLFOS_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, DINOLFOS_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, DINOLFOS_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, DINOLFOS_DMGEFF_NONE),
};

// MM ignores these while frozen: only a strong blow shatters the ice.
#define DINOLFOS_ICE_PROOF_DMG                                                                                         \
    (DMG_DEKU_NUT | DMG_DEKU_STICK | DMG_SLINGSHOT | DMG_BOOMERANG | DMG_ARROW_NORMAL | DMG_HOOKSHOT | DMG_ARROW_ICE | \
     DMG_ARROW_LIGHT | DMG_MAGIC_ICE)
#define DMG_KNOCKS_ALONG_SHOT (DMG_ARROW_NORMAL | DMG_ARROW_FIRE | DMG_ARROW_ICE | DMG_ARROW_LIGHT | DMG_SLINGSHOT)

static CollisionCheckInfoInit sColChkInfoInit = { DINOLFOS_HEALTH, 40, 100, 80 };

static const s8 sLimbToBodyPart[DINOLFOS_LIMB_MAX] = {
    [DINOLFOS_LIMB_NONE] = DINOLFOS_BODYPART_NONE,
    [DINOLFOS_LIMB_LOWER_BODY] = DINOLFOS_BODYPART_NONE,
    [DINOLFOS_LIMB_LEFT_UPPER_LEG] = DINOLFOS_BODYPART_LEFT_UPPER_LEG,
    [DINOLFOS_LIMB_LEFT_LOWER_LEG] = DINOLFOS_BODYPART_NONE,
    [DINOLFOS_LIMB_LEFT_CLAWS] = DINOLFOS_BODYPART_LEFT_CLAWS,
    [DINOLFOS_LIMB_RIGHT_UPPER_LEG] = DINOLFOS_BODYPART_RIGHT_UPPER_LEG,
    [DINOLFOS_LIMB_RIGHT_LOWER_LEG] = DINOLFOS_BODYPART_NONE,
    [DINOLFOS_LIMB_RIGHT_CLAWS] = DINOLFOS_BODYPART_RIGHT_CLAWS,
    [DINOLFOS_LIMB_UPPER_BODY] = DINOLFOS_BODYPART_UPPER_BODY,
    [DINOLFOS_LIMB_LEFT_UPPER_ARM] = DINOLFOS_BODYPART_NONE,
    [DINOLFOS_LIMB_LEFT_FOREARM] = DINOLFOS_BODYPART_LEFT_FOREARM,
    [DINOLFOS_LIMB_LEFT_HAND] = DINOLFOS_BODYPART_LEFT_HAND,
    [DINOLFOS_LIMB_RIGHT_UPPER_ARM] = DINOLFOS_BODYPART_NONE,
    [DINOLFOS_LIMB_RIGHT_FOREARM] = DINOLFOS_BODYPART_RIGHT_FOREARM,
    [DINOLFOS_LIMB_RIGHT_HAND] = DINOLFOS_BODYPART_RIGHT_HAND,
    [DINOLFOS_LIMB_NECK] = DINOLFOS_BODYPART_NONE,
    [DINOLFOS_LIMB_HEAD] = DINOLFOS_BODYPART_HEAD,
    [DINOLFOS_LIMB_JAW] = DINOLFOS_BODYPART_JAW,
    [DINOLFOS_LIMB_UPPER_TAIL] = DINOLFOS_BODYPART_NONE,
    [DINOLFOS_LIMB_LOWER_TAIL] = DINOLFOS_BODYPART_LOWER_TAIL,
    [DINOLFOS_LIMB_KNIFE] = DINOLFOS_BODYPART_NONE,
};

static Vec3f sKnifeTipOffset = { 400.0f, -3600.0f, 0.0f };
static Vec3f sKnifeBaseOffset = { 300.0f, 500.0f, 0.0f };
static Vec3f sClawOffset = { 300.0f, 0.0f, 0.0f };
static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

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

static void Dinolfos_Idle(EnDinolfos* this, PlayState* play);
static void Dinolfos_Walk(EnDinolfos* this, PlayState* play);
static void Dinolfos_TurnToPlayer(EnDinolfos* this, PlayState* play);
static void Dinolfos_CircleAroundPlayer(EnDinolfos* this, PlayState* play);
static void Dinolfos_ChooseJump(EnDinolfos* this, PlayState* play);
static void Dinolfos_Jump(EnDinolfos* this, PlayState* play);
static void Dinolfos_JumpSlash(EnDinolfos* this, PlayState* play);
static void Dinolfos_Land(EnDinolfos* this, PlayState* play);
static void Dinolfos_Slash(EnDinolfos* this, PlayState* play);
static void Dinolfos_Recoil(EnDinolfos* this, PlayState* play);
static void Dinolfos_Stunned(EnDinolfos* this, PlayState* play);
static void Dinolfos_Damaged(EnDinolfos* this, PlayState* play);
static void Dinolfos_StartBreatheFire(EnDinolfos* this, PlayState* play);
static void Dinolfos_BreatheFire(EnDinolfos* this, PlayState* play);
static void Dinolfos_EndBreatheFire(EnDinolfos* this, PlayState* play);
static void Dinolfos_Die(EnDinolfos* this, PlayState* play);
static void Dinolfos_DodgeProjectile(EnDinolfos* this, PlayState* play);
static void Dinolfos_SetupIdle(EnDinolfos* this);
static void Dinolfos_SetupWalk(EnDinolfos* this, PlayState* play);
static void Dinolfos_SetupTurnToPlayer(EnDinolfos* this);
static void Dinolfos_SetupCircleAroundPlayer(EnDinolfos* this, PlayState* play);
static void Dinolfos_SetupChooseJump(EnDinolfos* this, DinolfosJump jumpType);
static void Dinolfos_SetupSlash(EnDinolfos* this);
static void Dinolfos_SetupStartBreatheFire(EnDinolfos* this);
static void Dinolfos_SetupDie(EnDinolfos* this);
static void Dinolfos_SetupLand(EnDinolfos* this);

static void Dinolfos_Blink(EnDinolfos* this) {
    if (this->eyeIndex != 0) {
        this->eyeIndex = (this->eyeIndex + 1) % ARRAY_COUNT(sEyeTextures);
    } else if (Rand_ZeroOne() < 0.05f) {
        this->eyeIndex = 1;
    }
}

static bool Dinolfos_IsFacingPlayer(EnDinolfos* this) {
    s16 angleToPlayer = this->actor.yawTowardsPlayer - this->headRotY - this->actor.shape.rot.y;

    return ABS(angleToPlayer) < 0x3000;
}

static bool Dinolfos_IsSlashChosen(EnDinolfos* this, PlayState* play) {
    if (Actor_OtherIsTargeted(play, &this->actor)) {
        return false;
    }
    bool isSlashing = this->actionFunc == Dinolfos_Slash;

    return isSlashing ? Rand_ZeroOne() > 0.8f : Rand_ZeroOne() > 0.35f;
}

static bool Dinolfos_IsLeapChosen(EnDinolfos* this) {
    bool hasLanded = this->actionFunc == Dinolfos_Land;

    return hasLanded ? Rand_ZeroOne() < 0.05f : Rand_ZeroOne() < 0.1f;
}

static void Dinolfos_ChooseAction(EnDinolfos* this, PlayState* play) {
    if (!Dinolfos_IsFacingPlayer(this)) {
        if (Rand_ZeroOne() < 0.6f) {
            Dinolfos_SetupTurnToPlayer(this);
        } else {
            Dinolfos_SetupIdle(this);
        }
        return;
    }
    f32 distance = this->actor.xzDistToPlayer;

    if (distance < 100.0f) {
        if (Dinolfos_IsSlashChosen(this, play)) {
            Dinolfos_SetupSlash(this);
        } else {
            Dinolfos_SetupCircleAroundPlayer(this, play);
        }
    } else if (distance < 260.0f && distance > 180.0f) {
        if (Dinolfos_IsLeapChosen(this)) {
            Dinolfos_SetupChooseJump(this, DINOLFOS_JUMP_FORWARD);
        } else {
            Dinolfos_SetupCircleAroundPlayer(this, play);
        }
    } else if (Rand_ZeroOne() < 0.8f) {
        Dinolfos_SetupWalk(this, play);
    } else {
        Dinolfos_SetupCircleAroundPlayer(this, play);
    }
}

static void Dinolfos_Freeze(EnDinolfos* this) {
    this->isFrozen = true;
    this->collider.base.colType = COLTYPE_HIT3;
    this->timer = 80;
    this->actor.flags &= ~ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 80);
}

static void Dinolfos_ThawIfFrozen(EnDinolfos* this, PlayState* play) {
    if (!this->isFrozen) {
        return;
    }
    this->isFrozen = false;
    this->collider.base.colType = COLTYPE_HIT0;
    this->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    PlaySfx(&this->actor, MM_SE_EV_ICE_BROKEN, NA_SE_EV_ICE_BROKEN);
    for (s32 i = 0; i < DINOLFOS_BODYPART_MAX; i++) {
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &this->bodyPartsPos[i], 150, 150, 150, 250, 235, 245, 255,
                                       1.0f);
    }
}

static void Dinolfos_SetupDodgeProjectile(EnDinolfos* this, s16 angleToProjectile) {
    AnimationHeader* sidestep = (AnimationHeader*)sDinolfosSidestepAnim;

    this->actor.speedXZ = angleToProjectile >= 0 ? -15.0f : 15.0f;
    Animation_Change(&this->skelAnime, sidestep, this->actor.speedXZ * (1.0f / 7.5f), 0.0f, 0.0f, ANIMMODE_LOOP, -4.0f);
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x4000;
    this->stepTimer = 10;
    this->circlingRate = 0.0f;
    this->actionFunc = Dinolfos_DodgeProjectile;
}

static s16 Dinolfos_GetAngleToActor(EnDinolfos* this, Actor* actor) {
    return Actor_WorldYawTowardActor(&this->actor, actor) - this->actor.shape.rot.y - this->headRotY;
}

static bool Dinolfos_IsPlayerSwingingAtIt(EnDinolfos* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    return this->actor.xzDistToPlayer < 100.0f && player->meleeWeaponState != 0 && this->actor.isTargeted &&
           Rand_ZeroOne() < 0.5f && Dinolfos_IsFacingPlayer(this) && Player_IsFacingActor(&this->actor, 0x2000, play);
}

// Sidesteps shots, hops over bombchus and hammer shockwaves, and backflips or leaps over a sword swing.
static bool Dinolfos_Dodge(EnDinolfos* this, PlayState* play) {
    Actor* projectile = Actor_GetProjectileActor(play, &this->actor, 80.0f);

    if (projectile != NULL) {
        s16 angle = Dinolfos_GetAngleToActor(this, projectile);

        if (ABS(angle) < 0x3000) {
            Dinolfos_SetupDodgeProjectile(this, angle + this->headRotY);
            PlaySfx(&this->actor, MM_SE_EN_RIZA_WARAU, NA_SE_EN_RIZA_WARAU);
            return true;
        }
    }
    Actor* bombchu = Actor_FindNearby(play, &this->actor, ACTOR_EN_BOM_CHU, ACTORCAT_EXPLOSIVE, 80.0f);

    if (bombchu != NULL && ABS(Dinolfos_GetAngleToActor(this, bombchu)) < 0x3000) {
        Dinolfos_SetupChooseJump(this, DINOLFOS_JUMP_IN_PLACE);
        PlaySfx(&this->actor, MM_SE_EN_RIZA_WARAU, NA_SE_EN_RIZA_WARAU);
        return true;
    }
    if (play->actorCtx.unk_02 != 0) {
        Dinolfos_SetupChooseJump(this, DINOLFOS_JUMP_IN_PLACE);
        return true;
    }
    if (!Dinolfos_IsPlayerSwingingAtIt(this, play)) {
        return false;
    }
    Dinolfos_SetupChooseJump(this, Rand_ZeroOne() < 0.5f ? DINOLFOS_JUMP_BACKWARD : DINOLFOS_JUMP_SLASH);
    PlaySfx(&this->actor, MM_SE_EN_RIZA_WARAU, NA_SE_EN_RIZA_WARAU);
    return true;
}

static void Dinolfos_SetupIdle(EnDinolfos* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sDinolfosIdleAnim, -4.0f);
    this->timer = (s32)Rand_ZeroFloat(20.0f) + 40;
    this->stepTimer = 30;
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Dinolfos_Idle;
}

static void Dinolfos_Idle(EnDinolfos* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->stepTimer != 0) {
        this->stepTimer--;
    } else if (Dinolfos_IsFacingPlayer(this)) {
        this->timer = 0;
    }
    if (this->timer != 0) {
        this->timer--;
    } else {
        Dinolfos_ChooseAction(this, play);
    }
}

static f32 Dinolfos_GetKeepAwayDistance(EnDinolfos* this, PlayState* play) {
    return Actor_OtherIsTargeted(play, &this->actor) ? 170.0f : 70.0f;
}

static void Dinolfos_SetupWalk(EnDinolfos* this, PlayState* play) {
    if (this->actionFunc != Dinolfos_Walk) {
        Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sDinolfosWalkAnim, -4.0f);
        bool isTooClose = this->actor.xzDistToPlayer <= Dinolfos_GetKeepAwayDistance(this, play);

        this->actor.speedXZ = isTooClose ? -3.5f : 3.5f;
    }
    this->stepTimer = (s32)Rand_ZeroFloat(10.0f) + 20;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Dinolfos_Walk;
}

static void Dinolfos_Walk(EnDinolfos* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Dinolfos_Dodge(this, play)) {
        return;
    }
    bool isTooClose = this->actor.xzDistToPlayer <= Dinolfos_GetKeepAwayDistance(this, play);

    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0x100);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    Math_StepToF(&this->actor.speedXZ, isTooClose ? -7.0f : 7.0f, 0.5f);
    if (this->actor.xzDistToPlayer < 80.0f) {
        this->stepTimer = 0;
    }
    if (this->stepTimer != 0) {
        this->stepTimer--;
    } else {
        Dinolfos_ChooseAction(this, play);
    }
    if (Animation_OnFrame(&this->skelAnime, 0.0f) || Animation_OnFrame(&this->skelAnime, 6.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_RIZA_WALK, NA_SE_EN_RIZA_WALK);
    }
}

static void Dinolfos_SetupTurnToPlayer(EnDinolfos* this) {
    if (this->actionFunc != Dinolfos_TurnToPlayer) {
        Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sDinolfosWalkAnim, -4.0f);
        this->actor.speedXZ = 0.0f;
    }
    s16 offPlayer = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;

    this->targetRotY = this->actor.shape.rot.y + (offPlayer > 0 ? 0x4000 : -0x4000);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Dinolfos_TurnToPlayer;
}

static void Dinolfos_TurnToPlayer(EnDinolfos* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Math_ScaledStepToS(&this->actor.shape.rot.y, this->targetRotY, 0x400)) {
        Dinolfos_ChooseAction(this, play);
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
}

static f32 Dinolfos_PickCircleSpeed(EnDinolfos* this, PlayState* play) {
    s16 offPlayerFacing = GET_PLAYER(play)->actor.shape.rot.y - this->actor.shape.rot.y;

    if (ABS(offPlayerFacing) > 0x7800) {
        return Rand_ZeroOne() < 0.5f ? 6.0f : -6.0f;
    }
    return offPlayerFacing >= 0 ? 6.0f : -6.0f;
}

static void Dinolfos_SetupCircleAroundPlayer(EnDinolfos* this, PlayState* play) {
    if (this->actionFunc != Dinolfos_CircleAroundPlayer) {
        this->actor.speedXZ = Dinolfos_PickCircleSpeed(this, play);
        if (this->actionFunc == Dinolfos_DodgeProjectile) {
            this->skelAnime.playSpeed = this->actor.speedXZ * (1.0f / 6.0f);
        } else {
            Animation_Change(&this->skelAnime, (AnimationHeader*)sDinolfosSidestepAnim,
                             this->actor.speedXZ * (1.0f / 6.0f), 0.0f, 0.0f, ANIMMODE_LOOP, -4.0f);
        }
        this->actor.world.rot.y = this->actor.shape.rot.y + 0x4000;
        this->circlingRate = 0.0f;
        this->stepTimer = (s32)Rand_ZeroFloat(10.0f) + 5;
    }
    this->actionFunc = Dinolfos_CircleAroundPlayer;
}

static void Dinolfos_BounceOffWall(EnDinolfos* this) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
        return;
    }
    s16 sideYaw = this->actor.shape.rot.y + (this->actor.speedXZ >= 0.0f ? 0x4000 : -0x4000);
    s16 offWall = this->actor.wallYaw - sideYaw;

    if (ABS(offWall) > 0x4000) {
        this->actor.speedXZ *= -0.8f;
        this->actor.speedXZ += this->actor.speedXZ < 0.0f ? -0.5f : 0.5f;
    }
}

static void Dinolfos_CircleAroundPlayer(EnDinolfos* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xBB8);
    if (Dinolfos_Dodge(this, play)) {
        return;
    }
    Dinolfos_BounceOffWall(this);

    s16 offPlayerFacing = player->actor.shape.rot.y - this->actor.shape.rot.y;

    if (offPlayerFacing >= 0 && offPlayerFacing < 0x7800) {
        this->actor.speedXZ += 0.125f;
    } else if (offPlayerFacing < 0 && offPlayerFacing > -0x7800) {
        this->actor.speedXZ -= 0.125f;
    }
    this->skelAnime.playSpeed = this->actor.speedXZ > 0.0f ? 1.0f : -1.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x4000;

    f32 extraDistance = Actor_OtherIsTargeted(play, &this->actor) ? 100.0f : 0.0f;

    if (this->actor.xzDistToPlayer <= 70.0f + extraDistance) {
        Math_StepToF(&this->circlingRate, -4.0f, 1.5f);
    } else if (this->actor.xzDistToPlayer > 90.0f + extraDistance) {
        Math_StepToF(&this->circlingRate, 4.0f, 1.5f);
    } else {
        Math_StepToF(&this->circlingRate, 0.0f, 5.65f);
    }
    this->actor.world.pos.x += Math_SinS(this->actor.shape.rot.y) * this->circlingRate;
    this->actor.world.pos.z += Math_CosS(this->actor.shape.rot.y) * this->circlingRate;
    SkelAnime_Update(&this->skelAnime);
    if (this->stepTimer != 0) {
        this->stepTimer--;
    } else {
        Dinolfos_ChooseAction(this, play);
    }
    if (Animation_OnFrame(&this->skelAnime, 0.0f) || Animation_OnFrame(&this->skelAnime, 5.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_RIZA_WALK, NA_SE_EN_RIZA_WALK);
    }
}

static void Dinolfos_SetupChooseJump(EnDinolfos* this, DinolfosJump jumpType) {
    bool isEvasive = jumpType == DINOLFOS_JUMP_BACKWARD || jumpType == DINOLFOS_JUMP_SLASH;

    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sDinolfosJumpAnim,
                              jumpType == DINOLFOS_JUMP_FORWARD ? 2.0f : -1.0f);
    if (isEvasive) {
        this->collider.base.acFlags &= ~AC_ON;
    }
    this->actor.speedXZ = 0.0f;
    this->jumpType = jumpType;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Dinolfos_ChooseJump;
}

static void Dinolfos_SetupJump(EnDinolfos* this) {
    if (this->jumpType == DINOLFOS_JUMP_BACKWARD) {
        this->actor.speedXZ = -10.0f;
        this->actor.velocity.y = 9.0f;
        this->collider.base.acFlags |= AC_ON;
    } else {
        this->actor.velocity.y = 12.5f;
        if (this->jumpType == DINOLFOS_JUMP_FORWARD) {
            this->actor.speedXZ = 4.5f;
        }
    }
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    PlaySfx(&this->actor, MM_SE_EN_RIZA_JUMP, NA_SE_EN_RIZA_JUMP);
    this->actionFunc = Dinolfos_Jump;
}

static void Dinolfos_SetupJumpSlash(EnDinolfos* this) {
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    this->actor.speedXZ = 8.0f;
    this->actor.velocity.y = 16.0f;
    PlaySfx(&this->actor, MM_SE_EN_RIZA_JUMP, NA_SE_EN_RIZA_JUMP);
    this->timer = 0;
    this->lastKnifeFrame = -1;
    this->actionFunc = Dinolfos_JumpSlash;
}

static void Dinolfos_ChooseJump(EnDinolfos* this, PlayState* play) {
    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0x1000);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    SkelAnime_Update(&this->skelAnime);
    if (!Animation_OnFrame(&this->skelAnime, 1.0f)) {
        return;
    }
    if (this->jumpType == DINOLFOS_JUMP_SLASH) {
        Dinolfos_SetupJumpSlash(this);
    } else {
        Dinolfos_SetupJump(this);
    }
}

static void Dinolfos_Jump(EnDinolfos* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Dinolfos_SetupLand(this);
    }
}

// The knife always swings once the leap over Link starts to fall.
static void Dinolfos_JumpSlash(EnDinolfos* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->timer++;
    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0x1800);
    if (this->actor.velocity.y < 0.0f && this->skelAnime.animation == sDinolfosJumpAnim) {
        this->knifeCollider.base.atFlags |= AT_ON;
        this->collider.base.acFlags |= AC_ON;
        Animation_Change(&this->skelAnime, (AnimationHeader*)sDinolfosAttackAnim, 1.0f, 7.0f, 13.0f, ANIMMODE_ONCE,
                         -2.0f);
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        this->knifeCollider.base.atFlags &= ~AT_ON;
        Dinolfos_SetupLand(this);
    }
}

static void Dinolfos_SetupLand(EnDinolfos* this) {
    if (this->actionFunc != Dinolfos_JumpSlash) {
        Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sDinolfosLandAnim);
    } else {
        this->skelAnime.endFrame = Animation_GetLastFrame((void*)sDinolfosAttackAnim);
    }
    this->isJumpingBackward = this->actor.speedXZ < 0.0f;
    this->actor.speedXZ = 0.0f;
    PlaySfx(&this->actor, MM_SE_EN_BOMCHU_WALK, NA_SE_EN_RIZA_ONGND);
    this->actionFunc = Dinolfos_Land;
}

static void Dinolfos_Land(EnDinolfos* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    bool isFireChosen = this->isJumpingBackward && this->actor.xzDistToPlayer < 280.0f &&
                        Dinolfos_IsFacingPlayer(this) && Rand_ZeroOne() < 0.6f;

    if (isFireChosen) {
        Dinolfos_SetupStartBreatheFire(this);
    } else {
        Dinolfos_ChooseAction(this, play);
    }
}

static void Dinolfos_SetupSlash(EnDinolfos* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sDinolfosAttackAnim);
    this->knifeCollider.base.atFlags &= ~AT_BOUNCED;
    PlaySfx(&this->actor, MM_SE_EN_RIZA_CRY, NA_SE_EN_RIZA_CRY);
    this->timer = 0;
    this->lastKnifeFrame = -1;
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Dinolfos_Slash;
}

static void Dinolfos_Slash(EnDinolfos* this, PlayState* play) {
    this->timer++;
    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0x800);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (SkelAnime_Update(&this->skelAnime)) {
        EffectBlure_AddSpace(Effect_GetByIndex(this->blureIndex));
        this->knifeCollider.base.atFlags &= ~AT_ON;
        Dinolfos_ChooseAction(this, play);
    } else if (Animation_OnFrame(&this->skelAnime, 7.0f)) {
        this->knifeCollider.base.atFlags |= AT_ON;
    } else if (Animation_OnFrame(&this->skelAnime, 13.0f)) {
        this->knifeCollider.base.atFlags &= ~AT_ON;
    } else if (Animation_OnFrame(&this->skelAnime, 11.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_RIZA_ATTACK, NA_SE_EN_RIZA_ATTACK);
    } else if (this->skelAnime.curFrame < 7.0f) {
        Dinolfos_Dodge(this, play);
    }
}

static void Dinolfos_SetupRecoil(EnDinolfos* this) {
    Animation_Change(&this->skelAnime, (AnimationHeader*)sDinolfosAttackAnim, -1.0f, this->skelAnime.curFrame, 0.0f,
                     ANIMMODE_ONCE, 0.0f);
    this->knifeCollider.base.atFlags &= ~(AT_ON | AT_BOUNCED);
    if (this->actionFunc == Dinolfos_JumpSlash) {
        this->actor.speedXZ = 3.0f;
    } else {
        this->actor.world.rot.y = this->actor.shape.rot.y;
    }
    this->actionFunc = Dinolfos_Recoil;
}

static void Dinolfos_Recoil(EnDinolfos* this, PlayState* play) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Math_StepToF(&this->actor.speedXZ, 0.0f, 0.5f);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        Dinolfos_ChooseAction(this, play);
    }
}

static void Dinolfos_SetupStunned(EnDinolfos* this) {
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = MIN(this->actor.velocity.y, 0.0f);
    this->actionFunc = Dinolfos_Stunned;
}

static void Dinolfos_Stunned(EnDinolfos* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer != 0) {
        return;
    }
    Dinolfos_ThawIfFrozen(this, play);
    if (this->actor.colChkInfo.health == 0) {
        Dinolfos_SetupDie(this);
    } else if (Dinolfos_IsFacingPlayer(this) && this->actor.xzDistToPlayer < 100.0f) {
        Dinolfos_SetupStartBreatheFire(this);
    } else {
        Dinolfos_ChooseAction(this, play);
    }
}

static void Dinolfos_FaceAwayFromAttacker(EnDinolfos* this, s32 elementIndex) {
    Actor* attacker = this->collider.base.ac;
    ColliderInfo* hitBy = this->collider.elements[elementIndex].info.acHitInfo;

    if (attacker == NULL) {
        return;
    }
    if (hitBy != NULL && (hitBy->toucher.dmgFlags & DMG_KNOCKS_ALONG_SHOT)) {
        this->actor.world.rot.y = attacker->shape.rot.y;
    } else {
        this->actor.world.rot.y = Actor_WorldYawTowardActor(attacker, &this->actor);
    }
}

static void Dinolfos_SetupDamaged(EnDinolfos* this, s32 elementIndex) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sDinolfosHitAnim);
    Dinolfos_FaceAwayFromAttacker(this, elementIndex);
    this->actor.shape.rot.y = BINANG_ROT180(this->actor.world.rot.y);
    this->actor.speedXZ = 10.0f;
    this->actor.velocity.y = MIN(this->actor.velocity.y, 0.0f);
    this->headRotY = 0;
    PlaySfx(&this->actor, MM_SE_EN_RIZA_DAMAGE, NA_SE_EN_RIZA_DAMAGE);
    this->collider.base.acFlags &= ~AC_ON;
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 18);
    this->actionFunc = Dinolfos_Damaged;
}

static void Dinolfos_Damaged(EnDinolfos* this, PlayState* play) {
    Math_StepToF(&this->actor.speedXZ, 0.0f, 0.5f);
    if (!SkelAnime_Update(&this->skelAnime) || !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return;
    }
    if (this->actor.colChkInfo.health == 0) {
        Dinolfos_SetupDie(this);
    } else if (this->actor.xzDistToPlayer < 280.0f) {
        Dinolfos_SetupStartBreatheFire(this);
    } else {
        this->collider.base.acFlags |= AC_ON;
        Dinolfos_ChooseAction(this, play);
    }
}

static void Dinolfos_SetupStartBreatheFire(EnDinolfos* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sDinolfosFireStartAnim, -5.0f);
    this->collider.base.acFlags |= AC_ON;
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Dinolfos_StartBreatheFire;
}

static void Dinolfos_SetupBreatheFire(EnDinolfos* this) {
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sDinolfosFireLoopAnim);
    this->timer = DINOLFOS_FIRE_FRAMES;
    this->actor.speedXZ = 0.0f;
    this->collider.base.atFlags |= AT_ON;
    for (s32 i = DINOLFOS_FIRE_START_INDEX; i < DINOLFOS_COLLIDER_COUNT; i++) {
        this->collider.elements[i].dim.worldSphere.center = this->bodyPartsPos[DINOLFOS_BODYPART_JAW];
    }
    this->actionFunc = Dinolfos_BreatheFire;
}

static void Dinolfos_StartBreatheFire(EnDinolfos* this, PlayState* play) {
    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0x800);
    Math_StepToF(&this->actor.speedXZ, 0.0f, 0.5f);
    if (SkelAnime_Update(&this->skelAnime)) {
        Dinolfos_SetupBreatheFire(this);
    } else if (!Dinolfos_Dodge(this, play) && Animation_OnFrame(&this->skelAnime, 12.0f)) {
        this->actor.speedXZ = 8.0f;
    }
}

static void Dinolfos_SpawnFireBreath(EnDinolfos* this, PlayState* play) {
    s16 fireYaw = (s32)Rand_CenteredFloat(0x1000) + this->actor.shape.rot.y + this->headRotY;
    f32 sin = Math_SinS(fireYaw);
    f32 cos = Math_CosS(fireYaw);
    s32 dimSteps = MAX(10 - this->timer, 0);
    Vec3f velocity = { 11.0f * sin, Rand_CenteredFloat(2.0f) - 5.4f, 11.0f * cos };
    Vec3f accel = { 0.9f * sin, Rand_CenteredFloat(0.6f) + 1.4f, 0.9f * cos };

    PlaySfx(&this->actor, MM_SE_EN_DODO_J_FIRE - SFX_FLAG, NA_SE_EN_DODO_J_FIRE - SFX_FLAG);
    EffectSsDFire_Spawn(play, &this->bodyPartsPos[DINOLFOS_BODYPART_JAW], &velocity, &accel, 30, 22,
                        255 - dimSteps * 20, 3, 8);
}

// The fire balls leave the jaw one after the other and sweep in front of the Dinolfos.
static void Dinolfos_SweepFireColliders(EnDinolfos* this) {
    s32 end = DINOLFOS_FIRE_START_INDEX;
    Vec3f* jaw = &this->bodyPartsPos[DINOLFOS_BODYPART_JAW];

    for (s32 i = DINOLFOS_COLLIDER_COUNT - DINOLFOS_FIRE_START_INDEX; i > 0; i--) {
        if (this->timer < DINOLFOS_FIRE_FRAMES - i * 2) {
            end = i + DINOLFOS_FIRE_START_INDEX;
            break;
        }
    }
    for (s32 i = DINOLFOS_FIRE_START_INDEX; i < end; i++) {
        ColliderJntSphElementDim* dim = &this->collider.elements[i].dim;
        s16 sweepYaw = (s32)(Math_CosF((this->timer + ((i - 5) << 1)) * (M_PI / 20)) * DINOLFOS_HEAD_SWEEP) +
                       this->actor.shape.rot.y;

        dim->worldSphere.center.x = jaw->x + Math_SinS(sweepYaw) * dim->modelSphere.center.z;
        dim->worldSphere.center.y = jaw->y + dim->modelSphere.center.y;
        dim->worldSphere.center.z = jaw->z + Math_CosS(sweepYaw) * dim->modelSphere.center.z;
    }
}

static void Dinolfos_SetupEndBreatheFire(EnDinolfos* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sDinolfosFireEndAnim);
    this->collider.base.atFlags &= ~AT_ON;
    this->actionFunc = Dinolfos_EndBreatheFire;
}

static void Dinolfos_BreatheFire(EnDinolfos* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0x800);
    Dinolfos_SpawnFireBreath(this, play);
    Dinolfos_SweepFireColliders(this);
    if (this->timer != 0) {
        this->timer--;
    } else {
        Dinolfos_SetupEndBreatheFire(this);
    }
}

static void Dinolfos_EndBreatheFire(EnDinolfos* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        Dinolfos_ChooseAction(this, play);
    }
}

static void Dinolfos_SetupDie(EnDinolfos* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sDinolfosDieAnim);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    PlaySfx(&this->actor, MM_SE_EN_RIZA_DEAD, NA_SE_EN_RIZA_DEAD);
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = Dinolfos_Die;
}

// Stops counting as an enemy once the death animation ends, so the room clears while it fades.
static void Dinolfos_Die(EnDinolfos* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime)) {
        if (Animation_OnFrame(&this->skelAnime, 26.0f)) {
            PlaySfx(&this->actor, MM_SE_EN_GERUDOFT_DOWN, NA_SE_EN_RIZA_DOWN);
        }
        return;
    }
    if (this->actor.category == ACTORCAT_ENEMY) {
        Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_PROP);
        Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xE0);
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    }
    s32 alpha = this->envColorAlpha - 10;

    if (alpha <= 0) {
        this->envColorAlpha = 0;
        Actor_Kill(&this->actor);
    } else {
        this->envColorAlpha = alpha;
    }
    this->actor.shape.shadowAlpha = this->envColorAlpha;
}

static void Dinolfos_DodgeProjectile(EnDinolfos* this, PlayState* play) {
    f32 direction = this->actor.speedXZ >= 0.0f ? 1.0f : -1.0f;

    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xBB8);
    Math_StepToF(&this->actor.speedXZ, 0.0f, 2.0f);
    this->skelAnime.playSpeed = (1.0f + fabsf(this->actor.speedXZ * (1.0f / 15.0f))) * direction;
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x4000;
    SkelAnime_Update(&this->skelAnime);
    if (this->stepTimer != 0) {
        this->stepTimer--;
    } else {
        Dinolfos_ChooseAction(this, play);
    }
    if (Animation_OnFrame(&this->skelAnime, 0.0f) || Animation_OnFrame(&this->skelAnime, 5.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_RIZA_WALK, NA_SE_EN_RIZA_WALK);
    }
}

static void Dinolfos_RotateHead(EnDinolfos* this, PlayState* play) {
    if (this->actionFunc == Dinolfos_Idle && this->timer != 0) {
        Math_ScaledStepToS(&this->headRotY, Math_SinS(this->timer * 1400) * DINOLFOS_HEAD_SWEEP, 0x300);
        return;
    }
    if (this->actionFunc == Dinolfos_StartBreatheFire) {
        Math_ScaledStepToS(&this->headRotY, -DINOLFOS_HEAD_SWEEP, DINOLFOS_HEAD_SWEEP / 20);
        return;
    }
    if (this->actionFunc == Dinolfos_BreatheFire) {
        this->headRotY = Math_CosF(this->timer * (M_PI / 20)) * DINOLFOS_HEAD_SWEEP;
        return;
    }
    if (Play_InCsMode(play)) {
        return;
    }
    s16 offPlayer = this->actor.yawTowardsPlayer - (s16)(this->headRotY + this->actor.shape.rot.y);

    this->headRotY += CLAMP(offPlayer, -0x300, 0x300);
    this->headRotY = CLAMP(this->headRotY, -DINOLFOS_HEAD_SWEEP, DINOLFOS_HEAD_SWEEP);
}

static s32 Dinolfos_FindHitElement(EnDinolfos* this) {
    for (s32 i = 0; i < DINOLFOS_COLLIDER_COUNT; i++) {
        if (this->collider.elements[i].info.bumperFlags & BUMP_HIT) {
            return i;
        }
    }
    return -1;
}

static void Dinolfos_ReactToHit(EnDinolfos* this, s32 elementIndex) {
    u8 effect = this->actor.colChkInfo.damageEffect;

    if (effect == DINOLFOS_DMGEFF_ICE) {
        Dinolfos_Freeze(this);
        if (this->actor.colChkInfo.health == 0) {
            this->timer = 3;
            this->collider.base.acFlags &= ~AC_ON;
        }
        Dinolfos_SetupStunned(this);
        return;
    }
    if (effect == DINOLFOS_DMGEFF_STUN) {
        this->timer = 40;
        Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 255, COLORFILTER_OPA, 40);
        PlaySfx(&this->actor, MM_SE_EN_COMMON_FREEZE, NA_SE_EN_GOMA_JR_FREEZE);
        Dinolfos_SetupStunned(this);
        return;
    }
    Dinolfos_SetupDamaged(this, elementIndex);
}

static bool Dinolfos_UpdateDamage(EnDinolfos* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return false;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Actor_SetDropFlagJntSph(&this->actor, &this->collider, false);

    s32 hitIndex = Dinolfos_FindHitElement(this);

    if (hitIndex < 0) {
        return false;
    }
    ColliderInfo* hitBy = this->collider.elements[hitIndex].info.acHitInfo;
    bool isIceProof = this->isFrozen && hitBy != NULL && (hitBy->toucher.dmgFlags & DINOLFOS_ICE_PROOF_DMG);

    if (isIceProof || this->actor.colChkInfo.damageEffect == DINOLFOS_DMGEFF_IMMUNE) {
        return false;
    }
    if (Actor_ApplyDamage(&this->actor) == 0) {
        Enemy_StartFinishingBlow(play, &this->actor);
    }
    Dinolfos_ThawIfFrozen(this, play);
    this->knifeCollider.base.atFlags &= ~(AT_ON | AT_BOUNCED);
    this->collider.base.atFlags &= ~AT_ON;
    Dinolfos_ReactToHit(this, hitIndex);
    return true;
}

static void Dinolfos_SpawnWadeRipples(EnDinolfos* this, PlayState* play) {
    bool isWading = this->actor.yDistToWater > 0.0f && this->actor.yDistToWater < 10.0f;

    if (!isWading || ((play->gameplayFrames % 4) & 1) != 0) {
        return;
    }
    Vec3f pos = this->bodyPartsPos[DINOLFOS_BODYPART_LEFT_CLAWS + (play->gameplayFrames % 4)];

    pos.y = this->actor.world.pos.y + this->actor.yDistToWater;
    EffectSsGRipple_Spawn(play, &pos, 100, 320, 0);
}

static void Dinolfos_SubmitColliders(EnDinolfos* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    if (this->collider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->knifeCollider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->knifeCollider.base);
    }
}

static void EnDinolfos_Init(Actor* thisx, PlayState* play) {
    static EffectBlureInit2 sBlureInit = {
        .calcMode = 0,
        .flags = 8,
        .p1StartColor = { 255, 255, 255, 255 },
        .p2StartColor = { 255, 255, 255, 64 },
        .p1EndColor = { 255, 255, 255, 0 },
        .p2EndColor = { 255, 255, 255, 0 },
        .elemDuration = 8,
        .drawMode = 2,
    };
    EnDinolfos* this = (EnDinolfos*)thisx;

    this->actor.targetArrowOffset = 2000.0f;
    this->actor.gravity = -2.0f;
    Actor_SetScale(&this->actor, 0.015f);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawFeet, 90.0f);
    Effect_Add(play, &this->blureIndex, EFFECT_BLURE2, 0, 0, &sBlureInit);
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sColliderInit, this->colliderElements);
    Collider_InitQuad(play, &this->knifeCollider);
    Collider_SetQuad(play, &this->knifeCollider, &this->actor, &sKnifeInit);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sDinolfosSkel, (AnimationHeader*)sDinolfosIdleAnim,
                       this->jointTable, this->morphTable, DINOLFOS_LIMB_MAX);
    this->envColorAlpha = 255;
    for (s32 i = DINOLFOS_FIRE_START_INDEX; i < DINOLFOS_COLLIDER_COUNT; i++) {
        ColliderJntSphElementDim* dim = &this->collider.elements[i].dim;

        dim->worldSphere.radius = dim->modelSphere.radius;
    }
    Dinolfos_SetupIdle(this);
}

static void EnDinolfos_Destroy(Actor* thisx, PlayState* play) {
    EnDinolfos* this = (EnDinolfos*)thisx;

    Effect_Delete(play, this->blureIndex);
    Collider_DestroyJntSph(play, &this->collider);
    Collider_DestroyQuad(play, &this->knifeCollider);
}

static void EnDinolfos_Update(Actor* thisx, PlayState* play) {
    EnDinolfos* this = (EnDinolfos*)thisx;
    bool isStunned = this->actionFunc == Dinolfos_Stunned;

    if (!isStunned) {
        Dinolfos_Blink(this);
    }
    if (!Dinolfos_UpdateDamage(this, play) && (this->knifeCollider.base.atFlags & AT_BOUNCED)) {
        Dinolfos_SetupRecoil(this);
    }
    this->actionFunc(this, play);
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 25.0f, 30.0f, 60.0f, BGCHECK_UPDATE_WALL_FLOOR);
    if (!isStunned) {
        Dinolfos_SpawnWadeRipples(this, play);
        if (this->actionFunc != Dinolfos_Die) {
            Dinolfos_RotateHead(this, play);
        }
    }
    if (this->actionFunc == Dinolfos_Die || this->actionFunc == Dinolfos_Damaged) {
        Math_ScaledStepToS(&this->headRotY, 0, 2000);
    }
    this->actor.focus.pos = this->bodyPartsPos[DINOLFOS_BODYPART_UPPER_BODY];
    this->actor.focus.rot.y = this->actor.shape.rot.y + this->headRotY;
    Dinolfos_SubmitColliders(this, play);
}

static s32 Dinolfos_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx,
                                     Gfx** gfx) {
    EnDinolfos* this = (EnDinolfos*)thisx;

    if (limbIndex == DINOLFOS_LIMB_HEAD) {
        rot->y -= this->headRotY;
    }
    return false;
}

static bool Dinolfos_IsSlashing(EnDinolfos* this) {
    return this->actionFunc == Dinolfos_Slash || this->actionFunc == Dinolfos_JumpSlash;
}

// The knife quad trails from last frame's edge to this frame's, once per animation frame.
static void Dinolfos_UpdateKnife(EnDinolfos* this) {
    Vec3f previousBase = this->knifeCollider.dim.quad[0];
    Vec3f previousTip = this->knifeCollider.dim.quad[1];
    Vec3f tip;
    Vec3f base;

    Matrix_MultVec3f(&sKnifeTipOffset, &tip);
    Matrix_MultVec3f(&sKnifeBaseOffset, &base);
    Collider_SetQuadVertices(&this->knifeCollider, &base, &tip, &previousBase, &previousTip);
    if (this->knifeCollider.base.atFlags & AT_ON) {
        EffectBlure_AddVertex(Effect_GetByIndex(this->blureIndex), &tip, &base);
    }
    this->lastKnifeFrame = this->timer;
}

static void Dinolfos_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx) {
    EnDinolfos* this = (EnDinolfos*)thisx;
    s8 bodyPart = sLimbToBodyPart[limbIndex];

    Collider_UpdateSpheres(limbIndex, &this->collider);
    if (bodyPart != DINOLFOS_BODYPART_NONE) {
        Matrix_MultVec3f(&sZeroVec, &this->bodyPartsPos[bodyPart]);
    }
    if (limbIndex == DINOLFOS_LIMB_RIGHT_HAND && this->lastKnifeFrame != this->timer && Dinolfos_IsSlashing(this)) {
        Dinolfos_UpdateKnife(this);
    }
    if (limbIndex == DINOLFOS_LIMB_LEFT_CLAWS) {
        Matrix_MultVec3f(&sClawOffset, &this->actor.shape.feetPos[0]);
    } else if (limbIndex == DINOLFOS_LIMB_RIGHT_CLAWS) {
        Matrix_MultVec3f(&sClawOffset, &this->actor.shape.feetPos[1]);
    }
}

// MM's Scene_SetRenderModeXlu tables for segment 0x0C: object_dinofos calls entries 0 and 2.
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

static void EnDinolfos_Draw(Actor* thisx, PlayState* play) {
    EnDinolfos* this = (EnDinolfos*)thisx;
    const char* eyeTexture = sEyeTextures[this->eyeIndex];

    OPEN_DISPS(play->state.gfxCtx);
    if (this->envColorAlpha == 255) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)eyeTexture);
        gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
        gDPSetEnvColor(POLY_OPA_DISP++, 20, 40, 40, 255);
        POLY_OPA_DISP =
            SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                               Dinolfos_OverrideLimbDraw, Dinolfos_PostLimbDraw, this, POLY_OPA_DISP);
    } else {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)eyeTexture);
        gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)sRenderModeXluDL);
        gDPSetEnvColor(POLY_XLU_DISP++, 20, 40, 40, this->envColorAlpha);
        POLY_XLU_DISP =
            SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                               Dinolfos_OverrideLimbDraw, Dinolfos_PostLimbDraw, this, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool CanBeGoron(void) {
    int32_t goronMask = sApi->GetRandoItem(GORON_MASK_KEY);

    return goronMask >= 0 && sApi->LogicHasItem(goronMask);
}

// MM's CanKillEnemy(ACTOR_EN_DINOFOS): a sword, the Goron form or the bow; the hammer stands in for Goron fists.
static bool Dinolfos_CanKillInLogic(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RG_FAIRY_BOW) ||
           sApi->LogicCanUseItem(RG_MEGATON_HAMMER) || CanBeGoron();
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
    static Vec3s sSoulJointTable[DINOLFOS_LIMB_MAX];
    static Vec3s sSoulMorphTable[DINOLFOS_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sDinolfosSkel,
                           (AnimationHeader*)sDinolfosIdleAnim, sSoulJointTable, sSoulMorphTable, DINOLFOS_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.014f, 0.014f, 0.014f, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-sSoulJointTable[0].x, -sSoulJointTable[0].y, -sSoulJointTable[0].z, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyeTextures[0]);
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
    SOHCustomItemDefinition soul = Z64Items_Define(DINOLFOS_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Dinolfos";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Dinolfos%w!&Majora's Mask's Dinolfos can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " DINOLFOS_SOUL_ITEM_KEY);
    }
}

static void RegisterDinolfos(void) {
    SOHActorDefinition dinolfos = { 0 };

    dinolfos.structSize = sizeof(dinolfos);
    dinolfos.key = DINOLFOS_KEY;
    dinolfos.description = "Dinolfos (Majora's Mask)";
    dinolfos.category = ACTORCAT_ENEMY;
    dinolfos.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                          ACTOR_FLAG_DRAW_CULLING_DISABLED | ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    dinolfos.objectId = OBJECT_GAMEPLAY_KEEP;
    dinolfos.instanceSize = sizeof(EnDinolfos);
    dinolfos.init = EnDinolfos_Init;
    dinolfos.destroy = EnDinolfos_Destroy;
    dinolfos.update = EnDinolfos_Update;
    dinolfos.draw = EnDinolfos_Draw;
    dinolfos.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_MINIBOSS | SOH_ACTOR_ENEMY_NOT_IN_WATER;
    dinolfos.naviHint = "Dinolfos&%cIt dodges arrows and breathes fire. Wait for it to swing, then strike back with "
                        "your %rsword%c!%w";
    dinolfos.soulKey = DINOLFOS_KEY;
    dinolfos.logicKill = Dinolfos_CanKillInLogic;
    if (sApi->RegisterActor(&dinolfos) < 0) {
        sApi->Log("Could not register " DINOLFOS_KEY);
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
    RegisterDinolfos();
}
