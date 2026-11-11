# Unbound: known issues

Defects found but not yet fixed. Each entry says what a player or mod author sees, what it takes
to trigger it, where it comes from, and the fix we would make. Engine *limits* (things the format
cannot express yet) are in [`README.md`](./README.md) "Known remaining limits" and SPEC §9, not
here. Remove an entry when its fix lands.

| # | Issue | Who hits it | Severity |
|---|---|---|---|
| 1 | Dungeon keys and items land in a vanilla dungeon's slot | any custom scene with keys, locked doors or dungeon items | high: can break vanilla dungeon progress |
| 2 | Registry `titleCardTexture` is never shown | anyone using custom title cards | low: cosmetic |
| 3 | Item restrictions alias vanilla scenes from the 129th custom scene | 129+ custom scenes mounted at once | low–medium, very rare |
| 4 | Actors keep object bank slots in signed bytes | a room listing ~125+ objects | low–medium, rare |

Found 2026-09-23 while surveying what custom dungeons are missing; not yet reproduced in game.

## 1. Dungeon keys and items land in a vanilla dungeon's slot

**Symptom.** In a custom scene, small keys, the boss key, the map and the compass are credited to —
and locked doors spend keys from — whichever vanilla dungeon's slot was current. The key counter is
never drawn. Keys seem to vanish when the player leaves and re-enters through a different area.
Worse, the custom scene reads and writes a *vanilla* dungeon's inventory: it can spend keys the
player picked up in the Fire Temple, or hand out the Forest Temple boss key.

**Trigger.** A custom scene that gives or uses a dungeon item: a chest or drop holding a small key,
boss key, map or compass, or a key-locked `DoorShutter`/`EnDoor`/`DoorGerudo`, or a boss door. The
slot is that of the last overworld area or vanilla dungeon visited (interiors, shops and grottos do
not change it): Hyrule Field → Deku Tree, Zora's River → Forest Temple, Kokiri Forest → Fire Temple,
Gerudo's Fortress → Thieves' Hideout. Outside Ganon's Castle gives index 19, one past the end of
`dungeonKeys[19]`, so a key there increments `inventory.defenseHearts` and a locked door spends one.

**Cause.** Vanilla behaviour that custom scenes expose (stock SoH hits it too for a dungeon placed in
a spare debug scene slot). `gSaveContext.mapIndex` is the index into `inventory.dungeonKeys[19]` and
`inventory.dungeonItems[20]`, and only `Map_Init` (`z_map_exp.c`) sets it, from `switch`es over
vanilla overworld and dungeon scenes with no `default`. It is not saved. Readers:
`Item_Give` and `Item_CheckObtainability` (`z_parameter.c`), the HUD key counter (a separate
vanilla-scene `switch` in `z_parameter.c`), `z_door_shutter.c`, `z_en_door.c`, `z_door_gerudo.c`,
`z_en_takara_man.c`, `z_elf_message.c`, and the compass checks in `z_kaleido_scope_PAL.c`.

**Fix.** Not a standalone patch: there is no spare slot, and clamping `mapIndex` only stops the
overflow. Route every reader through an accessor keyed by scene, as `SceneFlags_Get` does for scene
flags: vanilla scenes get `&dungeonKeys[mapIndex]` exactly as today; custom scenes get storage
persisted by name in the `unbound` save section. Default a scene's dungeon id to its own scene id so
a later shared `dungeon` registry key (a dungeon and its boss scene sharing keys) stays compatible
with saves made before it. Decide what `OnDungeonKeyUsed` passes to randomizer/Anchor/tracker hooks
in a custom scene. About 15 sites in ~9 game files; additive save change, no SPEC change unless the
registry key is added. Best done as the first slice of custom-dungeon support.

## 2. Registry `titleCardTexture` is never shown

**Symptom.** An entrance with `showTitleCard` into a custom scene shows no title card. Nothing
else breaks.

**Trigger.** Any custom scene with `titleCardTexture` (Prelude emits it) and a `showTitleCard`
entrance.

**Cause.** Introduced by Unbound. `TitleCard_InitPlaceName` (`z_actor.c`) looks the texture up in
`SceneDB_GetTitleCardTexture` *after* its vanilla-scene `switch`, whose `default:` clears the
texture and returns. A custom scene always takes `default`.

**Fix.** Look up the registry texture before the `switch`; when there is one, use it and skip both
the `switch` and the language renaming. The renaming overwrites characters 4–6 from the end of the
name (`ENG` → `FRA`/`GER`/`JPN`) and would corrupt a custom path. One function, ~10 lines, very low
risk.

## 3. Item restrictions alias vanilla scenes from the 129th custom scene

**Symptom.** A custom scene inherits the item restrictions of an unrelated vanilla scene: greyed-out
items, or warp songs and Farore's Wind blocked as in a boss room. Harmless for some aliases (the
Deku Tree row allows everything).

**Trigger.** At least 129 custom scenes mounted at once. Custom ids start at 128; id 256 and up
wrap in a byte onto vanilla ids.

**Cause.** Vanilla code made reachable by Unbound's wider scene ids. The `sRestrictionFlags`
lookup in `z_parameter.c` compares `(u8)play->sceneNum`, and the Better Farore's Wind check reuses
the same truncated value. Stock SoH ids never exceed 127.

**Fix.** Compare the full scene id. Ids 128–254 then never match a vanilla row, and 255 matches the
all-zero terminator row, which is what an unlisted scene gets anyway. One line, no vanilla risk.

## 4. Actors keep object bank slots in signed bytes

**Symptom.** An actor that loads an extra object from the room's list — the boss door, and about
30 NPCs and enemies — goes missing because it deletes itself. Some may not check the wrapped value
and read out of bounds instead; not all have been checked.

**Trigger.** A room whose object list puts such an actor's object at bank slot 128 or later
(roughly 125+ objects, with that object listed late). Doors in custom scenes normally use the keep
objects near slot 0 and are unaffected; the boss door's `OBJECT_BDOOR` is the realistic door case.

**Cause.** Vanilla code made reachable by Unbound's 1024-slot object bank (stock SoH caps it at 128,
which a signed byte holds). `counts.md` widened `Actor.objBankIndex` but not the per-actor copies:
e.g. `DoorShutter.requiredObjBankIndex` and its `(s8)objectIndex < 0` test, `EnDoor
.requiredObjBankIndex`, and `s8` fields and locals such as `objBankIndex`, `moriTexObjIndex` and
`objBankIndexSkel1` in ~33 files under `soh/src/overlays`.

**Fix.** Widen them to `s16` and drop the `(s8)` casts, then run
`scripts/unbound-pointer-drift.sh --narrowing` for anything left. Mechanical and low risk per site
(actor structs are not saved); the risk is missing one, since the game's C builds with `-w`.
