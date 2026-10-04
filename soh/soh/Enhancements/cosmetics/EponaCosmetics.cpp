#include "EponaCosmetics.h"
#include "EponaCosmeticMasks.h"
#include "EponaCosmeticsDL.h"
#include "EponaCosmeticsNativeTemplates.h"

#include "global.h"
#include "soh/cvar_prefixes.h"
#include "soh/frame_interpolation.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <fast/resource/type/DisplayList.h>
#include <ship/resource/ResourceManager.h>
#ifdef COMBO_BUILD
#include <ship/resource/CrossRMRegistry.h>
#else
#include <ship/Context.h>
#endif

#include <array>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace EponaCosmetics;

constexpr const char* kColorCVars[2][3] = {
    { CVAR_COSMETIC("NPC.Epona.Coat.Value"), CVAR_COSMETIC("NPC.Epona.WhiteHair.Value"),
      CVAR_COSMETIC("NPC.Epona.Eyes.Value") },
    { CVAR_COSMETIC("NPC.YoungEpona.Coat.Value"), CVAR_COSMETIC("NPC.YoungEpona.WhiteHair.Value"),
      CVAR_COSMETIC("NPC.YoungEpona.Eyes.Value") },
};
constexpr const char* kChangedCVars[2][3] = {
    { CVAR_COSMETIC("NPC.Epona.Coat.Changed"), CVAR_COSMETIC("NPC.Epona.WhiteHair.Changed"),
      CVAR_COSMETIC("NPC.Epona.Eyes.Changed") },
    { CVAR_COSMETIC("NPC.YoungEpona.Coat.Changed"), CVAR_COSMETIC("NPC.YoungEpona.WhiteHair.Changed"),
      CVAR_COSMETIC("NPC.YoungEpona.Eyes.Changed") },
};
constexpr Color_RGBA8 kDefaults[3] = { { 170, 58, 2, 255 }, { 255, 255, 255, 255 }, { 40, 24, 16, 255 } };
constexpr const char* kEyeTextures[2][3] = {
    { "objects/object_horse/gEponaEyeOpenTex", "objects/object_horse/gEponaEyeHalfTex",
      "objects/object_horse/gEponaEyeClosedTex" },
    { "objects/object_horse_link_child/gChildEponaEyeOpenTex", "objects/object_horse_link_child/gChildEponaEyeHalfTex",
      "objects/object_horse_link_child/gChildEponaEyeCloseTex" },
};
constexpr const char* kTPBody = "__OTR__objects/object_horse/tp_poc4_cosmetics/BodyDL";
constexpr const char* kTPOverlays[2] = { "__OTR__objects/object_horse/tp_poc4_cosmetics/CoatOverlayDL",
                                         "__OTR__objects/object_horse/tp_poc4_cosmetics/HairOverlayDL" };

struct CachedMasks {
    std::string path;
    std::array<std::vector<uint8_t>, 3> masks;
};
struct CachedList {
    std::shared_ptr<Fast::DisplayList> source;
    std::array<std::vector<Gfx>, 8> variants;
    std::array<bool, 8> built{};
};
struct LimbSwap {
    SkinLimb* limb;
    SkinAnimatedLimbData* animated;
    void* original;
};

std::map<std::string, CachedMasks> sMasks;
// Retain prior sources: submitted/interpolated frames may still refer to their immutable copies.
std::map<std::pair<std::string, const Fast::DisplayList*>, CachedList> sLists;
std::vector<LimbSwap> sSwaps;
bool sActive = false;

std::shared_ptr<Ship::ResourceManager> OwnResourceManager() {
#ifdef COMBO_BUILD
    return Ship::CrossRMRegistry::GetOrActive("oot");
#else
    return Ship::Context::GetRawInstance()->GetResourceManager();
#endif
}

MaskTarget MasksFor(const std::string& name, bool eye) {
    auto [it, inserted] = sMasks.try_emplace(name);
    auto& cached = it->second;
    if (inserted) {
        cached.path = "__OTR__" + name;
        for (unsigned part = 0; part < 3; ++part) {
            cached.masks[part] = EponaMasks::BuildInverseMask(name, static_cast<EponaMasks::Part>(part));
            if (eye && part == 2 && cached.masks[part].empty()) {
                // Closed eyes have no iris. Explicitly hide the entire overlay during that blink.
                cached.masks[part].assign(32 * 16, 1);
            }
        }
    }
    MaskTarget result{ cached.path.c_str(), {} };
    for (unsigned part = 0; part < 3; ++part) {
        result.inverseMasks[part] = cached.masks[part].empty() ? nullptr : cached.masks[part].data();
    }
    return result;
}

bool NativePath(std::string_view path, bool young) {
    constexpr std::string_view prefixes[] = { "__OTR__objects/object_horse/gEpona",
                                              "__OTR__objects/object_horse_link_child/gChildEponaSkelLimbsLimb_" };
    return path.starts_with(prefixes[young]) && path.find("DL_") != std::string_view::npos;
}

Gfx* NativeList(const std::shared_ptr<Ship::ResourceManager>& rm, const char* path, bool young, unsigned changed) {
    std::string resolved = path + 7; // NativePath has already checked the OTR signature.
    if (rm->IsAltAssetsEnabled() && (rm->GetArchiveManager()->HasFile("alt/" + resolved) ||
                                     rm->GetArchiveManager()->HasFile("alt/" + resolved + ".meta"))) {
        resolved = "alt/" + resolved;
    }
    auto resource = rm->GetCachedResource(resolved, true);
    if (!resource) {
        resource = rm->LoadResource(resolved, true);
    }
    auto source = std::dynamic_pointer_cast<Fast::DisplayList>(resource);
    if (!source || source->GetInitData()->IsCustom) {
        return nullptr;
    }
    auto& cache = sLists[{ path, source.get() }];
    if (!cache.source) {
        cache.source = source;
        if (!NativeTemplateMatches(path, source->Instructions, false)) {
            cache.built.fill(true);
            return nullptr;
        }
    }
    if (!cache.built[changed]) {
        cache.built[changed] = true;
        cache.variants[changed] = BuildNativeDisplayList(source->Instructions, changed, [&](uint64_t hash, bool eye) {
            TextureMaterial result;
            if (eye) {
                for (const auto* name : kEyeTextures[young]) {
                    result.targets.push_back(MasksFor(name, true));
                }
            } else if (const auto* name = rm->GetArchiveManager()->HashToString(hash)) {
                result.palette = name->find("TLUT") != std::string::npos;
                if (!result.palette) {
                    result.targets.push_back(MasksFor(*name, false));
                }
            }
            return result;
        });
    }
    auto& result = cache.variants[changed];
    return result.empty() ? nullptr : result.data();
}

std::vector<LimbSwap> DrawLimbs(Skin* skin) {
    std::vector<LimbSwap> result;
    if (!skin || !skin->skeletonHeader || !skin->skeletonHeader->segment) {
        return result;
    }
    auto limbs = reinterpret_cast<SkinLimb**>(SEGMENTED_TO_VIRTUAL(skin->skeletonHeader->segment));
    for (unsigned i = 0; i < skin->skeletonHeader->limbCount; ++i) {
        auto* limb = static_cast<SkinLimb*>(SEGMENTED_TO_VIRTUAL(limbs[i]));
        if (!limb || !limb->segment) {
            continue;
        }
        if (limb->segmentType == SKIN_LIMB_TYPE_NORMAL) {
            result.push_back({ limb, nullptr, limb->segment });
        } else if (limb->segmentType == SKIN_LIMB_TYPE_ANIMATED) {
            auto* data = static_cast<SkinAnimatedLimbData*>(SEGMENTED_TO_VIRTUAL(limb->segment));
            result.push_back({ limb, data, data->dlist });
        }
    }
    return result;
}

} // namespace

// OPEN_DISPS/CLOSE_DISPS redeclare C functions; keep their caller out of the anonymous namespace.
static void BindColors(PlayState* play, bool young, unsigned changed, bool tp) {
    // Colors are the only rainbow data built each frame. Masks, texture data and limb lists are immutable.
    auto* lists = static_cast<Gfx*>(Graph_Alloc(play->state.gfxCtx, sizeof(Gfx) * 32));
    OPEN_DISPS(play->state.gfxCtx);
    for (unsigned part = 0; part < 4; ++part) {
        Gfx* start = lists;
        gDPPipeSync(lists++);
        if (part < 3 && (changed & (1 << part))) {
            auto color = CVarGetColor(kColorCVars[young][part], kDefaults[part]);
            gDPSetGrayscaleColor(lists++, color.r, color.g, color.b, 255);
            gSPGrayscale(lists++, true);
        } else {
            gSPGrayscale(lists++, false);
        }
        gSPEndDisplayList(lists++);
        gSPSegment(POLY_OPA_DISP++, 9 + part, reinterpret_cast<uintptr_t>(start));
    }
    if (tp) {
        for (unsigned part = 0; part < 2; ++part) {
            Gfx* start = lists;
            if (changed & (1 << part)) {
                gSPDisplayList(lists++, reinterpret_cast<Gfx*>(((9 + part) << 24) | 1));
                gSPDisplayList(lists++, reinterpret_cast<Gfx*>(const_cast<char*>(kTPOverlays[part])));
                gSPDisplayList(lists++, reinterpret_cast<Gfx*>(0x0C000001));
            }
            gSPEndDisplayList(lists++);
            gSPSegment(POLY_OPA_DISP++, 13 + part, reinterpret_cast<uintptr_t>(start));
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void EponaCosmetics_BeginDraw(PlayState* play, Skin* skin, s32 young) {
    // Each actor pairs this call with End, including unsupported/default paths.
    if (!play || sActive) {
        return;
    }
    young = young != 0;
    unsigned changed = 0;
    for (unsigned part = 0; part < 3; ++part) {
        changed |= (CVarGetInteger(kChangedCVars[young][part], 0) != 0) << part;
    }
    const auto rm = OwnResourceManager();
    if (!rm) {
        return;
    }
    auto limbs = DrawLimbs(skin);
    bool tp = false;
    for (const auto& limb : limbs) {
        const auto* path = static_cast<const char*>(limb.original);
        if (!young && reinterpret_cast<uintptr_t>(path) > 0x0FFFFFFF && rm->OtrSignatureCheck(path) &&
            std::string_view(path) == kTPBody) {
            tp = true;
            break;
        }
    }
    if (!changed && !tp) {
        return; // Original/default native draw: no display-list allocations, patches, or render-state changes.
    }
    if (!tp) {
        for (const auto& limb : limbs) {
            const auto* path = static_cast<const char*>(limb.original);
            if (reinterpret_cast<uintptr_t>(path) <= 0x0FFFFFFF || !rm->OtrSignatureCheck(path) ||
                !NativePath(path, young)) {
                continue;
            }
            if (auto* replacement = NativeList(rm, path, young, changed)) {
                sSwaps.push_back(limb);
                if (limb.animated) {
                    limb.animated->dlist = replacement;
                } else {
                    limb.limb->segment = replacement;
                }
            }
        }
        if (sSwaps.empty()) {
            return;
        }
    }
    BindColors(play, young, changed, tp);
    sActive = true;
}

extern "C" void EponaCosmetics_EndDraw(PlayState* play) {
    for (const auto& swap : sSwaps) {
        if (swap.animated) {
            swap.animated->dlist = static_cast<Gfx*>(swap.original);
        } else {
            swap.limb->segment = swap.original;
        }
    }
    sSwaps.clear();
    if (sActive && play) {
        OPEN_DISPS(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gSPGrayscale(POLY_OPA_DISP++, false);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    sActive = false;
}
