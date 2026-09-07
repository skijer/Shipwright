/**
 * boss_remains.h - The four boss remains (Odolwa/Goht/Gyorg/Twinmold) as custom
 * wearable "masks" (Skijer's NEI) — SoH/OoT port of the 2ship module.
 *
 * OoT has no native remains items, so they live on four repurposed u8 item ids
 * (ITEM_MM_REMAINS_ODOLWA..TWINMOLD = 0x80/0x81/0x9C/0x89 — the only free
 * C-button-visible u8 slots, NON-contiguous; use BossRemains_ItemIndex /
 * BossRemains_IndexItem instead of range tests). They fit directly in the u8
 * buttonItems array (slots 1-7 = C-left/down/right + D-pad) — no extButtons/u16
 * infra needed (unlike the spiritual stones). Ownership is the Fleet combo sync
 * bit: Nei_Save()->mmQuestItems & FC_MMQ_REMAINS_* (bits 0-3). Their worn-on-
 * face geometry is the Moon Child's masks (gMoonChild*MaskDL in object_ob),
 * which ARE face-fitted, loaded from mm.o2r via MmAssets_LoadResource and drawn
 * on Link's head node.
 *
 * Three phases:
 *   1) Equip a remains to a C or D-pad slot from the NEI MM quest page
 *      (KaleidoScope_DrawMmQuestStatus, cursor points 0-3).
 *   2) Press that button in-game to don/doff it on Link's face (mask toggle).
 *   3) While worn, A/B/R do the remains' own actions (+ summon friendly allies).
 */
#ifndef BOSS_REMAINS_H
#define BOSS_REMAINS_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// The remains item ids are NON-contiguous in OoT (0x80, 0x81, 0x9C, 0x89) — never
// range-test them. Item id -> 0..3 index (Odolwa/Goht/Gyorg/Twinmold), or -1.
s32 BossRemains_ItemIndex(s16 item);
// 0..3 index -> item id (ITEM_MM_REMAINS_*), or ITEM_NONE for anything else.
s16 BossRemains_IndexItem(s32 idx);

// Phase 1 — equip a boss remains to a C or D-pad slot from the NEI MM kaleido quest
// page. Call from KaleidoScope_DrawMmQuestStatus's idle input block while the cursor
// is on a remains point (0-3), passing the hovered remains item id
// (BossRemains_IndexItem(sMmPagePoint)). Returns true if it consumed a button press.
// NOTE (OoT port): unlike MM, the MM quest page zeroes cursorItem[PAUSE_QUEST] for
// non-song points, so the hovered item is passed explicitly instead of read back.
s32 BossRemains_TryEquipAtCursor(PlayState* play, Input* input, s16 item);

// Phase 2 — per-frame input tick (from Player_UpdateCommon): a press on the C/D-pad
// button that holds a remains toggles wearing that remains on Link's face.
void BossRemains_TickInput(PlayState* play, Player* player);

// Phase 2 — draw the worn remains mask on Link's face. Call from
// Player_PostLimbDrawGameplay at the PLAYER_LIMB_HEAD node (head matrix current).
void BossRemains_DrawWornMask(PlayState* play, Player* player);

// The currently-worn remains item id (ITEM_MM_REMAINS_*), or ITEM_NONE if none.
s16 BossRemains_GetWorn(void);

// Force-remove any worn remains (e.g. when a native mask is donned).
void BossRemains_ClearWorn(void);

// Phase 3 (deprecated test hook) — kept as a no-op so the header/ABI stays stable.
s32 BossRemains_TryActionA(PlayState* play, Player* player);

// Odolwa A-action accessors, read by z_player.c:
//   BossRemains_RunSpeedMul: 2x while Odolwa's boosted run (A held) is active,
//   else 1.0x. BossRemains_IsOdolwaRunning: true while that state is active (drives the red trail).
f32 BossRemains_RunSpeedMul(void);
s32 BossRemains_IsOdolwaRunning(void);

// True whenever the Odolwa remains is worn — gates the sword/shield swap + faster sword attacks.
s32 BossRemains_IsOdolwaWorn(void);

// Draw Odolwa's red running afterimage (frozen-pose ghosts of Link, dark-purple fog tint). Call from
// the player draw (z_player.c). No-op unless Odolwa is running.
void BossRemains_DrawOdolwaTrail(Player* player, PlayState* play);

// Draw Odolwa's sword / shield in Link's hands (called from Player_PostLimbDrawGameplay at the
// LEFT_HAND / RIGHT_HAND limbs, where the native ones are hidden while the Odolwa remains is worn).
void BossRemains_DrawOdolwaSword(PlayState* play, Player* player);
void BossRemains_DrawOdolwaShield(PlayState* play, Player* player);

// Sword swing with magic → fire a moth projectile at the Z-target (FD-beam style). Call from the
// melee-attack setup in z_player.c. Self-guards on Odolwa-worn + available magic + lock-on.
void BossRemains_OdolwaSwordMoth(PlayState* play, Player* player);

// Idle stance override: while the Odolwa remains is worn, Link's idle pose is Odolwa's "ready"
// stance. Call from the player idle-anim selection; returns NULL (keep vanilla idle) if the
// retargeted anim (npc_link_anims.o2r) isn't loaded.
LinkAnimationHeader* BossRemains_GetOdolwaIdleAnim(void);
// True while Link rides the Odolwa moth-cloud ("Nimbus" flight). Read by the walk-off handler +
// gravity hooks so the flight driver owns movement (no fall action), like the Goht charge.
s32 BossRemains_IsOdolwaFlying(void);
// Per-frame Odolwa flight driver (A on the ground → moth-summon dance → free 3D float on the cloud).
// NOTE (OoT port): OoT has no deku flowers, so takeoff needs no flower — A while grounded anywhere.
// Call from Player_UpdateCommon AFTER the action func so the overrides win.
void BossRemains_OdolwaFlightTick(PlayState* play, Player* player);

// ── Goht ─────────────────────────────────────────────────────────────────────
// True while the Goht remains is worn.
s32 BossRemains_IsGohtWorn(void);
// True while the bull charge is running. Read by the walk-off handler in z_player.c so a charging
// Goht is exempt from the fall action / auto-hop / ledge-grab, exactly like the Goron roll in MM —
// that's what lets a ledge or ramp launch Link instead of dropping him.
s32 BossRemains_IsGohtCharging(void);

// ── Gyorg ────────────────────────────────────────────────────────────────────
// True while the Gyorg remains is worn — drives human Link's Zora-style free 3D dive/swim, water-
// current immunity, "can't walk in water", and damage resilience (each z_player.c hook adds its own
// in-water test on top of this).
s32 BossRemains_IsGyorgWorn(void);
// Spawn Gyorg's fish school near Link. Called from the R (swim) / R+B (land) summon.
void BossRemains_GyorgSummonFish(PlayState* play, Player* player);
// Draw Gyorg's whirlpool funnel (B while swimming). Kept as a no-op for ABI stability — the MM
// visual (EnWaterEffect) has no OoT counterpart yet; the pull/damage logic runs in the tick.
void BossRemains_DrawGyorgWhirlpool(Player* player, PlayState* play);
// True while any remains that takes over the A button is worn (Odolwa runs, Goht bull-charges) —
// suppresses the roll at its z_player.c choke point.
s32 BossRemains_SuppressRoll(void);
// The bull QUAKE POUND (jump-attack hop → landing quake → landing anim). While Goht is worn the
// sword is unequipped, so this is triggered directly from R+A in BossRemains_GohtPostAction.
s32 BossRemains_GohtQuakeStart(PlayState* play, Player* player);
// Per-frame Goht driver (bull charge speed/anim/crash, quake landing). Call from
// Player_UpdateCommon AFTER the action func so the overrides win.
void BossRemains_GohtPostAction(PlayState* play, Player* player);
// Majora-red bull-charge cone around Link. Call from the player draw, next to the Odolwa trail.
void BossRemains_DrawGohtCone(Player* player, PlayState* play);
// Goht's charging-thunder VFX (growing light orb + crossed bolts) in front of Link while B is held.
// Call from the player draw, next to the cone.
void BossRemains_DrawGohtChargingThunder(Player* player, PlayState* play);

#ifdef __cplusplus
}
#endif

#endif // BOSS_REMAINS_H
