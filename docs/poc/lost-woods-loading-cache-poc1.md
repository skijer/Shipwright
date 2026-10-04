# Lost Woods loading cache and pedestal animation POC

Base: `8481c2575973cf6d66a90a42ae584b4369eb351d` (`poc/heart-magic-cosmetics`).
Candidate branch: `poc/lost-woods-cache-animations`.

Cor approved restoring the material metadata cache with diagnostics and including
the remaining Master Sword exit animation / child sword orientation fixes, plus
giving the pedestal priority over nearby static Saria and Skull Kid talk offers.
This is a test candidate; runtime acceptance and master promotion are pending.

## Loading change

Compatible custom Prelude display-list imports previously reread and decompressed
the entire project metadata before checking a binding. The supplied room-11
export contains 4,706,089 bytes of metadata; its room-9 graph imports 204 custom
lists. A production-factory regression reproduces 32 metadata reads for 32
unbound Alt imports before the fix, and one read after it.

The cache is owned by the physical archive, including negative binding results.
An initial read/parse is serialized so concurrent imports do not duplicate it.
Failed or malformed reads remain retryable. Weak ownership does not keep removed
archives alive. Explicit resource refresh and the existing archive reopen/reorder
paths invalidate metadata. Binding recognition, selected materials and generated
scroll commands retain their existing behavior. Native resources outside the
custom Prelude namespace continue through the original parser.

This restores the bounded `4b8dc69` cache after the separately diagnosed dungeon
asset-path repair. It adds cache-hit/miss and invalidation-generation diagnostics
to help explain the previous report that the cache did not improve the game.

## Default-on Lost Woods diagnostics

`gDeveloperTools.PreludeLoadProbe` defaults to 1 in this POC. No console setup is
required. `[PreludeLoadProbe]` records are emitted for Lost Woods room changes,
material imports, metadata invalidations during a frame, and frames at least
250 ms long. Quiet warm frames do not emit.

Each record includes scene, starting/rendered/previous room, Alt setting,
frame/update/graphics time, binary-list decoding, inclusive material lookup,
metadata read/parse time, bytes/read count by physical archive, cache hits/misses,
and invalidation generation. Debug-level `[PreludeMaterialCache]` records name
the invalidation boundary. Generation changes between frames remain visible in
the next emitted record.

Root-level frame/update/graphics and import timings describe the main thread.
The nested `all_threads_completed_work` record also includes worker imports,
with metadata reads/bytes by archive and decode/lookup/parse durations. Work is
counted when it completes, including operations started before the frame.
These work durations can overlap and are not additive frame attribution.
Material lookup includes metadata I/O and parsing. Frame residuals can still
include waits, texture work, actors, rendering and GPU work. Log formatting
occurs after the measured interval. Disabled/out-of-scene diagnostics do not
take the metrics mutex; imports retain a timestamp and atomic completion check.

A separate `kind: state_reload` record measures from before destruction of the
old Lost Woods play state through initialization of the next state, including
worker imports. It reports `state_reload_ms`, source/target scene and room, and
metadata work even when the interval is shorter than 250 ms. Initial entry into
Lost Woods from another state is not covered by this departure-scoped record.
Reports also group resident actors by id, parameters and room so repeated actors
can be compared across the room-10 age swap and subsequent room transitions.
Snapshots are taken only when a report is emitted, after the timed interval.
`actors_after_reload` is immediately after initialization, before setup actors
spawn; use the following frame's `actors` snapshot to compare resident room NPCs.

## Native CPU comparison

Archive: `1-room 11 switch probe.prelude.o2r`, SHA-256
`6254e351a61a7c4578280e2d95d33d6c70c63c11d9255aea4d0db1d813a55b6d`.
Same graph replay, route 0 → 9 → 11 → 8 → 10, retaining resources between rooms:

| Room | Baseline metadata reads | Candidate reads | Baseline replay | Candidate replay |
|---|---:|---:|---:|---:|
| 9 | 204 | 1 | 1,661.6 ms | 811.9 ms |
| 11 | 691 | 0 | 4,064.6 ms | 7.1 ms |
| 10 | 385 | 0 | 2,440.1 ms | 190.3 ms |

The candidate's counters agree with independent read totals: room 9 has one miss
and 203 hits, room 11 has 691 hits, and room 10 has 385 hits. Both main-thread
and all-thread diagnostic counts match the independent totals. The final
candidate spends 119.4 ms in room-9 metadata lookup and under 0.2 ms in room-11
and room-10 lookup. Host I/O varies between runs (an earlier candidate replay
was 457.0 / 6.5 / 178.4 ms respectively); read counts are the deterministic
regression result. This replay compiles
the production ZIP read, metadata functions, material resolver and binary-list
decoder. It excludes the complete resource manager, XML parsing, actor execution,
texture upload and renderer/GPU. These are host CPU measurements, not measured
game frames or a promised game speedup.

## Pedestal change

The persistent Skip Story Cutscenes setting was also cancelling the independent
post-reload exit action. The local child hop/adult sword swing now runs unless
a fresh B press skips that phase. Entrance and exit skips remain independent.
The custom arrival also starts moving immediately instead of holding its first
pose for 20 action updates. Native Temple of Time synchronization is unchanged.
Completion restores the saved floor-safe position while retaining the animation's
pedestal-facing yaw, avoiding a final twist back to the arbitrary approach angle.

Child ceremonies using an ordinary selected Pak sword or custom equipment sword
receive a model-local half-turn. The inspected compatible replacement blade runs
along local -X, and a full ZYX direction-vector test verifies reversal even with
nonzero incoming hand angles. The dedicated native ceremonial sword keeps its
original rotation; an Alt replacement for that same ceremonial resource retains
the native asset contract. Selected assets remain authoritative. Root/forearm,
adult and ordinary gameplay rotations are unchanged. Visual blade direction
still needs game proof.
The local room-10 swap/return, camera, equipment and progression safeguards remain.

The supplied cancellation clip shows about 0.53 seconds of whole-scene stillness
at the instant age reload, followed by a longer first-pose hold while the timer
and fairies keep moving. Source/fixture checks clear the departure's cutscene,
audio and player lock through teardown and fresh-B arrival completion. The cache
addresses the repeated metadata cost during loading; the custom-only arrival
change addresses the separate pose hold. This is not proof that every remaining
game stall is eliminated.

## Pedestal interaction priority

The supplied room-10 archive places static Saria (`En_Viewer` params `0x7F23`)
beside the custom pedestal (`Bg_Toki_Swd` params `0x4C57`) and static Skull Kid
(`0x7E08`) nearby. The player checks dialogue before carry/sword interactions;
targeted actors bypass ordinary dialogue distance checks.

When the pedestal has already offered a valid interaction through the existing
distance/facing/player-state checks, only static Saria and Skull Kid stop offering
a competing conversation. Existing accepted or active dialogue continues, and
ordinary NPC interaction resumes when the pedestal is no longer offered. No NPC
is moved and no global targeting range is changed.

## Verification and next runtime check

- Factory tests cover canonical/Alt ownership, explicit parent precedence,
  unbound read reuse, refresh/removal of bindings, failed and malformed read
  recovery, and eight concurrent imports sharing one metadata read.
- Native scroll tests cover profile recognition, command insertion safety,
  generated commands, frame reset, disabled state and buffer lifetime.
- Diagnostic tests cover room tags, separate main/worker counts, worker operations
  spanning frame start, frame reset, quiet frames, disabled/scene filtering,
  cache hit/miss totals, cross-thread invalidation generation, and the separate
  state-reload interval with source/target tags and main/worker work.
- Pedestal tests exercise both ages and randomizer states, both story-skip settings,
  independent fresh-B skips, immediate custom arrival versus native hold,
  floor-safe return without yaw snap, teardown/control release, sword-model
  selection and progression/equipment preservation.
- Static dialogue tests cover Saria/Skull Kid yielding to the pedestal, ordinary
  offers away from it, other actors, and accepted/in-progress conversation.
- Local cache harness stubs the logging backend; it is not a complete game build.
  Windows build status must be checked on this exact candidate commit.

Use the same active scene packs and Alt setting as the supplied test, with the
accepted dungeon asset repair retained. Restart for a fresh 0 → 9 → 11 route,
then repeat the route warm. Test both age-swap directions with Skip Story
Cutscenes enabled, check that the exit action starts moving immediately, and
verify child Link's blade points down during the pull. A fresh B should skip
only the active phase and restore control. Approach the sword beside Saria and
Skull Kid, confirm A starts the pedestal, then step away and talk to both NPCs.
After the age swap, also repeat 10 → 8 → 10 and check geometry and actor counts.
The resulting log helps attribute any remaining freeze or actor duplication.

No scene archive or dungeon repair is modified by this source POC. The reported
post-swap missing geometry and duplicated actors still require a game check;
this patch does not claim to repair actor lifecycle or scene geometry.
Rollback is the `8481c25` executable with the same accepted asset configuration.
