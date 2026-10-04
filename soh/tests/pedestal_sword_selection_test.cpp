#include "test_require.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <map>
#include <vector>

using s32 = int32_t;
using u32 = uint32_t;
struct Gfx {
    struct {
        uintptr_t w0, w1;
    } words;
};
struct PakModel {
    std::map<u32, Gfx*> adultEquipDLs, childEquipDLs;
};
static std::vector<PakModel> sModels(6);
static s32 sSelectedAdultIndex = 0, sSelectedEquipIndex = -1, sForcedEquipIndex = -1, sForcedModelIndex = -1;
static s32 sSlotMix[] = { -1, -1, -1 };
static std::vector<Gfx*> sRuntimeCombinedDLs;
static bool enabled = true, remote = false;
static void EnsureSlotMixLoaded() {
}
static bool PakLoader_IsRemoteRenderActive() {
    return remote;
}
static int CVarGetInteger(const char*, int) {
    return enabled;
}
#define PAK_DL_STUB ((Gfx*)(uintptr_t)1)
static bool IsValidGfxPtrOrOtrPath(Gfx* p) {
    return reinterpret_cast<uintptr_t>(p) > 4096;
}
static int ResourceMgr_OTRSigCheck(char* p) {
    return strncmp(p, "__OTR__", 7) == 0;
}
#include "pedestal_sword.inc"

static Gfx blades[12]{};
static void ExpectSword(Gfx* hilt, Gfx* blade) {
    Gfx* result = PakLoader_GetTimePedestalSwordDL();
    REQUIRE(result != nullptr && result != PAK_DL_STUB);
    REQUIRE(result[0].words.w1 == reinterpret_cast<uintptr_t>(hilt));
    REQUIRE(result[1].words.w1 == reinterpret_cast<uintptr_t>(blade));
    REQUIRE(result[2].words.w0 == 0xDF000000);
}

int main() {
    sModels[0].adultEquipDLs = { { 0x50E0, &blades[0] }, { 0x50F8, &blades[1] } };
    sModels[0].childEquipDLs = { { 0x50E0, &blades[2] }, { 0x50F8, &blades[3] } };
    // A Master Sword ceremony consistently uses the adult Master slot, not
    // whichever age-specific body/equipment cache is currently being drawn.
    ExpectSword(&blades[0], &blades[1]);
    sModels[1].adultEquipDLs = { { 0x50E0, &blades[4] }, { 0x50F8, &blades[5] } };
    sSelectedEquipIndex = 1;
    enabled = false; // Equipment-only selections work with the body checkbox off.
    ExpectSword(&blades[4], &blades[5]);
    sModels[2].childEquipDLs = { { 0x50E0, &blades[6] }, { 0x50F8, &blades[7] } };
    sSlotMix[1] = 2; // Child-only donor still supplies the selected Master slot.
    ExpectSword(&blades[6], &blades[7]);
    sModels[3].adultEquipDLs = { { 0x50E0, &blades[8] }, { 0x50F8, &blades[9] } };
    sForcedEquipIndex = 3;
    ExpectSword(&blades[8], &blades[9]);
    remote = true;
    REQUIRE(PakLoader_GetTimePedestalSwordDL() == nullptr);
    remote = false;
    sForcedEquipIndex = -1;
    sSlotMix[1] = -1;
    sSelectedEquipIndex = -1;
    REQUIRE(PakLoader_GetTimePedestalSwordDL() == nullptr);
    enabled = true;
    sModels[0].adultEquipDLs[0x50F8] = PAK_DL_STUB;
    REQUIRE(PakLoader_GetTimePedestalSwordDL() == nullptr);
    sModels[0].adultEquipDLs.erase(0x50F8);
    ExpectSword(&blades[0], &blades[3]); // Per-piece fallback, no wrong weapon slot.
    for (Gfx* dl : sRuntimeCombinedDLs)
        free(dl);
    puts("PASS real Pak source layers, age-separated maps, selected slot, disabled body, remote and stub guards");
}
