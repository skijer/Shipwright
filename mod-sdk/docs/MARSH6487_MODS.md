# marsh6487/Shipwright: mod catalogue

Survey of the 66 feature branches of [marsh6487/Shipwright](https://github.com/marsh6487/Shipwright), measured
against what the Unbound ModApi can do today. Their work is engine patches (`z_player.c`, `z_en_horse.c`,
`z_draw.c`, …) on top of `unbound` / `unbound-nei`; here each feature is classified by whether it can live in a
standalone `.o2r` or needs a new host hook first. The branches are mostly stacked: the tip
`feat/nei-gi-upgrade-recovered-20260927` carries ~200 commits over `unbound-nei`, and the `codex/*` branches are
earlier POCs of the weather / static-actor work that the later branches absorbed.

Status: **done** = in `mod-sdk/mods/`, syntax-checked; **portable** = doable with the current ModApi;
**needs host** = needs an engine hook or table entry the ModApi does not have yet (per `mod-sdk/AGENTS.md`, ask
for it, do not patch the engine from a mod); **not a mod** = engine, build or tooling work.

## Features

| # | Mod (proposed key) | Source branch / commit | What it does | Status |
|---|---|---|---|---|
| 1 | `chest_size_matches_contents` | `feat/nei-gi-upgrade-…` d676a5ff | Small chests for junk/keys/tokens, large for the rest. | **done** (size and focus; the opening light and ice-smoke dimensions of the original are not reproduced) |
| 2 | `young_epona` | `poc/child-epona-song-riding`, `feat/nei-young-epona` (bcb0450e) | Child Link rides Epona once the song is learned. | **needs host**: rewrites `z_horse.c`, `z_en_horse.c`, `z_player.c` mount logic and adds a save field. Needs mount/dismount VBs, a horse-object override and per-file save storage for the riding flag. |
| 3 | `midna_navi` | `poc/midna-navi-soh-poc2` (f889527c…822f46b1) | Midna replaces Navi (model, blink, yawn, private audio). | **portable**: `OnActorDraw` for `ACTOR_EN_ELF` with `drawVanilla=false`, plus `RegisterAudioMixInGroup`. Needs the model assets packed into the `.o2r`. |
| 4 | `din_fire_shield` / `din_fire_sword` | `poc/transform-cosmetics-…` line, d54391b5…628deea3 | Din's Fire flames on shield and sword, optional fire damage and charge SFX. | **portable**: `OnPlayerPostLimbDraw` + `OnCollisionResolveDamage`; HD textures go in `assets/`. |
| 5 | `custom_heart_magic_cosmetics` | `poc/heart-magic-cosmetics` (8481c257) | Cosmetic Editor colours for custom heart and magic models. | **needs host**: edits `z_draw.c`/`z_en_item00.c` colour paths; needs an `OnItemDrawColor`-style hook. |
| 6 | `transform_cosmetics` + `zora_shield_anchor` | `poc/transform-cosmetics-20260921`, `fix/zora-shield-anchor-20260922` | Cosmetic Editor entries for transformations, MM-sized Zora shield anchored at the feet and following the swim pose. | **portable** as a patch to the existing `zora`/form mods (`OnPlayerPostLimbDraw`). |
| 7 | `rpg_stat_pickups` | `poc/soh-stat-items-20260923` (0574f79c, 119 files) | Stat upgrade items (strength, etc.) that scale pushing, rolling, etc. | **portable in part**: items via `Z64Items_Register`; the stat effects on pushable actors need `OnActorUpdate` per actor id. |
| 8 | `spin_reimagined` | `poc/spin-reimagined-20260921` | HD sampling and medallion/ice/fire spin and arrow textures. | **needs host**: custom `ResourceFactory` for `SpinEffectTexture`. |
| 9 | `lost_woods_time_pedestal` | `feat/lost-woods-materials-time-pedestal`, `poc/pedestal-*`, `fix/pedestal-*`, `fix/lost-woods-exit-access-…` | Sage motion, non-logic Time pedestal in Lost Woods, local sword skips and exit animations. | **needs host**: edits `Bg_Toki_Swd`, `z_play.c`, `z_sram.c`; needs scene-actor replacement and a fixed-camera service. |
| 10 | `reusable_water_caustics` | `feat/reusable-water-temple-caustics` | Caustic motion for Water Temple and Zora's Domain. | **needs host**: native material scroll binding (`probe/prelude-native-materials`). |
| 11 | `weather_audio` | `codex/concurrent-weather-*`, `codex/poc5…poc12-weather-*`, `poc/lost-woods-rain-small-fixes` | Proximity rain/thunder, authored rain ownership, mixer controls. | **portable in part**: audio via `RegisterAudioMixInGroup`, thunder via `OnSceneInit`/`OnGameFrameUpdate`; the nature-channel guards in `code_800EC960.c` need host work. |
| 12 | `hyrule_field_night_music` | `codex/hyrule-field-night-audio-poc5` | Night music in Hyrule Field, bridged to enemy BGM handoff. | **portable**: `OnSeqPlayerInit` + VB for BGM handoff. |
| 13 | `static_story_actors` (Zelda, Ruto, Great Fairy, Impa, Ganon, Saria/Malon, Kokiri) | `codex/static-story-actors-poc*`, `codex/poc6…poc14-*`, `fix/static-*` | Scene-viewer story actors with tracking and dialogue. | **portable** as actors (`ACTORS.md`), with MM/OoT assets in the package. |
| 14 | `mm_catalogue_actors` (Skull Kid + Tael, Anju umbrella, Kafei, HMS Keaton, Lulu, Shop Gal, Great Fairy blink) | `design/mm-catalogue-fountain`, `work/mm-*`, `fix/mm-*`, `fix/shop-gal-*`, `fix/lulu-*`, `feat/skull-kid-3ds-tael`, `feat/anju-umbrella-kafei` | MM NPCs in OoT scenes, bomb shop lady replaced by the treasure shop gal. | **portable** as actors; needs the player's private MM archive (strict texture binding) and HD blink heads. |
| 15 | `young_fado_npc` | `poc/young-fado-npc-cloth-reviewed-20261002` (cd54719c) | Young Fado rig and seated cloth routing in Kokiri forest. | **needs host**: edits `En_Ko` model routing; portable once `OnActorDraw` covers it. |
| 16 | `shadow_scepter_fix` | `poc/shadow-scepter-visibility-20260930` (7ba95d89) | Shadow Scepter visibility and homing stun targeting. | **portable** as a patch to `elemental_wand` (wand_shadow). |
| 17 | `nei_gi_models` + rod effects | `feat/nei-gi-upgrade-recovered-20260927`, `bea6f438`…`2ce2e5a2` | Upgraded GI models, held-model fitting, rod effects, hidden rods in first person. | **portable** as asset replacements and patches to the existing NEI mods (200 files, mostly `tools/nei_gi` sources). |

## Not mods

| Branch | Why |
|---|---|
| `poc/soh-performance-port-20260928` | ComboShip CPU optimisations in the renderer (`gfx_*`); engine. |
| `fix/pak-menu-selection-and-back-equipment` | `pak_loader` menu code; host feature. |
| `poc/nei-hint-name-fallback-20260928` | Randomizer hint text fallback in the host. |
| `poc/pedestal-camera-build-fix`, `codex/fix-*-test/red`, `test/adcdb7d-base-*`, `codex/concurrent-weather-base-*` | Build fixes, regression fixtures and base markers. |

## Base branch

The request named `unbound-modapi-nei`; no branch has that name. `marsh6487-mods` is based on `unbound-mod-nei`
(`unbound-modapi` plus the 60 NEI mods and the SDK), the reference layout for a fork whose mods live in
`mod-sdk/mods/`.
