# Ikaxe — NEI appearance

Reference: `Shipwright-nei/soh/mods/equipment/objects/ikaxe_DL/model.inc.c`.
All 69 vertex positions, UV coordinates and normal/alpha bytes match that source.
Every triangle index, flag and ordering matches the same display list.

The original materials are restored through the same native OTR resources used
by NEI: `gIronKnuckleMetalTex`, `gIronKnuckleBlockPatternTex`,
`gIronKnuckleJewelTex`, and `object_ik_Tlut_00F630` in `objects/object_ik`.
Metal uses the original environment mapping, texture scales, gold/silver
primitive/environment colors and combiner. The shaft retains its RGBA16 block
texture; the jewel uses CI8 and its original 256-color RGBA16 palette.
No flat-color substitute or new texture is used.

The mod preloads its display list at initialization to validate resource parsing.
A draw hook resolves native, root and previously resolved custom world drops
against the live SAVE progression, without mutating the actor entry or inventory.
The held get-item model remains fixed during its acquisition animation. This uses
existing hooks only; no engine or SDK changes were made for this correction.

Production preview-function tests cover every chain, already resolved drops,
base/maximum levels, explicit upgrade floors and unrelated items. Visual approval
in gameplay remains pending user feedback.
