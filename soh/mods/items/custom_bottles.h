// Skijer's NEI — Bottle Randomizer: content model + kaleido wheel helpers (separated module).
//
// The bottle inventory is NeiSaveData.bottleSlots[8] (Wheel A = slots 0-3, Wheel B = slots 4-7). Each
// slot holds an OoT ITEM_ content id, ITEM_BOTTLE (0x14, an empty bottle), or 0xFF (empty slot). The
// dev save-editor shows all 8 as a 4x2 grid; the kaleido Wheel A/B each cycle their 4 slots' non-empty
// bottles (via z_kaleido_item.c + KaleidoScope_(Handle|Draw)ItemCycleExtras). MM contents are
// standalone custom items; vanilla contents reuse their vanilla ids.
#ifndef CUSTOM_BOTTLES_H
#define CUSTOM_BOTTLES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Every bottle content (vanilla + MM). Used by the editor's content picker.
typedef enum {
    BOTTLE_C_RUTO_LETTER = 0,
    BOTTLE_C_BIG_POE,
    BOTTLE_C_BLUE_FIRE,
    BOTTLE_C_BLUE_POTION,
    BOTTLE_C_RED_POTION,
    BOTTLE_C_GREEN_POTION,
    BOTTLE_C_FAIRY,
    BOTTLE_C_FISH,
    BOTTLE_C_BUG,
    BOTTLE_C_POE,
    BOTTLE_C_MILK,
    BOTTLE_C_GOLD_DUST,
    BOTTLE_C_HOT_SPRING_WATER,
    BOTTLE_C_DEKU_PRINCESS,
    BOTTLE_C_SEAHORSE,
    BOTTLE_C_SPRING_WATER,
    BOTTLE_C_ZORA_EGG,
    BOTTLE_C_HYLIAN_LOACH,
    BOTTLE_C_OBABA_DRINK,
    BOTTLE_C_CHATEAU,
    BOTTLE_C_MAGIC_MUSHROOM,
    BOTTLE_C_COUNT
} BottleContent;

#define BOTTLE_WHEEL_A 0
#define BOTTLE_WHEEL_B 1
#define BOTTLE_SLOT_EMPTY 0xFF // empty bottle slot (no bottle at all)

// Content <-> OoT inventory ITEM_ id. Bottle_ContentFromItemId returns BOTTLE_C_COUNT if not a content.
uint16_t Bottle_ContentItemId(BottleContent c);
BottleContent Bottle_ContentFromItemId(uint16_t itemId);

// Bottle inventory accessors (NeiSaveData.bottleSlots[8]; slotIndex 0..7). item id, ITEM_BOTTLE
// (empty bottle), or BOTTLE_SLOT_EMPTY.
uint8_t Bottle_GetSlot(uint8_t slotIndex);
void Bottle_SetSlot(uint8_t slotIndex, uint8_t item);

// Kaleido wheel over a wheel's 4 bottle slots (cycling the non-empty ones). curItem = the kaleido
// slot's current item. Returns curItem when nothing else to cycle.
uint16_t Bottle_WheelPrevItem(uint8_t wheel, uint16_t curItem);
uint16_t Bottle_WheelNextItem(uint8_t wheel, uint16_t curItem);
uint8_t Bottle_WheelCanCycle(uint8_t wheel);                // >1 non-empty bottle in the wheel
uint16_t Bottle_WheelFirstItem(uint8_t wheel);              // first non-empty bottle item, or 0xFF
uint8_t Bottle_WheelContains(uint8_t wheel, uint16_t item); // is item one of the wheel's bottles?

// Index-based cycling: steps by SLOT so identical empty bottles (all ITEM_BOTTLE) are distinct,
// reachable positions and the wheel stays cyclable even when every bottle is empty. Skijer's NEI
uint8_t Bottle_WheelBottleCount(uint8_t wheel);       // # bottles (incl. empty bottles) in the wheel
uint16_t Bottle_WheelPeek(uint8_t wheel, int8_t dir); // item at next(+1)/prev(-1) bottle slot (no change)
uint16_t Bottle_WheelStep(uint8_t wheel, int8_t dir); // move active to next/prev slot; returns its item

// Drink/refill persistence (call once per frame around the cycler):
//   Bottle_WheelPersist(wheel, slotItem)      — at the START: if the slot changed in-game (drinking
//       emptied it, catching refilled it) since the wheel last set it, write that into the ACTIVE slot.
//   Bottle_WheelRecordActive(wheel, slotItem) — at the END (after cycling): record the active slot +
//       the value the wheel is showing, so the next frame can detect external changes.
void Bottle_WheelPersist(uint8_t wheel, uint16_t slotItem);
void Bottle_WheelRecordActive(uint8_t wheel, uint16_t slotItem);
// Reset the per-wheel active/last-set trackers. MUST be called on save init/load: they are session
// statics, and with the per-frame reconcile a stale last-set from the previous file would let
// Bottle_WheelPersist ghost-write that file's slot value into the freshly loaded wheel.
void Bottle_WheelResetTracking(void);

// ── Net (SLOT_BOTTLE_3) + Bottomless Bottle (SLOT_BOTTLE_4) — Skijer's NEI ───────────────────────
// The Bottomless Bottle is a single bottle slot with a per-content use-counter ("ammo"). Catch/drink/
// sell run through vanilla (the slot always holds a real bottle content; the empty Bottomless Bottle
// item uses PLAYER_IA_BOTTLE so catching works). Each empty decrements the counter; while >0 the
// content auto-refills, at 0 it becomes an empty Bottomless Bottle. Net is a separate item (deferred).
#define BOTTLE_CONTENT_EMPTY 0xFF

// Per-content counter: potions 3, bug/fish 5, milk 3, Hylian Loach 1, "major-like" + everything else 1.
uint8_t Bottle_ContentMaxUses(uint16_t itemId);

// Ownership (persisted in NeiSaveData; reuses netEquipped / bottomlessBottleMode).
uint8_t Bottle_NetOwned(void);
void Bottle_SetNetOwned(uint8_t owned);
uint8_t Bottle_BottomlessOwned(void);
void Bottle_SetBottomlessOwned(uint8_t owned);

// Bottomless content + counter state.
uint8_t Bottle_BottomlessContent(void);           // content id, or ITEM_BOTTLE/0xFF when empty
uint8_t Bottle_BottomlessCount(void);             // remaining uses
uint8_t Bottle_BottomlessIsEmpty(void);           // 1 = no usable content (show empty bottle)
void Bottle_BottomlessFill(uint16_t contentItem); // set content + reset counter to its max uses
void Bottle_BottomlessEmpty(void);                // force empty
uint8_t Bottle_BottomlessConsume(void);           // decrement counter; returns remaining (0 = empty)
void Bottle_BottomlessSetCount(uint8_t count);    // set counter directly (save editor)

// Net catch: put `content` into the first available empty bottle. Priority: empty Bottomless Bottle
// ("si existe, la reemplaza") -> the ACTIVE slot of each wheel (the visible bottle; queues a visible
// sync) -> any empty wheel bottle. Returns 1 if placed, 0 if no empty bottle was available.
uint8_t Bottle_CatchIntoEmpty(uint16_t content);

// Give a NEW bottle (content id, or the empty-bottle id for empty) into the first free wheel slot.
// Returns 1 if placed, 0 if all 8 slots are full. Rando bottle GIVES use this (creates a bottle),
// NOT a plain content give (which only fills an existing empty bottle and is lost when none exists).
uint8_t Bottle_GiveBottle(uint16_t contentItem);

// 1 = Bottle_GiveBottle would succeed. Obtainability checks ask this before offering a bottle.
uint8_t Bottle_HasFreeSlot(void);

// Pending visible-slot sync from a catch that filled a wheel's ACTIVE slot. Consumed once per catch by
// the per-frame enforcer (mm_bottle_items.cpp), which writes SLOT_BOTTLE_1/2 + refreshes the C-button.
uint8_t Bottle_ConsumeCatchSync(uint8_t* outWheel, uint8_t* outItem);

#ifdef __cplusplus
}
#endif

#endif // CUSTOM_BOTTLES_H
