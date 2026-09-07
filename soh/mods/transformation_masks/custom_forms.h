/**
 * custom_forms.h — single registry for every CUSTOM player form.
 *
 * Phase 1 of the unified-forms effort: one table describes all custom forms
 * (Garo, Gerudo, Pikachu, Kafei, Keaton, Rito) so mask handling, the menu and
 * future ports (2ship) read ONE source of truth instead of scattered
 * switch/ifs. All form assets live inside soh.o2r under a consistent
 * namespace: objects/forms/<modelName>/...
 *
 * Two kinds of custom form:
 *  - CUSTOM_FORM_FULL: runs through the MmForm state machine (gFormState) —
 *    transformation cutscene, combat/moveset, item restrictions. The state
 *    machine itself still lives in mm_player_form.cpp; the registry only
 *    declares the form.
 *  - CUSTOM_FORM_SKIN: visual-only. Wearing the trigger mask toggles a
 *    Link-rigged replacement skeleton via O2rLoader (Player_Draw swap).
 *    Link's animations, moveset and collider stay 100% vanilla.
 *
 * Pikachu, Wolf Link and Mario stay their own thing gameplay-wise; the first
 * two appear here so their trigger items and CVars live in the same registry.
 */
#ifndef CUSTOM_FORMS_H
#define CUSTOM_FORMS_H

#include <libultraship/libultra.h>
#include "z64.h" // Gfx / Vec3f / Vec3s for the limb-draw wrapper

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CustomFormKind {
    CUSTOM_FORM_SKIN = 0, // visual skeleton swap only (O2rLoader)
    CUSTOM_FORM_FULL = 1, // full MmForm state machine (mm_player_form.cpp)
} CustomFormKind;

typedef struct CustomFormDef {
    const char* modelName; // O2rLoader entry name AND objects/forms/<name>/ namespace (NULL = no o2r model)
    const char* label;     // display name
    u8 kind;               // CustomFormKind
    const char* cvar;      // enable gate ("" = always on)
    u8 cvarDefault;
    s32 items[3]; // trigger item ids, ITEM_NONE-padded
    // Root-limb scale applied while ADULT, the same trick MM forms use
    // (sFormProps[].rootAnimScale in mm_player_form.cpp): the animation drives the
    // root limb's height, so a skeleton whose legs are shorter than the animation
    // expects floats above the ground. Scaling the root position instead of the
    // model keeps ONE model for both ages. 1.0f = the rig already matches adult
    // Link. A rig built on MM/OoT-child proportions wants 2376/3377 ≈ 0.7036f.
    // Child never needs it: OoT's child skeleton and MM's human skeleton carry
    // byte-identical jointPos.
    f32 rootScaleAdult;
} CustomFormDef;

extern const CustomFormDef gCustomForms[];
extern const s32 gCustomFormCount;

// Registry lookups.
const CustomFormDef* CustomForms_ByName(const char* modelName);
const CustomFormDef* CustomForms_ByItem(s32 itemId);
// Like ByItem but only returns SKIN entries whose CVar gate is currently on.
const CustomFormDef* CustomForms_SkinByItem(s32 itemId);

// SKIN activation: toggles the O2rLoader forced model for this entry.
// Returns 1 if it handled the item (activated, switched or deactivated).
u8 CustomForms_ToggleSkin(const CustomFormDef* def);
// Model name of the active SKIN, or NULL (FULL forms also force models —
// this filters to registry SKIN entries only).
const char* CustomForms_ActiveSkin(void);
// Clear any active SKIN (scene resets, deaths, save load).
void CustomForms_ClearSkin(void);
// 1 if this SKIN's skeleton actually resolves in the mounted archives
// (Keaton/Rito return 0 until their models ship in soh.o2r).
u8 CustomForms_SkinAvailable(const CustomFormDef* def);

// One-stop item hook for z_player.c's mask-use dispatch: if itemId triggers an
// enabled SKIN form, toggles it (with SFX) and returns 1 — caller should stop
// processing the item. Returns 0 for everything else (including SKIN items
// whose model isn't shipped — those fall through to cosmetic mask wearing).
u8 CustomForms_TrySkinItem(PlayState* play, Player* player, s32 itemId);

// Draw-time redirection. The active form's o2r mirrors vanilla player resource
// names under objects/forms/<model>/object_link_boy|child/, and the wrapper
// swaps in whichever of those the form actually ships — everything it doesn't
// ship keeps rendering vanilla, so equipment keeps working untouched.
// Install the vanilla override with SetChainedOverride, then pass
// CustomForms_OverrideLimbDraw to Player_DrawImpl.
// The rule the whole system runs on: given the vanilla resource name the engine
// is about to use, return the active form's copy of it, or NULL to keep vanilla.
// Call these where a vanilla NAME becomes a pointer — after that the name is
// gone and no redirect is possible.
void* CustomForms_ResolveVanillaResource(const char* vanillaPath);  // display lists
void* CustomForms_ResolveVanillaTexture(const char* vanillaSymbol); // eyes / mouth

// Rito Mask sharing the Farore's Wind cell, the way Roc's Feather shares the
// Nayru's Love one. OtherItem is what the cell can flip to (ITEM_NONE = nothing);
// a two-item cycle answers the same for prev and next. Only offered to a save
// that owns Farore's Wind — the cell IS the state, nothing extra is stored.
void RitoItem_NoteCellItem(s32 item); // remember that this file owns that cell item
void RitoItem_SyncCell(void);         // record ownership (call before the cycler); never writes the cell
s32 RitoItem_OtherItem(void);
u8 RitoItem_CanCycle(void);

u8 CustomForms_WantsPathSwap(void);
void CustomForms_SetChainedOverride(void* fn);
s32 CustomForms_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg);

#ifdef __cplusplus
}
#endif

#endif // CUSTOM_FORMS_H
