#include <string.h>

#include "z64items.h"
#include "z64wheel.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Obj_Syokudai/z_obj_syokudai.h"
#include "overlays/actors/ovl_Bg_Po_Syokudai/z_bg_po_syokudai.h"
#include "overlays/actors/ovl_En_Po_Sisters/z_en_po_sisters.h"
#include "overlays/actors/ovl_En_Poh/z_en_poh.h"
#include "overlays/actors/ovl_En_Light/z_en_light.h"

#define LANTERN_KEY "nei.lantern"
#define LANTERN_BUTTON_COUNT 8
// The lamp is modelled at 2842 units and hangs upside down, so it rides the hand at a hundredth of that.
#define LANTERN_HELD_SCALE 0.004f
#define LANTERN_FLAME_SCALE 0.0008f
#define LANTERN_FLAME_DROP 7.0f
#define LANTERN_GIVE_SCALE 0.025f
#define LANTERN_LIGHT_RADIUS 200
#define LANTERN_STOWED_LIGHT_HEIGHT 40.0f
#define LANTERN_CATCH_RANGE 80.0f
#define LANTERN_CATCH_FIRST_FRAME 2
#define LANTERN_CATCH_LAST_FRAME 5
#define LANTERN_SPAWN_REACH 30.0f
#define LANTERN_BLUE_FIRE_HEIGHT 30.0f
#define LANTERN_MAX_FLAMES 8
#define LANTERN_FLAME_LIFETIME 40
#define LANTERN_GREEN_LIFETIME 120
#define LANTERN_FLAME_SEED_SCALE 0.06f
#define LANTERN_FLAME_FULL_SCALE 0.33f
#define LANTERN_FLAME_GROW_STEP 0.05f
#define LANTERN_FLAME_RADIUS 30
#define LANTERN_FLAME_HEIGHT 80
#define LANTERN_BURN_RANGE 120.0f
#define LANTERN_BURN_TIME 80
#define LANTERN_BURN_SPREAD_RANGE 80.0f
#define LANTERN_BURN_SPREAD_FRAME 20
#define LANTERN_MAX_BURNING 16
#define LANTERN_UPDRAFT_RANGE 60.0f
#define LANTERN_UPDRAFT_FORCE 8.0f
#define LANTERN_GREEN_STILL_EPS 1.5f
#define LANTERN_GREEN_HEAL_RATE 60
#define LANTERN_GREEN_HEAL_AMOUNT 4
#define LANTERN_GREEN_MAGIC 4

typedef enum {
    LANTERN_OFF,
    LANTERN_FIRE,
    LANTERN_BLUE,
    LANTERN_POE,
    LANTERN_GREEN,
    LANTERN_FLAME_MAX,
} LanternFlame;

typedef struct {
    Color_RGB8 light;
    Color_RGB8 flamePrim;
    Color_RGB8 flameEnv;
    s16 spawnParams;
    u32 damageFlags;
    u8 damage;
    s16 lifetime;
} LanternFire;

typedef struct {
    Actor* actor;
    s16 timer;
    f32 scale;
    u8 flame;
    bool isColliderReady;
    ColliderCylinder collider;
} TrackedFlame;

typedef struct {
    Actor* grass;
    s16 timer;
    u8 flame;
} BurningGrass;

static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gLanternNameTex";
static const ALIGN_ASSET(2) char sLanternDL[] = "__OTR__objects/object_poh/gPoeLanternDL";
static const ALIGN_ASSET(2) char sFireDL[] = "__OTR__objects/gameplay_keep/gEffFire1DL";
static const ALIGN_ASSET(2) char sSwingAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_bottle_bug_miss";
static const ALIGN_ASSET(2) char sSwingWaterAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_bottle_fish_miss";
static const ALIGN_ASSET(2) char sCatchAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_bottle_bug_in";
static const ALIGN_ASSET(2) char sFlameIcons[LANTERN_FLAME_MAX][64] = {
    "__OTR__textures/icon_item_custom/gItemIconLanternTex",
    "__OTR__textures/icon_item_custom/gItemIconLanternFireTex",
    "__OTR__textures/icon_item_custom/gItemIconLanternBlueTex",
    "__OTR__textures/icon_item_custom/gItemIconLanternPoeTex",
    "__OTR__textures/icon_item_custom/gItemIconLanternGreenTex",
};

// Every flame keeps DMG_ARROW_FIRE so all four light torches and burn webs; spawnParams picks En_Light's colour.
static const LanternFire sFires[LANTERN_FLAME_MAX] = {
    { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, 0x0000, 0, 0, 0 },
    { { 255, 180, 80 }, { 255, 200, 0 }, { 255, 0, 0 }, 0x0000, DMG_ARROW_FIRE, 2, LANTERN_FLAME_LIFETIME },
    { { 80, 150, 255 },
      { 0, 170, 255 },
      { 0, 0, 255 },
      0x0002,
      DMG_ARROW_FIRE | DMG_ARROW_ICE | DMG_MAGIC_ICE,
      2,
      LANTERN_FLAME_LIFETIME },
    { { 180, 80, 255 },
      { 255, 170, 255 },
      { 100, 0, 255 },
      0x000D,
      DMG_ARROW_FIRE | DMG_ARROW_LIGHT,
      2,
      LANTERN_FLAME_LIFETIME },
    { { 80, 255, 120 }, { 170, 255, 0 }, { 0, 150, 0 }, 0x0003, DMG_ARROW_FIRE, 0, LANTERN_GREEN_LIFETIME },
};

static const Z64WheelEntry sWheelEntries[LANTERN_FLAME_MAX] = {
    { sFlameIcons[LANTERN_OFF], "Unlit" },        { sFlameIcons[LANTERN_FIRE], "Fire" },
    { sFlameIcons[LANTERN_BLUE], "Blue Fire" },   { sFlameIcons[LANTERN_POE], "Poe Fire" },
    { sFlameIcons[LANTERN_GREEN], "Green Fire" },
};

// The order the Poe sisters and their torch stands cycle: Meg, Joelle, Beth, Amy.
static const u8 sPoeColorToFlame[] = { LANTERN_POE, LANTERN_FIRE, LANTERN_BLUE, LANTERN_GREEN };

// En_Light's sixteen colours (params & 0xF) mapped onto the four the lamp can hold.
static const u8 sEnLightToFlame[16] = {
    LANTERN_FIRE, LANTERN_FIRE, LANTERN_BLUE, LANTERN_GREEN, LANTERN_FIRE, LANTERN_FIRE, LANTERN_GREEN, LANTERN_BLUE,
    LANTERN_FIRE, LANTERN_FIRE, LANTERN_FIRE, LANTERN_GREEN, LANTERN_POE,  LANTERN_POE,  LANTERN_BLUE,  LANTERN_BLUE,
};

static ColliderCylinderInit sFlameColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER | AT_TYPE_OTHER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_ARROW_FIRE, 0x01, 0x02 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { LANTERN_FLAME_RADIUS, LANTERN_FLAME_HEIGHT, 0, { 0, 0, 0 } },
};

static const u16 sItemButtons[LANTERN_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                        BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnSceneInit", "OnActorDrawEnd", "OnInterfaceDrawEnd",
                                              "OnLoadFile" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static LightNode* sLightNode;
static LightInfo sLightInfo;
static TrackedFlame sFlames[LANTERN_MAX_FLAMES];
static BurningGrass sBurning[LANTERN_MAX_BURNING];
static bool sIsLanternOut;
static bool sIsCatchResolved;
static bool sIsTextboxPending;
static bool sWasCatchMessageUp;
static u8 sCaughtFlame;
static u8 sFlameTexScroll;
static s16 sHealTimer;
static Vec3f sLastPlayerPos;
static bool sIsLensBorrowed;

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_ZeroSpeedXZ(Player* player);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
void func_80839FFC(Player* player, PlayState* play);

static void SwingAction(Player* player, PlayState* play);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static u8 CurrentFlame(void) {
    u32 selection = Z64Wheel_GetSelection();

    return selection < LANTERN_FLAME_MAX ? (u8)selection : LANTERN_OFF;
}

static u16 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < LANTERN_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, LANTERN_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

// A lit lamp stays in the hand that is otherwise empty; unlit, it only shows while it is being swung.
static bool IsLanternShowing(Player* player, u8 fire) {
    if (FindEquippedButtonMask() == 0) {
        return false;
    }
    if (sIsLanternOut) {
        return true;
    }
    return fire != LANTERN_OFF && player->heldItemAction == PLAYER_IA_NONE;
}

// A Poe recolours its lamp as it changes mood, so its fire has to be read from the colour, not a table.
static u8 ClassifyFlameColor(u8 red, u8 green, u8 blue) {
    u8 strongest = MAX(red, MAX(green, blue));
    u8 weakest = MIN(red, MIN(green, blue));

    if (strongest - weakest < 60) {
        return LANTERN_POE;
    }
    if (red >= 90 && blue >= 90 && green < red && green < blue) {
        return LANTERN_POE;
    }
    if (green >= red && green >= blue) {
        return LANTERN_GREEN;
    }
    if (blue > red && blue > green) {
        return LANTERN_BLUE;
    }
    return LANTERN_FIRE;
}

static u8 FlameOfSource(Actor* actor, PlayState* play) {
    switch (actor->id) {
        case ACTOR_BG_PO_SYOKUDAI:
            return Flags_GetSwitch(play, actor->params) ? sPoeColorToFlame[((BgPoSyokudai*)actor)->flameColor & 3]
                                                        : LANTERN_OFF;
        case ACTOR_EN_PO_SISTERS:
            return sPoeColorToFlame[((EnPoSisters*)actor)->unk_194 & 3];
        case ACTOR_EN_POH: {
            Color_RGBA8* color = &((EnPoh*)actor)->lightColor;

            return ClassifyFlameColor(color->r, color->g, color->b);
        }
        case ACTOR_EN_PO_FIELD:
        case ACTOR_EN_PO_DESERT:
            return LANTERN_POE;
        case ACTOR_EN_LIGHT:
            return sEnLightToFlame[actor->params & 0xF];
        case ACTOR_EN_ICE_HONO:
            return LANTERN_BLUE;
        case ACTOR_OBJ_SYOKUDAI:
            return ((ObjSyokudai*)actor)->litTimer != 0 ? LANTERN_FIRE : LANTERN_OFF;
        case ACTOR_EN_BW:
            return LANTERN_FIRE;
        default:
            return LANTERN_OFF;
    }
}

static bool IsInFrontOfPlayer(Player* player, Actor* actor) {
    f32 deltaX = actor->world.pos.x - player->actor.world.pos.x;
    f32 deltaZ = actor->world.pos.z - player->actor.world.pos.z;
    s16 angleToActor = Math_Atan2S(deltaX, deltaZ);
    s16 angleDiff = ABS((s16)(angleToActor - player->actor.shape.rot.y));

    if (SQ(deltaX) + SQ(deltaZ) >= SQ(LANTERN_CATCH_RANGE)) {
        return false;
    }
    if (angleDiff > 0x4000) {
        angleDiff = 0x7FFF - angleDiff;
    }
    return angleDiff < 0x4000;
}

static Actor* FindNearbyFire(Player* player, PlayState* play, u8* flame) {
    static const u8 categories[] = { ACTORCAT_ITEMACTION, ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_BG, ACTORCAT_MISC };

    for (u8 index = 0; index < ARRAY_COUNT(categories); index++) {
        for (Actor* actor = play->actorCtx.actorLists[categories[index]].head; actor != NULL; actor = actor->next) {
            u8 sourceFlame = actor->update == NULL ? LANTERN_OFF : FlameOfSource(actor, play);

            if (sourceFlame != LANTERN_OFF && IsInFrontOfPlayer(player, actor)) {
                *flame = sourceFlame;
                return actor;
            }
        }
    }
    return NULL;
}

// The Poe itself is swallowed with its fire; torches and loose flames keep burning.
static void SwallowPoe(PlayState* play, Actor* source) {
    switch (source->id) {
        case ACTOR_EN_PO_SISTERS:
            Flags_SetSwitch(play, source->params);
            PlaySfxAt(NA_SE_EV_FLAME_IGNITION, &source->world.pos);
            if (((EnPoSisters*)source)->unk_194 == 0) {
                Flags_UnsetSwitch(play, 0x1B);
            }
            Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
            break;
        case ACTOR_EN_POH:
            if (source->params == EN_POH_SHARP || source->params == EN_POH_FLAT) {
                return;
            }
            break;
        case ACTOR_EN_PO_FIELD:
        case ACTOR_EN_PO_DESERT:
            break;
        default:
            return;
    }
    PlaySfxAt(NA_SE_EN_PO_LAUGH2, &source->world.pos);
    Actor_Kill(source);
}

static void TrackFlame(PlayState* play, Actor* flame, u8 fire) {
    TrackedFlame* slot = &sFlames[0];

    for (u8 index = 0; index < LANTERN_MAX_FLAMES; index++) {
        if (sFlames[index].timer <= 0) {
            slot = &sFlames[index];
            break;
        }
    }
    if (slot->timer > 0 && slot->actor != NULL) {
        Actor_Kill(slot->actor);
    }
    if (!slot->isColliderReady) {
        Collider_InitCylinder(play, &slot->collider);
        Collider_SetCylinder(play, &slot->collider, flame, &sFlameColliderInit);
        slot->isColliderReady = true;
    }
    slot->collider.info.toucher.dmgFlags = sFires[fire].damageFlags;
    slot->collider.info.toucher.damage = sFires[fire].damage;
    slot->actor = flame;
    slot->flame = fire;
    slot->timer = sFires[fire].lifetime;
    slot->scale = LANTERN_FLAME_SEED_SCALE;
    flame->scale.x *= LANTERN_FLAME_SEED_SCALE;
    flame->scale.y *= LANTERN_FLAME_SEED_SCALE;
    flame->scale.z *= LANTERN_FLAME_SEED_SCALE;
}

// En_Light lights its own halo on spawn; a lamp's flame is a bare point light instead.
static Actor* SpawnFlameActor(PlayState* play, Vec3f* pos, u8 fire) {
    Actor* flame =
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, pos->x, pos->y, pos->z, 0, 0, 0, sFires[fire].spawnParams);

    if (flame == NULL) {
        return NULL;
    }
    ((EnLight*)flame)->lightInfo.type = LIGHT_POINT_NOGLOW;
    TrackFlame(play, flame, fire);
    return flame;
}

static void UpdateFlames(PlayState* play) {
    for (u8 index = 0; index < LANTERN_MAX_FLAMES; index++) {
        TrackedFlame* slot = &sFlames[index];

        if (slot->timer <= 0) {
            continue;
        }
        if (slot->actor == NULL || slot->actor->update == NULL) {
            slot->timer = 0;
            slot->actor = NULL;
            continue;
        }
        f32 grown = MIN(slot->scale + LANTERN_FLAME_GROW_STEP, LANTERN_FLAME_FULL_SCALE);

        slot->actor->scale.x *= grown / slot->scale;
        slot->actor->scale.y *= grown / slot->scale;
        slot->actor->scale.z *= grown / slot->scale;
        slot->scale = grown;
        slot->collider.dim.pos.x = (s16)slot->actor->world.pos.x;
        slot->collider.dim.pos.y = (s16)slot->actor->world.pos.y;
        slot->collider.dim.pos.z = (s16)slot->actor->world.pos.z;
        slot->collider.base.atFlags |= AT_ON;
        CollisionCheck_SetAT(play, &play->colChkCtx, &slot->collider.base);
        slot->collider.base.atFlags &= ~AT_HIT;
        if (--slot->timer <= 0) {
            Actor_Kill(slot->actor);
            slot->actor = NULL;
        }
    }
}

static bool IsBurnable(Actor* actor) {
    return actor->update != NULL && (actor->id == ACTOR_EN_KUSA || actor->id == ACTOR_OBJ_MURE3);
}

static void IgniteGrass(Actor* grass, u8 fire) {
    for (u8 index = 0; index < LANTERN_MAX_BURNING; index++) {
        if (sBurning[index].grass == grass && sBurning[index].timer > 0) {
            return;
        }
    }
    for (u8 index = 0; index < LANTERN_MAX_BURNING; index++) {
        if (sBurning[index].timer <= 0) {
            sBurning[index].grass = grass;
            sBurning[index].timer = LANTERN_BURN_TIME;
            sBurning[index].flame = fire;
            return;
        }
    }
}

static void IgniteGrassAround(PlayState* play, Vec3f* center, f32 range, u8 fire) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].head; actor != NULL; actor = actor->next) {
        f32 deltaX = actor->world.pos.x - center->x;
        f32 deltaZ = actor->world.pos.z - center->z;

        if (IsBurnable(actor) && SQ(deltaX) + SQ(deltaZ) < SQ(range)) {
            IgniteGrass(actor, fire);
        }
    }
}

// Burning grass throws Link upward: the heat is what carries him, so his own jump keeps whatever it had.
static void LiftPlayerOverFire(Player* player, Vec3f* firePos) {
    f32 deltaX = player->actor.world.pos.x - firePos->x;
    f32 deltaZ = player->actor.world.pos.z - firePos->z;

    if (SQ(deltaX) + SQ(deltaZ) < SQ(LANTERN_UPDRAFT_RANGE) && player->actor.velocity.y < LANTERN_UPDRAFT_FORCE) {
        player->actor.velocity.y = LANTERN_UPDRAFT_FORCE;
    }
}

static void UpdateBurningGrass(Player* player, PlayState* play) {
    for (u8 index = 0; index < LANTERN_MAX_BURNING; index++) {
        BurningGrass* fire = &sBurning[index];

        if (fire->timer <= 0) {
            continue;
        }
        if (fire->grass == NULL || fire->grass->update == NULL) {
            fire->timer = 0;
            fire->grass = NULL;
            continue;
        }
        if (fire->timer == LANTERN_BURN_TIME - 1) {
            SpawnFlameActor(play, &fire->grass->world.pos, fire->flame);
        }
        if (fire->timer == LANTERN_BURN_SPREAD_FRAME) {
            IgniteGrassAround(play, &fire->grass->world.pos, LANTERN_BURN_SPREAD_RANGE, fire->flame);
        }
        LiftPlayerOverFire(player, &fire->grass->world.pos);
        if (--fire->timer <= 0) {
            Actor_Kill(fire->grass);
            fire->grass = NULL;
        }
    }
}

static void SetFlameDown(Player* player, PlayState* play, u8 fire) {
    Vec3f pos = { player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * LANTERN_SPAWN_REACH,
                  player->actor.world.pos.y,
                  player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * LANTERN_SPAWN_REACH };

    // Red ice melts only for the blue-fire actor itself: Bg_Ice_Shelter checks the actor id, not a damage flag.
    if (fire == LANTERN_BLUE) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ICE_HONO, pos.x, pos.y + LANTERN_BLUE_FIRE_HEIGHT, pos.z, 0, 0, 0,
                    0);
    }
    SpawnFlameActor(play, &pos, fire);
    IgniteGrassAround(play, &player->actor.world.pos, LANTERN_BURN_RANGE, fire);
    PlaySfxAt(NA_SE_EV_FLAME_IGNITION, &player->actor.world.pos);
}

static void CatchFire(Player* player, PlayState* play, Actor* source, u8 fire) {
    Z64Wheel_SetEntryUnlocked(fire, true);
    Z64Wheel_SetSelection(fire);
    sCaughtFlame = fire;
    sIsTextboxPending = true;
    PlaySfxAt(NA_SE_EV_FLAME_IGNITION, &player->actor.world.pos);
    SwallowPoe(play, source);
    player->stateFlags1 |= PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE;
    LinkAnimation_PlayOnce(play, &player->skelAnime, (LinkAnimationHeader*)sCatchAnim);
    Camera_ChangeSetting(Play_GetCamera(play, 0), CAM_SET_TURN_AROUND);
    Camera_SetCameraData(Play_GetCamera(play, 0), 4, NULL, NULL, 10, 0, 0);
}

static const char* CatchMessage(u8 fire) {
    switch (fire) {
        case LANTERN_BLUE:
            return "The lamp swallows a %bblue flame%w!&It gives off a cold light, and&melts %bred ice%w where you "
                   "set it&down.";
        case LANTERN_POE:
            return "The lamp swallows a %pghost fire%w!&Its light shows you what hides&from ordinary eyes while you "
                   "hold&it.";
        case LANTERN_GREEN:
            return "The lamp swallows a %ggreen flame%w!&Stand still by its light and it&mends you, little by "
                   "little.";
        default:
            return "The lamp swallows the %rflame%w!&It lights your way, and burns&grass and cobwebs where you set "
                   "it&down.";
    }
}

static void ShowCatchMessage(PlayState* play) {
    sIsTextboxPending = false;
    sWasCatchMessageUp = false;
    sApi->ShowTextbox(play, CatchMessage(sCaughtFlame), false);
    Audio_PlayFanfare(NA_BGM_ITEM_GET | 0x900);
}

static void ReleaseCatchCamera(Player* player, PlayState* play) {
    player->stateFlags1 &= ~(PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE);
    func_8005B1A4(Play_GetCamera(play, 0));
    func_80839FFC(player, play);
}

// The swing is the bottle's: the catch lands on the same frames a bug would be scooped up.
static void SwingAction(Player* player, PlayState* play) {
    s32 activeFrame = (s32)player->skelAnime.curFrame;

    Player_ZeroSpeedXZ(player);
    // The item's own update stops while this action owns the player, and a stalled item is put away.
    if (SOH_MOD_API_HAS(sApi, KeepCustomItemHeld)) {
        sApi->KeepCustomItemHeld();
    }
    if (!sIsCatchResolved && activeFrame >= LANTERN_CATCH_FIRST_FRAME && activeFrame <= LANTERN_CATCH_LAST_FRAME) {
        u8 sourceFlame = LANTERN_OFF;
        Actor* source = FindNearbyFire(player, play, &sourceFlame);
        u8 held = CurrentFlame();

        sIsCatchResolved = true;
        if (source != NULL && sourceFlame != held) {
            CatchFire(player, play, source, sourceFlame);
            return;
        }
        if (held != LANTERN_OFF) {
            SetFlameDown(player, play, held);
        }
    }
    if (!LinkAnimation_Update(play, &player->skelAnime)) {
        return;
    }
    if (sIsTextboxPending) {
        ShowCatchMessage(play);
        return;
    }
    if (!(player->stateFlags1 & PLAYER_STATE1_IN_ITEM_CS)) {
        func_80839FFC(player, play);
        return;
    }
    // The box takes a few frames to come up, so Link only lets go once it has been on screen and closed again.
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        sWasCatchMessageUp = true;
    }
    if (sWasCatchMessageUp && play->msgCtx.msgMode == MSGMODE_NONE) {
        ReleaseCatchCamera(player, play);
    }
}

static void StartSwing(Player* player, PlayState* play) {
    bool isInWater = player->actor.yDistToWater > 12.0f;

    Player_SetupAction(play, player, SwingAction, 0);
    Player_ZeroSpeedXZ(player);
    LinkAnimation_PlayOnce(play, &player->skelAnime, (LinkAnimationHeader*)(isInWater ? sSwingWaterAnim : sSwingAnim));
    Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_SWING);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_AUTO_JUMP);
    sIsCatchResolved = false;
    sIsTextboxPending = false;
}

static s32 SwingLantern(Player* player, PlayState* play) {
    u16 buttonMask = FindEquippedButtonMask();

    if (buttonMask == 0 || Z64Wheel_IsOpen() || player->actionFunc == SwingAction ||
        !(play->state.input[0].press.button & buttonMask)) {
        return 0;
    }
    StartSwing(player, play);
    return 0;
}

static void TakeOutLantern(PlayState* play, Player* player) {
    sIsLanternOut = true;
    sIsTextboxPending = false;
}

static void PutLanternAway(Player* player, PlayState* play) {
    if (player->actionFunc == SwingAction) {
        ReleaseCatchCamera(player, play);
    }
    sIsLanternOut = false;
    sIsTextboxPending = false;
}

static void GetLampPos(Player* player, Vec3f* pos) {
    *pos = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
}

// Stowed, the glow falls back to Link's chest so a lit lamp still lights the room from his belt.
static void HoldLight(PlayState* play, Player* player, u8 fire) {
    if (fire == LANTERN_OFF) {
        if (sLightNode != NULL) {
            LightContext_RemoveLight(play, &play->lightCtx, sLightNode);
            sLightNode = NULL;
        }
        return;
    }
    f32 flicker = 0.8f + Rand_ZeroOne() * 0.2f;
    Color_RGB8 color = sFires[fire].light;
    Vec3f pos = player->actor.world.pos;

    if (IsLanternShowing(player, fire)) {
        GetLampPos(player, &pos);
    } else {
        pos.y += LANTERN_STOWED_LIGHT_HEIGHT;
    }
    Lights_PointNoGlowSetInfo(&sLightInfo, pos.x, pos.y, pos.z, (u8)(color.r * flicker), (u8)(color.g * flicker),
                              (u8)(color.b * flicker), (s16)(LANTERN_LIGHT_RADIUS * (0.9f + flicker * 0.1f)));
    if (sLightNode == NULL) {
        sLightNode = LightContext_InsertLight(play, &play->lightCtx, &sLightInfo);
    }
}

// Ghost fire shows what hides from ordinary eyes, without spending the magic the Lens would.
static void HoldLens(PlayState* play, u8 fire) {
    bool wantsLens = fire == LANTERN_POE && play->csCtx.state == CS_STATE_IDLE;

    if (wantsLens) {
        play->actorCtx.lensActive = true;
        sIsLensBorrowed = true;
        return;
    }
    if (sIsLensBorrowed) {
        sIsLensBorrowed = false;
        if (gSaveContext.magicState != MAGIC_STATE_CONSUME_LENS) {
            play->actorCtx.lensActive = false;
        }
    }
}

static void SpawnHealSparks(PlayState* play, Player* player, f32 height, s16 life, s16 size) {
    Vec3f pos = { player->actor.world.pos.x + Rand_CenteredFloat(20.0f), player->actor.world.pos.y + height,
                  player->actor.world.pos.z + Rand_CenteredFloat(20.0f) };
    Vec3f velocity = { 0.0f, 2.0f, 0.0f };
    Vec3f accel = { 0.0f, -0.1f, 0.0f };
    Color_RGBA8 primary = { 80, 255, 120, 255 };
    Color_RGBA8 secondary = { 40, 200, 80, 200 };

    EffectSsKiraKira_SpawnFocused(play, &pos, &velocity, &accel, &primary, &secondary, life, size);
}

// Green fire only mends someone who stands still under it.
static void HealWhileStill(PlayState* play, Player* player, u8 fire) {
    f32 moved = SQ(player->actor.world.pos.x - sLastPlayerPos.x) + SQ(player->actor.world.pos.y - sLastPlayerPos.y) +
                SQ(player->actor.world.pos.z - sLastPlayerPos.z);

    sLastPlayerPos = player->actor.world.pos;
    if (fire != LANTERN_GREEN || moved >= SQ(LANTERN_GREEN_STILL_EPS)) {
        sHealTimer = 0;
        return;
    }
    if ((++sHealTimer % 6) == 0) {
        SpawnHealSparks(play, player, 10.0f + Rand_ZeroFloat(30.0f), 400, 12);
    }
    if (sHealTimer < LANTERN_GREEN_HEAL_RATE) {
        return;
    }
    sHealTimer = 0;
    Health_ChangeBy(play, LANTERN_GREEN_HEAL_AMOUNT);
    Magic_RequestChange(play, LANTERN_GREEN_MAGIC, MAGIC_ADD);
    SpawnHealSparks(play, player, 30.0f + Rand_ZeroFloat(20.0f), 600, 20);
}

static void DrawLampFlame(PlayState* play, Player* player, Vec3f* lampPos, u8 fire) {
    Color_RGB8 prim = sFires[fire].flamePrim;
    Color_RGB8 env = sFires[fire].flameEnv;
    s16 towardsCamera = (s16)(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - player->actor.shape.rot.y + 0x8000);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(lampPos->x, lampPos->y - LANTERN_FLAME_DROP, lampPos->z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(towardsCamera), MTXMODE_APPLY);
    Matrix_Scale(LANTERN_FLAME_SCALE, LANTERN_FLAME_SCALE, LANTERN_FLAME_SCALE, MTXMODE_APPLY);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0, (sFlameTexScroll * -20) & 0x1FF, 32, 128,
                                  0, 0, 0, -20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, prim.r, prim.g, prim.b, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, env.r, env.g, env.b, 0);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sFireDL);
    CLOSE_DISPS(play->state.gfxCtx);
    sFlameTexScroll++;
}

// The lamp hangs from the hand, upside down as the model is authored, and burns in its flame's colour.
static void DrawLantern(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    u8 fire = CurrentFlame();
    Vec3f lampPos;

    if (!IsLanternShowing(player, fire) || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    GetLampPos(player, &lampPos);

    OPEN_DISPS(play->state.gfxCtx);
    if (fire == LANTERN_OFF) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetEnvColor(POLY_XLU_DISP++, 40, 40, 50, 120);
    } else {
        f32 flicker = 0.8f + Rand_ZeroOne() * 0.2f;
        Color_RGB8 color = sFires[fire].light;

        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPSetEnvColor(POLY_OPA_DISP++, (u8)(color.r * flicker), (u8)(color.g * flicker), (u8)(color.b * flicker), 255);
    }
    Matrix_Translate(lampPos.x, lampPos.y, lampPos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(M_PI, MTXMODE_APPLY);
    Matrix_Scale(LANTERN_HELD_SCALE, LANTERN_HELD_SCALE, LANTERN_HELD_SCALE, MTXMODE_APPLY);
    if (fire == LANTERN_OFF) {
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sLanternDL);
    } else {
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sLanternDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);

    if (fire != LANTERN_OFF) {
        DrawLampFlame(play, player, &lampPos, fire);
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    Matrix_RotateX(M_PI, MTXMODE_APPLY);
    Matrix_Scale(LANTERN_GIVE_SCALE, LANTERN_GIVE_SCALE, LANTERN_GIVE_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sLanternDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void UpdateLantern(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);
    u8 fire = CurrentFlame();
    bool isCarried = FindEquippedButtonMask() != 0;

    if (play == NULL || player == NULL) {
        return;
    }
    UpdateFlames(play);
    UpdateBurningGrass(player, play);
    if (!isCarried) {
        HoldLight(play, player, LANTERN_OFF);
        HoldLens(play, LANTERN_OFF);
        return;
    }
    HoldLight(play, player, fire);
    HoldLens(play, fire);
    HealWhileStill(play, player, fire);
}

// The scene rebuilds the light list and every actor, so the lamp's own bookkeeping starts over with it.
static void ForgetSceneState(int16_t sceneNum) {
    sLightNode = NULL;
    sIsLensBorrowed = false;
    sIsTextboxPending = false;
    sHealTimer = 0;
    for (u8 index = 0; index < LANTERN_MAX_FLAMES; index++) {
        sFlames[index].timer = 0;
        sFlames[index].actor = NULL;
        sFlames[index].isColliderReady = false;
    }
    for (u8 index = 0; index < LANTERN_MAX_BURNING; index++) {
        sBurning[index].timer = 0;
        sBurning[index].grass = NULL;
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, StorageSet)) {
        return;
    }

    SOHCustomItemDefinition lantern = Z64Items_Define(LANTERN_KEY, sFlameIcons[LANTERN_OFF], sNameTex);

    Z64Items_SetButtons(&lantern, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&lantern, 0, 19, 0);
    Z64Items_SetTextbox(&lantern, "You got the %rLantern%w!&A gravekeeper's lamp, empty and&waiting for a flame.^"
                                  "Swing it at a fire with %y\xA1%w to&drink the flame in: the lamp&lights your way "
                                  "in that colour.^Swing it away from a fire to&set the flame down and burn what&it "
                                  "finds. %y\xA2%w picks your flames.");
    Z64Items_SetPauseText(&lantern, "%rLantern&%wSwing %y\xA1%w at a fire to catch it,&away from one to set it down.");
    Z64Items_SetAction(&lantern, TakeOutLantern, SwingLantern);
    Z64Items_SetHeldCallbacks(&lantern, NULL, PutLanternAway, NULL);
    lantern.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    lantern.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &lantern)) {
        return;
    }
    Z64Wheel_Register(sApi, LANTERN_KEY, LANTERN_KEY, sWheelEntries, ARRAY_COUNT(sWheelEntries), false);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateLantern);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawLantern);
}
