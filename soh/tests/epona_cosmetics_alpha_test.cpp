// Runs the production overlay builder through the renderer's real combiner
// decoder and shader input mapping. The final two-cycle alpha equation is
// evaluated headlessly so an invisible overlay cannot pass as a valid stream.
#include <fast/lus_gbi.h>
#undef GIMMCMD
#include "EponaCosmeticsDL.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace Fast {
#include "epona_combiner_types.inc"

struct AlphaState {
    uint64_t combine_mode = 0;
    uint8_t prim_lod_fraction = 0;
    struct {
        uint8_t r, g, b, a;
    } prim_color{};
};
class Interpreter {
  public:
    AlphaState state;
    AlphaState* mRdp = &state;
    void GenerateCC(ColorCombiner*, const ColorCombinerKey&);
    void GfxDpSetCombineMode(uint32_t, uint32_t, uint32_t, uint32_t);
    void GfxDpSetPrimColor(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
};
static auto sInterpreter = std::make_shared<Interpreter>();
static std::weak_ptr<Interpreter> mInstance = sInterpreter;
#define C0(pos, width) ((cmd->words.w0 >> (pos)) & ((1U << width) - 1))
#define C1(pos, width) ((cmd->words.w1 >> (pos)) & ((1U << width) - 1))
#include "epona_combiner_production.inc"

static float Alpha(float textureAlpha) {
    ColorCombiner comb{};
    const auto& state = sInterpreter->state;
    sInterpreter->GenerateCC(&comb, { state.combine_mode, SHADER_OPT(ALPHA) | SHADER_OPT(_2CYC), 0 });
    float combined = 0;
    for (unsigned cycle = 0; cycle < 2; ++cycle) {
        float values[4]{};
        for (unsigned slot = 0; slot < 4; ++slot) {
            const auto source = (comb.shader_id0 >> (cycle * 32 + 16 + slot * 4)) & 15;
            switch (source) {
                case SHADER_0:
                    values[slot] = 0;
                    break;
                case SHADER_1:
                    values[slot] = 1;
                    break;
                case SHADER_COMBINED:
                    values[slot] = combined;
                    break;
                case SHADER_TEXEL0:
                case SHADER_TEXEL1:
                    values[slot] = textureAlpha;
                    break;
                default:
                    if (source < SHADER_INPUT_1 || source > SHADER_INPUT_7) {
                        std::abort();
                    }
                    switch (comb.shader_input_mapping[1][source - SHADER_INPUT_1]) {
                        case G_ACMUX_PRIMITIVE:
                            values[slot] = state.prim_color.a / 255.0f;
                            break;
                        case G_ACMUX_PRIM_LOD_FRAC:
                            values[slot] = state.prim_lod_fraction / 255.0f;
                            break;
                        default:
                            std::abort();
                    }
            }
        }
        combined = (values[0] - values[1]) * values[2] + values[3];
    }
    return combined;
}
} // namespace Fast

int main() {
    using namespace EponaCosmetics;
    static const char path[] = "__OTR__objects/object_horse/test";
    static const uint8_t mask[] = { 0, 1 };
    const auto resolver = [](uint64_t, bool) { return TextureMaterial{ false, { { path, { mask, mask, mask } } } }; };
    const std::vector<Gfx> source = {
        gsDPSetCycleType(G_CYC_2CYCLE),
        { 0x20100000, 0 },
        { 0, 1 }, // Native RGBA16 texture hash.
        gsDPLoadSync(),
        gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_SURF2),
        gsDPSetCombineMode(G_CC_MODULATERGB, G_CC_PASS2),
        gsDPSetPrimColor(0, 0, 255, 255, 255, 255),
        gsSP1Triangle(0, 1, 2, 0),
        gsSPEndDisplayList(),
    };
    unsigned tested = 0;
    for (unsigned changed = 1; changed < 8; ++changed) {
        const auto built = BuildNativeDisplayList(source, changed, resolver);
        if (built.empty())
            std::abort();
        bool overlay = false;
        unsigned part = 0;
        for (size_t i = 0; i < built.size(); ++i) {
            Fast::F3DGfx packet{};
            packet.words.w0 = built[i].words.w0;
            packet.words.w1 = built[i].words.w1;
            auto* command = &packet;
            const auto opcode = command->words.w0 >> 24;
            if (opcode == G_SETCOMBINE)
                Fast::gfx_set_combine_handler_rdp(&command);
            if (opcode == G_SETPRIMCOLOR)
                Fast::gfx_set_prim_color_handler_rdp(&command);
            if (opcode == G_DL) {
                part = (command->words.w1 >> 24) - 9;
                overlay = part < 3;
            }
            if (opcode == G_TRI1 && overlay) {
                for (const float expected : { 0.0f, 0.5f, 1.0f }) {
                    const float actual = Fast::Alpha(expected);
                    if (std::abs(actual - expected) > 0.0001f) {
                        std::fprintf(
                            stderr,
                            "FAIL Epona overlay part %u changed=%u: texture alpha %.1f becomes %.1f (prim LOD=%u)\n",
                            part, changed, expected, actual, Fast::sInterpreter->state.prim_lod_fraction);
                        return 1;
                    }
                }
                ++tested;
            }
            if (opcode == G_SETTIMG_OTR_HASH || opcode == G_REGBLENDEDTEX)
                ++i;
        }
    }
    if (tested != 12)
        std::abort();
    std::puts("PASS Epona coat/hair/eyes overlays preserve opaque, masked and filtered alpha through the real renderer "
              "combiner");
}
