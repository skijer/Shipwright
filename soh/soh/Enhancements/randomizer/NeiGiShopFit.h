#pragma once

namespace NeiGi {
// Applied before the GI draw scale, only after a complete replacement resolves.
// EnGirlA's .25 actor scale and 24-local-unit origin put its counter at Y=-24.
struct ShopFit {
    float scale = 1.f;
    float lift = 0.f;
};

// Accepted shelf poses retained unchanged.
inline constexpr ShopFit kRodShopFit{ .85f, 14.f };
inline constexpr ShopFit kBallAndChainShopFit{ .62f, 9.f };
inline constexpr ShopFit kShovelShopFit{ .82f, 13.f };

// Bounds include both opaque and translucent serialized vertices.
inline constexpr ShopFit kSpellShopFit{ 1.f, 16.f };
inline constexpr ShopFit kTimeGateShopFit{ .60f, 8.f };
inline constexpr ShopFit kSwitchHookShopFit{ .70f, 8.f };
inline constexpr ShopFit kFeatherShopFit{ 1.f, 16.f };
inline constexpr ShopFit kSomariaShopFit{ 1.f, 2.f };
inline constexpr ShopFit kRocsCapeShopFit{ 1.f, 4.f };
} // namespace NeiGi
