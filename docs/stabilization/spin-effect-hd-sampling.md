# Spin-effect high-resolution sampling POC

Oversized replacements for the four spin-effect intensity textures need both
correct pixel decoding and the original sampling domain. Raw I8 upscales can be
read with a native-size row stride. Conventional V1 raw RGBA metadata fixes that
stride, but its non-unit scales activate the renderer's HD clamp to the actor's
8x8 tile-1 scroll window. That changes the pattern's repetition and scroll scale.

The SoH V0/V1 texture factories now delegate to libultraship, then normalize only
uniform integer upscales of these exact I8 paths, including their `alt/` forms:

| Resource under `objects/gameplay_keep/` | Native dimensions |
| --- | --- |
| `gTorchFlameTex` | 64x32 |
| `gEffUnknown1Tex` | 64x32 |
| `gFlameWall1Tex` | 32x32 |
| `gFlameWall2Tex` | 32x32 |

V0 I8 is expanded to owned RGBA with intensity in all four channels, preserving
transparent dark texels. Conventional V1 RGBA retains all authored bytes and its
original owning buffer. Both use `TEX_FLAG_LOAD_AS_IMG | TEX_FLAG_LOAD_AS_RAW`
with unit scales: whole-image upload uses physical dimensions, while native
loads and UV calculations keep their original sample domain. RAW also preserves
correct RGBA previews in the resource viewer.

Native-sized images, other paths or formats, nonuniform dimensions, dimensions
above 4096, and V1 replacements with explicit nonstandard flags/scales are left
to the existing factories. Truncated eligible payloads are rejected. All native
display-list references to this family use whole-image loads; custom display
lists that reuse these paths for partial loads are outside this POC's scope.

No actor geometry, display-list selection, cosmetic color logic, or libultraship
source is changed. Circular geometry and neutral materials remain in separate
asset archives. The POC archives already carry the whole-image metadata, so
they do not depend on this factory change to express that sampling behavior.
Their TorchFlame remains 64x32; this code also supports future larger TorchFlame
experiments without including one in the current asset candidates.

## Validation

Based on Shipwright `9a76eb637a4090279ccbcca14e5c043412ea766b` and libultraship
`c57da1b4afa775b24b58b2adf93d63d3b561bb65`.

`spin_effect_texture_test` compiles the real texture factories, resource types,
binary reader and memory stream. Tests cover all four paths, alternate paths,
RGBA channel/alpha preservation, source-row ordering, buffer lifetime, V1
metadata normalization, unchanged native/unrelated resources and truncated
payload rejection. The whole-image requirement failed before the sampling
correction and passed afterward. AddressSanitizer and UndefinedBehaviorSanitizer
passed in the local C++20 harness. LeakSanitizer was disabled because the executor
runs under ptrace. A CTest target is included and inherits libultraship's MSVC
runtime library setting.

Independent asset inspection passed for six candidate archives: circular radii,
closed seams, winding, triangle indices, 32-slot vertex cache limits, path hashes,
custom-resource headers, grayscale/intensity-alpha data, dimensions, primary RGB
combiner, runtime fade, and identical base/alt resources. The science pairs differ
only in their two flame-wall textures.

No full application build or in-game validation was performed locally. Runtime
acceptance requires both charge levels, charge/release/fade, full-primary-hex
color changes, moving-camera seam inspection, and comparison of native versus
128x128 flame walls with the actual active mod stack. Keep this as a candidate
until those checks pass.
