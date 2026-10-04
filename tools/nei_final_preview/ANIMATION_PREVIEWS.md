# Source-sampled player fitting previews

These scripts read the user's external, unmodified `oot.o2r` and
`Young_Din_POC4_Equipment_HairFix_OOT.o2r`. They do not bundle either archive or
native player mesh/animation bytes in this repository. Item GLBs are the actual
candidate checkpoints, with their scene-node conversion matrices and gameplay
draw scales applied.

```bash
python tools/nei_final_preview/render_player_fit.py \
  --player /path/Young_Din_POC4_Equipment_HairFix_OOT.o2r \
  --native /path/oot.o2r --output /path/previews
python tools/nei_final_preview/render_player_fit.py \
  --player /path/Young_Din_POC4_Equipment_HairFix_OOT.o2r \
  --native /path/oot.o2r --output /path/previews --mode cane --cane-speed nominal
python tools/nei_final_preview/render_held_fit.py \
  --player /path/Young_Din_POC4_Equipment_HairFix_OOT.o2r \
  --native /path/oot.o2r --output /path/previews
```

Dependencies: NumPy, SciPy, Pillow, system Mesa/EGL and ffmpeg. The renderer reuses
the repository's headless EGL context and native resource readers.

## Evidence supplied

- Spinner: actual 35-frame `link_fighter_Lpower_kiru_wait`; `R_UPDATE_RATE=3`,
  play speed 1; platform rotates `0x800` binary-angle units per game update.
  Actor root is fixed at the normal riding height of 17 world units. Contact is
  evaluated in a stationary, flat-floor source-animation control.
- Somaria: every packed source frame is checked against the 60-frame C array.
  The actual `sUpperBodyLimbCopyMap` selects joints 10–21 over native
  `link_fighter_wait_long`, beginning at idle phase zero. The normal clip uses
  source play speed 2 and `R_UPDATE_RATE=3`: three source frames per update.
  The slow scan shows all 60 source frames at one-third cast speed.
- Rods: actual native two-handed ready frame zero, captured left wrist,
  native child palm socket `(0,216.22,4.5)` and `RZ(-122°)`, draw scale `.05`.
  Separate geometry views check the same child socket against native child and
  Young Din fists, and the native adult socket `(0,328,-77)` against adult fist.
- Shovel: actual source dig frame 37/49, actual upper-body mask over native
  default idle. Existing two-hand wrist midpoint and yaw/pitch calculation are
  preserved. The comparison shows `.06` before and `.048` for child after;
  adult `.054` is recorded but not presented as visually tested.

The sampler traverses the real skeleton and near display-list hierarchy. Each
vertex load retains its dynamic flex-matrix slot, so geometry shared between
limbs follows the correct transform. Player root translation receives the
engine's `.64` child correction; actor scale is `.01`. World matrices for every
player limb and attached item are exported as review evidence. Manifests record
archive, animation, GLB, checkpoint, source and output hashes.

## Scope and limits

This is **offline source evidence**, not a game capture or runtime acceptance.
The preview has a fixed studio camera and light, approximate texture combining
and reflection sampling, and a deliberately chosen idle phase. Runtime inverse
kinematics, head tracking, preceding-action morphs, scene collision, effects,
first-person transitions and state-dependent hand selection are not simulated.
The main cast/rod fitting configuration uses closed fists and hides back
equipment; shovel uses the source default open hands. Neither player archive is
modified by this workflow.

The native Spinner stance's Young Din rear-foot support extends to radius
18.880475 world units. The final usable deck radius 19.65 supplies about 0.77
units of margin. Source sole heights vary slightly between feet; the deck at
`-.5625` relative to actor root clears the lowest source sole, while small
central relief can overlap the sole. This is a bounded fitting observation,
not collision or runtime floor-contact proof.

The source-grounded preview found the old deck support and wrist-origin grip
issues, informed the candidate changes, and then rendered the final geometry.
No turntable or invented skeleton pose is substituted for animation evidence.

`inspect_cast_clearance.py --player /path/YoungDin.o2r --output /path/previews`
performs a narrow triangle-surface check using the exported slow-scan matrices.
Source frames 12–48 of the final cane have zero detected surface intersections
with head/hair, either forearm, torso or shoulders. Fists are deliberately
excluded. Coplanar contact and enclosed-volume tests are outside this check;
it does not replace the stated runtime review.
