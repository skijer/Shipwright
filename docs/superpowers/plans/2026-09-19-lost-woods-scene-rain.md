# Authored Lost Woods continuous-rain probe

Approved by cor on September 19 after individual rain placements continued to fail.
Base: d3323797f9c29f422031493a33b44512020c632a (published tree bcde7a5).
Implementation branch: poc/lost-woods-rain-small-fixes.

## Contract

The archive that supplies the loaded Lost Woods scene can explicitly request
continuous rain in normal child/adult setups, independent of room tags, entrance,
global rain enablement, or intermittent mode. Native rain actors retain their
proximity behavior elsewhere, with independent ownership and cleanup. Sky
restoration cannot cancel active rain. Underwater drawing stays suppressed while
logical rain persists. Explicit scripted weather takes precedence until the
cutscene ends. Preserve private rain audio, ordinary SFX/BGM, live volume and
thunder controls. No pedestal, geometry, collision, material, or cache changes.

## Task 1: ownership and engine integration

- Add a failing overlap/remaining-owner test to the existing real weather-audio
  fixture. Run it against the old implementation; expect target zero failure.
- Replace anonymous active/inactive notification with per-actor requests keyed
  by PlayState and actor identity. Resolve once after actor updates, before the
  existing environment particle-count update. Release on actor destruction and
  scene reset; deduplicate revisited persistent tags by authored placement.
- Give scene-level intent precedence over global intermittent settings. Honor
  explicit cutscene rain commands until cutscene completion. Keep Song of Storms
  on its separate existing density channel.
- Test native overlap in both actor orders, final release, thunder/volume,
  lifecycle reuse, mode changes, sky restoration, and cutscene release.
- Verify: bash scripts/diagnostics/run_stabilization_tests.sh. Expected: green.

## Task 2: archive-scoped policy and diagnostics

- Parse a small versioned JSON policy for scene 0x5B, normal setups 0..3.
- Read it only from the archive that supplied the loaded scene resource, once
  per scene initialization; normal/Alt resources use the same policy.
- Reject malformed, incompatible or absent policy without changing vanilla rain.
- Log policy binding, room/source/owner changes and draw-gate changes through the
  normal game logger. Include target/current count, camera water flag and height.
- Verify parser and binding tests, production compilation, and focused suite.

## Task 3: candidate and preservation evidence

- Add only the policy entry to the existing Room10 Recovery POC2 archive.
- Compare every existing uncompressed entry byte for byte and record hashes.
- Prepare a separate code candidate and durable handoff; check existing build
  activity before publication. Never cancel a build or promote a master archive.
- Request one fresh code review; address correctness findings with regression
  evidence. Record full-build and in-game limits separately.

## Runtime acceptance (pending cor's game)

Child/adult; Alt on/off; global rain off/persistent/intermittent; direct pool entry
0→9, surface/submerge/resurface, rooms 3→4→7→8→10 and return, stationary room 10,
revisits, reset/re-entry and pedestal age swap. Verify rain continuity, audio
coexistence, thunder disabled/enabled, and no outside-scene rain leak.

## Review focus

Actor lifetime and duplicate placement handling; scene/resource archive identity;
cutscene precedence; sky writer order; no unintended rain-channel or SFX changes;
malformed JSON and setup boundaries; diagnostics that distinguish density loss
from underwater draw suppression. Standalone tests cannot prove rendered rain,
sound-device behavior, or acceptance of the current pedestal animation.

## Completion evidence

Implementation and focused validation are complete. Cor subsequently requested
Time Gate chest, Treasure Gal eye contrast, and child Ruto face parity in the
same candidate. See docs/stabilization/2026-09-19-rain-small-fixes.md and the
individual component notes. Fresh review findings were fixed and verified.
Full build and runtime acceptance are separate gates.
