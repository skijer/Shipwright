# Credits: mods from marsh6487's work

These mods carry **marsh6487** as author. They are the work of [marsh6487](https://github.com/marsh6487), from the
branches of [marsh6487/Shipwright](https://github.com/marsh6487/Shipwright) and its review PR
[skijer/Shipwright#17](https://github.com/skijer/Shipwright/pull/17), re-made for Unbound's mod SDK. The original
branches target ComboShip; here each feature is a standalone `.o2r`. Nothing of Skijer's items, mods or branches is
modified: the mods that extend NEI items sit on top of them through hooks.

| Mod | What it is | Original work |
|---|---|---|
| `young_epona` | Ride Epona as a child | `poc/child-epona-song-riding`, `feat/nei-young-epona`. The MM clips come straight from `mm.o2r`, the follow-up the PR describes as not yet implemented. |
| `midna_navi` | Midna in place of Navi | `poc/midna-navi-soh-poc2` (visual half; the model pack is separate) |
| `din_fire_equipment` | Din's Fire on shield and sword | `poc/transform-cosmetics-20260921` line, Din fire POCs (re-made with vanilla fire particles) |
| `heart_magic_cosmetics` | Cosmetic Editor colours on custom heart and magic models | `poc/heart-magic-cosmetics` |
| `chest_size_matches_contents` | Chest size follows contents | `feat/nei-gi-upgrade-recovered-20260927` (d676a5ff) |
| `stat_upgrades` | Power / Defense / Speed / Quarter Heart pickups | `poc/soh-stat-items-20260923`, itself a port of **Jepvid**'s [HarbourMasters/Shipwright#6760](https://github.com/HarbourMasters/Shipwright/pull/6760) |
| `nei_gi_redesign` | Redesigned get-item models for 20 NEI items | `soh/assets/custom/objects/nei_gi_redesign` and `NeiGiPresentation.cpp`; sources in `tools/nei_gi` |

## Provenance notes, kept from the PR

The PR states that its asset payloads are **not represented as copyright-cleared**, and lists what needs review. The
assets that this branch carries from it:

- `nei_gi_redesign`: the 20 redesigned GI models. The PR flags the Whip (a reconstruction from a screenshot of an
  unreleased design, not the original mesh) and the Time Gate (its faces sample the supplied icon).
- `stat_upgrades`: the stat models and icons come from the donor PR above; donor origin does not by itself clear them.
- Some effect textures in the original work are AI-generated (the PR names ImageGen for the Fire/Ice flow art). None
  of those is included here yet.

Skijer's NEI items that these models and effects build on are credited in their own manifests.
