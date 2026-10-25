#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "z64items.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/ModApi/VanillaItems/VanillaItems.h"
#include <ctype.h>
#include "soh/ModApi/ModMenu/ModUI.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"

#define KEY_SIZE 256
// Exported C bridges used by the randomizer itself.
GetItemEntry ItemTable_RetrieveEntry(s16 tableId, s16 getItemId);
GetItemEntry GetItemMystery(void);
typedef struct { int32_t rg; const char* name; const char* key; } NativeItem;
static const NativeItem sNative[] = {
#include "rando_catalog.inc.h"
};
static int32_t sPendingRg = -1;
static int32_t sCatalog;
static int32_t sSelectedRg = -1;
static const SOHModApi* sApi;
static bool sChecklistDirty;
static char sPending[KEY_SIZE];
static char sSelected[KEY_SIZE];
static char sFilter[128];
static const char* sStatus = "Click an item to queue a pickup above Link. Resume gameplay to spawn it.";
static const char* const sHooks[] = { "OnGameFrameUpdate" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sHooks, 1 };

static void DrawNativeDrop(Actor* actor, PlayState* play) {
    EnItem00* item = (EnItem00*)actor;
    GetItemEntry entry = item->itemEntry;
    if (CVarGetInteger("gRandoEnhancements.MysteriousShuffle", 0)) entry = GetItemMystery();
    Matrix_Scale(CVarGetFloat("gRandoEnhancements.TimeSavers.SkipGetItemAnimationScale", 10.0f),
                 CVarGetFloat("gRandoEnhancements.TimeSavers.SkipGetItemAnimationScale", 10.0f),
                 CVarGetFloat("gRandoEnhancements.TimeSavers.SkipGetItemAnimationScale", 10.0f), MTXMODE_APPLY);
    func_8002EBCC(actor, play, 0);
    func_8002ED80(actor, play, 0);
    EnItem00_CustomItemsParticles(actor, play, entry);
    GetItemEntry_Draw(play, entry);
}
static void Tick(void) {
    if (sChecklistDirty) { CVarSave(); sChecklistDirty = false; }
    PlayState* play = gPlayState;
    if ((!sPending[0] && sPendingRg < 0) || play == NULL || !IS_RANDO || play->pauseCtx.state != 0 ||
        play->msgCtx.msgMode != 0 || play->transitionTrigger != 0) return;
    Player* player = GET_PLAYER(play);
    if (Player_InBlockingCsMode(play, player)) return;
    Vec3f pos = player->actor.world.pos;
    pos.y += 80.0f;
    GetItemEntry entry = {0};
    const char* custom = sPending[0] ? sPending : NULL;
    if (custom == NULL) {
        entry = ItemTable_RetrieveEntry(MOD_RANDOMIZER, (s16)sPendingRg);
        if (CustomItemRegistry_IsCustomGetItem(&entry)) {
            const SOHCustomItemDefinition* resolved = sApi->FindCustomItemByRuntimeId(entry.tableId);
            custom = resolved != NULL ? resolved->key : NULL;
        }
    }
    EnItem00* drop = NULL;
    if (custom != NULL) {
        drop = Item_DropCollectible2(play, &pos, ITEM00_SOH_CUSTOM);
        if (drop != NULL && !CustomItemRegistry_AttachDrop(&drop->actor, custom)) {
            Actor_Kill(&drop->actor);
            drop = NULL;
        }
    } else {
        // The resolved row is the same one the randomizer passes to actor-driven pickups.
        if (entry.itemId != ITEM_NONE && entry.gi != 0) {
            drop = Item_DropCollectible2(play, &pos, ITEM00_SOH_GIVE_ITEM_ENTRY_GI);
            if (drop != NULL) {
                drop->itemEntry = entry;
                drop->actor.draw = DrawNativeDrop;
            }
        }
    }
    sStatus = drop != NULL ? "Pickup spawned. Collect it to test the real rando acquisition." :
                            "No collectible entry for this reward, or actor allocation failed.";
    sPending[0] = '\0';
    sPendingRg = -1;
}
static bool Matches(const char* text) {
    if (!sFilter[0]) return true;
    if (text == NULL) return false;
    for (; *text; ++text) {
        const char* a = text; const char* b = sFilter;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { ++a; ++b; }
        if (!*b) return true;
    }
    return false;
}
static void QueueCustom(const char* key) {
    snprintf(sPending, sizeof(sPending), "%s", key);
    snprintf(sSelected, sizeof(sSelected), "%s", key);
    sPendingRg = sSelectedRg = -1;
    sStatus = "Queued. Close the menu and resume gameplay.";
}
static bool HasVariants(const char* key) {
    for (uint32_t i = 0; i < sApi->GetCustomItemCount(); ++i) {
        const SOHCustomItemDefinition* child = sApi->GetCustomItemAt(i);
        const SOHCustomPickupVariant* v = child ? sApi->GetPickupVariant(child->key) : NULL;
        if (v != NULL && !strcmp(v->itemKey, key)) return true;
    }
    return false;
}
static void CustomRow(const SOHCustomItemDefinition* item) {
    if (item == NULL || sApi->GetPickupVariant(item->key) != NULL) return;
    bool match = Matches(item->key);
    bool variants = HasVariants(item->key);
    if (!match && variants) for (uint32_t i = 0; i < sApi->GetCustomItemCount(); ++i) {
        const SOHCustomItemDefinition* child = sApi->GetCustomItemAt(i);
        const SOHCustomPickupVariant* v = child ? sApi->GetPickupVariant(child->key) : NULL;
        if (v && !strcmp(v->itemKey, item->key) && (Matches(v->variantKey) || Matches(v->key))) match = true;
    }
    if (!match) return;
    const int32_t rg = sApi->GetRandoItem(item->key);
    if (rg <= 0 && !variants) return;
    ModUI_PushID(item->key);
    if (ModUI_Selectable(item->key, !strcmp(sSelected, item->key), 0, 0, 0)) {
        snprintf(sSelected, sizeof(sSelected), "%s", item->key); sSelectedRg = -1;
    }
    if (rg > 0 && ModUI_Button("Drop", 0, 0)) QueueCustom(item->key);
    for (uint32_t i = 0; i < sApi->GetCustomItemCount(); ++i) {
        const SOHCustomItemDefinition* child = sApi->GetCustomItemAt(i);
        const SOHCustomPickupVariant* v = child ? sApi->GetPickupVariant(child->key) : NULL;
        if (v == NULL || strcmp(v->itemKey, item->key) || sApi->GetRandoItem(v->key) <= 0) continue;
        ModUI_PushID(v->key);
        if (ModUI_Button(v->variantKey, 0, 0)) QueueCustom(v->key);
        ModUI_PopID();
        ModUI_SameLine(0, -1);
    }
    if (variants) ModUI_NewLine();
    ModUI_Separator();
    ModUI_PopID();
}
static void DrawChecklist(const char* key) {
    static const char* const labels[] = { "Pickup", "Icon", "Acquisition model", "Held model", "Use", "Save/load", "Anchor" };
    static const char* const fields[] = { "pickup", "icon", "acquisition", "held", "use", "save", "anchor" };
    ModUI_Text("Checklist (manual results, saved locally):");
    for (int i = 0; i < 7; ++i) {
        char cvar[KEY_SIZE + 64];
        snprintf(cvar, sizeof(cvar), "gMods.ItemDropTester.Check.%s.%s", key, fields[i]);
        bool passed = CVarGetInteger(cvar, 0) != 0;
        if (ModUI_Checkbox(labels[i], &passed)) {
            CVarSetInteger(cvar, passed);
            sChecklistDirty = true;
        }
    }
}
static void CopyChecklist(void) {
    const uint32_t count = sApi->GetCustomItemCount();
    if (count > 10000) return;
    const size_t capacity = ((size_t)count + sizeof(sNative)/sizeof(sNative[0])) * (KEY_SIZE + 160) + 256;
    char* text = malloc(capacity);
    if (text == NULL) return;
    size_t used = snprintf(text, capacity, "# Ship/OoT rando item checklist\n\n| Item / subtype | Pickup | Icon | Acquisition | Held | Use | Save/load | Anchor |\n|---|---|---|---|---|---|---|---|\n");
    static const char* const fields[] = { "pickup", "icon", "acquisition", "held", "use", "save", "anchor" };
    for (uint32_t i = 0; i < count; ++i) {
        const SOHCustomItemDefinition* item = sApi->GetCustomItemAt(i);
        if (item == NULL || item->key == NULL || strlen(item->key) >= KEY_SIZE) continue;
        used += snprintf(text + used, capacity - used, "| %s |", item->key);
        for (int f = 0; f < 7; ++f) {
            char cvar[KEY_SIZE + 64];
            snprintf(cvar, sizeof(cvar), "gMods.ItemDropTester.Check.%s.%s", item->key, fields[f]);
            used += snprintf(text + used, capacity - used, " %s |", CVarGetInteger(cvar, 0) ? "OK" : "pending");
        }
        used += snprintf(text + used, capacity - used, "\n");
    }
    for (size_t i = 0; i < sizeof(sNative)/sizeof(sNative[0]); ++i) {
        used += snprintf(text + used, capacity - used, "| %s |", sNative[i].name);
        for (int f = 0; f < 7; ++f) {
            char cvar[KEY_SIZE + 64];
            snprintf(cvar, sizeof(cvar), "gMods.ItemDropTester.Check.%s.%s", sNative[i].key, fields[f]);
            used += snprintf(text + used, capacity - used, " %s |", CVarGetInteger(cvar, 0) ? "OK" : "pending");
        }
        used += snprintf(text + used, capacity - used, "\n");
    }
    ModUI_SetClipboardText(text);
    free(text);
    sStatus = "Markdown checklist copied to clipboard.";
}
static void DrawMenu(void) {
    static const char* const modes[] = { "Custom items and variants", "Vanilla / built-in rando rewards" };
    ModUI_TextWrapped("Real rando pickups above Link. Progressives use your current inventory. Ownership is granted only when collected; no seed checks are marked collected.");
    ModUI_TextWrapped(sStatus);
    if (ModUI_Button("Copy full checklist (Markdown)", 0, 0)) CopyChecklist();
    ModUI_Combo("Catalog", &sCatalog, modes, 2);
    ModUI_InputTextWithHint("Filter", "Name, key or subtype", sFilter, sizeof(sFilter), 0);
    if (ModUI_Button("Cancel queued drop", 0, 0)) { sPending[0] = '\0'; sPendingRg = -1; }
    if (gPlayState == NULL || !IS_RANDO) ModUI_TextWrapped("Load a randomizer save to test the real rando acquisition flow.");
    bool visible = ModUI_BeginChild("DropCatalog", 0, 320, true, 0);
    if (visible) {
        ModUI_BeginDisabled(gPlayState == NULL || !IS_RANDO || sPending[0] || sPendingRg >= 0);
        if (sCatalog == 0) {
            for (uint32_t i = 0; i < sApi->GetCustomItemCount(); ++i) CustomRow(sApi->GetCustomItemAt(i));
        } else {
            for (size_t i = 0; i < sizeof(sNative)/sizeof(sNative[0]); ++i) {
                const NativeItem* item = &sNative[i];
                if (!Matches(item->name) && !Matches(item->key)) continue;
                ModUI_PushID(item->key);
                if (ModUI_Selectable(item->name, sSelectedRg == item->rg, 0, 0, 0)) {
                    sSelectedRg = item->rg;
                    snprintf(sSelected, sizeof(sSelected), "%s", item->key);
                }
                if (ModUI_Button("Drop", 0, 0)) {
                    sPendingRg = sSelectedRg = item->rg;
                    snprintf(sSelected, sizeof(sSelected), "%s", item->key);
                    sStatus = "Queued. Close the menu and resume gameplay.";
                }
                const char* replacement = VanillaItems_GetCustomKeyForRandoItem(item->rg);
                if (replacement != NULL) { ModUI_Text("Active mod replacement:"); ModUI_Text(replacement); }
                ModUI_PopID();
            }
        }
        ModUI_EndDisabled();
    }
    ModUI_EndChild();
    if (sSelected[0]) { ModUI_Text(sSelected); DrawChecklist(sSelected); }
    const SOHCustomItemDefinition* selected = sSelectedRg < 0 ? sApi->FindCustomItem(sSelected) : NULL;
    if (selected != NULL) {
        const SOHCustomPickupVariant* subtype = sApi->GetPickupVariant(selected->key);
        if (subtype != NULL) { ModUI_Text(subtype->itemKey); ModUI_Text(subtype->variantKey); }
        ModUI_Text("Icon:"); ModUI_Text(selected->iconPath ? selected->iconPath : "(none)");
        ModUI_Text("Acquisition model:"); ModUI_Text(selected->getItemModelPath ? selected->getItemModelPath :
            selected->getItemEntry.drawFunc ? "Custom draw callback" : "Native draw ID / icon fallback");
    }
}
SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) { sApi = api; }
SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) { return &sRequirements; }
SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, GetPickupVariant)) return;
    sApi->RegisterMenuSidebar("Mods", "Item Drop Tester", "gMods.Menu.Page", 1);
    SOHModMenuWidget widget = {0};
    widget.structSize = sizeof(widget);
    widget.section = "Mods";
    widget.sidebar = "Item Drop Tester";
    widget.type = SOH_MOD_MENU_CUSTOM;
    widget.label = "Item Drop Tester";
    widget.onChange = DrawMenu;
    sApi->RegisterMenuWidget(&widget);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, Tick);
}
