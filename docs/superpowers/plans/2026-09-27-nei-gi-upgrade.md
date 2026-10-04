# NEI GI Upgrade Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this approved plan inline. Checkpoint each model, continue, and submit one unified PR.

**Goal:** Upgrade 16 NEI get-item models and add a cosmetic item-effects checkbox covering NEI GI, shop, and shuffled freestanding displays.

**Architecture:** Author standalone GI resources and bind them at the shared GetItemEntry_Draw boundary. Preserve the original renderer as fallback. A bounded, deterministic presentation effect runs in the same draw, without actor spawning or changes to item/randomizer state.

**Tech Stack:** libultraship XML/binary resources, C/C++, Python/numpy/Pillow asset authoring and verification.

**Spec:** User-approved conversation decisions recorded below.

## Global Constraints

- One PR contains models, bindings, particles, UI, and verification. One combined archive, created once at completion.
- 16 models: three elemental rods, feather, Time Gate, Whip A, tilted shovel, upright jar, three spells, ball and chain, Deku Leaf, Mogma Mitts, switch hook, beetle.
- Equipment model upgrades remain outside this batch.
- Demise core is #000000 with neutral translucent shell. Magic effects use native material hex values; Deku Leaf effects are green, with style flexible. Other items use neutral shimmer.
- Checkbox belongs near NEI Custom Items settings and affects presentation only.
- Preserve source baselines and supplied archives. Do not modify the concurrent ComboShip checkout.

## Review Focus

- Checkbox off must skip effects and allocations while retaining upgraded models.
- Shared GI draw must cover shop and freestanding paths without leaking CPU matrix state.
- Missing replacement resource must retain the original item's draw, without crash or wrong origin effects.
- Particle colors, bounds, and frame wrap must remain deterministic and within the documented budget.
- Vertex cache limits, texture references, fixed-point matrices, and translucent spell shell ordering must match the target loader.

## Task 1: Assets and checkpoints

Files: `tools/nei_gi/`, `soh/assets/custom/objects/nei_gi_redesign/`.

- [ ] Build the 15 remaining approved designs and import Whip A checkpoint.
- [ ] Write all GLBs and resources from identical quantized triangles; validate independent serialized artifacts, matrix scale, cache indices, references and winding.
- [ ] Review front/back/rotation previews and consolidate one candidate archive after all models are ready.

## Task 2: Unified renderer and UI

Files: `soh/soh/Enhancements/randomizer/NeiGiEffectPolicy.h`, `NeiGiPresentation.cpp`, `NeiGiPresentation.h`, `soh/src/code/z_draw.c`, `soh/soh/SohGui/SohMenuNEI.cpp`, `tests/nei_gi/`.

Interfaces: `bool NeiGi_Draw(PlayState*, GetItemEntry*)` handles only explicitly listed NEI custom-item draw functions; returns false for all other items. Pure effect policy returns up to eight positions, sizes, color, and alpha per item and frame.

- [ ] Write and run failing C++ tests for exact native colors, bounds, deterministic frame sampling and disabled behavior.
- [ ] Implement policy and shared renderer; use archive resources for all 16 models and original draw fallback for missing resources and other NEI items.
- [ ] Add `NEI item effects` checkbox in Custom Items using a cosmetic CVar. Default off to make the new effect opt-in; no seed setting or saved-item mutation.
- [ ] Run focused policy tests, compile/syntax checks where dependencies permit, and resource validation. Record absent runtime proof explicitly.

## Task 3: One reviewable PR

- [ ] Fresh whole-change review under requesting-code-review; fix material findings with reproducing checks.
- [ ] Commit assets and integration on one feature branch in cor's Shipwright fork, based on the current integration branch.
- [ ] Open one draft PR against `marsh6487/Shipwright:integration/nei-weather-static-actors`, with consolidated previews and validation limits. Do not merge.

## Execution ledger

- Baseline: clean shallow isolated clone at c29262b. Concurrent ComboShip checkout is read-only.
- Ruling: use GI-specific resource names for upgraded assets; existing actor/in-hand resource names may be shared. Cost: all 16 presentation bindings are explicit in the shared renderer.
- Ruling: new cosmetic checkbox defaults off, matching opt-in shimmer behavior and avoiding unrequested performance cost. User can enable live.
- User correction: green Deku Leaf, not blue. Dust was an example, not a requirement.

## Recovery at 2026-09-27

- Recovered all 16 checkpoint GLBs, front/back renders, and serialized resources from the interrupted session.
- Original session had already begun migration to integration commit `628deea3cf851af19a18f915e0665d772294c383`; its cherry-pick was interrupted by conflicting includes in `z_draw.c`.
- Continued in an independent copy; the original workspace and uploaded archives remain intact.
- Resolved the conflict by retaining the Din fire-shield includes and adding the GI presentation header.
- Fixed real-header vertex pointer conversions and interpolation callback C linkage exposed by compiling/linking the renderer.
- Use interpreter resource-path commands to avoid the legacy loader's Alt resource eviction on every draw.
- Validation separates serialized-asset checks and compiled renderer tests from in-game acceptance; runtime remains untested.

## Visual approval at 2026-09-27 13:41 Chicago

- All 16 model previews were delivered before PR publication.
- The user accepted Beetle revision 2 and Roc's Feather / Ball and Chain revision 3, then authorized proceeding.
- Beetle uses mechanical casing, flush brass fittings, rigid wings and crescent pincers.
- Feather's approved open tuft clefts are preserved; revision 3 improves barb shading and relief readability.
- Ball and Chain's mistaken rounded bosses are replaced by pointed metal spikes.
- The user was told that crystal edge strips are actual geometry and core textures are current assets shown without runtime particles.
- Visual approval permits completing one combined archive and the unified draft PR. In-game validation remains separate; do not merge or promote on offline preview approval alone.
