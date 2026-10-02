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

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(CLIMB_BOOTS_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECK_ON_GROUND) != 0;
}

static bool IsOnSteepSlope(Player* player, PlayState* play) {
    CollisionPoly* floor = player->actor.floorPoly;

    return floor != NULL && SurfaceType_GetSlope(&play->colCtx, floor, player->actor.floorBgId) == SLOPE_STEEP;
}

// Skipping the assignment keeps the previous, non-ice floor type: that is what stops the skid.
static void KeepGripOnIce(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;

    if (!IsWorn() || play == NULL || !IsGrounded(player) || player->actor.floorPoly == NULL) {
        return;
    }
    if (func_80041D4C(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) == FLOOR_TYPE_ICE) {
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
static Gfx* BuildRecoloredDL(const char* path, Gfx* out, bool* built) {
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
            out[count - 1].words.w1 = YellowIronRamp((u32)out[count - 1].words.w1);
        }
    }
    *built = true;
    return out;
}

static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    static Gfx sBootsDL[GI_DL_CAPACITY];
    static Gfx sRivetsDL[GI_DL_CAPACITY];
    static bool sBootsBuilt;
    static bool sRivetsBuilt;
    Gfx* boots = BuildRecoloredDL(gGiIronBootsDL, sBootsDL, &sBootsBuilt);
    Gfx* rivets = BuildRecoloredDL(gGiIronBootsRivetsDL, sRivetsDL, &sRivetsBuilt);

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

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayerActionHandler" };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHCustomEquipDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = CLIMB_BOOTS_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rClimb Boots&%wIce and steep slopes no longer move you, and %y\x9F%w jumps even from "
                           "a slope.";
    definition.getItemText = "You got the %rClimb Boots%w!&Iron soles with a miner's grip. Wear them and neither "
                             "ice nor a steep slope will carry you away.";
    definition.slot = SOH_EQUIP_SLOT_BOOTS;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_BOOTS;
    definition.column = 2;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_ANY;
    definition.vanillaBase = EQUIP_VALUE_BOOTS_KOKIRI;
    definition.toggles = 1;
    definition.getItemEntry.drawFunc = DrawGetItem;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, KeepFootingOnSlopes);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_ZTARGET_A, JumpFromSteepSlope);
    sApi->RegisterVB(VB_SET_STATIC_FLOOR_TYPE, KeepGripOnIce);
}
