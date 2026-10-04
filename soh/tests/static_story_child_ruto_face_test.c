/* The runner inserts the production Child Ruto draw functions unchanged. */
#include <string.h>

#include "tests/test_require.h"
#include "src/overlays/actors/ovl_En_Viewer/z_en_viewer.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_actor.h"
#include "objects/object_ru1/object_ru1.h"

Gfx D_80116280[3];
static Gfx commands[16];
static unsigned drawCalls;
static const char* expectedEye;

void FrameInterpolation_RecordOpenChild(const void* source, int line) {
}

void FrameInterpolation_RecordCloseChild(void) {
}

/* Record the actual GBI segment commands; resource/GPU execution is the boundary. */
void gSPSegment(void* command, int segment, uintptr_t target) {
    __gSPSegment((Gfx*)command, segment, target);
}

static uintptr_t segmentAtDraw(const PlayState* play, unsigned segment) {
    uintptr_t address = 0;
    for (const Gfx* command = commands; command < play->state.gfxCtx->polyOpa.p; ++command) {
        if (command->words.w0 == (0xDB060000u | (segment * 4))) {
            address = command->words.w1;
        }
    }
    return address;
}

static s32 EnViewer_StaticChildRutoOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                                    void* thisx);

void SkelAnime_DrawSkeletonOpa(PlayState* play, SkelAnime* skelAnime, OverrideLimbDrawOpa overrideLimbDraw,
                               PostLimbDrawOpa postLimbDraw, void* actorPointer) {
    EnViewer* actor = actorPointer;
    const char* eye = (const char*)segmentAtDraw(play, 0x08);
    const char* mouth = (const char*)segmentAtDraw(play, 0x09);

    REQUIRE(skelAnime == &actor->skin.skelAnime);
    REQUIRE(overrideLimbDraw == EnViewer_StaticChildRutoOverrideLimbDraw);
    REQUIRE(postLimbDraw == NULL);
    REQUIRE(eye != NULL && strcmp(eye, expectedEye) == 0);
    /* Native object_ru1 reads its mouth from 0x09000000, unlike adult object_ru2. */
    if (mouth == NULL || strcmp(mouth, gRutoChildMouthClosedTex) != 0) {
        fprintf(stderr, "Child Ruto mouth segment 0x09 resolves to %s (eye state %u, pose %u)\n",
                mouth == NULL ? "unbound" : mouth, actor->staticState.eyeIndex, actor->staticState.pose);
        REQUIRE(false);
    }
    REQUIRE(segmentAtDraw(play, 0x0C) == (uintptr_t)&D_80116280[2]);
    ++drawCalls;
}

/* PRODUCTION_CHILD_RUTO_DRAW */

int main(void) {
    static PlayState play;
    GraphicsContext gfx = { 0 };
    const char* eyes[] = { gRutoChildEyeOpenTex, gRutoChildEyeHalfTex, gRutoChildEyeClosedTex };
    play.state.gfxCtx = &gfx;

    for (unsigned pose = 0; pose < 3; ++pose) {
        for (unsigned eye = 0; eye < 3; ++eye) {
            EnViewer actor = { 0 };
            actor.staticState.type = STATIC_STORY_ACTOR_CHILD_RUTO;
            actor.staticState.pose = pose;
            actor.staticState.eyeIndex = eye;
            actor.actor.params = 0x7F07 | (pose << 4);
            EnViewer before = actor;
            expectedEye = eyes[eye];
            gfx.polyOpa.p = commands;
            /* A preceding actor's face must not leak into this placement. */
            gSPSegment(gfx.polyOpa.p++, 0x08, (uintptr_t) "previous actor eye");
            gSPSegment(gfx.polyOpa.p++, 0x09, (uintptr_t) "previous actor mouth");
            EnViewer_DrawStaticChildRuto(&actor, &play);
            REQUIRE(memcmp(&actor, &before, sizeof(actor)) == 0);
        }
    }
    REQUIRE(drawCalls == 9);
    puts("PASS Child Ruto: native mouth binding, all three blink states/poses, symbolic assets, actor state preserved");
    return 0;
}
