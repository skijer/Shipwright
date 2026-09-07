/**
 * weapon_upgrades.h - NEI Weapon Upgrades (progressive weapon levels)
 *
 * The four base weapons become progressive randomizer items. Level 1 is the
 * vanilla weapon (tracked by normal equipment/inventory state); the levels above
 * are these upgrade bits:
 *
 *   progressiveKokiriSword:  L1 Kokiri  → L2 Razor   → L3 Gilded
 *   progressiveMasterSword:  L1 Master  → L2 Real Master Sword
 *   progressiveHammer:       L1 Hammer  → L2 Iron Knuckle's Axe
 *   progressiveBGS:          L1 BGS     → L2 Great Fairy's Sword
 *
 * Persistence: Nei_Save()->weaponUpgrades is a u8 bitfield (one bit per upgrade),
 * serialized by the "nei" SaveManager section in mods/nei_save.cpp. The Kokiri
 * chain uses two bits (Razor then Gilded). The level-1 vanilla weapons persist via
 * the normal SaveContext (sword equip flags / ITEM_HAMMER inventory slot).
 */
#ifndef WEAPON_UPGRADES_H
#define WEAPON_UPGRADES_H

#include "z64.h"

#define WEAPON_UPGRADE_HAMMER_AXE (1 << 0)      // Hammer  → Iron Knuckle's Axe
#define WEAPON_UPGRADE_KOKIRI_RAZOR (1 << 1)    // Kokiri  → Razor Sword
#define WEAPON_UPGRADE_KOKIRI_GILDED (1 << 2)   // Kokiri  → Gilded Sword
#define WEAPON_UPGRADE_MASTER_TRUE (1 << 3)     // Master  → True Master Sword
#define WEAPON_UPGRADE_BGS_GREAT_FAIRY (1 << 4) // Biggoron→ Great Fairy's Sword
#define WEAPON_UPGRADE_ALL                                                                    \
    (WEAPON_UPGRADE_HAMMER_AXE | WEAPON_UPGRADE_KOKIRI_RAZOR | WEAPON_UPGRADE_KOKIRI_GILDED | \
     WEAPON_UPGRADE_MASTER_TRUE | WEAPON_UPGRADE_BGS_GREAT_FAIRY)

#ifdef __cplusplus
extern "C" {
#endif

// Bit-level queries — return 1 if the specific upgrade is owned.
u8 WeaponUpgrade_HasHammerAxe(void);
u8 WeaponUpgrade_HasRazor(void);
u8 WeaponUpgrade_HasGilded(void);
u8 WeaponUpgrade_HasTrueMaster(void);
u8 WeaponUpgrade_HasGreatFairy(void);

// Returns the highest Kokiri Sword upgrade level: 0 = none, 1 = Razor, 2 = Gilded.
u8 WeaponUpgrade_KokiriLevel(void);

// Per-bit setters — set or clear a single upgrade (used by the save-flag UI).
void WeaponUpgrade_SetHammerAxe(u8 on);
void WeaponUpgrade_SetRazor(u8 on);
void WeaponUpgrade_SetGilded(u8 on);
void WeaponUpgrade_SetTrueMaster(u8 on);
void WeaponUpgrade_SetGreatFairy(u8 on);

// Progressive Kokiri Sword give: first call grants Razor, second grants Gilded.
void WeaponUpgrade_GiveProgressiveKokiri(void);

// Grant the full set at once (debug convenience). Idempotent.
void WeaponUpgrade_GrantAll(void);

// Iron Knuckle's Axe prop-smash. A prop actor calls this from its Update; it detects an axe
// strike (player owns the Axe upgrade, is mid-hammer-swing, facing within reach) and tracks a
// per-actor hit counter in an internal side table (keyed by Actor*, no actor-struct changes).
// Returns 1 on the strike that reaches `hitsNeeded` (the caller then breaks + rewards + kills
// the actor). `range` is the max actor-to-player distance that still counts as a hit.
u8 WeaponUpgrade_IKAxeStrike(struct Actor* actor, struct PlayState* play, u8 hitsNeeded, f32 range);

// In-hand held-sword model swap. Builds a compound DL [OOT hand] + [MM sword pieces from o2r] and
// writes it to *dList when the matching upgrade is owned and that sword is wielded. Returns 1 if it
// set *dList, 0 to keep the vanilla DL. `ootHand` is the resolved OOT hand DL for this limb/LOD.
// Called from the L_HAND limb draw in z_player_lib.c. `bodyEnvR/G/B` is the tunic env color the
// player body expects (set once before the skeleton draw); the compound re-applies it after the MM
// sword DL so a combined DL that sets its own env color (the GFS) doesn't blacken the torso.
u8 WeaponUpgrade_ApplyHeldSwordDL(Gfx** dList, void* ootHand, struct Player* player, u8 bodyEnvR, u8 bodyEnvG,
                                  u8 bodyEnvB);

#ifdef __cplusplus
}
#endif

#endif // WEAPON_UPGRADES_H
