# Pause layouts

`Mods Items Layout` in Enhancements / Quality of Life opens the layout window when
custom items, equipment, or page templates are registered. The same editor is
available under the Inventory, Equipment, and Quest Status tabs of Save Editor.

Select a page, click a cell, and select entries to move there. Multiple entries
in one cell form a wheel. Dragging a cell moves its first entry; the popup can
move any entry independently. This edits presentation, not save ownership.
New pages have configurable rows and columns (up to 48 cells, 8 rows).
Shrinking a grid groups entries from removed cells in the last remaining cell.
Vanilla item/equipment grids can also be resized. Quest templates retain their
game/mod-provided geometry. Reset restores registered defaults.

In pause, the existing extra-page button cycles registered and user-created pages,
including pages whose entries have not been acquired yet. A toggles the
selected cell's wheel, and the stick selects a wheel entry. A confirms equipment;
C/D-pad buttons equip compatible items. Custom pause descriptions use C-Up.
Custom items use the vanilla flight-to-button animation and commit their keyed
equipment assignment through the registry when the animation finishes.
Vanilla songs on rearranged pages provide audio previews, not the original
interactive note-staff demonstration.

## Mod registration

After registering items, guard `SOH_MOD_API_HAS(api, RegisterLayoutVanilla)` and:

1. Call `RegisterLayoutPage` with a stable namespaced key, family, rows and columns.
2. Optionally provide explicit `SOHLayoutCell` coordinates and sizes. Coordinates
   are pause-space, x rightwards and y upwards; widths/heights are positive.
3. Call `PlaceLayoutItem(itemKey, pageKey, zeroBasedCell)` for defaults. `pageKey` may be `vanilla`,
   and `itemKey` may be a native entry (`vanilla.<kind>.<source>`), so a mod can regroup the OoT grid
   into wheels. A mod item placed on the vanilla item grid shares the cell's wheel instead of covering it.
4. Optionally call `RegisterLayoutVanilla` to reference a native slot on this page.
   These references share ownership/state with their source; they do not grant
   or duplicate inventory contents. Each reference needs its own stable key.

The host copies page data and reference keys. Native source indices are inventory
slots (0–23), equipment row * 4 + column (0–15, upgrades in column 0), and quest
points (0–24). Page keys must not use the reserved `vanilla`, `user.`, or `page.`
namespaces. User overrides are persisted by entry key in console variables,
independently of transient registry indices.
