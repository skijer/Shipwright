#include <stdbool.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "mario_body.h"
#include "mario_buttons.h"
#include "mario_config.h"
#include "mario_controls.h"
#include "mario_form.h"
#include "mario_item.h"
#include "mario_menu.h"
#include "mario_page.h"
#include "mario_powers.h"
#include "mario_rando.h"
#include "mario_rom.h"

#define MARIO_BETA_CVAR "gMods.MarioMask.BetaNoticeVersion"
#define MARIO_BETA_VERSION 1

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };
static const SOHModApi* sApi;

// Shown once per version on the first played frame: Fleet's seed workers load every mod but never
// play, and the blocking dialog would stall them.
static void TellBetaOnce(void) {
    if (CVarGetInteger(MARIO_BETA_CVAR, 0) == MARIO_BETA_VERSION) {
        return;
    }
    sApi->TellPlayer("Mario Mask (Beta)", "Mario Mask is a beta: expect bugs and missing moves. Please report "
                                       "what you find in Skijer's thread on the HM Discord.");
    CVarSetInteger(MARIO_BETA_CVAR, MARIO_BETA_VERSION);
    CVarSave();
}

static void FinishSetupOnFirstFrame(void) {
    static bool sIsSetUp = false;

    if (sIsSetUp) {
        return;
    }
    sIsSetUp = true;
    TellBetaOnce();
    MarioRando_RegisterMenuLink(sApi);
    MarioRom_LoadOrAskOnce();
}

// Only a change of the switch moves the player in or out, so the mask keeps working while Mario Mode is off. The
// form needs its mask, so Mario Mode hands it over.
static void FollowMarioModeSwitch(void) {
    static bool sWasOn = false;
    const bool isOn = MarioConfig_IsMarioModeOn() && MarioRom_Get() != NULL;

    if (isOn == sWasOn) {
        return;
    }
    sWasOn = isOn;
    if (isOn) {
        sApi->SetCustomItemOwned(MARIO_MASK_KEY, true);
    }
    if (isOn != sApi->IsFormActive(MARIO_FORM_KEY)) {
        sApi->SetActiveForm(isOn ? MARIO_FORM_KEY : NULL);
    }
}

static void UpdateFrame(void) {
    FinishSetupOnFirstFrame();
    FollowMarioModeSwitch();
    MarioPowers_GrantWithMask();
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, IsFormSuspended)) {
        return;
    }
    sApi->RegisterMenuSidebar(MARIO_MENU_SECTION, MARIO_MENU_SIDEBAR, MARIO_MENU_PAGE_CVAR, MARIO_COLUMN_COUNT);
    MarioControls_RegisterMenu(sApi);
    MarioRando_RegisterOptions(sApi);
    MarioConfig_RegisterMenu(sApi);
    MarioRom_RegisterMenu(sApi);
    MarioBody_Register(sApi);
    MarioButtons_Register(sApi);
    MarioItem_Register(sApi);
    MarioPage_Register(sApi);
    MarioPowers_Register(sApi);
    MarioForm_Register(sApi);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, UpdateFrame);
}
