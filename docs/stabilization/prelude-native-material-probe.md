# Prelude native-material scrolling probe

Target: integrated Shipwright, starting at `9a76eb637a4090279ccbcca14e5c043412ea766b`.
Scope: Lake Hylia water, Pool/Kakariko Well water, and the supplied Lost Woods light sheet.
Status: source/export investigation and CPU probe. In-game validation is still required before permanent promotion or merging downstream to ComboShip.

## 1. Exported identity and rendering

The supplied water surfaces converge to `pastes[]` with `placed.recipe = "water"`, four vertices and two triangles each, routed to a translucent host display list. Their collision waterboxes are separate entries with position/extents, camera, lighting, room, and swimmability fields; those entries do not carry the water visual profile.

| Reference | Exported provenance | Generated resource |
|---|---|---|
| Lake | `chain[0].path = objects/object_spot06_objects/gLakeHyliaHighWaterDL` | `custom/prelude/market_ruins_scene/paste0` |
| Pool | `chain[0].path = objects/object_spot01_objects/gKakarikoWellWaterDL` | `custom/prelude/market_ruins_scene/paste1` |
| Light sheet | `stored.textures[0..1].label = spot10_room_1Tex_008030` | `custom/prelude/market_ruins_scene/recipe23` |

Water retains source display-list provenance even though the exported instructions inline static material setup and omit the segment call. The two water lists share tile/combine/render-mode setup; their texture hashes, vertex hashes, marker, and primitive alpha differ. Lake alpha is 160 and Pool alpha is 127 in these exports. The probe preserves these differences.

The GLB light sheet has `FAST64_materials_n64` material state, two texture layers, combiner and blending settings, but no native animation declaration. Its exported recipe has an empty source chain. The original stored material label survives the supplied retexture, separately from the generated/resolved texture paths. V1 recognizes that exact two-layer provenance. This is weaker than a formal immutable material ID: if a different export removes or renames the original labels, it is intentionally left unanimated. A future Prelude exporter should preserve an explicit native-material ID through recipe export/import. No runtime texture pixels, dimensions, contents, hashes, or resolved filenames are used to choose a profile.

The older `recipe22` is also identifiable as this light-sheet material, but contains no triangle commands. The probe leaves it untouched. The supplied `recipe23` is drawable and is the retextured placement.

## 2–3. Exact native scrolling setup

| Profile | Native caller | Native clock | Layer 0 offset | Layer 1 offset | Logical tile size | Interpolation increments |
|---|---|---|---|---|---|---|
| Lake | `BgSpot06Objects_DrawLakeHyliaWater` | `play->state.frames = f` | `(-f, f)` | `(f, f)` | 32×32, both | `(-1,+1), (+1,+1)` |
| Pool | `BgSpot01Idomizu_Draw` | `play->state.frames = f` | `(127-f%128, f&127)` | `(f%128, f&127)` | 32×32, both | `(-1,+1), (+1,+1)` |
| Light sheet | scene config `SDC_LOST_WOODS` (9), `func_8009EE44` | `play->gameplayFrames = f` | `(f%128, 0)` | `(f%128, 0)` | 32×16, both | `(+1,0), (+1,0)` |

Each native caller supplies `Gfx_TwoTexScrollEx(...)` to translucent Segment 08. The generator in `soh/src/code/z_rcp.c` allocates 12 `Gfx` commands using `Graph_Alloc`, wraps coordinates modulo 2048, emits tile synchronization and two five-command interpolated tile-size operations, and ends the list. It does not load a texture or require scene initialization.

Lake's actor also supplies Segment 09 with six-times vertical increments. That is a separate native setup and is not substituted for the selected high-water surface's Segment 08 profile. Lost Woods likewise supplies a separate water-style Segment 09; it is not the light-sheet behavior.

The source trace confirms native callers and generators. The original native display-list binaries are not bundled in these exports, so their internal calls were not independently decoded here; the Lake Segment 08 selection also uses the previously supplied relocation evidence.

**Correction to the initial speed expectation:** native Lake Segment 08 and native Well Segment 08 have the same scroll increments on this branch. Their phase differs by one subtexel modulo the native period. The perceived calmer Pool movement must be assessed with its texture mapping/artwork in game; this probe does not invent a slower speed.

## 4–5. Per-draw isolation

The probe decorates the existing binary display-list factory. Only recognized `custom/prelude/` F3DEX2 lists with two initialized texture tiles, straight-line geometry, no nested/segmented calls, and no later material changes are eligible. Native and already-relocated segmented lists stay unchanged.

One direct display-list call is inserted immediately before the first primitive, after the surface's static material setup. It invokes that profile's native-generated scrolling commands. It does not write Segment 08 (or any segment), so Lake, Pool, and light-sheet draws can coexist without rebinding shared scene state.

Segment 08 is shared renderer state. An architecture using it would need the right binding at each surface and restoration for subsequent callers. Directly invoking the same generated list avoids that state-management requirement. The dynamic command contents can be generated once per profile per rendered game frame; the appropriate list still executes at each surface draw.

## 6–7. Texture dimensions and retexture probe

Native logical scroll dimensions are 32×32 or 32×16. They describe N64 tile coordinates, not image-resolution limits. The probe uses them exactly and leaves texture loading, sampler shifts, resource hashes, high-resolution scale metadata, and image data unchanged. The existing `G_SETTIMG_OTR_HASH` interpreter path resolves current resources and passes `Width`, `Height`, `HByteScale`, and `VPixelScale` through its normal HD texture path.

The supplied fixtures contain:

| Resource | Vanilla-light export | Retextured-light export |
|---|---|---|
| Lake replacement | 4096×4096 | Same bytes, 4096×4096 |
| `recipe23_tex0` | 32×16 | 4096×2048 |
| `recipe23_tex1` | 32×16 | Same bytes, 32×16 |

CPU/export checks select identical profiles and insertion positions for both archives. All three drawable reference display lists are byte-identical between the two supplied versions. Their texture-loading instructions therefore remain identical despite the first light-sheet layer's replacement. The newer light-sheet vertex resource also differs between exports; the probe preserves each export's own geometry/UV data.

No downscaling, staging texture, texture copying, or parallel texture pipeline is introduced. **This is not yet an in-game high-resolution rendering pass.** Neither the fire-water visual acceptance test nor native-scene comparisons have been marked passed.

## 8–10. Patch boundary and risks

- `NativeMaterialProfile.{h,cpp}`: narrow exported-provenance resolver, conservative command analyzer, exact native generator parameters.
- `PreludeNativeMaterialScroll.{h,cpp}`: binary-factory decorator, owning-archive metadata, stable scrolling-list storage and frame update.
- `OTRGlobals.cpp`: register the decorator in place of the original binary factory; it delegates parsing to that original factory.
- `z_play.c`: refresh scrolling commands after world drawing, before graphics interpretation.
- Focused tests and `scripts/diagnostics/run_native_material_probe.py`.

The branch automatically enables recognized materials; no console commands or per-scene setup are required. A troubleshooting CVar can disable the inserted lists without rewriting resources. The original native actor/scene code, XML factory, collision, geometry, and resource manager are unchanged.

Lifetime: cached resource calls point only into process-lifetime profile buffers. Native frame allocations are copied, never retained. `Graph_Update` finishes before synchronous `Graph_ProcessGfxCommands`; buffers remain unchanged during interpolated rendering and graphics-debugger replay. Frame reset is tested independently from allocation reuse.

Archive ownership: metadata comes from the archive supplying that particular display list, not an unrelated mod's globally winning `edits.json`. Archive objects can survive reopening, so cached parsed metadata is refreshed by comparing the actual raw metadata bytes whenever a display list is imported. Texture bytes are never involved in this comparison. Metadata read cost is confined to imports; 1,000 repeated reads/comparisons of the supplied 2.3 MB metadata took about 4.5 seconds in a Python ZIP measurement on this host. This is a rough load-cost measurement, not a game benchmark.

Remaining risks: unsupported/exporter-lost provenance, complex/multi-material recipes, first-load cost, and runtime texture-coordinate behavior with the user's mod stack. Do not broaden recognition to guess at ambiguous materials.

## Verification performed

- Focused native-profile and command-safety tests passed.
- A CPU harness compiled the production runtime bridge and extracted native `Gfx_TwoTexScrollEx` verbatim from this checkout. All 12 generated commands matched each native reference at frame/wrap boundaries; independent profile buffers, default-on behavior, disable/re-enable, frame reset, and transient-allocation reuse passed.
- The same-archive metadata-change regression failed before the cache fix and passed afterward.
- Both actual supplied O2Rs selected the same drawable profiles: the three ruins references plus an existing Lake paste in Market Day. Empty `recipe22` was skipped. Profile selection does not filter destination scenes.
- Existing stabilization suite passed after the change: 12 standalone binaries, 10 Python tests, streamed-audio runtime and night-combat bridge checks.
- New production runtime C++ compiled in the focused harness. This host has no CMake installation; a complete game/Windows build has not been claimed from these checks.
- Independent code review checked native parameter parity, direct display-list address resolution, graphics scheduling, and pointer lifetime. Its archive-cache finding was fixed and re-reviewed.

## Runtime acceptance checklist

Use one supplied O2R at a time because they override the same resource paths. No rewritten O2R is required for this probe.

- [ ] Ruins: Lake and Pool scroll using their respective native setups.
- [ ] Ruins: newer light sheet scrolls using native Lost Woods setup.
- [ ] All three coexist without changing one another or other scene materials.
- [ ] Repeat with the retextured-light O2R; preserve the vanilla second layer.
- [ ] Check the included 4096×4096 Lake replacement and 4096×2048 light replacement visually.
- [ ] Check 2K and Pool replacements, and the intended 4K fire-water artwork.
- [ ] Leave/re-enter room, reload scene, and test applicable age/time/header transitions.
- [ ] Check vanilla Lake Hylia, Kakariko Well, and Lost Woods light sheets.
- [ ] Check the previously working relocated Lake surface and collision/swimming.

Promote the minimal implementation only after these runtime checks. Keep ComboShip untouched until Shipwright is stable.
