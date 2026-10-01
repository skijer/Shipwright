# ModApi host

The game side of the mod SDK. Modders read `mod-sdk/`; this page is for whoever changes the host. Every rule below
exists because breaking it broke something.

## How mods get in

- `ModLoader` mounts every `mods/*.o2r`, reads `manifest.json`, extracts `binaries[<platform>]`, checks the
  declared hooks and `requires`, then calls `ModSetApi` → `ModGetRequirements` → `ModInit`. The hook check is
  cached per binary and host catalogue in `mods/.unbound-modapi-cache.json`; `requires` is never cached, so a
  package added between two boots is picked up.
- `ModApi_Init()` runs once, from `OTRGlobals`, after `ShipInit::InitAll()`. Never register mod boot work through
  `RegisterShipInitFunc`: `InitAll` re-runs on every preset apply and would load mods twice. The menu builds before
  mods load, so late menu registrations apply immediately.
- `SOHModApi` is an ABI. **Append only**: removing or reordering a row shifts every later offset and silently
  breaks every compiled mod. Mods test rows with `SOH_MOD_API_HAS`.
- Definition structs (`SOHCustomItemDefinition`, `SOHFormDefinition`, `SOHActorDefinition`…) start with
  `structSize` and only grow at the end; registries copy `min(structSize, sizeof)` so an older mod hands over a
  shorter block safely.

## Windows exports

- `GenerateWindowsExports.cmake` builds `WindowsExports.def` at pre-link from every `.obj` of the exe **and** of
  libultraship (its C surface: logger, CVars, file API), so mods call engine functions directly.
- Data symbols need `HOST_DATA` (`__declspec(dllimport)` under `UNBOUND_MOD`): exports tagged `DATA` get no thunk.
  `ListCommonSymbols.py` adds COMMON globals that `__create_def` misses.
- Removing a source file: also delete its orphan `.obj` and the cached `modapi-exports.def`/`modapi-objects.txt`,
  or the stale symbol comes back as `LNK2001` pointing at `WindowsExports.def`.
- Any global `add_compile_definitions` that changes a shared struct (`GBI_S32_VTX`, `GBI_FLOAT_MTX`,
  `CONTROLLERBUTTONS_T`) must also be in `mod-sdk/cmake/UnboundModSdk.cmake`, or mods see a different layout.

## C++ files that emit display lists

`OPEN_DISPS` redeclares `FrameInterpolation_RecordOpenChild/CloseChild` at block scope. Inside an anonymous
namespace that redeclaration names a different, C++-mangled entity that nobody defines. Declare both `extern "C"`
at file scope (and inside the namespace when the drawing function lives there), or keep drawing functions at
global `static` scope. In C++, include `z64.h` before `macros.h`.

## Custom items

- All custom items share `PLAYER_IA_CUSTOM` (0x43, after the masks). Every `>=` range check over item actions in
  `z_player.c` must be closed, or custom items fall into the mask or bottle branches.
- Putting a custom item away must empty the hand (`Player_InitItemActionWithAnim(PLAYER_IA_NONE)`): a hand left on
  `PLAYER_IA_CUSTOM` answers "already in hand" to every later press.
- `PutAwayStalledHeldItem` puts away an item whose upper action has not run for 4 frames (cutscene, first person),
  so Link is never left frozen. `KeepCustomItemHeld` is the escape hatch for an item that owns the player through
  its own action func.
- `ITEM_CUSTOM` is 0x9C; message code reads `msgBuf` as signed `char`, so it must be cast to `u8`.
- Randomizer `GetItemEntry`s reuse the numeric fields for `RandomizerGet`: only `MOD_NONE` entries are custom items.
- Kaleido: a custom item's `cursorSlot` is `SLOT_CUSTOM` wherever it sits; index by `cursorPoint`.
  `sCellToVanillaSlot` is a permutation for display only: `gSaveContext.inventory` is never reordered because
  `SLOT()` indexes it everywhere. Two items asking for the same vanilla cell: the first wins and the other stays
  home, so a half-edited layout never hides an item.
- Placement pages: `< 0xE0` item pages, `0xFF` the OoT item grid, `0xE0` the OoT quest screen, `0xE1..0xFE` extra
  quest pages. Test with `CustomItemRegistry_IsQuestPage`.
- Vanilla item modes: `Item_Give` is not the only way in (old saves, save editor, console), so blocked items are
  swept on load, pause and save-editor draws instead of every frame.

## Equipment

`SetWorn` is the only writer of `sWorn`. Order matters: outgoing `onUnequip`, then the vanilla value, then
incoming `onEquip`, so an outgoing piece never undoes the incoming one. `sAppliedValue` remembers what the registry
wrote into `gSaveContext.equips`; when it differs, vanilla won the slot (vanilla equip, age change) and the custom
piece leaves without touching it. Swords live in two places (equipment nibble and B button, plus the swordless
flag): change both together.

## Forms

- The form stash (what the form unequipped) travels with the save and is discarded on file close before leaving
  the form, or its sword would be equipped in the next file.
- A form's `height` must stay positive: the camera divides `68 / height` in about 25 places.
- The transformation cutscene installs its action func from `OnPlayerUpdate`, never from where the mask was
  pressed: an action set inside the handler pass is overwritten that frame. The camera is driven by a fresh
  sub-camera with no target (`Camera_Special0` leaves eye/at/fov/roll alone); its pose is copied back to the main
  camera before release. Link's yaw is set before taking the camera, because a new sub-camera's `camDir` is
  garbage until its first update. Scene change forgets camera and light ids.
- `FormModel`: resources that do not exist crash the loader instead of returning `NULL` — check
  `ResourceMgr_FileExists` first. Limb display lists that are still vanilla *names* after the hook are equipment
  and are left alone; resolved pointers are Link's body and get the form's mesh. Mirrored face texture paths live
  in a node-stable set because the segment keeps the pointer until the frame is rendered. The far LOD has no
  mirrored meshes, so a form forces the near LOD and only restores the setting if it changed it.

## Randomizer

- `itemTable` is a `std::vector` and `Settings::mOptions`/`Context` options are `std::deque`s: mods append after
  `RG_MAX`/`RSK_MAX`, and `OptionGroup` and the exclude-location lists keep `Option*` that must not move.
- Mod `RandomizerGet`s start at `RG_MAX + 1`: `RG_MAX` is the "not found" sentinel. `ItemFromGIID` stays bounded to
  the vanilla range, or a GI of 0 would resolve to a mod item.
- Mod option keys are positional and never serialized as numbers; saves and spoilers use the key string.
- Logic has two banks: the live save and the fill's simulated inventory (`saveContext == &gSaveContext`). Mod
  ownership in the save bank reads the item registry, never a second copy.
- Rules are held in a `std::list` because the index keeps pointers and mods register after the first index.
  Capability selectors match `Name(` so `CanBreakPots` never matches `CanBreakPotsSomething`.

## Audio

- `AudioMix` callbacks run on the audio thread. Gains are refreshed on the game thread and read through
  `std::atomic<float>`; the registration count is published with release/acquire because the audio thread may
  iterate while mods are still loading. No CVars, no allocation on that thread (the scratch buffer only grows to
  the engine's block size).
- Archives of another game (`version` not OoT) reuse OoT's audio paths with their own numbering. `AudioLoad_Init`
  moves them to the custom path (new ids) and ignores any sequence or font whose file-given id is out of range or
  taken; `seqCachePolicyMap` is fixed-size. Foreign fonts remember their original id and archive so their
  sequences' font lists can be translated.
- `SinkForeignGameArchives` mounts foreign game archives right under the topmost OoT archive: the last mounted
  archive wins every shared path (mm.o2r shares ~2,500 with OoT), and the game region comes from the first game
  archive. `GetArchives()` returns a copy: keep it before iterating.
- Custom sequence ids do not fit the sequence command byte; they travel in `seqToPlay`.

## Engine hooks added for mods

Each hook sits at the single point where vanilla decides the thing it exposes, so one subscriber covers every
caller: `OnActorResolvePlayerRelation` (what an actor knows about Link: 557 distance reads and 54 facing checks
downstream), `OnActorResolveBgCheckFlags` (the mask every caller already picks), `OnCameraResolveView` (after the
camera modes, before the view is built), `OnResolveSwordDamage` (after Ivan's multiplier), `OnMagicResolveCost`
(before the "enough magic?" gate, so a discount can make a spell castable), `OnPlayerShieldBlocked` (the only frame
the guard flags still say who was blocked), `OnModVanillaBehavior` (a phase after the built-in VB handlers, which
re-register on every settings apply). `OnBgCheckResolveWallFlags` is not id-dispatched on purpose: bg ids are
assigned at scene load.
