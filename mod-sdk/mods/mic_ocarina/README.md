# Mic Ocarina

Recognizes the twelve native OoT songs and the ten owned songs from mm_collectables:
Sonata, Goron Lullaby, New Wave Bossa Nova, Elegy, Oath, Healing, Soaring, Fugue of Home,
Command Melody and Ballad of Hero. The latter three use the NEI branch fingering.

Native recognition keeps the game availability mask. MM/NEI candidates require
ownership and free play; they cannot satisfy NPC lessons, memory games or song prompts.
The audio thread validates a match and queues a custom result. The game thread
delivers the complete phrase through the existing note hook, preserving song effects
and other mods such as Command Melody. No extra native song bitmask slots are used.

A repeated button requires a release; a held note is not counted repeatedly.
The custom catalog must match mm_collectables. Calibration/real microphone quality
and song effects still require in-game verification.
