# Bosses

A mod can replace a boss fight, including the final battle. The fight takes over one of the game's ten boss
**slots**: whenever the game would load that boss's room, it loads your arena instead, under the slot's scene
id. Everything the game and the randomizer key on that scene keeps working unchanged: the blue warp and the
reward it grants, the heart container check, boss souls, Boss Rush, save and death warps, and both boss and
dungeon entrance shuffle.

| Slot | Scene | Age |
|---|---|---|
| `SOH_BOSS_SLOT_GOHMA` | `SCENE_DEKU_TREE_BOSS` | child |
| `SOH_BOSS_SLOT_KING_DODONGO` | `SCENE_DODONGOS_CAVERN_BOSS` | child |
| `SOH_BOSS_SLOT_BARINADE` | `SCENE_JABU_JABU_BOSS` | child |
| `SOH_BOSS_SLOT_PHANTOM_GANON` | `SCENE_FOREST_TEMPLE_BOSS` | adult |
| `SOH_BOSS_SLOT_VOLVAGIA` | `SCENE_FIRE_TEMPLE_BOSS` | adult |
| `SOH_BOSS_SLOT_MORPHA` | `SCENE_WATER_TEMPLE_BOSS` | adult |
| `SOH_BOSS_SLOT_BONGO_BONGO` | `SCENE_SHADOW_TEMPLE_BOSS` | adult |
| `SOH_BOSS_SLOT_TWINROVA` | `SCENE_SPIRIT_TEMPLE_BOSS` | adult |
| `SOH_BOSS_SLOT_GANONDORF` | `SCENE_GANONDORF_BOSS` | final |
| `SOH_BOSS_SLOT_GANON` | `SCENE_GANON_BOSS` | final |

## Registering

```c
static const SOHBossDefinition sBoss = {
    sizeof(SOHBossDefinition),
    "yourname.stone_golem",
    "Stone Golem",
    "yourname.golem_arena",            // SceneDB name of the arena; NULL fights in the slot's own room
    SOH_BOSS_SLOTS_ADULT,              // slots it may take
    SOH_BOSS_SLOT_VOLVAGIA,            // preferred slot outside the randomizer, or -1
    "RG_MEGATON_HAMMER or canKill(RE_IRON_KNUCKLE)",
    "gMods.StoneGolem.Enabled",        // optional toggle; NULL is always on
};

if (SOH_MOD_API_HAS(sApi, RegisterBoss)) {
    sApi->RegisterBoss(&sBoss);
}
```

The arena is a scene of your own in `unbound/scenes.json`, or any registered scene. Spawn 0 of the arena is
where Link arrives through the boss door.

**Which slot it takes.** Outside the randomizer, the preferred slot when it is free, otherwise the first free
allowed slot (mods are served in key order, so the result never depends on load order). A randomizer seed draws
the slot at random among the allowed free ones, records it in the spoiler log under `customBosses` and the save
file keeps it for the whole run. A fight registered with a single allowed slot always gets that slot.

**What it inherits.** The fight keeps the replaced boss's checks: the dungeon reward on the blue warp and the
heart container. The pool does not change, so a seed stays beatable.

**Logic.** `logic` replaces the condition the randomizer uses to beat the replaced boss (the dungeon's clear
event, Ganondorf's exit to the escape, or the Ganon check). It uses the condition language of
[Dungeons](DUNGEONS.md#conditions). Leave it NULL to keep the vanilla boss's condition.

## Running the fight

```c
#include "z64boss.h"

static void OnSceneInit(int16_t sceneNum) {
    if (Z64Boss_IsFightHere(sApi, "yourname.stone_golem", gPlayState)) {
        Actor_Spawn(&gPlayState->actorCtx, gPlayState, sGolemActorId, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);
    }
}

static void Golem_Die(Golem* this, PlayState* play) {
    Vec3f warp = { 0.0f, 0.0f, 0.0f };
    Vec3f heart = { 0.0f, 0.0f, -200.0f };
    Z64Boss_Finish(sApi, play, &this->actor, warp, heart);
}
```

`Z64Boss_Finish` ends the fight exactly as the replaced boss does:

- sets the room clear flag;
- reports the defeat (`ReportBossDefeated`), which runs the `OnBossDefeat` hooks with the replaced boss's actor
  id, so boss timestamps, Boss Rush progress, Anchor and the game-complete stat see it;
- dungeon slots: spawns the blue warp (Ruto first for Barinade, like vanilla) and the heart container, each
  behind its usual vanilla behaviour so the randomizer and Boss Rush can replace them;
- Ganondorf's slot: starts the tower collapse;
- Ganon's slot: rolls the credits.

Your actor should check `Flags_GetClear(play, play->roomCtx.curRoom.num)` on init and, when the room is already
clear, spawn only the warp (`Z64Boss_SpawnRewards` with the slot from `GetBossSlot`), like a vanilla boss does.

## Queries

| Entry | Answer |
|---|---|
| `GetBossSlot(key)` | Slot the boss holds in this file, or -1. |
| `GetSceneBoss(sceneNum)` | Key of the custom boss fighting in that scene, or NULL. |
| `GetBossSlotScene(slot)` | The slot's scene id. |

## Limits

- Boss Rush spawns its blue warp at the vanilla room's coordinates; place your arena's floor so that spot is
  reachable.
- The arena shares the replaced boss's scene flags (it loads under that scene id), so pick flags as the vanilla
  room does: the heart container uses collectible flag `0x1F`.
