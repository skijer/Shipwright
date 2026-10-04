# Midna as Navi — SoH POC1

Status: implemented, focused CPU checks pass; full executable build and in-game acceptance are separate gates.

## Candidate and preservation

| Field | Record |
|---|---|
| Baseline | `integration/nei-weather-static-actors`, `7eefc9986d4b641400b500f88c79a6abf8367895` |
| Candidate | Prepared on `poc/midna-navi-soh-poc1`; authorized for publication to `integration/nei-weather-static-actors` |
| Source | User-provided `Wii U - Hyrule Warriors - Playable Characters - Midna.zip` |
| Source SHA-256 | `901d738e1b26b33edfd865bcd61de2026c6c2a3ec48b4cd6274d26c531253183` |
| Combined model/audio pack | `Midna_Navi_SoH_POC1_Model_Audio.o2r` |
| Pack SHA-256 | `1d3ab51863432b1948c53117331fd35c27a6786c63d0ba2cc4fa4ad129dfb810` |
| Scope | Navi-only model draw and optional private sound clips |
| Protected | Native companion logic, targeting, hints, dialogue, visibility, emergence scale; healing/bottled/Kokiri fairies; shared samples, music, weather and Link voice packs |
| Promotion | Runtime-unaccepted candidate. Publication is authorized on top of the exact integration baseline; prior history is retained. ComboShip port and accepted-master promotion remain deferred. |

The uploaded OBJ has no rig or animations. This POC bakes a floating pose and adds slight draw-only sway. Native Navi travel, appearance/shrink and alpha remain in control. Full limb animation and blinking are not implemented.

The model's three private resources live under `objects/midna_navi/poc1/`; there are no `alt/` entries or replacements for `gameplay_keep`. The source texture is retained. The exporter preserves all 12,842 source triangles. Native fairy cosmetics size applies; Navi target color does not tint Midna. A failed model load falls back to native Navi.

Midna uses depth testing for her solid mesh, so Link and scenery can occlude her. Native Navi's glow draws through geometry. The existing hidden-state, first-person, scale and alpha gates are preserved; glow-style occlusion is intentionally not copied to the solid model.

The shared child/adult draw scale is 60% of the original export. At native Navi scale `0.008` and Fairy Size `1.0`, the packed mesh is **21.4608 world units tall, helmet included** (about 49% of the child height reference and 32% of the adult reference from `Player_GetHeight`). This is a uniform draw-matrix multiplier: the original vertices, normals, UVs, texture, pose and all audio bytes remain unchanged. An offline experiment rounding resized vertex coordinates collapsed 18 tiny triangles; that experiment was rejected and is not packaged. Native emergence/shrink and the Fairy Size cosmetic multiplier still apply.

## Sound routing

`NA_SE_EV_FAIRY_DASH` is shared outside Navi, including `En_Partner`. Replacing its bank/sample globally would affect those users. Instead, this candidate intercepts specific Navi callsites and the HUD's Navi calls. No bank IDs, sample data or global sound mappings change.

Optional clips are **raw RIFF WAV, signed PCM16 little-endian, mono, 32,000 Hz, at most ten seconds**, stored at the exact paths below in an `.o2r` ZIP. They are loaded through the archive manager's exact filenames, not the extension-stripped resource existence cache. The combined pack includes nine mapped clips from the ten WAVs supplied by the user. They were resampled from 16/22.05 kHz with a polyphase filter, preserving duration and source gain; no samples clipped. The original model-only export remains byte-identical inside the combined pack.

| Event | Clip under `objects/midna_navi/audio/` | Supplied Midna clip | Native fallback |
|---|---|---|---|
| Dash, fast return, introductory flight | `dash.wav` | `Appear` | `NA_SE_EV_FAIRY_DASH` |
| Emerge from hidden state | `appear.wav` | `Appear` | `NA_SE_EV_FAIRY_DASH` |
| Disappear | `vanish.wav` | `Vanish` | `NA_SE_EV_NAVY_VANISH` |
| Target an NPC | `target_npc.wav` | `Hey` | `NA_SE_VO_NAVY_HELLO` |
| Watch out / target an enemy | `target_enemy.wav` | `Laugh3` | `NA_SE_VO_NAVY_ENEMY` |
| Listen / target another actor | `target_other.wav` | `Laugh2` | `NA_SE_VO_NAVY_HEAR` |
| HUD call, state `0x1E` | `call.wav` | `Hey` | `NA_SE_VO_NAVY_CALL` |
| HUD hint, state `0x1D` | `hint.wav` | `Hey` | `NA_SE_VO_NA_HELLO_2` |
| Begin companion dialogue | `talk.wav` | `Laugh1` | Existing `NA_SE_VO_SK_LAUGH` call |

The user selected Appear for dash/emergence, Vanish for recall into Link, and Hey for call/hello. Laugh3 covers Watch out, Laugh2 covers Listen, and Laugh1 begins dialogue. Mmm, Uhhm and both yawns remain preserved as source clips for later variations/idle work. No new periodic idle sounds are added.

Missing, empty, malformed or unsupported clips retain the original sound and its original playback arguments independently. Audio alone does not activate on native Navi: the Midna model resource must be installed at startup. Install/remove packs with the game closed and restart; hot mounting is not provided.

The small player preloads clips before starting the audio thread, then mixes into the existing stereo output. It owns one movement voice and one speech voice. A different event in a category replaces that category's current clip; repeating the same active event does not restart it. Scene teardown clears only Midna's voices. It respects the integer Master/SFX sliders, the existing HUD call mute and cutscene gates, and the existing final inactive-game mute.

POC limitation: custom clips play dry and centered. Native positional attenuation, pan, reverb and sound-mode processing are not reproduced. Actual clip gain, lengths, event selection and transitions need listening tests. In particular, verify that a long call does not mask a useful later cue. Other audio players and all shared banks are unchanged.

## Evidence

Run from the repository root, with libultraship checked out:

```sh
python3 scripts/diagnostics/run_midna_navi_draw_test.py
python3 scripts/diagnostics/run_midna_audio_test.py
```

These compile the real modified actor/HUD translation units and exercise extracted production functions with platform boundaries mocked. They cover Navi-only selection, absent/failed model fallback, native visibility gates, faded depth writes, balanced matrices, unchanged actor/player state, HUD call suppression, original fallback arguments, exact WAV lookup, Master/SFX gain, PCM chunk validation, clipping, voice replacement and thread-safe reset. The mixer also passed AddressSanitizer/UBSan; LeakSanitizer was unavailable in the executor's ptrace environment.

All nine WAV entries were read back from the final combined archive and decoded/mixed successfully through the production Midna player. The archive adapter also compiles against the actual libultraship headers.

Asset verification reads the actual exported display list/vertices/texture back, checks all resource hashes and vertex-cache references, checks for degenerate quantized triangles and unexpected matrix commands, and renders a CPU preview. Four pose tests protect the face/helmet and fingernail placement while checking the lowered arms. CPU preview and harness results are not game-renderer or audio-playback proof.

## Small runtime comparison

Use the same save, current baseline packs and load order for the baseline and candidate. Add the named Midna pack only to the candidate. Record executable commit, pack hashes, scene, age and Alt Assets state with each observation.

1. In a normal child scene, check Midna's size, lighting, UVs, flight, targeting, emergence and disappearance. Open companion dialogue and verify hint/progression behavior.
2. Compare a Kokiri fairy and a released/bottled healing fairy. Their appearance and healing cues must remain native. Check an adult scene's companion briefly.
3. Repeat the affected visual checks with Alt Assets off and on. Enter/leave a room, pause/unpause and re-enter the scene; look for stale graphics or crashes.
4. With real Midna clips installed, trigger each mapped event. Check Master/SFX mute and the HUD's disable-call setting, rapid targeting/call transitions, and room teardown. Confirm shared dash users, music and weather retain their baseline audio.
5. Remove one clip and restart: only that event must fall back. Remove the combined Midna pack and restart: native Navi must return. The model-only pack should keep Midna's model with native sounds.

All runtime checks above remain untested. Acceptance and master promotion require observed results; this candidate does not establish ComboShip compatibility.
