# Trirod — Echoes of Wisdom's Tri Rod in OoT (Skijer's NEI)

Level 3 of the Somaria chain (`CANE_TYPE_TRIROD`, wheel entry on `SLOT_CANE_OF_SOMARIA`).
Point the rod at things in the world to **learn** them as echoes, then **summon** copies,
paying for each in **triangles** — Echoes of Wisdom's exact economy.

## Controls (Trirod selected in the kaleido wheel)

| Input | Effect |
|---|---|
| Aim at an unlearned table actor | It glows **white** — C learns it (free, instant) |
| Aim at one of *your* summons | It glows **red** — C dismisses it (refunds its triangles) |
| C anywhere else | Cast: swing + summon the selected echo at the ghost marker |
| L / R | Step through learned echoes (notification shows name + cost) |

- Budget: **6 triangles** of summons alive at once (`TRIROD_BUDGET`). Casting past it
  despawns the **oldest** summons until the new one fits — EoW's rule, not an error.
- The ghost marker is a floating billboard of the selected echo's miniature at the
  landing point. No floor there → no cast.
- Summons pulse **blue** periodically so you can tell yours apart.

## The echo table

Source of truth: `trirod_echoes.inc.c`. **Row order is the save format** (bit index in
`trirodEchoesLo/Hi`) — append-only, never reorder.

| # | Echo (OoT name) | EoW echo | Actor | params | Cost |
|---|---|---|---|---|---|
| 0 | Pot | Pot | `Obj_Tsubo` | 0 | 1 |
| 1 | Flying Pot | Flying Tile | `En_Tubo_Trap` | 0 | 1 |
| 2 | Rock | Rock | `En_Ishi` | 0 | 1 |
| 3 | Boulder | Boulder | `En_Ishi` | 1 | 2 |
| 4 | Grass | — | `En_Kusa` | 0 | 1 |
| 5 | Small Crate | — | `Obj_Kibako` | 0 | 1 |
| 6 | Crate | Crate | `Obj_Kibako2` | 0 | 1 |
| 7 | Sign | — | `En_Kanban` | 0 | 1 |
| 8 | Brazier | Brazier | `Obj_Syokudai` | 0x2400 | 1 |
| 9 | Bomb Flower | — | `En_Bombf` | 0 | 1 |
| 10 | Armos Statue | — | `En_Am` | 0 | 1 |
| 11 | Keese | Keese | `En_Firefly` | 2 | 1 |
| 12 | Fire Keese | Fire Keese | `En_Firefly` | 0 | 1 |
| 13 | Ice Keese | Ice Keese | `En_Firefly` | 4 | 1 |
| 14 | Guay | Crow | `En_Crow` | 0 | 1 |
| 15 | Stalchild | — | `En_Skb` | 0 | 1 |
| 16 | Leever | — | `En_Reeba` | 0 | 1 |
| 17 | Baby Dodongo | — | `En_Dodojr` | 0 | 1 |
| 18 | Biri | Zol | `En_Bili` | 0 | 1 |
| 19 | Shabom | — | `En_Bubble` | 0 | 1 |
| 20 | Skullwalltula | — | `En_Sw` | 0 | 1 |
| 21 | Cucco | — | `En_Niw` | 0 | 1 |
| 22 | Octorok | Octorok | `En_Okuta` | 0 | 2 |
| 23 | Tektite | Tektite | `En_Tite` | -1 | 2 |
| 24 | Blue Tektite | — | `En_Tite` | -2 | 2 |
| 25 | Peahat | Peahat | `En_Peehat` | -1 | 2 |
| 26 | Deku Baba | — | `En_Dekubaba` | 0 | 2 |
| 27 | Mad Scrub | — | `En_Dekunuts` | 0 | 2 |
| 28 | Bari | — | `En_Vali` | 0 | 2 |
| 29 | Shell Blade | — | `En_Sb` | 0 | 2 |
| 30 | Spike | Caromadillo | `En_Ny` | 0 | 2 |
| 31 | Stinger | — | `En_Eiyer` | 0 | 2 |
| 32 | Poe | Ghini | `En_Poh` | 0 | 2 |
| 33 | Bubble | — | `En_Bb` | -2 | 2 |
| 34 | Torch Slug | — | `En_Bw` | 0 | 2 |
| 35 | Freezard | — | `En_Fz` | 0 | 2 |
| 36 | Wallmaster | — | `En_Wallmas` | 0 | 2 |
| 37 | Floormaster | — | `En_Floormas` | 0 | 2 |
| 38 | Like Like | — | `En_Rr` | 0 | 2 |
| 39 | Skulltula | — | `En_St` | 0 | 2 |
| 40 | Armos | — | `En_Am` | 1 | 2 |
| 41 | Beamos | — | `En_Vm` | 0 | 2 |
| 42 | Moblin | Moblin | `En_Mb` | 0 (club) | 3 |
| 43 | Stalfos | — | `En_Test` | 2 | 3 |
| 44 | Lizalfos | Lizalfos | `En_Zf` | -1 | 3 |
| 45 | Dinolfos | — | `En_Zf` | -2 | 3 |
| 46 | Wolfos | — | `En_Wf` | 0 | 3 |
| 47 | Gibdo | Gibdo | `En_Rd` | -2 | 3 |
| 48 | ReDead | — | `En_Rd` | 0 | 3 |
| 49 | Dodongo | — | `En_Dodongo` | 0 | 3 |
| 50 | Iron Knuckle | Darknut | `En_Ik` | 2 | 3 |

"—" = OoT-only bonus with no EoW counterpart. EoW echoes with no viable OoT actor were
left out on purpose: Bed/Table/Trampoline/Water Block/Cloud (no actor exists), Wizzrobe
(OoT has none), Rope (no snake), spear Moblins (path-followers — spawning one without a
scene path is a crash), Anubis (needs its Tag spawner).

## Miniatures

`assets/custom/textures/trirod/gTrirodEcho<Actor>Tex.rgba32.png` — 43 shots, 32×32
rgba32, generated from prelude.roborich.com's actor-mode renders
(`/screenshots/oot/actors/<slug>.png`), auto-trimmed and packed by the soh.o2r build.
They feed the in-world ghost billboard. Variants share their actor's shot (all three
Keese use the same miniature). Bombchu had no upstream shot — dropped from the table.

## Files / wiring

- `trirod.h` / `trirod_echoes.inc.c` / `trirod.c` — all compiled by `#include` from
  `mods/items/logic/item_cane_of_somaria.c` (NOT vcxproj entries; same unity chain as
  `cane_pacci.c`).
- Handler hooks: `Trirod_Aim` + `Trirod_OnPress` before the generic cast,
  `Trirod_Cycle` from `Cane_CycleSummon`, `Trirod_FireSummon` as `Cane_FireSkill`
  case 6 (the sentinel `Nei_CaneActiveSkill` already returns for the Trirod),
  `Trirod_DrawPreview` from the cane's draw hook.
- Save: `trirodEchoesLo/Hi` (learned bitmask) + `trirodSel` in `NeiSaveData`,
  serialized in `mods/nei_save.cpp`, which also hosts the notification bridge.

## Known limits (by design, for now)

- Summoned enemies keep their vanilla AI, which targets **Link** — echoes are
  distractions/obstacles, not allies. Making them fight for you means porting ally
  AI per actor (the boss-remains allies show the shape of that work).
- Learning matches actor id + params variant; a Fire Keese teaches only Fire Keese.
- MM port not started (the table is OoT actors — an MM table is its own project).
