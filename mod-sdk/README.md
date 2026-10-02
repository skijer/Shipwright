# Unbound Mod SDK

Build new items, transformations, actors and enemies for **Ship of Harkinian: Unbound** without touching the
game. A mod is one `.o2r` file: its compiled code plus its assets. Players drop it in `mods/` and it loads at
boot, next to every other mod.

```
mods/
└── my_mod.o2r
    ├── manifest.json            name, author, version, what it needs
    ├── bin/windows_x64/my_mod.dll
    ├── bin/linux_x64/my_mod.so  one binary per platform; the game loads its own
    ├── bin/darwin/my_mod.so
    └── textures/…, objects/…    your assets, at the paths your code asks for
```

## Start here

| I want to… | Read |
|---|---|
| Build my first mod and see it in game | [Getting started](docs/GETTING_STARTED.md) |
| Add an item: C-button tool, weapon, mask, upgrade, pickup | [Items](docs/ITEMS.md) |
| Turn Link into something else | [Forms](docs/FORMS.md) |
| Add a new actor or enemy to the world | [Actors](docs/ACTORS.md) |
| Make icons, models and animations for my mod | [Assets](docs/ASSETS.md) |
| React to the game: hooks, vanilla behaviours, time, input, audio, menus | [Hooks and services](docs/HOOKS.md) |
| Put my item in the randomizer, change its logic | [Randomizer](docs/RANDOMIZER.md) |
| Ship one `.o2r` for Windows, Linux and macOS from CI | [Publishing](docs/PUBLISHING.md) |
| Get my mod signed so release builds run it | [Signing](docs/SIGNING.md) |

## The short version

```sh
git clone --recursive <your fork of Unbound>
cp -r mod-sdk/templates/item_template mod-sdk/mods/my_item
python mod-sdk/build_mods.py --game "<folder with soh.exe and soh.lib>"
```

No compiler? Push to your fork and download the `unbound-mods` artifact of the **build-mods** workflow.

## What is in this folder

| Path | What it is |
|---|---|
| `mods/` | Your mods. Every folder with a `manifest.json` is built, no CMake needed. |
| `build_mods.py` | One command: build every mod and install it into the game's `mods/` folder. |
| `ci/build-mods.yml` | CI template for a mods repository of its own. |
| `templates/item_template` | A C-button item: a Heart Charm that trades rupees for hearts. |
| `templates/form_template` | A mask and the form it grants: a faster Link who leaps instead of rolling. |
| `templates/actor_template` | A new actor with a collider that bursts into rupees when hit. |
| `include/z64items` | Header-only helpers to fill a custom item definition. |
| `include/z64wheel` | Header-only in-game wheel for items with several modes. |
| `include/z64aiming` | Header-only first-person aiming for items that own the player. |
| `cmake/UnboundModSdk.cmake` | `unbound_add_mod()`: compiles a mod and packages it as `.o2r`. |
| `tools/pack_mod.py` | Packages one binary plus `assets/` into an `.o2r`. |
| `tools/merge_mod_packages.py` | Merges the per-platform packages of a mod into one universal `.o2r`. |
| `tools/otex.py` | Converts PNG images to game textures (and back). |

The templates are the smallest working example of each kind of mod. Copy one, rename its keys, and grow it.

## Real mods to read

The templates stay small on purpose. For complete, shipped mods built on this SDK (held items with their own
actions, projectiles, item wheels, full transformations with movesets, enemies for the enemy randomizer), see the
[`unbound-mod-nei` branch](https://github.com/skijer/Shipwright/tree/unbound-mod-nei/mod-sdk/mods). It is also
the reference layout for a fork whose mods live in `mod-sdk/mods/` and are built by the **build-mods** workflow.

## The rules of the road

- A mod only talks to the game through the ModApi table and the exported engine symbols. It never patches
  engine code. If something you need is missing, open an issue describing the engine function, what your mod
  would do with it and why the existing hooks cannot; the host grows generic hooks, never mod-specific ones.
- Name everything with a namespace you own: `author.item_name`, `author.enemy.name`. Keys are how the game
  saves your items, forms and flags, so they never change once released.
- A mod is self-contained: its effects, sounds and helpers live in its own sources, even if another mod has
  similar code. Mods do not link against each other; they can share functions through
  [services](docs/HOOKS.md#services-between-mods).
- Everything is keyed, nothing is numbered: actor ids, message ids and randomizer ids depend on which mods are
  installed, so ask for them by key every session and never store them.
