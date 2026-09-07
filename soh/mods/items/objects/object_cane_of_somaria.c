/**
 * object_cane_of_somaria.c - Cane of Somaria 3D model and draw functions
 */

#include "z64.h"
#include "../custom_items.h"
#include "../logic/item_cane_of_somaria.h"
#include "../../actors/cane_pacci.h"
#include "macros.h"
#include "functions.h"
#include <math.h>
// 3D model lives in soh.o2r: objects/object_somaria/g_somaria_cane_dl (XML emitted
// by apps/dl_c_to_xml.py, packed via rebuild_soh_otr.bat). No inline C model.
// ResourceMgr_LoadGfxByName crashes on a missing path, so gate with FileExists;
// returns NULL (cane simply not drawn) if the archive lacks the object.
extern u8 ResourceMgr_FileExists(const char* resName);
extern Gfx* ResourceMgr_LoadGfxByName(const char* path);

static Gfx* Somaria_GetHandDL(void) {
    static Gfx* sCached = NULL;
    static u8 sTried = 0;
    if (!sTried) {
        sTried = 1;
        const char* otr = "__OTR__objects/object_somaria/g_somaria_cane_dl";
        if (ResourceMgr_FileExists(otr)) {
            sCached = ResourceMgr_LoadGfxByName(otr);
        }
    }
    return sCached;
}

void CustomItems_DrawCaneOfSomaria(Player* player, PlayState* play) {
    // Ultrahand is a gesture with no visible cane, so its energy/tint/control UI
    // must draw before the staff's own early-out.
    Pacci_UltrahandDrawVfx(play, player);
    // Ultrahand's Zonai weld beads draw before the cane's own early-out: the mode
    // hides the staff entirely, so anything gated behind shSomariaActive would be
    // invisible exactly when it is needed.
    Pacci_FuseDrawPreview(play);

    if (!shSomariaActive)
        return;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    // Get forearm and hand positions to calculate hand direction
    Vec3f forearmPos = player->bodyPartsPos[PLAYER_BODYPART_R_FOREARM];
    Vec3f handPos = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];

    // Calculate direction vector from forearm to hand
    f32 dx = handPos.x - forearmPos.x;
    f32 dy = handPos.y - forearmPos.y;
    f32 dz = handPos.z - forearmPos.z;

    // Calculate yaw and pitch from direction
    f32 handYaw = atan2f(dx, dz);
    f32 horizDist = sqrtf(dx * dx + dz * dz);
    f32 handPitch = atan2f(dy, horizDist);

    // Position at hand
    Matrix_Translate(handPos.x, handPos.y, handPos.z, MTXMODE_NEW);

    // Apply hand rotation
    Matrix_RotateY(handYaw, MTXMODE_APPLY);
    Matrix_RotateX(-handPitch, MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(0x4000), MTXMODE_APPLY);

    // Offset up in local Y after rotation
    Matrix_Translate(-2.5f, 15.0f, 1.0f, MTXMODE_APPLY);

    Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);

    // Ultrahand is empty-handed: no cane in the hand at all. It is a gesture, not a
    // tool you hold out, so drawing the staff there reads wrong.
    Gfx* handDL = (Cane_GetType() == CANE_TYPE_ULTRAHAND) ? NULL : Somaria_GetHandDL();
    if (handDL != NULL) {
        // Both canes share this display list; only the tint tells them apart —
        // Somaria is red, Pacci is yellow (user-locked). Components are spelled out
        // on purpose: MSVC hands a multi-value #define to a function-like macro as a
        // SINGLE argument, so gDPSetPrimColor would not expand.
        if (Cane_GetType() == CANE_TYPE_PACCI) {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 215, 70, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 150, 105, 0, 255);
        } else {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 60, 60, 255);
            gDPSetEnvColor(POLY_OPA_DISP++, 140, 0, 0, 255);
        }
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, handDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);

    // Flip's cast visual (stub — see pacci_flip_vfx.h).
    PacciFlipVfx_Draw(play, player);

    // Trirod ghost: the selected echo's miniature floating over the landing spot.
    // Same reasoning as the Somaria preview below — this is already the cane's
    // per-frame draw hook, so no new call site.
    if ((Cane_GetType() == CANE_TYPE_TRIROD) && !shSomariaAnimating) {
        Trirod_DrawPreview(play, player);
    }

    // Placement ghost for the aimed summons (Block / Platform). Drawn from here
    // because this is already the cane's per-frame draw hook — no new call site.
    if (Cane_IsAiming() && !shSomariaAnimating) {
        u8 skill = Cane_GetActiveSkill();
        CaneSummonKind kind = (skill == CANE_SKILL_SOMARIA_PLATFORM) ? CANE_SUMMON_PLATFORM : CANE_SUMMON_BLOCK;
        CaneSummon_DrawPreview(play, kind, &canePreviewPos, canePreviewYaw, canePreviewValid);
    }
}
