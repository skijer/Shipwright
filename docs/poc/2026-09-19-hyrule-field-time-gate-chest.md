# Hyrule Field Time Gate chest candidate

This code-only addition gives the existing adult Zelda/Lullaby chest the Time
Gate item in normal and randomizer saves. It does not add the item to a pool or
change a scene archive. Status: implemented and focused-test verified; in-game
appearance, full save/reload and user acceptance remain untested.

## Baseline and exact identity

Source parent: `d3323797f9c29f422031493a33b44512020c632a`, whose tree matches
published `bcde7a53ea261d25dca3ae212bd1349c6b73db66`. The concurrent rain work is
separate from these four production-file changes.

The accepted Field archive is
`Hyrule_Field_Master_R4_1_Grotto_Draw_Pass_Fix.o2r`, version 2,
190,740,482 bytes, SHA-256
`b14455cd72f5b47c1dfcc7aaaeb48f26e3e291bdae0aa4b0b9c2533f12d17f46`.
Its compiled chest and trigger records and editor setup 2 were directly checked
and match the table below. Chest offset is 6830 and song-trigger offset is 6814
in the listed compiled resource.

Exact placement also agrees with both available authoring roundtrips, including
their editor records and compiled actor bytes:

- `Field_R3_Roundtrip_Check.prelude.o2r`, export September 19 at 02:18:41 UTC;
  SHA-256 `b895c91ea7cffb82f3a3296e2a04de593011c7e1d9b84c44b98a88014f6793d1`.
- `Field_R4_Roundtrip_Check.prelude.o2r`, export September 19 at 03:13:33 UTC;
  SHA-256 `2f83a0f28b4f3dacdd5b7a6ae5b733c46523290a37541b2813b306d25aaec78d`.

| Property | Authored value |
| --- | --- |
| Scene / room / setup | Hyrule Field / 0 / adult setup 2 |
| Chest | `ACTOR_EN_BOX` (`0x000A`) |
| Parameters | `0xB7A5`: big switch chest, Heart Container placeholder, treasure flag 5 |
| Position | `(-1582, 220, 1961)` |
| Rotation | `(4316, 21604, 32)`; rotation Z supplies switch 32 |
| Compiled resource | `alt/scenes/shared/spot00_scene/spot00_room_0Set_000770` |
| Nearby song trigger | `En_Okarina_Tag`, params `0x1CA0`, position `(-1627, 224, 2047)`; type 7, Zelda's Lullaby, switch 32 |
| Nearby adult Zelda | `En_Viewer`, params `0x7F04`, position `(-1537, 222, 2033)` |

The chest's compiled 16-byte actor record is
`0a00d2f9dc00a907dc1064542000a5b7`. Neither roundtrip nor the accepted archive
was modified. The roundtrips are identity evidence, not replacement masters.
R4.1 has this exact chest in its Alt adult room resource; the ordinary namespace
does not contain a matching record. The code preserves the archive's existing
placement availability. Normal-save support and the Alt Assets setting are
separate concerns.

## Change and preservation

`EnBox_IsTimeGateChest` requires adult Link, Hyrule Field, room 0, exact authored
params, switch 32 and exact home XYZ. Only that chest selects the existing
`MOD_RANDOMIZER` / `RG_TIME_GATE` item-table entry. That table is available in
normal saves too. Other chest initialization and randomizer lookups are intact.

The fixed chest offers a copy of its full item entry with a negative get-item
ID through `GiveItemEntryFromActorWithFixedRange`. The existing chest handoff
normalizes the ID and the normal player receipt code calls
`Randomizer_Item_Give` in either save mode. The item's existing registry, draw
function, icon, inventory slot 31 and NEI save serialization are reused.

The native chest animation asks `Item_CheckObtainability` before receipt. A
Time Gate-specific early ownership check reads extended slot 31 before that
legacy function can index its vanilla item arrays with the custom item ID.
All other item obtainability cases retain their previous handling.

The custom-item text hook is now registered in normal saves, with a runtime
guard allowing only Time Gate's valid custom entry on text `0xF8` there. Other
normal uses of `0xF8` retain the ordinary message, and randomizer text handling
and the other message IDs retain their previous behavior.

Chest appearance, the song actor, switch 32, parameter bits, treasure flag 5,
the native open sequence and randomizer chest-opening progression are unchanged.
There is no migration, collection reset, archive rewrite, or new save bit.
**A save that already collected the placeholder Heart Container keeps the chest
open and does not receive a retroactive Time Gate.**

## Verification

Run `python3 -B scripts/diagnostics/run_time_gate_chest_tests.py`.

Both groups failed against the old production code for the expected reasons:
the reward remained a Heart Container and the normal-save text handler was
absent. Both pass with this change. The runner executes production function
bodies with real engine types for the chest and small fixtures at graphics,
collision, item-table, message rendering and item-grant boundaries.
The obtainability regression separately failed with a bounds-sanitizer report
(`index 173 out of bounds for u8[56]`) before its guard, then passed with the
guard. The chest test keeps bounds checks enabled.

Covered behavior:

- Normal and randomizer initialization both select Time Gate without querying
  seed placement for this chest.
- Native obtainability safely distinguishes owned/unowned Time Gate using its
  extended slot; ordinary bow/stick checks retain their behavior.
- Switch 32 remains required, the chest appearance sequence reaches its normal
  interaction state, and merely approaching does not collect the reward.
- The offered full entry survives negative-ID chest handoff and the production
  player receipt branch dispatches Time Gate exactly once, without a vanilla
  table fallback or a Heart Container grant.
- Treasure flag 5 is retained; re-entering after collection initializes the
  open chest rather than offering another reward.
- Child age, another scene, room, params, switch or any changed position axis
  leave the ordinary reward path intact; an ordinary randomized chest keeps
  its seed-selected style/reward path.
- Normal-save Time Gate text routes to the custom message handler even when
  registration occurred before pickup; unrelated vanilla messages stay native.

`git diff --check` passed. A bare invocation of the shared stabilization script
was blocked at compilation because this environment's default include path
lacked `nlohmann/json.hpp` for the separate scene-rain tests. The parent agent
reported its configured shared run passing; this note does not replace its
combined verification record.

No CMake production-source registration is needed. The focused runner can be
added to the shared diagnostics runner with the command above.

## Remaining runtime check

On the intended executable and accepted Field pack, use an adult save with
treasure flag 5 clear. Play Zelda's Lullaby by Zelda, open the chest, confirm
Time Gate's model/text/inventory entry, then save/reload and revisit. Check both
normal and randomizer saves with the authored Alt adult placement active.
The automated probe verifies the grant dispatch boundary, not rendered output,
disk persistence, or full randomizer-hook ordering in the running engine.
No archive or runtime-master promotion is implied.
