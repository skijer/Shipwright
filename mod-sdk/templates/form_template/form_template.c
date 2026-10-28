#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define SWIFT_MASK_KEY "template.swift_mask"
#define SWIFT_FORM_KEY "template.swift"
#define SWIFT_MASK_PAGE 0
#define SWIFT_MASK_SLOT 1
#define SWIFT_MOTION_SCALE 1.4f
#define SWIFT_LEAP_MIN_SPEED 4.0f
#define SWIFT_LEAP_VELOCITY 9.0f
#define BG_ON_GROUND 1

static const ALIGN_ASSET(2) char sMaskIconTex[] = "__OTR__textures/icon_item_static/gItemIconMaskKeatonTex";
static const ALIGN_ASSET(2) char sMaskNameTex[] = "__OTR__textures/item_name_static/gKeatonMaskItemNameENGTex";
static const ALIGN_ASSET(2) char sMaskFaceDL[] = "__OTR__objects/object_link_child/gLinkChildKeatonMaskDL";
static const ALIGN_ASSET(2) char sLeapAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_jump";

static const char* const sRequiredHooks[] = { "OnPlayerActionHandler" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;

static bool IsOnGround(Player* player) {
    return (player->actor.bgCheckFlags & BG_ON_GROUND) != 0;
}

static bool IsRunning(Player* player) {
    return player->linearVelocity >= SWIFT_LEAP_MIN_SPEED;
}

static int32_t LeapInsteadOfRolling(PlayState* play, Player* player) {
    bool pressedA = CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A);
    if (!pressedA || !IsRunning(player) || !IsOnGround(player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    func_80838940(player, (LinkAnimationHeader*)sLeapAnim, SWIFT_LEAP_VELOCITY, play, NA_SE_VO_LI_AUTO_JUMP);
    return SOH_FORM_ACTION_STARTED;
}

static uint16_t KeepHandsFree(int32_t equipType, uint16_t value) {
    if (equipType == EQUIP_TYPE_SHIELD) {
        return EQUIP_VALUE_SHIELD_NONE;
    }
    return value;
}

static const SOHFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_ROLL, LeapInsteadOfRolling },
};

static bool CanWearMask(Player* player, PlayState* play) {
    return IsOnGround(player) &&
           !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_CUTSCENE));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(SWIFT_FORM_KEY);
}

static bool RegisterMask(void) {
    SOHCustomItemDefinition mask = Z64Items_Define(SWIFT_MASK_KEY, sMaskIconTex, sMaskNameTex);

    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&mask, SWIFT_MASK_PAGE, SWIFT_MASK_SLOT, 0);
    Z64Items_SetTextbox(&mask, "You got the %rSwift Mask%w!&Wear it with %y\xA1%w to run faster.&"
                               "Press %y\x9F%w while running to leap.");
    Z64Items_SetPauseText(&mask, "%rSwift Mask&%wRun faster; %y\x9F%w while running leaps.&No shield while worn.");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    return Z64Items_Register(sApi, &mask);
}

static bool RegisterForm(void) {
    SOHFormDefinition swift = { 0 };

    swift.structSize = sizeof(swift);
    swift.key = SWIFT_FORM_KEY;
    swift.label = "Swift";
    swift.kind = SOH_FORM_KIND_LINK;
    swift.item = SWIFT_MASK_KEY;
    swift.motionScale = SWIFT_MOTION_SCALE;
    swift.actions = sActions;
    swift.actionCount = ARRAY_COUNT(sActions);
    swift.resolveEquipment = KeepHandsFree;
    swift.transformMask = sMaskFaceDL;
    return sApi->RegisterForm(&swift);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, ToggleForm)) {
        return;
    }
    if (RegisterMask()) {
        RegisterForm();
    }
}
