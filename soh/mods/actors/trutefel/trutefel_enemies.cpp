/**
 * trutefel_enemies.cpp - Host TU for trueffel/syeo501's three custom enemies (Skijer's NEI).
 *
 * Unity-#includes (inside ONE extern "C" block, same pattern as boss_remains.cpp):
 *   1. The compiled asset tables (assets/object_*_assets.inc.c): FlexSkeletonHeader +
 *      StandardLimb tables whose display lists are "__OTR__objects/trutefel/..." path
 *      strings (resolved at draw time by SoH's gSPDisplayList wrapper against
 *      trutefel-enemies.o2r in mods/), and AnimationHeaders with the frame data compiled
 *      in (SkelAnime takes the raw pointers directly).
 *   2. The three ported actors (actors/z_en_*.c): Miniblin (rupee thief), Hammergeist /
 *      Molmauk (ice+fire hammers) and the Scissors Beetle (boomerang pincers).
 *
 * Registration is NOT here: trutefel_actor_reg.cpp registers the three profiles with
 * ActorDB at boot (gated on trutefel-enemies.o2r being present) so the debug console can
 * `spawn En_Miniblin 0` etc. The actor ids live in gEnMiniblinId/gEnHammergeistId/
 * gEnSbeetleId (-1 while unregistered).
 *
 * NOTE: like boss_remains.cpp, this file must be added to the VS solution by hand
 * (soh.vcxproj); CMake builds glob it, but cmake regeneration is forbidden in this fork.
 */

// At GLOBAL scope, before the extern "C" block below: z64.h pulls in <memory> under C++, and a
// template cannot have C linkage. Getting it in first makes the include inside that block a no-op —
// boss_remains.cpp survives the same pattern only because its own header lands here first.
#include "z64.h"
#include <math.h> // sqrtf / fabsf, used by the ported actor .c files

// OPEN_DISPS / CLOSE_DISPS redeclare these two symbols inline at each call site; in a C++ TU that
// takes C++ linkage unless a C declaration exists at file scope. Force the C symbols (same trick as
// boss_remains.cpp / spiritual_stones.cpp) so the macro's redeclaration matches and links.
extern "C" {
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h" // ALIGN_ASSET used by the generated asset tables
extern SaveContext gSaveContext;
// gRupeeDL / gRupeeRedTex (OTR path symbols) for the Miniblin's stolen-rupee draw.
#include "objects/gameplay_keep/gameplay_keep.h"

// ── Compiled assets (skeletons + animations; meshes/textures live in trutefel-enemies.o2r) ──
#include "assets/object_miniblin_assets.inc.c"
#include "assets/object_hammergeist_assets.inc.c"
#include "assets/object_sbeetle_assets.inc.c"

// ── Ported actors (each defines its gEn*Id / gEn*StructSize globals) ──
#include "actors/z_en_miniblin.c"
#include "actors/z_en_hammergeist.c"
#include "actors/z_en_sbeetle.c"
}
