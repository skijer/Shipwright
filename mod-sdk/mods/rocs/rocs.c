#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define ROCS_FEATHER_KEY "nei.rocs_feather"
#define ROCS_CAPE_KEY "nei.rocs_cape"
#define ROCS_PROGRESSIVE_KEY "nei.progressive_rocs"
#define ROCS_JUMP_VELOCITY 11.0f
#define ROCS_WATER_JUMP_VELOCITY 5.5f
#define ROCS_FEATHER_GIVE_SCALE 0.5f
#define ROCS_CAPE_GIVE_SCALE 0.6f
#define ROCS_BUTTON_COUNT 8

#define ROCS_BLOCKING_STATES                                                                                     \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING | PLAYER_STATE1_IN_ITEM_CS |          \
     PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_HANGING_OFF_LEDGE |                       \
     PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_ON_HORSE |                      \
     PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_CARRYING_ACTOR)

static const ALIGN_ASSET(2) char sFeatherIconTex[] = "__OTR__textures/icon_item_custom/gItemIconRocsFeatherTex";
static const ALIGN_ASSET(2) char sFeatherNameTex[] = "__OTR__textures/item_name_custom/gRocsFeatherNameTex";
static const ALIGN_ASSET(2) char sCapeIconTex[] = "__OTR__textures/icon_item_custom/gItemIconRocsCapeTex";
static const ALIGN_ASSET(2) char sCapeNameTex[] = "__OTR__textures/item_name_custom/gRocsCapeNameTex";
static const ALIGN_ASSET(2) char sFeatherDL[] = "__OTR__objects/object_nei_rocs_feather/rocs_feather_dl";
static const ALIGN_ASSET(2) char sCapeDL[] = "__OTR__objects/object_nei_rocs_cape/rocs_cape_mesh_dl";
static const ALIGN_ASSET(2) char sJumpAnim[] = "__OTR__objects/object_nei_rocs/gPlayerAnim_nei_rocs_jump";
static const ALIGN_ASSET(2) char sAirJumpAnim[] = "__OTR__objects/object_nei_rocs/gPlayerAnim_nei_rocs_front_flip";

static const u16 sItemButtons[ROCS_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                     BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnResolveCustomGetItem" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static bool sHasDoubleJumped;
static u32 sGroundJumpFrame;

static bool IsOwned(const char* key) {
    return sApi->IsCustomItemOwned(key);
}

static bool IsEquippedOn(u8 button, const char* key) {
    const char* equipped = sApi->GetEquippedCustomItem(button);
    return equipped != NULL && strcmp(equipped, key) == 0;
}

static void SpawnSparkles(Player* player, PlayState* play) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 255, 255, 200, 255 };
    Color_RGBA8 env = { 200, 200, 100, 0 };

    for (s32 i = 0; i < 5; i++) {
        s16 angle = (s16)(Rand_ZeroOne() * 0xFFFF);
        f32 distance = 5.0f + Rand_ZeroOne() * 5.0f;
        Vec3f pos = { player->actor.world.pos.x + Math_SinS(angle) * distance, player->actor.world.pos.y + 5.0f,
                      player->actor.world.pos.z + Math_CosS(angle) * distance };

        EffectSsKiraKira_SpawnSmall(play, &pos, &zero, &zero, &prim, &env);
    }
}

static void SpawnDoubleJumpShockwave(Player* player, PlayState* play) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f pos = { player->actor.world.pos.x, player->actor.floorHeight + 2.0f, player->actor.world.pos.z };

    EffectSsBlast_SpawnWhiteCustomScale(play, &pos, &zero, &zero, 60, 150, 8);
}

static void KeepLedgesGrabbable(Player* player) {
    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
}

static void Jump(Player* player, PlayState* play, const char* animation) {
    func_80838940(player, (LinkAnimationHeader*)animation, ROCS_JUMP_VELOCITY, play, NA_SE_VO_LI_AUTO_JUMP);
    KeepLedgesGrabbable(player);
    SpawnSparkles(player, play);
}

static bool CanJumpFromGround(Player* player, PlayState* play) {
    // A transformation which allows Roc owns its form-specific animation and
    // movement. This item mod supplies vanilla Link's behavior only.
    return sApi->GetActiveForm() == NULL && (player->actor.bgCheckFlags & 1) &&
           !(player->stateFlags1 & ROCS_BLOCKING_STATES);
}

static void JumpFromGround(PlayState* play, Player* player) {
    sHasDoubleJumped = false;
    sGroundJumpFrame = play->state.frames;
    Jump(player, play, sJumpAnim);
}

static s32 FindEquippedTier(const char** key) {
    for (u8 button = 0; button < ROCS_BUTTON_COUNT; button++) {
        if (IsEquippedOn(button, ROCS_CAPE_KEY)) {
            *key = ROCS_CAPE_KEY;
            return button;
        }
        if (IsEquippedOn(button, ROCS_FEATHER_KEY)) {
            *key = ROCS_FEATHER_KEY;
            return button;
        }
    }
    return -1;
}

static void JumpFromAirOrWater(Player* player, PlayState* play) {
    const char* tier = NULL;
    s32 button = FindEquippedTier(&tier);

    if (button < 0 || !(play->state.input[0].press.button & sItemButtons[button]) ||
        play->state.frames == sGroundJumpFrame || (player->stateFlags1 & ROCS_BLOCKING_STATES)) {
        return;
    }
    if (player->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        player->actor.velocity.y = ROCS_WATER_JUMP_VELOCITY;
        Player_PlaySfx(&player->actor, NA_SE_PL_SKIP);
        SpawnSparkles(player, play);
        return;
    }
    if (strcmp(tier, ROCS_CAPE_KEY) != 0 || sHasDoubleJumped) {
        return;
    }
    sHasDoubleJumped = true;
    Jump(player, play, sAirJumpAnim);
    SpawnDoubleJumpShockwave(player, play);
}

static void UpgradeFeatherToCape(void) {
    if (!IsOwned(ROCS_CAPE_KEY) || !IsOwned(ROCS_FEATHER_KEY)) {
        return;
    }
    for (u8 button = 0; button < ROCS_BUTTON_COUNT; button++) {
        if (IsEquippedOn(button, ROCS_FEATHER_KEY)) {
            sApi->EquipCustomItem(button, ROCS_CAPE_KEY);
        }
    }
    sApi->SetCustomItemOwned(ROCS_FEATHER_KEY, false);
}

static void UpdateRocs(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (play == NULL || player == NULL) {
        return;
    }
    UpgradeFeatherToCape();
    if (sApi->GetActiveForm() != NULL) {
        sHasDoubleJumped = false;
        return;
    }
    if (player->actor.bgCheckFlags & 1) {
        sHasDoubleJumped = false;
        return;
    }
    JumpFromAirOrWater(player, play);
}

static bool HasAnyTier(void) {
    return IsOwned(ROCS_FEATHER_KEY) || IsOwned(ROCS_CAPE_KEY);
}

static void ResolveProgressive(Actor* actor, PlayState* play, GetItemEntry* entry, const char** key) {
    if (*key == NULL || strcmp(*key, ROCS_PROGRESSIVE_KEY) != 0) {
        return;
    }
    *key = HasAnyTier() ? ROCS_CAPE_KEY : ROCS_FEATHER_KEY;
}

static void DrawSpinningModel(PlayState* play, const char* displayList, f32 scale) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)displayList);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawFeather(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSpinningModel(play, sFeatherDL, ROCS_FEATHER_GIVE_SCALE);
}

static void DrawCape(PlayState* play, GetItemEntry* getItemEntry) {
    DrawSpinningModel(play, sCapeDL, ROCS_CAPE_GIVE_SCALE);
}

static void DrawProgressive(PlayState* play, GetItemEntry* getItemEntry) {
    if (HasAnyTier()) {
        DrawCape(play, getItemEntry);
    } else {
        DrawFeather(play, getItemEntry);
    }
}

static bool RegisterTier(SOHCustomItemDefinition* tier, CustomDrawFunc draw, int32_t wheelPriority) {
    Z64Items_SetButtons(tier, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(tier, 0, 0, wheelPriority);
    Z64Items_SetCanUse(tier, CanJumpFromGround);
    Z64Items_SetAction(tier, JumpFromGround, NULL);
    tier->flags |= SOH_CUSTOM_ITEM_INSTANT;
    tier->getItemEntry.drawFunc = draw;
    return Z64Items_Register(sApi, tier);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, SetCustomItemOwned) || !SOH_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    SOHCustomItemDefinition feather = Z64Items_Define(ROCS_FEATHER_KEY, sFeatherIconTex, sFeatherNameTex);
    Z64Items_SetTextbox(&feather, "You got %rRoc's Feather%w!&A plume of the giant bird Roc,&light as the wind "
                                  "itself.^"
                                  "Press %y\xA1%w to leap higher than&any run-up could carry you,&or to hop up out of "
                                  "the %bwater%w.");
    Z64Items_SetPauseText(&feather, "%rRoc's Feather&%wPress %y\xA1%w to jump, or to hop up&out of the water.");

    SOHCustomItemDefinition cape = Z64Items_Define(ROCS_CAPE_KEY, sCapeIconTex, sCapeNameTex);
    Z64Items_SetTextbox(&cape, "You got %rRoc's Cape%w!&Roc's feathers woven whole, so&the wind carries you twice.^"
                               "Press %y\xA1%w to jump, then %y\xA1%w again&in midair to ride the wind a&%gsecond "
                               "time%w, even as you fall.");
    Z64Items_SetPauseText(&cape, "%rRoc's Cape&%wPress %y\xA1%w to jump, again in midair&for a second jump.");

    SOHCustomItemDefinition progressive = Z64Items_Define(ROCS_PROGRESSIVE_KEY, NULL, NULL);
    progressive.presentationFlags |= SOH_ITEM_HIDE_FROM_SAVE_EDITOR;
    progressive.getItemEntry.drawFunc = DrawProgressive;

    if (!RegisterTier(&feather, DrawFeather, 0) || !RegisterTier(&cape, DrawCape, 1) ||
        !Z64Items_Register(sApi, &progressive)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateRocs);
    SOH_REGISTER_HOOK(sApi, OnResolveCustomGetItem, ResolveProgressive);
}
