# Portable MM fountain animation implementation

Implemented the approved animation-only binding on existing flattened water materials. Six versioned profiles preserve MM lower A, lower B and central cadence at logical 32/64 dimensions; 32 adapts the native displacement to preserve normalized texture phase. Gameplay frames drive the native Gfx_TwoTexScrollEx generator. The first layer stays stationary. Existing Lake, Pool and light profiles remain available when explicit metadata is absent.

The factory uses independent permanent 12-command buffers. Fountain insertion validates both authored render descriptors, sizes, wrap masks, command boundaries and primitive placement, while preserving nonzero authored tile shifts. Explicit invalid metadata and conflicting duplicate declarations fail closed, including A/B/A duplicates. Direct singleton donor chains resolve to 64 profiles, but raw segmented donor lists remain unsupported for insertion.

`scripts/bind_mm_fountain_animation.py INPUT OUTPUT [--dry-run]` recognizes complete finite fountain families from material state, primitive order, original stone texture labels, ordered UVs and common group geometry. It accepts translation, upright yaw and positive uniform scale with a one-world-unit integer export residual ceiling. It rejects incomplete/ambiguous groups, inconsistent geometry, bad blob ranges/hash references and conflicting declarations. Resource and scene spelling, recipe IDs, current texture pixels and world height are not recognition predicates. Every stored component must resolve; no nearest-candidate fallback. Output is an exclusive new copy that changes only `prelude/project/edits.json`.

## Verification

- Production profile and bridge CPU probes pass, including the verbatim native scroll generator for all nine active profiles, overflow/wrap boundary frames, independent stable buffers, frame reset, transient allocator reuse, disable/re-enable, metadata negatives and permanent duplicate conflicts.
- Strict tile fixtures use actual flat/raised R5 words and preserved shifts, plus 64-mask/extent variants. Invalid dimensions/masks/clamp/mirror/duplicate setup/control flow/material mutation are rejected.
- Real R5 export probe: exactly six materials resolve to lower A32 or central32 and insert before primitive index31.
- Python association tests pass for 75 yaw/scale/stone6-family combinations, fractional translations, independent scenes and malformed/nonuniform/tilted/reflected/detached/duplicate geometry negatives.
- Real renamed archive test passes after renaming resource/scene names and rebuilding actual hash references, with a common yaw/scale/translation applied to stored vertices. Current texture pixel changes preserve recognition. Truncated resources, bad blob ranges and explicit conflicts fail closed.
- Real copy test passes: six annotation changes, all other metadata rows unchanged, every non-edits ZIP payload byte-equal, input SHA unchanged, idempotent analysis and existing-output refusal.

Commands (from repository root with the configured dependency environment):

```sh
source .superpowers/sdd/mm-recovery/build-env.sh
python3 scripts/diagnostics/run_native_material_probe.py --json-include ../linux-sysroot/usr/include --spdlog-include ../linux-sysroot/usr/include --bind-mm-fountain ../donor-inputs/MarketSuite_RuntimeMaster_R5.o2r
python3 scripts/diagnostics/test_mm_fountain_binding.py ../donor-inputs/MarketSuite_RuntimeMaster_R5.o2r
```

## Supplied archive result and limits

Night resource70/78 and day100 receive lower A32; night77/85 and day107 receive central32. Eighteen stone rows are unchanged. Four complete stored groups are recognized, but the archived day display lists load/draw only the first component of their doubled vertex arrays. The binder preserves those references; it does not imply four visible fountains or repair geometry.

The derived `MarketSuite_RuntimeMaster_R5_MM_Fountain.o2r` is a separate deliverable, not a source asset committed to GitHub. CPU tests establish metadata, command cadence and byte preservation. In-game appearance, coexistence, visible copy coverage and scene changes still require observing the normal candidate executable. Prelude may discard unknown metadata when re-exporting; rerun the binder on a fresh supported export in that case.
