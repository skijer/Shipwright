#pragma once

#include <stdint.h>
#include <vector>
#include <ship/resource/Resource.h>
#include "SceneCommand.h"
// #include <libultraship/libultra/types.h>
#include "soh/resource/type/Path.h"

namespace SOH {

class SetPathways : public SceneCommand<PathData*> {
  public:
    using SceneCommand::SceneCommand;

    PathData** GetPointer();
    size_t GetPointerSize();

    void AddPathResource(const std::shared_ptr<Path>& resource, const std::string& fileName);
    PathData* GetPathList();

    uint32_t numPaths;
    std::vector<std::string> pathFileNames;
    std::vector<PathData*> paths;
    std::vector<PathData> pathList;
    std::vector<std::shared_ptr<Ship::IResource>> pathResources;
};
}; // namespace SOH
