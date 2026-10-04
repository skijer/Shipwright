/* The diagnostic runner inserts the production initialization/culling functions. */
#include <math.h>
#include <string.h>

#include "tests/test_require.h"
#include "src/overlays/actors/ovl_En_Viewer/z_en_viewer.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_actor.h"

static int drawMultiplier = 1;
static int widescreen = 0;

int32_t CVarGetInteger(const char* name, int32_t fallback) {
    if (strcmp(name, CVAR_ENHANCEMENT("DisableDrawDistance")) == 0)
        return drawMultiplier;
    if (strcmp(name, CVAR_ENHANCEMENT("WidescreenActorCulling")) == 0)
        return widescreen;
    return fallback;
}

float OTRGetAspectRatio(void) {
    return 16.0f / 9.0f;
}

void ActorCatalogue_LogLifecycle(const char* stage, int params, int type, int pose, int modelId, int modelSlot,
                                 int animationId, int animationSlot) {
}

void osSyncPrintfUnused(const char* format, ...) {
}

void EnViewerStatic_WaitForObjects(EnViewer* actor, PlayState* play) {
}

/* PRODUCTION_CULLING_FUNCTIONS */

static EnViewer placement(PlayState* play, s16 params) {
    EnViewer actor = { 0 };
    actor.actor.id = ACTOR_EN_VIEWER;
    actor.actor.params = params;
    actor.actor.room = 7;
    actor.actor.world.pos = (Vec3f){ 175, 40, -310 };
    actor.actor.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_ATTENTION_ENABLED;
    actor.actor.uncullZoneForward = 1000.0f;
    actor.actor.uncullZoneScale = 300.0f;
    actor.actor.uncullZoneDownward = 700.0f;
    EnViewerStatic_Init(&actor, play);
    return actor;
}

static void expectDraw(PlayState* play, Actor* actor, Vec3f position, float w, bool expected) {
    Actor before = *actor;
    bool draw = false, update = false;
    Ship_CalcShouldDrawAndUpdate(play, actor, &position, w, &draw, &update);
    REQUIRE(draw == expected && update == expected);
    REQUIRE(memcmp(actor, &before, sizeof(before)) == 0);
}

static void testDefaultRange(PlayState* play) {
    const s16 prefixes[] = { 0x7F00, 0x7E00 };
    unsigned checked = 0;
    for (unsigned prefix = 0; prefix < ARRAY_COUNT(prefixes); ++prefix) {
        for (s16 id = 1; id <= 13; ++id) {
            s16 params = prefixes[prefix] | id;
            if (!StaticStoryActor_IsAvailable(StaticStoryActor_GetType(params)))
                continue;
            EnViewer actor = placement(play, params);
            Vec3f distant = { 0, 0, 4000 }, outside = { 0, 0, 6500 };
            REQUIRE(Actor_CullingVolumeTest(play, &actor.actor, &distant, 4000));
            REQUIRE(!Actor_CullingVolumeTest(play, &actor.actor, &outside, 6500));
            REQUIRE(actor.actor.params == params && actor.actor.room == 7);
            REQUIRE(actor.actor.world.pos.x == 175 && actor.actor.world.pos.y == 40 && actor.actor.world.pos.z == -310);
            REQUIRE(actor.actor.flags & ACTOR_FLAG_UPDATE_CULLING_DISABLED);
            REQUIRE(!(actor.actor.flags & ACTOR_FLAG_DRAW_CULLING_DISABLED));
            REQUIRE(actor.actionFunc == EnViewerStatic_WaitForObjects);
            REQUIRE(!actor.staticState.initialized);
            ++checked;
        }
    }
    REQUIRE(checked >= 20);
    puts(
        "PASS static placements: visible across a 4000-unit view, finite range, placement and loading state preserved");
}

static void testSettings(PlayState* play) {
    const s16 params[] = { 0x7F02, 0x7F0A, 0x7E09, 0x7E08, 0x7E0C, 0x7E0D };
    for (unsigned i = 0; i < ARRAY_COUNT(params); ++i) {
        EnViewer actor = placement(play, params[i]);
        /* Isolate the settings exclusion from the separate default-range change. */
        actor.actor.uncullZoneForward = 6000;
        drawMultiplier = 1;
        widescreen = 0;
        expectDraw(play, &actor.actor, (Vec3f){ 0, 0, 9000 }, 9000, false);
        drawMultiplier = 2;
        expectDraw(play, &actor.actor, (Vec3f){ 0, 0, 9000 }, 9000, true);
        expectDraw(play, &actor.actor, (Vec3f){ 0, 0, 13000 }, 13000, false);
        drawMultiplier = 1;
        expectDraw(play, &actor.actor, (Vec3f){ 1600, 0, 1000 }, 1000, false);
        widescreen = 1;
        expectDraw(play, &actor.actor, (Vec3f){ 1600, 0, 1000 }, 1000, true);
        expectDraw(play, &actor.actor, (Vec3f){ 5000, 0, 1000 }, 1000, false);
        expectDraw(play, &actor.actor, (Vec3f){ 0, 0, -400 }, 1, false);
    }
    puts("PASS placed Malon, salesman, Skull Kid and Lulu: distance/widescreen settings work; frustum culling remains");
}

static void testLegacyActors(PlayState* play) {
    drawMultiplier = 5;
    widescreen = 1;
    Actor actor = { 0 };
    actor.id = ACTOR_EN_VIEWER;
    actor.uncullZoneForward = 1000;
    actor.uncullZoneScale = 300;
    actor.uncullZoneDownward = 700;
    for (unsigned type = 0; type <= 9; ++type) {
        actor.params = type << 8;
        expectDraw(play, &actor, (Vec3f){ 0, 0, 500 }, 500, true);
        expectDraw(play, &actor, (Vec3f){ 0, 0, 4000 }, 4000, false);
        expectDraw(play, &actor, (Vec3f){ 1600, 0, 1000 }, 1000, false);
    }
    actor.id = ACTOR_EN_MA1;
    actor.params = 0x7F02;
    expectDraw(play, &actor, (Vec3f){ 0, 0, 4000 }, 4000, true);
    puts("PASS original cutscene exclusions and ordinary NPC settings remain unchanged");
}

int main(int argc, char** argv) {
    static PlayState play;
    REQUIRE(argc == 2);
    if (strcmp(argv[1], "default") == 0)
        testDefaultRange(&play);
    else if (strcmp(argv[1], "settings") == 0)
        testSettings(&play);
    else if (strcmp(argv[1], "legacy") == 0)
        testLegacyActors(&play);
    else
        REQUIRE(false);
    return 0;
}
