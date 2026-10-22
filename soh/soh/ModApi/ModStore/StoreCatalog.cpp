#include "StoreCatalog.h"

#include <algorithm>
#include <cctype>
#include <regex>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace {

using Json = nlohmann::json;

constexpr int CatalogFormat = 1;
constexpr int ReleaseFormat = 1;
const std::regex ModId("^[a-z0-9_-]{1,64}$");
const std::regex Hash("^[0-9a-f]{128}$");

std::string ReadString(const Json& object, const char* key) {
    const auto field = object.find(key);
    return field != object.end() && field->is_string() ? field->get<std::string>() : std::string();
}

std::vector<std::string> ReadStrings(const Json& object, const char* key) {
    std::vector<std::string> values;
    const auto field = object.find(key);
    if (field == object.end() || !field->is_array()) {
        return values;
    }
    for (const Json& value : *field) {
        if (value.is_string()) {
            values.push_back(value.get<std::string>());
        }
    }
    return values;
}

bool IsHttps(const std::string& url) {
    return url.rfind("https://", 0) == 0;
}

std::optional<StoreDownload> ReadDownload(const Json& object) {
    if (!object.is_object()) {
        return std::nullopt;
    }
    StoreDownload download;
    download.file = ReadString(object, "file");
    download.url = ReadString(object, "url");
    download.hash = ReadString(object, "hash");
    download.size = object.value("size", uint64_t(0));
    const bool safeFile = !download.file.empty() && download.file.find_first_of("/\\:") == std::string::npos;
    if (!safeFile || !IsHttps(download.url) || !std::regex_match(download.hash, Hash) || download.size == 0) {
        return std::nullopt;
    }
    return download;
}

std::optional<StoreMod> ReadMod(const std::string& id, const Json& object) {
    if (!std::regex_match(id, ModId) || !object.is_object() || !object.value("listed", true)) {
        return std::nullopt;
    }
    StoreMod mod;
    mod.id = id;
    mod.name = ReadString(object, "name");
    mod.author = ReadString(object, "author");
    mod.description = ReadString(object, "description");
    mod.version = ReadString(object, "version");
    mod.repository = ReadString(object, "repository");
    mod.permissions = ReadStrings(object, "permissions");
    mod.dependencies = ReadStrings(object, "dependencies");
    const auto builds = object.find("builds");
    if (builds == object.end() || !builds->is_object()) {
        return std::nullopt;
    }
    for (const auto& [major, build] : builds->items()) {
        auto download = ReadDownload(build);
        const bool isMajor = !major.empty() && major.size() < 6 &&
                             std::all_of(major.begin(), major.end(), [](unsigned char c) { return std::isdigit(c); });
        if (download && isMajor) {
            mod.builds[std::stoi(major)] = *download;
        }
    }
    if (mod.name.empty()) {
        mod.name = id;
    }
    return mod;
}

} // namespace

std::optional<StoreCatalog> ModStore_ParseCatalog(const std::string& payload) {
    const Json root = Json::parse(payload, nullptr, false);
    if (!root.is_object() || root.value("format", 0) != CatalogFormat) {
        SPDLOG_ERROR("[ModStore] The catalog has an unknown format");
        return std::nullopt;
    }

    StoreCatalog catalog;
    catalog.sequence = root.value("sequence", uint64_t(0));
    const auto mods = root.find("mods");
    if (mods != root.end() && mods->is_object()) {
        for (const auto& [id, object] : mods->items()) {
            auto mod = ReadMod(id, object);
            if (mod) {
                catalog.mods.emplace(id, std::move(*mod));
            }
        }
    }
    for (const Json& object : root.value("packs", Json::array())) {
        if (object.is_object()) {
            catalog.packs.push_back({ ReadString(object, "id"), ReadString(object, "name"),
                                      ReadString(object, "author"), ReadStrings(object, "mods") });
        }
    }
    for (const Json& object : root.value("links", Json::array())) {
        const std::string url = object.is_object() ? ReadString(object, "url") : std::string();
        if (IsHttps(url)) {
            catalog.links.push_back({ ReadString(object, "name"), url, ReadString(object, "description") });
        }
    }
    return catalog;
}

std::optional<StoreRelease> ModStore_ParseRelease(const std::string& payload) {
    const Json root = Json::parse(payload, nullptr, false);
    if (!root.is_object() || root.value("format", 0) != ReleaseFormat) {
        SPDLOG_ERROR("[ModStore] The release manifest has an unknown format");
        return std::nullopt;
    }

    StoreRelease release;
    release.version = ReadString(root, "version");
    release.pageUrl = ReadString(root, "page");
    release.modApiMajor = root.value("modapi", 0);
    const auto assets = root.find("assets");
    if (assets != root.end() && assets->is_object()) {
        for (const auto& [platform, object] : assets->items()) {
            auto download = ReadDownload(object);
            if (download) {
                release.assets.emplace(platform, std::move(*download));
            }
        }
    }
    if (release.version.empty() || release.modApiMajor <= 0 || !IsHttps(release.pageUrl)) {
        return std::nullopt;
    }
    return release;
}

int ModStore_CompareVersions(const std::string& left, const std::string& right) {
    const std::regex number("[0-9]+");
    auto leftNumber = std::sregex_iterator(left.begin(), left.end(), number);
    auto rightNumber = std::sregex_iterator(right.begin(), right.end(), number);
    const std::sregex_iterator end;
    for (; leftNumber != end && rightNumber != end; ++leftNumber, ++rightNumber) {
        const unsigned long leftValue = std::stoul(leftNumber->str());
        const unsigned long rightValue = std::stoul(rightNumber->str());
        if (leftValue != rightValue) {
            return leftValue < rightValue ? -1 : 1;
        }
    }
    return (leftNumber != end) - (rightNumber != end);
}
