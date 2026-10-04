// Exact production samplers, in world units, quantized as NeiGi_DrawMesh.
#include "soh/Enhancements/randomizer/NeiUsedMagicPolicy.h"
#include <fstream>
#include <iostream>
#include <vector>
using namespace NeiUsedMagic;
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ofstream out(argv[1], std::ios::binary);
    constexpr float x = 24 * Tau / 360, y = -18 * Tau / 360;
    const float sx = std::sin(x), cx = std::cos(x), sy = std::sin(y), cy = std::cos(y);
    const Basis camera{{cy,0,sy}, {sx*sy,cx,-sx*cy}, {-cx*sy,sx,cx*cy}};
    for (uint32_t frame = 0; frame < 180; ++frame) for (int panel = 0; panel < 10; ++panel) {
        struct Layer { uint32_t material; std::vector<NeiGi::EffectVertex> vertices; };
        std::vector<Layer> layers;
        auto append = [&](const Mesh& m, Point offset = {}, uint32_t material = 0) {
            Layer layer{material,{}};
            for (size_t i = 0; i < m.count; ++i) { auto v=m.vertices[i]; v.p = v.p + offset; layer.vertices.push_back(v); }
            layers.push_back(layer);
        };
        auto iced = [&](const Mesh& m, Point offset = {}) { append(IceAtmosphere(m),offset); append(IceSurface(m),offset,1); };
        const float charge = std::min(1.f, (frame % 120u + 1) / 50.f);
        if (panel < 2) {
            const Kind kind = panel ? Kind::Ice : Kind::Light;
            const Point wake[] = {{25,0,0}, {7,0,0}, {-11,0,0}, {-29,0,0}, {-47,0,0}, {-65,0,0}};
            const auto trail = SampleTrail(kind, frame, wake, 6, 2, camera);
            const auto head = SampleProjectile(kind, frame, 2, {1,0,0}, camera);
            if (panel) { iced(trail); iced(head,{25,0,0}); }
            else { append(trail); append(head,{25,0,0}); }
        } else if (panel == 2) {
            // Existing item_time_gate.c deltas; the hold interval is sampled.
            const float growth = frame < 140 ? std::min(1.f,(frame+1)*.05f) : std::max(0.f,1-(frame-139)*.04f);
            const float alpha = frame < 140 ? std::min(255.f,(frame+1)*8.f) : std::max(0.f,255-(frame-139)*12.f);
            append(SamplePortal(frame, growth, alpha, camera));
            append(SamplePortalSurface(frame,growth,alpha),{},2);
        } else if (panel < 6) {
            const Kind kind = panel == 3 ? Kind::Fire : panel == 4 ? Kind::Ice : Kind::Light;
            if(panel==4)iced(SampleCharge(kind,frame,charge,camera));
            else {
                append(SampleCharge(kind, frame, charge, camera));
                append(SampleChargeSurface(kind,frame,charge,camera),{},panel==3?2:3);
            }
            const auto sparks=SampleChargeSparks(kind,frame,charge,camera);
            if(panel==4) iced(sparks,{16,48,0}); else append(sparks,{16,48,0});
        } else if (panel < 8) {
            const float life = 1.f - (frame % 30u) / 30.f;
            for (int i = 0; i < 6; ++i) {
                auto m = SampleBurst(panel == 6 ? Kind::Ice : Kind::Light, frame, life, camera);
                Fade(m, .6f + i * .15f, 1);
                if(panel==6) iced(m,{-100.f + i*40,0,0}); else append(m, {-100.f + i*40, 0, 0});
            }
        } else {
            const auto m=SampleSpin(panel == 8 ? Kind::Ice : Kind::Light, frame,
                                    std::min(500.f,50+(frame%30u+1)*30.f), true, camera);
            if(panel==8) iced(m); else append(m);
        }
        uint32_t count = layers.size(); out.write(reinterpret_cast<char*>(&count), 4);
        for (auto& layer:layers) {
          out.write(reinterpret_cast<char*>(&layer.material),4);
          count=layer.vertices.size();out.write(reinterpret_cast<char*>(&count),4);
          for (auto v : layer.vertices) {
            const float p[] = {std::round(v.p.x*16)/16, std::round(v.p.y*16)/16, std::round(v.p.z*16)/16};
            const uint8_t rgba[] = {uint8_t(v.rgb>>16),uint8_t(v.rgb>>8),uint8_t(v.rgb),v.alpha};
            out.write(reinterpret_cast<const char*>(p), 12); out.write(reinterpret_cast<const char*>(rgba),4);
            const float uv[]={v.u,v.v};out.write(reinterpret_cast<const char*>(uv),8);
          }
        }
    }
    return !out;
}
