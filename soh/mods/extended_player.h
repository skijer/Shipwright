/**
 * extended_player.h - Extended player item action system
 *
 * Maps custom item IDs (ITEM_xxx) to player actions (PLAYER_IA_xxx).
 * Provides lookup functions for item behavior, model groups, and initialization.
 *
 * Used by: z_player.c, kaleido_scope, item logic files
 */
#ifndef EXTENDED_PLAYER_H
#define EXTENDED_PLAYER_H
#include <stdint.h>
#include <stdbool.h>
#include "z64player.h"
#include "z64item.h"
#ifdef __cplusplus
extern "C" {
#endif

// Vanilla array sizes (these are the original array sizes before custom items)
#define VANILLA_SITEMACTIONS_SIZE 56 // Original sItemActions size (up to ITEM_CLAIM_CHECK)
#define VANILLA_PLAYER_IA_COUNT 67   // PLAYER_IA 0x00-0x42 (67 actions)

// Custom item range in ITEM_xxx enum
#define CUSTOM_ITEM_START ITEM_ROCS_FEATHER_SKIJER
#define CUSTOM_ITEM_END ITEM_MM_MASK_FIERCE_DEITY

// Custom PLAYER_IA range
#define CUSTOM_PLAYER_IA_START 0x43 // PLAYER_IA_ROCS_FEATHER_SKIJER
#define CUSTOM_PLAYER_IA_END 0x7F   // PLAYER_IA_BOTTOMLESS_BOTTLE

// MM Mask PLAYER_IA values (0x5D-0x74) — all no-op, transformation handled by item ID check
#define PLAYER_IA_MM_MASK_POSTMAN 0x5D
#define PLAYER_IA_MM_MASK_ALL_NIGHT 0x5E
#define PLAYER_IA_MM_MASK_BLAST 0x5F
#define PLAYER_IA_MM_MASK_STONE 0x60
#define PLAYER_IA_MM_MASK_GREAT_FAIRY 0x61
#define PLAYER_IA_MM_MASK_DEKU 0x62
#define PLAYER_IA_MM_MASK_KEATON 0x63
#define PLAYER_IA_MM_MASK_BREMEN 0x64
#define PLAYER_IA_MM_MASK_BUNNY 0x65
#define PLAYER_IA_MM_MASK_DON_GERO 0x66
#define PLAYER_IA_MM_MASK_SCENTS 0x67
#define PLAYER_IA_MM_MASK_GORON 0x68
#define PLAYER_IA_MM_MASK_ROMANI 0x69
#define PLAYER_IA_MM_MASK_CIRCUS_LEADER 0x6A
#define PLAYER_IA_MM_MASK_KAFEI 0x6B
#define PLAYER_IA_MM_MASK_COUPLE 0x6C
#define PLAYER_IA_MM_MASK_TRUTH 0x6D
#define PLAYER_IA_MM_MASK_ZORA 0x6E
#define PLAYER_IA_MM_MASK_KAMARO 0x6F
#define PLAYER_IA_MM_MASK_GIBDO 0x70
#define PLAYER_IA_MM_MASK_GARO 0x71
#define PLAYER_IA_MM_MASK_CAPTAIN 0x72
#define PLAYER_IA_MM_MASK_GIANT 0x73
#define PLAYER_IA_MM_MASK_FIERCE_DEITY 0x74

// Bottle-with-Magic-Mushroom (caught from Mask of Scents spots in Lost Woods).
// Placed past the MM-mask range so it doesn't collide with PLAYER_IA_MM_MASK_POSTMAN
// (which used to be 0x5D, same as the original enum slot).
#define PLAYER_IA_BOTTLE_MAGIC_MUSHROOM 0x75

// MM bottle-content custom items (Bottle Randomizer, Skijer's NEI). Generic no-op IAs — the
// per-content behavior is dispatched from mm_bottles_behavior when the bottle is used. (Chateau
// Romani + Magic Mushroom already exist with their own IAs and are not re-added here.)
#define PLAYER_IA_BOTTLE_GOLD_DUST 0x76
#define PLAYER_IA_BOTTLE_HOT_SPRING_WATER 0x77
#define PLAYER_IA_BOTTLE_DEKU_PRINCESS 0x78
#define PLAYER_IA_BOTTLE_SEAHORSE 0x79
#define PLAYER_IA_BOTTLE_SPRING_WATER 0x7A
#define PLAYER_IA_BOTTLE_ZORA_EGG 0x7B
#define PLAYER_IA_BOTTLE_HYLIAN_LOACH 0x7C
#define PLAYER_IA_BOTTLE_OBABA_DRINK 0x7D
// Bottle Randomizer extra items (Net + Bottomless Bottle) — behavior deferred.
#define PLAYER_IA_NET 0x7E
#define PLAYER_IA_BOTTOMLESS_BOTTLE 0x7F

// Elemental Wand (Skijer's NEI). It has NO item action of its own, and cannot have one: SoH's
// PlayerItemAction space 0x00-0x7F is completely full (0x5B, the last "unused" slot, is the Mario
// Mask's), and `heldItemAction` / ExtPlayer_GetItemAction's return are BOTH s8 — so 0x80 does not
// mean 128, it means -128. A negative action then walks off the front of sActionModelGroups[] and
// sItemActionUpdateFuncs[] (their bounds check has no lower bound), which is a garbage model group
// and a garbage function pointer: the bow's hand model vanishes and the game crashes on use.
//
// So the wand shares PLAYER_IA_UNUSED_5B with the Mario Mask. That is safe ONLY because both rows
// resolve identically through ExtPlayer_FindByIA — same model group (DEFAULT), same update func
// (func_8083485C), same init (Player_InitDefaultIA) — so it does not matter which one the search
// finds first. Icon, name, slot and RG come from Nei_FindByItem (keyed by ITEM, not IA), so those
// stay the wand's own.
//
// WHEN THE SIX RODS GET REAL BEHAVIOR they will need a distinct action, which means either freeing
// one of the 128 or widening heldItemAction to s16. That is a decision for that task.
#define PLAYER_IA_ELEMENTAL_WAND PLAYER_IA_UNUSED_5B

// ============================================================================
// FUNCTION POINTER TYPES
// ============================================================================
// Guard shared with extended_inventory.h (NeiItem) so neither header redefines
// these typedefs when both are included in one TU. Skijer's NEI
#ifndef NEI_ITEM_ACTION_FUNC_TYPES
#define NEI_ITEM_ACTION_FUNC_TYPES
struct Player;
struct PlayState;
typedef int32_t (*ItemActionUpdateFunc)(struct Player* player, struct PlayState* play);
typedef void (*ItemActionInitFunc)(struct PlayState* play, struct Player* player);
#endif

// ============================================================================
// HELPER FUNCTIONS - Use these instead of directly accessing arrays
// ============================================================================

/**
 * Get the PLAYER_IA_xxx value for a given ITEM_xxx value.
 * Handles both vanilla and custom items using switch for custom items.
 */
int8_t ExtPlayer_GetItemAction(int32_t item);

/**
 * Get the model group for a given PLAYER_IA_xxx value.
 * Handles both vanilla and custom item actions.
 */
uint8_t ExtPlayer_GetActionModelGroup(int32_t itemAction);

/**
 * Get the update function for a given PLAYER_IA_xxx value.
 * Handles both vanilla and custom item actions.
 */
ItemActionUpdateFunc ExtPlayer_GetItemActionUpdateFunc(int32_t itemAction);

/**
 * Get the init function for a given PLAYER_IA_xxx value.
 * Handles both vanilla and custom item actions.
 */
ItemActionInitFunc ExtPlayer_GetItemActionInitFunc(int32_t itemAction);

/**
 * Check if an item ID is a custom item.
 */
static inline bool ExtPlayer_IsCustomItem(int32_t item) {
    return (item >= CUSTOM_ITEM_START && item <= CUSTOM_ITEM_END);
}

/**
 * Check if an item ID is an MM mask item.
 */
static inline bool ExtPlayer_IsMmMaskItem(int32_t item) {
    return (item >= ITEM_MM_MASK_POSTMAN && item <= ITEM_MM_MASK_FIERCE_DEITY);
}

/**
 * Check if a PLAYER_IA value is a custom action.
 */
static inline bool ExtPlayer_IsCustomItemAction(int32_t itemAction) {
    return (itemAction >= CUSTOM_PLAYER_IA_START && itemAction <= CUSTOM_PLAYER_IA_END);
}

#ifdef __cplusplus
}
#endif
#endif // EXTENDED_PLAYER_H
