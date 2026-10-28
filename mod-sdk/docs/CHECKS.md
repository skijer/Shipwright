# Checks in vanilla areas

A new patch of grass, pot or sign in a vanilla scene can be a randomizer check of its own: declare it in
`unbound/checks.json` and the randomizer puts it in the pool, places an item in it and finds it in game. Like
[dungeons](DUNGEONS.md), it is data: a mod without a binary can add checks. Nocturne writes this file for you.

## `unbound/checks.json`

Layer-merged like `scenes.json`: one entry per check, keyed by a namespaced id.

```json
{
  "yourname.kf_grass_by_shop": {
    "name": "KF Grass by the Shop",
    "type": "grass",
    "scene": "SCENE_KOKIRI_FOREST",
    "region": "RR_KOKIRI_FOREST",
    "position": [-420, 0, 615],
    "vanilla": "RG_GREEN_RUPEE",
    "logic": "child and CanCutShrubs"
  }
}
```

| Field | Meaning |
|---|---|
| `name` | Spoiler log, hints and save name. Unique; renaming it after release loses it in existing saves. |
| `type` | `grass`, `bush`, `tree`, `pot`, `crate`, `small_crate`, `rock`, `boulder`, `icicle`, `red_ice`, `sign`, `wonder_item` or `freestanding`. |
| `scene` | The scene the actor is placed in (`SCENE_*`, or a mod scene id). |
| `region` | The vanilla logic region it is reached from (`RR_*`). |
| `position` | The actor's position as placed in the scene. The game recognises the check by its actor, its scene and its X and Z, as it does the vanilla checks of that type, so moving the actor needs this updated. |
| `actor` | Optional: the actor's name when it is not the type's usual one (`En_A_Obj` for a sign made from that actor). |
| `vanilla` | What it holds unrandomized: an `RG_*` name or a mod item key. |
| `logic` | Condition to collect it from its region, in the [dungeon condition language](DUNGEONS.md#conditions). |
| `shuffleOption` | Optional: a mod randomizer option that must also be on for the check to be shuffled. |

## When it is shuffled

The check follows the setting that shuffles its type, exactly as the vanilla checks of that type do: *Shuffle
Grass* for grass, *Shuffle Pots* for pots, and so on, including the *Dungeons* and *Overworld* variants. A
`shuffleOption` narrows that further: the check is shuffled only when both are on. When it is not shuffled it
keeps its vanilla item.

Each check has its own collected flag, saved by its id.
