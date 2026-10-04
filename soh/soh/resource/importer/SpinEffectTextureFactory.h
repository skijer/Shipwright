#pragma once

#include "ship/resource/ResourceFactoryBinary.h"

namespace SOH {

// Keep exact allowlisted I4/I8 effect replacements in their native UV domain.
// Historical factory names remain stable for existing resource registration.
// Geometry and display-list selection remain entirely asset controlled.
class SpinEffectTextureFactoryV0 final : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class SpinEffectTextureFactoryV1 final : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

} // namespace SOH
