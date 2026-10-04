# Shipwright working integration

Repository: `marsh6487/Shipwright`.
Working integration branch: `integration/nei-weather-static-actors`.

The working build is cumulative. Start new work from the current remote tip of
this branch, then retain any newer published working candidates. A recent
commit date or a pushed feature branch does not establish that its parent
contains the other working features. Verify ancestry and the relevant code
before publishing a replacement working build.

## September 22 recovery

The September 21 spin/cosmetics line started from September 12 commit `9a76eb63`.
The later working feature chain was still published through `6f03c439`, but was
not in that build. In particular, **Hide Back Equipment and Scabbard** was added
by `61bf5bad` on September 18 UTC and was never deleted from the later chain.

This recovery joins the complete chain at `6f03c439` with `f8b87d2e`, preserving
both histories. The cumulative branch must contain these feature commits:

| Working feature | Included commit |
| --- | --- |
| Native Prelude material scrolling | `0a4dc064` |
| MM catalogue and actor rendering fixes | `286a92d3`, `c8a3186e` |
| HD actor blinking and retained scene resources | `8092059a`, `4658ac61`, `9f95b9fc` |
| 3DS Skull Kid, Tael and static-actor draw distance | `e4e71eee` |
| Seated/standing Anju and Kafei draw repair | `4773c4bb`, `a89adb09` |
| PAK selection crash fix and hide-back-equipment option | `61bf5bad` |
| Reusable Water Temple and Zora's Domain caustics | `8db6380f` |
| Authored rain, Time Gate chest and Ruto face fixes | `c433b6c7` |
| Custom heart and magic model colors | `8481c257` |
| Lost Woods material cache and independent pedestal skips | `4a16cd05` |
| Pedestal sword, camera, stump prompt and Saria clearance; interaction still needs iteration | `6f03c439` |
| Native sampling for HD TorchFlame effects | `51889826` |
| Transformation cosmetics, scene reentry colors and Zora shield | `f8b87d2e` |

The recovery preserves the later functional files when they overlap the older
formatting-only commit. The CMake test list retains both PAK and sampling tests.
Source integration does not modify scene/model archives or replace accepted
asset masters. Existing asset-pack requirements still apply.

The user reports that the pedestal's collision/geometry still makes the A-button
prompt awkward to obtain. Preserve the existing pedestal work in the cumulative
build, but keep that interaction issue open. Passing the current automated tests
does not establish that prompt accessibility is resolved. The recovery itself
did not redesign the pedestal interaction or change its collision.

The subsequent [pedestal prompt and child grip follow-up](stabilization/pedestal-prompt-child-grip.md)
starts from the complete recovery at `74edaf10`. It removes the custom sword's
blocking cylinder and applies the native child ceremony's placement to the
selected blade while preserving the animated hand. Focused regressions pass;
prompt ease and the selected model's appearance still need an in-game check.

## Verification and future changes

The [September 24 continuity audit](stabilization/2026-09-24-feature-continuity.md)
found two additional published follow-ups missing from `0574f79c`: Midna POC3
(`822f46b1`) and Zora shield ground/swim anchoring (`5d0b8384`). The repair merges
both histories into that cumulative tip, retaining chest sizing and RPG pickups.
The original complete Midna POC3 archive was correct; its newer rendering paths
were absent from the standalone executable.

[`required-feature-baselines.json`](required-feature-baselines.json) records the
required published feature milestones. Add each newly integrated feature's
commit to this inventory when carrying it forward. Run:

```sh
bash scripts/diagnostics/run_cumulative_regressions.sh
```

The distribution workflow now requires this check before generating archives or
platform builds. It uses complete Git history and checks both ancestry and the
restored production regressions. Ancestry alone cannot detect a later revert,
and this gate cannot protect an obsolete branch that omits the gate itself.
Review the source changes and publish from this cumulative branch.

Midna audio content already lives in the mod archive. See
[interchangeable Midna cues](poc/midna-audio-swaps.md); changing a WAV does not
require an executable build. Changing event routing or timing still does.

Run the focused PAK/pedestal, native-material, actor/weather/audio, custom-color,
and Zora barrier checks when their integration boundaries change. Validate the
published Windows build on the exact cumulative commit. Keep build/static
verification separate from runtime acceptance; this merge does not turn prior
untested configurations into accepted ones.

Local recovery checks passed: real-ImGui PAK menu/config selection, pedestal
interaction/sword/camera, transformation cosmetics and Zora barrier, and all
actor/weather/audio/Time Gate/custom-item checks from the stabilization runner.
The inherited Time Gate fixture required the production pedestal/Kaleido headers
and item-age table used by its newer extracted functions; only the fixture and
driver changed. Formatting changes preserve all noncomment source tokens.
Fresh application builds and combined-pack runtime confirmation remain separate.

The user requests that working features travel together. Do not replace this
working line with an older isolated feature branch. Shipwright and ComboShip
are separate publication destinations; match each requested feature to its
repository before pushing.
