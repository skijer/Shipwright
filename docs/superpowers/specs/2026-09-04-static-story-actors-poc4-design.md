# Static Story Actors POC4 Design

## Purpose

POC4 turns the existing `En_Viewer` static-story-actor experiment into a complete, safe placement interface for Prelude. It preserves the three proven actors and adds the approved story/cutscene roster with independently loaded models, selectable looping poses, soft collision, correct targeting focus, blinking, Alt Assets compatibility, and non-mutating post-story dialogue.

The validated proximity-weather implementation is carried forward unchanged. POC4 does not alter rain playback, thunder selection or timing, audio mixing, volume behavior, or room-transition persistence.

## Success criteria

- Every documented parameter renders the intended actor without a cutscene, room, age, or story-spawn requirement.
- Every actor uses the documented looping pose and remains stable after room transitions.
- Compatible actors blink using their native eye textures.
- Each actor has soft pushable collision, an appropriate shadow, and a model-height targeting focus rather than a floor-level focus.
- Alt Assets can substitute native model and texture resources through their ordinary resource names.
- Every actor can be spoken to and selects safe vanilla dialogue based on existing progression flags.
- Dialogue reads save state but never grants items, starts cutscenes, changes progression flags, moves the actor, or invokes the original actor's quest action state.
- Unsupported actor and pose values fail safely and never fall through to vanilla `En_Viewer` types or index beyond a table.

## Parameter encoding

`params = 0x7F00 | (pose << 4) | actorType`

- Low nibble: actor type (`1` through `9`).
- Next nibble: pose (`0` through `15`).
- Unsupported poses resolve to that actor's pose `0` and emit a debug warning.
- Unsupported actor types terminate the custom actor safely and emit a debug warning.

## Human-readable Prelude placement table

| Actor | Pose | Prelude param | Required in POC4 |
|---|---|---:|---|
| Impa | Standing idle | `0x7F01` | Yes |
| Child Malon | Normal idle | `0x7F02` | Yes |
| Child Malon | Singing animation, silent | `0x7F12` | Yes |
| Child Malon | Singing animation with native vocal pattern | `0x7F22` | Yes |
| Saria | Arms-at-sides idle | `0x7F03` | Yes |
| Saria | Hands behind back | `0x7F13` | Yes |
| Saria | Playing ocarina | `0x7F23` | Yes |
| Saria | Native seated pose | `0x7F33` | Yes |
| Adult Zelda | Stable neutral pose | `0x7F04` | Yes |
| Adult Zelda | Dynamic native idle | `0x7F14` | Best effort; falls back to stable neutral pose in POC4 if unsafe |
| Sheik | Standing idle | `0x7F05` | Yes |
| Sheik | Arms crossed | `0x7F15` | Yes |
| Sheik | Playing harp | `0x7F25` | Yes |
| Adult Ruto | Standing idle | `0x7F06` | Yes |
| Adult Ruto | Hands on hips | `0x7F16` | Yes |
| Adult Ruto | Looking down-left | `0x7F26` | Yes |
| Child Ruto | Hands behind back | `0x7F07` | Yes |
| Child Ruto | Hands on hips | `0x7F17` | Yes |
| Child Ruto | Sitting | `0x7F27` | Yes |
| Kokiri Girl | Standing idle | `0x7F08` | Yes |
| Kokiri Girl | Hands behind back | `0x7F18` | Yes |
| Kokiri Girl | Hands on hips | `0x7F28` | Yes |
| Kokiri Girl | Sitting, head on hand | `0x7F38` | Yes |
| Kokiri Girl | Sitting cross-legged | `0x7F48` | Yes |
| Kokiri Girl | Sitting, crossed arms and legs | `0x7F58` | Yes |
| Fado | Standing idle | `0x7F09` | Yes |
| Fado | Hands behind back | `0x7F19` | Yes |
| Fado | Hand on chest | `0x7F29` | Yes |
| Fado | Sitting, head on hand | `0x7F39` | Yes |
| Fado | Sitting cross-legged | `0x7F49` | Yes |
| Fado | Sitting, crossed arms and legs | `0x7F59` | Yes |

Fado has a unique head but shares the Kokiri girl body, legs, skeleton layout, and `object_os_anime` animation library. She can therefore use the girl sitting poses without skeletal remapping. Non-Kokiri actors only use animations authored for their own skeletons.

## Actor resource architecture

The implementation replaces the current coupling between draw-function indices and animation initialization with explicit static actor definitions. Each definition supplies:

- actor type and supported pose table;
- skeleton object and any separate animation objects;
- skeleton resource and limb count;
- scale, ground offset, shadow type, and shadow size;
- cylinder radius, height, and Y shift;
- targeting focus height;
- model-specific draw callback;
- eye and mouth texture policy;
- dialogue selector.

Impa, Malon, Saria, Zelda, Sheik, and both Rutos use their native single-character skeleton resources and model-specific draw callbacks. Kokiri Girl and Fado use a specialized composite renderer that loads the shared animation object plus the correct head, torso, and legs objects. The composite renderer preserves the ordinary native resource paths so Alt Assets remain eligible.

Object loading remains asynchronous. The actor does not initialize its skeleton, collider, dialogue, or draw state until every required object is loaded. A failed object request terminates the actor safely with a debug diagnostic.

## Animation behavior

- Every documented pose is a looping native animation compatible with that actor's skeleton.
- Pose selection occurs once after object loading; normal update advances the selected animation continuously.
- Performance poses (Malon singing, Saria ocarina, Sheik harp) do not head-track Link.
- Seated poses retain their authored root pose; Prelude placement Y remains the ground or seat surface chosen by the scene author.
- Adult Zelda pose `0` must render reliably. Pose `1` may use a dynamic native loop only if its animation object, segment handling, and draw state are stable outside her cutscene. Otherwise pose `1` deliberately aliases pose `0` for POC4 and remains the highest-priority animation follow-up.
- Malon's vocal pose uses her native singing cadence only. It must not take ownership of BGM or nature ambience channels. The silent singing pose is always available.

## Blinking and facial rendering

Each actor uses its native eye texture sequence and model-specific eye segment. Blink timers are randomized within the ordinary native range. Mouth textures remain neutral except where a performance pose requires an existing native mouth state. Models without a safe independent eye-texture path remain open-eyed rather than receiving an incompatible generic draw hook.

Impa's face path receives explicit Alt Assets testing because POC1 showed a distortion not reproduced on Malon or Saria.

## Collision, targeting, and head tracking

- Every actor receives a model-sized OC cylinder that is soft and pushable.
- Collision does not attack or damage Link and does not create hard environmental obstruction.
- Cylinder size and focus height are actor-specific rather than shared constants.
- `Actor_SetFocus` is updated at the documented model height, fixing POC3's floor-level Navi target.
- Basic head/torso tracking is enabled only while the actor is available for conversation and is standing in a compatible idle pose.
- Tracking is disabled for seated and performance poses to preserve authored silhouettes and avoid limb distortion.

## Dialogue milestone

Dialogue is required for every roster entry in POC4.

Each actor has a side-effect-free dialogue selector that reads relevant vanilla save flags and chooses an appropriate existing text resource. The selector may distinguish pre-completion and post-completion lines where a safe line exists, with post-story dialogue preferred after the corresponding quest milestone. It invokes only the message system; it does not call the native actor action function.

The dialogue layer must not:

- set event, quest, switch, or randomizer flags;
- award or offer items;
- start an ocarina check or cutscene;
- teleport, despawn, swim, float, or animate the actor into a scripted sequence;
- repeat a one-time reward;
- change the selected placement pose permanently.

During conversation, compatible standing idles may track Link. On message close, the actor returns to its selected placement pose. A safe curated vanilla line is used when the original actor has no standalone post-story conversation path.

## POC4 scope boundaries

Included:

- complete nine-entry roster;
- every pose in the placement table;
- Adult Zelda stable pose and best-effort dynamic idle;
- blinking where supported;
- soft collision, shadows, and correct targeting;
- basic conversation tracking where compatible;
- progression-aware, read-only vanilla dialogue;
- Alt Assets testing;
- strict parameter and table bounds validation.

Excluded:

- rewards, item checks, and randomizer location registration;
- progression mutation;
- actor wandering or schedules;
- cutscene actions and scripted disappearances;
- Ruto swimming/floating behavior;
- skeletal retargeting between unrelated actors;
- custom text resources;
- any behavioral change to the validated weather system.

## Verification

### Automated and build verification

- Add parameter decoder tests for every documented value and invalid actor/pose values.
- Add table-size assertions tying actor enums, definitions, and draw handlers together.
- Run formatting and the available native build checks before pushing.
- Run GitHub Windows, Linux, and macOS builds; Windows is required for the user's immediate test, while cross-platform failures are investigated rather than silently accepted.

### In-game matrix

Place every documented parameter in one controlled Prelude test scene, spaced far enough apart to identify each actor. Verify:

- correct model and pose;
- model-height Navi targeting;
- soft collision and shadow placement;
- blinking and face integrity with Alt Assets off and on;
- room reload and transition stability;
- dialogue before and after representative story flags;
- no rewards, cutscenes, flag mutation, or actor self-destruction;
- rain/BGM/thunder behavior remains identical to the validated weather build in a separate weather regression scene.

## Follow-up hierarchy

After POC4 validation:

1. Repair or expand Adult Zelda's dynamic idle if POC4 must use the neutral fallback.
2. Register optional actor placements as Randomizer checks with explicit duplicate-reward protection.
3. Add controlled reward configuration separately from the static presentation and dialogue subsystem.
4. Consider additional sages only after the nine-entry roster is stable.
