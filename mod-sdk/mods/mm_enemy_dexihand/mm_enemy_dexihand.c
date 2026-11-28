// Majora's Mask's Dexihand (En_Wdhand) as an Unbound custom enemy, drawn from mm.o2r.
// It anchors itself to the floor under the nearest water, grabs Link by the foot and hurls him away.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define DEXIHAND_KEY "mm.dexihand"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_WDHAND "__OTR__objects/object_wdhand/"

#define MM_SE_EN_HANDW_GET 0x39FC
#define MM_SE_EN_HANDW_RELEASE 0x39FD

#define DEXIHAND_LIMB_MAX 8
#define DEXIHAND_SEGMENTS 3
#define DEXIHAND_HAND DEXIHAND_SEGMENTS
#define DEXIHAND_COLLIDER_COUNT (2 * DEXIHAND_SEGMENTS + 1)
#define DEXIHAND_HAND_COLLIDER (DEXIHAND_COLLIDER_COUNT - 1)
#define DEXIHAND_CUT_POINT 0
#define DEXIHAND_HAND_POINT 1
#define DEXIHAND_THROW_PARAMS 0x3FFF
#define DEXIHAND_THROW_VELOCITY_MAX 0x7F
#define DEXIHAND_REACH 120.75f
#define DEXIHAND_MIN_DEPTH 60.0f
#define WATER_SEARCH_STEP 100.0f
#define WATER_SEARCH_RADIUS 800.0f
#define WATER_SEARCH_ANGLES 12
#define GRAB_RELEASE_STRUGGLE 200

struct EnDexihand;

typedef void (*EnDexihandActionFunc)(struct EnDexihand*, PlayState*);

typedef struct EnDexihand {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[DEXIHAND_LIMB_MAX];
    Vec3s morphTable[DEXIHAND_LIMB_MAX];
    EnDexihandActionFunc actionFunc;
    s16 limbIndexBeforeCut;
    s16 limbIndexAfterCut;
    s16 timer;
    Vec3s limbRotations[DEXIHAND_SEGMENTS + 1];
    f32 globalLimbScale;
    f32 limbScales[DEXIHAND_SEGMENTS];
    MtxF relativeToWorld;
    Vec3f endPoints[2];
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[DEXIHAND_COLLIDER_COUNT];
} EnDexihand;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sDexihandSkel[] = OBJ_WDHAND "gDexihandSkel";
static const ALIGN_ASSET(2) char sDexihandIdleAnim[] = OBJ_WDHAND "gDexihandIdleAnim";
static const ALIGN_ASSET(2) char sDexihandOpenAnim[] = OBJ_WDHAND "gDexihandOpenAnim";
static const ALIGN_ASSET(2) char sDexihandCloseAnim[] = OBJ_WDHAND "gDexihandCloseAnim";
static const ALIGN_ASSET(2) char sDexihandLetGoAnim[] = OBJ_WDHAND "object_wdhand_Anim_000364";
static const ALIGN_ASSET(2) char sDexihandBaseDL[] = OBJ_WDHAND "gDexihandBaseDL";
static const ALIGN_ASSET(2) char sDexihandArmSegmentDL[] = OBJ_WDHAND "gDexihandArmSegmentDL";

#define DEXIHAND_ELEMENT_INFO                                                                                      \
    {                                                                                                              \
        ELEMTYPE_UNK1, { 0xF7CFFFFF, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NONE, BUMP_ON, \
            OCELEM_ON                                                                                              \
    }

static ColliderJntSphElementInit sJntSphElementsInit[DEXIHAND_COLLIDER_COUNT] = {
    { DEXIHAND_ELEMENT_INFO, { 50, { { 0, 575, 0 }, 10 }, 100 } },
    { DEXIHAND_ELEMENT_INFO, { 50, { { 0, 1725, 0 }, 10 }, 100 } },
    { DEXIHAND_ELEMENT_INFO, { 50, { { 0, 575, 0 }, 10 }, 100 } },
    { DEXIHAND_ELEMENT_INFO, { 50, { { 0, 1725, 0 }, 10 }, 100 } },
    { DEXIHAND_ELEMENT_INFO, { 50, { { 0, 575, 0 }, 10 }, 100 } },
    { DEXIHAND_ELEMENT_INFO, { 50, { { 0, 1725, 0 }, 10 }, 100 } },
    { DEXIHAND_ELEMENT_INFO, { 50, { { 0, 1000, 0 }, 15 }, 100 } },
};

static ColliderJntSphInit sJntSphInit = {
    { COLTYPE_HIT0, AT_NONE | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_JNTSPH },
    DEXIHAND_COLLIDER_COUNT,
    sJntSphElementsInit,
};

static CollisionCheckInfoInit sColChkInfoInit = { 1, 25, 25, MASS_HEAVY };

// MM only lets Zora boomerangs and blasts through; OoT's underwater arsenal (sword, hookshot) counts too.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, 0x0),
    /* Deku stick    */ DMG_ENTRY(1, 0x0),
    /* Slingshot     */ DMG_ENTRY(1, 0x0),
    /* Explosive     */ DMG_ENTRY(1, 0x0),
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
    /* Hammer jump   */ DMG_ENTRY(1, 0x0),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

static Vec3f sBubbleAccel = { 0.0f, 0.05f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;

static void Dexihand_Idle(EnDexihand* this, PlayState* play);
static void Dexihand_LungeForPlayer(EnDexihand* this, PlayState* play);
static void Dexihand_FailedToGrabPlayer(EnDexihand* this, PlayState* play);
static void Dexihand_ReturnToIdle(EnDexihand* this, PlayState* play);
static void Dexihand_GrabbedPlayer(EnDexihand* this, PlayState* play);
static void Dexihand_Die(EnDexihand* this, PlayState* play);

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

static void MultVecY(f32 y, Vec3f* dest) {
    Vec3f src = { 0.0f, y, 0.0f };

    Matrix_MultVec3f(&src, dest);
}

static void RotateYS(s16 yaw) {
    Matrix_RotateY(BINANG_TO_RAD(yaw), MTXMODE_APPLY);
}

static void SetCurrentTranslation(Vec3f* translation) {
    MtxF current;

    Matrix_Get(&current);
    current.xw = translation->x;
    current.yw = translation->y;
    current.zw = translation->z;
    Matrix_Put(&current);
}

// The first floor below the probe must lie under the surface, or the water box belongs to a lower floor.
static bool FindUnderwaterFloor(PlayState* play, f32 x, f32 z, f32 originY, Vec3f* floorPos) {
    f32 surface;
    WaterBox* waterBox;

    if (!WaterBox_GetSurface1(play, &play->colCtx, x, z, &surface, &waterBox)) {
        return false;
    }
    CollisionPoly* poly;
    Vec3f probe = { x, MAX(surface, originY) + 50.0f, z };
    f32 floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);

    if (floorY <= BGCHECK_Y_MIN || surface - floorY < DEXIHAND_MIN_DEPTH) {
        return false;
    }
    floorPos->x = x;
    floorPos->y = floorY;
    floorPos->z = z;
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

// Rings outward from the spawn for the closest floor under deep enough water; stays put if there is none.
static void Dexihand_AnchorUnderWater(EnDexihand* this, PlayState* play) {
    Vec3f origin = this->actor.world.pos;
    Vec3f anchor;

    if (FindUnderwaterFloor(play, origin.x, origin.z, origin.y, &anchor)) {
        this->actor.world.pos = anchor;
        this->actor.home.pos = anchor;
        return;
    }
    for (f32 radius = WATER_SEARCH_STEP; radius <= WATER_SEARCH_RADIUS; radius += WATER_SEARCH_STEP) {
        for (s32 step = 0; step < WATER_SEARCH_ANGLES; step++) {
            s16 angle = step * (0x10000 / WATER_SEARCH_ANGLES);
            f32 x = origin.x + Math_SinS(angle) * radius;
            f32 z = origin.z + Math_CosS(angle) * radius;

            if (FindUnderwaterFloor(play, x, z, origin.y, &anchor) && !IsWallBetween(play, &origin, &anchor)) {
                this->actor.world.pos = anchor;
                this->actor.home.pos = anchor;
                return;
            }
        }
    }
}

static f32 Dexihand_GetThrowVelocity(s32 param) {
    return (param == DEXIHAND_THROW_VELOCITY_MAX ? 40 : param) * 0.2f;
}

// The stored transform is transposed while idle: it then maps world offsets into the hand's own frame.
static void Dexihand_GetRotationsToPlayerFoot(EnDexihand* this, Player* player, s16* pitch, s16* yaw) {
    Vec3f diff;
    Vec3f relative;

    Math_Vec3f_Diff(&player->bodyPartsPos[PLAYER_BODYPART_R_FOOT], &this->actor.world.pos, &diff);
    Matrix_Put(&this->relativeToWorld);
    Matrix_MultVec3f(&diff, &relative);
    *yaw = Actor_WorldYawTowardPoint(&this->actor, &relative);
    *pitch = Actor_WorldPitchTowardPoint(&this->actor, &relative) + 0x4000;
}

static void Dexihand_SetSegmentTransform(EnDexihand* this, s32 segment, bool isYawUndone, Vec3f* translation) {
    Vec3s* rot = &this->limbRotations[segment];

    Matrix_RotateZYX(rot->x, rot->y, 0, MTXMODE_APPLY);
    if (isYawUndone) {
        RotateYS(-rot->y);
    }
    if (segment == DEXIHAND_HAND) {
        if (this->globalLimbScale > 1.0f) {
            f32 scale = CLAMP_MAX(this->globalLimbScale, 1.5f);

            Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        }
    } else if (this->limbScales[segment] < 1.0f || this->globalLimbScale > 1.0f) {
        f32 length = this->globalLimbScale * this->limbScales[segment];
        f32 width = this->limbScales[segment] < 0.1f ? 0.0f : 1.0f;

        Matrix_Scale(width, length, width, MTXMODE_APPLY);
    }
    SetCurrentTranslation(translation);
}

static s16 Dexihand_GetSwayPitch(EnDexihand* this, s32 segment) {
    return this->limbRotations[segment].z * Math_SinF((this->timer - segment * 20) * (M_PI / 40));
}

static void Dexihand_SetupIdle(EnDexihand* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sDexihandIdleAnim, -5.0f);
    if (this->actionFunc != Dexihand_ReturnToIdle) {
        this->timer = 80;
        this->limbRotations[0].y = Rand_Next() >> 16;
        for (s32 i = 1; i <= DEXIHAND_SEGMENTS; i++) {
            this->limbRotations[i].y = this->limbRotations[i - 1].y + (s32)Rand_ZeroFloat(0x2000) + 0x8000;
        }
        for (s32 i = 0; i <= DEXIHAND_SEGMENTS; i++) {
            this->limbRotations[i].z = Rand_S16Offset(0x1800, 0x1000);
        }
    }
    for (s32 i = 0; i <= DEXIHAND_SEGMENTS; i++) {
        this->limbRotations[i].x = Dexihand_GetSwayPitch(this, i);
    }
    this->globalLimbScale = 1.0f;
    this->actionFunc = Dexihand_Idle;
}

static void Dexihand_SetupLungeForPlayer(EnDexihand* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sDexihandOpenAnim, -5.0f);
    this->timer = 5;
    this->actionFunc = Dexihand_LungeForPlayer;
}

// Sways each arm segment in turn and lunges once Link's foot comes within reach in front of it.
static void Dexihand_Idle(EnDexihand* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);
    this->timer--;
    for (s32 i = 0; i <= DEXIHAND_SEGMENTS; i++) {
        this->limbRotations[i].x = Dexihand_GetSwayPitch(this, i);
        if (this->timer != 20 * i) {
            continue;
        }
        if (i != 0) {
            this->limbRotations[i].y = this->limbRotations[i - 1].y + (s32)Rand_CenteredFloat(16384.0f) - 0x8000;
        } else {
            this->limbRotations[0].y += (s16)(s32)Rand_CenteredFloat(16384.0f);
        }
        this->limbRotations[i].z = Rand_S16Offset(0x1800, 0x1000);
    }
    if (this->timer == 0) {
        this->timer = 80;
    }
    bool isPlayerFree = !(player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY);

    if (!isPlayerFree || this->actor.xyzDistToPlayerSq >= SQ(DEXIHAND_REACH)) {
        return;
    }
    s16 pitch;
    s16 yaw;

    Dexihand_GetRotationsToPlayerFoot(this, player, &pitch, &yaw);
    if (ABS(pitch) <= 0x4000) {
        Dexihand_SetupLungeForPlayer(this);
    }
}

static void Dexihand_SetupGrabbedPlayer(EnDexihand* this, PlayState* play);

static void Dexihand_SetupFailedToGrabPlayer(EnDexihand* this) {
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sDexihandCloseAnim);
    this->collider.base.atFlags &= ~(AT_ON | AT_HIT);
    this->globalLimbScale = 1.5f;
    this->actionFunc = Dexihand_FailedToGrabPlayer;
}

static void Dexihand_LungeForPlayer(EnDexihand* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);
    if (this->timer > 0) {
        this->timer--;
        if (this->timer == 0) {
            this->collider.base.atFlags |= AT_ON;
        }
        return;
    }
    Math_StepToF(&this->globalLimbScale, 1.5f, 0.05f);
    s16 pitch;
    s16 yaw;
    bool areAllStepsDone = true;

    Dexihand_GetRotationsToPlayerFoot(this, player, &pitch, &yaw);
    for (s32 i = 0; i <= DEXIHAND_SEGMENTS; i++) {
        bool isFacingFoot = ABS((s16)(yaw - this->limbRotations[i].y)) < 0x4000;
        s16 stepX = (i * 0.1f + 1.0f) * 1920.0f;

        areAllStepsDone &= Math_ScaledStepToS(&this->limbRotations[i].x, isFacingFoot ? pitch : -pitch, stepX);
        areAllStepsDone &= Math_ScaledStepToS(&this->limbRotations[i].y, isFacingFoot ? yaw : yaw + 0x8000, 0x800);
    }
    bool hasTouchedPlayer = (this->collider.base.atFlags & AT_HIT) != 0;

    this->collider.base.atFlags &= ~AT_HIT;
    if (hasTouchedPlayer && play->grabPlayer(play, player)) {
        Dexihand_SetupGrabbedPlayer(this, play);
    } else if (areAllStepsDone) {
        Dexihand_SetupFailedToGrabPlayer(this);
    }
}

static void Dexihand_SetupReturnToIdle(EnDexihand* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sDexihandIdleAnim, -10.0f);
    for (s32 i = 0; i <= DEXIHAND_SEGMENTS; i++) {
        this->limbRotations[i].z = Rand_S16Offset(0x1800, 0x1000);
    }
    this->timer = 80;
    this->actionFunc = Dexihand_ReturnToIdle;
}

static void Dexihand_FailedToGrabPlayer(EnDexihand* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        Dexihand_SetupReturnToIdle(this);
    }
}

static void Dexihand_ReturnToIdle(EnDexihand* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->timer--;
    bool areAllStepsDone = Math_ScaledStepToS(&this->limbRotations[0].x, Dexihand_GetSwayPitch(this, 0), 0x200);

    for (s32 i = 1; i <= DEXIHAND_SEGMENTS; i++) {
        areAllStepsDone &= Math_ScaledStepToS(&this->limbRotations[i].x, Dexihand_GetSwayPitch(this, i), 0x300);
        if (this->timer == 20 * i) {
            this->limbRotations[i].z = Rand_S16Offset(0x1800, 0x1000);
        }
    }
    if (this->timer == 0) {
        this->timer = 80;
        this->limbRotations[0].z = Rand_S16Offset(0x1800, 0x1000);
    }
    Math_StepToF(&this->globalLimbScale, 1.0f, 0.05f);
    if (areAllStepsDone) {
        Dexihand_SetupIdle(this);
    }
}

static void Dexihand_SetupGrabbedPlayer(EnDexihand* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->collider.base.atFlags &= ~(AT_ON | AT_HIT);
    this->collider.base.ocFlags1 &= ~OC1_ON;
    Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sDexihandCloseAnim);
    for (s32 i = 0; i <= DEXIHAND_SEGMENTS; i++) {
        this->limbRotations[i].z = Rand_S16Offset(0x2000, 0x1000);
    }
    this->timer = 80;
    player->actor.parent = &this->actor;
    Matrix_Transpose(&this->relativeToWorld);
    this->globalLimbScale = 1.5f;
    PlaySfx(&this->actor, MM_SE_EN_HANDW_GET, NA_SE_EN_DEADHAND_GRIP);
    this->actionFunc = Dexihand_GrabbedPlayer;
}

// OoT's grabbed action lets go once its struggle counter passes 100; the knockback then flings Link.
static void Dexihand_ReleasePlayer(EnDexihand* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    player->av2.actionVar2 = GRAB_RELEASE_STRUGGLE;
    player->stateFlags2 &= ~PLAYER_STATE2_GRABBED_BY_ENEMY;
    player->actor.parent = NULL;
    player->actor.shape.rot.x = 0;
    player->actor.shape.rot.z = 0;
}

static void Dexihand_ShakePlayer(EnDexihand* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f limbPos;
    MtxF hand;

    Matrix_Put(&this->relativeToWorld);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MultVecY(300.0f, &limbPos);
    for (s32 i = 0; i < DEXIHAND_SEGMENTS; i++) {
        Matrix_Push();
        Dexihand_SetSegmentTransform(this, i, false, &limbPos);
        MultVecY(2300.0f, &limbPos);
        Matrix_Pop();
    }
    Dexihand_SetSegmentTransform(this, DEXIHAND_HAND, true, &limbPos);
    Matrix_Get(&hand);
    Matrix_MtxFToYXZRotS(&hand, &player->actor.shape.rot, 0);
    if (this->globalLimbScale > 1.0f) {
        f32 openness = CLAMP_MAX(this->globalLimbScale, 1.5f);

        Matrix_RotateZ(BINANG_TO_RAD((s16)((openness - 1.0f) * -32768.0f)), MTXMODE_APPLY);
    }
    MultVecY(1000.0f, &player->actor.world.pos);
    if (this->timer == 4 && player->actor.parent == &this->actor) {
        Dexihand_ReleasePlayer(this, play);
        Matrix_Transpose(&this->relativeToWorld);
        player->actor.world.pos.x += 2.0f * this->actor.velocity.x;
        player->actor.world.pos.y += 2.0f * this->actor.velocity.y;
        player->actor.world.pos.z += 2.0f * this->actor.velocity.z;
        Actor_SetPlayerKnockbackLarge(play, &this->actor, this->actor.speedXZ, this->actor.world.rot.y,
                                      this->actor.velocity.y, 0);
        PlaySfx(&this->actor, MM_SE_EN_HANDW_RELEASE, NA_SE_EN_DEADHAND_HAND_AT);
    } else if (this->timer == 2) {
        Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sDexihandLetGoAnim);
    }
}

static void Dexihand_GrabbedPlayer(EnDexihand* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);
    this->timer--;
    player->av2.actionVar2 = 0;
    s32 t = this->timer;

    for (s32 i = 0; i <= DEXIHAND_SEGMENTS; i++, t += 2) {
        s16 sway = this->limbRotations[i].z * Math_SinF(t * (M_PI / 8));

        if (this->timer < 76) {
            this->limbRotations[i].x = sway;
        } else {
            Math_ScaledStepToS(&this->limbRotations[i].x, sway, 0x400);
        }
        if (t % 16 != 0) {
            continue;
        }
        if (t == 16) {
            this->limbRotations[i].y = 0;
        } else if (i != 0) {
            this->limbRotations[i].y = this->limbRotations[i - 1].y + (s32)Rand_CenteredFloat(12288.0f);
        } else {
            this->limbRotations[0].y += (s16)(s32)Rand_CenteredFloat(12288.0f);
        }
        this->limbRotations[i].z = Rand_S16Offset(0x2000, 0x1000);
    }
    if (this->timer < 4) {
        Math_StepToF(&this->globalLimbScale, 1.5f, 1.0f / 8);
    } else if (this->timer < 16) {
        Math_StepToF(&this->globalLimbScale, 2.0f, 1.0f / 24);
    }
    if (this->timer == 0) {
        this->collider.base.ocFlags1 |= OC1_ON;
        Dexihand_SetupReturnToIdle(this);
    } else if (this->timer >= 4) {
        Dexihand_ShakePlayer(this, play);
    }
}

static bool Dexihand_ShrinkSegment(EnDexihand* this, s32 segment) {
    this->limbScales[segment] -= 0.1f;
    if (this->limbScales[segment] > 0.05f) {
        return false;
    }
    this->limbScales[segment] = 0.05f;
    return true;
}

static void Dexihand_SetupDie(EnDexihand* this) {
    s32 hitElement = 0;

    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    while (hitElement < DEXIHAND_COLLIDER_COUNT && !(this->collider.elements[hitElement].info.bumperFlags & BUMP_HIT)) {
        hitElement++;
    }
    this->limbIndexAfterCut = ((hitElement + 1) / 2) % 4;
    this->limbIndexBeforeCut = this->limbIndexAfterCut - 1;
    Matrix_Put(&this->relativeToWorld);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MultVecY(300.0f, &this->endPoints[DEXIHAND_HAND_POINT]);
    for (s32 i = 0; i < this->limbIndexAfterCut; i++) {
        Matrix_Push();
        Dexihand_SetSegmentTransform(this, i, false, &this->endPoints[DEXIHAND_HAND_POINT]);
        MultVecY(2300.0f, &this->endPoints[DEXIHAND_HAND_POINT]);
        Matrix_Pop();
    }
    this->actor.velocity.y = 2.45f;
    this->actor.velocity.x = 2.0f * Math_SinS(this->actor.world.rot.z);
    this->actor.velocity.z = 2.0f * Math_CosS(this->actor.world.rot.z);
    this->timer = 5;
    this->actionFunc = Dexihand_Die;
}

static void Dexihand_SpawnBubble(PlayState* play, Vec3f* pos, s16 scale) {
    Vec3f velocity = { 0.0f, Rand_ZeroOne() + 1.0f, 0.0f };

    EffectSsDtBubble_SpawnColorProfile(play, pos, &velocity, &sBubbleAccel, scale, 25, 2, true);
}

static void Dexihand_Vanish(EnDexihand* this, PlayState* play) {
    for (s32 point = 0; point < ARRAY_COUNT(this->endPoints); point++) {
        for (s32 i = 0; i < 5; i++) {
            Vec3f pos = this->endPoints[point];

            pos.x += Rand_CenteredFloat(12.0f);
            pos.y += Rand_CenteredFloat(12.0f);
            pos.z += Rand_CenteredFloat(12.0f);
            Dexihand_SpawnBubble(play, &pos, Rand_S16Offset(0x28, 0x28));
        }
    }
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

static void Dexihand_FlailStump(EnDexihand* this) {
    s32 t = this->timer;

    for (s32 i = 0; i <= this->limbIndexBeforeCut; i++, t += 2) {
        s16 sway = this->limbRotations[i].z * Math_SinF(t * (2 * M_PI / 7));

        if (this->timer > 0) {
            Math_ScaledStepToS(&this->limbRotations[i].x, sway, 0x200);
        } else if (Rand_ZeroOne() < 0.65f) {
            this->limbRotations[i].x = sway;
        }
        if (t % 7 != 0) {
            continue;
        }
        if (i != 0) {
            this->limbRotations[i].y = this->limbRotations[i - 1].y + (s32)Rand_CenteredFloat(12288.0f);
        } else {
            this->limbRotations[0].y += (s16)(s32)Rand_CenteredFloat(12288.0f);
        }
        this->limbRotations[i].z = Rand_S16Offset(i * 0x100 + 0xC00, 0x800);
    }
}

// The severed part drifts off spinning while both stumps shrink away in bubbles.
static void Dexihand_Die(EnDexihand* this, PlayState* play) {
    Vec3f* endPoint = &this->endPoints[DEXIHAND_HAND_POINT];
    Vec3f* handSphere = &this->collider.elements[DEXIHAND_HAND_COLLIDER].dim.worldSphere.center;
    Vec3f halfway = { (endPoint->x - handSphere->x) * 0.5f, (endPoint->y - handSphere->y) * 0.5f,
                      (endPoint->z - handSphere->z) * 0.5f };
    Vec3f turned;

    Matrix_RotateX(BINANG_TO_RAD(0x100), MTXMODE_NEW);
    Matrix_MultVec3f(&halfway, &turned);
    endPoint->x = halfway.x + turned.x + handSphere->x;
    endPoint->y = halfway.y + turned.y + handSphere->y;
    endPoint->z = halfway.z + turned.z + handSphere->z;
    if (this->actor.velocity.y > -2.0f) {
        this->actor.velocity.y -= 0.15f;
    }
    Math_Vec3f_Sum(endPoint, &this->actor.velocity, endPoint);
    this->actor.world.rot.x += 0x100;
    if (this->limbIndexBeforeCut >= 0 && Dexihand_ShrinkSegment(this, this->limbIndexBeforeCut)) {
        this->limbIndexBeforeCut--;
    }
    s32 segment = this->limbIndexAfterCut;

    if (segment < DEXIHAND_SEGMENTS) {
        if (Dexihand_ShrinkSegment(this, segment)) {
            this->limbIndexAfterCut++;
        }
        Matrix_RotateZYX(this->actor.world.rot.x, this->actor.world.rot.z, 0, MTXMODE_NEW);
        RotateYS(-this->actor.world.rot.z);
        Matrix_Mult(&this->relativeToWorld, MTXMODE_APPLY);
        Dexihand_SetSegmentTransform(this, segment, false, endPoint);
        MultVecY(2.3f / this->limbScales[segment], endPoint);
    }
    this->timer--;
    Dexihand_FlailStump(this);
    bool isFullyShrunk = this->limbIndexBeforeCut < 0 && this->limbIndexAfterCut >= DEXIHAND_SEGMENTS;

    if (isFullyShrunk && Math_StepToF(&this->actor.scale.x, 0.0f, 0.001f)) {
        Dexihand_Vanish(this, play);
        return;
    }
    if (isFullyShrunk) {
        this->actor.scale.y = this->actor.scale.x;
        this->actor.scale.z = this->actor.scale.x;
    }
    for (s32 i = 0; i < ARRAY_COUNT(this->endPoints); i++) {
        Dexihand_SpawnBubble(play, &this->endPoints[i], Rand_S16Offset(40, 40));
    }
}

// OoT lands zero-damage blows that MM drops on the floor; only weapons in the damage table cut the arm.
static void Dexihand_UpdateDamage(EnDexihand* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    if (this->actor.colChkInfo.damage == 0) {
        return;
    }
    Player* player = GET_PLAYER(play);

    this->collider.base.acFlags &= ~AC_ON;
    this->collider.base.atFlags &= ~AT_ON;
    this->collider.base.ocFlags1 &= ~OC1_ON;
    Actor_SetDropFlagJntSph(&this->actor, &this->collider, false);
    Actor_ApplyDamage(&this->actor);
    Enemy_StartFinishingBlow(play, &this->actor);
    if ((player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) && player->actor.parent == &this->actor) {
        Dexihand_ReleasePlayer(this, play);
        Actor_SetPlayerKnockbackLarge(play, &this->actor, this->actor.speedXZ, this->actor.world.rot.y,
                                      this->actor.velocity.y, 0);
    } else {
        Matrix_Transpose(&this->relativeToWorld);
    }
    Dexihand_SetupDie(this);
}

static void EnDexihand_Init(Actor* thisx, PlayState* play) {
    EnDexihand* this = (EnDexihand*)thisx;

    Dexihand_AnchorUnderWater(this, play);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sDexihandSkel, (AnimationHeader*)sDexihandIdleAnim,
                       this->jointTable, this->morphTable, DEXIHAND_LIMB_MAX);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    RotateYS(this->actor.shape.rot.y);
    Matrix_RotateX(BINANG_TO_RAD(this->actor.shape.rot.x), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(this->actor.shape.rot.z), MTXMODE_APPLY);
    MultVecY(3.0f, &this->endPoints[DEXIHAND_HAND_POINT]);
    Matrix_Get(&this->relativeToWorld);
    Matrix_MtxFToYXZRotS(&this->relativeToWorld, &this->actor.shape.rot, 0);
    Matrix_Transpose(&this->relativeToWorld);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.speedXZ = Dexihand_GetThrowVelocity((this->actor.params >> 7) & 0x7F);
    this->actor.velocity.y = Dexihand_GetThrowVelocity(this->actor.params & 0x7F);
    this->actor.velocity.x = Math_SinS(this->actor.world.rot.y) * this->actor.speedXZ;
    this->actor.velocity.z = Math_CosS(this->actor.world.rot.y) * this->actor.speedXZ;
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sJntSphInit, this->colliderElements);
    for (s32 i = 0; i < DEXIHAND_COLLIDER_COUNT; i++) {
        ColliderJntSphElement* element = &this->collider.elements[i];

        element->dim.worldSphere.radius = element->dim.modelSphere.radius;
        element->dim.worldSphere.center = this->actor.world.pos;
    }
    for (s32 i = 0; i < DEXIHAND_SEGMENTS; i++) {
        this->limbScales[i] = 1.0f;
    }
    this->limbIndexBeforeCut = -1;
    this->limbIndexAfterCut = 0;
    this->actor.world.rot.x = 0;
    this->actor.world.rot.z = Math_Atan2S(this->relativeToWorld.mf[2][1], this->relativeToWorld.mf[0][1]);
    Dexihand_SetupIdle(this);
}

static void EnDexihand_Destroy(Actor* thisx, PlayState* play) {
    EnDexihand* this = (EnDexihand*)thisx;
    Player* player = GET_PLAYER(play);

    // On scene exit the player category is destroyed before enemies, so Link may already be gone.
    if (player != NULL && player->actor.parent == &this->actor) {
        Dexihand_ReleasePlayer(this, play);
    }
    Collider_DestroyJntSph(play, &this->collider);
}

static void EnDexihand_Update(Actor* thisx, PlayState* play) {
    EnDexihand* this = (EnDexihand*)thisx;

    Dexihand_UpdateDamage(this, play);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
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

static void Dexihand_DrawSegment(EnDexihand* this, PlayState* play, s32 segment, Vec3f* limbPos) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Dexihand_SetSegmentTransform(this, segment, true, limbPos);
    MultVecY(575.0f, limbPos);
    this->collider.elements[2 * segment + 0].dim.worldSphere.center = *limbPos;
    MultVecY(1725.0f, limbPos);
    this->collider.elements[2 * segment + 1].dim.worldSphere.center = *limbPos;
    MultVecY(2300.0f, limbPos);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sDexihandArmSegmentDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// The arm is three hand-placed segments; the hand skeleton sits on the last one.
static void EnDexihand_Draw(Actor* thisx, PlayState* play) {
    EnDexihand* this = (EnDexihand*)thisx;
    Vec3f limbPos;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sDexihandBaseDL);
    CLOSE_DISPS(play->state.gfxCtx);
    MultVecY(300.0f, &limbPos);
    for (s32 i = 0; i <= this->limbIndexBeforeCut; i++) {
        Dexihand_DrawSegment(this, play, i, &limbPos);
    }
    this->endPoints[DEXIHAND_CUT_POINT] = limbPos;
    limbPos = this->endPoints[DEXIHAND_HAND_POINT];
    if (this->actor.world.rot.x != 0) {
        Matrix_RotateZYX(this->actor.world.rot.x, this->actor.world.rot.z, 0, MTXMODE_NEW);
        RotateYS(-this->actor.world.rot.z);
        Matrix_Mult(&this->relativeToWorld, MTXMODE_APPLY);
        Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    }
    for (s32 i = this->limbIndexAfterCut; i < DEXIHAND_SEGMENTS; i++) {
        Dexihand_DrawSegment(this, play, i, &limbPos);
    }
    Dexihand_SetSegmentTransform(this, DEXIHAND_HAND, true, &limbPos);
    if (this->globalLimbScale > 1.0f) {
        f32 openness = CLAMP_MAX(this->globalLimbScale, 1.5f);

        Matrix_RotateZ(BINANG_TO_RAD((s16)((openness - 1.0f) * -32768.0f)), MTXMODE_APPLY);
    }
    if (this->actor.scale.x < 0.01f) {
        Matrix_Translate(0.0f, (10.0f / this->actor.scale.x) - 1000.0f, 0.0f, MTXMODE_APPLY);
    }
    this->actor.focus.pos = limbPos;
    MultVecY(1000.0f, &limbPos);
    this->collider.elements[DEXIHAND_HAND_COLLIDER].dim.worldSphere.center = limbPos;
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          NULL, this);
}

// MM's CanKillEnemy(ACTOR_EN_WDHAND) needs Zora fins underwater; OoT reaches it in Iron Boots with a blade or hook.
static bool Dexihand_CanKillInLogic(void* userData) {
    bool canWeaponUnderwater = sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RG_HOOKSHOT);

    return sApi->LogicCanUseItem(RG_IRON_BOOTS) && canWeaponUnderwater;
}

static void RegisterDexihand(void) {
    SOHActorDefinition dexihand = { 0 };

    dexihand.structSize = sizeof(dexihand);
    dexihand.key = DEXIHAND_KEY;
    dexihand.description = "Dexihand";
    dexihand.category = ACTORCAT_ENEMY;
    dexihand.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
    dexihand.objectId = OBJECT_GAMEPLAY_KEEP;
    dexihand.instanceSize = sizeof(EnDexihand);
    dexihand.init = EnDexihand_Init;
    dexihand.destroy = EnDexihand_Destroy;
    dexihand.update = EnDexihand_Update;
    dexihand.draw = EnDexihand_Draw;
    dexihand.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NEEDS_WATER | SOH_ACTOR_ENEMY_UNDERWATER;
    dexihand.enemyParams = DEXIHAND_THROW_PARAMS;
    dexihand.naviHint = "Dexihand&%cA hand that lurks under the water. It grabs whoever wanders close and "
                        "flings them away!%w";
    dexihand.soulKey = DEXIHAND_KEY;
    dexihand.enemyCategory = SOH_ENEMY_CATEGORY_REGULAR;
    dexihand.soulModel.skeleton = sDexihandSkel;
    dexihand.soulModel.idleAnimation = sDexihandIdleAnim;
    dexihand.soulModel.isFlex = 1;
    dexihand.soulModel.limbCount = DEXIHAND_LIMB_MAX;
    dexihand.soulModel.scale = 0.015f;
    dexihand.logicKill = Dexihand_CanKillInLogic;
    if (sApi->RegisterActor(&dexihand) < 0) {
        sApi->Log("Could not register " DEXIHAND_KEY);
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
    RegisterDexihand();
}
