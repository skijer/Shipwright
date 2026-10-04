#!/usr/bin/env bash
# Standalone equivalents of focused CMake targets; does not build soh.exe.
set -euo pipefail
cd "$(dirname "$0")/../.."
stabilization_build=$(mktemp -d /tmp/soh-stabilization.XXXXXX)
printf 'Test build directory: %s\n' "$stabilization_build"
cxx=${CXX:-c++}
cc=${CC:-cc}
common=(-g -DNDEBUG)
audio_inc=(-Isoh/tests/audio_sequence_stubs -Isoh/include -Isoh)
weather_inc=(-Isoh/tests/weather_audio_stubs -Isoh/src -Isoh)
json_inc=()
if [[ -n ${SOH_TEST_JSON_INCLUDE:-} ]]; then json_inc=(-I"$SOH_TEST_JSON_INCLUDE"); fi
weather_defs=('-DCVAR_PREFIX_SETTING="gSettings"' '-DCVAR_PREFIX_AUDIO="gAudioEditor"')

"$cc" -std=c11 "${common[@]}" "${audio_inc[@]}" soh/tests/audio_font_id_test.c \
    -o "$stabilization_build/audio_font_id_test"

"$cc" -std=c11 "${common[@]}" "${audio_inc[@]}" -c soh/src/code/code_800F9280.c -o "$stabilization_build/seq.o"
"$cxx" -std=c++20 "${common[@]}" "${audio_inc[@]}" -DHYRULE_FIELD_NIGHT_MUSIC_TEST \
    soh/tests/hyrule_field_night_music_test.cpp soh/soh/Enhancements/audio/HyruleFieldNightMusic.cpp \
    "$stabilization_build/seq.o" -o "$stabilization_build/hyrule_field_night_music_test"
"$cxx" -std=c++20 "${common[@]}" -Isoh/tests/night_runtime_stubs "${audio_inc[@]}" \
    '-DCVAR_PREFIX_AUDIO="gAudioEditor"' soh/tests/hyrule_field_night_runtime_test.cpp \
    soh/soh/Enhancements/audio/HyruleFieldNightMusic.cpp "$stabilization_build/seq.o" \
    -o "$stabilization_build/hyrule_field_night_runtime_test"
"$cxx" -std=c++20 "${common[@]}" soh/tests/mm_display_list_patch_test.cpp \
    soh/mods/transformation_masks/assets/mm_display_list_patch.cpp -o "$stabilization_build/mm_display_list_patch_test"
"$cc" -std=c11 "${common[@]}" -Isoh/include soh/tests/static_story_mm_actor_test.c \
    soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c \
    soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c -lm -o "$stabilization_build/static_story_mm_actor_test"
"$cc" -std=c11 "${common[@]}" -Isoh/include soh/tests/static_story_actor_test.c \
    soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c -o "$stabilization_build/static_story_actor_test"
"$cc" -std=c11 "${common[@]}" soh/tests/static_story_ruto_water_test.c \
    soh/src/overlays/actors/ovl_En_Viewer/static_story_ruto_water.c -lm -o "$stabilization_build/static_story_ruto_water_test"
"$cc" -std=c11 "${common[@]}" -Isoh/include soh/tests/static_story_ganon_test.c \
    soh/src/overlays/actors/ovl_En_Viewer/static_story_ganon.c \
    soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c -lm -o "$stabilization_build/static_story_ganon_test"

"$cxx" -std=c++20 "${common[@]}" -DGLOBAL_OUTDOOR_RAIN_TEST -Isoh/include -Isoh \
    soh/tests/global_outdoor_rain_test.cpp soh/soh/Enhancements/audio/GlobalOutdoorRain.cpp \
    -o "$stabilization_build/global_outdoor_rain_test"
"$cc" -std=c11 "${common[@]}" soh/tests/concurrent_weather_audio_test.c soh/src/code/concurrent_weather_audio.c \
    -o "$stabilization_build/concurrent_weather_audio_test"
"$cxx" -std=c++20 "${common[@]}" "${weather_defs[@]}" "${weather_inc[@]}" \
    soh/tests/weather_sample_player_test.cpp soh/soh/Enhancements/audio/WeatherSamplePlayer.cpp \
    -o "$stabilization_build/weather_sample_player_test"
for source in soh/tests/weather_sfx_engine_fixture.c soh/src/code/concurrent_weather_audio.c \
              soh/src/code/code_800F7260.c soh/src/code/audio_sound_params.c; do
    "$cc" -std=c11 "${common[@]}" "${weather_defs[@]}" "${weather_inc[@]}" -c "$source" \
        -o "$stabilization_build/$(basename "$source").o"
done
"$cxx" -std=c++20 "${common[@]}" "${weather_defs[@]}" "${weather_inc[@]}" \
    soh/tests/global_outdoor_rain_audio_test.cpp soh/soh/Enhancements/audio/GlobalOutdoorRain.cpp \
    soh/soh/Enhancements/audio/WeatherSamplePlayer.cpp "$stabilization_build/"*.c.o \
    -o "$stabilization_build/global_outdoor_rain_audio_test"
"$cxx" -std=c++20 "${common[@]}" -DSCENE_RAIN_POLICY_TEST -Isoh "${json_inc[@]}" \
    soh/tests/scene_rain_policy_test.cpp soh/soh/Enhancements/audio/SceneRainPolicy.cpp \
    -o "$stabilization_build/scene_rain_policy_test"
"$cxx" -std=c++20 "${common[@]}" -Isoh/tests/scene_rain_stubs -Isoh/include -Isoh \
    -Ilibultraship/include "${json_inc[@]}" soh/tests/scene_rain_binding_test.cpp \
    soh/soh/Enhancements/audio/SceneRainPolicy.cpp -o "$stabilization_build/scene_rain_binding_test"
for test_binary in "$stabilization_build/"*_test; do
    "$test_binary"
    printf 'PASS %s\n' "$(basename "$test_binary")"
done
python3 -B -m unittest discover -s scripts/diagnostics -p 'test_audit_skull_kid.py' -v
python3 -B scripts/diagnostics/run_audio_runtime_test.py "$stabilization_build" "$cc"
python3 -B scripts/diagnostics/run_night_combat_test.py "$stabilization_build" "$cc" "$cxx"
python3 -B scripts/diagnostics/run_rain_runtime_test.py "$stabilization_build/rain" "$cc" "$cxx"
python3 -B scripts/diagnostics/run_time_gate_chest_tests.py
python3 -B scripts/diagnostics/run_child_ruto_face_test.py
python3 -B scripts/diagnostics/run_custom_item_color_tests.py

# Optional read-only real-archive check: pass the path to mm.o2r as argument 1.
if [[ $# -gt 0 ]]; then
    "$cxx" -std=c++20 "${common[@]}" -shared -fPIC soh/tests/mm_display_list_patch_bridge.cpp \
        soh/mods/transformation_masks/assets/mm_display_list_patch.cpp -o "$stabilization_build/patch.so"
    python3 -B scripts/diagnostics/test_skull_cull_archive.py "$stabilization_build/patch.so" "$1"
fi
