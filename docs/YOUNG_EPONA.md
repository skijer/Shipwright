# Ride Young Epona as Child

Enable **Enhancements → Quality of Life → Ride Young Epona as Child** and reload the area. The option defaults off. Child Link must know Epona's Song and have an ocarina; randomized ocarina-note requirements are respected. Play the song in Hyrule Field, Lake Hylia, Gerudo Valley, Gerudo's Fortress or Lon Lon Ranch to summon young Epona, then mount her with the action button.

## Required animation pack

Install `Young_Epona_SoH_POC1_Assets.o2r` in the mods folder. Without all eight resources the feature stays unavailable. The pack supplies four MM horse animations and two child mounting clips plus their data. The model and other movement animations come from OoT's native young Epona resources. No model replacement, cosmetic pack, or voice pack is required.

The existing pack's SHA-256 is `d1990518522c4b781a9b62f3b2df21e9aec5df0eae99542892093359ecab945d` (19,296 bytes). Game asset bytes are not included in this source change.

The included converter reproduces the pack from extracted donor archives:

```sh
python3 scripts/diagnostics/build_young_epona_assets.py \
  --mm /path/to/mm.o2r --oot /path/to/oot.o2r \
  --output /path/to/mods/Young_Epona_SoH_POC1_Assets.o2r
```

The converter currently accepts only the verified archive revisions pinned in `EXPECTED_SHA256`. It deliberately rejects other archive hashes; supporting another extraction requires checking its resource formats and animation compatibility before updating those pins. It writes a manifest next to the output and opens donor archives read-only.

## Behavior

- Reuses the existing horse controller, horse-blocked surfaces, fence-jump decisions and landing behavior, with the young model, child mounting animations and rider alignment.
- Keeps young Epona's saved position separate from adult Epona and does not grant adult Epona ownership. Old saves and changing save files clear missing child-horse data.
- Restricts the young riding actor to the child timeline, including live age changes. Existing adult Epona, Ingo's horse and Master Cycle paths remain available.
- Enables mounted scene entry and saved-position restoration through a child-only outdoor allowlist: the original five horse areas, Kakariko, Graveyard, Zora's River, Kokiri Forest, Sacred Forest Meadow, Zora's Fountain, Lost Woods, Desert Colossus, Haunted Wasteland, Hyrule Castle, Death Mountain Trail/Crater, outside Ganon's Castle, and the Market/Temple of Time exterior and alley variants. This is 29 scene IDs; collision and scene geometry still determine traversable routes.
- Fresh song summons use the five native horse areas. The other 24 allowlisted scene IDs have no native summon points; enter them while mounted or restore a horse previously saved there. Expanding fresh summons needs separate placement work.
- Loads the child horse object bank in scenes that do not already contain it. The native ranch NPC remains until the summoned riding horse is active.
- Includes a shared material lookup fix for cold Alt display lists replacing cached native textures, retaining both cache entries across Alt toggles.

## Automated checks

Initialize the `libultraship` submodule and install a C/C++ compiler, Python 3 and the normal `nlohmann/json.hpp` dependency. Run the six commands in `.github/workflows/young-epona.yml`.

The fixtures execute production spawn, actor, player and save functions. They cover access gates, scene entry, rider attachment, collision/jumping, camera handoff, dismounting, age changes, legacy saves and JSON round trips. Five affected C translation units also receive syntax checks against the real headers. Alt material tests use the pinned production cache logic. Donor-independent asset tests validate binary bounds and resource references; actual donor verification is optional and requires both archives.

## NEI gameplay validation

This extraction needs gameplay testing on NEI. Automated fixtures and syntax checks do not establish in-game visual or scene-transition correctness. Check song summon, mount/ride/jump/dismount, saving/reloading and mounted scene round trips, including Hyrule Field ↔ Kakariko. Compare vanilla and Alt Assets eyes, verify disabled/missing-pack behavior and confirm no young horse appears as adult. Outdoor allowlist coverage is not a claim that every route or scene has been tested in game.
