# Static Story Actors POC4 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windows-testable SoH POC4 in which nine story actors can be placed through documented Prelude parameters with native poses, safe dialogue, blinking, soft collision, correct targeting, and Alt Assets support.

**Architecture:** Keep vanilla `En_Viewer` behavior intact and route only the reserved `0x7Fpp` parameter family into a validated static-actor registry. Split common lifecycle/interaction behavior from character draw adapters, with a specialized multipart adapter for Kokiri Girl/Fado and an isolated Adult Zelda checkpoint. Carry the validated weather implementation forward byte-for-byte except for conflict resolution required to compile the actor additions.

**Tech Stack:** C11/C++17, SoH actor/object/message APIs, SkelAnime, OTR resource paths, CMake/CTest, clang-format, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-04-static-story-actors-poc4-design.md`

## Global Constraints

- Preserve all vanilla `En_Viewer` behavior unless `StaticStoryActor_IsParam(params)` is true.
- Preserve validated rain/BGM concurrency, thunder timing and selection, volume behavior, and room-transition persistence.
- Use `params = 0x7F00 | (pose << 4) | actorType`; unsupported poses fall back to pose `0`, while unsupported actor types terminate safely.
- Dialogue may read progression flags and open existing text, but must not mutate flags, grant items, start cutscenes or ocarina checks, move/despawn actors, or invoke native quest action functions.
- Non-Kokiri actors may use only animations authored for their own skeleton.
- Adult Zelda pose `0` is required; pose `1` must alias pose `0` if its dynamic animation object is not independently stable.
- Impa must use a correct model-specific face path under Alt Assets and a natural idle playback rate rather than the POC1 `3.0f` rate.
- Windows must compile before publishing POC4; Linux and macOS failures must be inspected and may not be silently ignored.

## File structure

- `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`: pure parameter decoder, actor/pose enums, definition interfaces, compile-time table contracts.
- `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`: static registry, pose sanitization, progression-aware text selection, pure helpers used by tests.
- `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h`: runtime fields for object slots, skeleton state, collider, face state, conversation state, character adapter.
- `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`: dispatch between untouched vanilla viewer behavior and the custom static lifecycle; single-skeleton character adapters.
- `soh/src/overlays/actors/ovl_En_Viewer/static_story_kokiri.c`: multipart Kokiri Girl/Fado object loading and limb drawing.
- `soh/tests/static_story_actor_test.c`: decoder, pose fallback, definition coverage, focus/collider data, and dialogue-selection tests.
- `soh/CMakeLists.txt`: test target and explicit new source registration if the existing source glob does not include them.
- `docs/static-story-actors-poc4-params.md`: concise user-facing Prelude placement and behavior table copied from the approved spec.

---

### Task 1: Parameter Registry and Coverage Guards

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Create: `soh/tests/static_story_actor_test.c`
- Modify: `soh/CMakeLists.txt`

**Interfaces:**
- Produces: `StaticStoryActor_IsParam`, `StaticStoryActor_GetType`, `StaticStoryActor_GetPose`, `StaticStoryActor_SanitizePose`, `StaticStoryActor_GetDefinition`, and nine contiguous actor definitions.

- [ ] **Step 1: Write the failing decoder and coverage tests**

```c
assert(StaticStoryActor_IsParam((int16_t)0x7F01));
assert(StaticStoryActor_GetType((int16_t)0x7F59) == STATIC_STORY_ACTOR_FADO);
assert(StaticStoryActor_GetPose((int16_t)0x7F59) == 5);
assert(!StaticStoryActor_IsParam((int16_t)0x0101));
assert(StaticStoryActor_GetType((int16_t)0x7F0A) == STATIC_STORY_ACTOR_NONE);
assert(StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_IMPA, 15) == 0);
assert(StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_FADO, 5) == 5);
for (int type = STATIC_STORY_ACTOR_IMPA; type < STATIC_STORY_ACTOR_MAX; ++type) {
    assert(StaticStoryActor_GetDefinition(type) != NULL);
}
```

- [ ] **Step 2: Register and run the focused CTest target**

Run: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build --target static_story_actor_test && ctest --test-dir build -R static_story_actor_test --output-on-failure`

Expected: compile failure because the expanded enum, definitions, and sanitizer do not exist.

- [ ] **Step 3: Implement the pure registry contract**

```c
typedef enum {
    STATIC_STORY_ACTOR_NONE = 0,
    STATIC_STORY_ACTOR_IMPA = 1,
    STATIC_STORY_ACTOR_CHILD_MALON,
    STATIC_STORY_ACTOR_SARIA,
    STATIC_STORY_ACTOR_ADULT_ZELDA,
    STATIC_STORY_ACTOR_SHEIK,
    STATIC_STORY_ACTOR_ADULT_RUTO,
    STATIC_STORY_ACTOR_CHILD_RUTO,
    STATIC_STORY_ACTOR_KOKIRI_GIRL,
    STATIC_STORY_ACTOR_FADO,
    STATIC_STORY_ACTOR_MAX
} StaticStoryActorType;

typedef enum {
    STATIC_ADAPTER_IMPA,
    STATIC_ADAPTER_MALON,
    STATIC_ADAPTER_SARIA,
    STATIC_ADAPTER_ADULT_ZELDA,
    STATIC_ADAPTER_SHEIK,
    STATIC_ADAPTER_ADULT_RUTO,
    STATIC_ADAPTER_CHILD_RUTO,
    STATIC_ADAPTER_KOKIRI_GIRL,
    STATIC_ADAPTER_FADO,
} StaticStoryActorAdapter;

typedef enum {
    STATIC_SKELETON_IMPA,
    STATIC_SKELETON_MALON,
    STATIC_SKELETON_SARIA,
    STATIC_SKELETON_ADULT_ZELDA,
    STATIC_SKELETON_SHEIK,
    STATIC_SKELETON_ADULT_RUTO,
    STATIC_SKELETON_CHILD_RUTO,
    STATIC_SKELETON_KOKIRI,
} StaticStorySkeletonFamily;

typedef struct {
    uint16_t animation;
    float playbackSpeed;
    uint16_t flags;
    StaticStorySkeletonFamily skeletonFamily;
} StaticStoryPoseDescriptor;

typedef struct {
    uint8_t maxPose;
    int16_t objectId;
    StaticStoryActorAdapter adapter;
    float scale;
    float focusHeight;
    int16_t colliderRadius;
    int16_t colliderHeight;
    int16_t colliderYShift;
} StaticStoryActorDefinition;

const StaticStoryActorDefinition* StaticStoryActor_GetDefinition(StaticStoryActorType type);
const StaticStoryPoseDescriptor* StaticStoryActor_ResolvePose(StaticStoryActorType type, uint8_t pose);
```

Decode only `0x7Fxx`. Validate the actor type before any array access. Sanitize each pose against the definition's `maxPose`.

- [ ] **Step 4: Run tests and formatting**

Run: `cmake --build build --target static_story_actor_test && ctest --test-dir build -R static_story_actor_test --output-on-failure`

Run: `clang-format --dry-run --Werror soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c soh/tests/static_story_actor_test.c`

Expected: focused tests and formatting pass.

- [ ] **Step 5: Commit the registry**

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c soh/tests/static_story_actor_test.c soh/CMakeLists.txt
git commit -m "feat: define static story actor registry"
```

### Task 2: Safe Static Lifecycle, Collision, and Target Focus

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Consumes: validated registry from Task 1.
- Produces: `EnViewerStatic_Init`, `EnViewerStatic_WaitForObjects`, `EnViewerStatic_Update`, `EnViewerStatic_Draw`, and `EnViewerStatic_Destroy`.

- [ ] **Step 1: Extend tests for physical definitions**

```c
for (int type = STATIC_STORY_ACTOR_IMPA; type < STATIC_STORY_ACTOR_MAX; ++type) {
    const StaticStoryActorDefinition* def = StaticStoryActor_GetDefinition(type);
    assert(def->scale > 0.0f);
    assert(def->focusHeight > 0.0f);
    assert(def->colliderRadius > 0);
    assert(def->colliderHeight > def->colliderRadius);
}
```

- [ ] **Step 2: Run the focused test to expose missing physical data**

Run: `cmake --build build --target static_story_actor_test && ctest --test-dir build -R static_story_actor_test --output-on-failure`

Expected: failure for incomplete focus or collider definitions.

- [ ] **Step 3: Add explicit runtime state and reserved-param dispatch**

```c
typedef struct {
    uint8_t type;
    uint8_t pose;
    uint8_t initialized;
    uint8_t eyeIndex;
    int16_t blinkTimer;
    int16_t objectSlots[4];
    ColliderCylinder collider;
} EnViewerStaticState;
```

At `EnViewer_Init`, test `StaticStoryActor_IsParam(this->actor.params)` before interpreting vanilla `params >> 8`. Invalid `0x7Fxx` values call `Actor_Kill`; all other values execute the original vanilla path unchanged.

- [ ] **Step 4: Implement asynchronous object initialization and physical behavior**

Request every definition object, wait until all are loaded, set the actor object dependency, initialize `SkelAnime`, then initialize the collider and shadow exactly once. Each update calls `Collider_UpdateCylinder`, registers `CollisionCheck_SetOC`, and calls `Actor_SetFocus(&this->actor, def->focusHeight)`. Do not call actor movement or background-check movement for the anchored static mode.

- [ ] **Step 5: Make destruction mode-aware**

Free only resources initialized by the active vanilla/static path. Destroy the cylinder only when `staticState.initialized` is true; preserve vanilla `Skin_Free` behavior for non-static viewers.

- [ ] **Step 6: Run tests, build, and commit**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure && cmake --build build --target soh`

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c soh/tests/static_story_actor_test.c
git commit -m "feat: add safe static actor lifecycle"
```

### Task 3: Proven Actors, Pose Selection, and Impa Repair

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: complete Impa, Child Malon, and Saria adapters and pose maps for types `1` through `3`.

- [ ] **Step 1: Write failing pose-map tests**

```c
assert(StaticStoryActor_ResolvePose(2, 0)->animation == STATIC_ANIM_MALON_IDLE);
assert(StaticStoryActor_ResolvePose(2, 1)->animation == STATIC_ANIM_MALON_SING);
assert(StaticStoryActor_ResolvePose(2, 2)->flags & STATIC_POSE_FLAG_VOCAL);
assert(StaticStoryActor_ResolvePose(3, 1)->animation == STATIC_ANIM_SARIA_HANDS_BEHIND);
assert(StaticStoryActor_ResolvePose(3, 2)->animation == STATIC_ANIM_SARIA_OCARINA);
assert(StaticStoryActor_ResolvePose(3, 3)->animation == STATIC_ANIM_SARIA_SEATED);
assert(StaticStoryActor_ResolvePose(1, 0)->playbackSpeed == 1.0f);
```

- [ ] **Step 2: Run tests and verify the new pose contract fails**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure`

Expected: compile or assertion failure for pose resolution and Impa speed.

- [ ] **Step 3: Implement native pose mappings**

Map Malon to `gMalonChildIdleAnim` and `gMalonChildSingAnim`. Map Saria to `gSariaWaitArmsToSideAnim`, `gSariaHandsBehindBackWaitAnim`, `gSariaPlayingOcarinaAnim`, and the verified native seated loop. Attach the native ocarina display list only for the ocarina pose.

- [ ] **Step 4: Repair Impa's face and timing**

Replace the generic static face callback with Impa's native limb override/segment setup for open, half, and closed eyes. Keep all texture/display-list references as OTR-native symbols so Alt Assets resolve normally. Set the static idle animation playback to `1.0f`; do not reuse `EnViewer_InitAnimImpa`'s vanilla cutscene rate of `3.0f`.

- [ ] **Step 5: Implement Malon vocal isolation**

For `0x7F22`, reproduce only the native distance/cadence-triggered vocal SFX associated with the singing actor. Do not enqueue, stop, fade, or replace BGM/nature sequences. `0x7F12` never dispatches vocals.

- [ ] **Step 6: Run tests, build, and commit**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure && cmake --build build --target soh`

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c soh/tests/static_story_actor_test.c
git commit -m "feat: complete proven static actor poses"
```

### Task 4: Sheik and Ruto Character Adapters

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: native model, face, and pose adapters for Sheik, Adult Ruto, and Child Ruto.

- [ ] **Step 1: Write failing adapter and pose tests**

```c
assert(StaticStoryActor_GetDefinition(5)->objectId == OBJECT_XC);
assert(StaticStoryActor_ResolvePose(5, 2)->animation == STATIC_ANIM_SHEIK_HARP);
assert(StaticStoryActor_GetDefinition(6)->objectId == OBJECT_RU2);
assert(StaticStoryActor_ResolvePose(6, 1)->animation == STATIC_ANIM_ADULT_RUTO_HANDS_HIPS);
assert(StaticStoryActor_GetDefinition(7)->objectId == OBJECT_RU1);
assert(StaticStoryActor_ResolvePose(7, 2)->animation == STATIC_ANIM_CHILD_RUTO_SITTING);
```

- [ ] **Step 2: Run tests and verify failure**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure`

- [ ] **Step 3: Implement Sheik poses and draw state**

Use `gSheikSkel` with `gSheikIdleAnim`, `gSheikArmsCrossedIdleAnim`, and the verified looping harp performance animation. Reproduce Sheik's native eye segment and limb override without any cutscene action, smoke, movement, or disappearance logic.

- [ ] **Step 4: Implement Adult Ruto poses and draw state**

Use `gAdultRutoSkel` with `gAdultRutoIdleAnim`, `gAdultRutoIdleHandsOnHipsAnim`, and `gAdultRutoLookingDownLeftAnim`. Reuse native eye and mouth segment selection. Do not import swimming, water-surface, or cutscene elevation logic.

- [ ] **Step 5: Implement Child Ruto poses and draw state**

Use `gRutoChildSkel` with `gRutoChildWaitHandsBehindBackAnim`, `gRutoChildWaitHandsOnHipsAnim`, and `gRutoChildSittingAnim`. Reuse native face segments without carry, sapphire, swimming, or Jabu-specific state.

- [ ] **Step 6: Run tests, build, and commit**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure && cmake --build build --target soh`

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c soh/tests/static_story_actor_test.c
git commit -m "feat: add static Sheik and Ruto adapters"
```

### Task 5: Multipart Kokiri Girl and Fado Adapter

**Files:**
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_kokiri.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: `StaticStoryKokiri_RequestObjects`, `StaticStoryKokiri_Init`, `StaticStoryKokiri_Draw`, and shared Girl/Fado pose compatibility.

- [ ] **Step 1: Write failing Kokiri compatibility tests**

```c
for (int pose = 0; pose <= 5; ++pose) {
    assert(StaticStoryActor_ResolvePose(8, pose) != NULL);
    assert(StaticStoryActor_ResolvePose(9, pose) != NULL);
    if (pose >= 3) {
        assert(StaticStoryActor_ResolvePose(8, pose)->animation ==
               StaticStoryActor_ResolvePose(9, pose)->animation);
    }
}
assert(StaticStoryActor_GetDefinition(8)->adapter == STATIC_ADAPTER_KOKIRI_GIRL);
assert(StaticStoryActor_GetDefinition(9)->adapter == STATIC_ADAPTER_FADO);
```

- [ ] **Step 2: Run tests and verify failure**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure`

- [ ] **Step 3: Implement multipart object loading**

Request `OBJECT_OS_ANIME` plus the native Kokiri girl head/body/legs objects. For Fado, substitute only the Fado head object and retain the girl body and legs. Wait for every slot before initializing the shared skeleton and never overwrite the actor's primary dependency while drawing another limb object.

- [ ] **Step 4: Implement pose and limb drawing**

Map poses to `gKokiriIdleAnim`, `gKokiriStandingArmsBehindBackAnim`, `gKokiriStandingHandsOnHipsAnim`, `gKokiriSittingHeadOnHandAnim`, `gKokiriSittingCrossedLegsAnim`, and `gKokiriSittingCrossedArmsLegsAnim`. Reuse the native En_Ko limb-object switching and color setup, including Fado's unique head.

- [ ] **Step 5: Run tests, build, and commit**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure && cmake --build build --target soh`

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_kokiri.c soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c soh/tests/static_story_actor_test.c
git commit -m "feat: add static Kokiri girl and Fado poses"
```

### Task 6: Adult Zelda Isolated Milestone

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: stable Adult Zelda pose `0`, blinking/face draw state, and safe pose `1` resolution.

- [ ] **Step 1: Write failing Zelda fallback tests**

```c
assert(StaticStoryActor_GetDefinition(4)->objectId == OBJECT_ZL2);
assert(StaticStoryActor_ResolvePose(4, 0) != NULL);
assert(StaticStoryActor_ResolvePose(4, 1) != NULL);
assert(StaticStoryActor_ResolvePose(4, 1)->skeletonFamily == STATIC_SKELETON_ADULT_ZELDA);
```

- [ ] **Step 2: Run tests and verify failure**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure`

- [ ] **Step 3: Implement required stable pose**

Load `OBJECT_ZL2`, initialize `gZelda2Skel`, reproduce native eye/mouth segments, and use a verified neutral frame or loop that does not require a live cutscene action. This step must compile and render independently of the optional animation object.

- [ ] **Step 4: Probe the dynamic idle without blocking POC4**

Request `OBJECT_ZL2_ANIME1` separately and test the native initialization loop used by `En_Zl2`. If segment ownership and looping remain stable after room reload, map `0x7F14` to it. If not, map `0x7F14` to the same stable pose descriptor as `0x7F04`; do not retain partially initialized animation state.

- [ ] **Step 5: Run tests, build, and commit**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure && cmake --build build --target soh`

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c soh/tests/static_story_actor_test.c
git commit -m "feat: add safe static adult Zelda"
```

### Task 7: Progression-Aware Read-Only Dialogue and Tracking

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: `StaticStoryActor_SelectTextId(type, progression)`, `EnViewerStatic_OfferTalk`, and compatible idle-only head tracking.

- [ ] **Step 1: Write failing side-effect-free dialogue-selection tests**

```c
StaticStoryProgression early = { 0 };
StaticStoryProgression complete = {
    .metZelda = true,
    .forestComplete = true,
    .waterComplete = true,
};
for (int type = STATIC_STORY_ACTOR_IMPA; type < STATIC_STORY_ACTOR_MAX; ++type) {
    assert(StaticStoryActor_SelectTextId(type, &early) != 0);
    assert(StaticStoryActor_SelectTextId(type, &complete) != 0);
}
assert(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SHEIK, 0));
assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SHEIK, 2));
assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_FADO, 3));
```

- [ ] **Step 2: Run tests and verify failure**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure`

- [ ] **Step 3: Implement pure dialogue selectors**

Create a progression snapshot from save flags, then map each actor to verified existing vanilla text IDs. Prefer appropriate post-quest lines when their corresponding milestone is complete. Where a cutscene-only character has no standalone post-story selector, choose one curated existing line that closes normally and contains no message-script item/cutscene continuation.

- [ ] **Step 4: Implement talk lifecycle without native action callbacks**

Call `Actor_OfferTalk` at the definition's talk distance. On acceptance, set only `actor.textId` and wait for message close. Never call En_Ma1, En_Sa, En_Xc, En_Ru1, En_Ru2, En_Zl2, or En_Ko action functions. Restore the selected placement pose after the message closes.

- [ ] **Step 5: Add constrained conversational tracking**

Use the actor's native-compatible head/torso limb rotations only for standing idle poses while talk is offered or active. Return smoothly to neutral on close. Performance and seated pose descriptors carry `STATIC_POSE_FLAG_NO_TRACKING` and bypass the override.

- [ ] **Step 6: Run tests, build, and commit**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure && cmake --build build --target soh`

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c soh/tests/static_story_actor_test.c
git commit -m "feat: add safe static actor dialogue"
```

### Task 8: Placement Guide, Regression Verification, and CI Build

**Files:**
- Create: `docs/static-story-actors-poc4-params.md`
- Modify only if required by compile conflict: validated actor files from Tasks 1-7

**Interfaces:**
- Consumes: complete POC4 implementation and frozen weather branch.
- Produces: reviewable placement guide, formatted branch, cross-platform CI run, Windows artifact link.

- [ ] **Step 1: Create the human-readable placement guide**

Copy the approved parameter table from the spec and add columns for actor height class, collision behavior, dialogue milestone, tracking eligibility, and whether the pose emits audio. Include a warning that Prelude accepts the hexadecimal values directly and that seat-height poses must be placed on the intended seat surface.

- [ ] **Step 2: Run the complete focused checks**

Run: `cmake --build build --target static_story_actor_test concurrent_weather_audio_test soh`

Run: `ctest --test-dir build -R 'static_story_actor_test|concurrent_weather_audio_test' --output-on-failure`

Expected: both actor and frozen-weather tests pass and SoH links.

- [ ] **Step 3: Run format and diff checks before pushing**

Run: `clang-format --dry-run --Werror soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c soh/src/overlays/actors/ovl_En_Viewer/static_story_kokiri.c soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c soh/tests/static_story_actor_test.c`

Run: `git diff --check`

Expected: no formatting or whitespace errors.

- [ ] **Step 4: Commit the guide and final integration adjustments**

```bash
git add docs/static-story-actors-poc4-params.md
git commit -m "docs: add POC4 static actor placement guide"
```

- [ ] **Step 5: Push and monitor GitHub Actions**

Push the POC4 branch and inspect every failed job's first substantive compiler/test error before changing code. Apply minimal fixes on the same branch and rerun affected jobs. Do not modify frozen weather behavior to resolve unrelated actor compile failures.

- [ ] **Step 6: Perform the in-game acceptance matrix**

In one spaced test room, place every parameter from the guide and verify model, pose, blink, face, collision, target height, shadow, and dialogue with Alt Assets off and on. Separately enter the validated weather/Lost Woods test route and confirm rain, scene music, thunder, and room transitions are unchanged.

- [ ] **Step 7: Record POC4 outcomes**

Document each parameter as pass/fail, explicitly record whether Adult Zelda `0x7F14` uses the dynamic idle or neutral fallback, and record Impa face/rate results under both asset modes before proposing POC5.
