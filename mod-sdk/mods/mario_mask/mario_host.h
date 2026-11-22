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
#define MARIO_PLAYER_SPEED(player) ((player)->speedXZ)
#define MARIO_SCENE_ID(play) ((play)->sceneId)
#define MARIO_IDENTITY_MTX gIdentityMtx
#define MARIO_ALLOC_GFX(gfxCtx, size) GRAPH_ALLOC(gfxCtx, size)
#define MARIO_STATE1_GETTING_ITEM PLAYER_STATE1_400
#define MARIO_STATE1_IN_ITEM_CS PLAYER_STATE1_10000000
#define MARIO_STATE1_SWIMMING PLAYER_STATE1_8000000
#define MARIO_ITEM_BUTTON_DRIGHT 5
#define MARIO_PLAYER_HEALTH (gSaveContext.save.saveInfo.playerData.health)
#define MARIO_PLAYER_HEALTH_CAPACITY (gSaveContext.save.saveInfo.playerData.healthCapacity)
#define MARIO_CONSUME_MAGIC(play, amount) Magic_Consume(play, amount, MAGIC_CONSUME_NOW)
#define MARIO_ITEM_BUTTON_DLEFT 6
#define MARIO_ITEM_BUTTON_DDOWN 7
#define MARIO_ITEM_BUTTON_DUP 8
#else
#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/ModMenu/ModUI.h"

#define MARIO_RANDO_SECTION "Randomizer"
#define MARIO_RANDO_SIDEBAR_CVAR "gSettings.Menu.RandomizerSidebarSection"
#define MARIO_RANDO_CVAR_PREFIX "gRandoSettings.Mods."
#define MARIO_PLAYER_SPEED(player) ((player)->linearVelocity)
#define MARIO_SCENE_ID(play) ((play)->sceneNum)
#define MARIO_IDENTITY_MTX gMtxClear
#define MARIO_ALLOC_GFX(gfxCtx, size) Graph_Alloc(gfxCtx, size)
#define MARIO_STATE1_GETTING_ITEM PLAYER_STATE1_GETTING_ITEM
#define MARIO_STATE1_IN_ITEM_CS PLAYER_STATE1_IN_ITEM_CS
#define MARIO_STATE1_SWIMMING PLAYER_STATE1_IN_WATER
#define MARIO_ITEM_BUTTON_DUP 4
#define MARIO_PLAYER_HEALTH (gSaveContext.health)
#define MARIO_PLAYER_HEALTH_CAPACITY (gSaveContext.healthCapacity)
#define MARIO_CONSUME_MAGIC(play, amount) Magic_RequestChange(play, amount, MAGIC_CONSUME_NOW)
#define MARIO_ITEM_BUTTON_DDOWN 5
#define MARIO_ITEM_BUTTON_DLEFT 6
#define MARIO_ITEM_BUTTON_DRIGHT 7
#endif

#endif
