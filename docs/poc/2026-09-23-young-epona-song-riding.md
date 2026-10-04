# Rideable Young Epona POC1

## Baseline and scope

Based directly on `46d01e8ab7e7894102e782a72ba2e4ead65a50c4` from
`marsh6487/Shipwright` / `integration/nei-weather-static-actors`.
All 25 required feature ancestors are retained. This adds the recovered
young-riding implementation to the cumulative integration, including the
latest Midna and Zora fixes, RPG items, chest sizing, weather/audio and actors.

The original interrupted source became accessible during recovery and was
reused. Review corrected mounted-entry parameters, guarded new mount attempts,
and kept native ranch Epona until the summoned riding horse actually appears.
The native collision and jump/landing decisions are retained.

## Use

Install `Young_Epona_SoH_POC1_Assets.o2r` in the SoH mods folder. Enable
**Enhancements → Quality of Life → Ride Young Epona as Child**, then reload
the area. The checkbox defaults off. Child Link must know Epona's Song and
have an ocarina; randomizer ocarina-note requirements are respected.
Play Epona's Song in Hyrule Field, Lake Hylia, Gerudo Valley, Gerudo's Fortress
or Lon Lon Ranch. Young Epona uses the native horse summon and follow logic.

The pack supplies four MM horse animations and the two 38-frame child mounting
clips plus their data. The existing native young horse supplies the model and
other movement animations. Adult TP Epona packs remain independent.

Pack SHA-256:
`d1990518522c4b781a9b62f3b2df21e9aec5df0eae99542892093359ecab945d`.
It contains eight resources under
`objects/object_horse_link_child/rideable/`, totaling 19,296 compressed bytes.
The converter and tests are in `scripts/diagnostics/*young_epona_assets.py`.
Native donor archives are not committed.

## Preservation and verification

- Young Epona has a separate saved position and never grants adult Epona's
  quest ownership. Legacy saves and save-file changes reset missing young data.
- The young actor is child-only. Missing animation resources fail closed.
  Existing adult Epona, Ingo's horse and Master Cycle paths remain available.
- Native world collision, horse-blocked surfaces and fence-jump/landing logic
  are shared. The young model and rider correction use scale `0.00648`;
  physical jump-root displacement remains `0.01`, because all low/high-jump
  root Y samples are identical to the adult clips.
- Tests exercise actual production spawning, actor attachment, collision,
  jumping, mounting, camera handoff, dismounting, age changes and JSON saves.
  The full five affected C translation units compile against production headers.
- Asset tests verify real donor clips, rig compatibility, resource references,
  CRC64, bounds and reproducible output. Local donor verification passes all
  15 tests. CI runs the donor-independent tests and all production fixtures.
- The cumulative regression runner retains every existing check and now also
  requires the young-horse fixtures before platform builds.

## Runtime status

This is an implemented, statically checked candidate for game testing.
It has not been run in the game and is not a promoted runtime master.
Test song summon, mount/ride/jump/dismount and a mounted scene round trip with
native child Link first. Then check child Din rider fit and the usual Alt
Assets configuration. Confirm a horse left in the child timeline never appears
as young Epona after becoming adult.

The proposed adult/young Epona cosmetic controls were not implemented in the
interrupted work and are not part of this riding commit.
