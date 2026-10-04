# NEI GI Runtime POC4 Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Correct shop fit and overhead GI routing, and provide animated previews of readable magical energy for approval.

**Architecture:** Preserve all accepted asset bytes. Route custom overhead GIs through the common entry renderer; add a shop-only presentation transform. Share deterministic effect geometry between the game renderer and offline animated previews.

**Tech Stack:** C/C++, existing N64 display-list bridge, Python/NumPy offline renderer.

**Spec:** User feedback and clips, 2026-09-27 16:25–16:32 America/Chicago: counter clearance; visible wonder shimmer; volatile rod tips; dramatic contained spell energy; upgraded overhead models; feather shop identity; preview approval before publication.

## Global Constraints
- Baseline bea6f438fc80e8fc92522bbba70e5938b034a617 remains unchanged.
- Preserve all 16 meshes, textures, gameplay, Alt resolution, and Demise's black core.
- No particle actors, random state, new dependencies, or per-draw resource eviction.
- Preview from production geometry; distinguish offline previews from runtime proof.
- Publication waits for effect-preview approval.

## Review Focus
- Ice-trap and Triforce overhead branches keep their special rendering.
- Missing/Alt-only resources retain usable fallback models and balanced matrices.
- Shop transforms cannot leak into world pickups or overhead poses.
- Energy remains visible with optional shimmer off and stays attached while spinning.
- The review seed must place Progressive Roc, not the separate vanilla rando feather.

### Task 1: Correct draw paths and shop placement
**Files:** z_player_lib.c, z_en_girla.c, NeiGiPresentation.{h,cpp}, tests/nei_gi/presentation_test.cpp, scripts/diagnostics/run_nei_gi_tests.py.
**Interfaces:** Add bool NeiGi_DrawShop(PlayState*, GetItemEntry*); false means caller uses normal dispatch.
- [x] Add failing execution tests for actual overhead body (replacement, vanilla, trap, Triforce) and shop geometry bounds.
- [x] Run python scripts/diagnostics/run_nei_gi_tests.py; observe the bypass/clearance failures.
- [x] Route overhead custom callbacks through GetItemEntry_Draw; apply shop-only scales/offsets to replacement models and effects, preserving fallback.
- [x] Run focused tests; retain the fix in the single reviewable candidate commit.

### Task 2: Separate shimmer and elemental energy
**Files:** NeiGiEffectPolicy.h, NeiGiPresentation.cpp, SohMenuNEI.cpp, tests/nei_gi/{effect_policy,presentation}_test.cpp.
**Interfaces:** Pure bounded SampleEnergy and SampleShimmer triangle geometry; renderer uses a stable frame arena and restores matrix/pipeline state.
- [x] Add failing tests for always-on intrinsic energy, independent optional glints, finite/bounded geometry, and fixed draw budget.
- [x] Run focused tests; observe expected missing energy behavior.
- [x] Implement animated flames, frost arcs, light coronas, and contained core energy; clarify checkbox tooltip.
- [x] Run focused tests and cumulative regressions, then include the implementation in the single candidate commit.

### Task 3: Reviewable previews and corrected seed
**Files:** tools/nei_gi effect preview exporter/renderer; separate user deliverables.
**Interfaces:** Export production effect triangles to offline preview, rendering accepted geometry under the same transforms.
- [x] Correct Bazaar Item 5 to Progressive Roc in a separately named seed and validate names against item table.
- [x] Generate animated previews for three rods, three cores, and shimmer plus shop-fit stills; visually inspect output.
- [x] Perform whole-branch review; preserve candidate and provide preview files. Runtime remains untested; do not publish before approval.
