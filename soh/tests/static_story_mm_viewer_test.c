/* Runner inserts unmodified production function bodies, compiled with the real viewer types. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "tests/test_require.h"
#include "src/overlays/actors/ovl_En_Viewer/z_en_viewer.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_actor.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_ganon.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_kokiri.h"
#include "mods/transformation_masks/assets/mm_asset_loader.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_dy_obj/object_dy_obj.h"
#include "soh/ResourceManagerHelpers.h"

void osSyncPrintfUnused(const char* format, ...) {
}
void FrameInterpolation_RecordOpenChild(const void* a, int b) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void gSPSegment(void* value, int segment, uintptr_t target) {
    __gSPSegment((Gfx*)value, segment, target);
}
void gSPDisplayList(Gfx* pkt, Gfx* dl) {
    __gSPDisplayList(pkt, dl);
}

static unsigned initCalls, freeCalls, releaseCalls, drawCalls, killCalls, objectCalls, colliderFrees, loopCalls;
static bool loadSuccess = true, allocationSuccess = true;
static float rotateX;
static unsigned char eyes[8][16], mouths[4][16];
static FlexSkeletonHeader skeleton;
static AnimationHeader animation, footAnimations[2];
static bool footAnimExists, footAnimLoads = true;
static EnViewer* drawing;
static Gfx commands[16];
static unsigned faceCommands;
static bool altAssets, hdHeadExists = true, hdHeadLoads = true;
static MmSkullKidDisplayLists skullModels[2];
static Gfx skullGeometry[2][25][1];
static bool skullModelAvailable = true;
static MmAnjuDisplayLists anjuModels[2];
static Gfx anjuGeometry[2][24][1];
static bool umbrellaAvailable = true;
static uint8_t anjuRequestedPose;
const MmAnjuDisplayLists* MmAssets_GetAnjuDisplayLists(uint8_t pose) {
    anjuRequestedPose = pose;
    return umbrellaAvailable && pose < 2 ? &anjuModels[altAssets] : NULL;
}
const MmSkullKidDisplayLists* MmAssets_GetSkullKidDisplayLists(void) {
    return skullModelAvailable ? &skullModels[altAssets] : NULL;
}
static Gfx originalHead[1], hdHeads[2][4][1], shopGalHeads[3][1], greatFairyHeads[3][1];
static unsigned greatFairyExpectedEye;
static Gfx* expectedHead = originalHead;
static unsigned hdLoads;
bool ResourceMgr_IsAltAssetsEnabled(void) {
    return altAssets;
}
uint8_t ResourceMgr_FileExists(const char* path) {
    if (strstr(path, "ShopGalMMDLevel") != NULL)
        return footAnimExists;
    REQUIRE(strncmp(path, "alt/objects/object_zov/Lulu3DSHDBlinkHead", 39) == 0 ||
            strncmp(path, "alt/objects/object_bg/ShopGalMMDBlinkHead", 39) == 0 ||
            strstr(path, "alt/objects/object_dy_obj/HWGreatFairyCharcoalBlinkHead") == path);
    return hdHeadExists;
}
char* ResourceMgr_GetResourceDataByNameHandlingMQ(const char* path) {
    if (strstr(path, "ShopGalMMDLevel") != NULL) {
        REQUIRE(altAssets && footAnimExists);
        if (strcmp(path, "alt/objects/object_bg/ShopGalMMDLevelIdleAnim") == 0)
            return footAnimLoads ? (char*)&footAnimations[0] : NULL;
        REQUIRE(strcmp(path, "alt/objects/object_bg/ShopGalMMDLevelSwayAnim") == 0);
        return footAnimLoads ? (char*)&footAnimations[1] : NULL;
    }
    ++hdLoads;
    REQUIRE(altAssets && hdHeadExists);
    for (unsigned mouth = 0; mouth < 2; ++mouth)
        for (unsigned eye = 0; eye < 4; ++eye) {
            char expected[96];
            snprintf(expected, sizeof(expected), "alt/objects/object_zov/Lulu3DSHDBlinkHead%u%sDL", eye,
                     mouth ? "" : "MouthClosed");
            if (strcmp(path, expected) == 0)
                return hdHeadLoads ? (char*)hdHeads[mouth][eye] : NULL;
        }
    for (unsigned eye = 0; eye < 3; ++eye) {
        char expected[96];
        snprintf(expected, sizeof(expected), "alt/objects/object_bg/ShopGalMMDBlinkHead%uDL", eye);
        if (strcmp(path, expected) == 0)
            return hdHeadLoads ? (char*)shopGalHeads[eye] : NULL;
        snprintf(expected, sizeof(expected), "alt/objects/object_dy_obj/HWGreatFairyCharcoalBlinkHead%uDL", eye);
        if (strcmp(path, expected) == 0)
            return hdHeadLoads ? (char*)greatFairyHeads[eye] : NULL;
    }
    REQUIRE(false);
    return NULL;
}
uintptr_t gSegments[16];
static GameInfo gameInfo;
GameInfo* gGameInfo = &gameInfo;
static int16_t playerData[89 * 67];
static unsigned lodDraws;
static Gfx umbrella[1];
static Mtx umbrellaMatrix;
static unsigned umbrellaLoads, umbrellaMatrices, bindingChecks;
Mtx* Matrix_NewMtx(GraphicsContext* gfx, char* file, s32 line) {
    ++umbrellaMatrices;
    return &umbrellaMatrix;
}
static ColliderCylinderInit sStaticCylinderInit;
void EnViewerStatic_Update(EnViewer*, PlayState*);
static void EnViewerStatic_UpdateTracking(EnViewer* self, PlayState* play) {
    (void)self;
    (void)play;
}
void EnViewerStatic_OfferTalk(EnViewer* self, PlayState* play) {
    (void)self;
    (void)play;
}
static void EnViewerStatic_InitRutoWater(EnViewer* a, PlayState* p, const StaticStoryPoseDescriptor* d) {
    REQUIRE(false);
}
static bool EnViewerStatic_UpdateRutoWater(EnViewer* a, PlayState* p, bool ended) {
    REQUIRE(false);
    return false;
}
static void EnViewerStatic_InitSkeleton(EnViewer* a, PlayState* p, const StaticStoryPoseDescriptor* d) {
    REQUIRE(false);
}
static void EnViewerStatic_SetDaruniaDanceStep(EnViewer* a, u8 step) {
    REQUIRE(false);
}
void EnViewer_SetupAction(EnViewer* self, EnViewerActionFunc action) {
    self->actionFunc = action;
}
void ActorCatalogue_LogLifecycle(const char* s, int a, int b, int c, int d, int e, int f, int g) {
}
void MmAssets_Init(void) {
}
bool MmAssets_LoadNormalActor(int type, unsigned char pose, MmNormalActorResources* output) {
    memset(output, 0, sizeof(*output));
    if (!loadSuccess)
        return false;
    const StaticStoryMmPresentation* p = StaticStoryMm_GetPresentation(type, pose);
    skeleton.sh.limbCount = p->limbCount;
    skeleton.dListCount = p->matrixCount;
    animation.common.frameCount = p->frameCount;
    output->owner = malloc(1);
    output->skeleton = &skeleton;
    output->animation = &animation;
    for (unsigned i = 0; i < p->eyeCount; ++i)
        output->eyes[i] = eyes[i];
    for (unsigned i = 0; i < p->mouthCount; ++i)
        output->mouths[i] = mouths[i];
    return true;
}
bool MmAssets_LoadKafei(unsigned char pose, MmNormalActorResources* output) {
    bool loaded = MmAssets_LoadNormalActor(STATIC_STORY_ACTOR_CHILD_KAFEI, pose, output);
    output->animation = NULL;
    output->playerFrames = loaded ? playerData : NULL;
    return loaded;
}
void MmAssets_ReleaseNormalActor(void* owner) {
    if (owner) {
        ++releaseCalls;
        free(owner);
    }
}
void* MmAssets_LoadSkeleton(const char* path) {
    REQUIRE(strcmp(path, "objects/object_bg/gTreasureChestShopGalSkel") == 0);
    skeleton.sh.limbCount = 23;
    skeleton.dListCount = 16;
    return &skeleton;
}
void* MmAssets_LoadAnimation(const char* path) {
    REQUIRE(strcmp(path, "objects/object_bg/object_bg_Anim_009890") == 0 ||
            strcmp(path, "objects/object_bg/object_bg_Anim_001384") == 0);
    animation.common.frameCount = 32;
    return &animation;
}
void* MmAssets_LoadResource(const char* path) {
    for (unsigned eye = 0; eye < 3; ++eye)
        if (strcmp(path, StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, eye)) == 0)
            return eyes[eye];
    REQUIRE(false);
    return NULL;
}
Gfx* MmAssets_LoadDisplayListGraphStrict(const char* path) {
    REQUIRE(strcmp(path, "objects/object_an2/gAnju2UmbrellaDL") == 0);
    ++umbrellaLoads;
    return umbrellaAvailable ? umbrella : NULL;
}
void Actor_Kill(Actor* a) {
    ++killCalls;
}
s32 Object_GetIndex(ObjectContext* c, s16 id) {
    ++objectCalls;
    return -1;
}
s32 Object_Spawn(ObjectContext* c, s16 id) {
    ++objectCalls;
    return -1;
}
s32 Object_IsLoaded(ObjectContext* c, s32 slot) {
    ++objectCalls;
    return 0;
}
void Actor_SetObjectDependency(PlayState* p, Actor* a) {
    ++objectCalls;
}
int StaticStoryKokiri_RequestObjects(EnViewer* a, PlayState* p) {
    REQUIRE(false);
    return false;
}
void StaticStoryKokiri_Init(EnViewer* a, PlayState* p) {
    REQUIRE(false);
}
static EnViewer* expectedKokiri;
static unsigned kokiriUpdates;
static float kokiriStep;
void StaticStoryKokiri_UpdatePose(EnViewer* a, f32 step) {
    REQUIRE(a == expectedKokiri);
    /* The base animation must already have sampled this tick. */
    REQUIRE(a->skin.skelAnime.jointTable[0].x == 7);
    ++kokiriUpdates;
    kokiriStep = step;
}
s32 SkelAnime_InitFlex(PlayState* p, SkelAnime* s, FlexSkeletonHeader* h, AnimationHeader* a, Vec3s* j, Vec3s* m,
                       s32 n) {
    ++initCalls;
    REQUIRE(a == NULL && j == NULL && m == NULL);
    s->limbCount = h->sh.limbCount + 1;
    s->dListCount = h->dListCount;
    s->jointTable = calloc(s->limbCount, sizeof(Vec3s));
    s->morphTable = allocationSuccess ? calloc(s->limbCount, sizeof(Vec3s)) : NULL;
    return 0; // The production function's return value is undefined; caller must inspect allocations.
}
s32 SkelAnime_Init(PlayState* p, SkelAnime* s, SkeletonHeader* h, AnimationHeader* a, Vec3s* j, Vec3s* m, s32 n) {
    REQUIRE(false);
    return 0;
}
void Animation_PlayLoopSetSpeed(SkelAnime* s, AnimationHeader* a, f32 speed) {
    ++loopCalls;
    REQUIRE(s->jointTable && s->morphTable);
    REQUIRE(speed == 1.0f);
    s->animation = a;
    s->curFrame = 0;
    s->endFrame = a->common.frameCount - 1;
    s->jointTable[0] = (Vec3s){ 123, 456, 789 };
}
s32 SkelAnime_Update(SkelAnime* s) {
    s->curFrame += 1;
    s->jointTable[0].x += 7;
    return 0;
}
void SkelAnime_Free(SkelAnime* s, PlayState* p) {
    ++freeCalls;
    free(s->jointTable);
    free(s->morphTable);
}
void Skin_Free(PlayState* p, Skin* s) {
    REQUIRE(false);
}
void Actor_SetScale(Actor* a, f32 scale) {
    a->scale = (Vec3f){ scale, scale, scale };
}
void ActorShape_Init(ActorShape* shape, f32 y, ActorShadowFunc draw, f32 radius) {
    shape->yOffset = y;
}
void ActorShadow_DrawCircle(Actor* a, Lights* l, PlayState* p) {
}
s32 Collider_InitCylinder(PlayState* p, ColliderCylinder* c) {
    return 1;
}
s32 Collider_SetCylinder(PlayState* p, ColliderCylinder* c, Actor* a, ColliderCylinderInit* i) {
    return 1;
}
s32 Collider_DestroyCylinder(PlayState* p, ColliderCylinder* c) {
    ++colliderFrees;
    return 1;
}
void Collider_UpdateCylinder(Actor* a, ColliderCylinder* c) {
}
s32 CollisionCheck_SetOC(PlayState* p, CollisionCheckContext* c, Collider* a) {
    return 0;
}
void Actor_SetFocus(Actor* a, f32 y) {
    a->focus.pos = a->world.pos;
    a->focus.pos.y += y;
}
s16 Rand_S16Offset(s16 base, s16 range) {
    return base;
}
f32 Math_SinS(s16 angle) {
    return 0;
}
s16 Math_SmoothStepToS(s16* value, s16 target, s16 scale, s16 step, s16 min) {
    return 0;
}
bool StaticRutoWater_ShouldTurnBody(const StaticRutoWaterState* state) {
    return false;
}
void Matrix_RotateX(f32 x, u8 mode) {
    REQUIRE(mode == MTXMODE_APPLY);
    rotateX = x;
}
void Matrix_Push(void) {
}
void Matrix_Pop(void) {
}
void Matrix_RotateY(f32 y, u8 mode) {
    REQUIRE(mode == MTXMODE_APPLY);
}
void Gfx_SetupDL_25Opa(GraphicsContext* gfx) {
}
void Graph_OpenDisps(Gfx** dList, GraphicsContext* gfx, const char* file, s32 line) {
}
void Graph_CloseDisps(Gfx** dList, GraphicsContext* gfx, const char* file, s32 line) {
}
void MmAssets_EnsureStrictTextureBindings(void) {
    ++bindingChecks;
}
static s32 EnViewer_StaticSkullKidOverrideLimbDraw(PlayState* p, s32 i, Gfx** d, Vec3f* v, Vec3s* r, void* a) {
    return false;
}
static void EnViewer_StaticSkullKidPostLimbDraw(PlayState* p, s32 i, Gfx** d, Vec3s* r, void* a) {
}
static void EnViewer_DrawStaticTatl(EnViewer* a, PlayState* p) {
}
void SkelAnime_DrawSkeletonOpa(PlayState* play, SkelAnime* skel, OverrideLimbDrawOpa override, PostLimbDrawOpa post,
                               void* arg) {
    ++drawCalls;
    REQUIRE(arg == drawing);
    if (drawing->staticState.type == STATIC_STORY_ACTOR_SKULL_KID) {
        const MmSkullKidDisplayLists* selected = &skullModels[altAssets];
        REQUIRE(drawing->staticState.skullKidHeadDL == selected->head);
        REQUIRE(drawing->staticState.skullKidEyesDL == selected->eyes);
        REQUIRE(drawing->staticState.skullKidMaskDL == selected->mask);
        for (unsigned limb = 0; limb < 22; ++limb)
            REQUIRE(drawing->staticState.skullKidLimbDLs[limb] == selected->limbs[limb]);
        REQUIRE(post == EnViewer_StaticSkullKidPostLimbDraw);
        REQUIRE(play->state.gfxCtx->polyOpa.p == commands + 2);
        REQUIRE((commands[0].words.w0 >> 24) == G_RDPPIPESYNC);
        REQUIRE((commands[1].words.w0 >> 24) == G_SETENVCOLOR);
        REQUIRE(commands[1].words.w1 == 0xffffffffU);
        return;
    }
    if (drawing->staticState.type == STATIC_STORY_ACTOR_ANJU) {
        REQUIRE(post != NULL);
        REQUIRE(play->state.gfxCtx->polyOpa.p == commands + 3);
        REQUIRE(commands[0].words.w1 == (uintptr_t)eyes[0]);
        REQUIRE(commands[1].words.w1 == (uintptr_t)mouths[0]);
        Vec3s rot = { 0 };
        Gfx* dl = originalHead;
        for (unsigned limb = 1; limb <= 20; ++limb) {
            const MmAnjuDisplayLists* model = &anjuModels[altAssets];
            Vec3f pos = { 10, 20, 30 };
            REQUIRE(override != NULL);
            override(play, limb, &dl, &pos, &rot, arg);
            if (limb > 1)
                REQUIRE(dl == (limb == 9 ? expectedHead : model->limbs[limb]));
            REQUIRE(pos.x == 10 && pos.z == 30);
            REQUIRE(pos.y == 20 + (altAssets && (limb == 2 || limb == 10 || limb == 17) ? 476 : 0));
            Gfx* before = play->state.gfxCtx->polyOpa.p;
            post(play, limb, &dl, &rot, arg);
            REQUIRE(play->state.gfxCtx->polyOpa.p == before + (limb == 8 ? 2 : 0));
        }
        REQUIRE(play->state.gfxCtx->polyOpa.p == commands + 5);
        REQUIRE((commands[3].words.w0 >> 24) == G_MTX);
        REQUIRE(commands[3].words.w1 == (uintptr_t)&umbrellaMatrix);
        REQUIRE((commands[4].words.w0 >> 24) == G_DL);
        REQUIRE(commands[4].words.w1 == (uintptr_t)umbrella);
        return;
    }
    REQUIRE(post == NULL);
    if (drawing->staticState.type == STATIC_STORY_ACTOR_GREAT_FAIRY) {
        const char* const eyePaths[] = { gGreatFairyEyeOpenTex, gGreatFairyEyeHalfTex, gGreatFairyEyeClosedTex };
        REQUIRE(play->state.gfxCtx->polyOpa.p == commands + 3);
        REQUIRE((commands[0].words.w0 & 0xffff) == 8 * 4);
        REQUIRE((commands[1].words.w0 & 0xffff) == 9 * 4);
        REQUIRE((commands[2].words.w0 & 0xffff) == 10 * 4);
        REQUIRE(strcmp((const char*)commands[0].words.w1, eyePaths[greatFairyExpectedEye]) == 0);
        REQUIRE(strcmp((const char*)commands[1].words.w1, eyePaths[greatFairyExpectedEye]) == 0);
        REQUIRE(strcmp((const char*)commands[2].words.w1, gGreatFairyMouthClosedTex) == 0);
        Vec3s rot = { 0 };
        Vec3f pos = { 0 };
        Gfx* dl = originalHead;
        REQUIRE(override != NULL);
        override(play, 14, &dl, &pos, &rot, arg);
        REQUIRE(dl == originalHead && rot.x == 0 && rot.z == 0);
        override(play, 15, &dl, &pos, &rot, arg);
        REQUIRE(dl == expectedHead);
        REQUIRE(rot.x == (drawing->staticState.pose == 0 ? 6000 : 4096));
        REQUIRE(rot.z == (drawing->staticState.pose == 0 ? -6000 : -4096));
        rot = (Vec3s){ 0 };
        dl = originalHead;
        override(play, 8, &dl, &pos, &rot, arg);
        REQUIRE(dl == originalHead);
        REQUIRE(rot.x == (drawing->staticState.pose == 0 ? 200 : 0));
        return;
    }
    if (drawing->staticState.type == STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL) {
        REQUIRE(play->state.gfxCtx->polyOpa.p == commands + 1);
        REQUIRE((commands[0].words.w0 & 0xffff) == 8 * 4);
        const char* eyePath = (const char*)commands[0].words.w1;
        REQUIRE(((uintptr_t)eyePath & 1) == 0);       // Odd addresses are interpreted as segmented pointers.
        REQUIRE(strncmp(eyePath, "__OTR__", 7) == 0); // Keep HD metadata, not just ImageData.
        REQUIRE(strcmp(eyePath + 7,
                       StaticStoryMm_GetEyeTexturePath(drawing->staticState.type, drawing->staticState.eyeIndex)) == 0);
        Vec3s rot = { 0 };
        Vec3f pos = { 0 };
        Gfx* dl = originalHead;
        REQUIRE(override != NULL);
        override(play, 5, &dl, &pos, &rot, arg);
        REQUIRE(dl == expectedHead);
        return;
    }
    // Segment commands must already be emitted before the skeleton draw boundary.
    REQUIRE(play->state.gfxCtx->polyOpa.p > commands);
    faceCommands = play->state.gfxCtx->polyOpa.p - commands - 1;
    REQUIRE((commands[faceCommands].words.w0 >> 24) == G_SETENVCOLOR);
    REQUIRE(commands[faceCommands].words.w1 == 0xffffffffU);
    const StaticStoryMmPresentation* p =
        StaticStoryMm_GetPresentation(drawing->staticState.type, drawing->staticState.pose);
    unsigned firstFace = drawing->staticState.type == STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN;
    REQUIRE(faceCommands == firstFace + (p->eyeCount != 0) + (p->mouthCount != 0));
    if (firstFace) {
        REQUIRE((commands[0].words.w0 & 0xffff) == 0x0C * 4);
        REQUIRE(commands[0].words.w1 == (uintptr_t)MmAssets_GetOpaqueRenderMode());
        for (unsigned index = 0; index < 4; ++index) {
            REQUIRE((MmAssets_GetOpaqueRenderMode()[index].words.w0 >> 24) == G_ENDDL);
            REQUIRE(MmAssets_GetOpaqueRenderMode()[index].words.w1 == 0);
        }
    }
    if (p->eyeCount || p->mouthCount) {
        StaticStoryMmFace face =
            StaticStoryMm_ResolveFace(drawing->staticState.type, drawing->staticState.pose, skel->curFrame,
                                      drawing->staticState.eyeIndex, drawing->staticState.tracking);
        REQUIRE(commands[firstFace].words.w1 == (uintptr_t)eyes[face.eye]);
        REQUIRE(commands[firstFace + 1].words.w1 == (uintptr_t)mouths[face.mouth]);
        REQUIRE((commands[firstFace].words.w0 & 0xffff) == p->eyeSegment * 4);
        REQUIRE((commands[firstFace + 1].words.w0 & 0xffff) == p->mouthSegment * 4);
    }
    Vec3s rot = { 0 };
    Vec3f pos = { 0 };
    Gfx* dl = originalHead;
    REQUIRE(override != NULL);
    override(play, 11, &dl, &pos, &rot, arg);
    REQUIRE(dl == originalHead);
    if (drawing->staticState.type == STATIC_STORY_ACTOR_LULU && drawing->staticState.pose == 0 &&
        drawing->staticState.tracking)
        REQUIRE(rot.x == 200);
    override(play, 12, &dl, &pos, &rot, arg);
    REQUIRE(dl == expectedHead);
    if (drawing->staticState.type == STATIC_STORY_ACTOR_LULU && drawing->staticState.pose == 0 &&
        drawing->staticState.tracking)
        REQUIRE(rot.x == 300 && rot.z == 50);
}
void SkelAnime_DrawFlexLod(PlayState* play, void** skeleton, Vec3s* joints, s32 count, OverrideLimbDrawOpa override,
                           PostLimbDrawOpa post, void* arg, s32 lod) {
    ++lodDraws;
    REQUIRE(count == 18 && lod == 0 && post == NULL && arg == drawing);
    REQUIRE(joints == drawing->skin.skelAnime.jointTable);
    REQUIRE(play->state.gfxCtx->polyOpa.p == commands + 3);
    StaticStoryMmFace face = StaticStoryMm_KafeiFace(drawing->staticState.mmAppearance);
    REQUIRE(commands[0].words.w1 == (uintptr_t)eyes[face.eye]);
    REQUIRE(commands[1].words.w1 == (uintptr_t)mouths[face.mouth]);
    REQUIRE((commands[0].words.w0 & 0xffff) == 8 * 4 && (commands[1].words.w0 & 0xffff) == 9 * 4);
    REQUIRE(commands[2].words.w1 == 0xffffffffU);
    Vec3f pos = { 17, 34, -17 };
    Vec3s rot = { 1, 2, 3 };
    Gfx* dl = (Gfx*)0x1234;
    REQUIRE(!override(play, 1, &dl, &pos, &rot, arg));
    REQUIRE(fabsf(pos.x - 11) < 0.0001f && fabsf(pos.y - 22) < 0.0001f && fabsf(pos.z + 11) < 0.0001f);
    REQUIRE(dl == (Gfx*)0x1234 && rot.x == 1 && rot.y == 2 && rot.z == 3);
    pos = (Vec3f){ 17, 34, -17 };
    override(play, 2, &dl, &pos, &rot, arg);
    REQUIRE(pos.x == 17 && pos.y == 34);
}
/* Only unrelated OoT renderers are stubbed. MM tests enter the public draw
 * function and its real static-actor dispatcher, not the MM helper directly. */
#define UNEXPECTED_DRAW(name)                    \
    void name(EnViewer* self, PlayState* play) { \
        REQUIRE(false);                          \
    }
UNEXPECTED_DRAW(EnViewer_DrawGanondorf)
UNEXPECTED_DRAW(EnViewer_DrawHorse)
UNEXPECTED_DRAW(EnViewer_DrawZelda)
UNEXPECTED_DRAW(EnViewer_DrawImpa)
UNEXPECTED_DRAW(EnViewer_DrawStaticChildMalon)
UNEXPECTED_DRAW(EnViewer_DrawStaticSaria)
UNEXPECTED_DRAW(EnViewer_DrawStaticAdultZelda)
UNEXPECTED_DRAW(EnViewer_DrawStaticSheik)
UNEXPECTED_DRAW(EnViewer_DrawStaticAdultRuto)
UNEXPECTED_DRAW(EnViewer_DrawStaticAdultRutoXlu)
UNEXPECTED_DRAW(EnViewer_DrawStaticChildRuto)
UNEXPECTED_DRAW(StaticStoryKokiri_Draw)
UNEXPECTED_DRAW(EnViewer_DrawStaticAdultMalon)
UNEXPECTED_DRAW(EnViewer_DrawStaticDarunia)
UNEXPECTED_DRAW(EnViewer_DrawStaticNabooru)
UNEXPECTED_DRAW(EnViewer_DrawStaticPhantomGanon)
#undef UNEXPECTED_DRAW
/* PRODUCTION_OPAQUE_RENDER_MODE */
/* PRODUCTION_VIEWER_FUNCTIONS */

static void prepare(EnViewer* viewer, int type, int pose) {
    memset(viewer, 0, sizeof(*viewer));
    viewer->animObjBankIndex = -1;
    viewer->staticState.staticMode = true;
    viewer->staticState.type = type;
    viewer->staticState.pose = pose;
    viewer->actor.params = 0x7E00;
    for (unsigned i = 0; i < 4; ++i)
        viewer->staticState.objectSlots[i] = -1;
    viewer->actor.world.pos = (Vec3f){ 12, 345, 67 };
    viewer->actor.home.pos = viewer->actor.world.pos;
}
static void draw(EnViewer* viewer, PlayState* play) {
    drawing = viewer;
    play->state.gfxCtx->polyOpa.p = commands;
    play->state.gfxCtx->polyXlu.p = commands + 15;
    rotateX = 0;
    EnViewer_Draw(&viewer->actor, play);
}
static void testAnjuPoseAnchor(PlayState* play, uint8_t pose) {
    const StaticStoryActorType anju = STATIC_STORY_ACTOR_ANJU;
    REQUIRE(StaticStoryActor_GetType(pose ? 0x7E1D : 0x7E0D) == anju);
    REQUIRE(StaticStoryActor_IsAvailable(anju));
    REQUIRE(!StaticStoryActor_CanTrack(anju, pose) && StaticStoryActor_CanTalk(anju));
    REQUIRE(StaticStoryActor_SelectTextId(anju, NULL) == 0x8F26);
    REQUIRE(!StaticStoryActor_LocksRootTranslation(anju, pose));
    const StaticStoryMmPresentation* p = StaticStoryMm_GetPresentation(anju, pose);
    REQUIRE(p && strcmp(p->animationPath, pose ? "objects/object_an2/gAnju2UmbrellaIdleAnim"
                                               : "objects/object_an2/gAnju2UmbrellaCryAnim") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetEyeTexturePath(anju, 0), "objects/object_an1/gAnju1EyeSadTex") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetMouthTexturePath(anju, 0), "objects/object_an1/gAnju1MouthClosedTex") == 0);
    for (unsigned scene = 0; scene < 4; ++scene) {
        altAssets = scene % 2;
        EnViewer a;
        prepare(&a, anju, pose);
        a.actor.world.rot.y = a.actor.shape.rot.y = -12345;
        // Deliberately place the inferred floor above the actor and supply motion:
        // presentation updates must never run floor correction or movement.
        a.actor.floorHeight = 9000;
        a.actor.gravity = -9;
        a.actor.velocity = (Vec3f){ 123, 456, 789 };
        a.actor.colChkInfo.displacement = (Vec3f){ 11, 999, -13 };
        unsigned before = drawCalls, loads = umbrellaLoads, matrices = umbrellaMatrices, bindings = bindingChecks;
        EnViewerStatic_WaitForObjects(&a, play);
        REQUIRE(a.staticState.initialized && a.isVisible);
        REQUIRE(anjuRequestedPose == pose);
        REQUIRE(a.skin.skelAnime.limbCount == 21 && a.skin.skelAnime.dListCount == 19);
        REQUIRE(a.skin.skelAnime.endFrame == (pose ? 31 : 42) && a.actor.shape.yOffset == 0);
        REQUIRE(a.actor.colChkInfo.mass == MASS_IMMOVABLE);
        for (unsigned tick = 0; tick < 86; ++tick) {
            EnViewer_Update(&a.actor, play);
            static const unsigned sequence[] = { 0, 1, 2, 2, 1 };
            expectedHead = anjuModels[altAssets].heads[altAssets ? sequence[a.staticState.anjuBlinkPhase] : 0];
            draw(&a, play);
            REQUIRE(anjuRequestedPose == pose);
            REQUIRE(a.actor.world.pos.x == 12 && a.actor.world.pos.y == 345 && a.actor.world.pos.z == 67);
            REQUIRE(a.actor.home.pos.y == 345 && a.actor.shape.yOffset == 0);
            REQUIRE(a.actor.world.rot.y == -12345 && a.actor.shape.rot.y == -12345);
            REQUIRE(a.skin.skelAnime.jointTable[0].y == 456);
        }
        REQUIRE(drawCalls == before + 86 && umbrellaLoads == loads);
        REQUIRE(umbrellaMatrices == matrices + 86 && bindingChecks == bindings + 86);
        EnViewer_Destroy(&a.actor, play);
        draw(&a, play);
        REQUIRE(drawCalls == before + 86);
    }
    EnViewer failed;
    prepare(&failed, anju, pose);
    umbrellaAvailable = false;
    unsigned releases = releaseCalls, kills = killCalls, inits = initCalls;
    EnViewerStatic_WaitForObjects(&failed, play);
    REQUIRE(!failed.staticState.initialized && initCalls == inits && killCalls == kills + 1);
    EnViewer_Destroy(&failed.actor, play);
    REQUIRE(releaseCalls == releases + 1);
    umbrellaAvailable = true;
    printf("PASS Anju pose %u: public draw, right-hand umbrella, fixed XYZ/yaw, missing umbrella and reentry\n", pose);
}
static void testAnjuHdBlink(PlayState* play, uint8_t pose) {
    EnViewer first, second;
    altAssets = true;
    prepare(&first, STATIC_STORY_ACTOR_ANJU, pose);
    prepare(&second, STATIC_STORY_ACTOR_ANJU, pose);
    EnViewerStatic_WaitForObjects(&first, play);
    EnViewerStatic_WaitForObjects(&second, play);
    first.staticState.anjuBlinkTimer = 0;
    const unsigned sequence[] = { 1, 2, 2, 1, 0 };
    for (unsigned tick = 0; tick < 5; ++tick) {
        EnViewer_Update(&first.actor, play);
        expectedHead = anjuModels[1].heads[sequence[tick]];
        uint8_t phase = first.staticState.anjuBlinkPhase;
        int16_t timer = first.staticState.anjuBlinkTimer;
        draw(&first, play);
        draw(&first, play);
        REQUIRE(first.staticState.anjuBlinkPhase == phase && first.staticState.anjuBlinkTimer == timer);
        REQUIRE(second.staticState.anjuBlinkPhase == 0 && second.staticState.anjuBlinkTimer == 30);
        expectedHead = anjuModels[1].heads[0];
        draw(&second, play);
    }
    REQUIRE(first.staticState.anjuBlinkPhase == 0 && first.staticState.anjuBlinkTimer == 30);
    first.staticState.anjuBlinkPhase = 2;
    altAssets = false;
    expectedHead = anjuModels[0].heads[0];
    draw(&first, play);
    REQUIRE(first.staticState.anjuModel == &anjuModels[0]);
    altAssets = true;
    expectedHead = anjuModels[1].heads[2];
    draw(&first, play);
    REQUIRE(first.staticState.anjuModel == &anjuModels[1]);
    first.staticState.anjuBlinkPhase = 255;
    expectedHead = anjuModels[1].heads[0];
    draw(&first, play);
    EnViewer_Destroy(&first.actor, play);
    EnViewer_Destroy(&second.actor, play);
    altAssets = false;
    puts("PASS Anju HD blink: 100ms full closure, independent instances, render independence, live Alt and bounded "
         "phase");
}
static void testLuluHdBlink(PlayState* play) {
    altAssets = true;
    for (unsigned pose = 0; pose < 4; ++pose) {
        EnViewer first, second;
        prepare(&first, STATIC_STORY_ACTOR_LULU, pose);
        prepare(&second, STATIC_STORY_ACTOR_LULU, pose);
        EnViewerStatic_WaitForObjects(&first, play);
        EnViewerStatic_WaitForObjects(&second, play);
        first.staticState.tracking = pose == 0;
        first.staticState.interactInfo.headRot = (Vec3s){ 50, 100, 0 };
        first.staticState.interactInfo.torsoRot.y = 200;
        unsigned mouth = pose == 2;
        expectedHead = hdHeads[mouth][0];
        draw(&first, play);
        for (unsigned tick = 0; tick < 30; ++tick)
            EnViewer_Update(&first.actor, play);
        draw(&first, play);
        const unsigned sequence[] = { 1, 2, 3, 2, 1, 0 };
        for (unsigned tick = 0; tick < 6; ++tick) {
            EnViewer_Update(&first.actor, play);
            expectedHead = hdHeads[mouth][sequence[tick]];
            draw(&first, play);
            draw(&first, play); // Extra renders must not advance the blink.
            expectedHead = hdHeads[mouth][0];
            draw(&second, play); // Each placed Lulu has independent timing.
        }
        expectedHead = originalHead;
        unsigned loads = hdLoads;
        altAssets = false;
        draw(&first, play);
        REQUIRE(hdLoads == loads);
        altAssets = true;
        hdHeadExists = false;
        draw(&first, play);
        REQUIRE(hdLoads == loads);
        hdHeadExists = true;
        hdHeadLoads = false;
        draw(&first, play);
        hdHeadLoads = true;
        expectedHead = hdHeads[mouth][0];
        draw(&first, play); // Live re-enable resumes the HD head.
        EnViewer_Destroy(&first.actor, play);
        EnViewer_Destroy(&second.actor, play);
    }
    altAssets = false;
    expectedHead = originalHead;
    puts("PASS Lulu HD blink: four poses, all eye frames, mouth selection, private timing, render independence, "
         "optional-asset fallback");
}
static void testShopGalBlink(PlayState* play) {
    for (unsigned useAlt = 0; useAlt < 2; ++useAlt)
        for (unsigned pose = 0; pose < 3; ++pose) {
            altAssets = useAlt;
            expectedHead = useAlt ? shopGalHeads[0] : originalHead;
            EnViewer first, second;
            prepare(&first, STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, pose);
            prepare(&second, STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, pose);
            EnViewerStatic_WaitForObjects(&first, play);
            EnViewerStatic_WaitForObjects(&second, play);
            REQUIRE(first.staticState.initialized && second.staticState.initialized);
            draw(&first, play);
            for (unsigned tick = 0; tick < 30; ++tick)
                EnViewer_Update(&first.actor, play);
            REQUIRE(first.staticState.eyeIndex == 0);
            for (unsigned tick = 1; tick <= 4; ++tick) {
                EnViewer_Update(&first.actor, play);
                REQUIRE(first.staticState.eyeIndex == tick % 4);
                const unsigned states[] = { 0, 1, 2, 1 };
                expectedHead = useAlt ? shopGalHeads[states[tick % 4]] : originalHead;
                draw(&first, play);
                draw(&first, play);
                REQUIRE(second.staticState.eyeIndex == 0);
                expectedHead = useAlt ? shopGalHeads[0] : originalHead;
                draw(&second, play);
            }
            REQUIRE(first.staticState.blinkTimer == 30);
            expectedHead = originalHead;
            unsigned loads = hdLoads;
            hdHeadExists = false;
            draw(&first, play);
            REQUIRE(hdLoads == loads);
            hdHeadExists = true;
            hdHeadLoads = false;
            draw(&first, play);
            hdHeadLoads = true;
            EnViewer_Destroy(&first.actor, play);
            EnViewer_Destroy(&second.actor, play);
        }
    altAssets = false;
    expectedHead = originalHead;
    puts("PASS Treasure Chest Shop Gal: three poses, complete blink, per-instance timing, aligned OTR eye paths retain "
         "HD metadata");
}
static void testShopGalFootAnimations(PlayState* play) {
    for (unsigned pose = 0; pose < 3; ++pose)
        for (unsigned mode = 0; mode < 5; ++mode) {
            altAssets = mode != 0;
            footAnimExists = mode != 1;
            footAnimLoads = mode != 2;
            footAnimations[0].common.frameCount = footAnimations[1].common.frameCount = mode == 3 ? 31 : 32;
            EnViewer viewer;
            prepare(&viewer, STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, pose);
            EnViewerStatic_WaitForObjects(&viewer, play);
            REQUIRE(viewer.staticState.initialized);
            REQUIRE(viewer.skin.skelAnime.animation == (mode == 4 ? &footAnimations[pose == 1] : &animation));
            EnViewer_Destroy(&viewer.actor, play);
        }
    altAssets = false;
    footAnimExists = false;
    footAnimLoads = true;
    puts("PASS Shop Gal foot clips: three poses, alt-disabled, absent, failed and mismatched-duration fallbacks");
}
static void drawGreatFairy(EnViewer* viewer, PlayState* play) {
    drawing = viewer;
    play->state.gfxCtx->polyOpa.p = commands;
    EnViewer_DrawStaticGreatFairy(viewer, play);
}
static void testGreatFairyBlink(PlayState* play) {
    const unsigned eyesAtTick[] = { 0, 1, 2, 2, 1, 0 };
    for (unsigned pose = 0; pose < 3; ++pose) {
        EnViewer first, second;
        Vec3s joints[28] = { 0 }, otherJoints[28] = { 0 };
        prepare(&first, STATIC_STORY_ACTOR_GREAT_FAIRY, pose);
        first.staticState.initialized = true;
        first.staticState.interactInfo.headRot = (Vec3s){ -6000, 6000, 0 };
        first.staticState.interactInfo.torsoRot.y = 200;
        first.skin.skelAnime.jointTable = joints;
        second = first;
        second.skin.skelAnime.jointTable = otherJoints;
        altAssets = true;
        for (unsigned tick = 0; tick <= 5; ++tick) {
            if (tick)
                EnViewerStatic_Update(&first, play);
            greatFairyExpectedEye = eyesAtTick[tick];
            expectedHead = greatFairyHeads[greatFairyExpectedEye];
            drawGreatFairy(&first, play);
            drawGreatFairy(&first, play);
            REQUIRE(second.staticState.eyeIndex == 0);
        }
        REQUIRE(first.staticState.eyeIndex == 0 && first.staticState.blinkTimer == 20);
        // Invalid state must select the open face, never index past the arrays.
        first.staticState.eyeIndex = 255;
        drawGreatFairy(&first, play);
        for (unsigned mode = 0; mode < 3; ++mode) {
            altAssets = mode != 0;
            hdHeadExists = mode != 1;
            hdHeadLoads = mode != 2;
            first.staticState.eyeIndex = 2;
            greatFairyExpectedEye = 2;
            expectedHead = originalHead;
            unsigned loads = hdLoads;
            drawGreatFairy(&first, play);
            if (mode != 2)
                REQUIRE(hdLoads == loads);
        }
        hdHeadExists = hdHeadLoads = true;
    }
    altAssets = false;
    expectedHead = originalHead;
    puts("PASS Great Fairy: three poses, 100 ms closure, private timing, safe indices, head/torso tracking and "
         "optional-head fallbacks");
}
static void testKokiriUpdateTiming(PlayState* play) {
    s32 savedRate = R_UPDATE_RATE;
    for (unsigned type = STATIC_STORY_ACTOR_KOKIRI_GIRL; type <= STATIC_STORY_ACTOR_FADO; ++type) {
        for (s32 rate = 1; rate <= 3; ++rate) {
            EnViewer actor;
            Vec3s joints[16] = { 0 };
            prepare(&actor, type, 4);
            actor.staticState.initialized = true;
            actor.skin.skelAnime.jointTable = joints;
            actor.skin.skelAnime.limbCount = 16;
            expectedKokiri = &actor;
            unsigned before = kokiriUpdates;
            R_UPDATE_RATE = rate;
            EnViewerStatic_Update(&actor, play);
            REQUIRE(kokiriUpdates == before + 1);
            /* Native normal-animation playback advances one frame per three
             * update-rate units (Link animations use a different clock). */
            REQUIRE(fabsf(kokiriStep - (float)rate / 3.0f) < 0.00001f);
        }
    }
    R_UPDATE_RATE = savedRate;
    expectedKokiri = NULL;
    puts("PASS Kokiri/Fado update bridge: native normal-animation clock, after base sampling");
}

int main(void) {
    static PlayState play;
    static GraphicsContext gfx;
    play.state.gfxCtx = &gfx;
    testKokiriUpdateTiming(&play);
    EnViewer skull = { 0 };
    skull.staticState.type = STATIC_STORY_ACTOR_SKULL_KID;
    for (unsigned model = 0; model < 2; ++model) {
        anjuModels[model].custom = model;
        anjuModels[model].umbrella = umbrella;
        for (unsigned limb = 2; limb <= 20; ++limb)
            anjuModels[model].limbs[limb] = anjuGeometry[model][limb];
        for (unsigned phase = 0; phase < 3; ++phase)
            anjuModels[model].heads[phase] = anjuGeometry[model][21 + (model ? phase : 0)];
        for (unsigned limb = 0; limb < 22; ++limb)
            if (limb != 0 && limb != 1 && limb != 17)
                skullModels[model].limbs[limb] = skullGeometry[model][limb];
        skullModels[model].head = skullGeometry[model][22];
        skullModels[model].eyes = skullGeometry[model][23];
        skullModels[model].mask = skullGeometry[model][24];
    }
    for (unsigned toggle = 0; toggle < 3; ++toggle) {
        altAssets = toggle == 1;
        drawing = &skull;
        gfx.polyOpa.p = commands;
        EnViewer_DrawStaticSkullKid(&skull, &play);
    }
    skullModelAvailable = false;
    unsigned beforeMissingDraw = drawCalls;
    EnViewer_DrawStaticSkullKid(&skull, &play);
    REQUIRE(drawCalls == beforeMissingDraw);
    skullModelAvailable = true;
    puts("PASS Skull Kid: complete model selection updates at draw after live Alt changes");
    gSegments[6] = 0x12345678;
    const int actors[] = { STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, STATIC_STORY_ACTOR_KEATON, STATIC_STORY_ACTOR_LULU };
    for (unsigned a = 0; a < 3; ++a)
        for (unsigned pose = 0; pose < (a == 2 ? 4 : 3); ++pose) {
            EnViewer first, second;
            prepare(&first, actors[a], pose);
            prepare(&second, actors[a], pose);
            EnViewerStatic_WaitForObjects(&first, &play);
            EnViewerStatic_WaitForObjects(&second, &play);
            REQUIRE(first.staticState.initialized && second.staticState.initialized);
            REQUIRE(first.skin.skelAnime.jointTable != second.skin.skelAnime.jointTable);
            REQUIRE(first.staticState.mmResourceOwner != second.staticState.mmResourceOwner);
            unsigned beforeInit = initCalls;
            EnViewerStatic_WaitForObjects(&first, &play);
            REQUIRE(beforeInit == initCalls);
            REQUIRE(first.actor.world.pos.y == 345 && first.actor.home.pos.y == 345);
            REQUIRE(first.actor.scale.x == 0.01f && first.actor.shape.yOffset == 0);
            REQUIRE(objectCalls == 0 && gSegments[6] == 0x12345678);
            first.staticState.blinkTimer = 0;
            first.staticState.eyeIndex = 0;
            EnViewer_Update(&first.actor, &play);
            REQUIRE(gSegments[6] == 0x12345678);
            REQUIRE(first.skin.skelAnime.jointTable[0].x == 130 && first.skin.skelAnime.jointTable[0].y == 456);
            REQUIRE(second.skin.skelAnime.jointTable[0].x == 123 && second.staticState.eyeIndex == 0);
            first.staticState.tracking = true;
            first.staticState.interactInfo.headRot.y = 100;
            first.staticState.interactInfo.headRot.x = 50;
            first.staticState.interactInfo.torsoRot.y = 200;
            draw(&first, &play);
            if (a == 0 && pose == 0)
                REQUIRE(rotateX > 0);
            else
                REQUIRE(rotateX == 0);
            first.skin.skelAnime.curFrame = 43;
            draw(&first, &play);
            unsigned frees = freeCalls, releases = releaseCalls;
            EnViewer_Destroy(&first.actor, &play);
            EnViewer_Destroy(&first.actor, &play);
            REQUIRE(freeCalls == frees + 1 && releaseCalls == releases + 1);
            REQUIRE(second.staticState.initialized);
            draw(&second, &play);
            EnViewer_Destroy(&second.actor, &play);
        }
    EnViewer failed;
    prepare(&failed, STATIC_STORY_ACTOR_LULU, 2);
    loadSuccess = false;
    unsigned before = initCalls;
    EnViewerStatic_WaitForObjects(&failed, &play);
    EnViewer_Destroy(&failed.actor, &play);
    REQUIRE(initCalls == before && !failed.staticState.initialized);
    loadSuccess = true;
    allocationSuccess = false;
    prepare(&failed, STATIC_STORY_ACTOR_LULU, 3);
    unsigned frees = freeCalls, releases = releaseCalls, loops = loopCalls;
    EnViewerStatic_WaitForObjects(&failed, &play);
    EnViewer_Destroy(&failed.actor, &play);
    REQUIRE(freeCalls == frees + 1 && releaseCalls == releases + 1 && loopCalls == loops);
    REQUIRE(killCalls == 2 && colliderFrees == 20 && objectCalls == 0);
    allocationSuccess = true;
    R_UPDATE_RATE = 2;
    for (unsigned f = 0; f < 89; ++f) {
        for (unsigned j = 0; j < 66; ++j)
            playerData[f * 67 + j] = f * 100 + j;
        playerData[f * 67 + 66] = 0;
    }
    for (unsigned pose = 0; pose < 2; ++pose) {
        EnViewer a, b;
        prepare(&a, STATIC_STORY_ACTOR_CHILD_KAFEI, pose);
        prepare(&b, STATIC_STORY_ACTOR_CHILD_KAFEI, pose);
        unsigned loopsBefore = loopCalls;
        EnViewerStatic_WaitForObjects(&a, &play);
        EnViewerStatic_WaitForObjects(&b, &play);
        REQUIRE(a.staticState.initialized && b.staticState.initialized && loopCalls == loopsBefore);
        REQUIRE(a.skin.skelAnime.limbCount == 22 && a.skin.skelAnime.jointTable[0].x == 0);
        EnViewer_Update(&a.actor, &play);
        REQUIRE(a.skin.skelAnime.curFrame == 1 && a.skin.skelAnime.jointTable[0].x == 100);
        REQUIRE(b.skin.skelAnime.curFrame == 0 && b.skin.skelAnime.jointTable[0].x == 0);
        REQUIRE(gSegments[6] == 0x12345678 && a.actor.world.pos.y == 345 && a.actor.home.pos.y == 345);
        a.staticState.mmAppearance = 0x48;
        unsigned drawnBefore = lodDraws;
        draw(&a, &play);
        draw(&a, &play);
        REQUIRE(lodDraws == drawnBefore + 2);
        REQUIRE(a.skin.skelAnime.jointTable[0].x == 100);
        unsigned freesBefore = freeCalls, releaseBefore = releaseCalls, drawBefore = lodDraws;
        EnViewer_Destroy(&a.actor, &play);
        EnViewer_Destroy(&a.actor, &play);
        EnViewerStatic_Update(&a, &play);
        draw(&a, &play);
        REQUIRE(freeCalls == freesBefore + 1 && releaseCalls == releaseBefore + 1 && lodDraws == drawBefore);
        draw(&b, &play);
        EnViewer_Destroy(&b.actor, &play);
    }
    for (bool failLoad = false;; failLoad = true) {
        prepare(&failed, STATIC_STORY_ACTOR_CHILD_KAFEI, 0);
        loadSuccess = !failLoad;
        allocationSuccess = false;
        unsigned freesBefore = freeCalls, releaseBefore = releaseCalls;
        EnViewerStatic_WaitForObjects(&failed, &play);
        EnViewer_Destroy(&failed.actor, &play);
        REQUIRE(!failed.staticState.initialized);
        REQUIRE(freeCalls == freesBefore + !failLoad && releaseCalls == releaseBefore + !failLoad);
        if (failLoad)
            break;
    }
    allocationSuccess = true;
    loadSuccess = true;
    testLuluHdBlink(&play);
    testShopGalBlink(&play);
    testShopGalFootAnimations(&play);
    testGreatFairyBlink(&play);
    for (uint8_t pose = 0; pose < 2; ++pose) {
        testAnjuPoseAnchor(&play, pose);
        testAnjuHdBlink(&play, pose);
    }
    puts("PASS Kafei production viewer: no Player calls; private sampling, LOD draw/root/face commands and failure "
         "lifecycle");
    puts("PASS compiled production viewer init/update/draw/free: 10 poses, independent state, root preservation, face "
         "order, typed-load and partial-allocation failure");
    return 0;
}
