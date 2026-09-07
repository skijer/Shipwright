// Skijer's NEI — MM bottle-content behaviors (separated module).
//
// ALL MM-origin bottle-content USE behaviors live here, so adding a behavior for any MM bottle
// item later means editing ONE place. This module only DECIDES what a content should do; the
// OoT-side execution (animations, actor spawns) stays in z_player.c, which owns the static
// Player action tables. Wire-up: z_player.c maps its bottle item -> MmBottle_FromItemId ->
// MmBottle_GetUseBehavior, then runs the chosen behavior.
#ifndef MM_BOTTLES_BEHAVIOR_H
#define MM_BOTTLES_BEHAVIOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// MM-origin bottle contents ported into OoT (concept keys, independent of the OoT ITEM_ ids,
// which are wired in via MmBottle_FromItemId as the item ports land).
typedef enum {
    MM_BOTTLE_NONE = -1,
    MM_BOTTLE_DEKU_PRINCESS = 0,
    MM_BOTTLE_SEAHORSE,
    MM_BOTTLE_ZORA_EGG,
    MM_BOTTLE_GOLD_DUST,
    MM_BOTTLE_CHATEAU_ROMANI,
    MM_BOTTLE_SPRING_WATER,
    MM_BOTTLE_HOT_SPRING_WATER,
    MM_BOTTLE_MAGIC_MUSHROOM,
    MM_BOTTLE_HYLIAN_LOACH,
    MM_BOTTLE_OBABA_DRINK,
    MM_BOTTLE_COUNT
} MmBottleContent;

// What USING the content does in OoT (its MM function may be absent there).
typedef enum {
    MM_BOTTLE_USE_CANT_USE = 0, // trade-quest "can't use here" hold-up anim; bottle stays filled
    MM_BOTTLE_USE_BLUE_FIRE,    // spawn blue fire (ACTOR_EN_ICE_HONO); empties the bottle
    MM_BOTTLE_USE_NATIVE,       // defer to OoT's existing behavior for this content
    MM_BOTTLE_USE_CUSTOM        // FUTURE: a bespoke behavior handled by MmBottle_RunCustom
} MmBottleUseBehavior;

// Decide the use-behavior for an MM bottle content. Edit the table in the .cpp to give a content
// a real behavior later. Out-of-range -> MM_BOTTLE_USE_CANT_USE.
MmBottleUseBehavior MmBottle_GetUseBehavior(MmBottleContent content);

// Map an OoT inventory item id (a ported bottle content) to its concept key, or MM_BOTTLE_NONE if
// it isn't one of the MM bottle contents. Filled in as the item ports land.
MmBottleContent MmBottle_FromItemId(uint16_t ootItemId);

// FUTURE: run a content's bespoke behavior (for rows set to MM_BOTTLE_USE_CUSTOM). play/player are
// PlayState*/Player* passed as void* to keep this header decomp-agnostic. Returns 1 if handled.
int MmBottle_RunCustom(MmBottleContent content, void* play, void* player);

#ifdef __cplusplus
}
#endif

#endif // MM_BOTTLES_BEHAVIOR_H
