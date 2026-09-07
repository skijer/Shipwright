// Skijer's NEI — Bottle Randomizer content model + kaleido wheel helpers (see header).
//
// Pure logic (only nei_save.h + stdint) so it compiles standalone as a mods/*.cpp. The bottle
// inventory lives in NeiSaveData.bottleSlots[8]; Wheel A = slots 0-3, Wheel B = slots 4-7.
#include "custom_bottles.h"
#include "mods/nei_save.h" // Nei_Save() -> NeiSaveData.bottleSlots[8]

// OoT inventory ITEM_ id per content. Vanilla contents reuse vanilla ids (soh/include/z64item.h);
// MM contents are the standalone custom items added there (Chateau 0xB6 + Magic Mushroom 0xDD were
// pre-existing). Keep these in sync with z64item.h if the MM enum values move.
#define BC_ITEM_NONE 0xFF
#define BC_ITEM_BOTTLE 0x14 // empty bottle
// Vanilla contents
#define BC_ITEM_LETTER_RUTO 0x1B
#define BC_ITEM_BIG_POE 0x1E
#define BC_ITEM_BLUE_FIRE 0x1C
#define BC_ITEM_POTION_BLUE 0x17
#define BC_ITEM_POTION_RED 0x15
#define BC_ITEM_POTION_GREEN 0x16
#define BC_ITEM_FAIRY 0x18
#define BC_ITEM_FISH 0x19
#define BC_ITEM_BUG 0x1D
#define BC_ITEM_POE 0x20
#define BC_ITEM_MILK 0x1A
// MM contents (custom items)
#define BC_ITEM_GOLD_DUST 0xEC
#define BC_ITEM_HOT_SPRING_WATER 0xED
#define BC_ITEM_DEKU_PRINCESS 0xEE
#define BC_ITEM_SEAHORSE 0xEF
#define BC_ITEM_SPRING_WATER 0xF0
#define BC_ITEM_ZORA_EGG 0xF1
#define BC_ITEM_HYLIAN_LOACH 0xF2
#define BC_ITEM_OBABA_DRINK 0xF3
#define BC_ITEM_CHATEAU 0xB6        // pre-existing ITEM_CHATEAU_ROMANI
#define BC_ITEM_MAGIC_MUSHROOM 0xDD // pre-existing ITEM_MAGIC_MUSHROOM

// Order MUST match the BottleContent enum.
static const uint16_t sContentItem[BOTTLE_C_COUNT] = {
    /* RUTO_LETTER     */ BC_ITEM_LETTER_RUTO,
    /* BIG_POE         */ BC_ITEM_BIG_POE,
    /* BLUE_FIRE       */ BC_ITEM_BLUE_FIRE,
    /* BLUE_POTION     */ BC_ITEM_POTION_BLUE,
    /* RED_POTION      */ BC_ITEM_POTION_RED,
    /* GREEN_POTION    */ BC_ITEM_POTION_GREEN,
    /* FAIRY           */ BC_ITEM_FAIRY,
    /* FISH            */ BC_ITEM_FISH,
    /* BUG             */ BC_ITEM_BUG,
    /* POE             */ BC_ITEM_POE,
    /* MILK            */ BC_ITEM_MILK,
    /* GOLD_DUST       */ BC_ITEM_GOLD_DUST,
    /* HOT_SPRING_WATER*/ BC_ITEM_HOT_SPRING_WATER,
    /* DEKU_PRINCESS   */ BC_ITEM_DEKU_PRINCESS,
    /* SEAHORSE        */ BC_ITEM_SEAHORSE,
    /* SPRING_WATER    */ BC_ITEM_SPRING_WATER,
    /* ZORA_EGG        */ BC_ITEM_ZORA_EGG,
    /* HYLIAN_LOACH    */ BC_ITEM_HYLIAN_LOACH,
    /* OBABA_DRINK     */ BC_ITEM_OBABA_DRINK,
    /* CHATEAU         */ BC_ITEM_CHATEAU,
    /* MAGIC_MUSHROOM  */ BC_ITEM_MAGIC_MUSHROOM,
};

extern "C" uint16_t Bottle_ContentItemId(BottleContent c) {
    if (c < 0 || c >= BOTTLE_C_COUNT)
        return BC_ITEM_NONE;
    return sContentItem[c];
}

extern "C" BottleContent Bottle_ContentFromItemId(uint16_t itemId) {
    if (itemId == BC_ITEM_NONE)
        return BOTTLE_C_COUNT;
    for (int i = 0; i < BOTTLE_C_COUNT; i++) {
        if (sContentItem[i] == itemId)
            return (BottleContent)i;
    }
    return BOTTLE_C_COUNT;
}

extern "C" uint8_t Bottle_GetSlot(uint8_t slotIndex) {
    if (slotIndex >= 8)
        return BOTTLE_SLOT_EMPTY;
    return Nei_Save()->bottleSlots[slotIndex];
}

extern "C" void Bottle_SetSlot(uint8_t slotIndex, uint8_t item) {
    if (slotIndex >= 8)
        return;
    Nei_Save()->bottleSlots[slotIndex] = item;
}

// Build the wheel's non-empty bottle item list (slots wheel*4 .. +3). Returns the count.
static int Bottle_BuildWheelList(uint8_t wheel, uint16_t out[4]) {
    int base = (wheel == BOTTLE_WHEEL_B) ? 4 : 0;
    const uint8_t* slots = Nei_Save()->bottleSlots;
    int n = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t it = slots[base + i];
        if (it != BOTTLE_SLOT_EMPTY)
            out[n++] = it;
    }
    return n;
}

extern "C" uint8_t Bottle_WheelCanCycle(uint8_t wheel) {
    uint16_t list[4];
    return (uint8_t)(Bottle_BuildWheelList(wheel, list) > 1);
}

extern "C" uint16_t Bottle_WheelFirstItem(uint8_t wheel) {
    uint16_t list[4];
    int n = Bottle_BuildWheelList(wheel, list);
    return n > 0 ? list[0] : (uint16_t)BOTTLE_SLOT_EMPTY;
}

extern "C" uint8_t Bottle_WheelContains(uint8_t wheel, uint16_t item) {
    uint16_t list[4];
    int n = Bottle_BuildWheelList(wheel, list);
    for (int i = 0; i < n; i++) {
        if (list[i] == item)
            return 1;
    }
    return 0;
}

// Per-wheel active tracking (not persisted — resets each load, which is fine).
//   sActive  = the slot index (0..3) currently shown in SLOT_BOTTLE_1/2.
//   sLastSet = the item the wheel itself last placed in that slot. Used to tell an EXTERNAL change
//              (drinking empties / catching refills the bottle in-game) apart from the wheel's own
//              cycling, so a drink writes back to the ACTIVE slot (not a wrong slot found by search).
static uint8_t sBottleActive[2] = { 0, 0 };
static uint16_t sBottleLastSet[2] = { BOTTLE_SLOT_EMPTY, BOTTLE_SLOT_EMPTY };

// ── Index-based wheel cycling (Skijer's NEI) ──────────────────────────────────────────────────────
// The value-based cycler (Bottle_WheelPrev/NextItem + KaleidoScope_HandleItemCycleExtras) can't move
// between two identical values, so several EMPTY bottles (all ITEM_BOTTLE) collapse into a single
// wheel position — you could only reach one. These step by SLOT INDEX so every physical bottle (empty
// ones included) is a distinct, reachable position, and the wheel stays cyclable even when all bottles
// are empty.

// Count of BOTTLES in the wheel (slots holding ITEM_BOTTLE or a content; truly-empty slots don't count).
extern "C" uint8_t Bottle_WheelBottleCount(uint8_t wheel) {
    int base = (wheel == BOTTLE_WHEEL_B) ? 4 : 0;
    const uint8_t* slots = Nei_Save()->bottleSlots;
    uint8_t n = 0;
    for (int i = 0; i < 4; i++) {
        if (slots[base + i] != BOTTLE_SLOT_EMPTY)
            n++;
    }
    return n;
}

// Snap the active index onto a bottle slot if it currently points at a truly-empty slot.
static void Bottle_WheelClampActive(uint8_t w) {
    const uint8_t* slots = Nei_Save()->bottleSlots;
    int base = w * 4;
    if (slots[base + sBottleActive[w]] != BOTTLE_SLOT_EMPTY) {
        return;
    }
    for (int i = 0; i < 4; i++) {
        if (slots[base + i] != BOTTLE_SLOT_EMPTY) {
            sBottleActive[w] = (uint8_t)i;
            return;
        }
    }
}

// Item at the next(dir=+1)/prev(dir=-1) bottle slot from the active one, WITHOUT changing state.
// Returns BC_ITEM_NONE if there's no other bottle to move to.
extern "C" uint16_t Bottle_WheelPeek(uint8_t wheel, int8_t dir) {
    uint8_t w = (wheel == BOTTLE_WHEEL_B) ? 1 : 0;
    int base = w * 4;
    const uint8_t* slots = Nei_Save()->bottleSlots;
    Bottle_WheelClampActive(w);
    int idx = sBottleActive[w];
    for (int step = 0; step < 4; step++) {
        idx = (idx + dir + 4) % 4;
        if (idx == sBottleActive[w]) {
            break;
        }
        if (slots[base + idx] != BOTTLE_SLOT_EMPTY) {
            return slots[base + idx];
        }
    }
    return BC_ITEM_NONE;
}

// Advance the active index to the next/prev bottle slot; returns the item now active. Also refreshes
// sBottleLastSet so the gameplay Persist won't mistake this deliberate move for an external change.
extern "C" uint16_t Bottle_WheelStep(uint8_t wheel, int8_t dir) {
    uint8_t w = (wheel == BOTTLE_WHEEL_B) ? 1 : 0;
    int base = w * 4;
    uint8_t* slots = Nei_Save()->bottleSlots;
    Bottle_WheelClampActive(w);
    int idx = sBottleActive[w];
    for (int step = 0; step < 4; step++) {
        idx = (idx + dir + 4) % 4;
        if (slots[base + idx] != BOTTLE_SLOT_EMPTY) {
            sBottleActive[w] = (uint8_t)idx;
            break;
        }
    }
    uint16_t item = slots[base + sBottleActive[w]];
    sBottleLastSet[w] = item;
    return item;
}

// Call at the START of the frame. If the slot's item differs from what the wheel last set (i.e. it was
// changed in-game by drinking/catching), write that change into the ACTIVE slot so it persists and the
// refill won't undo it.
extern "C" void Bottle_WheelPersist(uint8_t wheel, uint16_t slotItem) {
    uint8_t w = (wheel == BOTTLE_WHEEL_B) ? 1 : 0;
    int base = w * 4;
    uint8_t* slots = Nei_Save()->bottleSlots;
    if (slotItem != sBottleLastSet[w] && sBottleLastSet[w] != BOTTLE_SLOT_EMPTY && slotItem != BOTTLE_SLOT_EMPTY &&
        sBottleActive[w] < 4) {
        slots[base + sBottleActive[w]] = (uint8_t)slotItem;
    }
}

extern "C" void Bottle_WheelResetTracking(void) {
    sBottleActive[0] = sBottleActive[1] = 0;
    sBottleLastSet[0] = sBottleLastSet[1] = BOTTLE_SLOT_EMPTY;
}

// Call at the END of the frame (after the cycler ran). Record which slot is now active (the one whose
// content equals the visible slot) and remember the value the wheel is showing.
extern "C" void Bottle_WheelRecordActive(uint8_t wheel, uint16_t slotItem) {
    uint8_t w = (wheel == BOTTLE_WHEEL_B) ? 1 : 0;
    int base = w * 4;
    const uint8_t* slots = Nei_Save()->bottleSlots;
    if (slotItem != BOTTLE_SLOT_EMPTY) {
        for (int i = 0; i < 4; i++) {
            if (slots[base + i] == slotItem) {
                sBottleActive[w] = (uint8_t)i;
                break;
            }
        }
    }
    sBottleLastSet[w] = slotItem;
}

extern "C" uint16_t Bottle_WheelNextItem(uint8_t wheel, uint16_t curItem) {
    uint16_t list[4];
    int n = Bottle_BuildWheelList(wheel, list);
    if (n == 0)
        return curItem;
    for (int i = 0; i < n; i++) {
        if (list[i] == curItem)
            return list[(i + 1) % n];
    }
    return list[0];
}

// ── Net + Bottomless Bottle (SLOT_BOTTLE_3/4) ───────────────────────────────────────────────────
static int Bottle_ContentIsReal(uint8_t c) {
    return c != BOTTLE_CONTENT_EMPTY && c != BC_ITEM_BOTTLE; // a usable content (not empty / empty bottle)
}

extern "C" uint8_t Bottle_ContentMaxUses(uint16_t itemId) {
    // Per-content use counter (user-chosen). Multi-use (>1) get full/mid/low fill-state icons; the
    // effect of each use stays vanilla-bottle. Single-use (1) behave as a normal bottle.
    switch (itemId) {
        case BC_ITEM_POTION_RED:
        case BC_ITEM_POTION_BLUE:
        case BC_ITEM_POTION_GREEN:
        case BC_ITEM_MILK:
            return 5;
        case BC_ITEM_FISH:
            return 6;
        case BC_ITEM_BUG:
            return 9;
        case BC_ITEM_FAIRY:
            return 3;
        case BC_ITEM_BLUE_FIRE:
            return 7;
        case BC_ITEM_POE:
            return 3;
        case BC_ITEM_BIG_POE:
            return 3;
        case BC_ITEM_HOT_SPRING_WATER:
            return 7;
        case BC_ITEM_CHATEAU:
            return 3;
        case BC_ITEM_MAGIC_MUSHROOM:
            return 3;
        case BC_ITEM_HYLIAN_LOACH:
            return 3;
        // Spring Water, Gold Dust, Obaba's Drink + "major-like" (seahorse, deku princess, zora egg,
        // ruto's letter, ...) = single-use normal bottle.
        default:
            return 1;
    }
}

extern "C" uint8_t Bottle_NetOwned(void) {
    return Nei_Save()->netEquipped;
}
extern "C" void Bottle_SetNetOwned(uint8_t owned) {
    Nei_Save()->netEquipped = owned ? 1 : 0;
}
extern "C" uint8_t Bottle_BottomlessOwned(void) {
    return Nei_Save()->bottomlessBottleMode;
}
extern "C" void Bottle_SetBottomlessOwned(uint8_t owned) {
    NeiSaveData* s = Nei_Save();
    s->bottomlessBottleMode = owned ? 1 : 0;
    if (!owned) {
        // Wipe the content on un-own so nothing lingers: otherwise the leftover content sits in
        // SLOT_BOTTLE_4 as a vanilla-looking bottle and the residue killer migrates it (a "free
        // bottle") AND re-grants Bottomless. The slot itself is cleared by the caller. Skijer's NEI
        s->bottomlessContent = BOTTLE_CONTENT_EMPTY;
        s->bottomlessCount = 0;
    }
}

extern "C" uint8_t Bottle_BottomlessContent(void) {
    return Nei_Save()->bottomlessContent;
}
extern "C" uint8_t Bottle_BottomlessCount(void) {
    return Nei_Save()->bottomlessCount;
}
extern "C" uint8_t Bottle_BottomlessIsEmpty(void) {
    NeiSaveData* s = Nei_Save();
    return (!Bottle_ContentIsReal(s->bottomlessContent) || s->bottomlessCount == 0) ? 1 : 0;
}
extern "C" void Bottle_BottomlessFill(uint16_t contentItem) {
    NeiSaveData* s = Nei_Save();
    if (!Bottle_ContentIsReal((uint8_t)contentItem)) { // filling with "empty" just empties
        Bottle_BottomlessEmpty();
        return;
    }
    s->bottomlessContent = (uint8_t)contentItem;
    s->bottomlessCount = Bottle_ContentMaxUses(contentItem);
}
extern "C" void Bottle_BottomlessEmpty(void) {
    NeiSaveData* s = Nei_Save();
    s->bottomlessContent = BC_ITEM_BOTTLE;
    s->bottomlessCount = 0;
}
extern "C" void Bottle_BottomlessSetCount(uint8_t count) {
    NeiSaveData* s = Nei_Save();
    s->bottomlessCount = count;
    if (count == 0) {
        s->bottomlessContent = BC_ITEM_BOTTLE; // 0 uses -> empty
    }
}
extern "C" uint8_t Bottle_BottomlessConsume(void) {
    NeiSaveData* s = Nei_Save();
    if (s->bottomlessCount > 0) {
        s->bottomlessCount--;
    }
    if (s->bottomlessCount == 0) {
        s->bottomlessContent = BC_ITEM_BOTTLE; // fully drained -> empty Bottomless Bottle
    }
    return s->bottomlessCount;
}

// Pending "visible slot" sync: a catch filled the ACTIVE slot of a wheel (the bottle the player SEES
// in SLOT_BOTTLE_1/2 and possibly on a C-button). Out of kaleido the wheels only sync on pause, so the
// per-frame enforcer (mm_bottle_items.cpp) consumes this and updates the vanilla slot + C icon NOW.
static uint8_t sCatchSyncWheel = 0xFF; // BOTTLE_WHEEL_A/B, 0xFF = nothing pending
static uint8_t sCatchSyncItem = 0xFF;

extern "C" uint8_t Bottle_ConsumeCatchSync(uint8_t* outWheel, uint8_t* outItem) {
    if (sCatchSyncWheel == 0xFF) {
        return 0;
    }
    *outWheel = sCatchSyncWheel;
    *outItem = sCatchSyncItem;
    sCatchSyncWheel = 0xFF;
    return 1;
}

extern "C" uint8_t Bottle_CatchIntoEmpty(uint16_t content) {
    if (!Bottle_ContentIsReal((uint8_t)content)) {
        return 0;
    }
    // 1) Bottomless Bottle FIRST when owned and empty — "si existe, la reemplaza": the per-frame
    //    enforcer projects the new content into SLOT_BOTTLE_4 + its C-button immediately.
    if (Bottle_BottomlessOwned() && Bottle_BottomlessIsEmpty()) {
        Bottle_BottomlessFill(content);
        return 1;
    }
    uint8_t* slots = Nei_Save()->bottleSlots;
    // 2) The ACTIVE slot of each wheel (the bottle currently VISIBLE in SLOT_BOTTLE_1/2 / on C): fill
    //    it and queue the visible sync so the on-screen bottle + C icon update this frame, not on the
    //    next kaleido open.
    for (int w = 0; w < 2; w++) {
        int idx = w * 4 + sBottleActive[w];
        if (slots[idx] == BC_ITEM_BOTTLE) {
            slots[idx] = (uint8_t)content;
            sBottleLastSet[w] = (uint16_t)content; // wheel Persist: this is NOT an external change
            sCatchSyncWheel = (uint8_t)w;
            sCatchSyncItem = (uint8_t)content;
            return 1;
        }
    }
    // 3) Any other empty bottle in the wheels (not visible now; shows next kaleido open).
    for (int i = 0; i < 8; i++) {
        if (slots[i] == BC_ITEM_BOTTLE) {
            slots[i] = (uint8_t)content;
            return 1;
        }
    }
    return 0;
}

extern "C" uint16_t Bottle_WheelPrevItem(uint8_t wheel, uint16_t curItem) {
    uint16_t list[4];
    int n = Bottle_BuildWheelList(wheel, list);
    if (n == 0)
        return curItem;
    for (int i = 0; i < n; i++) {
        if (list[i] == curItem)
            return list[(i + n - 1) % n];
    }
    return list[0];
}

extern "C" uint8_t Bottle_HasFreeSlot(void) {
    const uint8_t* slots = Nei_Save()->bottleSlots;
    for (int i = 0; i < 8; i++) {
        if (slots[i] == BOTTLE_SLOT_EMPTY) {
            return 1;
        }
    }
    return 0;
}

// Give a NEW rando bottle: drop `contentItem` (a bottle content id, or the empty-bottle id for an empty
// bottle) into the first free wheel slot — Wheel A (0-3) first, then Wheel B (4-7). Returns 1 if placed,
// 0 when all 8 slots are occupied. Rando bottle GIVES MUST use this (creates a bottle), NOT a plain
// content-item give that only FILLS an already-empty bottle and is silently lost when none exists.
// Kept in sync with the MM side (mm/mods/items/custom_bottles.cpp).
extern "C" uint8_t Bottle_GiveBottle(uint16_t contentItem) {
    uint8_t* slots = Nei_Save()->bottleSlots;
    for (int i = 0; i < 8; i++) {
        if (slots[i] == BOTTLE_SLOT_EMPTY) {
            slots[i] = (uint8_t)contentItem;
            // Owning a bottle converts the row to the NEI layout, exactly as the residue killer did
            // when bottles still landed in a vanilla slot on their way here.
            Bottle_SetNetOwned(1);
            Bottle_SetBottomlessOwned(1);
            return 1;
        }
    }
    return 0; // all 8 bottle slots full
}
