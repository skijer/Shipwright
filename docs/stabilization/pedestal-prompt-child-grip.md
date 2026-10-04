# Custom pedestal prompt and child sword grip

Repository: `marsh6487/Shipwright` (SoH only).
Baseline: cumulative integration `74edaf1019dfdd0ad1cb40dbece407995ac7a174`.
Candidate branch: `fix/pedestal-prompt-child-grip-20260922`.

The user reports that the A-button prompt requires awkward repositioning around
the stump, and that child Link's left hand and held sword look crooked. The
existing skip/fade, selected weapon, local age swap and return sequence must be
preserved. This follow-up changes neither scene archives nor ComboShip.

## Prompt diagnosis

The current Lost Woods authoring archive, `!Lost Woods Scene - FINAL.o2r`, has
SHA-256 `988c7c0a49e43acdc941894de9ba391f32831a302205e6699251ea80d83979d6`.
Its normal and Alt collision resources are byte-identical, with SHA-256
`0466ee04dc05f19dab79ee9d9bbe42505d6cba99a5675af2840b6f0ba1da22f9`.
The pedestal is at `(-736, -63, -2395)`, yaw `15101`, params `0x4C57`.

The eleven cap triangles are flat at Y=-56 and fit within 35.17 horizontal
units of the anchor. Thus the existing 60-unit offer range already covers the
cap, and loosening the floor-height test is not supported by this geometry.
The neighboring bark descends toward Y=-76 and the ground is Y=-96.

The custom actor still registers the original sword's immovable collision
cylinder: radius 10, height 70, starting at the anchor. Together with Link's
12-unit cylinder, it excludes player centers within 22 units of the sword.
That occupies the cap's center and leaves a narrow approach rim. Saria's
separate collider further restricts one side. The correction omits
this sword-cylinder registration for the exact custom pedestal only. The
actual stump collision, grounded/top-surface checks and stock Temple of Time
sword remain in use.

## Child grip diagnosis

The custom/PAK path composes the selected Master Sword with the current age's
closed fist. The existing correction then rotates the entire left-hand limb
by a half turn, including the fist and wrist. Native child fist vertices extend
along positive local Y; this rotation puts that geometry on negative local Y.

The native child ceremonial sword also has a distinct coordinate basis from
ordinary held Master Sword geometry. Corresponding native blade vertices and
triangle topology permit a measured rigid transform. Reversing the whole limb
does not reproduce that transform. The correction keeps the animated hand
unchanged and applies the native ceremonial transform only to the selected
sword. Selection precedence, age-specific hand resources and adult rendering
remain intact. The sword matrix is allocated from the current graphics frame,
pushed after the fist is drawn, and popped after the sword is drawn.

The two native sword meshes have 136 corresponding vertices and 66 identical
triangles. Rigid registration has 0.388 model-unit RMS component error; the
production matrix differs from the integer native coordinates by at most 1.038
units per component. The former whole-limb half turn misses by up to 1,306 units.
The mapped hilt center lies inside both the ordinary child fist and native
ceremonial hand bounds. Bounds establish gross placement, not finger contact.

The native vertex resources used for the reproducible measurement are:

| Resource | SHA-256 |
| --- | --- |
| `objects/object_link_boy/object_link_boyVtx_010EE8` | `eb0e8fda39539e5859c8f9899a7f4014f00126c07dc98b3a0d1a4f8936ca344f` |
| `objects/object_link_child/object_link_childVtx_00D4E0` | `8ddb6a653f9e5a03b73478bcfb33168e01503dd07189be81abe00f7822d5ae24` |

## Evidence and acceptance

The diagnosis uses the actual authored collision and native model resources,
plus the production actor and rendering paths. Older pedestal clips are no
longer available, so this record does not claim fresh visual runtime proof.

The collision regression failed before the fix at the custom sword's center
shove; the grip regression failed before the fix at the changed joint rotation.
After the fix, these commands passed on the combined candidate:

```sh
python3 -B scripts/diagnostics/run_time_pedestal_tests.py
python3 -B scripts/diagnostics/run_pedestal_sword_selection_tests.py
python3 -B scripts/diagnostics/check_time_pedestal_syntax.py
python3 -B scripts/diagnostics/derive_pedestal_child_grip.py --archive /path/to/native/oot.o2r
```

The pedestal runner requires the nlohmann JSON headers. The geometry derivation
is optional and needs a user-supplied native archive plus the existing preview
reader dependencies; CI uses literal native landmarks and does not need an
archive. The focused CI job runs the first three commands.

The passing checks cover custom-versus-stock cylinder registration,
stump/ground/airborne interaction, both ages, NPC offer priority, unchanged hand
joint rotation, sword-only transform scope and native geometry correspondence.
The grip regression uses the production fixed-point matrix conversion and a
rotated, translated parent limb to check multiplication order and stack cleanup.
The existing independent departure/arrival skips, camera, equipment and
progression checks also pass. Full application builds and runtime acceptance
remain separate gates.

In game, walk across the cap and approach from each side, confirming that the
prompt remains available on top and absent on the ground. Test child withdrawal
with the selected sword and Alt Assets, inspecting the wrist and blade through
the handoff and hold. Repeat the adult insertion and both independently skipped
phases. These are candidate runtime checks, not claimed acceptance.
