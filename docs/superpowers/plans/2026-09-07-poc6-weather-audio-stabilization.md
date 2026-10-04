# POC6 Weather and Audio Stabilization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore native and enhanced rain/SFX coexistence and make Hyrule Field night music deterministic across day/night cycles, fanfares, and the existing audio-randomization lifecycle.

**Architecture:** Keep rain-source policy and Hyrule Field music policy in small pure helpers covered by native tests. Runtime hooks translate engine state into those helpers; the audio editor remains the sole owner of sequence replacement mappings.

**Tech Stack:** C23, C++20, SoH GameInteractor hooks, AudioCollection, CMake/CTest, clang-format

**Spec:** `docs/superpowers/specs/2026-09-07-poc6-weather-static-actor-expansion-design.md`

## Global Constraints

- Do not bundle Lost Woods room assets or Prelude projects.
- Do not change existing weather CVar names or remove the Weather UI divider.
- Do not hardcode Lost Woods room numbers.
- Do not create a second RNG for Hyrule Field night music.
- Preserve thunder synchronization, thunder selection, rain color, rain volume, and intermittent fades.
- Weather must never reset or monopolize unrelated SFX, Navi voice, fanfare, or scene BGM playback.
- Run Linux, macOS, and Windows CI; retain the Windows artifact for the in-game pass.

---

### Task 1: Remove the disproven native story-NPC workaround

**Files:**
- Delete: `soh/soh/Enhancements/randomizer/StoryNpcCheckLifecycle.h`
- Delete: `soh/tests/story_npc_check_lifecycle_test.cpp`
- Modify: `soh/CMakeLists.txt`
- Modify: `soh/soh/Enhancements/randomizer/hook_handlers.cpp`
- Modify: `soh/src/overlays/actors/ovl_En_Ma1/z_en_ma1.c`
- Modify: `soh/src/overlays/actors/ovl_En_Sa/z_en_sa.c`

**Interfaces:**
- Consumes: native Malon/Saria spawn predicates and existing randomizer hooks
- Produces: the pre-workaround native behavior with no `StoryNpcCheckLifecycle` dependency or POC5 spawn diagnostics

- [ ] **Step 1: Record the exact workaround delta**

Run: `git diff a31a90a8..HEAD -- soh/CMakeLists.txt soh/soh/Enhancements/randomizer/hook_handlers.cpp soh/src/overlays/actors/ovl_En_Ma1/z_en_ma1.c soh/src/overlays/actors/ovl_En_Sa/z_en_sa.c soh/soh/Enhancements/randomizer/StoryNpcCheckLifecycle.h soh/tests/story_npc_check_lifecycle_test.cpp`

Expected: only the disproven lifecycle helper, its test/target, randomizer gate changes, and diagnostic logging are shown.

- [ ] **Step 2: Revert only those hunks**

Restore the six listed paths to their `a31a90a8` content, preserving every unrelated weather and static-actor change.

- [ ] **Step 3: Verify removal and native tests**

Run: `rg -n "StoryNpcCheckLifecycle|POC5.*(Malon|Saria)|native (Malon|Saria)" soh || true`

Expected: no lifecycle helper or temporary spawn diagnostic remains.

Run: `cmake --build build --target static_story_actor_test && ctest --test-dir build -R static_story_actor_test --output-on-failure`

Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add -A soh/CMakeLists.txt soh/soh/Enhancements/randomizer soh/tests soh/src/overlays/actors/ovl_En_Ma1 soh/src/overlays/actors/ovl_En_Sa
git commit -m "fix: remove disproven story NPC spawn workaround"
```

### Task 2: Replace the Hyrule Field ownership decision with an explicit lifecycle

**Files:**
- Modify: `soh/soh/Enhancements/audio/HyruleFieldNightMusic.h`
- Modify: `soh/soh/Enhancements/audio/HyruleFieldNightMusic.cpp`
- Modify: `soh/tests/hyrule_field_night_music_test.cpp`

**Interfaces:**
- Produces: `HyruleFieldNightMusicDecision HyruleFieldNightMusic_Select(const HyruleFieldNightMusicState&)`
- Produces: `uint16_t HyruleFieldNightMusic_ResolveSequence(uint16_t selected, uint16_t replacement)`
- Produces: explicit decisions `StartNight`, `RestoreNight`, `StopNightRestoreDay`, and `NoChange`

- [ ] **Step 1: Write failing lifecycle tests**

Extend `hyrule_field_night_music_test.cpp` so it asserts:

```cpp
state.isNight = true;
state.ownsNightBgm = false;
assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StartNight);
state.ownsNightBgm = true;
state.nightBgmPlaying = false;
state.fanfarePlaying = false;
assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::RestoreNight);
state.isNight = false;
assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StopNightRestoreDay);
assert(HyruleFieldNightMusic_ResolveSequence(0x21, 0x35) == 0x35);
```

Also cover two complete day/night cycles, a fanfare-active no-change state, leaving Hyrule Field, disabling the enhancement, and an explicit cutscene override.

- [ ] **Step 2: Run the focused test and confirm failure**

Run: `cmake --build build --target hyrule_field_night_music_test && ctest --test-dir build -R hyrule_field_night_music_test --output-on-failure`

Expected: compilation/test failure because the new state field and decisions do not exist.

- [ ] **Step 3: Implement the pure decision model**

Remove `WaitForNightAmbience` and `nightAmbienceReady`. Add `fanfarePlaying`. A cutscene/explicit override or active fanfare returns `NoChange`; night without ownership returns `StartNight`; lost night playback with ownership returns `RestoreNight`; dawn, disable, or leaving the field while owning returns `StopNightRestoreDay`.

- [ ] **Step 4: Run the focused test**

Run: `cmake --build build --target hyrule_field_night_music_test && ctest --test-dir build -R hyrule_field_night_music_test --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add soh/soh/Enhancements/audio/HyruleFieldNightMusic.h soh/soh/Enhancements/audio/HyruleFieldNightMusic.cpp soh/tests/hyrule_field_night_music_test.cpp
git commit -m "fix: model Hyrule Field night music lifecycle"
```

### Task 3: Connect night playback to existing sequence replacement

**Files:**
- Modify: `soh/soh/Enhancements/audio/HyruleFieldNightMusic.cpp`
- Modify: `soh/tests/hyrule_field_night_music_test.cpp`

**Interfaces:**
- Consumes: `AudioCollection::Instance->GetReplacementSequence(uint16_t)`
- Produces: one cached `sNightPlaybackSeq` per existing audio-editor mapping lifecycle

- [ ] **Step 1: Add failing sequence-resolution tests**

Assert that valid replacement `0x35` wins over configured `0x21`, invalid configured values fall back before replacement, and repeated restore decisions reuse the cached resolved value rather than resolve again.

- [ ] **Step 2: Run and confirm failure**

Run the `hyrule_field_night_music_test` target and expect the replacement/caching assertions to fail.

- [ ] **Step 3: Implement runtime resolution and restoration**

At the first `StartNight`, validate `HyruleFieldNightSequence`, call `AudioCollection::Instance->GetReplacementSequence(valid)`, validate the returned sequence, and cache it in `sNightPlaybackSeq`. `RestoreNight` queues that cached sequence. `StopNightRestoreDay` stops the owned SUB sequence and explicitly requests the current replacement for `NA_BGM_FIELD_LOGIC` on MAIN; it then clears ownership and the cache. Do not re-resolve on fanfare completion.

- [ ] **Step 4: Run focused tests and commit**

Run: `cmake --build build --target hyrule_field_night_music_test && ctest --test-dir build -R hyrule_field_night_music_test --output-on-failure`

Expected: PASS.

```bash
git add soh/soh/Enhancements/audio/HyruleFieldNightMusic.cpp soh/tests/hyrule_field_night_music_test.cpp
git commit -m "feat: randomize Hyrule Field night music through audio mappings"
```

### Task 4: Separate native and enhanced rain ownership

**Files:**
- Modify: `soh/soh/Enhancements/audio/GlobalOutdoorRain.h`
- Modify: `soh/soh/Enhancements/audio/GlobalOutdoorRain.cpp`
- Modify: `soh/tests/global_outdoor_rain_test.cpp`
- Modify: `soh/src/overlays/actors/ovl_En_Weather_Tag/z_en_weather_tag.c`

**Interfaces:**
- Produces: `GlobalOutdoorRainSource { None, NativePlaced, EnhancedOutdoor }`
- Produces: `GlobalOutdoorRainDecision GlobalOutdoorRain_Select(const GlobalOutdoorRainState&)`
- Consumes: native weather-tag activation state without room identifiers

- [ ] **Step 1: Write failing source-ownership tests**

Add cases proving that native placed rain wins over enhanced rain, disabling enhanced rain never stops native rain, room transition does not clear an active native source, and leaving outdoors stops only `EnhancedOutdoor` ownership.

- [ ] **Step 2: Run and confirm failure**

Run the `global_outdoor_rain_test` target; expect failure because source ownership is currently a Boolean.

- [ ] **Step 3: Implement source-aware policy**

Replace `ownsRain` in the pure state with the source enum. Export a minimal bridge notification from `En_Weather_Tag` when native rain activates/deactivates; do not pass scene or room numbers. `GlobalOutdoorRain_Update` may scale only density that it owns and must leave native density/lifecycle untouched.

- [ ] **Step 4: Run tests and commit**

Run: `cmake --build build --target global_outdoor_rain_test && ctest --test-dir build -R global_outdoor_rain_test --output-on-failure`

Expected: PASS.

```bash
git add soh/soh/Enhancements/audio/GlobalOutdoorRain.* soh/tests/global_outdoor_rain_test.cpp soh/src/overlays/actors/ovl_En_Weather_Tag/z_en_weather_tag.c
git commit -m "fix: preserve native placed rain ownership"
```

### Task 5: Remove the SFX-monopolizing persistent rain loop

**Files:**
- Modify: `soh/CMakeLists.txt`
- Modify: `soh/soh/Enhancements/audio/GlobalOutdoorRain.cpp`
- Modify: `soh/src/code/concurrent_weather_audio.c`
- Modify: `soh/src/code/concurrent_weather_audio.h`
- Modify: `soh/tests/concurrent_weather_audio_test.c`
- Modify: `soh/soh/Enhancements/audio/WeatherSamplePlayer.cpp`
- Modify: `soh/tests/weather_sample_player_test.cpp`

**Interfaces:**
- Consumes: `Audio_IsNatureRainEnabled()` and the existing concurrent-weather channel policy
- Produces: rain intensity routed without stopping unrelated SFX voices

- [ ] **Step 1: Add failing coexistence tests**

Extend the policy tests to assert rain, thunder, Navi voice, decorative flame, enemy, and pickup SFX are simultaneously serviceable. Add a mixer test with an active loop plus two one-shot voices and assert all samples contribute without clearing one another.

- [ ] **Step 2: Run and confirm failure**

Register `concurrent_weather_audio_test` in `soh/CMakeLists.txt` from `tests/concurrent_weather_audio_test.c` and `src/code/concurrent_weather_audio.c`, then run: `cmake --build build --target weather_sample_player_test concurrent_weather_audio_test`

Expected: the new multi-voice/ownership assertions fail or expose the current loop-reset path.

- [ ] **Step 3: Implement coexistence-safe rain audio**

Route enhanced rain through the existing native rain-channel policy when available. If the decoded sample mixer remains necessary as fallback, make `SetLoop` mutate only its private loop voice and never the engine SFX banks or one-shot voice list. Remove per-frame null-loop calls when enhanced rain does not own audio.

- [ ] **Step 4: Run the complete audio test set and commit**

Run: `ctest --test-dir build -R "(global_outdoor_rain|weather_sample_player|concurrent_weather_audio|hyrule_field_night_music)" --output-on-failure`

Expected: PASS.

```bash
git add soh/CMakeLists.txt soh/soh/Enhancements/audio soh/src/code/concurrent_weather_audio.* soh/tests
git commit -m "fix: preserve SFX during enhanced rain"
```

### Task 6: Weather release gate

**Files:**
- Modify: `docs/superpowers/specs/2026-09-07-poc6-weather-static-actor-expansion-design.md` only if verified behavior requires a factual correction

**Interfaces:**
- Produces: a weather-stable checkpoint that the actor plan consumes

- [ ] **Step 1: Format and run all native tests**

Run the repository clang-format check used by CI, then `ctest --test-dir build --output-on-failure`.

Expected: formatting clean; all tests PASS.

- [ ] **Step 2: Build the full project locally where supported**

Run: `cmake --build build --parallel 2`

Expected: build completes without new warnings promoted to errors.

- [ ] **Step 3: Push the weather checkpoint and run CI**

Push the branch only after local verification. Require Linux, macOS, and Windows jobs to complete; diagnose failures before actor work begins.

- [ ] **Step 4: Execute the in-game weather matrix**

Verify Lost Woods Room 4 native rain; persistent rain with Navi, enemies, torches, pickups, and fanfares; configured thunder; repeated Hyrule Field day/night cycles; randomization on/off; and exact dawn restoration. Record the tested commit hash and result.

- [ ] **Step 5: Commit any factual test-record update**

```bash
git add docs/superpowers/specs/2026-09-07-poc6-weather-static-actor-expansion-design.md
git commit -m "docs: record POC6 weather verification"
```
