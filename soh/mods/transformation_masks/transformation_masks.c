/**
 * transformation_masks.c - MM Transformation Masks Router
 *
 * Routes calls from z_player.c hooks to mm_player_form.cpp implementation.
 * Asset replacement getters remain here (they work independently).
 */

#include "mods/transformation_masks/transformation_masks.h"
#include "mods/transformation_masks/assets/mm_asset_loader.h"
#include "mods/transformation_masks/gerudo_form.h"
#include "mods/transformation_masks/custom_forms.h"
#include "mods/transformation_masks/boss_super_damage.h"
#include "mods/items/logic/weapon_upgrades.h" // NEI Real Master Sword super-damage
#include "mods/actors/trident_charge_ball.h"  // Trident charged ball super-damage claim
#include "mods/actors/byrna_orb.h"            // Byrna orb (Insect Glaive Kinsect) super-damage claim
#include "mods/o2r_loader/o2r_loader.h"
#include "overlays/effects/ovl_Effect_Ss_Fhg_Flash/z_eff_ss_fhg_flash.h"
#include "functions.h"
#include <libultraship/log/luslog.h>
#include <string.h>

// Needed for the Gerudo / Shielding probe in TransformMasks_FilterB.
extern PlayState* gPlayState;

// Matrix_GetCurrent is defined in soh/src/code/sys_matrix.c but is not declared
// in any SoH public header — it leaks out of frame_interpolation.cpp and
// sw97_compat.h with the proper `MtxF*` return type. Without a forward decl
// here, line 576 below makes the compiler synthesize an implicit `int()`
// prototype, which then conflicts with sw97_compat.h's correct prototype the
// moment z_player.c includes sw97_router.c → sw97_compat.h. Result: C2040 in
// sw97_compat.h. Forward-declaring it here matches the actual signature and
// resolves the conflict.
extern MtxF* Matrix_GetCurrent(void);

// =============================================================================
// Global MmPlayer instance (declared extern in transformation_masks.h)
// =============================================================================

MmPlayerCore gMmPlayer;

// Zora fin DLs for boomerang visual override (set by mm_player_form.cpp on load/unload)
Gfx* gZoraFinBoomerangLDL = NULL;
Gfx* gZoraFinBoomerangRDL = NULL;

// =============================================================================
// Forward declarations to mm_player_form.cpp (compiled separately as .cpp)
// =============================================================================

extern void MmForm_Init(PlayState* play, Player* player);
extern u8 MmForm_IsEnabled(void);
extern u8 MmForm_IsTransformed(void);
extern u8 MmForm_IsTransformedAny(void);
extern u8 MmForm_HasSkeleton(void);
extern u8 MmForm_IsFDSkinMode(void);
extern u8 MmForm_IsPikachuActive(void);
extern u8 MmForm_IsItemAllowed(s32 item);
extern u8 MmForm_IsSlotAllowed(u8 slot);
extern u8 MmForm_IsGoronRolling(void);
extern u8 MmForm_OnWaterSwimAttempt(PlayState* play, Player* player);
extern TransformMaskId MmForm_GetMaskType(s32 item);
extern void MmForm_HandleMaskUse(PlayState* play, Player* player, s32 item);
extern void MmForm_Update(PlayState* play, Player* player);
extern void MmForm_Draw(PlayState* play, Player* player);
extern void MmForm_Reset(void);
extern void MmForm_OnDeath(void);
extern f32 MmForm_GetCameraHeight(void);
extern u8 MmForm_BlocksLedgeGrab(void);
extern u8 MmForm_IsZoraSwimEnabled(void);
extern void MmForm_SetZoraSwimEnabled(u8 enabled);
extern Gfx* MmForm_LoadAndPreResolveMmDL(const char* path);
extern u8 MmForm_DragonScaleEnterSwim(PlayState* play, Player* player);
extern void MmForm_DragonScaleSwimUpdate(PlayState* play, Player* player);
extern void MmForm_DragonScaleExitSwim(Player* player);

// mm_mask_wear.cpp
extern void MmMaskWear_Toggle(PlayState* play, Player* player, s32 itemId);
extern void MmMaskWear_Draw(PlayState* play, Player* player);
extern void MmMaskWear_Update(PlayState* play, Player* player);
extern s32 MmMaskWear_GetCurrent(void);
extern void MmMaskWear_Clear(void);
extern void MmMaskWear_DeactivateChateauRomani(void);

// garo_form.cpp (custom Garo skin-swap + attack kit, not a real MM form).
// Activated via O2rLoader_ForceModel("garo"); independent of MmForm.
extern void GaroForm_Update(PlayState* play, Player* player);
extern void GaroForm_DrawProjectiles(PlayState* play);
// True when Link has a non-grab contextual A action pending (speak/open/etc.);
// FilterB uses it to decide whether to strip A for the Garo moveset.
extern u8 GaroForm_VanillaWantsAButton(Player* player);

// gerudo_mhr_combat.inc.c: 1 while the scimitars are actually in her hands (i.e.
// Link is holding a melee weapon). FilterB uses it to tell "draw" from "swing".
extern u8 GerudoMhr_SwordsOut(void);
extern u8 GerudoMhr_LOwnsB(void);

// (GerudoForm_Update is gone — see the tombstone at the bottom of
// gerudo_form.cpp. Gerudo combat is dispatched by MmForm_GerudoMhrUpdate from
// MmForm_UpdateActive, the same single-owner rule Garo got in v9.)

// =============================================================================
// No-op Action Function (replaces OOT actionFunc while transformed)
//
// OOT's Player_UpdateCommon (z_player.c ~line 11994) runs BEFORE actionFunc:
//   - invincibility timer, Player_UpdateInterface, Player_UpdateZTargeting
//   - Player_ProcessControlStick (populates prevControlStickMagnitude/Angle)
//   - Collision/gravity (Actor_MoveXZGravity, Player_ProcessSceneCollision)
// Then calls: this->actionFunc(this, play)  <-- lands here
// After: Player_UpdateCamAndSeqModes, Collider_UpdateCylinder, etc.
//
// We intentionally do nothing here. All gameplay comes from MmForm_Update.
// =============================================================================

void MmForm_OotNoopAction(Player* thisx, PlayState* play) {
    // Empty: OOT actions disabled while MM form is active.
}

f32 TransformMasks_GetStickMagnitude(void) {
    return Player_GetControlStickMagnitude();
}

s32 TransformMasks_GetFloorType(void) {
    return Player_GetFloorType();
}

// =============================================================================
// Routing to mm_player_form.cpp
// =============================================================================

u8 TransformMasks_IsEnabled(void) {
    return MmForm_IsEnabled();
}

u8 TransformMasks_IsTransformed(void) {
    return MmForm_IsTransformed();
}

// Redirect OOT voice SFX to MM equivalent for current form.
// OOT voice base = 0x6800 (NA_SE_VO_LI_SWORD_N).
// MM voiceSfxIdOffset per form (from 2Ship z_player.c sPlayerAgeProperties):
//   FD=0x00, Human=0x20, Deku=0x80, Zora=0xA0, Goron=0xC0
// Returns 1 if a form-specific voice was played (caller must NOT play the OOT
// voice afterwards); 0 if no override applies — no mm.o2r, action out of range,
// or a form with no voice bank (Human/Gerudo/Pikachu) — in which case the caller
// should fall back to Link's OOT voice.
u8 TransformMasks_TryPlayMmVoice(u16 ootVoiceSfxId, Vec3f* pos) {
    if (!MmSfx_IsAvailable())
        return 0;

    // Compute action index: ootVoiceSfxId is the BASE sfxId (before OOT age offset)
    // e.g., NA_SE_VO_LI_DAMAGE_S = 0x6805, action = 5. The OOT child (_KID) voice
    // IDs live at 0x6820+ and are intentionally out of range here — callers pass
    // the adult base id and the form voice bank supplies the form's "age".
    u16 action = ootVoiceSfxId - 0x6800;
    if (action >= 0x20)
        return 0; // Out of range

    // Get MM voice offset for current form
    u16 mmOffset;
    MmPlayerTransformation form = MmForm_GetCurrentForm();
    switch (form) {
        case MM_PLAYER_FORM_GORON:
            mmOffset = 0xC0;
            break;
        case MM_PLAYER_FORM_ZORA:
            mmOffset = 0xA0;
            break;
        case MM_PLAYER_FORM_DEKU:
        // Keaton borrows the Deku bank. Routing him through MM's engine is also what
        // drops the reverb: it renders SFX dry, OOT's applies the scene send.
        case MM_PLAYER_FORM_KEATON:
            mmOffset = 0x80;
            break;
        case MM_PLAYER_FORM_FIERCE_DEITY:
            mmOffset = 0x00;
            break;
        case MM_PLAYER_FORM_GERUDO:
            // No MM voice samples for Gerudo — return 0 so the caller falls back
            // to the OOT path (Link's normal voice). Mapping a fake offset like
            // 0x40 used to crash the audio thread because MmSfx_PlayAtPos
            // dereferenced a NULL sample for SFX IDs the SF0 doesn't contain.
            // When a gerudo voice pack is added later (pitch-shifted Link or
            // sampled NPC), assign an unused 0x20-wide block (e.g. 0x40 or 0x60)
            // and ship the samples in mm.o2r.
            return 0;
        case MM_PLAYER_FORM_GARO: {
            // Garo does NOT get a voice-bank offset, and the 0x60 one it used to carry was
            // simply wrong. The "each form owns a 0x20-wide block" rule is real only for the
            // forms MM actually has: 0x6800 Link (FD), 0x6880 Deku, 0x68A0 Zora, 0x68C0
            // Goron — the three DUMMY-named blocks. 0x6860 is not a player block at all, it
            // holds real NPC voices (RT/ST/Z0/SK/NA), so the Garo was asking for samples that
            // were never his. Nothing played, which is what "Garo sigue sin voice" was.
            //
            // MM has no Garo transformation, so there is no player bank to point at. His
            // voice is the one the user asked for: Igos du Ikana's, from En_Osk — the enemy
            // bank's NA_SE_EN_BOSU_* set. That needs an explicit per-action map rather than
            // an offset, because these ids are scattered, not contiguous.
            //
            // EVERY action is mapped, so Garo never speaks with Link's voice: a row left
            // at 0 falls through to the OOT sample, and hearing Link grunt out of a Garo
            // was the whole complaint. Where Igos has no obvious counterpart the nearest
            // one in character is reused — his bank is small (roughly attack / damage /
            // shock / cynical / laugh / talk / stand / dead), so the pairing is by TONE,
            // not by a literal match.
            static const u16 sGaroVoiceByAction[0x20] = {
                [0x00] = 0x3A30, // SWORD_N        -> BOSU_ATTACK
                [0x01] = 0x3A4C, // SWORD_L        -> BOSU_ATTACK_K
                [0x02] = 0x3A4A, // LASH           -> BOSU_ATTACK_W
                [0x03] = 0x3A2B, // HANG           -> BOSU_HAND    (effort, hanging on)
                [0x04] = 0x3A2A, // CLIMB_END      -> BOSU_STAND
                [0x05] = 0x3A3A, // DAMAGE_S       -> BOSU_DAMAGE
                [0x06] = 0x3A2E, // FREEZE         -> BOSU_SHOCK
                [0x07] = 0x3A90, // FALL_S         -> BOSU_TALK
                [0x08] = 0x3A3A, // FALL_L         -> BOSU_DAMAGE
                [0x09] = 0x3A9C, // BREATH_REST    -> BOSU_STAND_RAPID (panting)
                [0x0A] = 0x3A90, // BREATH_DRINK   -> BOSU_TALK
                [0x0B] = 0x3A5B, // DOWN           -> BOSU_DEAD_VOICE
                [0x0C] = 0x3A2E, // TAKEN_AWAY     -> BOSU_SHOCK
                [0x0D] = 0x3A2B, // HELD           -> BOSU_HAND
                [0x0E] = 0x3A2F, // SNEEZE         -> BOSU_SHIT     (his short splutter)
                [0x0F] = 0x3A90, // SWEAT          -> BOSU_TALK
                [0x10] = 0x3A90, // DRINK          -> BOSU_TALK
                [0x11] = 0x3A29, // RELAX          -> BOSU_SIT
                [0x12] = 0x3A4D, // SWORD_PUTAWAY  -> BOSU_SWORD
                [0x13] = 0x3A31, // GROAN          -> BOSU_CYNICAL
                [0x14] = 0x3A2A, // AUTO_JUMP      -> BOSU_STAND
                [0x15] = 0x3A32, // MAGIC_NALE     -> BOSU_LAUGH
                [0x16] = 0x3A2E, // SURPRISE       -> BOSU_SHOCK
                [0x17] = 0x3A47, // MAGIC_FROL     -> BOSU_LAUGH_K
                [0x18] = 0x3A2B, // PUSH           -> BOSU_HAND
                [0x19] = 0x3A2B, // HOOKSHOT_HANG  -> BOSU_HAND
                [0x1A] = 0x3A3A, // LAND_DAMAGE_S  -> BOSU_DAMAGE
                [0x1B] = 0x3A90, // NULL_0x1b      -> BOSU_TALK     (unused in OOT)
                [0x1C] = 0x3A33, // MAGIC_ATTACK   -> BOSU_LAUGH_DEMO
                [0x1D] = 0x3A45, // (unused id)    -> BOSU_LAUGH_DEMO_K
                [0x1E] = 0x3A3D, // DEMO_DAMAGE    -> BOSU_DEAD
                [0x1F] = 0x3A2E, // ELECTRIC_SHOCK -> BOSU_SHOCK
            };
            u16 garoSfx = sGaroVoiceByAction[action];
            if (garoSfx == 0) {
                // Every row is filled, so this is unreachable today; kept as the
                // safe landing for a row someone blanks out later.
                return 0;
            }
            lusprintf(__FILE__, __LINE__, LUSLOG_LEVEL_INFO, "[MmVoice] GARO oot=0x%04X action=0x%X -> BOSU 0x%04X",
                      (u32)ootVoiceSfxId, (u32)action, (u32)garoSfx);
            MmSfx_PlayAtPos(garoSfx, pos);
            return 1;
        }
        default:
            return 0;
    }

    u16 mmSfxId = 0x6800 + mmOffset + action;
    // Diagnostic so we can see at runtime whether the voice routing fires for
    // each transformed form. If this log line appears in the SoH log but the
    // user hears nothing, the failure is downstream (sample missing in SF0,
    // soundEffects index out of range, or volume=0). If the line does NOT
    // appear, the upstream Player_PlayVoiceSfx hook never reached us.
    lusprintf(__FILE__, __LINE__, LUSLOG_LEVEL_INFO,
              "[MmVoice] form=%d oot=0x%04X offset=0x%X action=0x%X -> mmSfxId=0x%04X", (s32)form, (u32)ootVoiceSfxId,
              (u32)mmOffset, (u32)action, (u32)mmSfxId);
    MmSfx_PlayAtPos(mmSfxId, pos);
    return 1;
}

// Void wrapper kept for the Player_PlayVoiceSfx hook (z_player.c), which
// suppresses the OOT voice whenever transformed regardless of the result.
void TransformMasks_PlayMmVoice(u16 ootVoiceSfxId, Vec3f* pos) {
    TransformMasks_TryPlayMmVoice(ootVoiceSfxId, pos);
}

// Redirect OOT floor/walk SFX to MM equivalent for current form. MM's
// Player_GetFloorSfxByAge adds ageProperties->surfaceSfxIdOffset to the base
// step SFX so each form picks its own playerbank slot (Deku +0xF0, Zora
// +0x120, Goron +0x150). Returns 1 if it played a form-specific SFX and the
// caller should skip the OOT step SFX; returns 0 if no override applies
// (human/FD/Garo/Gerudo — let OOT handle it).
u8 TransformMasks_TryPlayMmStepSfx(u16 ootStepSfxId, Vec3f* pos) {
    if (!MmSfx_IsAvailable()) {
        return 0;
    }

    // Values verified verbatim against MM decomp sPlayerAgeProperties[]:
    //   FD     = 0x80  (mm z_player.c:800)
    //   Goron  = 0x150 (mm z_player.c:896)
    //   Zora   = 0x120 (mm z_player.c:992)
    //   Deku   = 0xF0  (mm z_player.c:1088)
    //   Human  = 0     (mm z_player.c:1184 — no offset, use OOT path)
    // Goron ball-roll / ground-pound emits its own rolling SFX — no footsteps.
    // Return "handled" so the caller skips OOT's step too (total silence).
    if (MmForm_IsGoronRolling()) {
        return 1;
    }

    u16 mmOffset = 0;
    switch (MmForm_GetCurrentForm()) {
        case MM_PLAYER_FORM_FIERCE_DEITY:
            mmOffset = 0x80;
            break;
        case MM_PLAYER_FORM_GORON:
            mmOffset = 0x150;
            break;
        case MM_PLAYER_FORM_ZORA:
            mmOffset = 0x120;
            break;
        case MM_PLAYER_FORM_DEKU:
            mmOffset = 0xF0;
            break;
        default:
            // Garo/Gerudo/Human/Pikachu: no MM step SFX override — use OOT.
            return 0;
    }

    // MM step IDs live at 0x800-base + floorOffset + form offset. The
    // ootStepSfxId already contains the floor offset (caller built it via
    // Player_ApplyFloorSfxOffset or similar), so we just add the form offset
    // on top.
    u16 mmSfxId = ootStepSfxId + mmOffset;
    MmSfx_PlayAtPos(mmSfxId, pos);
    return 1;
}

u8 TransformMasks_HasSkeleton(void) {
    return MmForm_HasSkeleton();
}

TransformMaskId TransformMasks_GetMaskType(s32 item) {
    return MmForm_GetMaskType(item);
}

void TransformMasks_HandleMaskUse(PlayState* play, Player* player, s32 item) {
    MmMaskWear_Clear(); // Clear worn mask before transformation
    MmForm_HandleMaskUse(play, player, item);
}

void TransformMasks_Init(PlayState* play, Player* player) {
    MmForm_Init(play, player);
}

// =============================================================================
// Input filter — strip BTN_B for systems that reserve it.
//
// Called from z_player.c Player_Update right after `sp44 = play->state.input[0]`
// and before Player_UpdateCommon. Replaces the inline blocks for Blast Mask /
// Great Fairy Mask / Garo skin that used to live in z_player.c.
// =============================================================================
// True while a textbox or the ocarina owns the buttons. Forms that read the RAW
// play->state.input[0] (Garo's moveset dispatcher, Pikachu's bindings) bypass the filtered
// copy below, so they must consult this before acting on a press — otherwise they fire
// their moveset while the player is playing notes.
u8 MmForm_InputOwnedByMessage(void) {
    return (gPlayState != NULL) && (gPlayState->msgCtx.msgMode != MSGMODE_NONE);
}

void TransformMasks_FilterB(Input* input) {
    if (input == NULL)
        return;

    // ── One rule for every form: while a message or the ocarina is up, NO form eats input ──
    // Those buttons belong to the textbox/ocarina — OoT suppresses its own item pipeline in
    // that state on purpose. Each form below only ever stripped the buttons IT cared about
    // (Garo B/A, Gerudo B, masks B), so everything they did not strip leaked through and
    // kept firing movesets and equipped items while the player was playing notes. Filtering
    // here covers every form at once, including future ones, instead of each of them having
    // to remember the rule.
    //
    // This is the FORMS' filtered copy (sp44), not the raw play->state.input[0] that the
    // ocarina itself reads, so the notes still get their input.
    if (gPlayState != NULL && gPlayState->msgCtx.msgMode != MSGMODE_NONE) {
        const u16 owned = BTN_A | BTN_B | BTN_CUP | BTN_CDOWN | BTN_CLEFT | BTN_CRIGHT;
        input->cur.button &= ~owned;
        input->press.button &= ~owned;
        return;
    }

    // Blast Mask + Great Fairy Mask: B handled by MmMaskWear_Update on raw input.
    s32 wornMask = MmMaskWear_GetCurrent();
    if (wornMask == ITEM_MM_MASK_BLAST || wornMask == ITEM_MM_MASK_GREAT_FAIRY) {
        input->cur.button &= ~BTN_B;
        input->press.button &= ~BTN_B;
        return;
    }

    // Garo: B is always reserved for the Garo combat moveset (3-slash combo +
    // rod aim). A is reserved EXCEPT when Link has a non-grab contextual
    // action pending (speak / read / open / enter / climb / mount) — then A
    // passes through to vanilla so the player can still interact with the
    // world. Grab is NOT a pass-through: Garo can't lift objects, and since
    // the grab handler reads this same filtered input (sControlInput == sp44),
    // stripping A here also suppresses the grab cleanly.
    //
    // GaroForm_Update reads the raw play->state.input[0] (not this sp44 copy)
    // so its A dispatcher still sees the press when we strip it here; it gates
    // on the SAME predicate so it doesn't double-fire while vanilla owns A.
    if (MmForm_GetCurrentForm() == MM_PLAYER_FORM_GARO) {
        input->cur.button &= ~BTN_B;
        input->press.button &= ~BTN_B;

        Player* p = (gPlayState != NULL) ? GET_PLAYER(gPlayState) : NULL;
        if (!GaroForm_VanillaWantsAButton(p)) {
            input->cur.button &= ~BTN_A;
            input->press.button &= ~BTN_A;
        }
    }

    // Gerudo Form: B is OOT's own sword pipeline wearing the dual-blade clips, and
    // the draw out of free hands is taken over inside Player_UseItem
    // (GerudoMhr_InterceptUseItem) — so B must reach OOT to get there. The ONE case
    // it must not: L held. L is the modifier (L+B = front slash, L+R = rage), and
    // without this strip OOT started a normal swing on the same press.
    if (GerudoMhr_LOwnsB()) {
        input->cur.button &= ~BTN_B;
        input->press.button &= ~BTN_B;
    }

    // Rito: B is the bow entirely. Stripping it here only hides it from OOT's own scan
    // — the form controller reads the raw play->state.input[0] and still sees the press.
    if (MmForm_GetCurrentForm() == MM_PLAYER_FORM_RITO) {
        input->cur.button &= ~BTN_B;
        input->press.button &= ~BTN_B;
    }
}

void TransformMasks_Update(PlayState* play, Player* player) {
    // Scan C-button/D-pad for transformation mask presses.
    // OOT's Player_UseItem pipeline doesn't run in two cases, so we need a fallback:
    //   1. Transformed (any form): the form's own action loop owns gameplay; OOT's
    //      upper-body update never reaches Player_UseItem for mask items.
    //   2. Swimming (IN_WATER): surface swim actions (D610/D84C/DAB4) don't call
    //      Player_UpdateUpperBody → pipeline never runs. In this case only the Zora
    //      mask is meaningful (and it's the only one whose buttonStatus stays
    //      enabled underwater — see z_parameter.c:1353).
    //
    // The water gate is filtered ONLY for the not-transformed case: a transformed
    // form pressing its own mask must always be able to detransform, even underwater.
    // (Bug previously: as Zora in water, pressing Zora mask did nothing because the
    // `!isNoop` arm of the filter never went false — `isNoop` is dead code.)
    u8 isTransformed = MmForm_IsTransformedAny();
    u8 isInWater = (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0;

    // ...but NEVER while a message or the ocarina is on screen. OOT suppresses its item
    // pipeline in that state ON PURPOSE: the C buttons belong to the ocarina/textbox, not to
    // the equipped items. This fallback bypassed that rule, so playing the ocarina while
    // transformed fired the mask equipped on that C button instead of the note — using the
    // item AND interrupting the ocarina action. MM has no such problem because it never
    // hand-scans the C buttons: it opens the normal ocarina and only swaps the instrument
    // (AudioOcarina_SetInstrument, z_message.c:4719).
    u8 msgActive = play->msgCtx.msgMode != MSGMODE_NONE;

    Input* input = Player_GetControlInput();

    if ((isTransformed || isInWater) && input != NULL && !msgActive) {
        static const u16 sBtns[] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT };
        for (s32 i = 0; i < 3; i++) {
            if (CHECK_BTN_ALL(input->press.button, sBtns[i])) {
                s32 item = C_BTN_ITEM(i);
                if (item != ITEM_NONE && MmForm_GetMaskType(item) != TRANSFORM_MASK_NONE) {
                    // Not transformed + in water: only Zora mask allowed.
                    if (!isTransformed && MmForm_GetMaskType(item) != TRANSFORM_MASK_ZORA)
                        break;
                    TransformMasks_HandleMaskUse(play, player, item);
                    break;
                }
            }
        }
        if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) != 0) {
            static const u16 sDpad[] = { BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };
            // mods/items/helpers/equip_helper.c - the one place that knows who owns the pad.
            // This hand-scan is a THIRD path past Player_GetItemOnButton and ItemInput_Update,
            // and it is how the Kafei mask kept coming on during Ultrahand: both of those were
            // guarded, and none of that reaches here.
            extern u8 ItemInput_ButtonIsClaimed(u16 button);

            for (s32 i = 0; i < 4; i++) {
                if (ItemInput_ButtonIsClaimed(sDpad[i])) {
                    continue;
                }
                if (CHECK_BTN_ALL(input->press.button, sDpad[i])) {
                    s32 item = DPAD_ITEM(i);
                    if (item != ITEM_NONE && MmForm_GetMaskType(item) != TRANSFORM_MASK_NONE) {
                        if (!isTransformed && MmForm_GetMaskType(item) != TRANSFORM_MASK_ZORA)
                            break;
                        TransformMasks_HandleMaskUse(play, player, item);
                        break;
                    }
                }
            }
        }
    }

    MmForm_Update(play, player);

    // v9: Garo combat dispatch is now driven exclusively by MmForm_UpdateActive
    // (mm_player_form.cpp:11030) when the Garo Mask activates the form. Calling
    // GaroForm_Update here ALSO caused the state machine to tick twice per
    // frame — the visible bug was the 3-slash combo blurring into a single
    // instantaneous swing, animations playing at 2x, and the rod charge timer
    // ramping in half the expected time. The MmForm path is the canonical
    // activation, the only one that should exist; this legacy unconditional
    // call is the old O2rLoader skin-swap pathway and is now dead.
    //
    // GaroForm_Update(play, player);  // intentionally removed in v9

    // Gerudo: same story, removed 2026-08-07. The MHR moveset is dispatched from
    // MmForm_UpdateActive (MmForm_GerudoMhrUpdate); this call ticked the retired
    // pre-MHR state machine on player->skelAnime in parallel with it.
    //
    // GerudoForm_Update(play, player);  // intentionally removed
}

void TransformMasks_Draw(PlayState* play, Player* player) {
    MmForm_Draw(play, player);
    // Garo's projectile draw is folded into GaroForm_TryDrawSmoothSkin, which
    // z_player.c calls inside its o2rActive Player_DrawGameplay branch — Garo
    // is a skin-swap (not an MmForm), so this TransformMasks_Draw path isn't
    // reached when Garo is the active model.
}

void TransformMasks_Reset(void) {
    MmForm_Reset();
    MmMaskWear_Clear();
}

void TransformMasks_OnDeath(void) {
    MmMaskWear_DeactivateChateauRomani();

    // Garo: spawn the MM-canon 9-flame death ring BEFORE MmForm_OnDeath rolls
    // back the form. Once MmForm_OnDeath runs, MmForm_GetCurrentForm flips off
    // and any form-aware callback can no longer fire, so the ordering matters.
    if (MmForm_GetCurrentForm() == MM_PLAYER_FORM_GARO && gPlayState != NULL) {
        Player* p = GET_PLAYER(gPlayState);
        if (p != NULL) {
            GaroForm_OnDeath(p, gPlayState);
        }
    }

    // Roll back equipment / strength / pending-reactivate state synchronously.
    // The scene reload that follows will call MmForm_Reset, but by then the
    // backup is empty so the equipment stays restored (instead of being wiped).
    MmForm_OnDeath();
}

u8 TransformMasks_IsFDSkinMode(void) {
    return MmForm_IsFDSkinMode();
}

u8 TransformMasks_IsTransformedAny(void) {
    return MmForm_IsTransformedAny();
}

// NOTE: this is a STATE flag ("the Zora swim is currently active"), NOT a capability gate — the
// Dragon-Scale driver reads it as "am I already swimming" to decide between Enter/Update/Exit. Do NOT
// OR extra "can swim" conditions in here: doing that makes the `if (!IsZoraSwimEnabled())` enter-branch
// unreachable, so the swim never starts. Gyorg's remains instead extends the ACTIVATION gate in
// DragonScale_Behavior (equip_dragonscale.c).
u8 TransformMasks_IsZoraSwimEnabled(void) {
    return MmForm_IsZoraSwimEnabled();
}

void TransformMasks_SetZoraSwimEnabled(u8 enabled) {
    MmForm_SetZoraSwimEnabled(enabled);
}

u8 TransformMasks_HasFireResistance(void) {
    return MmForm_HasFireResistance();
}

u8 TransformMasks_HasWaterBreathing(void) {
    return MmForm_HasWaterBreathing();
}

u8 TransformMasks_GetShieldMode(void) {
    return MmForm_GetShieldMode();
}

u8 TransformMasks_GetWaterMode(void) {
    return MmForm_GetWaterMode();
}

// These bodies ship in soh.o2r, so they must not be gated on mm.o2r being mounted.
static u8 MaskShipsOutsideMmAssets(s32 item) {
    return item == ITEM_MM_MASK_GARO || item == ITEM_MM_MASK_KEATON || item == ITEM_MM_MASK_KAFEI ||
           item == ITEM_RITO_MASK || item == ITEM_MASK_KEATON;
}

u8 TransformMasks_TryFormFromItem(PlayState* play, Player* player, s32 item) {
    if (TransformMasks_IsEnabled() || MaskShipsOutsideMmAssets(item)) {
        if (TransformMasks_GetMaskType(item) != TRANSFORM_MASK_NONE) {
            TransformMasks_HandleMaskUse(play, player, item);
            return 1;
        }
    }
    return CustomForms_TrySkinItem(play, player, item);
}

// =============================================================================
// "Is vanilla already offering something on A?" (Skijer 2026-07-28)
//
// Broader sibling of GaroForm_VanillaWantsAButton: that one deliberately EXCLUDES
// grab (Garo can't lift), this one includes everything the A button can mean so a
// form's custom A move only fires when the button is genuinely free.
//
// Mirrors the arms of Player_UpdateInterface's doAction chain that we can read from
// outside z_player.c: open/enter door, speak/check/read, grab (both the in-range
// actor and the DO_ACTION_GRAB flag), climb, enter, drop/throw while carrying, and
// down (ledge hang / ladder). Deliberately does NOT include roll — callers that also
// need to yield to the roll check movement themselves, since "moving" is what
// distinguishes roll from putaway.
//
// Used by the Zora sea-floor fast-swim gate in MmForm_Action_SwimIdle.
// =============================================================================
u8 TransformMasks_AButtonIsOffered(Player* player) {
    if (player == NULL) {
        return 0;
    }
    if (player->doorType != PLAYER_DOORTYPE_NONE) {
        return 1; // open / enter door
    }
    if ((player->stateFlags2 & PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER) && (player->talkActor != NULL)) {
        return 1; // speak / check / read
    }
    if (player->interactRangeActor != NULL) {
        return 1; // grab / open chest / pick up
    }
    if (player->stateFlags2 & PLAYER_STATE2_DO_ACTION_GRAB) {
        return 1; // grab (wall / ledge / pushable)
    }
    if (player->stateFlags2 & PLAYER_STATE2_DO_ACTION_CLIMB) {
        return 1; // climb wall / vine / ladder
    }
    if (player->stateFlags2 & PLAYER_STATE2_DO_ACTION_ENTER) {
        return 1; // enter crawlspace / transition
    }
    if ((player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) && (player->heldActor != NULL)) {
        return 1; // drop / throw what we're holding
    }
    if (player->stateFlags1 & (PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LADDER)) {
        return 1; // down / let go
    }
    return 0;
}

void* TransformMasks_LoadMmDL(const char* path) {
    return (void*)MmForm_LoadAndPreResolveMmDL(path);
}

u8 TransformMasks_DragonScaleEnterSwim(void* play, void* player) {
    return MmForm_DragonScaleEnterSwim((PlayState*)play, (Player*)player);
}

void TransformMasks_DragonScaleSwimUpdate(void* play, void* player) {
    MmForm_DragonScaleSwimUpdate((PlayState*)play, (Player*)player);
}

void TransformMasks_DragonScaleExitSwim(void* player) {
    MmForm_DragonScaleExitSwim((Player*)player);
}

u8 TransformMasks_IsItemAllowed(s32 item) {
    return MmForm_IsItemAllowed(item);
}

u8 TransformMasks_IsSlotAllowed(u8 slot) {
    return MmForm_IsSlotAllowed(slot);
}

MmPlayerTransformation MmPlayer_GetForm(void) {
    return MmForm_GetCurrentForm();
}

u8 TransformMasks_OnWaterSwimAttempt(PlayState* play, Player* player) {
    return MmForm_OnWaterSwimAttempt(play, player);
}

f32 TransformMasks_GetFormHeight(void) {
    return MmForm_GetCameraHeight();
}

u8 TransformMasks_BlocksLedgeGrab(void) {
    return MmForm_BlocksLedgeGrab();
}

// =============================================================================
// Boss Super-Damage API (FD / Pikachu Gigantamax → paralyze-or-damage on bosses)
// Header: boss_super_damage.h. Bosses call IsActive() in their hit handler
// and choose their own paralyzed-state condition (typically an actionFunc).
// =============================================================================

// Defined in pikachu_form.cpp (extern "C").
//   gPikaGigantamaxActive — true only during Pikachu's attack/locked-action frames.
//   gPikaGigantamaxMode   — true the whole time Gigantamax is on (persistent).
extern u8 gPikaGigantamaxActive;
extern u8 gPikaGigantamaxMode;
extern u8 gPikaThunderActive;

// Tunable reach distances (world units, before the boss adds its own body slack).
#define BSD_REACH_RANGED 70.0f // FD beam / Pika Thunder — can be farther
#define BSD_REACH_MELEE 30.0f  // FD sword swing / Pika melee — must be near

// SM64 Mario's spin (ACT_TWIRLING) is his boss-room super attack — defined in
// sm64_mario.c (#included into z_player.c). extern'd here to avoid pulling the
// whole SM64 header into the masks TU.
extern u8 Sm64Mario_IsSuperAttacking(void);
// Fire Flower fireballs (sm64_mario_items.c) — a fireball in flight / near a part
// makes the fire count as a super attack so it can break/kill bosses.
extern u8 Sm64Mario_FireballActive(void);
extern u8 Sm64Mario_FireballNear(Vec3f* pos, f32 range);

// NEI Real Master Sword: at FULL health the Master Sword fires the FD thunder beam, so it counts
// as a super attack just like Fierce Deity — the reworked bosses take super damage from the beam
// (and from a full-health Master Sword hit). Persistent while the conditions hold; the bosses AND
// this with an actual AC_HIT / reach test, so merely standing near a boss never triggers it.
static u8 BsdRealMasterSwordActive(PlayState* play) {
    Player* player = (play != NULL) ? GET_PLAYER(play) : NULL;
    if (player == NULL) {
        return 0;
    }
    return WeaponUpgrade_HasTrueMaster() && (player->heldItemAction == PLAYER_IA_SWORD_MASTER) &&
                   (gSaveContext.health >= gSaveContext.healthCapacity)
               ? 1
               : 0;
}

u8 BossSuperDamage_IsActive(PlayState* play) {
    // SM64 Mario: the spin counts as a super attack (only bosses query this, so
    // it's implicitly boss-room-gated).
    if (Sm64Mario_IsSuperAttacking()) {
        return 1;
    }
    // NEI Real Master Sword: super attack while swinging at full health (the beam frame).
    if (BsdRealMasterSwordActive(play)) {
        Player* msPlayer = GET_PLAYER(play);
        if (msPlayer != NULL && msPlayer->meleeWeaponState != 0) {
            return 1;
        }
    }
    // Fire Flower: a fireball in flight counts as a super attack so the fire can
    // break/kill bosses (gated naturally — fireballs only exist while throwing fire).
    if (Sm64Mario_FireballActive()) {
        return 1;
    }
    // Trident (ext sword 3) charged energy ball: same shape as the fireball above —
    // the PROJECTILE carries the super-attack claim, never Link, so nothing about
    // the player's state can make an ordinary trident swing paralyze a boss. The
    // accessor includes a post-impact grace window (bosses read BUMP_HIT one frame
    // late, after the ball has already died). Skijer's NEI
    if (TridentChargeBall_IsActive()) {
        return 1;
    }
    // Byrna orb (ext sword 1) LAUNCHED at a target: identical shape to the ball
    // above — the orb carries the claim and it is only true while it is actually
    // flying (plus its grace window). An ORBITING orb deliberately does not count,
    // or simply standing next to a boss with the barrier up would paralyze it
    // every frame. Skijer's NEI
    if (ByrnaOrb_IsActive()) {
        return 1;
    }
    // Insect Glaive aerial attacks, and only at a full Kinsect bar. Unlike the orb
    // this claim DOES hang off Link's state, so it is scoped to the three airborne
    // states that actually swing — a ground slash can never reach it. Declared here
    // because extended_equipment.h is not in this TU. Skijer's NEI
    extern u8 ByrnaIg_AirSuperDamage(void);
    if (ByrnaIg_AirSuperDamage()) {
        return 1;
    }
    // AND the gPika* mirror with the authoritative form-state check so a latched
    // flag (left over from a previous Pika session) can never fire in normal play.
    if (MmForm_IsPikachuActive() && gPikaGigantamaxActive) {
        return 1;
    }
    // FD form: only count it as a super attack while the melee weapon is
    // actively swinging. Idle FD walking past a boss must NOT paralyze it.
    if (MmForm_IsFDSkinMode()) {
        Player* player = GET_PLAYER(play);
        if (player != NULL && player->meleeWeaponState != 0) {
            return 1;
        }
    }
    return 0;
}

u8 BossSuperDamage_IsFormActive(PlayState* play) {
    // Persistent: true the whole time the player is in FD form or Pika Gigantamax
    // mode, regardless of whether a swing is active this exact frame. For
    // contact/AT-based boss triggers where the hit is read one frame late.
    // NEI Real Master Sword counts the whole time the full-health beam can be in flight.
    if (BsdRealMasterSwordActive(play)) {
        return 1;
    }
    // gPikaGigantamaxMode is ANDed with the authoritative form-state check: it's
    // only a mirror of sPika.gigantamax refreshed while PikachuForm_Update runs, so
    // it can stay latched at 1 after leaving Pika form (e.g. if Cleanup didn't run).
    // Without this AND, normal play (boomerang/bow) would be treated as a super
    // attack and skip the bosses' real phases — breaking the regression.
    // Trident charge ball counts here too: contact-based boss triggers read the
    // hit a frame late, which is exactly what its grace window covers. The Byrna
    // orb joins for the same reason.
    return (Sm64Mario_IsSuperAttacking() || Sm64Mario_FireballActive() || TridentChargeBall_IsActive() ||
            ByrnaOrb_IsActive() || (MmForm_IsPikachuActive() && gPikaGigantamaxMode) || MmForm_IsFDSkinMode())
               ? 1
               : 0;
}

// True if point p is within `range` of the segment [a,b] (clamped to the ends).
// Uses squared distances so there's no sqrt dependency.
static u8 BsdSegmentWithin(Vec3f* p, Vec3f* a, Vec3f* b, f32 range) {
    f32 abx = b->x - a->x, aby = b->y - a->y, abz = b->z - a->z;
    f32 apx = p->x - a->x, apy = p->y - a->y, apz = p->z - a->z;
    f32 abLen2 = abx * abx + aby * aby + abz * abz;
    f32 t, dx, dy, dz, d2;
    f32 range2 = range * range;

    if (abLen2 < 0.001f) {
        d2 = apx * apx + apy * apy + apz * apz; // degenerate segment = a point
        return (d2 < range2) ? 1 : 0;
    }
    t = (apx * abx + apy * aby + apz * abz) / abLen2;
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    dx = p->x - (a->x + abx * t);
    dy = p->y - (a->y + aby * t);
    dz = p->z - (a->z + abz * t);
    d2 = dx * dx + dy * dy + dz * dz;
    return (d2 < range2) ? 1 : 0;
}

u8 BossSuperDamage_FormAttackReaches(PlayState* play, Vec3f* targetPos, f32 range) {
    Player* player;
    s32 k;

    if (play == NULL || targetPos == NULL) {
        return 0;
    }
    player = GET_PLAYER(play);
    if (player == NULL) {
        return 0;
    }

    // Fire Flower: a fireball within `range` of this part breaks/kills it. Uses the
    // FIREBALL'S position (not the player's), so it only lands where fire actually
    // reaches — independent of the FD/Pika form gate below.
    if (Sm64Mario_FireballNear(targetPos, range)) {
        return 1;
    }

    // Must be in a super form (FD or Pika Gigantamax) — or wielding the full-health Real Master
    // Sword — for ANY of this to fire, so normal play / the boomerang regression is never affected.
    if (!(MmForm_IsPikachuActive() && gPikaGigantamaxMode) && !MmForm_IsFDSkinMode() &&
        !BsdRealMasterSwordActive(play)) {
        return 0;
    }

    // TOUCH: the player's body within `range` of the target — no swing required.
    // "Touching Barinade" breaks/paralyzes/damages it (per user). Covers Pika and
    // FD when near a part (the orbiting baris, the lowered body, the core).
    if (Math_Vec3f_DistXYZ(&player->actor.world.pos, targetPos) < range) {
        return 1;
    }

    // FD RANGED: the long sword blade reaching the target while swinging. FD's
    // blade reaches ~5500 units, so we measure distance from the target to the
    // whole blade SEGMENT (base→tip), not just the endpoints — a target anywhere
    // along the swing, near or far, counts. Geometric, so it lands regardless of
    // whether the boss's bumper accepts FD's toucher dmgFlags.
    if (MmForm_IsFDSkinMode() && player->meleeWeaponState != 0) {
        for (k = 0; k < 3; k++) {
            if (BsdSegmentWithin(targetPos, &player->meleeWeaponInfo[k].base, &player->meleeWeaponInfo[k].tip, range)) {
                return 1;
            }
        }
    }
    return 0;
}

f32 BossSuperDamage_FormAttackRange(PlayState* play) {
    Player* player;

    if (play == NULL) {
        return 0.0f;
    }
    player = GET_PLAYER(play);
    if (player == NULL) {
        return 0.0f;
    }

    // Fire Flower: while a fireball is in flight, give the long ranged reach so the
    // "one attack breaks everything when close" bosses also fall to the fire.
    if (Sm64Mario_FireballActive()) {
        return BSD_REACH_RANGED;
    }

    // Pikachu Gigantamax: only while an attack action is live. Thunder = ranged.
    if (MmForm_IsPikachuActive() && gPikaGigantamaxActive) {
        return gPikaThunderActive ? BSD_REACH_RANGED : BSD_REACH_MELEE;
    }

    // Fierce Deity: only while swinging. The sword fires its energy beam at full
    // health (the ranged "thunder"); otherwise it's a melee-range swing.
    if (MmForm_IsFDSkinMode() && player->meleeWeaponState != 0) {
        u8 fullHealth = (gSaveContext.health >= gSaveContext.healthCapacity);
        return fullHealth ? BSD_REACH_RANGED : BSD_REACH_MELEE;
    }

    // NEI Real Master Sword: the full-health beam is a ranged super attack.
    if (BsdRealMasterSwordActive(play)) {
        return BSD_REACH_RANGED;
    }
    return 0.0f;
}

// Per-super-hit damage the reworked bosses subtract. Pikachu Gigantamax = the max (8, the game's
// top sword-attack value); Fierce Deity = its MM Fierce Deity slash (4). MM FD melee is 4 normal /
// 8 strong (D_8085D09C dmgTransformed fields), but the boss handlers don't distinguish the swing
// type, so the normal value is used for FD. Falls back to 4 if neither form is active (handlers are
// form-gated, so this is just a safe default).
u8 BossSuperDamage_FormDamage(PlayState* play) {
    (void)play;
    if (Sm64Mario_FireballActive()) {
        return 8; // Fire Flower hits hard — kills bosses fast
    }
    if (MmForm_IsPikachuActive() && gPikaGigantamaxMode) {
        return 8;
    }
    // NEI Real Master Sword: same per-hit super damage as Fierce Deity.
    if (BsdRealMasterSwordActive(play)) {
        return 4;
    }
    return 4;
}

void BossSuperDamage_SpawnVfx(PlayState* play, Actor* boss, Vec3f* limbWorldPos, s16 scale, s16 count) {
    s16 i;

    if (limbWorldPos == NULL || boss == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        EffectSsFhgFlash_SpawnShock(play, boss, limbWorldPos, scale, FHGFLASH_SHOCK_ANY_ACTOR);
    }
}

// ─── Light-orb glow (OOT gGanonLightOrbModelDL — Dark Beast Ganon transformation)
//
// This is the EXACT effect drawn on each of Ganon's limbs as Ganondorf transforms
// into the giant beast and his body scales up (z_boss_ganon2.c func_80904D88): a
// soft white/blue light orb billboarded over every limb position (unk_234[15]),
// with random Z-spin. White PRIM = bright core, light-blue ENV = the glow that
// "surrounds the limb" — exactly the look requested.
//
// Two OOT-native DLs (always in oot.otr, no mm.o2r):
//   gGanonLightOrbMaterialDL — sets the I8 glow texture + (PRIM-ENV)*TEXEL+ENV
//                              combiner + CLD (soft additive) render mode. Drawn
//                              ONCE per frame before the orbs.
//   gGanonLightOrbModelDL    — a ~14-unit centered quad. Drawn per limb.
// PRIM/ENV are NOT set by the DLs, so we set them ourselves (white core + blue
// glow). Both DLs are referenced by their OTR path strings; SOH resolves them via
// the DL signature check (no manual texture/vertex loading).

#define BSD_SPARK_SLOTS 8
#define BSD_ORB_MATERIAL_DL "__OTR__overlays/ovl_Boss_Ganon2/gGanonLightOrbMaterialDL"
#define BSD_ORB_MODEL_DL "__OTR__overlays/ovl_Boss_Ganon2/gGanonLightOrbModelDL"

typedef struct {
    Actor* actor;
    s16 timer;
} BsdSparkSlot;

static BsdSparkSlot sBsdSparkSlots[BSD_SPARK_SLOTS];

void BossSuperDamage_StartElectricSparks(Actor* boss, s16 durationFrames) {
    s32 i;
    s32 freeSlot = -1;

    if (boss == NULL || durationFrames <= 0) {
        return;
    }

    // Ganon body-spark SFX — the exact sound from the beast transformation this
    // VFX comes from. Played from the boss on each (re)trigger; the boss's own
    // invincibility-frame gap between hits throttles it into a continuous crackle.
    Audio_PlayActorSound2(boss, NA_SE_EN_GANON_BODY_SPARK);

    for (i = 0; i < BSD_SPARK_SLOTS; i++) {
        if (sBsdSparkSlots[i].actor == boss) {
            sBsdSparkSlots[i].timer = durationFrames;
            return;
        }
        if (sBsdSparkSlots[i].actor == NULL && freeSlot < 0) {
            freeSlot = i;
        }
    }
    if (freeSlot >= 0) {
        sBsdSparkSlots[freeSlot].actor = boss;
        sBsdSparkSlots[freeSlot].timer = durationFrames;
    }
}

void BossSuperDamage_DrawElectricSparks(Actor* boss, PlayState* play, Vec3f* limbsPos, s32 limbCount, f32 scale) {
    s32 slot = -1;
    s32 i;
    GraphicsContext* gfxCtx;
    f32 finalScale;
    s32 alpha;

    if (boss == NULL || limbsPos == NULL || limbCount <= 0 || play == NULL) {
        return;
    }
    for (i = 0; i < BSD_SPARK_SLOTS; i++) {
        if (sBsdSparkSlots[i].actor == boss) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return;
    }
    if (sBsdSparkSlots[slot].timer <= 0) {
        sBsdSparkSlots[slot].actor = NULL;
        return;
    }
    // Alpha fades over the last 20 frames so the burst trails off cleanly.
    alpha = (sBsdSparkSlots[slot].timer >= 20) ? 255 : (sBsdSparkSlots[slot].timer * 255 / 20);
    sBsdSparkSlots[slot].timer--;

    // gGanonLightOrbModelDL's quad is ~14 units. Big orbs (scale ~6 → ~84 units)
    // so they overlap into a continuous electric shell over the body rather than
    // discrete dots. A gentle frame-based pulse makes it "breathe" without the
    // per-frame jitter/flicker the random version had.
    {
        // Math_SinS gives a smooth -1..1 wave; ~0.12 amplitude = subtle pulse.
        f32 pulse = 1.0f + 0.12f * Math_SinS(play->gameplayFrames * 0x0C00);
        finalScale = 6.0f * scale * pulse;
    }

    gfxCtx = play->state.gfxCtx;

    OPEN_DISPS(gfxCtx);

    Gfx_SetupDL_25Xlu(gfxCtx);

    // Set the orb color ONCE (white core + light-blue glow), then run the material
    // DL ONCE to bind the I8 glow texture + combiner — exactly as func_80904D88.
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (u8)alpha); // white core
    gDPSetEnvColor(POLY_XLU_DISP++, 100, 200, 255, 0);                // light-blue glow
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)BSD_ORB_MATERIAL_DL);

    // One big orb per limb. STABLE: positions are the exact joint world-pos (no
    // random offset) and the rotation is a fixed per-orb angle (no per-frame
    // random) — that's what makes the glow continuous instead of flickering. The
    // orb translates to the joint, billboards to the camera (ReplaceRotation keeps
    // the translation), scales, and tilts a fixed amount.
    for (i = 0; i < limbCount; i++) {
        Matrix_Translate(limbsPos[i].x, limbsPos[i].y, limbsPos[i].z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(finalScale, finalScale, finalScale, MTXMODE_APPLY);
        Matrix_RotateZ(i * 0.7f, MTXMODE_APPLY); // fixed per-orb tilt, no flicker

        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)BSD_ORB_MODEL_DL);
    }

    CLOSE_DISPS(gfxCtx);
}

// Build spark anchors from a JntSph collider's world-sphere centers, then draw the
// glow. Collapses the identical per-boss copy loop (Fd, Fd2, Dodongo, Sst, ...).
void BossSuperDamage_DrawGlowFromSpheres(Actor* boss, PlayState* play, ColliderJntSph* collider, s32 sphereCount,
                                         f32 scale) {
    Vec3f anchors[32]; // headroom; the largest reworked boss collider is 19 spheres
    s32 i;

    if (collider == NULL || sphereCount <= 0) {
        return;
    }
    if (sphereCount > 32) {
        sphereCount = 32;
    }
    for (i = 0; i < sphereCount; i++) {
        anchors[i].x = collider->elements[i].dim.worldSphere.center.x;
        anchors[i].y = collider->elements[i].dim.worldSphere.center.y;
        anchors[i].z = collider->elements[i].dim.worldSphere.center.z;
    }
    BossSuperDamage_DrawElectricSparks(boss, play, anchors, sphereCount, scale);
}

// =============================================================================
// MM Mask Wearing Routing (to mm_mask_wear.cpp)
// =============================================================================

void TransformMasks_WearToggle(PlayState* play, Player* player, s32 itemId) {
    MmMaskWear_Toggle(play, player, itemId);
}

void TransformMasks_WearDraw(PlayState* play, Player* player) {
    // Only draw worn MM mask for the real local player.
    // Remote/dummy Player actors share the same PostLimbDraw path but must not
    // render the local player's worn mask on their head.
    if (player != GET_PLAYER(play))
        return;
    MmMaskWear_Draw(play, player);
}

void TransformMasks_WearUpdate(PlayState* play, Player* player) {
    MmMaskWear_Update(play, player);
}

s32 TransformMasks_WearGetCurrent(void) {
    return MmMaskWear_GetCurrent();
}

void TransformMasks_WearClear(void) {
    MmMaskWear_Clear();
}

// =============================================================================
// Raw MmPlayer Accessors (gMmPlayer lives in this TU via z_player.c includes)
// mm_player_form.cpp is a separate TU and cannot access gMmPlayer directly.
// =============================================================================

u32 MmPlayerRaw_GetStateFlags3(void) {
    return gMmPlayer.stateFlags3;
}
f32 MmPlayerRaw_GetSpeedXZ(void) {
    return gMmPlayer.speedXZ;
}

// =============================================================================
// Network Visual State Routing (to mm_player_form.cpp)
// =============================================================================

extern u8 MmForm_GetModelType(void);
extern u32 MmForm_GetStateFlags3(void);
extern f32 MmForm_GetSpeedXZ(void);
extern Vec3s* MmForm_GetJointTable(void);
extern s32 MmForm_GetJointCount(void);
extern s32 MmForm_GetGoronAction(void);
extern u8 MmForm_GetEyeIndex(void);
extern f32 MmForm_GetRollSquash(void);
extern s16 MmForm_GetRollSpikeActive(void);
extern s16 MmForm_GetRollChargeLevel(void);

u8 TransformMasks_GetModelType(void) {
    return MmForm_GetModelType();
}
u32 TransformMasks_GetMmStateFlags3(void) {
    return MmForm_GetStateFlags3();
}
f32 TransformMasks_GetMmSpeedXZ(void) {
    return MmForm_GetSpeedXZ();
}
Vec3s* TransformMasks_GetFormJointTable(void) {
    return MmForm_GetJointTable();
}
s32 TransformMasks_GetFormJointCount(void) {
    return MmForm_GetJointCount();
}
s32 TransformMasks_GetGoronAction(void) {
    return MmForm_GetGoronAction();
}
u8 TransformMasks_GetEyeIndex(void) {
    return MmForm_GetEyeIndex();
}
f32 TransformMasks_GetRollSquash(void) {
    return MmForm_GetRollSquash();
}
s16 TransformMasks_GetRollSpikeActive(void) {
    return MmForm_GetRollSpikeActive();
}
s16 TransformMasks_GetRollChargeLevel(void) {
    return MmForm_GetRollChargeLevel();
}

// Route to MmForm_GetFDHandDL for sword beam (FD_DL_SWORD_BEAM = 4)
extern Gfx* MmForm_GetFDSwordBeamDL(PlayState* play);
Gfx* TransformMasks_GetFDSwordBeamDL(PlayState* play) {
    return MmForm_GetFDSwordBeamDL(play);
}

f32 TransformMasks_GetItemScale(void) {
    if (TransformMasks_IsFDSkinMode()) {
        return 1.5f; // FD actor.scale = 0.015f vs standard 0.01f
    }
    return 1.0f;
}
