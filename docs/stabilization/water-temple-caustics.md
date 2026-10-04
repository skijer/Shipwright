# Reusable Water Temple caustics

The `feat/reusable-water-temple-caustics` build adds an explicit native material-motion binding. The supplied Zora's Domain probe keeps Djipi's existing geometry and artist vertex colors, overrides one selected XML material, and packages its native material, caustic texture, and binding together in the same small O2R. Runtime clips confirmed the original horizontal motion and the later R3 material's improved wall readability. The new vertical-motion profile still requires in-game verification.

## Native Zora's Domain motion

The material artwork and motion source are independent. The version-1 metadata schema now supports these two caustic sources:

| `nativeAnimation.source` | Profile | Caustic motion |
| --- | --- | --- |
| `oot.water_temple.caustics` | WaterTempleCaustics, 10 | S phase `gameplayFrames`, step +1; T remains 0 |
| `oot.zoras_domain.caustics` | ZorasDomainCaustics, 11 | T phase `127 - gameplayFrames % 128`, step -1; S remains 0 |

Both sources require logical dimensions 32×32 and animate only tile 1. The Domain adapter reuses the active-water direction, phase, rate, and native interpolation from `func_8009E730` / opaque segment 0C. It retains the authored material's 32×32 caustic window instead of importing that segment's 64×32 layout or any tile-0 update. Visible world-space direction and speed also depend on the target's UV mapping and tile shifts.

This explicit reusable binding stays active at either age, including a thawed adult Domain. It intentionally does not inherit the original scene function's adult-age stop, nor change ice/thaw controls or season handling. Existing Water Temple, Lake Hylia, pool, light-sheet, and fountain bindings are unchanged. This is a supported source adapter, not automatic inheritance of arbitrary scene segments.

Select Domain motion when authoring an overlay with `--motion-source oot.zoras_domain.caustics`. The default remains Water Temple. Changing this option changes only the binding metadata; artwork, material commands, and wrapper paths remain the same. For an already tuned material, preserve its resources and change only `nativeAnimation.source` in the owning archive's `prelude/project/edits.json`.

Domain motion requires a build containing `ZorasDomainCaustics`; the earlier `a68d9e5` build supports only Water Temple motion and leaves an unknown Domain binding static. Install only one probe for the same material, above Djipi in the visible Mods list, Apply & Close, and restart.

The separately delivered R4 vertical/highlight probe retains R3's shaded additive blend and S/T shifts 1/1, with environment alpha raised from 95 to 112 (about 18% more caustic contribution). Its equation is `(stone + 112/255 × caustic) × vertexRGB`. The stone contribution remains full strength. The builder below still defaults to the original material blend; it is not a command to recreate the separately tuned R4 blend.

To validate a Domain-bound archive, use `--require-zoras-domain-caustics` in the focused runner below. It requires profile 11 and a valid insertion index. The existing Water Temple flag still requires profile 10.

The following original-probe instructions describe the original Water Temple source and default builder material.

## Install the probe

1. Use a build from `feat/reusable-water-temple-caustics` containing `WaterTempleCaustics` and `materials` metadata support. The runtime feature is commit `a68d9e55f946bc8436df95b4c028669b2918e7d2` or a descendant containing it. Older builds can draw the generated static caustic material, but cannot animate this new binding.
2. Put `Zoras_Domain_Water_Temple_Caustics_Probe.o2r` in the game's mods directory alongside the existing Djipi scenes and textures packs. Enable the overlay and those packs.
3. In the visible **Mods** menu, place the overlay **ABOVE Djipi's scenes pack**. The menu displays the enabled list in reverse while archives mount forward; filename sorting alone does not establish priority. Choose **Apply & Close**, then restart the game.
4. Keep **Enable Mods** / alternate assets enabled (`gSettings.AltAssets`). The existing `gEnhancements.PreludeNativeMaterialScroll` CVar defaults to `1`; keep it enabled for motion. Reload the scene after changes that leave cached resources loaded.
5. Check the selected surface in child Zora's Domain, then adult Zora's Domain with the existing thaw command enabled. That command still controls thawing; this overlay never changes ice state, room headers, collision, actors, Water Temple water levels, or scene segments. Frozen adult geometry can hide the selected surface, so it is not a reliable motion test.

Verify that the base stone texture stays fixed while the lighter caustic layer moves horizontally, with the existing teal/blue vertex shading retained. Inspect edges, UV seams, fog, and texture density at normal gameplay scale. Compare with the scroll CVar disabled and then reenabled. The code does not restrict the binding to either age.

To roll back, disable/remove only the probe O2R, Apply & Close, and restart. The original archives remain unchanged. Disabling alternate assets also restores the normal asset selection. The existing thaw command remains independent.

## What the overlay contains

The initial selected material is `scenes/shared/spot07_scene/mat_spot07_room_1DL_004200_ZoraDomain00`.

| Resource | Purpose |
| --- | --- |
| `alt/<selected material>` | Tiny XML wrapper: call the generated native material, then return |
| `custom/prelude/materials/water_temple_caustics_<selection hash>` | Native binary material, including the unchanged base texture reference and tile-0 setup |
| `custom/prelude/textures/water_temple_caustics_rgba16` | Native 32×32 RGBA16 donor texture copied from `MIZUsin_sceneTex_014430` |
| `prelude/project/edits.json` | Explicit `materials` binding for the native material in this same archive |

There are no parent-list, vertex, triangle, culling-resource, base-texture, collision, actor, room-header, or scene-segment replacements. Existing material culling flags remain. The compiler disables `G_LIGHTING` and enables `G_SHADE` before return so vertex RGB becomes the shade input; the vertex bytes themselves are untouched. Vertex colors, UVs, and lighting should already be authored for the target surface. Normal-vector bytes interpreted as colors will not produce useful shading. **Call the native material before loading the target surface's drawing vertices and before its triangles.** Lighting versus vertex RGB is resolved during vertex loading, so calling the material only after those loads is too late. Culling-only vertex loads may remain earlier. This is an authoring order requirement, not a geometry identity rule. The caller must retain the material state through those vertex loads and triangles, and authored UVs require texture-coordinate generation to be disabled in the caller's setup. The converter rejects explicitly enabled texture generation.

Tile 0 retains the base texture, descriptor, dimensions, origin, masks, shifts, and texture scale. Tile 1 uses TMEM 256, line 8, RGBA16, wrap in S/T, masks 5, and a static zero-origin 32×32 size. Defaults are shift S=3, shift T=0, environment RGB=(0,0,0), and alpha=95. The two-cycle color equation is:

```text
(base + (caustic - base) × 95/255) × vertex SHADE
```

This swaps the donor's texture roles and retains its 160/255 base weighting. No extra blue tint is painted into the texture or vertices. Copying donor vertex colors blindly would not reproduce authored lighting on a different mesh. UVs and colors affect visual tuning, never geometry eligibility.

Native motion updates only tile 1, horizontally by `gameplayFrames`, using the engine's interpolation-aware scroll generator. Logical dimensions stay 32×32 even with a higher-resolution replacement image. It does not animate tile 0 or import Water Temple water-level alpha/segment state.

## Rebuild or select another compatible material

From an initialized checkout, use Python 3.11+; MPQ `.otr` inputs additionally need `mpyq`. ZIP `.o2r` inputs need no third-party Python dependencies.

```sh
python3 scripts/build_water_temple_caustics.py \
  --source-scenes /path/to/scenes.otr \
  --vanilla /path/to/vanilla.o2r \
  --material scenes/shared/spot07_scene/mat_spot07_room_1DL_004200_ZoraDomain00 \
  --output /path/to/caustics-overlay.o2r \
  --strength 95 --shift-s 3 --shift-t 0
```

The material argument is an unprefixed resource path. The tool prefers `alt/<material>` in the scenes archive and falls back to the unprefixed resource if absent. Defaults select the initial Zora material and the values above. Strength accepts 0–255; shifts accept 0–15 in native GBI shift encoding. Another compatible XML material can be selected without any source mesh, scene, age, or texture-name match. The output path must differ from the inputs. The tool writes deterministic ZIP entries and a companion `.manifest.json` with source/archive and resource SHA256 values, the exact resource allowlist, and validation limits. It copies only the caustic donor texture, never a full input archive.

The compiler deliberately supports a narrow **opaque authoring grammar**, not arbitrary XML display lists:

- `DisplayList Version="0"`; sync commands; one terminal `EndDisplayList`; known set/clear geometry flags.
- Exactly one RGBA16 image reference, load tile 7 at TMEM 0 and line 0, `LoadSync`, `LoadBlock`, render tile 0, and tile-0 size, in that order. No texture LUT, LOD chain, nested lists, segment references, vertices, or drawing commands. Tile-0 dimensions and offsets are not fixed to the probe.
- One `Texture` command with enabled tile 0, level 0; one `SetTextureLUT Mode="G_TT_NONE"`.
- Complete high mode (`Sft=4`, `Length=20`) with one supported choice per field, two-cycle mode, filtered conversion, tile LOD, clamp detail, and no color key. Dither, point/average/bilinear filter, perspective, and pipeline choices are preserved.
- Complete low mode (`Sft=0`, `Length=32`): no alpha comparison, pixel depth, fog-shade or pass cycle 1, and AA/Z-buffered opaque surface cycle 2.
- Source combine: cycle 0 RGB is `TEXEL0 × SHADE`, alpha is `1`; cycle 1 RGB is `COMBINED × PRIMITIVE`, alpha is `COMBINED`. One white, fully opaque primitive color is required. This establishes the supported source alpha/tint behavior before substituting the caustic color equation. Fog/render mode and constant-one alpha remain intact.
- Unknown opcodes, unknown/missing attributes, invalid numeric fields, contradictory state, existing multiple textures, translucent/texture-edge alpha, nonwhite primitive tint, and other combiners fail with an error. Flag attributes must be `"1"`, since the XML factory tests their presence.

Binary serialization follows the actual libultraship conventions: a 64-byte little-endian resource header, F3DEX2 microcode byte 4 padded to offset 72, then little-endian uint32 word pairs. Texture-image hash commands have a separate high/low CRC64 payload pair. CRC64 uses the same polynomial, all-ones seed, and no final XOR as `StrHash64::CRC64`. The tests compile actual GBI macros and `StrHash64.cpp` and compare the complete synthetic output command stream.

## Reuse in Prelude or another room

For a compatible opaque XML material, point `--material` at that material and load the resulting overlay above its source pack. No geometry needs to be transplanted. An authored XML material can also call the generated native material using the wrapper below. Its caller must invoke this wrapper **before the target surface's drawing vertex loads and triangles**; culling-only vertex loads can precede it. The supplied Djipi caller already follows that order: culling vertices and cull, material call, then the triangle-list call whose first command loads the drawing vertices.

```xml
<DisplayList Version="0">
  <CallDisplayList Path="custom/prelude/materials/water_temple_caustics_YOUR_SELECTION_HASH"/>
  <EndDisplayList/>
</DisplayList>
```

Use the actual generated path from the manifest. Keep the called native material and its explicit binding in the **same archive**. A Prelude exporter can put the following entry inside `edits -> scene key -> edit -> data -> materials` (the scene key does not select motion):

```json
{
  "newDlPath": "custom/prelude/materials/water_temple_caustics_YOUR_SELECTION_HASH",
  "nativeAnimation": {
    "version": 1,
    "binding": "material-motion",
    "source": "oot.water_temple.caustics",
    "logicalWidth": 32,
    "logicalHeight": 32
  }
}
```

The same exact `nativeAnimation` object can accompany an explicitly authored `pastes` or `shapes` entry. Material entries require explicit metadata; they do not fall back to recognized geometry. Export paths must be under `custom/prelude/`. The native list must contain the complete validated tile-0/tile-1 setup, followed by a terminal return or a flat sequence of triangles. Calls/branches/segment writes inside that native list and ambiguous/incomplete layouts remain unbound. Metadata comes from the native material's owning archive; a separate globally overriding JSON file is insufficient.

The **runtime binding** is independent of geometry, scene, and alpha/color-combine choices. The **current converter** supports the opaque grammar above. A future translucent Lost Woods pool must retain its appropriate alpha/render state in an authored two-texture material before using this same tile-1 native-motion binding. No additional scene-specific runtime patch is needed for a compatible authored material. Adding a third texture to an existing two-texture water shader is outside this converter's scope. The Lost Woods pool geometry was not supplied with this probe and has not been patched. Obtain its material/geometry in Prelude, author or select the appropriate two-texture material, keep its transparency if applicable, write the binding in its owning O2R, and visually tune UV scale/shift and vertex colors on that mesh.

## Focused validation

```sh
python3 -m unittest discover -s soh/tests -p test_water_temple_caustics.py -v
python3 scripts/diagnostics/run_native_material_probe.py \
  --json-include /path/to/nlohmann/include \
  --spdlog-include /path/to/spdlog/include \
  --require-water-temple-caustics /path/to/caustics-overlay.o2r
git diff --check
```

The runner exercises production identity/layout validation and actual native interpolation command generation. It reads `materials`, `pastes`, and `shapes`, and explicitly checks WaterTempleCaustics profile ID 10 and a valid insertion index; a successful export-probe process alone does not establish binding success. Before filtering declarations, the diagnostic conservatively rejects any duplicate metadata path across the archive, including identical declarations that the runtime can accept. This diagnostic-only restriction prevents a valid declaration from hiding a conflicting invalid or unbound declaration. The real initial overlay has one declaration and resolved with insertion index 31, immediately before its terminal `ENDDL`. Input SHA256 equality and the four-resource allowlist were independently checked. These checks do not establish final in-game appearance or verify a full platform build.
