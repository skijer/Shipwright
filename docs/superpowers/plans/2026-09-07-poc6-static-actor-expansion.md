# POC6 Static Actor Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand the stable Prelude static-actor catalogue with the remaining desired sages, decorative villains, a friendly Gerudo guard, and opt-in Adult Ruto water modes without affecting native actor behavior.

**Architecture:** Preserve existing `0x7Fxx` parameters and add a separate `0x7Exx` catalogue page. Keep definition/decoding policy pure, place family-specific rendering and behavior in focused adapter files, and admit experimental villain forms only after standard actors pass.

**Tech Stack:** C23, SoH actor/object/animation APIs, SkelAnime, CollisionCheck, CMake/CTest, clang-format

**Spec:** `docs/superpowers/specs/2026-09-07-poc6-weather-static-actor-expansion-design.md`

## Global Constraints

- Begin only after the weather/audio release gate passes.
- Preserve every existing `0x7Fxx` meaning and behavior.
- Put all new actors on the `0x7Exx` page.
- Static actors never call native story, reward, cutscene, combat, capture, or progression action functions.
- Dialogue is read-only and uses the existing message layer.
- Missing objects or animations remove only the affected placement.
- Rauru, Zelda cloak forms, hostile Gerudo behavior, and Lost Woods room assets are excluded.

---

### Task 1: Add versioned catalogue-page decoding

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: `StaticStoryActorKey { uint8_t page; uint8_t type; }`
- Produces: `int StaticStoryActor_DecodeParam(int16_t params, StaticStoryActorKey* key, uint8_t* pose)`
- Preserves: `StaticStoryActor_IsParam`, `StaticStoryActor_GetType`, and `StaticStoryActor_GetPose` for page-one callers

- [ ] **Step 1: Write failing compatibility/page tests**

Assert every documented `0x7Fxx` value decodes unchanged, `0x7E01` resolves page 2/type 1, unregistered page/type combinations fail, and pose remains bits 4–7 on both pages.

- [ ] **Step 2: Run and confirm failure**

Run the `static_story_actor_test` target and expect failure because page-two decoding is absent.

- [ ] **Step 3: Implement page-aware keys and tables**

Add `STATIC_STORY_ACTOR_PARAM_PREFIX_PAGE_1 0x7F00` and `STATIC_STORY_ACTOR_PARAM_PREFIX_PAGE_2 0x7E00`. Index page-two definitions separately rather than renumbering the page-one enum. Keep all array access bounds-checked.

- [ ] **Step 4: Run tests and commit**

Run: `cmake --build build --target static_story_actor_test && ctest --test-dir build -R static_story_actor_test --output-on-failure`

Expected: PASS.

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.* soh/tests/static_story_actor_test.c
git commit -m "feat: add second static actor catalogue page"
```

### Task 2: Add Darunia and Nabooru adapters

**Files:**
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_sages.h`
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_sages.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: `int StaticStorySages_RequestObjects(EnViewer*, PlayState*)`
- Produces: `int StaticStorySages_Init(EnViewer*, PlayState*)`
- Produces: `void StaticStorySages_Update(EnViewer*, PlayState*)`
- Produces: `void StaticStorySages_Draw(EnViewer*, PlayState*)`

- [ ] **Step 1: Add failing definitions and dance-sequence tests**

Define page-two type 1 as Darunia and type 2 as Nabooru. Assert their object IDs are `OBJECT_DU` and `OBJECT_NB`, pose zero is a stable native idle, and Darunia's dance transition helper produces the ordered cycle `Hop -> SpinEntry -> Spin -> Dance -> Return -> Hop`.

- [ ] **Step 2: Run and confirm failure**

Run `static_story_actor_test`; expect missing enums/definitions.

- [ ] **Step 3: Implement model initialization and stable poses**

Use `gDaruniaSkel`/`gDaruniaIdleAnim` and `gNabooruSkel`/`gNabooruStandingHandsOnHipsChamberOfSagesAnim`. Copy only the native limb/eye segment conventions required to draw, blink, and track; do not call `En_Du`, `Demo_Du`, or `En_Nb` action functions.

- [ ] **Step 4: Implement Darunia's full dance cycle**

Map each explicit dance state to the corresponding native Darunia animation, advance only when `SkelAnime_Update` completes, lock authored root translation to the Prelude placement, disable tracking for the dance pose, pause while talking, and restart at `Hop` after dialogue.

- [ ] **Step 5: Run tests and commit**

Run the focused test and then build the full `soh` target.

```bash
git add soh/src/overlays/actors/ovl_En_Viewer soh/tests/static_story_actor_test.c
git commit -m "feat: add static Darunia and Nabooru"
```

### Task 3: Add friendly Gerudo guard and standing Ganondorf

**Files:**
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_villains.h`
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_villains.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.*`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.*`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: page-two type 3 `STATIC_STORY_ACTOR_GANONDORF`
- Produces: page-two type 4 `STATIC_STORY_ACTOR_GERUDO_GUARD`
- Produces: villain request/init/update/draw functions parallel to the sage adapter API

- [ ] **Step 1: Write failing definition/dialogue-safety tests**

Assert both entries have valid objects, pose zero, collider/focus dimensions, and nonzero safe text IDs. Assert their behavior flags exclude combat, capture, patrol, cutscene, and root motion.

- [ ] **Step 2: Run and confirm failure**

Run `static_story_actor_test`; expect missing page-two types.

- [ ] **Step 3: Implement standing renderers**

Initialize only stable native standing animations. Implement native eye segments where available, compatible head tracking, soft collision, and the existing read-only dialogue flow. Do not instantiate Ganondorf fire arrays or Gerudo detection/capture state.

- [ ] **Step 4: Run tests and commit**

Run the focused test and full `soh` build.

```bash
git add soh/src/overlays/actors/ovl_En_Viewer soh/tests/static_story_actor_test.c
git commit -m "feat: add decorative Ganondorf and Gerudo guard"
```

### Task 4: Add a pure Adult Ruto water controller

**Files:**
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_ruto_water.h`
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_ruto_water.c`
- Create: `soh/tests/static_story_ruto_water_test.c`
- Modify: `soh/CMakeLists.txt`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`

**Interfaces:**
- Produces: `StaticStoryRutoWaterMode { Grounded, SurfaceIdle, DiveLoop }`
- Produces: `StaticStoryRutoWaterState`
- Produces: `void StaticStoryRutoWater_Advance(StaticStoryRutoWaterState*, const StaticStoryRutoWaterInput*)`

- [ ] **Step 1: Write the pure state-machine test**

Cover grounded no-movement, surface approach and clamp, dive-to-minimum, resurface-to-maximum, surface pause, dialogue pause, resume, and invalid-water fallback to authored Y.

- [ ] **Step 2: Register and run the failing test**

Add `static_story_ruto_water_test` to `soh/CMakeLists.txt`; build it and expect missing interface failures.

- [ ] **Step 3: Implement bounded vertical movement**

Store authored Y, resolved surface Y, lower bound, phase, and pause timer. Clamp every output Y to `[lowerBound, surfaceY]`. Invalid water sets `Grounded` and authored Y. The pure controller contains no engine globals.

- [ ] **Step 4: Integrate new Adult Ruto poses**

Keep `0x7F06`, `0x7F16`, and `0x7F26` unchanged. Assign new page-two Ruto behavior parameters, query water at the placement once during initialization and on room reload, update only Y, pause movement during dialogue, and use compatible native swim/tread animations.

- [ ] **Step 5: Run tests and commit**

Run `static_story_ruto_water_test`, `static_story_actor_test`, and full build.

```bash
git add soh/CMakeLists.txt soh/src/overlays/actors/ovl_En_Viewer soh/tests/static_story_ruto_water_test.c soh/tests/static_story_actor_test.c
git commit -m "feat: add selectable Adult Ruto water modes"
```

### Task 5: Repair the isolated Adult Zelda renderer

**Files:**
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_zelda.h`
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_zelda.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: request/init/draw functions using only `OBJECT_ZL2` and explicitly declared animation objects
- Preserves: existing `0x7F04` and `0x7F14` decoding

- [ ] **Step 1: Add failing dependency-policy tests**

Assert Zelda neutral pose declares its skeleton and every animation object it needs, supports no cloak/Sheik form, and does not report available until all declared slots load.

- [ ] **Step 2: Run and confirm failure**

Run `static_story_actor_test`; expect the new dependency assertions to fail.

- [ ] **Step 3: Implement the isolated renderer**

Move Zelda-specific segment, skeleton, neutral-animation, eye, and limb logic out of the generic draw switch. Initialize `gZelda2Skel` only after all declared object slots load. First enable only the neutral pose; alias pose one to neutral until a separate verified animation is admitted.

- [ ] **Step 4: Run tests and commit**

Run the actor test and full build.

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_zelda.* soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c soh/tests/static_story_actor_test.c
git commit -m "fix: isolate Adult Zelda static renderer"
```

### Task 6: Admit experimental mounted and Phantom Ganondorf adapters

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_villains.*`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.*`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.*`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: page-two type 5 mounted Ganondorf and type 6 Phantom Ganon when their complete dependencies pass the admission check
- Produces: safe unavailability with a diagnostic when a self-contained renderer cannot be formed

- [ ] **Step 1: Enumerate dependencies from native init/draw paths**

Trace `En_Viewer` horse/Ganondorf init data and Phantom Ganon's model draw path. Record exact skeleton, animation, texture, horse, and effect object IDs in the definition table. Reject any dependency that requires boss room switches, encounter state, or native combat actions.

- [ ] **Step 2: Write failing composite-load tests**

Assert mounted Ganondorf requires both rider and horse slots and initializes only when both load. Assert Phantom Ganon's admitted pose lists all renderer dependencies and excludes portals, attacks, damage, and room-state flags.

- [ ] **Step 3: Implement the minimal inert renderers**

Mounted mode draws the stationary horse/rider composite and targets dialogue at the rider. Phantom mode draws only the proven self-contained body/effects. If the dependency audit cannot satisfy the exclusion rules, mark only that definition unavailable and keep its test asserting safe rejection.

- [ ] **Step 4: Run tests and commit**

Run the actor test and full build.

```bash
git add soh/src/overlays/actors/ovl_En_Viewer soh/tests/static_story_actor_test.c
git commit -m "feat: add experimental Ganondorf displays"
```

### Task 7: Catalogue regression gate and parameter guide

**Files:**
- Create: `docs/static-story-actors-poc6-params.md`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: complete human-readable Prelude parameter table with standard/experimental labels

- [ ] **Step 1: Add a table-driven assertion for every documented parameter**

The test table must contain exact parameter, page, type, pose, availability, tracking policy, movement mode, and dialogue safety for every old and new entry.

- [ ] **Step 2: Write the parameter guide from the same table**

Document actor, pose/behavior, hexadecimal Prelude parameter, collision, tracking, dialogue, water requirement, and experimental status. State that Rauru uses Prelude's ordinary actor placement.

- [ ] **Step 3: Run formatting, all CTest targets, and the full build**

Expected: formatting clean, all tests PASS, and full build succeeds.

- [ ] **Step 4: Push and run cross-platform CI**

Require Linux, macOS, and Windows. Retain the Windows artifact only after all failures attributable to this branch are resolved.

- [ ] **Step 5: Execute the in-game matrix**

Test every old parameter first, then each standard actor, Ruto's three modes, Zelda, and experimental villains with Alt Assets off/on, dialogue, collision, targeting, room reloads, and native story sequences. Re-run the weather/audio smoke matrix before accepting the combined build.

- [ ] **Step 6: Commit the verified guide**

```bash
git add docs/static-story-actors-poc6-params.md soh/tests/static_story_actor_test.c
git commit -m "docs: publish POC6 static actor catalogue"
```
