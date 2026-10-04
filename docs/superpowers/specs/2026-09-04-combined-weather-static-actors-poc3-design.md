# Combined Weather and Static Story Actors POC3 Design

## Purpose

Produce one Windows test build that combines the next proximity-weather audio iteration with an expanded static story-actor system. Keep the weather and actor changes separable in source control so either subsystem can be diagnosed independently, while delivering one executable to the tester.

The spin-attack asset iteration is intentionally separate from this build and retains its current archive as the fallback baseline.

## Success Criteria

- Scene BGM and proximity rain play concurrently.
- Rain audio continues across room boundaries while the originating proximity-weather effect remains active.
- Rain and thunder have dedicated volume controls that prevent the SFX mix from becoming overpowering.
- Thunder can be disabled entirely.
- Enabled thunder can use either the native `Low Thunder` sample or a layered `Lightning` plus `Low Thunder` presentation.
- Thunder plays at the already-proven synchronized lightning-strike point without changing visual flash timing.
- Every supported static story actor renders outside its native scene or story gate without self-terminating.
- Every supported actor has a stable default idle, soft NPC collision, and blinking when compatible eye textures exist.
- Every supported actor offers a normal talk prompt and safe, repeatable vanilla dialogue.
- Actors remain anchored at their Prelude placement and do not autonomously walk away.
- The release includes an exact Prelude placement parameter table derived from the implemented constants.

## Build and Branch Structure

Weather and actor work remain logically independent. Each subsystem is implemented and locally verified before the combined branch is built. The user receives one Windows executable containing both sets of changes. The separate history is an internal diagnostic mechanism and does not require separate downloads or user-facing installation steps.

## Weather Design

### User Interface

Replace the POC2 boss-SFX audition list with:

- `Enable Proximity Weather Thunder` checkbox.
- `Thunder Style` selector, enabled only when thunder is enabled:
  - `Low Thunder` (default).
  - `Layered Thunder`.
- `Proximity Rain Volume` slider.
- `Proximity Thunder Volume` slider.

The controls must be visibly rendered and locally smoke-tested before triggering the Windows build.

### Rain Playback

Continue using the proven standalone SFX route for rain so scene BGM remains on the main music player. Apply explicit proximity-rain gain in addition to the user's global SFX volume.

Preserve rain-loop refreshing across room transitions. The fix must follow the active weather state rather than depend on a room-local actor continuing to exist. It must stop refreshing when the proximity-weather effect ends, the scene changes, or precipitation is otherwise disabled.

### Thunder Playback

Do not use `NA_SE_EV_LIGHTNING (0x282E)` or bomb/boss effects.

Load the native ROM-derived resources from the user's generated OoT archive:

- `audio/samples/Low Thunder_META`
- `audio/samples/Lightning_META`

Use an isolated one-shot playback route that does not mutate or repurpose a shared SFX-table slot. The route must mix with normal SFX and scene BGM, obey global SFX volume and the dedicated thunder-volume control, and release all playback state after the sample finishes.

`Low Thunder` plays the native low-thunder sample alone. `Layered Thunder` starts both native samples at the existing synchronized strike point with conservative gain staging to prevent clipping. Visual lightning timing and frequency remain unchanged.

If direct native-resource playback proves technically unsafe during implementation, stop and report the blocker rather than silently falling back to an unrelated SFX or shared soundfont mutation.

## Static Story Actor Design

### Supported Roster

- Impa
- Child Malon
- Saria
- Fado
- Selected Kokiri girls
- Child Ruto
- Adult Ruto
- Adult Zelda
- Sheik

### Common Host Behavior

Static variants bypass their original scene-header and story-state self-termination while leaving vanilla instances unchanged.

Each static actor receives:

- On-demand loading of required object resources.
- A stable looping idle or explicitly selected pose.
- An anchored world position and rotation from Prelude.
- A body-scaled cylinder using ordinary soft NPC overlap/push behavior.
- Randomized vanilla-style blinking when the native model exposes compatible open, half, and closed eye textures.
- A normal proximity talk offer using a character-appropriate, repeatable vanilla text ID.
- Alt Asset compatibility when the replacement preserves the native skeleton and material/eye-resource contract.

Collision is actor behavior, not model geometry. Adult Ruto therefore receives a collider even though her vanilla Water Temple instance is cutscene-driven. Actors do not acquire combat collision, damage handling, progression-changing story logic, or autonomous navigation in this POC.

POC3 dialogue is deliberately read-only. Each static character uses a contextually appropriate post-event or otherwise safe vanilla line that can be repeated without granting an item, starting a cutscene, setting a quest flag, or making the actor disappear. If a character has no suitable post-event line, implementation must select and document another existing context-neutral vanilla line rather than silently attaching story logic or authoring replacement text.

The talk interaction is the extension point for a later Randomizer reward-host phase. The static actor architecture must allow future dialogue selection, Randomizer location registration, reward granting, and completion checks without replacing the placement, rendering, animation, collision, or blink systems.

Head and torso tracking and additional facial expressions are secondary enhancements. They may be enabled per character only after rendering, animation, collision, and blinking are stable; failure to support tracking must not prevent the character from shipping in the POC.

### Character Poses

- Impa: neutral standing idle; use the correct unmasked head presentation unless a selected variant explicitly requires the mask.
- Child Malon:
  - neutral idle;
  - silent singing animation;
  - singing animation with vanilla distance-based song behavior, if the audio behavior can be isolated safely.
- Saria:
  - standard standing idle;
  - hands-behind-back idle;
  - seated ocarina animation. The stump is scene geometry and is not spawned automatically.
- Fado and Kokiri girls: native stable standing idle for each selected subtype.
- Child Ruto: native stable standing idle.
- Adult Ruto: neutral standing idle.
- Adult Zelda: neutral standing idle.
- Sheik: standard standing idle, reusing proven Randomizer behavior where applicable.

Animation or audio variants are selected by actor parameters. Unsupported parameter combinations fall back to the character's safe default idle rather than crashing or disappearing.

## Prelude Parameter Deliverable

The completed POC includes a table with these columns:

| Prelude field | Meaning |
| --- | --- |
| Actor name and ID | Exact actor selected in Prelude |
| Params | Exact hexadecimal value to enter |
| Character | Rendered character/subtype |
| Pose or audio mode | Idle, singing, seated, or other supported variant |
| Objects | Resources loaded automatically by the patch |
| Placement notes | Scale, stump requirement, Alt Asset limitation, or other caveat |

The table is generated only after final parameter values are implemented and tested. Existing proven values remain stable unless a verified collision is discovered.

## Compatibility and Failure Handling

- Vanilla actor behavior is unchanged unless the reserved static parameter marker is present.
- Unknown static character or pose values use a visible safe fallback or terminate cleanly; they never index tables out of bounds.
- Missing object, skeleton, animation, eye texture, or audio resources are detected before dereference.
- Closing or repeating a conversation leaves story flags, inventory, Randomizer checks, and actor visibility unchanged.
- A model with incompatible custom eye materials may remain open-eyed while retaining rendering, animation, and collision.
- The Hyrule Warriors Adult Ruto replacement is expected to inherit behavior only if it preserves the native Adult Ruto skeleton and material interfaces; explicit compatibility is not guaranteed without inspecting that mod.
- Weather audio must stop on scene teardown and must not leak across unrelated scenes.
- Audio gain is clamped to prevent layered-sample clipping.

## Verification

### Local Tests

- Parameter decoding and safe fallback tests for every character and pose.
- Collider size/registration tests where practical, plus in-game contact checks.
- Blink state-machine tests, including missing-texture fallback.
- Talk-offer and message-selection tests, including verification that conversations do not mutate progression state.
- Weather state tests for thunder disabled, both thunder styles, and volume limits.
- Room-transition tests for rain refresh and termination.
- Audio lifecycle tests for one-shot completion and scene teardown.
- Formatting and applicable unit-test targets must pass before CI.

### In-Game Matrix

Test at least one neutral custom room and one edited multi-room scene with:

- Alt Assets off.
- Alt Assets on.
- Minimal mod loadout.
- Representative audio replacement/music mod loadout.
- Each actor parameter at least once.
- Player contact from multiple directions.
- Several complete blink cycles.
- Talk prompt, conversation completion, and repeat conversation for every supported character.
- Rain-only mode.
- Low Thunder mode.
- Layered Thunder mode.
- Room transition while raining, followed by leaving the weather region or scene.

## Out of Scope

- Bundling or redistributing external thunder recordings.
- Repurposing existing bomb, boss, or dummy SFX slots.
- Reward granting, story progression, and Randomizer location registration in POC3. These are planned extensions of the POC3 talk interface, not exclusions from the overall static-actor direction.
- Free-roaming NPC AI or pathfinding.
- Guaranteeing arbitrary custom-model eye animation without a compatible material contract.
- Spin-attack asset changes in the combined executable.
