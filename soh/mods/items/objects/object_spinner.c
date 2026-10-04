/**
 * object_spinner.c - Spinner 3D model and draw functions
 *
 * Draws the spinner when riding and during tricks.
 * Model: Custom DL in spinner_giveDL/
 */
#include "z64.h"
#include "../custom_items.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "soh/Enhancements/randomizer/NeiHeldPresentation.h"

// Spinner 3D model lives in soh.o2r (object_nei_spinner). No inline C model.
extern u8 ResourceMgr_FileExists(const char* resName);
extern Gfx* ResourceMgr_LoadGfxByName(const char* path);

// ============================================================================
// DRAW FUNCTION CALLER
// ============================================================================

void CustomItems_DrawSpinner(Player* this, PlayState* play) {
    if (gCustomItemState.spinnerActive) {
        // The new mesh is authored around its riding deck. The player already
        // follows the unchanged hover/attack/homing heights, so this frame also
        // carries the platform through the complete movement without reapplying
        // the old export's 150-unit origin correction.
        u8 replacementDrawn;
        s16 spinRot = (s16)(play->gameplayFrames * 0x800);
        Matrix_Push();
        Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
        // Matrix_RotateY takes radians; the intended 0x800 step is 1/32 turn.
        Matrix_RotateY(BINANG_TO_RAD(spinRot), MTXMODE_APPLY);
        Matrix_Scale(0.20f, 0.20f, 0.20f, MTXMODE_APPLY);
        replacementDrawn = NeiHeld_DrawModel(play, NEI_HELD_PATH("spinner"), NULL);
        Matrix_Pop();
        if (replacementDrawn) {
            return;
        }

        static Gfx* sDL = NULL;
        static u8 sTried = 0;
        if (!sTried) {
            sTried = 1;
            const char* otr = "__OTR__objects/object_nei_spinner/n0b0_opaque_dl";
            if (ResourceMgr_FileExists(otr)) {
                sDL = ResourceMgr_LoadGfxByName(otr);
            }
        }
        if (sDL == NULL)
            return;

        OPEN_DISPS(play->state.gfxCtx);

        // Position the spinner at the player's location
        Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);

        // Constant rotation calculation
        Matrix_RotateY(spinRot, MTXMODE_APPLY);

        // Handle scaling logic (Expand when attacking)
        f32 baseScale = 0.20f;
        Matrix_Scale(baseScale, baseScale, baseScale, MTXMODE_APPLY);

        // New model origin is ~150 units higher than old; shift down to match
        Matrix_Translate(0.0f, -150.0f, 0.0f, MTXMODE_APPLY);

        // Apply the generated matrix to the display list
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, sDL);

        CLOSE_DISPS(play->state.gfxCtx);
    }
}
