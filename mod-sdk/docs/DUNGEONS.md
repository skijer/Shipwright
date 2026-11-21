# Dungeons

A dungeon is data, not code: its scenes in `unbound/scenes.json`, its logic in `unbound/dungeons.json`. The game
gives it small keys, a boss key, a map and a compass of its own, puts its checks in the randomizer, and adds its
door to the dungeon entrance shuffle. A mod with no binary can ship a whole dungeon.

## `unbound/dungeons.json`

Layer-merged like `scenes.json`: one entry per dungeon, keyed by a namespaced id.

```json
{
  "yourname.sky_temple": {
    "name": "Sky Temple",
    "scenes": ["yourname.sky_temple"],
    "entrance": "yourname.sky_temple/front",
    "exit": "yourname.field_delta/sky_temple_door",
    "overworldRegion": "RR_HYRULE_FIELD",
    "entranceLogic": "RG_HOOKSHOT",
    "entryRegion": "Lobby",
    "smallKeys": 2,
    "bossKey": true,
    "map": true,
    "compass": true,
    "regions": {
      "Lobby": {
        "events": { "Lobby Torches": "HasFireSource" },
        "checks": {
          "Lobby Chest": { "type": "chest", "flag": 1, "params": "0x5A21", "vanilla": "@map", "logic": "true" }
        },
        "exits": { "Hall": "keys(1)", "Altar": "event(Lobby Torches)" }
      },
      "Hall": {
        "checks": {
          "Hall Heart Piece": { "type": "collectible", "flag": 4, "params": "0x0406", "vanilla": "RG_PIECE_OF_HEART" }
        },
        "exits": { "Lobby": "true", "Boss Room": "bosskey" }
      },
      "Altar": { "exits": { "Lobby": "true" } },
      "Boss Room": { "exits": { "Hall": "true" } }
    }
  }
}
```

| Field | Meaning |
|---|---|
| `name` | Shown in the spoiler log, the hints and item names ("Sky Temple Small Key"). |
| `scenes` | Every SceneDB scene of the dungeon. Its keys work in these scenes. |
| `entrance` | SceneDB entrance into the dungeon (`"<scene>/<entrance>"`). |
| `exit` | SceneDB entrance in the overworld where leaving the dungeon puts Link. |
| `overworldRegion` | The vanilla logic region that holds the door (`RR_*`). |
| `entranceLogic` | What it takes to go through the door; empty means always. |
| `entryRegion` | The region `entrance` lands in. An exit back to `overworldRegion` is added for you. |
| `smallKeys`, `bossKey`, `map`, `compass` | Dungeon items to create and put in the pool. |
| `regions` | The logic graph inside the dungeon. |

A region has an optional `scene` (default: the first of `scenes`), `events`, `checks` and `exits`. An exit names
another region of the same dungeon, or `@outside` for the overworld region.

### Checks

| Field | Meaning |
|---|---|
| `type` | `chest` (treasure flag, `En_Box`) or `collectible` (collectible flag, `En_Item00`). |
| `flag` | The chest's treasure flag or the collectible's flag in its scene. |
| `params` | The actor's params as placed in the scene, so the randomizer can recognise it. |
| `scene` | Scene of the actor; default: the region's. |
| `vanilla` | What it holds unrandomized: an `RG_*` name, a mod item key, or `@smallkey`, `@bosskey`, `@map`, `@compass`. |
| `logic` | Condition to collect it from its region. |

Every chest and collectible you place must be a check: the randomizer replaces the content of every chest it
recognises. A reward that is not a chest or a pickup (an NPC, a switch puzzle) is still a `collectible`: have
your actor call `Flags_SetCollectible(play, flag)` when it pays out, and the randomizer gives the placed item.

Check names must be unique; the spoiler log calls them `"<name> <check>"`. The save file stores them by name, so
renaming one after release loses it in existing files.

## Conditions

The same small language is used by `logic`, `entranceLogic`, events, exits and boss logic. Words combine with
`and`, `or`, `not` and parentheses (`&&`, `||`, `!` also work).

| Write | True when |
|---|---|
| `true`, `false` | Always, never. |
| `RG_HOOKSHOT`, `yourname.lantern` | Link has the item and can use it (`CanUse`). |
| `has(RG_DINS_FIRE)` | Link has the item (`HasItem`). |
| `HasFireSource`, `CanUseSword`, … | A logic capability (the list in `RandoLogic.h`). |
| `canKill(RE_IRON_KNUCKLE)` | Link can kill that enemy. |
| `event(Lobby Torches)` | An event of this dungeon happened. `event(LOGIC_*)` reads a vanilla event. |
| `keys(2)` | Link holds at least that many small keys of this dungeon. |
| `bosskey` | Link has this dungeon's boss key (always true without one). |
| `tokens(10)` | At least that many Gold Skulltula tokens. |
| `trick(RT_*)` | That trick is enabled. |
| `setting(RSK_*, n)` | That setting has value `n`. |
| `child`, `adult`, `day`, `night` | Age and time of day. |

A word the game does not know is reported in the log with its column, and the condition is treated as always
true, so the seed still generates; fix it before release.

## How it plays

- **Keys.** Inside your dungeon the game counts your keys in a slot no vanilla scene uses, so doors, chests and
  the HUD counter work unchanged. Each dungeon's keys, boss key, map and compass are saved by dungeon id.
- **Randomizer.** The dungeon's checks join the pool; its items follow the dungeon item settings: *Vanilla*
  puts them back in their `vanilla` checks, *Own dungeon* and *Start with* keep them in this dungeon, and *Any
  dungeon*, *Overworld* and *Anywhere* let them land anywhere.
- **Entrance shuffle.** The door is a two-way dungeon entrance: with dungeon entrance shuffle on, your dungeon
  can be behind any dungeon door and any dungeon behind yours.
- **Hints and the tracker.** Hints call the dungeon's checks "an isolated place", and the check tracker does
  not list them yet.

## Limits

- Scenes with an id above `0xFF` are rejected: the randomizer tracks checks by an 8-bit scene. Register mod
  dungeons before other large scene packs, or keep the total number of custom scenes under 128.
- Up to 64 mod dungeons take part in the entrance shuffle at once.
