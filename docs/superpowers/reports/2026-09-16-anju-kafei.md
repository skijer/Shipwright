# Seated Anju and Kafei draw dispatch

Baseline: `e4e71eeea6e173a35405aa708507d5afbdea4089`.

## Changes

- Kafei's resource loading and LOD renderer were already present, but
  `EnViewerStatic_Draw` omitted his type. Add the missing case. The regression now
  enters `EnViewer_Draw`, then the real dispatcher, instead of bypassing both by
  calling the MM drawing helper. Both Kafei poses emitted no LOD draw before the
  change and pass afterward.
- Append Anju as identity 23, `En_Viewer` params `0x7E0D`. Use the existing typed
  normal-flex loader: `gAnju1Skel` (20 limbs, 19 matrices) and
  `gAnju2UmbrellaCryAnim` (43 frames, 21 joint entries), looping at normal speed.
- Retain the native sad eyes and closed mouth. Draw `gAnju2UmbrellaDL` in the
  right-hand post-limb callback (limb 8), matching MM's `EnAn_DrawAccessory` and
  `EnAn_PostLimbDraw`. The strict graph loader owns the umbrella vertices and
  texture aliases across cache eviction; restore texture bindings before drawing.
- Preserve authored world XYZ and yaw, with zero shape offset. The immovable
  collider does not introduce gravity, floor snapping or collision displacement
  into this static update path. Preserve the clip's seated root motion inside the
  skeleton. No schedule, quest, tracking or audio actions are added.
- Add the requested dialogue through catalogue text `0x8F26` and the existing
  `OnOpenText` hook: "...Kafei... I promised I'd wait for you." This is custom
  scene-dressing text, not a quote claimed from MM. Talk range is 90; talking
  and closing the message leave the seated animation and placement unchanged.

Native behavior reference:
<https://github.com/zeldaret/mm/blob/main/src/overlays/actors/ovl_En_An/z_en_an.c>.

## Verification

- Before fixing Kafei, the public-draw fixture failed its expected LOD draw count.
- Before registering Anju, the placement fixture failed decoding `0x7E0D`.
- Production viewer, registry and MM presentation objects compile with the local
  configured build. All seven static-story CTest cases pass.
- `mm_ordinary_verify.py`: actual archive loads for all 11 normal presentations
  with Alt off/on, retained skeleton children after cache eviction, Kafei's two
  native player-format clips and 12 facial textures, and prior Skull Kid ownership
  tests pass. Viewer coverage includes Lulu, Shop Gal and Great Fairy blink paths.
- Anju's viewer test checks the actual outer draw dispatch, right-hand-only
  umbrella matrix/list commands, sad face segments, repeated destroy/recreate,
  and rejection before skeleton initialization if the umbrella is unavailable.
  Across 86 updates per instance, even with floor height above the actor, gravity,
  velocity and collision displacement populated, world XYZ/yaw and shape offset
  remain unchanged. The skeletal root remains animated.
- `mm_skull_kid_verify.py`: Anju's actual umbrella graph has three owned vertex
  loads and three texture commands. Geometry and texture bytes survive four
  cache/Alt cycles. Existing Skull Kid strict graph, corrupt-pack fallback and
  eight real-pack cache/Alt cycles also pass.
- Static actor draw-distance diagnostics pass, including Anju.
- Dialogue lookup and English fallback pass for the new ID; the production talk
  fixture verifies offering, accepting and closing Anju's message without changing
  the pose, animation pointer, placed position or yaw.

## Preview and runtime acceptance

`scripts/diagnostics/render_anju_preview.py MM_ARCHIVE OUTPUT_DIR --animate`
renders the original geometry, textures, matrices and all 43 animation frames at
20 fps. Dependencies: Python, NumPy, SciPy, Pillow. The preview shows two camera
angles and marks actor Y. It does not render the game or its scene lighting.
`--standing` previews her native 32-frame umbrella idle as an additional candidate.
Only the seated crying entry is wired into the catalogue in this draft. The archive
also has a 24-frame umbrella walk; scene-path movement is not implemented here.

The actor origin is near the feet, below the seat. Place the seated body against
the chosen ledge, chair or log; do not treat the furniture's top as the actor's
origin. Native root Y ranges from 2526 to 2548 model units, so this movement stays
inside the skeleton rather than changing the placed actor Y.

Pending user preview approval and an in-game test of both Kafei poses and Anju on
collision-enabled furniture, including scene exit/reentry. No Windows runtime
rendering is claimed by the local checks.
