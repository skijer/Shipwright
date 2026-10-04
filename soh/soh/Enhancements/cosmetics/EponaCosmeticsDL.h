#pragma once

#include <libultraship/libultra/gbi.h>
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace EponaCosmetics {
struct MaskTarget {
    const char* path;
    std::array<const uint8_t*, 3> inverseMasks{};
};

struct TextureMaterial {
    bool palette = false;
    std::vector<MaskTarget> targets;
};

using TextureResolver = std::function<TextureMaterial(uint64_t, bool)>;

inline std::vector<Gfx> BuildNativeDisplayList(const std::vector<Gfx>& source, unsigned changed,
                                               const TextureResolver& resolve) {
    if (changed == 0) {
        return source;
    }
    std::vector<Gfx> result, textureSetup;
    TextureMaterial material;
    Gfx prim{}, combine{}, render{};
    bool captureTexture = false, havePrim = false, haveCombine = false, haveRender = false, intensityCoat = false;
    const auto append = [&](Gfx command) { result.push_back(command); };
    const auto registerMask = [&](const MaskTarget& target, const uint8_t* mask) {
        Gfx command{};
        command.words.w0 = static_cast<uintptr_t>(G_REGBLENDEDTEX) << 24;
        command.words.w1 = reinterpret_cast<uintptr_t>(target.path);
        append(command);
        command.words.w0 = reinterpret_cast<uintptr_t>(mask);
        command.words.w1 = 0;
        append(command);
    };
    const auto segment = [&](unsigned number) {
        Gfx command = gsSPDisplayList(reinterpret_cast<Gfx*>((number << 24) | 1));
        append(command);
    };

    for (size_t i = 0; i < source.size();) {
        const auto opcode = static_cast<uint8_t>(source[i].words.w0 >> 24);
        size_t length = 1;
        switch (opcode) {
            case G_SETTIMG_OTR_HASH:
            case G_VTX_OTR_HASH:
            case G_MARKER:
                length = 2;
                break;
            case G_VTX:
            case G_TRI1:
            case G_TRI2:
            case G_TEXTURE:
            case G_GEOMETRYMODE:
            case G_ENDDL:
            case G_SETOTHERMODE_L:
            case G_SETOTHERMODE_H:
            case G_RDPLOADSYNC:
            case G_RDPPIPESYNC:
            case G_RDPTILESYNC:
            case G_LOADTLUT:
            case G_SETTILESIZE:
            case G_LOADBLOCK:
            case G_SETTILE:
            case G_SETPRIMCOLOR:
            case G_SETENVCOLOR:
            case G_SETCOMBINE:
            case G_SETTIMG:
                break;
            default:
                // These are known native leaf lists. Never rewrite custom branches or matrix operations.
                return {};
        }
        if (i + length > source.size()) {
            return {};
        }
        if (opcode == G_ENDDL) {
            if (i + 1 != source.size()) {
                return {};
            }
            append(source[i]);
            return result;
        }
        if (opcode == G_SETTIMG_OTR_HASH || opcode == G_SETTIMG) {
            if (opcode == G_SETTIMG && source[i].words.w1 != 0x08000001) {
                return {};
            }
            const auto resolved =
                opcode == G_SETTIMG
                    ? resolve(0, true)
                    : resolve((static_cast<uint64_t>(source[i + 1].words.w0) << 32) | source[i + 1].words.w1, false);
            if (!resolved.palette) {
                material = resolved;
                intensityCoat = ((source[i].words.w0 >> 21) & 7) == G_IM_FMT_I && material.targets.size() == 1 &&
                                material.targets[0].inverseMasks[0] && !material.targets[0].inverseMasks[1] &&
                                !material.targets[0].inverseMasks[2];
                textureSetup.clear();
                captureTexture = true;
            }
        }
        if (opcode == G_SETPRIMCOLOR) {
            prim = source[i];
            havePrim = true;
        } else if (opcode == G_SETCOMBINE) {
            combine = source[i];
            haveCombine = true;
        } else if (opcode == G_SETOTHERMODE_L) {
            // Other low-mode commands (alpha compare etc.) are unsupported by this native template.
            if ((source[i].words.w0 & 0xFFFFFF) != 0x1C) {
                return {};
            }
            render = source[i];
            haveRender = true;
        }
        if (opcode == G_TRI1 || opcode == G_TRI2) {
            captureTexture = false;
            size_t end = i;
            while (end < source.size()) {
                const auto next = static_cast<uint8_t>(source[end].words.w0 >> 24);
                if (next != G_TRI1 && next != G_TRI2) {
                    break;
                }
                ++end;
            }
            const bool directCoat = intensityCoat && (changed & 1);
            if (directCoat) {
                if (!havePrim) {
                    return {};
                }
                // Adult chest/neck/shoulder/thigh are entirely coat. I8 alpha is intensity, so an
                // alpha overlay would incorrectly blend the original chestnut into the chosen hue.
                append(gsDPSetPrimColor(0, 0, 255, 255, 255, 255));
                segment(9);
            }
            result.insert(result.end(), source.begin() + i, source.begin() + end);
            if (directCoat) {
                segment(12);
                append(prim);
            }
            for (unsigned part = 0; part < 3; ++part) {
                if (part == 0 && directCoat) {
                    continue;
                }
                bool hasMask = false;
                for (const auto& target : material.targets) {
                    hasMask |= target.inverseMasks[part] != nullptr;
                }
                if (!(changed & (1 << part)) || !hasMask) {
                    continue;
                }
                for (const auto& target : material.targets) {
                    if (!target.inverseMasks[part]) {
                        // A closed blink needs an all-hidden mask, not an unmasked tinted eyelid.
                        return {};
                    }
                }
                if (textureSetup.empty() || !havePrim || !haveCombine || !haveRender) {
                    return {};
                }
                append(gsDPPipeSync());
                for (const auto& target : material.targets) {
                    // All blinking textures are registered together; segment 08 selects the current one.
                    if (target.inverseMasks[part]) {
                        registerMask(target, target.inverseMasks[part]);
                    }
                }
                result.insert(result.end(), textureSetup.begin(), textureSetup.end());
                append(gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_DECAL2));
                // Native horse lists ignore texture alpha. The overlay must use it to cut out the mask.
                // Alpha C cannot select constant 1: mux 6 there is PRIM_LOD_FRAC (zero here).
                // Pass first-cycle alpha through D so selected coat/hair/eye pixels remain visible.
                append(gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, TEXEL0, 0, PRIMITIVE, 0, COMBINED, 0, PRIMITIVE, 0, 0, 0,
                                          0, COMBINED));
                append(gsDPSetPrimColor(0, 0, 255, 255, 255, 255));
                segment(9 + part);
                // Duplicate only triangles: never reload or transform the shared Skin vertex cache.
                result.insert(result.end(), source.begin() + i, source.begin() + end);
                append(gsDPPipeSync());
                for (const auto& target : material.targets) {
                    if (target.inverseMasks[part]) {
                        registerMask(target, nullptr);
                    }
                }
                segment(12);
                result.insert(result.end(), textureSetup.begin(), textureSetup.end());
                append(render);
                append(combine);
                append(prim);
            }
            i = end;
            continue;
        }
        result.insert(result.end(), source.begin() + i, source.begin() + i + length);
        if (captureTexture && (opcode == G_SETTIMG_OTR_HASH || opcode == G_SETTIMG || opcode == G_SETTILE ||
                               opcode == G_LOADBLOCK || opcode == G_LOADTLUT || opcode == G_SETTILESIZE ||
                               opcode == G_RDPLOADSYNC || opcode == G_RDPPIPESYNC || opcode == G_RDPTILESYNC)) {
            textureSetup.insert(textureSetup.end(), source.begin() + i, source.begin() + i + length);
        }
        i += length;
    }
    return {}; // A truncated resource must leave the actor on its original display list.
}
} // namespace EponaCosmetics
