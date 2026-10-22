#include "ModStore.h"
#include "StoreHttp.h"
#include "StoreInternal.h"

#include <spdlog/spdlog.h>

#if defined(_WIN32)
#include <algorithm>
#include <cwctype>
#include <fstream>

#include <nlohmann/json.hpp>
#include <windows.h>
#include <zip.h>

#include "soh/ModApi/ModTrust/ModTrust.h"
#endif

namespace {

#if defined(_WIN32)

constexpr const char* WindowsAsset = "windows_x64";
using Json = nlohmann::json;
namespace fs = std::filesystem;

constexpr size_t MaxReleaseBytes = 1024ull * 1024 * 1024;
constexpr const char* StagedListName = "unbound-update.json";
constexpr const char* UpdateSuffix = ".update";
constexpr const char* OldSuffix = ".old";
constexpr const char* ReleaseExecutable = "soh.exe";

struct StagedFile {
    std::string name;
    std::vector<char> data;
};

fs::path ExecutablePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    buffer.resize(length);
    return fs::path(buffer);
}

bool IsSafeEntryName(const std::string& name) {
    const fs::path path(name);
    if (name.empty() || path.is_absolute() || path.has_root_name() || name.find(':') != std::string::npos) {
        return false;
    }
    return std::none_of(path.begin(), path.end(), [](const fs::path& part) { return part == ".."; });
}

bool IsSameFileName(const fs::path& left, const fs::path& right) {
    auto lowercase = [](std::wstring text) {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
        return text;
    };
    return lowercase(left.filename().wstring()) == lowercase(right.filename().wstring());
}

std::optional<std::vector<StagedFile>> ReadZip(const std::vector<char>& bytes, std::string& error) {
    zip_error_t zipError;
    zip_error_init(&zipError);
    zip_source_t* source = zip_source_buffer_create(bytes.data(), bytes.size(), 0, &zipError);
    zip_t* archive = source != nullptr ? zip_open_from_source(source, ZIP_RDONLY, &zipError) : nullptr;
    if (archive == nullptr) {
        if (source != nullptr) {
            zip_source_free(source);
        }
        error = std::string("the update is not a valid zip: ") + zip_error_strerror(&zipError);
        zip_error_fini(&zipError);
        return std::nullopt;
    }

    std::vector<StagedFile> files;
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_int64_t index = 0; index < count && error.empty(); ++index) {
        zip_stat_t stat;
        if (zip_stat_index(archive, index, 0, &stat) != 0 || stat.name == nullptr) {
            error = "the update zip could not be read";
            break;
        }
        const std::string name = stat.name;
        if (!name.empty() && name.back() == '/') {
            continue;
        }
        if (!IsSafeEntryName(name)) {
            error = "the update zip holds a file outside the game folder: " + name;
            break;
        }
        StagedFile file = { name, std::vector<char>(static_cast<size_t>(stat.size)) };
        zip_file_t* entry = zip_fopen_index(archive, index, 0);
        const bool read =
            entry != nullptr && zip_fread(entry, file.data.data(), stat.size) == static_cast<zip_int64_t>(stat.size);
        if (entry != nullptr) {
            zip_fclose(entry);
        }
        if (!read) {
            error = "could not unpack " + name + " from the update";
            break;
        }
        files.push_back(std::move(file));
    }
    zip_close(archive);
    zip_error_fini(&zipError);
    return error.empty() ? std::optional(std::move(files)) : std::nullopt;
}

bool WriteFile(const fs::path& path, const std::vector<char>& data) {
    std::error_code ignored;
    fs::create_directories(path.parent_path(), ignored);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(output);
}

// Windows lets a running exe be renamed, never overwritten: the old image keeps running from the .old name.
bool ReplaceExecutable(const fs::path& executable, const std::vector<char>& data, std::string& error) {
    const fs::path old = executable.string() + OldSuffix;
    std::error_code ignored;
    fs::remove(old, ignored);
    std::error_code renameError;
    fs::rename(executable, old, renameError);
    if (renameError) {
        error = "could not move the running exe aside: " + renameError.message();
        return false;
    }
    if (!WriteFile(executable, data)) {
        fs::rename(old, executable, ignored);
        error = "could not write the new exe";
        return false;
    }
    return true;
}

bool StageFiles(const std::vector<StagedFile>& files, std::string& error) {
    const fs::path executable = ExecutablePath();
    const fs::path directory = executable.parent_path();
    const StagedFile* newExecutable = nullptr;
    Json staged = Json::array();
    for (const StagedFile& file : files) {
        // The running exe may have been renamed; the release always ships it as soh.exe.
        if (IsSameFileName(file.name, ReleaseExecutable) && !fs::path(file.name).has_parent_path()) {
            newExecutable = &file;
            continue;
        }
        if (!WriteFile(directory / (file.name + UpdateSuffix), file.data)) {
            error = "could not write " + file.name;
            return false;
        }
        staged.push_back(file.name);
    }
    if (newExecutable == nullptr) {
        error = std::string("the update has no ") + ReleaseExecutable;
        return false;
    }

    std::ofstream list(directory / StagedListName, std::ios::trunc);
    list << Json{ { "files", staged } }.dump(2) << '\n';
    return list && ReplaceExecutable(executable, newExecutable->data, error);
}

#endif

} // namespace

bool ModStore_CanInstallRelease([[maybe_unused]] const StoreRelease& release) {
#if defined(_WIN32)
    return release.assets.count(WindowsAsset) != 0;
#else
    return false;
#endif
}

bool ModStore_StageRelease([[maybe_unused]] const StoreRelease& release, std::string& error) {
#if defined(_WIN32)
    const auto asset = release.assets.find(WindowsAsset);
    if (asset == release.assets.end()) {
        error = "this release has no Windows build";
        return false;
    }
    const StoreHttpResult download = ModStore_HttpsGet(asset->second.url, MaxReleaseBytes);
    if (!download.error.empty()) {
        error = download.error;
        return false;
    }
    if (download.body.size() != asset->second.size ||
        ModTrust_HashBytes(download.body.data(), download.body.size()) != asset->second.hash) {
        error = "the download did not match the signed release manifest";
        return false;
    }
    auto files = ReadZip(download.body, error);
    return files && StageFiles(*files, error);
#else
    error = "updating in place is only supported on Windows; download it from the release page";
    return false;
#endif
}

void ModStore_ApplyStagedUpdate() {
#if defined(_WIN32)
    const fs::path executable = ExecutablePath();
    const fs::path directory = executable.parent_path();
    std::error_code error;
    fs::remove(executable.string() + OldSuffix, error);

    const fs::path listPath = directory / StagedListName;
    std::ifstream input(listPath);
    if (!input) {
        return;
    }
    const Json list = Json::parse(input, nullptr, false);
    input.close();
    for (const Json& name : list.is_object() ? list.value("files", Json::array()) : Json::array()) {
        if (!name.is_string() || !IsSafeEntryName(name.get<std::string>())) {
            continue;
        }
        const fs::path target = directory / name.get<std::string>();
        fs::rename(target.string() + UpdateSuffix, target, error);
        if (error) {
            SPDLOG_ERROR("[ModStore] Could not finish updating {}: {}", target.string(), error.message());
        }
    }
    fs::remove(listPath, error);
#endif
}
