# Cucui Ganon

A dance-fight replacement for the final Ganondorf battle.

## Playing

Enable **Enhancements → Cucui Ganon → Cucui Ganon battle** and reach the final battle (`warp 517 adult` from Dev Tools). Reload the scene to repeat it. Setting `gMods.CucuiGanon.PlayFullM5` plays the whole M5 routine with Link free, for recording.

The intro replaces the celestial transformation: the spotlights come on as the intro dance ends, then camera and control return to Link. The model uses `gGanondorfSkel` and the original display lists, so model replacements still apply.

## Fight

- Ganondorf has 80 internal points and summons four Phantoms at 40. Back hits and back arrows take 1 point.
- Three back arrows or reflected orbs knock him down, vulnerable for 120 updates; an orb that completes the count takes 5 points.
- He throws fire, ice, electric and explosive orbs from his hands, raises rocks, sends shockwaves and dashes. Orbs can be reflected with the sword.
- The Phantoms use `object_gnd` at 80% scale. Each dies to a back hit, a Bomb Arrow, or a reflected orb aimed at it. While any remain, Ganondorf is shielded; once all four fall, one reflected orb stuns him.
- At 1 point, a back slash during a stun triggers the finisher. The mod reports the victory with `ReportBossDefeated`, and a blue portal in the center rolls the credits.

The boss title card reads **papi chulo** over **Ganondorf**. Navi's C-Up descriptions are in English. Spanish lyrics scroll on their own translucent layer from the start of the dance; they hide while a real game message is open. `ganondorf_lyrics.inc` holds the 169 verses, synced as 610 beats at 125 BPM.

## Randomizer

The mod registers with `RegisterBoss` in Ganon's slot (`SOH_BOSS_SLOT_GANON`). Outside the randomizer the **Cucui Ganon battle** checkbox decides; in a seed, the fight is fixed when the seed is generated and listed under `customBosses` in the spoiler. Logic is vanilla Ganon's. Phantoms only appear in this fight.

## Music

The mod interrupts `NA_BGM_OPENING_GANON`, plays the RC1066 intro and entrance, one loop pass without the three vocal tracks, then the full loop, using fonts 22, 27 and 32 from `oot.o2r`. Animations and meshes are referenced by OTR path for resource-mod compatibility.
