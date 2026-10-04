// marsh6487's redesigned get-item models for Skijer's NEI items, as a separate mod. Models and presentation
// (scale, spin) from marsh6487's feat/nei-gi-upgrade-recovered-20260927 (bea6f438..c77c1858,
// soh/assets/custom/objects/nei_gi_redesign, NeiGiPresentation.cpp). The items themselves are not touched: the mod
// listens to OnGetItemDraw, recognises an item by the draw function its definition registered, and draws the
// redesigned model in its place. If this mod or a model is missing, the item's own model draws as before.
//
// Not ported: the optional shimmer / energy-orb effects and the shop-shelf fitting of the original.

#include "soh/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"

#define GI_ROOT "__OTR__objects/nei_gi_redesign/"
#define GI_PATHS(name, slug)                                                                  \
    static const ALIGN_ASSET(2) char name##Opa[] = GI_ROOT slug "/gi_dl";                     \
    static const ALIGN_ASSET(2) char name##Xlu[] = GI_ROOT slug "/gi_xlu_dl"

GI_PATHS(sFireRod, "fire_rod");
GI_PATHS(sIceRod, "ice_rod");
GI_PATHS(sLightRod, "light_rod");
GI_PATHS(sFeather, "rocs_feather");
GI_PATHS(sCape, "rocs_cape");
GI_PATHS(sTimeGate, "time_gate");
GI_PATHS(sWhip, "whip");
GI_PATHS(sShovel, "shovel");
GI_PATHS(sGustJar, "gust_jar");
GI_PATHS(sZonai, "zonai_permafrost");
GI_PATHS(sDemise, "demise_destruction");
GI_PATHS(sBall, "ball_and_chain");
GI_PATHS(sLeaf, "deku_leaf");
GI_PATHS(sMitts, "mogma_mitts");
GI_PATHS(sHook, "switch_hook");
GI_PATHS(sBeetle, "beetle");
GI_PATHS(sLantern, "lantern");
GI_PATHS(sSpinner, "spinner");
GI_PATHS(sSomaria, "cane_of_somaria");
GI_PATHS(sMinish, "minish_cap");

typedef struct {
    const char* key;
    const char* opaque;
    const char* translucent; // NULL when the model has no translucent pass
    f32 scale;
    CustomDrawFunc draw; // the draw function the item registered, learned on first use
} Presentation;

static Presentation sItems[] = {
    { "nei.rod_fire", sFireRodOpa, NULL, 0.2f },
    { "nei.rod_ice", sIceRodOpa, NULL, 0.2f },
    { "nei.rod_light", sLightRodOpa, NULL, 0.2f },
    { "nei.rocs_feather", sFeatherOpa, NULL, 0.5f },
    { "nei.rocs_cape", sCapeOpa, NULL, 0.6f },
    { "nei.time_gate", sTimeGateOpa, NULL, 0.5f },
    { "nei.whip", sWhipOpa, NULL, 0.5f },
    { "nei.shovel", sShovelOpa, NULL, 0.2f },
    { "nei.gust_jar", sGustJarOpa, NULL, 5.0f },
    { "nei.zonai_permafrost", sZonaiOpa, sZonaiXlu, 1.0f },
    { "nei.demise_destruction", sDemiseOpa, sDemiseXlu, 1.0f },
    { "nei.ball_and_chain", sBallOpa, NULL, 0.25f },
    { "nei.deku_leaf", sLeafOpa, NULL, 0.5f },
    { "nei.mogma_mitts", sMittsOpa, NULL, 0.5f },
    { "nei.switch_hook", sHookOpa, NULL, 0.01f },
    { "nei.beetle", sBeetleOpa, NULL, 0.3f },
    { "nei.lantern", sLanternOpa, sLanternXlu, 0.025f },
    { "nei.spinner", sSpinnerOpa, NULL, 0.3f },
    { "nei.somaria", sSomariaOpa, NULL, 0.25f },
    { "nei.minish_cap", sMinishOpa, NULL, 0.5f },
};

static const char* const sRequiredHooks[] = { "OnGetItemDraw" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;

// Items register in load order, so the draw functions are resolved when first needed and kept.
static Presentation* FindPresentation(const GetItemEntry* entry) {
    if (entry->drawFunc == NULL) {
        return NULL;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sItems); i++) {
        Presentation* item = &sItems[i];

        if (item->draw == NULL) {
            const SOHCustomItemDefinition* definition = sApi->FindCustomItem(item->key);

            if (definition != NULL) {
                item->draw = definition->getItemEntry.drawFunc;
            }
        }
        if (item->draw != NULL && item->draw == entry->drawFunc) {
            return item;
        }
    }
    return NULL;
}

// The same spin as the items' own diamonds: a signed 16-bit angle, two steps per frame.
static f32 Spin(PlayState* play) {
    return (s16)(play->gameplayFrames * 2) * 0.01f;
}

static void DrawPass(PlayState* play, const char* model, f32 scale, bool translucent) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_RotateY(Spin(play), MTXMODE_APPLY);
    if (translucent) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)model);
    } else {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)model);
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawRedesign(PlayState* play, GetItemEntry* entry, bool* handled) {
    Presentation* item = FindPresentation(entry);

    // Only take over when every pass this model needs is in the mounted archives.
    if (item == NULL || !sApi->HasResource(item->opaque) ||
        (item->translucent != NULL && !sApi->HasResource(item->translucent))) {
        return;
    }
    DrawPass(play, item->opaque, item->scale, false);
    if (item->translucent != NULL) {
        DrawPass(play, item->translucent, item->scale, true);
    }
    *handled = true;
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOH_REGISTER_HOOK(sApi, OnGetItemDraw, DrawRedesign);
}
