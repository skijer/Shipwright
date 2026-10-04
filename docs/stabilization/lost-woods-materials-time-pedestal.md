# Lost Woods material binding and decorative time pedestal

The Lost Woods scene uses an authored time pedestal and a transplanted Chamber
of Sages material. These opt-ins do not replace the original Temple of Time
sword or infer animation from texture identity.

## Sages platform

Prelude material/paste records can request:

```json
{
  "nativeAnimation": {
    "version": 1,
    "binding": "material-motion",
    "source": "oot.chamber_of_sages.platform",
    "logicalWidth": 32,
    "logicalHeight": 32
  }
}
```

This is the opaque main platform material from
`kenjyanoma_room_0DL_001020`, using `kenjyanoma_room_0Tex_00D618`.
The profile preserves authored sampling shifts and uses native gameplay-frame
scroll: layer 0 moves `(-1, +1)`, layer 1 moves `(+1, +1)`, with logical 32x32
tiles. The import validator requires the expected two RGBA16 render tiles and
rejects unsupported control flow or repeated material setup.

The factory accepts ordinary `custom/prelude/` paths and their single `alt/`
companions. Only the metadata key is normalized. Archive ownership still comes
from the physical resource path, or the explicit resource parent, so an
unrelated higher-priority project's metadata cannot bind the material.

The scene's caustics continue using the existing
`oot.zoras_domain.caustics` profile. Caustic brightness and face selection are
authored in the scene archive; this profile does not introduce dynamic lights.

## Time pedestal

Set a `Bg_Toki_Swd` actor's parameters to exactly `0x4C57`. Other parameter
values keep the original story sword behavior. The authored Lost Woods actor
is in room 10 at `(-736, -63, -2395)`, yaw `15101`.

The actor clones the native child/adult sword cutscene, keeping its player cue,
four camera splines and white transition. Camera and player positions receive
the same rigid transform from the native Temple of Time sword origin
`(-1, 68, 0)` to the authored actor. The native scene destination, story effects
and Temple-specific lighting commands are excluded. The original destination
frame determines when the local age swap occurs.

The player returns to the position and facing captured at interaction, in the
same room, through the existing SwitchAge respawn machinery. This variant does
not award an item, require Prelude of Light, discover the vanilla child/adult
spawn entrances, or change quest/event/info flags. The equipment handoff uses
owned, age-compatible gear and supports an empty sword slot. The ceremonial
adult sword is a live player model, without an ownership grant.

A default-zero field in the existing NEI save metadata distinguishes this
intentional age swap from vanilla adult-save repair. It allows saving and
reloading without the engine granting an unowned Master Sword. Existing saves
retain the original repair behavior; genuine Master Sword acquisition ends
the exception. This does not use a quest flag or change the vanilla save layout.

## Independent pedestal animation phases

The local draw/return ceremony honors `TimeSavers.SkipCutscene.Story` and a
fresh B press after its opening 20 frames. Completing or manually skipping
that ceremony performs the same local age swap and starts the native arrival
animation: adult Link's sword flourish or child Link's hop. Arrival can be
skipped separately with a fresh B press; automatic story skip applies to departure.
Holding B across the reload does not generate a second press. Input is read
only during the live update, after the engine polls the controller.

The arrival continuation is consumed by the next primary player initialization
and requires the captured file, age, scene, entrance, room, position and yaw.
It does not invoke the Temple of Time/randomizer start-mode hooks. Completion
or skipping restores the captured return position after native root-motion
cleanup and clears the temporary ceremonial equipment without changing owned
items or save equipment.

During the child's frame-87 withdrawal handoff, the adult's insertion before
frame 70, and the adult arrival flourish, the hand uses the selected PAK Master
Sword pieces, then the custom AltAssets Master Sword, then the native/alternate
age-specific hand display list. Both ages and the local pedestal resolve the
same weapon pieces, including adult-only equipment packs used during child
Link's ceremony. Selection follows body, equipment pack, Master Sword slot,
then forced equipment precedence; within each source, adult pieces take priority
with a child-piece fallback. The weapon is composed with the current age's hand
and does not depend on the weapon equipped on B. The adult's frame-70 insertion
closes the hand even when a late equipment hook would otherwise redraw a sword.
Without a selected PAK or custom Master Sword resource, the pedestal retains its
native/alternate world-object display list. Legacy packs replacing only that
world-object path still need a ceremonial resource replacement for matching hands.

The local pedestal resolves the supporting stump surface at initialization and
offers within 60 horizontal units only when Link is grounded on that surface
(same collision owner, floor height within 2 units, feet within 4 units).
The existing 40-unit actor-height check remains a backstop. Any heading works;
the ceremony aligns Link. The turn-in-place state also checks the native grab
handler for this exact custom pedestal, so orienting the stick does not discard A.
It keeps the existing Saria/Skull Kid talk suppression while the sword is offered.
Active dialogue, cutscenes, airborne states and real item offers keep their guards.
Skipping or completing the departure now uses a fast white fade before the age
reload and a matching fade-in. Link retains his ceremony pose during departure;
the saved return position remains the original approach position.

## Verification commands

```sh
python3 scripts/diagnostics/run_time_pedestal_tests.py
python3 scripts/diagnostics/run_pedestal_sword_selection_tests.py
python3 scripts/diagnostics/check_time_pedestal_syntax.py
python3 scripts/diagnostics/run_native_material_probe.py \
  --json-include /path/to/nlohmann/include \
  --spdlog-include /path/to/spdlog/include
```

The material tests exercise the production adapter and binary display-list
parser with controlled archive I/O, including physical archive ownership for
alternate resources. The sword tests compile the relevant production C/C++
functions and native cutscene data with engine boundary fixtures. Neither is
an in-game rendering test or a substitute for a complete platform build.

The September 20 candidate is based on `4a16cd05ce904bbc19ef2707ed22454c6d5bfe65`.
The supplied clips show a frozen last frame during the instant reload, tight
interaction angles beside Saria, and mismatched pedestal/hand swords. Focused
tests now cover proximity and talk priority, normal/manual/automatic departure
and arrival, progression preservation, selected weapon handoffs and age-separated
PAK source maps. Modified C translation units also pass real-header syntax checks;
the existing static story talk and viewer integration tests pass.

Runtime acceptance is pending. In particular, verify child grip and downward
blade direction, the selected sword's placement in the pedestal, no model change
at either handoff, and fade behavior with Story Skip both on and off. The selected
pedestal mesh uses Link's weapon scale and places the grip 40 world units above
the actor anchor; the tests check source selection, not this visual calibration.

## Fixed exit camera candidate

`poc/pedestal-exit-camera` builds on `a42472291f7fc8df7d329aaa8e0df089bf284621`,
whose Windows and Linux jobs passed in Actions run `35532669510`. That parent
contains the wider interaction and shared sword changes; their runtime acceptance
is still pending. The user reports the fade looks acceptable and requests a fixed
exit view, like the Temple of Time, instead of the normal respawn follow camera.
The supplied `20260920151506404.mp4` ends as that follow view becomes visible.

The local exit now acquires a manual subcamera on its first player update, after
scene initialization finishes configuring the main camera. Its front-side shot
is fixed to the pedestal's position and yaw, with age-specific height and a
55-degree field of view. This is an authored local composition, not a copy of the
Temple's scene camera data. It remains fixed through the hop or flourish. Normal
completion and a fresh B skip copy the view back to the main camera, release the
owned subcamera and end letterboxing. Player teardown uses the same cleanup;
camera identity checks protect a reused slot. An unavailable or already occupied
camera system leaves animation and skipping usable.

Focused production-function tests cover first-update acquisition, both ages,
fixed framing despite player movement, normal and B release, immediate B before
acquisition, allocation failure, another active camera, reused IDs and teardown.
The fade, interaction reach, equipment handoffs and save/progression rules remain
unchanged. Runtime framing and clearance with the user's scene and model packs
still need confirmation; this camera follow-up is a candidate, not a promoted master.

In game, verify repeated child/adult cycles, camera clearance, exact room-10
return placement, reset/skip behavior, owned and unowned sword saves, and music
resumption. Check the Sages tunnel with alternate assets both enabled and
disabled, and evaluate the authored caustic brightness against water clarity.

## Stump clearance follow-up

The user accepted the skip/fade and reported the selected pedestal weapon is
correct, but its resting blade points upward and interacting still requires
too much repositioning. Run `35537908841` was cancelled at their request.
This follow-up preserves the previous fixed exit camera and shared sword source.
Only the resting selected mesh changes from +90 to -90 degrees around Z.

The scene's pedestal anchor is Y=-63, its flat stump top is Y=-56, and nearby
ground is Y=-96. The former 100x40 offer included that ground. The new floor
checks cover the stump while excluding ground, bark below the top and airborne
players. Native turn-in-place only tries item use; the custom offer now reaches
the existing A grab handler through the ordinary cutscene/held-item guards.
Tests reproduce the old off-stump offer and swallowed turn-in-place A, then
exercise both ages, eight headings, talk priority and the state exclusions.
The grab handler itself is an engine boundary in this dispatch test.

Static Saria's cylinder radius is reduced from 20 to 8, preserving height and
the original shadow size. With Link's 12-unit radius, this clears the nominal
ceremony center even after the cylinder coordinates are rounded to integers.
The scene candidate moves only room-10 Saria from `(-710,-68,-2373)` to
`(-710,-68,-2371)`, two world units toward the left edge as approached in the clip.

`Lost_Woods_Stump_Clearance_POC4.prelude.o2r` is built from the exact Continuous
Rain POC3 archive by `scripts/diagnostics/build_stump_clearance_poc.py`. Its two
normal/Alt compiled room records and retained Prelude actor placement agree;
all other 6,410 archive members are byte-identical. Candidate SHA-256:
`f89b1efd8f918de33454076f082cf97487c3059f374ff4868477efaa4fd71bfe`.
Use it as the replacement Lost Woods archive, together with the new executable.

The real-header C23 check also exposed a missing declaration for
`Play_CameraGetUID` in the prior camera candidate. Its existing implementation
now has the matching public declaration. Focused tests and syntax checks are
implementation evidence; actual prompt stability, the downward resting model,
Saria clearance and camera framing still require the user's runtime check.

## September 22 user feedback

The user still has to reposition repeatedly to obtain the pedestal's A-button
prompt and reports that its geometry/collision makes the interaction awkward.
The interaction remains unresolved despite the existing automated checks. Carry
the current pedestal work forward with the cumulative integration and retain
this limitation; the publication recovery does not change the interaction or
claim runtime acceptance of the stump-clearance candidate.

The user subsequently authorized a prompt fix and diagnosis of the child's
crooked left hand and slanted sword. The
[prompt and child grip follow-up](pedestal-prompt-child-grip.md) records the
actual collision and native-model evidence, the scoped corrections, passing
regressions, and remaining in-game acceptance checks. It starts from the
complete SoH integration at `74edaf10` and requires no new scene archive.
