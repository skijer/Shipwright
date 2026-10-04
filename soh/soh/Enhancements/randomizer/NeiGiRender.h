#pragma once

#include "NeiGiEffectPolicy.h"
#include "z64.h"
namespace NeiGi {
// Private RGBA32 resources use a 32x32 logical tile; resource metadata owns
// the high-resolution scale. Path and material must have static lifetime.
struct TextureMaterial {
    const char* path;
    bool repeatS = false;
    bool repeatT = false;
};
} // namespace NeiGi
extern "C" {

// Shared presentation primitives. The caller owns pose, origin and scale;
// these functions do not add GI rotation or optional pickup shimmer.
NeiGi::Basis NeiGi_CameraBasis(PlayState* play);
void NeiGi_DrawMesh(PlayState* play, const NeiGi::Mesh& mesh, NeiGi::Kind orb = NeiGi::Kind::Neutral);
// False queues nothing, so the caller can keep its geometry-only fallback.
bool NeiGi_DrawTexturedMesh(PlayState* play, const NeiGi::Mesh& mesh, const NeiGi::TextureMaterial& material);
}
