// The runner inserts production draw bracketing unchanged; RM and GPU are the boundaries.
#include "functions.h"
#include "macros.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/color.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#define CHECK(value)                                                      \
    do {                                                                  \
        if (!(value)) {                                                   \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); \
            std::exit(1);                                                 \
        }                                                                 \
    } while (0)

/* PRODUCTION_CONSTANTS */
/* PRODUCTION_SWAP */
static std::vector<LimbSwap> sSwaps;
static bool sActive = false;
static unsigned selected[2]{};
static unsigned allocations = 0, cacheCalls = 0, lastYoung = 0;
static Gfx commands[128], allocated[128], replacement[1];
static uintptr_t segments[16]{};
static bool supported = true;

struct FakeRM {
    bool OtrSignatureCheck(const char* path) {
        CHECK(reinterpret_cast<uintptr_t>(path) > 0x0FFFFFFF);
        return std::strncmp(path, "__OTR__", 7) == 0;
    }
};
static auto OwnResourceManager() {
    return std::make_shared<FakeRM>();
}
static Gfx* NativeList(const std::shared_ptr<FakeRM>&, const char*, bool young, unsigned changed) {
    ++cacheCalls;
    lastYoung = young;
    CHECK(changed == selected[young]);
    return supported ? replacement : nullptr;
}
extern "C" int32_t CVarGetInteger(const char* name, int32_t fallback) {
    for (unsigned young = 0; young < 2; ++young) {
        for (unsigned part = 0; part < 3; ++part) {
            if (std::strcmp(name, kChangedCVars[young][part]) == 0)
                return (selected[young] >> part) & 1;
        }
    }
    return fallback;
}
extern "C" Color_RGBA8 CVarGetColor(const char*, Color_RGBA8 fallback) {
    return fallback;
}
extern "C" void* Graph_Alloc(GraphicsContext*, size_t size) {
    CHECK(size <= sizeof(allocated));
    ++allocations;
    return allocated;
}
extern "C" void FrameInterpolation_RecordOpenChild(const void*, int) {
}
extern "C" void FrameInterpolation_RecordCloseChild(void) {
}
extern "C" void gSPSegment(void* command, int segment, uintptr_t target) {
    __gSPSegment(static_cast<Gfx*>(command), segment, target);
    segments[segment] = target;
}
extern "C" void gSPDisplayList(Gfx* command, Gfx* target) {
    __gSPDisplayList(command, target);
}

/* PRODUCTION_DRAW */

static void Reset(PlayState& play, GraphicsContext& gfx) {
    CHECK(!sActive && sSwaps.empty());
    allocations = cacheCalls = 0;
    supported = true;
    std::memset(segments, 0, sizeof(segments));
    std::memset(commands, 0, sizeof(commands));
    gfx.polyOpa.p = commands;
    play.state.gfxCtx = &gfx;
}
int main() {
    static PlayState play{};
    GraphicsContext gfx{};
    static const char adult[] = "__OTR__objects/object_horse/gEponaBodyLimbSkinLimbDL_00B7C0";
    static const char young[] = "__OTR__objects/object_horse_link_child/gChildEponaSkelLimbsLimb_007858DL_000C70";
    SkinAnimatedLimbData data{};
    data.dlist = reinterpret_cast<Gfx*>(const_cast<char*>(adult));
    SkinLimb body{}, head{};
    body.segmentType = SKIN_LIMB_TYPE_ANIMATED;
    body.segment = &data;
    head.segmentType = SKIN_LIMB_TYPE_NORMAL;
    head.segment = const_cast<char*>(adult);
    SkinLimb* limbs[] = { &body, &head };
    SkeletonHeader header{};
    header.segment = reinterpret_cast<void**>(limbs);
    header.limbCount = 2;
    Skin skin{};
    skin.skeletonHeader = &header;

    Reset(play, gfx);
    EponaCosmetics_BeginDraw(&play, &skin, 0);
    EponaCosmetics_EndDraw(&play);
    CHECK(allocations == 0 && cacheCalls == 0 && gfx.polyOpa.p == commands);
    CHECK(data.dlist == reinterpret_cast<Gfx*>(const_cast<char*>(adult)) && head.segment == adult);

    selected[0] = 1;
    Reset(play, gfx);
    EponaCosmetics_BeginDraw(&play, &skin, 0);
    CHECK(allocations == 1 && cacheCalls == 2 && sActive);
    CHECK(data.dlist == replacement && head.segment == replacement);
    CHECK(segments[9] && segments[10] && segments[11] && segments[12] && !segments[8]);
    EponaCosmetics_EndDraw(&play);
    CHECK(data.dlist == reinterpret_cast<Gfx*>(const_cast<char*>(adult)) && head.segment == adult);
    CHECK((gfx.polyOpa.p[-1].words.w0 >> 24) == G_SETGRAYSCALE && !gfx.polyOpa.p[-1].words.w1);

    Reset(play, gfx);
    supported = false;
    EponaCosmetics_BeginDraw(&play, &skin, 0);
    EponaCosmetics_EndDraw(&play);
    CHECK(!allocations && gfx.polyOpa.p == commands);
    head.segment = reinterpret_cast<void*>(0x06000001);
    data.dlist = nullptr;
    Reset(play, gfx);
    EponaCosmetics_BeginDraw(&play, &skin, 0);
    EponaCosmetics_EndDraw(&play);
    CHECK(!allocations && !cacheCalls);

    data.dlist = reinterpret_cast<Gfx*>(const_cast<char*>(young));
    head.segment = const_cast<char*>(young);
    Reset(play, gfx);
    EponaCosmetics_BeginDraw(&play, &skin, 1);
    EponaCosmetics_EndDraw(&play);
    CHECK(!allocations && !cacheCalls); // Adult setting must not color young Epona.
    selected[1] = 6;
    Reset(play, gfx);
    EponaCosmetics_BeginDraw(&play, &skin, 1);
    EponaCosmetics_EndDraw(&play);
    CHECK(allocations == 1 && lastYoung == 1 && data.dlist == reinterpret_cast<Gfx*>(const_cast<char*>(young)));

    data.dlist = reinterpret_cast<Gfx*>(const_cast<char*>("__OTR__objects/object_horse/tp_poc2/BodyDL"));
    head.segment = nullptr;
    Reset(play, gfx);
    EponaCosmetics_BeginDraw(&play, &skin, 0);
    EponaCosmetics_EndDraw(&play);
    CHECK(!allocations && !cacheCalls); // The sacred original POC3 has no dispatch protocol.

    data.dlist = reinterpret_cast<Gfx*>(const_cast<char*>(kTPBody));
    selected[0] = 0;
    Reset(play, gfx);
    EponaCosmetics_BeginDraw(&play, &skin, 0);
    CHECK(allocations == 1 && !cacheCalls);
    CHECK((reinterpret_cast<Gfx*>(segments[13])->words.w0 >> 24) == G_ENDDL);
    CHECK((reinterpret_cast<Gfx*>(segments[14])->words.w0 >> 24) == G_ENDDL);
    EponaCosmetics_EndDraw(&play);
    selected[0] = 7;
    Reset(play, gfx);
    EponaCosmetics_BeginDraw(&play, &skin, 0);
    CHECK(reinterpret_cast<Gfx*>(segments[13])[1].words.w1 == reinterpret_cast<uintptr_t>(kTPOverlays[0]));
    CHECK(reinterpret_cast<Gfx*>(segments[14])[1].words.w1 == reinterpret_cast<uintptr_t>(kTPOverlays[1]));
    EponaCosmetics_EndDraw(&play);
    CHECK(data.dlist == reinterpret_cast<Gfx*>(const_cast<char*>(kTPBody)));
    std::puts("PASS: production SoH draw defaults/reset, age independence, pointer restoration, no tint leak, "
              "unsupported pointers/POC3, and POC4 no-op/dispatch");
}
