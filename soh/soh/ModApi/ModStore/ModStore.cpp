#include "ModStore.h"
#include "StoreHttp.h"
#include "StoreInternal.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <functional>
#include <iterator>
#include <regex>
#include <set>
#include <thread>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/window/Window.h>

#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/ModTrust/ModTrust.h"
#include "soh/Notification/Notification.h"
#include "variables.h"

namespace {

using Json = nlohmann::json;
namespace fs = std::filesystem;

constexpr const char* CatalogUrl = "https://github.com/skijer/Shipwright/releases/download/mods/catalog.json";
constexpr const char* ReleaseUrl = "https://github.com/skijer/Shipwright/releases/latest/download/unbound-release.json";
constexpr size_t MaxDocumentBytes = 4 * 1024 * 1024;
constexpr size_t MaxPackageBytes = 512ull * 1024 * 1024;
constexpr const char* StateFileName = ".unbound-store.json";
constexpr const char* OperationsFileName = ".unbound-store-pending.json";
constexpr const char* PackageSuffix = ".o2r";
constexpr const char* PendingSuffix = ".pending";
constexpr const char* DisabledSuffix = ".disabled";
constexpr const char* DisableOperation = "disable";
constexpr const char* RemoveOperation = "remove";
#define CVAR_STORE_CHECK_AT_STARTUP CVAR_SETTING("ModStore.CheckAtStartup")
#define CVAR_STORE_SKIPPED_VERSION CVAR_SETTING("ModStore.SkippedVersion")

const std::regex ModId("^[a-z0-9_-]{1,64}$");

std::mutex sViewMutex;
StoreView sView;
bool sAnnounceNextRefresh = false;
bool sReleaseStaged = false;
std::vector<std::string> sAnnouncements;

class StoreWorker {
  public:
    ~StoreWorker() {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStopping = true;
        }
        mWake.notify_all();
        if (mThread.joinable()) {
            mThread.join();
        }
    }

    void Post(std::function<void()> job) {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mJobs.push_back(std::move(job));
            if (!mThread.joinable()) {
                mThread = std::thread([this] { Run(); });
            }
        }
        mWake.notify_one();
    }

  private:
    void Run() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mWake.wait(lock, [this] { return mStopping || !mJobs.empty(); });
                if (mStopping) {
                    return;
                }
                job = std::move(mJobs.front());
                mJobs.pop_front();
            }
            RunGuarded(job);
        }
    }

    static void RunGuarded(const std::function<void()>& job) {
        {
            std::lock_guard<std::mutex> lock(sViewMutex);
            sView.busy = true;
        }
        try {
            job();
        } catch (const std::exception& exception) {
            SPDLOG_ERROR("[ModStore] {}", exception.what());
            std::lock_guard<std::mutex> lock(sViewMutex);
            sView.status = std::string("Something went wrong: ") + exception.what();
        }
        std::lock_guard<std::mutex> lock(sViewMutex);
        sView.busy = false;
    }

    std::mutex mMutex;
    std::condition_variable mWake;
    std::deque<std::function<void()>> mJobs;
    std::thread mThread;
    bool mStopping = false;
};

StoreWorker& Worker() {
    static StoreWorker worker;
    return worker;
}

void SetStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(sViewMutex);
    sView.status = status;
}

fs::path PackagePath(const fs::path& modsDirectory, const std::string& id, const char* suffix = "") {
    return modsDirectory / (id + PackageSuffix + suffix);
}

Json LoadJsonFile(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        return Json::object();
    }
    const Json object = Json::parse(input, nullptr, false);
    return object.is_object() ? object : Json::object();
}

void SaveJsonFile(const fs::path& path, const Json& object) {
    std::ofstream output(path, std::ios::trunc);
    output << object.dump(2) << '\n';
}

bool WriteFileReplacing(const fs::path& target, const std::vector<char>& data) {
    const fs::path temporary = target.string() + ".writing";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!output) {
            std::error_code ignored;
            fs::remove(temporary, ignored);
            return false;
        }
    }
    std::error_code error;
    fs::rename(temporary, target, error);
    return !error;
}

std::string HashFile(const fs::path& path) {
    struct CachedHash {
        uintmax_t size;
        fs::file_time_type time;
        std::string hash;
    };
    static std::map<std::string, CachedHash> cache;

    std::error_code error;
    const uintmax_t size = fs::file_size(path, error);
    const fs::file_time_type time = fs::last_write_time(path, error);
    if (error) {
        return {};
    }
    auto cached = cache.find(path.string());
    if (cached != cache.end() && cached->second.size == size && cached->second.time == time) {
        return cached->second.hash;
    }

    std::ifstream input(path, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    std::string hash = ModTrust_HashBytes(bytes.data(), bytes.size());
    cache[path.string()] = { size, time, hash };
    return hash;
}

std::set<std::string> ReadOperation(const Json& operations, const char* kind) {
    std::set<std::string> ids;
    for (const Json& id : operations.value(kind, Json::array())) {
        if (id.is_string() && std::regex_match(id.get<std::string>(), ModId)) {
            ids.insert(id.get<std::string>());
        }
    }
    return ids;
}

void SetOperation(const std::string& id, const char* kind) {
    const fs::path path = ModStore_ModsDirectory() / OperationsFileName;
    Json operations = LoadJsonFile(path);
    for (const char* other : { DisableOperation, RemoveOperation }) {
        std::set<std::string> ids = ReadOperation(operations, other);
        ids.erase(id);
        if (kind != nullptr && std::string(kind) == other) {
            ids.insert(id);
        }
        operations[other] = ids;
    }
    SaveJsonFile(path, operations);
}

StoreModState ReadState(const StoreMod& mod, const Json& operations) {
    const fs::path mods = ModStore_ModsDirectory();
    const auto build = mod.builds.find(SOH_MOD_API_MAJOR);
    std::error_code error;
    if (ReadOperation(operations, RemoveOperation).count(mod.id)) {
        return StoreModState::RemovedOnRestart;
    }
    if (ReadOperation(operations, DisableOperation).count(mod.id)) {
        return StoreModState::DisabledOnRestart;
    }
    if (fs::exists(PackagePath(mods, mod.id, PendingSuffix), error)) {
        return StoreModState::InstallsOnRestart;
    }
    if (fs::exists(PackagePath(mods, mod.id), error)) {
        const bool current = build == mod.builds.end() || HashFile(PackagePath(mods, mod.id)) == build->second.hash;
        return current ? StoreModState::Installed : StoreModState::Outdated;
    }
    if (fs::exists(PackagePath(mods, mod.id, DisabledSuffix), error)) {
        return StoreModState::Disabled;
    }
    return build == mod.builds.end() ? StoreModState::Unavailable : StoreModState::NotInstalled;
}

std::map<std::string, StoreModState> ReadStates(const StoreCatalog& catalog) {
    const Json operations = LoadJsonFile(ModStore_ModsDirectory() / OperationsFileName);
    std::map<std::string, StoreModState> states;
    for (const auto& [id, mod] : catalog.mods) {
        states[id] = ReadState(mod, operations);
    }
    return states;
}

bool IsRestartState(StoreModState state) {
    return state == StoreModState::InstallsOnRestart || state == StoreModState::DisabledOnRestart ||
           state == StoreModState::RemovedOnRestart;
}

void PublishStates() {
    StoreCatalog catalog;
    {
        std::lock_guard<std::mutex> lock(sViewMutex);
        catalog = sView.catalog;
    }
    auto states = ReadStates(catalog);
    const bool restartNeeded =
        std::any_of(states.begin(), states.end(), [](const auto& entry) { return IsRestartState(entry.second); });
    std::lock_guard<std::mutex> lock(sViewMutex);
    sView.states = std::move(states);
    sView.restartNeeded = sReleaseStaged || restartNeeded;
}

std::optional<std::string> DownloadSignedDocument(const char* url, SignedDocumentKind kind, std::string& error) {
    const StoreHttpResult download = ModStore_HttpsGet(url, MaxDocumentBytes);
    if (!download.error.empty()) {
        error = download.error;
        return std::nullopt;
    }
    auto payload = ModTrust_OpenSignedDocument(std::string(download.body.begin(), download.body.end()), kind);
    if (!payload) {
        error = "its signature does not match the key built into this Unbound";
    }
    return payload;
}

std::optional<StoreRelease> FetchNewerRelease(const std::string& skippedVersion) {
    std::string error;
    auto payload = DownloadSignedDocument(ReleaseUrl, SignedDocumentKind::Release, error);
    if (!payload) {
        SPDLOG_WARN("[ModStore] No release manifest: {}", error);
        return std::nullopt;
    }
    auto release = ModStore_ParseRelease(*payload);
    const bool wanted = release && ModStore_CompareVersions(release->version, gBuildForkVersion) > 0 &&
                        release->version != skippedVersion;
    return wanted ? release : std::nullopt;
}

void QueueAnnouncements(const StoreView& view) {
    if (view.newerRelease) {
        sAnnouncements.push_back("Unbound " + view.newerRelease->version + " is available");
    }
    const auto outdated = std::count_if(view.states.begin(), view.states.end(),
                                        [](const auto& entry) { return entry.second == StoreModState::Outdated; });
    if (outdated > 0) {
        sAnnouncements.push_back(std::to_string(outdated) + " mod update(s) available");
    }
}

void RefreshNow(const std::string& skippedVersion) {
    SetStatus("Checking for updates...");
    std::string error;
    auto payload = DownloadSignedDocument(CatalogUrl, SignedDocumentKind::Catalog, error);
    if (!payload) {
        SetStatus("Could not read the mod catalog: " + error);
        return;
    }
    auto catalog = ModStore_ParseCatalog(*payload);
    if (!catalog) {
        SetStatus("The mod catalog could not be read");
        return;
    }

    const fs::path statePath = ModStore_ModsDirectory() / StateFileName;
    Json state = LoadJsonFile(statePath);
    // A replayed older catalog could bring back a build that was since withdrawn.
    if (catalog->sequence < state.value("sequence", uint64_t(0))) {
        SetStatus("Ignored a mod catalog older than one already seen");
        return;
    }
    state["sequence"] = catalog->sequence;
    SaveJsonFile(statePath, state);

    auto states = ReadStates(*catalog);
    auto release = FetchNewerRelease(skippedVersion);
    std::lock_guard<std::mutex> lock(sViewMutex);
    sView.catalog = std::move(*catalog);
    sView.states = std::move(states);
    sView.newerRelease = std::move(release);
    sView.hasCatalog = true;
    sView.status = "Up to date with the catalog";
    if (sAnnounceNextRefresh) {
        sAnnounceNextRefresh = false;
        QueueAnnouncements(sView);
    }
}

std::vector<std::string> ResolveInstallOrder(const StoreCatalog& catalog, const std::vector<std::string>& ids,
                                             std::string& error) {
    std::vector<std::string> order;
    std::set<std::string> visiting;
    std::function<void(const std::string&)> visit = [&](const std::string& id) {
        if (!error.empty() || visiting.count(id) != 0) {
            return;
        }
        visiting.insert(id);
        const auto mod = catalog.mods.find(id);
        if (mod == catalog.mods.end()) {
            error = "the store has no mod called " + id;
            return;
        }
        for (const std::string& dependency : mod->second.dependencies) {
            visit(dependency);
        }
        order.push_back(id);
    };
    for (const std::string& id : ids) {
        visit(id);
    }
    return order;
}

bool NeedsDownload(StoreModState state, bool requested) {
    switch (state) {
        case StoreModState::NotInstalled:
        case StoreModState::Outdated:
        case StoreModState::RemovedOnRestart:
            return true;
        case StoreModState::Installed:
        case StoreModState::InstallsOnRestart:
        case StoreModState::Unavailable:
            return false;
        case StoreModState::Disabled:
        case StoreModState::DisabledOnRestart:
            return requested;
        default:
            SPDLOG_WARN("[ModStore] Unknown mod state {}", static_cast<int>(state));
            return false;
    }
}

bool DownloadPackage(const StoreMod& mod, int major, std::string& error) {
    const auto build = mod.builds.find(major);
    if (build == mod.builds.end()) {
        error = mod.name + " has no build for this version of Unbound yet";
        return false;
    }
    const StoreHttpResult download = ModStore_HttpsGet(build->second.url, MaxPackageBytes);
    if (!download.error.empty()) {
        error = download.error;
        return false;
    }
    if (download.body.size() != build->second.size ||
        ModTrust_HashBytes(download.body.data(), download.body.size()) != build->second.hash) {
        error = mod.name + " did not match the catalog; nothing was installed";
        return false;
    }

    const fs::path mods = ModStore_ModsDirectory();
    std::error_code ignored;
    fs::create_directories(mods, ignored);
    if (!WriteFileReplacing(PackagePath(mods, mod.id, PendingSuffix), download.body)) {
        error = "could not write " + PackagePath(mods, mod.id, PendingSuffix).string();
        return false;
    }
    fs::remove(PackagePath(mods, mod.id, DisabledSuffix), ignored);
    SetOperation(mod.id, nullptr);
    return true;
}

void InstallNow(const std::vector<std::string>& ids) {
    StoreCatalog catalog;
    std::map<std::string, StoreModState> states;
    {
        std::lock_guard<std::mutex> lock(sViewMutex);
        catalog = sView.catalog;
        states = sView.states;
    }
    std::string error;
    const std::vector<std::string> order = ResolveInstallOrder(catalog, ids, error);
    const std::set<std::string> requested(ids.begin(), ids.end());
    std::vector<std::string> downloads;
    for (const std::string& id : order) {
        if (NeedsDownload(states[id], requested.count(id) != 0)) {
            downloads.push_back(id);
        }
    }

    for (size_t i = 0; i < downloads.size() && error.empty(); ++i) {
        const StoreMod& mod = catalog.mods.at(downloads[i]);
        SetStatus("Downloading " + mod.name + " (" + std::to_string(i + 1) + "/" + std::to_string(downloads.size()) +
                  ")...");
        DownloadPackage(mod, SOH_MOD_API_MAJOR, error);
    }
    PublishStates();
    SetStatus(error.empty() ? std::to_string(downloads.size()) + " mod(s) ready: they load after a restart"
                            : "Stopped: " + error);
}

void StageModsForMajor(int major) {
    StoreCatalog catalog;
    std::map<std::string, StoreModState> states;
    {
        std::lock_guard<std::mutex> lock(sViewMutex);
        catalog = sView.catalog;
        states = sView.states;
    }
    for (const auto& [id, state] : states) {
        if (!ModStore_IsInstalled(state)) {
            continue;
        }
        std::string error;
        if (!DownloadPackage(catalog.mods.at(id), major, error)) {
            SPDLOG_WARN("[ModStore] Disabling {} until it has a build for mod API {}: {}", id, major, error);
            SetOperation(id, DisableOperation);
        }
    }
}

void EmitAnnouncements() {
    std::vector<std::string> announcements;
    {
        std::unique_lock<std::mutex> lock(sViewMutex, std::try_to_lock);
        if (!lock.owns_lock() || sAnnouncements.empty()) {
            return;
        }
        announcements.swap(sAnnouncements);
    }
    for (const std::string& message : announcements) {
        Notification::Emit({ .prefix = "NEI-Store", .message = message, .remainingTime = 10.0f });
    }
}

} // namespace

std::unique_lock<std::mutex> ModStore_LockView() {
    return std::unique_lock<std::mutex>(sViewMutex);
}

StoreView& ModStore_View() {
    return sView;
}

fs::path ModStore_ModsDirectory() {
    return ModLoader_GetModsDirectory();
}

bool ModStore_IsInstalled(StoreModState state) {
    return state == StoreModState::Installed || state == StoreModState::Outdated ||
           state == StoreModState::InstallsOnRestart;
}

std::vector<std::string> ModStore_FindDependents(const StoreView& view, const std::string& id) {
    std::vector<std::string> dependents;
    for (const auto& [otherId, mod] : view.catalog.mods) {
        const auto state = view.states.find(otherId);
        const bool active = state != view.states.end() && ModStore_IsInstalled(state->second);
        if (active && std::find(mod.dependencies.begin(), mod.dependencies.end(), id) != mod.dependencies.end()) {
            dependents.push_back(mod.name);
        }
    }
    return dependents;
}

void ModStore_Refresh() {
    // CVars belong to the game thread; the worker only gets a copy.
    std::string skippedVersion = CVarGetString(CVAR_STORE_SKIPPED_VERSION, "");
    Worker().Post([skippedVersion] { RefreshNow(skippedVersion); });
}

void ModStore_Install(const std::vector<std::string>& ids) {
    Worker().Post([ids] { InstallNow(ids); });
}

void ModStore_Disable(const std::string& id) {
    Worker().Post([id] {
        const fs::path mods = ModStore_ModsDirectory();
        std::error_code ignored;
        fs::remove(PackagePath(mods, id, PendingSuffix), ignored);
        if (fs::exists(PackagePath(mods, id), ignored)) {
            SetOperation(id, DisableOperation);
        }
        PublishStates();
    });
}

void ModStore_Enable(const std::string& id) {
    Worker().Post([id] {
        const fs::path mods = ModStore_ModsDirectory();
        SetOperation(id, nullptr);
        std::error_code error;
        // An archive that is not mounted can be renamed now; the mods folder scan picks it up on the next launch.
        if (fs::exists(PackagePath(mods, id, DisabledSuffix), error)) {
            fs::rename(PackagePath(mods, id, DisabledSuffix), PackagePath(mods, id, PendingSuffix), error);
        }
        PublishStates();
    });
}

void ModStore_Remove(const std::string& id) {
    Worker().Post([id] {
        const fs::path mods = ModStore_ModsDirectory();
        std::error_code ignored;
        fs::remove(PackagePath(mods, id, PendingSuffix), ignored);
        fs::remove(PackagePath(mods, id, DisabledSuffix), ignored);
        SetOperation(id, fs::exists(PackagePath(mods, id), ignored) ? RemoveOperation : nullptr);
        PublishStates();
    });
}

void ModStore_SkipRelease(const std::string& version) {
    CVarSetString(CVAR_STORE_SKIPPED_VERSION, version.c_str());
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    std::lock_guard<std::mutex> lock(sViewMutex);
    sView.newerRelease.reset();
}

void ModStore_InstallRelease() {
    Worker().Post([] {
        std::optional<StoreRelease> release;
        {
            std::lock_guard<std::mutex> lock(sViewMutex);
            release = sView.newerRelease;
        }
        if (!release) {
            return;
        }
        SetStatus("Downloading Unbound " + release->version + "...");
        std::string error;
        if (!ModStore_StageRelease(*release, error)) {
            SetStatus("Unbound was not updated: " + error);
            return;
        }
        if (release->modApiMajor != SOH_MOD_API_MAJOR) {
            SetStatus("Downloading your mods for Unbound " + release->version + "...");
            StageModsForMajor(release->modApiMajor);
        }
        {
            std::lock_guard<std::mutex> lock(sViewMutex);
            sReleaseStaged = true;
        }
        PublishStates();
        std::lock_guard<std::mutex> lock(sViewMutex);
        sView.newerRelease.reset();
        sView.status = "Unbound " + release->version + " is ready: close Unbound and open it again";
    });
}

void ModStore_ApplyPending(const fs::path& modsDirectory) {
    const fs::path path = modsDirectory / OperationsFileName;
    const Json operations = LoadJsonFile(path);
    std::error_code error;
    for (const std::string& id : ReadOperation(operations, DisableOperation)) {
        fs::remove(PackagePath(modsDirectory, id, DisabledSuffix), error);
        fs::rename(PackagePath(modsDirectory, id), PackagePath(modsDirectory, id, DisabledSuffix), error);
    }
    for (const std::string& id : ReadOperation(operations, RemoveOperation)) {
        fs::remove(PackagePath(modsDirectory, id), error);
    }
    fs::remove(path, error);
}

void ModStore_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(EmitAnnouncements);
    if (CVarGetInteger(CVAR_STORE_CHECK_AT_STARTUP, 0) == 1 && ModStore_HasHttps()) {
        sAnnounceNextRefresh = true;
        ModStore_Refresh();
    }
}
