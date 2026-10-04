#include "z64.h"
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
}
#include "soh/cvar_prefixes.h"
#include "mods/transformation_masks/transformation_masks.h"
#include "test_require.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "zora_barrier_properties.inc"

// Use the real GBI macros, production draw body and matrix operations. Only
// graphics allocation, interpolation recording and resource services are stubbed;
// the game and pixels are not exercised.
static struct {
    s16 barrierIntensity;
    bool fastSwimActive;
    s16 swimPitch;
    s16 swimRollSmoothed;
    MmPlayerTransformation currentForm;
    bool skeletonLoaded;
    SkelAnime formSkelAnime;
} gFormState;
static std::vector<Gfx> sBarrierDLSafeCopy;
static size_t sBarrierDLCount;
static Gfx sXlu[256], sAllocated[256], sScroll[4];
static Mtx sMatrix;
static GraphicsContext sGfx;
static PlayState sPlay;
static Player sPlayer;
static MtxF sMatrices[2], sDrawMatrix;
static MtxF* sMatrixStack = sMatrices;
static MtxF* sCurrentMatrix = sMatrices;
static int sMatrixCalls;
static bool sChanged[3];
static const Color_RGB8 sColors[] = { { 17, 34, 51 }, { 68, 85, 102 }, { 119, 136, 153 } };
static const char* sNames[] = { "Custom.ZoraMagicShield", "Custom.ZoraMagicShieldGlow",
                                "Custom.ZoraMagicShieldHighlights" };

Gfx gCullBackDList[1];
Gfx gEmptyDL[1];

int32_t CVarGetInteger(const char* key, int32_t fallback) {
    for (int i = 0; i < 3; ++i) {
        if (std::string(key) == std::string("gCosmetics.") + sNames[i] + ".Changed") {
            return sChanged[i];
        }
    }
    return fallback;
}

Color_RGB8 CVarGetColor24(const char* key, Color_RGB8 fallback) {
    for (int i = 0; i < 3; ++i) {
        if (std::string(key) == std::string("gCosmetics.") + sNames[i] + ".Value") {
            return sColors[i];
        }
    }
    return fallback;
}

void FrameInterpolation_RecordMatrixPush(void) {
}
void FrameInterpolation_RecordMatrixPop(void) {
}
void FrameInterpolation_RecordMatrixTranslate(f32, f32, f32, u8) {
}
void FrameInterpolation_RecordMatrixRotate1Coord(u32, f32, u8) {
}
void FrameInterpolation_RecordMatrixScale(f32, f32, f32, u8) {
}
void FrameInterpolation_RecordMatrixRotateZYX(s16, s16, s16, u8) {
}
void FrameInterpolation_RecordMatrixSetTranslateRotateYXZ(f32, f32, f32, Vec3s*) {
}
#include "zora_barrier_matrix.inc"

f32 Math_SinS(s16 value) {
    return sinf(BINANG_TO_RAD(value));
}
f32 Math_CosS(s16 value) {
    return cosf(BINANG_TO_RAD(value));
}
Mtx* Matrix_NewMtx(GraphicsContext*, char*, s32) {
    sDrawMatrix = *sCurrentMatrix;
    ++sMatrixCalls;
    return &sMatrix;
}
void* Graph_Alloc(GraphicsContext*, size_t size) {
    REQUIRE(size <= sizeof(sAllocated));
    return sAllocated;
}
void Gfx_SetupDL_25Xlu(GraphicsContext*) {
}
void FrameInterpolation_RecordOpenChild(const void*, int) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void gSPDisplayList(Gfx* packet, Gfx* list) {
    __gSPDisplayList(packet, list);
}
void gSPSegment(void* packet, int segment, uintptr_t target) {
    __gSPSegment(static_cast<Gfx*>(packet), segment, target);
}
Gfx* Gfx_TwoTexScroll(GraphicsContext*, s32, u32, u32, s32, s32, s32, u32, u32, s32, s32) {
    return sScroll;
}
static void MmForm_PatchSegmentedDL(Gfx*, size_t, u8, Gfx*) {
}
static void MmForm_PatchCullDLIndex(Gfx*, size_t) {
}
void ApplyCustomCosmeticsToDisplayListCopy(const char*, Gfx*, size_t) {
}
#define gLinkZoraBarrierDL "__OTR__objects/object_link_zora/object_link_zora_DL_011760"

#include "zora_barrier_production.inc"

static std::vector<Gfx> NativeColors() {
    return {
        gsDPSetPrimColor(0, 128, 0, 150, 255, 130),   gsDPSetEnvColor(0, 0, 100, 255),   gsDPPipeSync(),
        gsDPSetPrimColor(0, 128, 170, 255, 255, 130), gsDPSetEnvColor(0, 150, 255, 255), gsSPEndDisplayList(),
    };
}

static void Draw(int intensity, bool swimming) {
    sGfx.polyXlu.p = sXlu;
    sPlay.state.gfxCtx = &sGfx;
    gFormState.barrierIntensity = intensity;
    gFormState.fastSwimActive = swimming;
    sMatrixCalls = 0;
    sCurrentMatrix = sMatrixStack;
    SkinMatrix_SetTranslate(sCurrentMatrix, 12.0f, 34.0f, 56.0f);
    const MtxF parent = *sCurrentMatrix;
    sBarrierDLSafeCopy = NativeColors();
    sBarrierDLCount = sBarrierDLSafeCopy.size();
    MmForm_DrawZoraBarrier(&sPlayer, &sPlay);
    REQUIRE(sCurrentMatrix == sMatrixStack);
    REQUIRE(memcmp(sCurrentMatrix, &parent, sizeof(parent)) == 0);
}

static Vec3f BarrierPoint(Vec3f point) {
    return { sDrawMatrix.xw + sDrawMatrix.xx * point.x + sDrawMatrix.xy * point.y + sDrawMatrix.xz * point.z,
             sDrawMatrix.yw + sDrawMatrix.yx * point.x + sDrawMatrix.yy * point.y + sDrawMatrix.yz * point.z,
             sDrawMatrix.zw + sDrawMatrix.zx * point.x + sDrawMatrix.zy * point.y + sDrawMatrix.zz * point.z };
}

static void ExpectPoint(Vec3f point, Vec3f expected) {
    REQUIRE(fabsf(point.x - expected.x) < 0.001f);
    REQUIRE(fabsf(point.y - expected.y) < 0.001f);
    REQUIRE(fabsf(point.z - expected.z) < 0.001f);
}

static void ExpectNativeScale(int intensity) {
    const float expectedScale = intensity * (10.0f / 51.0f) * 0.01f;
    REQUIRE(fabsf(sqrtf(SQ(sDrawMatrix.xx) + SQ(sDrawMatrix.yx) + SQ(sDrawMatrix.zx)) - expectedScale) < 0.00001f);
    REQUIRE(fabsf(sqrtf(SQ(sDrawMatrix.xy) + SQ(sDrawMatrix.yy) + SQ(sDrawMatrix.zy)) - expectedScale) < 0.00001f);
    REQUIRE(fabsf(sqrtf(SQ(sDrawMatrix.xz) + SQ(sDrawMatrix.yz) + SQ(sDrawMatrix.zz)) - expectedScale) < 0.00001f);
}

int main() {
    // MM's model-space scale is intensity * 10/51 with actor scale 0.01.
    for (bool swimming : { false, true }) {
        for (int intensity : { 1, 50, 128, 255 }) {
            Draw(intensity, swimming);
            REQUIRE(sMatrixCalls == 1);
            ExpectNativeScale(intensity);
        }
    }
    Draw(0, false);
    REQUIRE(sMatrixCalls == 0);
    puts("PASS native MM scale on land and in water, charge/fade, and inactive draw");

    // Ground and idle-water draws start at the feet. Native -1800 model units
    // become -18 world Y after the quarter-turn, independent of yaw/charge.
    // The previous +40 chest offset leaves the origin above the feet and fails.
    sPlayer.actor.world.pos = { 137.0f, 82.0f, -211.0f };
    for (s16 yaw : { -32768, -16384, 0, 16384 }) {
        sPlayer.actor.shape.rot.y = yaw;
        for (int intensity : { 1, 50, 128, 255 }) {
            Draw(intensity, false);
            ExpectPoint(BarrierPoint({ 0, 0, 0 }), { 137, 64, -211 });
            REQUIRE(BarrierPoint({ 0, 0, 0 }).y < sPlayer.actor.world.pos.y);
        }
        // At full strength, +36 along the shield's axis reaches the feet plane.
        ExpectPoint(BarrierPoint({ 0, 0, 36 }), { 137, 82, -211 });
    }
    puts("PASS ground/idle shield origin below feet through turns and charge/fade; matrix stack restored");

    // Preserve the fallback used without an active Zora skeleton (human tunic).
    sPlayer.actor.shape.rot.y = 0;
    Draw(255, true);
    ExpectPoint(BarrierPoint({ 0, 0, 0 }), { 137, 122, -171 });
    gFormState.swimPitch = 0x4000;
    Draw(255, true);
    ExpectPoint(BarrierPoint({ 0, 0, 0 }), { 137, 80, -211 });
    gFormState.swimPitch = 0;
    gFormState.swimRollSmoothed = 0x4000;
    sPlayer.actor.shape.rot.y = 0x4000;
    Draw(255, true);
    ExpectPoint(BarrierPoint({ 0, 0, 100 }), { 127, 122, -211 });
    gFormState.swimRollSmoothed = 0;
    sPlayer.actor.shape.rot.y = 0;
    puts("PASS existing fast-swim fallback without an active Zora skeleton");

    // Native MM anchors a swimming Zora's shield to the animated root. These
    // landmarks include a nonzero actor offset, root position and root rotation;
    // actor yaw/pitch alone cannot place or orient this shield correctly.
    Vec3s swimJoints[] = { { 100, 3000, 200 }, { 0x4000, 0, 0 } };
    gFormState.currentForm = MM_PLAYER_FORM_ZORA;
    gFormState.skeletonLoaded = true;
    gFormState.formSkelAnime.jointTable = swimJoints;
    gFormState.formSkelAnime.limbCount = 2;
    sPlayer.actor.scale = { 0.01f, 0.01f, 0.01f };
    sPlayer.actor.shape.yOffset = 100;
    for (int intensity : { 1, 50, 128, 255 }) {
        Draw(intensity, true);
        ExpectNativeScale(intensity);
        ExpectPoint(BarrierPoint({ 0, 0, 0 }), { 138, 73, -209 });
    }
    ExpectPoint(BarrierPoint({ 0, 0, 100 }), { 138, 123, -209 });
    sPlayer.actor.shape.rot.y = 0x4000;
    Draw(255, true);
    ExpectPoint(BarrierPoint({ 0, 0, 0 }), { 139, 73, -212 });
    ExpectPoint(BarrierPoint({ 0, 0, 100 }), { 139, 123, -212 });

    sPlayer.actor.shape.rot.y = 0;
    gFormState.swimPitch = 0x4000;
    Draw(255, true);
    ExpectPoint(BarrierPoint({ 0, 0, 0 }), { 138, 111, -249 });
    ExpectPoint(BarrierPoint({ 0, 0, 100 }), { 138, 111, -199 });
    gFormState.swimPitch = -0x4000;
    Draw(255, true);
    ExpectPoint(BarrierPoint({ 0, 0, 0 }), { 138, 111, -169 });
    ExpectPoint(BarrierPoint({ 0, 0, 100 }), { 138, 111, -219 });

    gFormState.swimPitch = 0;
    gFormState.swimRollSmoothed = 0x4000;
    sPlayer.actor.shape.rot.y = 0x4000;
    Draw(255, true);
    ExpectPoint(BarrierPoint({ 0, 0, 0 }), { 139, 113, -252 });
    ExpectPoint(BarrierPoint({ 0, 0, 100 }), { 139, 113, -202 });

    // A new animation frame must move the anchor, and leaving fast swim must
    // return to the feet instead of retaining a cached swimming pose.
    gFormState.swimRollSmoothed = 0;
    sPlayer.actor.shape.rot.y = 0;
    swimJoints[0] = { 300, 3200, -100 };
    Draw(255, true);
    ExpectPoint(BarrierPoint({ 0, 0, 0 }), { 140, 75, -212 });
    ExpectPoint(BarrierPoint({ 0, 0, 100 }), { 140, 125, -212 });
    Draw(255, false);
    ExpectPoint(BarrierPoint({ 0, 0, 0 }), { 137, 64, -211 });
    puts("PASS animated Zora swim root, yaw, dive/rise, roll, charge/fade and return to feet");

    auto original = NativeColors();
    auto commands = original;
    MmForm_PatchZoraBarrierColors(commands.data(), commands.size());
    REQUIRE(memcmp(commands.data(), original.data(), original.size() * sizeof(Gfx)) == 0);
    for (int i = 0; i < 3; ++i) {
        sChanged[i] = true;
    }
    Draw(255, false);
    REQUIRE(sAllocated[0].words.w1 == 0x11223382);
    REQUIRE(sAllocated[1].words.w1 == 0x445566FF);
    REQUIRE(sAllocated[3].words.w1 == 0x77889982);
    REQUIRE(sAllocated[4].words.w1 == 0x112233FF);
    REQUIRE(sAllocated[0].words.w0 == original[0].words.w0);
    REQUIRE(sAllocated[2].words.w0 == original[2].words.w0);
    REQUIRE(memcmp(sBarrierDLSafeCopy.data(), original.data(), original.size() * sizeof(Gfx)) == 0);
    puts("PASS live main/glow/highlight colors preserve alpha, LOD, and cached source");

    // Hash payloads can look like color opcodes; they must stay byte-identical.
    Gfx hashHeader = {};
    hashHeader.words.w0 = 0x20000000;
    Gfx hashPayload = {};
    hashPayload.words.w0 = 0xFA112233;
    hashPayload.words.w1 = 0x44556677;
    commands = original;
    commands.insert(commands.begin(), { hashHeader, hashPayload });
    MmForm_PatchZoraBarrierColors(commands.data(), commands.size());
    REQUIRE(commands[1].words.w0 == hashPayload.words.w0 && commands[1].words.w1 == hashPayload.words.w1);
    REQUIRE(commands[2].words.w1 == 0x11223382);
    // XML materials emit filepath calls as one command. The next color must
    // still be visited (the shield cosmetic O2R uses these split-list calls).
    for (u8 opcode : { 0x25, 0x27 }) {
        Gfx filepath = {};
        filepath.words.w0 = static_cast<uint32_t>(opcode) << 24;
        filepath.words.w1 = reinterpret_cast<uintptr_t>("objects/shield_layer");
        commands = original;
        commands.insert(commands.begin(), filepath);
        MmForm_PatchZoraBarrierColors(commands.data(), commands.size());
        REQUIRE(commands[1].words.w1 == 0x11223382);
        REQUIRE(commands[0].words.w1 == filepath.words.w1);
    }
    auto unsupported = original;
    unsupported.insert(unsupported.begin(), gsDPSetEnvColor(20, 30, 40, 50));
    commands = unsupported;
    MmForm_PatchZoraBarrierColors(commands.data(), commands.size());
    REQUIRE(memcmp(commands.data(), unsupported.data(), commands.size() * sizeof(Gfx)) == 0);
    for (int i = 0; i < 3; ++i) {
        sChanged[i] = false;
    }
    Draw(255, false);
    REQUIRE(memcmp(sAllocated, original.data(), original.size() * sizeof(Gfx)) == 0);
    puts("PASS hash payloads, unfamiliar mod shaders, and reset to native commands");
}
