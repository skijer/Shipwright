#include "banjo_item.h"

#include "banjo_animation_ids.h"
#include "banjo_body.h"
#include "banjo_form.h"
#include "z64items.h"
#ifdef S2H_MOD
#include "z64formmasks.h"
#endif

#define BANJO_MASK_PAGE 1
#define BANJO_MASK_SLOT 19

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconBanjoMaskTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gBanjoMaskNameTex";

static const SOHModApi* sApi;

static const SOHCustomItemRandomizer sRandomizerItem = {
    sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, BANJO_MASK_KEY, BANJO_MASK_KEY, "", NULL,
    "banjo_mask.shuffle_mask", 1,
};

// The mask's get-item is Banjo himself, standing idle.
static void DrawMaskGetItem(PlayState* play, BANJO_GET_ITEM_ENTRY* entry) {
    BanjoBody_DrawFigure(play, BANJO_ANIM_STAND_IDLE, false);
}

static bool CanWearMask(Player* player, PlayState* play) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !(player->stateFlags1 & BANJO_STATE1_SWIMMING);
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(BANJO_FORM_KEY);
}

void BanjoItem_Register(const SOHModApi* api) {
    SOHCustomItemDefinition mask = Z64Items_Define(BANJO_MASK_KEY, sIconTex, sNameTex);

    sApi = api;
    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, BANJO_MASK_PAGE, BANJO_MASK_SLOT, 0);
    Z64Items_SetTextbox(&mask, "You got the %rBanjo Mask%w! A honey bear&and a red-crested breegull.^"
                               "Wear it with %y\xA1%w to become Banjo,&with Kazooie in his backpack.");
    Z64Items_SetPauseText(&mask, "%rBanjo Mask&%wPress %y\xA1%w to become Banjo and Kazooie.&"
                                 "Press it again to be yourself.");
    mask.getItemEntry.drawFunc = DrawMaskGetItem;
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_SetLogic(&mask, &sRandomizerItem);
    Z64Items_Register(api, &mask);
#ifdef S2H_MOD
    Z64FormMasks_PlaceMask(api, BANJO_MASK_KEY, BANJO_MASK_SLOT);
#endif
}
