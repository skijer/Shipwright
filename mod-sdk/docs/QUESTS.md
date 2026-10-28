# Quests

A mod can add its own quest to the file select, next to Original Quest, Master Quest, Randomizer and Boss Rush.
A romhack port or a total conversion becomes something the player chooses for a new file instead of something
that takes over every file.

Each mod quest is one more stop on the quest carousel, drawn with the quest's own logo (or the Ocarina of Time logo
with its name under it). Its files carry its label on their buttons, like `MQ` or `RAND`.

## Registering

```c
static const SOHModQuest sQuest = {
    sizeof(SOHModQuest),
    "yourname_quest",                  // stored in each file of the quest
    "Your Quest",                      // read out on the quest menu, drawn there when there is no logo
    "YQ",                              // on the file's button: up to 4 capitals or digits
    0,                                 // flags, below
    "YourQuest.o2r",                   // the archive in the mods folder that holds the quest's world, or NULL
    "textures/your_quest/gLogoTex",    // a 160x160 RGBA32 texture for the carousel, or NULL
};

if (SOH_MOD_API_HAS(sApi, RegisterQuest)) {
    sApi->RegisterQuest(&sQuest);
}
```

A quest's files play as Original Quest. The save keeps the quest's key, so:

- `sApi->GetQuest()` gives the key of the file being played (`NULL` for the game's own quests). It is set before
  `OnLoadFile` runs, and for a new file before its first save.
- A file of a quest whose mod isn't loaded is greyed out and doesn't open, just as a Master Quest file does when
  there is no Master Quest archive.

## The quest's world

`archive` names the archive that holds your scenes, text and audio. It covers the game's files only while a file of
your quest is played; for every other file it sits under them, so only the files the game doesn't have (your new
scenes, your logo) still show. The switch happens live when a file opens or is made: no restart.

- `sApi->GetQuestWorld()` gives the key of the quest whose world is on top, or `NULL` for the game's own.
- Hooks and VB handlers you register **after** `RegisterQuest` run only while your world is on top. Register the
  ones that must always run (crash notes, a setup assistant) before it.
- Whatever you write into the game's own tables (an actor's functions in `ActorDB`, entrance rows, item tables) stays
  written. Register `OnQuestWorldChange` before `RegisterQuest` and put the game's values back when `world` isn't
  your key, and yours when it is. The world changes only on the file select or at boot, with no actor loaded.
- The host resets the texture cache, the text tables and the audio caches on a switch; labels you set (`SetLabel`,
  time splits) are kept per world.

## Flags

- `SOH_MOD_QUEST_ADOPTS_ORIGINAL_FILES`: the first time the quest is registered, Original Quest files that belong
  to no mod quest become yours. Use it when players already have files made while your mod was the only world.
