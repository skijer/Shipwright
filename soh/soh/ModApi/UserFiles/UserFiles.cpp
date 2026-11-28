#include "UserFiles.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <libultraship/libultraship.h>
#include <spdlog/spdlog.h>

#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/ModPermissions/ModPermissions.h"
#include "soh/ModApi/AssetSetup/AssetSetup.h"

#if !defined(__SWITCH__) && !defined(__WIIU__) && !defined(__EMSCRIPTEN__)
#define USER_FILES_HAS_DIALOGS 1
#include "soh/Extractor/portable-file-dialogs.h"
#endif

namespace {

constexpr uint64_t kMaxPickedFileSize = 1ULL << 30;
constexpr size_t kMaxFileNameLength = 128;
constexpr const char* kPendingSuffix = ".pending";

// Letters, digits, '.', '_' and '-', not starting with '.': no folders, and none of the game's own dotfiles.
bool IsPlainFileName(const char* name) {
    if (name == nullptr || name[0] == '\0' || name[0] == '.' || std::strlen(name) > kMaxFileNameLength) {
        return false;
    }
    const std::string text(name);
    const bool plain = std::all_of(text.begin(), text.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '.' || character == '_' || character == '-';
    });
    return plain && std::filesystem::path(text).extension() != kPendingSuffix;
}

std::string DescribeSize(uint64_t bytes) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    return text;
}

bool CanUseFiles(const ModIdentity* mod) {
    if (mod == nullptr) {
        SPDLOG_ERROR("[UserFiles] Only a mod can ask for files");
        return false;
    }
    return ModPermissions_HasDeclared(*mod, ModPermission::Files);
}

bool PlaceInModsFolder(const std::filesystem::path& source, const std::string& fileName) {
    const std::filesystem::path mods = ModLoader_GetModsDirectory();
    std::error_code error;
    std::filesystem::create_directories(mods, error);
    const bool isTaken = std::filesystem::exists(mods / fileName);
    const std::filesystem::path target = isTaken ? mods / (fileName + kPendingSuffix) : mods / fileName;
    std::filesystem::remove(target, error);
    std::filesystem::rename(source, target, error);
    if (error) {
        std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, error);
        std::filesystem::remove(source);
    }
    if (error) {
        SPDLOG_ERROR("[UserFiles] Could not place '{}' in the mods folder: {}", fileName, error.message());
        return false;
    }
    SPDLOG_INFO("[UserFiles] Wrote '{}'{}", target.string(), isTaken ? ", installed at the next start" : "");
    return true;
}

bool WriteBytes(const std::filesystem::path& path, const void* data, uint64_t size) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return output.good();
}

bool ReadPickedFile(const std::filesystem::path& path, SOHUserFile* file) {
    std::error_code error;
    const uint64_t size = std::filesystem::file_size(path, error);
    if (error || size > kMaxPickedFileSize) {
        SPDLOG_ERROR("[UserFiles] '{}' cannot be read or is larger than 1 GB", path.string());
        return false;
    }
    auto* data = static_cast<uint8_t*>(std::malloc(size > 0 ? size : 1));
    std::ifstream input(path, std::ios::binary);
    if (data == nullptr || !input.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size))) {
        std::free(data);
        return false;
    }
    file->data = data;
    file->size = size;
    std::snprintf(file->name, sizeof(file->name), "%s", path.filename().string().c_str());
    return true;
}

// The asset loaders ask for the other game's ROM; Fleet's setup already holds the one the player chose.
bool IsRomGivenByAssetSetup(const char* filterPatterns) {
    return AssetSetup_IsRunning() && !AssetSetup_GetOtherGameRom().empty() && filterPatterns != nullptr &&
           std::strstr(filterPatterns, ".z64") != nullptr;
}

bool PickFor(const ModIdentity* mod, const char* title, const char* filterName, const char* filterPatterns,
             SOHUserFile* file) {
    if (file == nullptr || file->structSize < SOH_USER_FILE_MIN_SIZE || !CanUseFiles(mod)) {
        return false;
    }
    if (IsRomGivenByAssetSetup(filterPatterns)) {
        if (!ReadPickedFile(AssetSetup_GetOtherGameRom(), file)) {
            return false;
        }
        SPDLOG_INFO("[UserFiles] Fleet's asset setup gave '{}' the ROM '{}'", mod->name, file->name);
        return true;
    }
#ifdef USER_FILES_HAS_DIALOGS
    const std::string dialogTitle = mod->name + ": " + (title != nullptr ? title : "Choose a file to give it");
    const std::vector<std::string> filters = { filterName != nullptr ? filterName : "Files",
                                               filterPatterns != nullptr ? filterPatterns : "*", "All files", "*" };
    const std::vector<std::string> chosen = pfd::open_file(dialogTitle, ".", filters).result();
    if (chosen.empty() || !ReadPickedFile(chosen[0], file)) {
        return false;
    }
    SPDLOG_INFO("[UserFiles] The player gave '{}' the file '{}'", mod->name, file->name);
    return true;
#else
    return false;
#endif
}

// The player picking the archive in the dialog is the consent; it stays mounted until the game closes.
bool MountFor(const ModIdentity* mod, const char* title) {
    if (!CanUseFiles(mod)) {
        return false;
    }
#ifdef USER_FILES_HAS_DIALOGS
    const std::string dialogTitle = mod->name + ": " + (title != nullptr ? title : "Choose an archive to load");
    const std::vector<std::string> chosen =
        pfd::open_file(dialogTitle, ".", { "Ship archives", "*.o2r *.otr *.zip" }).result();
    if (chosen.empty()) {
        return false;
    }
    auto archive = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->AddArchive(chosen[0]);
    if (archive == nullptr) {
        SPDLOG_ERROR("[UserFiles] '{}' could not mount '{}'", mod->name, chosen[0]);
        return false;
    }
    SPDLOG_INFO("[UserFiles] The player mounted '{}' for '{}'", chosen[0], mod->name);
    return true;
#else
    return false;
#endif
}

bool SaveFor(const ModIdentity* mod, const char* fileName, const void* data, uint64_t size) {
    if (!CanUseFiles(mod) || data == nullptr || !IsPlainFileName(fileName)) {
        return false;
    }
    if (!ModPermissions_ConfirmNow(*mod, "Save a file",
                                   std::string("wants to save '") + fileName + "' (" + DescribeSize(size) +
                                       ") in your mods folder.")) {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(ModLoader_GetModsDirectory(), error);
    const std::filesystem::path temporary = ModLoader_GetModsDirectory() / ("." + std::string(fileName) + ".writing");
    return WriteBytes(temporary, data, size) && PlaceInModsFolder(temporary, fileName);
}

constexpr uint64_t kMaxModsFolderFileSize = 64ULL * 1024 * 1024;

bool HasListedExtension(const std::filesystem::path& path, const std::string& extensions) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    size_t start = 0;
    while (start <= extensions.size()) {
        size_t end = extensions.find(';', start);
        std::string listed = extensions.substr(start, end == std::string::npos ? std::string::npos : end - start);
        std::transform(listed.begin(), listed.end(), listed.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (!extension.empty() && listed == extension) {
            return true;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

// Read-only and confined to the mods folder, like the archives mounted from it; dot-named entries are the game's.
uint32_t ReadModsFor(const ModIdentity* mod, const char* extensions,
                     bool (*visit)(const char*, const uint8_t*, uint64_t, void*), void* userData) {
    if (extensions == nullptr || visit == nullptr || !CanUseFiles(mod)) {
        return 0;
    }
    const std::filesystem::path mods = ModLoader_GetModsDirectory();
    std::error_code error;
    uint32_t visited = 0;
    auto entry = std::filesystem::recursive_directory_iterator(
        mods, std::filesystem::directory_options::skip_permission_denied, error);
    for (; !error && entry != std::filesystem::recursive_directory_iterator(); entry.increment(error)) {
        const std::filesystem::path path = entry->path();
        if (path.filename().string().rfind('.', 0) == 0) {
            if (entry->is_directory(error)) {
                entry.disable_recursion_pending();
            }
            continue;
        }
        const uint64_t size = entry->is_regular_file(error) ? entry->file_size(error) : 0;
        if (size == 0 || size > kMaxModsFolderFileSize || !HasListedExtension(path, extensions)) {
            continue;
        }
        std::vector<uint8_t> bytes(size);
        std::ifstream input(path, std::ios::binary);
        if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))) {
            continue;
        }
        ++visited;
        const std::string relative = path.lexically_relative(mods).generic_string();
        if (!visit(relative.c_str(), bytes.data(), size, userData)) {
            break;
        }
    }
    return visited;
}

} // namespace

extern "C" bool UserFiles_Pick(const char* title, const char* filterName, const char* filterPatterns,
                               SOHUserFile* file) {
    return PickFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), title, filterName, filterPatterns, file);
}

extern "C" void UserFiles_Free(SOHUserFile* file) {
    if (file == nullptr) {
        return;
    }
    std::free(file->data);
    file->data = nullptr;
    file->size = 0;
}

extern "C" bool UserFiles_SaveToMods(const char* fileName, const void* data, uint64_t size) {
    return SaveFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), fileName, data, size);
}

extern "C" uint32_t UserFiles_ReadMods(const char* extensions,
                                       bool (*visit)(const char*, const uint8_t*, uint64_t, void*), void* userData) {
    return ReadModsFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), extensions, visit, userData);
}

extern "C" bool UserFiles_MountArchive(const char* title) {
    return MountFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), title);
}

// Collected first: renaming while a recursive iterator walks the folder invalidates it.
static std::vector<std::filesystem::path> ListPendingFiles(const std::filesystem::path& modsDirectory) {
    std::vector<std::filesystem::path> pending;
    std::error_code error;
    auto entry = std::filesystem::recursive_directory_iterator(
        modsDirectory, std::filesystem::directory_options::skip_permission_denied, error);
    for (; !error && entry != std::filesystem::recursive_directory_iterator(); entry.increment(error)) {
        if (entry->is_regular_file() && entry->path().extension() == kPendingSuffix) {
            pending.push_back(entry->path());
        }
    }
    return pending;
}

void UserFiles_ApplyPending(const std::filesystem::path& modsDirectory) {
    std::error_code error;
    for (const std::filesystem::path& pending : ListPendingFiles(modsDirectory)) {
        const std::filesystem::path target = pending.parent_path() / pending.stem();
        std::filesystem::remove(target, error);
        std::filesystem::rename(pending, target, error);
        if (error) {
            SPDLOG_ERROR("[UserFiles] Could not install '{}': {}", target.string(), error.message());
            continue;
        }
        SPDLOG_INFO("[UserFiles] Installed '{}'", target.string());
    }
}
