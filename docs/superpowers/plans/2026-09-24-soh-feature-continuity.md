# SoH feature continuity recovery

> **For agentic workers:** Use superpowers:executing-plans for this bounded integration, followed by one independent whole-branch review.

**Goal:** Recover omitted published Midna and Zora shield implementations without dropping the current chest/RPG integration or any earlier cumulative feature.

**Architecture:** Start at remote integration `0574f79cb3838fdf79a8466393bc73eda5ef2aa9`. Merge the Midna donor `822f46b15dcae98b0a0564ec469a061f5106d4c4` and Zora shield donor `5d0b8384d4885b4e1f54c041afcc432542848257`, preserving their histories. Gate distributed builds on a versioned feature inventory and focused production-code regressions.

**Tech Stack:** Git, C/C++, Python, GitHub Actions.

**Spec:** `docs/WORKING_INTEGRATION.md` and cor's instruction that implemented features travel together.

## Global constraints

- Keep the current chest/RPG tip as an ancestor; never reset the integration branch to a feature branch.
- Preserve accepted asset archives byte-for-byte; no mod repack is needed.
- Keep Epona's ongoing experimental work isolated.
- Existing runtime defects and unverified configurations stay explicitly open.
- Audit old branches semantically when their changes were copied or superseded; do not merge obsolete experiments wholesale.

## Review focus

- Native and missing-pack fairy fallback retain their lights, audio and visibility.
- Midna uses the POC3 pose/blink resources and pauses yawns while recalled or moving.
- Zora shield retains scale/colors and the human Zora-tunic fallback while restoring ground/swim placement.
- No chest/RPG, weather, actor, pedestal, cache, equipment or cosmetic source is lost.
- The build gate rejects a missing required ancestor and cannot distribute artifacts after regression checks fail.

### Task 1: Audit and recover the published implementations

- [ ] Audit all 60 published branch heads and resolve divergent implementations against current source.
- [ ] Reproduce the missing halo-suppression and shield-anchor behavior against the original integration.
- [ ] Merge the original Midna commits and Zora shield implementation, retaining exact donor source where no conflict exists.
- [ ] Run Midna draw/audio, shield/custom-color, chest/stat, pedestal, weather/audio/actor/material and equipment checks that cover the audited boundaries.
- [ ] Record confirmed omissions, retained implementations and runtime limitations in `docs/stabilization/2026-09-24-feature-continuity.md`.

### Task 2: Protect cumulative publication

- [ ] Add `docs/required-feature-baselines.json` with full commit IDs for the cumulative feature milestones and recovered donors.
- [ ] Add an ancestry checker and exercise it on temporary real repositories: included feature, missing feature, missing object, invalid ref and malformed inventory.
- [ ] Add a regression job to `generate-builds.yml`; require it before generating distributable game archives and executables.
- [ ] Verify the guard rejects the original integration and passes the merged candidate.
- [ ] Obtain an independent review, confirm remote head is still preserved, publish without force, and verify exact tree/parents and CI status.

Execution is already authorized by the user's standing cumulative-integration instruction and prior authorization to push this working branch. Implementation and build checks do not imply in-game acceptance.
