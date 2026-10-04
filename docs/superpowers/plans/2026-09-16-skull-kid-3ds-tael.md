# Skull Kid 3DS model and Tael implementation

Approved scope: use the supplied Skull Kid 3DS mesh, HD diffuse textures and ocarina for the existing masked static actor. Preserve both native floating animations, actor placement parameters and scene-reentry protections. Correct his companion to purple Tael. Hidden face blinking is out of scope.

Additional user direction: raise only Skull Kid's arms-crossed pose so its feet clear the placement floor, add smooth whole-body turning toward Link in both poses, and normalize Lulu's look-left presentation facing while retaining its gesture. Other pose heights and finished models stay unchanged.

## Resource contract

- Keep the native 21-limb skeleton and native animations from `mm.o2r`.
- A separate optional O2R supplies a complete replacement display-list graph under `objects/object_stk_3ds/v1/`.
- Replacement root names mirror the native Skull Kid limb, head, eyes and mask display-list basenames. All referenced vertices/textures are in that same namespace and archive.
- The selected pack must validate completely before any replacement pointer is published. Missing or invalid packs leave the entire original appearance active.
- Loading keeps resource owners, patched display lists and private texture aliases alive across cache clearing and scene transitions; alternate assets disabled selects the original appearance.
- Model conversion fits 3DS joints to the native rest skeleton and exports rigid vertex ownership suitable for the engine's matrix palette. Preview the exported data in both native animations.

## Work items

- [x] Trace current native skeleton, strict graph loader and companion color commands.
- [x] Add regression coverage for complete-pack selection, invalid-pack fallback and resource lifetime before changing the adapter.
- [x] Extend `mm_asset_loader.cpp/.h` and `z_en_viewer.c` to select the complete pack atomically, preserving the strict original fallback.
- [x] Verify Tael colors against the original MM actor source and correct the actual primitive/environment commands; test the emitted commands.
- [x] Convert the supplied skinned DAE mesh, omit its flute, retain the mask, fit the ocarina to the appropriate hand and apply the supplied matching HD textures.
- [x] Export an O2R; verify all graph references, vertex ranges, matrix slots, texture dimensions/scales and ZIP CRCs.
- [x] Render front/side views and both animation loops from exported resources, including ocarina clearance checks.
- [x] Verify the crossed-arms floor bound across the full native loop; add only that pose's anchor and test the independent hover trough.
- [x] Add smooth body-yaw tracking with a return to placement yaw, and verify Lulu's local facing correction across the full look-left clip.
- [x] Build changed production code and run relevant existing and new lifecycle fixtures; review changes before publishing.
- [ ] Publish the fork update/build and save the mod, source recipe and preview for the user. Clearly distinguish offline validation from in-game testing.

## Verification focus

No loose per-resource overrides across different archives. No reuse of scene-owned pointers after release. No missing matrix-slot data when drawing shared-joint triangles. Preserve source diffuse/alpha layouts and the engine's raw RGBA byte scaling. The ocarina remains attached through both full pose loops. Test alternate-assets on/off plus cache eviction/reentry, not only a fresh debug warp.

## Approved preview and final checks

The user approved the R2 preview after correcting each foot's quarter-turn rest-frame mismatch. The converter changes only the affected lower-leg and boot vertex positions/normals; both native animation clips and upper-body geometry remain unchanged. Runtime confirmation is still required.

The matching separate mod is `Skull_Kid_3DS_HD_Ocarina_R2.o2r`, SHA-256 `2b341ac375d3ee02ef46c5e91d706bf63d7748218454db13258ba06e103b3a64`. Install it in the build's existing `mods` folder, keep the required original `mm.o2r`, and enable Alternate Assets. It contains no replacement skeleton or animation tracks.

Final local verification covers all 15 configured CTests, compilation of the four changed production objects, the compiled production viewer/texture/lifecycle fixtures, Tael's actual draw colors, both full native animation loops, and Lulu's facing correction. The actual R2 archive passes eight Alternate Assets/cache-reentry cycles with all resource owners retained, plus incomplete/corrupt-pack fallback. These headless checks do not substitute for an in-game scene transition test.

Runtime checks: enter a scene normally, leave and re-enter repeatedly, approach the actor from outside draw range, check both poses and the arms-crossed floor clearance, toggle Alternate Assets, and confirm Tael's colors and body turning. Check Lulu and Treasure Chest Shop Gal on scene re-entry as well.
