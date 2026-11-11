#ifndef Z64ITEMS_Z64ITEMDRAW_H
#define Z64ITEMS_Z64ITEMDRAW_H

#include "functions.h"
#include "macros.h"

// For an item without a 3D model: its 32x32 RGBA32 icon on a camera-facing quad.
static inline void Z64Items_DrawIconBillboard(PlayState* play, const char* iconPath) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Vtx* vertices = Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
    vertices[0] = (Vtx)VTX(-12, 12, 0, 0, 0, 255, 255, 255, 255);
    vertices[1] = (Vtx)VTX(12, 12, 0, 1024, 0, 255, 255, 255, 255);
    vertices[2] = (Vtx)VTX(-12, -12, 0, 0, 1024, 255, 255, 255, 255);
    vertices[3] = (Vtx)VTX(12, -12, 0, 1024, 1024, 255, 255, 255, 255);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
    gDPLoadTextureBlock(POLY_XLU_DISP++, iconPath, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0, G_TX_CLAMP, G_TX_CLAMP,
                        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPVertex(POLY_XLU_DISP++, (uintptr_t)vertices, 4, 0);
    gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 1, 3, 2, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

#endif
