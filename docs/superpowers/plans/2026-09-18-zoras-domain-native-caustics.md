# Zora's Domain native caustic motion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the existing Water Temple caustic material use Zora's Domain's native vertical motion, preserving the R3 surface balance and stationary stone.

**Architecture:** Append an explicit `ZorasDomainCaustics` profile and source `oot.zoras_domain.caustics` to the existing archive-owned material-motion contract. Adapt the native Domain segment 0C's vertical phase and rate to the material's logical 32×32 caustic tile, without executing its 64×32 base-tile setup. Reuse its active-water motion independently of age, preserving the user-confirmed animation in child and thawed adult Domain. Existing ice/thaw controls and other profiles remain unchanged.

**Tech Stack:** C++20 profile/importer, C gameplay draw hook, existing native F3DEX2 interpolation generator, Python ZIP authoring and diagnostics.

**Spec:** User approved implementation of native motion while independently testing R3's wall readability. Keep Djipi's artwork and R3's material commands except the approved environment-alpha increase; change the motion source. Source of truth is `func_8009E730` in `soh/src/code/z_scene_table.c`: active-water phase `127 - gameplayFrames % 128`, S step 0, T step -1. After the user tested R3 in both scene versions, the plan was corrected to avoid importing vanilla's adult-age stop into this reusable material. This inherits active-water motion, not the source scene's entire state machine. No geometry expansion or change to ice/thaw behavior is part of this pass.

## Global Constraints

- Preserve profile IDs 1–10 and existing metadata. Append Domain as 11.
- Only explicit `oot.zoras_domain.caustics` metadata selects this behavior; texture paths and destination scene do not select it implicitly.
- Keep the current five-field version-1 schema and logical dimensions 32×32.
- Animate only tile 1, using persistent command storage and the native interpolation generator.
- Preserve tile 0's authored dimensions, offsets, descriptor, and artwork.
- Retain R3's texture, additive shaded blend, shifts, alpha, fog, vertices, triangles, actors, collision, and scope. The user subsequently approved bundling a modest luminosity increase: environment alpha 95 → 112, preserving the full stone term.
- No generic raw-segment inheritance: other native sources need their own compatible adapter.
- Existing Water Temple, water, light-sheet, and fountain bindings retain their behavior at both ages.
- Generated game assets stay out of git. The original local feature checkout remains untouched; publish by fast-forwarding the remote feature branch.

## Task 1: Native profile and runtime integration

**Files:**
- Modify `soh/soh/Enhancements/Graphics/NativeMaterialProfile.{h,cpp}`.
- Modify `soh/soh/Enhancements/Graphics/PreludeNativeMaterialScroll.cpp`.
- Test `soh/tests/native_material_scroll_test.cpp` and `native_material_runtime_test.cpp`.

**Interfaces:**
- Preserve the existing `NativeScrollParameters(profile, stateFrames, gameplayFrames)` and C draw-hook interfaces; there is no new age or scene restriction.
- Domain metadata resolves to `NativeMaterialProfile::ZorasDomainCaustics`; the existing caustic command validator and tile-1-only copy apply to both caustic profiles.

- [x] Verify the existing focused runner passes on the isolated baseline.
- [x] Add a failing identity test for `oot.zoras_domain.caustics` resolving to profile 11. Exercise malformed version/size/extra fields and the same supported/rejected layouts as Water Temple.
- [x] Add parameter and actual-command tests at frames 0, 1, 126, 127, 128, 129, 2047, 2048, UINT32_MAX, deliberately differing gameplay and state clocks. Assert `x2=0`, `y2=127-f%128`, `dx2=0`, `dy2=-1`, dimensions32. Include literal float-bit checks at wrap, tile-1-only writes, independent buffers, reset/disable/null-context behavior, and unchanged other profiles.
- [x] Run tests to capture the intended failure before production changes. Baseline rejected `oot.zoras_domain.caustics`: the profile-11 assertion failed as intended.
- [x] Append the profile and recognize its source; share caustic validation. Implement the parameters:

```cpp
case NativeMaterialProfile::ZorasDomainCaustics:
    return {0, 0, 0, 127u - gameplayFrames % 128u, 32, 32, 0, 0, 0, -1};
```

- [x] Generate/copy only tile 1 for either caustic profile, without changing the draw hook. Run the focused tests and inspect the diff.

## Task 2: Authoring, diagnostics, probe, and delivery

**Files:**
- Modify `scripts/build_water_temple_caustics.py`, `scripts/diagnostics/run_native_material_probe.py`, and `soh/tests/test_water_temple_caustics.py`.
- Update `docs/stabilization/water-temple-caustics.md`.
- Create the R3-based vertical probe and user guide outside the repository.

**Interfaces:**
- Builder adds `--motion-source` with choices `oot.water_temple.caustics` (unchanged default) and `oot.zoras_domain.caustics`. The chosen source changes metadata only, not compiled material/image/wrapper paths or bytes.
- Diagnostic adds `--require-zoras-domain-caustics`, asserting profile 11 and a valid insertion. Existing `--require-water-temple-caustics` continues to assert profile 10.

- [x] Add failing builder tests: identical material and texture bytes for either motion source, selected metadata source survives round trip, unsupported source fails before writing; default output remains compatible.
- [x] Implement the source option and diagnostic checks, then run the existing Python suite and focused C++ runner.
- [x] Derive R4 from R3 by changing only `nativeAnimation.source` and the environment-alpha command from 95 to 112. Assert ZIP integrity, identical resource set, exactly one changed material command, all other resource bytes unchanged, and original archive hash unchanged.
- [x] Verify that actual archive resolves profile11/insertion31 through the production export probe. Run the surface-preservation checks and bound RGB using the supplied texture pixels and vertex colors; ensure the added strength cannot darken stone or introduce clipping for these assets.
- [ ] Document the required new build, install one probe at a time above Djipi, age behavior, current single-material scope, and pending in-game validation.
- [ ] Review and commit code/docs, fast-forward the existing remote feature branch, verify one new build run was triggered, and deliver the saved probe/guide. Do not wait for the full platform compilation.

## Validation commands

```sh
python3 -m unittest discover -s soh/tests -p test_water_temple_caustics.py -v
python3 scripts/diagnostics/run_native_material_probe.py \
  --json-include /path/to/nlohmann/include --spdlog-include /path/to/spdlog/include \
  --require-zoras-domain-caustics /path/to/vertical-probe.o2r
git diff --check
```
