// Reversing the entire limb turns the child's palm back through the wrist.
// A half-turn also misses the native ceremonial sword's grip, blade and guard
// axes. Exercise the real final override and real GBI output against landmarks
// from the matching native adult/child sword vertices, not guessed angles.
#include "test_require.h"
#include <libultraship/libultra/gbi.h>
#include <cmath>
#include <cstring>
#include <vector>

struct Vec3s {
    s16 x, y, z;
};
struct Vec3f {
    float x, y, z;
};
struct PlayState {
    struct {
        void* gfxCtx;
    } state;
};
struct Player {
    Gfx** leftHandDLists;
};
enum { PLAYER_LIMB_L_HAND, PLAYER_LIMB_L_FOREARM };
enum { BG_TOKI_SWD_HAND_UNCHANGED, BG_TOKI_SWD_HAND_MASTER_SWORD, BG_TOKI_SWD_HAND_CLOSED };
static bool adult, altAssets = true, customAsset, altNative;
static s32 handState = BG_TOKI_SWD_HAND_MASTER_SWORD, sDListsLodOffset;
static Gfx childHand, adultHand, pakHand, pakSword, customSword, nativeSword, alternateSword, unrelated;
static Gfx* selectedPakSword;
static Gfx* selectedPakHand;
static std::vector<void*> allocations;
#define LINK_IS_ADULT adult
#define CVAR_SETTING(name) name
#define PAK_DL_STUB ((Gfx*)(uintptr_t)1)
static constexpr float HAND_COUNTER_SCALE_Y_OFFSET = 100.0f;
static const char gLinkChildLeftFistNearDL[] = "child-fist";
static const char gLinkAdultLeftHandClosedNearDL[] = "adult-fist";
static const char gCustomMasterSwordDL[] = "custom-sword";
static const char gLinkChildLeftHandHoldingMasterSwordDL[] = "child-ceremony";
static const char gLinkAdultLeftHandHoldingMasterSwordNearDL[] = "adult-ceremony-near";
static const char gLinkAdultLeftHandHoldingMasterSwordFarDL[] = "adult-ceremony-far";
static const char* ResolveCustomFPSHand(const char* path) {
    return path;
}
static bool ResourceMgr_FileAltExists(const char* path) {
    return customAsset && path == gCustomMasterSwordDL;
}
static bool ResourceGetIsCustomByName(const char*) {
    return false;
}
static bool TransformMasks_IsTransformedAny() {
    return false;
}
static s32 CVarGetInteger(const char*, s32) {
    return altAssets;
}
static Gfx* ResourceMgr_LoadGfxByName(const char* path) {
    if (path == gLinkChildLeftFistNearDL)
        return &childHand;
    if (path == gLinkAdultLeftHandClosedNearDL)
        return &adultHand;
    REQUIRE(path == gCustomMasterSwordDL);
    return &customSword;
}
static Gfx* PakLoader_GetTimePedestalSwordDL() {
    return selectedPakSword;
}
static Gfx* PakLoader_GetTimePedestalHandDL() {
    return selectedPakHand;
}
static Gfx* PakLoader_GetEquipDL(Player*, s32) {
    return selectedPakHand;
}
static s32 BgTokiSwd_GetTimePedestalHandState(PlayState*, Player*) {
    return handState;
}
static Gfx* Player_ResolveLimbDLForDummyOrLocal(void* resource) {
    if (resource == &childHand || resource == &adultHand)
        return static_cast<Gfx*>(resource);
    REQUIRE(resource == gLinkChildLeftHandHoldingMasterSwordDL ||
            resource == gLinkAdultLeftHandHoldingMasterSwordNearDL ||
            resource == gLinkAdultLeftHandHoldingMasterSwordFarDL);
    return altNative ? &alternateSword : &nativeSword;
}
static void* Graph_Alloc(void*, size_t size) {
    void* result = calloc(1, size);
    REQUIRE(result != nullptr);
    allocations.push_back(result);
    return result;
}
#include "pedestal_matrix.inc"
static Mtx* Matrix_MtxFToMtx(MtxF* source, Mtx* destination) {
    // Pack through the production conversion used by SoH's fixed-point GBI.
    guMtxF2L(source->mf, destination);
    return destination;
}
static void gSPDisplayList(Gfx* command, Gfx* resource) {
    // All fixture resources are already loaded Gfx pointers, so the engine's
    // OTR-path resolver takes its ordinary G_DL branch.
    __gSPDisplayList(command, resource);
}
#include "pedestal_child_grip.inc"

static MtxF ParentLimbMatrix() {
    // A 90-degree Y rotation plus translation exposes reversed multiplication.
    MtxF result = { { { 0, 0, -1, 0 }, { 0, 1, 0, 0 }, { 1, 0, 0, 0 }, { 5, 9, 13, 1 } } };
    return result;
}
static Vec3f Transform(const MtxF& m, Vec3f v) {
    return { m.xx * v.x + m.xy * v.y + m.xz * v.z + m.xw, m.yx * v.x + m.yy * v.y + m.yz * v.z + m.yw,
             m.zx * v.x + m.zy * v.y + m.zz * v.z + m.zw };
}
static void ExpectPoint(Vec3f actual, Vec3f expected, float tolerance) {
    REQUIRE(fabsf(actual.x - expected.x) <= tolerance);
    REQUIRE(fabsf(actual.y - expected.y) <= tolerance);
    REQUIRE(fabsf(actual.z - expected.z) <= tolerance);
}
struct Draw {
    Gfx* resource;
    MtxF matrix;
};

static void MultiplyMatrices(float result[4][4], const float a[4][4], const float b[4][4]) {
    float tmp[4][4];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            tmp[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        }
    }
    memcpy(result, tmp, sizeof(tmp));
}

static std::vector<Draw> ExecuteComposite(Gfx* dl) {
    std::vector<Draw> draws;
    std::vector<MtxF> stack;
    MtxF current = ParentLimbMatrix();
    for (s32 index = 0; index < 12; ++index) {
        const Gfx& command = dl[index];
        switch (command.words.w0 >> 24) {
            case G_DL:
                draws.push_back({ reinterpret_cast<Gfx*>(command.words.w1), current });
                break;
            case G_MTX: {
                const u32 flags = (command.words.w0 & 0xFF) ^ G_MTX_PUSH;
                REQUIRE(flags == (G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW));
                stack.push_back(current);
                // Decode the same packed MtxS format consumed by the engine,
                // then apply the local sword matrix before the parent limb.
                const int32_t* addr = reinterpret_cast<const int32_t*>(command.words.w1);
                float matrix[4][4];
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; j += 2) {
                        int32_t intPart = addr[i * 2 + j / 2];
                        uint32_t fracPart = addr[8 + i * 2 + j / 2];
                        matrix[i][j] = (int32_t)((intPart & 0xffff0000) | (fracPart >> 16)) / 65536.0f;
                        matrix[i][j + 1] = (int32_t)((intPart << 16) | (fracPart & 0xffff)) / 65536.0f;
                    }
                }
                MultiplyMatrices(current.mf, matrix, current.mf);
                break;
            }
            case G_POPMTX:
                REQUIRE(!stack.empty());
                current = stack.back();
                stack.pop_back();
                break;
            case G_ENDDL:
                REQUIRE(stack.empty());
                ExpectPoint(Transform(current, { 17, 29, 43 }), Transform(ParentLimbMatrix(), { 17, 29, 43 }), 0.0001f);
                return draws;
            default:
                REQUIRE(false);
        }
    }
    REQUIRE(false);
    return draws;
}
static void ExpectNativeSwordBasis(const MtxF& matrix) {
    // Native adult object_link_boyVtx_010EE8[0..135] and child
    // object_link_childVtx_00D4E0[27..162] have matching triangle topology.
    // These literal correspondences cover blade tip, pommel, guard and grip.
    static const Vec3f adultPoints[] = { { 3387, 328, -77 }, { -787, 328, -152 }, { 565, -305, -78 },
                                         { 565, 962, -78 },  { 327, 328, -153 },  { 432, 513, -78 } };
    static const Vec3f childPoints[] = { { -3171, -784, -41 }, { 788, 543, -48 },  { -389, -203, 496 },
                                         { -575, 344, -633 },  { -275, 206, -36 }, { -382, 190, -234 } };
    for (size_t i = 0; i < sizeof(adultPoints) / sizeof(adultPoints[0]); ++i)
        ExpectPoint(Transform(matrix, adultPoints[i]), Transform(ParentLimbMatrix(), childPoints[i]), 1.1f);
}
static void CheckSelectedSword(Gfx* expectedHand, Gfx* expectedSword) {
    PlayState play = {};
    Player player = {};
    const Vec3s original = { 0x1234, 0x2345, 0x3456 };
    Vec3s rot = original;
    Gfx* dl = &unrelated;
    Player_ApplyTimePedestalSword(&play, &player, PLAYER_LIMB_L_HAND, &dl, &rot);
    REQUIRE(rot.x == original.x && rot.y == original.y && rot.z == original.z);
    const auto draws = ExecuteComposite(dl);
    REQUIRE(draws.size() == 2);
    REQUIRE(draws[0].resource == expectedHand && draws[1].resource == expectedSword);
    // Actual ordinary child fist fingertip: keep its palm beyond the +Y wrist,
    // as both native fist and native ceremonial hand meshes are authored.
    ExpectPoint(Transform(draws[0].matrix, { 73, 499, -80 }), Transform(ParentLimbMatrix(), { 73, 499, -80 }), 0.0001f);
    if (adult)
        ExpectPoint(Transform(draws[1].matrix, { 3387, 328, -77 }), Transform(ParentLimbMatrix(), { 3387, 328, -77 }),
                    0.0001f);
    else
        ExpectNativeSwordBasis(draws[1].matrix);
    rot = original;
    dl = &unrelated;
    Player_ApplyTimePedestalSword(&play, &player, PLAYER_LIMB_L_FOREARM, &dl, &rot);
    REQUIRE(dl == &unrelated && memcmp(&rot, &original, sizeof(rot)) == 0);
}
int main() {
    selectedPakSword = &pakSword;
    CheckSelectedSword(&childHand, &pakSword);
    selectedPakHand = &pakHand;
    CheckSelectedSword(&pakHand, &pakSword);
    selectedPakHand = PAK_DL_STUB;
    CheckSelectedSword(&childHand, &pakSword);
    adult = true;
    CheckSelectedSword(&adultHand, &pakSword);
    selectedPakHand = &pakHand;
    CheckSelectedSword(&pakHand, &pakSword);
    adult = false;
    selectedPakSword = selectedPakHand = nullptr;
    customAsset = true;
    CheckSelectedSword(&childHand, &customSword);
    selectedPakSword = &pakSword; // Selected PAK still wins over Alt custom sword.
    CheckSelectedSword(&childHand, &pakSword);
    selectedPakSword = nullptr;
    PlayState play = {};
    Gfx* emptyHand[] = { &childHand };
    Player player = { emptyHand };
    for (bool alternate : { false, true }) {
        altAssets = false;
        altNative = alternate;
        Gfx* dl = nullptr;
        Vec3s rot = { 1, 2, 3 };
        Player_ApplyTimePedestalSword(&play, &player, PLAYER_LIMB_L_HAND, &dl, &rot);
        REQUIRE(dl == (alternate ? &alternateSword : &nativeSword));
        REQUIRE(rot.x == 1 && rot.y == 2 && rot.z == 3);
    }
    handState = BG_TOKI_SWD_HAND_CLOSED;
    Gfx* dl = nullptr;
    Vec3s rot = { 1, 2, 3 };
    Player_ApplyTimePedestalSword(&play, &player, PLAYER_LIMB_L_HAND, &dl, &rot);
    REQUIRE(dl == &childHand && rot.x == 1 && rot.y == 2 && rot.z == 3);
    handState = BG_TOKI_SWD_HAND_UNCHANGED;
    dl = &unrelated;
    Player_ApplyTimePedestalSword(&play, &player, PLAYER_LIMB_L_HAND, &dl, &rot);
    REQUIRE(dl == &unrelated);
    for (void* allocation : allocations)
        free(allocation);
    puts("PASS native child sword landmarks, unchanged wrist/palm, balanced matrix stack, adult and source fallbacks");
}
