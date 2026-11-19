// Private receiving-game renderer. All native C calls use the Fleet adapter's typed table.
#include "align_asset_macro.h"
#include "macros.h"
#include <map>
#include <vector>
#include <string>
#define gSPDisplayList(pkt, dl) sContext->displayList((pkt), (dl))
#define gSPSegment(pkt, segIndex, target) sContext->segment((pkt), (segIndex), (const void*)(target))
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#undef OPEN_DISPS
#undef CLOSE_DISPS
#define OPEN_DISPS(ctx)                    \
    {                                      \
        GraphicsContext* __gfxCtx = (ctx); \
        sContext->openInterpolationChild(__FILE__, __LINE__);
#define CLOSE_DISPS(ctx)                 \
    sContext->closeInterpolationChild(); \
    }
#undef MATRIX_NEWMTX
#undef MATRIX_FINALIZE_AND_LOAD
#define MATRIX_NEWMTX(ctx) sContext->newMtx((ctx), __FILE__, __LINE__)
#define MATRIX_FINALIZE_AND_LOAD(pkt, ctx) \
    gSPMatrix((pkt), MATRIX_NEWMTX(ctx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW)
#define Matrix_NewMtx(ctx, file, line) sContext->newMtx((ctx), (file), (line))
#define Matrix_Push() sContext->matrixPush()
#define Matrix_Pop() sContext->matrixPop()
#define Matrix_ReplaceRotation(m) sContext->matrixReplaceRotation(m)
#define Matrix_Scale(x, y, z, m) sContext->matrixScale((x), (y), (z), (m))
#define Matrix_Translate(x, y, z, m) sContext->matrixTranslate((x), (y), (z), (m))
#define Matrix_RotateY(x, m) sContext->matrixRotateY((x), (m))
#define Matrix_RotateZ(x, m) sContext->matrixRotateZ((x), (m))
#define Matrix_RotateZYX(x, y, z, m) sContext->matrixRotateZYX((x), (y), (z), (m))
#define Gfx_SetupDL sContext->setupDL
#define Gfx_TexScrollEx sContext->texScroll
#define Gfx_TwoTexScrollEx sContext->twoTexScroll
#define Play_SetFog sContext->setFog
#define CVarGetInteger sContext->cvarInteger
#define CVarGetColor24 sContext->cvarColor24
#ifndef CVAR_COSMETIC
#define CVAR_COSMETIC(name) "gCosmetics." name
#endif
static void FleetSetupOpa(GraphicsContext* gfx, uint32_t index) {
    gfx->polyOpa.p = sContext->setupDL(gfx->polyOpa.p, index);
}
static void FleetSetupXlu(GraphicsContext* gfx, uint32_t index) {
    gfx->polyXlu.p = sContext->setupDL(gfx->polyXlu.p, index);
}
#define Gfx_SetupDL_25Opa(ctx) FleetSetupOpa((ctx), 25)
#define Gfx_SetupDL_25Xlu(ctx) FleetSetupXlu((ctx), 25)
#define Gfx_SetupDL25_Opa(ctx) FleetSetupOpa((ctx), 25)
#define Gfx_SetupDL25_Xlu(ctx) FleetSetupXlu((ctx), 25)
#define Gfx_SetupDL23_Opa(ctx) FleetSetupOpa((ctx), 23)
#define Gfx_SetupDL_26Opa(ctx) FleetSetupOpa((ctx), 26)

struct FleetScrollLayer {
    int8_t x, y;
    uint8_t width, height;
};
struct FleetScrollSegment {
    uint8_t segment;
    bool dual;
    FleetScrollLayer layers[2];
};
struct FleetMaterialRecipe {
    bool valid = false;
    std::vector<FleetScrollSegment> segments;
};
static std::map<std::string, FleetMaterialRecipe> sMaterials;
static bool ReadFleetMaterial(const char*, const uint8_t* data, uint64_t size, void* output) {
    auto& result = *static_cast<FleetMaterialRecipe*>(output);
    // Native MM TextureAnimation binary: little-endian OTAN header, then inline typed entries.
    if (!data || size < 68 || data[0] != 0 || data[4] != 'N' || data[5] != 'A' || data[6] != 'T' || data[7] != 'O')
        return false;
    const uint32_t count =
        uint32_t(data[64]) | uint32_t(data[65]) << 8 | uint32_t(data[66]) << 16 | uint32_t(data[67]) << 24;
    if (!count || count > 6)
        return false;
    size_t at = 68;
    std::vector<FleetScrollSegment> parsed;
    for (uint32_t i = 0; i < count; ++i) {
        if (at + 3 > size)
            return false;
        const int8_t segment = static_cast<int8_t>(data[at++]);
        const uint16_t type = uint16_t(data[at]) | uint16_t(data[at + 1]) << 8;
        at += 2;
        if (!segment || segment < -6 || segment > 6 || type > 1)
            return false;
        FleetScrollSegment entry = {};
        entry.segment = static_cast<uint8_t>((segment < 0 ? -segment : segment) + 7);
        entry.dual = type == 1;
        const int layers = entry.dual ? 2 : 1;
        if (at + 4 * layers > size)
            return false;
        for (int layer = 0; layer < layers; ++layer) {
            entry.layers[layer] = { static_cast<int8_t>(data[at]), static_cast<int8_t>(data[at + 1]), data[at + 2],
                                    data[at + 3] };
            at += 4;
            if (!entry.layers[layer].width || !entry.layers[layer].height)
                return false;
        }
        parsed.push_back(entry);
    }
    if (at != size)
        return false;
    result.segments = std::move(parsed);
    result.valid = true;
    return false; // Exactly one resource, never a wildcard traversal.
}
static const FleetMaterialRecipe* FleetGetMaterial(const char* path) {
    auto existing = sMaterials.find(path);
    if (existing != sMaterials.end())
        return &existing->second;
    if (!SOH_MOD_API_HAS(sApi, ReadArchiveFiles) || !sApi->ReadArchiveFiles)
        return nullptr;
    FleetMaterialRecipe recipe;
    sApi->ReadArchiveFiles(std::strncmp(path, "__OTR__", 7) == 0 ? path + 7 : path, ReadFleetMaterial, &recipe);
    // Missing/unsupported data is retried after resource initialization, never cached as success.
    if (!recipe.valid)
        return nullptr;
    return &sMaterials.emplace(path, std::move(recipe)).first->second;
}
static bool FleetMaterial(PlayState* play, const char* path) {
    const auto recipe = FleetGetMaterial(path);
    if (!recipe)
        return false;
    for (const auto& segment : recipe->segments) {
        const auto& a = segment.layers[0];
        const auto& b = segment.layers[1];
        const uint32_t frame = play->state.frames;
        Gfx* dl = segment.dual
                      ? sContext->twoTexScroll(play->state.gfxCtx, 0, frame * a.x, frame * a.y, a.width, a.height, 1,
                                               frame * b.x, frame * b.y, b.width, b.height, a.x, a.y, b.x, b.y)
                      : sContext->texScroll(play->state.gfxCtx, frame * a.x, frame * a.y, a.width, a.height, a.x, a.y);
        OPEN_DISPS(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, segment.segment, dl);
        gSPSegment(POLY_XLU_DISP++, segment.segment, dl);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    return true;
}
static uint32_t sDrawingTriforce;
static uint32_t FleetOwnerLevel(const char*) {
    return sDrawingTriforce;
}
#include "native_draw.inc.h"

static bool FleetNativeDrawReady(int drawId) {
    if (drawId < 0 || drawId >= static_cast<int>(sizeof(sForeignDrawTable) / sizeof(sForeignDrawTable[0])) ||
        !sForeignDrawTable[drawId].draw || !sContext || !sContext->isComboMode || !sContext->isComboMode())
        return false;
    if (!sContext->setupDL || !sContext->newMtx || !sContext->matrixPush || !sContext->matrixPop ||
        !sContext->matrixReplaceRotation || !sContext->matrixScale || !sContext->matrixTranslate ||
        !sContext->matrixRotateX || !sContext->matrixRotateY || !sContext->matrixRotateZ ||
        !sContext->matrixRotateZYX || !sContext->texScroll || !sContext->twoTexScroll ||
        !sContext->openInterpolationChild || !sContext->closeInterpolationChild || !sContext->setFog ||
        !sContext->cvarInteger || !sContext->cvarColor24 || !sContext->displayList || !sContext->segment)
        return false;
    if (!SOH_MOD_API_HAS(sApi, HasResource) || !sApi->HasResource)
        return false;
    for (const auto resource : sForeignDrawTable[drawId].resources)
        if (resource && !sApi->HasResource(static_cast<const char*>(resource)))
            return false;
    if (drawId == FD_GID_MOONS_TEAR && !FleetGetMaterial(gGiMoonsTearTexAnim))
        return false;
    return true;
}
static bool FleetDrawNative(PlayState* play, int drawId) {
    if (!play || !FleetNativeDrawReady(drawId))
        return false;
    sContext->matrixPush();
    sForeignDrawTable[drawId].draw(play, static_cast<int16_t>(drawId));
    sContext->matrixPop();
    return true;
}
