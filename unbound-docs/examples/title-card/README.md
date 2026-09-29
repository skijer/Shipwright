# Example: a custom scene's title card

Registers a custom scene that reuses Kokiri Forest's converted scene document and gives it Hyrule Field's
place-name card, using only vanilla resources. It is the test fixture for SPEC §7.1 `titleCardTexture`.

| Entry | What it exercises |
|---|---|
| `example/forest_copy` | a custom scene (id 128 or later) with `titleCardTexture`: no case of the game's own title-card table names it |
| `titleCardTexture` | a path written without the `__OTR__` prefix, as SPEC writes it |
| `entrances.main` | `showTitleCard`, so arriving shows the card |

Package with any zip tool, keeping the paths, and drop the result in SoH's `mods/` folder:

    cd title-card && zip -r ../title-card.o2r unbound.json unbound

## Expected

- The log registers `example/forest_copy` with its entrance.
- Arriving through `example/forest_copy/main` (the Better Debug Warp screen lists it, or the console's
  `entrance example/forest_copy/main`) shows Kokiri Forest with "Hyrule Field" on the place-name card, in every
  language.
- Without the mod, nothing changes.
