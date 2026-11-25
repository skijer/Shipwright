// Majora's Mask's Wart (Boss_04 + its 82 bubbles, En_Tanron2) as an Unbound custom enemy, drawn from mm.o2r.
// The fight starts as after MM's intro. Its arena is the box its wall probes find, centred on the spawn point
// (+-1000) along any axis without a wall, as 2Ship does away from Great Bay Temple.
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

#define WART_KEY "mm.wart"
#define WART_SOUL_ITEM_KEY "soul.mm.wart"
#define WART_BUBBLE_KEY "mm.wart.bubble"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_BOSS04 "__OTR__objects/object_boss04/"

#define BGCHECK_UPDATE_WALL_FLOOR 0x05

#define MM_SE_IT_BIG_BOMB_EXPLOSION 0x185C
#define MM_SE_EN_B_SLIME_EAT 0x38D4
#define MM_SE_EN_IKURA_JUMP1 0x3A0F
#define MM_SE_EN_IKURA_JUMP2 0x3A10
#define MM_SE_EN_IKURA_DAMAGE 0x3A11
#define MM_SE_EN_IKURA_DEAD 0x3A12
#define MM_SE_EN_ME_DAMAGE 0x3A13
#define MM_SE_EN_ME_DEAD 0x3A14
#define MM_SE_EN_ME_ATTACK 0x3A16

#define WART_HEALTH 20
#define WART_SCALE 0.1f
#define WART_BUBBLE_COUNT 82
#define WART_ARENA_HALF_SIZE 1000.0f
#define WART_HOVER_HEIGHT 160.0f
#define WART_MUSIC_DELAY 50
#define WART_EYE_OPEN_THRESHOLD 3000.0f
#define WART_EYELID_OPEN 10000.0f

typedef enum {
    WART_LIMB_NONE,
    WART_LIMB_ROOT,
    WART_LIMB_BODY,
    WART_LIMB_EYE_ROOT,
    WART_LIMB_EYE,
    WART_LIMB_TOP_EYELID_ROOT,
    WART_LIMB_TOP_EYELID,
    WART_LIMB_BOTTOM_EYELID_ROOT,
    WART_LIMB_BOTTOM_EYELID,
    WART_LIMB_MAX
} WartLimb;

// What Wart tells the bubbles still stuck to its shell.
typedef enum {
    SHELL_ORDER_HOLD,
    SHELL_ORDER_RELEASE,
    SHELL_ORDER_BURST = 10,
} WartShellOrder;

typedef enum { BUBBLE_HOP_WANDER, BUBBLE_HOP_HOME, BUBBLE_HOP_CHASE } BubbleHopMode;

struct EnWart;
struct EnWartBubble;

typedef void (*EnWartActionFunc)(struct EnWart*, PlayState*);
typedef void (*EnWartBubbleActionFunc)(struct EnWartBubble*, PlayState*);

typedef struct EnWartBubble {
    Actor actor;
    EnWartBubbleActionFunc actionFunc;
    s16 frameCount;
    s16 wobbleAngle;
    s16 wobbleSpeed;
    s16 hopTimer;
    s16 squashTimer;
    s16 wanderTimer;
    s16 hitCooldown;
    u8 hopMode;
    bool hasLatched;
    bool isActive;
    u8 attackCooldown;
    bool hasShadow;
    f32 latchFraction;
    f32 latchStep;
    ColliderCylinder bodyCollider;
    ColliderCylinder attackCollider;
} EnWartBubble;

typedef struct EnWart {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[WART_LIMB_MAX];
    Vec3s morphTable[WART_LIMB_MAX];
    EnWartActionFunc actionFunc;
    s16 frameCount;
    u8 shellOrder;
    u8 pendingBreakouts;
    s16 moveTimer;
    s16 waitTimer;
    s16 invincibilityTimer;
    s16 flashTimer;
    s16 eyelidToggleTimer;
    s16 eyeYaw;
    s16 eyeYawTarget;
    s16 eyePitch;
    s16 eyeShakeTimer;
    s16 shellYaw;
    s16 shellPitch;
    s16 shellYawSpeed;
    s16 shellPitchSpeed;
    s16 squashTimer;
    s16 bubbleCount;
    s16 musicTimer;
    bool isFightStarted;
    f32 eyelidOpening;
    f32 eyelidTarget;
    f32 wanderSpeed;
    f32 arenaMinX;
    f32 arenaMaxX;
    f32 arenaMinZ;
    f32 arenaMaxZ;
    f32 arenaCenterX;
    f32 arenaCenterZ;
    f32 stretchX;
    f32 stretchY;
    f32 stretchZ;
    f32 rippleScale;
    Vec3f bubbleSlots[WART_BUBBLE_COUNT];
    Vec3f shellCenter;
    Vec3f wanderTarget;
    EnWartBubble* bubbles[WART_BUBBLE_COUNT];
    ColliderJntSph eyeCollider;
    ColliderJntSphElement eyeElements[1];
    ColliderJntSph bodyCollider;
    ColliderJntSphElement bodyElements[1];
} EnWart;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sWartSkel[] = OBJ_BOSS04 "gWartSkel";
static const ALIGN_ASSET(2) char sIdleAnim[] = OBJ_BOSS04 "gWartIdleAnim";
static const ALIGN_ASSET(2) char sShadowMaterialDL[] = OBJ_BOSS04 "gWartShadowMaterialDL";
static const ALIGN_ASSET(2) char sShadowModelDL[] = OBJ_BOSS04 "gWartShadowModelDL";
static const ALIGN_ASSET(2) char sBubbleMaterialDL[] = OBJ_BOSS04 "gWartBubbleMaterialDL";
static const ALIGN_ASSET(2) char sBubbleModelDL[] = OBJ_BOSS04 "gWartBubbleModelDL";

static ColliderJntSphElementInit sEyeElementsInit[1] = {
    { { ELEMTYPE_UNK3,
        { 0xFFCFFFFF, 0x00, 0x08 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_ON },
      { 0, { { 0, 0, 0 }, 50 }, 100 } },
};

static ColliderJntSphInit sEyeJntSphInit = {
    { COLTYPE_HIT3, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_PLAYER, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    ARRAY_COUNT(sEyeElementsInit),
    sEyeElementsInit,
};

static ColliderJntSphElementInit sBodyElementsInit[1] = {
    { { ELEMTYPE_UNK3,
        { 0xFFCFFFFF, 0x00, 0x08 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_ON },
      { 0, { { 0, 0, 0 }, 70 }, 100 } },
};

static ColliderJntSphInit sBodyJntSphInit = {
    { COLTYPE_METAL, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_PLAYER, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    ARRAY_COUNT(sBodyElementsInit),
    sBodyElementsInit,
};

static ColliderCylinderInit sBubbleBodyCylinderInit = {
    { COLTYPE_HIT3, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK3,
      { 0xFFCFFFFF, 0x00, 0x04 },
      { 0xFFFFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_ON | BUMP_HOOKABLE,
      OCELEM_ON },
    { 30, 50, -25, { 0, 0, 0 } },
};

static ColliderCylinderInit sBubbleAttackCylinderInit = {
    { COLTYPE_HIT3, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK3,
      { 0xFFCFFFFF, 0x00, 0x04 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_ON | BUMP_HOOKABLE,
      OCELEM_ON },
    { 22, 42, -21, { 0, 0, 0 } },
};

// MM entries multiply the weapon's own damage; OoT's are absolute.
static DamageTable sWartDamageTable = {
    /* Deku nut      */ DMG_ENTRY(1, 0xF),
    /* Deku stick    */ DMG_ENTRY(2, 0xF),
    /* Slingshot     */ DMG_ENTRY(2, 0xF),
    /* Explosive     */ DMG_ENTRY(4, 0xF),
    /* Boomerang     */ DMG_ENTRY(2, 0xF),
    /* Normal arrow  */ DMG_ENTRY(4, 0xF),
    /* Hammer swing  */ DMG_ENTRY(4, 0xF),
    /* Hookshot      */ DMG_ENTRY(2, 0xF),
    /* Kokiri sword  */ DMG_ENTRY(2, 0xF),
    /* Master sword  */ DMG_ENTRY(4, 0xF),
    /* Giant's Knife */ DMG_ENTRY(8, 0xF),
    /* Fire arrow    */ DMG_ENTRY(4, 0x2),
    /* Ice arrow     */ DMG_ENTRY(4, 0x3),
    /* Light arrow   */ DMG_ENTRY(4, 0x4),
    /* Unk arrow 1   */ DMG_ENTRY(4, 0xF),
    /* Unk arrow 2   */ DMG_ENTRY(4, 0xF),
    /* Unk arrow 3   */ DMG_ENTRY(4, 0xF),
    /* Fire magic    */ DMG_ENTRY(4, 0x2),
    /* Ice magic     */ DMG_ENTRY(4, 0x3),
    /* Light magic   */ DMG_ENTRY(4, 0x4),
    /* Shield        */ DMG_ENTRY(0, 0xF),
    /* Mirror Ray    */ DMG_ENTRY(0, 0xF),
    /* Kokiri spin   */ DMG_ENTRY(2, 0xF),
    /* Giant spin    */ DMG_ENTRY(8, 0xF),
    /* Master spin   */ DMG_ENTRY(4, 0xF),
    /* Kokiri jump   */ DMG_ENTRY(4, 0xF),
    /* Giant jump    */ DMG_ENTRY(8, 0xF),
    /* Master jump   */ DMG_ENTRY(8, 0xF),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(4, 0xF),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

static DamageTable sBubbleDamageTable = {
    /* Deku nut      */ DMG_ENTRY(1, 0xF),
    /* Deku stick    */ DMG_ENTRY(1, 0xF),
    /* Slingshot     */ DMG_ENTRY(1, 0xF),
    /* Explosive     */ DMG_ENTRY(2, 0xF),
    /* Boomerang     */ DMG_ENTRY(1, 0xF),
    /* Normal arrow  */ DMG_ENTRY(2, 0xF),
    /* Hammer swing  */ DMG_ENTRY(2, 0xF),
    /* Hookshot      */ DMG_ENTRY(0, 0xF),
    /* Kokiri sword  */ DMG_ENTRY(2, 0xF),
    /* Master sword  */ DMG_ENTRY(2, 0xF),
    /* Giant's Knife */ DMG_ENTRY(2, 0xF),
    /* Fire arrow    */ DMG_ENTRY(2, 0x2),
    /* Ice arrow     */ DMG_ENTRY(2, 0x3),
    /* Light arrow   */ DMG_ENTRY(2, 0x4),
    /* Unk arrow 1   */ DMG_ENTRY(2, 0xF),
    /* Unk arrow 2   */ DMG_ENTRY(2, 0xF),
    /* Unk arrow 3   */ DMG_ENTRY(2, 0xF),
    /* Fire magic    */ DMG_ENTRY(2, 0x2),
    /* Ice magic     */ DMG_ENTRY(2, 0x3),
    /* Light magic   */ DMG_ENTRY(2, 0x4),
    /* Shield        */ DMG_ENTRY(0, 0xF),
    /* Mirror Ray    */ DMG_ENTRY(0, 0xF),
    /* Kokiri spin   */ DMG_ENTRY(2, 0xF),
    /* Giant spin    */ DMG_ENTRY(2, 0xF),
    /* Master spin   */ DMG_ENTRY(2, 0xF),
    /* Kokiri jump   */ DMG_ENTRY(2, 0xF),
    /* Giant jump    */ DMG_ENTRY(2, 0xF),
    /* Master jump   */ DMG_ENTRY(2, 0xF),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(2, 0xF),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;
static int16_t sBubbleActorId = -1;

static void Bubble_Latched(EnWartBubble* this, PlayState* play);
static void Bubble_Hop(EnWartBubble* this, PlayState* play);
static void Wart_Dying(EnWart* this, PlayState* play);

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

static void Wart_StartMinibossMusic(EnWart* this) {
    if (this->isFightStarted) {
        return;
    }
    this->isFightStarted = true;
    func_800F5ACC(NA_BGM_MINI_BOSS);
}

static void Wart_StopMinibossMusic(EnWart* this) {
    if (!this->isFightStarted) {
        return;
    }
    this->isFightStarted = false;
    func_800F5B58();
}

static void Wart_PlaceJntSph(ColliderJntSph* collider, Vec3f* center) {
    ColliderJntSphElementDim* dim = &collider->elements[0].dim;

    dim->worldSphere.center.x = center->x;
    dim->worldSphere.center.y = center->y;
    dim->worldSphere.center.z = center->z;
    dim->worldSphere.radius = dim->modelSphere.radius * dim->scale;
}

// Wall probes along +-X and +-Z bound the arena; an axis without a wall keeps the default box around the spawn.
static void Wart_FindArena(EnWart* this, PlayState* play) {
    static Vec3f sProbeOffsets[4] = {
        { -WART_ARENA_HALF_SIZE, 0.0f, 0.0f },
        { WART_ARENA_HALF_SIZE, 0.0f, 0.0f },
        { 0.0f, 0.0f, -WART_ARENA_HALF_SIZE },
        { 0.0f, 0.0f, WART_ARENA_HALF_SIZE },
    };
    f32* bounds[4] = { &this->arenaMinX, &this->arenaMaxX, &this->arenaMinZ, &this->arenaMaxZ };

    this->arenaMinX = this->actor.world.pos.x - WART_ARENA_HALF_SIZE;
    this->arenaMaxX = this->actor.world.pos.x + WART_ARENA_HALF_SIZE;
    this->arenaMinZ = this->actor.world.pos.z - WART_ARENA_HALF_SIZE;
    this->arenaMaxZ = this->actor.world.pos.z + WART_ARENA_HALF_SIZE;
    for (s32 i = 0; i < ARRAY_COUNT(sProbeOffsets); i++) {
        Vec3f probeEnd = { this->actor.world.pos.x + sProbeOffsets[i].x, this->actor.world.pos.y + 200.0f,
                           this->actor.world.pos.z + sProbeOffsets[i].z };
        CollisionPoly* poly;
        Vec3f hitPos;
        s32 bgId;

        if (BgCheck_EntityLineTest1(&play->colCtx, &this->actor.world.pos, &probeEnd, &hitPos, &poly, true, false,
                                    false, true, &bgId)) {
            *bounds[i] = i < 2 ? hitPos.x : hitPos.z;
        }
    }
    this->arenaCenterX = this->arenaMinX + (this->arenaMaxX - this->arenaMinX) * 0.5f;
    this->arenaCenterZ = this->arenaMinZ + (this->arenaMaxZ - this->arenaMinZ) * 0.5f;
}

// Spawns the bubbles in a spiral on the floor; they fly up and latch onto the shell.
static bool Wart_SpawnBubbles(EnWart* this, PlayState* play) {
    f32 radius = 110.0f;
    s16 yaw = 0;

    for (s32 i = 0; i < WART_BUBBLE_COUNT; i++) {
        EnWartBubble* bubble = (EnWartBubble*)Actor_Spawn(
            &play->actorCtx, play, sBubbleActorId, this->arenaCenterX + Math_SinS(yaw) * radius,
            this->actor.floorHeight, this->arenaCenterZ + Math_CosS(yaw) * radius, 0, 0, 0, i);

        if (bubble == NULL) {
            return false;
        }
        // OoT runs Init inside Actor_Spawn, before a parent could be set, so the bubble is attached here.
        bubble->actor.parent = &this->actor;
        this->bubbles[i] = bubble;
        radius += 2.5f;
        yaw += 0x1300;
    }
    this->bubbleCount = WART_BUBBLE_COUNT;
    return true;
}

static void Wart_Float(EnWart* this, PlayState* play);

static void EnWart_Init(Actor* thisx, PlayState* play) {
    EnWart* this = (EnWart*)thisx;

    Actor_SetScale(&this->actor, WART_SCALE);
    this->actor.colChkInfo.health = WART_HEALTH;
    this->actor.colChkInfo.damageTable = &sWartDamageTable;
    this->stretchX = 1.0f;
    this->stretchY = 1.0f;
    this->stretchZ = 1.0f;
    Collider_InitJntSph(play, &this->eyeCollider);
    Collider_SetJntSph(play, &this->eyeCollider, &this->actor, &sEyeJntSphInit, this->eyeElements);
    Collider_InitJntSph(play, &this->bodyCollider);
    Collider_SetJntSph(play, &this->bodyCollider, &this->actor, &sBodyJntSphInit, this->bodyElements);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sWartSkel, (AnimationHeader*)sIdleAnim,
                       this->jointTable, this->morphTable, WART_LIMB_MAX);
    Wart_FindArena(this, play);
    this->actor.world.pos.x = this->arenaCenterX;
    this->actor.world.pos.z = this->arenaCenterZ;
    Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 60.0f, 60.0f, 0x04);
    this->actor.world.pos.y = this->actor.floorHeight + WART_HOVER_HEIGHT;
    this->musicTimer = WART_MUSIC_DELAY;
    this->actionFunc = Wart_Float;
    if (!Wart_SpawnBubbles(this, play)) {
        Actor_Kill(&this->actor);
    }
}

static void EnWart_Destroy(Actor* thisx, PlayState* play) {
    EnWart* this = (EnWart*)thisx;

    for (s32 i = 0; i < WART_BUBBLE_COUNT; i++) {
        if (this->bubbles[i] != NULL) {
            this->bubbles[i]->actor.parent = NULL;
            Actor_Kill(&this->bubbles[i]->actor);
        }
    }
    Wart_StopMinibossMusic(this);
    Collider_DestroyJntSph(play, &this->eyeCollider);
    Collider_DestroyJntSph(play, &this->bodyCollider);
}

static void Wart_SetupCharge(EnWart* this);

// Phase one: hovers around the arena centre watching Link; every so often a bubble breaks off the shell.
static void Wart_Float(EnWart* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->frameCount >= 0x500) {
        this->frameCount -= 0x300;
        this->pendingBreakouts++;
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 10, 0x200);
    this->actor.world.pos.y = this->actor.floorHeight + WART_HOVER_HEIGHT + Math_SinS(this->frameCount * 512) * 10.0f;
    Math_ApproachF(&this->actor.speedXZ, this->wanderSpeed, 1.0f, 0.5f);
    if (this->moveTimer == 0) {
        this->moveTimer = Rand_ZeroFloat(100.0f) + 50.0f;
        this->wanderSpeed = Rand_ZeroFloat(3.0f);
        if (Rand_ZeroOne() < 0.1f) {
            this->wanderTarget = player->actor.world.pos;
        } else {
            this->wanderTarget.x = Rand_CenteredFloat(600.0f) + this->arenaCenterX;
            this->wanderTarget.z = Rand_CenteredFloat(600.0f) + this->arenaCenterZ;
        }
    }
    s16 yawToTarget =
        Math_Atan2S(this->wanderTarget.z - this->actor.world.pos.z, this->wanderTarget.x - this->actor.world.pos.x);

    Math_ApproachS(&this->actor.world.rot.y, yawToTarget, 5, 0x200);
    if ((s8)this->actor.colChkInfo.health <= 10) {
        Wart_SetupCharge(this);
    }
}

static void Wart_Charge(EnWart* this, PlayState* play);

static void Wart_SetupCharge(EnWart* this) {
    this->actor.world.rot = this->actor.shape.rot;
    this->moveTimer = 0;
    this->shellOrder = SHELL_ORDER_RELEASE;
    this->waitTimer = 60;
    this->actor.speedXZ = 0.0f;
    this->actor.gravity = -3.0f;
    this->actionFunc = Wart_Charge;
}

static void Wart_HitWall(EnWart* this, PlayState* play) {
    PlaySfx(&this->actor, MM_SE_IT_BIG_BOMB_EXPLOSION, NA_SE_IT_BOMB_EXPLOSION);
    Actor_RequestQuakeAndRumble(&this->actor, play, 15, 10);
    this->squashTimer = 15;
    func_80033480(play, &this->actor.focus.pos, 100.0f, 40, 500, 10, false);
}

// Phase two: drops to the floor bare and rams around the arena, turning on Link now and then with its eye open.
static void Wart_Charge(EnWart* this, PlayState* play) {
    bool isTouchingWall = (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) != 0;

    if (this->invincibilityTimer == 14 || (isTouchingWall && this->moveTimer == 0)) {
        this->moveTimer = 20;
        if (Rand_ZeroOne() < 0.2f && this->invincibilityTimer == 0) {
            this->actor.world.rot.y = this->actor.yawTowardsPlayer;
            this->eyelidTarget = WART_EYELID_OPEN;
            this->eyelidToggleTimer = 100;
        } else {
            this->actor.world.rot.y = BINANG_ROT180((s16)Rand_ZeroFloat(8000.0f) + this->actor.world.rot.y);
        }
        this->actor.speedXZ = 0.0f;
        if (isTouchingWall) {
            Wart_HitWall(this, play);
        }
    }
    Math_ApproachS(&this->actor.shape.rot.x, Math_SinS(this->frameCount * 0xFB8) * 5000.0f, 5, 0x800);
    Math_ApproachS(&this->actor.shape.rot.z, Math_SinS(this->frameCount * 0xCD0) * 3000.0f, 5, 0x800);
    if (this->squashTimer != 0) {
        return;
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 5, 0x1000);
    if (this->waitTimer != 0) {
        return;
    }
    Vec3f ripplePos = { this->actor.world.pos.x, this->actor.floorHeight + 2.0f, this->actor.world.pos.z };

    Math_ApproachF(&this->actor.speedXZ, 20.0f, 1.0f, 1.0f);
    EffectSsGRipple_Spawn(play, &ripplePos, 1400, 500, 0);
    PlaySfx(&this->actor, MM_SE_EN_ME_ATTACK - SFX_FLAG, NA_SE_EN_OCTAROCK_FLOAT - SFX_FLAG);
}

static void Wart_SetupDying(EnWart* this, PlayState* play) {
    this->moveTimer = 60;
    this->waitTimer = 100;
    this->actor.speedXZ = 0.0f;
    this->eyelidTarget = WART_EYELID_OPEN;
    this->eyelidToggleTimer = 200;
    PlaySfx(&this->actor, MM_SE_EN_ME_DEAD, NA_SE_EN_GOMA_DEAD);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Wart_StopMinibossMusic(this);
    this->shellOrder = SHELL_ORDER_BURST;
    this->invincibilityTimer = 100;
    this->flashTimer = 100;
    Enemy_StartFinishingBlow(play, &this->actor);
    this->actionFunc = Wart_Dying;
}

static void Wart_Explode(EnWart* this, PlayState* play) {
    EffectSsBomb2_SpawnLayered(play, &this->actor.world.pos, &sZeroVec, &sZeroVec, 200, 19);
    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_IT_BOMB_EXPLOSION);
}

// Wobbles, swells and bursts in two explosions.
static void Wart_Dying(EnWart* this, PlayState* play) {
    this->eyeShakeTimer = 10;
    if (this->moveTimer >= 5) {
        this->stretchY = Math_SinS(this->moveTimer * 0x3000) * 0.1f + 1.0f;
        this->stretchZ = Math_CosS(this->moveTimer * 0x3000) * 0.1f + 1.0f;
        this->stretchX = this->stretchZ;
    }
    if (this->moveTimer == 5) {
        this->stretchX *= 1.3f;
        this->stretchY *= 1.3f;
        this->stretchZ *= 1.3f;
    }
    if (this->moveTimer == 3) {
        this->stretchX = 0.0f;
        this->stretchY = 0.0f;
        this->stretchZ = 0.0f;
    }
    if (this->moveTimer == 2 || this->moveTimer == 5) {
        Wart_Explode(this, play);
    }
    if (this->waitTimer == 0) {
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
        Actor_Kill(&this->actor);
    }
}

static void Wart_CheckEyeHit(EnWart* this, PlayState* play) {
    ColliderInfo* eye = &this->eyeCollider.elements[0].info;

    if (this->invincibilityTimer != 0 || !(eye->bumperFlags & BUMP_HIT)) {
        return;
    }
    eye->bumperFlags &= ~BUMP_HIT;
    PlaySfx(&this->actor, MM_SE_EN_ME_DAMAGE, NA_SE_EN_GOMA_DAM1);
    this->actor.colChkInfo.health -= this->actor.colChkInfo.damage;
    if ((s8)this->actor.colChkInfo.health <= 0) {
        Wart_SetupDying(this, play);
        return;
    }
    this->eyeShakeTimer = 15;
    this->invincibilityTimer = 15;
    this->flashTimer = 15;
}

static void Wart_ComputeRing(Vec3f* slots, s32 count, f32 offsetX, f32 angleStep, f32 radius) {
    Vec3f radial = { 0.0f, 0.0f, radius };

    Matrix_Push();
    Matrix_Translate(offsetX, 0.0f, 0.0f, MTXMODE_APPLY);
    for (s32 i = 0; i < count; i++) {
        Matrix_RotateX(angleStep, MTXMODE_APPLY);
        Matrix_MultVec3f(&radial, &slots[i]);
    }
    Matrix_Pop();
}

// The shell is nine rings of slots around the eye; it spins faster as bubbles are lost.
static void Wart_ComputeBubbleSlots(EnWart* this) {
    static const struct {
        s32 count;
        f32 offsetX;
        f32 angleStep;
        f32 radius;
    } sRings[] = {
        { 16, 0.0f, 0.3926991f, 100.0f }, { 15, 38.0f, 0.41887903f, 92.0f }, { 15, -38.0f, 0.41887903f, 92.0f },
        { 11, 71.0f, 0.5711987f, 71.0f }, { 11, -71.0f, 0.5711987f, 71.0f }, { 6, 92.0f, 1.0471976f, 38.0f },
        { 6, -92.0f, 1.0471976f, 38.0f }, { 1, 100.0f, 0.0f, 0.0f },         { 1, -100.0f, 0.0f, 0.0f },
    };
    Vec3f* slot = this->bubbleSlots;

    this->shellPitch += this->shellPitchSpeed;
    this->shellYaw += this->shellYawSpeed;
    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_Translate(0.0f, 0.0f, 10.0f, MTXMODE_APPLY);
    Matrix_MultVec3f(&sZeroVec, &this->shellCenter);
    Matrix_Scale(this->actor.scale.x * 13.0f, this->actor.scale.y * 13.0f, this->actor.scale.z * 16.0f, MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(this->shellYaw), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(this->shellPitch), MTXMODE_APPLY);
    for (s32 i = 0; i < ARRAY_COUNT(sRings); i++) {
        Wart_ComputeRing(slot, sRings[i].count, sRings[i].offsetX, sRings[i].angleStep, sRings[i].radius);
        slot += sRings[i].count;
    }
}

static void Wart_UpdateTimers(EnWart* this) {
    this->frameCount++;
    DECR(this->moveTimer);
    DECR(this->waitTimer);
    DECR(this->eyelidToggleTimer);
    DECR(this->invincibilityTimer);
    DECR(this->flashTimer);
    DECR(this->eyeShakeTimer);
    DECR(this->squashTimer);
}

// The eyelids open and shut on a random rhythm; a fresh hit snaps them shut.
static void Wart_UpdateEyelids(EnWart* this) {
    if (this->flashTimer == 10) {
        this->eyelidTarget = 0.0f;
        this->eyelidToggleTimer = Rand_ZeroFloat(100.0f) + 60.0f;
    }
    if (this->eyelidToggleTimer == 0) {
        this->eyelidToggleTimer = Rand_ZeroFloat(100.0f) + 60.0f;
        this->eyelidTarget = WART_EYELID_OPEN - this->eyelidTarget;
    }
    Math_ApproachF(&this->eyelidOpening, this->eyelidTarget, 0.2f, 1000.0f);
}

static void Wart_TrackPlayer(EnWart* this) {
    s16 yawToPlayer = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;

    this->eyeYawTarget = ABS(yawToPlayer) < 0x6000 ? CLAMP(yawToPlayer, -0x2500, 0x2500) : 0;
    Math_ApproachS(&this->eyeYaw, this->eyeYawTarget, 5, 0x800);
    this->eyePitch = 2000;
    this->stretchZ = Math_SinS(this->squashTimer * 0x3000) * this->squashTimer * 0.02f + 1.0f;
    this->stretchY = Math_CosS(this->squashTimer * 0x3000) * this->squashTimer * 0.02f + 1.0f;
    this->stretchX = this->stretchY;
    this->actor.shape.yOffset = (this->stretchY - 1.0f) * 1000.0f;
}

static void Wart_SubmitColliders(EnWart* this, PlayState* play) {
    if (this->eyelidOpening > WART_EYE_OPEN_THRESHOLD) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->eyeCollider.base);
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    } else {
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->bodyCollider.base);
}

// Its floor check runs 100 units below the body, so it hovers (and lands) that high above the ground.
static void Wart_Move(EnWart* this, PlayState* play) {
    Actor_MoveXZGravity(&this->actor);
    this->actor.world.pos.y -= 100.0f;
    this->actor.prevPos.y -= 100.0f;
    Actor_UpdateBgCheckInfo(play, &this->actor, 100.0f, 120.0f, 200.0f, BGCHECK_UPDATE_WALL_FLOOR);
    this->actor.world.pos.y += 100.0f;
    this->actor.prevPos.y += 100.0f;
}

static void EnWart_Update(Actor* thisx, PlayState* play) {
    EnWart* this = (EnWart*)thisx;

    Wart_UpdateTimers(this);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    Wart_Move(this, play);
    this->rippleScale = Math_SinS(play->gameplayFrames * 0x3000) * 0.1f + 1.0f;
    Wart_UpdateEyelids(this);
    if (this->actionFunc != Wart_Dying) {
        Wart_TrackPlayer(this);
        Wart_CheckEyeHit(this, play);
        Wart_SubmitColliders(this, play);
        Wart_ComputeBubbleSlots(this);
    }
    if (this->musicTimer != 0 && --this->musicTimer == 0) {
        Wart_StartMinibossMusic(this);
    }
}

static s32 Wart_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnWart* this = (EnWart*)thisx;

    if (limbIndex == WART_LIMB_TOP_EYELID_ROOT || limbIndex == WART_LIMB_BOTTOM_EYELID_ROOT) {
        rot->y = rot->y + (s16)this->eyelidOpening - 0x500;
    }
    if (limbIndex != WART_LIMB_EYE) {
        return false;
    }
    rot->y += this->eyePitch;
    rot->z += this->eyeYaw;
    if (this->eyeShakeTimer != 0) {
        rot->y += (s16)(Math_SinS(this->frameCount * 0x3000) * (this->eyeShakeTimer * 500));
        rot->z += (s16)(Math_SinS(this->frameCount * 0x3500) * (this->eyeShakeTimer * 300));
    }
    return false;
}

static void Wart_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sFocusOffset = { 0.0f, -500.0f, 0.0f };
    static Vec3f sEyeOffset = { 0.0f, -200.0f, 0.0f };
    static Vec3f sBodyOffset = { 0.0f, 720.0f, 0.0f };
    EnWart* this = (EnWart*)thisx;
    Vec3f center;

    if (limbIndex != WART_LIMB_ROOT) {
        return;
    }
    Matrix_MultVec3f(&sFocusOffset, &this->actor.focus.pos);
    Matrix_MultVec3f(&sEyeOffset, &center);
    Wart_PlaceJntSph(&this->eyeCollider, &center);
    Matrix_MultVec3f(&sBodyOffset, &center);
    Wart_PlaceJntSph(&this->bodyCollider, &center);
}

static void Wart_DrawShadow(EnWart* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_44Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 0, 0, 0, 150);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sShadowMaterialDL);
    Matrix_Translate(this->shellCenter.x, this->actor.floorHeight, this->shellCenter.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_Translate(0.0f, 0.0f, -20.0f, MTXMODE_APPLY);
    Matrix_Scale(this->stretchX * 1.8f, 0.0f, this->stretchZ * 2.8f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sShadowModelDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Translucent bubbles are drawn back to front.
static s32 Wart_SortBubbles(EnWart* this, EnWartBubble** sorted) {
    s32 count = 0;

    for (s32 i = 0; i < WART_BUBBLE_COUNT; i++) {
        if (this->bubbles[i] != NULL) {
            sorted[count] = this->bubbles[i];
            count++;
        }
    }
    for (s32 i = 1; i < count; i++) {
        EnWartBubble* bubble = sorted[i];
        s32 j = i - 1;

        while (j >= 0 && sorted[j]->actor.projectedPos.z < bubble->actor.projectedPos.z) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = bubble;
    }
    return count;
}

static void Wart_DrawBubbles(EnWart* this, PlayState* play) {
    EnWartBubble* sorted[WART_BUBBLE_COUNT];
    s32 count = Wart_SortBubbles(this, sorted);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBubbleMaterialDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 150);
    for (s32 i = 0; i < count; i++) {
        Actor* bubble = &sorted[i]->actor;
        f32 wobble = BINANG_TO_RAD(sorted[i]->wobbleAngle);

        Matrix_Translate(bubble->world.pos.x, bubble->world.pos.y, bubble->world.pos.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(bubble->scale.x, bubble->scale.y, 0.0f, MTXMODE_APPLY);
        Matrix_RotateZ(wobble, MTXMODE_APPLY);
        Matrix_Scale(0.13f, 0.14299999f, 0.13f, MTXMODE_APPLY);
        Matrix_RotateZ(-wobble, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBubbleModelDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// Loose bubbles near Link get a shadow, and a ripple while they touch the floor.
static void Wart_DrawBubbleGroundMarks(EnWart* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_44Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 0, 0, 0, 150);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sShadowMaterialDL);
    for (s32 i = 0; i < WART_BUBBLE_COUNT; i++) {
        EnWartBubble* bubble = this->bubbles[i];

        if (bubble == NULL || !bubble->hasShadow) {
            continue;
        }
        Matrix_Translate(bubble->actor.world.pos.x, this->actor.floorHeight, bubble->actor.world.pos.z, MTXMODE_NEW);
        Matrix_Scale(0.6f, 0.0f, 0.6f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sShadowModelDL);
    }
    Gfx_SetupDL_60NoCDXlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    gDPSetAlphaDither(POLY_XLU_DISP++, G_AD_NOISE);
    gDPSetColorDither(POLY_XLU_DISP++, G_CD_NOISE);
    for (s32 i = 0; i < WART_BUBBLE_COUNT; i++) {
        EnWartBubble* bubble = this->bubbles[i];

        if (bubble == NULL || !bubble->hasShadow || bubble->actor.world.pos.y > bubble->actor.floorHeight) {
            continue;
        }
        Matrix_Translate(bubble->actor.world.pos.x, this->actor.floorHeight + 2.0f, bubble->actor.world.pos.z,
                         MTXMODE_NEW);
        Matrix_Scale(this->rippleScale, 0.0f, this->rippleScale, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffWaterRippleDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnWart_Draw(Actor* thisx, PlayState* play) {
    EnWart* this = (EnWart*)thisx;
    bool isFlashing = this->flashTimer & 1;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (isFlashing) {
        POLY_OPA_DISP = Gfx_SetFog(POLY_OPA_DISP, 255, 0, 0, 255, 900, 1099);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(0.0f, 0.0f, 800.0f, MTXMODE_APPLY);
    Matrix_Scale(this->stretchX, this->stretchY, this->stretchZ, MTXMODE_APPLY);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          Wart_OverrideLimbDraw, Wart_PostLimbDraw, this);
    Matrix_Pop();
    OPEN_DISPS(play->state.gfxCtx);
    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
    Wart_DrawShadow(this, play);
    Wart_DrawBubbles(this, play);
    Wart_DrawBubbleGroundMarks(this, play);
}

static EnWart* Bubble_GetWart(EnWartBubble* this) {
    return (EnWart*)this->actor.parent;
}

static void Bubble_SetHopVelocity(EnWartBubble* this, s16 yaw, f32 speed) {
    this->actor.velocity.x = Math_SinS(yaw) * speed;
    this->actor.velocity.z = Math_CosS(yaw) * speed;
}

static void Bubble_SetupLatched(EnWartBubble* this) {
    this->actionFunc = Bubble_Latched;
    this->latchStep = 0.0f;
    this->latchFraction = 0.0f;
    this->bodyCollider.dim.radius = 25;
    this->bodyCollider.dim.height = 46;
    this->bodyCollider.dim.yShift = -23;
    this->hasLatched = false;
}

// Glides to its slot on the shell, then rides it.
static void Bubble_Latched(EnWartBubble* this, PlayState* play) {
    Vec3f* slot = &Bubble_GetWart(this)->bubbleSlots[this->actor.params];

    if (this->latchFraction == 1.0f) {
        this->actor.world.pos = *slot;
    } else {
        f32 remaining =
            Math_SmoothStepToF(&this->actor.world.pos.x, slot->x, this->latchFraction, this->latchStep, 0.0f) +
            Math_SmoothStepToF(&this->actor.world.pos.y, slot->y, this->latchFraction, this->latchStep, 0.0f) +
            Math_SmoothStepToF(&this->actor.world.pos.z, slot->z, this->latchFraction, this->latchStep, 0.0f);

        if (!this->hasLatched && remaining < 2.0f) {
            this->hasLatched = true;
            PlaySfx(&this->actor, MM_SE_EN_B_SLIME_EAT, NA_SE_EN_OCTAROCK_BUBLE);
        }
        Math_ApproachF(&this->latchFraction, 1.0f, 1.0f, 0.02f);
        Math_ApproachF(&this->latchStep, 20.0f, 1.0f, 1.0f);
    }
    Math_ApproachF(&this->actor.scale.x, 1.0f, 0.1f, 0.2f);
    Math_ApproachF(&this->actor.scale.y, 1.0f, 0.1f, 0.2f);
}

static void Bubble_SetupHop(EnWartBubble* this, u8 hopMode) {
    this->actionFunc = Bubble_Hop;
    this->actor.speedXZ = 0.0f;
    this->actor.velocity = sZeroVec;
    this->hopMode = hopMode;
    this->isActive = true;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->bodyCollider.dim.radius = 30;
    this->bodyCollider.dim.height = 50;
    this->bodyCollider.dim.yShift = -25;
}

static s16 Bubble_YawToShell(EnWartBubble* this) {
    EnWart* wart = Bubble_GetWart(this);

    return Math_Atan2S(wart->shellCenter.z - this->actor.world.pos.z, wart->shellCenter.x - this->actor.world.pos.x);
}

// Picks the next hop: a random wander, back toward Wart, or at Link until its chase runs out.
static void Bubble_Bounce(EnWartBubble* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 yaw;

    PlaySfx(&this->actor, MM_SE_EN_IKURA_JUMP2, NA_SE_EN_OCTAROCK_LAND);
    switch (this->hopMode) {
        case BUBBLE_HOP_HOME:
            yaw = Bubble_YawToShell(this);
            this->actor.speedXZ = CLAMP_MAX(this->actor.speedXZ + 2.0f, 10.0f);
            break;
        case BUBBLE_HOP_CHASE:
            yaw = Math_Atan2S(player->actor.world.pos.z - this->actor.world.pos.z,
                              player->actor.world.pos.x - this->actor.world.pos.x) +
                  (s16)Rand_ZeroFloat(20000.0f);
            this->actor.speedXZ = Rand_ZeroFloat(7.0f) + 7.0f;
            if (this->wanderTimer == 0 && Bubble_GetWart(this)->shellOrder == SHELL_ORDER_HOLD) {
                this->hopMode = BUBBLE_HOP_HOME;
            }
            break;
        default:
            yaw = Rand_ZeroOne() > 0.2f ? (s16)Rand_ZeroFloat(0x10000) : Bubble_YawToShell(this);
            this->actor.speedXZ = Rand_ZeroFloat(5.0f) + 5.0f;
            break;
    }
    Bubble_SetHopVelocity(this, yaw, this->actor.speedXZ);
    this->actor.velocity.y = Rand_ZeroFloat(5.0f) + 12.0f;
    this->hopTimer = 5;
}

static void Bubble_Hop(EnWartBubble* this, PlayState* play) {
    if (this->hopTimer != 0) {
        Math_ApproachF(&this->actor.scale.x, 1.2f, 0.75f, 1.5f);
        Math_ApproachF(&this->actor.scale.y, 0.75f, 0.75f, 1.5f);
        if (this->hopTimer != 1) {
            return;
        }
        Vec3f* shell = &Bubble_GetWart(this)->shellCenter;
        f32 dx = shell->x - this->actor.world.pos.x;
        f32 dz = shell->z - this->actor.world.pos.z;

        if (this->hopMode != BUBBLE_HOP_WANDER && SQ(dx) + SQ(dz) < SQ(120.0f)) {
            Bubble_SetupLatched(this);
        } else {
            this->squashTimer = 10;
            PlaySfx(&this->actor, MM_SE_EN_IKURA_JUMP1, NA_SE_EN_OCTAROCK_JUMP);
        }
        return;
    }
    Math_Vec3f_Sum(&this->actor.world.pos, &this->actor.velocity, &this->actor.world.pos);
    this->actor.velocity.y -= 2.0f;
    if (this->actor.world.pos.y <= this->actor.floorHeight) {
        this->actor.world.pos.y = this->actor.floorHeight;
        Bubble_Bounce(this, play);
    }
    bool isStretching = this->squashTimer != 0;

    Math_ApproachF(&this->actor.scale.x, isStretching ? 0.75f : 1.0f, isStretching ? 0.5f : 0.1f,
                   isStretching ? 1.0f : 0.2f);
    Math_ApproachF(&this->actor.scale.y, isStretching ? 1.2f : 1.0f, isStretching ? 0.5f : 0.1f,
                   isStretching ? 1.0f : 0.2f);
}

static void Bubble_Burst(EnWartBubble* this, PlayState* play) {
    static Color_RGBA8 sPrimColor = { 255, 255, 255, 255 };
    static Color_RGBA8 sEnvColor = { 255, 100, 100, 255 };

    for (s32 i = 0; i < 15; i++) {
        Vec3f velocity;

        Matrix_RotateY(Rand_ZeroFloat(2 * M_PI), MTXMODE_NEW);
        Matrix_RotateX(Rand_ZeroFloat(2 * M_PI), MTXMODE_APPLY);
        Matrix_MultVec3f(&(Vec3f){ 0.0f, 0.0f, Rand_ZeroFloat(10.0f) + 5.0f }, &velocity);
        Vec3f pos = { this->actor.world.pos.x + velocity.x, this->actor.world.pos.y + velocity.y,
                      this->actor.world.pos.z + velocity.z };
        Vec3f accel = { velocity.x * -0.03f, velocity.y * -0.03f, velocity.z * -0.03f };

        EffectSsDtBubble_SpawnCustomColor(play, &pos, &velocity, &accel, &sPrimColor, &sEnvColor,
                                          Rand_ZeroFloat(100.0f) + 200.0f, Rand_ZeroFloat(5.0f) + 15.0f, false);
    }
    Actor_Kill(&this->actor);
}

static void Bubble_Hurt(EnWartBubble* this, PlayState* play) {
    Math_Vec3f_Sum(&this->actor.world.pos, &this->actor.velocity, &this->actor.world.pos);
    this->actor.velocity.y -= 2.0f;
    if (this->actor.world.pos.y > this->actor.floorHeight) {
        return;
    }
    this->actor.world.pos.y = this->actor.floorHeight;
    if ((s8)this->actor.colChkInfo.health <= 0) {
        EnWart* wart = Bubble_GetWart(this);

        wart->bubbleCount--;
        wart->shellPitchSpeed += 4;
        wart->shellYawSpeed += 4;
        Bubble_Burst(this, play);
        return;
    }
    this->actor.velocity.x *= 0.5f;
    this->actor.velocity.z *= 0.5f;
    Math_ApproachF(&this->actor.scale.x, 1.2f, 0.75f, 1.5f);
    Math_ApproachF(&this->actor.scale.y, 0.75f, 0.75f, 1.5f);
    if (this->actor.velocity.x < 0.1f && this->actor.velocity.z < 0.1f) {
        Bubble_SetupHop(this, BUBBLE_HOP_HOME);
    }
}

static void Bubble_SetupHurt(EnWartBubble* this, PlayState* play) {
    this->actor.colChkInfo.health -= this->actor.colChkInfo.damage;
    this->actionFunc = Bubble_Hurt;
    if ((s8)this->actor.colChkInfo.health > 0) {
        PlaySfx(&this->actor, MM_SE_EN_IKURA_DAMAGE, NA_SE_EN_BARI_DAMAGE);
        return;
    }
    Enemy_StartFinishingBlow(play, &this->actor);
    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 30, NA_SE_EN_BARI_DEAD);
}

static void Bubble_BreakAway(EnWartBubble* this) {
    Vec3f* shell = &Bubble_GetWart(this)->shellCenter;

    Bubble_SetupHop(this, BUBBLE_HOP_CHASE);
    Bubble_SetHopVelocity(this, Math_Atan2S(this->actor.world.pos.z - shell->z, this->actor.world.pos.x - shell->x),
                          10.0f);
    this->wanderTimer = Rand_ZeroFloat(100.0f) + 200.0f;
}

// The hookshot plucks a bubble and sends it home; other blows knock a stuck bubble loose or hurt a loose one.
static void Bubble_CheckHit(EnWartBubble* this, PlayState* play) {
    if (this->hitCooldown != 0 || !(this->bodyCollider.base.acFlags & AC_HIT)) {
        return;
    }
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    ColliderInfo* attacker = this->bodyCollider.info.acHitInfo;
    u32 dmgFlags = attacker != NULL ? attacker->toucher.dmgFlags : 0;

    if (dmgFlags & DMG_HOOKSHOT) {
        Bubble_SetupHop(this, BUBBLE_HOP_HOME);
        PlaySfx(&this->actor, MM_SE_EN_IKURA_DAMAGE, NA_SE_EN_BARI_DAMAGE);
        return;
    }
    this->hitCooldown = 15;
    if (this->actionFunc == Bubble_Latched) {
        PlaySfx(&this->actor, MM_SE_EN_IKURA_DAMAGE, NA_SE_EN_BARI_DAMAGE);
        Bubble_BreakAway(this);
        return;
    }
    bool isShieldPush = (dmgFlags & (DMG_SHIELD | DMG_MIR_RAY)) != 0;

    this->hitCooldown = isShieldPush ? 10 : 15;
    Bubble_SetHopVelocity(this, this->actor.yawTowardsPlayer, isShieldPush ? -10.0f : -20.0f);
    this->actor.velocity.y = 0.0f;
    if (!isShieldPush) {
        Bubble_SetupHurt(this, play);
    }
}

static void Bubble_FollowShellOrder(EnWartBubble* this, PlayState* play) {
    EnWart* wart = Bubble_GetWart(this);
    bool isLatched = this->actionFunc == Bubble_Latched;
    bool isBreakingOut = wart->pendingBreakouts != 0 && Rand_ZeroOne() < 0.1f;

    if (isLatched && (wart->shellOrder == SHELL_ORDER_RELEASE || isBreakingOut)) {
        if (wart->pendingBreakouts != 0) {
            wart->pendingBreakouts--;
        }
        Bubble_BreakAway(this);
    } else if (wart->shellOrder == SHELL_ORDER_BURST) {
        Bubble_Burst(this, play);
    }
}

// Loose bubbles bounce off the arena walls Wart found.
static void Bubble_StayInArena(EnWartBubble* this) {
    EnWart* wart = Bubble_GetWart(this);

    if (this->actor.world.pos.x < wart->arenaMinX + 20.0f || wart->arenaMaxX - 20.0f < this->actor.world.pos.x) {
        this->actor.world.pos.x = this->actor.prevPos.x;
        this->actor.velocity.x *= -1.0f;
    }
    if (this->actor.world.pos.z < wart->arenaMinZ + 20.0f || wart->arenaMaxZ - 20.0f < this->actor.world.pos.z) {
        this->actor.world.pos.z = this->actor.prevPos.z;
        this->actor.velocity.z *= -1.0f;
    }
}

static void Bubble_SubmitColliders(EnWartBubble* this, PlayState* play) {
    if (!this->isActive) {
        return;
    }
    if (this->actor.xzDistToPlayer < 100.0f) {
        if (this->attackCooldown == 0) {
            Collider_UpdateCylinder(&this->actor, &this->attackCollider);
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->attackCollider.base);
        } else {
            this->attackCooldown--;
        }
    }
    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
}

// Every 32 frames: stuck bubbles only react on the side facing Link; loose ones near Link cast a shadow.
static void Bubble_RefreshExposure(EnWartBubble* this) {
    if (this->frameCount & 0x1F) {
        return;
    }
    bool isLatched = this->actionFunc == Bubble_Latched;
    EnWart* wart = Bubble_GetWart(this);

    this->hasShadow = !isLatched && this->actor.xyzDistToPlayerSq < SQ(200.0f);
    if (!isLatched) {
        return;
    }
    s16 yawFromShell =
        Math_Atan2S(this->actor.world.pos.z - wart->shellCenter.z, this->actor.world.pos.x - wart->shellCenter.x);
    s16 offset = wart->actor.yawTowardsPlayer - yawFromShell;

    this->isActive = ABS(offset) <= 0x3000;
    if (this->isActive) {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    } else {
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    }
}

static void EnWartBubble_Init(Actor* thisx, PlayState* play) {
    EnWartBubble* this = (EnWartBubble*)thisx;

    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR;
    Actor_SetScale(&this->actor, 1.0f);
    this->actor.colChkInfo.health = 1;
    this->actor.colChkInfo.damageTable = &sBubbleDamageTable;
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBubbleBodyCylinderInit);
    Collider_InitCylinder(play, &this->attackCollider);
    Collider_SetCylinder(play, &this->attackCollider, &this->actor, &sBubbleAttackCylinderInit);
    Bubble_SetupLatched(this);
    this->wobbleAngle = Rand_ZeroFloat(0x10000);
    this->wobbleSpeed = Rand_ZeroFloat(800.0f) + 1400.0f;
    if (Rand_ZeroOne() < 0.5f) {
        this->wobbleSpeed = -this->wobbleSpeed;
    }
    Actor_UpdateBgCheckInfo(play, &this->actor, 35.0f, 60.0f, 60.0f, 0x04);
    this->actor.floorHeight += 20.0f;
    this->frameCount = Rand_ZeroFloat(32.0f);
}

static void EnWartBubble_Destroy(Actor* thisx, PlayState* play) {
    EnWartBubble* this = (EnWartBubble*)thisx;
    EnWart* wart = Bubble_GetWart(this);

    if (wart != NULL) {
        wart->bubbles[this->actor.params] = NULL;
    }
    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyCylinder(play, &this->attackCollider);
}

static void EnWartBubble_Update(Actor* thisx, PlayState* play) {
    EnWartBubble* this = (EnWartBubble*)thisx;

    // Not attached yet (still inside Wart's Init) or Wart is gone: OoT never clears a dead parent pointer.
    if (this->actor.parent == NULL || this->actor.parent->update == NULL) {
        Actor_Kill(&this->actor);
        return;
    }
    this->frameCount++;
    Actor_SetFocus(&this->actor, 0.0f);
    DECR(this->hopTimer);
    DECR(this->squashTimer);
    DECR(this->wanderTimer);
    DECR(this->hitCooldown);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    if (this->actionFunc != Bubble_Latched) {
        Bubble_StayInArena(this);
    }
    this->wobbleAngle += this->wobbleSpeed;
    Bubble_CheckHit(this, play);
    Bubble_FollowShellOrder(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    Bubble_SubmitColliders(this, play);
    Bubble_RefreshExposure(this);
    if ((this->actor.flags & ACTOR_FLAG_HOOKSHOT_ATTACHED) && this->actor.xzDistToPlayer < 80.0f) {
        this->actor.flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
        this->attackCooldown = 25;
        this->isActive = true;
    }
}

// MM's CanKillEnemy(ACTOR_BOSS_04): the bow or the hookshot reach its eye through the bubbles.
static bool Wart_CanKillInLogic(void* userData) {
    return sApi->LogicCanUseItem(RG_FAIRY_BOW) || sApi->LogicCanUseItem(RG_HOOKSHOT);
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
    static Vec3s sSoulJointTable[WART_LIMB_MAX];
    static Vec3s sSoulMorphTable[WART_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sWartSkel, (AnimationHeader*)sIdleAnim,
                           sSoulJointTable, sSoulMorphTable, WART_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
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
    SOHCustomItemDefinition soul = Z64Items_Define(WART_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Wart";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Wart%w!&Wart can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_Register(sApi, &soul)) {
        sApi->Log("Could not register " WART_SOUL_ITEM_KEY);
    }
}

static void RegisterBubble(void) {
    SOHActorDefinition bubble = { 0 };

    bubble.structSize = sizeof(bubble);
    bubble.key = WART_BUBBLE_KEY;
    bubble.description = "Wart's Bubble";
    bubble.category = ACTORCAT_ENEMY;
    bubble.actorFlags = ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    bubble.objectId = OBJECT_GAMEPLAY_KEEP;
    bubble.instanceSize = sizeof(EnWartBubble);
    bubble.init = EnWartBubble_Init;
    bubble.destroy = EnWartBubble_Destroy;
    bubble.update = EnWartBubble_Update;
    sBubbleActorId = sApi->RegisterActor(&bubble);
}

static void RegisterWart(void) {
    SOHActorDefinition wart = { 0 };

    wart.structSize = sizeof(wart);
    wart.key = WART_KEY;
    wart.description = "Wart";
    wart.category = ACTORCAT_ENEMY;
    wart.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                      ACTOR_FLAG_DRAW_CULLING_DISABLED;
    wart.objectId = OBJECT_GAMEPLAY_KEEP;
    wart.instanceSize = sizeof(EnWart);
    wart.init = EnWart_Init;
    wart.destroy = EnWart_Destroy;
    wart.update = EnWart_Update;
    wart.draw = EnWart_Draw;
    wart.enemyFlags =
        SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_MINIBOSS | SOH_ACTOR_ENEMY_NOT_IN_WATER | SOH_ACTOR_ENEMY_NEEDS_CEILING;
    wart.naviHint = "Wart&%cPull its bubbles away with the %rhookshot%c and pop them. Shoot the eye when it "
                    "opens!%w";
    wart.soulKey = WART_KEY;
    wart.logicKill = Wart_CanKillInLogic;
    if (sApi->RegisterActor(&wart) < 0) {
        sApi->Log("Could not register " WART_KEY);
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
    RegisterBubble();
    if (sBubbleActorId < 0) {
        sApi->Log("Could not register " WART_BUBBLE_KEY);
        return;
    }
    RegisterSoul();
    RegisterWart();
}
