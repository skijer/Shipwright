// Cosmetic Editor colours for replacement (Alt / pack) heart and magic-jar models. Port of marsh6487's
// poc/heart-magic-cosmetics (8481c257). The Cosmetic Editor patches the vanilla display lists at fixed command
// offsets, which does not work on a custom model, so the original wrapped every draw site in the engine. Here the
// same grayscale tint is injected from the draw hooks instead: switched on before the actor or get-item model
// is drawn and off again right after it. Vanilla models keep the editor's own offset patches untouched.

#include "soh/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_gi_hearts/object_gi_hearts.h"
#include "objects/object_gi_magicpot/object_gi_magicpot.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define CVAR_HEARTS "gCosmetics.Consumable.Hearts"
#define CVAR_MAGIC "gCosmetics.Consumable.Magic"

typedef enum {
    TINT_NONE,
    TINT_HEARTS,
    TINT_MAGIC,
} TintKind;

static const char* const sRequiredHooks[] = { "OnActorDraw", "OnActorDrawEnd", "OnGetItemDraw", "OnGetItemDrawPost" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static bool sTintActive;

static bool UsesCustomModel(const char* dlist) {
    return ResourceMgr_FileIsCustomByName(dlist);
}

static TintKind KindForActor(Actor* actor) {
    if (actor->id == ACTOR_ITEM_B_HEART) {
        return UsesCustomModel(gGiHeartContainerDL) ? TINT_HEARTS : TINT_NONE;
    }
    switch (actor->params & 0xFF) {
        case ITEM00_HEART_PIECE:
            return UsesCustomModel(gHeartPieceInteriorDL) || UsesCustomModel(gGiHeartPieceDL) ? TINT_HEARTS : TINT_NONE;
        case ITEM00_HEART_CONTAINER:
            return UsesCustomModel(gHeartContainerInteriorDL) || UsesCustomModel(gGiHeartContainerDL) ? TINT_HEARTS
                                                                                                    : TINT_NONE;
        case ITEM00_MAGIC_LARGE:
            return UsesCustomModel(gGiMagicJarLargeDL) ? TINT_MAGIC : TINT_NONE;
        case ITEM00_MAGIC_SMALL:
            return UsesCustomModel(gGiMagicJarSmallDL) ? TINT_MAGIC : TINT_NONE;
        default:
            return TINT_NONE;
    }
}

static TintKind KindForGetItem(GetItemEntry* entry) {
    switch (entry->gid) {
        case GID_HEART_PIECE:
            return UsesCustomModel(gGiHeartPieceDL) ? TINT_HEARTS : TINT_NONE;
        case GID_HEART_CONTAINER:
            return UsesCustomModel(gGiHeartContainerDL) ? TINT_HEARTS : TINT_NONE;
        case GID_MAGIC_LARGE:
            return UsesCustomModel(gGiMagicJarLargeDL) ? TINT_MAGIC : TINT_NONE;
        case GID_MAGIC_SMALL:
            return UsesCustomModel(gGiMagicJarSmallDL) ? TINT_MAGIC : TINT_NONE;
        default:
            return TINT_NONE;
    }
}

static bool TintBegin(PlayState* play, TintKind kind) {
    Color_RGB8 color;

    if (kind == TINT_HEARTS && CVarGetInteger(CVAR_HEARTS ".Changed", 0)) {
        color = CVarGetColor24(CVAR_HEARTS ".Value", (Color_RGB8){ 255, 70, 50 });
    } else if (kind == TINT_MAGIC && CVarGetInteger(CVAR_MAGIC ".Changed", 0)) {
        color = CVarGetColor24(CVAR_MAGIC ".Value", (Color_RGB8){ 0, 200, 0 });
    } else {
        return false;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, color.r, color.g, color.b, 255);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, color.r, color.g, color.b, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

static void TintEnd(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPGrayscale(POLY_OPA_DISP++, false);
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void BeginActor(Actor* actor, PlayState* play, bool* drawVanilla) {
    sTintActive = TintBegin(play, KindForActor(actor));
}

static void EndActor(Actor* actor, PlayState* play) {
    if (sTintActive) {
        TintEnd(play);
        sTintActive = false;
    }
}

static void BeginGetItem(PlayState* play, GetItemEntry* entry, bool* handled) {
    sTintActive = TintBegin(play, KindForGetItem(entry));
}

static void EndGetItem(PlayState* play, GetItemEntry* entry) {
    if (sTintActive) {
        TintEnd(play);
        sTintActive = false;
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_EN_ITEM00, BeginActor);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_EN_ITEM00, EndActor);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_ITEM_B_HEART, BeginActor);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_ITEM_B_HEART, EndActor);
    SOH_REGISTER_HOOK(sApi, OnGetItemDraw, BeginGetItem);
    SOH_REGISTER_HOOK(sApi, OnGetItemDrawPost, EndGetItem);
}
