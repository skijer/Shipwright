/**
 * equip_breastplate.c - Spirit Tunic (Extended Tunic Slot 2)
 *
 * Behavior: Magic Armor (TP-style) — rupee-cost damage immunity + environment protection.
 * - Immune to all damage while wearing AND holding rupees; each HP of damage costs 1 rupee.
 * - Absorbed hits play like a shield block: no knockback, no damage animation, no hurt voice
 *   (gated per damage path in z_player.c via Player_SpiritTunicAbsorbHit).
 * - 30% of each charge (ceil) SPILLS out of the wallet as real rupee pickups tossed around Link —
 *   recoverable if the player dares to grab them mid-fight.
 * - No rupees = slow movement (cursed weight) and NO protection.
 * - Passive rupee drain: 1 rupee per 30 frames.
 * - With rupees ALSO: the FIRE (hot-room) and WATER (underwater breath) survival timers are skipped
 *   — the money-gated environment immunity (gated in z_parameter.c via ExtEquip_SpiritHasMoney).
 *
 * Skijer 2026-07-16 rework: this is now a RECOLOR tunic (no armor overlay). The visual is the tunic
 * env color painted in Player_DrawImpl — ORANGE while active (rupees > 0), BLACK when broke. So the
 * old Iron-Knuckle armor DLs + gold/dark material are GONE.
 *
 * Damage immunity is a direct call from Health_ChangeBy (z_parameter.c) to
 * Breastplate_OnHealthChangeBefore below (invincibilityTimer runs too late in Player_Update).
 * Included by ext_equip_behavior.c (unity build).
 */

// No extra includes — unity-built from ext_equip_behavior.c
extern void Rupees_ChangeBy(s16 rupeeChange);
u8 Breastplate_IsActive(void);

#define BREASTPLATE_RUPEE_INTERVAL 30 // Passive drain: 1 rupee every N frames
#define BREASTPLATE_SLOW_MULT 0.5f    // Speed multiplier when broke

static s16 sBreastplateRupeeTick = 0;

static void Breastplate_Cleanup(void) {
    sBreastplateRupeeTick = 0;
}

// ---------------------------------------------------------------------------
// Main Behavior — passive rupee drain + broke-mode movement penalty (damage interception is
// separate, in Breastplate_OnHealthChangeBefore; the fire/water timer skip is in z_parameter.c).
// ---------------------------------------------------------------------------
static void Spirit_Behavior(Player* player, PlayState* play) {
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        return;
    }

    if (gSaveContext.rupees > 0) {
        sBreastplateRupeeTick++;
        if (sBreastplateRupeeTick >= BREASTPLATE_RUPEE_INTERVAL) {
            sBreastplateRupeeTick = 0;
            Rupees_ChangeBy(-1);
        }
    } else {
        // No rupees: heavy and slow, no protection
        sBreastplateRupeeTick = 0;
        player->linearVelocity *= BREASTPLATE_SLOW_MULT;
        player->actor.speedXZ *= BREASTPLATE_SLOW_MULT;
    }
}

// ---------------------------------------------------------------------------
// Coin spill — ceil(30%) of the charge falls out of the wallet as REAL rupee actors (EnItem00),
// tossed around Link with the standard drop bounce (Item_DropCollectible: random yaw, vy 8).
// Decomposed into red/blue/green denominations so a big hit stays a handful of actors.
//
// Each coin is placed on a random bearing around Link at a short radius, mirroring the harpoon
// death-pile scatter (soh/Network/Harpoon/DroppedItems.cpp SpawnInScene): without the offset the
// coins spawn on the same XZ and z-fight into one blob.
//
// The coins spawn ABOVE Link's head and rain down. This is not cosmetic: EnItem00_Update collects
// on `xzDistToPlayer <= 30 && |yDistToPlayer| <= 50` (z_en_item00.c:846), and the scatter radius is
// well inside 30 units — spawning at Link's feet meant he swallowed the whole spill on the very
// frame it appeared, so the drop was invisible and free. Clearing the 50-unit vertical window at
// spawn keeps them uncollectable until they have fallen, and Item_DropCollectible's outward
// speedXZ carries them past the 30-unit ring on the way down, so they must be walked back to.
// ---------------------------------------------------------------------------
#define BREASTPLATE_SPILL_RADIUS_MIN 8.0f
#define BREASTPLATE_SPILL_RADIUS_MAX 20.0f
// Clearance above the head. Must keep (height + this) > 50 for every form so the spawn starts
// outside the pickup window; 40 leaves margin even for the shortest transformation.
#define BREASTPLATE_SPILL_HEIGHT_MARGIN 40.0f

static void Breastplate_SpillRupees(PlayState* play, s16 rupeeCost) {
    Player* player = GET_PLAYER(play);
    s16 spill = (s16)((rupeeCost * 3 + 9) / 10); // ceil(rupeeCost * 0.3)
    f32 spawnY = player->actor.world.pos.y + Player_GetHeight(player) + BREASTPLATE_SPILL_HEIGHT_MARGIN;

    while (spill > 0) {
        s16 params;
        if (spill >= 20) {
            params = ITEM00_RUPEE_RED;
            spill -= 20;
        } else if (spill >= 5) {
            params = ITEM00_RUPEE_BLUE;
            spill -= 5;
        } else {
            params = ITEM00_RUPEE_GREEN;
            spill -= 1;
        }

        s16 angle = (s16)Rand_CenteredFloat(65536.0f);
        f32 radius = BREASTPLATE_SPILL_RADIUS_MIN +
                     Rand_ZeroOne() * (BREASTPLATE_SPILL_RADIUS_MAX - BREASTPLATE_SPILL_RADIUS_MIN);
        Vec3f pos = { player->actor.world.pos.x + Math_CosS(angle) * radius, spawnY,
                      player->actor.world.pos.z + Math_SinS(angle) * radius };

        Item_DropCollectible(play, &pos, params);
    }
}

// ---------------------------------------------------------------------------
// Pre-damage hook — convert incoming damage to rupee cost while Spirit is active and Link has rupees.
// Called from Health_ChangeBy (z_parameter.c) BEFORE health is mutated; *amount = 0 blocks it.
// ---------------------------------------------------------------------------
void Breastplate_OnHealthChangeBefore(PlayState* play, int16_t* amount) {
    if (!Breastplate_IsActive()) {
        return;
    }
    if (*amount >= 0) {
        return; // healing — pass through
    }
    if (gSaveContext.rupees <= 0) {
        return; // broke — take damage normally
    }

    s16 damageHP = -*amount;
    s16 rupeeCost = damageHP;
    if (rupeeCost > gSaveContext.rupees) {
        rupeeCost = (s16)gSaveContext.rupees;
    }
    Rupees_ChangeBy(-rupeeCost);
    Sfx_PlaySfxCentered(NA_SE_IT_SHIELD_BOUND);
    Breastplate_SpillRupees(play, rupeeCost);

    *amount = 0; // block the damage
}

u8 Breastplate_IsActive(void) {
    return ExtEquip_IsEnabled() && gExtEquipState.currentExtTunic == 2;
}
