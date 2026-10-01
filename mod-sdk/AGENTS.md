# Unbound mods: rules for AI assistants

You are helping someone write a mod for Ship of Harkinian: Unbound. A mod is a standalone `.o2r` package that
talks to the game only through the ModApi and the game's exported C symbols. The game itself is not yours to change.

## What you may edit

- The mod's own folder: its sources, `manifest.json` and `assets/**`.
- The `unbound_add_mod(...)` entry for that mod in the CMakeLists.txt that builds it.

## What you must never edit

- The game: `soh/`, `libultraship/`, `ZAPDTR/`, `OTRExporter/`, the root CMake files.
- The SDK: `mod-sdk/cmake`, `mod-sdk/include`, `mod-sdk/tools`, the docs, other mods' folders.

If the mod needs something the game does not expose (a hook, a global, a table entry), do not patch the engine to
get it. Stop and tell the user exactly what is missing: which engine function or behaviour, what the mod would do
with it, and why the existing hooks cannot do it.

## Where the API lives (read, do not change)

- `soh/soh/ModApi/ModApi.h`: the `SOHModApi` table, `SOH_MOD_EXPORT`, `SOH_REGISTER_HOOK`,
  `SOH_REGISTER_HOOK_FOR_ID`, `SOH_MOD_API_HAS`.
- `soh/soh/Enhancements/game-interactor/GameInteractor_HookTable.h`: every hook name and signature.
- `soh/soh/Enhancements/game-interactor/vanilla-behavior/GIVanillaBehavior.h`: every `VB_*`, its default result
  and its arguments.
- `soh/soh/ModApi/**/*.h`: the registries (items, equipment, forms, actors, messages, randomizer).
- `mod-sdk/docs/*.md`: how each kind of mod works, and the engine traps already paid for.
- `mod-sdk/templates/*`: minimal working examples of an item, a form and an actor.

## How a mod is shaped

- Export exactly `ModSetApi`, `ModGetRequirements` and `ModInit` with `SOH_MOD_EXPORT`; everything else is
  `static`. List every hook the mod registers in `ModGetRequirements`.
- Guard newer table entries with `SOH_MOD_API_HAS(api, Entry)` before calling them.
- Keep the mod self-contained: effects, sounds and helpers live in the mod's sources. No shared helper libraries
  besides the SDK's `include/` headers.
- Keys are namespaced (`author.thing`) and never change once released. Ids (actors, messages, randomizer items)
  are asked for by key every session and never stored.
- Assets are referenced by `__OTR__` path in `static const ALIGN_ASSET(2) char` arrays; textures are never
  compiled into C.

## Checking without a full build

Syntax-check a C mod from the Unbound root without building anything:

```sh
clang -fsyntax-only -std=c11 -Wno-everything -Werror=implicit-function-declaration \
    -DF3DEX_GBI_2 -DNOMINMAX -DUNBOUND_MOD -DGBI_S32_VTX=1 -DGBI_FLOAT_MTX=1 -DCONTROLLERBUTTONS_T=uint32_t \
    -Isoh -Isoh/include -Isoh/src -Isoh/assets -Ilibultraship/include \
    -Imod-sdk/include/z64items -Imod-sdk/include/z64aiming -Imod-sdk/include/z64wheel path/to/mod.c
```

Build only mods, never the game: `cmake --build <mods build dir> --config Release --target <mod>`.
