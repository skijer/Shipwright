#include "O2rExtractor.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <spdlog/spdlog.h>
#include <libultraship/libultraship.h>

#include "soh/ModApi/ModPermissions/ModPermissions.h"
#include "soh/ResourceManagerHelpers.h"

namespace {

// Mods reach these writers through the API table, which no binary scan can see, so the host confines them.
bool IsInsideGameDirectories(const std::filesystem::path& path) {
    std::error_code error;
    const std::filesystem::path target = std::filesystem::weakly_canonical(std::filesystem::absolute(path), error);
    if (error) {
        return false;
    }
    for (const std::string& root : { Ship::Context::GetAppBundlePath(), Ship::Context::GetAppDirectoryPath("soh") }) {
        const std::filesystem::path base = std::filesystem::weakly_canonical(root, error);
        if (error || base.empty()) {
            continue;
        }
        const std::filesystem::path relative = target.lexically_relative(base);
        if (!relative.empty() && *relative.begin() != "..") {
            return true;
        }
    }
    SPDLOG_ERROR("[O2rExtractor] Refusing to write outside the game folders: '{}'", path.string());
    return false;
}

bool WriteFile(const std::filesystem::path& path, const std::vector<char>& data) {
    std::error_code error;

    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    return output.good();
}

bool CanModWriteFiles(const ModIdentity* mod, const std::string& message) {
    if (mod == nullptr || !ModPermissions_HasDeclared(*mod, ModPermission::Files)) {
        return false;
    }
    return ModPermissions_ConfirmNow(*mod, "Write files", message);
}

std::shared_ptr<Ship::File> LoadArchiveFile(const std::string& path) {
    auto file = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->LoadFile(path);
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        SPDLOG_WARN("[O2rExtractor] Could not read '{}' from the mounted archives", path);
        return nullptr;
    }
    return file;
}

} // namespace

uint32_t O2rExtractor_ExportMatching(const std::string& searchMask, const std::filesystem::path& destination) {
    auto paths = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->ListFiles(searchMask);
    uint32_t written = 0;

    for (const std::string& path : *paths) {
        if (!IsInsideGameDirectories(destination / path)) {
            continue;
        }
        auto file = LoadArchiveFile(path);
        if (file == nullptr) {
            continue;
        }
        if (!WriteFile(destination / path, *file->Buffer)) {
            SPDLOG_ERROR("[O2rExtractor] Could not write '{}' under '{}'", path, destination.string());
            continue;
        }
        written++;
    }
    return written;
}

uint32_t O2rExtractor_ExportFiles(const char* searchMask, const char* destinationDir) {
    const ModIdentity* mod = ModPermissions_FindCaller(MOD_CALLER_ADDRESS());
    if (searchMask == nullptr || destinationDir == nullptr) {
        return 0;
    }
    if (!CanModWriteFiles(mod, std::string("wants to copy files out of its archive into '") + destinationDir + "'.")) {
        return 0;
    }
    return O2rExtractor_ExportMatching(searchMask, destinationDir);
}

// Read-only: the mounted archives are already open to every mod through the resource manager.
uint32_t O2rExtractor_ReadFiles(const char* searchMask, SOHArchiveFileFunc visit, void* userData) {
    if (searchMask == nullptr || visit == nullptr) {
        return 0;
    }
    auto paths = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->ListFiles(searchMask);
    uint32_t visited = 0;

    for (const std::string& path : *paths) {
        auto file = LoadArchiveFile(path);
        if (file == nullptr) {
            continue;
        }
        visited++;
        const auto* data = reinterpret_cast<const uint8_t*>(file->Buffer->data());
        if (!visit(path.c_str(), data, file->Buffer->size(), userData)) {
            break;
        }
    }
    return visited;
}

// The game's own copy of each matching file: the top Ocarina of Time archive that holds it (oot.o2r, oot-mq.o2r,
// Unbound's conversion), as if no mod covered it. Paths only mods hold are skipped. Read-only, like ReadFiles.
uint32_t O2rExtractor_ReadGameFiles(const char* searchMask, SOHArchiveFileFunc visit, void* userData) {
    if (searchMask == nullptr || visit == nullptr) {
        return 0;
    }
    auto archiveManager = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    auto archives = archiveManager->GetArchives();
    std::vector<std::shared_ptr<Ship::Archive>> game;
    for (auto archive = archives->rbegin(); archive != archives->rend(); ++archive) {
        if ((*archive)->HasGameVersion() && ResourceMgr_IsOcarinaOfTimeVersion((*archive)->GetGameVersion())) {
            game.push_back(*archive);
        }
    }
    auto paths = archiveManager->ListFiles(searchMask);
    uint32_t visited = 0;

    for (const std::string& path : *paths) {
        for (const auto& archive : game) {
            if (!archive->HasFile(path)) {
                continue;
            }
            auto file = archive->LoadFile(path);
            if (file == nullptr || file->Buffer == nullptr) {
                SPDLOG_WARN("[O2rExtractor] Could not read '{}' from '{}'", path, archive->GetPath());
                break;
            }
            visited++;
            const auto* data = reinterpret_cast<const uint8_t*>(file->Buffer->data());
            if (!visit(path.c_str(), data, file->Buffer->size(), userData)) {
                return visited;
            }
            break;
        }
    }
    return visited;
}

uint32_t O2rExtractor_ListFiles(const char* searchMask, SOHArchivePathFunc visit, void* userData) {
    if (searchMask == nullptr || visit == nullptr) {
        return 0;
    }
    auto paths = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->ListFiles(searchMask);
    if (paths == nullptr) {
        return 0;
    }
    uint32_t visited = 0;

    for (const std::string& path : *paths) {
        visited++;
        if (!visit(path.c_str(), userData)) {
            break;
        }
    }
    return visited;
}
