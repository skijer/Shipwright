/**
 * gerudo_form.h — Gerudo Form (OOT Gerudo Mask transformation, Garo-style)
 *
 * Wires the OOT Gerudo Mask to the O2rLoader's "gerudo" model. When the
 * cheat `gMods.GerudoMaskTransform` is on and Link equips the mask, we call
 * `O2rLoader_ForceModel("gerudo")`. Link keeps his own skeleton and anims;
 * the gerudo look comes from a draw-time DL path-swap
 * (CustomForms_OverrideLimbDraw) that redirects vanilla Link DL references to
 * the gerudo .o2r's `objects/forms/gerudo/...` twins, tinted with Link's
 * current tunic color (redirection lives in custom_forms.h now).
 *
 * Mask is removed → O2rLoader_ClearForcedModel → Link's vanilla skel/skin
 * returns. The toggle is edge-detected per-frame from an OnPlayerUpdate hook.
 *
 * Effects active while the mask is worn:
 *   - Sandstorm OFF in Haunted Wasteland (per-frame + on transition end), plus
 *     a skippable Yes/No offer to warp straight across the desert.
 *   - Gerudo NPCs friendly: VB_GERUDOS_BE_FRIENDLY → true.
 *   - Skip card-give: VB_GIVE_ITEM_GERUDO_MEMBERSHIP_CARD → false (access is
 *     temporary; no QUEST_GERUDO_CARD is granted).
 *   - No jail: VB_GERUDO_FIGHTER_THROW_LINK_TO_JAIL → false.
 *
 * Combat is NOT here — it belongs to gerudo_mhr_combat.inc.c
 * (MmForm_GerudoMhrUpdate), dispatched from MmForm_UpdateActive.
 *
 * The Ge1/Ge2/Ge3 actor patches still call GerudoForm_IsActive() — the
 * function stays in the public API and now returns true when the O2rLoader
 * has "gerudo" forced.
 */

#ifndef GERUDO_FORM_H
#define GERUDO_FORM_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// One-time init: registers VB + frame hooks. Idempotent.
void GerudoForm_Init(void);

// True if the O2rLoader currently has "gerudo" forced (which happens iff
// the cheat is on AND the OOT Gerudo Mask is equipped).
u8 GerudoForm_IsActive(void);

// Resolve Link's current tunic into a Color_RGB8, honouring the cosmetic
// CVar overrides (CVAR_COSMETIC("Link.KokiriTunic.Value"), etc). Used by
// custom_forms.cpp::CustomForms_OverrideLimbDraw to tint the gerudo outfit.
void GerudoForm_GetTunicColor(s32 tunic, Color_RGB8* out);

// Native OOT VFX timeline for the demon-mode L+B attack (Urbosa's Fury).
// `sourceFrame` is the current frame of gs_wirebug_attack04; `active` must be
// supplied every Gerudo combat tick so an interrupted attack restores the
// environment immediately. Visual/audio only: damage and collision stay in
// gerudo_mhr_combat.inc.c.
void GerudoForm_TickUrbosaFuryVfx(PlayState* play, Player* player, f32 sourceFrame, u8 active);

// Retained no-op (always returns 0). The gerudo form now draws entirely
// through Link's own Player_DrawImpl with a DL path-swap, so there is no
// separate gerudo body pass to trigger here. Kept for ABI stability with any
// surviving z_player.c callsite.
s32 GerudoForm_TryDrawSmoothSkin(PlayState* play, Player* player);

// Dual-wield hand DLs. ONE scimitar DL from the gerudo .o2r serves both hands
// and both ages: the right-hand bone matrix mirrors it, and the child skel's
// smaller bone scale shrinks it proportionally.
//
// Returns NULL when the scimitars are sheathed — visibility is owned entirely
// by the MHR fighter latch (GerudoMhr_SwordsOut, gerudo_mhr_combat.inc.c),
// which also plays the unsheath/sheathe SFX on its edges. Also NULL if the
// resource is missing (cosmetic miss, not a crash — caller falls back to the
// vanilla hand DL).
Gfx* GerudoForm_GetSwordDL_L(void);
Gfx* GerudoForm_GetSwordDL_R(void);

// Hand/sheath DL for the active Gerudo Form; returns 1 if it claimed limbIndex (caller skips vanilla). Skijer's NEI
u8 GerudoForm_ResolveLimbDL(s32 limbIndex, Gfx** dList);

// MM Gerudo combo bridge — implemented in mm_player_form.cpp. Called from
// Player_PostLimbDrawGameplay (z_player_lib.c) at the L_HAND / R_HAND limbs,
// where the bone matrix is in scope for Matrix_MultVec3f. PunchActiveThisFrame
// gates trail+hitbox setup to the active damage window; the R-trail index
// getter returns the EffectBlure slot spawned for the R sword (the L sword
// piggybacks on Link's vanilla meleeWeaponEffectIndex). Damage is the
// per-slash value flagged by the action handler.
//
// NOTE: PunchActiveThisFrame currently has no in-tree caller — it lost its last
// one when the sword-visibility latch moved to GerudoMhr_SwordsOut (2026-08-07).
// Kept as the public read of gFormState.gerudoQuadsActive.
u8 GerudoForm_PunchActiveThisFrame(void);
s32 GerudoForm_GetRightTrailEffectIndex(void);
u8 GerudoForm_GetCurrentDamage(void);

// Gerudo does NOT shield. R is the wirebug modifier, owned by
// MmForm_GerudoMhrUpdate, and OOT's shield actions are gated off form-side via
// MmForm_GetShieldMode() == MMFORM_SHIELD_BLOCK — PLAYER_STATE1_SHIELDING never
// sets for this form. The player's equipped shield is neither read nor written
// (decision 2026-07-28: no form touches the player's equipment), and there is no
// heldItemAction pinning left in this module (removed 2026-08-07 with the
// pre-MHR combat state machine).

#ifdef __cplusplus
}
#endif

#endif // GERUDO_FORM_H
