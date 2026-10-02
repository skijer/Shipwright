# Getting started

From nothing to the Heart Charm template in your inventory.

You never need to build the game to build a mod. You need the game's source for its headers, and on Windows the
`soh.lib` that comes with the game.

## 1. What you need

- A checkout of this repository (a fork is fine), with submodules: `git clone --recursive`.
- The game itself: download an Unbound release. On Windows the release zip has `soh.lib` next to `soh.exe`; it is
  also attached to every release as its own asset.
- CMake 3.24+ and Python 3 with Pillow (`pip install pillow`).
- A C compiler: Visual Studio 2022 (Windows), GCC or Clang (Linux), Xcode command line tools (macOS).

On Windows a mod links against `soh.lib`, the import library of `soh.exe`. Use the `soh.lib` of the same release
you play with. On Linux and macOS nothing is linked: a mod's calls are resolved against the running game when it
loads.

## 2. Build and install, in one command

From the repository root:

```sh
python mod-sdk/build_mods.py --game "C:/Games/SoH Unbound"
```

It builds every mod in `mod-sdk/mods/` plus the three templates, and copies each `.o2r` into the game's `mods/`
folder. `--game` is the folder with `soh.exe` (and `soh.lib`) on Windows, or the folder the game reads `mods/`
from on Linux and macOS.

| Option | Use |
|---|---|
| `--mod lantern` | Build only that mod (repeatable). |
| `--no-templates` | Skip the three templates. |
| `--soh-lib <path>` | Windows: `soh.lib` when it is not in the game folder. |
| `--config Debug` | Debug build of your mods. |

Without `--game` the packages stay in `build/mod-sdk/packages/Release/`.

Prefer plain CMake? The script only runs:

```sh
cmake -S mod-sdk -B build/mod-sdk -DCMAKE_BUILD_TYPE=Release -DUNBOUND_GAME_DIR=<game> -DUNBOUND_IMPORT_LIBRARY=<game>/soh.lib
cmake --build build/mod-sdk --config Release
```

### Without a local compiler

Push your mod to your fork: the **build-mods** workflow (`.github/workflows/build-mods.yml`) runs on every push or
pull request that changes `mod-sdk/mods/`, builds every mod for Windows, Linux and macOS, and uploads the
`unbound-mods` artifact with one universal `.o2r` per mod. On a pull request it also lists every mod in the
description, with a link to the run. Download the artifact from the run's page and drop the files into `mods/`.
Pull requests that change the game instead run **generate-builds**, which builds Unbound itself and links the
builds in the pull request. Run it by hand from the
Actions tab to pick another release's `soh.lib` (`release_tag`) or another repository's releases
(`release_repository`; the repository variable `UNBOUND_RELEASE_REPOSITORY` changes the default).

## 3. Try it

Load a save file and open the console (Developer Tools > Console in the menu bar):

```
give custom_item "template.heart_charm"
equip custom_item "template.heart_charm" cleft
```

Take some damage, press C-Left, and 10 rupees become two hearts. The item also shows up on the first extra pause
page of the pause menu (press L on the item screen, or Z with the GameCube page switcher), where it can be equipped like any vanilla item.

The other two templates:

```
give custom_item "template.swift_mask"
spawn custom_actor "template.rupee_sprite"
```

## 4. Make it yours

1. Copy `mod-sdk/templates/item_template` to `mod-sdk/mods/lantern`.
2. Rename the `.c` to `lantern.c`, the `name` in `manifest.json`, and every key
   (`template.heart_charm` → `yourname.lantern`).
3. Run `python mod-sdk/build_mods.py --game <game> --mod lantern`.

Every folder in `mod-sdk/mods/` with a `manifest.json` is a mod: all its `.c`/`.cpp` files are compiled (except
`*.inc.c` and `assets/`) with every SDK helper available. A mod that needs more control ships its own
`CMakeLists.txt` calling `unbound_add_mod(...)`. To keep your mods in a repository of their own instead, see
[Publishing](PUBLISHING.md).

## 5. Anatomy of a mod

Every mod exports exactly three C functions and keeps everything else `static`:

```c
static const SOHModApi* sApi;

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    ...register items, forms, actors, hooks...
}
```

- **`ModSetApi`** hands you the API table. Keep the pointer.
- **`ModGetRequirements`** lists, by name, every hook the mod registers. A game that lacks one of them refuses to
  load the mod and says which in the log, instead of crashing later.
- **`ModInit`** runs once per boot, after the game and before the title screen. Register everything here.
  It does not run again when the player loads a preset or a save.

The table only grows at the end. A mod built against a newer SDK can still run on an older game if it checks
before calling anything recent:

```c
if (SOH_MOD_API_HAS(sApi, RegisterActor)) {
    sApi->RegisterActor(&definition);
}
```

Hooks are registered by name and typed at compile time:

```c
static void OnSceneStart(int16_t sceneNum) {
}

SOH_REGISTER_HOOK(sApi, OnSceneInit, OnSceneStart);
SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_ARROW, WatchArrow);
```

Every hook name and signature is in `soh/soh/Enhancements/game-interactor/GameInteractor_HookTable.h`.

## 6. Calling the game directly

Every plain C function and global of the game is exported, so a mod calls the engine the same way vanilla actors
do: `Actor_Spawn`, `Player_SetupAction`, `Audio_PlaySoundGeneral`, `gSaveContext`, `gPlayState`, `CVarGetInteger`…
Include `functions.h`, `variables.h` and `macros.h`. Functions that `functions.h` does not declare (many
`z_player.c` internals) are still exported: declare them in your mod with the signature from the source.

The table is only for what the host must arbitrate between mods that do not know each other: registries,
owners and priorities.

## 7. When it does not load

- Everything the loader decides is in the game log (`logs/` next to the executable): which mods loaded, which
  hook was missing, which resource a `requires` entry could not find.
- Log from your mod with `sApi->Log("...")` (or `LUSLOG_INFO` from `<libultraship/log/luslog.h>`).
- `LNK2019 __imp_<name>` on Windows means that global is not exported to mods. Only globals declared
  `extern HOST_DATA` in `soh/include/variables.h` are. Ask for it to be added; do not work around it.
- The loader caches its checks in `mods/.unbound-modapi-cache.json`. Deleting it is always safe.
