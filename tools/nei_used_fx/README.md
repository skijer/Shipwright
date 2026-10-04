# NEI USED magic presentation candidate

Parent: `32036693c3c1d0ba6f44ee5d321c6aacf60def69` (published PR17 tree).
This is a local presentation candidate. No game runtime or user acceptance is claimed.

The pure `NeiUsedMagicPolicy.h` samplers drive both runtime and offline preview:

- Light: radiant white/gold projectile, continuous wake, rising charge rays, jump beam and spin ring.
- Ice: moving faceted lance cluster, fractured material, falling shard wake, collapsing charge fragments, jump ice and spin ice.
- Fire: remastered charge aura and embers; native fired fireballs remain byte-for-byte unchanged.
- Time Gate: temporal dial, counter-rotating arcs and curling vertical energy, using existing alpha/scale state.

`NeiUsedMagicPresentation.cpp` preserves the caller's matrix and uses the existing shared batcher. Private RGBA32 surfaces are resolved by static paths in `objects/nei_used_magic`. Missing material resources retain bounded geometry. Base and Alt copies must be identical in the final archive. Texture metadata uses 32×32 logical tiles, actual native pixel sizes and matching scale factors. `material_manifest.json` records source pack entries and hashes. `build_textures.py` reproduces the conversion from the supplied medallion pack; it writes no global resource paths.

All Ice facets are submitted once: the textured pass replaces their untextured fallback. Their front-facing culling remains the approved GI primitive's culling. The main geometry and texture surfaces are separate bounded meshes (at most 512 triangles each).

Charge sparks originate at the exact existing focus matrix inside `NeiHeld_DrawRod`, after its approved GI energy. They never use an inferred offset or the melee collider tip. The preview sheet displays spark geometry at an illustrative isolated focus so the effects can be inspected without a Link pose; runtime origin is covered by the production dispatch test.

The old charging GSpk instance is retained with zero-size geometry. Its original ten-frame lifetime, pool slot, update and two RNG calls per tick therefore remain intact. Light retains its original single draw-time RNG call. New visual animation is deterministic and never uses gameplay RNG.

## Source ownership trace

| Original visual | Active source/resource ownership | Candidate |
|---|---|---|
| Fire fired shot | `object_firerod.c` → `objects/gameplay_keep/gEffFire1DL`, the shared torch-flame DL | Unchanged |
| Ice fired shot | `object_icerod.c` → the same `gEffFire1DL`, recolored blue | Replaced only in item draw tail |
| Light fired shot | `object_lightrod.c` → `objects/object_fhg/gPhantomEnergyBallDL` | Replaced only in item draw tail |
| Rod charge aura / Ice and Light spin | `FX_DrawChargeAura` / `FX_DrawSpinFireCylinder` → `gameplay_keep/gEffFireCircleDL` | Item call-site replacement |
| Rod charging sparks | `FX_SpawnRodSwingParticles` → GSpk → `gameplay_keep/gEffSparkDL`, `gEffSpark1Tex`–`gEffSpark4Tex` | Original invisible lifecycle + private sampled sparks |
| Vanilla sword charge | EnMThunder → `gameplay_keep/gSpinAttackChargingDL` | Unchanged |
| Time Gate portal | `object_timegate.c` → `object_warp1/gWarpPortalDL` / `gWarpPortalTex` | Item portal draw replacement |

The supplied native `oot.o2r` was decoded directly: `gEffFire1DL` references both
`objects/gameplay_keep/gDecorativeFlameTex` (I8, 32×64) and
`objects/gameplay_keep/gDecorativeFlameMaskTex` (I4, 32×128). The display-list
resource SHA256 is `f0af13f7d9e9c28f4e55aedd858c96cfb256ccd933e5562f99139f482305d290`.
The complete Fire projectile draw tail and these shared resources are unchanged.

## Verification

```sh
CPLUS_INCLUDE_PATH=/tmp/combo-json-fix/include python tests/nei_used_fx/run_tests.py
CPLUS_INCLUDE_PATH=/tmp/combo-json-fix/include python scripts/diagnostics/run_nei_gi_tests.py --held
```

Source contracts preserve every rod gameplay function after removing only visual calls and the old Light charge palette block, the entire Time Gate logic file, the native GSpk update, and the Fire projectile tail. A separate source contract verifies that the material-null shared batcher remains token-equivalent to the accepted baseline.

The real shared renderer was sampled at all 180 phases with the maximum existing 5×3 active projectile heads, five wakes, full charge aura/material/focus and approved held GI orb/energy:

| Element | Maximum arena vertex bytes | Maximum XLU commands |
|---|---:|---:|
| Ice | 68,832 | 1,898 |
| Light | 64,976 | 1,957 |

The gate retains ordinary scene space (`<90,000` vertex bytes and `<2,400` XLU commands versus the engine's 4,096-command XLU pool). This checks bounded submission cost, not scene-specific total arena use or game frame rate. Base/Alt resource presence, missing-resource fallback, C header compatibility, world placement, matrix restoration, finite bounds, Ice facets, portal growth/fade and texture dimensions/alpha/hashes are tested.

## Preview reproduction

```sh
c++ -std=c++20 -O2 -Isoh tools/nei_used_fx/export.cpp -o /tmp/nei-used-export
/tmp/nei-used-export /tmp/nei-used-final.bin
python tools/nei_used_fx/render.py /tmp/nei-used-final.bin /path/to/output --video
```

These are production triangles and private native RGBA texels under standard alpha blending. No bloom or invented particles are added. Camera, isolated focus placement and hold intervals are fixture choices; Time Gate uses the real +8/−12 alpha and +.05/−.04 scale increments. Videos are 9 seconds at 20 FPS. The main sheet shows released projectiles, charge effects and Time Gate; the second shows jump and spin attacks. Gameplay transitions, native supplemental hit particles and audio still require an in-game check in the desired pack/Alt configuration.
