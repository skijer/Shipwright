# PAK menu recovery and back-equipment visibility

The PAK loader used numeric positions in a changing file inventory as saved
selections. Adding or removing a file could select a different model, including
an adult-only model in the child slot. Three menu validation branches had empty
bodies, so the map combobox threw `std::out_of_range` while drawing its preview.

## Selection behavior

- The existing numeric CVars remain available to the menu and renderer. A sibling
  `Path` CVar records each chosen file relative to `mods/` (for example,
  `gMods.PakLoader.ChildModelPath` or `gMods.PakLoader.SlotMix.Sword0Path`).
- Startup resolves paths after both local and Harpoon skin inventories are loaded.
  File additions, removals, and enumeration-order changes cannot transfer an
  established choice to an unrelated file.
- A missing or incompatible selected file falls back to Default. An empty path
  explicitly means Default; an absent path means an older, numeric-only config.
- Older configs migrate their currently valid numeric choices. If an old numeric
  choice has already shifted onto a different compatible model, its former intent
  cannot be recovered automatically: select the intended model once.
- Adult, child, equipment, and individual equipment slots use the same eligibility
  rules as their dropdowns. Combined body/equipment PAKs remain valid donors.
- Turning off custom bodies, using a forced model, and drawing a remote player do
  not overwrite saved choices. Menu choice persistence is separate from activation.
- Map comboboxes can display stale or empty option maps without throwing or
  silently choosing a different entry. PAK menus also repair invalid values.

## Hide Back Equipment and Scabbard

The checkbox is available in Skijer's NEI > Pak Loader and alongside the graphics
model/equipment enhancements. Both entries use
`gEnhancements.HideBackEquipment`, default off, saved by the standard CVar system.

When enabled, the sheath limb's display list is suppressed after vanilla, PAK,
and custom-equipment overrides. This covers stowed swords, back shields, and the
empty scabbard when a sword is drawn. Hand limbs are untouched. It also applies to
the pause preview, first-person/crawling callbacks, NEI's additional back-shield
draw, and the Rito form's separate shield path. Skeleton traversal, collision
updates, and shield reflection matrices remain active.

This does not add display lists, textures, geometry, or matrix loads. It does not
edit or repack model assets. Equipment embedded directly into an unrelated body
mesh cannot be separated by a sheath-limb visibility setting.

## Regression checks

The headless ImGui test reproduced the original `map::at` abort before the fix.
Run the focused tests with the same ImGui version as libultraship:

```sh
git clone --depth 1 --branch v1.91.9b-docking https://github.com/ocornut/imgui.git /tmp/soh-test-imgui
python3 scripts/diagnostics/run_pak_menu_tests.py /tmp/soh-test-imgui
```

The tests cover inventory additions/removals/reordering, legacy migration,
missing files, equal basenames in different folders, relocation, wrong-age and
wrong-slot picks, combined equipment donors, body toggle persistence, reset,
cached slot refresh, and stale/disabled/empty map rendering. The runtime fixture
compiles the actual production selection functions with config/mount boundaries
stubbed. The UI fixture runs the actual map combobox template with real ImGui.
They do not replace a full platform build or in-game rendering checks.

In-game acceptance: enable the checkbox, try adult and child models, each sword
and shield, and both drawn/stowed combinations. Confirm hands remain visible,
the back and scabbard stay hidden, and the pause preview agrees. Toggle it off to
restore ordinary equipment rendering. Restart after changing the PAK inventory
and confirm both selections and menu access remain stable.
