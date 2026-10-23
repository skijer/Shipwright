# Wind Waker Style: Cel Shading for OoT Unbound

Enable **Wind Waker Style - Cel Shading** in the mod manager, restart, then open
**Enhancements → Cel Shading**. The effect starts enabled. Band defaults match
the reference: boundary 0.5, softness 0.02, highlight 0.6 and shadow 0.6.

Each actor receives one dominant light: the nearest point light within its radius,
otherwise the brighter environment directional light. Direction and color ease
between sources. Scene geometry, doors and unlit effects retain native lighting.

The menu controls the light/shadow boundary, softness, highlight/shadow intensity,
point-light range, Navi's light, transition time, item/chest shading and a monochrome diagnostic.
Untick Enable Cel Shading for an immediate comparison.

This package requires the corresponding Unbound renderer update and regenerated
`soh.o2r`. The mod owns the settings and per-actor light selection; the host exposes
the optional `SetCelShadingRamp` API entry and the Fast3D transport. Its existing
`OnActorDraw` / `OnActorDrawEnd` hooks scope the effect to each actor.

Adapted from roborich's `wind-waker-style-cel-shading` branch, Fast3D transport
commit `a001ff42`, through the 2Ship port. This package contains cel shading;
light-casting pools, silhouette shadows and the Wind Waker sky are separate features.

Build and install from the sibling `Shipwright-unbound` checkout:

```powershell
.\build-all.local.ps1 -Only Mods -Reconfigure
```

Validation: 64 native Direct3D shader combinations (128 vertex/pixel stages), 64
OpenGL template combinations, vertex strides, dominant-light selection, day/night
fallback and smooth transitions. In-game visual validation remains pending.
