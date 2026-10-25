#include <string.h>
#include <math.h>
/**
 * Climb Boots: full traction. Ice keeps no momentum, a steep slope neither slides Link down nor slows him
 * climbing it, and A still jumps from one.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/object_gi_boots_2/object_gi_boots_2.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "soh/ModApi/Player/PlayerHookTypes.h"

#define CLIMB_BOOTS_KEY "nei.equip.climb_boots"
#define FLOOR_TYPE_ICE 5
#define SLOPE_STEEP 1
#define BGCHECK_ON_GROUND 0x0001
#define GI_DL_CAPACITY 512

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconClimbBootsTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gClimbBootsNameTex";

// Imported by address: a plain declaration would compare actionFunc against the DLL's own thunk.
extern HOST_DATA void Player_Action_SlideOnSlope(Player* player, PlayState* play);
int Player_IsZTargeting(Player* player);
int Player_CanUpdateItems(Player* player);
void Player_SetupRoll(Player* player, PlayState* play);
void func_80839FFC(Player* player, PlayState* play);
void func_8083BA90(PlayState* play, Player* player, s32 meleeWeaponAnim, f32 xzVelocity, f32 yVelocity);
void func_8083BCD0(Player* player, PlayState* play, s32 controlStickDirection);

static const SOHModApi* sApi;
#define REMOTE_VISUAL_INTERVAL 5u

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual);
static void InitRemoteVisuals(void);

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(CLIMB_BOOTS_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECK_ON_GROUND) != 0;
}

static bool IsOnSteepSlope(Player* player, PlayState* play) {
    CollisionPoly* floor = player->actor.floorPoly;

    return floor != NULL && SurfaceType_GetFloorEffect(&play->colCtx, floor, player->actor.floorBgId) == SLOPE_STEEP;
}

// Skipping the assignment keeps the previous, non-ice floor type: that is what stops the skid.
static void KeepGripOnIce(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;

    if (!IsWorn() || play == NULL || !IsGrounded(player) || player->actor.floorPoly == NULL) {
        return;
    }
    if (SurfaceType_GetFloorType(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) == FLOOR_TYPE_ICE) {
        *should = false;
    }
}

// The slide starts inside the scene collision; ending it here, after the whole update, costs no frame. Walking
// downhill restarts it every frame, so the walk's own speed and heading are handed back each time.
static void KeepFootingOnSlopes(void) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL || !IsWorn()) {
        return;
    }
    player = GET_PLAYER(play);
    if (!IsGrounded(player)) {
        return;
    }
    if (player->actionFunc == Player_Action_SlideOnSlope) {
        f32 speed = player->linearVelocity;
        s16 heading = player->yaw;

        func_80839FFC(player, play);
        player->linearVelocity = speed;
        player->yaw = heading;
    }
    if (IsOnSteepSlope(player, play)) {
        player->pushedSpeed = 0.0f;
    }
}

// Player_ActionHandler_10 without its steep-slope gate, which is the only thing standing between a slope
// and the hop, backflip or jump slash.
static bool StartSlopeJump(Player* player, PlayState* play) {
    s32 stickDirection = player->controlStickDirections[player->controlStickDataIndex];

    if (stickDirection > PLAYER_STICK_DIR_FORWARD) {
        func_8083BCD0(player, play, stickDirection);
        return true;
    }
    if (!Player_IsZTargeting(player)) {
        return false;
    }
    if ((Player_GetMeleeWeaponHeld(player) != 0) && Player_CanUpdateItems(player)) {
        func_8083BA90(play, player, PLAYER_MWA_JUMPSLASH_START, 5.0f, 5.0f);
    } else {
        Player_SetupRoll(player, play);
    }
    return true;
}

static void JumpFromSteepSlope(PlayState* play, Player* player, int32_t action, bool* consumed, bool* startedAction) {
    if (*consumed || !IsWorn() || !IsOnSteepSlope(player, play)) {
        return;
    }
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) ||
        play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_2) {
        return;
    }
    *consumed = true;
    *startedAction = StartSlopeJump(player, play);
}

// The iron boots' get-item model, recoloured per section: warm leather turns yellow, cool iron silver.
static u32 YellowIronRamp(u32 rgba) {
    u8 r = (rgba >> 24) & 0xFF;
    u8 g = (rgba >> 16) & 0xFF;
    u8 b = (rgba >> 8) & 0xFF;
    f32 lum = 0.299f * r + 0.587f * g + 0.114f * b;
    bool isLeather = r > b + 30;
    f32 newR = isLeather ? lum * 2.2f : lum * 1.2f + 25.0f;
    f32 newG = isLeather ? lum * 1.75f : lum * 1.25f + 25.0f;
    f32 newB = isLeather ? lum * 0.35f : lum * 1.35f + 28.0f;

    return ((u32)CLAMP_MAX(newR, 255.0f) << 24) | ((u32)CLAMP_MAX(newG, 255.0f) << 16) |
           ((u32)CLAMP_MAX(newB, 255.0f) << 8) | (rgba & 0xFF);
}

static bool IsTwoWordCommand(u8 opcode) {
    return opcode == 0x20 || opcode == 0x24 || opcode == 0x25 || opcode == 0x27 || opcode == 0x31 ||
           opcode == 0x32 || opcode == 0x33 || opcode == 0x35 || opcode == 0x36 || opcode == 0x42;
}

// A local copy, never the shared resource: the vanilla Iron Boots must keep their own colours.
static Gfx* BuildRecoloredDL(const char* path, Gfx* out, bool* built, bool gray) {
    Gfx* source;
    s32 count = 0;

    if (*built) {
        return out;
    }
    source = ResourceMgr_LoadGfxByName(path);
    if (source == NULL) {
        return NULL;
    }
    while (count < GI_DL_CAPACITY) {
        u8 opcode = (source[count].words.w0 >> 24) & 0xFF;

        out[count] = source[count];
        count++;
        if (opcode == G_ENDDL) {
            break;
        }
        if (IsTwoWordCommand(opcode) && count < GI_DL_CAPACITY) {
            out[count] = source[count];
            count++;
            continue;
        }
        if (opcode == G_SETPRIMCOLOR || opcode == G_SETENVCOLOR) {
            u32 rgba = (u32)out[count - 1].words.w1;
            if (gray) {
                u32 lum = (((rgba >> 24) & 255) * 3 + ((rgba >> 16) & 255) * 6 + ((rgba >> 8) & 255)) / 10;
                lum = CLAMP_MAX(lum * 1.15f + 30.0f, 255.0f);
                out[count - 1].words.w1 = (lum << 24) | (lum << 16) | (lum << 8) | (rgba & 255);
            } else { out[count - 1].words.w1 = YellowIronRamp(rgba); }
        }
    }
    *built = true;
    return out;
}

static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    static Gfx sBootsDL[GI_DL_CAPACITY];
    static Gfx sRivetsDL[GI_DL_CAPACITY];
    bool sBootsBuilt = false;
    bool sRivetsBuilt = false;
    Gfx* boots = BuildRecoloredDL(gGiIronBootsDL, sBootsDL, &sBootsBuilt, false);
    Gfx* rivets = BuildRecoloredDL(gGiIronBootsRivetsDL, sRivetsDL, &sRivetsBuilt, false);

    if (boots == NULL || rivets == NULL) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, boots);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, rivets);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Keep the exact env colour emitted for this player, including custom tunics and cosmetics.
static u32 CurrentPlayerEnv(PlayState* play) {
    Gfx* begin=play->state.gfxCtx->polyOpa.bufp;
    Gfx* cursor=play->state.gfxCtx->polyOpa.p;
    while (cursor>begin) {
        --cursor;
        if (((cursor->words.w0>>24)&255)==G_SETENVCOLOR) { return (u32)cursor->words.w1; }
    }
    return 0xFFFFFFFF;
}
// Visual-only iron boot overlays; currentBoots and vanilla equipment remain Kokiri boots.
static void DrawClimbBootsOnFeet(PlayState* play, Player* player, int32_t limbIndex) {
    RemoteVisualState visual;
    bool visible = player == GET_PLAYER(gPlayState) ? IsWorn() : ReadRemoteVisual(player, &visual);
    if (!visible || (limbIndex != PLAYER_LIMB_L_FOOT && limbIndex != PLAYER_LIMB_R_FOOT)) { return; }
    u32 savedEnv=CurrentPlayerEnv(play);
    const char* path = limbIndex == PLAYER_LIMB_L_FOOT ? gLinkAdultLeftIronBootDL : gLinkAdultRightIronBootDL;
    // Scope both the model transform and the tint; never leave Link's tunic env colour white.
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, 205, 210, 220, 150);
    gSPGrayscale(POLY_OPA_DISP++, true);
    Matrix_Push();
    if (!LINK_IS_ADULT) { Matrix_Scale(0.65f, 0.65f, 0.65f, MTXMODE_APPLY); }
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)path);
    Matrix_Pop();
    gSPGrayscale(POLY_OPA_DISP++, false);
    gDPSetEnvColor(POLY_OPA_DISP++, savedEnv>>24, (savedEnv>>16)&255, (savedEnv>>8)&255, savedEnv&255);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Taking off custom boots writes 0 in the equipment slot; vanilla's draw requires a valid boot index.
static void KeepBootDrawValid(Actor* actor, PlayState* play, bool* drawVanilla) {
    if (gPlayState == NULL || actor != &GET_PLAYER(gPlayState)->actor) {
        return;
    }

    if (actor != &GET_PLAYER(play)->actor) { return; }
    if (CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS) < EQUIP_VALUE_BOOTS_KOKIRI ||
        CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS) >= EQUIP_VALUE_BOOTS_MAX) {
        Inventory_ChangeEquipment(EQUIP_TYPE_BOOTS, EQUIP_VALUE_BOOTS_KOKIRI);
        Player_SetEquipmentData(play, (Player*)actor);
    }
}

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate", "OnPlayerUpdate", "OnPlayerActionHandler", "OnPlayerPostLimbDraw", "OnActorDraw" };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static bool sRemoteVisualReady;
static bool sRemoteVisualSent;
static uint32_t sRemoteVisualFrame;
static RemoteVisualState sLastRemoteVisual;

static void ReceiveRemoteVisual(const SOHNetMessage* message) {
    (void)message;
}

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual) {
    if (!sRemoteVisualReady || gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        return false;
    }
    uint32_t peerId = sApi->GetNetPeerIdForActor(&player->actor);
    if (peerId == 0 || sApi->GetNetPeerState(peerId, "nei.climb_boots.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->adult > 1 || visual->scene != gPlayState->sceneNum ||
        visual->flags[0] > 255 || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return false;
    }
    for (uint32_t i = 0; i < ARRAY_COUNT(visual->values); i++) {
        if (!isfinite(visual->values[i]) || fabsf(visual->values[i]) > 10000000.0f) {
            return false;
        }
    }
    return true;
}

static void PublishRemoteVisual(void) {
    if (!sRemoteVisualReady) {
        return;
    }
    RemoteVisualState visual = { 0 };
    PlayState* play = gPlayState;
    visual.scene = -1;
    if (play != NULL) {
        Player* player = GET_PLAYER(play);
        visual.scene = play->sceneNum;
        visual.adult = LINK_IS_ADULT;
        visual.active = (IsWorn());
        if (visual.active) {

        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (!sRemoteVisualSent || memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) != 0) {
        if (sApi->SetNetPeerState("nei.climb_boots.visual", &visual, sizeof(visual))) {
            sLastRemoteVisual = visual;
            sRemoteVisualSent = true;
            sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
        }
    }
}



static void InitRemoteVisuals(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterNetChannel) || !SOH_MOD_API_HAS(sApi, SetNetPeerState) ||
        !SOH_MOD_API_HAS(sApi, GetNetPeerState) || !SOH_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.climb_boots.visual", 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
    }
}




SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    InitRemoteVisuals();
    SOHCustomEquipDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = CLIMB_BOOTS_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rClimb Boots&%wIce and steep slopes no longer&"
                           "move you, and %y\x9F%w jumps even from&a slope.";
    definition.getItemText = "You got the %rClimb Boots%w! Iron&soles with a miner's grip.^"
                             "Wear them and neither ice nor&a steep slope will carry you&away.";
    definition.slot = SOH_EQUIP_SLOT_BOOTS;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_BOOTS;
    definition.column = 2;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_ANY;
    definition.vanillaBase = EQUIP_VALUE_BOOTS_KOKIRI;
    definition.toggles = 1;
    definition.getItemEntry.drawFunc = DrawGetItem;
    // The equipment registry creates the pickup; this block also puts it into new seeds.
    static const SOHCustomItemRandomizer randomizer = {
        sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_EQUIP, 0, 1,
        CLIMB_BOOTS_KEY, CLIMB_BOOTS_KEY, "",
    };
    definition.randomizer = &randomizer;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, KeepFootingOnSlopes);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_PLAYER, KeepBootDrawValid);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawClimbBootsOnFeet);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_ZTARGET_A, JumpFromSteepSlope);
    sApi->RegisterVB(VB_SET_STATIC_FLOOR_TYPE, KeepGripOnIce);
}
