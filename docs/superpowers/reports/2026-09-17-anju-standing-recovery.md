# Anju standing pose recovery

The published branch ended at `4773c4bb`, with seated crying Anju only.
Recovered the uncommitted standing adapter and both intact R8 archives.

- `En_Viewer 0x7E1D`: native 32-frame standing umbrella idle.
- `En_Viewer 0x7E0D`: existing 43-frame seated crying pose.
- Separate HD pose caches select R8 torso, arm, hand and umbrella roots.
- Optional preserved-standing skirt roots are accepted only as a complete set.
- Native fallback, actor placement, blinking and retained graph ownership remain intact.

Verification on the recovered source:

- All 15 available CTests pass after rebuilding affected targets.
- Compiled production viewer fixture passes both Anju poses, independent blinking,
  fixed XYZ/yaw, live alternate-asset changes, missing-umbrella failure and reentry.
- `verify_anju_pose_selection.py` compiles the unmodified production selector and
  checks separate pose roots, immutable seated data, cache reuse, missing standing
  resources, complete/partial standing skirt resources and native sharing.
- The same script decodes actual native animation and HD geometry for all 43 seated
  and 32 standing frames of each R8 palette. Archive CRC checks pass.
- clang-format 14 and `git diff --check` pass.

The selector fixture substitutes lookup at the display-list resolver boundary;
it is not the full ResourceManager integration fixture. That fixture's Anju loop
now includes both native poses, but the full engine dependencies were unavailable
locally after recovery. A full build and in-game visual acceptance remain distinct
from the focused checks above.

R8 archive SHA-256:

- Auburn: `b16994376c158692e99dfb7adc70d32e4a0e12ecd5703b18e24dccef51c95f20`
- Goth: `5b370bace0aa36c38b3b809c50b0f39de08365694dab895a2f2d6d86a48d5add`

R8 retains the seated R5 mesh. The user's later seated forearm and skirt repair
was not found among the recovered exports; do not label R8 as that finished repair.
