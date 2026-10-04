# Streamed audio beyond 256 entries

Status: prepared for an integration-branch Windows test build; not game-runtime validated.
Focused regression tests pass, including intermittent rain and static-actor coverage.
The ComboShip merge remains out of scope until game-runtime validation succeeds.

## Changes

- Host font IDs stay full-width in shared C/C++ sequence metadata, players,
  channels, notes, slow loads, sample-cache records, and cache keys.
- Runtime no-font is `-1`; bank 255 is a usable bank. Native sample-bank and
  bytecode sentinels are unchanged. Sequence ID 65535 remains reserved.
- Binary archive operands remain bytes. Streamed XML packs retain their existing
  eight-byte CRC encoding; the widened host array decodes it explicitly.
- C6/EB bank selection checks operand bounds and uses the already-resolved
  player ID. Fanfare comparison uses full bank IDs and resource-owned storage,
  preserving native request flags and pending MM requests.
- Soundfonts stay in their authoritative resource-manager storage instead of
  exhausting/evicting entries in the small native font cache. The metadata table
  is sized for the complete catalog plus MM headroom, outside the N64 init pool.
- Sequence bytecode retains its mutable cache copies. Cache entry limits are
  checked; permanent overflow uses the normal cache. Cleanup and temporary
  allocation cannot overwrite an enabled player's sequence. If no safe buffer
  is available, the new load fails rather than corrupting an existing player.
- MM font remapping preserves full IDs, refreshes aliased metadata after reload,
  and uses contiguous sequence registration instead of wasting 16 slots per entry.

## Reproduce local checks (Linux/GNU toolchain)

From the repository root:

```sh
bash scripts/diagnostics/run_stabilization_tests.sh /path/to/mm.o2r
```

The archive argument is optional and is read-only. This runs 12 standalone
binaries, 10 structural-audit tests, the streamed-audio runtime harness, and,
when supplied, the Skull Kid display-list cull replay.

The runtime harness compiles selected complete production function bodies with
the real engine types. Resource/archive access, OS dependencies, and audio output
are fixtures. It tests 600 distinct font loads and 600 sequence starts; boundary
IDs 254/255/256/257/349/511/512; actual channel/note inheritance, instrument lookup,
readiness, release, cache identity, native/custom fanfare comparison, active/resting
MAIN protection, invalid IDs, and 128 MM registrations without moving the maps.
It is not a full translation-unit or game build and does not decode/play audio.

The harness also passes address/undefined-behavior sanitizers. Leak detection
could not run in this environment because its process inspection was blocked;
it was explicitly disabled for the sanitizer run.

## Remaining gates

- Build the complete integration branch with its dependencies once ready for a
  game build; the current workspace lacks the required submodules/toolchain.
- Load the actual Twilight Princess pack and mod stack to verify the selected
  Hyrule Field (Night) track's runtime mapping, decoding, looping, and handoffs.
- Listen through vanilla child Market, day/night Field, fanfares, scene reloads,
  rain, and MM audio transitions. A focused test pass is not audible proof.
- Retest both Skull Kid variants in game. The earlier archive-backed cull fix
  is separate from these audio changes and has not yet proved crash freedom.

This removes the byte-index ceiling; it does not promise unlimited resident audio
or unlimited simultaneous streams. Native asynchronous command operands and
sample-bank formats are not expanded. Loaded audio still consumes host memory,
and sequence caches retain finite capacity with safe failure on exhaustion.
