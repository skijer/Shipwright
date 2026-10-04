#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "soh/util.h"
#include "soh/ResourceManagerHelpers.h"

// Expose fixture setup and direct section entry points without adding test-only
// methods to the production class. Standard library headers are already loaded.
#define private public
#include "soh/SaveManager.h"
#undef private

using json = nlohmann::json;
SaveContext gSaveContext{};
SaveManager* SaveManager::Instance = nullptr;

// Engine boundaries unrelated to horse persistence or JSON. New-file routines
// still execute in full, including debug/maxed branching and base initialization.
static int debugSaveMode = 1;
class OTRGlobals {
  public:
    static OTRGlobals* Instance;
    bool HasOriginal() {
        return true;
    }
};
static OTRGlobals globals;
OTRGlobals* OTRGlobals::Instance = &globals;
extern "C" uint32_t ResourceMgr_GetGameRegion(int) {
    return GAME_REGION_NTSC;
}
extern "C" int32_t CVarGetInteger(const char*, int32_t) {
    return debugSaveMode;
}
extern "C" void Inventory_ChangeEquipment(s16, u16) {
}
extern "C" void Flags_SetInfTable(s32) {
}
extern "C" void Flags_SetEventChkInf(s32) {
}
extern "C" void Flags_SetRandomizerInf(RandomizerInf) {
}
#define SPDLOG_WARN(...) ((void)0)
#define SPDLOG_ERROR(...) ((void)0)

#include "young_epona_save_production.inc"

static void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static void CheckHorse(const HorseData& horse, s16 scene, s16 x, s16 y, s16 z, s16 angle, const char* message) {
    Check(horse.scene == scene && horse.pos.x == x && horse.pos.y == y && horse.pos.z == z && horse.angle == angle,
          message);
}

static void CheckYoungDefault() {
    Check(gSaveContext.ship.youngHorseDataValid == 0, "missing/new-file young horse remains invalid");
    CheckHorse(gSaveContext.ship.youngHorseData, -1, 0, 0, 0, 0, "missing/new-file young horse has safe defaults");
}

static void SeedStaleYoungHorse() {
    gSaveContext.ship.youngHorseDataValid = 1;
    gSaveContext.ship.youngHorseData = { 99, { 111, 222, 333 }, 444 };
}

static json BaseFile(int version, json data) {
    return { { "version", 1 },
             { "fileType", FILE_TYPE_SAVE_VANILLA },
             { "sections", { { "base", { { "version", version }, { "data", data } } } } } };
}

static void TestSnapshotAndRoundTrip(SaveManager& manager, const char* path) {
    SaveContext snapshot{};
    snapshot.horseData = { 81, { -1234, 456, 7890 }, -2345 };
    snapshot.ship.youngHorseData = { 99, { 31000, -32000, 32767 }, -32768 };
    snapshot.ship.youngHorseDataValid = 1;
    gSaveContext.horseData = { 1, { 2, 3, 4 }, 5 };
    gSaveContext.ship.youngHorseData = { 6, { 7, 8, 9 }, 10 };
    gSaveContext.ship.youngHorseDataValid = 0;
    json data;
    manager.currentJsonContext = &data;
    SaveManager::SaveBase(&snapshot, SECTION_ID_BASE, true);
    const json expectedAdult = { { "scene", 81 },
                                 { "pos", { { "x", -1234 }, { "y", 456 }, { "z", 7890 } } },
                                 { "angle", -2345 } };
    const json expectedYoung = { { "valid", 1 },
                                 { "scene", 99 },
                                 { "pos", { { "x", 31000 }, { "y", -32000 }, { "z", 32767 } } },
                                 { "angle", -32768 } };
    Check(data.at("horseData") == expectedAdult, "adult horse is saved from the supplied snapshot");
    Check(data.at("youngHorseData") == expectedYoung, "young horse is saved from the supplied snapshot");
    CheckHorse(gSaveContext.horseData, 1, 2, 3, 4, 5, "saving does not change live adult horse");
    CheckHorse(gSaveContext.ship.youngHorseData, 6, 7, 8, 9, 10, "saving does not change live young horse");

    // Real JSON encoding, disk write/read, parse and full section dispatch.
    std::ofstream output(path);
    output << BaseFile(4, data).dump();
    output.close();
    Check(static_cast<bool>(output), "round-trip file writes successfully");
    std::ifstream input(path);
    json restored;
    input >> restored;
    manager.LoadFromJsonObject(restored);
    CheckHorse(gSaveContext.horseData, 81, -1234, 456, 7890, -2345, "adult horse survives JSON round trip");
    CheckHorse(gSaveContext.ship.youngHorseData, 99, 31000, -32000, 32767, -32768,
               "young horse coordinates survive JSON round trip");
    Check(gSaveContext.ship.youngHorseDataValid == 1, "young horse validity survives JSON round trip");
    const json resaved = manager.SaveToJsonObject();
    Check(resaved.at("sections").at("base").at("version") == 4, "base remains version 4");
    const json& resavedData = resaved.at("sections").at("base").at("data");
    Check(resavedData.at("horseData") == expectedAdult, "full-file save preserves adult horse");
    Check(resavedData.at("youngHorseData") == expectedYoung, "full-file save registers young horse serialization");
    std::puts("PASS: real JSON/disk round trip, full-file dispatch, snapshot isolation, adult preservation");
}

static void TestLegacyAndFileIsolation(SaveManager& manager) {
    const json adult = { { "scene", 87 },
                         { "pos", { { "x", 2000 }, { "y", -500 }, { "z", 3000 } } },
                         { "angle", 12345 } };
    const SaveManager::LoadFunc loaders[] = { SaveManager::LoadBaseVersion1, SaveManager::LoadBaseVersion2,
                                              SaveManager::LoadBaseVersion3, SaveManager::LoadBaseVersion4 };
    for (int version = 1; version <= 4; ++version) {
        // Call each complete loader directly as well as through file dispatch:
        // InitFile must not hide a missing version-specific reset.
        json legacy = { { "horseData", adult } };
        SeedStaleYoungHorse();
        manager.currentJsonContext = &legacy;
        loaders[version - 1]();
        CheckYoungDefault();
        CheckHorse(gSaveContext.horseData, 87, 2000, -500, 3000, 12345, "legacy load preserves adult horse");

        SeedStaleYoungHorse();
        json file = BaseFile(version, { { "horseData", adult } });
        manager.LoadFromJsonObject(file);
        CheckYoungDefault();
        CheckHorse(gSaveContext.horseData, 87, 2000, -500, 3000, 12345, "legacy file dispatch loads adult horse");
    }

    // Two modern files may both contain horse state; validity and all data must
    // follow the selected file rather than retain any previous file's values.
    SeedStaleYoungHorse();
    json fileA = manager.SaveToJsonObject();
    gSaveContext.ship.youngHorseData = { 90, { -9, -8, -7 }, -6 };
    gSaveContext.ship.youngHorseDataValid = 0;
    json fileB = manager.SaveToJsonObject();
    manager.LoadFromJsonObject(fileA);
    CheckHorse(gSaveContext.ship.youngHorseData, 99, 111, 222, 333, 444, "file A owns its young horse record");
    Check(gSaveContext.ship.youngHorseDataValid == 1, "file A owns its valid state");
    manager.LoadFromJsonObject(fileB);
    CheckHorse(gSaveContext.ship.youngHorseData, 90, -9, -8, -7, -6, "file B owns its young horse record");
    Check(gSaveContext.ship.youngHorseDataValid == 0, "file B invalidates the previously loaded young horse");

    SeedStaleYoungHorse();
    json partial = BaseFile(4, { { "horseData", adult }, { "youngHorseData", { { "valid", 0 } } } });
    manager.LoadFromJsonObject(partial);
    CheckYoungDefault();
    CheckHorse(gSaveContext.horseData, 87, 2000, -500, 3000, 12345, "partial young data cannot overwrite adult horse");
    std::puts("PASS: complete v1-v4 legacy loaders, sequential file isolation, partial-record defaults");
}

static void TestNewFileModes(SaveManager& manager) {
    gSaveContext.fileNum = 0;
    SeedStaleYoungHorse();
    manager.InitFile(false);
    CheckYoungDefault();
    CheckHorse(gSaveContext.horseData, SCENE_HYRULE_FIELD, -1840, 72, 5497, -0x6AD9,
               "normal creation retains vanilla adult horse start");
    for (int mode : { 0, 1, 2 }) {
        SeedStaleYoungHorse();
        debugSaveMode = mode;
        manager.InitFile(true);
        CheckYoungDefault();
        CheckHorse(gSaveContext.horseData, SCENE_HYRULE_FIELD, -1840, 72, 5497, -0x6AD9,
                   "debug/maxed creation retains vanilla adult horse start");
        Check(gSaveContext.healthCapacity == (mode == 0   ? STARTING_HEALTH
                                              : mode == 1 ? 0xE0
                                                          : MAX_HEALTH),
              "fixture reaches the selected normal/debug/maxed initialization branch");
    }
    SeedStaleYoungHorse();
    gSaveContext.fileNum = 0xFF;
    manager.InitFile(true);
    CheckYoungDefault();
    std::puts("PASS: normal, debug, maxed and title initialization clear prior young horse state");
}

int main(int argc, char** argv) {
    Check(argc == 2, "round-trip file path is provided");
    SaveManager manager;
    SaveManager::Instance = &manager;
    TestSnapshotAndRoundTrip(manager, argv[1]);
    TestLegacyAndFileIsolation(manager);
    TestNewFileModes(manager);
    return 0;
}
