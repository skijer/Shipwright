#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct StoreDownload {
    std::string file;
    std::string url;
    std::string hash;
    uint64_t size = 0;
};

struct StoreMod {
    std::string id;
    std::string name;
    std::string author;
    std::string description;
    std::string version;
    std::string repository;
    std::vector<std::string> permissions;
    std::vector<std::string> dependencies;
    std::map<int, StoreDownload> builds;
};

struct StorePack {
    std::string id;
    std::string name;
    std::string author;
    std::vector<std::string> mods;
};

struct StoreLink {
    std::string name;
    std::string url;
    std::string description;
};

struct StoreCatalog {
    uint64_t sequence = 0;
    std::map<std::string, StoreMod> mods;
    std::vector<StorePack> packs;
    std::vector<StoreLink> links;
};

struct StoreRelease {
    std::string version;
    std::string pageUrl;
    int modApiMajor = 0;
    std::map<std::string, StoreDownload> assets;
};

std::optional<StoreCatalog> ModStore_ParseCatalog(const std::string& payload);
std::optional<StoreRelease> ModStore_ParseRelease(const std::string& payload);
// Compares the digit groups of two fork versions ("9.3.0-unbound0.12"): negative, zero or positive.
int ModStore_CompareVersions(const std::string& left, const std::string& right);
