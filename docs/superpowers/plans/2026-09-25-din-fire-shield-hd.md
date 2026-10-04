# Din Fire Shield 256px Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan inline.

**Goal:** Deliver a bounded 256×256 core, rim, and inventory-icon POC with upward-oriented flame flow, the existing bold shield silhouette, and opt-in Shield SFX.

**Architecture:** Keep native display-list texture coordinates, tile sizes, and animation speed. Bind aligned OTR texture names so libultraship receives the archive's physical size and whole-image RGBA metadata. Rebuild only private shield textures and the requested UV/core-opacity data from immutable POC1.

**Tech Stack:** C, existing GBI commands, libultraship texture V1, Python/Pillow, o2r ZIP resources.

**Spec:** The user requested fresh readable core flames, vertical scrolling, a recognizable bold border, 256×256 maps, and a 256×256 inventory icon. This paragraph is the complete bounded spec; in-game acceptance remains a later user judgment.

## Global Constraints

- Parent code: d54391b55d8d8945cc4b00ffabae17d6677c333c on integration/nei-weather-static-actors.
- Parent archive SHA256: a479e5a736b6b6ea02ea1a2f60808a80c18b0fc81dee5fe297b0a1942a445869.
- Preserve geometry positions, rim alpha, bracer model, display lists, attachment, cosmetic entries, eligibility, blocking, and cumulative branch work.
- Generated square fire sources are new art; the icon derives from its original full-resolution artwork.
- Native POC2 was not delivered or accepted. This HD candidate starts from POC1.
- No master promotion or claim of gameplay verification.
- User steering: add default-off "Shield SFX", visible only after enabling Din Fire Shield, using the Fire Arrow charge flame sound while guarding. Investigate a matching fire sword; do not impose a 256px renderer cap.

## Review Focus

- A raw pointer bypasses resource metadata: inspect emitted texture-image packets and returned icon handles.
- Odd-address strings bypass the interpreter signature check: require alignment of all three handles.
- Missing or failed resources must retain the existing safe fallback.
- HD resources must upload all 256×256 pixels while retaining native UV periods; use IMG|RAW with unit scales and RGBA intensity alpha.
- Rotation must change the hand-local scroll axis without altering geometry; held direction and menu appearance need runtime confirmation.

## Task 1: Preserve HD metadata at the rendering boundary

**Files:** `soh/src/code/din_fire_shield.c`, `soh/tests/din_fire_shield_test.c`.

**Interfaces:** Existing public APIs unchanged. Renderer texture-image commands and icon API return aligned `__OTR__objects/din_fire_shield/poc1/` names.

- [x] Compile and run the existing renderer fixture. Expected: passes.
- [x] Add assertions on actual G_SETTIMG packets for FlowTex and FlameTex, and icon handles for both shield items. Require `(uintptr_t)handle % 2 == 0` and exact OTR names; also check failed icon loading returns NULL.
- [x] Run `python3 scripts/diagnostics/run_din_fire_shield_tests.py`. Expected: old code fails the OTR-handle assertion.
- [x] Add three aligned resource-name handles. Keep resource load probes. Pass texture handles to the two load macros; return the icon handle only after successful loading.
- [x] Rerun the fixture. Expected: all prior and HD contract checks pass.

## Task 2: Build and inspect the matching HD asset candidate

**Files:** New `scripts/assets/build_din_fire_shield_hd.py`; extend existing archive validator and preview reader for V1 whole-image RGBA.

**Interfaces:** Builder consumes immutable POC1 plus three PNG masters, emits an isolated archive with the same nine private resource paths. Physical textures are 256×256; flame logical coordinates remain 64×32 and the icon remains 32×32.

- [x] Encode both grayscale maps as RGBA with R=G=B=A=intensity, bottom-up rows for positive-T movement. Encode the original icon as 256×256 RGBA.
- [x] Use resource V1 headers, flags=3, HByteScale=VPixelScale=1, complete 262144-byte RGBA payloads. Preserve opaque black gaps as zero intensity/alpha.
- [x] Rotate UVs to S=(Y+1000)*1.024, T=(X+1000)*0.512; preserve all geometry and border alpha. Core alpha: 215/215/205/0.
- [x] Independently validate resource formats, full payload lengths, hashes, dependencies, unchanged geometry, and unchanged display lists.
- [x] Render actual archive pixels and UVs against light/dark backgrounds, clearly labeled offline. Expected: more legible flame detail with existing border shape.

## Task 3: Verify and hand off

- [x] Add failing lifecycle tests for opt-in shield sound, then request Fire Arrow's sustained sound once per eligible guarding update. Preserve the player's existing audio slot. Add the conditional "Shield SFX" menu checkbox.
- [x] Run `bash scripts/diagnostics/run_cumulative_regressions.sh`. Expected: all 26 required baselines retained and existing suite passes with any known skipped checks recorded.
- [x] Fresh review of the renderer change and archive contract; resolve important findings.
- [ ] Push the reviewed code as a fast-forward of the current cumulative branch, using the existing push authorization. Confirm exact parent and tree; no forced update.
- [ ] Package candidate, source artwork, reproducible builder, evidence, and install instructions. Save the deliverable.
- [ ] Report new-build requirement and actual CI state. Decisive runtime check: flame direction/detail while guarding and 256px icon in the equipment menu.
