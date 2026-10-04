# Held models POC1 evidence

Baseline: `3ba8d915c245b80ba48b082fc59ab43545a52513`.
Candidate branch: `feat/nei-held-models-poc1-20260927`.

## Implemented presentation paths

| Item | Gameplay presentation |
| --- | --- |
| Fire, Ice, Light Rods | Grip-centered meshes and approved tip energy; projectiles unchanged |
| Shovel | Shaft fitted to the existing two-hand frame |
| Ball and Chain | Centered spiked ball; existing moving physics chain |
| Deku Leaf | Approved leaf with existing hold, swing and glide scale states |
| Beetle | Mechanical body and separate animated wing blades |
| Mogma Mitts | One glove on each hand, aligned to its forearm |
| Gust Jar | Approved vessel plus separate direction/charge-colored wind band |
| Time Gate | Engraved gear in casting pose; portal unchanged |
| Switch Hook | Live first/third-person hand, docked assembly and detached actor tip |
| Whip | Equipped coil; wrist-mounted handle, flexible braid and separate head during use |
| Lantern | New seventeenth GI; same housing held, unlit or four distinct contained effects |

The feather and three spell abilities have no equipped physical mesh to replace.
Their approved GI/world/shop/overhead models and ability effects are retained.

## Checks

- Real-header GI and held render tests exercise complete-resource fallback,
  deferred normal/Alt paths, matrix balance, rod energy, paired mitts and jar
  feedback colors including clamping.
- Lantern tests cover 180 frames, chamber containment, unlit/invalid values,
  variant-specific motion, complete OPA/glass fallback, native scaling, exact
  hand-minus-seven flame origin, and unchanged player state.
- Tests compile the real whip and wrist helper, exercise equipped/lash/hit/
  latch/swing/retract/launched/inactive states, and check contiguous braid
  intervals, sag, socket continuity, partial-resource fallback and unchanged
  player/item state. Five geometry/export tests pass for the articulated parts.
- The actual player unity, common GI, shop and hook-actor C translation units
  pass syntax checks. New held/lantern C++ renderers compile independently.
- Serialized display lists, native matrices, vertex batches, textures, winding
  and transparency match all 17 GI and 20 held GLB checkpoints.
- The combined archive contains 502 entries; every normal/Alt pair is identical
  and matches the installed source resources.
- All sixteen pre-existing GI resource/checkpoint trees remain byte-identical
  to POC7. All 26 required feature baselines remain in the branch ancestry.
- Previews use the actual exported meshes and production effect samplers,
  native orb texels and conventional alpha blending. No added bloom.
- Independent review of rigid/lantern and articulated integration found no
  Critical or Important issues. The candidate is ready for visual review.

## Runtime boundary

These checks do not constitute a linked game build or a gameplay test.
Review child/adult grips, animation clearance,
first-person switch aim, launch/retraction/swap, whip lash/latch/swing/release,
beetle aim/flight, jar direction/charge, leaf hold/swing/glide, lantern carry/catch
and all four flame types, with normal and Alt assets. Existing gameplay state,
collision, projectiles, save/progression and lantern behavior were not changed
by this presentation checkpoint. The subsequent put-away/audio fixes are
recorded separately in `tests/nei_item_stow/README.md`.

Use `NEI_Held_Models_POC1.o2r` with code built from this candidate; the archive
alone cannot add the new held draw paths to an older executable. It replaces
the prior GI preview archive and includes its unchanged approved models.
