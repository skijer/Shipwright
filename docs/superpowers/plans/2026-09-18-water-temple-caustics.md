# Reusable Water Temple caustic material

**Goal:** Restore the Water Temple's scrolling, vertex-shaded caustic look on explicitly selected Djipi Zora's Domain surfaces, and make the material reusable on unrelated geometry such as the Lost Woods pool.

**Architecture:** Extend the existing native material importer with an explicit Water Temple caustics profile. A material-only display list can bind the caustic animation before its caller draws any geometry. The base texture stays on tile 0, with its authored dimensions, UVs, and artwork; the caustic uses logical 32x32 tile 1. Generate the interpolation commands with the native engine function and copy only the tile 1 update into stable storage. An asset overlay replaces one selected XML material with a call to this native binary material, preserving every vertex, triangle, room header, actor, and collision resource.

**Tech stack:** Existing C++20 native material bridge, F3DEX2/libultraship GBI, Python asset tooling, ZIP O2R overlays; MPQ input through mpyq.

**Spec:** User approved code plus assets and reuse on different geometry/artwork. The initial material probe is `mat_spot07_room_1DL_004200_ZoraDomain00`. This is a targeted first implementation, not a blanket conversion of every wall or proof of final in-game appearance. The Water Temple reference is `func_8009B0FC` opaque segments 08/09/0A and `MIZUsin_sceneTex_014430`. This is a vertex-shaded texture effect, not dynamic light projected onto actors. Base/caustic roles are reversed relative to the donor so environment alpha 95 gives the donor's wet 160/255 base weighting. Existing Djipi vertex colors remain authored as-is.

## Global Constraints

- No geometry identity, source texture name, destination scene, or age requirement for an explicit caustic binding.
- Preserve existing Lake Hylia, pool, Lost Woods light sheet, and MM fountain behavior and metadata compatibility.
- Do not modify the Zora's Domain ice/thaw console command, collision, actors, room headers, Water Temple water-level state, or scene segment assignments.
- Animate only tile 1; do not replace tile 0's authored dimensions or offsets. Logical caustic size is 32x32 regardless of replacement image resolution.
- Metadata uses `{"version":1,"binding":"material-motion","source":"oot.water_temple.caustics","logicalWidth":32,"logicalHeight":32}` under `nativeAnimation`.
- Reuse the owning archive's metadata and stable native display-list storage. Unsupported or ambiguous command layouts must remain unbound.
- Keep copyrighted input archives and generated game assets out of git. Deliver only the small overlay separately, with reproducible source tooling and truthful validation limits.

## Preflight

| Task | Entry point | Output | Dependency |
|---|---|---|---|
| 1 | Existing native importer and tests | Explicit profile; safe material-only lists; tile 1 native interpolation | Current fork 61bf5bad |
| 2 | Explicit XML material and vanilla caustic texture | Reusable material authoring tool; one Zora's Domain overlay; usage guide | Task 1 metadata and importer contract |

The fresh isolated checkout is on `feat/reusable-water-temple-caustics`, based on `61bf5bad25fe2300b43c779b6c65047db08391c6`. Baseline diagnostic tests pass. No repository AGENTS.md applies. Dependencies for the focused runner are `/path/to/nlohmann/include` (nlohmann) and `/path/to/spdlog/include`; libultraship is initialized.

## Task 1: Native caustics profile and material-only binding

Work from the repository root on the existing feature branch. This task owns only the native bridge/profile and their C++ tests. Do not spawn subagents. Follow TDD, commit the result, and write the requested report.

Files:
- Modify `soh/soh/Enhancements/Graphics/NativeMaterialProfile.h` and `.cpp`.
- Modify `soh/soh/Enhancements/Graphics/PreludeNativeMaterialScroll.cpp`.
- Modify `soh/tests/native_material_scroll_test.cpp` and `soh/tests/native_material_runtime_test.cpp`.

Requirements:
1. Append `WaterTempleCaustics` before `Count`, preserving current numeric IDs. Explicit metadata is exactly `{"version":1,"binding":"material-motion","source":"oot.water_temple.caustics","logicalWidth":32,"logicalHeight":32}`. Reject malformed types, versions, extra fields, and any other size. It resolves independently of provenance, scene, geometry, and retexture paths for both pasted and non-pasted items. No automatic recognition.
2. Bound all fountain range checks to the final fountain enum, so the new profile cannot be mistaken for a fountain.
3. Support a `materials` array alongside `pastes` and `shapes` within the existing `edits -> scene -> edit -> data` metadata structure. A materials entry has `newDlPath` under `custom/prelude/` and explicit `nativeAnimation`; material entries lacking explicit metadata must not fall back to shape recognition. Keep owning-archive lookup, caching, and duplicate conflict behavior.
4. For this profile only, `FindNativeScrollInsertion` accepts a complete material-only display list, inserting immediately before its terminal ENDDL if there are no triangles. It also supports the existing flat material-plus-triangles layout, inserting before the first primitive. Continue to reject nested calls, branches, segment writes, prebound interpolation, truncated hash payloads, nonterminal ends, and material changes after drawing. Existing profiles still require triangles.
5. Validate caustic render setup: exactly one size and render descriptor each for tiles 0 and 1; tile 0 must use TMEM 0 and may have arbitrary valid authored size/offset/masks/shifts; tile 1 must have a zero-origin 32x32 tile size, RGBA16 format, line 8, nonzero TMEM (the builder uses 256), wrap in S/T, masks 5. Its shifts may be authored (donor default S=3,T=0). Permit repeated load tile 7 setup. Fail closed on aliased TMEM, missing/duplicate render setup, or unsupported tile numbers. Do not constrain the base texture to a particular name, resolution, or mesh. Material colors/combine/render mode remain asset-owned.
6. `NativeScrollParameters` for this profile is base offset/step zero, caustic x=`gameplayFrames`, y=0, dimensions 32x32, caustic step (+1,0). This matches donor opaque segment horizontal motion with texture roles swapped. Do not import water-level alpha state.
7. In the frame update, generate commands using existing `Gfx_TwoTexScrollEx`, then for this profile copy only its tile-sync, tile 1 interpolation command (all five command words), and end into the stable profile list. Never execute a tile 0 update, environment-color write, or scene-segment write. Keep default-enabled `gEnhancements.PreludeNativeMaterialScroll`, disable/null-context termination, reset, and frame-allocation independence.
8. Behavioral tests: valid metadata for arbitrary materials/geometry; malformed/64x64 rejection; material-only and flat draw acceptance; existing profiles reject material-only; base dimensions 64x64 and 256x256/nonzero tile offset survive; tile 1 missing/bad dimensions/masks/alias rejected; hidden opcodes in hash payload not interpreted; all earlier profile regressions; actual native generated tile-1 command values across 0,1,127,128,2047,2048,UINT32_MAX with different state/gameplay clocks; stable lifetime, null/disable/re-enable, and materials metadata refresh/conflicts.

Run focused tests via:
`python3 scripts/diagnostics/run_native_material_probe.py --json-include /path/to/nlohmann/include --spdlog-include /path/to/spdlog/include`

Record RED evidence before implementation and GREEN output after. Tests should evaluate real command generation/import decisions; do not write source-text assertions. Check `git diff --check`, self-review, and commit only the task's files. Do not upload assets or modify build workflows.

## Task 2: Reusable material authoring tool and Zora's Domain probe

Work from the repository root, after Task 1 passes review. Do not spawn subagents. Follow TDD for binary encoding/asset invariants, commit source code and documentation only, and write the requested report.

Files:
- Create `scripts/build_water_temple_caustics.py` (tool/CLI; split binary encoding into a small helper only if clearly needed).
- Create `soh/tests/test_water_temple_caustics.py` with meaningful command/round-trip/overlay invariants, no bundled copyrighted fixtures.
- Create `docs/stabilization/water-temple-caustics.md`.
- Update `scripts/diagnostics/run_native_material_probe.py` to inspect entries in `materials` as well as existing kinds.

Requirements:
1. Authoring targets an explicitly selected single-base-texture XML material resource, not recognized geometry or a hardcoded destination. Provide CLI arguments for source scenes archive (.otr MPQ or .o2r ZIP), vanilla archive (for the donor caustic texture), selected material path, output overlay path, and optional caustic strength/shift S/T. Default source is `scenes/shared/spot07_scene/mat_spot07_room_1DL_004200_ZoraDomain00`; defaults strength=95, shift S=3/T=0. Default names may be Zora-specific, but another selected compatible material must work without any geometry match. Validate inputs and report unsupported layouts clearly.
2. Compile the selected material's supported XML commands to a binary native material under a deterministic `custom/prelude/materials/` path. Preserve its base texture reference, tile-0 descriptor/size/offset, texture scale, render state, and geometry flags except necessary two-cycle/vertex-shade caustic combine. Refuse existing multi-texture/control-flow/material layouts that cannot be safely converted; don't silently flatten or replace geometry. Fail closed for unsupported XML attributes/opcodes. Encode using the actual libultraship GBI conventions: header 64 bytes, microcode byte + padding to 72, little-endian uint32 word pairs; texture hashes carry separate payload command; use checked CRC64 compatible with StrHash64. Demonstrate representative command encoding against actual GBI macros via a focused compiler fixture, not only a duplicate Python implementation.
3. Add the native Water Temple caustic texture (32x32 RGBA16) copied from `scenes/nonmq/MIZUsin_scene/MIZUsin_sceneTex_014430` under a custom path. Load it to nonzero TMEM 256, render tile 1, line 8, wrap/masks5, authored shifts, static zero-origin32x32size. Base stays tile0. Use donor two-cycle shaded blend with swapped texture weighting: `(base + (caustic-base)*envAlpha) * SHADE`, environment alpha 95 by default, RGB0. Preserve appropriate alpha/render state for the selected material; document any initially unsupported alpha/blend modes. Do not put scene segments into generated material.
4. Overlay only overrides `alt/<selected material path>` with a tiny XML wrapper calling the custom binary material. Include the custom material, its caustic texture, and `prelude/project/edits.json` with a `materials` entry carrying the exact nativeAnimation contract. Do not override parents, vertices, triangle lists, culling, room headers, collision, actors, base textures, or original input archives. Preserve the existing source alpha behavior for supported opaque defaults; don't claim arbitrary translucent alpha already handled if it isn't.
5. Build `/path/to/deliverables/Zoras_Domain_Water_Temple_Caustics_Probe.o2r` from the provided uploads. Also write a concise manifest/report there with source SHA256s, exactly what resources the overlay changes/adds, selected material, native profile, tint/strength choices, unsupported layouts, and validation limits. Do not commit generated assets or private upload paths to source docs. No copied full input archive.
6. Generic material-only design is reusable in any room: document how an authored XML material calls the generated native material before arbitrary triangles, or how a Prelude exporter writes the explicit materials/pastes/shapes binding. Different UVs/vertex colors need visual tuning but are not geometry eligibility conditions. Current support is the opaque Water Temple caustic layer, not a third texture added to an existing two-texture water shader. The Lost Woods pool geometry is not provided in the current three inputs, so don't claim that room is patched; document precise reuse steps.
7. Explain installation priority (overlay must win over Djipi's scenes material), required feature build/branch, existing alt-assets/pack setting, existing native scroll CVar, child and thawed adult verification, existing thaw command remains in control, scene reload as appropriate, and removal of overlay as rollback. Original assets on an older build display a static caustic material; motion requires new code. This is CPU/asset verified, not visually verified in-game. Existing artist vertex colors are preserved; copying donor vertex colors blindly would not recreate authored lighting on another mesh.
8. Verify real generated overlay through the actual production native profile/export probe; assert it resolves as WaterTempleCaustics with a valid insertion index. Verify source archives SHA unchanged and exact overlay allowlist. Run Python focused tests plus the C++ runner with this overlay and `git diff --check`. Generate user-facing artifacts outside git; parent will save them and publish branch.

Input archive roles (substitute the local confidential input paths):
- Vanilla donor archive: `/path/to/vanilla.o2r`
- Djipi scenes archive: `/path/to/scenes.otr`
- Djipi textures archive: `/path/to/textures.otr`

Extracted material/source references are under `/path/to/private-investigation/`. Actual libultraship source under `libultraship/src/fast/resource/factory/DisplayListFactory.cpp`, GBI headers and `StrHash64.h` are authoritative for compiler encoding. mpyq is installed. User-supplied files are confidential inputs; tests use generated/synthetic fixtures.
