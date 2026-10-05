# marsh6487/Shipwright: mod catalogue

Survey of the 66 branches of [marsh6487/Shipwright](https://github.com/marsh6487/Shipwright) and its review PR
[skijer/Shipwright#17](https://github.com/skijer/Shipwright/pull/17), and what each became here. Their work is engine
patches (`z_player.c`, `z_en_horse.c`, `z_draw.c`, ...) for ComboShip; here each feature is a standalone `.o2r` built
on the Unbound ModApi. Credits and provenance notes: [CREDITS_MARSH6487.md](CREDITS_MARSH6487.md). These mods are
marsh6487's. Skijer's items, mods and branches are not modified; the mods that extend NEI items sit on top of them
through hooks.

Everything below builds to an `.o2r` on Linux with `python mod-sdk/build_mods.py` and passes
`tools/scan_mod_sources.py`. **None of it has been run in the game.**

## Mods in `mod-sdk/mods/`

| Mod | What it does | Original | Notes |
|---|---|---|---|
| `young_epona` | Ride Epona as a child | `poc/child-epona-song-riding`, `feat/nei-young-epona` | Own actor whose struct is an `EnHorse`: the engine riding code works unchanged and the vanilla controller is its brain. Child skeleton; pose re-evaluated from OoT child clips and the MM clips of `mm.o2r`, by normalised time. Mount clips from `mm.o2r`. Remembers the horse per save file, rides through scene exits. Fresh summon: gallops in from the native spawn points in four areas, elsewhere appears in front of Link. |
| `midna_navi` | Midna in place of Navi | `poc/midna-navi-soh-poc2` | Needs the Midna model pack (`objects/midna_navi/**`); voice clips optional (`custom/samples/midna_*`). Navi's HUD call and talk laugh are not routed through her actor, so stay vanilla. |
| `din_fire_equipment` | Din's Fire on shield and sword, optional fire damage | Din fire POCs | Flames are vanilla fire particles (no HD flame meshes, no pack). Fire damage reproduces the original's rules through `OnCollisionResolveDamage`; default off, as in the original. |
| `heart_magic_cosmetics` | Cosmetic Editor colours on custom heart and magic models | `poc/heart-magic-cosmetics` | Tint injected from the draw hooks instead of patching `z_draw.c`. |
| `chest_size_matches_contents` | Chest size follows contents | `feat/nei-gi-upgrade-...` d676a5ff | Size and focus; the opening light and ice-smoke sizes are not reproduced. |
| `stat_upgrades` | Power, Defense, Speed, Climb, Crawl, Push, Quarter Heart | `poc/soh-stat-items-20260923` | Shuffled custom items with the original models and icons. Climb and Push borrow the game's own ClimbSpeed / FasterBlockPush options while the movement lasts. The magic stat is not ported. |
| `nei_gi_redesign` | Redesigned get-item models for 20 NEI items | `nei_gi_redesign`, `NeiGiPresentation.cpp` | Matched by the draw function each item registered. Shimmer / energy orbs and shop-shelf fitting not ported. |
| `nei_held_redesign` | Redesigned held Fire/Ice/Light Rods and Gust Jar | `nei_held_redesign`, `NeiHeldPresentation.cpp`, `equip_helper.c` | The item's own held draw stands down while the model draws on the captured hand matrix. Other held components need their items' internal state. |
| `hyrule_field_night_music` | Night music in Hyrule Field | `HyruleFieldNightMusic` | One state machine over the vanilla sequence commands; track chosen in the menu. |
| `outdoor_rain` | Continuous rain in the Lost Woods, optional rain across outdoor areas | `GlobalOutdoorRain`, `poc/lost-woods-rain-small-fixes` | Writes the engine's rain density; private rain/thunder audio, rain colour, overcast and lightning not ported. |
| `story_npcs` | Static NPCs: Malon (both), Saria, Darunia, Nabooru, Impa, Sheik, Ruto (both), Keaton and Anju (from `mm.o2r`) | `codex/static-story-actors-*`, `codex/poc6..poc14-*` | Same params as the catalogue (0x7Fxx / 0x7Exx); an `En_Viewer` placed with one gets the NPC instead. No head tracking, talking or collision yet. |

A patch for Skijer's `elemental_wand` (Shadow Scepter homing and visibility, `poc/shadow-scepter-visibility-20260930`)
is in `docs/patches/` and is not applied.

## Not ported, and why

| Branch / feature | Reason |
|---|---|
| Rod charge/release revisions, projectile interpolation, first-person rod hiding, Whip / Switch Hook / Beetle / Ball and Chain articulated parts, Shovel, Spinner, Deku Leaf, Somaria, Mitts held models | They change or depend on the internal state of Skijer's items (swing, charge, extension). Those are in the package for when the items expose that state. |
| `feat/reusable-water-temple-caustics`, `probe/prelude-native-materials`, Lost Woods materials and time pedestal (`feat/lost-woods-materials-time-pedestal`, `poc/pedestal-*`, `fix/pedestal-*`, `fix/lost-woods-exit-access-*`) | Part of the Prelude scene-pack pipeline (native material scroll bindings in scene archives, a pedestal in a custom Lost Woods scene). They only mean something with those scene packs. |
| `poc/spin-reimagined-20260921` (HD spin / TorchFlame sampling, medallion arrows and spells) | A resource-factory change plus art packs. |
| `poc/transform-cosmetics-20260921`, `fix/zora-shield-anchor-20260922` | Cosmetic Editor entries; the Zora barrier of Skijer's `zora` mod is already anchored to the player. |
| Skull Kid + Tael, Kafei, Lulu, Shop Gal, Great Fairy blink heads, Zelda, Kokiri, Fado, Ganondorf, Young Fado NPC | Per-character draw code and the 3DS / HD model packs; the rest of the catalogue (`story_npcs`) is table-driven and can take them. |
| `poc/soh-performance-port-20260928`, `fix/pak-menu-selection-and-back-equipment`, `poc/nei-hint-name-fallback-20260928` | Renderer, `pak_loader` and randomizer-text changes inside the host. |
| Test, red/green and base-marker branches | Build and regression fixtures. |

## Base branch

The request named `unbound-modapi-nei`; no branch has that name. `marsh6487-mods` is based on `unbound-mod-nei`
(`unbound-modapi` plus the 60 NEI mods and the SDK).
