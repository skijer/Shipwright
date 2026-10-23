# Switch Hook material — 21 October 2026

Compared the authoritative Gust Jar `jar_body_dl` material with Switch Hook.
Gust Jar keeps `G_LIGHTING`: vertex RGB bytes are normals. It renders SHADE in
cycle 1, then multiplies COMBINED by each material's PRIMITIVE tint in cycle 2.
Its render mode pairs the fog blender with the opaque surface's second cycle.
Switch Hook had forced one cycle while retaining that two-cycle render mode.

Switch Hook now uses Gust Jar's complete geometry/combiner/other-mode setup,
with purple, gold and silver primitive colors for its existing vertex batches.
No texture is necessary. Positions, normals, triangle indices and scale remain
unchanged. The full model is used as a drop. Final hand calibration is
translation (-1.90, 0.95, 4.43), rotation deltas zero and scale multiplier 0.52.
The hand pose is a fixed model matrix; temporary menu controls are removed.
The purple/gold body (160 triangles) remains in the hand during flight;
only the silver head (280 triangles) travels with the chain. The head's
chain socket is shared by launch, return and chain drawing. The animated
hand frame uses normalized arm vectors instead of repeated trigonometry.

Build/package checks do not confirm appearance in the running game.
