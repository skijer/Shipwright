# POC6 Stable-Weather Static Actor Expansion Design

## Purpose

Expand the proven static story-actor catalogue without carrying forward the
unresolved POC5/POC6 weather and audio regressions. This branch prioritizes a
clean Adult Ruto water adapter, adds the remaining sages, and finally resolves
Adult Zelda's invisible rendering failure.

## Branch Foundation

- Branch from `b13d157843d4825d1718e6b279de9488ac489f4e`, the proven POC4 static actor
  dialogue/tracking build.
- This ancestry already contains `d54a4c33da15eaf7eb8dd5eca65581b39c83d38c`,
  the stable POC2 weather build with the thunder-type dropdown.
- Do not merge or cherry-pick the later global outdoor rain, private weather
  mixer, Hyrule Field night-music, or POC6 weather stabilization work.
- Preserve all existing `0x7Fxx` actor parameters and their validated behavior.

## Catalogue Framework

Introduce a second static-actor parameter page in the `0x7Exx` range. Dispatch
must be explicit and table-driven enough that adding a new actor or pose does
not consume or reinterpret an existing `0x7Fxx` value. Unknown and unsupported
parameters must fail safely without drawing invalid geometry, offering broken
dialogue, or mutating story state.

The catalogue guide must remain human-readable and identify actor, parameter,
pose or behavior, collision, blinking, tracking, dialogue, and experimental
status.

## Adult Ruto Adapter

Adult Ruto is the first and signature adapter after the catalogue framework.
Provide three explicit modes:

1. Grounded static idle.
2. Water-surface idle using a narrow port of native `En_Zora` water-height and
   buoyancy behavior.
3. A selectable native-style dive loop.

The water adapter may consume water-surface queries, vertical approach,
buoyancy, and the minimum animation state required to surface and dive. It must
not inherit `En_Zora` dialogue, rewards, patrol routes, actor replacement,
story flags, or scene-specific behavior. It must not execute Adult Ruto's Water
Temple cutscene logic or Child Ruto's Jabu-Jabu sequence.

If a water mode is placed outside valid water, Adult Ruto must fail safely into
the grounded static mode rather than sinking, floating at an arbitrary height,
or repeatedly searching for a nonexistent surface. Room transitions and
re-entry must initialize the selected mode deterministically.

## Remaining Sages

### Darunia

- Provide a stable standing idle.
- Provide the complete native dance cycle as a separate explicit parameter.
- Preserve blinking, conversational head tracking, safe post-story dialogue,
  and soft pushable collision where supported by the existing framework.
- Do not initiate Goron story events or rewards.

### Nabooru

- Provide a stable standing idle with blinking, conversational head tracking,
  safe post-story dialogue, and soft pushable collision.
- Do not initiate Spirit Temple, Iron Knuckle, capture, reward, or cutscene
  state.

Both adapters must use static-owned state and must not alter the initialization
or lifecycle of their native story actors.

## Adult Zelda Repair

Treat Zelda as a rendering investigation before a feature expansion. Trace the
existing invisible-model and yellow-polygon symptom through:

- required object and resource availability;
- skeleton and animation initialization;
- limb override and post-limb callbacks;
- eye and mouth segment assignment;
- opaque/translucent display-list state; and
- model variant selection.

The first milestone is a visibly correct, stable Adult Zelda standing model.
Only after rendering is proven may blinking, tracking, dialogue, collision, or
additional poses be enabled. A talkable invisible actor is a failure, not a
partial success. Zelda must not trigger courtyard, final battle, tower escape,
or ending cutscene state.

## Explicitly Deferred Actors

The following actors are outside this build:

- standing Ganondorf;
- mounted Ganondorf;
- Phantom Ganon; and
- non-hostile decorative Gerudo guard.

They remain future catalogue additions after Adult Ruto, the remaining sages,
and Zelda have passed runtime testing.

## Vanilla and Existing-Actor Safety

The static adapter path must remain distinguishable from native actor
placements. No static parameter may modify global initialization, kill rules,
story gates, dialogue selection, or cutscene behavior for native Malon, Saria,
Child Ruto, Adult Ruto, Darunia, Nabooru, or Zelda actors.

Existing POC4 catalogue actors must retain their current rendering, pose,
blinking, tracking, dialogue, and collision behavior. In particular, native
Child Ruto must remain free of the previously observed Jabu-Jabu dialogue
softlock.

## Development and Verification Strategy

Implement one bounded capability at a time using red-green-refactor tests:

1. Catalogue page dispatch and invalid-parameter handling.
2. Adult Ruto grounded mode.
3. Adult Ruto water-surface mode.
4. Adult Ruto dive-loop mode and invalid-water fallback.
5. Darunia idle and dance modes.
6. Nabooru idle mode.
7. Zelda rendering diagnosis and repair.
8. Whole-catalogue and vanilla-story regression gate.

Automated tests must exercise parameter decoding, adapter selection, safe
fallbacks, animation selection, and isolation from story behavior. CI must build
the complete branch on supported platforms. The user-facing runtime matrix must
cover existing POC4 actors, native Malon/Saria/Child Ruto, Adult Ruto on land and
in shallow/deep water, room re-entry, Darunia's full dance loop, Nabooru, and
Zelda under vanilla and alternate assets.

## Release Boundary

This actor branch does not claim to repair the weather/audio issues documented
for the later diagnostic weather pass. The actor build is pushed independently
from the stable POC2 weather foundation. Weather diagnostics resume only after
the actor checkpoint has been built, preserving a clean comparison boundary.
