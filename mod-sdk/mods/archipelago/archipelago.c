#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ModApi/ModApi.h"

// Fleet's "ArchipelagoItems" service (src/archipelago/ApSession.h); Fleet owns the strings for the whole run.
typedef struct {
    const char* key;
    const char* item;
    const char* player;
    const char* game;
    uint32_t flags;
} FleetArchipelagoItem;

typedef const FleetArchipelagoItem* (*FleetArchipelagoItemsFn)(const char* game, uint32_t* count);

void* FleetGame_GetHostFunction(const char* name);

enum {
    AP_FLAG_PROGRESSION = 1 << 0,
    AP_FLAG_USEFUL = 1 << 1,
    AP_FLAG_TRAP = 1 << 2,
};

typedef enum {
    AP_CLASS_PROGRESSION,
    AP_CLASS_USEFUL,
    AP_CLASS_TRAP,
    AP_CLASS_FILLER,
    AP_CLASS_COUNT,
} ApClass;

// The Archipelago logo models of the Shipwright Archipelago branch, drawn at that branch's scale.
#define AP_MODEL_SCALE 0.035f

static const ALIGN_ASSET(2) char sProgressionDL[] = "__OTR__objects/object_archipelago_item/gArchipelagoProgressiveDL";
static const ALIGN_ASSET(2) char sUsefulDL[] = "__OTR__objects/object_archipelago_item/gArchipelagoItemDL";
static const ALIGN_ASSET(2) char sJunkDL[] = "__OTR__objects/object_archipelago_item/gArchipelagoJunkDL";

static const SOHModApi* sApi;
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };

static ApClass ClassOf(uint32_t flags) {
    if (flags & AP_FLAG_PROGRESSION) {
        return AP_CLASS_PROGRESSION;
    }
    if (flags & AP_FLAG_TRAP) {
        return AP_CLASS_TRAP;
    }
    return (flags & AP_FLAG_USEFUL) ? AP_CLASS_USEFUL : AP_CLASS_FILLER;
}

static void DrawModel(PlayState* play, const char* model) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(AP_MODEL_SCALE, AP_MODEL_SCALE, AP_MODEL_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)model);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawProgression(PlayState* play, GetItemEntry* entry) {
    DrawModel(play, sProgressionDL);
}

static void DrawUseful(PlayState* play, GetItemEntry* entry) {
    DrawModel(play, sUsefulDL);
}

static void DrawFiller(PlayState* play, GetItemEntry* entry) {
    DrawModel(play, sJunkDL);
}

// Like the Archipelago branch, anything neither progression nor junk, traps included, takes the useful model.
static const CustomDrawFunc sDraws[AP_CLASS_COUNT] = { DrawProgression, DrawUseful, DrawUseful, DrawFiller };

// The registry keeps these pointers, so each string lives for the whole run.
static const char* Format(const char* format, const FleetArchipelagoItem* item) {
    const int size = snprintf(NULL, 0, format, item->item, item->player, item->game) + 1;
    char* text = malloc(size);
    snprintf(text, size, format, item->item, item->player, item->game);
    return text;
}

static bool Register(const FleetArchipelagoItem* item) {
    const ApClass itemClass = ClassOf(item->flags);
    SOHCustomItemRandomizer* randomizer = calloc(1, sizeof(SOHCustomItemRandomizer));
    randomizer->structSize = sizeof(SOHCustomItemRandomizer);
    randomizer->type = SOH_CUSTOM_ITEM_TYPE_ITEM;
    randomizer->poolCount = 0;
    randomizer->flags = itemClass == AP_CLASS_PROGRESSION ? SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT : 0;
    SOHCustomItemDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = item->key;
    definition.displayName = Format("%s (%s's)", item);
    definition.pauseText = definition.displayName;
    definition.getItemText = Format("You found %%y%s%%w&for %s (%s)!", item);
    definition.presentationFlags = SOH_ITEM_HIDE_FROM_SAVE_EDITOR | SOH_ITEM_HIDE_FROM_KALEIDO;
    definition.disabledIconSurfaces = ~0u;
    definition.getItemEntry.drawFunc = sDraws[itemClass];
    definition.randomizer = randomizer;
    return sApi->RegisterCustomItem(&definition);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    const FleetArchipelagoItemsFn items = (FleetArchipelagoItemsFn)FleetGame_GetHostFunction("ArchipelagoItems");
    if (items == NULL) {
        return;
    }
    uint32_t count = 0;
    const FleetArchipelagoItem* list = items("oot", &count);
    for (uint32_t index = 0; index < count; index++) {
        if (!Register(&list[index])) {
            return;
        }
    }
}
