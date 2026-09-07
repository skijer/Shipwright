# Not Enough Items — Controls

Source of truth for every in-game control string: the get-item textboxes
(`mods/extended_player.c` `sNeiItems[]`, `soh/Enhancements/randomizer/randomizer.cpp`
`customItemMessages[]`) and the C-Up pause descriptions
(`soh/Enhancements/custom-message/PauseItemDescriptions.cpp`, and the 2ship/ComboShip copies).

Every row below was read off the behaviour code, not off those strings. When they disagree, the
code wins and the strings get fixed.

**C** = the button the item sits on (C-Left/Down/Right, plus the D-pad when that enhancement is on).

Message-font button glyphs (`sFontWidths` in `z_message_PAL.c`, indexed from `' '`): `\x9F` A ·
`\xA0` B · `\xA1` C · `\xA2` L · `\xA3` R · `\xA4` Z · `\xA5` C-Up · `\xA6` C-Down · `\xA7` C-Left ·
`\xA8` C-Right · `\xAA` control stick · `\xAB` D-pad.

---

## Page-2 items

| Item | Input | Result |
|---|---|---|
| Roc's Feather | C | High jump. Works in water (shorter) |
| Roc's Cape | C | High jump |
| | C in mid-air | Double jump, once per landing |
| Deku Leaf | C on the ground | Wind gust that pushes objects and enemies |
| | Hold C in the air | Glide. Drains magic |
| Whip | C | Lash forward; latches onto beams and bars |
| | Stick while swinging | Steer the pendulum |
| | Release | Launch with the swing's momentum |
| Spinner | Hold C | Charge (longer = faster ride) |
| | Release C | Ride forward |
| | Release C while Z-targeting | Homing dash attack at the target |
| Ball and Chain | Hold C | Spin the ball overhead |
| | Release C | Throw. Z-targeting aims it at the target, otherwise the stick aims |
| | C-Up | Toggle first-person aim |
| Fire / Ice / Light Rod | B slash | 3 projectiles at +30/0/-30 degrees |
| | B stab | One long-range shot |
| | B jump slash | Downward cone |
| | Spin attack | Expanding ring |
| | Hold C | Charge attack |
| | C-Up | Toggle first-person aim |
| Bomb Arrows | Hold C | First-person aim |
| | Release C | Fire. Costs 1 arrow (or seed) + 1 bomb |
| | Hold C past 70 frames | Drops a live bomb in hand instead |
| Switch Hook | Hold C | First-person aim |
| | Release C | Fire. Swaps places with the target, or damages it |
| | Z-target | Aim in third person |
| Gust Jar | Hold C | Suck. Pulls objects and enemies in |
| | Release C | Fire what you caught |
| | Hold C 20 frames while idle | Element wheel |
| | L / R in the wheel | Step the element |
| Dominion Rod | C | Fire the possession orb |
| | Stick, possessed | Move the possessed actor |
| | A, possessed | Jump |
| | C, possessed | The actor's own attack |
| | C-Up | Toggle first-person aim |
| Beetle | Hold C | Aim |
| | C-Up while aiming | Toggle first person |
| | Release C | Launch |
| | Stick, flying | Steer |
| | A, flying | Boost |
| | Z, flying | Lock / unlock the nearest enemy |
| | B, flying | Let go: the beetle flies home, or kamikazes a locked target first |
| Shovel | C | Dig |
| Mogma Mitts | C | Toggle wall climbing. Drains magic |
| Demise Destruction | C | AoE lightning explosion. Ground only |
| Time Gate | C | "Travel through time?" prompt |
| | A / B | Confirm / cancel. 48 MP, charged on confirm |
| Zonai Permafrost | C | Toggle the time stop |
| | | 4 MP to start, then 1 MP every 10 frames. Ends on a second press or an empty meter |
| Minish Cap | C near a pod soil | Fast-travel map |
| | C away from one | Shrink to Minish size / grow back |
| | | Any scene load grows you back |
| Lantern | C | Swing. Catches fire from a nearby flame |
| | Hold C in the pause cell | Put the flame out |
| Phantom Hourglass | C | Raise it: time stops and you aim |
| | C again | Rewind whatever the reticle holds along its own path |
| | C / B / any other button | Let go |
| Shadow Crystal | C | Wolf Link / back. OoT only |
| Rod of Seasons | C | First press draws the rod |
| | C again | Season prompt. A confirms, B cancels; a new season reloads the scene. OoT only |
| Elemental Wand | Hold L | Rod wheel, when you own more than one |
| | C | Cast the active rod |
| Pokeball | C | Pikachu / back |
| Power Keg | C | Drop a lit keg |
| Net | Swing | Scoop bugs, fish and fairies into a bottle |

### Dual Cane (one cell, four canes, six skills)

| Input | Result |
|---|---|
| A on the pause cell | Switch cane: Somaria / Trirod / Pacci / Ultrahand |
| C | First press draws the cane; after that it casts |
| L / R with the cane drawn | Step the summon (Somaria) or the echo (Trirod) |
| Hold L, Trirod | Echo grid |

Pacci's Flip is the one skill that splits C:

| Input | Result |
|---|---|
| Tap C | Flip the target onto its back |
| Hold C | Lift it into the air |
| Release | Throw it, at your lock-on target if you have one |

Trirod:

| Input | Result |
|---|---|
| C on a highlighted prop | Learn its echo |
| C on one of your summons | Dismiss it |
| C otherwise | Summon the selected echo at the ghost marker |
| | Creature echoes are learned by killing the creature with the rod drawn |

Ultrahand (C opens the mode; B always leaves):

| Input | Result |
|---|---|
| A | Grab, then weld, then release |
| D-pad | Push / pull / slide the held object |
| L + D-pad | Rotate (snapped) |
| R + D-pad | Raise / lower / slide |
| Z | Reset the rotation to how you grabbed it |
| Shake the stick | Pull a structure apart |
| L + R + A | Store the structure |
| Keep holding C | Rebuild the stored structure |

---

## Extended equipment (page 2, cycle with L in the pause menu)

| Slot | Item | Input | Result |
|---|---|---|---|
| Sword 1 | Cane of Byrna | B, B, B | Ground chain |
| | *(Insect Glaive — OoT only; 2ship keeps the Biggoron-reach cane that restores HP and MP on hit)* | Forward + B | Thrust |
| | | Hold B | Quick spin |
| | | Z + A | Jump slash |
| | | R | Kinsect; at full charge it launches you |
| | | A / A+back / B / R in the air | Dash / climb / spin / ground pound |
| Sword 2 | Four Sword | Hold R + B, 15 frames | 3 clones that mirror your attacks. 12 MP each |
| | | Hold L | Formation wheel |
| Sword 3 | Trident | B, B, B | Three-slash chain |
| | | Forward + B | Lunging thrust |
| | | Hold B | Charge, 3 levels; full fires the magic ball |
| | | Z + A | Rising strike |
| | | R | Shield |
| | | R + B | Guard dash |
| | | Hold R + A | Flight — stick moves, R up, L down, B light ball, A launch at the target |
| Shield 1 | Divine Shield | R | Block. Fireproof |
| | | Block within 10 frames of the attack | Stuns every enemy nearby |
| Shield 2 | Kite Shield | R in mid-air | Drop the shield and surf. Downhill builds speed |
| | | A / B / B+R | Hop / spin / dismount |
| Shield 3 | Shield of Ikana | Block within 12 frames | Drains the attacker's HP |
| | | On a killing blow | Revives you once per scene with 3 hearts |
| Tunic 1 | Champion's Tunic | Sidehop or backflip past an attack | Flurry Rush: slow motion, i-frames, up to 7 hits |
| | | Aim while airborne | Bullet Time |
| Tunic 2 | Spirit Tunic | Passive | Rupees absorb damage, 1 HP = 1 rupee, and 30% of each charge spills as pickups |
| | | Passive | Fire and underwater timers are skipped while you hold rupees |
| | | At zero rupees | No protection and half speed |
| Tunic 3 | Sage's Tunic | Passive | One resistance per medallion owned |
| Boots 1 | Pegasus Anklet | Keep holding B after a swing | Dash forward, sword first. Wind barrier costs 1 MP per 15 frames |
| Boots 2 | Climb Boots | Passive | Ice stops being slippery, steep slopes stop sliding you |
| Boots 3 | Roc Boots | Passive | Walk on water and lava; half gravity |
| Upgrades | Magic Cape | Passive on pickup | Halves every magic cost, rounded down |
| | | A on its cell | Show / hide the cape |
| Upgrades | Pendant of Memories | B near an enemy, sheathed, still, no Z | Mortal Draw |
| | | B in mid-air | Ground Pound |
| | | Z + 3 sidehops + B | Parry Leap |
| | | A on its cell | Turn the moveset on / off |

---

## SW97 medallion spells (quest page; C there equips the spell to that button)

Costs are `sSw97MagicSpellCosts` in `expansions/sw97/player/sw97_player_behavior.inc.c`.

| Medallion | Spell | MP | Effect |
|---|---|---|---|
| Forest | Wind | 12 | Tornado: drags enemies into the core and damages them there. It pulls you too, but never hurts you |
| Fire | Fire | 12 | A column of flame whose damage grows the longer it stands |
| Water | Ice | 24 | Freezes what it hits for 120 frames (6 s) |
| Spirit | Soul | 24 | Hands off to the fairy form — a toggle, no timer |
| Shadow | Dark | 12 | Nayru's-Love shield: no damage for 1200 frames, and the lighting dims |
| Light | Light | 24 | Sun's Song blast: undead freeze for 600 frames (30 s) and you heal 6 hearts |

## Boss remains (MM quest page; a C or D-pad press there equips one, then that button wears it)

| Remains | Input | Result |
|---|---|---|
| Odolwa | Move while worn | Red trail |
| | Hold A | ~2x run with a purple trail |
| | R + B | 6 friendly beetles. 6 MP |
| | A near soft soil | Take off on the moth cloud; auto-lands after 200 frames |
| Goht | Hold A | Bull charge (~3x run). Magic powers its contact damage and cone |
| | R + A | Ground-pound quake |
| | Hold B, release | Charged thunder bolt — longer charge, more range and damage. 4 MP |
| | R + B | Throw a friendly bombchu; R + B again detonates it |
| Gyorg | — | Zora-style swimming |
| | R in water | Fish school |
| | Hold B in water | Whirlpool, while magic lasts |
| | R + B on land | Fish school |
| Twinmold | — | Nothing yet: the Dark Link companion is stubbed |

---

## Weapon upgrades

| Item | Input | Result |
|---|---|---|
| Iron Knuckle's Axe | Passive | Double damage and reach, slower walk |
| | C-Up with the hammer out | Aim the throw |
| | The hammer's own C button | Throw. It returns like a boomerang |
| Razor / Gilded Sword | Passive | Double damage per level |
| True Master Sword | Swing at full health | Fires a thunder beam |
| Great Fairy's Sword | On hit | Restores HP and magic |
| Ultrashot | Passive | Longshot with 4x reach and 2x speed |
