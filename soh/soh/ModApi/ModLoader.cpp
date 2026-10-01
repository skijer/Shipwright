#include "ModApi.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "soh/ResourceManagerHelpers.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

using Json = nlohmann::json;

constexpr uint32_t MaxRequirements = 4096;
constexpr const char* AppShortName = "soh";

class CurrentModScope {
  public:
    CurrentModScope(const std::string& name) {
        ModApi_SetCurrentMod(name);
    }

    ~CurrentModScope() {
        ModApi_ClearCurrentMod();
    }
};

const char* GetPlatformKey() {
#if defined(_WIN64)
    return "windows_x64";
#elif defined(_WIN32)
    return "windows_x86";
#elif defined(__APPLE__)
    return "darwin";
#elif defined(__linux__)
    return "linux_x64";
#else
    return nullptr;
#endif
}

uint64_t HashBytes(const std::vector<char>& bytes) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char value : bytes) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t HashText(uint64_t hash, const std::string& text) {
    for (unsigned char value : text) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string Hex(uint64_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(16, '0');
    for (size_t i = 0; i < result.size(); ++i) {
        result[result.size() - i - 1] = digits[value & 0xF];
        value >>= 4;
    }
    return result;
}

class ModLibrary {
  public:
    ModLibrary() = default;
    ModLibrary(const ModLibrary&) = delete;
    ModLibrary& operator=(const ModLibrary&) = delete;

    ModLibrary(ModLibrary&& other) noexcept {
        *this = std::move(other);
    }

    ModLibrary& operator=(ModLibrary&& other) noexcept {
        if (this != &other) {
            Unload();
            mHandle = other.mHandle;
            mFileToDelete = std::move(other.mFileToDelete);
            other.mHandle = nullptr;
            other.mFileToDelete.clear();
        }
        return *this;
    }

    ~ModLibrary() {
        Unload();
    }

    bool Load(const std::vector<char>& libraryImage) {
        const std::string path = WriteToTemporaryFile(libraryImage);
        if (path.empty()) {
            return false;
        }

#if defined(_WIN32)
        mHandle = LoadLibraryA(path.c_str());
        mFileToDelete = path;
#elif defined(__APPLE__) || defined(__linux__)
        mHandle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        unlink(path.c_str());
#else
        std::filesystem::remove(path);
#endif
        return mHandle != nullptr;
    }

    void* GetFunction(const char* name) const {
        if (mHandle == nullptr) {
            return nullptr;
        }
#if defined(_WIN32)
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(mHandle), name));
#elif defined(__APPLE__) || defined(__linux__)
        return dlsym(mHandle, name);
#else
        return nullptr;
#endif
    }

  private:
    static std::string WriteToTemporaryFile(const std::vector<char>& libraryImage) {
        std::string path;

#if defined(_WIN32)
        char temporaryDirectory[MAX_PATH];
        char temporaryFile[MAX_PATH];
        if (GetTempPathA(MAX_PATH, temporaryDirectory) == 0 ||
            GetTempFileNameA(temporaryDirectory, "oub", 0, temporaryFile) == 0) {
            return {};
        }
        path = temporaryFile;
#elif defined(__APPLE__) || defined(__linux__)
        char pathTemplate[] = "/tmp/unbound_mod_XXXXXX";
        int descriptor = mkstemp(pathTemplate);
        if (descriptor == -1) {
            return {};
        }
        fchmod(descriptor, 0755);
        close(descriptor);
        path = pathTemplate;
#else
        return {};
#endif

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(libraryImage.data(), static_cast<std::streamsize>(libraryImage.size()));
        output.close();
        if (!output) {
            std::filesystem::remove(path);
            return {};
        }
        return path;
    }

    void Unload() {
        if (mHandle == nullptr) {
            return;
        }
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(mHandle));
        if (!mFileToDelete.empty()) {
            DeleteFileA(mFileToDelete.c_str());
        }
#elif defined(__APPLE__) || defined(__linux__)
        dlclose(mHandle);
#endif
        mHandle = nullptr;
    }

#if defined(_WIN32)
    HMODULE mHandle = nullptr;
#else
    void* mHandle = nullptr;
#endif
    std::string mFileToDelete;
};

struct LoadedMod {
    std::string name;
    std::string archivePath;
    std::string binaryHash;
    ModLibrary library;
    SOHModGetRequirementsFunc getRequirements = nullptr;
    SOHModInitFunc init = nullptr;
    std::vector<std::string> requiredHooks;
    std::vector<std::string> requiredResources;
    bool compatible = false;
    bool compatibilityCached = false;
};

std::vector<std::unique_ptr<LoadedMod>> sLoadedMods;

std::filesystem::path GetCachePath() {
    std::string modsPath = Ship::Context::LocateFileAcrossAppDirs("mods", AppShortName);
    if (modsPath.empty()) {
        modsPath = Ship::Context::GetPathRelativeToAppDirectory("mods", AppShortName);
    }
    return std::filesystem::path(modsPath) / ".unbound-modapi-cache.json";
}

Json LoadCache(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return Json::object();
    }

    try {
        Json cache = Json::parse(input);
        return cache.is_object() ? cache : Json::object();
    } catch (const std::exception& exception) {
        SPDLOG_WARN("[ModApi] Ignoring compatibility cache '{}': {}", path.string(), exception.what());
        return Json::object();
    }
}

void SaveCache(const std::filesystem::path& path, const Json& cache) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        SPDLOG_WARN("[ModApi] Could not write compatibility cache '{}'", path.string());
        return;
    }
    output << cache.dump(2) << '\n';
}

bool ReadManifest(const std::shared_ptr<Ship::Archive>& archive, Json& manifest) {
    auto file = archive->LoadFile("manifest.json");
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        return false;
    }

    manifest = Json::parse(file->Buffer->begin(), file->Buffer->end());
    return manifest.is_object();
}

std::unique_ptr<LoadedMod> LoadLibraryFromArchive(const std::shared_ptr<Ship::Archive>& archive,
                                                  const std::string& platform) {
    Json manifest;
    if (!ReadManifest(archive, manifest) || !manifest.contains("binaries") || !manifest["binaries"].is_object()) {
        return nullptr;
    }

    auto binaryEntry = manifest["binaries"].find(platform);
    if (binaryEntry == manifest["binaries"].end() || !binaryEntry->is_string()) {
        return nullptr;
    }

    const std::string binaryPath = binaryEntry->get<std::string>();
    const std::string fallbackName = std::filesystem::path(archive->GetPath()).stem().string();
    const std::string name = manifest.value("name", fallbackName);
    auto file = archive->LoadFile(binaryPath);
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        SPDLOG_ERROR("[ModApi] Could not read '{}' from mod '{}'", binaryPath, name);
        return nullptr;
    }

    auto mod = std::make_unique<LoadedMod>();
    mod->name = name;
    mod->archivePath = archive->GetPath();
    if (manifest.contains("requires") && manifest["requires"].is_array()) {
        for (const Json& required : manifest["requires"]) {
            if (required.is_string()) {
                mod->requiredResources.push_back(required.get<std::string>());
            }
        }
    }
    mod->binaryHash = Hex(HashBytes(*file->Buffer));
    if (!mod->library.Load(*file->Buffer)) {
        SPDLOG_ERROR("[ModApi] Native library for mod '{}' could not be loaded", name);
        return nullptr;
    }

    auto setApi = reinterpret_cast<SOHModSetApiFunc>(mod->library.GetFunction("ModSetApi"));
    if (setApi == nullptr) {
        SPDLOG_ERROR("[ModApi] Mod '{}' exports no ModSetApi", name);
        return nullptr;
    }

    mod->getRequirements = reinterpret_cast<SOHModGetRequirementsFunc>(mod->library.GetFunction("ModGetRequirements"));
    mod->init = reinterpret_cast<SOHModInitFunc>(mod->library.GetFunction("ModInit"));

    CurrentModScope currentMod(name);
    setApi(ModApi_Get());
    return mod;
}

bool ReadRequirements(LoadedMod& mod) {
    if (mod.getRequirements == nullptr) {
        return true;
    }

    const SOHModRequirements* requirements = mod.getRequirements();
    if (requirements == nullptr || requirements->structSize < sizeof(SOHModRequirements) ||
        requirements->requiredHookCount > MaxRequirements ||
        (requirements->requiredHookCount != 0 && requirements->requiredHooks == nullptr)) {
        SPDLOG_ERROR("[ModApi] Mod '{}' returned invalid requirements", mod.name);
        return false;
    }

    for (uint32_t i = 0; i < requirements->requiredHookCount; ++i) {
        const char* name = requirements->requiredHooks[i];
        if (name == nullptr) {
            SPDLOG_ERROR("[ModApi] Mod '{}' returned a null hook requirement", mod.name);
            return false;
        }
        mod.requiredHooks.emplace_back(name);
    }
    return true;
}

bool CheckRequirements(const LoadedMod& mod) {
    bool compatible = true;
    for (const auto& name : mod.requiredHooks) {
        if (!ModApi_HasHook(name.c_str())) {
            SPDLOG_ERROR("[ModApi] Mod '{}' requires missing hook '{}'", mod.name, name);
            compatible = false;
        }
    }
    return compatible;
}

bool HasRequiredResources(const LoadedMod& mod) {
    for (const auto& path : mod.requiredResources) {
        if (!ResourceMgr_FileExists(path.c_str())) {
            SPDLOG_ERROR("[ModApi] Mod '{}' needs the resource '{}', which no loaded archive provides", mod.name, path);
            return false;
        }
    }
    return true;
}

std::string CacheKey(const LoadedMod& mod) {
    return mod.archivePath + "\n" + mod.name;
}

std::string GetEnvironmentFingerprint() {
    uint64_t hash = 1469598103934665603ULL;
    hash = HashText(hash, ModApi_GetCatalogFingerprint());
    return Hex(hash);
}

} // namespace

void ModLoader_LoadMods() {
    const char* platform = GetPlatformKey();
    if (platform == nullptr) {
        SPDLOG_INFO("[ModApi] Native mods are not supported on this platform");
        return;
    }

    auto archives = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager()->GetArchives();
    std::unordered_set<std::string> modNames;
    for (const auto& archive : *archives) {
        try {
            auto mod = LoadLibraryFromArchive(archive, platform);
            if (mod != nullptr) {
                if (!modNames.insert(mod->name).second) {
                    SPDLOG_ERROR("[ModApi] Native mod name '{}' is duplicated; ignoring '{}'", mod->name,
                                 mod->archivePath);
                    continue;
                }
                sLoadedMods.push_back(std::move(mod));
            }
        } catch (const std::exception& exception) {
            std::string message =
                "[ModApi] Failed to inspect archive '" + archive->GetPath() + "': " + exception.what();
            SPDLOG_ERROR("{}", message);
        }
    }

    const std::string environment = GetEnvironmentFingerprint();
    const std::filesystem::path cachePath = GetCachePath();
    const Json previousCache = LoadCache(cachePath);
    Json nextCache = Json::object();

    for (const auto& mod : sLoadedMods) {
        const std::string key = CacheKey(*mod);
        const bool cached = previousCache.contains(key) && previousCache[key].is_object() &&
                            previousCache[key].value("binary", "") == mod->binaryHash &&
                            previousCache[key].value("environment", "") == environment &&
                            previousCache[key].contains("compatible") &&
                            previousCache[key]["compatible"].is_boolean() && previousCache[key].contains("hooks") &&
                            previousCache[key]["hooks"].is_array();
        mod->compatibilityCached = cached;
        mod->compatible = cached && previousCache[key]["compatible"].get<bool>();

        if (cached) {
            const bool validHooks = std::all_of(previousCache[key]["hooks"].begin(), previousCache[key]["hooks"].end(),
                                                [](const Json& value) { return value.is_string(); });
            if (validHooks) {
                mod->requiredHooks = previousCache[key]["hooks"].get<std::vector<std::string>>();
            } else {
                mod->compatibilityCached = false;
                mod->compatible = false;
            }
        }

        if (!mod->compatibilityCached) {
            try {
                CurrentModScope currentMod(mod->name);
                mod->compatible = ReadRequirements(*mod) && CheckRequirements(*mod);
            } catch (const std::exception& exception) {
                SPDLOG_ERROR("[ModApi] Mod '{}' failed compatibility verification: {}", mod->name, exception.what());
                mod->compatible = false;
            }
        }
    }

    for (const auto& mod : sLoadedMods) {
        const std::string key = CacheKey(*mod);
        nextCache[key] = { { "binary", mod->binaryHash },
                           { "environment", environment },
                           { "compatible", mod->compatible },
                           { "hooks", mod->requiredHooks } };
        if (!mod->compatible || !HasRequiredResources(*mod)) {
            continue;
        }

        try {
            if (mod->init != nullptr) {
                CurrentModScope currentMod(mod->name);
                mod->init();
            }
            SPDLOG_INFO("[ModApi] Loaded mod '{}'{}", mod->name,
                        mod->compatibilityCached ? " (compatibility cached)" : "");
        } catch (const std::exception& exception) {
            SPDLOG_ERROR("[ModApi] Mod '{}' failed to initialize: {}", mod->name, exception.what());
        }
    }

    SaveCache(cachePath, nextCache);
}
