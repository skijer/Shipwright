// Chest size follows the contents (the classic CSMC size half), as an Unbound mod. Port of marsh6487's
// "Chest Size Matches Contents" (feat/nei-gi-upgrade-recovered-20260927, d676a5ff). Authored chest types,
// params, switches and rewards are untouched: only the scale and focus height are set, after the chest's own
// update has applied its type-based size.

#include "soh/ModApi/ModApi.h"
#include "soh/Enhancements/item-tables/ItemTableTypes.h"
#include "soh/Enhancements/randomizer/item_category_adj.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "overlays/actors/ovl_En_Box/z_en_box.h"

#define CSMC_CVAR "gMods.ChestSizeMatchesContents.Enabled"
#define CSMC_SMALL_SCALE 0.005f
#define CSMC_LARGE_SCALE 0.01f
#define CSMC_SMALL_FOCUS 20.0f
#define CSMC_LARGE_FOCUS 40.0f

static const char* const sRequiredHooks[] = { "OnActorUpdate" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;

static bool IsSmallContents(GetItemCategory category) {
    return category == ITEM_CATEGORY_JUNK || category == ITEM_CATEGORY_SMALL_KEY ||
           category == ITEM_CATEGORY_SKULLTULA_TOKEN;
}

static void ResizeChest(void* actor) {
    EnBox* this = (EnBox*)actor;
    PlayState* play = gPlayState;

    if (play == NULL || !CVarGetInteger(CSMC_CVAR, 0)) {
        return;
    }
    // The guessing rooms of the Treasure Chest Game keep their authored sizes so the contents stay hidden.
    if (play->sceneNum == SCENE_TREASURE_BOX_SHOP && this->dyna.actor.room != 6) {
        return;
    }

    bool small = IsSmallContents(Randomizer_AdjustItemCategory(this->getItemEntry));

    Actor_SetScale(&this->dyna.actor, small ? CSMC_SMALL_SCALE : CSMC_LARGE_SCALE);
    Actor_SetFocus(&this->dyna.actor, small ? CSMC_SMALL_FOCUS : CSMC_LARGE_FOCUS);
}

static void RegisterMenuToggle(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        return;
    }
    SOHModMenuWidget widget = { sizeof(SOHModMenuWidget) };
    widget.section = "Enhancements";
    widget.sidebar = "Items";
    widget.type = SOH_MOD_MENU_CHECKBOX;
    widget.label = "Chest Size Matches Contents";
    widget.cvar = CSMC_CVAR;
    widget.tooltip = "Junk, small keys and Skulltula tokens come in small chests; everything else in large ones.";
    widget.defaultInt = 0;
    sApi->RegisterMenuWidget(&widget);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_BOX, ResizeChest);
    RegisterMenuToggle();
}
