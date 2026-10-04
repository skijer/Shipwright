/* The runner inserts unmodified production viewer/math functions below. */
#include <math.h>
#include <string.h>

#include "tests/test_require.h"
#include "src/overlays/actors/ovl_En_Viewer/z_en_viewer.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_actor.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h"
#include "mods/transformation_masks/assets/mm_asset_loader.h"
#include "soh/ResourceManagerHelpers.h"
#include "align_asset_macro.h"

static unsigned headTrackingCalls;
void Npc_TrackPoint(Actor* actor, NpcInteractInfo* info, s16 preset, s16 mode) {
    if (mode != NPC_TRACKING_NONE)
        ++headTrackingCalls;
}

static float matrixYaw, savedMatrixYaw, drawYaw;
static unsigned matrixDepth, drawCount;
void FrameInterpolation_RecordOpenChild(const void* actor, int id) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void Matrix_Push(void) {
    REQUIRE(matrixDepth++ == 0);
    savedMatrixYaw = matrixYaw;
}
void Matrix_Pop(void) {
    REQUIRE(matrixDepth-- == 1);
    matrixYaw = savedMatrixYaw;
}
void Matrix_RotateY(f32 yaw, u8 mode) {
    REQUIRE(mode == MTXMODE_APPLY);
    matrixYaw += yaw;
}
void Gfx_SetupDL_25Opa(GraphicsContext* context) {
}
void Graph_OpenDisps(Gfx** displayList, GraphicsContext* context, const char* file, s32 line) {
}
void Graph_CloseDisps(Gfx** displayList, GraphicsContext* context, const char* file, s32 line) {
}
#define gSPSegment(command, segment, target) __gSPSegment((Gfx*)(command), (segment), (uintptr_t)(target))
Gfx* MmAssets_GetOpaqueRenderMode(void) {
    REQUIRE(false);
    return NULL;
}
static s32 EnViewer_StaticTreasureChestShopGalOverrideLimbDraw(PlayState* play, s32 limb, Gfx** displayList, Vec3f* pos,
                                                               Vec3s* rot, void* actor) {
    return false;
}
static s32 EnViewer_StaticOrdinaryMmOverrideLimbDraw(PlayState* play, s32 limb, Gfx** displayList, Vec3f* pos,
                                                     Vec3s* rot, void* actor) {
    return false;
}
void SkelAnime_DrawSkeletonOpa(PlayState* play, SkelAnime* skeleton, OverrideLimbDrawOpa override, PostLimbDrawOpa post,
                               void* actor) {
    drawYaw = matrixYaw;
    ++drawCount;
}
void SkelAnime_DrawFlexLod(PlayState* play, void** skeleton, Vec3s* joints, s32 count, OverrideLimbDrawOpa override,
                           PostLimbDrawOpa post, void* actor, s32 lod) {
    REQUIRE(false);
}

/* PRODUCTION_POSE_FUNCTIONS */

static void testBodyTurning(PlayState* play) {
    for (unsigned pose = 0; pose < 2; ++pose) {
        EnViewer actor = { 0 };
        actor.staticState.type = STATIC_STORY_ACTOR_SKULL_KID;
        actor.staticState.pose = pose;
        actor.staticState.tracking = true;
        actor.actor.params = 0x7E08 | (pose << 4);
        actor.actor.home.rot.y = actor.actor.shape.rot.y = actor.actor.world.rot.y = 0x2000;
        actor.actor.yawTowardsPlayer = 0x6000;
        actor.actor.home.pos = actor.actor.world.pos = (Vec3f){ 127, 345, -82 };
        actor.staticState.interactInfo.headRot = (Vec3s){ 11, 22, 33 };
        actor.staticState.interactInfo.torsoRot = (Vec3s){ 44, 55, 66 };

        EnViewerStatic_UpdateTracking(&actor, play);
        REQUIRE(actor.actor.shape.rot.y == 0x2400);
        REQUIRE(actor.actor.world.rot.y == actor.actor.shape.rot.y);
        REQUIRE(actor.actor.home.rot.y == 0x2000);
        REQUIRE(actor.actor.shape.rot.x == 0 && actor.actor.shape.rot.z == 0);
        REQUIRE(actor.actor.world.rot.x == 0 && actor.actor.world.rot.z == 0);
        REQUIRE(actor.actor.params == (0x7E08 | (pose << 4)));
        REQUIRE(memcmp(&actor.actor.world.pos, &actor.actor.home.pos, sizeof(Vec3f)) == 0);
        REQUIRE(actor.staticState.interactInfo.headRot.x == 0 && actor.staticState.interactInfo.headRot.y == 0);
        REQUIRE(actor.staticState.interactInfo.torsoRot.x == 0 && actor.staticState.interactInfo.torsoRot.y == 0);
        for (unsigned tick = 0; tick < 160; ++tick) {
            s16 previous = actor.actor.shape.rot.y;
            EnViewerStatic_UpdateTracking(&actor, play);
            REQUIRE((s16)(actor.actor.shape.rot.y - previous) >= 0);
            REQUIRE((s16)(actor.actor.shape.rot.y - previous) <= 0x400);
        }
        REQUIRE(actor.actor.shape.rot.y == 0x6000);
        actor.staticState.tracking = false;
        EnViewerStatic_UpdateTracking(&actor, play);
        REQUIRE(actor.actor.shape.rot.y == 0x5C00);
        for (unsigned tick = 0; tick < 160; ++tick)
            EnViewerStatic_UpdateTracking(&actor, play);
        REQUIRE(actor.actor.shape.rot.y == 0x2000 && actor.actor.home.rot.y == 0x2000);

        /* Crossing the signed-angle boundary takes the short path. */
        actor.staticState.tracking = true;
        actor.actor.shape.rot.y = actor.actor.world.rot.y = 32700;
        actor.actor.yawTowardsPlayer = -32700;
        EnViewerStatic_UpdateTracking(&actor, play);
        REQUIRE(actor.actor.shape.rot.y == 32734);
        for (unsigned tick = 0; tick < 80; ++tick)
            EnViewerStatic_UpdateTracking(&actor, play);
        REQUIRE(actor.actor.shape.rot.y == -32700 && actor.actor.world.rot.y == -32700);
    }
    REQUIRE(headTrackingCalls == 0);
}

static void testLuluPresentation(PlayState* play) {
    const s16 placementYaw = 0x2800;
    for (unsigned pose = 0; pose < 4; ++pose) {
        EnViewer actor = { 0 };
        Vec3s joints[23], savedJoints[23];
        Gfx commands[16];
        memset(joints, 0x23, sizeof(joints));
        memcpy(savedJoints, joints, sizeof(joints));
        actor.staticState.type = STATIC_STORY_ACTOR_LULU;
        actor.staticState.pose = pose;
        actor.staticState.initialized = true;
        actor.skin.skelAnime.jointTable = joints;
        actor.actor.shape.rot.y = actor.actor.world.rot.y = actor.actor.home.rot.y = placementYaw;
        play->state.gfxCtx->polyOpa.p = commands;
        matrixYaw = placementYaw * (M_PI / 32768.0f);
        float before = matrixYaw;
        unsigned beforeDraw = drawCount;
        EnViewer_DrawStaticMmActor(&actor, play);
        REQUIRE(drawCount == beforeDraw + 1);
        REQUIRE(matrixDepth == 0 && fabsf(matrixYaw - before) < 0.0001f);
        REQUIRE(actor.actor.shape.rot.y == placementYaw && actor.actor.home.rot.y == placementYaw);
        REQUIRE(memcmp(joints, savedJoints, sizeof(joints)) == 0);
        if (pose == 1) {
            /* Native frame 29 left/right hip vector. The other clips' hip axis is +X.
             * This catches missing, reversed, or world-space facing compensation. */
            float yaw = drawYaw - before;
            float x = -138.20035f * cosf(yaw) + -479.07670f * sinf(yaw);
            float z = 138.20035f * sinf(yaw) + -479.07670f * cosf(yaw);
            REQUIRE(fabsf(atan2f(-z, x)) < 0.001f);
        } else {
            REQUIRE(fabsf(drawYaw - before) < 0.0001f);
        }
    }
}

int main(void) {
    static PlayState play;
    static Player player;
    static GraphicsContext gfx;
    play.state.gfxCtx = &gfx;
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    testBodyTurning(&play);
    testLuluPresentation(&play);
    puts(
        "PASS Skull Kid body turning: both poses, smooth approach/return, angle wrap, placement and limb preservation");
    puts("PASS Lulu facing: local pose correction, placement yaw, gesture joints, and render matrix restoration");
    return 0;
}
