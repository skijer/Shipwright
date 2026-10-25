# Malon Companion

Enable **Mods → Partners → Malon Companion**. Malon's native OoT model follows
Link's age. The original ranch/town Malon actors and quests are untouched.

- Follows Link, jumps short gaps with a safe landing, and recalls to grounded
  Link when too far away. Persists across rooms; respawns after scene changes.
- Shoots nearby hostile actors with native arrows, without charging Link ammo.
  Prioritizes Link's target and checks line of sight. Ordinary native enemy
  collision and damage decide whether a shot is effective.
- Rides behind Link on Epona and can shoot while mounted.
- Approach her and use A/Grab to carry her; the native Throw action launches
  her. She lands, looks around, and follows again.
- No Lon Lon Ranch music/singing callback is used by the companion.

## Command Melody

Requires `command_melody`, which uses the existing song from `mm_collectables`:

`drop custom_item "nei.mm_song_command_melody"`

Learn it, enable Malon, then play **A, A, C-Up, C-Right, C-Left, C-Down** on the
ocarina. Target a companion to select it; otherwise the nearest available
registered companion is chosen. Link remains physically where he stood.

- Stick: move relative to the camera.
- A: jump.
- B: bow shot; uses a visible nearby hostile target, or fires forward.
- C-Left / C-Right: rotate camera.
- C-Up: return to Link.

Link taking damage, losing the ground under him, transitions, losing the
companion or exceeding 650 units ends control. Pause is available. Control
does not cross a scene boundary or load rooms through a substitute player.

## Animation provenance and rebuilding

Romani Partner by **Luv4os**, CC0:
<https://github.com/Luv4os/mm_recomp_mod_romani_partner>
Pinned source: `cced2388ea8c8a9082346ce70e227d805162a8ee`.
Original source snapshots and license are in `reference/` (not compiled).

`retarget.py.txt --archive <oot.o2r>` bakes ten clips for each native Malon rig,
using anatomical mapping, reference-pose correction in model space, target
hierarchy reconstruction, and proportional vertical root motion. Horizontal
travel is actor-driven. Bow hand orientation is baked as well. Animation
transitions and subframes use the native interpolation modes. No retargeting
calculations run during play.

`build_bow.py.txt` converts the original bow topology and textures into native XML
and pre-encoded textures. The build does not require Pillow.

Rebuilding assets uses Python with NumPy and Pillow. `verify_assets.py.txt` checks
every baked frame against the mapped source motion and writes a skeleton
preview. Compile/install with the repository's `build-all.local.ps1 -Only Mods
-Reconfigure` from the game checkout.

Automated logic tests live in `NEI-Unbound-pruebas/test_malon_command.py`.
Actual pose fit, skirt deformation, bow grip, carrying and riding still need
in-game feedback; they are not marked passed in the checklist.
