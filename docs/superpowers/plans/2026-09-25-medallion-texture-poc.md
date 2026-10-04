# Independent Medallion Arrow Textures Implementation Plan

> **For agentic workers:** Use the existing parallel task workflow for the bounded code task and root-owned artwork. Complete the implementation and review without another permission pause; the user requested the entire set together.

**Goal:** Give Shadow, Water, Forest and Spirit arrow overlays independent optional texture pairs while preserving the accepted Fire and Ice snowflake work; deliver all six arrow looks plus Din’s Fire together.

**Architecture:** Keep native Fire/Ice/Light material and model display lists. After the normal material is applied, optionally reload just its two TMEM image slots with a complete private Alt texture pair. Do not reset render-tile shifts, masks, wrap/mirror, combiner or colors. Existing scroll and geometry commands continue unchanged.

**Tech Stack:** C, F3DEX2 display lists, SoH named resources, Python archive packaging and focused C fixtures.

**Spec:** User’s current conversation: Henriko Fire remains readable/accepted; dramatic Light and Din’s Fire; Shadow, Water, Forest and Spirit remain in scope. Shared-pair facts are documented in the 2026-09-25 Magic Overlays Texture Audit. The user subsequently expanded this grouped POC to the Forest, Fire, and Water medallion cast spells. Their independent cast resources also receive optional private texture bindings. Light, Shadow, and Spirit cast effects remain unchanged. The user corrected the requested element to Water medallion, not Ice Arrow; Water’s underlying SW97 ArrowIce actor gets private water artwork, while native Ice Arrow assets and its snowflake remain unchanged.

## Global Constraints

- Baseline: Shipwright `5a8b8c6f3999b55f473b88c0c078663e3760e91f`; protect cumulative integration and pinned submodules.
- Preserve native and Alt behavior when the private pair is absent or incomplete, and when Alt Assets is off.
- Private resources: `custom/medallion_magic/arrows/{shadow,water,forest,spirit}/s1Tex` and `s2Tex`, authored as Alt resources; named references retain `__OTR__`.
- Native logical texture domain: 32×64 I8 for both image slots; TMEM 0 and 0x100; retain existing render tile descriptors.
- Fire bytes remain the recovered Henriko POC1 bytes. Native Ice texture bytes and accepted charge/impact styling remain unchanged. The later release-continuity and optional impact-audio additions below explicitly extend this initial scope; gameplay and color controls remain intact.
- The four paired arrows retain their current fallback native material/model references.
- Root owns asset generation and packaging outside the repository. No arbitrary image or generated binary is added to the game code repository.

## Review Focus

- Alt Assets changes while an arrow is active: resolve eligibility each draw, no stale cached choice.
- Partial private pair: choose neither replacement and retain both native images.
- TMEM slots: update exactly the two image loads without overwriting render tile settings or color state.
- Native resources: no mutation of shared material resources, geometry, timing or damage. Independent cosmetic colors and requested audio changes are applied at their existing draw/event points.
- Missing pack: all four fall back to their previous Fire/Ice/Light paths; native Ice snowflake styling stays intact, with the subsequently requested flight continuity.

## Task 1: Private texture pair hook

**Files:** Create a small shared header under `soh/expansions/sw97/actors/arrows/`; modify only `z_arrow_dark.inc.c`, `z_arrow_ice.inc.c`, `z_arrow_wind.inc.c`, `z_arrow_soul.inc.c`; add a focused C test and diagnostic runner; register that runner in the cumulative gate.

**Interface:** A helper takes the current `Gfx*` plus two named private texture references and returns the advanced `Gfx*`. It returns the same pointer when Alt is off or either resource is absent. Call it after the existing native material display list and before the existing texture-scroll list.

- [x] Write a focused regression fixture that runs production helper code and captures emitted commands: eligibility, absent/partial pairs, mid-run toggles, named pointers, 32×64 load size, TMEM destinations, and absence of render-tile/color/combiner/geometry changes.
- [x] Run the fixture before implementation and confirm the missing behavior fails.
- [x] Implement the helper with load-tile-only commands; preserve both render-tile descriptors. Wire Shadow/Water/Forest/Spirit to the exact resource roots above.
- [x] Run the fixture and existing ice snowflake test; clang-format touched C/C++ files with version 14.
- [x] Review native actor timing, damage and geometry preservation; colors and sounds must change only at the explicitly requested controls/events.

## Task 2: Full texture candidate

**Files:** Root-owned `/workspace/scratch/0f82c27c79c6/dins_fire_poc3/` artifact workspace.

- [x] Finish independent Shadow, Water, Forest and Spirit images; preserve Henriko Fire and completed Light/Din’s art.
- [x] Package each private pair at the agreed paths, with intensity in alpha and explicit whole-image upload metadata; preserve native dimensions for UV accounting.
- [x] Validate exact archive scope, hashes, dimensions, alpha, and fallback dependencies. Execute the focused renderer sampling probe against all packaged image profiles.
- [ ] Deliver one grouped candidate package and comparison previews, with runtime status stated honestly and the shared-branch build link for the resource hook.

## Task 3: Integration and review

- [x] Independent review of the hook, asset paths and dependency contract.
- [x] Run the required cumulative gate once after final changes; inspect formatting and final diff.
- [ ] Push the bounded hook together to the existing authorized integration branch, preserving its current remote head, and provide the associated build.
- [ ] Save user-facing artifacts and hand off a focused in-game check. Static success does not constitute runtime acceptance.

## Scope addition: Forest, Fire, and Water cast spells

The user explicitly requested these three cast effects on 2026-09-25. Preserve current skeletons, geometry, animation, timing, colors, gameplay, and audio. Root owns artwork and packaging; the delegated cast-hook task owns only those three actors, the isolated Forest dust selector, and its focused tests.

- Fire: optional complete pair at `custom/medallion_magic/spells/fire/{s1Tex,s2Tex}`, logical64×64I4.
- Water: optional `custom/medallion_magic/spells/water/sTex`, logical64×64I8, reused in both samplers.
- Forest initial cast: optional `custom/medallion_magic/spells/forest/sTex`, logical64×64I8, reused in both samplers.
- Forest lingering tornado: isolate its dust particles with a reserved draw-flag bit and optional complete private `dust1Tex` through `dust8Tex` set; ordinary dust stays unchanged. Verify native dust dimension/format before packaging.
- Native material-plus-model lists call dynamic scroll lists after material and before vertices. A bounded optional wrapper can reload only the image slots and then invoke the original scroll list. It must not rewrite render-tile descriptors or shared display lists.
- Alt off, missing/incomplete archive, and mid-effect toggles must retain their original fallback behavior.
- Cumulative checks cover both arrow and cast hooks; one grouped source push and one grouped texture deliverable. Runtime acceptance remains untested until a game comparison.

## Subsequent user steering

- Cosmetic Editor → Effects → Magic Effects: add independent primary/secondary colors for Fire, Water, and Forest medallion cast spells. Retain all native alpha/timing and distinct limb defaults on reset; color controls work with Alt Assets off or without the texture pack. Forest particle colors update live for only its tagged particles. Correct the existing Din’s Primary CVar typo so its existing editor entry responds.
- Water medallion bow/slingshot charge should visibly flow as water, and its icy SFX should become native flowing-water charge / water-splash impact sounds. Preserve sound trigger timing, stop/cleanup and all gameplay. Native Ice Arrows remain unchanged.

- Add all six medallion arrow primary/secondary color pairs to Effects → Arrow Effects, independently from native arrow controls and medallion cast controls. Reset preserves existing RGB defaults; alpha and overlay geometry/timing remain unchanged. The shared medallion actors serve both bow and slingshot.

- Latest request explicitly expands the existing HD texture importer normalization to decorative flame, Din’s Fire, and the arrow/spell overlay resources. Preserve the native load/UV domain; safely handle legacy V0 intensity and conventional V1 RGBA replacements with verified native dimensions. Include the private resources and retain all previous TorchFlame behavior. This replaces reliance on special candidate metadata alone.
- User reported on the newest build they were testing that fire interactions work (2026-09-25 16:24 America/Chicago). Build SHA/configuration was not supplied; record as user-reported evidence only, not acceptance of this unpushed texture candidate.
- Native Fire/Ice/Light impacts intentionally call `NA_SE_IT_EXPLOSION_FRAME`, `NA_SE_IT_EXPLOSION_ICE`, and `NA_SE_IT_EXPLOSION_LIGHT`; the subsequently approved opt-in setting preserves these IDs when off. Water medallion independently uses the newly requested flowing-water/splash cues.

- User’s first clip shows native Ice Arrow’s cyan flight mesh popping in around 6.383s when the custom snowflake path excludes Fly. Extend only the custom-enabled visual path through flight; preserve native fallback and accepted charge/impact animation. Add production lifecycle regression coverage.
- Add default-off “Elemental impact sounds” in Effects. Opt-in replaces Fire/Ice/Light impact SFX only with native ignition/ice break/light-hit samples at existing impact calls (and corresponding SW97 Fire/Light). Native defaults and all charge/shot timing remain.
- User added Demise’s Destruction lightning from an ALttP Quake reference clip (second attachment). Visible reference behavior: short purple jagged bolts around the slam, spreading across the ground. Add isolated spell-owned purple/black lightning alongside its existing explosions; preserve animation/timing/cost/damage and unrelated effect consumers.
- User explicitly approved the default-off elemental impact SFX checkbox at 16:40 America/Chicago.
- User explicitly requires medallion bow/slingshot appearance continuity through charge, release, flight and impact at 16:46 America/Chicago. Verify all six production draw/lifecycle paths so the active custom textures cannot silently revert to native textures on release; retain deliberate Alt-off/missing-resource fallback.

## Source verification before grouped integration

Final cumulative gate exited 0; changed-file clang-format 14.0.6 and `git diff --check` passed. Independent source review has no unresolved Critical or Important findings. Source/artifact manifests record the eventual push/build identity separately. In-game appearance and audio remain untested.
