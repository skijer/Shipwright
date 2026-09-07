// =============================================================================
// Quartz of Motion — kaleido glue (OoT side).
//
// Level 2 of the progressive Stone of Agony. Pressing A on the Stone of Agony
// quest slot opens a modal list of tracking categories; confirming one spends a
// heart container and runs the sensor for 5 minutes.
//
// Split of responsibilities, mirroring the 2ship implementation:
//   - INPUT  lives here (and in the kaleido state machine, via unk_1E4 == 11)
//   - PIXELS live in soh/Enhancements/randomizer/DesireCompassHud.cpp (ImGui)
//   - BRAIN  lives in soh/Enhancements/randomizer/DesireCompass.cpp
//
// Only two lines are added to the kaleido overlays themselves:
//   z_kaleido_collect.c   -> Quartz_TryOpenAtCursor(play, input)   (opens it)
//   z_kaleido_scope_PAL.c -> case 11: Quartz_UpdateModal(play, input)  (drives it)
// This follows the same "extern + one call" pattern as the spiritual stones
// (mods/spiritual_stones/spiritual_stones.cpp).
// =============================================================================

#include "soh/Enhancements/randomizer/DesireCompass.h"
#include "mods/nei_save.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
}

// The kaleido sub-state we own while the list is up. Picked one past the
// engine's highest (10 = C-Up description textbox) so no vanilla branch runs —
// crucially, the state-0 branch that closes the pause menu on B/START.
#define QUARTZ_KALEIDO_SUBSTATE 11

namespace {
bool sListOpen = false;
s32 sListIndex = 0;
bool sStickHeld = false;
} // namespace

// --- Read by the ImGui overlay ------------------------------------------------

extern "C" u8 Quartz_IsListOpen(void) {
    return sListOpen ? 1 : 0;
}

extern "C" s32 Quartz_GetListIndex(void) {
    return sListIndex;
}

// --- Open: A on the Stone of Agony slot ---------------------------------------

extern "C" s32 Quartz_TryOpenAtCursor(PlayState* play, Input* input) {
    if (sListOpen) {
        return false;
    }
    if (!CHECK_BTN_ALL(input->press.button, BTN_A)) {
        return false;
    }
    if (play->pauseCtx.cursorPoint[PAUSE_QUEST] != QUEST_STONE_OF_AGONY) {
        return false;
    }
    if (!CHECK_QUEST_ITEM(QUEST_STONE_OF_AGONY)) {
        return false;
    }
    // Without the Quartz the stone is just its passive vanilla self.
    if (!Rando_DesireCompass_IsOwned()) {
        Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return false;
    }

    NeiSaveData* nei = Nei_Save();
    sListIndex = (nei != nullptr) ? (s32)nei->quartzCategory : 0;
    if (sListIndex < 0 || sListIndex >= DCOMPASS_CAT_MAX) {
        sListIndex = 0;
    }
    sListOpen = true;
    sStickHeld = true; // swallow the stick until it recenters
    play->pauseCtx.unk_1E4 = QUARTZ_KALEIDO_SUBSTATE;
    Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return true;
}

// --- Drive: runs every frame while our sub-state is active --------------------

extern "C" void Quartz_UpdateModal(PlayState* play, Input* input) {
    PauseContext* pauseCtx = &play->pauseCtx;

    if (!sListOpen) { // defensive: never strand the menu in our sub-state
        pauseCtx->unk_1E4 = 0;
        return;
    }

    // Vertical nav with debounce so one tilt = one row.
    if ((pauseCtx->stickRelY > 30) || (pauseCtx->stickRelY < -30)) {
        if (!sStickHeld) {
            sListIndex += (pauseCtx->stickRelY > 30) ? -1 : 1;
            if (sListIndex < 0) {
                sListIndex = DCOMPASS_CAT_MAX - 1;
            } else if (sListIndex >= DCOMPASS_CAT_MAX) {
                sListIndex = 0;
            }
            Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            sStickHeld = true;
        }
    } else {
        sStickHeld = false;
    }

    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        if (Rando_DesireCompass_RequestActivation((DesireCompassCategory)sListIndex, DCOMPASS_SUBCAT_ANY)) {
            Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            sListOpen = false;
            sStickHeld = false;
            // Leave the pause menu so the attuning animation plays in-world and
            // the heart is charged there. Same close sequence the B/START path
            // uses (z_kaleido_scope_PAL.c) and that NeiPausePlay_Start copies.
            pauseCtx->state = 0x12;
            WREG(2) = -6240;
            func_800F64E0(0);
            pauseCtx->unk_1E4 = 0;
        } else {
            // Refused: not enough heart capacity (or ownership lost somehow).
            // Keep the list open so the player can see why nothing happened.
            Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
        return;
    }

    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        sListOpen = false;
        sStickHeld = false;
        pauseCtx->unk_1E4 = 0;
        Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

// Called when the pause menu closes, so the modal never survives into gameplay.
extern "C" void Quartz_ResetModal(void) {
    sListOpen = false;
    sStickHeld = false;
}
