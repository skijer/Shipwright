#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define HEART_CHARM_KEY "template.heart_charm"
#define HEART_CHARM_PAGE 0
#define HEART_CHARM_SLOT 0
#define HEART_CHARM_RUPEE_COST 10
#define HEART_CHARM_HEALING (2 * 0x10)
#define HEART_CHARM_USES_PER_SCENE 3

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_static/gItemIconBottleFairyTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_static/gBottledFairyItemNameENGTex";

static const char* const sRequiredHooks[] = { "OnSceneInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static int32_t sUsesThisScene;

static bool IsHealthFull(void) {
    return gSaveContext.health >= gSaveContext.healthCapacity;
}

static bool CanAffordCharm(void) {
    return gSaveContext.rupees >= HEART_CHARM_RUPEE_COST;
}

static bool CanUseCharm(Player* player, PlayState* play) {
    return CanAffordCharm() && !IsHealthFull() && sUsesThisScene < HEART_CHARM_USES_PER_SCENE &&
           !(player->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE);
}

static void UseCharm(PlayState* play, Player* player) {
    Rupees_ChangeBy(-HEART_CHARM_RUPEE_COST);
    Health_ChangeBy(play, HEART_CHARM_HEALING);
    Sfx_PlaySfxCentered(NA_SE_SY_HP_RECOVER);
    sUsesThisScene++;
}

static int32_t CountAffordableUses(const char* key) {
    return gSaveContext.rupees / HEART_CHARM_RUPEE_COST;
}

static void ResetSceneCounter(int16_t sceneNum) {
    sUsesThisScene = 0;
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHCustomItemDefinition charm = Z64Items_Define(HEART_CHARM_KEY, sIconTex, sNameTex);

    charm.flags |= SOH_CUSTOM_ITEM_INSTANT;
    Z64Items_SetButtons(&charm, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&charm, HEART_CHARM_PAGE, HEART_CHARM_SLOT, 0);
    Z64Items_SetTextbox(&charm, "You got the %rHeart Charm%w!&Press %y\xA1%w to trade %g10 rupees%w&for two hearts, "
                                "three times per area.");
    Z64Items_SetPauseText(&charm, "%rHeart Charm&%wTrades 10 rupees for two hearts,&three times per area.");
    Z64Items_SetCanUse(&charm, CanUseCharm);
    Z64Items_SetAction(&charm, UseCharm, NULL);
    Z64Items_SetAmmo(&charm, CountAffordableUses);

    if (!Z64Items_Register(sApi, &charm)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ResetSceneCounter);
}
