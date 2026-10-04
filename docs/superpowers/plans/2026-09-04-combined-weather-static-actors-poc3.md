# Combined Weather and Static Story Actors POC3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build one Windows Ship of Harkinian executable containing persistent concurrent proximity-weather audio and an expanded, interactive static story-actor roster.

**Architecture:** Weather and actors remain independent implementation tracks with separate tests and commits. Weather loads the native OoT thunder resources into an isolated one-shot mixer; static actors use a parameter registry plus character adapters for resources, animation, blinking, soft collision, and read-only talk behavior. A final integration task verifies both tracks together and publishes one GitHub Actions artifact.

**Tech Stack:** C/C++17, SoH actor/audio APIs, libultraship resource manager, ImGui enhancement UI, CMake/CTest, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-04-combined-weather-static-actors-poc3-design.md`

## Global Constraints

- Preserve current scene-BGM/rain concurrency and lightning flash timing.
- Never use `NA_SE_EV_LIGHTNING (0x282E)`, bomb SFX, boss SFX, or a repurposed shared SFX slot for weather thunder.
- Load `audio/samples/Low Thunder_META` and `audio/samples/Lightning_META` from the user's generated OoT archive.
- Vanilla actor behavior changes only when the reserved static parameter marker is present.
- Static dialogue never grants items, starts cutscenes, changes progression flags, or removes the actor.
- The Hyrule Warriors Adult Ruto model is compatible only when it preserves native Adult Ruto skeleton and material interfaces.
- The spin-attack archive is not modified by this plan.

---

### Task 1: Native Weather One-Shot Decoder and Mixer

**Files:**
- Create: `soh/src/code/weather_sample_player.h`
- Create: `soh/src/code/weather_sample_player.cpp`
- Create: `soh/tests/weather_sample_player_test.cpp`
- Modify: `soh/CMakeLists.txt`

**Interfaces:**
- Consumes: `ResourceMgr_LoadAudioSample(const char*)`, `SoundFontSample`, and the existing synthesized stereo PCM frame.
- Produces: `WeatherSamplePlayer_Init`, `WeatherSamplePlayer_Play`, `WeatherSamplePlayer_Mix`, and `WeatherSamplePlayer_Reset`.

- [ ] **Step 1: Write decoder and lifecycle tests**

```cpp
TEST_CASE("weather sample player rejects missing resources") {
    CHECK_FALSE(WeatherSamplePlayer_Play(nullptr, 1.0f));
}

TEST_CASE("weather sample player clamps gain") {
    CHECK(WeatherSamplePlayer_ClampGain(-1.0f) == 0.0f);
    CHECK(WeatherSamplePlayer_ClampGain(2.0f) == 1.0f);
}

TEST_CASE("weather sample player releases a completed one-shot") {
    WeatherSamplePlayer_TestInstallPcm({ 100, -100 });
    WeatherSamplePlayer_TestMixFrames(2);
    CHECK_FALSE(WeatherSamplePlayer_IsActive());
}
```

- [ ] **Step 2: Run the focused test and verify failure**

Run: `cmake --build build --target weather_sample_player_test && ctest --test-dir build -R weather_sample_player_test --output-on-failure`

Expected: compile failure because `weather_sample_player.h` and its functions do not exist.

- [ ] **Step 3: Implement isolated native-sample playback**

```cpp
extern "C" bool WeatherSamplePlayer_Play(const char* resourcePath, float gain);
extern "C" void WeatherSamplePlayer_Mix(int16_t* interleavedStereo, size_t frameCount);
extern "C" void WeatherSamplePlayer_Reset(void);
```

Decode codec 0/3 VADPCM using the proven predictor algorithm already used by the fork's direct-audio implementation. Cache decoded PCM by resource path, maintain at most two concurrent weather voices, saturating-add into the existing stereo synthesis buffer, and clear voices on completion/reset. Do not submit a second stream directly to the platform audio device.

- [ ] **Step 4: Attach the mixer at the final SoH PCM mix point**

Call `WeatherSamplePlayer_Mix` immediately before the completed SoH audio frame is handed to the backend. Multiply voice gain by the current global SFX volume and the weather-specific gain supplied at playback.

- [ ] **Step 5: Run tests and build the game target**

Run: `cmake --build build --target weather_sample_player_test soh && ctest --test-dir build -R weather_sample_player_test --output-on-failure`

Expected: focused tests pass and `soh` links on the local platform.

- [ ] **Step 6: Commit the isolated player**

```bash
git add soh/src/code/weather_sample_player.h soh/src/code/weather_sample_player.cpp soh/tests/weather_sample_player_test.cpp soh/CMakeLists.txt
git commit -m "feat: add isolated native weather sample player"
```

### Task 2: Weather POC3 State, UI, and Room Persistence

**Files:**
- Modify: `soh/src/code/concurrent_weather_audio.h`
- Modify: `soh/src/code/z_kankyo.c`
- Modify: `soh/src/overlays/actors/ovl_En_Weather_Tag/z_en_weather_tag.c`
- Modify: `soh/soh/Enhancements/audio/AudioEditor.cpp`
- Modify: `soh/tests/concurrent_weather_audio_test.c`

**Interfaces:**
- Consumes: `WeatherSamplePlayer_Play` and `WeatherSamplePlayer_Reset` from Task 1.
- Produces: thunder enable/style selection, rain/thunder gain CVars, persistent rain refresh state, and synchronized native thunder dispatch.

- [ ] **Step 1: Replace POC2 selector expectations with POC3 tests**

```c
assert(ConcurrentWeatherAudio_ShouldPlayThunder(false, 0) == false);
assert(ConcurrentWeatherAudio_ShouldPlayThunder(true, 0) == true);
assert(ConcurrentWeatherAudio_ClampPercent(-5) == 0);
assert(ConcurrentWeatherAudio_ClampPercent(125) == 100);
assert(ConcurrentWeatherAudio_ShouldRefreshRain(true, true) == true);
assert(ConcurrentWeatherAudio_ShouldRefreshRain(true, false) == false);
```

- [ ] **Step 2: Run the focused test and verify failure**

Run: `cmake --build build --target concurrent_weather_audio_test && ctest --test-dir build -R concurrent_weather_audio_test --output-on-failure`

Expected: compile failure for the new POC3 helpers.

- [ ] **Step 3: Implement POC3 thunder dispatch**

```c
typedef enum {
    CONCURRENT_WEATHER_THUNDER_LOW = 0,
    CONCURRENT_WEATHER_THUNDER_LAYERED = 1,
} ConcurrentWeatherThunderStyle;
```

At the existing `LIGHTNING_STRIKE_WAIT` transition, preserve all visual state changes. If thunder is enabled, play `Low Thunder_META`; for layered mode, also play `Lightning_META`. Clamp each gain and attenuate layered voices sufficiently to avoid summed clipping.

- [ ] **Step 4: Implement persistent rain refresh**

Track proximity-weather rain activation independently of the room-local actor lifetime. Refresh `NA_SE_EV_RAIN - SFX_FLAG` while the owning weather state is active, and clear the state on radius exit, weather shutdown, scene teardown, or a scene change. Do not set a room actor permanently global unless lifecycle tests prove cleanup is correct.

- [ ] **Step 5: Replace the visible POC2 controls**

```cpp
UIWidgets::EnhancementCheckbox("Enable Proximity Weather Thunder", CVAR_AUDIO("ProximityWeatherThunder"));
UIWidgets::CVarCombobox("Thunder Style", CVAR_AUDIO("ProximityWeatherThunderStyle"), thunderStyles);
UIWidgets::CVarSliderInt("Proximity Rain Volume: %d%%", CVAR_AUDIO("ProximityWeatherRainVolume"), 0, 100, 1);
UIWidgets::CVarSliderInt("Proximity Thunder Volume: %d%%", CVAR_AUDIO("ProximityWeatherThunderVolume"), 0, 100, 1);
```

Remove the boss-effect candidates and audition button. Disable the style and thunder-volume widgets when thunder is unchecked.

- [ ] **Step 6: Verify formatting, focused tests, and UI call presence**

Run: `clang-format --dry-run --Werror soh/src/code/z_kankyo.c soh/src/overlays/actors/ovl_En_Weather_Tag/z_en_weather_tag.c soh/soh/Enhancements/audio/AudioEditor.cpp`

Run: `ctest --test-dir build -R concurrent_weather_audio_test --output-on-failure`

Run: `rg -n 'Enable Proximity Weather Thunder|Thunder Style|Proximity Rain Volume|Proximity Thunder Volume' soh/soh/Enhancements/audio/AudioEditor.cpp`

Expected: formatting and tests pass; all four visible controls have draw calls.

- [ ] **Step 7: Commit Weather POC3**

```bash
git add soh/src/code/concurrent_weather_audio.h soh/src/code/z_kankyo.c soh/src/overlays/actors/ovl_En_Weather_Tag/z_en_weather_tag.c soh/soh/Enhancements/audio/AudioEditor.cpp soh/tests/concurrent_weather_audio_test.c
git commit -m "feat: add native proximity weather thunder controls"
```

### Task 3: Static Actor Parameter Registry and Common Interaction

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: validated static character/pose decoding, common blink state, soft cylinder collision, anchored placement, and read-only talk handling.

- [ ] **Step 1: Add failing registry and fallback tests**

```c
assert(StaticStoryActor_IsStatic(0x7F01));
assert(StaticStoryActor_Character(0x7F01) == STATIC_STORY_IMPA);
assert(StaticStoryActor_Pose(0x7F12) == STATIC_POSE_VARIANT_1);
assert(StaticStoryActor_IsStatic(0x0101) == false);
assert(StaticStoryActor_SanitizePose(STATIC_STORY_SARIA, 0xF) == STATIC_POSE_DEFAULT);
```

- [ ] **Step 2: Run the focused test and verify failure**

Run: `cmake --build build --target static_story_actor_test && ctest --test-dir build -R static_story_actor_test --output-on-failure`

Expected: failure for the new decoder and pose-sanitizing interfaces.

- [ ] **Step 3: Add common runtime state**

Extend `EnViewer` with a `ColliderCylinder`, blink timer/index, static character and pose IDs, and initialization flags. Initialize the collider once after the requested object is loaded; register soft OC collision each update; update cylinder position without applying movement to the actor.

- [ ] **Step 4: Add safe blinking and talk state machines**

```c
static void EnViewerStatic_UpdateBlink(EnViewer* this);
static void EnViewerStatic_OfferTalk(EnViewer* this, PlayState* play);
static u16 EnViewerStatic_GetTextId(const EnViewer* this);
```

Use randomized open-eye duration followed by open/half/closed/half/open frames. Characters without compatible eye textures remain open-eyed. `Actor_OfferTalk` uses the actor's body height; completing or repeating dialogue changes no inventory, event flag, Randomizer check, or actor action state.

- [ ] **Step 5: Verify common behavior and commit**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure`

Expected: parameter, fallback, blink, and dialogue-selection tests pass.

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c soh/tests/static_story_actor_test.c
git commit -m "feat: add common static npc interaction behavior"
```

### Task 4: Impa, Malon, and Saria Pose Adapters

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Consumes: Task 3 common interaction and registry.
- Produces: stable Impa head/eyes, Malon idle/singing variants, and Saria standing/hands-behind/seated-ocarina variants.

- [ ] **Step 1: Add failing character/pose mapping tests**

Cover the preserved values `0x7F01` (Impa), `0x7F02` (Malon idle), and `0x7F03` (Saria default), plus `0x7F12`/`0x7F22` for Malon and `0x7F13`/`0x7F23` for Saria.

- [ ] **Step 2: Implement verified native animations**

Map Malon singing to `gMalonChildSingAnim`. Map Saria variants to `gSariaHandsBehindBackWaitAnim` and the seated/ocarina animation resources; attach `gSariaRightHandAndOcarinaDL` in the appropriate limb callback. Do not spawn a stump.

- [ ] **Step 3: Correct Impa rendering and enable compatible eyes**

Use the unmasked Impa head display list for the static neutral variant and drive eye segments through the common blink index. Retain Alt Asset resource resolution.

- [ ] **Step 4: Gate Malon's audible song variant**

Reuse vanilla distance-based Malon song behavior only if it does not replace scene BGM or conflict with proximity weather. Otherwise preserve `0x7F22` as a safe silent-singing fallback and document the limitation in the parameter table.

- [ ] **Step 5: Run tests, build, and commit**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure && cmake --build build --target soh`

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c soh/tests/static_story_actor_test.c
git commit -m "feat: add static Malon Saria and Impa variants"
```

### Task 5: Kokiri Girls and Fado Static Modes

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Ko/z_en_ko.h`
- Modify: `soh/src/overlays/actors/ovl_En_Ko/z_en_ko.c`
- Create: `soh/src/overlays/actors/ovl_En_Ko/static_kokiri.h`
- Create: `soh/tests/static_kokiri_test.c`

**Interfaces:**
- Produces: high-byte `0x7F` static marker while preserving native low-byte Kokiri subtype IDs `0x01`, `0x05`, `0x06`, `0x09`, `0x0A`, and Fado `0x0C`.

- [ ] **Step 1: Add failing static subtype tests**

```c
assert(StaticKokiri_IsStatic(0x7F0C));
assert(StaticKokiri_Subtype(0x7F0C) == 0x0C);
assert(StaticKokiri_IsSupportedGirl(0x01));
assert(StaticKokiri_IsSupportedGirl(0x05));
assert(StaticKokiri_IsSupportedGirl(0x06));
assert(StaticKokiri_IsSupportedGirl(0x09));
assert(StaticKokiri_IsSupportedGirl(0x0A));
```

- [ ] **Step 2: Preserve native multipart rendering with static lifecycle**

Bypass scene/story kill conditions only for `0x7Fxx`. Reuse native skeleton, blink, head tracking where stable, collider, and rendering. Anchor the actor and replace movement/action transitions with a looping native standing idle and read-only talk action.

- [ ] **Step 3: Run tests and commit**

Run: `cmake --build build --target static_kokiri_test && ctest --test-dir build -R static_kokiri_test --output-on-failure`

```bash
git add soh/src/overlays/actors/ovl_En_Ko/z_en_ko.h soh/src/overlays/actors/ovl_En_Ko/z_en_ko.c soh/src/overlays/actors/ovl_En_Ko/static_kokiri.h soh/tests/static_kokiri_test.c
git commit -m "feat: add static Kokiri girl and Fado modes"
```

### Task 6: Ruto, Adult Zelda, and Sheik Adapters

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/tests/static_story_actor_test.c`
- Inspect and reuse where applicable: `soh/soh/Enhancements/randomizer/` Sheik placement hooks

**Interfaces:**
- Consumes: Task 3 common host behavior.
- Produces: Child Ruto, Adult Ruto, Adult Zelda, and Sheik neutral static variants with resource loading, safe idle, collision, blink compatibility, and read-only dialogue.

- [ ] **Step 1: Add failing roster/resource mapping tests**

For each new character, assert a unique parameter, valid object ID, valid default animation choice, nonzero collider dimensions, and nonzero safe text ID. Assert unknown values fall back without indexing outside the registry.

- [ ] **Step 2: Implement resource and animation adapters**

Use the native character objects and skeletons. Reuse Randomizer's proven Sheik initialization/visibility bypass where it is independent of reward logic. Do not call Adult Ruto's Water Temple approach, float, or cutscene-start actions.

- [ ] **Step 3: Implement per-character eyes and draw callbacks**

Bind the common blink index to each character's native eye textures. If a model lacks compatible half/closed textures, retain open eyes while preserving all other behavior. Keep native segment paths so compatible Alt Asset replacements, including Adult Ruto replacements, can inherit the draw contract.

- [ ] **Step 4: Run tests, build, and commit**

Run: `ctest --test-dir build -R static_story_actor_test --output-on-failure && cmake --build build --target soh`

```bash
git add soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c soh/tests/static_story_actor_test.c
git commit -m "feat: expand static story actor roster"
```

### Task 7: Prelude Parameter Table and Combined Verification

**Files:**
- Create: `docs/static-story-actor-prelude-parameters.md`
- Modify: `.github/workflows/generate-builds.yml` only if the existing branch workflow cannot publish the Windows artifact.

**Interfaces:**
- Consumes: final constants and validated behavior from Tasks 1–6.
- Produces: exact user placement reference and one combined Windows CI artifact.

- [ ] **Step 1: Generate the placement table from implemented constants**

Include Actor name/ID, Params, Character, Pose/audio mode, automatically loaded objects, and placement notes. Preserve every proven parameter and list the exact new values selected by the registry.

- [ ] **Step 2: Run the complete local verification set**

Run: `git diff --check`

Run: `clang-format --dry-run --Werror` on every changed C/C++ source and header.

Run: `ctest --test-dir build -R 'weather_sample_player|concurrent_weather_audio|static_story_actor|static_kokiri' --output-on-failure`

Run: `cmake --build build --target soh`

Expected: no whitespace or formatting failures; all focused tests pass; the game target links.

- [ ] **Step 3: Perform source-level integration checks**

Confirm that POC2 boss candidates are absent, all four POC3 weather controls have visible draw calls, every documented parameter exists in the registry, vanilla parameter paths remain reachable, and weather reset is called on scene teardown.

- [ ] **Step 4: Commit documentation**

```bash
git add docs/static-story-actor-prelude-parameters.md
git commit -m "docs: add static actor Prelude parameter guide"
```

- [ ] **Step 5: Push the combined branch and monitor CI**

Push only after local verification. Confirm `clang-format`, Windows generation, and Windows build jobs succeed. Provide the direct GitHub Actions run and Windows artifact links to the user.

- [ ] **Step 6: Execute the in-game acceptance matrix**

The user verifies every parameter, repeated conversations, soft collision, multiple blink cycles, Alt Assets on/off, minimal/full mod stacks, rain-only mode, both thunder styles, volume controls, and rain continuity across Lost Woods room boundaries. Record failures by subsystem and parameter before changing code.
