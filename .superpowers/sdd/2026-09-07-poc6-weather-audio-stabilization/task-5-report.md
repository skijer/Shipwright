# Task 5 report: preserve SFX during enhanced rain

Base: `1a15c2fc6ac675dac4bb16365f17e4811503dc09`.

## Status and findings

Implemented and focused tests pass. No broader runtime audio changes were made.
The base mixer already used separate private loop and two one-shot voices; investigation did not find engine-bank resets in `SetLoop`. The reproduced defect was stale loop playback when an intermittent cycle or mode change reached zero intensity. The updater also sent unconditional null-loop requests without owning enhanced audio.

## Changes

- Added `ConcurrentWeatherAudio_SelectRainAction` in a new C implementation file (the brief-listed file did not previously exist). It prefers already-active native nature rain, otherwise selects only the private decoded loop. It never starts/replaces BGM or changes SFX banks.
- Tracks fallback ownership and releases it once on zero gain, native takeover or teardown. Frames without enhanced ownership no longer send per-frame null-loop requests.
- Calls the audio route at zero intensity as well as positive intensity, preserving existing volume scaling and visual fades while ending stale audio during dry intervals.
- Preserved the already-isolated mixer implementation; documented the loop-only mutation contract.
- Registered the concurrent policy test, upgraded the mixer target to compile actual production mixer code, and added an actual updater-plus-mixer integration target.
- Added narrow test boundary stubs for resources, CVars, hooks and the required runtime types; no production test-only API was added.
- Added enhanced-to-native bridge takeover coverage across indoor/outdoor updates and native deactivation, addressing the Task 4 deferred integration-test gap in the real updater.

## Test-first evidence

Before production edits, the real updater/mixer integration binary failed with exit 134:

```text
global_outdoor_rain_audio_test.cpp:47: Assertion `MixFrame() == 100' failed.
```

This is the persistent-to-intermittent dry transition retaining the old rain loop. The new policy test initially failed compilation for the missing `ConcurrentWeatherAudio_SelectRainAction` interface. The upgraded multi-voice mixer characterization passed against the base implementation, confirming that its existing voice isolation should be retained rather than rewritten.

## Verification

Fresh strict-warning direct builds and runs passed (exit 0):

```text
cc -std=c11 -Wall -Wextra -Werror -pedantic soh/tests/concurrent_weather_audio_test.c soh/src/code/concurrent_weather_audio.c -o /tmp/poc6-task5-policy
/tmp/poc6-task5-policy
cc -std=c11 -Wall -Wextra -Werror -pedantic -c soh/src/code/concurrent_weather_audio.c -o /tmp/poc6-task5-policy.o
c++ -std=c++20 -Wall -Wextra -Werror -pedantic '-DCVAR_PREFIX_SETTING="gSettings"' '-DCVAR_PREFIX_AUDIO="gAudioEditor"' -Isoh/tests/weather_audio_stubs -Isoh -Isoh/src soh/tests/global_outdoor_rain_audio_test.cpp soh/soh/Enhancements/audio/GlobalOutdoorRain.cpp soh/soh/Enhancements/audio/WeatherSamplePlayer.cpp /tmp/poc6-task5-policy.o -o /tmp/poc6-task5-runtime
/tmp/poc6-task5-runtime
c++ -std=c++20 -Wall -Wextra -Werror -pedantic '-DCVAR_PREFIX_SETTING="gSettings"' -Isoh/tests/weather_audio_stubs -Isoh soh/tests/weather_sample_player_test.cpp soh/soh/Enhancements/audio/WeatherSamplePlayer.cpp -o /tmp/poc6-task5-mixer
/tmp/poc6-task5-mixer
c++ -std=c++20 -Wall -Wextra -Werror -pedantic -DGLOBAL_OUTDOOR_RAIN_TEST -Isoh soh/tests/global_outdoor_rain_test.cpp soh/soh/Enhancements/audio/GlobalOutdoorRain.cpp -o /tmp/poc6-task5-global
/tmp/poc6-task5-global
build-poc5-tests-manual/hyrule_field_night_music_test
build-poc5-tests-manual/static_story_actor_test
git diff --check
```

The mixer test checks literal sample sums for rain plus both one-shots over a pre-existing stereo engine mix, continuity through loop gain changes/stops, SFX-volume mute, one-shot completion, and loop wrap. Runtime tests cover native-channel priority and fallback resumption, mode reset, intermittent dry state, native bridge ownership through room updates, zero volume, disable fade, teardown, and repeated non-owning updates/resets.

## Self-review and concerns

- Thunder timing/selection, CVar names, rain color/density/fade formulas, and Weather UI were not modified.
- Native rain priority uses `Audio_IsNatureRainEnabled()`; no nature sequence is force-started because doing so would replace MAIN BGM.
- The decoder and sample mixing arithmetic remain unchanged. `SetLoop` still mutates only `sLoopVoice`, while engine audio enters as an already-populated output buffer.
- Local CMake/CTest and full production dependencies remain unavailable. CI must validate CMake integration/full-platform compilation. Night-music and static-actor checks used existing reviewed binaries; affected policy, mixer and rain code was freshly compiled.
- These tests verify weather routing and preservation of the incoming engine mix, not live Navi/flame/enemy/pickup/fanfare audibility or engine bank allocation under gameplay load. Full in-game listening remains required before declaring the user-reported masking symptom eliminated.
- No independent reviewer was spawned. Work stopped at the requested clean, tested checkpoint.

## Review fix round 1 (base `38d4cca9`)

Both Important findings addressed without modifying production runtime behavior.

### Dispatch audit and coexistence coverage

- `code_800F7260.c:Audio_PlaySoundGeneral` filters muted banks and applies configured swaps before enqueueing; `Audio_ProcessSoundRequest` applies the audio-editor replacement and allocates bank entries; `Audio_ChooseActiveSounds` and `Audio_PlayActiveSounds` select and dispatch real SFX channels. There is no weather-specific filter in these functions. Weather rain/thunder instead use the private decoded mixer or native nature channels (`GlobalOutdoorRain.cpp`, `z_kankyo.c:Environment_UpdateLightningStrike`, `code_800EC960.c:Audio_IsNatureRainEnabled/Audio_IsNatureLightningEnabled`).
- The expanded `global_outdoor_rain_audio_test` compiles unmodified `code_800F7260.c` and `audio_sound_params.c` alongside the real updater/mixer. It submits `NA_SE_VO_NA_HELLO_2` (Navi), `NA_SE_EV_TORCH - SFX_FLAG` (the continuous decorative-flame ID used by En_Light/Obj_Syokudai), `NA_SE_EN_STALKID_ATTACK`, and `NA_SE_SY_GET_RUPY` **after** rain and both thunder layers start. Each must receive a distinct active engine channel and an emitted start command, with an unmuted bank. Checks continue through native-rain takeover, fallback resumption, dry transitions and teardown; torch requests are refreshed while the one-shots retain their voices.
- Native-rain eligibility now runs the production concurrent-weather policy using tracked nature-channel state. Both fallback rain plus layered thunder and native rain plus layered thunder are covered; enabling rain alone must not suppress thunder eligibility. Literal decoded sums independently verify that fallback rain/thunder are contributing while the engine voices remain assigned.
- Removed the misleading Navi/flame/enemy/pickup labels on arbitrary incoming PCM in the isolated mixer test. That test proves additive mixing, while the updater/engine test proves request and voice serviceability.
- The engine fixture supplies the supported default channel-layout row from `code_800EC960.c`, valid sequence-channel IO, disabled randomization, deterministic random values, and captured synth transport. The real request filtering, allocation, priority tables and channel selection are not mocked. No new production audio subsystem or test-only production API was introduced.

### Release-safe checks

Added shared C/C++ `tests/test_require.h`: `REQUIRE` always evaluates and aborts with file/line/expression diagnostics, independently of `NDEBUG`. Converted all four weather tests and the resource fixture. Voice starts, mixing, and render-color output writes are performed outside check expressions. No `-UNDEBUG` or compiler-specific assertion override is used.

### Fresh direct build/run evidence

The following exact Bash build/run loop completed with exit 0. All eight suite/configuration combinations printed PASS. The two warning suppressions apply only to unchanged legacy allocator code (unused decompilation locals and a signed/unsigned array-count comparison); every changed test/fixture and the other production inputs compiled with strict warnings as errors.

```bash
set -e
for variant in normal ndebug; do
  define_flags=()
  if [ "$variant" = ndebug ]; then define_flags=(-DNDEBUG); fi
  c_flags=(-std=c11 -Wall -Wextra -Werror -pedantic "${define_flags[@]}")
  cpp_flags=(-std=c++20 -Wall -Wextra -Werror -pedantic "${define_flags[@]}")
  includes=(-Isoh/tests/weather_audio_stubs -Isoh -Isoh/src)
  cc "${c_flags[@]}" soh/tests/concurrent_weather_audio_test.c soh/src/code/concurrent_weather_audio.c -o "/tmp/poc6-task5-fix-$variant-policy"
  cc "${c_flags[@]}" -c soh/src/code/concurrent_weather_audio.c -o "/tmp/poc6-task5-fix-$variant-policy.o"
  cc "${c_flags[@]}" "${includes[@]}" -c soh/tests/weather_sfx_engine_fixture.c -o "/tmp/poc6-task5-fix-$variant-fixture.o"
  cc "${c_flags[@]}" -Wno-unused-variable -Wno-sign-compare "${includes[@]}" -c soh/src/code/code_800F7260.c -o "/tmp/poc6-task5-fix-$variant-engine.o"
  cc "${c_flags[@]}" "${includes[@]}" -c soh/src/code/audio_sound_params.c -o "/tmp/poc6-task5-fix-$variant-params.o"
  c++ "${cpp_flags[@]}" '-DCVAR_PREFIX_SETTING="gSettings"' "${includes[@]}" soh/tests/weather_sample_player_test.cpp soh/soh/Enhancements/audio/WeatherSamplePlayer.cpp -o "/tmp/poc6-task5-fix-$variant-mixer"
  c++ "${cpp_flags[@]}" -DGLOBAL_OUTDOOR_RAIN_TEST -Isoh soh/tests/global_outdoor_rain_test.cpp soh/soh/Enhancements/audio/GlobalOutdoorRain.cpp -o "/tmp/poc6-task5-fix-$variant-global"
  c++ "${cpp_flags[@]}" '-DCVAR_PREFIX_SETTING="gSettings"' '-DCVAR_PREFIX_AUDIO="gAudioEditor"' "${includes[@]}" soh/tests/global_outdoor_rain_audio_test.cpp soh/soh/Enhancements/audio/GlobalOutdoorRain.cpp soh/soh/Enhancements/audio/WeatherSamplePlayer.cpp "/tmp/poc6-task5-fix-$variant-policy.o" "/tmp/poc6-task5-fix-$variant-fixture.o" "/tmp/poc6-task5-fix-$variant-engine.o" "/tmp/poc6-task5-fix-$variant-params.o" -o "/tmp/poc6-task5-fix-$variant-runtime"
  for suite in policy mixer global runtime; do
    "/tmp/poc6-task5-fix-$variant-$suite"
    echo "PASS $variant $suite"
  done
done
```

### Mutation evidence (normal AND `-DNDEBUG`)

Mutation inputs were streamed from the production sources through `sed` into the compiler (`-x c -` / `-x c++ -`), leaving repository sources untouched. The build flags and inputs were those above, with `-Isoh/src/code -Isoh/soh/Enhancements/audio` added for stdin-source local includes. Each mutated object replaced only its corresponding normal object. `ulimit -c 0` disabled core files. All **16** negative runs below exited **134**, checked explicitly by the shell; none passed silently with `NDEBUG`.

| Production mutation | Affected binary suffix (both configurations) | Exact observed check failure |
| --- | --- | --- |
| In `SelectRainAction`, replace `return CONCURRENT_WEATHER_RAIN_SET_LOOP;` with `return CONCURRENT_WEATHER_RAIN_NO_CHANGE;` | `policy` | `concurrent_weather_audio_test.c:11: REQUIRE(ConcurrentWeatherAudio_SelectRainAction(0, 1, 0, 0.5f) == CONCURRENT_WEATHER_RAIN_SET_LOOP) failed` |
| Insert `return GlobalOutdoorRainDecision::NoChange;` at the start of `GlobalOutdoorRain_Select` | `global` | `global_outdoor_rain_test.cpp:15: REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::Start) failed` |
| Insert `sVoices = {};` at the start of `WeatherSamplePlayer_SetLoop` | `mixer` and `runtime` | `weather_sample_player_test.cpp:30: REQUIRE(next[0] == 108 && next[1] == -92) failed`; `global_outdoor_rain_audio_test.cpp:43: REQUIRE(buffer[0] == expected) failed` |
| Reject bank 6 in real `Audio_PlaySoundGeneral` | `bank6` | `Missing engine voice for SFX 0x685f`; `weather_sfx_engine_fixture.c:85: REQUIRE(found) failed` |
| Reject bank 2 in real `Audio_PlaySoundGeneral` | `bank2` | `Missing engine voice for SFX 0x2031`; same `REQUIRE(found)` failure |
| Reject bank 3 in real `Audio_PlaySoundGeneral` | `bank3` | `Missing engine voice for SFX 0x3831`; same `REQUIRE(found)` failure |
| Reject bank 4 in real `Audio_PlaySoundGeneral` | `bank4` | `Missing engine voice for SFX 0x4803`; same `REQUIRE(found)` failure |

Exact dispatcher mutation/build command used inside `for bank in 6 2 3 4` for each configuration:

```bash
sed "s/if (!gSoundBankMuted\[SFX_BANK_SHIFT(sfxId)\])/if (SFX_BANK(sfxId) != $bank \&\& !gSoundBankMuted[SFX_BANK_SHIFT(sfxId)])/" soh/src/code/code_800F7260.c | cc "${c_flags[@]}" -Wno-unused-variable -Wno-sign-compare "${includes[@]}" -x c - -c -o "/tmp/poc6-task5-mut-$variant-bank$bank.o"
```

After mutation checks, all eight unmutated binaries passed again and `git diff --check` passed. A source audit found no remaining `assert(...)` in the affected weather tests/fixtures.

### Remaining concerns

- This supersedes the earlier report's lack of engine-allocation coverage, but is still not a listening test: synth PCM generation, attenuation, configured audio randomization and native nature-sequence playback are outside the fixture. Default-layout eligibility and private weather PCM coexistence are verified, not audible loudness under arbitrary gameplay load.
- `cmake` and `ctest` are not installed in this environment. CMake target inputs were updated, but full CMake/CTest/platform compilation still requires CI.
- No production runtime source changed in this fix round; thunder timing, routing behavior, UI and rain visuals remain as reviewed. No subagent was spawned.
