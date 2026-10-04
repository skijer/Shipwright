# Final Static Actor Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the MM Bomb Shop Lady, Skull Kid with Tatl, Phantom Ganon, adult human Ganondorf, and the Happy Mask Salesman to the static `En_Viewer` catalogue in the approved order.

**Architecture:** Extend the pure catalogue registry, then introduce a narrow MM presentation adapter around the archive-scoped `MmAssets_*` API. MM behavior stays outside the generic registry; the two OoT Ganondorf models use dedicated viewer draw contracts. Skull Kid owns a second decorative `SkelAnime` for Tatl, and optional resources fail atomically before interaction is enabled.

**Tech Stack:** C23 actor helpers and host tests, C/C++ SoH overlay integration, libultraship resource loading, OoT `SkelAnime`, GitHub Actions Linux/Windows builds.

**Spec:** `docs/superpowers/specs/2026-09-09-final-static-actor-expansion-design.md`

## Global Constraints

- Order: MM Bomb Shop Lady, Skull Kid plus Tatl, Phantom Ganon, adult human Ganondorf, Happy Mask Salesman.
- Load MM resources only from the user's mounted `mm.o2r` or compatible override archives; redistribute none.
- `0x7E06` is adult human Ganondorf only. Beast Ganon, transformation logic, and `En_Ganon_Mant` are forbidden.
- Tatl appears in both Skull Kid poses but has no `En_Elf` AI, light, collision, targeting, hints, dialogue, or save state.
- No weather or dialogue-pool changes.
- Every production behavior begins with a focused test observed failing for the intended reason.
- Preserve the Great Fairy, Zelda, Sheik, Impa, and Adult Ruto baseline.

---

### Task 1: Extend the Pure Catalogue Registry

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces types, adapters, skeleton families, animations, tracking policies, and draw contracts for all five additions.
- Produces exact `0x7E05` through `0x7E09` decoding while preserving all existing mappings.

- [ ] **Step 1: Write failing assertions** for all ten approved parameters, invalid adjacent poses, availability, pose-to-animation mapping, tracking flags, MM-vs-OoT source, and human Ganondorf's draw contract.

- [ ] **Step 2: Verify RED.**

```bash
cc -std=c23 -Wall -Wextra -Werror -Isoh/include \
  soh/tests/static_story_actor_test.c \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c \
  -o /tmp/static_story_actor_test && /tmp/static_story_actor_test
```

Expected: compilation or assertion failure because the new actor types do not exist.

- [ ] **Step 3: Add minimal definitions.** Map expanded IDs 5-9 to Bomb Shop Lady, adult Ganondorf, Phantom Ganon, Skull Kid, and Happy Mask Salesman. Define the approved pose rows and an explicit resource-source policy so MM actors bypass OoT object-slot requests. Enable tracking only for Bomb Shop Lady idle and Mask Salesman idle.

- [ ] **Step 4: Verify GREEN** with the command from Step 2; expect exit code 0 and no warnings.

- [ ] **Step 5: Commit.**

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c \
  soh/tests/static_story_actor_test.c
git commit -m "feat: register final static actor catalogue entries"
```

---

### Task 2: Prove the MM Adapter with Bomb Shop Lady

**Files:**
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h`
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c`
- Create: `soh/tests/static_story_mm_actor_test.c`
- Modify: `soh/CMakeLists.txt`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`

**Interfaces:**
- Produces pure MM path, pose, tracking, and lifecycle policy helpers.
- Produces `StaticStoryMm_Init`, `StaticStoryMm_Update`, `StaticStoryMm_Draw`, and `StaticStoryMm_Destroy` with explicit success/failure.
- Consumes `MmAssets_LoadSkeleton`, `MmAssets_LoadAnimation`, and `MmAssets_LoadResource` without global OoT fallback.

- [ ] **Step 1: Write failing tests** for the exact Bomb Shop Lady skeleton and three animation paths, pose-zero-only tracking, and atomic missing-resource failure.

- [ ] **Step 2: Add the host target and verify RED.**

```bash
cmake -S soh -B build-actor-tests -DBUILD_TESTING=ON
cmake --build build-actor-tests --target static_story_mm_actor_test
ctest --test-dir build-actor-tests -R static_story_mm_actor_test --output-on-failure
```

Expected: failure because the helper API is absent.

- [ ] **Step 3: Implement the pure descriptor and runtime adapter.** Keep all MM paths localized. Extend `EnViewerStaticState` with guarded MM animation state. Bypass OoT object requests for MM entries and enable draw/collision/talk only after the complete 18-limb skeleton and selected animation load.

- [ ] **Step 4: Integrate update, draw, tracking, and destroy.** Use fixed eyes, track only pose zero, assign fresh draw state, destroy only initialized state, and log unavailable resources once.

- [ ] **Step 5: Verify GREEN.**

```bash
cmake --build build-actor-tests --target static_story_mm_actor_test static_story_actor_test
ctest --test-dir build-actor-tests -R 'static_story_(mm_)?actor_test' --output-on-failure
git diff --check
```

- [ ] **Step 6: Commit** as `feat: add MM Bomb Shop Lady adapter` with only the files listed above.

---

### Task 3: Add Both Skull Kid Poses and Embedded Tatl

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c`
- Modify: `soh/tests/static_story_mm_actor_test.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`

**Interfaces:**
- Produces authored-Y hover composition with phase increment `0x4B0` and amplitude near `10.0f`.
- Produces pose-specific Tatl anchors and bounded color/alpha/scale pulse helpers.
- Extends MM state with a second guarded `SkelAnime` and Tatl phase.

- [ ] **Step 1: Write failing tests** for reclining/upright paths, Majora's Mask attachment, disabled tracking, bounded non-drifting hover, Tatl in both poses, distinct anchors, pulse bounds, and archive-scoped `gameplay_keep` resources.

- [ ] **Step 2: Verify RED** by running `static_story_mm_actor_test`.

- [ ] **Step 3: Implement Skull Kid.** Load the pose animation, attach the mask in the head/post-limb callback, and set Y to `authoredY + hoverOffset` each frame.

- [ ] **Step 4: Implement Tatl.** Load her six-limb skeleton, animation, and limbs archive-specifically; initialize atomically with Skull Kid; update a local orbit; draw XLU with warm Tatl colors and bounded pulses; create no actor, light, collision, or attention state.

- [ ] **Step 5: Verify GREEN** for MM and registry tests, including missing-Tatl suppression and guarded destruction.

- [ ] **Step 6: Commit** as `feat: add Skull Kid and decorative Tatl poses`.

---

### Task 4: Add Phantom Ganon

**Files:**
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_ganon.h`
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_ganon.c`
- Create: `soh/tests/static_story_ganon_test.c`
- Modify: `soh/CMakeLists.txt`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`

**Interfaces:**
- Produces pure skeleton, animation, eye/segment, tracking, and cape descriptors for Ganon-family presentations.
- Produces a dedicated Phantom Ganon initialization/draw route without boss behavior.

- [ ] **Step 1: Write failing tests** for `OBJECT_GND`, `gPhantomGanonSkel`, `gPhantomGanonNeutralAnim`, disabled tracking, omitted horse/projectiles/AI, and an explicit harmless segment `0x08` assignment.

- [ ] **Step 2: Add `static_story_ganon_test` and verify RED.**

- [ ] **Step 3: Implement the neutral loop** through normal OoT object slots and a dedicated draw function that installs segment `0x08` before drawing. Call no fight actions or effects.

- [ ] **Step 4: Verify GREEN** with `static_story_ganon_test`, `static_story_actor_test`, and `git diff --check`.

- [ ] **Step 5: Commit** as `feat: add static Phantom Ganon adapter`.

---

### Task 5: Add Cape-Free Adult Human Ganondorf

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_ganon.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_ganon.c`
- Modify: `soh/tests/static_story_ganon_test.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`

**Interfaces:**
- Adds `gGanondorfSkel`, `gGanondorfStandIdleAnim`, normal-eye, and separate-eyes-display-list policy.
- Reuses the proven viewer face contract but never accesses global `sGanondorfCape` for the static entry.

- [ ] **Step 1: Write failing tests** for `OBJECT_GANON`, the human skeleton/standing animation, `gGanondorfNormalEyeTex`, `gGanondorfEyesDL`, disabled tracking, and `spawnDynamicCape == false`. Assert no Beast Ganon or transformation policy.

- [ ] **Step 2: Verify RED** with `static_story_ganon_test`.

- [ ] **Step 3: Implement the standing presentation.** Assign the normal eye every draw, render `gGanondorfEyesDL` from the post-limb callback, and never call `Actor_SpawnAsChild` for `ACTOR_EN_GANON_MANT` on the static route.

- [ ] **Step 4: Verify GREEN** for Ganon and registry tests plus `git diff --check`.

- [ ] **Step 5: Commit** as `feat: add static adult Ganondorf presentation`.

---

### Task 6: Add Happy Mask Salesman

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c`
- Modify: `soh/tests/static_story_mm_actor_test.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`

**Interfaces:**
- Adds the three `object_osn` pose descriptors and explicit cheerful facial-segment contract.
- Reuses the proven MM lifecycle without trade or cutscene state.

- [ ] **Step 1: Write failing tests** for skeleton/animation paths, backpack-bearing model, cheerful face, per-draw facial segments, pose-zero-only tracking, and absence of trade/inventory/cutscene policy.

- [ ] **Step 2: Verify RED** with `static_story_mm_actor_test`.

- [ ] **Step 3: Implement all three poses** using the shared lifecycle. Assign cheerful facial textures each draw; track only neutral idle; preserve authored limbs for hands-clasped and arms-out.

- [ ] **Step 4: Verify GREEN** for MM and registry tests plus `git diff --check`.

- [ ] **Step 5: Commit** as `feat: add static Happy Mask Salesman poses`.

---

### Task 7: Full Regression and Publication Readiness

**Files:**
- Modify only if a scoped failing test exposes a defect in Tasks 1-6.

**Interfaces:**
- Produces one clean actor-only branch ready for GitHub Windows/Linux CI and runtime validation.

- [ ] **Step 1: Run all focused tests.**

```bash
cmake -S soh -B build-actor-tests -DBUILD_TESTING=ON
cmake --build build-actor-tests --target static_story_actor_test \
  static_story_ruto_water_test static_story_mm_actor_test static_story_ganon_test
ctest --test-dir build-actor-tests --output-on-failure
```

- [ ] **Step 2: Run strict and hygiene checks.**

```bash
cc -std=c23 -Wall -Wextra -Werror -Isoh/include \
  soh/tests/static_story_actor_test.c \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c \
  -o /tmp/static_story_actor_test && /tmp/static_story_actor_test
git diff --check
git status --short
git diff eca463bff --stat
git diff eca463bff -- soh/src/overlays/actors/ovl_En_Viewer soh/tests soh/CMakeLists.txt
```

Expected: zero warnings/failures and no weather or unrelated catalogue changes.

- [ ] **Step 3: Build the integration target** when local dependencies permit. Otherwise record the limitation and rely on the unchanged GitHub workflow without claiming local integration success.

- [ ] **Step 4: Prepare runtime acceptance:** test every parameter with vanilla OoT assets, intended OoT alt stack, stock `mm.o2r`, compatible MM overrides, simultaneous placements, room reloads, and adjacent-actor eye/segment checks.

- [ ] **Step 5: Commit only test-driven stabilization changes;** do not create an empty verification commit.

