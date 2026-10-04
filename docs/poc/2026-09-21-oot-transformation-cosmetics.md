# OoT transformation cosmetics candidate

Repository: `marsh6487/Shipwright`.
Corrected cumulative baseline: the working feature chain through
`6f03c439ab83cbf149c4a368b529e53724f6fc47`, combined with the cosmetics and native
TorchFlame sampling line at `f8b87d2e1d010bceebae6292613194b3fb92b1a4`.
The original publication used the older September 12 integration tip and
omitted the later actor, equipment, caustic, item-color and pedestal work.
The cumulative merge restores that entire chain; see `docs/WORKING_INTEGRATION.md`.
The baseline rain, audio, actor behavior, geometry and archives are preserved.
The TorchFlame loader is already included through that parent; it is not
reimplemented by this patch. This work targets Shipwright, not ComboShip.

## Changes

- Cosmetic Editor > Mods groups loaded tagged materials by Child, Adult,
  Deku, Goron, Zora and Fierce Deity. Existing custom CVar keys and manifest
  category/entry names remain intact. Only forms with available entries appear.
  The user's 3DS Custom Colors manifest also uses `object_link_goy` for Goron
  materials; skeleton/category identification covers that custom directory.
- Mod colors participate in Randomize All and the existing automatic modes.
  On New Scene generates fresh colors on each scene initialization, including
  reentry. Seeded modes retain their repeatability, and locked colors stay put.
  Global reset/lock/rainbow controls include the same custom entries.
- Rescanning uses color-command position instead of original RGB equality,
  and refreshes CVar string pointers after moving/sorting entries. Alt changes
  rescan bindings. Cached barrier copies receive live tagged color updates.
- Zora barrier land/idle-water scale now matches MM's intensity-dependent
  scale, also used by fast swimming: `intensity * (10 / 51) * 0.01` in world
  space. At full intensity the land effect is twice its former linear size.
- Cosmetic Editor > Effects > Magic Effects adds Zora Magic Shield,
  Zora Shield Glow and Zora Shield Highlights. The native two-layer shader
  keeps its alpha, primitive LOD fields, texture scroll and geometry.
  These controls share the existing Zora shield mod's custom color keys, so
  its Mods controls and the native effect controls stay consistent.
  Unrecognized custom shader layouts retain their own tagged controls.

The shield change affects visuals only. Magic drain, damage collider, movement,
immunity, sound and environment lighting are not changed. It also applies to
the existing human Zora-tunic barrier, which shares the same draw function.

## Verification and acceptance

Focused production-code fixtures cover scanner bindings, grouping, rescan,
randomization/locks/seeded modes, cached-copy colors, and the barrier's matrix
scale and emitted color commands. Both runners are included in the Shipwright CI checks:

```
python3 scripts/diagnostics/run_oot_custom_cosmetics_tests.py
python3 scripts/diagnostics/run_zora_barrier_cosmetics_tests.py
```

These fixtures replace resource/graphics services; they do not render the game.
Full Windows/Linux builds and runtime acceptance are separate evidence.

Runtime check: enable the relevant form packs in the OoT mod set; inspect each
loaded form's Mods group; change and reset one color; use On New Scene and leave
and reenter twice, also checking a locked color. Test Zora R+B on land and R
underwater, the three shield colors, reset, charge/fade, and an Alt-assets toggle.
Keep the prior build until the candidate is accepted.

## September 22 shield anchor follow-up

Baseline: cumulative SoH integration `7eefc9986d4b641400b500f88c79a6abf8367895`.
Candidate branch: `fix/zora-shield-anchor-20260922`, retaining that complete
history, including the custom cosmetic controls and hide-scabbard restoration.
The user confirmed the cosmetic dials and shield scale in game, but the supplied
clip shows the grounded shield rising from the torso instead of enclosing Link.

The non-fast-swim draw added 40 world units to the actor's Y before applying
MM's native quarter-turn and -18-unit local offset. Native MM's ground draw
starts from the actor root. Remove the extra +40: the resulting effect origin
is 18 world units below the feet, independent of yaw or charge intensity.
Ground, iron-boots and idle-water shielding share this path. Preserve the
accepted intensity scale and shader colors/alpha/scroll.

The user also requested correct swimming orientation. The old fast-swim draw
used a fixed chest offset and pitch/roll but omitted the animation root position
and rotation. For an active Zora skeleton, rebuild the actor transform and apply
the current animation root, swim pitch, native smoothed shield roll, root rotation,
and native shield flip/offset. This follows the same root pose used by the model
and native MM's shield draw. Actor scale is applied once, retaining the accepted
normal size. The human Zora-tunic/no-skeleton fallback keeps its existing swim
transform. No gameplay, UI, model or texture resource changes are included.

The existing barrier fixture now executes production matrix operations and
checks world-space landmarks, animation-root changes, turns, diving/rising,
smoothed roll, charge/fade, return to ground and matrix-stack restoration. The
new ground-position and animated-swim assertions each failed before their
correction and pass afterward; the existing color and custom-cosmetic scanner
regressions also pass. These checks do not render the shield mesh.

Candidate runtime check: R+B on land at several camera angles, R while idle
underwater/on the floor, then fast swim, turn, dive, rise, roll, and return to
standing. Confirm leg/head enclosure and alignment along the swimming body
with the same packs and Alt setting used in the clip. Full platform build status
and user acceptance remain separate from these local checks.

The requested Expressive Zora/custom-color pack compatibility is a separate,
unfinished asset task. The stored pack files could not be materialized because
the file service returned an invalid-metadata error. Their resource paths and
materials have not been compared, so this candidate makes no compatibility claim.
