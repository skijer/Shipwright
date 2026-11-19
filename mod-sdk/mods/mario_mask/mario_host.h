#ifndef MARIO_HOST_H
#define MARIO_HOST_H

#ifdef S2H_MOD
#include "global.h"
#include "2s2h/ModApi/ModApi.h"
#include "2s2h/ModApi/ModMenu/ModUI.h"
#include "soh_api_names.h"

#define SOH_MOD_RANDO_OPTION_HIDDEN_FROM_RANDO_MENU S2H_MOD_RANDO_OPTION_HIDDEN_FROM_RANDO_MENU
#define MARIO_RANDO_SECTION "Rando"
#define MARIO_RANDO_SIDEBAR_CVAR "gSettings.Menu.RandoSidebarSection"
#define MARIO_RANDO_CVAR_PREFIX "gRando.ModOptions."
#else
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/ModMenu/ModUI.h"

#define MARIO_RANDO_SECTION "Randomizer"
#define MARIO_RANDO_SIDEBAR_CVAR "gSettings.Menu.RandomizerSidebarSection"
#define MARIO_RANDO_CVAR_PREFIX "gRandoSettings.Mods."
#endif

#endif
