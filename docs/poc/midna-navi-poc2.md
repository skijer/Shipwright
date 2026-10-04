# Midna as Navi — SoH POC2

POC2 addresses the accepted POC1 gameplay feedback: the native glow obscured Midna, movement chatter repeated during targeting, and the static pose felt rigid.

## Candidate identity

| Item | Value |
| --- | --- |
| Base | `integration/nei-weather-static-actors` at `f889527c4c0f97403eae3806e43ec367cd22c7ad` |
| Candidate branch | `poc/midna-navi-soh-poc2` |
| Combined asset pack | `Midna_Navi_SoH_POC2_Blink_Model_Audio.o2r` |
| Pack SHA-256 | `ae656678a703733814778e351ca1c2ce57f88672d936a3088b82daac72d7bf0e` |
| POC1 baseline pack SHA-256 | `1d3ab51863432b1948c53117331fd35c27a6786c63d0ba2cc4fa4ad129dfb810` |

The candidate includes the full cumulative integration baseline. This change is SoH-only. POC1 is retained for comparison and fallback. Build evidence does not establish gameplay acceptance or authorize master promotion.

## Behavior

- Emergence and fairy dash use the supplied `TP_Midna_Hm.wav`, resampled to 32 kHz mono PCM16 at -6 dB. They share an eight-second minimum gap measured on the mixer timeline. Suppressed cues are handled silently, do not queue, and do not fall through to vanilla dash. Scene voice teardown retains the gap; audio initialization clears it.
- Recall, Hey, dialogue giggle and all other POC1 clips remain byte-for-byte identical. Missing clips keep their existing native fallback. No global sound bank changes.
- Navi's existing Primary RGB colors tint Midna's turquoise markings; Secondary RGB colors tint six small, softly fading diamond motes. Idle, NPC, enemy and prop transitions, custom colors and rainbow continue through the native `innerColor` and `outerColor` values. Motes sit around the torso and legs to keep her face clear.
- The separate native spherical glow is suppressed only when the Midna model loads. Local point illumination remains. Ordinary fairies and native Navi fallback keep their normal glow.
- A 64-frame, 3.2-second loop adds restrained breathing, limb drift, head motion and delayed ponytail movement inside the original pose. Gameplay travel, orbit, emergence scale, visibility and actor state remain native. Child and adult share POC1's model size.
- Optional blink atlases close the complete eye, including its separate yellow sclera. The original open eye remains untouched. Half/closed states use a native-material bake with dedicated 256x256 eye textures and the existing face skin color; rejected generated artwork is unused. A 200 ms half/closed/closed/half sequence runs at alternating 4.6/5.4-second intervals on the existing actor clock, without RNG or actor-state changes.

## Resource and rendering boundaries

POC2 adds resources under `objects/midna_navi/poc2/`. The original POC1 model resources remain available. POC2 selection requires the current immutable pose, body and markings display lists, shimmer display list and vertices, and both textures to exist and load; otherwise it draws POC1. If Midna itself cannot load, the existing native draw path remains available. Resources are resolved for each draw rather than retaining raw pointers across resource reloads.

Each pose preserves all 12,842 triangles and POC1 UV coordinates. Cached pose vertices total 12,900,352 bytes. Body and markings share the same pose vertex array; the markings use a masked, unlit decal pass. The shimmer uses vertex alpha with a non-fog blender. There is no runtime vertex deformation or per-triangle matrix allocation. The extra geometry pass and cache require a gameplay performance check.

The blink extension adds two display lists and two small textures, totaling 524,288 additional texture bytes. Blink display lists preserve the original triangle multiset, use the original body material for all non-eye triangles, and remap only the 268 iris/sclera triangles to the eye atlas via `G_MWO_POINT_ST` in the loaded vertex cache. No pose resource is changed. If any blink resource is absent or fails to load, the open POC2 model remains active. All 82 pre-blink POC2 resources are byte-identical to pack SHA-256 `01a0663ff85b282b4c049a7cef97f66308a7a3be564614e5be9044e96f3e38ac`.

## Verification

Both focused diagnostics pass, including full actor C syntax compilation:

```sh
python3 scripts/diagnostics/run_midna_audio_test.py
python3 scripts/diagnostics/run_midna_navi_draw_test.py
```

Checks cover alternating movement cues, exact cooldown expiry during silence, no queued cue, unrestricted speech/recall, scene reset, missing clips, palette wiring, frame selection, fade, matrix balance, native and POC1 fallback, ordinary fairies, glow behavior, missing or failed shimmer/texture loads, and the shimmer render mode. Independent review findings were corrected and rechecked before publication.

The asset checkpoint's `tools/verify_poc2.py` verifies the exported archive, display-list references, vertex ranges, topology, normals, unchanged UVs, seams and loop closure. Maximum adjacent movement is 0.03256 world units, below the asserted 0.05 limit. Exactly two of the twelve POC1 entries change: `audio/dash.wav` and `audio/appear.wav`; the other ten are identical. Offline previews render the exported pose data.

`tools/verify_blink.py` decodes the exported texture, vertex, UV-modification and triangle commands, checking complete iris/sclera coverage, original triangle preservation, unchanged non-eye UVs, valid indices, opaque texture data and full closure without exposed iris/sclera colors. The preview uses those decoded display lists. Draw diagnostics additionally cover blink boundaries, repeated draws at the same update time, u16 wrap, and each missing/failed blink resource.

## Install and runtime check

With the game closed, use the matching POC2 executable and replace the POC1 Midna pack with the POC2 combined pack in the same mods location. Keep all other baseline packs and their order. Restart. Avoid enabling both Midna packs because they share fallback and audio resource paths.

For the baseline comparison, use the retained POC1 executable and pack. Test the candidate with the same save and settings: crowded NPC targeting, all four cosmetic color categories (including rainbow), child/adult, scene reentry, Alt Assets off/on, and the C-up/answer/recall sequence. Check readability, subtle motion, shimmer depth/fade, audio repetition and frame rate. These runtime observations remain unproven by offline checks.
