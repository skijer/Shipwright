# PAK Loader for Unbound

ModLoader64 packs in Unbound: bodies, equipment pieces, voices and emotes. Open **Mods > PAK Loader** after
restarting Unbound.

The same mod exists for 2Ship (`2ship-unbound-mod-nei/mod-sdk/mods/pak_loader`). Both share `archive.h`, `model.h`,
`names.h`, `library.h`, `equipment.h`, `sounds.h`, `emotes.h`, `session.h`, `catalog.h`, `keep_textures.h`,
`vorbis.c` and `third_party/`; keep the copies identical.

## Page

- **Models and voices** (column 1): one model and one voice for Child Link, Adult Link and the `nei.fierce_deity`
  form. Abilities stay with the current form. With `mm.o2r` loaded, **Apply MM Child / MM FD / OoT Adult** selects
  the MM models. **Restore Defaults** clears models, voices, equipment and root adjustments.
- **Equipment and Items** (column 2): Kokiri/Master/Biggoron Sword, Deku/Hylian/Mirror Shield, Bow, Hookshot,
  Slingshot, Boomerang, Megaton Hammer, Fairy Ocarina and Ocarina of Time. Each lists every piece of its kind from
  every imported pack, bodies included ("Saria (OoT Adult) - Master Sword", "Claymore (SC3) (Biggoron Sword)").
- **Emotes** (column 2): choose one, then hold L and press D-Pad Down, or press **Play Emote**. Moving or acting
  stops it.
- **Pack Library** (column 3): import PAK/ZOBJ, install O2R/OTR, root height per appearance, status.

`.pak` and `.zobj` files anywhere under the mods folder, subfolders included, are read at start (host
`ReadModsFolderFiles`). **Import PAK / ZOBJ** copies the chosen file there under a plain name; hosts without that
row get it wrapped as `pakloader_import_<hash>.o2r` instead. Restart to update the lists.

## What each pack provides

- **Bodies**: OoT adult, OoT child and MM human ZOBJs in either game. Kind from the ZOBJ header.
- **Equipment**: Z64Online equipment ZOBJs (`EQUIPMANIFEST`) and the pieces inside every body (universal aliases
  or the zzplayas lookup tables). A held item is rebuilt as hand + pieces, as Z64Online's combined lists are; the
  hand is the selected body's own fist, else the game's. The sheath limb is rebuilt from sheath, sheathed hilt
  (back matrix) and back shield; parts not replaced keep the game's own lists.
- **Voices**: `sounds/<sfx id>/*.ogg`, one clip picked at random per effect. Player effects are silenced through
  `OnActorPlaySfx` and the clip is mixed through `AudioMix` (SFX group, "Voice Pack Volume").
- **Emotes**: `package.json` `emote` entry; raw link_animetion frames (134 bytes each) posed over Link at draw
  time, optional Ogg sound, optional loop.

## Names

Catalog names and authors come from `catalog.h`: `python gen_catalog.py.txt catalog.h models.json audio.json` with
`https://modloader64.com/assets/data/models.json` and `audio.json`. Other packs use their `package.json` name or a
cleaned-up file name. `keep_textures.h` comes from `gen_keep_textures.py.txt` and OoT's N64 NTSC 1.0
`gameplay_keep.xml`.

## Formats

- zzplayas: skeleton at `0x500C`, or the fixed `0x5380` / `0x53A8` / `0x5420` (adult / child / MM) when exporters
  blank it. Body kind at `0x500B`: 0 adult, 1 child, 4 or 0x68 MM human. Back matrices at `0x5010`.
- Z64O universal: skeleton section at `0x501C`, kind at `0x501B`, 256 aliases from `0x5020`, back matrices at
  `0x5890` (swords) and `0x5950` (shields).
- Segment `0x0C` (cull list) passes through. Segment 4 textures are OoT `gameplay_keep` textures by offset; a
  texture the running game lacks is drawn white.
- Matrix references (`0x0D`) are renumbered: exporters count the model's own meshed limbs, SkelAnime the base
  skeleton's. Meshes on joint-only limbs (root, lower, upper) are dropped.

## Boundaries

Items drawn by other actors (flying boomerang, hookshot chain, arrows, deku stick, bottles) keep the game's models.
Other forms (Deku/Goron/Zora ZOBJs) and PlayAs work files are skipped. O2R/OTR archives replace resources as a
whole. The mod changes the local player only.

Ship's menu throws on `SOH_MOD_MENU_TEXT`; text rows are custom widgets.

Fierce Deity lazily asks these optional services, regardless of mod load order:

- `nei.pak_loader.fd_body.v1`: `bool(Player*, int32_t, Gfx**, Gfx*, Vec3f*)`.
- `nei.pak_loader.fd_face.v1`: `bool(const char**, const char**)`.
- `nei.pak_loader.fd_active.v1`: `bool(void)`; suppresses FD's additional original sword drawing.

## Validation

The shared library was run (MSVC, AddressSanitizer) over the ModLoader64 catalog: 263 body packs, 46 equipment
packs, 19 emotes and sample voice packs all import (549 bodies, 3459 pieces), and every voice decodes. In-game
drawing, voices and emotes remain to be checked.
