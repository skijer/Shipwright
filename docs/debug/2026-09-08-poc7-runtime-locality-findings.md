# POC7 Runtime Locality Findings — 2026-09-08

## Status

This began as a stop-point handoff. The user subsequently approved continuing
with the diagnostic POC, but the Windows build remains intentionally deferred
until the probe produces one coherent behavior fix.

Branch: `codex/poc7-weather-audio-diagnostics`

Code checkpoint before this note: `54c59e2ea` (`fix: preserve authored rain
loop across outdoor scenes`)

The checkpoint currently:

- retains the soundfont's authored `loopStart` and `loopEnd` instead of always
  restarting rain at sample zero;
- preserves the global rain loop/cycle across ordinary `PlayState` destruction
  and resets it on `OnExitGame`;
- corrects the SFX allocation fixture to exercise Navi's actual
  `NA_SE_EV_FAIRY_DASH`, not the unrelated `NA_SE_VO_NA_HELLO_2` voice line;
- contains no Hyrule Field night-music or thunder behavior change.

Local focused results already obtained:

- weather sample-player test: PASS in normal and `NDEBUG` builds;
- global outdoor-rain pure test: PASS;
- concurrent-weather policy test: PASS;
- integrated global-rain/audio test with actual `FAIRY_DASH`, torch, enemy,
  pickup, rain, and two private thunder voices: PASS;
- `git diff --check`: PASS.

One strict manual compile attempt exposed only pre-existing warnings in
`code_800F7260.c` when promoted to `-Werror`; rerunning with those known legacy
warnings suppressed passed. Finish the complete compile/test matrix later.

## New user runtime evidence

### Hyrule Field locality

- Missing Navi emergence, suppressed continuous decorative-flame crackle, and
  missing dropdown thunder are isolated to the **central Hyrule Field hub**.
- In the Lake Hylia/MM-fountain corner of Hyrule Field, Navi emergence and
  decorative-flame crackle remain audible as intended.
- The Lake Hylia corner has approximately **two** decorative flame actors.
- The central hub has approximately **ten** decorative flame actors, each
  rendering and attempting to submit its continuous torch SFX alongside rain,
  enemies, ambience, and miscellaneous sounds.
- The dropdown thunder works correctly in Lost Woods and synchronizes with the
  weather actor's lightning there, but it is not audible in central Hyrule
  Field.

The new ten-flame fixture runs the real request filtering, priority selection,
bank allocation, and channel assignment. With all ten independent torch loops
submitted before `FAIRY_DASH`, Navi is still selected and started; both private
thunder layers also remain in the final test mix. This rules out simple
environment-bank exhaustion as the direct cause. Do not special-case Navi,
torches, or Hyrule Field unless the runtime PCM evidence contradicts this
fixture.

### Opt-in runtime probe

POC7 now exposes **Audio Editor → Audio Options → Weather → Log Weather Audio
Diagnostics**. Enable it only for the short reproduction. Its console lines
report:

- scene, room, `indoors`, rain source, and density whenever location
  eligibility changes;
- each thunder transient admission/rejection and occupied private voice count;
- each lightning trigger's low/layered return values;
- periodic pre-weather and post-weather PCM peaks plus exact int16 clamp count
  (and immediately whenever any clamp occurs).

For the useful capture, enable the checkbox, reproduce once in the working Lost
Woods area, cross rooms 4 and 7, then reproduce at the central Hyrule Field hub.
The capture will distinguish room eligibility, exhausted thunder voices, and
post-mix clipping without another hypothesis-only build.

### Lost Woods locality

- Rain audio dropoff is isolated to **rooms 4 and 7**.
- Crossing out of room 7 causes rain to return immediately.

This points toward room-local environment eligibility/state (especially the
current raw `envCtx.indoors` gate or room commands), not a global decoder or
loop-voice failure. Compare room 4/7 environment headers and runtime `indoors`
state with adjacent working Lost Woods rooms. Do not hardcode room numbers.

### Static actor catalogue findings (separate branch)

The user expected Nabooru, Adult Zelda, Darunia, and the two Adult Ruto water
placements to be usable. Current runtime findings:

- Darunia is interactable and his dialogue path works, but his model does **not**
  render. He is therefore not a healthy control actor.
- Nabooru, Adult Zelda, and both Ruto water variants are not interactable.
- Their targeting/focus marker falls approximately halfway into the ground.

Keep this entirely out of the weather branch. On the actor branch, split the
failure into two boundaries:

- Darunia: object/animation readiness and draw dispatch fail while attention
  and dialogue work.
- Nabooru, Zelda, and Ruto water variants: trace both rendering and the broken
  focus/talk position after their final world-Y/water-state update.

Trace object slots, animation-object loading, skeleton initialization, draw
dispatch, `Actor_SetFocus`, attention flags, talk-distance checks, collider Y
shift, and water-mode world-position changes.

## Revised systematic order

1. Preserve this diagnostic checkpoint; do not spend the Windows build until
   it is paired with a coherent fix.
2. Use the opt-in runtime probe for one focused capture.
3. Trace the thunder trigger and `WeatherSamplePlayer_Play` result separately
   in central Hyrule Field; Lost Woods proves the sample/player path can work.
4. Compare Lost Woods rooms 4/7 eligibility state with adjacent rooms and
   replace the overly broad room gate with a scene/environment-derived outdoor
   decision if confirmed.
5. Only change final-mix headroom if PCM/clamp evidence proves it is
   involved after the hub allocation trace.
6. Finish the local compile/test matrix and run one Windows build for the
   resulting behavior checkpoint.
7. Keep actor focus/talk repair on the dedicated actor branch.
