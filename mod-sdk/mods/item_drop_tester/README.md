# Item Drop Tester — Ship/OoT

Standalone local test mod. Enable `item_drop_tester`, then open **Mods > Item Drop Tester**
with a randomizer save loaded. No game or SDK source changes are required.

- **Custom items and variants**: one row per parent item, a Drop button for its base
  rando identity (if registered) and separate buttons for registered pickup variants.
- **Vanilla / built-in rando rewards**: 293 named rewards from the current OoT rando
  catalog, with their actual rando IDs. Filter by name, key or subtype.
- A click queues a physical pickup 80 units above Link. Close the menu and unpause.
  Ownership is granted only when collected. No location/check or seed placement is changed.
- Native rewards call `ItemTable_RetrieveEntry(MOD_RANDOMIZER, rg)`, the same bridge
  to `GetGIEntry_Copy` used for final rando entries. Progressives resolve against the
  current save; resolved custom entries use the host's custom-drop pipeline.
- Custom pickups use `ITEM00_SOH_CUSTOM` and `CustomItemRegistry_AttachDrop`, preserving
  variant identity, models, messages, receive callbacks and acquisition settings.
- Native pickups use `ITEM00_SOH_GIVE_ITEM_ENTRY_GI` and the randomizer's acquisition
  behavior. The mod reproduces its world-draw calls using exported C functions.

Select an entry without dropping it to inspect its identity/checklist. For custom
entries the panel also displays the icon and acquisition-model source.
The checklist covers pickup, icon, acquisition model, held model, use, save/load
and Anchor. Results persist locally. **Copy full checklist (Markdown)** exports
pending and tested entries for both catalogs.

Tests use the current inventory, mods and rando settings: duplicate/progressive
rewards keep their normal effects. The tester does not reset ownership or fake an
unowned inventory. It does not emulate a location's obtainability-to-blue-rupee
fallback or its seed-specific ice-trap disguise; it tests the selected reward.
Logic-only/event/shop pseudo-items are omitted from the native catalog.

Native display names and RG constants in `rando_catalog.inc.h` were extracted from
`Shipwright-unbound/soh/soh/Enhancements/randomizer/item_list.cpp` on 2026-10-24.
The custom catalog is read live from loaded mods, including their variants.

Build/install from the authoritative host checkout:

```powershell
.\build-all.local.ps1 -Only Mods -Reconfigure
```

In-game validation belongs to the user; the game is not launched by the build.
