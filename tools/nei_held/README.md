# Held models POC1

The current final-pass candidate has 22 held components and 21 GI models. The
Spinner and Somaria additions use the separate completion builder:

```
python tools/nei_gi/SOURCE/build_completion.py --held --install
```

This rebuilds only the four final GI additions and their two used components.
See `tools/nei_final_preview/VERIFICATION.md` for the current branch, fitting
checks, source-animation previews and review boundary. The original pass below
remains the preserved baseline.

Baseline: `3ba8d915c` (GI Ice POC7). Work branch: `feat/nei-held-models-poc1-20260927`.

This candidate carries the approved GI designs into actual equipped and active
item rendering. It adds a seventeenth GI, the lantern, with four distinct held
fire presentations and an unlit state. The prior GI meshes, shop clearance,
pickup/overhead routing, green optional Deku Leaf shimmer, spell energy and
POC7 ice cleanup remain the visual baseline.

## Boundaries

Only presentation transforms, resources and draw dispatch change. Item state
machines, physics positions, aiming, projectiles, colliders, damage, saves,
progression, lantern catching and passive effects remain unchanged. The switch
hook uses the live hookshot hand/actor path; its obsolete custom draw functions
are not the integration point. The whip, beetle and ball have separate moving
parts. The gust jar retains its direction/charge color feedback.

Resources use `objects/nei_held_redesign/`. All required parts must exist before
an item suppresses its old model, with byte-identical base/Alt archive copies.
GI and held rendering share effect geometry/texture generation, without adding
GI spin or optional pickup shimmer to gameplay equipment.

The feather and three spell items have no physical held mesh: their abilities
remain unchanged, and their upgraded shop/world/overhead presentations continue
through the GI renderer.

## Production and evidence

`python tools/nei_held/build.py` exports the rigid components from approved
authoring sources. Checkpoints contain the actual serialized triangles, explicit
native conversion matrices, grip/tip markers and transform notes. Articulated
components are rebuilt with `python tools/nei_held/SOURCE/articulated.py --install`.
Lantern authoring lives beside the GI sources. Generated `RESOURCES/` directories
are staging output; the installed resources and exact GLB checkpoints are kept.

Run the focused real-header harness with:

```
CPLUS_INCLUDE_PATH=/tmp/combo-json-fix/include python scripts/diagnostics/run_nei_gi_tests.py --held
CPLUS_INCLUDE_PATH=/tmp/combo-json-fix/include python tests/nei_held/run_articulated_tests.py
python tools/nei_held/verify_assets.py
python tools/nei_held/package_archive.py /path/to/NEI_Held_Models_POC1.o2r
```

Offline geometry/effect previews are review aids, not gameplay captures.
Child/adult hand fit, first person, Alt assets, full animations and runtime
performance require a playable build. Do not call those checks passed from
offline previews or compilation. User visual review precedes publication.
