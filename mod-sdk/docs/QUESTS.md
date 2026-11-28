# Quests

A mod can add its own quest to the file select, next to Original Quest, Master Quest, Randomizer and Boss Rush.
The player picks it for a new file, and the mod runs only in that quest's files instead of taking over every file.

There are two kinds:

- **romhack**: the quest has its own world (scenes, text, audio) in its archives. A port of a romhack such as
  The Missing Link.
- **mode**: the quest plays in Ocarina of Time's world with its own rules. A survivor mode, a challenge run.

## Declaring

The quest goes in the mod's `manifest.json`, so the host knows about it before your code runs:

```json
"quest": {
    "key": "yourname_quest",
    "name": "Your Quest",
    "label": "YQ",
    "kind": "romhack",
    "world": ["YourQuest_World.o2r"],
    "logo": "textures/your_quest/gLogoTex",
    "adopt_original_files": false
}
```

| Field | Meaning |
|---|---|
| `key` | Stored in each file of the quest. Letters, digits, `_` and `-`, under 32 characters. |
| `name` | Read out on the quest menu, and drawn under the Ocarina of Time logo when there is no `logo`. |
| `label` | On the file's button, like `MQ` or `RAND`: up to 4 capitals or digits. |
| `kind` | `romhack` or `mode`. Anything else and the mod doesn't load. |
| `world` | romhack only: more archives in the mods folder that belong to the world. The mod's own archive always does. |
| `logo` | A 160x160 RGBA32 texture for the quest menu, in a path the game doesn't have. |
| `adopt_original_files` | The first time the quest loads, Original Quest files that belong to no quest become its files. For players who made files when your mod took over every file. |

A quest's files play as Original Quest. A file whose mod isn't installed is greyed out and doesn't open.

## When your mod runs

Every hook and vanilla behaviour your mod registers runs only while its quest is **awake**:

- a romhack is awake while its world is on top: one of its files is played, or the title screen shows it;
- a mode is awake while one of its files is played.

A hook that must run in every world (crash notes, a setup assistant) is registered with
`SOH_REGISTER_PERSISTENT_HOOK` instead of `SOH_REGISTER_HOOK`.

- `sApi->GetQuest()` gives the key of the loaded file's quest, or `NULL` for the game's own quests.
- `sApi->GetDeclaredQuest()` gives your own quest's key, or `NULL` when the host didn't take it: a romhack whose
  `world` archives are missing, say before its world is built, runs as a plain mod, in every file.

Your `ModInit` runs awake, as if your world were on top; once every mod is loaded the host puts your quest to sleep.
Whatever your mod writes straight into the game's tables (actor functions in `ActorDB`, entrance rows, item tables)
stays written, asleep or not, so put the game's values back in `OnModQuestAwake`, which always runs:

```c
static void OnQuestAwake(const char* quest, bool isAwake) {
    if (isAwake) {
        InstallActorOverrides();
    } else {
        RestoreVanillaActors();
    }
}

SOH_REGISTER_HOOK(sApi, OnModQuestAwake, OnQuestAwake);
```

Labels you set with `SetLabel`, time splits and item icons are kept for you: they show while the quest is awake
and the game's come back when it sleeps.

## A romhack's world

The game reads the topmost copy of each file. While a romhack sleeps, its archives sit under every other archive:
your scenes, text and music are covered by Ocarina of Time's, and only paths the game doesn't have (your logo, new
items' models) still show. Text overrides in `override/` are skipped too. When one of your files opens, your
archives go on top of everything.

The switch is live, without a restart: the host reloads cached files, textures, text and audio. It happens only
with no scene loaded: on the file select, and before the title screen.

The title screen shows the last world played; players can pin it to Ocarina of Time or to a romhack in
Enhancements > Graphics > File Select > Title Screen.
