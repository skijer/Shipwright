#ifdef __EMSCRIPTEN__
#include "EmbedderBridge.h"

#include <emscripten.h>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <libultraship/libultraship.h>
#include <libultraship/bridge/audiobridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include <z64.h>
#include <variables.h>
extern PlayState* gPlayState;
}

// Listeners may call Soh_RunConsoleCommand, which must not act on a half-updated frame, so events raised
// during a frame are held and dispatched after it.
static std::vector<std::string> sHeldEvents;

static void DispatchEvent(const std::string& json) {
    EM_ASM({ window.dispatchEvent(new CustomEvent('soh', { detail : JSON.parse(UTF8ToString($0)) })); }, json.c_str());
}

static void DispatchHeldEvents() {
    std::vector<std::string> events;
    events.swap(sHeldEvents);
    for (const auto& json : events) {
        DispatchEvent(json);
    }
}

static std::string VfsPath(const std::filesystem::path& path) {
    return std::filesystem::absolute(path).lexically_normal().generic_string();
}

// OnLoadGame fires before the play state exists, so the load is reported with the game's first scene.
static int32_t sLoadedFileNum = -1;

static void OnLoadGame(int32_t fileNum) {
    sLoadedFileNum = fileNum;
}

static void OnSceneInit(int16_t sceneNum) {
    if (sLoadedFileNum >= 0) {
        sHeldEvents.push_back(fmt::format(R"({{"type":"load-game","fileNum":{}}})", sLoadedFileNum));
        sLoadedFileNum = -1;
    }
    sHeldEvents.push_back(fmt::format(R"({{"type":"scene","sceneNum":{},"entranceIndex":{},"setup":{}}})", sceneNum,
                                      gSaveContext.entranceIndex, gSaveContext.sceneLayer));
}

// FS.readFile copies out of the in-memory filesystem; staging large files in the wasm heap would grow it for good.
static void DispatchFileSaved(const std::string& path) {
    std::string vfsPath = VfsPath(path);
    EM_ASM(
        {
            const path = UTF8ToString($0);
            window.dispatchEvent(
                new CustomEvent('soh', { detail : { type : 'file-saved', path : path, bytes : FS.readFile(path) } }));
        },
        vfsPath.c_str());
}

static void DispatchFileRemoved(const std::string& path) {
    DispatchEvent(fmt::format(R"({{"type":"file-removed","path":{}}})", nlohmann::json(VfsPath(path)).dump()));
}

struct FileStamp {
    std::filesystem::file_time_type mtime;
    uintmax_t size;
    bool operator!=(const FileStamp& o) const {
        return mtime != o.mtime || size != o.size;
    }
};

using FileSnapshot = std::map<std::string, FileStamp>;

struct FileChanges {
    std::vector<std::string> saved;
    std::vector<std::string> removed;
};

static FileSnapshot sSeenFiles;
static bool sSeenFilesPrimed = false;

static std::vector<std::filesystem::path> WatchedFiles() {
    std::vector<std::filesystem::path> files;
    files.push_back(Ship::Context::GetPathRelativeToAppDirectory("shipofharkinian.json"));
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::directory_iterator(Ship::Context::GetPathRelativeToAppDirectory("Save"), ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() != ".temp") {
            files.push_back(entry.path());
        }
    }
    return files;
}

static FileSnapshot SnapshotWatchedFiles() {
    FileSnapshot snapshot;
    for (const auto& path : WatchedFiles()) {
        std::error_code ec;
        FileStamp stamp{ std::filesystem::last_write_time(path, ec), std::filesystem::file_size(path, ec) };
        if (!ec) {
            snapshot[path.generic_string()] = stamp;
        }
    }
    return snapshot;
}

static FileChanges DiffSnapshots(const FileSnapshot& before, const FileSnapshot& after) {
    FileChanges changes;
    for (const auto& [path, stamp] : after) {
        auto seen = before.find(path);
        if (seen == before.end() || seen->second != stamp) {
            changes.saved.push_back(path);
        }
    }
    for (const auto& [path, stamp] : before) {
        if (!after.contains(path)) {
            changes.removed.push_back(path);
        }
    }
    return changes;
}

static void ReportFileChanges() {
    FileSnapshot now = SnapshotWatchedFiles();
    if (sSeenFilesPrimed) {
        FileChanges changes = DiffSnapshots(sSeenFiles, now);
        for (const auto& path : changes.saved) {
            DispatchFileSaved(path);
        }
        for (const auto& path : changes.removed) {
            DispatchFileRemoved(path);
        }
    }
    sSeenFiles = std::move(now);
    sSeenFilesPrimed = true;
}

static uint64_t sTicks = 0;
static uint64_t sDraws = 0;
static uint64_t sAudioDrops = 0;

void Soh_EmbedderCountDraws(size_t draws) {
    sDraws += draws;
}

void Soh_EmbedderCountAudioDrop(void) {
    sAudioDrops++;
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* Soh_GetStats(void) {
    static std::string json;
    json = fmt::format(R"({{"ticks":{},"draws":{},"updateRate":{},"audioBuffered":{},"audioDrops":{},"sceneNum":{}}})",
                       sTicks, sDraws, R_UPDATE_RATE, AudioPlayerBuffered(), sAudioDrops,
                       gPlayState != nullptr ? gPlayState->sceneNum : -1);
    return json.c_str();
}

void Soh_InitEmbedderBridge(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>(OnLoadGame);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(OnSceneInit);
    ReportFileChanges();
}

void Soh_EmbedderAfterFrame(void) {
    sTicks++;
    DispatchHeldEvents();
    ReportFileChanges();
}

void Soh_EmbedderQuit(void) {
    DispatchHeldEvents();
    ReportFileChanges();
    DispatchEvent(R"({"type":"quit"})");
}

void Soh_EmbedderError(const char* message) {
    // The loop stops after this, so report what the failing frame raised or wrote now.
    DispatchHeldEvents();
    ReportFileChanges();
    // `replace`: a what() containing non-UTF-8 bytes would otherwise make dump() throw.
    std::string text = nlohmann::json(message).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    DispatchEvent(fmt::format(R"({{"type":"error","message":{}}})", text));
}

void Soh_EmbedderReportFile(const char* path) {
    DispatchFileSaved(path);
}

// Returns -1 before the console exists and -2 for an unknown command, which Console::Run itself reports as 0.
extern "C" EMSCRIPTEN_KEEPALIVE int32_t Soh_RunConsoleCommand(const char* command) {
    auto context = Ship::Context::GetRawInstance();
    if (context == nullptr || context->GetConsole() == nullptr) {
        SPDLOG_WARN("Soh_RunConsoleCommand(\"{}\") before the console exists; ignored", command);
        return -1;
    }
    auto console = context->GetConsole();
    std::string line = command;
    std::string name = line.substr(0, line.find(' '));
    if (!console->HasCommand(name)) {
        SPDLOG_WARN("Soh_RunConsoleCommand: unknown command \"{}\"", name);
        return -2;
    }
    std::string output;
    return console->Run(line, &output);
}
#endif
