# POC6 Weather Stabilization and Static Actor Expansion Design

## Purpose

POC6 stabilizes the POC5 weather and Hyrule Field night-audio features before expanding the proven `En_Viewer` static-story-actor catalogue. Weather and actor work remain independently testable so an experimental actor cannot conceal or block an audio regression.

The Lost Woods room project is complete and is not bundled into this source branch.

## Success Criteria

- Native Prelude `En_Weather_Tag` rain survives Lost Woods room transitions, including Room 4.
- Persistent outdoor rain preserves ordinary scene sound effects and Navi audio.
- Hyrule Field reliably transitions from day music to the configured night track and back to the correct day music through repeated cycles, fanfares, pause/unpause, and scene transitions.
- The Hyrule Field night slot participates in the existing music-randomization lifecycle and keeps its resolved sequence stable until that lifecycle rerolls it.
- Every existing static-story-actor parameter retains its POC5 behavior.
- Darunia, Nabooru, standing Ganondorf, and a non-hostile Gerudo guard become stable catalogue entries.
- Adult Ruto receives opt-in grounded, surface-idle, and dive-loop modes without altering existing parameters.
- Adult Zelda is repaired in an isolated normal-model adapter.
- Mounted Ganondorf and Phantom Ganon are experimental entries that can be removed without disturbing standard actors.
- Static placements never invoke native story, reward, cutscene, combat, capture, or progression actions.

## Scope

### Weather and Audio Stabilization

Required:

- Preserve native placed weather behavior without room-number hardcoding.
- Repair the persistent outdoor-rain audio route so it coexists with all ordinary SFX.
- Preserve the current rain color, volume, intermittent fade, thunder, and Weather UI divider controls.
- Repair Hyrule Field night-BGM ownership and restoration.
- Route the configured night sequence through the existing sequence-replacement/randomization mechanism when music randomization is enabled.
- Preserve the resolved randomized night track across fanfares and repeated day/night transitions. Reroll only when the existing new-scene/randomizer-generation enhancement rerolls music.

Excluded:

- A second nightly random-number generator.
- Per-dusk track rerolls.
- Room-specific weather exceptions.
- Coupling weather updates to static actors.
- Lost Woods room assets or Prelude project files.

### Static Actor Expansion

Standard additions:

- Darunia
- Nabooru
- Standing Ganondorf
- Non-hostile Gerudo guard

Expanded existing actor:

- Adult Ruto with selectable grounded, surface-idle, and dive-loop modes

Isolated repair:

- Normal Adult Zelda only

Experimental additions:

- Mounted Ganondorf
- Phantom Ganon

Explicitly excluded:

- Rauru, because Prelude already exposes him as an ordinary placement
- Zelda escape-cloak variations
- New Sheik/Zelda linkage
- Hostile Gerudo behavior
- Native villain combat or encounter behavior

## Architecture

### Independent Work Tracks

Weather/audio stabilization lands and is verified before catalogue expansion. Actor families then land as isolated commits and checkpoints. Experimental villain adapters are last and must remain removable without reverting standard actors or weather fixes.

The existing static-story definition table remains the catalogue interface. Each definition declares object dependencies, adapter, skeleton family, poses, scale, focus height, collider, blink policy, tracking policy, and dialogue selector. Existing `0x7Fxx` meanings do not change.

The original encoding has only a four-bit actor-type field and therefore cannot hold the current ten entries plus every approved addition. POC6 adds a second reserved catalogue page using prefix `0x7E00`. Both pages retain the same low-nibble actor and next-nibble pose layout, but page-two actor IDs are resolved through a separate definition range. All new POC6 actors use `0x7Exx`; no existing parameter is renumbered or reinterpreted. The final guide documents both pages and rejects unregistered page/type combinations safely.

Actor-specific behavior does not enter the generic catalogue core. The core decodes parameters, loads declared dependencies, initializes shared interaction state, and dispatches to the selected adapter. Ruto water movement, Darunia dance sequencing, mounted composition, and Phantom rendering remain within their adapters.

### Safe Dialogue

All talkable static placements use the existing read-only message layer. Dialogue may select curated vanilla text from save-state snapshots, but it cannot call the original actor action function.

This applies to Ganondorf and Phantom Ganon as well as friendly NPCs. Dialogue must not start battles, cutscenes, capture sequences, rewards, teleports, switches, or progression changes. Mounted Ganondorf targets the rider rather than the horse.

## Actor Designs

### Darunia

Darunia uses his native skeleton, face resources, and compatible animations. Alongside a stable idle, he exposes a full decorative dance-cycle pose. The dance adapter chains the native hop, spin, dance loops, and transitions in their intended order, repeats from a clean boundary, locks placement translation, and disables tracking during the performance.

Talking pauses the performance safely. After dialogue closes, the sequence restarts at a defined boundary. The dance owns no BGM, sound effect, ocarina, reward, or story state.

### Nabooru

Nabooru uses her native skeleton and stable looping poses, beginning with standing hands-on-hips. Compatible standing poses support blinking, soft collision, targeting, tracking, and safe dialogue. Cutscene collapse, vortex, combat, running, and scripted transition states are excluded unless a later design explicitly admits a self-contained decorative animation.

### Adult Zelda

Adult Zelda receives a dedicated normal-model adapter isolated from other definitions. The first milestone is one reliable neutral pose with correct skeleton initialization, limb drawing, textures, eyes, Alt Assets substitution, collision, focus height, and dialogue.

Further poses remain disabled until the neutral renderer passes. Zelda does not share a renderer or parameter with Sheik and does not expose escape-cloak forms.

### Ganondorf Forms

Standing Ganondorf is a standard static entry using a stable native pose, soft collision, correct targeting, and curated safe dialogue. Fire effects, cutscene movement, attacks, damage, and encounter state are disabled.

Mounted Ganondorf is experimental. Its adapter owns rider and horse object dependencies and their composite placement. Both remain inert; dialogue targets Ganondorf. Failure to load either dependency removes only that placement.

Phantom Ganon is experimental. It may use only self-contained native model/render effects proven safe without an encounter controller. Portals, room switches, attacks, damage, flight AI, and boss-state ownership are excluded. It uses curated safe dialogue through the static message layer.

### Non-Hostile Gerudo Guard

The Gerudo guard is a friendly decorative entry with stable idle poses, native blinking, compatible head tracking, soft collision, correct focus height, and generic safe dialogue. Patrol, detection, whistle, capture, ejection, alarm, and combat behavior are never initialized.

### Adult Ruto Water Modes

Existing Adult Ruto parameters keep their exact grounded behavior. New explicit pose values select:

1. Grounded static mode.
2. Surface-idle mode.
3. Dive-loop mode.

Surface-idle mode resolves the local water surface at the placement, moves vertically to an actor-correct resting height, and plays a compatible treading or swimming loop. Dive-loop mode moves between bounded depths around the original placement, resurfaces, pauses, and repeats.

Dialogue pauses water movement and resumes cleanly after closing. Invalid or missing water falls back to the original grounded placement. The controller must not perform a room-wide water search, drift horizontally without an explicit future design, rise indefinitely, or borrow Child Ruto/Zora story actions.

## Weather and Night-BGM Lifecycle

### Weather Ownership

Native placed weather and persistent outdoor weather remain distinct sources feeding a shared, coexistence-safe audio/render lifecycle. The persistent option must not stop or replace general SFX playback. Native room transitions must not clear an active placed-weather source merely because the next room lacks a duplicate placement when the native weather lifecycle says the storm continues.

Any raw metadata-loop implementation that monopolizes or resets the SFX channel must be removed or routed through the compatible native mechanism. Existing thunder synchronization, thunder enablement, rain color, volume, and intermittent fades remain intact.

### Hyrule Field Music State

The night controller owns an explicit day/night state instead of inferring ownership from the current main sequence alone. On dusk it resolves the configured night slot through the ordinary sequence-replacement system and requests that resolved track exactly once. On dawn it restores the correct current daytime Hyrule Field sequence.

Fanfare completion, pause/unpause, and unrelated audio restarts must consult the controller state: during night they restore the resolved night sequence, and during day they restore the correct day sequence. Repeated day-to-night-to-day cycles cannot fall silent or layer two field tracks.

When existing music randomization is enabled, the night slot is eligible under the same new-scene/randomizer-generation lifecycle. The selected replacement remains stable across dusk, dawn, and fanfares until that existing lifecycle generates a new mapping.

## Error Handling

- Invalid actor types terminate the custom placement safely.
- Unsupported poses fall back to pose zero only when pose zero is valid for that actor.
- Missing objects, animations, or composite dependencies emit a diagnostic and remove only the affected placement.
- Experimental Zelda, mounted Ganondorf, and Phantom Ganon failures cannot mutate global tables or disable standard entries.
- Ruto water-query failure returns to the authored placement without repeated ascent/descent attempts.
- Animation sequences use explicit bounds and terminal transitions; malformed state returns to the stable idle.
- Weather source loss fades or transitions according to its lifecycle and never globally resets unrelated SFX.
- Night-sequence resolution failure falls back to the configured non-randomized night selection, then to silence only if neither is valid; it must not select an unrelated sequence.

## Implementation Order

1. Remove obsolete native Malon/Saria diagnostics and false fixes introduced while diagnosing debug-warp behavior.
2. Stabilize weather audio, placed-weather transitions, and Hyrule Field night-BGM lifecycle, including existing-lifecycle randomization.
3. Verify the complete weather/audio matrix before actor expansion.
4. Add Darunia and Nabooru, including Darunia's complete dance cycle.
5. Add the non-hostile Gerudo guard and standing Ganondorf.
6. Add Adult Ruto's selectable water modes.
7. Repair the isolated normal Adult Zelda adapter.
8. Probe mounted Ganondorf and Phantom Ganon as removable experimental adapters.
9. Run the full regression matrix and publish a human-readable Prelude parameter guide.

## Verification

### Automated Tests

- Every old and new parameter decodes to the documented actor and pose.
- Existing `0x7Fxx` values remain byte-for-byte compatible, while registered `0x7Exx` page-two values resolve independently.
- Invalid actor and pose values fail safely.
- Definition, pose, draw, and adapter tables have compile-time or unit-tested bounds.
- Darunia's dance state follows the intended sequence and restarts safely after dialogue.
- Ruto surface and dive state transitions remain bounded and handle missing water.
- Dialogue selectors never mutate progression.
- Hyrule Field state restores the correct sequence after dawn and fanfares.
- Randomized night selection uses and preserves the existing resolved mapping.
- Weather source transitions do not stop unrelated SFX.

### In-Game Matrix

- Test all existing catalogue parameters before testing additions.
- Test every standard addition with Alt Assets off and on, including blinking, tracking, soft collision, focus height, dialogue, room reloads, and scene transitions.
- Test native story versions of Malon, Saria, Ruto, Darunia, Nabooru, Zelda, and Ganondorf without debug-menu warp shortcuts.
- Test Ruto grounded, surface, dive, missing-water, conversation, room reload, and multiple-placement cases.
- Test mounted Ganondorf and Phantom Ganon in a separate scene so experimental failures are attributable.
- Test Lost Woods native placed rain through Room 4.
- Test persistent outdoor rain with Navi, enemies, torches, pickups, menus, fanfares, and scene SFX.
- Test repeated Hyrule Field day/night cycles, including fanfares during day and night and music randomization on and off.
- Verify that the configured or resolved night track is the one heard and that no tracks layer.

### Build Gate

Run formatting and available native tests locally, followed by Linux, macOS, and Windows CI. Windows produces the user test artifact, but failures on other platforms are investigated rather than ignored.

## Deliverables

- Stabilized source branch with independently reviewable weather and actor commits.
- Automated tests and CI evidence.
- Windows test artifact.
- Human-readable Prelude parameter table labeling standard and experimental entries.
- Regression checklist recording Alt Assets, native story, weather, audio, and actor results.
