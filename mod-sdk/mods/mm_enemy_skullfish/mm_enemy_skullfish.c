// Majora's Mask's Skullfish (En_Pr2, the territorial kind) as an Unbound custom enemy, drawn from mm.o2r.
// Params 10 and up are the bones it falls apart into, spawned by its own death.
#include "z64items.h"

#include <math.h>
#include <stdio.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define SKULLFISH_KEY "mm.skullfish"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_PR "__OTR__objects/object_pr/"

#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_CEILING_FLOOR ((1 << 0) | (1 << 1) | (1 << 2))

#define MM_SE_EN_PIRANHA_EXIST 0x39F3
#define MM_SE_EN_PIRANHA_ATTACK 0x39F4
#define MM_SE_EN_PIRANHA_DEAD 0x39F5

#define SKULLFISH_LIMB_MAX 5
#define SKULLFISH_LIMB_JAW 2
#define SKULLFISH_BONE_PARAMS 10
#define SKULLFISH_TERRITORY 400.0f
#define SKULLFISH_SCALE 0.02f
#define SKULLFISH_OPAQUE 255
#define WATER_SEARCH_STEP 100.0f
#define WATER_SEARCH_RADIUS 1500.0f
#define WATER_SEARCH_ANGLES 16

struct EnSkullfish;

typedef void (*EnSkullfishActionFunc)(struct EnSkullfish*, PlayState*);

typedef struct EnSkullfish {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[SKULLFISH_LIMB_MAX];
    Vec3s morphTable[SKULLFISH_LIMB_MAX];
    EnSkullfishActionFunc actionFunc;
    bool isDead;
    bool isBubbling;
    s16 pauseTimer;
    s16 wanderTimer;
    s16 biteCooldown;
    s16 leashCooldown;
    s16 targetYaw;
    s16 wallDetour;
    s16 wallBumps;
    s16 boneTurnRoll;
    s16 boneTurnYaw;
    s16 brightness;
    s16 alpha;
    f32 animEndFrame;
    f32 jawYaw;
    f32 scale;
    f32 biteHeight;
    Vec3f target;
    Vec3f home;
    Vec3f mouthPos;
    Vec3f bonePos[SKULLFISH_LIMB_MAX];
    ColliderCylinder collider;
} EnSkullfish;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sSkullfishSkel[] = OBJ_PR "object_pr_Skel_004188";
static const ALIGN_ASSET(2) char sSkullfishSwimAnim[] = OBJ_PR "object_pr_Anim_004340";
static const ALIGN_ASSET(2) char sSkullfishBiteAnim[] = OBJ_PR "object_pr_Anim_004274";
static const ALIGN_ASSET(2) char sSkullfishBreakAnim[] = OBJ_PR "object_pr_Anim_003904";

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_1, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, HIT_SPECIAL_EFFECT_NONE, 0x04 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_ON,
      OCELEM_ON },
    { 17, 32, -10, { 0, 0, 0 } },
};

// One hit of anything that reaches it underwater breaks it apart.
static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(1, 0xF),
    /* Deku stick    */ DMG_ENTRY(1, 0xF),
    /* Slingshot     */ DMG_ENTRY(1, 0xF),
    /* Explosive     */ DMG_ENTRY(1, 0xF),
    /* Boomerang     */ DMG_ENTRY(1, 0xF),
    /* Normal arrow  */ DMG_ENTRY(1, 0xF),
    /* Hammer swing  */ DMG_ENTRY(1, 0xF),
    /* Hookshot      */ DMG_ENTRY(1, 0xF),
    /* Kokiri sword  */ DMG_ENTRY(1, 0xF),
    /* Master sword  */ DMG_ENTRY(1, 0xF),
    /* Giant's Knife */ DMG_ENTRY(1, 0xF),
    /* Fire arrow    */ DMG_ENTRY(1, 0xF),
    /* Ice arrow     */ DMG_ENTRY(1, 0xF),
    /* Light arrow   */ DMG_ENTRY(2, 0xF),
    /* Unk arrow 1   */ DMG_ENTRY(1, 0xF),
    /* Unk arrow 2   */ DMG_ENTRY(1, 0xF),
    /* Unk arrow 3   */ DMG_ENTRY(1, 0xF),
    /* Fire magic    */ DMG_ENTRY(1, 0xF),
    /* Ice magic     */ DMG_ENTRY(1, 0xF),
    /* Light magic   */ DMG_ENTRY(1, 0xF),
    /* Shield        */ DMG_ENTRY(0, 0x0),
    /* Mirror Ray    */ DMG_ENTRY(0, 0x0),
    /* Kokiri spin   */ DMG_ENTRY(1, 0xF),
    /* Giant spin    */ DMG_ENTRY(1, 0xF),
    /* Master spin   */ DMG_ENTRY(1, 0xF),
    /* Kokiri jump   */ DMG_ENTRY(1, 0xF),
    /* Giant jump    */ DMG_ENTRY(1, 0xF),
    /* Master jump   */ DMG_ENTRY(1, 0xF),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(1, 0xF),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;
static s16 sSkullfishActorId = -1;

static void Skullfish_Patrol(EnSkullfish* this, PlayState* play);
static void Skullfish_Bite(EnSkullfish* this, PlayState* play);
static void Skullfish_BreakApart(EnSkullfish* this, PlayState* play);
static void Skullfish_DriftAsBone(EnSkullfish* this, PlayState* play);

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

static bool Skullfish_IsBone(EnSkullfish* this) {
    return this->actor.params >= SKULLFISH_BONE_PARAMS;
}

static void Skullfish_ChangeAnim(EnSkullfish* this, const char* anim, u8 mode) {
    this->animEndFrame = Animation_GetLastFrame((void*)anim);
    Animation_Change(&this->skelAnime, (AnimationHeader*)anim, 1.0f, 0.0f, this->animEndFrame, mode, 0.0f);
}

static bool Skullfish_IsPlayerSwimming(PlayState* play) {
    return (GET_PLAYER(play)->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0;
}

static bool Skullfish_IsHeadingAt(EnSkullfish* this, f32 slack) {
    return ABS((s16)(this->actor.world.rot.y - this->targetYaw)) < slack;
}

// Banks the jaw into turns and pitches toward the target's depth.
static void Skullfish_SteerToward(EnSkullfish* this, s16 yaw) {
    s16 turn = CLAMP((s16)(yaw - this->actor.world.rot.y), -10000, 10000);

    Math_ApproachF(&this->actor.world.pos.y, this->target.y, 0.3f, 5.0f);
    if (fabsf(this->actor.world.pos.y - this->target.y) > 10.0f) {
        Math_SmoothStepToS(&this->actor.world.rot.x, Math_Vec3f_Pitch(&this->actor.world.pos, &this->target) * 0.3f, 20,
                           0x1388, 0x1F4);
    } else {
        Math_SmoothStepToS(&this->actor.world.rot.x, 0, 20, 0x1388, 0x1F4);
    }
    if (ABS((s16)(this->actor.world.rot.y - yaw)) < 30) {
        Math_ApproachZeroF(&this->jawYaw, 0.5f, 20.0f);
    } else {
        Math_ApproachF(&this->jawYaw, turn, 0.5f, 3000.0f);
    }
    Math_SmoothStepToS(&this->actor.world.rot.y, yaw, 1, 0x7D0, 300);
}

static void Skullfish_SetupPatrol(EnSkullfish* this) {
    Skullfish_ChangeAnim(this, sSkullfishSwimAnim, ANIMMODE_LOOP);
    this->pauseTimer = 2;
    this->wanderTimer = 0;
    this->target = this->home;
    this->actionFunc = Skullfish_Patrol;
}

static void Skullfish_SetupBite(EnSkullfish* this, PlayState* play) {
    this->wallDetour = 0;
    Skullfish_ChangeAnim(this, sSkullfishBiteAnim, ANIMMODE_LOOP);
    PlaySfx(&this->actor, MM_SE_EN_PIRANHA_ATTACK, NA_SE_EN_OCTAROCK_BUBLE);
    this->target = GET_PLAYER(play)->actor.world.pos;
    this->targetYaw = Math_Vec3f_Yaw(&this->actor.world.pos, &this->target);
    this->biteHeight = Rand_ZeroFloat(30.0f);
    this->biteCooldown = 0;
    this->wanderTimer = 70;
    this->actionFunc = Skullfish_Bite;
}

static f32 Skullfish_GetDistanceFromHome(Vec3f* pos, Vec3f* home) {
    return sqrtf(SQ(pos->x - home->x) + SQ(pos->z - home->z));
}

// Strikes at Link once he swims into its patch of water, but never strays from it.
static bool Skullfish_TryNoticePlayer(EnSkullfish* this, PlayState* play, bool isFacingTarget) {
    if (this->leashCooldown != 0) {
        if (Skullfish_GetDistanceFromHome(&this->actor.world.pos, &this->home) > 20.0f) {
            this->leashCooldown = 5;
            this->biteCooldown = 0;
            this->target = this->home;
            Math_ApproachF(&this->actor.speedXZ, 3.0f, 0.3f, 0.2f);
            return true;
        }
        return false;
    }
    f32 playerFromHome = Skullfish_GetDistanceFromHome(&GET_PLAYER(play)->actor.world.pos, &this->home);

    if (isFacingTarget && Skullfish_IsPlayerSwimming(play) && playerFromHome < SKULLFISH_TERRITORY) {
        Skullfish_SetupBite(this, play);
        return true;
    }
    return false;
}

static void Skullfish_Wander(EnSkullfish* this, PlayState* play) {
    this->target.y = this->actor.world.pos.y;
    if (this->pauseTimer != 0) {
        this->isBubbling = this->isBubbling || Rand_ZeroOne() < 0.3f;
        Math_ApproachZeroF(&this->actor.speedXZ, 0.1f, 0.2f);
        if (this->pauseTimer == 1) {
            this->wanderTimer = Rand_S16Offset(100, 100);
            this->target = this->home;
            this->target.x += Rand_CenteredFloat(300.0f);
            this->target.z += Rand_CenteredFloat(300.0f);
        }
        return;
    }
    Vec3f ahead = { this->actor.world.pos.x + Math_SinS(this->actor.world.rot.y) * 20.0f, this->actor.world.pos.y,
                    this->actor.world.pos.z + Math_CosS(this->actor.world.rot.y) * 20.0f };

    Math_ApproachF(&this->actor.speedXZ, 2.0f, 0.3f, 0.2f);
    if (Skullfish_IsHeadingAt(this, 100.0f)) {
        if (BgCheck_SphVsFirstPoly(&play->colCtx, &ahead, 20.0f) || (this->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
            this->biteCooldown = 0;
            this->wallBumps++;
            this->target = this->home;
            if (this->wallBumps > 10) {
                this->wallDetour += 0x2000;
            }
        } else {
            Math_SmoothStepToS(&this->wallDetour, 0, 1, 0x3E8, 0x64);
            this->wallBumps = 0;
        }
    }
    bool hasArrived = fabsf(this->target.x - this->actor.world.pos.x) < 10.0f &&
                      fabsf(this->target.z - this->actor.world.pos.z) < 10.0f;

    if (this->wanderTimer == 0 || hasArrived) {
        this->pauseTimer = Rand_S16Offset(20, 30);
    }
}

static void Skullfish_Patrol(EnSkullfish* this, PlayState* play) {
    bool isFacingTarget = Skullfish_IsHeadingAt(this, 200.0f);

    Math_ApproachF(&this->scale, SKULLFISH_SCALE, 0.1f, 0.005f);
    PlaySfx(&this->actor, MM_SE_EN_PIRANHA_EXIST - SFX_FLAG, NA_SE_EN_OCTAROCK_BUBLE);
    if (isFacingTarget) {
        SkelAnime_Update(&this->skelAnime);
    }
    if (!Skullfish_TryNoticePlayer(this, play, isFacingTarget)) {
        Skullfish_Wander(this, play);
    }
    if (this->pauseTimer == 0) {
        this->targetYaw = Math_Vec3f_Yaw(&this->actor.world.pos, &this->target) + this->wallDetour;
        Skullfish_SteerToward(this, this->targetYaw);
    }
}

// Keeps below the surface while it chases, and gives up when Link leaves the water or its territory.
static void Skullfish_Bite(EnSkullfish* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    WaterBox* waterBox;
    f32 waterSurface;

    Math_ApproachF(&this->scale, SKULLFISH_SCALE, 0.1f, 0.005f);
    if (this->wanderTimer == 0 || !Skullfish_IsPlayerSwimming(play)) {
        Skullfish_SetupPatrol(this);
        return;
    }
    SkelAnime_Update(&this->skelAnime);
    if (this->biteCooldown == 0 && Skullfish_IsHeadingAt(this, 200.0f)) {
        this->target = player->actor.world.pos;
    }
    if (Rand_ZeroOne() < 0.3f && !this->isBubbling) {
        this->isBubbling = true;
        this->biteHeight = Rand_ZeroFloat(30.0f);
    }
    this->target.y = player->actor.world.pos.y + 30.0f + this->biteHeight;
    Math_ApproachF(&this->actor.speedXZ, 5.0f, 0.3f, 1.0f);
    this->wallDetour = 0;
    if (Skullfish_GetDistanceFromHome(&this->actor.world.pos, &this->home) > SKULLFISH_TERRITORY) {
        this->leashCooldown = 20;
        Skullfish_SetupPatrol(this);
        return;
    }
    if (WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &waterSurface,
                             &waterBox)) {
        this->target.y = MIN(this->target.y, waterSurface - 40.0f);
    }
    if (this->collider.base.atFlags & AT_HIT) {
        this->biteCooldown = Rand_S16Offset(30, 30);
        this->wanderTimer = 100;
    }
    this->targetYaw = Math_Vec3f_Yaw(&this->actor.world.pos, &this->target);
    Skullfish_SteerToward(this, this->targetYaw);
}

static void Skullfish_SetupBreakApart(EnSkullfish* this) {
    this->brightness = 0;
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Skullfish_ChangeAnim(this, sSkullfishBreakAnim, ANIMMODE_ONCE);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 10);
    this->actionFunc = Skullfish_BreakApart;
}

static void Skullfish_BreakApart(EnSkullfish* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->skelAnime.curFrame < this->animEndFrame) {
        return;
    }
    for (s32 i = 0; i < SKULLFISH_LIMB_MAX; i++) {
        Actor_Spawn(&play->actorCtx, play, sSkullfishActorId, this->bonePos[i].x, this->bonePos[i].y,
                    this->bonePos[i].z, this->actor.world.rot.x, this->actor.world.rot.y, this->actor.world.rot.z,
                    i + SKULLFISH_BONE_PARAMS);
    }
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    Actor_Kill(&this->actor);
}

static void Skullfish_SpawnBubbles(PlayState* play, Vec3f* origin, s32 count) {
    for (s32 i = 0; i < count; i++) {
        Vec3f pos = { origin->x + Rand_CenteredFloat(20.0f), origin->y + Rand_CenteredFloat(5.0f),
                      origin->z + Rand_CenteredFloat(20.0f) };

        EffectSsBubble_Spawn(play, &pos, 0.0f, 5.0f, 5.0f, Rand_ZeroFloat(0.03f) + 0.07f);
    }
}

static void Skullfish_SetupBone(EnSkullfish* this) {
    AnimationHeader* breakAnim = (AnimationHeader*)sSkullfishBreakAnim;

    this->animEndFrame = Animation_GetLastFrame(breakAnim);
    Animation_Change(&this->skelAnime, breakAnim, 1.0f, this->animEndFrame, this->animEndFrame, ANIMMODE_ONCE, 0.0f);
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->scale = SKULLFISH_SCALE;
    this->boneTurnRoll = Rand_ZeroOne() < 0.5f ? -0x4000 : 0x4000;
    this->boneTurnYaw = this->actor.world.rot.y;
    this->actor.shape.rot = this->actor.world.rot;
    this->wanderTimer = 30;
    this->actor.speedXZ = Rand_ZeroFloat(0.5f);
    this->actor.world.rot.y = Rand_CenteredFloat(0x8000);
    this->actionFunc = Skullfish_DriftAsBone;
}

// Each bone rolls over, floats up and dissolves into bubbles.
static void Skullfish_DriftAsBone(EnSkullfish* this, PlayState* play) {
    WaterBox* waterBox;
    f32 waterSurface;
    bool hasSurfaced = false;

    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToS(&this->actor.shape.rot.x, 0, 5, 0x2710, 0x3E8);
    Math_SmoothStepToS(&this->actor.shape.rot.z, this->boneTurnRoll, 5, 0x2710, 0x3E8);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->boneTurnYaw, 5, 0x2710, 0x3E8);
    if (WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &waterSurface,
                             &waterBox)) {
        hasSurfaced = this->actor.world.pos.y >= waterSurface - 15.0f;
        if (!hasSurfaced) {
            Math_ApproachF(&this->actor.world.pos.y, waterSurface - 15.0f, 0.3f, 1.0f);
        }
    }
    if (this->wanderTimer != 0 && !hasSurfaced) {
        return;
    }
    Math_SmoothStepToS(&this->alpha, 0, 1, 15, 50);
    if (this->alpha < 2) {
        Skullfish_SpawnBubbles(play, &this->actor.world.pos, 10);
        Actor_Kill(&this->actor);
    }
}

static void Skullfish_Die(EnSkullfish* this, PlayState* play) {
    this->isDead = true;
    this->actor.colChkInfo.health = 0;
    this->actor.speedXZ = 0.0f;
    PlaySfx(&this->actor, MM_SE_EN_PIRANHA_DEAD, NA_SE_EN_BARI_DEAD);
    Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos, 0x30);
    Skullfish_SetupBreakApart(this);
}

static void Skullfish_CheckDamage(EnSkullfish* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        Actor_ApplyDamage(&this->actor);
        if (this->actor.colChkInfo.health <= 0 && !this->isDead) {
            Enemy_StartFinishingBlow(play, &this->actor);
            Skullfish_Die(this, play);
        }
    }
    if (this->collider.base.atFlags & AT_BOUNCED) {
        this->actor.speedXZ = -10.0f;
    }
}

// The first floor below the probe must lie under the surface, or the water box belongs to a lower floor.
static bool FindSwimmableWater(PlayState* play, f32 x, f32 z, f32 originY, Vec3f* swimPos) {
    f32 surface;
    WaterBox* waterBox;

    if (!WaterBox_GetSurface1(play, &play->colCtx, x, z, &surface, &waterBox)) {
        return false;
    }
    CollisionPoly* poly;
    Vec3f probe = { x, MAX(surface, originY) + 50.0f, z };
    f32 floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);

    if (floorY <= BGCHECK_Y_MIN || surface <= floorY) {
        return false;
    }
    f32 lowestY = floorY + 30.0f;
    f32 highestY = surface - 40.0f;
    swimPos->x = x;
    swimPos->y = (lowestY < highestY) ? CLAMP(originY, lowestY, highestY) : (floorY + surface) * 0.5f;
    swimPos->z = z;
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

// Like vanilla En_Pr2 it only needs to touch water; off a submerged row it rings outward for the nearest patch.
static bool Skullfish_MoveIntoWater(EnSkullfish* this, PlayState* play) {
    Vec3f origin = this->actor.world.pos;
    Vec3f swimPos;
    bool isFound = FindSwimmableWater(play, origin.x, origin.z, origin.y, &swimPos);

    for (f32 radius = WATER_SEARCH_STEP; !isFound && radius <= WATER_SEARCH_RADIUS; radius += WATER_SEARCH_STEP) {
        for (s32 step = 0; !isFound && step < WATER_SEARCH_ANGLES; step++) {
            s16 angle = step * (0x10000 / WATER_SEARCH_ANGLES);
            f32 x = origin.x + Math_SinS(angle) * radius;
            f32 z = origin.z + Math_CosS(angle) * radius;

            isFound = FindSwimmableWater(play, x, z, origin.y, &swimPos) && !IsWallBetween(play, &origin, &swimPos);
        }
    }
    if (isFound) {
        this->actor.world.pos = swimPos;
        this->actor.home.pos = swimPos;
        this->home = swimPos;
    }
    return isFound;
}

// Vanilla removes a dry Skullfish; this one dies the normal way, so drops, souls and clear rooms see a kill.
static void Skullfish_DieOutOfWater(EnSkullfish* this, PlayState* play) {
    char message[128];

    snprintf(message, sizeof(message), "mm.skullfish: no swimmable water near (%.0f, %.0f, %.0f); it dies on land.",
             this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z);
    sApi->Log(message);
    for (s32 i = 0; i < SKULLFISH_LIMB_MAX; i++) {
        this->bonePos[i] = this->actor.world.pos;
    }
    this->scale = SKULLFISH_SCALE;
    Skullfish_Die(this, play);
}

static void EnSkullfish_Init(Actor* thisx, PlayState* play) {
    EnSkullfish* this = (EnSkullfish*)thisx;

    this->actor.targetMode = 3;
    this->actor.colChkInfo.health = 1;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    this->actor.colChkInfo.mass = 10;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkullfishSkel,
                       (AnimationHeader*)sSkullfishSwimAnim, this->jointTable, this->morphTable, SKULLFISH_LIMB_MAX);
    this->home = this->actor.home.pos;
    this->brightness = SKULLFISH_OPAQUE;
    this->alpha = SKULLFISH_OPAQUE;
    this->actor.shape.yOffset = 500.0f;
    if (Skullfish_IsBone(this)) {
        Skullfish_SetupBone(this);
        return;
    }
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 19.0f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    if (Skullfish_MoveIntoWater(this, play)) {
        Skullfish_SetupPatrol(this);
    } else {
        Skullfish_DieOutOfWater(this, play);
    }
}

static void EnSkullfish_Destroy(Actor* thisx, PlayState* play) {
    EnSkullfish* this = (EnSkullfish*)thisx;

    if (!Skullfish_IsBone(this)) {
        Collider_DestroyCylinder(play, &this->collider);
    }
}

static void Skullfish_SubmitCollider(EnSkullfish* this, PlayState* play) {
    this->actor.shape.rot = this->actor.world.rot;
    if (this->actionFunc == Skullfish_BreakApart) {
        return;
    }
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
}

static void EnSkullfish_Update(Actor* thisx, PlayState* play) {
    EnSkullfish* this = (EnSkullfish*)thisx;

    Actor_SetScale(&this->actor, this->scale);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    DECR(this->pauseTimer);
    DECR(this->wanderTimer);
    DECR(this->biteCooldown);
    DECR(this->leashCooldown);
    Actor_SetFocus(&this->actor, 10.0f);
    if (!Skullfish_IsBone(this)) {
        Skullfish_CheckDamage(this, play);
    }
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 10.0f, 20.0f, BGCHECK_UPDATE_WALL_CEILING_FLOOR);
    if (this->isBubbling) {
        this->isBubbling = false;
        Skullfish_SpawnBubbles(play, &this->mouthPos, 2);
    }
    if (!Skullfish_IsBone(this)) {
        Skullfish_SubmitCollider(this, play);
    }
}

static s32 Skullfish_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx,
                                      Gfx** gfx) {
    EnSkullfish* this = (EnSkullfish*)thisx;

    if (Skullfish_IsBone(this)) {
        if (limbIndex + SKULLFISH_BONE_PARAMS != this->actor.params) {
            *dList = NULL;
        }
    } else if (limbIndex == SKULLFISH_LIMB_JAW) {
        rot->y -= (s16)this->jawYaw;
    }
    return false;
}

static void Skullfish_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx) {
    static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
    EnSkullfish* this = (EnSkullfish*)thisx;

    if (Skullfish_IsBone(this)) {
        return;
    }
    if (limbIndex == SKULLFISH_LIMB_JAW) {
        Matrix_MultVec3f(&sZeroVec, &this->mouthPos);
    }
    Matrix_MultVec3f(&sZeroVec, &this->bonePos[limbIndex]);
}

// MM's Scene_SetRenderModeXlu tables for segment 0x0C: object_pr calls entries 0 and 2.
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

static void EnSkullfish_Draw(Actor* thisx, PlayState* play) {
    EnSkullfish* this = (EnSkullfish*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    if (this->alpha == SKULLFISH_OPAQUE) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, this->brightness, this->brightness, this->brightness, 255);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, this->alpha);
        gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
        POLY_OPA_DISP =
            SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                               Skullfish_OverrideLimbDraw, Skullfish_PostLimbDraw, this, POLY_OPA_DISP);
    } else {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->alpha);
        gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)sRenderModeXluDL);
        POLY_XLU_DISP =
            SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                               Skullfish_OverrideLimbDraw, NULL, this, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// MM's CanKillEnemy(ACTOR_EN_PR2) needs Zora fins underwater; OoT reaches it in Iron Boots with a blade or hook.
static bool Skullfish_CanKillInLogic(void* userData) {
    bool canWeaponUnderwater = sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RG_HOOKSHOT);

    return sApi->LogicCanUseItem(RG_IRON_BOOTS) && canWeaponUnderwater;
}

static void RegisterSkullfish(void) {
    SOHActorDefinition skullfish = { 0 };

    skullfish.structSize = sizeof(skullfish);
    skullfish.key = SKULLFISH_KEY;
    skullfish.description = "Skullfish";
    skullfish.category = ACTORCAT_ENEMY;
    skullfish.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    skullfish.objectId = OBJECT_GAMEPLAY_KEEP;
    skullfish.instanceSize = sizeof(EnSkullfish);
    skullfish.init = EnSkullfish_Init;
    skullfish.destroy = EnSkullfish_Destroy;
    skullfish.update = EnSkullfish_Update;
    skullfish.draw = EnSkullfish_Draw;
    skullfish.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NEEDS_WATER | SOH_ACTOR_ENEMY_UNDERWATER;
    skullfish.naviHint = "Skullfish&%cIt bites anyone who swims into its waters!%w";
    // No soulModel: the Skullfish family soul item and its model belong to mm_enemy_desbreko.
    skullfish.soulKey = SKULLFISH_KEY;
    skullfish.enemyCategory = SOH_ENEMY_CATEGORY_REGULAR;
    skullfish.logicKill = Skullfish_CanKillInLogic;
    sSkullfishActorId = sApi->RegisterActor(&skullfish);
    if (sSkullfishActorId < 0) {
        sApi->Log("Could not register " SKULLFISH_KEY);
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
    RegisterSkullfish();
}
