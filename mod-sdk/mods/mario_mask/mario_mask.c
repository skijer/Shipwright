#include <stdbool.h>

#include "mario_config.h"
#include "mario_controls.h"
#include "mario_form.h"
#include "mario_menu.h"
#include "mario_rando.h"
#include "mario_rom.h"

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
    MarioRando_RegisterMenuLink(sApi);
    MarioRom_LoadOrAskOnce();
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, ReadModsFolderFiles)) {
        return;
    }
    sApi->RegisterMenuSidebar(MARIO_MENU_SECTION, MARIO_MENU_SIDEBAR, MARIO_MENU_PAGE_CVAR, MARIO_COLUMN_COUNT);
    MarioControls_RegisterMenu(sApi);
    MarioRando_RegisterOptions(sApi);
    MarioConfig_RegisterMenu(sApi);
    MarioRom_RegisterMenu(sApi);
    MarioForm_Register(sApi);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, FinishSetupOnFirstFrame);
}
