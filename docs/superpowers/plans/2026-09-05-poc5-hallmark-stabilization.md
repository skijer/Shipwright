# POC5 Hallmark Stabilization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Stabilize the tested POC4 static-story actors and POC5 Hyrule Field/global-rain enhancements without changing proven concurrent weather audio or thunder timing.

**Architecture:** Keep every enhancement behind its existing explicit `0x7Fxx` parameter or Audio Editor CVar. Add pure, unit-tested policy helpers for static placement, safe dialogue, night-BGM lifecycle, and rain volume transitions; runtime hooks only translate game state into those policies.

**Tech Stack:** C/C++, Shipwright actor/audio hooks, CMake/CTest, GitHub Actions.

---

### Task 1: Static actor isolation and safe dialogue

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.{c,h}`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Test: `soh/tests/static_story_actor_test.c`

1. Add failing assertions that ordinary parameters remain outside static mode and Child Ruto receives a terminal, non-script-driving dialogue ID.
2. Run `static_story_actor_test` and confirm the new Child Ruto assertion fails.
3. Make the static dialogue selector return a safe informational line for Child Ruto without invoking `En_Ru1` state transitions.
4. Verify the test passes.

### Task 2: Static placement transforms

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.{c,h}`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Test: `soh/tests/static_story_actor_test.c`

1. Add failing policy tests for neutral pitch/roll with preserved yaw and root locking on Saria's seated pose only.
2. Neutralize static actor pitch/roll before skeleton initialization.
3. Lock Saria seated root translation after animation updates; leave other animation roots untouched.
4. Run the static actor unit test.

### Task 3: Hyrule Field night-BGM lifecycle

**Files:**
- Modify: `soh/soh/Enhancements/audio/HyruleFieldNightMusic.{cpp,h}`
- Test: `soh/tests/hyrule_field_night_music_test.cpp`

1. Add failing state-machine coverage for two complete day-night-day cycles and scene exit.
2. Stop the owned sub-player track on dawn/exit and explicitly restore the scene's daytime sequence at dawn.
3. Verify repeated cycles never settle into silence.
4. Run the night-music unit test.

### Task 4: Persistent/intermittent global rain modes

**Files:**
- Modify: `soh/soh/Enhancements/audio/GlobalOutdoorRain.{cpp,h}`
- Test: `soh/tests/global_outdoor_rain_test.cpp`

1. Add failing tests for a deterministic dry/fade-in/sustain/fade-out lifecycle and clamped volume/density envelopes.
2. Add an Audio Editor mode dropdown with `Persistent` and `Intermittent Storms` choices.
3. In intermittent mode, cycle randomized dry and sustained intervals through approximately one-second fades; retain the configured rain-volume slider as peak audio volume.
4. Drive visual rain density from the same envelope while preserving existing thunder and concurrent-BGM behavior.
5. Reset ownership and scheduling safely on disable, indoor transition, play destruction, or mode change.
6. Run the global-rain unit test.

### Task 5: Rain color control and blue cutscene rain feasibility gate

**Files:**
- Inspect: `soh/src/overlays/actors/ovl_Demo_Kankyo/z_demo_kankyo.c`
- Inspect: environment rain renderer and Audio Editor registration

1. Identify whether the blue appearance is a reusable native texture/color/render-state selection.
2. Defer the exact `Demo_Kankyo` blue-rain style because it requires its separate particle geometry and motion system.
3. Add a native color picker with hex entry and reset for global outdoor rain droplets.
4. Gate the renderer bridge on global-rain ownership so story and proximity-weather rain retain vanilla color.

### Task 6: Verification and CI

1. Run all focused tests, formatting checks on changed files, and a local compile target where available.
2. Review the diff against the POC5 branch for scope containment.
3. Commit and push `codex/poc5-hallmark-fixes`.
4. Trigger/monitor GitHub CI and report exact artifacts or failures.
