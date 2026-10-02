#include "O2rExtractor.h"

#include <atomic>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include <spdlog/spdlog.h>
#include <libultraship/libultraship.h>

#include "soh/ModApi/ModPermissions/ModPermissions.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
}

extern "C" int zapd_report(int argc, char** argv, std::atomic<size_t>* extractCount, std::atomic<size_t>* totalExtract);

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

bool IsValidRequest(const SOHO2rExtractRequest* request) {
    return request != nullptr && request->structSize >= SOH_O2R_EXTRACT_REQUEST_MIN_SIZE &&
           request->romPath != nullptr && request->assetsDir != nullptr && request->xmlDir != nullptr &&
           request->configPath != nullptr && request->filelistDir != nullptr && request->outputPath != nullptr;
}

int RunZapdWithoutThrowing(int argc, const char** argv) {
    try {
        return zapd_report(argc, const_cast<char**>(argv), nullptr, nullptr);
    } catch (const std::exception& exception) {
        SPDLOG_ERROR("[O2rExtractor] ZAPD threw: {}", exception.what());
    } catch (...) { SPDLOG_ERROR("[O2rExtractor] ZAPD threw"); }
    return -1;
}

bool CanModWriteFiles(const ModIdentity* mod, const std::string& message) {
    if (mod == nullptr || !ModPermissions_HasDeclared(*mod, ModPermission::Files)) {
        return false;
    }
    return ModPermissions_ConfirmNow(*mod, "Write files", message);
}

} // namespace

uint32_t O2rExtractor_ExportMatching(const std::string& searchMask, const std::filesystem::path& destination) {
    auto archiveManager = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();
    auto paths = archiveManager->ListFiles(searchMask);
    uint32_t written = 0;

    for (const std::string& path : *paths) {
        if (!IsInsideGameDirectories(destination / path)) {
            continue;
        }
        auto file = archiveManager->LoadFile(path);
        if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
            SPDLOG_WARN("[O2rExtractor] Could not read '{}' from the mounted archives", path);
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

bool O2rExtractor_RunZapd(const O2rZapdJob& job) {
    const std::filesystem::path romPath = std::filesystem::absolute(job.romPath);
    const std::filesystem::path outputPath = std::filesystem::absolute(job.outputPath);
    const std::filesystem::path assetsDir = std::filesystem::absolute(job.assetsDir);
    if (!IsInsideGameDirectories(outputPath) || !IsInsideGameDirectories(assetsDir)) {
        return false;
    }
    if (!std::filesystem::exists(romPath) || !std::filesystem::is_directory(assetsDir)) {
        SPDLOG_ERROR("[O2rExtractor] '{}' or '{}' is missing", romPath.string(), assetsDir.string());
        return false;
    }

    std::error_code error;
    const std::filesystem::path previousDir = std::filesystem::current_path(error);
    std::filesystem::current_path(assetsDir, error);
    if (error) {
        SPDLOG_ERROR("[O2rExtractor] Could not enter '{}'", assetsDir.string());
        return false;
    }

    const std::string romArgument = romPath.string();
    const std::string archiveName = outputPath.filename().string();
    const std::string portVersion = std::to_string(gBuildVersionMajor) + "." + std::to_string(gBuildVersionMinor) +
                                    "." + std::to_string(gBuildVersionPatch);
    const char* argv[] = {
        "ZAPD",      "ed",
        "-i",        job.xmlDir.c_str(),
        "-b",        romArgument.c_str(),
        "-fl",       job.filelistDir.c_str(),
        "-gsf",      "0",
        "-rconf",    job.configPath.c_str(),
        "-se",       "OTR",
        "--otrfile", archiveName.c_str(),
        "--portVer", portVersion.c_str(),
        "-o",        "placeholder",
        "-osf",      "placeholder",
    };
    const int result = RunZapdWithoutThrowing(static_cast<int>(std::size(argv)), argv);

    bool extracted = result == 0 && std::filesystem::exists(archiveName);
    if (extracted) {
        std::filesystem::create_directories(outputPath.parent_path(), error);
        std::filesystem::rename(assetsDir / archiveName, outputPath, error);
        if (error) {
            std::filesystem::copy_file(assetsDir / archiveName, outputPath,
                                       std::filesystem::copy_options::overwrite_existing, error);
            extracted = !error;
        }
    }
    if (!extracted) {
        SPDLOG_ERROR("[O2rExtractor] ZAPD returned {} and left no archive at '{}'", result, outputPath.string());
    }

    std::filesystem::current_path(previousDir, error);
    return extracted;
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

bool O2rExtractor_Run(const SOHO2rExtractRequest* request) {
    const ModIdentity* mod = ModPermissions_FindCaller(MOD_CALLER_ADDRESS());
    if (!IsValidRequest(request)) {
        SPDLOG_ERROR("[O2rExtractor] Invalid extraction request");
        return false;
    }
    if (!CanModWriteFiles(mod, std::string("wants to build '") + request->outputPath + "' out of the ROM '" +
                                   request->romPath + "'.")) {
        return false;
    }
    return O2rExtractor_RunZapd({ request->romPath, request->assetsDir, request->xmlDir, request->configPath,
                                  request->filelistDir, request->outputPath });
}
