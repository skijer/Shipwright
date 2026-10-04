#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace Prelude {
enum class NativeMaterialProfile {
    None,
    LakeHylia,
    Pool,
    LostWoodsLightSheet,
    FountainLowerA32,
    FountainLowerB32,
    FountainCentral32,
    FountainLowerA64,
    FountainLowerB64,
    FountainCentral64,
    WaterTempleCaustics,
    ZorasDomainCaustics,
    ChamberOfSagesPlatform,
    Count
};
struct NativeMaterialCommand {
    uintptr_t w0;
    uintptr_t w1;
};
struct ScrollParameters {
    uint32_t x1, y1, x2, y2;
    int width, height, dx1, dy1, dx2, dy2;
};
NativeMaterialProfile ResolveNativeMaterial(const nlohmann::json& item, bool pasted);
std::optional<size_t> FindNativeScrollInsertion(const std::vector<NativeMaterialCommand>& commands,
                                                NativeMaterialProfile profile = NativeMaterialProfile::None);
ScrollParameters NativeScrollParameters(NativeMaterialProfile profile, uint32_t stateFrames, uint32_t gameplayFrames);
} // namespace Prelude
