#include "NeiGiPresentation.h"
#include "NeiGiEffectPolicy.h"
#include "NeiGiEnergyTexture.h"
#include "NeiGiRender.h"
#include "NeiGiShopFit.h"
#include <algorithm>
#include <cstring>
#include "draw.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/frame_interpolation.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
}

namespace {
using NeiGi::Kind;
struct Presentation {
    CustomDrawFunc draw;
    const char* opaque;
    const char* translucent;
    float scale;
    Kind effect;
    Vec3f effectCenter;
    NeiGi::ShopFit shop;
};

#define GI_PATH(slug) "__OTR__objects/nei_gi_redesign/" slug "/gi_dl"
#define GI_XLU(slug) "__OTR__objects/nei_gi_redesign/" slug "/gi_xlu_dl"
// These bindings are for presentation only. Actor and held-item resources retain their own paths.
const Presentation kPresentations[] = {
    { Randomizer_DrawRocsFeatherSkijer,
      GI_PATH("rocs_feather"),
      nullptr,
      .5f,
      Kind::Neutral,
      {},
      NeiGi::kFeatherShopFit },
    { Randomizer_DrawRocsFeather, GI_PATH("rocs_feather"), nullptr, .5f, Kind::Neutral, {}, NeiGi::kFeatherShopFit },
    { Randomizer_DrawWhip, GI_PATH("whip"), nullptr, .5f, Kind::Neutral, {} },
    { Randomizer_DrawFireRod,
      GI_PATH("fire_rod"),
      nullptr,
      .2f,
      Kind::Fire,
      { 9.883f, 30.415f, 0 },
      NeiGi::kRodShopFit },
    { Randomizer_DrawIceRod, GI_PATH("ice_rod"), nullptr, .2f, Kind::Ice, { 10.365f, 31.899f, 0 }, NeiGi::kRodShopFit },
    { Randomizer_DrawLightRod,
      GI_PATH("light_rod"),
      nullptr,
      .2f,
      Kind::Light,
      { 9.883f, 30.415f, 0 },
      NeiGi::kRodShopFit },
    { Randomizer_DrawDekuLeaf, GI_PATH("deku_leaf"), nullptr, .5f, Kind::Leaf, {} },
    { Randomizer_DrawSwitchHook, GI_PATH("switch_hook"), nullptr, .01f, Kind::Neutral, {}, NeiGi::kSwitchHookShopFit },
    { Randomizer_DrawMogmaMitts, GI_PATH("mogma_mitts"), nullptr, .5f, Kind::Neutral, {} },
    { Randomizer_DrawGustJar, GI_PATH("gust_jar"), nullptr, 5.f, Kind::Neutral, {} },
    { Randomizer_DrawBallAndChain,
      GI_PATH("ball_and_chain"),
      nullptr,
      .25f,
      Kind::Neutral,
      {},
      NeiGi::kBallAndChainShopFit },
    { Randomizer_DrawTimeGate, GI_PATH("time_gate"), nullptr, .5f, Kind::Neutral, {}, NeiGi::kTimeGateShopFit },
    { Randomizer_DrawBeetle, GI_PATH("beetle"), nullptr, .3f, Kind::Neutral, {} },
    { Randomizer_DrawShovel, GI_PATH("shovel"), nullptr, .2f, Kind::Neutral, {}, NeiGi::kShovelShopFit },
    { Randomizer_DrawHyliaGrace,
      GI_PATH("hylia_grace"),
      GI_XLU("hylia_grace"),
      1.f,
      Kind::Hylia,
      {},
      NeiGi::kSpellShopFit },
    { Randomizer_DrawZonaiPermafrost,
      GI_PATH("zonai_permafrost"),
      GI_XLU("zonai_permafrost"),
      1.f,
      Kind::Zonai,
      {},
      NeiGi::kSpellShopFit },
    { Randomizer_DrawDemiseDestruction,
      GI_PATH("demise_destruction"),
      GI_XLU("demise_destruction"),
      1.f,
      Kind::Demise,
      {},
      NeiGi::kSpellShopFit },
    { Randomizer_DrawRocsCape, GI_PATH("rocs_cape"), nullptr, .6f, Kind::Neutral, {}, NeiGi::kRocsCapeShopFit },
    { Randomizer_DrawSpinner, GI_PATH("spinner"), nullptr, .3f, Kind::Neutral, {} },
    { Randomizer_DrawBombArrows, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawCaneOfSomaria,
      GI_PATH("cane_of_somaria"),
      nullptr,
      .25f,
      Kind::Neutral,
      {},
      NeiGi::kSomariaShopFit },
    { Randomizer_DrawCaneSomariaUpgrade,
      GI_PATH("cane_of_somaria"),
      nullptr,
      .25f,
      Kind::Neutral,
      {},
      NeiGi::kSomariaShopFit },
    { Randomizer_DrawMinishCap, GI_PATH("minish_cap"), nullptr, .5f, Kind::Neutral, {} },
    { Randomizer_DrawDominionRod, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawMagnesis, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawStasis, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawLantern, GI_PATH("lantern"), GI_XLU("lantern"), .025f, Kind::Neutral, {} },
    { Randomizer_DrawCryonis, nullptr, nullptr, 1.f, Kind::Neutral, {} },
};
#undef GI_PATH
#undef GI_XLU

float Spin(PlayState* play) {
    // Match DrawCustomItemDiamond's signed 16-bit rotation, including wrap.
    const uint32_t bits = (static_cast<uint32_t>(play->gameplayFrames) * 2u) & 0xFFFFu;
    const int32_t signedBits = bits >= 0x8000u ? static_cast<int32_t>(bits) - 0x10000 : bits;
    return signedBits * .01f;
}

bool HasResource(const char* path) {
    return path != nullptr &&
           (ResourceMgr_FileExists(path) || (ResourceMgr_IsAltAssetsEnabled() && ResourceMgr_FileAltExists(path)));
}
} // namespace

// OPEN_DISPS declares interpolation callbacks with the enclosing C linkage.
extern "C" {
NeiGi::Basis NeiGi_CameraBasis(PlayState* play) {
    MtxF m;
    Matrix_Get(&m);
    auto local = [&](NeiGi::Point p) {
        return NeiGi::Point{ p.x * m.xx + p.y * m.yx + p.z * m.zx, p.x * m.xy + p.y * m.yy + p.z * m.zy,
                             p.x * m.xz + p.y * m.yz + p.z * m.zz };
    };
    const auto& b = play->billboardMtxF;
    const auto right = NeiGi::Unit(local({ b.xx, b.yx, b.zx }));
    const auto up = NeiGi::Unit(local({ b.xy, b.yy, b.zy }));
    return { right, up, NeiGi::Unit(NeiGi::Cross(right, up)) };
}

static void NeiGi_DrawMeshMaterial(PlayState* play, const NeiGi::Mesh& mesh, Kind orb,
                                   const NeiGi::TextureMaterial* material) {
    if (mesh.count == 0)
        return;
    // Reuse shared vertices within each 32-entry RSP cache load. A full potion
    // shop must leave room in both the OPA arena and XLU command buffer.
    struct Batch {
        size_t vertexStart, vertexCount, indexStart, indexCount;
    };
    std::array<Vtx, 1536> packed{};
    std::array<uint8_t, 1536> indices{};
    std::array<Batch, 64> batches{};
    size_t vertexCount = 0, indexCount = 0, batchCount = 0;
    Batch batch{};
    auto equal = [](const Vtx& a, const Vtx& b) {
        return std::memcmp(a.v.ob, b.v.ob, sizeof(a.v.ob)) == 0 && std::memcmp(a.v.tc, b.v.tc, sizeof(a.v.tc)) == 0 &&
               std::memcmp(a.v.cn, b.v.cn, 4) == 0;
    };
    auto finish = [&]() {
        if (batch.indexCount)
            batches[batchCount++] = batch;
        batch = { vertexCount, 0, indexCount, 0 };
    };
    for (size_t i = 0; i < mesh.count; i += 3) {
        Vtx triangle[3]{};
        for (size_t j = 0; j < 3; ++j) {
            const auto& v = mesh.vertices[i + j];
            triangle[j] = { { { static_cast<int16_t>(std::lround(v.p.x * 16)),
                                static_cast<int16_t>(std::lround(v.p.y * 16)),
                                static_cast<int16_t>(std::lround(v.p.z * 16)) },
                              0,
                              { static_cast<int16_t>(std::lround(v.u * (material ? 32 : 63) * 32)),
                                static_cast<int16_t>(std::lround(v.v * (material ? 32 : 63) * 32)) },
                              { static_cast<uint8_t>(v.rgb >> 16), static_cast<uint8_t>(v.rgb >> 8),
                                static_cast<uint8_t>(v.rgb), v.alpha } } };
        }
        // Conservative reservation of three vertices keeps a complete triangle
        // in one load. Unused cache slots do not consume arena storage.
        if (batch.vertexCount > 29)
            finish();
        for (const auto& vertex : triangle) {
            size_t found = 0;
            while (found < batch.vertexCount && !equal(vertex, packed[batch.vertexStart + found]))
                ++found;
            if (found == batch.vertexCount) {
                packed[vertexCount++] = vertex;
                ++batch.vertexCount;
            }
            indices[indexCount++] = static_cast<uint8_t>(found);
            ++batch.indexCount;
        }
    }
    finish();
    auto* vertices = static_cast<Vtx*>(Graph_Alloc(play->state.gfxCtx, vertexCount * sizeof(Vtx)));
    if (vertices == nullptr)
        return;
    std::memcpy(vertices, packed.data(), vertexCount * sizeof(Vtx));
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPClearGeometryMode(POLY_XLU_DISP++,
                         G_LIGHTING | G_CULL_BACK | G_CULL_FRONT | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_FOG);
    gSPSetGeometryMode(POLY_XLU_DISP++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_2CYCLE);
    gDPSetAlphaCompare(POLY_XLU_DISP++, G_AC_NONE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
    if (material) {
        gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
        gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
        gDPSetTextureFilter(POLY_XLU_DISP++, G_TF_BILERP);
        gDPLoadTextureBlock(POLY_XLU_DISP++, material->path, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0,
                            material->repeatS ? G_TX_WRAP : G_TX_CLAMP, material->repeatT ? G_TX_WRAP : G_TX_CLAMP, 5,
                            5, G_TX_NOLOD, G_TX_NOLOD);
        // Resource RGBA multiplied by each sampled vertex's color AND alpha.
        gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA, G_CC_PASS2);
    } else if (orb != Kind::Neutral) {
        const auto color = NeiGi::OrbPalette(orb);
        gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
        gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
        gDPSetTextureFilter(POLY_XLU_DISP++, G_TF_BILERP);
        gDPLoadTextureBlock(POLY_XLU_DISP++, NeiGi::OrbTexture(orb, play->gameplayFrames).data(), G_IM_FMT_I,
                            G_IM_SIZ_8b, 64, 64, 0, G_TX_CLAMP, G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, color.hot >> 16, (color.hot >> 8) & 255, color.hot & 255, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, color.edge >> 16, (color.edge >> 8) & 255, color.edge & 255, 255);
        // I8 controls both the hot/edge gradient and alpha, exactly as previewed.
        gDPSetCombineMode(POLY_XLU_DISP++, G_CC_BLENDPE, G_CC_PASS2);
    } else {
        gSPTexture(POLY_XLU_DISP++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
        gDPSetCombineMode(POLY_XLU_DISP++, G_CC_SHADE, G_CC_SHADE);
    }
    Matrix_Push();
    Matrix_Scale(1.f / 16, 1.f / 16, 1.f / 16, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    for (size_t b = 0; b < batchCount; ++b) {
        const auto& part = batches[b];
        gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(vertices + part.vertexStart), part.vertexCount, 0);
        const auto* index = indices.data() + part.indexStart;
        for (size_t j = 0; j < part.indexCount; j += 6) {
            if (j + 3 < part.indexCount) {
                gSP2Triangles(POLY_XLU_DISP++, index[j], index[j + 1], index[j + 2], 0, index[j + 3], index[j + 4],
                              index[j + 5], 0);
            } else {
                gSP1Triangle(POLY_XLU_DISP++, index[j], index[j + 1], index[j + 2], 0);
            }
        }
    }
    Matrix_Pop();
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
}

void NeiGi_DrawMesh(PlayState* play, const NeiGi::Mesh& mesh, Kind orb) {
    NeiGi_DrawMeshMaterial(play, mesh, orb, nullptr);
}

bool NeiGi_DrawTexturedMesh(PlayState* play, const NeiGi::Mesh& mesh, const NeiGi::TextureMaterial& material) {
    if (!play || !mesh.count || !HasResource(material.path))
        return false;
    NeiGi_DrawMeshMaterial(play, mesh, Kind::Neutral, &material);
    return true;
}

static void NeiGi_DrawEffects(PlayState* play, const Presentation& item, bool upgraded) {
    const bool shimmer = CVarGetInteger(CVAR_NEI_GI_EFFECTS, 0) != 0;
    const bool energy = upgraded && (NeiGi::IsRod(item.effect) || NeiGi::IsSpell(item.effect));
    if (!shimmer && !energy)
        return;
    Matrix_Push();
    Matrix_RotateY(Spin(play), MTXMODE_APPLY);
    const auto camera = NeiGi_CameraBasis(play);
    if (energy) {
        Matrix_Push();
        Matrix_Translate(item.effectCenter.x, item.effectCenter.y, item.effectCenter.z, MTXMODE_APPLY);
        NeiGi_DrawMesh(play, NeiGi::SampleOrb(item.effect, camera), item.effect);
        NeiGi_DrawMesh(play, NeiGi::SampleEnergy(item.effect, play->gameplayFrames, camera));
        Matrix_Pop();
    }
    if (shimmer)
        NeiGi_DrawMesh(play, NeiGi::SampleShimmer(play->gameplayFrames, true, camera, item.effect));
    Matrix_Pop();
}
}

static bool NeiGi_DrawImpl(PlayState* play, GetItemEntry* entry, bool shop) {
    if (play == nullptr || entry == nullptr || entry->drawFunc == nullptr)
        return false;
    const Presentation* item = nullptr;
    for (const auto& candidate : kPresentations) {
        if (candidate.draw == entry->drawFunc) {
            item = &candidate;
            break;
        }
    }
    if (item == nullptr)
        return false;
    // Queue stable paths for the interpreter. Loading through the legacy GBI wrapper
    // here would evict/reload base resources on each draw when Alt Assets is enabled.
    // Archive presence checks preserve the original model if a required pass is absent.
    const bool upgraded = HasResource(item->opaque) && (!item->translucent || HasResource(item->translucent));
    Matrix_Push();
    if (shop && upgraded) {
        // The same shelf pose encloses the mesh, energy and crystal skin.
        // World/overhead sizes and incomplete-resource fallbacks stay intact.
        Matrix_Translate(0, item->shop.lift, 0, MTXMODE_APPLY);
        Matrix_Scale(item->shop.scale, item->shop.scale, item->shop.scale, MTXMODE_APPLY);
    }
    if (upgraded && item->draw == Randomizer_DrawCaneSomariaUpgrade) {
        // Retain the original red skill-upgrade flame with the authored cane.
        Randomizer_DrawCaneSomariaUpgradeFlame(play);
    }
    Matrix_Push();
    if (upgraded) {
        OPEN_DISPS(play->state.gfxCtx);
        Matrix_Scale(item->scale, item->scale, item->scale, MTXMODE_APPLY);
        Matrix_RotateY(Spin(play), MTXMODE_APPLY);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gDma1p(POLY_OPA_DISP++, G_DL_OTR_FILEPATH, item->opaque, 0, G_DL_PUSH);
        CLOSE_DISPS(play->state.gfxCtx);
    } else {
        entry->drawFunc(play, entry);
    }
    Matrix_Pop();
    NeiGi_DrawEffects(play, *item, upgraded);
    if (upgraded && item->translucent != nullptr) {
        // Composite the crystal skin over its contained energy, using the same pose.
        OPEN_DISPS(play->state.gfxCtx);
        Matrix_Push();
        Matrix_Scale(item->scale, item->scale, item->scale, MTXMODE_APPLY);
        Matrix_RotateY(Spin(play), MTXMODE_APPLY);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gDma1p(POLY_XLU_DISP++, G_DL_OTR_FILEPATH, item->translucent, 0, G_DL_PUSH);
        Matrix_Pop();
        CLOSE_DISPS(play->state.gfxCtx);
    }
    Matrix_Pop();
    return true;
}

extern "C" bool NeiGi_Draw(PlayState* play, GetItemEntry* entry) {
    return NeiGi_DrawImpl(play, entry, false);
}

extern "C" bool NeiGi_DrawShop(PlayState* play, GetItemEntry* entry) {
    return NeiGi_DrawImpl(play, entry, true);
}
