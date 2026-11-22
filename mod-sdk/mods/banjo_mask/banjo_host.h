#ifndef BANJO_HOST_H
#define BANJO_HOST_H

#ifdef S2H_MOD
#include "global.h"
#include "2s2h/ModApi/ModApi.h"
#include "2s2h/ModApi/ModMenu/ModUI.h"
#include "soh_api_names.h"

#define SOH_MOD_RANDO_OPTION_HIDDEN_FROM_RANDO_MENU S2H_MOD_RANDO_OPTION_HIDDEN_FROM_RANDO_MENU
#define BANJO_RANDO_SECTION "Rando"
#define BANJO_RANDO_SIDEBAR_CVAR "gSettings.Menu.RandoSidebarSection"
#define BANJO_RANDO_CVAR_PREFIX "gRando.ModOptions."
#define BANJO_SCENE_ID(play) ((play)->sceneId)
#define BANJO_ALLOC_GFX(gfxCtx, size) GRAPH_ALLOC(gfxCtx, size)
#define BANJO_FINALIZE_MATRIX(gfxCtx) Matrix_Finalize(gfxCtx)
#define BANJO_STATE1_SWIMMING PLAYER_STATE1_8000000
#define BANJO_PLAYER_SPEED(player) ((player)->speedXZ)
#define BANJO_PLAYER_HEALTH (gSaveContext.save.saveInfo.playerData.health)
#define BANJO_PLAYER_HEALTH_CAPACITY (gSaveContext.save.saveInfo.playerData.healthCapacity)
#define BANJO_WATERBOX_MIN_X(box) ((box)->minPos.x)
#define BANJO_SURFACE_MATERIAL(colCtx, poly, bgId) SurfaceType_GetMaterial(colCtx, poly, bgId)
#define BANJO_WATERBOX_SURFACE(box) ((box)->minPos.y)
#define BANJO_WATERBOX_MIN_Z(box) ((box)->minPos.z)
#define BANJO_ITEM_BUTTON_DRIGHT 5
#define BANJO_ITEM_BUTTON_DLEFT 6
#define BANJO_ITEM_BUTTON_DDOWN 7
#define BANJO_ITEM_BUTTON_DUP 8
#else
#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/ModMenu/ModUI.h"

#define BANJO_RANDO_SECTION "Randomizer"
#define BANJO_RANDO_SIDEBAR_CVAR "gSettings.Menu.RandomizerSidebarSection"
#define BANJO_RANDO_CVAR_PREFIX "gRandoSettings.Mods."
#define BANJO_SCENE_ID(play) ((play)->sceneNum)
#define BANJO_ALLOC_GFX(gfxCtx, size) Graph_Alloc(gfxCtx, size)
#define BANJO_FINALIZE_MATRIX(gfxCtx) MATRIX_NEWMTX(gfxCtx)
#define BANJO_STATE1_SWIMMING PLAYER_STATE1_IN_WATER
#define BANJO_PLAYER_SPEED(player) ((player)->linearVelocity)
#define BANJO_PLAYER_HEALTH (gSaveContext.health)
#define BANJO_PLAYER_HEALTH_CAPACITY (gSaveContext.healthCapacity)
#define BANJO_WATERBOX_MIN_X(box) ((box)->xMin)
#define BANJO_SURFACE_MATERIAL(colCtx, poly, bgId) SurfaceType_GetSfx(colCtx, poly, bgId)
#define BANJO_WATERBOX_SURFACE(box) ((box)->ySurface)
#define BANJO_WATERBOX_MIN_Z(box) ((box)->zMin)
#define BANJO_ITEM_BUTTON_DUP 4
#define BANJO_ITEM_BUTTON_DDOWN 5
#define BANJO_ITEM_BUTTON_DLEFT 6
#define BANJO_ITEM_BUTTON_DRIGHT 7
#endif

#endif
