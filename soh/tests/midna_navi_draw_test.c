/* The runner inserts the production draw functions unchanged. GPU/resource
 * execution is the boundary; actor types, visibility and GBI commands are real. */
#include <math.h>
#include <string.h>
#include "tests/test_require.h"
#include "src/overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "soh/ResourceManagerHelpers.h"

static Gfx commands[160], resource[1], body[1], markings[1], shimmer[1], allocated[4];
static Gfx blinkHalf[1], blinkClosed[1];
static Vtx pose[1];
static unsigned nativeDraws, midnaSetups, resourceChecks, resourceLoads, pushes, pops;
static int packPresent, loadSucceeds;
static int midnaEnabled;
static int poc2Present, poseMissing, markingsMissing, lastPoseFrame, inputTimer = 17;
static int shimmerVerticesMissing, shimmerVerticesFailed, textureFailed;
static int blinkPresent, blinkMissing, blinkFailed, expectedBlink;
static int inputVisibleTimer;
static u8 inputAlpha = 255;
static f32 fairySize = 1.0f;
static Mtx matrix;
static GameInfo registers;
GameInfo* gGameInfo = &registers;

#define FAIRY_FLAG_BIG (1 << 9)
void func_80A04D90(EnElf* fairy, PlayState* play) {
}
bool ResourceMgr_IsAltAssetsEnabled(void) {
    return false;
}
void lusprintf(const char* file, int32_t line, int32_t level, const char* fmt, ...) {
}

static int blinkResource(const char* path) {
    static const char* paths[] = { "objects/midna_navi/poc2/BlinkHalfDL", "objects/midna_navi/poc2/BlinkClosedDL",
                                   "objects/midna_navi/poc2/DiffuseHalf", "objects/midna_navi/poc2/DiffuseClosed" };
    for (int i = 0; i < 4; ++i)
        if (strcmp(path, paths[i]) == 0)
            return i + 1;
    return 0;
}

uint8_t ResourceMgr_FileExists(const char* path) {
    ++resourceChecks;
    if (strcmp(path, "objects/midna_navi/poc1/MidnaFloatDL") == 0)
        return packPresent;
    int blink = blinkResource(path);
    if (blink)
        return blinkPresent && blinkMissing != blink;
    REQUIRE(strncmp(path, "objects/midna_navi/poc2/", 23) == 0);
    if (strcmp(path, "objects/midna_navi/poc2/ShimmerVertices") == 0 && shimmerVerticesMissing)
        return 0;
    return poc2Present;
}
Gfx* ResourceMgr_LoadGfxByName(const char* path) {
    ++resourceLoads;
    int blink = blinkResource(path);
    if (blink == 1 || blink == 2)
        return blinkFailed == blink ? NULL : (blink == 1 ? blinkHalf : blinkClosed);
    if (strcmp(path, "objects/midna_navi/poc1/MidnaFloatDL") == 0)
        return loadSucceeds ? resource : NULL;
    if (strcmp(path, "objects/midna_navi/poc2/BodyDL") == 0)
        return body;
    if (strcmp(path, "objects/midna_navi/poc2/MarkingsDL") == 0)
        return markingsMissing ? NULL : markings;
    REQUIRE(strcmp(path, "objects/midna_navi/poc2/ShimmerDL") == 0);
    return shimmer;
}
Vtx* ResourceMgr_LoadVtxByName(char* path) {
    if (strcmp(path, "objects/midna_navi/poc2/ShimmerVertices") == 0)
        return shimmerVerticesFailed ? NULL : pose;
    REQUIRE(sscanf(path, "objects/midna_navi/poc2/Pose%d", &lastPoseFrame) == 1);
    REQUIRE(lastPoseFrame >= 0 && lastPoseFrame < 64);
    return poseMissing ? NULL : pose;
}
char* ResourceMgr_GetResourceDataByNameHandlingMQ(const char* path) {
    int blink = blinkResource(path);
    if (blink == 3 || blink == 4)
        return blinkFailed == blink ? NULL : (char*)pose;
    REQUIRE(strcmp(path, "objects/midna_navi/poc2/DiffuseNeutral") == 0 ||
            strcmp(path, "objects/midna_navi/poc2/MarkingsMask") == 0);
    return textureFailed ? NULL : (char*)pose;
}
void* Graph_Alloc(GraphicsContext* gfx, size_t size) {
    REQUIRE(size == sizeof(allocated));
    return allocated;
}
void Gfx_SetupDL_27Xlu(GraphicsContext* gfx) {
}
void Gfx_SetupDL_25Xlu(GraphicsContext* gfx) {
    ++midnaSetups;
}
void FrameInterpolation_RecordOpenChild(const void* source, int line) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void gSPSegment(void* command, int segment, uintptr_t target) {
    __gSPSegment((Gfx*)command, segment, target);
}
void gSPDisplayList(Gfx* command, Gfx* list) {
    __gSPDisplayList(command, list);
}
void Matrix_Push(void) {
    ++pushes;
}
void Matrix_Pop(void) {
    ++pops;
}
void Matrix_RotateZ(f32 angle, u8 mode) {
    REQUIRE(isfinite(angle) && fabsf(angle) <= 0.04f);
}
void Matrix_Translate(f32 x, f32 y, f32 z, u8 mode) {
    REQUIRE(isfinite(x) && isfinite(y) && isfinite(z));
    REQUIRE(y < 0); // motes stay below the face/helmet
}
void Matrix_ReplaceRotation(MtxF* matrix) {
}
f32 Math_CosS(s16 angle) {
    return cosf(angle * (3.14159265358979323846f / 32768.0f));
}
s16 Math_Atan2S(f32 y, f32 x) {
    return 0;
}
void Actor_SetScale(Actor* actor, f32 scale) {
    actor->scale.x = actor->scale.y = actor->scale.z = scale;
}
void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode) {
    /* POC1's packed 4471-unit height becomes 21.4608 world units at
     * native Navi scale 0.008. Apply the same factor on every axis. */
    REQUIRE(fabsf(x * 4471.0f * 0.008f - 21.4608f * fairySize) < 0.001f);
    REQUIRE(x == y && y == z && mode == MTXMODE_APPLY);
}
Mtx* Matrix_NewMtx(GraphicsContext* gfx, char* file, s32 line) {
    return &matrix;
}
f32 Math_SinS(s16 angle) {
    return sinf(angle * (3.14159265358979323846f / 32768.0f));
}
int32_t CVarGetInteger(const char* name, int32_t defaultValue) {
    REQUIRE(strcmp(name, CVAR_ENHANCEMENT("MidnaCompanion")) == 0);
    REQUIRE(defaultValue == 0);
    return midnaEnabled;
}
float CVarGetFloat(const char* name, float defaultValue) {
    return fairySize;
}
s32 EnElf_OverrideLimbDraw(PlayState* play, s32 limb, Gfx** dl, Vec3f* p, Vec3s* r, void* actor, Gfx** gfx) {
    return 0;
}
Gfx* SkelAnime_DrawSkeleton2(PlayState* play, SkelAnime* skel, OverrideLimbDrawOpa override, PostLimbDrawOpa post,
                             void* actor, Gfx* gfx) {
    ++nativeDraws;
    return gfx;
}

/* PRODUCTION_MIDNA_DRAW */

static void checkCase(int fairyType, int present, int loaded, int hiddenState, int hiddenFlag, int firstPerson,
                      int inFront, int expectMidna, int expectNative) {
    static PlayState play;
    static Player player;
    GraphicsContext gfx = { 0 };
    EnElf fairy = { 0 };
    memset(&play, 0, sizeof(play));
    memset(&player, 0, sizeof(player));
    memset(commands, 0, sizeof(commands));
    gfx.polyXlu.p = commands;
    play.state.gfxCtx = &gfx;
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    player.stateFlags1 = firstPerson ? PLAYER_STATE1_FIRST_PERSON : 0;
    fairy.actor.params = fairyType;
    fairy.actor.projectedPos.z = inFront ? 1000 : -1000;
    fairy.actor.scale.x = fairy.actor.scale.y = fairy.actor.scale.z = 0.008f;
    fairy.unk_2A8 = hiddenState ? 8 : 0;
    fairy.fairyFlags = hiddenFlag ? 8 : 0;
    fairy.innerColor.a = inputAlpha;
    fairy.innerColor.r = 217;
    fairy.innerColor.g = 43;
    fairy.innerColor.b = 91;
    fairy.outerColor.r = 17;
    fairy.outerColor.g = 101;
    fairy.outerColor.b = 233;
    fairy.timer = inputTimer;
#ifdef MIDNA_VISIBLE_CLOCK
    fairy.midnaBlinkTimer = inputVisibleTimer;
#endif
    packPresent = present;
    loadSucceeds = loaded;
    nativeDraws = midnaSetups = resourceChecks = resourceLoads = pushes = pops = 0;
    EnElf before = fairy;
    Player playerBefore = player;
    EnElf_Draw(&fairy.actor, &play);
    REQUIRE(nativeDraws == expectNative);
    REQUIRE(midnaSetups == expectMidna);
    int enhanced = poc2Present && !poseMissing && !markingsMissing && !shimmerVerticesMissing &&
                   !shimmerVerticesFailed && !textureFailed && expectMidna;
    REQUIRE(pushes == pops && pushes == expectMidna + (enhanced ? 6 : 0));
    REQUIRE(memcmp(&before, &fairy, sizeof(fairy)) == 0);
    REQUIRE(memcmp(&playerBefore, &player, sizeof(player)) == 0);
    if (!midnaEnabled || fairyType != FAIRY_NAVI || hiddenState || hiddenFlag || (firstPerson && !inFront)) {
        REQUIRE(resourceChecks == 0 && resourceLoads == 0);
    }
    if (expectMidna) {
        unsigned found = 0, markingsDraws = 0, shimmerDraws = 0, primary = 0, secondary = 0;
        Gfx* expectedModel = enhanced ? (expectedBlink == 1   ? blinkHalf
                                         : expectedBlink == 2 ? blinkClosed
                                                              : body)
                                      : resource;
        uintptr_t renderMode = 0;
        for (Gfx* cmd = commands; cmd < gfx.polyXlu.p; ++cmd) {
            if (cmd->words.w0 == 0xE200001C)
                renderMode = cmd->words.w1;
            if ((cmd->words.w0 >> 24) == G_DL && cmd->words.w1 == (uintptr_t)expectedModel)
                ++found;
            if ((cmd->words.w0 >> 24) == G_DL && cmd->words.w1 == (uintptr_t)markings)
                ++markingsDraws;
            if ((cmd->words.w0 >> 24) == G_DL && cmd->words.w1 == (uintptr_t)shimmer) {
                REQUIRE(renderMode == (G_RM_PASS | G_RM_AA_ZB_XLU_SURF2));
                ++shimmerDraws;
            }
            if ((cmd->words.w0 >> 24) == G_SETPRIMCOLOR && (cmd->words.w1 >> 8) == 0xD92B5B)
                ++primary;
            if ((cmd->words.w0 >> 24) == G_SETPRIMCOLOR && (cmd->words.w1 >> 8) == 0x1165E9)
                ++secondary;
            if (cmd->words.w0 == 0xE200001C && inputAlpha < 255)
                REQUIRE(!(cmd->words.w1 & Z_UPD));
        }
        REQUIRE(found == 1);
        REQUIRE(markingsDraws == enhanced && shimmerDraws == (enhanced ? 6 : 0));
        if (enhanced)
            REQUIRE(primary == 1 && secondary == 1);
    }
}

static void checkLights(int fairyType, int present, int loaded, int hidden, int expectType) {
    static PlayState play;
    EnElf fairy = { 0 };
    fairy.actor.params = fairyType;
    fairy.unk_2A8 = hidden ? 8 : 0;
    packPresent = present;
    loadSucceeds = loaded;
    EnElf_UpdateLights(&fairy, &play);
    REQUIRE(fairy.lightInfoGlow.type == expectType);
    REQUIRE(fairy.lightInfoGlow.params.point.radius == (hidden ? 0 : 100));
}

static void idleAction(EnElf* fairy, PlayState* play) {
}

static void checkVisibleClock(void) {
    static PlayState play;
    EnElf fairy = { 0 };
    fairy.actor.params = FAIRY_NAVI;
    fairy.actor.scale.x = 0.008f;
    fairy.innerColor.a = 255;
    fairy.actionFunc = idleAction;
    fairy.unk_2A8 = 8;
    for (int i = 0; i < 80; ++i)
        EnElf_Update(&fairy.actor, &play);
    REQUIRE(fairy.timer == 80);
    inputTimer = fairy.timer;
#ifdef MIDNA_VISIBLE_CLOCK
    REQUIRE(fairy.midnaBlinkTimer == 0);
    inputVisibleTimer = fairy.midnaBlinkTimer;
#endif
    /* Time inside Link must not consume a blink or emerge with a closed eye. */
    expectedBlink = 0;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    fairy.unk_2A8 = 0;
    for (int i = 1; i <= 230; ++i) {
        EnElf_Update(&fairy.actor, &play);
        inputTimer = fairy.timer;
#ifdef MIDNA_VISIBLE_CLOCK
        REQUIRE(fairy.midnaBlinkTimer == i % 200);
        inputVisibleTimer = fairy.midnaBlinkTimer;
#endif
        int phase = i % 200;
        int blink = phase >= 116 ? phase - 116 : phase - 24;
        expectedBlink = blink < 0 || blink > 5 ? 0 : (blink == 2 || blink == 3 ? 2 : 1);
        checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    }
#ifdef MIDNA_VISIBLE_CLOCK
    u16 held = fairy.midnaBlinkTimer;
    midnaEnabled = 0;
    EnElf_Update(&fairy.actor, &play);
    REQUIRE(fairy.midnaBlinkTimer == held);
    midnaEnabled = 1;
    fairy.fairyFlags = 8;
    EnElf_Update(&fairy.actor, &play);
    REQUIRE(fairy.midnaBlinkTimer == held);
    fairy.fairyFlags = 0;
    fairy.actor.scale.x = 0.001f;
    EnElf_Update(&fairy.actor, &play);
    REQUIRE(fairy.midnaBlinkTimer == held);
    fairy.actor.scale.x = 0.008f;
    fairy.innerColor.a = 0;
    EnElf_Update(&fairy.actor, &play);
    REQUIRE(fairy.midnaBlinkTimer == held);
    fairy.innerColor.a = 255;
    fairy.actor.params = FAIRY_HEAL;
    EnElf_Update(&fairy.actor, &play);
    REQUIRE(fairy.midnaBlinkTimer == held);
#endif
    puts("PASS: hidden, zero-alpha and tiny emergence frames do not consume the blink cycle");
}

int main(void) {
    /* An installed pack is inactive until explicitly enabled; disabling also
     * restores the native halo without touching actor or player state. */
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 0, 1);
    checkLights(FAIRY_NAVI, 1, 1, 0, LIGHT_POINT_GLOW);
    midnaEnabled = 1;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    midnaEnabled = 0;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 0, 1);
    checkLights(FAIRY_NAVI, 1, 1, 0, LIGHT_POINT_GLOW);
    midnaEnabled = 1;
    /* Installed pack selects Midna only for the companion fairy. */
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    fairySize = 1.5f;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    fairySize = 1.0f;
    for (int type = FAIRY_REVIVE_BOTTLE; type <= FAIRY_HEAL_BIG; ++type)
        checkCase(type, 1, 1, 0, 0, 0, 1, 0, 1);
    /* Removing the pack or a failed load retains the native draw. */
    checkCase(FAIRY_NAVI, 0, 1, 0, 0, 0, 1, 0, 1);
    checkCase(FAIRY_NAVI, 1, 0, 0, 0, 0, 1, 0, 1);
    checkCase(FAIRY_NAVI, 1, 1, 1, 0, 0, 1, 0, 0);
    checkCase(FAIRY_NAVI, 1, 1, 0, 1, 0, 1, 0, 0);
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 1, 0, 0, 0);
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 1, 1, 1, 0);
    /* Simulated resource availability changes check fallback without pointer caching.
     * This does not claim support for mounting new archives without restarting. */
    checkCase(FAIRY_NAVI, 0, 0, 0, 0, 0, 1, 0, 1);
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    inputAlpha = 128;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    inputAlpha = 0;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 0, 0);
    inputAlpha = 255;
    poc2Present = 1;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    REQUIRE(lastPoseFrame == 17);
    inputTimer = 33;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    REQUIRE(lastPoseFrame == 33);
    poseMissing = 1;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    poseMissing = 0;
    markingsMissing = 1;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    markingsMissing = 0;
    shimmerVerticesMissing = 1;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    shimmerVerticesMissing = 0;
    shimmerVerticesFailed = 1;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    shimmerVerticesFailed = 0;
    textureFailed = 1;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    textureFailed = 0;
    checkCase(FAIRY_HEAL, 1, 1, 0, 0, 0, 1, 0, 1);
    inputAlpha = 128;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    checkLights(FAIRY_NAVI, 1, 1, 0, LIGHT_POINT_NOGLOW);
    checkLights(FAIRY_NAVI, 1, 1, 1, LIGHT_POINT_NOGLOW);
    checkLights(FAIRY_NAVI, 0, 0, 0, LIGHT_POINT_GLOW);
    checkLights(FAIRY_NAVI, 1, 0, 0, LIGHT_POINT_GLOW);
    checkLights(FAIRY_HEAL, 1, 1, 0, LIGHT_POINT_GLOW);
    /* A complete blink is brief, then the original eye returns. Repeated draw
     * calls use the same state; the render path must not advance actor time. */
    inputAlpha = 255;
    blinkPresent = 1;
    checkVisibleClock();
    const int blinkSequence[][2] = { { 23, 0 },  { 24, 1 },  { 25, 1 },  { 26, 2 },  { 27, 2 },  { 28, 1 },
                                     { 29, 1 },  { 30, 0 },  { 115, 0 }, { 116, 1 }, { 117, 1 }, { 118, 2 },
                                     { 119, 2 }, { 120, 1 }, { 121, 1 }, { 122, 0 }, { 199, 0 }, { 0, 0 } };
    for (unsigned i = 0; i < sizeof(blinkSequence) / sizeof(blinkSequence[0]); ++i) {
        inputTimer = 1400 + i;
        inputVisibleTimer = blinkSequence[i][0];
        expectedBlink = blinkSequence[i][1];
        checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
        checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    }
    /* Missing or unloadable blink resources leave the open POC2 eye intact. */
    inputTimer = 1500;
    inputVisibleTimer = 26;
    expectedBlink = 0;
    blinkPresent = 0;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    blinkPresent = 1;
    for (int i = 1; i <= 4; ++i) {
        blinkMissing = i;
        checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
        blinkMissing = 0;
        blinkFailed = i;
        checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
        blinkFailed = 0;
    }
    puts("PASS: occasional blink sequence, unchanged actor clock, and open-eye fallback");
    puts("PASS: Midna is Navi-only; fallback, visibility, fade depth, actor state and matrix balance");
    return 0;
}
