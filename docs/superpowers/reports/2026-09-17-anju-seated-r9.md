# Anju R9 seated repair and R8 standing preservation

The user requested one palette archive containing both umbrella poses, separate pose-specific forearms, and a slimmer seated skirt without the sharp central cut. The supplied comparison clip confirms the lap ridges and V-shaped center discontinuity.

R8 and the standing adapter were recovered. No later finished seated archive was recovered. R9 rebuilds that seated pass from R8; it does not claim recovery of the missing export.

## Changes

- One continuous seated skirt, fitted near the unchanged sash and with room below the knees; hem stays near the recovered height.
- Subdivided/smoothed connected crying forearm, retaining exact sleeve and wrist boundary vertex/bone records and exact wrist normals.
- Complete preserved-standing lower-body roots, used with the pose selector pushed in `a89adb09c7fe62e32a028d3e7d9ad4d96fa2aba1`.
- Textures, both hands, umbrella, blinking resources, skeleton, native clips, and placement remain unchanged.
- One O2R per palette; each includes standing `0x7E1D` and seated `0x7E0D`.

## Verification

Both output archives passed CRC checks, the compiled production pose-selector fixture, 43 actual seated animation frame readbacks, two-boundary manifold checks on skirt and forearm, and exact sleeve/wrist endpoint comparisons. All protected source payloads are byte-identical. Standing decoded positions, normals, UVs and triangles match R8 exactly at frames 0, 16 and 31. Independent review also followed 120 recursively reachable standing payload pairs and found them identical.

Front/side and close-up offline renders were inspected. Independent review found no blocking archive/topology/ownership issues. Sleeve-edge skin normals vary by up to 16.65 degrees from R8; wrist-edge normals remain exact. The new skin normals are intentional smoothing and do not create a geometric gap.

Full platform CI remains associated with the standing commit; this diagnostics-only commit skips CI so it does not cancel that running build. In-game visual verification remains required.

## Reproduce

Run `scripts/diagnostics/fit_anju_seated.py NATIVE_MM R8_SOURCE OUTPUT_O2R`, then `scripts/diagnostics/verify_anju_seated_fit.py NATIVE_MM R8_SOURCE OUTPUT_O2R REPORT_JSON`. Repeat for the other palette. `render_anju_seated_review.py` produces matching-camera archive comparisons. Python dependencies: numpy, scipy and Pillow.

## Export evidence

```json
[
  {
    "file": "Anju_HD_Auburn_Umbrella_R9.o2r",
    "sha256": "66110ea1416eab3fb8cd173e8f9ff84d20b57cb96c461072cb2893cd2cbffc67",
    "size_bytes": 9906140,
    "resources": 191,
    "seated_frames": 43,
    "standing_sample_frames": [
      0,
      16,
      31
    ],
    "standing_payloads_exact": true,
    "sleeve_wrist_boundaries_exact": true,
    "minimum_triangle_area": 36.52565043460753,
    "maximum_triangle_edge": 258.6078111735997,
    "topology": {
      "objects/object_anju_hd/v1/SeatedForearmR9SeatedR9DL": {
        "vertices": 414,
        "triangles": 793,
        "boundary_loops": 2
      },
      "objects/object_anju_hd/v1/SeatedFittedSkirtR9SeatedR9DL": {
        "vertices": 1584,
        "triangles": 3072,
        "boundary_loops": 2
      }
    }
  },
  {
    "file": "Anju_HD_Goth_Umbrella_R9.o2r",
    "sha256": "7a61fb109e3c95b0675f9b0ef317cd7db4be94da735b071943d66c49b1c04662",
    "size_bytes": 9004931,
    "resources": 199,
    "seated_frames": 43,
    "standing_sample_frames": [
      0,
      16,
      31
    ],
    "standing_payloads_exact": true,
    "sleeve_wrist_boundaries_exact": true,
    "minimum_triangle_area": 36.52565043460753,
    "maximum_triangle_edge": 258.6078111735997,
    "topology": {
      "objects/object_anju_hd/v1/SeatedForearmR9SeatedR9DL": {
        "vertices": 414,
        "triangles": 793,
        "boundary_loops": 2
      },
      "objects/object_anju_hd/v1/SeatedFittedSkirtR9SeatedR9DL": {
        "vertices": 1584,
        "triangles": 3072,
        "boundary_loops": 2
      }
    }
  }
]
```
