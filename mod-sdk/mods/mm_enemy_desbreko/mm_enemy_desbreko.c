// Majora's Mask's Desbreko (En_Pr) and its Skullfish escort (En_Prz) as an Unbound custom enemy, drawn from mm.o2r.
// Params bits 8-15 count the escort (default 5); world rot z times 20 is the territory radius.
// MM only lets Zora fists hurt it; here a blade or hook does, and it also hunts Link walking the bottom in Iron Boots.
#include "z64items.h"

#include <math.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "soh_assets.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define DESBREKO_KEY "mm.desbreko"
#define SKULLFISH_FAMILY_KEY "mm.skullfish"
#define SKULLFISH_SOUL_ITEM_KEY "soul.mm.skullfish"
#define DESBREKO_ESCORT_KEY "mm.desbreko.escort"
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"

#define OBJ_PR "__OTR__objects/object_pr/"

#define COLORFILTER_RED 0x4000
#define COLORFILTER_OPA 0x0000
#define BGCHECK_UPDATE_WALL_FLOOR_WATER ((1 << 0) | (1 << 2) | (1 << 3) | (1 << 4))

#define MM_SE_EN_BUBLEWALK_DAMAGE 0x3977
#define MM_SE_EN_BUBLEWALK_DEAD 0x3978

#define DESBREKO_LIMB_MAX 10
#define DESBREKO_LIMB_HEAD 2
#define ESCORT_LIMB_MAX 5
#define ESCORT_LIMB_HEAD 2
#define DESBREKO_DEFAULT_ESCORT 5
#define DESBREKO_MAX_ESCORT 20
#define DESBREKO_DEFAULT_TERRITORY 300.0f
#define DESBREKO_MIN_TERRITORY 80.0f
#define DESBREKO_MIN_DEPTH 120.0f
#define WATER_SEARCH_STEP 100.0f
#define WATER_SEARCH_RADIUS 1500.0f
#define WATER_SEARCH_ANGLES 16
#define DESBREKO_BITE_HEIGHT 15.0f
#define ESCORT_BITE_HEIGHT 30.0f
#define DESBREKO_OPAQUE 255

typedef enum {
    DESBREKO_STATE_PATROL,
    DESBREKO_STATE_RETURN,
    DESBREKO_STATE_TURN,
    DESBREKO_STATE_PICK_DEPTH,
    DESBREKO_STATE_CHASE,
    DESBREKO_STATE_BITE,
    DESBREKO_STATE_HURT,
    DESBREKO_STATE_DEAD
} DesbrekoState;

typedef enum {
    DESBREKO_ANIM_SWIM,
    DESBREKO_ANIM_UNUSED,
    DESBREKO_ANIM_SWIM_FAST,
    DESBREKO_ANIM_BITE,
    DESBREKO_ANIM_HURT,
    DESBREKO_ANIM_MAX
} DesbrekoAnim;

struct EnDesbreko;

typedef void (*EnDesbrekoActionFunc)(struct EnDesbreko*, PlayState*);

typedef struct EnDesbreko {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[DESBREKO_LIMB_MAX];
    Vec3s morphTable[DESBREKO_LIMB_MAX];
    EnDesbrekoActionFunc actionFunc;
    bool hasEscortSpawned;
    bool isBubbling;
    bool isFadingOut;
    s16 state;
    s16 brightness;
    s16 alpha;
    s16 timer;
    s16 patienceTimer;
    s16 bubbleTimer;
    s16 headYaw;
    s16 headYawTarget;
    s16 targetYaw;
    s16 bob;
    s32 animIndex;
    f32 animEndFrame;
    f32 targetY;
    f32 waterSurface;
    f32 territory;
    f32 bobHeight;
    Vec3f headPos;
    Vec3f lookAhead;
    ColliderCylinder collider;
} EnDesbreko;

struct EnDesbrekoEscort;

typedef void (*EnEscortActionFunc)(struct EnDesbrekoEscort*, PlayState*);

typedef struct EnDesbrekoEscort {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[ESCORT_LIMB_MAX];
    Vec3s morphTable[ESCORT_LIMB_MAX];
    EnEscortActionFunc actionFunc;
    Actor* leader;
    bool isBubbling;
    bool isDying;
    s16 index;
    s16 brightness;
    s16 alpha;
    s16 targetYaw;
    s16 timer;
    s16 calmTimer;
    s16 dyingRoll;
    s16 dyingYaw;
    s16 turnBonus;
    s32 animIndex;
    f32 sway;
    f32 swing;
    f32 waterSurface;
    Vec3f headPos;
    Vec3f target;
    ColliderCylinder collider;
} EnDesbrekoEscort;

typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

static const ALIGN_ASSET(2) char sDesbrekoSkel[] = OBJ_PR "object_pr_Skel_0038B8";
static const ALIGN_ASSET(2) char sDesbrekoSwimAnim[] = OBJ_PR "object_pr_Anim_0021E8";
static const ALIGN_ASSET(2) char sDesbrekoUnusedAnim[] = OBJ_PR "object_pr_Anim_001E10";
static const ALIGN_ASSET(2) char sDesbrekoBiteAnim[] = OBJ_PR "object_pr_Anim_000740";
static const ALIGN_ASSET(2) char sDesbrekoHurtAnim[] = OBJ_PR "object_pr_Anim_000268";
static const ALIGN_ASSET(2) char sEscortSkel[] = OBJ_PR "object_pr_Skel_004188";
static const ALIGN_ASSET(2) char sEscortSwimAnim[] = OBJ_PR "object_pr_Anim_004340";
static const ALIGN_ASSET(2) char sEscortBiteAnim[] = OBJ_PR "object_pr_Anim_004274";

static const char* const sDesbrekoAnimations[DESBREKO_ANIM_MAX] = {
    sDesbrekoSwimAnim, sDesbrekoUnusedAnim, sDesbrekoSwimAnim, sDesbrekoBiteAnim, sDesbrekoHurtAnim,
};

static const u8 sDesbrekoAnimModes[DESBREKO_ANIM_MAX] = {
    ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE,
};

static ColliderCylinderInit sDesbrekoCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK4,
      { DMG_UNBLOCKABLE, HIT_SPECIAL_EFFECT_NONE, 0x04 },
      { 0xF7CFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_ON,
      OCELEM_NONE },
    { 18, 20, 5, { 0, 0, 0 } },
};

static ColliderCylinderInit sEscortCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK4,
      { DMG_UNBLOCKABLE, HIT_SPECIAL_EFFECT_NONE, 0x04 },
      { 0xF7CFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_ON,
      OCELEM_NONE },
    { 10, 10, 0, { 0, 0, 0 } },
};

// MM only takes Zora fists, shots and explosives; blades stand in for the fists, scaled like OoT's weapons.
static DamageTable sDesbrekoDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, 0x0),
    /* Deku stick    */ DMG_ENTRY(0, 0x0),
    /* Slingshot     */ DMG_ENTRY(1, 0xF),
    /* Explosive     */ DMG_ENTRY(2, 0xF),
    /* Boomerang     */ DMG_ENTRY(1, 0xF),
    /* Normal arrow  */ DMG_ENTRY(2, 0xF),
    /* Hammer swing  */ DMG_ENTRY(0, 0x0),
    /* Hookshot      */ DMG_ENTRY(2, 0xF),
    /* Kokiri sword  */ DMG_ENTRY(1, 0xF),
    /* Master sword  */ DMG_ENTRY(2, 0xF),
    /* Giant's Knife */ DMG_ENTRY(4, 0xF),
    /* Fire arrow    */ DMG_ENTRY(0, 0x0),
    /* Ice arrow     */ DMG_ENTRY(0, 0x0),
    /* Light arrow   */ DMG_ENTRY(4, 0x4),
    /* Unk arrow 1   */ DMG_ENTRY(0, 0x0),
    /* Unk arrow 2   */ DMG_ENTRY(0, 0x0),
    /* Unk arrow 3   */ DMG_ENTRY(0, 0x0),
    /* Fire magic    */ DMG_ENTRY(0, 0x0),
    /* Ice magic     */ DMG_ENTRY(0, 0x0),
    /* Light magic   */ DMG_ENTRY(0, 0x0),
    /* Shield        */ DMG_ENTRY(0, 0x0),
    /* Mirror Ray    */ DMG_ENTRY(0, 0x0),
    /* Kokiri spin   */ DMG_ENTRY(2, 0xF),
    /* Giant spin    */ DMG_ENTRY(8, 0xF),
    /* Master spin   */ DMG_ENTRY(4, 0xF),
    /* Kokiri jump   */ DMG_ENTRY(2, 0xF),
    /* Giant jump    */ DMG_ENTRY(8, 0xF),
    /* Master jump   */ DMG_ENTRY(4, 0xF),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(0, 0x0),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

// A single hit of anything that reaches it underwater breaks a Skullfish apart.
static DamageTable sEscortDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, 0x0),
    /* Deku stick    */ DMG_ENTRY(0, 0x0),
    /* Slingshot     */ DMG_ENTRY(1, 0xF),
    /* Explosive     */ DMG_ENTRY(1, 0xF),
    /* Boomerang     */ DMG_ENTRY(1, 0xF),
    /* Normal arrow  */ DMG_ENTRY(1, 0xF),
    /* Hammer swing  */ DMG_ENTRY(0, 0x0),
    /* Hookshot      */ DMG_ENTRY(1, 0xF),
    /* Kokiri sword  */ DMG_ENTRY(1, 0xF),
    /* Master sword  */ DMG_ENTRY(1, 0xF),
    /* Giant's Knife */ DMG_ENTRY(1, 0xF),
    /* Fire arrow    */ DMG_ENTRY(1, 0xF),
    /* Ice arrow     */ DMG_ENTRY(1, 0xF),
    /* Light arrow   */ DMG_ENTRY(2, 0xF),
    /* Unk arrow 1   */ DMG_ENTRY(0, 0x0),
    /* Unk arrow 2   */ DMG_ENTRY(0, 0x0),
    /* Unk arrow 3   */ DMG_ENTRY(0, 0x0),
    /* Fire magic    */ DMG_ENTRY(0, 0x0),
    /* Ice magic     */ DMG_ENTRY(0, 0x0),
    /* Light magic   */ DMG_ENTRY(0, 0x0),
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
    /* Hammer jump   */ DMG_ENTRY(0, 0x0),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static bool sIsMmSfxSearched;
static int16_t sEscortActorId = -1;

static void Desbreko_Patrol(EnDesbreko* this, PlayState* play);
static void Desbreko_Return(EnDesbreko* this, PlayState* play);
static void Desbreko_Turn(EnDesbreko* this, PlayState* play);
static void Desbreko_PickDepth(EnDesbreko* this, PlayState* play);
static void Desbreko_Chase(EnDesbreko* this, PlayState* play);
static void Desbreko_Bite(EnDesbreko* this, PlayState* play);
static void Desbreko_Hurt(EnDesbreko* this, PlayState* play);
static void Desbreko_Dead(EnDesbreko* this, PlayState* play);
static void Escort_Follow(EnDesbrekoEscort* this, PlayState* play);
static void Escort_Turn(EnDesbrekoEscort* this, PlayState* play);
static void Escort_Hunt(EnDesbrekoEscort* this, PlayState* play);
static void Escort_Dying(EnDesbrekoEscort* this, PlayState* play);

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

static bool IsLinkInWater(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return (player->stateFlags1 & PLAYER_STATE1_IN_WATER) || player->actor.yDistToWater > 40.0f;
}

static f32 GetDistXZ(Vec3f* a, Vec3f* b) {
    return sqrtf(SQ(a->x - b->x) + SQ(a->z - b->z));
}

static void SpawnBubbles(PlayState* play, Vec3f* origin, s32 count) {
    for (s32 i = 0; i < count; i++) {
        Vec3f pos = { origin->x + Rand_CenteredFloat(20.0f), origin->y + Rand_CenteredFloat(5.0f),
                      origin->z + Rand_CenteredFloat(20.0f) };

        EffectSsBubble_Spawn(play, &pos, 0.0f, 5.0f, 5.0f, Rand_ZeroFloat(0.03f) + 0.07f);
    }
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

static void DrawFish(PlayState* play, SkelAnime* skelAnime, s16 brightness, s16 alpha,
                     OverrideLimbDraw overrideLimbDraw, PostLimbDraw postLimbDraw, void* fish) {
    OPEN_DISPS(play->state.gfxCtx);
    if (alpha == DESBREKO_OPAQUE) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, brightness, brightness, brightness, alpha);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, alpha);
        gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sRenderModeOpaDL);
        POLY_OPA_DISP = SkelAnime_DrawFlex(play, skelAnime->skeleton, skelAnime->jointTable, skelAnime->dListCount,
                                           overrideLimbDraw, postLimbDraw, fish, POLY_OPA_DISP);
    } else {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, alpha);
        gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)sRenderModeXluDL);
        POLY_XLU_DISP = SkelAnime_DrawFlex(play, skelAnime->skeleton, skelAnime->jointTable, skelAnime->dListCount,
                                           NULL, NULL, fish, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Desbreko_ChangeAnim(EnDesbreko* this, s32 animIndex) {
    AnimationHeader* animation = (AnimationHeader*)sDesbrekoAnimations[animIndex];
    f32 playSpeed = animIndex == DESBREKO_ANIM_SWIM_FAST ? 2.0f : 1.0f;

    this->animIndex = animIndex;
    this->animEndFrame = Animation_GetLastFrame(animation);
    Animation_Change(&this->skelAnime, animation, playSpeed, 0.0f, this->animEndFrame, sDesbrekoAnimModes[animIndex],
                     -2.0f);
}

static void Desbreko_SwimIfIdle(EnDesbreko* this) {
    if (this->animIndex != DESBREKO_ANIM_SWIM) {
        Desbreko_ChangeAnim(this, DESBREKO_ANIM_SWIM);
    }
}

typedef enum { DESBREKO_PATH_CLEAR, DESBREKO_PATH_WALL, DESBREKO_PATH_DEPTH_CLAMPED } DesbrekoPath;

// Keeps it 30 below the surface and 20 above the floor; a wall 70 ahead turns it around.
static DesbrekoPath Desbreko_CheckPath(EnDesbreko* this, PlayState* play) {
    CollisionPoly* poly;
    Vec3f hit;
    s32 bgId;
    WaterBox* waterBox;

    if (BgCheck_EntityLineTest1(&play->colCtx, &this->actor.world.pos, &this->lookAhead, &hit, &poly, true, false,
                                false, true, &bgId)) {
        return DESBREKO_PATH_WALL;
    }
    if (WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &this->waterSurface,
                             &waterBox) &&
        this->waterSurface - 30.0f < this->actor.world.pos.y) {
        this->targetY = this->waterSurface - 30.0f;
        return DESBREKO_PATH_DEPTH_CLAMPED;
    }
    if (this->actor.world.pos.y < this->actor.floorHeight + 20.0f) {
        this->targetY = this->actor.floorHeight + 20.0f;
        return DESBREKO_PATH_DEPTH_CLAMPED;
    }
    return DESBREKO_PATH_CLEAR;
}

// Swings its head into the turn; returns true while still turning.
static bool Desbreko_Steer(EnDesbreko* this) {
    s16 step = ABS((s16)(this->headYaw * 0.2f));

    Math_SmoothStepToS(&this->actor.world.rot.y, this->targetYaw, 1, step + 1, 0);
    this->headYawTarget = CLAMP((s16)(this->actor.world.rot.y - this->targetYaw), -10000, 10000);
    s16 yawLeft = this->actor.world.rot.y - this->targetYaw;

    if (ABS(yawLeft) > 0x100) {
        return true;
    }
    this->headYawTarget = 0;
    return false;
}

static void Desbreko_SetState(EnDesbreko* this, s16 state, EnDesbrekoActionFunc actionFunc) {
    this->state = state;
    this->actionFunc = actionFunc;
}

static void Desbreko_SetupPatrol(EnDesbreko* this) {
    Desbreko_SwimIfIdle(this);
    this->actor.speedXZ = 1.0f;
    Desbreko_SetState(this, DESBREKO_STATE_PATROL, Desbreko_Patrol);
}

static void Desbreko_SetupReturn(EnDesbreko* this) {
    Desbreko_SwimIfIdle(this);
    this->skelAnime.playSpeed = 1.0f;
    this->actor.speedXZ = 1.0f;
    Desbreko_SetState(this, DESBREKO_STATE_RETURN, Desbreko_Return);
}

static void Desbreko_SetupChase(EnDesbreko* this) {
    this->skelAnime.playSpeed = 2.0f;
    Desbreko_SetState(this, DESBREKO_STATE_CHASE, Desbreko_Chase);
}

static bool Desbreko_IsLinkInTerritory(EnDesbreko* this, PlayState* play) {
    return GetDistXZ(&GET_PLAYER(play)->actor.world.pos, &this->actor.home.pos) < this->territory;
}

static void Desbreko_Patrol(EnDesbreko* this, PlayState* play) {
    DesbrekoPath path = Desbreko_CheckPath(this, play);

    if (path == DESBREKO_PATH_WALL) {
        this->targetYaw = this->actor.world.rot.y + 0x4000;
        Desbreko_SetState(this, DESBREKO_STATE_TURN, Desbreko_Turn);
        return;
    }
    if (path == DESBREKO_PATH_DEPTH_CLAMPED) {
        Desbreko_SetState(this, DESBREKO_STATE_PICK_DEPTH, Desbreko_PickDepth);
    }
    if (GetDistXZ(&this->actor.world.pos, &this->actor.home.pos) > this->territory) {
        Desbreko_SetupReturn(this);
    } else if (this->patienceTimer == 0 && IsLinkInWater(play) && Desbreko_IsLinkInTerritory(this, play)) {
        Desbreko_SetupChase(this);
    }
}

static void Desbreko_Return(EnDesbreko* this, PlayState* play) {
    this->targetYaw = Math_Vec3f_Yaw(&this->actor.world.pos, &this->actor.home.pos);
    Desbreko_Steer(this);
    if (GetDistXZ(&this->actor.world.pos, &this->actor.home.pos) < 0.5f * this->territory) {
        Desbreko_SetState(this, DESBREKO_STATE_PICK_DEPTH, Desbreko_PickDepth);
    }
}

static void Desbreko_Turn(EnDesbreko* this, PlayState* play) {
    if (IsLinkInWater(play) && this->patienceTimer == 0) {
        this->targetYaw = this->actor.world.rot.y;
        Desbreko_SetupChase(this);
        return;
    }
    if (Desbreko_Steer(this)) {
        return;
    }
    if (Desbreko_CheckPath(this, play) != DESBREKO_PATH_CLEAR) {
        this->targetYaw += 0x1000;
        return;
    }
    Desbreko_SwimIfIdle(this);
    Desbreko_SetState(this, DESBREKO_STATE_PICK_DEPTH, Desbreko_PickDepth);
}

static void Desbreko_PickDepth(EnDesbreko* this, PlayState* play) {
    WaterBox* waterBox;
    f32 depth = Rand_CenteredFloat(50.0f) + this->actor.home.pos.y;

    if (WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &this->waterSurface,
                             &waterBox)) {
        this->targetY = MIN(depth, this->waterSurface - 30.0f);
    }
    Desbreko_SetState(this, DESBREKO_STATE_PATROL, Desbreko_Patrol);
}

static void Desbreko_AimAtLink(EnDesbreko* this, PlayState* play) {
    Math_SmoothStepToS(&this->targetYaw, this->actor.yawTowardsPlayer, 1, 0x3E8, 0);
    this->targetY = GET_PLAYER(play)->actor.world.pos.y + DESBREKO_BITE_HEIGHT;
    Desbreko_CheckPath(this, play);
}

static void Desbreko_Chase(EnDesbreko* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actor.speedXZ = 3.0f;
    Desbreko_AimAtLink(this, play);
    if (!IsLinkInWater(play) || !Desbreko_IsLinkInTerritory(this, play)) {
        Desbreko_SetupReturn(this);
        return;
    }
    bool isInReach =
        this->actor.xzDistToPlayer < 200.0f && fabsf(player->actor.world.pos.y - this->actor.world.pos.y) < 80.0f;

    if (!Desbreko_Steer(this) && isInReach) {
        this->skelAnime.playSpeed = 1.0f;
        Desbreko_ChangeAnim(this, DESBREKO_ANIM_BITE);
        this->timer = 400;
        Desbreko_SetState(this, DESBREKO_STATE_BITE, Desbreko_Bite);
    }
}

static void Desbreko_Bite(EnDesbreko* this, PlayState* play) {
    if (!IsLinkInWater(play) || !Desbreko_IsLinkInTerritory(this, play)) {
        Desbreko_SetupReturn(this);
        return;
    }
    Desbreko_AimAtLink(this, play);
    Desbreko_Steer(this);
}

static void Desbreko_SetupDead(EnDesbreko* this, PlayState* play) {
    this->timer = 50;
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->bobHeight = 0.0f;
    Enemy_StartFinishingBlow(play, &this->actor);
    PlaySfx(&this->actor, MM_SE_EN_BUBLEWALK_DEAD, NA_SE_EN_BUBLEWALK_DEAD);
    this->headYawTarget = 0;
    Desbreko_SetState(this, DESBREKO_STATE_DEAD, Desbreko_Dead);
}

static void Desbreko_Hurt(EnDesbreko* this, PlayState* play) {
    if (this->skelAnime.curFrame < this->animEndFrame) {
        return;
    }
    if (this->actor.colChkInfo.health == 0) {
        Desbreko_SetupDead(this, play);
    } else {
        Desbreko_SetupPatrol(this);
    }
}

// Belly-up, it bobs to the surface, goes dark and fades away.
static void Desbreko_Dead(EnDesbreko* this, PlayState* play) {
    WaterBox* waterBox;

    this->brightness = MAX(this->brightness - 2, 0);
    if (WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &this->waterSurface,
                             &waterBox)) {
        if (this->waterSurface - 100.0f < this->actor.world.pos.y) {
            this->bob += 0xBB8;
            this->bobHeight = 2.0f * Math_SinS(this->bob);
        }
        Math_ApproachF(&this->actor.world.pos.y, this->waterSurface - 16.0f + this->bobHeight, 0.5f, 2.0f);
        Math_SmoothStepToS(&this->actor.world.rot.z, 0x7700, 0x12C, 0x3E8, 0x3E8);
    }
    if (this->timer == 0 && this->brightness == 0) {
        this->isFadingOut = true;
    }
    if (!this->isFadingOut) {
        return;
    }
    Math_SmoothStepToS(&this->alpha, 0, 1, 15, 50);
    if (this->alpha < 2) {
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
        Actor_Kill(&this->actor);
    }
}

static void Desbreko_UpdateDamage(EnDesbreko* this) {
    if (this->state == DESBREKO_STATE_DEAD || this->state == DESBREKO_STATE_HURT) {
        return;
    }
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Actor_ApplyDamage(&this->actor);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 8);
    PlaySfx(&this->actor, MM_SE_EN_BUBLEWALK_DAMAGE, NA_SE_EN_BUBLEWALK_DAMAGE);
    Desbreko_ChangeAnim(this, DESBREKO_ANIM_HURT);
    this->actor.speedXZ = 0.0f;
    Desbreko_SetState(this, DESBREKO_STATE_HURT, Desbreko_Hurt);
}

// Each Skullfish trails the one spawned before it, the first trails the Desbreko.
static void Desbreko_SpawnEscort(EnDesbreko* this, PlayState* play) {
    s32 count = CLAMP((this->actor.params >> 8) & 0xFF, 0, DESBREKO_MAX_ESCORT);
    Actor* leader = &this->actor;

    this->hasEscortSpawned = true;
    for (s32 i = 0; i < count; i++) {
        EnDesbrekoEscort* escort = (EnDesbrekoEscort*)Actor_SpawnAsChild(
            &play->actorCtx, &this->actor, play, sEscortActorId, this->actor.world.pos.x, this->actor.world.pos.y,
            this->actor.world.pos.z, 0, this->actor.world.rot.y, 0, i + 1);

        if (escort != NULL) {
            escort->leader = leader;
            leader = &escort->actor;
        }
    }
}

static void Desbreko_TickTimers(EnDesbreko* this) {
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->patienceTimer != 0) {
        this->patienceTimer--;
    }
    if (this->bubbleTimer != 0) {
        this->bubbleTimer--;
    }
}

static void Desbreko_SwimToDepth(EnDesbreko* this) {
    Vec3f level = { this->actor.world.pos.x, this->targetY, this->actor.world.pos.z };

    Math_ApproachF(&this->actor.world.pos.y, this->targetY, 0.3f, 1.0f);
    s16 pitch = Math_Vec3f_Pitch(&this->actor.world.pos, &level) * 0.3f;

    if (fabsf(this->actor.world.pos.y - this->targetY) > 8.0f) {
        Math_SmoothStepToS(&this->actor.world.rot.x, pitch, 1, 0x1F4, 0);
    } else {
        Math_SmoothStepToS(&this->actor.world.rot.x, 0, 1, 0x1F4, 0);
    }
}

static void Desbreko_EmitBubbles(EnDesbreko* this, PlayState* play) {
    if (this->bubbleTimer == 0) {
        this->bubbleTimer = Rand_S16Offset(10, 30);
        this->isBubbling = true;
    }
    if (!this->isBubbling) {
        return;
    }
    this->isBubbling = false;
    SpawnBubbles(play, &this->headPos, (s32)Rand_ZeroFloat(5.0f) + 5);
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

    if (floorY <= BGCHECK_Y_MIN || surface - floorY < DESBREKO_MIN_DEPTH) {
        return false;
    }
    swimPos->x = x;
    swimPos->y = CLAMP(originY, floorY + 40.0f, surface - 60.0f);
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

// Placement only knows the room has water, so the school rings outward from its spot for the nearest deep patch.
static void Desbreko_MoveIntoWater(EnDesbreko* this, PlayState* play) {
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
    }
}

static void EnDesbreko_Init(Actor* thisx, PlayState* play) {
    EnDesbreko* this = (EnDesbreko*)thisx;

    Desbreko_MoveIntoWater(this, play);
    this->actor.colChkInfo.health = 3;
    this->actor.colChkInfo.mass = 50;
    this->actor.colChkInfo.damageTable = &sDesbrekoDamageTable;
    this->actor.targetMode = 3;
    this->actor.shape.yOffset = 1500.0f;
    if (((this->actor.params >> 8) & 0xFF) == 0) {
        this->actor.params |= DESBREKO_DEFAULT_ESCORT << 8;
    }
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sDesbrekoCylinderInit);
    Actor_SetScale(&this->actor, 0.01f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sDesbrekoSkel, (AnimationHeader*)sDesbrekoSwimAnim,
                       this->jointTable, this->morphTable, DESBREKO_LIMB_MAX);
    this->territory = this->actor.world.rot.z != 0 ? MAX(this->actor.world.rot.z * 20.0f, DESBREKO_MIN_TERRITORY)
                                                   : DESBREKO_DEFAULT_TERRITORY;
    this->actor.world.rot.z = 0;
    this->brightness = 255;
    this->alpha = DESBREKO_OPAQUE;
    this->patienceTimer = 30;
    this->targetY = this->actor.world.pos.y;
    this->waterSurface = this->actor.world.pos.y;
    this->headPos = this->actor.world.pos;
    Desbreko_SetupPatrol(this);
}

static void EnDesbreko_Destroy(Actor* thisx, PlayState* play) {
    EnDesbreko* this = (EnDesbreko*)thisx;

    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; actor != NULL; actor = actor->next) {
        EnDesbrekoEscort* escort = (EnDesbrekoEscort*)actor;

        if (actor->id != sEscortActorId || actor->parent != thisx) {
            continue;
        }
        actor->parent = NULL;
        if (escort->leader == thisx) {
            escort->leader = NULL;
        }
    }
    Collider_DestroyCylinder(play, &this->collider);
}

static void EnDesbreko_Update(Actor* thisx, PlayState* play) {
    EnDesbreko* this = (EnDesbreko*)thisx;

    if (!this->hasEscortSpawned) {
        Desbreko_SpawnEscort(this, play);
    }
    SkelAnime_Update(&this->skelAnime);
    Desbreko_TickTimers(this);
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    Actor_SetFocus(&this->actor, 20.0f);
    Desbreko_UpdateDamage(this);
    this->lookAhead.x = this->actor.world.pos.x + Math_SinS(this->actor.world.rot.y) * 70.0f;
    this->lookAhead.y = this->headPos.y - 10.0f;
    this->lookAhead.z = this->actor.world.pos.z + Math_CosS(this->actor.world.rot.y) * 70.0f;
    Math_SmoothStepToS(&this->headYaw, this->headYawTarget, 1, 0x7D0, 0);
    Actor_MoveXZGravity(&this->actor);
    if (this->state != DESBREKO_STATE_DEAD) {
        Desbreko_SwimToDepth(this);
    }
    Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 30.0f, 20.0f, BGCHECK_UPDATE_WALL_FLOOR_WATER);
    this->actor.shape.rot = this->actor.world.rot;
    if (this->state == DESBREKO_STATE_DEAD) {
        return;
    }
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    Desbreko_EmitBubbles(this, play);
}

static s32 Desbreko_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx,
                                     Gfx** gfx) {
    EnDesbreko* this = (EnDesbreko*)thisx;

    if (limbIndex == DESBREKO_LIMB_HEAD) {
        rot->y += this->headYaw;
    }
    return false;
}

static void Desbreko_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx) {
    EnDesbreko* this = (EnDesbreko*)thisx;

    if (limbIndex == DESBREKO_LIMB_HEAD) {
        Matrix_MultVec3f(&sZeroVec, &this->headPos);
    }
}

static void EnDesbreko_Draw(Actor* thisx, PlayState* play) {
    EnDesbreko* this = (EnDesbreko*)thisx;

    DrawFish(play, &this->skelAnime, this->brightness, this->alpha, Desbreko_OverrideLimbDraw, Desbreko_PostLimbDraw,
             this);
}

static void Escort_ChangeAnim(EnDesbrekoEscort* this, s32 animIndex) {
    AnimationHeader* animation = (AnimationHeader*)(animIndex == 0 ? sEscortSwimAnim : sEscortBiteAnim);

    this->animIndex = animIndex;
    Animation_Change(&this->skelAnime, animation, 1.0f, 0.0f, Animation_GetLastFrame(animation), ANIMMODE_LOOP, -2.0f);
}

static void Escort_ClampDepth(EnDesbrekoEscort* this, PlayState* play) {
    WaterBox* waterBox;

    if (WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &this->waterSurface,
                             &waterBox)) {
        if (this->waterSurface - 10.0f < this->actor.world.pos.y) {
            this->target.y = this->waterSurface - 10.0f;
        }
    } else if (this->actor.world.pos.y < this->actor.floorHeight + 35.0f) {
        this->target.y = this->actor.floorHeight + 35.0f;
    }
}

// Sways its tail into the turn and climbs or dives toward its target.
static void Escort_SwimToward(EnDesbrekoEscort* this, s16 yaw, PlayState* play) {
    s16 swing = CLAMP((s16)((s16)(yaw - this->actor.world.rot.y) * 0.01f), -100, 100);
    s16 yawLeft = this->actor.world.rot.y - yaw;

    if (ABS(yawLeft) < Rand_CenteredFloat(20.0f) + 100.0f) {
        Math_ApproachZeroF(&this->swing, 0.5f, 20.0f);
    } else {
        Math_ApproachF(&this->swing, swing, 0.5f, 5.0f);
    }
    s16 turnScale = MAX((s16)(fabsf(this->swing) * 0.1f), 2);

    Math_SmoothStepToS(&this->actor.world.rot.y, yaw, turnScale, this->turnBonus + 0x1388, 0);
    Escort_ClampDepth(this, play);
    Math_ApproachF(&this->actor.world.pos.y, this->target.y, 0.5f, 1.0f);
    s16 pitch = Math_Vec3f_Pitch(&this->actor.world.pos, &this->target) * 0.5f;

    if (fabsf(this->actor.world.pos.y - this->target.y) > 4.0f) {
        Math_SmoothStepToS(&this->actor.world.rot.x, pitch, 1, 0x1F4, 0);
    } else {
        Math_SmoothStepToS(&this->actor.world.rot.x, 0, 1, 0xBB8, 0);
    }
}

static bool Escort_IsWallAhead(EnDesbrekoEscort* this, PlayState* play) {
    CollisionPoly* poly;
    Vec3f hit;
    s32 bgId;
    Vec3f ahead = { this->actor.world.pos.x + Math_SinS(this->actor.world.rot.y) * 40.0f, this->actor.world.pos.y,
                    this->actor.world.pos.z + Math_CosS(this->actor.world.rot.y) * 40.0f };

    return BgCheck_EntityLineTest1(&play->colCtx, &this->actor.world.pos, &ahead, &hit, &poly, true, false, false, true,
                                   &bgId);
}

static void Escort_SetupFollow(EnDesbrekoEscort* this) {
    this->actor.speedXZ = Rand_CenteredFloat(1.0f) + 4.0f;
    Escort_ChangeAnim(this, 0);
    this->actionFunc = Escort_Follow;
}

static void Escort_SetupTurn(EnDesbrekoEscort* this) {
    this->timer = 0;
    this->targetYaw = this->actor.world.rot.y + 0x4000;
    this->actionFunc = Escort_Turn;
}

static void Escort_SetupHunt(EnDesbrekoEscort* this) {
    this->actor.speedXZ = Rand_CenteredFloat(1.0f) + 3.0f;
    this->timer = 0;
    this->skelAnime.playSpeed = 2.0f;
    this->actionFunc = Escort_Hunt;
}

// Picks a spot just behind its leader every second or so, catching up when it falls 100 behind the Desbreko.
static void Escort_PickFollowSpot(EnDesbrekoEscort* this) {
    Actor* desbreko = this->actor.parent;

    if (this->timer == 0 && this->calmTimer == 0) {
        f32 offset = Rand_CenteredFloat(40.0f);
        s16 behind = BINANG_ROT180(this->leader->world.rot.y);

        this->target.x += Math_SinS(behind) * offset;
        this->target.y = Rand_CenteredFloat(40.0f) + this->leader->world.pos.y + 40.0f;
        this->target.z += Math_CosS(behind) * offset;
        this->timer = Rand_S16Offset(20, 30);
    } else if (GetDistXZ(&this->actor.world.pos, &desbreko->world.pos) > 100.0f) {
        this->target = desbreko->world.pos;
    }
}

static void Escort_Follow(EnDesbrekoEscort* this, PlayState* play) {
    if (IsLinkInWater(play) && this->calmTimer == 0) {
        Escort_SetupHunt(this);
        return;
    }
    if (Escort_IsWallAhead(this, play)) {
        Escort_SetupTurn(this);
        return;
    }
    Escort_PickFollowSpot(this);
    if (Rand_ZeroOne() < 0.1f) {
        this->isBubbling = true;
    }
    Escort_SwimToward(this, Math_Vec3f_Yaw(&this->actor.world.pos, &this->target), play);
}

static void Escort_Turn(EnDesbrekoEscort* this, PlayState* play) {
    Escort_SwimToward(this, this->targetYaw, play);
    s16 yawLeft = this->actor.world.rot.y - this->targetYaw;

    if (ABS(yawLeft) >= 0x100) {
        return;
    }
    if (Escort_IsWallAhead(this, play)) {
        this->targetYaw += 0x1500 + (s16)Rand_ZeroFloat(5000.0f);
        return;
    }
    if (!IsLinkInWater(play)) {
        this->timer = 10;
    }
    this->actionFunc = Escort_Follow;
}

static void Escort_Hunt(EnDesbrekoEscort* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    EnDesbreko* desbreko = (EnDesbreko*)this->actor.parent;

    if (Escort_IsWallAhead(this, play)) {
        Escort_ChangeAnim(this, 0);
        this->actor.speedXZ = Rand_CenteredFloat(1.0f) + 4.0f;
        Escort_SetupTurn(this);
        return;
    }
    if (!IsLinkInWater(play) || GetDistXZ(&player->actor.world.pos, &desbreko->actor.home.pos) > desbreko->territory) {
        this->calmTimer = 100;
        this->skelAnime.playSpeed = 1.0f;
        Escort_SetupFollow(this);
        return;
    }
    bool isInReach =
        this->actor.xzDistToPlayer < 200.0f && fabsf(player->actor.world.pos.y - this->actor.world.pos.y) < 80.0f;

    if (this->animIndex != (isInReach ? 1 : 0)) {
        Escort_ChangeAnim(this, isInReach ? 1 : 0);
    }
    this->skelAnime.playSpeed = isInReach ? 1.0f : 2.0f;
    if (this->timer == 0) {
        this->timer = Rand_S16Offset(2, 3);
        this->target = player->actor.world.pos;
    }
    if (Rand_ZeroOne() < 0.1f) {
        this->isBubbling = true;
    }
    this->target.y = player->actor.world.pos.y + ESCORT_BITE_HEIGHT + Rand_CenteredFloat(2.0f * this->index + 1.0f);
    Escort_SwimToward(this, Math_Vec3f_Yaw(&this->actor.world.pos, &this->target), play);
}

static void Escort_SetupDying(EnDesbrekoEscort* this) {
    this->isDying = true;
    this->brightness = 0;
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    PlaySfx(&this->actor, MM_SE_EN_BUBLEWALK_DEAD, NA_SE_EN_BUBLEWALK_DEAD);
    this->dyingRoll = Rand_ZeroOne() < 0.5f ? -0x4000 : 0x4000;
    this->dyingYaw = this->actor.world.rot.y;
    this->actor.speedXZ = Rand_ZeroFloat(0.5f);
    this->actor.world.rot.y = Rand_CenteredFloat(0x8000);
    Actor_SetColorFilter(&this->actor, COLORFILTER_RED, 255, COLORFILTER_OPA, 30);
    this->timer = 50;
    Escort_ChangeAnim(this, 0);
    this->actionFunc = Escort_Dying;
}

// Rolls onto its side, drifts up and dissolves into bubbles.
static void Escort_Dying(EnDesbrekoEscort* this, PlayState* play) {
    WaterBox* waterBox;
    bool hasSurfaced = false;

    Math_SmoothStepToS(&this->actor.shape.rot.x, 0, 5, 0x2710, 0x3E8);
    Math_SmoothStepToS(&this->actor.shape.rot.z, this->dyingRoll, 5, 0x2710, 0x3E8);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->dyingYaw, 5, 0x2710, 0x3E8);
    if (WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &this->waterSurface,
                             &waterBox)) {
        hasSurfaced = this->waterSurface - 15.0f <= this->actor.world.pos.y;
        if (!hasSurfaced) {
            Math_ApproachF(&this->actor.world.pos.y, this->waterSurface - 14.0f, 0.3f, 1.0f);
        }
    }
    if (this->timer != 0 && !hasSurfaced) {
        return;
    }
    Math_SmoothStepToS(&this->alpha, 0, 1, 15, 50);
    if (this->alpha < 2) {
        SpawnBubbles(play, &this->actor.world.pos, 10);
        Actor_Kill(&this->actor);
    }
}

static bool Escort_ShouldDie(EnDesbrekoEscort* this) {
    if (this->collider.base.acFlags & AC_HIT) {
        Actor_ApplyDamage(&this->actor);
    }
    return this->actor.colChkInfo.health == 0 || this->actor.parent == NULL ||
           this->actor.parent->colChkInfo.health == 0;
}

static void EnDesbrekoEscort_Init(Actor* thisx, PlayState* play) {
    EnDesbrekoEscort* this = (EnDesbrekoEscort*)thisx;

    Actor_SetScale(&this->actor, 0.01f);
    this->actor.targetMode = 3;
    this->actor.colChkInfo.damageTable = &sEscortDamageTable;
    this->actor.colChkInfo.health = 1;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sEscortSkel, (AnimationHeader*)sEscortSwimAnim,
                       this->jointTable, this->morphTable, ESCORT_LIMB_MAX);
    this->index = this->actor.params & 0xFF;
    this->actor.shape.yOffset = 500.0f;
    this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sEscortCylinderInit);
    this->target = this->actor.world.pos;
    this->brightness = 255;
    this->alpha = DESBREKO_OPAQUE;
    this->turnBonus = Rand_CenteredFloat(this->index * 100.0f + 1000.0f);
    Escort_SetupFollow(this);
}

// Whoever trailed this fish now trails the one it followed.
static void EnDesbrekoEscort_Destroy(Actor* thisx, PlayState* play) {
    EnDesbrekoEscort* this = (EnDesbrekoEscort*)thisx;

    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; actor != NULL; actor = actor->next) {
        EnDesbrekoEscort* follower = (EnDesbrekoEscort*)actor;

        if (actor->id == sEscortActorId && follower->leader == thisx) {
            follower->leader = this->leader;
        }
    }
    Collider_DestroyCylinder(play, &this->collider);
}

static void Escort_EmitBubbles(EnDesbrekoEscort* this, PlayState* play) {
    if (!this->isBubbling) {
        return;
    }
    this->isBubbling = false;
    SpawnBubbles(play, &this->headPos, 1);
}

static void EnDesbrekoEscort_Update(Actor* thisx, PlayState* play) {
    EnDesbrekoEscort* this = (EnDesbrekoEscort*)thisx;

    if (!this->isDying) {
        SkelAnime_Update(&this->skelAnime);
    }
    bool isOrphaned = this->actor.parent == NULL || this->leader == NULL;

    if (!this->isDying && isOrphaned) {
        Escort_SetupDying(this);
    }
    this->actionFunc(this, play);
    if (this->actor.update == NULL) {
        return;
    }
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->calmTimer != 0) {
        this->calmTimer--;
    }
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 10.0f, 10.0f, BGCHECK_UPDATE_WALL_FLOOR_WATER);
    if (this->isDying) {
        return;
    }
    this->actor.shape.rot = this->actor.world.rot;
    if (Escort_ShouldDie(this)) {
        this->collider.base.acFlags &= ~AC_HIT;
        Escort_SetupDying(this);
        return;
    }
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    Escort_EmitBubbles(this, play);
}

static s32 Escort_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx,
                                   Gfx** gfx) {
    EnDesbrekoEscort* this = (EnDesbrekoEscort*)thisx;

    if (limbIndex == ESCORT_LIMB_HEAD) {
        rot->y += (s16)this->swing * -100;
    }
    return false;
}

static void Escort_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx) {
    EnDesbrekoEscort* this = (EnDesbrekoEscort*)thisx;

    if (limbIndex == ESCORT_LIMB_HEAD) {
        Matrix_MultVec3f(&sZeroVec, &this->headPos);
    }
}

static void EnDesbrekoEscort_Draw(Actor* thisx, PlayState* play) {
    EnDesbrekoEscort* this = (EnDesbrekoEscort*)thisx;

    DrawFish(play, &this->skelAnime, this->brightness, this->alpha, Escort_OverrideLimbDraw, Escort_PostLimbDraw, this);
}

// MM's CanKillEnemy(ACTOR_EN_PR) needs Zora fins underwater; OoT reaches it in Iron Boots with a blade or hook.
static bool Desbreko_CanKillInLogic(void* userData) {
    bool canWeaponUnderwater = sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RG_HOOKSHOT);

    return sApi->LogicCanUseItem(RG_IRON_BOOTS) && canWeaponUnderwater;
}

// The Skullfish family soul, shared with mm_enemy_skullfish: both define it alike, so either can register it.
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
    static Vec3s sSoulJointTable[DESBREKO_LIMB_MAX];
    static Vec3s sSoulMorphTable[DESBREKO_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    DrawSoulFlame(play);
    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sDesbrekoSkel,
                           (AnimationHeader*)sDesbrekoSwimAnim, sSoulJointTable, sSoulMorphTable, DESBREKO_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
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
    SOHCustomItemDefinition soul = Z64Items_Define(SKULLFISH_SOUL_ITEM_KEY, gBossSoulTex, NULL);

    soul.displayName = "Soul of Skullfish";
    soul.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    Z64Items_SetTextbox(&soul, "You got the %rSoul of Skullfish%w!&Skullfish can now be defeated.");
    Z64Items_SetGetItemModel(&soul, gBossSoulSkullDL);
    soul.getItemEntry.drawFunc = DrawSoul;
    Z64Items_SetLogic(&soul, &sSoulRandomizer);
    if (!Z64Items_RegisterShared(sApi, &soul)) {
        sApi->Log("Could not register " SKULLFISH_SOUL_ITEM_KEY);
    }
}

static void RegisterEscort(void) {
    SOHActorDefinition escort = { 0 };

    escort.structSize = sizeof(escort);
    escort.key = DESBREKO_ESCORT_KEY;
    escort.description = "Desbreko's Skullfish";
    escort.category = ACTORCAT_ENEMY;
    escort.actorFlags = ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    escort.objectId = OBJECT_GAMEPLAY_KEEP;
    escort.instanceSize = sizeof(EnDesbrekoEscort);
    escort.init = EnDesbrekoEscort_Init;
    escort.destroy = EnDesbrekoEscort_Destroy;
    escort.update = EnDesbrekoEscort_Update;
    escort.draw = EnDesbrekoEscort_Draw;
    sEscortActorId = sApi->RegisterActor(&escort);
}

static void RegisterDesbreko(void) {
    SOHActorDefinition desbreko = { 0 };

    desbreko.structSize = sizeof(desbreko);
    desbreko.key = DESBREKO_KEY;
    desbreko.description = "Desbreko";
    desbreko.category = ACTORCAT_ENEMY;
    desbreko.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    desbreko.objectId = OBJECT_GAMEPLAY_KEEP;
    desbreko.instanceSize = sizeof(EnDesbreko);
    desbreko.init = EnDesbreko_Init;
    desbreko.destroy = EnDesbreko_Destroy;
    desbreko.update = EnDesbreko_Update;
    desbreko.draw = EnDesbreko_Draw;
    desbreko.enemyFlags = SOH_ACTOR_ENEMY | SOH_ACTOR_ENEMY_NEEDS_WATER | SOH_ACTOR_ENEMY_UNDERWATER;
    desbreko.enemyParams = DESBREKO_DEFAULT_ESCORT << 8;
    desbreko.naviHint = "Desbreko&%cThe bony leader of a Skullfish school. Defeat it and the whole school falls "
                        "apart!%w";
    desbreko.soulKey = SKULLFISH_FAMILY_KEY;
    desbreko.logicKill = Desbreko_CanKillInLogic;
    if (sApi->RegisterActor(&desbreko) < 0) {
        sApi->Log("Could not register " DESBREKO_KEY);
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
    RegisterEscort();
    if (sEscortActorId < 0) {
        sApi->Log("Could not register " DESBREKO_ESCORT_KEY);
        return;
    }
    RegisterDesbreko();
}
