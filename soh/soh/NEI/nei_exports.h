/**
 * nei_exports.h - Not-Enough-Items C-linkage export declarations
 *
 * Single home for the small set of NEI symbols that must be declared with C
 * linkage so they can be linked across the C / C++ boundary (C mod TUs calling
 * into NEI C++ code, or low-level draw-wrapper TUs forward-declaring an NEI
 * symbol they cannot pull a full header for).
 *
 * This header only CENTRALIZES declarations that were otherwise re-declared
 * ad-hoc at the call site. The canonical/documented declarations still live in
 * each feature's own header (e.g. SwitchAge.h, pak_loader.h); including this
 * header alongside one of those is harmless — the signatures are identical.
 *
 * Keep this header dependency-light: only types already provided by z64.h.
 */

#ifndef NEI_EXPORTS_H
#define NEI_EXPORTS_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// Toggle Link between child and adult, reloading the scene with the opposite
// age. Implemented in soh/Enhancements/SwitchAge.cpp; the documented decl lives
// in soh/Enhancements/SwitchAge.h. Called from C mod TUs (item_time_gate.c) and
// C++ menu/QoL code.
void SwitchAge(void);

// Resolve a gSPDisplayList OTR path to a pak/skin/harpoon custom Gfx* override,
// or NULL to keep the vanilla display list. Implemented in pak_loader.cpp; the
// documented decl lives in mods/pak_loader/pak_loader.h. Consulted by the
// gSPDisplayList wrapper in GbiWrap.cpp.
Gfx* PakLoader_GetDLOverride(const char* otrPath);

#ifdef __cplusplus
}
#endif

#endif // NEI_EXPORTS_H
