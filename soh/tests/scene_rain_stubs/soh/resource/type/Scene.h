#pragma once
#include <ship/resource/archive/Archive.h>
namespace SOH {
class Scene {
  public:
    std::shared_ptr<Ship::ResourceInitData> data = std::make_shared<Ship::ResourceInitData>();
    std::shared_ptr<Ship::ResourceInitData> GetInitData() {
        return data;
    }
};
} // namespace SOH
