# Great Fairy and Facial Stabilization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add three safe static Great Fairy poses and stabilize the remaining Sheik, Impa, and Adult Ruto facial contracts.

**Architecture:** Extend the existing table-driven `En_Viewer` catalogue with one `OBJECT_DY_OBJ` skeleton family and adapter. Keep selection and tracking policy in pure helpers, while model-specific segment binding and limb rotation stay in `z_en_viewer.c`; do not import the native Great Fairy state machine.

**Tech Stack:** C/C++, Ship of Harkinian actor/object APIs, focused standalone C tests, Git.

**Spec:** `docs/superpowers/specs/2026-09-09-great-fairy-facial-stabilization-design.md`

## Global Constraints

- Start from `codex/poc12-ruto-final-stabilization`.
- Do not change weather code or add Bombchu Lady, Ganon, or Phantom Ganon.
- Great Fairy parameters are `0x7E04`, `0x7E14`, and `0x7E24`.
- Never invoke Great Fairy reward, healing, particle, growth, disappearance, or cutscene logic.
- Preserve alternate-resource parity by using the native symbolic `OBJECT_DY_OBJ` skeleton, animation, display-list, eye, and mouth references; never embed vanilla geometry.
- Preserve Zelda and Adult Ruto POC12 behavior.

---

### Task 1: Great Fairy catalogue contract

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Test: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: Great Fairy type, adapter, skeleton, animation IDs, parameter decoding, pose descriptors, text ID `0x00DB`, and tracking policy.

- [x] Add failing assertions for `0x7E04`, all three poses, `OBJECT_DY_OBJ`, safe text, and sitting/full versus laying/flourish/head-only tracking.
- [x] Run the focused actor test and confirm failure because Great Fairy is absent.
- [x] Add the minimal enums, definition, pose row, expanded-ID mapping, animation/object selection, and tracking policy.
- [x] Run the focused actor test and confirm it passes.

### Task 2: Great Fairy runtime adapter

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h`
- Test: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Consumes: Great Fairy catalogue descriptors from Task 1.
- Produces: skeleton initialization, animation mapping, eye/mouth segments, limb 8 torso tracking, limb 15 head tracking, and bounded head-only rotations.

- [x] Add failing pure assertions for Great Fairy tracking-limb classification and conservative head-only rotation bounds.
- [x] Run the focused actor test and confirm the new helper assertions fail.
- [x] Include `object_dy_obj`, map the three animations, initialize `gGreatFairySkel`, and add the Great Fairy draw adapter using segments `0x08`, `0x09`, and `0x0A`.
- [x] Apply full sitting tracking and clamped head-only tracking for laying/flourish; leave body animation untouched.
- [x] Route the Great Fairy through `EnViewerStatic_Draw` and verify the actor test passes.

### Task 3: Facial-contract audit and fixes

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`
- Test: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: explicit eye-state and facial-segment decisions reusable by OPA and XLU draw paths.

- [x] Add failing assertions proving Sheik harp resolves to closed eyes, Impa resolves its native face-segment contract, and Adult Ruto returns identical eye/mouth resources for OPA and XLU.
- [x] Run the focused test and confirm failures identify missing selector contracts.
- [x] Centralize the model-specific facial selections without changing valid animation behavior.
- [x] Keep Sheik's TP replacement geometry when it ignores native segmented eyes; do not substitute a vanilla head.
- [x] Apply an Impa-specific face setup only if comparison with `Demo_Im` demonstrates a missing runtime contract.
- [x] Reuse one Adult Ruto facial binding helper from both draw streams.
- [x] Run the actor and Ruto water tests and confirm they pass.

### Task 4: Verification and publication

**Files:**
- Verify all files changed by Tasks 1–3.

**Interfaces:**
- Produces: isolated actor POC branch and CI build.

- [x] Run all focused actor, Ruto-water, and face-flex tests with strict warnings.
- [x] Run `git diff --check` and confirm the diff contains no weather files.
- [x] Review the branch against every acceptance item in the approved spec.
- [ ] Commit the implementation, publish without force-pushing, verify the remote tree, and report the CI link and runtime parameter sheet.
