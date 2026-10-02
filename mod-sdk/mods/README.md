# Your mods go here

Every folder in here with a `manifest.json` is built as a mod, with no CMake to write:

```
mod-sdk/mods/
└── lantern/
    ├── manifest.json
    ├── lantern.c          every .c and .cpp in the folder is compiled (except *.inc.c and assets/)
    └── assets/            packaged into the .o2r as-is (Name.rgba32.png / Name.ia4.png become textures)
```

The folder name is the mod's target name and package name (`lantern.o2r`). All SDK helpers (`z64items.h`,
`z64aiming.h`, `z64wheel.h`) are available. A mod that needs more control puts its own `CMakeLists.txt` in its
folder, calling `unbound_add_mod(...)`, and that is used instead.

Build and install every mod in here with one command, from the repository root:

```sh
python mod-sdk/build_mods.py --game "C:/Games/SoH Unbound"
```

See [Getting started](../docs/GETTING_STARTED.md).
