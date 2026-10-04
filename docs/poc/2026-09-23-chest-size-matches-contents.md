# Chest Size Matches Contents restoration

Baseline: `marsh6487/Shipwright`, `integration/nei-weather-static-actors`,
`f889527c4c0f97403eae3806e43ec367cd22c7ad` (remote tip inspected September 23).
Candidate: `poc/soh-chest-size-matches-contents`, a direct child of that baseline.
The baseline is retained in git history. Later Midna POC commits in another
worktree are separate work and are not reverted or silently promoted here.

## Behavior

**Enhancements → Quality of Life → Containers Match Contents →
Chest Size Matches Contents** is an independent, default-off checkbox.
The CVar is `gEnhancements.ChestSizeMatchesContents`.

| Contents | Size |
| --- | --- |
| Major items, lesser items, health rewards, boss keys | Large |
| Junk, small keys, Skulltula Tokens | Small |

This restores the legacy category mapping removed in upstream
[a134d2c5](https://github.com/HarbourMasters/Shipwright/commit/a134d2c59a6cbccbe739fb8f73413a4d331b5114).
It uses the current item entry/category adjustment for normal, NEI and randomizer
chests, including the fixed Hyrule Field Time Gate reward. The current container
texture checkbox and its Stone of Agony requirement remain independent.
The guessing rooms of the Treasure Chest Game retain their authored sizes.

Actor type, params, switches, rewards, collection flags, Lens behavior and spawn
conditions are preserved. Scale drives both rendering and the existing dynamic
collision transform. Focus height, opening light and ice-smoke dimensions follow
the effective size. The native item-opening animation and Fast Chests policies
remain in control. Size changes apply to loaded chests; disabling the option
restores their authored size.

The four original reachability corrections are restored for the Ganon's Castle
Light Trial/Gold Gauntlets chests, MQ Deku Tree Song of Time chest and Spirit
Temple Compass chest. Corrections require the original scene/room/params and
known native coordinates on the affected axes. They do not accumulate and reverse
when disabled. A custom chest retaining all those identifiers and affected-axis
coordinates can still match; this is not a full custom-scene detector.

## Preservation and verification

No scene archives, textures, item tables, weather, audio, static actors, Midna,
pedestal or scabbard implementations are changed. Production changes are limited
to the menu and chest actor, with CTest registration for the new regression.

Verified locally:

- `python3 scripts/diagnostics/run_chest_size_tests.py`: real production sizing,
  update, opening and smoke functions; all 12 authored types; seven categories in
  normal/randomizer contexts; disabled defaults and toggling; Time Gate metadata;
  texture/Agony independence and model fallback; Treasure Chest Game exclusions;
  all four placement corrections, reversal, no drift and placements moved away
  from their affected native coordinates. Runs with UndefinedBehaviorSanitizer.
- The same script syntax-compiles the complete chest actor against integration
  headers. It does not claim a complete game build.
- `python3 scripts/diagnostics/run_time_gate_chest_tests.py`: unchanged adult
  placement, switch/offer/receipt, re-entry, ordinary chests and message routing.
- `git diff --check` and clang-format 14 on changed C/C++ code.

Status: implemented and locally verified candidate. Full CI and in-game runtime
acceptance are separate checks. No claim of observed gameplay or Alt Assets parity.

Smallest runtime check: enable the checkbox, inspect/open a normally small chest
with a major reward and a normally large chest with junk, toggle off, and re-enter.
Repeat with the active Alt Assets pack. Confirm the Time Gate chest still awards
the Time Gate. In unmodified dungeons, check the four affected ledges for access
when their contents produce the opposite size.

Recovery: disable the checkbox to restore default sizing; revert the isolated
restoration commit to remove the feature while retaining the baseline integration.
