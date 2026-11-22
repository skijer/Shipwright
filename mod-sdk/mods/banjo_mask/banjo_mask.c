#include <stdbool.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "banjo_audio.h"
#include "banjo_body.h"
#include "banjo_buttons.h"
#include "banjo_config.h"
#include "banjo_controls.h"
#include "banjo_form.h"
#include "banjo_item.h"
#include "banjo_menu.h"
#include "banjo_moves.h"
#include "banjo_pads.h"
#include "banjo_page.h"
#include "banjo_rando.h"
#include "banjo_rom.h"

#define BANJO_BETA_CVAR "gMods.BanjoMask.BetaNoticeVersion"
#define BANJO_BETA_VERSION 1

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };
static const SOHModApi* sApi;

static void FinishSetupOnFirstFrame(void) {
    static bool sIsSetUp = false;

    if (sIsSetUp) {
        return;
    }
    sIsSetUp = true;
    BanjoRando_RegisterMenuLink(sApi);
    BanjoRom_LoadOrAskOnce();
}

// Only a change of the switch moves the player in or out, so the mask keeps working while Banjo Mode is off. The
// form needs its mask, so Banjo Mode hands it over.
static void FollowBanjoModeSwitch(void) {
    static bool sWasOn = false;
    const bool isOn = BanjoConfig_IsBanjoModeOn() && BanjoRom_Get() != NULL;

    if (isOn == sWasOn) {
        return;
    }
    sWasOn = isOn;
    if (isOn) {
        sApi->SetCustomItemOwned(BANJO_MASK_KEY, true);
    }
    if (isOn != sApi->IsFormActive(BANJO_FORM_KEY)) {
        sApi->SetActiveForm(isOn ? BANJO_FORM_KEY : NULL);
    }
}

static void UpdateFrame(void) {
    FinishSetupOnFirstFrame();
    FollowBanjoModeSwitch();
    BanjoMoves_GrantWithMask();
    BanjoPads_GrantWithMask();
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

// Shown once per version: the mask is still in testing.
static void TellBetaOnce(void) {
    if (CVarGetInteger(BANJO_BETA_CVAR, 0) == BANJO_BETA_VERSION) {
        return;
    }
    sApi->TellPlayer("Banjo Mask (Beta)", "Banjo Mask is a beta: expect bugs and missing moves. Please report what you "
                                       "find on the NEI Discord.");
    CVarSetInteger(BANJO_BETA_CVAR, BANJO_BETA_VERSION);
    CVarSave();
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, IsFormSuspended) || !SOH_MOD_API_HAS(sApi, ReadModsFolderFiles)) {
        return;
    }
    TellBetaOnce();
    sApi->RegisterMenuSidebar(BANJO_MENU_SECTION, BANJO_MENU_SIDEBAR, BANJO_MENU_PAGE_CVAR, BANJO_COLUMN_COUNT);
    BanjoControls_RegisterMenu(sApi);
    BanjoRando_RegisterOptions(sApi);
    BanjoConfig_RegisterMenu(sApi);
    BanjoRom_RegisterMenu(sApi);
    BanjoAudio_Register(sApi);
    BanjoBody_Register(sApi);
    BanjoButtons_Register(sApi);
    BanjoItem_Register(sApi);
    BanjoPage_Register(sApi);
    BanjoMoves_Register(sApi);
    BanjoPads_Register(sApi);
    BanjoForm_Register(sApi);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, UpdateFrame);
}
