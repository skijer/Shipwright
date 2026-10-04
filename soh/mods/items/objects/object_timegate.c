#include "soh/Enhancements/randomizer/NeiUsedMagicPresentation.h"
#include "soh/Enhancements/randomizer/NeiHeldPresentation.h"
/**
 * object_timegate.c - Time Gate draw functions
 *
 * Keeps the item model hidden during activation and draws the blue warp portal
 * effect on the ground.
 */

#include "z64.h"
#include "../custom_items.h"
#include "../logic/item_time_gate.h"
#include "macros.h"
#include "functions.h"

// Time-gate model now in soh.o2r (object_nei_time_gate). Cached gated load.
extern u8 ResourceMgr_FileExists(const char* resName);
extern Gfx* ResourceMgr_LoadGfxByName(const char* path);

static Gfx* TimeGate_GetDL(void) {
    static Gfx* sDL = NULL;
    static u8 sTried = 0;
    if (!sTried) {
        sTried = 1;
        const char* otr = "__OTR__objects/object_nei_time_gate/g_timegate_dl";
        if (ResourceMgr_FileExists(otr)) {
            sDL = ResourceMgr_LoadGfxByName(otr);
        }
    }
    return sDL;
}

/**
 * Draw the item only outside its activation sequence.
 */
void CustomItems_DrawTimeGate(Player* player, PlayState* play) {
    // Casting sets this visibility flag as the portal-start latch and retains
    // it through the confirmation dialogue. Keep that timing intact, but hide
    // the physical model for the whole activation, including cancel/exit.
    if (tgActive || !tgItemVisible)
        return;
    if (!NeiHeld_HasResources(NEI_HELD_PATH("time_gate"), NULL) && TimeGate_GetDL() == NULL)
        return;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    // Position at Link's left hand (he's placing the item)
    Vec3f handPos = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];

    f32 forwardOffset = 5.0f;
    f32 downOffset = -10.0f; // Lower it towards the ground

    Matrix_Translate(handPos.x + Math_SinS(player->actor.shape.rot.y) * forwardOffset, handPos.y + downOffset,
                     handPos.z + Math_CosS(player->actor.shape.rot.y) * forwardOffset, MTXMODE_NEW);

    // Rotate to face forward and tilt slightly
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(-0x1000), MTXMODE_APPLY); // Tilt forward

    // Scale appropriate for hand-held size
    Matrix_Scale(0.008f, 0.008f, 0.008f, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    if (!NeiHeld_DrawModel(play, NEI_HELD_PATH("time_gate"), NULL)) {
        gSPDisplayList(POLY_OPA_DISP++, TimeGate_GetDL());
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

/** Draw temporal energy using the existing portal growth/fade state. */
void CustomItems_DrawTimeGatePortal(Player* player, PlayState* play) {
    if (!tgPortalActive || tgPortalAlpha <= 0.0f)
        return;
    NeiUsedMagic_DrawPortal(play, player, tgPortalScale, tgPortalAlpha);
}
