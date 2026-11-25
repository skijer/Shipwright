// Majora's Mask's Real Bombchu (En_Rat) as an Unbound custom enemy, drawn from mm.o2r.
// Params 0x8000 is the overworld kind that burrows back up at home; the low byte scales its sight range.
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

#define REAL_BOMBCHU_KEY "mm.real_bombchu"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_RAT "__OTR__objects/object_rat/"

#define COLORFILTER_BLUE 0x0000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_CEILING_FLOOR ((1 << 0) | (1 << 1) | (1 << 2))

#define MM_SE_EN_BOMCHU_WALK 0x3828
#define MM_SE_EN_BOMCHU_VOICE 0x3854
#define MM_SE_EN_BOMCHU_AIM 0x3855
#define MM_SE_EN_BOMCHU_RUN 0x3856

#define REAL_BOMBCHU_OVERWORLD 0x8000
#define REAL_BOMBCHU_DMGEFF_STUN 0x1
#define REAL_BOMBCHU_DMGEFF_HOOKSHOT 0xF
#define REAL_BOMBCHU_RADIUS 23
#define REAL_BOMBCHU_SPARK_SCALE 45.0f

typedef enum {
    REAL_BOMBCHU_LIMB_NONE,
    REAL_BOMBCHU_LIMB_BODY,
    REAL_BOMBCHU_LIMB_HEAD,
    REAL_BOMBCHU_LIMB_TAIL_BASE,
    REAL_BOMBCHU_LIMB_TAIL_MIDDLE,
    REAL_BOMBCHU_LIMB_TAIL_END,
    REAL_BOMBCHU_LIMB_FRONT_LEFT_LEG,
    REAL_BOMBCHU_LIMB_BACK_LEFT_LEG,
    REAL_BOMBCHU_LIMB_FRONT_RIGHT_LEG,
    REAL_BOMBCHU_LIMB_BACK_RIGHT_LEG,
    REAL_BOMBCHU_LIMB_MAX
} RealBombchuLimb;

typedef enum { REAL_BOMBCHU_NOT_HOOKED, REAL_BOMBCHU_HOOK_STARTED, REAL_BOMBCHU_HOOKED } RealBombchuHookState;

struct EnRealBombchu;

typedef void (*EnRealBombchuActionFunc)(struct EnRealBombchu*, PlayState*);

typedef struct EnRealBombchu {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[REAL_BOMBCHU_LIMB_MAX];
    Vec3s morphTable[REAL_BOMBCHU_LIMB_MAX];
    EnRealBombchuActionFunc actionFunc;
    bool isOverworld;
    bool hasLostTrack;
    bool shouldRotateOntoSurfaces;
    s16 animLoopCounter;
    s16 timer;
    s16 stunTimer;
    s16 hookState;
    s32 blureIndices[2];
    f32 visualJitter;
    f32 attackRange;
    f32 reviveDepth;
    Vec3f axisForwards;
    Vec3f axisUp;
    Vec3f axisLeft;
    Vec3f smokePos;
    Vec3f sparkOffsets[2];
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[1];
} EnRealBombchu;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sRealBombchuSkel[] = OBJ_RAT "gRealBombchuSkel";
static const ALIGN_ASSET(2) char sRealBombchuRunAnim[] = OBJ_RAT "gRealBombchuRunAnim";
static const ALIGN_ASSET(2) char sRealBombchuSpotAnim[] = OBJ_RAT "gRealBombchuSpotAnim";

static const char* const sSparkTextures[] = { gEffSpark1Tex, gEffSpark2Tex, gEffSpark3Tex, gEffSpark4Tex };

// MM's ColliderSphere: OoT has none, so a one-element JntSph whose world sphere is placed by hand.
static ColliderJntSphElementInit sElementsInit[1] = {
    {
        { ELEMTYPE_UNK0,
          { 0xF7CFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x08 },
          { 0xF7CFFFFF, 0x00, 0x00 },
          TOUCH_ON | TOUCH_SFX_HARD,
          BUMP_ON | BUMP_HOOKABLE,
          OCELEM_ON },
        { REAL_BOMBCHU_LIMB_BODY, { { 0, 0, 0 }, REAL_BOMBCHU_RADIUS }, 100 },
    },
};

static ColliderJntSphInit sColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_JNTSPH },
    1,
    sElementsInit,
};

// It has one hit point: anything that connects sets it off, except nuts that stun it and the hookshot that reels it in.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, REAL_BOMBCHU_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(1, 0x0),
    /* Slingshot     */ DMG_ENTRY(1, 0x0),
    /* Explosive     */ DMG_ENTRY(1, 0x0),
    /* Boomerang     */ DMG_ENTRY(1, 0x0),
    /* Normal arrow  */ DMG_ENTRY(1, 0x0),
    /* Hammer swing  */ DMG_ENTRY(1, 0x0),
    /* Hookshot      */ DMG_ENTRY(0, REAL_BOMBCHU_DMGEFF_HOOKSHOT),
    /* Kokiri sword  */ DMG_ENTRY(1, 0x0),
    /* Master sword  */ DMG_ENTRY(1, 0x0),
    /* Giant's Knife */ DMG_ENTRY(1, 0x0),
    /* Fire arrow    */ DMG_ENTRY(2, 0x0),
    /* Ice arrow     */ DMG_ENTRY(2, 0x0),
    /* Light arrow   */ DMG_ENTRY(2, 0x0),
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
    /* Hammer jump   */ DMG_ENTRY(1, 0x0),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

static CollisionCheckInfoInit sColChkInfoInit = { 1, 30, 30, 50 };

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sSmokeAccel = { 0.0f, 0.6f, 0.0f };
static Color_RGBA8 sSmokeColor = { 255, 255, 255, 255 };
static Vec3f sBlureP1Offset = { 0.0f, 10.5f, -9.0f };
static Vec3f sBlureP2Offsets[2] = { { 18.0f, 0.0f, -7.5f }, { -18.0f, 0.0f, -7.5f } };
static Vec3f sDustVelocity = { 0.0f, 3.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

static void RealBombchu_Revive(EnRealBombchu* this, PlayState* play);
static void RealBombchu_Idle(EnRealBombchu* this, PlayState* play);
static void RealBombchu_SpottedLink(EnRealBombchu* this, PlayState* play);
static void RealBombchu_Chase(EnRealBombchu* this, PlayState* play);
static void RealBombchu_Bounced(EnRealBombchu* this, PlayState* play);
static void RealBombchu_PostDetonation(EnRealBombchu* this, PlayState* play);
static void RealBombchu_Explode(EnRealBombchu* this, PlayState* play);
static void EnRealBombchu_Draw(Actor* thisx, PlayState* play);

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

static void RealBombchu_InitAxes(EnRealBombchu* this) {
    Vec3f unitX = { 1.0f, 0.0f, 0.0f };
    Vec3f unitY = { 0.0f, 1.0f, 0.0f };
    Vec3f unitZ = { 0.0f, 0.0f, 1.0f };

    Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_NEW);
    Matrix_RotateX(BINANG_TO_RAD(this->actor.shape.rot.x), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(this->actor.shape.rot.z), MTXMODE_APPLY);
    Matrix_MultVec3f(&unitZ, &this->axisForwards);
    Matrix_MultVec3f(&unitY, &this->axisUp);
    Matrix_MultVec3f(&unitX, &this->axisLeft);
}

static void RealBombchu_UpdateRotation(EnRealBombchu* this) {
    MtxF mf;

    mf.xx = this->axisLeft.x;
    mf.yx = this->axisLeft.y;
    mf.zx = this->axisLeft.z;
    mf.xy = this->axisUp.x;
    mf.yy = this->axisUp.y;
    mf.zy = this->axisUp.z;
    mf.xz = this->axisForwards.x;
    mf.yz = this->axisForwards.y;
    mf.zz = this->axisForwards.z;
    Matrix_MtxFToYXZRotS(&mf, &this->actor.world.rot, false);
    this->actor.world.rot.x = -this->actor.world.rot.x;
}

// Tilts the axes onto the new surface; returns true when they changed. A degenerate tilt sets it off.
static bool RealBombchu_UpdateFloorPoly(EnRealBombchu* this, CollisionPoly* floorPoly, PlayState* play) {
    Vec3f normal = { 0.0f, 1.0f, 0.0f };
    Vec3f axis;

    this->actor.floorPoly = floorPoly;
    if (floorPoly != NULL) {
        normal.x = COLPOLY_GET_NORMAL(floorPoly->normal.x);
        normal.y = COLPOLY_GET_NORMAL(floorPoly->normal.y);
        normal.z = COLPOLY_GET_NORMAL(floorPoly->normal.z);
    }
    f32 normDotUp = DOTXYZ(normal, this->axisUp);

    if (fabsf(normDotUp) >= 0.999f) {
        return false;
    }
    f32 angle = Math_FAcosF(normDotUp);

    if (angle < 0.001f) {
        return false;
    }
    Math3D_Vec3f_Cross(&this->axisUp, &normal, &axis);
    f32 magnitude = Math3D_Vec3fMagnitude(&axis);

    if (magnitude < 0.001f) {
        RealBombchu_Explode(this, play);
        return false;
    }
    Math_Vec3f_Scale(&axis, 1.0f / magnitude);
    Matrix_RotateAxis(angle, &axis, MTXMODE_NEW);
    Vec3f left;

    Matrix_MultVec3f(&this->axisLeft, &left);
    this->axisLeft = left;
    Math3D_Vec3f_Cross(&this->axisLeft, &normal, &this->axisForwards);
    magnitude = Math3D_Vec3fMagnitude(&this->axisForwards);
    if (magnitude < 0.001f) {
        RealBombchu_Explode(this, play);
        return false;
    }
    Math_Vec3f_Scale(&this->axisForwards, 1.0f / magnitude);
    this->axisUp = normal;
    return true;
}

// Unrotates by its home rotation so it steers back home on whatever wall or ceiling it roams.
static s16 RealBombchu_GetYawTowardHome(EnRealBombchu* this) {
    Vec3f upInHome;
    Vec3f aheadInHome;
    Vec3f homeInHome;
    Vec3f worldInHome;
    Vec3f ahead;

    Matrix_RotateZ(BINANG_TO_RAD(-this->actor.home.rot.z), MTXMODE_NEW);
    Matrix_RotateX(BINANG_TO_RAD(-this->actor.home.rot.x), MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(-this->actor.home.rot.y), MTXMODE_APPLY);
    Matrix_MultVec3f(&this->axisUp, &upInHome);
    Math_Vec3f_Sum(&this->actor.world.pos, &this->axisForwards, &ahead);
    Matrix_MultVec3f(&ahead, &aheadInHome);
    Matrix_MultVec3f(&this->actor.home.pos, &homeInHome);
    Matrix_MultVec3f(&this->actor.world.pos, &worldInHome);
    s16 angle = Math_Vec3f_Yaw(&worldInHome, &homeInHome) - Math_Vec3f_Yaw(&worldInHome, &aheadInHome);

    if (upInHome.y < -0.25f) {
        angle -= 0x8000;
    }
    return angle + (s16)Rand_CenteredFloat(0x800);
}

static s16 RealBombchu_GetTurn(EnRealBombchu* this) {
    if (this->actionFunc != RealBombchu_Idle) {
        s16 angle = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;

        return this->axisUp.y < -0.25f ? angle - 0x8000 : angle;
    }
    if (Actor_WorldDistXYZToPoint(&this->actor, &this->actor.home.pos) > 50.0f) {
        return RealBombchu_GetYawTowardHome(this);
    }
    return Rand_ZeroOne() < 0.1f ? (s16)Rand_CenteredFloat(0x800) : 0;
}

static void RealBombchu_ChooseDirection(EnRealBombchu* this) {
    s16 turn = CLAMP(RealBombchu_GetTurn(this), -0x800, 0x800);
    Vec3f forwards;

    Matrix_RotateAxis(BINANG_TO_RAD(turn), &this->axisUp, MTXMODE_NEW);
    Matrix_MultVec3f(&this->axisForwards, &forwards);
    this->axisForwards = forwards;
    Math3D_Vec3f_Cross(&this->axisUp, &this->axisForwards, &this->axisLeft);
    this->shouldRotateOntoSurfaces = true;
}

// Treats the water surface as ground it can run across; crawlspace walls are not surfaces.
static bool RealBombchu_FindSurface(PlayState* play, Vec3f* posA, Vec3f* posB, Vec3f* hit, CollisionPoly** poly,
                                    s32* bgId) {
    WaterBox* waterBox;
    f32 waterSurface;
    bool isOnWater = WaterBox_GetSurface1(play, &play->colCtx, posB->x, posB->z, &waterSurface, &waterBox) &&
                     waterSurface <= posA->y && posB->y <= waterSurface;

    if (BgCheck_EntityLineTest1(&play->colCtx, posA, posB, hit, poly, true, true, true, true, bgId) &&
        !(SurfaceType_GetWallFlags(&play->colCtx, *poly, *bgId) & WALL_FLAG_CRAWLSPACE) &&
        (!isOnWater || waterSurface <= hit->y)) {
        return true;
    }
    if (!isOnWater) {
        return false;
    }
    hit->x = posB->x;
    hit->y = waterSurface;
    hit->z = posB->z;
    *poly = NULL;
    *bgId = BGCHECK_SCENE;
    return true;
}

static void RealBombchu_LandOn(EnRealBombchu* this, PlayState* play, CollisionPoly* poly, Vec3f* pos, s32 bgId) {
    this->shouldRotateOntoSurfaces |= RealBombchu_UpdateFloorPoly(this, poly, play);
    this->actor.world.pos = *pos;
    this->actor.floorBgId = bgId;
}

// Off its surface it looks behind, left and right for one to grab.
static bool RealBombchu_GrabNearbySurface(EnRealBombchu* this, PlayState* play, Vec3f* origin, f32 reach) {
    Vec3f directions[3] = {
        { -this->axisForwards.x, -this->axisForwards.y, -this->axisForwards.z },
        this->axisLeft,
        { -this->axisLeft.x, -this->axisLeft.y, -this->axisLeft.z },
    };

    for (s32 i = 0; i < ARRAY_COUNT(directions); i++) {
        Vec3f posB = { origin->x + directions[i].x * reach, origin->y + directions[i].y * reach,
                       origin->z + directions[i].z * reach };
        CollisionPoly* poly = NULL;
        s32 bgId = BGCHECK_SCENE;
        Vec3f hit;

        if (RealBombchu_FindSurface(play, origin, &posB, &hit, &poly, &bgId)) {
            RealBombchu_LandOn(this, play, poly, &hit, bgId);
            return true;
        }
    }
    return false;
}

static bool RealBombchu_IsTouchingSurface(EnRealBombchu* this, PlayState* play) {
    CollisionPoly* polySide = NULL;
    CollisionPoly* polyUpDown = NULL;
    s32 bgIdSide = BGCHECK_SCENE;
    s32 bgIdUpDown = BGCHECK_SCENE;
    f32 lineLength = 2.0f * this->actor.speedXZ;
    Vec3f posSide;
    Vec3f posUpDown;
    Vec3f posA = { this->actor.world.pos.x + this->axisUp.x * 5.0f, this->actor.world.pos.y + this->axisUp.y * 5.0f,
                   this->actor.world.pos.z + this->axisUp.z * 5.0f };
    Vec3f posB = { this->actor.world.pos.x - this->axisUp.x * 4.0f, this->actor.world.pos.y - this->axisUp.y * 4.0f,
                   this->actor.world.pos.z - this->axisUp.z * 4.0f };

    if (!RealBombchu_FindSurface(play, &posA, &posB, &posUpDown, &polyUpDown, &bgIdUpDown)) {
        this->actor.speedXZ = 0.0f;
        return RealBombchu_GrabNearbySurface(this, play, &posB, lineLength * 3.0f);
    }
    Vec3f ahead = { posA.x + this->axisForwards.x * lineLength, posA.y + this->axisForwards.y * lineLength,
                    posA.z + this->axisForwards.z * lineLength };

    if (RealBombchu_FindSurface(play, &posA, &ahead, &posSide, &polySide, &bgIdSide)) {
        if (polySide != NULL && this->hasLostTrack) {
            return false;
        }
        RealBombchu_LandOn(this, play, polySide, &posSide, bgIdSide);
        this->actor.speedXZ = 0.0f;
        return true;
    }
    if (polyUpDown != this->actor.floorPoly) {
        this->shouldRotateOntoSurfaces |= RealBombchu_UpdateFloorPoly(this, polyUpDown, play);
    }
    this->actor.world.pos = posUpDown;
    this->actor.floorBgId = bgIdUpDown;
    return true;
}

// `offset` is in its own frame (x left, y up, z forwards), already at world scale.
static void RealBombchu_ActorCoordsToWorld(EnRealBombchu* this, Vec3f* offset, Vec3f* pos) {
    f32 x = offset->x + this->visualJitter;

    pos->x =
        this->actor.world.pos.x + this->axisLeft.x * x + this->axisUp.x * offset->y + this->axisForwards.x * offset->z;
    pos->y =
        this->actor.world.pos.y + this->axisLeft.y * x + this->axisUp.y * offset->y + this->axisForwards.y * offset->z;
    pos->z =
        this->actor.world.pos.z + this->axisLeft.z * x + this->axisUp.z * offset->y + this->axisForwards.z * offset->z;
}

static void RealBombchu_SpawnWaterEffects(EnRealBombchu* this, PlayState* play) {
    Vec3f splashPos = { this->actor.world.pos.x - this->axisForwards.x * 10.0f, this->actor.world.pos.y + 5.0f,
                        this->actor.world.pos.z - this->axisForwards.z * 10.0f };

    EffectSsGRipple_Spawn(play, &this->actor.world.pos, 70, 500, 0);
    EffectSsGSplash_Spawn(play, &splashPos, NULL, NULL, 1, 450);
}

// A platform carrying it may turn: the axes turn with it.
static void RealBombchu_RideDynaPoly(EnRealBombchu* this, PlayState* play) {
    s16 previousYaw = this->actor.shape.rot.y;

    func_800433A4(&play->colCtx, this->actor.floorBgId, &this->actor);
    if (previousYaw == this->actor.shape.rot.y) {
        return;
    }
    s16 turn = this->actor.shape.rot.y - previousYaw;
    f32 sin = Math_SinS(turn);
    f32 cos = Math_CosS(turn);
    Vec3f* axes[3] = { &this->axisForwards, &this->axisUp, &this->axisLeft };

    for (s32 i = 0; i < ARRAY_COUNT(axes); i++) {
        f32 x = axes[i]->x;

        axes[i]->x = sin * axes[i]->z + cos * x;
        axes[i]->z = cos * axes[i]->z - sin * x;
    }
}

static void RealBombchu_SetupIdle(EnRealBombchu* this) {
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sRealBombchuRunAnim);
    this->animLoopCounter = 5;
    this->actor.speedXZ = 2.0f;
    this->actionFunc = RealBombchu_Idle;
}

static void RealBombchu_SetupRevive(EnRealBombchu* this) {
    this->hasLostTrack = false;
    this->timer = 200;
    this->stunTimer = 0;
    this->hookState = REAL_BOMBCHU_NOT_HOOKED;
    this->actor.world.pos = this->actor.home.pos;
    this->actor.shape.yOffset = 0.0f;
    this->actor.shape.rot = this->actor.home.rot;
    RealBombchu_InitAxes(this);
    RealBombchu_UpdateRotation(this);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.speedXZ = 0.0f;
    Animation_PlayLoopSetSpeed(&this->skelAnime, (AnimationHeader*)sRealBombchuSpotAnim, 0.0f);
    this->reviveDepth = 2666.6667f;
    this->actor.draw = NULL;
    this->actor.shape.shadowDraw = NULL;
    this->actionFunc = RealBombchu_Revive;
}

// The overworld kind tunnels back up out of the ground at home.
static void RealBombchu_Revive(EnRealBombchu* this, PlayState* play) {
    if (this->timer > 0) {
        this->timer--;
        if (this->timer == 0) {
            this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
            this->actor.draw = EnRealBombchu_Draw;
            this->skelAnime.playSpeed = 1.0f;
        }
        return;
    }
    Math_StepToF(&this->reviveDepth, 0.0f, 666.6667f);
    if (Animation_OnFrame(&this->skelAnime, 2.0f)) {
        func_800286CC(play, &this->actor.world.pos, &this->axisUp, &sZeroVec, 600, 100);
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
    }
    if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
        this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        this->timer = 150;
        RealBombchu_SetupIdle(this);
    }
}

static void RealBombchu_Idle(EnRealBombchu* this, PlayState* play) {
    this->actor.speedXZ = 2.0f;
    if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_BOMCHU_WALK, NA_SE_IT_BOMBCHU_MOVE);
        if (this->animLoopCounter != 0) {
            this->animLoopCounter--;
        }
    }
    if (this->animLoopCounter == 0 && Rand_ZeroOne() < 0.05f) {
        PlaySfx(&this->actor, MM_SE_EN_BOMCHU_VOICE, NA_SE_IT_BOMBCHU_MOVE);
        this->animLoopCounter = 5;
    }
    if (this->actor.xzDistToPlayer < this->attackRange && Actor_IsFacingPlayer(&this->actor, 0x3800)) {
        this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sRealBombchuSpotAnim, -5.0f);
        this->animLoopCounter = 3;
        this->actor.speedXZ = 0.0f;
        this->actionFunc = RealBombchu_SpottedLink;
    }
}

static void RealBombchu_ShuffleSparks(EnRealBombchu* this) {
    for (s32 i = 0; i < ARRAY_COUNT(this->sparkOffsets); i++) {
        this->sparkOffsets[i].x = Rand_CenteredFloat(6.0f);
        this->sparkOffsets[i].y = Rand_CenteredFloat(6.0f);
        this->sparkOffsets[i].z = Rand_CenteredFloat(6.0f);
    }
}

// Squeaks over its catch a few times, then charges.
static void RealBombchu_SpottedLink(EnRealBombchu* this, PlayState* play) {
    if (this->animLoopCounter == 3 && Animation_OnFrame(&this->skelAnime, 5.0f)) {
        PlaySfx(&this->actor, MM_SE_EN_BOMCHU_AIM, NA_SE_IT_BOMBCHU_MOVE);
    }
    if (!Animation_OnFrame(&this->skelAnime, 0.0f)) {
        return;
    }
    this->animLoopCounter--;
    if (this->animLoopCounter == 0) {
        Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sRealBombchuRunAnim, -5.0f);
        this->actor.speedXZ = 6.1f;
        RealBombchu_ShuffleSparks(this);
        this->actionFunc = RealBombchu_Chase;
    }
}

static void RealBombchu_TrailEffects(EnRealBombchu* this, PlayState* play) {
    if (!this->isOverworld) {
        Vec3f blureP1;

        RealBombchu_ActorCoordsToWorld(this, &sBlureP1Offset, &blureP1);
        for (s32 i = 0; i < ARRAY_COUNT(this->blureIndices); i++) {
            Vec3f blureP2;

            RealBombchu_ActorCoordsToWorld(this, &sBlureP2Offsets[i], &blureP2);
            EffectBlure_AddVertex(Effect_GetByIndex(this->blureIndices[i]), &blureP1, &blureP2);
        }
    } else if (this->actor.floorPoly != NULL && (play->gameplayFrames % 4) == 0) {
        func_800286CC(play, &this->actor.world.pos, &sDustVelocity, &sZeroVec, 550, 50);
    }
    if (this->actor.floorPoly == NULL && Animation_OnFrame(&this->skelAnime, 0.0f)) {
        RealBombchu_SpawnWaterEffects(this, play);
    }
}

// The dungeon kind's fuse never runs out; the overworld kind's burns down 150 frames into the chase.
static void RealBombchu_Chase(EnRealBombchu* this, PlayState* play) {
    this->actor.speedXZ = 6.1f;
    if (this->hasLostTrack && Actor_IsFacingPlayer(&this->actor, 0x3000)) {
        this->hasLostTrack = false;
    }
    if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
        if (this->animLoopCounter != 0) {
            this->animLoopCounter--;
        }
        PlaySfx(&this->actor, MM_SE_EN_BOMCHU_WALK, NA_SE_IT_BOMBCHU_MOVE);
    }
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 0 && !this->isOverworld) {
        this->timer = 30;
    }
    func_8002836C(play, &this->smokePos, &sZeroVec, &sSmokeAccel, &sSmokeColor, &sSmokeColor, 75, 7, 8);
    this->visualJitter =
        (5.0f + Rand_ZeroOne() * 3.0f) * Math_SinS((s16)((Rand_ZeroOne() * 0x200 + 0x3000) * this->timer));
    RealBombchu_TrailEffects(this, play);
    if (this->animLoopCounter == 0 && Rand_ZeroOne() < 0.05f) {
        PlaySfx(&this->actor, MM_SE_EN_BOMCHU_AIM, NA_SE_IT_BOMBCHU_MOVE);
        this->animLoopCounter = 5;
    }
    PlaySfx(&this->actor, MM_SE_EN_BOMCHU_RUN - SFX_FLAG, NA_SE_IT_BOMBCHU_MOVE - SFX_FLAG);
    RealBombchu_ShuffleSparks(this);
}

static void RealBombchu_SetupBounced(EnRealBombchu* this) {
    this->actor.speedXZ = 5.0f;
    this->actor.velocity.y = 8.0f;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer + 0x8000;
    this->actor.gravity = -1.0f;
    RealBombchu_ShuffleSparks(this);
    this->actor.bgCheckFlags &= ~(BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL | BGCHECKFLAG_CEILING);
    this->actionFunc = RealBombchu_Bounced;
}

// Knocked back off a shield, it somersaults and blows up on whatever it lands against.
static void RealBombchu_Bounced(EnRealBombchu* this, PlayState* play) {
    this->actor.shape.rot.x -= 0x700;
    Math_StepToF(&this->actor.shape.yOffset, 1700.0f, 170.0f);
    this->timer--;
    if (this->timer == 0) {
        this->timer = 30;
    }
    if (this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL | BGCHECKFLAG_CEILING)) {
        RealBombchu_Explode(this, play);
    }
}

static void RealBombchu_Explode(EnRealBombchu* this, PlayState* play) {
    EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, this->actor.world.pos.x,
                                      this->actor.world.pos.y, this->actor.world.pos.z, 0, 0, 0, BOMB_BODY);

    if (bomb != NULL) {
        bomb->timer = 0;
    }
    if (this->isOverworld) {
        Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x70);
    } else {
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    }
    this->actor.speedXZ = 0.0f;
    this->actionFunc = RealBombchu_PostDetonation;
}

static void RealBombchu_PostDetonation(EnRealBombchu* this, PlayState* play) {
    if (this->isOverworld) {
        RealBombchu_SetupRevive(this);
    } else {
        Actor_Kill(&this->actor);
    }
}

static bool RealBombchu_IsBumpedIntoFoe(EnRealBombchu* this) {
    Actor* other = this->collider.base.oc;

    if (!(this->collider.base.ocFlags1 & OC1_HIT) || other == NULL) {
        return false;
    }
    return other->category == ACTORCAT_ENEMY || other->category == ACTORCAT_BOSS || other->category == ACTORCAT_PLAYER;
}

static void RealBombchu_ReactToHit(EnRealBombchu* this, PlayState* play) {
    this->collider.base.acFlags &= ~AC_HIT;
    if (this->actor.colChkInfo.damageEffect == REAL_BOMBCHU_DMGEFF_HOOKSHOT) {
        this->hookState = REAL_BOMBCHU_HOOK_STARTED;
        return;
    }
    if (this->actor.colChkInfo.damageEffect != REAL_BOMBCHU_DMGEFF_STUN) {
        RealBombchu_Explode(this, play);
        return;
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
    Actor_SetColorFilter(&this->actor, COLORFILTER_BLUE, 120, COLORFILTER_OPA, 40);
    if (this->actionFunc != RealBombchu_Bounced) {
        this->stunTimer = 40;
        return;
    }
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = MIN(this->actor.velocity.y, 0.0f);
}

// Returns true once it has blown up this frame.
static bool RealBombchu_CheckCollisions(EnRealBombchu* this, PlayState* play) {
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        this->collider.base.acFlags &= ~AC_HIT;
        this->collider.base.ocFlags1 &= ~OC1_HIT;
        if (this->collider.base.atFlags & AT_BOUNCED) {
            RealBombchu_SetupBounced(this);
            return false;
        }
        RealBombchu_Explode(this, play);
        return true;
    }
    if (this->collider.base.acFlags & AC_HIT) {
        RealBombchu_ReactToHit(this, play);
        return this->actionFunc == RealBombchu_PostDetonation;
    }
    bool hasFuseRunOut = this->actionFunc == RealBombchu_Chase && this->timer == 0;

    if (RealBombchu_IsBumpedIntoFoe(this) || hasFuseRunOut) {
        this->collider.base.ocFlags1 &= ~OC1_HIT;
        RealBombchu_Explode(this, play);
        return true;
    }
    return false;
}

// The hookshot reels it in; once it lets go, the Real Bombchu goes off.
static bool RealBombchu_UpdateHook(EnRealBombchu* this, PlayState* play) {
    if (this->hookState == REAL_BOMBCHU_HOOK_STARTED) {
        this->hookState = REAL_BOMBCHU_HOOKED;
        return false;
    }
    if (!CHECK_FLAG_ALL(this->actor.flags, ACTOR_FLAG_HOOKSHOT_ATTACHED)) {
        RealBombchu_Explode(this, play);
        return true;
    }
    return false;
}

// Returns true once it has blown up this frame.
static bool RealBombchu_Crawl(EnRealBombchu* this, PlayState* play) {
    if (this->actor.floorBgId != BGCHECK_SCENE) {
        RealBombchu_RideDynaPoly(this, play);
    }
    if (!this->hasLostTrack) {
        RealBombchu_ChooseDirection(this);
    }
    bool isOffSurface = this->actionFunc != RealBombchu_SpottedLink && !RealBombchu_IsTouchingSurface(this, play);

    if (this->actionFunc == RealBombchu_PostDetonation) {
        return true;
    }
    if (isOffSurface) {
        RealBombchu_Explode(this, play);
        return true;
    }
    if (this->shouldRotateOntoSurfaces) {
        RealBombchu_UpdateRotation(this);
        this->actor.shape.rot.x = -this->actor.world.rot.x;
        this->actor.shape.rot.y = this->actor.world.rot.y;
        this->actor.shape.rot.z = this->actor.world.rot.z;
    }
    Actor_MoveXYZ(&this->actor);
    this->actor.floorHeight = this->actor.world.pos.y;
    return false;
}

// Returns true once it has blown up this frame.
static bool RealBombchu_Move(EnRealBombchu* this, PlayState* play) {
    if (this->stunTimer > 0) {
        this->stunTimer--;
        return false;
    }
    if (this->hookState != REAL_BOMBCHU_NOT_HOOKED) {
        return RealBombchu_UpdateHook(this, play);
    }
    if (this->actionFunc != RealBombchu_Bounced) {
        return RealBombchu_Crawl(this, play);
    }
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 30.0f, 60.0f, BGCHECK_UPDATE_WALL_CEILING_FLOOR);
    return false;
}

static void RealBombchu_SubmitCollider(EnRealBombchu* this, PlayState* play) {
    ColliderJntSphElementDim* dim = &this->collider.elements[0].dim;

    dim->worldSphere.center.x = this->actor.world.pos.x + this->axisUp.x * 10.0f;
    dim->worldSphere.center.y = this->actor.world.pos.y + this->axisUp.y * 10.0f;
    dim->worldSphere.center.z = this->actor.world.pos.z + this->axisUp.z * 10.0f;
    if (this->stunTimer == 0) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void EnRealBombchu_Init(Actor* thisx, PlayState* play) {
    static EffectBlureInit2 sBlureInit = {
        .calcMode = 0,
        .flags = 0,
        .p1StartColor = { 250, 0, 0, 250 },
        .p2StartColor = { 200, 0, 0, 130 },
        .p1EndColor = { 150, 0, 0, 100 },
        .p2EndColor = { 100, 0, 0, 50 },
        .elemDuration = 16,
        .drawMode = 0,
    };
    EnRealBombchu* this = (EnRealBombchu*)thisx;
    s32 range = this->actor.params & 0xFF;

    this->actor.targetArrowOffset = 5000.0f;
    Actor_SetScale(&this->actor, 0.015f);
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sColliderInit, this->colliderElements);
    this->collider.elements[0].dim.worldSphere.radius = REAL_BOMBCHU_RADIUS;
    this->isOverworld = (this->actor.params & REAL_BOMBCHU_OVERWORLD) != 0;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sRealBombchuSkel,
                       (AnimationHeader*)sRealBombchuRunAnim, this->jointTable, this->morphTable,
                       REAL_BOMBCHU_LIMB_MAX);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 25.0f);
    this->blureIndices[0] = -1;
    this->blureIndices[1] = -1;
    if (this->isOverworld) {
        this->timer = 150;
    } else {
        Effect_Add(play, &this->blureIndices[0], EFFECT_BLURE2, 0, 0, &sBlureInit);
        Effect_Add(play, &this->blureIndices[1], EFFECT_BLURE2, 0, 0, &sBlureInit);
        this->timer = 30;
    }
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    RealBombchu_InitAxes(this);
    RealBombchu_UpdateRotation(this);
    if (range == 0xFF || range == 0) {
        this->attackRange = 350.0f;
    } else {
        this->attackRange = range * (this->isOverworld ? 0.5f : 0.1f) * 40.0f;
    }
    RealBombchu_SetupIdle(this);
}

static void EnRealBombchu_Destroy(Actor* thisx, PlayState* play) {
    EnRealBombchu* this = (EnRealBombchu*)thisx;

    for (s32 i = 0; i < ARRAY_COUNT(this->blureIndices); i++) {
        if (this->blureIndices[i] >= 0) {
            Effect_Delete(play, this->blureIndices[i]);
        }
    }
    Collider_DestroyJntSph(play, &this->collider);
}

static void EnRealBombchu_Update(Actor* thisx, PlayState* play) {
    EnRealBombchu* this = (EnRealBombchu*)thisx;

    this->shouldRotateOntoSurfaces = false;
    if (this->stunTimer == 0) {
        SkelAnime_Update(&this->skelAnime);
    }
    if (RealBombchu_CheckCollisions(this, play)) {
        return;
    }
    this->actionFunc(this, play);
    bool isOutOfPlay = this->actionFunc == RealBombchu_PostDetonation || this->actionFunc == RealBombchu_Revive;

    if (isOutOfPlay || RealBombchu_Move(this, play)) {
        return;
    }
    if (SurfaceType_IsWallDamage(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId)) {
        RealBombchu_Explode(this, play);
        return;
    }
    RealBombchu_SubmitCollider(this, play);
    Actor_SetFocus(&this->actor, this->actor.shape.yOffset * 0.015f);
}

static s32 RealBombchu_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                        void* thisx) {
    EnRealBombchu* this = (EnRealBombchu*)thisx;

    if (limbIndex == REAL_BOMBCHU_LIMB_BODY) {
        pos->y -= this->reviveDepth;
    }
    if (limbIndex == REAL_BOMBCHU_LIMB_TAIL_END) {
        *dList = NULL;
    }
    return false;
}

static void RealBombchu_DrawSparks(EnRealBombchu* this, PlayState* play, MtxF* fuse) {
    MtxF spark = *fuse;

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 150, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, 0);
    for (s32 i = 0; i < ARRAY_COUNT(this->sparkOffsets); i++) {
        spark.xw = this->smokePos.x + this->sparkOffsets[i].x;
        spark.yw = this->smokePos.y + this->sparkOffsets[i].y;
        spark.zw = this->smokePos.z + this->sparkOffsets[i].z;
        Matrix_Put(&spark);
        Matrix_Scale(REAL_BOMBCHU_SPARK_SCALE, REAL_BOMBCHU_SPARK_SCALE, REAL_BOMBCHU_SPARK_SCALE, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sSparkTextures[(play->gameplayFrames + i) & 3]);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffSparkDL);
    }
    Matrix_Put(fuse);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The bomb on its back flashes red, faster as the overworld kind's fuse burns down.
static f32 RealBombchu_GetFlash(EnRealBombchu* this) {
    s32 period = 30;

    if (this->isOverworld && this->timer < 30) {
        period = 3;
    } else if (this->isOverworld && this->timer < 120) {
        period = 6;
    }
    f32 phase = this->isOverworld ? (this->timer % period) : this->timer;

    return fabsf(Math_CosF(phase * (M_PI / period)));
}

static void RealBombchu_DrawBomb(EnRealBombchu* this, PlayState* play) {
    MtxF fuse;
    s32 red = (s32)((1.0f - RealBombchu_GetFlash(this)) * 255.0f);

    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_MultVec3f(&sZeroVec, &this->smokePos);
    this->smokePos.y += 15.0f;
    Matrix_Get(&fuse);
    if (this->actionFunc == RealBombchu_Chase) {
        RealBombchu_DrawSparks(this, play, &fuse);
    }
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBombCapDL);
    gDPSetEnvColor(POLY_OPA_DISP++, red, 0, 40, 255);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, red, 0, 40, 255);
    Matrix_RotateZYX(0x4000, 0, 0, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBombBodyDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RealBombchu_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    if (limbIndex == REAL_BOMBCHU_LIMB_TAIL_END) {
        RealBombchu_DrawBomb((EnRealBombchu*)thisx, play);
    }
}

static void EnRealBombchu_Draw(Actor* thisx, PlayState* play) {
    EnRealBombchu* this = (EnRealBombchu*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_60NoCDXlu(play->state.gfxCtx);
    func_8002EBCC(&this->actor, play, 0);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          RealBombchu_OverrideLimbDraw, RealBombchu_PostLimbDraw, this);
}

// MM's CanKillEnemy(ACTOR_EN_RAT) is always true: it blows itself up on Link. No logic callback needed.
// Soulless, as in 2Ship: it dies to any collision, so a missing soul could never protect it.
static void RegisterRealBombchu(void) {
    SOHActorDefinition realBombchu = { 0 };

    realBombchu.structSize = sizeof(realBombchu);
    realBombchu.key = REAL_BOMBCHU_KEY;
    realBombchu.description = "Real Bombchu";
    realBombchu.category = ACTORCAT_ENEMY;
    realBombchu.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR;
    realBombchu.objectId = OBJECT_GAMEPLAY_KEEP;
    realBombchu.instanceSize = sizeof(EnRealBombchu);
    realBombchu.init = EnRealBombchu_Init;
    realBombchu.destroy = EnRealBombchu_Destroy;
    realBombchu.update = EnRealBombchu_Update;
    realBombchu.draw = EnRealBombchu_Draw;
    realBombchu.enemyFlags = SOH_ACTOR_ENEMY;
    realBombchu.enemyParams = 0xFF;
    realBombchu.naviHint = "Real Bombchu&%cA rat with a bomb on its back! It runs along walls and ceilings to blow "
                           "up on you. Hit it from afar!%w";
    realBombchu.soulKey = NULL;
    if (sApi->RegisterActor(&realBombchu) < 0) {
        sApi->Log("Could not register " REAL_BOMBCHU_KEY);
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterActor)) {
        return;
    }
    RegisterRealBombchu();
}
