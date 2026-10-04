#include "NeiUsedMagicPresentation.h"
#include "NeiUsedMagicPolicy.h"
#include "NeiGiRender.h"
#include "mods/items/custom_items.h"
#include "soh/ResourceManagerHelpers.h"
extern "C" {
#include "functions.h"
}

namespace {
alignas(2) static const char kIceTexture[] = "__OTR__objects/nei_used_magic/ice_fracture";
alignas(2) static const char kFireTexture[] = "__OTR__objects/nei_used_magic/fire_wisp";
alignas(2) static const char kLightTexture[] = "__OTR__objects/nei_used_magic/light_rays";
const NeiGi::TextureMaterial kIceMaterial{ kIceTexture, true, true };
const NeiGi::TextureMaterial kFireMaterial{ kFireTexture, true, false };
const NeiGi::TextureMaterial kLightMaterial{ kLightTexture, true, false };
alignas(2) static const char kFireAttackTexture[] = "__OTR__objects/nei_rod_attack/fire_surge";
alignas(2) static const char kIceAttackTexture[] = "__OTR__objects/nei_rod_attack/frost_surge";
alignas(2) static const char kLightAttackTexture[] = "__OTR__objects/nei_rod_attack/light_surge";
const NeiGi::TextureMaterial kAttackMaterial[] = { { kFireAttackTexture, false, false },
                                                   { kIceAttackTexture, false, false },
                                                   { kLightAttackTexture, false, false } };
alignas(2) static const char kIceWakeTexture[] = "__OTR__objects/nei_rod_attack/ice_release_flow";
const NeiGi::TextureMaterial kIceWakeMaterial{ kIceWakeTexture, true, true };
alignas(2) static const char kFireReleaseTexture[] = "__OTR__objects/nei_rod_attack/fire_release_crest";
alignas(2) static const char kIceReleaseTexture[] = "__OTR__objects/nei_rod_attack/ice_release_crest";
const NeiGi::TextureMaterial kReleaseMaterial[] = { { kFireReleaseTexture, true, false },
                                                    { kIceReleaseTexture, true, false },
                                                    { kLightAttackTexture, false, false } };
void DrawAttackSurface(PlayState* play, const NeiGi::Mesh& mesh, int element, bool release = false) {
    if (element < 0 || element > 2 || !mesh.count)
        return;
    if (!NeiGi_DrawTexturedMesh(play, mesh, release ? kReleaseMaterial[element] : kAttackMaterial[element])) {
        auto fallback = mesh;
        const uint32_t colors[] = { 0xFFB657, 0xA5E9FF, 0xFFF3B8 };
        for (size_t i = 0; i < fallback.count; ++i) {
            fallback.vertices[i].rgb = colors[element];
            fallback.vertices[i].alpha = NeiUsedMagic::Alpha(fallback.vertices[i].alpha * .28f);
        }
        NeiGi_DrawMesh(play, fallback);
    }
}
NeiGi::Kind Element(int element) {
    switch (element) {
        case 0:
            return NeiGi::Kind::Fire;
        case 1:
            return NeiGi::Kind::Ice;
        case 2:
            return NeiGi::Kind::Light;
        default:
            return NeiGi::Kind::Neutral;
    }
}
bool Position(const Vec3f* p) {
    return p && std::isfinite(p->x) && std::isfinite(p->y) && std::isfinite(p->z);
}
NeiGi::Point Point(const Vec3f& p) {
    return { p.x, p.y, p.z };
}
struct MatrixScope {
    MatrixScope(const Vec3f& position) {
        Matrix_Push();
        Matrix_Translate(position.x, position.y, position.z, MTXMODE_NEW);
    }
    ~MatrixScope() {
        Matrix_Pop();
    }
};
void Draw(PlayState* play, const NeiGi::Mesh& mesh, bool ice) {
    if (ice && (ResourceMgr_FileExists(kIceMaterial.path) ||
                (ResourceMgr_IsAltAssetsEnabled() && ResourceMgr_FileAltExists(kIceMaterial.path)))) {
        NeiGi_DrawMesh(play, NeiUsedMagic::IceAtmosphere(mesh));
        const auto surface = NeiUsedMagic::IceSurface(mesh);
        if (!NeiGi_DrawTexturedMesh(play, surface, kIceMaterial))
            NeiGi_DrawMesh(play, surface);
    } else {
        NeiGi_DrawMesh(play, mesh);
    }
}
} // namespace

extern "C" void NeiUsedMagic_DrawProjectile(PlayState* play, int element, const Vec3f* position, const Vec3f* velocity,
                                            float scale, unsigned phase) {
    if (!play || !Position(position) || element < 0 || element > 2 || scale <= 0)
        return;
    const MatrixScope matrix(*position);
    const auto direction = Position(velocity) ? Point(*velocity) : NeiGi::Point{ 0, 0, 1 };
    DrawAttackSurface(play,
                      NeiUsedMagic::SampleProjectileSurface(Element(element), play->gameplayFrames + phase, scale,
                                                            direction, NeiGi_CameraBasis(play)),
                      element);
    Draw(play,
         NeiUsedMagic::SampleProjectile(Element(element), play->gameplayFrames + phase, scale, direction,
                                        NeiGi_CameraBasis(play)),
         false);
}
extern "C" void NeiUsedMagic_DrawTrail(PlayState* play, int element, const Vec3f* positions, unsigned count,
                                       float scale) {
    if (!play || !positions || count < 2 || count > 6 || !Position(positions))
        return;
    NeiGi::Point relative[6]{};
    for (unsigned i = 0; i < count; ++i) {
        if (!Position(&positions[i]))
            return;
        relative[i] = Point(positions[i]) - Point(positions[0]);
        if (std::abs(relative[i].x) > 1500 || std::abs(relative[i].y) > 1500 || std::abs(relative[i].z) > 1500)
            return;
    }
    const MatrixScope matrix(positions[0]);
    const auto wake = NeiUsedMagic::SampleTrail(Element(element), play->gameplayFrames, relative, count, scale,
                                                NeiGi_CameraBasis(play));
    if (element != 1 || !NeiGi_DrawTexturedMesh(play, wake, kIceWakeMaterial))
        NeiGi_DrawMesh(play, wake);
}
extern "C" void NeiUsedMagic_DrawCharge(PlayState* play, Player* player, int element, float charge) {
    if (!play || !player || !Position(&player->actor.world.pos) || charge <= 0)
        return;
    Vec3f origin = player->actor.world.pos;
    origin.y += 5;
    {
        const MatrixScope matrix(origin);
        Draw(play, NeiUsedMagic::SampleCharge(Element(element), play->gameplayFrames, charge, NeiGi_CameraBasis(play)),
             element == 1);
        if (element != 1)
            NeiGi_DrawTexturedMesh(play,
                                   NeiUsedMagic::SampleChargeSurface(Element(element), play->gameplayFrames, charge,
                                                                     NeiGi_CameraBasis(play)),
                                   element == 0 ? kReleaseMaterial[0] : kLightMaterial);
    }
}
extern "C" void NeiUsedMagic_DrawChargeFocus(PlayState* play, int element) {
    if (!play || element < 0 || element > 2)
        return;
    const bool charging[] = { gCustomItemState.fireRodCharging != 0, gCustomItemState.iceRodCharging != 0,
                              gCustomItemState.lightRodCharging != 0 };
    const float charge[] = { gCustomItemState.fireRodChargeLevel, gCustomItemState.iceRodChargeLevel,
                             gCustomItemState.lightRodChargeLevel };
    if (!charging[element] || charge[element] <= 0)
        return;
    Vec3f zero{}, focus{};
    Matrix_MultVec3f(&zero, &focus);
    if (!Position(&focus))
        return;
    const MatrixScope matrix(focus);
    Draw(play,
         NeiUsedMagic::SampleChargeSparks(Element(element), play->gameplayFrames, charge[element],
                                          NeiGi_CameraBasis(play)),
         element == 1);
}
extern "C" void NeiUsedMagic_DrawSpin(PlayState* play, Player* player, int element, float radius, bool big) {
    if (!play || !player || !Position(&player->actor.world.pos))
        return;
    Vec3f origin = player->actor.world.pos;
    origin.y += 5;
    const MatrixScope matrix(origin);
    Draw(play, NeiUsedMagic::SampleSpin(Element(element), play->gameplayFrames, radius, big, NeiGi_CameraBasis(play)),
         false);
    DrawAttackSurface(play, NeiUsedMagic::SampleSpinSurface(Element(element), play->gameplayFrames, radius, big),
                      element, true);
    DrawAttackSurface(play, NeiUsedMagic::SampleSpinFlow(Element(element), play->gameplayFrames, radius, big, 1),
                      element, true);
}
extern "C" void NeiUsedMagic_DrawBurst(PlayState* play, int element, const Vec3f* position, float size, float life) {
    if (!play || !Position(position) || size <= 0 || life <= 0)
        return;
    const MatrixScope matrix(*position);
    size = NeiUsedMagic::Clamp(size, 0, 2);
    Matrix_Scale(size, size, size, MTXMODE_APPLY);
    Draw(play, NeiUsedMagic::SampleBurst(Element(element), play->gameplayFrames, life, NeiGi_CameraBasis(play)),
         element == 1);
}
extern "C" void NeiUsedMagic_DrawPortal(PlayState* play, Player* player, float scale, float alpha) {
    if (!play || !player || !Position(&player->actor.world.pos) || scale <= 0 || alpha <= 0)
        return;
    Vec3f origin = player->actor.world.pos;
    origin.y += 1;
    const MatrixScope matrix(origin);
    NeiGi_DrawMesh(play, NeiUsedMagic::SamplePortal(play->gameplayFrames, scale, alpha, NeiGi_CameraBasis(play)));
    NeiGi_DrawTexturedMesh(play, NeiUsedMagic::SamplePortalSurface(play->gameplayFrames, scale, alpha), kFireMaterial);
}
