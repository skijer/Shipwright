// Skijer's NEI — Bottle Randomizer runtime for the two extra items:
//   Net             -> SLOT_BOTTLE_3 (behavior deferred; just projected so it shows/equips)
//   Bottomless Bottle -> SLOT_BOTTLE_4 (a normal bottle slot with a per-content use-counter "ammo")
//
// Design (see custom_bottles.h): the Bottomless Bottle slot ALWAYS holds a real bottle content, so
// catching, drinking and selling (En_Hy) all run through 100% vanilla code. The only added layer is a
// use-counter: each empty (Inventory_UpdateBottleItem with item == ITEM_BOTTLE on SLOT_BOTTLE_4)
// decrements it; while >0 the content is kept (auto-refill), at 0 it becomes an empty Bottomless
// Bottle. Filling (catch) resets the counter to the content's max uses. The empty Bottomless Bottle
// item uses PLAYER_IA_BOTTLE (see extended_player.c) so the empty-bottle catch action triggers.
//
// Two hooks:
//   VB_UPDATE_BOTTLE_ITEM  — intercept fill/empty of SLOT_BOTTLE_4 to drive the counter.
//   OnInterfaceUpdate      — project ownership + counter state into inventory.items / buttonItems each
//                            frame (so the slot icon + C-button reflect content / empty-bottomless,
//                            and re-assert the content the frame after a >0 drain reset buttonItems).

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "mods/items/custom_bottles.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "variables.h"
extern SaveContext gSaveContext;
extern PlayState* gPlayState;
void Interface_LoadItemIcon1(PlayState* play, u16 button);
}

// Sync a bottle slot to `want` and any C-button equipping it (reloading the C-button icon on change).
static void BottleItems_SyncSlot(PlayState* play, u8 slot, u8 want) {
    if (gSaveContext.inventory.items[slot] != want) {
        gSaveContext.inventory.items[slot] = want;
    }
    for (s16 j = 1; j < 4; j++) { // C-left/down/right -> cButtonSlots[0..2]
        if (gSaveContext.equips.cButtonSlots[j - 1] == slot && gSaveContext.equips.buttonItems[j] != want) {
            gSaveContext.equips.buttonItems[j] = want;
            if (play != NULL) {
                Interface_LoadItemIcon1(play, j);
            }
        }
    }
}

// A VANILLA bottle item (empty bottle or any vanilla content, 0x14 ITEM_BOTTLE .. 0x20 ITEM_POE).
// These are "residue" when sitting raw in the four bottle slots — the slots belong to the Bottle
// Randomizer layout [Wheel A][Wheel B][Net][Bottomless] now.
static u8 BottleItems_IsVanillaBottle(u8 item) {
    return (item >= ITEM_BOTTLE) && (item <= ITEM_POE);
}

// Move a vanilla bottle item into the wheel inventory (first free bottleSlots entry, preferring the
// wheel whose visible slot it appeared in so it stays where the player saw it). 1 = migrated.
static u8 BottleItems_MigrateToWheel(u8 item, u8 preferWheel) {
    int base = (preferWheel == BOTTLE_WHEEL_B) ? 4 : 0;
    for (int k = 0; k < 8; k++) {
        int i = (base + k) % 8;
        if (Bottle_GetSlot((uint8_t)i) == BOTTLE_SLOT_EMPTY) {
            Bottle_SetSlot((uint8_t)i, item);
            return 1;
        }
    }
    return 0; // wheels full — leave the bottle alone (never destroy items)
}

static void BottleItems_Enforce(PlayState* play) {
    // Net catch filled the ACTIVE slot of a wheel — reflect it on the vanilla slot + C-button NOW
    // (out of kaleido the wheels only sync on pause; a visible catch shouldn't wait for that).
    {
        uint8_t w, item;
        if (Bottle_ConsumeCatchSync(&w, &item)) {
            BottleItems_SyncSlot(play, (w == BOTTLE_WHEEL_A) ? SLOT_BOTTLE_1 : SLOT_BOTTLE_2, item);
        }
    }

    // ── Vanilla-bottle residue killer (Skijer's NEI) ─────────────────────────────────────────────
    // Any vanilla bottle the game still hands out (rando gives, chests, shops, old saves) lands raw
    // in SLOT_BOTTLE_1..4. Detect it, migrate its content into a wheel bottle, and convert the row
    // to the new layout: the first residue also grants Net + Bottomless so the slots become
    // [Wheel A][Wheel B][Net][Bottomless] and no vanilla remnants can fight them again.
    {
        u8 cur3 = gSaveContext.inventory.items[SLOT_BOTTLE_3];
        if (cur3 != ITEM_NET && BottleItems_IsVanillaBottle(cur3) && BottleItems_MigrateToWheel(cur3, BOTTLE_WHEEL_A)) {
            gSaveContext.inventory.items[SLOT_BOTTLE_3] = ITEM_NONE; // Net enforcement refills below
            Bottle_SetNetOwned(1);
            Bottle_SetBottomlessOwned(1);
        }
        // SLOT_BOTTLE_4: only when Bottomless ISN'T owned — once owned, the adopt logic below absorbs
        // any external content into the bottomless counter instead.
        u8 cur4 = gSaveContext.inventory.items[SLOT_BOTTLE_4];
        if (!Bottle_BottomlessOwned() && BottleItems_IsVanillaBottle(cur4) &&
            BottleItems_MigrateToWheel(cur4, BOTTLE_WHEEL_B)) {
            gSaveContext.inventory.items[SLOT_BOTTLE_4] = ITEM_NONE;
            Bottle_SetNetOwned(1);
            Bottle_SetBottomlessOwned(1);
        }
    }

    // ── Wheels A/B: per-frame reconcile (SLOT_BOTTLE_1/2) ────────────────────────────────────────
    // The kaleido used to be the ONLY sync point, so drinks/catches/gives only reconciled on pause.
    // Run the same Persist/resync/RecordActive cycle every gameplay frame: drinking persists into the
    // wheel immediately, a residue vanilla bottle migrates into the wheel, and the visible slot + C
    // icon always show a bottle the wheel actually owns. Skipped while paused — the kaleido wheel
    // code runs its own Persist/cycle/RecordActive there and must not be fought.
    if (play == NULL || play->pauseCtx.state == 0) {
        for (int w = 0; w < 2; w++) {
            u8 slot = (w == BOTTLE_WHEEL_A) ? SLOT_BOTTLE_1 : SLOT_BOTTLE_2;
            u16 cur = gSaveContext.inventory.items[slot];
            // Drink/refill of the ACTIVE bottle -> persist into the wheel state.
            Bottle_WheelPersist((uint8_t)w, cur);
            // A vanilla bottle the wheel doesn't know = residue (give/chest/old save): migrate it.
            u8 unmanaged = BottleItems_IsVanillaBottle((u8)cur) && !Bottle_WheelContains((uint8_t)w, cur);
            if (unmanaged && BottleItems_MigrateToWheel((u8)cur, (u8)w)) {
                Bottle_SetNetOwned(1);
                Bottle_SetBottomlessOwned(1);
                unmanaged = 0; // now wheel-owned (this wheel, or the other if this one was full)
            }
            // Show a bottle the wheel owns (covers post-migration and wheel-emptied cases). NEVER
            // overwrite an unmigrated residue (wheels full) — that would destroy the item.
            u16 first = Bottle_WheelFirstItem((uint8_t)w);
            if (!unmanaged && first != BOTTLE_SLOT_EMPTY && !Bottle_WheelContains((uint8_t)w, cur)) {
                BottleItems_SyncSlot(play, slot, (u8)first);
                cur = first;
            }
            Bottle_WheelRecordActive((uint8_t)w, cur);
        }
    }

    // Net (SLOT_BOTTLE_3) — owned -> show ITEM_NET (C-button refreshed too), else clear if it was ours.
    if (Bottle_NetOwned()) {
        BottleItems_SyncSlot(play, SLOT_BOTTLE_3, ITEM_NET);
    } else if (gSaveContext.inventory.items[SLOT_BOTTLE_3] == ITEM_NET) {
        gSaveContext.inventory.items[SLOT_BOTTLE_3] = ITEM_NONE;
    }

    // Bottomless Bottle (SLOT_BOTTLE_4) — owned -> shows its content (identified only by the counter
    // number on multi-use contents); when empty it looks like a plain empty bottle (ITEM_BOTTLE), so
    // the vanilla catch works natively and there's no special empty art.
    if (Bottle_BottomlessOwned()) {
        // Adopt an external fill: if the slot got a real content not via our VB hook (shop potion, cow
        // milk, ...), record it + reset the counter. (A real content differs from ITEM_BOTTLE / empties.)
        u8 cur = gSaveContext.inventory.items[SLOT_BOTTLE_4];
        if (cur != ITEM_BOTTLE && cur != ITEM_BOTTOMLESS_BOTTLE && cur != ITEM_NONE &&
            cur != Bottle_BottomlessContent()) {
            Bottle_BottomlessFill(cur);
        }
        u8 want = Bottle_BottomlessIsEmpty() ? (u8)ITEM_BOTTLE : Bottle_BottomlessContent();
        BottleItems_SyncSlot(play, SLOT_BOTTLE_4, want);
    }
}

static void BottleItems_Register() {
    // Counter driver: fires for every bottle update; we only act on SLOT_BOTTLE_4.
    REGISTER_VB_SHOULD(VB_UPDATE_BOTTLE_ITEM, {
        u8 button = (u8)va_arg(args, int32_t);
        u8 item = (u8)va_arg(args, int32_t);
        if (button == 0) {
            return; // B-button (RBA) handled elsewhere
        }
        u8 slot = gSaveContext.equips.cButtonSlots[button - 1];
        if (slot != SLOT_BOTTLE_4 || !Bottle_BottomlessOwned()) {
            return; // not the Bottomless Bottle
        }
        if (item == ITEM_BOTTLE) {
            // Emptying: spend one use. If charges remain, KEEP the content in the slot (auto-refill);
            // the enforcer fixes buttonItems next frame. At 0, let it empty -> enforcer shows the
            // empty Bottomless Bottle.
            uint8_t remaining = Bottle_BottomlessConsume();
            if (remaining > 0) {
                *should = false; // don't overwrite inventory.items[SLOT_BOTTLE_4] with ITEM_BOTTLE
            }
        } else {
            // Filling (catch): record content + reset the counter to its max uses.
            Bottle_BottomlessFill(item);
        }
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnInterfaceUpdate>(
        []() { BottleItems_Enforce(gPlayState); });
}

static RegisterShipInitFunc gBottleItemsInit(BottleItems_Register);
