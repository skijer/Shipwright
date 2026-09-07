/**
 * object_dekuleaf.c - Deku Leaf 3D model and draw functions
 *
 * Draws the leaf when held and during gliding/swinging.
 * Model: Custom procedural leaf geometry from deku_leaf_giveDL/
 */

#include "z64.h"
#include "../custom_items.h"
#include "../logic/item_dekuleaf.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include <math.h>

// Angle to radians conversion for s16 angles
#define DEKULEAF_ANGLE_TO_RAD (M_PI / 0x8000)

// Leaf model now in soh.o2r (object_nei_deku_leaf). Cached gated load.
extern u8 ResourceMgr_FileExists(const char* resName);
extern Gfx* ResourceMgr_LoadGfxByName(const char* path);

static Gfx* DekuLeaf_GetDL(void) {
    static Gfx* sDL = NULL;
    static u8 sTried = 0;
    if (!sTried) {
        sTried = 1;
        const char* otr = "__OTR__objects/object_nei_deku_leaf/g_dekuleaf_dl";
        if (ResourceMgr_FileExists(otr)) {
            sDL = ResourceMgr_LoadGfxByName(otr);
        }
    }
    return sDL;
}

static void DekuLeaf_SetupGeometryMode(GraphicsContext* gfxCtx) {
    OPEN_DISPS(gfxCtx);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BACK | G_LIGHTING | G_TEXTURE_GEN);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_SHADE, G_CC_SHADE);
    CLOSE_DISPS(gfxCtx);
}

static void DekuLeaf_RestoreGeometryMode(GraphicsContext* gfxCtx) {
    OPEN_DISPS(gfxCtx);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_CULL_BACK | G_LIGHTING);
    CLOSE_DISPS(gfxCtx);
}

static void DekuLeaf_DrawModel(PlayState* play, f32 posX, f32 posY, f32 posZ, s16 rotY, f32 scale) {
    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Translate(posX, posY, posZ, MTXMODE_NEW);
    Matrix_RotateY((rotY * DEKULEAF_ANGLE_TO_RAD) + M_PI, MTXMODE_APPLY);
    // Counter-rotate X to restore original orientation (model vertices are rotated 90deg for giveDL)
    Matrix_RotateX(-M_PI / 2, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, DekuLeaf_GetDL());

    CLOSE_DISPS(play->state.gfxCtx);
}

// Draw with hand direction (for blowing mode) - Y rotation only, no pitch
static void DekuLeaf_DrawModelWithHandDir(PlayState* play, Vec3f* handPos, f32 handYaw, f32 scale) {
    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Translate(handPos->x, handPos->y, handPos->z, MTXMODE_NEW);
    Matrix_RotateY(handYaw + M_PI, MTXMODE_APPLY);
    // Counter-rotate X to restore original orientation (model vertices are rotated 90deg for giveDL)
    Matrix_RotateX(-M_PI / 2, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, DekuLeaf_GetDL());

    CLOSE_DISPS(play->state.gfxCtx);
}

void CustomItems_DrawDekuLeaf(Player* p, PlayState* play) {
    if (!dlGliding && !dlBlowing)
        return;
    if (DekuLeaf_GetDL() == NULL)
        return; // model not packed -> skip rather than draw a NULL DL

    DekuLeaf_SetupGeometryMode(play->state.gfxCtx);

    if (dlGliding) {
        // Gliding: the leaf is a CANOPY held above Link's HANDS (paraglider look), NOT floating at his
        // torso center — that made it clip into his chest as adult/other forms. Anchored to the
        // midpoint of both hands; the offsets below were dialed in live and baked. Skijer's NEI
        Vec3f* lHand = &p->bodyPartsPos[PLAYER_BODYPART_L_HAND];
        Vec3f* rHand = &p->bodyPartsPos[PLAYER_BODYPART_R_HAND];
        s16 rotY = p->actor.shape.rot.y;
        f32 posX = (lHand->x + rHand->x) * 0.5f;
        f32 posY = (lHand->y + rHand->y) * 0.5f + DEKULEAF_GLIDE_HAND_OFFSET;
        f32 posZ = (lHand->z + rHand->z) * 0.5f;

        DekuLeaf_DrawModel(play, posX, posY, posZ, rotY, DEKULEAF_GLIDE_SCALE);
    } else if (dlBlowing) {
        // Blowing: draw attached to LEFT hand with frame-based scale
        // Use forearm-to-hand direction for Y rotation only
        Vec3f forearmPos = p->bodyPartsPos[PLAYER_BODYPART_L_FOREARM];
        Vec3f handPos = p->bodyPartsPos[PLAYER_BODYPART_L_HAND];

        // Calculate direction vector from forearm to hand (Y rotation only)
        f32 dx = handPos.x - forearmPos.x;
        f32 dz = handPos.z - forearmPos.z;
        f32 handYaw = atan2f(dx, dz);

        // Determine scale based on current animation frame
        f32 scale;
        // Keyed off the ANIMATION frame (not a tick counter) so the big-leaf window tracks the swing
        // at any playback speed. Skijer's NEI
        if (p->upperSkelAnime.curFrame >= DEKULEAF_ATTACK_FRAME_START &&
            p->upperSkelAnime.curFrame <= DEKULEAF_ATTACK_FRAME_END) {
            scale = DEKULEAF_ATTACK_SCALE;
        } else {
            scale = DEKULEAF_HOLD_SCALE;
        }

        DekuLeaf_DrawModelWithHandDir(play, &handPos, handYaw, scale);
    }

    DekuLeaf_RestoreGeometryMode(play->state.gfxCtx);
}
