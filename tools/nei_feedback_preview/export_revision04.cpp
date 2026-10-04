// Production samplers/materials; trajectories below reproduce native rod update
// order. This offline preview does not replace the real interpolation regression.
#include "mods/items/helpers/ice_trail.h"
#include "soh/Enhancements/randomizer/NeiUsedMagicPolicy.h"
#include <fstream>
#include <vector>
using namespace NeiUsedMagic;
struct Layer { Mesh mesh; uint32_t material; };
struct Volley {
    bool active = false;
    unsigned age = 0;
    float scale = 0;
    Point pos[3]{}, direction[3]{}, history[6]{};
    void reset() {
        active = false; age = 0; scale = 0;
        for (auto& p : pos) p = {0,0,0};
        for (auto& p : direction) p = {0,0,0};
        for (auto& p : history) p = {0,0,0};
    }
    void spawn() {
        reset();
        active = true;
        for (int p = 0; p < 3; ++p) {
            const float a = (p == 1 ? -1.f : p == 2 ? 1.f : 0.f) * 5460 * Tau / 65536;
            direction[p] = {std::cos(a), 0, -std::sin(a)};
        }
    }
    void step(bool hit) {
        if (!active) return;
        const float target = age >= 12 || hit ? 0 : 2;
        scale += (target - scale) * .2f;
        if (target == 0 && scale < .1f) { active = false; return; }
        if (!hit) for (int p = 0; p < 3; ++p) pos[p] = pos[p] + direction[p] * 15;
        for (int i = 5; i > 0; --i) history[i] = history[i - 1];
        history[0] = pos[0];
        ++age;
    }
};
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const bool motion = std::string(argv[2]) == "motion";
    std::ofstream out(argv[1], std::ios::binary);
    const uint32_t frameCount = 180, panelCount = motion ? 2 : 6;
    out.write((const char*)&frameCount, 4); out.write((const char*)&panelCount, 4);
    const float x = 24 * Tau / 360, y = -18 * Tau / 360;
    const Basis camera{{std::cos(y),0,std::sin(y)},
                       {std::sin(x)*std::sin(y),std::cos(x),-std::sin(x)*std::cos(y)},
                       {-std::cos(x)*std::sin(y),std::sin(x),std::cos(x)*std::cos(y)}};
    Volley volleys[2];
    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        const unsigned age = frame % 60;
        if (age == 0) { volleys[0].spawn(); volleys[1].reset(); }
        if (age == 7) volleys[1].spawn();
        if (age == 27) volleys[0].spawn();
        for (int s = 0; s < 2; ++s) volleys[s].step(s == 1 && volleys[s].age >= 9);
        for (unsigned panel = 0; panel < panelCount; ++panel) {
            std::vector<Layer> layers;
            auto append = [&](Mesh m, Point offset = {}, uint32_t material = 0) {
                if (!m.count) return;
                for (size_t i = 0; i < m.count; ++i) m.vertices[i].p = m.vertices[i].p + offset;
                layers.push_back({m, material});
            };
            if (motion) {
                const Kind kind = panel == 0 ? Kind::Fire : Kind::Ice;
                for (int s = 0; s < 2; ++s) {
                    const auto& set = volleys[s];
                    if (!set.active) continue;
                    if (kind == Kind::Fire) append(SampleTrail(kind, frame, set.history, 6, set.scale, camera));
                    else for (unsigned p = 0; p < 3; ++p) {
                        NeiIceTrailPoint history[6], wake[6]; Point trail[6];
                        for (int i = 0; i < 6; ++i) history[i] = {set.history[i].x,set.history[i].y,set.history[i].z};
                        NeiIceTrail_Reconstruct(history,6,{set.pos[p].x,set.pos[p].y,set.pos[p].z},p,wake);
                        for (int i = 0; i < 6; ++i) trail[i] = {wake[i].x,wake[i].y,wake[i].z};
                        append(SampleTrail(kind, frame, trail, 6, set.scale, camera), {}, 8);
                    }
                    for (unsigned p = 0; p < 3; ++p) {
                        append(SampleProjectileSurface(kind, frame+s*19+p*7,set.scale,set.direction[p],camera), set.pos[p], panel == 0 ? 4 : 5);
                        append(SampleProjectile(kind, frame+s*19+p*7,set.scale,set.direction[p],camera), set.pos[p]);
                    }
                }
            } else {
                const Kind kind = panel % 3 == 0 ? Kind::Fire : panel % 3 == 1 ? Kind::Ice : Kind::Light;
                const unsigned tick = frame % 90;
                if (panel < 3 && tick < 52) {
                    const float charge = std::min(1.f, (tick + 1) * .02f);
                    const Point focus{16,48,0}, body{0,5,0};
                    const auto aura = SampleCharge(kind,frame,charge,camera);
                    const auto sparks = SampleChargeSparks(kind,frame,charge,camera);
                    if (kind == Kind::Ice) {
                        append(IceAtmosphere(aura),body); append(IceSurface(aura),body,1);
                        append(IceAtmosphere(sparks),focus); append(IceSurface(sparks),focus,1);
                    } else {
                        append(aura,body); append(SampleChargeSurface(kind,frame,charge,camera),body,kind == Kind::Fire ? 9 : 3); append(sparks,focus);
                    }
                } else if (panel >= 3 && tick >= 52 && tick < 68) {
                    const float radius = std::min(500.f,80+(tick-52)*30.f);
                    append(SampleSpin(kind,frame,radius,true,camera),{0,5,0});
                    if (kind != Kind::Light) {
                        const unsigned material = kind == Kind::Fire ? 9 : 10;
                        append(SampleSpinSurface(kind,frame,radius,true),{0,5,0},material);
                        append(SampleSpinFlow(kind,frame,radius,true,1),{0,5,0},material);
                    }
                }
            }
            uint32_t count = layers.size(); out.write((char*)&count,4);
            for (const auto& layer : layers) {
                out.write((const char*)&layer.material,4); count = layer.mesh.count; out.write((char*)&count,4);
                for (size_t i = 0; i < layer.mesh.count; ++i) {
                    const auto& v = layer.mesh.vertices[i];
                    const float p[] = {std::round(v.p.x*16)/16,std::round(v.p.y*16)/16,std::round(v.p.z*16)/16};
                    const uint8_t c[] = {uint8_t(v.rgb>>16),uint8_t(v.rgb>>8),uint8_t(v.rgb),v.alpha};
                    const float uv[] = {std::round(v.u*1024)/1024,std::round(v.v*1024)/1024};
                    out.write((const char*)p,12);out.write((const char*)c,4);out.write((const char*)uv,8);
                }
            }
        }
    }
    return !out;
}
