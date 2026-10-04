// Compile with the production bridge in this translation unit so the lifetime
// test can inspect the command buffers reached by cached display-list calls.
#include "../soh/Enhancements/Graphics/PreludeNativeMaterialScroll.cpp"
#include "test_require.h"
#include <iostream>

// A reloadable in-memory archive is the seam for testing provenance refresh.
// Reload explicitly invalidates metadata, like the host archive/resource hooks.
// Runtime byte parsing/cache behavior is production code; ZIP I/O is not mocked
// as a success assertion and real O2Rs are checked separately by the runner.
namespace Ship {
Archive::Archive(const std::string& path) : mPath(path) {
}
Archive::~Archive() = default;
const std::string& Archive::GetPath() {
    return mPath;
}
bool Archive::HasFile(const std::string&) {
    return true;
}
} // namespace Ship
class ReloadableArchive : public Ship::Archive {
  public:
    ReloadableArchive() : Archive("fixture") {
    }
    nlohmann::json project;
    void Reload() {
        PreludeNativeMaterialScroll_InvalidateMetadata("test refresh");
    }
    std::shared_ptr<Ship::File> LoadFile(const std::string&) override {
        auto file = std::make_shared<Ship::File>();
        auto text = project.dump();
        file->Buffer = std::make_shared<std::vector<char>>(text.begin(), text.end());
        return file;
    }
    std::shared_ptr<Ship::File> LoadFile(uint64_t) override {
        return nullptr;
    }
    bool Open() override {
        return true;
    }
    bool Close() override {
        return true;
    }
    bool WriteFile(const std::string&, const std::vector<uint8_t>&) override {
        return false;
    }
};

struct GraphicsContext {};
static int sEnabled = -1;
extern "C" int32_t CVarGetInteger(const char*, int32_t defaultValue) {
    return sEnabled < 0 ? defaultValue : sEnabled;
}
static std::array<Gfx, 12> sFrameAllocation;
static Gfx* Graph_Alloc(GraphicsContext*, size_t size) {
    REQUIRE(size == sizeof(sFrameAllocation));
    sFrameAllocation = {};
    return sFrameAllocation.data();
}

// The runner extracts this function verbatim from z_rcp.c, rather than testing
// a reimplementation. Only the allocation and interpolation-recording wrapper
// are replaced; actual native GBI macros generate all 12 commands.
#define gDPSetTileSizeLerp __gDPSetTileSizeLerp
#include "native_two_tex_scroll.inc"

static void CheckCommands(const std::array<Gfx, 12>& actual, const Gfx* expected) {
    for (size_t i = 0; i < actual.size(); ++i) {
        REQUIRE(actual[i].words.w0 == expected[i].words.w0);
        REQUIRE(actual[i].words.w1 == expected[i].words.w1);
    }
}

static void CheckCausticCommands(const std::array<Gfx, 12>& actual, const Gfx* expected) {
    REQUIRE(actual[0].words.w0 == expected[0].words.w0);
    REQUIRE(actual[0].words.w1 == expected[0].words.w1);
    for (size_t i = 0; i < 5; ++i) {
        REQUIRE(actual[i + 1].words.w0 == expected[i + 6].words.w0);
        REQUIRE(actual[i + 1].words.w1 == expected[i + 6].words.w1);
    }
    REQUIRE(actual[6].words.w0 == expected[11].words.w0);
    REQUIRE(actual[6].words.w1 == expected[11].words.w1);
}

static void CheckCausticWords(const std::array<Gfx, 12>& actual, uint32_t x, uint32_t lrs, uint32_t nextX,
                              uint32_t nextLrs) {
    const std::array<Prelude::NativeMaterialCommand, 7> expected = {
        Prelude::NativeMaterialCommand{ 0xe8000000, 0 }, Prelude::NativeMaterialCommand{ 0x4a000000, 0x01000000 },
        Prelude::NativeMaterialCommand{ x, 0 },          Prelude::NativeMaterialCommand{ lrs, 0x42f80000 },
        Prelude::NativeMaterialCommand{ nextX, 0 },      Prelude::NativeMaterialCommand{ nextLrs, 0x42f80000 },
        Prelude::NativeMaterialCommand{ 0xdf000000, 0 },
    };
    for (size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(actual[i].words.w0 == expected[i].w0);
        REQUIRE(actual[i].words.w1 == expected[i].w1);
    }
}

static void CheckVerticalCausticWords(const std::array<Gfx, 12>& actual, uint32_t y, uint32_t lrt, uint32_t nextY,
                                      uint32_t nextLrt) {
    const std::array<Prelude::NativeMaterialCommand, 7> expected = {
        Prelude::NativeMaterialCommand{ 0xe8000000, 0 }, Prelude::NativeMaterialCommand{ 0x4a000000, 0x01000000 },
        Prelude::NativeMaterialCommand{ 0, y },          Prelude::NativeMaterialCommand{ 0x42f80000, lrt },
        Prelude::NativeMaterialCommand{ 0, nextY },      Prelude::NativeMaterialCommand{ 0x42f80000, nextLrt },
        Prelude::NativeMaterialCommand{ 0xdf000000, 0 },
    };
    for (size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(actual[i].words.w0 == expected[i].w0);
        REQUIRE(actual[i].words.w1 == expected[i].w1);
    }
}

static void CheckSagePlatformMetadata() {
    const auto platform = nlohmann::json::parse(
        R"({"newDlPath":"custom/prelude/lost_woods/sage_platform","nativeAnimation":{"version":1,"binding":"material-motion","source":"oot.chamber_of_sages.platform","logicalWidth":32,"logicalHeight":32}})");
    auto owner = std::make_shared<ReloadableArchive>();
    auto other = std::make_shared<ReloadableArchive>();
    for (const char* kind : { "pastes", "shapes", "materials" }) {
        owner->project = {
            { "edits",
              { { "lost_woods",
                  nlohmann::json::array({ { { "data", { { kind, nlohmann::json::array({ platform }) } } } } }) } } }
        };
        owner->Reload();
        REQUIRE(Prelude::ProfileFor(owner, "custom/prelude/lost_woods/sage_platform") ==
                Prelude::NativeMaterialProfile::ChamberOfSagesPlatform);
        // Another archive declaring the same resource path must not borrow this binding.
        other->project = owner->project;
        other->project["edits"]["lost_woods"][0]["data"][kind][0].erase("nativeAnimation");
        other->Reload();
        REQUIRE(Prelude::ProfileFor(other, "custom/prelude/lost_woods/sage_platform") ==
                Prelude::NativeMaterialProfile::None);
        REQUIRE(Prelude::ProfileFor(owner, "custom/prelude/lost_woods/sage_platform") ==
                Prelude::NativeMaterialProfile::ChamberOfSagesPlatform);
        auto& data = owner->project["edits"]["lost_woods"][0]["data"];
        auto conflict = platform;
        conflict["nativeAnimation"]["source"] = "oot.water_temple.caustics";
        data[kind].push_back(conflict);
        owner->Reload();
        REQUIRE(Prelude::ProfileFor(owner, "custom/prelude/lost_woods/sage_platform") ==
                Prelude::NativeMaterialProfile::None);
        data[kind] = nlohmann::json::array({ platform });
        owner->Reload();
        REQUIRE(Prelude::ProfileFor(owner, "custom/prelude/lost_woods/sage_platform") ==
                Prelude::NativeMaterialProfile::ChamberOfSagesPlatform);
    }
}

int main() {
    CheckSagePlatformMetadata();
    auto archive = std::make_shared<ReloadableArchive>();
    archive->project = nlohmann::json::parse(
        R"({"edits":{"any_scene":[{"data":{"pastes":[{"newDlPath":"custom/prelude/any/paste0","chain":[{"path":"objects/object_spot06_objects/gLakeHyliaHighWaterDL"}]}]}}]}})");
    REQUIRE(Prelude::ProfileFor(archive, "custom/prelude/any/paste0") == Prelude::NativeMaterialProfile::LakeHylia);
    archive->project["edits"]["any_scene"][0]["data"]["pastes"][0]["chain"][0]["path"] = "unrelated";
    archive->Reload();
    REQUIRE(Prelude::ProfileFor(archive, "custom/prelude/any/paste0") == Prelude::NativeMaterialProfile::None);
    auto& items = archive->project["edits"]["any_scene"][0]["data"]["pastes"];
    auto a = items[0];
    a["chain"][0]["path"] = "objects/object_spot06_objects/gLakeHyliaHighWaterDL";
    auto b = a;
    b["chain"][0]["path"] = "objects/object_spot01_objects/gKakarikoWellWaterDL";
    items = nlohmann::json::array({ a, b, a });
    archive->Reload();
    REQUIRE(Prelude::ProfileFor(archive, "custom/prelude/any/paste0") == Prelude::NativeMaterialProfile::None);
    auto caustic = nlohmann::json::parse(
        R"({"newDlPath":"custom/prelude/water_temple/material0","geometry":"arbitrary","nativeAnimation":{"version":1,"binding":"material-motion","source":"oot.water_temple.caustics","logicalWidth":32,"logicalHeight":32}})");
    archive->project["edits"]["any_scene"][0]["data"] = {
        { "materials", nlohmann::json::array({ caustic }) },
    };
    archive->Reload();
    REQUIRE(Prelude::ProfileFor(archive, "custom/prelude/water_temple/material0") ==
            Prelude::NativeMaterialProfile::WaterTempleCaustics);
    archive->project["edits"]["any_scene"][0]["data"]["materials"][0]["nativeAnimation"]["source"] =
        "oot.zoras_domain.caustics";
    archive->Reload();
    REQUIRE(static_cast<int>(Prelude::ProfileFor(archive, "custom/prelude/water_temple/material0")) == 11);
    archive->project["edits"]["any_scene"][0]["data"]["materials"][0]["nativeAnimation"]["source"] = "bad";
    archive->Reload();
    REQUIRE(Prelude::ProfileFor(archive, "custom/prelude/water_temple/material0") ==
            Prelude::NativeMaterialProfile::None);
    auto shapeLikeMaterial = nlohmann::json::parse(
        R"({"newDlPath":"custom/prelude/water_temple/material1","stored":{"textures":[{"label":"spot10_room_1Tex_008030"},{"label":"spot10_room_1Tex_008030"}]}})");
    archive->project["edits"]["any_scene"][0]["data"]["materials"] = nlohmann::json::array({ shapeLikeMaterial });
    archive->Reload();
    REQUIRE(Prelude::ProfileFor(archive, "custom/prelude/water_temple/material1") ==
            Prelude::NativeMaterialProfile::None);
    auto conflictingCaustic = caustic;
    conflictingCaustic["nativeAnimation"]["source"] = "mm.bg_keikoku_spr.lower_a";
    archive->project["edits"]["any_scene"][0]["data"]["materials"] = nlohmann::json::array({ caustic, caustic });
    archive->Reload();
    REQUIRE(Prelude::ProfileFor(archive, "custom/prelude/water_temple/material0") ==
            Prelude::NativeMaterialProfile::WaterTempleCaustics);
    archive->project["edits"]["any_scene"][0]["data"]["materials"].push_back(conflictingCaustic);
    archive->Reload();
    REQUIRE(Prelude::ProfileFor(archive, "custom/prelude/water_temple/material0") ==
            Prelude::NativeMaterialProfile::None);
    GraphicsContext ctx;
    auto& lists = Prelude::Lists().lists;
    auto lakePointer = lists[1].data();
    const auto domainIndex = static_cast<size_t>(Prelude::NativeMaterialProfile::ZorasDomainCaustics);
    auto domainPointer = lists[domainIndex].data();
    const auto platformIndex = static_cast<size_t>(Prelude::NativeMaterialProfile::ChamberOfSagesPlatform);
    auto platformPointer = lists[platformIndex].data();
    REQUIRE(lakePointer != lists[2].data() && lakePointer != lists[3].data());
    for (uint32_t f : { 0u, 1u, 31u, 32u, 63u, 64u, 127u, 128u, 2047u, 2048u, 0xffffffffu }) {
        const uint32_t game = f + 53u; // Verify different native clock sources.
        PreludeNativeMaterialScroll_Update(&ctx, f, game);
        CheckCommands(lists[1], Gfx_TwoTexScrollEx(&ctx, 0, 0u - f, f, 32, 32, 1, f, f, 32, 32, -1, 1, 1, 1));
        CheckCommands(lists[2], Gfx_TwoTexScrollEx(&ctx, 0, 127 - f % 128, f & 127, 32, 32, 1, f % 128, f & 127, 32, 32,
                                                   -1, 1, 1, 1));
        CheckCommands(lists[3],
                      Gfx_TwoTexScrollEx(&ctx, 0, game % 128, 0, 32, 16, 1, game % 128, 0, 32, 16, 1, 0, 1, 0));
        for (size_t i = static_cast<size_t>(Prelude::NativeMaterialProfile::FountainLowerA32);
             i <= static_cast<size_t>(Prelude::NativeMaterialProfile::FountainCentral64); ++i) {
            const int size = i < 7 ? 32 : 64;
            const int rate = ((i - 4) % 3 == 0 ? -20 : (i - 4) % 3 == 1 ? 20 : 10) * (size / 32);
            CheckCommands(lists[i], Gfx_TwoTexScrollEx(&ctx, 0, 0, 0, size, size, 1, 0,
                                                       game * static_cast<uint32_t>(rate), size, size, 0, 0, 0, rate));
            for (size_t j = 1; j < i; ++j)
                REQUIRE(lists[i].data() != lists[j].data());
        }
        const auto causticIndex = static_cast<size_t>(Prelude::NativeMaterialProfile::WaterTempleCaustics);
        CheckCausticCommands(lists[causticIndex],
                             Gfx_TwoTexScrollEx(&ctx, 0, 0, 0, 32, 32, 1, game, 0, 32, 32, 0, 0, 1, 0));
        REQUIRE(lists[causticIndex][0].words.w0 >> 24 == 0xe8);
        REQUIRE(lists[causticIndex][1].words.w0 >> 24 == 0x4a);
        REQUIRE((lists[causticIndex][1].words.w1 >> 24 & 7) == 1);
        REQUIRE(lists[causticIndex][6].words.w0 == 0xdf000000);
        CheckCausticCommands(lists[domainIndex],
                             Gfx_TwoTexScrollEx(&ctx, 0, 0, 0, 32, 32, 1, 0, 127 - game % 128, 32, 32, 0, 0, 0, -1));
        for (size_t i = 1; i < domainIndex; ++i) {
            REQUIRE(domainPointer != lists[i].data());
        }
        CheckCommands(lists[platformIndex], Gfx_TwoTexScrollEx(&ctx, 0, 127 - game % 128, game % 128, 32, 32, 1,
                                                               game % 128, game % 128, 32, 32, -1, 1, 1, 1));
        for (size_t i = 1; i < platformIndex; ++i) {
            REQUIRE(platformPointer != lists[i].data());
        }
        auto before = lists;
        sFrameAllocation = {}; // Simulate transient allocation reuse after draw.
        for (size_t i = 1; i < lists.size(); ++i) {
            CheckCommands(lists[i], before[i].data());
        }
        REQUIRE(lakePointer == lists[1].data());
        REQUIRE(platformPointer == lists[platformIndex].data());
    }
    struct CausticCase {
        uint32_t game;
        uint32_t x;
        uint32_t lrs;
        uint32_t nextX;
        uint32_t nextLrs;
    };
    for (const auto& test : { CausticCase{ 0, 0x00000000, 0x42f80000, 0x3f800000, 0x42fa0000 },
                              CausticCase{ 1, 0x3f800000, 0x42fa0000, 0x40000000, 0x42fc0000 },
                              CausticCase{ 127, 0x42fe0000, 0x437b0000, 0x43000000, 0x437c0000 },
                              CausticCase{ 128, 0x43000000, 0x437c0000, 0x43010000, 0x437d0000 },
                              CausticCase{ 2047, 0x44ffe000, 0x4507b000, 0x45000000, 0x4507c000 },
                              CausticCase{ 2048, 0x00000000, 0x42f80000, 0x3f800000, 0x42fa0000 },
                              CausticCase{ 0xffffffff, 0x44ffe000, 0x4507b000, 0x45000000, 0x4507c000 } }) {
        PreludeNativeMaterialScroll_Update(&ctx, test.game ^ 0xa5a5a5a5u, test.game);
        const auto causticIndex = static_cast<size_t>(Prelude::NativeMaterialProfile::WaterTempleCaustics);
        CheckCausticWords(lists[causticIndex], test.x, test.lrs, test.nextX, test.nextLrs);
    }
    // Literal float command words verify vertical direction and negative interpolation
    // at the 0 -> 127 phase wrap, independently of the native generator comparison.
    for (const auto& test : { CausticCase{ 0, 0x42fe0000, 0x437b0000, 0x42fc0000, 0x437a0000 },
                              CausticCase{ 1, 0x42fc0000, 0x437a0000, 0x42fa0000, 0x43790000 },
                              CausticCase{ 126, 0x3f800000, 0x42fa0000, 0x00000000, 0x42f80000 },
                              CausticCase{ 127, 0x00000000, 0x42f80000, 0xbf800000, 0x42f60000 },
                              CausticCase{ 128, 0x42fe0000, 0x437b0000, 0x42fc0000, 0x437a0000 },
                              CausticCase{ 129, 0x42fc0000, 0x437a0000, 0x42fa0000, 0x43790000 },
                              CausticCase{ 2047, 0x00000000, 0x42f80000, 0xbf800000, 0x42f60000 },
                              CausticCase{ 2048, 0x42fe0000, 0x437b0000, 0x42fc0000, 0x437a0000 },
                              CausticCase{ 0xffffffff, 0x00000000, 0x42f80000, 0xbf800000, 0x42f60000 } }) {
        PreludeNativeMaterialScroll_Update(&ctx, test.game ^ 0xa5a5a5a5u, test.game);
        CheckVerticalCausticWords(lists[domainIndex], test.x, test.lrs, test.nextX, test.nextLrs);
    }
    // At gameplay frame 127 both layers interpolate through their wrap instead
    // of jumping early: tile 0 moves to x=-1, tile 1 moves to x=128, both y=128.
    const std::array<Prelude::NativeMaterialCommand, 12> platformWrap = {
        Prelude::NativeMaterialCommand{ 0xe8000000, 0 },
        { 0x4a000000, 0 },
        { 0x00000000, 0x42fe0000 },
        { 0x42f80000, 0x437b0000 },
        { 0xbf800000, 0x43000000 },
        { 0x42f60000, 0x437c0000 },
        { 0x4a000000, 0x01000000 },
        { 0x42fe0000, 0x42fe0000 },
        { 0x437b0000, 0x437b0000 },
        { 0x43000000, 0x43000000 },
        { 0x437c0000, 0x437c0000 },
        { 0xdf000000, 0 },
    };
    PreludeNativeMaterialScroll_Update(&ctx, 41, 127);
    for (size_t i = 0; i < platformWrap.size(); ++i) {
        REQUIRE(lists[platformIndex][i].words.w0 == platformWrap[i].w0);
        REQUIRE(lists[platformIndex][i].words.w1 == platformWrap[i].w1);
    }
    sEnabled = 0;
    PreludeNativeMaterialScroll_Update(&ctx, 5, 8);
    for (size_t i = 1; i < lists.size(); ++i) {
        REQUIRE(lists[i][0].words.w0 == 0xdf000000);
    }
    sEnabled = 1;
    PreludeNativeMaterialScroll_Update(&ctx, 0, 0); // New scene / reset frame counts.
    REQUIRE(lakePointer == lists[1].data());
    REQUIRE(lists[1][1].words.w0 >> 24 == 0x4a); // Native interpolated tile command.
    const auto causticIndex = static_cast<size_t>(Prelude::NativeMaterialProfile::WaterTempleCaustics);
    CheckCausticCommands(lists[causticIndex], Gfx_TwoTexScrollEx(&ctx, 0, 0, 0, 32, 32, 1, 0, 0, 32, 32, 0, 0, 1, 0));
    REQUIRE(domainPointer == lists[domainIndex].data());
    CheckVerticalCausticWords(lists[domainIndex], 0x42fe0000, 0x437b0000, 0x42fc0000, 0x437a0000);
    REQUIRE(platformPointer == lists[platformIndex].data());
    CheckCommands(lists[platformIndex], Gfx_TwoTexScrollEx(&ctx, 0, 127, 0, 32, 32, 1, 0, 0, 32, 32, -1, 1, 1, 1));
    PreludeNativeMaterialScroll_Update(nullptr, 7, 11);
    for (size_t i = 1; i < lists.size(); ++i) {
        REQUIRE(lists[i][0].words.w0 == 0xdf000000);
    }
    PreludeNativeMaterialScroll_Update(&ctx, 13, 17);
    CheckCausticCommands(lists[causticIndex], Gfx_TwoTexScrollEx(&ctx, 0, 0, 0, 32, 32, 1, 17, 0, 32, 32, 0, 0, 1, 0));
    CheckCausticCommands(lists[domainIndex], Gfx_TwoTexScrollEx(&ctx, 0, 0, 0, 32, 32, 1, 0, 110, 32, 32, 0, 0, 0, -1));
    CheckCommands(lists[platformIndex], Gfx_TwoTexScrollEx(&ctx, 0, 110, 17, 32, 32, 1, 17, 17, 32, 32, -1, 1, 1, 1));
    std::cout << "PASS native generated commands, independent buffers, frame reset, disable, lifetime\n";
}
