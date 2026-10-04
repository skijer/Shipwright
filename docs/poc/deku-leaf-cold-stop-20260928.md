# Deku Leaf completion hang: MM stop before first playback

Baseline: SoH GI PR #17, `68047d9a4d16a67f0ca47d2b48f9fec93c8cb4f1`.
Candidate: `poc/deku-leaf-stop-20260928`, isolated from integration/master.

## Evidence

The user reports that Link finishes the ground Leaf animation immediately before the freeze/crash. The supplied `Ship of Harkinian(2).log` identifies the baseline branch and commit above. Its SHA-256 is `b3becb5a7b269f44a6329fb070c8078c4622aebb65db3cc950679fc17f3de37f`.

The log ends at `09:58:12.438` with routine equipment-cache messages. It has no exception, stack trace, or Leaf-specific event. No `[MmSfx]` startup diagnostics appear in this session. Texture-null messages end at `09:53:37.365`, several minutes before the final entry; they do not establish the cause of this failure. The log alone cannot identify the exact instruction where the user's process stopped.

## Reproduced cause

1. `Player_UpperAction_DekuLeaf` calls `DekuLeaf_Stop` when the blow animation completes.
2. Cleanup calls `MmSfx_Stop(MM_NA_SE_IT_DEKUNUTS_FLOWER_ROLL)` for both ground and glide use. Ground use has not played this MM sound.
3. MM bank initialization occurs lazily through `MmSfx_PlayEx`. Before that, each bank's zero-filled head has `next == 0`, rather than the initialized empty-list marker `0xFF`.
4. `AudioMmSfx_StopById` traverses the uninitialized head forever. `MmSfx_Stop` holds the shared audio mutex while making this call, so the game cannot complete cleanup and the audio thread is also blocked.

The real production bank engine reproduces this hang before first playback. An otherwise identical process calling `AudioMmSfx_Reset` first returns normally. Extending the existing Leaf harness to invoke the real bank stop reproduces the hang at Leaf completion on the unmodified baseline. This fits the user's observed timing and the absence of MM startup diagnostics, but remains an inference about their process until runtime confirmation.

## Candidate and preservation

`AudioMmSfx_StopById` returns immediately while `sMmSfxEngineReady` is false. An engine that has not started has no active playback to stop. The change preserves lazy initialization and the existing initialized stop/request-guard logic.

Only this readiness guard changes production behavior. Leaf animation, gust timing, collision, magic cost, models, sound selection, and all approved GI/rod work remain unchanged. No full audio reset or eager resource loading is added.

## Verification and status

- Baseline: initialized-engine tests pass; the real ground Leaf completion hits the five-second timeout. A separate direct stop-before-play probe also times out.
- Candidate: `python3 tests/nei_leaf/run_tests.py` passes with ASan/UBSan, including cold completion, initialized stop, unrelated-sound preservation, pending-request removal, stale-request suppression, later replay, repeated Leaf use, interruption, missing clip, and insufficient magic.
- Resource/GPU execution, the outer audio mutex, and actual audible output are outside this fixture. Full application build and user runtime are untested for this candidate.
- Candidate is local and unpushed. PR #17 and integration/master remain unchanged.

Decisive runtime check: start a fresh game process, use the Leaf on the ground before using any MM sound-producing item, let the swing finish, and verify movement returns. Repeat the swing, then glide/cancel and verify the wind sound stops normally.
