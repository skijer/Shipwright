#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <fast/resource/type/Texture.h>

namespace Ship {
class ResourceManager;
}

// Owns immutable texture snapshots and their command-facing paths. The strict
// display-list cache owns this store for the same lifetime as its patched lists.
class MmStrictTextureBindings {
  public:
    const char* Bind(Ship::ResourceManager& manager, const std::string& canonical,
                     const std::shared_ptr<Ship::IResource>& privateResource);
    void EnsurePublished(Ship::ResourceManager& manager) const;
    static std::shared_ptr<Fast::Texture> Snapshot(const std::shared_ptr<Ship::IResource>& resource,
                                                   const std::string& canonical);

  private:
    struct Binding {
        std::string path;
        std::string altPath;
        std::shared_ptr<Fast::Texture> base;
        std::shared_ptr<Fast::Texture> alt;
    };
    std::unordered_map<std::string, std::unique_ptr<Binding>> bindings;
};
