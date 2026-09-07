/**
 * twilight_upgrade.c - Twilight Upgrade query/grant helpers.
 *
 * Logic lives in the per-item mod files (clawshot mode in z_player/arms_hook
 * hooks, gale boomerang in en_boom). This translation unit just owns the bit
 * accessors so other code doesn't need to know about the gSaveContext.ship
 * field layout.
 */
#include "twilight_upgrade.h"
#include "macros.h"
#include "functions.h"
#include "../../nei_save.h"           // Skijer's NEI
#include "../../extended_inventory.h" // ExtInv_GetItemSlot — custom items must NOT use vanilla SLOT()/INV_CONTENT()

u8 TwilightUpgrade_HasClawshot(void) {
    return (Nei_Save()->twilightUpgrade & TWILIGHT_UPGRADE_CLAWSHOT) != 0;
}

u8 TwilightUpgrade_HasBombArrows(void) {
    return (Nei_Save()->twilightUpgrade & TWILIGHT_UPGRADE_BOMB_ARROWS) != 0;
}

u8 TwilightUpgrade_HasGaleBoomerang(void) {
    return (Nei_Save()->twilightUpgrade & TWILIGHT_UPGRADE_GALE_BOOMERANG) != 0;
}

u8 TwilightUpgrade_IsObtained(void) {
    return Nei_Save()->twilightUpgrade != 0;
}

u8 TwilightUpgrade_IsFullyObtained(void) {
    return (Nei_Save()->twilightUpgrade & TWILIGHT_UPGRADE_ALL) == TWILIGHT_UPGRADE_ALL;
}

void TwilightUpgrade_Grant(void) {
    Nei_Save()->twilightUpgrade |= TWILIGHT_UPGRADE_ALL;
}

void TwilightUpgrade_SetClawshot(u8 on) {
    if (on) {
        Nei_Save()->twilightUpgrade |= TWILIGHT_UPGRADE_CLAWSHOT;
    } else {
        Nei_Save()->twilightUpgrade &= ~TWILIGHT_UPGRADE_CLAWSHOT;
    }
}

void TwilightUpgrade_SetBombArrows(u8 on) {
    if (on) {
        Nei_Save()->twilightUpgrade |= TWILIGHT_UPGRADE_BOMB_ARROWS;
    } else {
        Nei_Save()->twilightUpgrade &= ~TWILIGHT_UPGRADE_BOMB_ARROWS;
    }
}

void TwilightUpgrade_SetGaleBoomerang(u8 on) {
    if (on) {
        Nei_Save()->twilightUpgrade |= TWILIGHT_UPGRADE_GALE_BOOMERANG;
    } else {
        Nei_Save()->twilightUpgrade &= ~TWILIGHT_UPGRADE_GALE_BOOMERANG;
    }
}

u8 TwilightUpgrade_ClawshotAvailable(void) {
    if (!TwilightUpgrade_HasClawshot()) {
        return 0;
    }
    // Requires hookshot or longshot as the underlying weapon (clawshot is a mode of those).
    return (INV_CONTENT(ITEM_HOOKSHOT) == ITEM_HOOKSHOT) || (INV_CONTENT(ITEM_LONGSHOT) == ITEM_LONGSHOT);
}

u8 TwilightUpgrade_BombArrowsAvailable(void) {
    // Ownership moved off page-2 slot 27 (now the Elemental Wand's) onto NeiSaveData.bombArrowsOwned.
    // Sw97_BombArrowsOwned folds together the save flag, this upgrade bit and the "Bomb Bag" rando
    // mode, so there is one answer everywhere. Skijer's NEI
    return Sw97_BombArrowsOwned();
}

u8 TwilightUpgrade_GaleBoomerangAvailable(void) {
    if (!TwilightUpgrade_HasGaleBoomerang()) {
        return 0;
    }
    return INV_CONTENT(ITEM_BOOMERANG) == ITEM_BOOMERANG;
}

// Mode toggle accessors. Returning 0 when the upgrade isn't unlocked guards
// gameplay hooks so they don't accidentally apply modes the player hasn't
// earned (e.g. if the save bit got corrupted or set via debug without the
// upgrade flag).
u8 TwilightUpgrade_IsClawshotActive(void) {
    if (!TwilightUpgrade_HasClawshot()) {
        return 0;
    }
    return Nei_Save()->clawshotModeActive != 0;
}

u8 TwilightUpgrade_IsGaleBoomerangActive(void) {
    if (!TwilightUpgrade_HasGaleBoomerang()) {
        return 0;
    }
    return Nei_Save()->galeBoomerangModeActive != 0;
}

void TwilightUpgrade_SetClawshotActive(u8 active) {
    Nei_Save()->clawshotModeActive = active ? 1 : 0;
}

void TwilightUpgrade_SetGaleBoomerangActive(u8 active) {
    Nei_Save()->galeBoomerangModeActive = active ? 1 : 0;
}

// Clawshot-mode R-hand DL: compound the resolved OOT closed-hand DL (ootHand) with MM's hookshot
// body, rebuilt only when the pointers change. No-op (leaves *dList) unless clawshot active. Skijer's NEI
void TwilightUpgrade_ApplyClawshotHandDL(Gfx** dList, void* ootHand) {
    if (!TwilightUpgrade_IsClawshotActive()) {
        return;
    }
    extern void* MmAssets_LoadHookshotBodyDL(void);
    void* mmBody = MmAssets_LoadHookshotBodyDL();
    if (mmBody == NULL) {
        return;
    }
    static Gfx sClawshotHandBodyDL[3];
    static void* sLastOotHand = NULL;
    static void* sLastMmBody = NULL;
    if (sLastOotHand != ootHand || sLastMmBody != mmBody) {
        Gfx* dl = sClawshotHandBodyDL;
        gSPDisplayList(dl++, ootHand);
        gSPDisplayList(dl++, mmBody);
        gSPEndDisplayList(dl);
        sLastOotHand = ootHand;
        sLastMmBody = mmBody;
    }
    *dList = sClawshotHandBodyDL;
}
