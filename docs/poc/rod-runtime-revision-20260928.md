# Rod runtime revision, 28 September 2026

User feedback supersedes review03's offline acceptance: Fire/Ice projectiles appear to snap backward; Fire charge and Fire/Ice releases are rejected; both Light charge and release must match the previously accepted fixtures. Keep Light's projectile, Fire's accepted projectile appearance, Ice's visible following trail, and the native three-shot spread. Hold the cold MM-stop Leaf fix for the same eventual push. Show production previews before pushing.

Starting point: published GI PR17 `68047d9a4d16a67f0ca47d2b48f9fec93c8cb4f1`, plus local Leaf fix `d1896c85c`. Earlier GI fixture: `c77c18587a976f6d6cb5c8f91f27593286469218`.

## Evidence and open questions

- The revision replaced Light's separated release rays/rings with a continuous 192-unit textured cylinder. Restore the earlier Light release, excluding it from the Fire/Ice release treatment.
- Light charge samplers, material, object drawer, gameplay call and shared renderer have no source differences from c77. A sampler-only test cannot settle the user's runtime observation. Compare complete draw output and show the recovered fixture; do not claim the charge regression resolved solely from source equality.
- Projectile drawings have no identity scopes for native frame interpolation. Test disappearing earlier volleys and reused slots with the production interpolator.
- Collision zeros the velocity passed to elongated Fire/Ice projectile meshes. Test stationary fade heading separately from position interpolation.
- Fire/Ice release should retain the powerful native expansion/timing and original elemental art direction. Avoid another opaque repeated curtain and do not use the medallion/Henriko fire material.

Offline geometry, source and graphics tests are implementation evidence. Exact user configuration and in-game acceptance remain separate gates.

## Recovered review04 checkpoint — 29 September 2026 UTC

The previous session completed both nine-second, 20-fps previews. Recovery found no active rod render process. The source and packed materials predated those exports. A read-only review found that the late restored Light release was clipped by the preview viewport; only the release-camera framing was widened, and the charge/release video was regenerated. Projected bounds for every release vertex over all 180 preview frames fit the new viewport (including Light at radius 500). No effect geometry or material changed during recovery.

Implemented candidate:

- Local Fire/Ice volleys use set address plus launch epoch, with separate trail/head scopes. Production frame-interpolation checks cover removal of an earlier volley, slot reuse, ordinary travel, and impact heading.
- Zero-velocity local projectile fade reconstructs the launch heading from the preserved native yaw/pitch/spread. Gameplay velocity, lifetime, collision, and three-shot spread remain authoritative.
- Light's earlier separated release rays/rings are restored. Production graphics output for Light charge/focus/release matches c77 across 180 phases, three charge values, both release sizes, and three radii. Light projectile and Ice/Light charge policy preservation checks pass.
- Fire charge is a focused textured flame over a hot core. Fire/Ice releases use the new private crest artwork plus a low outward sweep. Accepted Fire projectile and Ice wake resources are preserved.
- The parent commit retains the cold MM-stop Deku Leaf fix.

Fresh recovery validation:

- `tests/nei_used_fx/run_tests.py`: exit 0. Includes production interpolation, baseline equality, material hashes, particle-input preservation, dispatch, C ABI, policy, and maximum-load budgets. Maxima: Fire 39,072 vertex bytes / 1,728 XLU commands; Ice 70,272 / 2,356; Light 69,712 / 2,124.
- `tests/nei_leaf/run_tests.py`: exit 0 with ASan/UBSan. Cold stop, initialized stop, first ground activation/completion, repeat/interruption, missing clip, and insufficient magic cases pass.
- `git diff --check`: exit 0.
- Both videos decode as 1,500 × 1,000 H.264, 180 frames, 20 fps, nine seconds. Charge/release uses the native 16-update release window. Stills and sampled video frames were inspected; these remain offline production-mesh previews, not captured gameplay.
- Missing checkout dependencies were restored from existing local copies: libultraship at the unchanged pinned `c57da1b4afa775b24b58b2adf93d63d3b561bb65`; the standalone effect runner additionally used the existing nlohmann include directory through `CPLUS_INCLUDE_PATH`.

Review scope and limits: the independent source review found no additional blocker in the local fix. Remote multiplayer paths are unchanged and do not gain launch identities or impact-heading recovery. The full-dispatch Light comparison uses one camera basis, and the production impact comparison exercises the center shot at zero yaw/pitch; broader runtime configurations remain unproven. Artwork preference and the user's observed Light charge mismatch require visual/runtime review. No push, game build, runtime acceptance, or master promotion occurred.

Final previews: `/workspace/scratch/fa90949b48fc/NEI_Rod_Review04/rod_charge_release_04.mp4`, `rod_charge_release_04.png`, and `rod_motion_04.mp4`. The earlier completed files remain untouched under `/workspace/scratch/a970576ebb44/previews/rod_review04/`.

## Final user-directed charge revision

The user accepted the release effects and directed that Fire charge resemble Light with readable fire substituted, then proceed. This supersedes the single tip flame and the previous preview-before-push hold for this final specified change.

Fire now uses the accepted Light charge's exact body placement, rings, rays, sparks, growth, UVs, colors, and alpha, binding only the private `fire_release_crest` texture in place of `light_rays`. The existing artwork is reused without modifying any packed texture. The tip-only plume is removed. Light itself and all release, projectile, and trail samplers remain unchanged.

A production-renderer regression compares Fire against the c77 Light charge/focus graphics, permitting only that resource substitution. It failed on the old Fire vertex count before implementation and passes afterward over 180 phases and three charge levels. The effect suite passes; Fire's maximum-load fixture now measures 49,728 vertex bytes / 2,038 XLU commands, within the existing 90,000 / 2,400 limits. The source-driven final still was inspected to confirm the flame material is readable. No additional artwork generation or user preview gate is needed for the explicitly requested substitution.

Final combined gate: `scripts/diagnostics/run_cumulative_regressions.sh` exited 0 after the exact clang-format14 pass. Formatting exposed a whitespace-dependent matcher in the source-preservation test; the matcher now accepts the formatter's line break without weakening its token comparison. The full native graphics equality and projectile interpolation checks remain green. Independent review found no blocking issue in the final charge change. Existing legacy C pointer/qualifier warnings remain; no full-application or runtime success is inferred from the source gate.
