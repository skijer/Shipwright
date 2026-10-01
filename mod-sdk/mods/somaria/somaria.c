// Cane of Somaria: conjures constructs that cost points from a budget the cane's level sets.
// Level 1 gives statues and crates, level 2 adds the bird and the pushable block, level 3 the platform.

#include <string.h>

#include "z64items.h"
#include "z64wheel.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Bg_Bdan_Switch/z_bg_bdan_switch.h"
#include "overlays/actors/ovl_Obj_Switch/z_obj_switch.h"

#define SOMARIA_KEY "nei.somaria"
#define SOMARIA_MOD "somaria"
#define SOMARIA_LEVEL_FIELD "level"
#define SOMARIA_MAX_LEVEL 3
#define SOMARIA_POINTS_PER_LEVEL 2
#define SOMARIA_MAX_SUMMONS 8
#define SOMARIA_BUTTON_COUNT 8

#define SOMARIA_CAST_ANIM_SPEED 2.0f
#define SOMARIA_CAST_FIRE_FRAME 8
#define SOMARIA_CAST_TIMEOUT 60
#define SOMARIA_PLACE_DIST 70.0f
#define SOMARIA_PLACE_RAY_UP 60.0f
#define SOMARIA_PLACE_RAY_DOWN 120.0f

#define SOMARIA_GRAVITY -2.0f
#define SOMARIA_MIN_VEL_Y -20.0f
#define SOMARIA_GROW_FRAMES 8
#define SOMARIA_BGCHECK_GROUND 1
#define SOMARIA_HEAVY_SWITCH_RANGE 40.0f
#define SOMARIA_HEAVY_SWITCH_HEIGHT 50.0f

#define SOMARIA_STATUE_SCALE 0.01f
#define SOMARIA_STATUE_RADIUS 25
#define SOMARIA_STATUE_HEIGHT 60
#define ARMOS_LIMB_COUNT 14

#define SOMARIA_CRATE_SCALE 0.1f
#define SOMARIA_CRATE_RADIUS 22
#define SOMARIA_CRATE_HEIGHT 40

#define SOMARIA_BIRD_SCALE 0.02f
#define SOMARIA_BIRD_RADIUS 14
#define SOMARIA_BIRD_HEIGHT 20
#define SOMARIA_BIRD_LIFETIME 360
#define SOMARIA_BIRD_COOLDOWN 120
#define SOMARIA_BIRD_SPEED 7.0f
#define SOMARIA_BIRD_TURN 0x800
#define SOMARIA_BIRD_HOVER 30.0f
#define SOMARIA_BIRD_SEARCH_RANGE 600.0f
#define SOMARIA_BIRD_STRIKE_RANGE 40.0f
#define SOMARIA_BIRD_DAMAGE 2

#define SOMARIA_BLOCK_PARAMS 0x0000
#define SOMARIA_BLOCK_HALF_WIDTH 30.0f
#define SOMARIA_BLOCK_HEIGHT 60.0f

#define SOMARIA_PLATFORM_SCALE 0.1f
#define SOMARIA_PLATFORM_RADIUS 60.0f
#define SOMARIA_PLATFORM_HEIGHT 20.0f
#define SOMARIA_PLATFORM_SPEED 2.5f
#define SOMARIA_PLATFORM_FAST_SPEED 6.0f
#define SOMARIA_PLATFORM_LIFT_SPEED 2.0f
#define SOMARIA_PLATFORM_STILL_SPEED 0.1f

typedef enum {
    SOMARIA_STATUE,
    SOMARIA_CRATE,
    SOMARIA_BIRD,
    SOMARIA_BLOCK,
    SOMARIA_PLATFORM,
    SOMARIA_MODE_MAX,
} SomariaMode;

typedef struct {
    uint8_t cost;
    uint8_t level;
} SomariaModeRule;

typedef struct {
    Actor* actor;
    uint8_t mode;
} SomariaSummon;

typedef struct {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[ARMOS_LIMB_COUNT];
    Vec3s morphTable[ARMOS_LIMB_COUNT];
    ColliderCylinder collider;
    s16 growTimer;
} SomariaStatueActor;

typedef struct {
    Actor actor;
    ColliderCylinder collider;
    s16 growTimer;
    bool isHeld;
} SomariaCrateActor;

typedef struct {
    Actor actor;
    SkelAnime skelAnime;
    ColliderCylinder collider;
    Actor* target;
    s16 life;
} SomariaBirdActor;

typedef struct {
    DynaPolyActor dyna;
    f32 speed;
    bool isPiloted;
} SomariaPlatformActor;

// Cost is what a construct holds from the budget for as long as it stands; level is what unlocks it.
static const SomariaModeRule sModeRules[SOMARIA_MODE_MAX] = {
    { 1, 1 }, { 2, 1 }, { 2, 2 }, { 3, 2 }, { 4, 3 },
};

static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gCaneOfSomariaNameTex";
static const ALIGN_ASSET(2) char sCaneDL[] = "__OTR__objects/object_somaria/g_somaria_cane_dl";
static const ALIGN_ASSET(2) char sCaneGiveDL[] = "__OTR__objects/object_somaria/g_somaria_cane_give_dl";
static const ALIGN_ASSET(2) char sCastAnim[] = "__OTR__misc/link_animetion/gPlayerAnim_nei_somaria";
static const ALIGN_ASSET(2) char sElegyShellDL[] = "__OTR__objects/gameplay_keep/gElegyShellHumanDL";
static const ALIGN_ASSET(2) char sArmosSkel[] = "__OTR__objects/object_am/gArmosSkel";
static const ALIGN_ASSET(2) char sArmosAnim[] = "__OTR__objects/object_am/gArmosRicochetAnim";
static const ALIGN_ASSET(2) char sCrateDL[] = "__OTR__objects/gameplay_dangeon_keep/gSmallWoodenBoxDL";
static const ALIGN_ASSET(2) char sBirdSkel[] = "__OTR__objects/object_bird/gBirdSkel";
static const ALIGN_ASSET(2) char sBirdAnim[] = "__OTR__objects/object_bird/gBirdFlyAnim";
static const ALIGN_ASSET(2) char sPlatformDL[] = "__OTR__objects/object_d_lift/gCollapsingPlatformDL";
static const ALIGN_ASSET(2) char sPlatformCol[] = "__OTR__objects/object_d_lift/gCollapsingPlatformCol";

static const ALIGN_ASSET(2) char sModeIcons[SOMARIA_MODE_MAX][64] = {
    "__OTR__textures/icon_item_custom/gSomariaIconEnAmTex",
    "__OTR__textures/icon_item_custom/gSomariaIconObjKibakoTex",
    "__OTR__textures/icon_item_custom/gSomariaIconEnCrowTex",
    "__OTR__textures/icon_item_custom/gSomariaIconObjOshihikiTex",
    "__OTR__textures/icon_item_custom/gSomariaIconObjLiftTex",
};

static const Z64WheelEntry sWheelEntries[SOMARIA_MODE_MAX] = {
    { sModeIcons[SOMARIA_STATUE], "Statue" },     { sModeIcons[SOMARIA_CRATE], "Crate" },
    { sModeIcons[SOMARIA_BIRD], "Bird" },         { sModeIcons[SOMARIA_BLOCK], "Block" },
    { sModeIcons[SOMARIA_PLATFORM], "Platform" },
};

static const u16 sItemButtons[SOMARIA_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                        BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnSceneInit",    "OnLoadFile",
                                              "OnActorDraw",    "OnActorDrawEnd", "OnInterfaceDrawEnd" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static ColliderCylinderInit sStatueColliderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0xFFCFFFFF, 0, 0 }, TOUCH_NONE, BUMP_ON | BUMP_HOOKABLE, OCELEM_ON },
    { SOMARIA_STATUE_RADIUS, SOMARIA_STATUE_HEIGHT, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sCrateColliderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0xFFCFFFFF, 0, 0 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
    { SOMARIA_CRATE_RADIUS, SOMARIA_CRATE_HEIGHT, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sBirdColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_DEKU_STICK | DMG_SLINGSHOT | DMG_BOOMERANG, 0x00, 0x01 },
      { 0, 0, 0 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { SOMARIA_BIRD_RADIUS, SOMARIA_BIRD_HEIGHT, 0, { 0, 0, 0 } },
};

static const SOHModApi* sApi;
static SomariaSummon sSummons[SOMARIA_MAX_SUMMONS];
static int16_t sStatueId = -1;
static int16_t sCrateId = -1;
static int16_t sBirdId = -1;
static int16_t sPlatformId = -1;
static uint8_t sLevel;
static bool sIsCaneOut;
static bool sIsCastPending;
static bool sIsCasting;
static s16 sCastTimer;
static s16 sBirdCooldown;
static Vec3f sGhostPos;
static s16 sGhostYaw;
static bool sIsGhostValid;
static SomariaPlatformActor* sPlatformUnderPlayer;
static SomariaPlatformActor* sPilotedPlatform;

s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);
LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
u8 ResourceMgr_FileExists(const char* resourcePath);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void PlayErrorSfx(void) {
    Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
}

// Level and budget.

static uint8_t CurrentMode(void) {
    uint32_t selection = Z64Wheel_GetSelection();

    return selection < SOMARIA_MODE_MAX ? (uint8_t)selection : SOMARIA_STATUE;
}

static uint8_t BudgetPoints(void) {
    return sLevel * SOMARIA_POINTS_PER_LEVEL;
}

static void ForgetDeadSummons(void) {
    for (uint8_t index = 0; index < SOMARIA_MAX_SUMMONS; index++) {
        if (sSummons[index].actor != NULL && sSummons[index].actor->update == NULL) {
            if (sSummons[index].mode == SOMARIA_BIRD) {
                sBirdCooldown = SOMARIA_BIRD_COOLDOWN;
            }
            sSummons[index].actor = NULL;
        }
    }
}

static uint8_t SpentPoints(void) {
    uint8_t spent = 0;

    ForgetDeadSummons();
    for (uint8_t index = 0; index < SOMARIA_MAX_SUMMONS; index++) {
        if (sSummons[index].actor != NULL) {
            spent += sModeRules[sSummons[index].mode].cost;
        }
    }
    return spent;
}

static bool CanAffordMode(uint8_t mode) {
    return (SpentPoints() + sModeRules[mode].cost) <= BudgetPoints();
}

static void TrackSummon(Actor* summon, uint8_t mode) {
    for (uint8_t index = 0; index < SOMARIA_MAX_SUMMONS; index++) {
        if (sSummons[index].actor == NULL) {
            sSummons[index].actor = summon;
            sSummons[index].mode = mode;
            return;
        }
    }
}

static bool IsTrackedSummon(Actor* actor) {
    for (uint8_t index = 0; index < SOMARIA_MAX_SUMMONS; index++) {
        if (sSummons[index].actor == actor) {
            return true;
        }
    }
    return false;
}

static void SyncWheelUnlocks(void) {
    for (uint8_t mode = 0; mode < SOMARIA_MODE_MAX; mode++) {
        Z64Wheel_SetEntryUnlocked(mode, sModeRules[mode].level <= sLevel);
    }
    if (sModeRules[CurrentMode()].level > sLevel) {
        Z64Wheel_SetSelection(SOMARIA_STATUE);
    }
}

static void LoadLevel(void) {
    sLevel = 0;
    sApi->StorageGet(SOMARIA_MOD, SOMARIA_LEVEL_FIELD, &sLevel, sizeof(sLevel));
    if (sLevel > SOMARIA_MAX_LEVEL) {
        sLevel = SOMARIA_MAX_LEVEL;
    }
    SyncWheelUnlocks();
}

// Every cane found is the same pickup, so the level is what the mod counts, not which key arrived.
static void RaiseLevel(const char* key) {
    if (sLevel < SOMARIA_MAX_LEVEL) {
        sLevel++;
    }
    sApi->StorageSet(SOMARIA_MOD, SOMARIA_LEVEL_FIELD, &sLevel, sizeof(sLevel));
    SyncWheelUnlocks();
}

static void ForgetLevel(const char* key) {
    sLevel = 0;
    sApi->StorageSet(SOMARIA_MOD, SOMARIA_LEVEL_FIELD, &sLevel, sizeof(sLevel));
    SyncWheelUnlocks();
}

// Shared construct behaviour.

// Vanilla gates YELLOW_HEAVY switches behind something heavy enough to hold them down, which is exactly
// what a conjured statue or crate is for.
static void PressHeavySwitches(Actor* construct, PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_SWITCH].head; actor != NULL; actor = actor->next) {
        if (actor->id != ACTOR_BG_BDAN_SWITCH || (actor->params & 0xFF) != YELLOW_HEAVY) {
            continue;
        }
        f32 deltaX = construct->world.pos.x - actor->world.pos.x;
        f32 deltaZ = construct->world.pos.z - actor->world.pos.z;
        f32 deltaY = construct->world.pos.y - actor->world.pos.y;
        u8 switchFlag = (actor->params >> 8) & 0x3F;

        if (SQ(deltaX) + SQ(deltaZ) >= SQ(SOMARIA_HEAVY_SWITCH_RANGE) || deltaY < 0.0f ||
            deltaY >= SOMARIA_HEAVY_SWITCH_HEIGHT || Flags_GetSwitch(play, switchFlag)) {
            continue;
        }
        Flags_SetSwitch(play, switchFlag);
        PlaySfxAt(NA_SE_EV_FOOT_SWITCH, &construct->world.pos);
        Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    }
}

static void GrowIntoPlace(Actor* actor, s16* growTimer, f32 fullScale) {
    if (*growTimer <= 0) {
        return;
    }
    (*growTimer)--;
    if (*growTimer == 0) {
        Actor_SetScale(actor, fullScale);
        return;
    }
    f32 grown = fullScale * (1.0f - (f32)*growTimer / SOMARIA_GROW_FRAMES);

    Actor_SetScale(actor, grown);
}

// Statue.

static bool HasElegyShell(void) {
    return ResourceMgr_FileExists(sElegyShellDL);
}

static void StatueInit(Actor* thisx, PlayState* play) {
    SomariaStatueActor* this = (SomariaStatueActor*)thisx;

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sStatueColliderInit);
    if (!HasElegyShell()) {
        SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sArmosSkel, (AnimationHeader*)sArmosAnim,
                       this->jointTable, this->morphTable, ARMOS_LIMB_COUNT);
        this->skelAnime.playSpeed = 0.0f;
    }
    thisx->gravity = SOMARIA_GRAVITY;
    thisx->minVelocityY = SOMARIA_MIN_VEL_Y;
    thisx->shape.shadowDraw = ActorShadow_DrawCircle;
    thisx->shape.shadowScale = 40.0f;
    this->growTimer = SOMARIA_GROW_FRAMES;
    Actor_SetScale(thisx, 0.0f);
}

static void StatueDestroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((SomariaStatueActor*)thisx)->collider);
}

static void StatueUpdate(Actor* thisx, PlayState* play) {
    SomariaStatueActor* this = (SomariaStatueActor*)thisx;

    GrowIntoPlace(thisx, &this->growTimer, SOMARIA_STATUE_SCALE);
    Math_StepToF(&thisx->speedXZ, 0.0f, 1.0f);
    Actor_MoveXZGravity(thisx);
    Actor_UpdateBgCheckInfo(play, thisx, 30.0f, 15.0f, 0.0f, 0x1D);
    if (thisx->bgCheckFlags & SOMARIA_BGCHECK_GROUND) {
        PressHeavySwitches(thisx, play);
    }
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 15.0f;
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
}

// MM's own draw sets segment 0x0C to no-op lists, and the shell's display list calls into it.
static Gfx sSegment0xCNoop[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

static void StatueDraw(Actor* thisx, PlayState* play) {
    SomariaStatueActor* this = (SomariaStatueActor*)thisx;

    if (thisx->scale.x <= 0.001f) {
        return;
    }
    if (!HasElegyShell()) {
        SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, NULL, NULL, thisx);
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sSegment0xCNoop);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    CLOSE_DISPS(play->state.gfxCtx);

    Gfx_DrawDListOpa(play, (Gfx*)sElegyShellDL);
}

// Crate.

static void CrateInit(Actor* thisx, PlayState* play) {
    SomariaCrateActor* this = (SomariaCrateActor*)thisx;

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCrateColliderInit);
    thisx->gravity = SOMARIA_GRAVITY;
    thisx->minVelocityY = SOMARIA_MIN_VEL_Y;
    thisx->shape.shadowDraw = ActorShadow_DrawCircle;
    thisx->shape.shadowScale = 30.0f;
    this->growTimer = SOMARIA_GROW_FRAMES;
    Actor_SetScale(thisx, 0.0f);
}

static void CrateDestroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((SomariaCrateActor*)thisx)->collider);
}

// Unlike a real crate this one never breaks: thrown, it simply lands and waits to be picked up again.
static void CrateUpdate(Actor* thisx, PlayState* play) {
    SomariaCrateActor* this = (SomariaCrateActor*)thisx;

    GrowIntoPlace(thisx, &this->growTimer, SOMARIA_CRATE_SCALE);
    if (this->isHeld) {
        if (!Actor_HasNoParent(thisx, play)) {
            return;
        }
        this->isHeld = false;
        PlaySfxAt(NA_SE_EV_PUT_DOWN_WOODBOX, &thisx->world.pos);
    } else if (Actor_HasParent(thisx, play)) {
        this->isHeld = true;
        PlaySfxAt(NA_SE_PL_PULL_UP_WOODBOX, &thisx->world.pos);
        return;
    } else if (this->growTimer <= 0) {
        Actor_OfferCarry(thisx, play);
    }
    Math_StepToF(&thisx->speedXZ, 0.0f, 0.5f);
    Actor_MoveXZGravity(thisx);
    Actor_UpdateBgCheckInfo(play, thisx, 20.0f, 20.0f, 0.0f, 0x1D);
    if (thisx->bgCheckFlags & SOMARIA_BGCHECK_GROUND) {
        thisx->velocity.y = 0.0f;
        PressHeavySwitches(thisx, play);
    }
    thisx->focus.pos = thisx->world.pos;
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
}

static void CrateDraw(Actor* thisx, PlayState* play) {
    if (thisx->scale.x > 0.001f) {
        Gfx_DrawDListOpa(play, (Gfx*)sCrateDL);
    }
}

// Bird.

static bool IsPressableSwitch(Actor* actor) {
    if (actor->id != ACTOR_OBJ_SWITCH || actor->update == NULL) {
        return false;
    }
    u8 type = actor->params & 7;

    return type == OBJSWITCH_TYPE_CRYSTAL || type == OBJSWITCH_TYPE_CRYSTAL_TARGETABLE ||
           type == OBJSWITCH_TYPE_FLOOR || type == OBJSWITCH_TYPE_FLOOR_RUSTY;
}

static Actor* FindNearestOfCategory(PlayState* play, Vec3f* from, uint8_t category, bool switchesOnly) {
    Actor* nearest = NULL;
    f32 nearestDistance = SQ(SOMARIA_BIRD_SEARCH_RANGE);

    for (Actor* actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
        if (actor->update == NULL || (switchesOnly && !IsPressableSwitch(actor))) {
            continue;
        }
        f32 distance = Math_Vec3f_DistXYZ(from, &actor->world.pos);

        if (SQ(distance) < nearestDistance) {
            nearestDistance = SQ(distance);
            nearest = actor;
        }
    }
    return nearest;
}

// The bird goes for what Link is locked on to first, then the nearest enemy, and failing both it presses a
// switch — which is the only reason it bothers with anything that is not alive.
static Actor* PickBirdTarget(Actor* bird, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (player->focusActor != NULL && player->focusActor->update != NULL) {
        return player->focusActor;
    }

    Actor* enemy = FindNearestOfCategory(play, &bird->world.pos, ACTORCAT_ENEMY, false);

    if (enemy != NULL) {
        return enemy;
    }
    return FindNearestOfCategory(play, &bird->world.pos, ACTORCAT_SWITCH, true);
}

static void BirdInit(Actor* thisx, PlayState* play) {
    SomariaBirdActor* this = (SomariaBirdActor*)thisx;

    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sBirdSkel, (AnimationHeader*)sBirdAnim, NULL, NULL, 0);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sBirdColliderInit);
    this->collider.info.toucher.damage = SOMARIA_BIRD_DAMAGE;
    this->life = SOMARIA_BIRD_LIFETIME;
    thisx->gravity = 0.0f;
    Actor_SetScale(thisx, SOMARIA_BIRD_SCALE);
}

static void BirdDestroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((SomariaBirdActor*)thisx)->collider);
}

static void BirdFlyTowards(SomariaBirdActor* this, Vec3f* goal) {
    Actor* actor = &this->actor;
    s16 yawToGoal = Math_Vec3f_Yaw(&actor->world.pos, goal);
    f32 distance = Math_Vec3f_DistXYZ(&actor->world.pos, goal);

    Math_ScaledStepToS(&actor->world.rot.y, yawToGoal, SOMARIA_BIRD_TURN);
    actor->shape.rot.y = actor->world.rot.y;
    actor->speedXZ = distance < SOMARIA_BIRD_STRIKE_RANGE ? 2.0f : SOMARIA_BIRD_SPEED;
    Math_ApproachF(&actor->world.pos.y, goal->y + SOMARIA_BIRD_HOVER, 0.2f, SOMARIA_BIRD_SPEED);
    Actor_MoveXZGravity(actor);
}

static void BirdCircleOverPlayer(SomariaBirdActor* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f circle = player->actor.world.pos;
    s16 angle = (s16)(play->gameplayFrames * 0x300);

    circle.x += Math_SinS(angle) * 80.0f;
    circle.z += Math_CosS(angle) * 80.0f;
    circle.y += 40.0f;
    BirdFlyTowards(this, &circle);
}

static void BirdUpdate(Actor* thisx, PlayState* play) {
    SomariaBirdActor* this = (SomariaBirdActor*)thisx;

    SkelAnime_Update(&this->skelAnime);
    if (--this->life <= 0) {
        PlaySfxAt(NA_SE_PL_MAGIC_SOUL_BALL, &thisx->world.pos);
        Actor_Kill(thisx);
        return;
    }
    if (this->target == NULL || this->target->update == NULL) {
        this->target = PickBirdTarget(thisx, play);
    }
    if (this->target == NULL) {
        BirdCircleOverPlayer(this, play);
        return;
    }
    BirdFlyTowards(this, &this->target->world.pos);

    // A switch is pressed by landing on it; anything alive is hit by the touch collider instead.
    if (IsPressableSwitch(this->target)) {
        thisx->flags |= ACTOR_FLAG_CAN_PRESS_SWITCHES;
    }
    Collider_UpdateCylinder(thisx, &this->collider);
    this->collider.base.atFlags |= AT_ON;
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        PlaySfxAt(NA_SE_EN_DODO_M_GND, &thisx->world.pos);
        this->target = NULL;
    }
}

static void BirdDraw(Actor* thisx, PlayState* play) {
    SomariaBirdActor* this = (SomariaBirdActor*)thisx;

    SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, NULL, NULL, thisx);
}

// Platform.

static bool IsPlayerStandingStill(PlayState* play, SomariaPlatformActor* this) {
    Player* player = GET_PLAYER(play);

    return DynaPolyActor_IsPlayerOnTop(&this->dyna) && fabsf(player->linearVelocity) < SOMARIA_PLATFORM_STILL_SPEED;
}

static void PlatformStartPiloting(SomariaPlatformActor* this, PlayState* play) {
    this->isPiloted = true;
    this->speed = 0.0f;
    sPilotedPlatform = this;
    sApi->BlockPlayerInput(SOMARIA_MOD, 0xFFFF, true);
    Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
}

static void PlatformStopPiloting(SomariaPlatformActor* this) {
    this->isPiloted = false;
    this->speed = 0.0f;
    sPilotedPlatform = NULL;
    sApi->ReleasePlayerInput(SOMARIA_MOD);
    Sfx_PlaySfxCentered(NA_SE_SY_PIECE_OF_HEART);
}

// Steering is read raw because the player's own copy of the input is blocked while he rides.
static void PlatformSteer(SomariaPlatformActor* this, PlayState* play) {
    Input* input = &play->state.input[0];
    u16 held = input->cur.button;
    s16 cameraYaw = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play));
    f32 forward = 0.0f;
    f32 strafe = 0.0f;
    f32 lift = 0.0f;

    if (held & BTN_DUP) {
        forward += 1.0f;
    }
    if (held & BTN_DDOWN) {
        forward -= 1.0f;
    }
    if (held & BTN_DRIGHT) {
        strafe += 1.0f;
    }
    if (held & BTN_DLEFT) {
        strafe -= 1.0f;
    }
    if (held & BTN_R) {
        lift += 1.0f;
    }
    if (held & BTN_L) {
        lift -= 1.0f;
    }
    f32 topSpeed = (held & BTN_A) ? SOMARIA_PLATFORM_FAST_SPEED : SOMARIA_PLATFORM_SPEED;

    Math_ApproachF(&this->speed, topSpeed, 0.3f, 1.0f);
    this->dyna.actor.world.pos.x += (Math_SinS(cameraYaw) * forward + Math_CosS(cameraYaw) * strafe) * this->speed;
    this->dyna.actor.world.pos.z += (Math_CosS(cameraYaw) * forward - Math_SinS(cameraYaw) * strafe) * this->speed;
    this->dyna.actor.world.pos.y += lift * SOMARIA_PLATFORM_LIFT_SPEED;
    if (forward == 0.0f && strafe == 0.0f && lift == 0.0f) {
        this->speed = 0.0f;
    }
}

static void PlatformInit(Actor* thisx, PlayState* play) {
    SomariaPlatformActor* this = (SomariaPlatformActor*)thisx;
    CollisionHeader* header = NULL;

    DynaPolyActor_Init(&this->dyna, DPM_PLAYER);
    CollisionHeader_GetVirtual((CollisionHeader*)sPlatformCol, &header);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, header);
    Actor_SetScale(thisx, SOMARIA_PLATFORM_SCALE);
}

static void PlatformDestroy(Actor* thisx, PlayState* play) {
    SomariaPlatformActor* this = (SomariaPlatformActor*)thisx;

    if (this->isPiloted) {
        PlatformStopPiloting(this);
    }
    if (sPlatformUnderPlayer == this) {
        sPlatformUnderPlayer = NULL;
    }
    if (this->dyna.bgId != BGACTOR_NEG_ONE) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    }
}

// The prompt and the button that answers it live in the player hook: a BG actor updates before the player,
// who would overwrite whatever do-action this set.
static void PlatformUpdate(Actor* thisx, PlayState* play) {
    SomariaPlatformActor* this = (SomariaPlatformActor*)thisx;

    if (this->isPiloted) {
        if (!DynaPolyActor_IsPlayerOnTop(&this->dyna) || (play->state.input[0].press.button & BTN_B)) {
            PlatformStopPiloting(this);
            return;
        }
        PlatformSteer(this, play);
        return;
    }
    if (IsPlayerStandingStill(play, this)) {
        sPlatformUnderPlayer = this;
    }
}

static void PlatformDraw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, 210, 70, 70, 255);
    CLOSE_DISPS(play->state.gfxCtx);

    Gfx_DrawDListOpa(play, (Gfx*)sPlatformDL);
}

// Placement.

static bool IsAimedMode(uint8_t mode) {
    return mode == SOMARIA_BLOCK || mode == SOMARIA_PLATFORM;
}

// Placement follows the camera rather than Link's body, so a construct lands where the player is looking.
static s16 CameraYaw(PlayState* play, Player* player) {
    Vec3f eye = play->view.eye;
    Vec3f at = play->view.lookAt;

    if (fabsf(at.x - eye.x) < 0.001f && fabsf(at.z - eye.z) < 0.001f) {
        return player->actor.shape.rot.y;
    }
    return Math_Vec3f_Yaw(&eye, &at);
}

static bool IsGroundUnder(PlayState* play, Player* player, Vec3f* pos) {
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;
    Vec3f rayFrom = *pos;

    rayFrom.y += SOMARIA_PLACE_RAY_UP;

    f32 floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bgId, &player->actor, &rayFrom);

    if (floorY <= BGCHECK_Y_MIN || (pos->y - floorY) > SOMARIA_PLACE_RAY_DOWN) {
        return false;
    }
    pos->y = floorY;
    return true;
}

static bool IsClearOfPlayerAndSummons(PlayState* play, Player* player, Vec3f* pos, f32 radius, f32 height) {
    f32 deltaY = pos->y - player->actor.world.pos.y;

    if (Math_Vec3f_DistXZ(pos, &player->actor.world.pos) < (radius + 22.0f) && deltaY > -height && deltaY < height) {
        return false;
    }
    for (uint8_t index = 0; index < SOMARIA_MAX_SUMMONS; index++) {
        Actor* other = sSummons[index].actor;

        if (other == NULL) {
            continue;
        }
        f32 otherDeltaY = pos->y - other->world.pos.y;

        if (Math_Vec3f_DistXZ(pos, &other->world.pos) < (radius + SOMARIA_BLOCK_HALF_WIDTH) && otherDeltaY > -height &&
            otherDeltaY < height) {
            return false;
        }
    }
    return true;
}

static void UpdateGhost(Player* player, PlayState* play, uint8_t mode) {
    s16 yaw = CameraYaw(play, player);
    Vec3f pos = player->actor.world.pos;

    pos.x += Math_SinS(yaw) * SOMARIA_PLACE_DIST;
    pos.z += Math_CosS(yaw) * SOMARIA_PLACE_DIST;
    sGhostYaw = yaw;

    // The platform is meant to float: it needs no floor, and half-buried in a wall it makes a ledge.
    if (mode == SOMARIA_PLATFORM) {
        pos.y = player->actor.world.pos.y + 10.0f;
        sGhostPos = pos;
        sIsGhostValid = true;
        return;
    }
    sIsGhostValid = IsGroundUnder(play, player, &pos) &&
                    IsClearOfPlayerAndSummons(play, player, &pos, SOMARIA_BLOCK_HALF_WIDTH, SOMARIA_BLOCK_HEIGHT);
    sGhostPos = pos;
}

static Vtx sGhostVtx[] = {
    VTX(-1, -1, -1, 0, 0, 0, 0, 0, 255), VTX(1, -1, -1, 0, 0, 0, 0, 0, 255), VTX(1, -1, 1, 0, 0, 0, 0, 0, 255),
    VTX(-1, -1, 1, 0, 0, 0, 0, 0, 255),  VTX(-1, 1, -1, 0, 0, 0, 0, 0, 255), VTX(1, 1, -1, 0, 0, 0, 0, 0, 255),
    VTX(1, 1, 1, 0, 0, 0, 0, 0, 255),    VTX(-1, 1, 1, 0, 0, 0, 0, 0, 255),
};

static Gfx sGhostDL[] = {
    gsSPVertex(sGhostVtx, 8, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(4, 6, 5, 0, 4, 7, 6, 0),
    gsSP2Triangles(0, 5, 1, 0, 0, 4, 5, 0),
    gsSP2Triangles(1, 6, 2, 0, 1, 5, 6, 0),
    gsSP2Triangles(2, 7, 3, 0, 2, 6, 7, 0),
    gsSP2Triangles(3, 4, 0, 0, 3, 7, 4, 0),
    gsSPEndDisplayList(),
};

static void DrawGhost(PlayState* play, uint8_t mode) {
    f32 radius = mode == SOMARIA_PLATFORM ? SOMARIA_PLATFORM_RADIUS : SOMARIA_BLOCK_HALF_WIDTH;
    f32 height = mode == SOMARIA_PLATFORM ? SOMARIA_PLATFORM_HEIGHT : SOMARIA_BLOCK_HEIGHT;
    f32 pulse = 0.94f + 0.06f * Math_SinS((s16)(play->gameplayFrames * 1500));

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(sGhostPos.x, sGhostPos.y + height * 0.5f, sGhostPos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(sGhostYaw), MTXMODE_APPLY);
    Matrix_Scale(radius * pulse, height * 0.5f * pulse, radius * pulse, MTXMODE_APPLY);
    if (sIsGhostValid) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 90, 170, 255, 110);
        gDPSetEnvColor(POLY_XLU_DISP++, 20, 60, 180, 110);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 70, 70, 110);
        gDPSetEnvColor(POLY_XLU_DISP++, 150, 0, 0, 110);
    }
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, sGhostDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Summoning.

static Actor* SpawnRegistered(PlayState* play, int16_t actorId, Vec3f* pos, s16 yaw) {
    if (actorId < 0) {
        return NULL;
    }
    return Actor_Spawn(&play->actorCtx, play, actorId, pos->x, pos->y, pos->z, 0, yaw, 0, 0);
}

// The pushable block is the vanilla actor: its object only lives in dungeons, so outside one the bank is
// requested and the next press lands.
static Actor* SpawnBlock(PlayState* play, Vec3f* pos, s16 yaw) {
    if (Object_GetIndex(&play->objectCtx, OBJECT_GAMEPLAY_DANGEON_KEEP) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_GAMEPLAY_DANGEON_KEEP);
        return NULL;
    }
    return Actor_Spawn(&play->actorCtx, play, ACTOR_OBJ_OSHIHIKI, pos->x, pos->y, pos->z, 0, yaw, 0,
                       SOMARIA_BLOCK_PARAMS);
}

static Actor* SpawnMode(PlayState* play, Player* player, uint8_t mode) {
    Vec3f pos = player->actor.world.pos;
    s16 yaw = player->actor.shape.rot.y;

    switch (mode) {
        case SOMARIA_STATUE:
            return SpawnRegistered(play, sStatueId, &pos, yaw);
        case SOMARIA_CRATE:
            pos.x += Math_SinS(yaw) * SOMARIA_PLACE_DIST;
            pos.z += Math_CosS(yaw) * SOMARIA_PLACE_DIST;
            return SpawnRegistered(play, sCrateId, &pos, yaw);
        case SOMARIA_BIRD:
            pos.y += 40.0f;
            return SpawnRegistered(play, sBirdId, &pos, yaw);
        case SOMARIA_BLOCK:
            return SpawnBlock(play, &sGhostPos, sGhostYaw);
        case SOMARIA_PLATFORM:
            return SpawnRegistered(play, sPlatformId, &sGhostPos, sGhostYaw);
        default:
            return NULL;
    }
}

static bool CanSummonMode(uint8_t mode) {
    if (sModeRules[mode].level > sLevel) {
        return false;
    }
    if (mode == SOMARIA_BIRD && sBirdCooldown > 0) {
        return false;
    }
    if (IsAimedMode(mode) && !sIsGhostValid) {
        return false;
    }
    return CanAffordMode(mode);
}

static void FireCast(Player* player, PlayState* play) {
    uint8_t mode = CurrentMode();
    Actor* summon = SpawnMode(play, player, mode);

    if (summon == NULL) {
        PlayErrorSfx();
        return;
    }
    TrackSummon(summon, mode);
    summon->room = -1;

    Vec3f flash = summon->world.pos;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    flash.y += 20.0f;
    EffectSsBlast_SpawnWhiteShockwave(play, &flash, &zero, &zero);
    PlaySfxAt(NA_SE_PL_MAGIC_SOUL_BALL, &summon->world.pos);
}

// The cane itself.

static u16 FindEquippedButtonMask(void) {
    for (uint8_t button = 0; button < SOMARIA_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);

        if (equipped != NULL && strcmp(equipped, SOMARIA_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static void TakeOutCane(PlayState* play, Player* player) {
    sIsCaneOut = true;
    sIsCasting = false;
    sIsCastPending = false;
    sCastTimer = 0;
}

static void PutCaneAway(Player* player, PlayState* play) {
    sIsCaneOut = false;
    sIsCasting = false;
    sIsCastPending = false;
    sIsGhostValid = false;
}

// The press only marks the cast: resolving it here would run inside the player's action handler list,
// where the animation about to start is overwritten the same frame.
static void PressCane(Player* player, PlayState* play) {
    if (Z64Wheel_IsOpen() || sIsCasting) {
        return;
    }
    sIsCastPending = true;
}

static bool CanUseCane(Player* player, PlayState* play) {
    return !(player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON) && sPilotedPlatform == NULL;
}

static void StartCast(Player* player, PlayState* play) {
    LinkAnimationHeader* anim = ResourceMgr_LoadPlayerAnimAsHeader(sCastAnim);

    sIsCastPending = false;
    if (!CanSummonMode(CurrentMode())) {
        PlayErrorSfx();
        return;
    }
    if (anim == NULL) {
        FireCast(player, play);
        return;
    }
    LinkAnimation_PlayOnceSetSpeed(play, &player->upperSkelAnime, anim, SOMARIA_CAST_ANIM_SPEED);
    sIsCasting = true;
    sCastTimer = 0;
}

// The cast lives in the upper action so Link keeps walking with it, and returning 1 is what claims his arms.
static s32 UpdateCane(Player* player, PlayState* play) {
    if (!sIsCaneOut || !sIsCasting) {
        return 0;
    }
    if (++sCastTimer > SOMARIA_CAST_TIMEOUT) {
        sIsCasting = false;
        return 0;
    }
    if (sCastTimer == SOMARIA_CAST_FIRE_FRAME) {
        FireCast(player, play);
    }
    if (LinkAnimation_Update(play, &player->upperSkelAnime)) {
        sIsCasting = false;
    }
    return 1;
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    Matrix_Scale(0.25f, 0.25f, 0.25f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sCaneGiveDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Outside the eight block dungeons ObjOshihiki reads its env colour from mREG(13..15), which are 0 there:
// a summoned block would render pure black.
static void TintSummonedBlock(Actor* actor, PlayState* play) {
    if (!IsTrackedSummon(actor)) {
        return;
    }
    mREG(13) = 150;
    mREG(14) = 120;
    mREG(15) = 120;
}

static void UpdatePlatformPrompt(PlayState* play) {
    if (sPilotedPlatform != NULL) {
        Interface_SetDoAction(play, DO_ACTION_FASTER);
        return;
    }
    if (sPlatformUnderPlayer == NULL) {
        return;
    }
    Interface_SetDoAction(play, DO_ACTION_ENTER);
    if (play->state.input[0].press.button & BTN_A) {
        PlatformStartPiloting(sPlatformUnderPlayer, play);
    }
    sPlatformUnderPlayer = NULL;
}

static void UpdateSomaria(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (play == NULL || player == NULL) {
        return;
    }
    UpdatePlatformPrompt(play);
    ForgetDeadSummons();
    if (sBirdCooldown > 0) {
        sBirdCooldown--;
    }
    if (FindEquippedButtonMask() == 0 || !sIsCaneOut) {
        sIsGhostValid = false;
        return;
    }
    uint8_t mode = CurrentMode();

    if (IsAimedMode(mode) && !sIsCasting) {
        UpdateGhost(player, play, mode);
    } else {
        sIsGhostValid = false;
    }
    if (sIsCastPending && !sIsCasting) {
        StartCast(player, play);
    }
}

static void DrawSomariaGhost(Actor* actor, PlayState* play) {
    uint8_t mode = CurrentMode();

    if (sIsCaneOut && IsAimedMode(mode) && !sIsCasting) {
        DrawGhost(play, mode);
    }
}

// A scene change takes every construct with it, and the platform must not keep the player's input.
static void ForgetSceneState(int16_t sceneNum) {
    for (uint8_t index = 0; index < SOMARIA_MAX_SUMMONS; index++) {
        sSummons[index].actor = NULL;
    }
    sBirdCooldown = 0;
    sIsGhostValid = false;
    sIsCasting = false;
    sIsCastPending = false;
    sPlatformUnderPlayer = NULL;
    if (sPilotedPlatform != NULL) {
        sPilotedPlatform = NULL;
        sApi->ReleasePlayerInput(SOMARIA_MOD);
    }
}

static void ReloadLevel(int32_t fileNum) {
    LoadLevel();
}

static int16_t RegisterSummonActor(const char* key, const char* description, uint8_t category, uint32_t flags,
                                   uint32_t instanceSize, ActorFunc init, ActorFunc destroy, ActorFunc update,
                                   ActorFunc draw) {
    SOHActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = key;
    definition.description = description;
    definition.category = category;
    definition.actorFlags = flags;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = instanceSize;
    definition.init = init;
    definition.destroy = destroy;
    definition.update = update;
    definition.draw = draw;
    return sApi->RegisterActor(&definition);
}

static bool RegisterSummonActors(void) {
    uint32_t propFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED |
                         ACTOR_FLAG_CAN_PRESS_SWITCHES | ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;

    sStatueId = RegisterSummonActor("nei.somaria.statue", "Cane of Somaria Statue", ACTORCAT_PROP, propFlags,
                                    sizeof(SomariaStatueActor), StatueInit, StatueDestroy, StatueUpdate, StatueDraw);
    sCrateId = RegisterSummonActor("nei.somaria.crate", "Cane of Somaria Crate", ACTORCAT_PROP, propFlags,
                                   sizeof(SomariaCrateActor), CrateInit, CrateDestroy, CrateUpdate, CrateDraw);
    sBirdId = RegisterSummonActor("nei.somaria.bird", "Cane of Somaria Bird", ACTORCAT_ITEMACTION,
                                  ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED,
                                  sizeof(SomariaBirdActor), BirdInit, BirdDestroy, BirdUpdate, BirdDraw);
    sPlatformId =
        RegisterSummonActor("nei.somaria.platform", "Cane of Somaria Platform", ACTORCAT_BG,
                            ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED,
                            sizeof(SomariaPlatformActor), PlatformInit, PlatformDestroy, PlatformUpdate, PlatformDraw);
    return sStatueId >= 0 && sCrateId >= 0 && sBirdId >= 0 && sPlatformId >= 0;
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterActor) || !SOH_MOD_API_HAS(sApi, StorageSet)) {
        return;
    }
    if (!RegisterSummonActors()) {
        return;
    }

    SOHCustomItemDefinition cane = Z64Items_Define(SOMARIA_KEY, sModeIcons[SOMARIA_STATUE], sNameTex);

    Z64Items_SetButtons(&cane, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&cane, 0, 21, 0);
    Z64Items_SetTextbox(&cane, "You got the %rCane of Somaria%w!&It conjures what is not there,&and holds only so much "
                               "at once.^"
                               "Press %y\xA1%w to draw it, %y\xA1%w again to&conjure. %y\xA2%w picks what it makes.^"
                               "Find another cane and it holds&more, and conjures more besides.");
    Z64Items_SetPauseText(&cane, "%rCane of Somaria&%wPress %y\xA1%w to conjure, %y\xA2%w to pick&what it makes.");
    Z64Items_SetAction(&cane, TakeOutCane, UpdateCane);
    Z64Items_SetCanUse(&cane, CanUseCane);
    Z64Items_SetHeldCallbacks(&cane, PressCane, PutCaneAway, NULL);
    Z64Items_SetHeldModel(&cane, sCaneDL, PLAYER_MODELGROUP_DEFAULT);
    cane.getItemEntry.drawFunc = DrawGetItem;
    cane.onReceive = RaiseLevel;
    cane.onRemove = ForgetLevel;

    if (!Z64Items_Register(sApi, &cane)) {
        return;
    }
    Z64Wheel_Register(sApi, SOMARIA_KEY, SOMARIA_KEY, sWheelEntries, ARRAY_COUNT(sWheelEntries), false);
    LoadLevel();
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateSomaria);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    SOH_REGISTER_HOOK(sApi, OnLoadFile, ReloadLevel);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_OBJ_OSHIHIKI, TintSummonedBlock);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawSomariaGhost);
}
