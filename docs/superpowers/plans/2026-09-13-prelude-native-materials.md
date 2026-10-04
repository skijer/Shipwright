# Prelude native material scrolling probe

Goal: restore native Lake Hylia, Kakariko Well, and Lost Woods light-sheet scrolling on recognized Prelude exports, independent of destination scene and replacement artwork.

Base: `integration/nei-weather-static-actors` at `9a76eb637a4090279ccbcca14e5c043412ea766b`. Work remains on `probe/prelude-native-materials` until runtime validation. ComboShip is downstream, after stabilization.

## Design and constraints

Use project metadata from the archive that supplies each display list. Water identity is the original `pastes[].chain[].path`. The supplied GLB recipes have no source chain; their stored original material labels survive the supplied texture replacement. Recognize only the exact Lost Woods material provenance in a single-material recipe. Do not inspect resolved texture names, hashes, pixels, dimensions, destination scene, or room to select a profile. Missing or ambiguous provenance leaves a resource unchanged. A future exporter should expose an explicit native-material identifier.

Wrap the existing binary display-list factory in Shipwright; do not modify libultraship. For recognized, straight-line F3DEX2 geometry with static material setup, insert a direct call to a stable per-profile scrolling list immediately before the first primitive. Reject existing nested/segmented calls, unsupported commands, and material changes after primitives. Preserve all original instructions, including texture hashes, vertices, colors, and render state. Direct native-list invocation avoids modifying shared Segment 08 entirely.

Generate each profile with the existing `Gfx_TwoTexScrollEx` routine and the exact native frame source, offsets, dimensions, and interpolation increments. Copy its commands into process-lifetime storage before graphics interpretation; never retain a frame-allocation pointer in a cached resource. Native scene/actor implementations remain unchanged. Per the user's follow-up, the probe is automatic by default: no console commands. `gEnhancements.PreludeNativeMaterialScroll` is only an optional troubleshooting override.

## Execution

- [x] Add failing focused tests in `soh/tests/native_material_scroll_test.cpp`: identity survives artwork replacement; unrelated and ambiguous materials stay untouched; payload words are not decoded as opcodes; native calls and complex lists stay untouched; each profile matches native parameters at wrap boundaries.
- [x] Implement pure metadata/command analysis in `soh/soh/Enhancements/Graphics/NativeMaterialProfile.h` and `.cpp`.
- [x] Add `PreludeNativeMaterialScroll.h` and `.cpp`: wrap binary factory, resolve provenance from owning archive, insert call, initialize/update native command buffers immediately after `OnPlayDrawEnd` in `z_play.c`. Refresh metadata from actual archive bytes on imports so archive reopening cannot retain stale profiles.
- [x] Change only binary display-list factory registration in `soh/soh/OTRGlobals.cpp`.
- [x] Validate the actual vanilla/retextured exports with the same production resolver and command analyzer, including 4K texture headers and byte-preservation checks.
- [x] Compile changed units where dependencies permit; run focused tests and existing stabilization tests. Record build/environment limitations without treating them as passes.
- [x] Report native call chains, metadata limits, empty geometry, retexture evidence, and remaining in-game acceptance tests. No permanent promotion before simultaneous profiles, room reloads, scene/age changes, and native-scene regression checks pass.
