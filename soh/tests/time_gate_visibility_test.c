// Exercise the actual Time Gate model and portal draw entry points. Graphics
// submission and resource loading are engine boundaries; no visibility policy
// is reimplemented in this fixture.
#include "global.h"
#include "mods/items/custom_items.h"
#include "mods/items/logic/item_time_gate.h"
#include "soh/Enhancements/randomizer/NeiHeldPresentation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
CustomItemState gCustomItemState;
static Player player;
static PlayState play;
static GraphicsContext graphics;
static Gfx commands[128], model[1];
static Mtx matrix;
static int modelDraws, portalDraws, useApproved;
static float portalScale, portalAlpha;
u8 ResourceMgr_FileExists(const char* path) {
    return 1;
}
Gfx* ResourceMgr_LoadGfxByName(const char* path) {
    return model;
}
bool NeiHeld_HasResources(const char* opaque, const char* translucent) {
    return useApproved;
}
bool NeiHeld_DrawModel(PlayState* p, const char* opaque, const char* translucent) {
    if (!useApproved)
        return false;
    modelDraws++;
    return true;
}
void gSPDisplayList(Gfx* packet, Gfx* list) {
    modelDraws++;
}
void Graph_OpenDisps(Gfx** refs, GraphicsContext* context, const char* file, s32 line) {
}
void Graph_CloseDisps(Gfx** refs, GraphicsContext* context, const char* file, s32 line) {
}
void FrameInterpolation_RecordOpenChild(const void* a, int b) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void Gfx_SetupDL_25Opa(GraphicsContext* context) {
}
void Matrix_Translate(f32 x, f32 y, f32 z, u8 mode) {
}
void Matrix_RotateX(f32 angle, u8 mode) {
}
void Matrix_RotateY(f32 angle, u8 mode) {
}
void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode) {
}
Mtx* Matrix_NewMtx(GraphicsContext* context, char* file, s32 line) {
    return &matrix;
}
f32 Math_SinS(s16 angle) {
    return 0;
}
f32 Math_CosS(s16 angle) {
    return 1;
}
void NeiUsedMagic_DrawPortal(PlayState* p, Player* a, float scale, float alpha) {
    portalDraws++;
    portalScale = scale;
    portalAlpha = alpha;
}
int main(void) {
    play.state.gfxCtx = &graphics;
    for (int approved = 0; approved <= 1; approved++) {
        useApproved = approved;
        // The visibility latch is set at the portal-start animation frame and
        // remains set while the player hovers over the confirmation textbox.
        const int states[] = { TGATE_STATE_CASTING, TGATE_STATE_HOVERING, TGATE_STATE_SWITCHING, TGATE_STATE_CANCEL };
        for (int i = 0; i < 4; i++) {
            memset(&gCustomItemState, 0, sizeof(gCustomItemState));
            graphics.polyOpa.p = commands;
            modelDraws = portalDraws = 0;
            tgActive = 1;
            tgState = states[i];
            tgItemVisible = 1;
            tgPromptShown = states[i] == TGATE_STATE_HOVERING;
            tgPortalActive = 1;
            tgPortalAlpha = 160;
            tgPortalScale = .75f;
            CustomItems_DrawTimeGate(&player, &play);
            assert(modelDraws == 0 && graphics.polyOpa.p == commands);
            CustomItems_DrawTimeGatePortal(&player, &play);
            assert(portalDraws == 1 && portalScale == .75f && portalAlpha == 160);
        }
        // Finish/cancel releases tgActive: this must not permanently suppress
        // ordinary visibility requests outside the activation sequence.
        tgActive = 0;
        tgState = TGATE_STATE_IDLE;
        tgPromptShown = 0;
        graphics.polyOpa.p = commands;
        modelDraws = 0;
        CustomItems_DrawTimeGate(&player, &play);
        assert(modelDraws == 1);
        // Existing visibility-off behavior still prevents drawing after Stop.
        tgItemVisible = 0;
        modelDraws = 0;
        CustomItems_DrawTimeGate(&player, &play);
        assert(modelDraws == 0);
    }
    puts("PASS: Time Gate model hidden throughout activation/dialogue, portal preserved, normal eligibility restored");
}
