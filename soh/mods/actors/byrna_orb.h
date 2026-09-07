/**
 * byrna_orb.h — Cane of Byrna (ext sword 1) Kinsect: the Insect Glaive's orb.
 *
 * NEAR     orbits the cane's tip. Link is INVULNERABLE while it is there, but
 *          every hit it eats costs magic — which is what makes keeping it home
 *          expensive and launching it the better play.
 * OUTBOUND launched with R. Aimed at the Z-target, or straight ahead when there
 *          is none. One Master-Sword-class hit.
 * RETURN   flying home; R recalls it once it has banked a charge.
 *
 * CHARGE: one per DISTINCT enemy struck, or a full bar from a single boss. At
 * BORB_CHARGE_MAX a wind column marks Link and R stops launching the orb and
 * launches HIM instead. Touching the ground spends the whole bar.
 *
 * SUPER DAMAGE: the launched orb claims it, plus its post-impact grace. An orb
 * sitting at the tip must NOT, or standing near a boss would paralyse it every
 * frame — the trap trident_charge_ball.h documents.
 *
 * Text-included from extended_equipment.c; only these accessors are exported.
 *
 * Skijer's NEI
 */

#ifndef BYRNA_ORB_H
#define BYRNA_ORB_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BYRNA_CHARGE_MAX 3

/** True while a LAUNCHED orb is in flight or inside the post-impact grace window.
 *
 * The grace is NOT optional. Actors update in category order (PLAYER = 2 before
 * BOSS = 9), so on the frame after its AT lands the orb sees its own AT_HIT and
 * dies BEFORE the boss reads BUMP_HIT — the boss would find this false and drop
 * the super hit in silence. Same bug as Mario's fireball (sm64_mario_items.c). */
u8 ByrnaOrb_IsActive(void);

/** The R button. Summons and launches the orb, or recalls it once it has banked
 *  a charge. Returns 0 when nothing happened. */
u8 ByrnaOrb_PressR(PlayState* play);

/** Z + R: a damageless seed that tints whatever it touches, the way the Cane of
 *  Pacci marks an Ultrahand target. */
u8 ByrnaOrb_ThrowSeed(PlayState* play);

/** Player damage path. While the orb is home Link cannot be hurt; the hit is
 *  paid for in magic instead. Returns 1 when the damage was consumed. */
u8 ByrnaOrb_TryAbsorb(PlayState* play);

/** Grace window + per-frame upkeep. Driven from ExtEquip_Update so it still runs
 *  with no orb alive. */
void ByrnaOrb_Tick(void);

/** Drop the orb and the charge — the slot was unequipped. */
void ByrnaOrb_Cleanup(void);

/** Forget the orb WITHOUT touching the actor. On scene load every actor is
 *  already freed, so Cleanup's Actor_Kill would follow a dangling pointer, and
 *  doing nothing would leave the pointer non-NULL forever, blocking every future
 *  summon for the rest of the session. */
void ByrnaOrb_Forget(void);

/** Banked charges, 0..BYRNA_CHARGE_MAX. */
u8 ByrnaOrb_GetCharge(void);

/** True at a full bar: the wind column shows, and R launches Link. */
u8 ByrnaOrb_IsCharged(void);

/** True while the orb is home and therefore shielding Link. */
u8 ByrnaOrb_IsGuarding(void);

/** Landing spends the bar. */
void ByrnaOrb_OnLand(void);

/** Draw hook for the orb and the full-charge wind column. */
void ByrnaOrb_Draw(PlayState* play);

#ifdef __cplusplus
}
#endif

#endif // BYRNA_ORB_H
