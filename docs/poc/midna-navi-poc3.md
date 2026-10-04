# Midna POC3 — visible blink, body alignment, and idle yawn

Parent executable: `b622efcfff225ca99d38bf52d7cb90519dea5911` on `poc/midna-navi-soh-poc2`. This remains a candidate on top of the cumulative SoH integration; no integration/master promotion.

Runtime evidence: `Ship of Harkinian(2).log` identifies that build and the FaceEyeFix pack, alongside the old POC1 pack. The two supplied clips show the corrected face/iris, short clear views of the face, and unintended shoulder/hip bands. They do not establish a frozen animation clock. POC1 does not own the POC2 pose/blink paths, so its presence alone is not the diagnosed cause.

## Code correction

The original blink schedule consumed intervals while Navi was hidden. A separate presentation clock now advances once per actor update only when Navi is out, has nonzero alpha, and has at least half emergence scale. All native/talk update paths advance it; pause/halt and hidden frames do not. Native gameplay timing, movement and appearance/fade remain unchanged. The first blink begins after 1.2 seconds out, followed by alternating 4.6/5.4-second intervals counted while out. Half/half/closed/closed/half/half lasts 300 ms at 20 Hz. Draw calls never advance actor time.

Optional missing or failed blink resources retain the open eye. A bounded trace logs the actual selected pose, blink (0 open, 1 half, 2 closed, -1 missing-resource fallback), first vertex, scene and Alt state. It is capped at 16 lines per actor/timer epoch, rather than logging every frame. Native timer rollover can restart the budget after about 55 minutes.

## Idle yawn

User-selected behavior: a randomized **20–45 seconds of standing idle with Midna out**. The unspent countdown pauses when Link moves or Midna recalls; targeting, dialogue, cutscenes, menus, transitions, and game-over also block it. This counts accumulated eligible idle time, not wall time. Native recall behavior is retained, so a recalled companion cannot yawn. There is a two-second quiet guard after an interruption before any already-due yawn can play.

Passive NPC proximity counts as idle when Link is stationary. Native Navi uses mode 0 for this and holds the recall timer at 100, so this is where sustained idle with her out naturally occurs. Active lock-on, no-target Z-hold, first-person aiming and native mode-1 non-NPC attention remain excluded. An arrow over a nearby NPC alone is not an interaction.

The optional `yawn.wav` uses the supplied shorter `TP_Midna_Yawn2.wav` (1.8355625 seconds), converted from 16 to 32 kHz and attenuated by 6 dB, matching the Hm conversion. Scheduling runs only from Navi's update paths at 20 Hz. A private RNG avoids consuming gameplay randomness. The idle cue shares the speech voice at lower priority: existing movement, recall, Hey, hint, targeting and talk cues win, even when they fall back to native audio. Leaving idle cancels the yawn. No absent-asset fallback noise is added. Teardown stops voices while preserving the remaining interval.

Audio diagnostics exercise the actual scheduler and mixer: 20–45-second eligible-time bounds, repeated timer pauses that would catch a reset instead of a pause, repeated/continued playback, movement cancellation, interruption priority, missing assets, teardown, and existing Hm spacing. Actor routing checks cover stationary versus moving, recalled/emerging/hidden, targeting, dialogue, cutscene, pause, transition and ordinary-fairy gates.

## Matching asset candidate

`Midna_Navi_SoH_POC3_Model_Audio.o2r`

SHA-256: `dbe4552da4110b845c00a78ea9b02dcc8239ec18214166120de46ddfc2f8c176`

Parent pack: FaceEyeFix SHA-256 `33c48517e0803536d0fb30e7f01c37ed485230156bd5909f532ee50800c01235`.

Only 64 existing pose arrays change, plus the new yawn clip. Existing deformation fields receive modest amplitude increases: arm/leg rotation about 5.7 degrees, with smaller breathing/head/tail motion. Maximum total vertex travel is 1.418 world units on the approximately 21.46-unit character; maximum adjacent step is 0.07152 units. This addresses readability, not a proven frozen renderer.

The prior face-only UV compensation left native body coordinates shifted into black atlas padding. The body alignment is corrected in packed ST before vertex scaling, leaving existing exact face/blink/glow cache-coordinate writes intact. All 26 other existing resources, including six corrected display lists, textures, prior audio, shimmer and original fallback, remain byte-identical. Triangle topology and size are unchanged. Tattoo colors still use the same native Primary/Secondary routing. The yawn addition also verifies all 90 resources from the intermediate motion/UV pack remain byte-identical.

## Evidence and next test

The visible-clock regression failed against the parent at hidden-frame emergence and passes with the correction. Production draw checks and the full actor syntax compile pass. The export verifier checks 155,544 triangle corners for native body UV alignment or preserved blink/glow UVs, all 64 poses, normalized normals and seam continuity (maximum packed delta 1). The compiled production resolver passes 103,808 vertex-address checks. Independent code and asset review precede publication.

These are offline/build checks. Runtime appearance, stability, idle audio and Alt parity require the candidate test. Close SoH, replace both the old POC1 and FaceEyeFix packs with the single complete POC3 pack, and use the matching new executable. Inspect shoulder/hip bands, body/hand markings, restrained limb motion and one close/reopen with her face held in view. For the yawn, accumulate 20–45 seconds standing idle while she is out; movement and recall should pause the remaining time, and targeting/talk should take priority. Check scene reentry and Alt off/on. Keep the parent executable and FaceEyeFix pack for rollback.
