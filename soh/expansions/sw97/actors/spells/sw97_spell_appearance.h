#ifndef SW97_SPELL_APPEARANCE_H
#define SW97_SPELL_APPEARANCE_H

#include "global.h"
#include "align_asset_macro.h"
#include "soh/ResourceManagerHelpers.h"

// Embedded SW97 materials call the dynamic scroll segment after their native
// image loads/colors and before vertices. Wrap that call without changing the
// shared material, its render tiles, or the original scroll commands.
static inline Gfx* Sw97_SpellScrollWithAppearance(GraphicsContext* gfxCtx, Gfx* scroll, const char* first,
                                                  const char* second, s32 imageSize, const Color_RGB8* primary,
                                                  const Color_RGB8* secondary) {
    const u8 textures =
        ResourceMgr_IsAltAssetsEnabled() && ResourceMgr_FileAltExists(first) && ResourceMgr_FileAltExists(second);
    if (!textures && primary == NULL && secondary == NULL) {
        return scroll;
    }

    Gfx* result = Graph_Alloc(gfxCtx, 15 * sizeof(Gfx));
    if (result == NULL) {
        return scroll;
    }
    Gfx* gfx = result;
    gDPPipeSync(gfx++);
    if (textures) {
        // Preserve the native 64x64 domain: Fire I4 is 2048 bytes, while
        // Water/Forest I8 is 4096 bytes. Only tile 7 is a transfer descriptor.
        const s32 loadCount = imageSize == G_IM_SIZ_4b ? ((64 * 64) / 4) - 1 : ((64 * 64) / 2) - 1;
        const s32 dxt = imageSize == G_IM_SIZ_4b ? CALC_DXT_4b(64) : CALC_DXT(64, G_IM_SIZ_8b_BYTES);
        gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_16b, 1, first);
        gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, G_TX_WRAP, 0, 0, G_TX_WRAP, 0, 0);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, loadCount, dxt);
        gDPPipeSync(gfx++);
        gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_16b, 1, second);
        gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_16b, 0, 0x100, G_TX_LOADTILE, 0, G_TX_WRAP, 0, 0, G_TX_WRAP, 0, 0);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, loadCount, dxt);
        gDPPipeSync(gfx++);
    }
    // Only Water/Forest use these overrides; preserve their embedded material
    // alpha and LOD values. Fire retains its dynamic alpha in MagicFire_Draw.
    if (primary != NULL) {
        gDPSetPrimColor(gfx++, 0, 0x80, primary->r, primary->g, primary->b, 255);
    }
    if (secondary != NULL) {
        gDPSetEnvColor(gfx++, secondary->r, secondary->g, secondary->b, 0);
    }
    gSPDisplayList(gfx++, scroll);
    gSPEndDisplayList(gfx);
    return result;
}

#endif
