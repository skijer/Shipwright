/* The verifier inserts the unmodified production initializer and draw function.
 * This catches Tatl's pale core, an incorrect halo, or a transient segment-08
 * setup reaching the renderer. Matrix and skeleton traversal are boundaries;
 * the existing matrix fixture separately exercises the real body callback. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/overlays/actors/ovl_En_Viewer/z_en_viewer.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h"
#include "tests/test_require.h"

static Gfx commands[2][8];
static Gfx transient[2][8];
static unsigned frame;
static unsigned drawCalls;
static const Gfx* stableSetup;
static Gfx savedSetup[4];
static u8 expectedAlpha;

void FrameInterpolation_RecordOpenChild(const void* file, int line) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void gSPSegment(void* value, int segment, uintptr_t target) {
    __gSPSegment((Gfx*)value, segment, target);
}
void Gfx_SetupDL_27Xlu(GraphicsContext* gfx) {
}
void Matrix_Push(void) {
}
void Matrix_Pop(void) {
}
void Matrix_Translate(f32 x, f32 y, f32 z, u8 mode) {
}
void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode) {
}
f32 Math_SinS(s16 angle) {
    return sinf((float)angle * (3.14159265358979323846f / 32768.0f));
}
f32 Math_CosS(s16 angle) {
    return cosf((float)angle * (3.14159265358979323846f / 32768.0f));
}
void* Graph_Alloc(GraphicsContext* gfx, size_t size) {
    REQUIRE(size <= sizeof(transient[0]));
    return transient[frame % 2];
}
static s32 EnViewer_StaticTatlOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                               void* actor, Gfx** gfx) {
    REQUIRE(false); /* The skeleton boundary must not execute callbacks. */
    return false;
}

Gfx* SkelAnime_Draw(PlayState* play, void** skeleton, Vec3s* joints, OverrideLimbDraw override, PostLimbDraw post,
                    void* actor, Gfx* output) {
    Gfx* emitted = commands[frame % 2];
    const Gfx* setup;
    ++drawCalls;
    REQUIRE(output == emitted + 2);
    REQUIRE((emitted[0].words.w0 >> 24) == G_MOVEWORD);
    REQUIRE((emitted[0].words.w0 & 0xFFFFFF) == ((G_MW_SEGMENT << 16) | (8 * 4)));
    setup = (const Gfx*)emitted[0].words.w1;
    REQUIRE(setup != NULL && ((uintptr_t)setup & 1) == 0);

    /* MM Dm_Char00 palette index 1: purple core and red-orange halo.
     * https://github.com/zeldaret/mm/blob/main/src/overlays/actors/ovl_Dm_Char00/z_dm_char00.c */
    REQUIRE(setup[0].words.w0 == 0xE7000000U && setup[0].words.w1 == 0);
    REQUIRE(setup[1].words.w0 == 0xFA000001U);
    REQUIRE(setup[1].words.w1 == 0x3F125DFFU);
    REQUIRE((setup[2].words.w0 >> 24) == G_SETOTHERMODE_L);
    REQUIRE(setup[2].words.w1 == (G_RM_PASS | G_RM_ZB_CLD_SURF2));
    REQUIRE(setup[3].words.w0 == 0xDF000000U && setup[3].words.w1 == 0);
    REQUIRE(emitted[1].words.w0 == 0xFB000000U);
    REQUIRE(emitted[1].words.w1 == (0xFA280A00U | expectedAlpha));

    if (stableSetup == NULL) {
        stableSetup = setup;
        memcpy(savedSetup, setup, sizeof(savedSetup));
    }
    REQUIRE(setup == stableSetup);
    REQUIRE(memcmp(savedSetup, setup, sizeof(savedSetup)) == 0);
    return output;
}

/* PRODUCTION_TAEL_SETUP */
/* PRODUCTION_TAEL_DRAW */

int main(void) {
    static PlayState play;
    static GraphicsContext graphics;
    EnViewer actor = { 0 };
    const u16 phases[] = { 0, 0x4000, 0x8000, 0xC000 };
    const u8 alphas[] = { 160, 240, 160, 240 };
    play.state.gfxCtx = &graphics;

    for (frame = 0; frame < 8; ++frame) {
        memset(commands, 0xA5, sizeof(commands));
        memset(transient, 0x5A, sizeof(transient));
        graphics.polyXlu.p = commands[frame % 2];
        actor.staticState.pose = frame / 4;
        actor.staticState.tatlPulsePhase = phases[frame % 4];
        expectedAlpha = alphas[frame % 4];
        EnViewer_DrawStaticTatl(&actor, &play);
        REQUIRE(graphics.polyXlu.p == commands[frame % 2] + 2);
        /* Recycled frame graphics must not invalidate the replayed setup. */
        memset(transient, 0xCC, sizeof(transient));
        REQUIRE(memcmp(savedSetup, stableSetup, sizeof(savedSetup)) == 0);
    }
    REQUIRE(drawCalls == 8);
    puts("PASS: production draw emits Tael core/halo and pulse with stable segment-08 storage");
    return 0;
}
