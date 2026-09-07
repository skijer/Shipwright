/**
 * equip_byrna.c - Cane of Byrna (Extended Sword Slot 1)
 *
 * Behavior: Biggoron Sword IA (long range, two-handed) + HP & MP recovery on hit.
 * - Forces PLAYER_IA_SWORD_BIGGORON for long reach
 * - Forces swordHealth > 0 so charge/spin attacks work
 * - Draws Somaria cane mesh with BLUE materials at 1.15x scale
 * - Follows left hand rotation (sword hand)
 * - On melee hit: recover HP + MP
 *
 * Included by ext_equip_behavior.c (unity build).
 */

// Byrna 3D model (blue cane) now lives in soh.o2r as
// objects/object_somaria/g_byrna_cane_dl and is loaded at draw time in
// extended_equipment.c (Byrna_GetCaneDL). No inline C model here anymore.

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define BYRNA_HP_RECOVER 16         // HP recovered per hit
#define BYRNA_MP_RECOVER 4          // MP recovered per hit
#define BYRNA_SCALE (0.05f * 1.15f) // Somaria base scale * 1.15

// ---------------------------------------------------------------------------
// Melee Hit Callback
// ---------------------------------------------------------------------------
static void GreatFairySword_RecoverOnHit(Player* player, PlayState* play) {
    s32 damage = 0;

    if (player->meleeWeaponQuads[0].base.atFlags & AT_HIT) {
        damage = player->meleeWeaponQuads[0].info.toucher.damage;
    } else if (player->meleeWeaponQuads[1].base.atFlags & AT_HIT) {
        damage = player->meleeWeaponQuads[1].info.toucher.damage;
    }

    if (damage <= 0)
        return;

    // Recover 16 HP per hit
    Health_ChangeBy(play, BYRNA_HP_RECOVER);

    // Recover 16 MP per hit
    gSaveContext.magic += BYRNA_MP_RECOVER;
    if (gSaveContext.magic > gSaveContext.magicCapacity) {
        gSaveContext.magic = gSaveContext.magicCapacity;
    }
}

// ---------------------------------------------------------------------------
// Cane of Byrna — Insect Glaive (Skijer 2026-08-15).
//
// The slot's HP/MP-on-hit recovery moved to the Great Fairy's Sword below and is
// NOT coming back. What lives here now is the MHR Insect Glaive kit; see
// nei_hd_models/gerudo_mhr_dualblades_lab/MHR_EXT_SWORD_PORT_SPEC.md §12.
//
// STAGED ON PURPOSE. This is the Kinsect only; the glaive MOVESET comes later and
// will take B through the melee-row swaps. R is free here because the cane already
// wields two-handed (ExtEquip_ByrnaIsTwoHanded), so no shield competes for it.
//
//   R      -> throw the Kinsect, or recall it once it has banked a charge
//   Z + R  -> a damageless seed that marks a target for it
// ---------------------------------------------------------------------------
// Not in functions.h; declared locally the same way cane_pacci.c does.
extern int Player_IsZTargeting(Player* this);

static void Byrna_Behavior(Player* player, PlayState* play) {
    Input* in;
    u8 rPress;

    if (player == NULL || play == NULL) {
        return;
    }
    // Never act while the player is not in control of himself.
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        return;
    }

    gExtEquipBehavior.byrnaActive = 1;

    in = &play->state.input[0];
    rPress = CHECK_BTN_ALL(in->press.button, BTN_R) != 0;

    // Z+R marks a target for the Kinsect; R alone throws or recalls it. Checked in
    // that order so the seed never eats the plain-R press.
    if (rPress && Player_IsZTargeting(player)) {
        ByrnaOrb_ThrowSeed(play);
        return;
    }
    // A full Kinsect bar turns R from "throw the orb" into "launch LINK".
    if (rPress && ByrnaOrb_IsCharged() && !ByrnaIg_IsAirborne() && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        ByrnaIg_EnterAir(play, player);
        return;
    }
    if (rPress) {
        ByrnaOrb_PressR(play);
    }
    // ByrnaOrb_OnLand() is deliberately NOT called here. Spending the bar on any
    // touchdown would wipe it every frame Link stands still, and even a rising-edge
    // landing would eat it after a step off a ledge. It belongs to the aerial
    // launch's own landing, which does not exist yet.
}

static void Byrna_Cleanup(void) {
    // Runs every frame while the slot is NOT equipped, so it has to be cheap and
    // idempotent — both of these are.
    ByrnaOrb_Cleanup();
    ByrnaIg_Cleanup();
    gExtEquipBehavior.byrnaActive = 0;
}

// Draw is now handled by PostLimbDraw in z_player_lib.c via ExtEquip_DrawSwordDL
// This ensures the cane follows the exact same rotation as the sword during swings

// ---------------------------------------------------------------------------
// Great Fairy's Sword (NEI progressive BGS level 2) — same combat perks as the
// Cane of Byrna, but it IS the player's real Biggoron Sword (no sword-slot hijack).
// Driven by WeaponUpgrade_HasGreatFairy() from ExtEquip_UpdateBehavior, independent
// of the extended-equipment cheat. We only top up swordHealth/bgsFlag (so charge/spin
// always work and a Giant's Knife never "breaks") and recover HP+MP on each melee hit.
// ---------------------------------------------------------------------------
static void GreatFairySword_Behavior(Player* player, PlayState* play) {
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        return;
    }
    // Only while actually wielding the Biggoron Sword.
    if (player->heldItemAction != PLAYER_IA_SWORD_BIGGORON) {
        return;
    }
    gSaveContext.bgsFlag = 1;
    if (gSaveContext.swordHealth <= 0.0f) {
        gSaveContext.swordHealth = 8.0f;
    }
}

static void GreatFairySword_OnMeleeHit(Player* player, PlayState* play) {
    GreatFairySword_RecoverOnHit(player, play);
}
