# Actors and enemies

`RegisterActor` adds your actor to the same table the engine looks vanilla actors up in. From then on
`Actor_Spawn`, the update and draw loops, collision and `Actor_Kill` treat it like any other actor, and the
`instanceSize` you declare is what gets allocated, so your actor carries its own fields.

Start from `templates/actor_template`.

## Writing the actor

An actor is written exactly like a vanilla overlay: a struct that begins with `Actor`, and four functions.

```c
typedef struct RupeeSprite {
    Actor actor;
    ColliderCylinder collider;
    Vec3f home;
    s16 bobPhase;
} RupeeSprite;

static void RupeeSprite_Init(Actor* thisx, PlayState* play);
static void RupeeSprite_Destroy(Actor* thisx, PlayState* play);
static void RupeeSprite_Update(Actor* thisx, PlayState* play);
static void RupeeSprite_Draw(Actor* thisx, PlayState* play);
```

Everything in `soh/src/overlays/actors/` is your reference: colliders, `SkelAnime`, effects, damage tables, the
action-function state machine. Copy what a similar vanilla actor does. Headers of vanilla overlays can be
included (`overlays/actors/ovl_En_Tk/z_en_tk.h`) when your actor needs to read another actor's fields.

## Registering

```c
SOHActorDefinition definition = { 0 };
definition.structSize = sizeof(definition);
definition.key = "yourname.rupee_sprite";
definition.description = "Rupee Sprite";
definition.category = ACTORCAT_MISC;
definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
definition.objectId = OBJECT_GAMEPLAY_KEEP;
definition.instanceSize = sizeof(RupeeSprite);
definition.init = RupeeSprite_Init;
definition.destroy = RupeeSprite_Destroy;
definition.update = RupeeSprite_Update;
definition.draw = RupeeSprite_Draw;
int16_t id = sApi->RegisterActor(&definition);
```

| Field | Meaning |
|---|---|
| `key` | Your actor's identity. Registering the same key twice returns the first id. |
| `description` | Human-readable name (menus, enemy randomizer list). |
| `category` | `ACTORCAT_*`, which update list it lives in. Categories update in order: `ACTORCAT_BG` before the player, the player before enemies. |
| `actorFlags` | `ACTOR_FLAG_*` set on spawn. |
| `objectId` | The object bank the engine keeps loaded for it. `OBJECT_GAMEPLAY_KEEP` is always resident and is what an actor that draws its models by `__OTR__` path wants. |
| `instanceSize` | `sizeof` your struct; `0` means a bare `Actor`. |
| `enemyFlags`, `enemyParams`, `enemySpawnHeight` | Enemy randomizer participation, below. |
| `naviHint` | What Navi says on C-Up while it is targeted. `NULL` leaves it without a hint, like many vanilla enemies. |

**The id depends on which mods are installed.** Never save it or hard-code it: ask `GetActorId(key)` each session
(for example to spawn it: `Actor_Spawn(&play->actorCtx, play, sApi->GetActorId(key), x, y, z, 0, 0, 0, params)`).

Console: `spawn custom_actor "<key>" <params> [x y z [rx ry rz]]`.

## Enemies

An actor with `SOH_ACTOR_ENEMY` in `enemyFlags` joins the Enemy Randomizer: it can be picked as a replacement
(with `enemyParams`), its own placed copies are shuffled like vanilla enemies, and it gets its own checkbox in
*Enhancements → Extra Modes*, saved by key.

| Flag / field | Meaning |
|---|---|
| `SOH_ACTOR_ENEMY_NOT_IN_CLEAR_ROOMS` | It needs more than nuts and a sword or sticks to kill: never put it in a "defeat every enemy" room. |
| `SOH_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS` | Keep it out of rooms with a timer. |
| `enemySpawnHeight` | Added to the floor height the randomizer found, for enemies that fly or hang. |

Enemy keys follow `<author>.enemy.<name>`. Console: `spawn custom_enemy "<key>" <params>`.

## Texts

`RegisterMessage` gives a text of yours an id from a range free in vanilla and in SoH:

```c
SOHModMessage greeting = { sizeof(SOHModMessage), "yourname.greeting", "Hello, %rhero%w!", NULL, NULL,
                           TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM, true };
uint16_t textId = sApi->RegisterMessage(&greeting);
```

Open it with `Message_StartTextbox(play, textId, actor)`, or put it in an actor's `textId` so it speaks when talked
to. German and French are optional and fall back to English; `autoFormat` wraps the lines for you. Like actor
ids, the text id depends on the installed mods: ask `GetMessageId(key)`, never store it. For a one-off textbox
without an id, `sApi->ShowTextbox(play, text, autoFormat)`.

## Changing vanilla actors

A mod can also change what vanilla actors do without replacing them, through hooks filtered by actor id:

```c
SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_EN_ARROW, ArmBombArrow);
SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_ARROW, WatchBombArrow);
SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_EN_BOM_CHU, DrawAsLandmine);
```

`OnActorUpdate` runs right after the actor's own update, even the frame it kills itself, which is where a mod
catches it. `OnActorDraw` can replace the vanilla draw (`*drawVanilla = false`), `OnActorDrawEnd` draws on top
with the actor's matrices still loaded. See [Hooks](HOOKS.md) for damage, collision, sound and grayscale hooks.

## Traps

- Moving an actor's `world.pos` does not move its colliders: many actors build collider geometry once, in
  world space. Update the collider from the actor (`Collider_UpdateCylinder`) or move it yourself.
- An actor that should be liftable follows `ObjKibako`'s flow (`Actor_HasParent` → held, `Actor_HasNoParent` →
  released); never clear `parent` by hand.
- A `DynaPolyActor` (moving platform) updates in `ACTORCAT_BG`, before the player; anything that must have the
  last word over the player (a "press A" prompt) belongs in `OnPlayerUpdate`, not in the platform's update.
- Unbound does not name the `bgCheckFlags` bits: bit 0 is "on ground", bit 3 "touching a wall", bit 5 "in water".

## Complete actors to read

The [`unbound-mod-nei` branch](https://github.com/skijer/Shipwright/tree/unbound-mod-nei/mod-sdk/mods): the Trutefel enemies (Miniblin, Molmauk, Stone
Beetle) with skeletons, animations and enemy-randomizer entries, and the Cane of Somaria's statues, crates and
rideable platform.
