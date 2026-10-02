#include "ModApi.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <SDL2/SDL_messagebox.h>

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "ModPermissions/ModPermissions.h"
#include "ModTrust/ModTrust.h"
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

    const void* GetBaseAddress() const {
#if defined(_WIN32)
        return mHandle;
#elif defined(__APPLE__) || defined(__linux__)
        Dl_info info;
        void* symbol = GetFunction("ModSetApi");
        return symbol != nullptr && dladdr(symbol, &info) != 0 ? info.dli_fbase : nullptr;
#else
        return nullptr;
#endif
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

struct NativeModPackage {
    std::string name;
    std::string archivePath;
    std::shared_ptr<std::vector<char>> binary;
    std::vector<std::string> requiredResources;
    std::vector<std::string> permissions;
    ModProvenance provenance;
};

std::vector<std::unique_ptr<LoadedMod>> sLoadedMods;
std::unordered_map<std::string, std::string> sTrustLabels;
std::vector<std::string> sRefusedMods;

std::filesystem::path GetCachePath() {
    return ModLoader_GetModsDirectory() / ".unbound-modapi-cache.json";
}

Json LoadJsonObject(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return Json::object();
    }

    try {
        Json object = Json::parse(input);
        return object.is_object() ? object : Json::object();
    } catch (const std::exception& exception) {
        SPDLOG_WARN("[ModApi] Ignoring '{}': {}", path.string(), exception.what());
        return Json::object();
    }
}

void SaveJsonObject(const std::filesystem::path& path, const Json& object) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        SPDLOG_WARN("[ModApi] Could not write '{}'", path.string());
        return;
    }
    output << object.dump(2) << '\n';
}

#if defined(UNBOUND_MOD_POLICY_ASK)
enum class UnverifiedModChoice : int {
    Refuse,
    LoadOnce,
    LoadAlways,
};

std::filesystem::path GetApprovalsPath() {
    return ModLoader_GetModsDirectory() / ".unbound-mod-approvals.json";
}

std::string DescribeUnverifiedMod(const NativeModPackage& package) {
    std::string message = "'" + package.name + "' wants to run native code on this computer, and Unbound's " +
                          "sign-mod workflow did not sign it (" + package.provenance.problem + ").\n\n";
    const std::vector<std::string> imports = ModTrust_ListUnusualImports(*package.binary);
    if (!imports.empty()) {
        message += "It also calls into the system beyond what mods need:\n";
        for (const std::string& import : imports) {
            message += "  " + import + "\n";
        }
        message += "\n";
    }
    return message + "Content hash " + package.provenance.contentHash.substr(0, 16) +
           "\n\nOnly load it if you trust whoever gave it to you.";
}

UnverifiedModChoice AskAboutUnverifiedMod(const NativeModPackage& package) {
    const std::string message = DescribeUnverifiedMod(package);
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
          static_cast<int>(UnverifiedModChoice::Refuse), "Don't load" },
        { 0, static_cast<int>(UnverifiedModChoice::LoadOnce), "Load once" },
        { 0, static_cast<int>(UnverifiedModChoice::LoadAlways), "Always load this version" },
    };
    const SDL_MessageBoxData box = {
        SDL_MESSAGEBOX_WARNING, nullptr, "Unsigned mod", message.c_str(), SDL_arraysize(buttons), buttons, nullptr,
    };
    int choice = static_cast<int>(UnverifiedModChoice::Refuse);
    if (SDL_ShowMessageBox(&box, &choice) != 0 || choice < 0) {
        return UnverifiedModChoice::Refuse;
    }
    return static_cast<UnverifiedModChoice>(choice);
}

bool CanRunUnverifiedMod(const NativeModPackage& package) {
    static Json approvals = LoadJsonObject(GetApprovalsPath());
    if (approvals.contains(package.provenance.contentHash)) {
        return true;
    }

    const UnverifiedModChoice choice = AskAboutUnverifiedMod(package);
    if (choice == UnverifiedModChoice::LoadAlways) {
        approvals[package.provenance.contentHash] = package.name;
        SaveJsonObject(GetApprovalsPath(), approvals);
    }
    return choice != UnverifiedModChoice::Refuse;
}

bool CanRunMod(const NativeModPackage& package) {
    return package.provenance.state == ModTrustState::Verified || CanRunUnverifiedMod(package);
}
#elif defined(UNBOUND_MOD_POLICY_SIGNED)
bool CanRunMod(const NativeModPackage& package) {
    if (package.provenance.state == ModTrustState::Verified) {
        return true;
    }
    sRefusedMods.push_back(package.name + ": " + ModTrust_Describe(package.provenance));
    return false;
}
#else
bool CanRunMod(const NativeModPackage&) {
    return true;
}
#endif

void ReportRefusedMods() {
    if (sRefusedMods.empty()) {
        return;
    }
    std::string message = "This build of Unbound only runs native mods signed by its sign-mod workflow. "
                          "These were not loaded:\n\n";
    for (const std::string& refused : sRefusedMods) {
        message += "- " + refused + "\n";
    }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Mods not loaded", message.c_str(), nullptr);
}

bool ReadManifest(const std::shared_ptr<Ship::Archive>& archive, Json& manifest) {
    auto file = archive->LoadFile("manifest.json");
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        return false;
    }

    manifest = Json::parse(file->Buffer->begin(), file->Buffer->end());
    return manifest.is_object();
}

std::vector<std::string> ReadStringArray(const Json& manifest, const char* key) {
    std::vector<std::string> values;
    const auto array = manifest.find(key);
    if (array == manifest.end() || !array->is_array()) {
        return values;
    }
    for (const Json& value : *array) {
        if (value.is_string()) {
            values.push_back(value.get<std::string>());
        }
    }
    return values;
}

std::optional<NativeModPackage> ReadNativeModPackage(const std::shared_ptr<Ship::Archive>& archive,
                                                     const std::string& platform) {
    Json manifest;
    if (!ReadManifest(archive, manifest) || !manifest.contains("binaries") || !manifest["binaries"].is_object()) {
        return std::nullopt;
    }

    auto binaryEntry = manifest["binaries"].find(platform);
    if (binaryEntry == manifest["binaries"].end() || !binaryEntry->is_string()) {
        return std::nullopt;
    }

    NativeModPackage package;
    const std::string binaryPath = binaryEntry->get<std::string>();
    const std::string fallbackName = std::filesystem::path(archive->GetPath()).stem().string();
    package.name = manifest.value("name", fallbackName);
    package.archivePath = archive->GetPath();
    auto file = archive->LoadFile(binaryPath);
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        SPDLOG_ERROR("[ModApi] Could not read '{}' from mod '{}'", binaryPath, package.name);
        return std::nullopt;
    }
    package.binary = file->Buffer;
    package.requiredResources = ReadStringArray(manifest, "requires");
    package.permissions = ReadStringArray(manifest, "permissions");
    package.provenance = ModTrust_Verify(*archive);
    return package;
}

std::string GetTrustKey(const ModProvenance& provenance) {
    return provenance.state == ModTrustState::Verified ? "repo:" + provenance.repository
                                                       : "hash:" + provenance.contentHash;
}

std::unique_ptr<LoadedMod> LoadNativeMod(const NativeModPackage& package) {
    auto mod = std::make_unique<LoadedMod>();
    mod->name = package.name;
    mod->archivePath = package.archivePath;
    mod->requiredResources = package.requiredResources;
    mod->binaryHash = Hex(HashBytes(*package.binary));
    if (!mod->library.Load(*package.binary)) {
        SPDLOG_ERROR("[ModApi] Native library for mod '{}' could not be loaded", package.name);
        return nullptr;
    }
    ModPermissions_RegisterMod({ package.name, ModTrust_Describe(package.provenance), GetTrustKey(package.provenance),
                                 ModPermissions_Parse(package.permissions, package.name),
                                 mod->library.GetBaseAddress() });

    auto setApi = reinterpret_cast<SOHModSetApiFunc>(mod->library.GetFunction("ModSetApi"));
    if (setApi == nullptr) {
        SPDLOG_ERROR("[ModApi] Mod '{}' exports no ModSetApi", package.name);
        return nullptr;
    }

    mod->getRequirements = reinterpret_cast<SOHModGetRequirementsFunc>(mod->library.GetFunction("ModGetRequirements"));
    mod->init = reinterpret_cast<SOHModInitFunc>(mod->library.GetFunction("ModInit"));

    CurrentModScope currentMod(package.name);
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
    std::vector<NativeModPackage> packages;
    for (const auto& archive : *archives) {
        try {
            auto package = ReadNativeModPackage(archive, platform);
            if (package) {
                packages.push_back(std::move(*package));
            }
        } catch (const std::exception& exception) {
            std::string message =
                "[ModApi] Failed to inspect archive '" + archive->GetPath() + "': " + exception.what();
            SPDLOG_ERROR("{}", message);
        }
    }

    std::unordered_set<std::string> modNames;
    for (const NativeModPackage& package : packages) {
        if (!modNames.insert(package.name).second) {
            SPDLOG_ERROR("[ModApi] Native mod name '{}' is duplicated; ignoring '{}'", package.name,
                         package.archivePath);
            continue;
        }

        const bool allowed = CanRunMod(package);
        const std::string trust = ModTrust_Describe(package.provenance);
        sTrustLabels[package.archivePath] = allowed ? trust : trust + ", not loaded";
        if (!allowed) {
            SPDLOG_WARN("[ModApi] Not loading mod '{}' ({})", package.name, trust);
            continue;
        }

        SPDLOG_INFO("[ModApi] Mod '{}' is {}", package.name, trust);
        try {
            auto mod = LoadNativeMod(package);
            if (mod != nullptr) {
                sLoadedMods.push_back(std::move(mod));
            }
        } catch (const std::exception& exception) {
            SPDLOG_ERROR("[ModApi] Mod '{}' failed to load: {}", package.name, exception.what());
        }
    }
    ReportRefusedMods();

    const std::string environment = GetEnvironmentFingerprint();
    const std::filesystem::path cachePath = GetCachePath();
    const Json previousCache = LoadJsonObject(cachePath);
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

    SaveJsonObject(cachePath, nextCache);
}

std::filesystem::path ModLoader_GetModsDirectory() {
    std::string modsPath = Ship::Context::LocateFileAcrossAppDirs("mods", AppShortName);
    if (modsPath.empty()) {
        modsPath = Ship::Context::GetPathRelativeToAppDirectory("mods", AppShortName);
    }
    return std::filesystem::path(modsPath);
}

std::string ModLoader_DescribeTrust(const std::string& archivePath) {
    auto label = sTrustLabels.find(archivePath);
    return label == sTrustLabels.end() ? std::string() : label->second;
}
