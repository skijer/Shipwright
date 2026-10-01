# Assets: creating the files your mod ships

Your mod's resources are files under `<mod>/assets/`. The packager puts each one into the `.o2r` at the same
relative path, and your code refers to it by that path with the `__OTR__` prefix:

```
my_mod/assets/textures/my_mod/gLanternIconTex      →  "__OTR__textures/my_mod/gLanternIconTex"
my_mod/assets/objects/object_lantern/gLanternDL    →  "__OTR__objects/object_lantern/gLanternDL"
```

```c
static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/my_mod/gLanternIconTex";
```

Declare paths as `ALIGN_ASSET(2)` arrays (from `align_asset_macro.h`): the renderer tells a path from a pointer by
its alignment. Pass these paths straight to `gSPDisplayList`, `gDPLoadTextureBlock`, `LinkAnimation_Change` and
the item definition; the engine resolves them.

**Put everything under a folder named after your mod** (`textures/my_mod/`, `objects/object_my_mod_*/`). Archives
are layered, the last one mounted wins, so a resource at a path another mod or the base game also uses replaces
theirs. That is how texture packs work, and it is almost never what a mod wants by accident.

Players' HD texture packs can replace your textures too, because they are loaded by path.

## Textures

Drop a PNG named `Name.<format>.png`; the packager converts it and stores it as `Name`:

```
assets/textures/my_mod/gLanternIconTex.rgba32.png   →  textures/my_mod/gLanternIconTex
assets/textures/my_mod/gLanternNameTex.ia4.png      →  textures/my_mod/gLanternNameTex
```

| Use | Size | Format |
|---|---|---|
| Item icon (pause menu, buttons, textbox) | 32×32 | `rgba32` |
| Item name on the pause screen | 128×16 | `ia4` (white text; the shadow comes from the engine) |
| Model textures | powers of two, as your material expects | `rgba32` |

To convert by hand or look at an existing texture: `python tools/otex.py encode icon.png gLanternIconTex
--format rgba32` and `python tools/otex.py decode gLanternIconTex icon.png`. A plain `.png` without a format
suffix is packaged untouched.

Vanilla textures are already there to reuse by path: `__OTR__textures/icon_item_static/gItemIconBottleFairyTex`,
`__OTR__textures/item_name_static/gKeatonMaskItemNameENGTex`… Their names are in `soh/assets/textures/**/*.h`.

**Never compile a texture into your C code** (Fast64's `u64 tex[] = { 0x... }` arrays). Those literals are
big-endian N64 data; compiled for a PC they come out byte-swapped and draw as noise. Ship the texture as an asset
and point the symbol at its path instead.

## Models (display lists)

Models are XML resources. A display list:

```xml
<DisplayList Version="0">
    <LoadVertices Path="objects/object_lantern/gLanternVtx" VertexBufferIndex="0" VertexOffset="0" Count="6"/>
    <Triangles2 V00="0" V01="1" V02="2" Flag0="0" V10="3" V11="4" V12="5" Flag1="0"/>
    <EndDisplayList/>
</DisplayList>
```

and its vertices:

```xml
<Vertex Version="0">
    <Vtx X="-188" Y="176" Z="-7" S="-5" T="-134" R="242" G="29" B="133" A="255"/>
    ...
</Vertex>
```

`R G B` are a vertex colour, or the normal (as signed bytes) when the material turns `G_LIGHTING` on.
Materials are display lists too (`SetCombineLERP`, `SetGeometryMode`, `LoadTextureBlock` pointing at a texture
path, `SetPrimColor`), called with `<CallDisplayList Path="…"/>`. The authority on every tag and attribute is
`libultraship/src/fast/resource/factory/DisplayListFactory.cpp`.

You rarely write these by hand:

- **Blender + Fast64**: model in Blender, export for SoH/Ship, and copy the resulting `objects/<object>/` XML
  files into `assets/objects/<object>/`. Paths inside the XML already point at their final place in the archive.
- **Vanilla models**: every OoT object is available by path, e.g.
  `__OTR__objects/object_link_child/gLinkChildKeatonMaskDL`. Names are in `soh/assets/objects/**/*.h`.

Draw a model from the item, actor or hook that owns it:

```c
OPEN_DISPS(play->state.gfxCtx);
Gfx_SetupDL_25Opa(play->state.gfxCtx);
gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sLanternDL);
CLOSE_DISPS(play->state.gfxCtx);
```

A lit model with no texture needs just a combiner of `PRIMITIVE * SHADE`, `G_LIGHTING | G_SHADE |
G_SHADING_SMOOTH | G_ZBUFFER`, normals in the vertices, and `SetPrimColor` between triangle groups for colours.
Translucent parts go in a second display list drawn after the opaque one, or they hide what is inside them.

Vertices and display lists *can* be compiled as C (Fast64's `.c` export), since those are native structs; only
textures cannot. The SDK compiles with the game's own `GBI_S32_VTX` and `GBI_FLOAT_MTX`, so the sizes match.

## Link's animations

A Link animation is a raw `PlayerAnimation` resource: a 0x40 header (magic `MAPO`), then a little-endian `u32`
count of values (frames × 67), then the values as little-endian `s16`. Load it with:

```c
LinkAnimationHeader* dig = ResourceMgr_LoadPlayerAnimAsHeader("__OTR__misc/link_animetion/gPlayerAnim_my_dig");
LinkAnimation_PlayOnce(play, &player->skelAnime, dig);
```

Do not cast a custom animation's path to `LinkAnimationHeader*`: that reads frame data as the frame count.
Vanilla animations are the other way round: pass their path cast to `LinkAnimationHeader*`
(`(LinkAnimationHeader*)"__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_jump"`).

## Actor skeletons and animations

A custom actor with a skeleton (see `SkelAnime_InitFlex` in any vanilla enemy) ships its skeleton, limbs, meshes
and animations from the Fast64 export like any other object, or compiles the Fast64 `.c` export into the mod
with its textures redirected to asset paths as above.

## Text

Mod texts are registered from code (`RegisterMessage`, the item text fields); see [Actors](ACTORS.md#texts). A mod
can also ship `text/<language>/messages.json`, merged with the game's texts, but those ids are fixed and two mods
that pick the same id overwrite each other.

## Music and sound

- **Sequences**: ship `custom/music/<name>` (and `custom/fonts/`, `custom/samples/`), then ask for its id by path
  with `GetSequenceId(path)`; ids are positional over every mounted archive, so never hard-code one. Add
  sequences or soundfonts after boot with `RegisterSequence`/`RegisterSoundFont`, from the `OnAudioTablesReady`
  hook or later.
- **Sound effects**: a mod cannot add a new sfx id (the sfx tables are compiled and dispatched by a sequence). Play
  your own samples with the audio mixer ([Hooks](HOOKS.md#audio)), or replace a vanilla sample by shipping it at the
  same path.

## Assets from other games

A mod that uses resources it does not ship (Majora's Mask animations or models extracted from the player's own
ROM by another mod) lists them in its manifest; if any is missing the mod is not loaded and the log says which:

```json
"requires": [
    "misc/link_animetion/gPlayerAnim_pg_punchA_Data"
]
```

For content that is only *nice to have*, check at runtime instead: `sApi->HasResource(path)`.

The host can run its built-in ZAPD over a ROM for a mod that brings the XML description of that ROM
(`ExtractRom`), and write files from mounted archives to disk for tools (`ExportArchiveFiles`). ROM data is never
distributed: only the XML offsets are.
