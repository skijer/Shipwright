#include "NeiHeldPresentation.h"
#include "NeiGiRender.h"
#include "NeiUsedMagicPresentation.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/frame_interpolation.h"
#include <algorithm>
#include <cmath>
extern "C" {
#include "functions.h"
#include "macros.h"
}

namespace {
bool Available(const char* path) {
    return path &&
           (ResourceMgr_FileExists(path) || (ResourceMgr_IsAltAssetsEnabled() && ResourceMgr_FileAltExists(path)));
}
constexpr float kPi = 3.14159265358979323846f;
} // namespace

extern "C" bool NeiHeld_HasResources(const char* opaque, const char* translucent) {
    return Available(opaque) && (!translucent || Available(translucent));
}

extern "C" bool NeiHeld_DrawModel(PlayState* play, const char* opaque, const char* translucent) {
    if (!play || !NeiHeld_HasResources(opaque, translucent))
        return false;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gDma1p(POLY_OPA_DISP++, G_DL_OTR_FILEPATH, opaque, 0, G_DL_PUSH);
    if (translucent) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gDma1p(POLY_XLU_DISP++, G_DL_OTR_FILEPATH, translucent, 0, G_DL_PUSH);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

extern "C" bool NeiHeld_DrawRod(PlayState* play, int element) {
    static const char* paths[] = { NEI_HELD_PATH("fire_rod"), NEI_HELD_PATH("ice_rod"), NEI_HELD_PATH("light_rod") };
    static const NeiGi::Kind kinds[] = { NeiGi::Kind::Fire, NeiGi::Kind::Ice, NeiGi::Kind::Light };
    if (!play || element < 0 || element > 2 || !NeiHeld_HasResources(paths[element], nullptr))
        return false;
    Matrix_Push();
    NeiHeld_DrawModel(play, paths[element], nullptr);
    constexpr float angle = 32 * kPi / 180;
    const float tip = (element == 1 ? 68.f : 66.f) * 4.f;
    Matrix_Translate(-std::sin(angle) * tip, std::cos(angle) * tip, 0, MTXMODE_APPLY);
    Matrix_RotateZ(angle, MTXMODE_APPLY);
    // Shared approved GI energy is authored in .78-scale GI space. Match it
    // to the held mesh's four native units per author unit, at the tip.
    constexpr float energyScale = 4.f / .78f;
    Matrix_Scale(energyScale, energyScale, energyScale, MTXMODE_APPLY);
    const auto camera = NeiGi_CameraBasis(play);
    NeiGi_DrawMesh(play, NeiGi::SampleOrb(kinds[element], camera), kinds[element]);
    NeiGi_DrawMesh(play, NeiGi::SampleEnergy(kinds[element], play->gameplayFrames, camera));
    NeiUsedMagic_DrawChargeFocus(play, element);
    Matrix_Pop();
    return true;
}

extern "C" bool NeiHeld_DrawMitts(Player* player, PlayState* play) {
    static const char* paths[] = { NEI_HELD_PATH("mitt_left"), NEI_HELD_PATH("mitt_right") };
    if (!player || !play || !NeiHeld_HasResources(paths[0], nullptr) || !NeiHeld_HasResources(paths[1], nullptr))
        return false;
    static const int hands[] = { PLAYER_BODYPART_L_HAND, PLAYER_BODYPART_R_HAND };
    static const int forearms[] = { PLAYER_BODYPART_L_FOREARM, PLAYER_BODYPART_R_FOREARM };
    Matrix_Push();
    for (int i = 0; i < 2; ++i) {
        const Vec3f& hand = player->bodyPartsPos[hands[i]];
        const Vec3f& forearm = player->bodyPartsPos[forearms[i]];
        const float dx = hand.x - forearm.x, dy = hand.y - forearm.y, dz = hand.z - forearm.z;
        Matrix_Translate(hand.x, hand.y, hand.z, MTXMODE_NEW);
        Matrix_RotateY(std::atan2(dx, dz), MTXMODE_APPLY);
        Matrix_RotateX(-std::atan2(dy, std::sqrt(dx * dx + dz * dz)), MTXMODE_APPLY);
        Matrix_RotateX(kPi * .5f, MTXMODE_APPLY);
        Matrix_RotateY(kPi, MTXMODE_APPLY);
        Matrix_Scale(.13f, .13f, .13f, MTXMODE_APPLY);
        NeiHeld_DrawModel(play, paths[i], nullptr);
    }
    Matrix_Pop();
    return true;
}

extern "C" bool NeiHeld_DrawGustJar(Player* player, PlayState* play, int direction, float heat) {
    const char* body = NEI_HELD_PATH("gust_jar");
    const char* band = NEI_HELD_PATH("gust_jar_band");
    if (!player || !play || !NeiHeld_HasResources(body, nullptr) || !NeiHeld_HasResources(band, nullptr))
        return false;
    Matrix_Push();
    const auto& left = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    const auto& right = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    Matrix_Translate((left.x + right.x) * .5f, (left.y + right.y) * .5f, (left.z + right.z) * .5f, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(kPi * .5f, MTXMODE_APPLY);
    Matrix_Scale(.22f, .22f, .22f, MTXMODE_APPLY);
    NeiHeld_DrawModel(play, body, nullptr);
    heat = std::clamp(heat, 0.f, 1.f);
    const uint8_t r = direction == 1 ? 180 + 75 * heat : 60 * (1 - heat);
    const uint8_t g = direction == 1 ? 40 * (1 - heat) : 120 + 60 * heat;
    const uint8_t b = direction == 1 ? 40 * (1 - heat) : 200 + 55 * heat;
    OPEN_DISPS(play->state.gfxCtx);
    // The band DL deliberately retains this primitive color and multiplies
    // its neutral porcelain relief by the original charge feedback color.
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, r, g, b, 255);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gDma1p(POLY_OPA_DISP++, G_DL_OTR_FILEPATH, band, 0, G_DL_PUSH);
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Pop();
    return true;
}
