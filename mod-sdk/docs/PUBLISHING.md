# Publishing

## The manifest

```json
{
    "name": "lantern",
    "author": "Your Name",
    "version": "1.2",
    "description": "One sentence players see in the mod list.",
    "requires": [
        "misc/link_animetion/gPlayerAnim_pg_punchA_Data"
    ]
}
```

`name` is required and must be unique among mods. `requires` lists resource paths the mod needs from *other*
archives; without them the loader skips the mod and says why in the log. The packager adds `binaries` itself.

## One package for every platform

The loader reads `manifest.binaries[<platform>]` and loads only its own binary, so one `.o2r` can carry all of them:

```json
"binaries": {
    "windows_x64": "bin/windows_x64/lantern.dll",
    "linux_x64": "bin/linux_x64/lantern.so",
    "darwin": "bin/darwin/lantern.so"
}
```

Build the mod once per platform (each build writes a package with its own binary), then merge:

```sh
python mod-sdk/tools/merge_mod_packages.py windows/lantern.o2r linux/lantern.o2r macos/lantern.o2r \
    --output universal/lantern.o2r
```

The merge refuses packages built from different manifests or with different assets, so a stale build cannot slip
in. Ship the universal package: players on any platform install the same file.

## Building in a fork

The simplest setup: fork Unbound, put your mods in `mod-sdk/mods/`, push. The fork's **build-mods** workflow
builds them for all three platforms, taking `soh.lib` from the newest Unbound release, and uploads one universal
`.o2r` per mod as the `unbound-mods` artifact. See
[Getting started](GETTING_STARTED.md#without-a-local-compiler).

To release your mods, push a tag containing `mods` (e.g. `nei-mods-1.0`) on the branch that has them: the same
workflow builds them and publishes a GitHub release with one universal `.o2r` per mod. Tags containing `unbound`
are for the game itself: they build and release Unbound (with `soh.lib`), not the mods.

## Keeping your mods in a repository of their own

Once you have more than a few mods, keep them apart from the game: Unbound stays a clean fork you can update, and
your mods are a separate project that builds against a pinned Unbound release. The layout:

```
your-mods/
├── CMakeLists.txt
├── UNBOUND_REF                      the Unbound release tag your mods are built against
├── .github/workflows/build-mods.yml copied from mod-sdk/ci/build-mods.yml
└── mods/
    ├── lantern/
    │   ├── lantern.c
    │   ├── manifest.json
    │   └── assets/...
    └── ...
```

```cmake
cmake_minimum_required(VERSION 3.24)
project(YourMods LANGUAGES C CXX)

set(UNBOUND_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Shipwright-unbound" CACHE PATH "Unbound checkout")
include("${UNBOUND_ROOT}/mod-sdk/cmake/UnboundModSdk.cmake")

unbound_add_mod(lantern
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/mods/lantern"
    SOURCES mods/lantern/lantern.c
    LIBRARIES z64items z64wheel
)
```

Locally, `UNBOUND_ROOT` points at your Unbound checkout. In CI, the workflow checks Unbound out at `UNBOUND_REF`
and passes its path. Bumping `UNBOUND_REF` to a newer release is how you move your mods to a newer game; nothing
else changes. `unbound_add_mods_in("${CMAKE_CURRENT_SOURCE_DIR}/mods")` picks up every mod folder the same way
`mod-sdk/mods/` does, if you prefer not to list them.

### What CI does

`mod-sdk/ci/build-mods.yml`:

1. Builds every mod on Windows, Linux and macOS, against the headers of the `UNBOUND_REF` release. On Windows it
   downloads that release's `soh.lib` asset, so the import library always matches the game players run.
2. Merges the three packages of each mod into one universal `.o2r`, uploaded as the `mods-universal` artifact.
3. On a tag, attaches the universal packages to the GitHub release.

Edit `UNBOUND_REPOSITORY` at the top of the workflow if you build against a different Unbound fork.

The [`unbound-mod-nei` branch](https://github.com/skijer/Shipwright/tree/unbound-mod-nei/mod-sdk/mods) is a
working example of the fork layout above, with dozens of real mods.

## Before you release

- Keys never change after a release: items, forms, flags and options are saved by key.
- Check your pause placement against the mods you expect players to combine with yours.
- A new version may add fields your old saves did not have; read storage and flags defensively.
- Guard every table entry newer than the oldest game you support with `SOH_MOD_API_HAS`.
- Builds with the `signed` mod policy only run signed mods; the [sign-mod workflow](SIGNING.md) publishes
  packages they accept.
