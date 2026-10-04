# Rod runtime revision implementation plan

> **For agentic workers:** Use superpowers:executing-plans to implement in the current isolated worktree. The user has already authorized these revisions; preview approval remains the gate before push.

**Goal:** Restore accepted Light fixtures and correct the Fire/Ice motion and presentation regressions alongside the held Leaf fix.

**Architecture:** Keep gameplay state and three-shot trajectories authoritative. Isolate presentation identity from draw order; preserve earlier Light output and revise only rejected Fire/Ice presentation.

**Tech Stack:** C/C++, native frame interpolation, existing Python regression runners and OpenGL production-mesh previews.

**Spec:** `docs/poc/rod-runtime-revision-20260928.md`

## Global constraints

- Preview before push; no integration promotion or claim of runtime acceptance.
- Preserve Light projectile, accepted Fire projectile material, Ice wake, native timing, collisions and spread.
- Do not replace shared gameplay_keep or medallion flame textures.

## Review focus

- Earlier projectile sets disappearing must not change later sets' interpolation identity.
- Reused projectile slots must not interpolate from a previous launch.
- Zero velocity during hit fade must not rotate a long projectile into another heading.
- Light charging and release must select only their recovered geometry/materials.
- Full-speed previews must show complete flight/expiry and the 16-update release window.

### Task 1: Recover Light

Files: `NeiUsedMagicPolicy.h`, `NeiUsedMagicPresentation.cpp`, `tests/nei_used_fx/preserve_approved_test.py`, production presentation fixture.

- [ ] Extend baseline equality to Light small/large releases at native radii and multiple camera bases. Observe failure on current continuous wall.
- [ ] Restore c77 Light release and prevent new release surfaces from drawing for Light. Preserve charge source/material output while investigating runtime discrepancy.
- [ ] Verify with baseline geometry and complete graphics dispatch checks; render recovered charge/release for review.

### Task 2: Diagnose and correct projectile motion

Files: Fire/Ice object drawers and a focused production interpolation fixture under `tests/nei_used_fx`.

- [ ] Reproduce draw-order misassociation with the real frame interpolator, comparing expected interpolated positions after an earlier set disappears.
- [ ] Reproduce impact heading discontinuity against the existing projectile orientation.
- [ ] Correct demonstrated presentation faults without changing gameplay position, velocity, collision or lifetime. Cover slot reuse and stopped fade.
- [ ] Run motion and existing object-dispatch/appearance tests.

### Task 3: Revise rejected Fire/Ice presentation and deliver preview

Files: elemental release/Fire charge policy and private assets if needed; `tools/nei_feedback_preview`.

- [ ] Use the established large elemental-release direction; keep transparent gaps and a readable moving front at native speed.
- [ ] Render production charge/release fixtures and complete projectile flight, label offline evidence accurately.
- [ ] Run effect/material/budget, object dispatch and held Leaf regression suites. Review final diff and record limitations.
- [ ] Present previews for user review. Keep changes local until preview acceptance.
