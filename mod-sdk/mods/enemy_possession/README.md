# Enemy Possession

`drop custom_item "nei.possession_cap"`

Equip the cap on C/D-pad, target an enemy, and use it. Without a target the
nearest compatible enemy within 400 units is selected. Console shortcut:

`give possess target`

## Initial compatibility list

| Actor | Native action on B | Additional controls |
| --- | --- | --- |
| Lizalfos / Dinolfos | Sword slash | A jump |
| Stalfos | Sword slash | A jump, R shield |
| Red / blue Tektite | Attack hop | A high jump; movement hops |
| Normal / fire / ice Keese | Flight dash | A ascend, R descend |
| Octorok (body only) | Rock projectile | Move on its water surface |

Stick moves relative to camera, C-Left/Right rotates camera, C-Up exits.
Other enemies and bosses are explicitly unsupported for now. These are
implemented adapters, not a claim of completed gameplay regression.

The original enemy actor, skeleton, collision and health remain active.
Native damage/stun/death can interrupt its controlled callback. Attacks are
redirected to enemy targets, then original collision type bits and damage
flags are restored on exit. Player visibility, invulnerability, parent,
midair flag, camera and controls are restored by Command Melody.

During possession the hidden real player tracks the host so room loading and
camera location use its actual position. On exit Link appears behind the host
on valid nearby ground; the starting location is a fallback when still safe.
Transitions and actor loss end control. Command Melody's ordinary companion
control leaves Link at his starting position instead.

## Shared control integration

Depends on `command_melody`. It publishes `nei.possession.request.v1`, a
`bool (*)(Actor*)` request service. It uses the same camera/input lifecycle
as companions via the appended `NEI_PARTNER_TRACK_PLAYER` provider flag.
Ivan's Revenge / second-player input is deferred at the user's request; no
PvP mode is registered in this release.

Geometry, icon and text are authored in `build_assets.py.txt`, with pre-encoded
textures so builds need no Pillow. Compile/install from Shipwright-unbound
with `./build-all.local.ps1 -Only Mods -Reconfigure`.

Tests cover compatibility guards and collision restoration; camera/player
restoration is covered by `test_malon_command.py`. Every native controller
still needs gameplay testing in the checklist.
