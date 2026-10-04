# Final NEI presentation POC1

Local candidate: `feat/nei-final-presentation-poc1-20260928`.
Preserved source baseline: `32036693c3c1d0ba6f44ee5d321c6aacf60def69`.
Its tree matches published PR17 head `cec63fce86b1f582f6e61ad6a98eca6c3cca784b`:
`43b3d54a2fbde5acd1cdd12164e4d0644953e5a0`.

The user authorized continuation through PR publication and the combined runtime-test
build on 2026-09-27. The ComboShip port follows runtime acceptance of this SoH pass
and carries the complete accumulated
GI, held, effects, stow and Ball-and-Chain audio changes.

## Requested scope

1. Fit Hylia's Grace, Zonai Permafrost and Demise Destruction above shop surfaces.
2. Reduce Time Gate and Switch Hook shop size without shrinking other contexts.
3. Modestly reduce the held shovel, especially for the child player.
4. Attach all three rods to the actual wrist through their held/use poses.
5. Remaster used Light/Ice effects; preserve the current Fire projectile surface.
6. Give charging sparks and the main charge presentation readable elements.
7. Remove the whip's brief equipped-coil flash while entering first person.
8. Present fitting/effect previews before publication; retain one combined build.
9. Add Spinner and Cane of Somaria GI and used models, with animation fit review.
10. Add the Minish Cap GI revision; its shrinking ability has no held mesh.
11. Add Roc's Cape GI from the supplied icon; map both feather draw routes to
    the approved feather while retaining their distinct gameplay identities.
12. Remaster the Time Gate's used warp-pad presentation.

All new GIs must fit shop shelves. Normal and Alt paths need matching replacement
resources and complete-pass fallback. Existing GI meshes, progression, item
pools, save data, combat/collision, put-away fixes, lantern variants, and optional
green Deku Leaf shimmer are protected except where explicitly listed above.

## Baseline evidence and reported defects

The baseline passed the cumulative local regression runner and linked Windows
and Linux builds. The user then reported shop clipping/oversizing, rod hand
misalignment, oversized child shovel, and the whip transition flash. These are
open defects, not accepted limitations. The separate formatting workflow also
reported style differences; its status must not be described as green.

The original icon reference is `Extra Items.o2r`. Its Roc's Cape is white fabric
with a gold neck clasp and blue feather tips; its Minish Cap is red with an
ornate gold band and tassel. The medallion effect reference is
`zzz_Medallion_Magic_POC1_HD.o2r`.

## Evidence boundary

Source and exported-geometry checks do not establish in-game visual acceptance.
Offline animation previews must identify their player archive, source animation,
pose composition and any missing native clips. A model turntable is not a
held-animation or clipping test. No gameplay capture or runtime acceptance has
been recorded for this candidate yet.

## Implemented candidate

- Shop transforms include translucent spell tips. Their lower bounds now clear
  the counter by .739 world units. Time Gate uses .60 of its previous shop size,
  Switch Hook .70; their world and overhead sizes are unchanged. Both feather
  callbacks use the approved mesh and the same shelf transform.
- The rods use captured left-wrist rotation and native palm sockets, with their
  authored shaft aligned to the native weapon axis. The right-hand Somaria
  socket uses the mirrored native fist. Both vanilla child and the supplied
  Young Din mesh were inspected. Shovel scale is .048 child / .054 adult.
- The whip changes to its handle immediately when first-person entry is active.
- Spinner and Somaria have separate GI and used exports. The Spinner deck was
  fitted against the actual native riding stance; the Somaria cast uses the
  shipped 60-frame animation and actual upper-body copy mask.
- Minish Cap has no held mesh dispatch. Its existing shrinking/warp behavior is
  unchanged. Roc's Cape and the cap use the supplied icon designs.
- Used Ice and Light effects and all three charge presentations use deterministic
  item-local meshes. Private converted materials live under
  `objects/nei_used_magic/`; the Fire projectile draw remains the baseline.
  Time Gate samples the existing portal growth and fade state.

## Reproduction

```
python3 tools/nei_gi/SOURCE/build_completion.py --held --install
python3 tools/nei_used_fx/build_textures.py /path/to/zzz_Medallion_Magic_POC1_HD.o2r
CPLUS_INCLUDE_PATH=/tmp/combo-json-fix/include bash scripts/diagnostics/run_cumulative_regressions.sh
```

For a shop matrix export, set `NEI_SHOP_PREVIEW_EXPORT=/path/to/shop.json` when
running `scripts/diagnostics/run_nei_gi_tests.py --held`, then feed that file to
`tools/nei_final_preview/render_shelves.py`. Animation commands and source
limitations are recorded in `ANIMATION_PREVIEWS.md` beside this document.
User-supplied native/player archives remain external review inputs; they are
not copied into this source tree.

## Final local verification, 2026-09-28

- The full cumulative regression command above completed with exit 0 after
  production formatting. It includes the new wrist/Spinner/Somaria/whip checks,
  used-effect checks, existing item stow/audio checks and preserved feature gates.
- Exact serialized resource/GLB parity passed for 21 GI models and 22 held
  components. Shop matrices and resource/source hashes were exported from the
  actual common, overhead and shop draw fixtures.
- The maximum 15-projectile workload, five wakes, full charge and held energy
  used 68,832 vertex bytes / 1,898 XLU commands for Ice and
  64,976 bytes / 1,957 commands for Light. This is a submission-budget check,
  not scene-specific frame-rate or total-arena proof.
- All 25 changed/new production C/C++ files passed clang-format 14's dry-run
  check. `git diff --check` passed. Compiler warnings inherited from the source
  headers remain; this is not a warning-free build claim.
- Actual source-animation frames were rendered with the final exported models.
  Somaria raised frames 12–48 had zero triangle-surface intersections against
  the inspected head/hair, forearms and torso/shoulders, excluding intended fist
  contact. Coplanar/enclosed-volume and runtime pose differences are outside
  that offline check.

The candidate has not received a new linked Windows/Linux build or an in-game
acceptance run. Preview acceptance, the combined publish/build, and the later
ComboShip port remain subsequent gates. No remote push was made for this pass.

## Put-away audio follow-up

The user supplied a rod equip/stow clip and requested the duplicate put-away
sound fix across all items in this same candidate. Shared transition-scoped
ownership now coordinates custom cleanup with native outgoing audio for all
14 custom emitters. Separate completion/attack cues and incoming equip audio
are preserved. See tests/nei_item_stow/README.md for the audited coverage.
The initial rod-only fix reproduced and resolved the reported duplicate; a
Mitts fixture then reproduced the broader problem. The final shared fixtures
pass. Runtime acceptance remains pending before the ComboShip port.

Publication follow-up: the final combined cumulative regression gate completed with exit 0
after the shared audio fix. The user directed this chat to finish the GI takeover,
including publication and a combined runtime-test build. Runtime acceptance is still pending.
