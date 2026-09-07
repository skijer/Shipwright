// Skijer's NEI — MM bottle-content behaviors (see header).
//
// One place for ALL MM bottle behaviors so new ones are easy to add. Pure decision logic (no
// decomp internals here) — z_player.c executes the chosen behavior because it owns the static
// Player action tables (the "can't use" exchange anim, the EN_ICE_HONO blue-fire spawn, etc.).
#include "mm_bottles_behavior.h"

// Per-content use-behavior table. DEFAULT = MM_BOTTLE_USE_CANT_USE (the trade-quest "can't use
// here" hold-up: Link raises the bottle, no effect, the bottle STAYS filled). To give a content a
// real behavior, change its row here (and, for MM_BOTTLE_USE_CUSTOM, fill MmBottle_RunCustom).
// Order MUST match the MmBottleContent enum.
static const MmBottleUseBehavior sMmBottleUse[MM_BOTTLE_COUNT] = {
    /* MM_BOTTLE_DEKU_PRINCESS    */ MM_BOTTLE_USE_CANT_USE,
    /* MM_BOTTLE_SEAHORSE         */ MM_BOTTLE_USE_CANT_USE,
    /* MM_BOTTLE_ZORA_EGG         */ MM_BOTTLE_USE_CANT_USE,
    /* MM_BOTTLE_GOLD_DUST        */ MM_BOTTLE_USE_CANT_USE,
    /* MM_BOTTLE_CHATEAU_ROMANI   */ MM_BOTTLE_USE_CANT_USE, // FUTURE idea: refill magic
    /* MM_BOTTLE_SPRING_WATER     */ MM_BOTTLE_USE_CANT_USE,
    /* MM_BOTTLE_HOT_SPRING_WATER */ MM_BOTTLE_USE_BLUE_FIRE, // user: acts as Blue Fire in OoT
    /* MM_BOTTLE_MAGIC_MUSHROOM   */ MM_BOTTLE_USE_NATIVE,    // OoT already has ITEM_MAGIC_MUSHROOM
    /* MM_BOTTLE_HYLIAN_LOACH     */ MM_BOTTLE_USE_CANT_USE,
    /* MM_BOTTLE_OBABA_DRINK      */ MM_BOTTLE_USE_CANT_USE,
};

extern "C" MmBottleUseBehavior MmBottle_GetUseBehavior(MmBottleContent content) {
    if (content < 0 || content >= MM_BOTTLE_COUNT) {
        return MM_BOTTLE_USE_CANT_USE;
    }
    return sMmBottleUse[content];
}

extern "C" MmBottleContent MmBottle_FromItemId(uint16_t ootItemId) {
    // OoT ITEM_ ids of the ported MM bottle-content custom items (z64item.h 0xDF-0xE1). More
    // contents (Deku Princess, Seahorse, Zora Egg, Spring Water...) get a case here as they land.
    switch (ootItemId) {
        case 0xEC:
            return MM_BOTTLE_GOLD_DUST; // ITEM_GOLD_DUST
        case 0xED:
            return MM_BOTTLE_HOT_SPRING_WATER; // ITEM_HOT_SPRING_WATER
        case 0xEE:
            return MM_BOTTLE_DEKU_PRINCESS; // ITEM_DEKU_PRINCESS
        case 0xEF:
            return MM_BOTTLE_SEAHORSE; // ITEM_SEAHORSE
        case 0xF0:
            return MM_BOTTLE_SPRING_WATER; // ITEM_SPRING_WATER
        case 0xF1:
            return MM_BOTTLE_ZORA_EGG; // ITEM_ZORA_EGG
        case 0xF2:
            return MM_BOTTLE_HYLIAN_LOACH; // ITEM_HYLIAN_LOACH
        case 0xF3:
            return MM_BOTTLE_OBABA_DRINK; // ITEM_OBABA_DRINK
        case 0xB6:
            return MM_BOTTLE_CHATEAU_ROMANI; // ITEM_CHATEAU_ROMANI (pre-existing)
        case 0xDD:
            return MM_BOTTLE_MAGIC_MUSHROOM; // ITEM_MAGIC_MUSHROOM (pre-existing)
        default:
            return MM_BOTTLE_NONE;
    }
}

extern "C" int MmBottle_RunCustom(MmBottleContent content, void* play, void* player) {
    // FUTURE: per-content bespoke behaviors. Add a case, set the row above to MM_BOTTLE_USE_CUSTOM,
    // and return 1 when handled. (Cast play/player to PlayState*/Player* inside the case.)
    (void)play;
    (void)player;
    switch (content) {
        // case MM_BOTTLE_CHATEAU_ROMANI: /* ...refill magic... */ return 1;
        default:
            return 0;
    }
}
