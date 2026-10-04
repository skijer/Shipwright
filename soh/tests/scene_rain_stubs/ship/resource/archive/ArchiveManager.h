#pragma once
#include "Archive.h"
namespace Ship {
class ArchiveManager {
  public:
    std::map<std::string, std::shared_ptr<Archive>> owners;
    std::string lastLookup;
    std::shared_ptr<Archive> GetArchiveFromFile(const std::string& path) {
        lastLookup = path;
        return owners.count(path) ? owners[path] : nullptr;
    }
};
} // namespace Ship
