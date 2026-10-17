# Example: custom actor types

Declares seventeen actor types in nineteen files under `unbound/actors/` (the path names the type:
`unbound/actors/example/carpenter.json` is `example/carpenter`): eleven that register (four of them
fail to spawn, and two turn off their head tracking, on purpose) and six that are rejected, plus two
files that are not a type at all, and places them
by name in Hyrule Field (`spot00`, room 0, setups 0–2: child day and night, adult day) just outside
the castle drawbridge, using only vanilla assets by path. It is the test fixture for
[`actors.md`](../../actors.md).

Walk out of Castle Town onto the field. Everything faces you; "left" and "right" are as you see
them.

| Placement | Type | Where | What it exercises |
|---|---|---|---|
| `90` | `example/carpenter` | front row, left | skeleton + looping animation, collision, talk with the type's default message (`params` 0), head tracking on limb 15 |
| `91` | `example/malon_pose` | front row, middle | flex skeleton held on one frame, `segments` 8 and 9 (eyes, mouth), `hideLimbs` (2 and 5, as her vanilla draw code, and 99, past her last limb: ignored with an error), message from `params` |
| `92` | `example/talking_pot` | front row, right | static display-list model that talks like a sign, message from `params` |
| `96` | `example/glass_pane` | back row, left | `translucent`, and `yOffset`: the Spirit Temple mirror's glass, centred on its origin, lifted to stand on the ground |
| `106` | `example/carpenter_looks_away` | back row, far left | `look.turnAxis` `[-2, 0, 0]` (normalized to `-X`, drawn through `Matrix_RotateAxis`) and a `nodAxis` written with a numeric string: the head turns *away* from Link as he walks past, and still nods toward him |
| `107` | `example/parallel_look_axes` | back row, far right | `look.turnAxis` `[0, 0, 3]`, parallel to the default `nodAxis`: the head does not turn, with an error |
| `99` | `En_Kanban` | back row, right | a vanilla actor placed by name: a wooden sign. Hyrule Field's rooms already load its object (`0x12F`); a vanilla actor placed by name needs its object in the room exactly as one placed by number does |
| `93` | `example/no_animation` | — | a skeleton with no animation: the type is rejected (an OoT skeleton has no usable rest pose) and the placement skipped |
| `94` | `example/uses_a_later_key` | — | an entry with a key this build does not know (`base`): the type is rejected and the placement skipped |
| `95` | `example/not_registered` | — | a name nothing registered: the placement is skipped |
| `97` | `example/bad_skeleton` | — | a skeleton path that does not exist: the type registers, this actor does not spawn |
| `98` | `example/bad_animation` | — | an "animation" that is a texture: the type registers, this actor does not spawn |
| `100` | `example/skin_skeleton` | — | Epona's skeleton, whose limbs are skin limbs: the type registers, this actor does not spawn |
| `101` | `example/too_few_joints` | — | Malon's skeleton (18 limbs) with the carpenter's animation (16): the type registers, this actor does not spawn |
| `102` | `example/later_model_key` | — | a key this build does not know inside `model` (`lod`): the type is rejected and the placement skipped |
| `103` | `0x1000` | — | a custom type's number instead of its name: the placement is skipped |
| `104` | `example/talking_pot` | — | `params` as an object (reserved for named arguments): the placement is skipped |
| `105` | `-5` | — | a negative number, which names no actor: the placement is skipped |

Six files are never placed: `unbound/actors/En_Kanban.json` (rejected, the name is a vanilla actor's),
`example/look_on_static` (rejected, `look` needs a skeleton), `example/zero_scale` (rejected, a
scale must be positive) and `example/short_look_axis` (registers; its two-number `nodAxis` turns
head tracking off, with an error), `example/not_an_object` (a JSON array: skipped) and
`example/broken_json` (not valid JSON: skipped).

The messages are `0xA001`–`0xA003` in `text/eng/messages.json`.

Package with any zip tool, keeping the paths, and drop the result in SoH's `mods/` folder:

    cd custom-actors && zip -r ../custom-actors.o2r unbound.json unbound text scenes

## Expected

- The log registers eleven types from `0x1000` (in name order, interleaved with any other mounted
  mod's types) and rejects six: `En_Kanban` ("already names an actor"), `look_on_static` ("needs
  a skeleton"), `no_animation` ("needs an animation"), `zero_scale` ("must be a positive
  number"), and `uses_a_later_key` and `later_model_key` ("not a key this build knows"). It logs
  one error each for `parallel_look_axes` ("are parallel") and `short_look_axis` ("not three
  numbers of nonzero length"), which still register. `example/not_an_object.json` logs "not a JSON
  object" and `example/broken_json.json` "invalid JSON in one layer", and neither stops any
  other type from registering. When the field loads, placements `93`–`95`
  and `102` are skipped as naming no known actor, `103` and `105` are skipped for their ids and
  `104` for its params, and `97`, `98`, `100` and `101` fail to spawn, each with one error naming
  the path it could not use ("actors of this type do not spawn"), logged the first time the type
  spawns and not again when the field reloads. Malon logs one error the same way for `hideLimbs`
  entry 99 ("not a limb of its skeleton (1-18)") and spawns.
- The carpenter stands in his idle loop, turns his head to follow Link within 200 units and while
  talking, and says `0xA001` (three lines over two boxes). He and Malon cast round shadows.
  Malon stands still in one frame of her singing pose with open eyes and a smile, no extra hands,
  and says `0xA002`. The pot says `0xA003`. None of the three can be walked through.
- Behind them, the round mirror glass stands on the ground, about 94 units tall, and the field
  shows through it. The sign reads as a vanilla sign.
- Two more carpenters flank the back row. Walking past the far-left one (`106`), his head turns
  away from Link, the mirror image of the front carpenter's, while still nodding toward him. The
  far-right one (`107`) keeps his head still.
- The Actor Viewer (Developer Tools) finds each type by its display name, and its Spawn button
  spawns it.
- Without the mod, Hyrule Field is unchanged.
