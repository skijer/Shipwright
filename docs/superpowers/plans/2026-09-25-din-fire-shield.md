# Din Fire Shield POC1 Implementation Plan

> **For agentic workers:** Execute inline with superpowers:executing-plans. The user authorized the animated code hook and paired archive on 2026-09-25.

**Goal:** Flames form a curved shield in front of Din's guarding arm, animate while held, and dissipate on release.

**Architecture:** Add a small C renderer called from the gameplay right-hand post-limb callback. A gameplay update advances formation/fade once per simulation frame. A separate O2R supplies private geometry and a private copy of the existing Fire POC7 texture; it overrides no player, equipment, or shared effect resources.

**Tech Stack:** Shipwright C23/F3DEX2, Python O2R tooling, compiled C fixtures using real engine headers/GBI.

**Spec:** User-approved in-chat arm-shield design; normal blocking and visual-only behavior. Code baseline `e1c78f8603d5bdb0e8f90516d86a5d095de3c15b` on `integration/nei-weather-static-actors`.

## Global Constraints

- Preserve the full integration and both original Din equipment archives unchanged.
- Scope POC1 to child Deku and adult Hylian shields in SoH with the matching existing Din bracer pack and Alternate Assets enabled.
- No collision, damage, item ownership, randomizer, body, sword, eye, weather, or audio changes.
- Feature checkbox defaults off. Missing resources, incompatible forms, hidden player, and disabled Alt Assets fail closed.
- Drawing must not change CPU matrices, player state, shared assets, or texture segments.
- Archive paths are private under `objects/din_fire_shield/poc1/`; existing TorchFlame and spin texture resources remain untouched.
- Implementation/build evidence is distinct from runtime acceptance.

## Review Focus

- Missing or unloaded assets: emit no commands and hold no cached pointers across frames.
- Alt toggle, age/scene change and respawn: discard an old shield's visibility immediately.
- Guard release versus loss of eligibility: only ordinary release receives a short fade.
- Pause and extra draws: animation advances with simulation, never draw count.
- Hand attachment and state restoration: use the real right-hand matrix and leave it balanced.

### Task 1: Guarded renderer and integration

Files: `soh/include/din_fire_shield.h`, `soh/src/code/din_fire_shield.c`, gameplay player init/update and right-hand draw call sites, `soh/soh/SohGui/SohMenuEnhancements.cpp`, `soh/tests/din_fire_shield_test.c`, `scripts/diagnostics/run_din_fire_shield_tests.py`.

Interface: `DinFireShield_Reset(void)`, `DinFireShield_Update(PlayState*, Player*)`, `DinFireShield_Draw(PlayState*, Player*)`.

- [ ] Compile a no-op implementation against a fixture that expects guarded drawing; observe the missing-draw assertion fail.
- [ ] Implement the update and draw functions. Use live resource resolution, local matrices, bounded scroll offsets, and translucent rendering.
- [ ] Verify default-off, missing pack/bracer, child/adult eligibility, guard/release, pause, repeated draw, hidden player, Alt-off, age change, transforms, and other-player rejection.
- [ ] Compile changed C translation units against actual headers; run cumulative regressions and preserve their outcome.

### Task 2: Standalone archive and offline visual check

Files: `scripts/assets/build_din_fire_shield.py`, archive validator, generated O2R and preview in the task output directory.

Interface: private `SurfaceDL`, `RimDL`, `FlameTex` resource paths; 64x32 I8 texture with tile masks 6/5 and 1/32-texel UV units.

- [ ] Build curved surface and feathered flame rim from deterministic vertex data. Copy/resample the Fire POC7 intensity texture into a new private texture; preserve source bytes.
- [ ] Validate every reference and vertex load, texture size/format, archive uniqueness, and absence of override namespaces.
- [ ] Render a labeled offline animated preview from output-archive vertices and texture, including formation, sustained flow, and fade. Inspect it before delivery.

### Task 3: Review and delivery

- [ ] Fresh code review of renderer, call sites, resources and fixtures; resolve correctness findings.
- [ ] Package the exact patch/source, paired archive, baseline IDs/hashes, verification record, installation steps, and narrow runtime checklist.
- [ ] Save deliverables and report honestly whether a full game build and runtime test were available.
