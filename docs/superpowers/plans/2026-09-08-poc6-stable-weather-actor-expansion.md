# POC6 Stable-Weather Static Actor Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an extensible second static-actor catalogue page, native-style Adult Ruto water behavior, Darunia, Nabooru, and a visibly repaired Adult Zelda without importing unstable later weather work.

**Architecture:** Extend the pure `static_story_actor` registry for parameter decoding and actor metadata, and keep runtime-only behavior inside `En_Viewer`. Put Adult Ruto's water state decisions in a small pure module so grounded fallback, surface holding, and dive cycling can be tested without the game runtime. Reuse actor-owned skeletons and animations but never call the original actors' story action functions.

**Tech Stack:** C23, Ship of Harkinian actor runtime, `En_Viewer`, OTR object resources, CMake/CTest, direct GCC fallback tests.

**Spec:** `docs/superpowers/specs/2026-09-08-poc6-stable-weather-actor-expansion-design.md`

## Global Constraints

- Foundation is `b13d157843d4825d1718e6b279de9488ac489f4e`, whose ancestry contains the stable thunder-dropdown commit `d54a4c33da15eaf7eb8dd5eca65581b39c83d38c`.
- Do not merge or cherry-pick later global outdoor rain, private weather mixer, Hyrule Field night-music, or POC6 weather stabilization work.
- Preserve every existing `0x7Fxx` parameter and its behavior.
- New catalogue entries use the `0x7Exx` page; unsupported values kill the static viewer safely.
- Never call native story action functions, initiate cutscenes, grant rewards, set story flags, or alter native actor initialization.
- Ganondorf, mounted Ganondorf, Phantom Ganon, and the decorative Gerudo guard are out of scope.
- Production changes follow red-green-refactor; each task ends in a focused commit.

---

### Task 1: Add the `0x7Exx` Catalogue Page

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Consumes: existing `StaticStoryActor_IsParam`, `StaticStoryActor_GetType`, and `StaticStoryActor_GetPose` callers.
- Produces: page-aware parameter decoding; new actor types `STATIC_STORY_ACTOR_DARUNIA`, `STATIC_STORY_ACTOR_NABOORU`, and `STATIC_STORY_ACTOR_ADULT_RUTO_WATER`.

- [ ] **Step 1: Write failing page-decoding tests**

Add checks proving these exact assignments:

```c
assert(StaticStoryActor_IsParam((int16_t)0x7E01));
assert(StaticStoryActor_GetType((int16_t)0x7E01) == STATIC_STORY_ACTOR_DARUNIA);
assert(StaticStoryActor_GetType((int16_t)0x7E02) == STATIC_STORY_ACTOR_NABOORU);
assert(StaticStoryActor_GetType((int16_t)0x7E03) == STATIC_STORY_ACTOR_ADULT_RUTO_WATER);
assert(StaticStoryActor_GetPose((int16_t)0x7E11) == 1);
assert(StaticStoryActor_GetPose((int16_t)0x7E23) == 2);
assert(StaticStoryActor_GetType((int16_t)0x7E00) == STATIC_STORY_ACTOR_NONE);
assert(StaticStoryActor_GetType((int16_t)0x7E04) == STATIC_STORY_ACTOR_NONE);
assert(StaticStoryActor_GetType((int16_t)0x7D01) == STATIC_STORY_ACTOR_NONE);
assert(StaticStoryActor_GetType((int16_t)0x7F06) == STATIC_STORY_ACTOR_ADULT_RUTO);
```

- [ ] **Step 2: Run the test and verify RED**

Run the existing CMake target when available; otherwise compile the exact existing test target directly:

```bash
cc -std=c2x -Wall -Wextra -Werror -pedantic \
  -Isoh/include -Isoh/src/overlays/actors/ovl_En_Viewer \
  soh/tests/static_story_actor_test.c \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c \
  -o /tmp/static_story_actor_test
/tmp/static_story_actor_test
```

Expected: compile failure because the three new enum values do not exist.

- [ ] **Step 3: Implement page-aware decoding**

Add:

```c
#define STATIC_STORY_ACTOR_LEGACY_PARAM_PREFIX 0x7F00
#define STATIC_STORY_ACTOR_EXPANDED_PARAM_PREFIX 0x7E00
```

Keep legacy low-nibble decoding unchanged. For `0x7E00`, map low IDs explicitly:

```c
switch ((uint16_t)params & 0x000F) {
    case 1: return STATIC_STORY_ACTOR_DARUNIA;
    case 2: return STATIC_STORY_ACTOR_NABOORU;
    case 3: return STATIC_STORY_ACTOR_ADULT_RUTO_WATER;
    default: return STATIC_STORY_ACTOR_NONE;
}
```

`StaticStoryActor_IsParam` accepts exactly `0x7F00` and `0x7E00`; pose remains bits 4–7.

- [ ] **Step 4: Run the focused test and verify GREEN**

Run the Step 2 command. Expected: PASS with exit 0.

- [ ] **Step 5: Commit**

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c \
  soh/tests/static_story_actor_test.c
git commit -m "feat: add expanded static actor catalogue page"
```

---

### Task 2: Add a Pure Adult Ruto Water State Machine

**Files:**
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_ruto_water.h`
- Create: `soh/src/overlays/actors/ovl_En_Viewer/static_story_ruto_water.c`
- Create: `soh/tests/static_story_ruto_water_test.c`
- Modify: `soh/CMakeLists.txt`

**Interfaces:**
- Consumes: no game runtime types.
- Produces:

```c
typedef enum {
    STATIC_RUTO_GROUNDED = 0,
    STATIC_RUTO_SURFACE = 1,
    STATIC_RUTO_DIVE_LOOP = 2,
} StaticRutoWaterMode;

typedef enum {
    STATIC_RUTO_PHASE_GROUNDED,
    STATIC_RUTO_PHASE_SURFACED,
    STATIC_RUTO_PHASE_DIVING,
    STATIC_RUTO_PHASE_SUBMERGED,
    STATIC_RUTO_PHASE_RISING,
} StaticRutoWaterPhase;

typedef struct {
    StaticRutoWaterMode mode;
    StaticRutoWaterPhase phase;
    float homeY;
    float surfaceY;
    float currentY;
    float velocityY;
    uint16_t phaseTimer;
} StaticRutoWaterState;

void StaticRutoWater_Init(StaticRutoWaterState* state, StaticRutoWaterMode mode,
                          bool hasWater, float homeY, float surfaceY);
void StaticRutoWater_Update(StaticRutoWaterState* state, bool hasWater,
                            float surfaceY, bool animationEnded);
```

- [ ] **Step 1: Write failing state-machine tests**

Test all of these behaviors:

```c
StaticRutoWater_Init(&state, STATIC_RUTO_SURFACE, false, 10.0f, 0.0f);
assert(state.mode == STATIC_RUTO_GROUNDED);
assert(state.currentY == 10.0f);

StaticRutoWater_Init(&state, STATIC_RUTO_SURFACE, true, 10.0f, 100.0f);
assert(state.phase == STATIC_RUTO_PHASE_SURFACED);
assert(state.currentY == 46.0f); /* surface minus 54, matching En_Zo */

StaticRutoWater_Init(&state, STATIC_RUTO_DIVE_LOOP, true, 10.0f, 100.0f);
assert(state.phase == STATIC_RUTO_PHASE_SURFACED);
state.phaseTimer = 1;
StaticRutoWater_Update(&state, true, 100.0f, false);
assert(state.phase == STATIC_RUTO_PHASE_DIVING);
StaticRutoWater_Update(&state, true, 100.0f, true);
assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);

StaticRutoWater_Update(&state, false, 0.0f, false);
assert(state.mode == STATIC_RUTO_GROUNDED);
assert(state.phase == STATIC_RUTO_PHASE_GROUNDED);
assert(state.currentY == state.homeY);
```

Also test that a changed water surface updates the surfaced target and that a
complete submerged timer enters rising, then returns to surfaced.

- [ ] **Step 2: Register and run the test to verify RED**

Add `static_story_ruto_water_test` to `soh/CMakeLists.txt`, then run:

```bash
cc -std=c2x -Wall -Wextra -Werror -pedantic \
  -Isoh/src/overlays/actors/ovl_En_Viewer \
  soh/tests/static_story_ruto_water_test.c \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_ruto_water.c \
  -o /tmp/static_story_ruto_water_test
```

Expected: failure because the new files/API are absent.

- [ ] **Step 3: Implement the minimal deterministic state machine**

Use `surfaceY - 54.0f` as the visible surface target inherited from `En_Zo`.
Use explicit constants in the C file:

```c
enum { STATIC_RUTO_SURFACE_FRAMES = 80, STATIC_RUTO_SUBMERGED_FRAMES = 40 };
static const float sSurfaceOffset = 54.0f;
static const float sDiveVelocity = -4.0f;
static const float sRiseVelocity = 4.0f;
```

The pure state machine never touches dialogue, flags, cutscenes, actors, or
random state. Invalid water always restores `homeY` and grounded mode.

- [ ] **Step 4: Run the focused test to verify GREEN**

Compile and run the target. Expected: PASS with exit 0.

- [ ] **Step 5: Commit**

```bash
git add soh/CMakeLists.txt \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_ruto_water.h \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_ruto_water.c \
  soh/tests/static_story_ruto_water_test.c
git commit -m "feat: model static Adult Ruto water behavior"
```

---

### Task 3: Integrate Adult Ruto Grounded, Surface, and Dive Modes

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Consumes: `StaticRutoWaterState`, `StaticRutoWater_Init`, and `StaticRutoWater_Update` from Task 2.
- Produces: `0x7E03` grounded, `0x7E13` water-surface, and `0x7E23` dive-loop Adult Ruto placements.

- [ ] **Step 1: Write failing registry tests**

```c
assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 0)->animation ==
       STATIC_ANIM_ADULT_RUTO_IDLE);
assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 1)->waterMode ==
       STATIC_RUTO_SURFACE);
assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 2)->waterMode ==
       STATIC_RUTO_DIVE_LOOP);
assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, &complete) == 0x403E);
```

Extend `StaticStoryPoseDescriptor` with `uint8_t waterMode`; all old pose macros
initialize it to `STATIC_RUTO_GROUNDED`.

- [ ] **Step 2: Run and verify RED**

Run `static_story_actor_test`. Expected: compile failure for `waterMode`.

- [ ] **Step 3: Implement runtime integration**

Add `StaticRutoWaterState rutoWater` to `EnViewerStaticState`. During object
initialization, query:

```c
WaterBox* waterBox = NULL;
float surfaceY = 0.0f;
bool hasWater = WaterBox_GetSurface1(play, &play->colCtx,
                                     this->actor.world.pos.x,
                                     this->actor.world.pos.z,
                                     &surfaceY, &waterBox) != 0;
```

Initialize the pure state using the selected pose's `waterMode`. During update,
repeat the water query, update the pure state, copy `currentY` and `velocityY`
onto the actor, and select Adult Ruto idle/swim-up animation only on phase
transitions. Surface mode remains talkable and tracked; diving/submerged phases
disable attention, dialogue offers, and collision until surfaced again.

Do not call any `EnZo_*` or `EnRu2_*` action function. Reuse only constants,
animation resources, `WaterBox_GetSurface1`, optional native ripple/splash
effect calls, and ordinary actor movement helpers.

- [ ] **Step 4: Verify focused tests and static isolation**

Run both focused tests, then confirm forbidden calls are absent:

```bash
rg -n "EnZo_|EnRu2_|OnePointCutscene|Flags_Set|Item_Give" \
  soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c
```

Review matches individually; existing vanilla `En_Viewer` cutscene code may
match, but no new static runtime block may call them.

- [ ] **Step 5: Commit**

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c \
  soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h \
  soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c \
  soh/tests/static_story_actor_test.c
git commit -m "feat: add static Adult Ruto water modes"
```

---

### Task 4: Add Darunia Idle and Complete Dance Cycle

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Consumes: expanded parameter page from Task 1.
- Produces: `0x7E01` Darunia idle and `0x7E11` continuous native dance sequence.

- [ ] **Step 1: Write failing Darunia registry tests**

```c
assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_DARUNIA)->objectId == OBJECT_DU);
assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_DARUNIA, 0)->animation == STATIC_ANIM_DARUNIA_IDLE);
assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_DARUNIA, 1)->animation == STATIC_ANIM_DARUNIA_DANCE_1);
assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_DARUNIA, 1));
assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_DARUNIA, &complete) != 0);
```

- [ ] **Step 2: Run and verify RED**

Expected: new Darunia animation symbols are absent.

- [ ] **Step 3: Implement Darunia adapter**

Add `STATIC_SKELETON_DARUNIA`, `STATIC_ADAPTER_DARUNIA`, and animation IDs for
idle plus dance loops 1–4. Initialize `gDaruniaSkel`; set eye segments from
`gDaruniaEyeOpenTex`, `gDaruniaEyeOpeningTex`, `gDaruniaEyeShutTex`, and mouth
segment to `gDaruniaMouthHappyTex` during dance or `gDaruniaMouthSeriousTex`
during idle.

For pose 1, advance this exact cycle when each one-shot animation ends:

```text
gDaruniaDancingLoop1Anim
gDaruniaDancingLoop2Anim
gDaruniaDancingLoop3Anim
gDaruniaDancingLoop4Anim
repeat
```

Use the native corrected speeds `0.78f`, `0.78f`, `0.77f`, and `0.78f`.
Performance pose disables tracking; idle supports safe conversational tracking,
blinking, dialogue, and soft collision.

- [ ] **Step 4: Run the focused registry test and format check**

Expected: PASS; `clang-format-14 --dry-run --Werror` passes changed C/H files.

- [ ] **Step 5: Commit**

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c \
  soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c \
  soh/tests/static_story_actor_test.c
git commit -m "feat: add static Darunia dance adapter"
```

---

### Task 5: Add Nabooru Static Adapter

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Consumes: expanded parameter page from Task 1.
- Produces: `0x7E02` Nabooru standing-hands-on-hips placement.

- [ ] **Step 1: Write failing Nabooru tests**

```c
assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_NABOORU)->objectId == OBJECT_NB);
assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_NABOORU, 0)->animation == STATIC_ANIM_NABOORU_IDLE);
assert(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_NABOORU, 0));
assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_NABOORU, &complete) != 0);
```

- [ ] **Step 2: Run and verify RED**

Expected: Nabooru definition/animation are absent.

- [ ] **Step 3: Implement Nabooru adapter**

Use `OBJECT_NB`, `gNabooruSkel`, and
`gNabooruStandingHandsOnHipsChamberOfSagesAnim`. Bind eye segments to
`gNabooruEyeOpenTex`, `gNabooruEyeHalfTex`, and `gNabooruEyeClosedTex`; use the
closed-mouth head display list. Reproduce only the native head/torso limb
rotation convention after confirming limb indices in `En_Nb`; do not call its
action, path, capture, chamber, or reward handlers.

Enable blinking, conversational tracking, safe ordinary dialogue, and a soft
pushable cylinder.

- [ ] **Step 4: Run focused tests and forbidden-call scan**

Run `static_story_actor_test`; scan the new adapter block for `EnNb_`,
`Flags_Set`, `DoorWarp1`, and `Item_Give`. Expected: no story calls.

- [ ] **Step 5: Commit**

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c \
  soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c \
  soh/tests/static_story_actor_test.c
git commit -m "feat: add static Nabooru adapter"
```

---

### Task 6: Diagnose and Repair Adult Zelda Rendering

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/tests/static_story_actor_test.c`
- Create: `docs/debug/poc6-adult-zelda-rendering.md`

**Interfaces:**
- Consumes: existing `0x7F04` Adult Zelda placement.
- Produces: a visibly rendered Zelda with a documented resource/draw contract; parameter remains unchanged.

- [ ] **Step 1: Trace the native rendering contract before changing behavior**

Document a side-by-side comparison of static `En_Viewer` and native `En_Zl2`
for object dependencies, skeleton/animation objects, skeleton initialization,
joint-table state, limb callbacks, segments `0x08`–`0x0B`, OPA/XLU routing, and
model variant. Record the first contract difference capable of explaining both
an invisible skeleton and the yellow follower polygon.

- [ ] **Step 2: Add a failing contract test**

Expose a pure descriptor selector if required:

```c
typedef struct {
    int16_t skeletonObjectId;
    int16_t animationObjectId;
    uint8_t requiresAnimationObject;
    uint8_t drawOpa;
} StaticStoryRenderContract;

const StaticStoryRenderContract* StaticStoryActor_GetRenderContract(StaticStoryActorType type);
```

Assert the exact object and draw requirements proven by Step 1. Verify RED by
temporarily exercising the current incomplete Zelda contract.

- [ ] **Step 3: Apply the smallest root-cause repair**

Request every proven required object through `objectSlots`, wait for all slots,
set the skeleton and animation segments before initialization, initialize a
real neutral/idle animation if the native contract requires one, and use the
same skeleton draw API and segment bindings as the verified native path.

Do not add tracking or additional poses until Zelda renders correctly. Do not
mask the symptom by disabling limbs or substituting Child Zelda.

- [ ] **Step 4: Run tests and inspect the diff against the documented cause**

Run `static_story_actor_test`, `git diff --check`, and clang-format on changed
files. Confirm the code change corresponds only to the documented contract
difference.

- [ ] **Step 5: Commit**

```bash
git add docs/debug/poc6-adult-zelda-rendering.md \
  soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c \
  soh/tests/static_story_actor_test.c
git commit -m "fix: restore static Adult Zelda rendering"
```

---

### Task 7: Whole-Catalogue Regression Gate and Placement Guide

**Files:**
- Modify: `soh/tests/static_story_actor_test.c`
- Modify: `docs/static_story_actor_poc4_params.md`
- Modify: `soh/CMakeLists.txt` only if target wiring needs correction.

**Interfaces:**
- Consumes: all Tasks 1–6.
- Produces: final parameter catalogue and release evidence.

- [ ] **Step 1: Convert the static test to release-safe checks**

Replace side-effect-bearing `assert` use with an unconditional `REQUIRE`
helper so Release/`NDEBUG` retains all checks. Add a table-driven pass over every
published parameter, confirming definition availability, valid pose, nonzero
object ID, safe dialogue, and expected tracking policy.

- [ ] **Step 2: Run normal and `NDEBUG` focused suites**

Build and run `static_story_actor_test` and `static_story_ruto_water_test` with
strict warnings in normal mode and with `-DNDEBUG`. Expected: all four runs pass.

- [ ] **Step 3: Run branch-isolation and formatting checks**

```bash
git diff --check b13d157843d4825d1718e6b279de9488ac489f4e..HEAD
git diff --name-only b13d157843d4825d1718e6b279de9488ac489f4e..HEAD | \
  rg 'GlobalOutdoorRain|WeatherSamplePlayer|HyruleFieldNightMusic'
```

Expected: diff check passes; weather-regression file scan returns no matches.
Run clang-format 14 on every changed C/C++ header and source file.

- [ ] **Step 4: Update the human-readable catalogue**

Document at minimum:

| Parameter | Actor | Mode |
|---|---|---|
| `0x7E01` | Darunia | Idle |
| `0x7E11` | Darunia | Full dance cycle |
| `0x7E02` | Nabooru | Standing idle |
| `0x7E03` | Adult Ruto | Grounded idle |
| `0x7E13` | Adult Ruto | Water-surface idle |
| `0x7E23` | Adult Ruto | Dive loop |
| `0x7F04` | Adult Zelda | Repaired standing model |

Retain the full existing POC4 table and include collision, blinking, tracking,
dialogue, water requirement, and experimental status columns.

- [ ] **Step 5: Commit the release gate**

```bash
git add soh/tests/static_story_actor_test.c docs/static_story_actor_poc4_params.md soh/CMakeLists.txt
git commit -m "test: gate expanded static actor catalogue"
```

- [ ] **Step 6: External gates**

After explicit authorization, push `codex/poc6-actor-expansion-stable` and run
GitHub CI. The user runtime matrix covers native Malon/Saria/Child Ruto,
existing POC4 placements, Adult Ruto on land and in shallow/deep water plus
room re-entry, Darunia's entire dance loop, Nabooru, and Zelda under vanilla
and alternate assets.
