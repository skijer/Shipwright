#include "mario_item.h"

#include "mario_form.h"
#include "z64items.h"
#ifdef S2H_MOD
#include "z64formmasks.h"
#endif

#define MARIO_MASK_PAGE 1
#define MARIO_MASK_SLOT 22

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconMarioMaskTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gMarioMaskNameTex";

static const SOHModApi* sApi;

static const SOHCustomItemRandomizer sRandomizerItem = {
    sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, MARIO_MASK_KEY, MARIO_MASK_KEY, "", NULL,
    "mario_mask.shuffle_mask", 1,
};

static bool CanWearMask(Player* player, PlayState* play) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !(player->stateFlags1 & MARIO_STATE1_SWIMMING);
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(MARIO_FORM_KEY);
}

void MarioItem_Register(const SOHModApi* api) {
    SOHCustomItemDefinition mask = Z64Items_Define(MARIO_MASK_KEY, sIconTex, sNameTex);

    sApi = api;
    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, MARIO_MASK_PAGE, MARIO_MASK_SLOT, 0);
    Z64Items_SetTextbox(&mask, "You got the %rMario Mask%w! A red cap&from another world.^"
                               "Wear it with %y\xA1%w to become Mario,&who jumps, punches and dives.");
    Z64Items_SetPauseText(&mask, "%rMario Mask&%wPress %y\xA1%w to become Mario.&Press it again to be yourself.");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_SetLogic(&mask, &sRandomizerItem);
    Z64Items_Register(api, &mask);
#ifdef S2H_MOD
    Z64FormMasks_PlaceMask(api, MARIO_MASK_KEY, MARIO_MASK_SLOT);
#endif
}
