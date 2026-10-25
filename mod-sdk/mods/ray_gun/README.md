# Ray Gun

`drop custom_item "nei.ray_gun"`

Equip on C or D-pad. First use draws the gun, subsequent presses fire.
C-Up enters/leaves aiming after drawing. Only the right shoulder, forearm and
hand blend into the drawing/holding pose. Native locomotion is
preserved for the torso, head, left arm and legs using the deferred animation
queue, so running continues across the rest of the body. Firing stops Link's
movement for the shooting animation and uses the complete retargeted stance,
including legs. The native torso yaw/pitch render fields point the upper body
and ray at Link's target, or forward without a target (manual aim retains its
chosen direction). Movement/action input is blocked during the shot; pause
and held Z-target remain available. Damage, dialogue and transitions cancel
the lock, and ordinary movement resumes after the animation.
No ammo or magic is charged in this initial implementation.

The ray sweeps the full movement segment and stops at walls. Enemies receive
lethal native collision damage and an upward/backward impulse, with removal
after 12 updates if their own death logic has not removed them first. Other
visible actors hit by the ray are killed directly, as requested. Dynamic
background actors can also be hit through their actual wall collision.
This is deliberately lethal rather than an ordinary damage weapon.

## Assets and animation

User-supplied `Downloads/Drawing Gun.fbx` and `Downloads/Shooting.fbx` contain
Mixamo rigs and character meshes, without a weapon model. This package has
its own low-poly gun, ray model, icon and item name.
The gun explicitly establishes two-cycle lighting times primitive color,
geometry modes and grayscale state, matching the corrected Switchhook
material setup. The beam uses an emissive primitive-color material.

`retarget_fbx.py.txt` runs in Blender background, bakes model-space motion deltas
onto native child/adult Link rigs, reconstructs local joint rotations, and
bakes a right-hand socket. Draw: 61 frames; shoot: 36 frames, both from 30fps
source. Hold is the last drawing frame; shooting includes the complete
source skeleton while drawing/holding keep the native lower-body reference.
Runtime uses native Link animation
interpolation and no retargeting calculations.

`build_assets.py.txt` regenerates XML geometry and pre-encoded textures, requiring
Python/Pillow only for regeneration. Packaging does not require Pillow.
Compile/install from Shipwright-unbound with
`./build-all.local.ps1 -Only Mods -Reconfigure`.

Production-function collision/death tests are in `test_new_items.py`; asset
validation is in `verify_partner_items.py`, both under NEI-Unbound-pruebas.
Visual grip, pose transitions and gameplay impacts await in-game feedback.
