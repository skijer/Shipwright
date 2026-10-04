#pragma once
#include <stdint.h>

#ifdef __cplusplus
#include <memory>
#include <ship/resource/ResourceFactoryBinary.h>
namespace Ship {
class ArchiveManager;
}
namespace Prelude {
// Keep the underlying libultraship loader unchanged; decorate only known exports.
class NativeMaterialDisplayListFactory final : public Ship::ResourceFactoryBinary {
  public:
    explicit NativeMaterialDisplayListFactory(std::shared_ptr<Ship::ArchiveManager> archives);
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;

  private:
    std::shared_ptr<Ship::ArchiveManager> mArchives;
};
} // namespace Prelude
extern "C" {
#endif
struct GraphicsContext;
// Call after reopening archives or before explicitly refreshing resources.
// Clears only metadata; cached display lists and scroll buffers remain valid.
void PreludeNativeMaterialScroll_InvalidateMetadata(const char* reason);
void PreludeNativeMaterialScroll_Update(struct GraphicsContext* gfxCtx, uint32_t stateFrames, uint32_t gameplayFrames);
#ifdef __cplusplus
}
#endif
