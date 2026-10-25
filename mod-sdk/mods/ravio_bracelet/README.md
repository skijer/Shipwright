# Ravio's Bracelet

`drop custom_item "nei.equip.ravio_bracelet"`

Passive equipment in the Magic Cape / Pendant column. A on its pause slot
toggles it; obtaining it enables it. Ownership and toggle use the native
custom equipment save section. The icon and get-item model temporarily use
the native Goron Bracelet until Ravio's own artwork is supplied.

The native magic bar turns purple and regenerates over time. Native spell costs,
capacity upgrades, and the requirement to obtain magic remain in force.
Spending magic resets a 40-update recovery delay and clears partial recovery
progress. The rate then rises linearly with the fraction of MP remaining:
1/6 of the former rate when empty, up to 1/3 when nearly full. The former
rate was one MP every two updates; the new rate is one MP every twelve
updates at empty, every eight at half capacity, and about every six near
full. An integer accumulator carries fractional progress between points,
with identical rates at the same fill percentage for single/double magic.
Lens use and native magic animations suspend
recovery. Pauses, textboxes, cutscenes and transitions do not advance recovery.
The recovery curve follows the user's 23 October feedback; the initial delay
is unchanged.

The original HUD draws the bar with its native position, dimensions, opacity,
single/double capacity and spending animation. The fill is purple, with a
lighter purple consumption preview. The native infinite-magic fill is purple
as well while this equipment is enabled. Color overrides last only through
interface rendering; existing cosmetic values (including unset values) are
restored immediately afterwards, with game-state main start and file exit
as cleanup fallbacks. Play-draw end occurs before the HUD and must not clear
the tint. Both paused and ordinary world draw paths retain it until the
interface has rendered. No config
save is requested. No wheel or second meter is drawn.

Only the magic/stamina utility is implemented; wall merging is deferred by
the user's explicit scope. Compile/install from Shipwright-unbound with
`./build-all.local.ps1 -Only Mods -Reconfigure`.

Production-function tests: `NEI-Unbound-pruebas/test_new_items.py`. In-game
bar appearance, interaction with spells, and balancing await user feedback.
