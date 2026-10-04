# Young Epona recovery implementation plan

**Goal:** Reconstruct the interrupted, authorized child-riding POC and push it on top of the latest cumulative SoH integration.

**Baseline:** `46d01e8ab7e7894102e782a72ba2e4ead65a50c4`, freshly cloned from `marsh6487/Shipwright`, branch `integration/nei-weather-static-actors`. All 25 required feature baseline ancestors passed. The original worktree initially appeared unavailable, then its source and assets became accessible during recovery and were reused.

**Approved design:** Visible default-off `Ride Young Epona as Child` enhancement; Epona's Song enables MM-style summon/run-up and riding in all five native horse areas. Reuse OoT's horse-blocked collision and small-fence jump/landing behavior. Keep young Epona child-only and store its location separately from adult Epona. Port the four missing MM horse clips and two child mount clips under private resource names. Do not alter unrelated features or adult TP Epona assets.

**Execution:** Existing user authorization covers implementation and push. Independent asset conversion and actor adaptation can run concurrently under the parallel-agents skill; the controller owns player/spawn/save/UI integration. Each writer owns disjoint files. One final independent review covers the assembled patch. Runtime remains unproven until tested in game.

## Interfaces

- `soh/include/young_epona.h`: `ENHORSE_YOUNG_PARAM` is `0x4000`; `s32 Horse_CanUseYoungEpona(void)` checks age/setting/song/ocarina/notes/assets; `s32 Horse_YoungEponaAssetsAvailable(void)` checks the added pack; `s32 Horse_TrySummonYoungEpona(PlayState*)`; `void Horse_SaveYoungEpona(PlayState*, Actor*)` persists separate child data.
- `HORSE_YOUNG_EPONA = 2` in the horse actor type enum. The actor consumes the parameter flag and clears it before native parameter dispatch. Player rendering/mounting dispatches by this type.
- `soh/assets/objects/object_horse_link_child/rideable_young_epona.h` declares `gYoungEponaStopAnim`, `gYoungEponaRearAnim`, `gYoungEponaLowJumpAnim`, `gYoungEponaHighJumpAnim`, `gYoungEponaMountLeftAnim`, `gYoungEponaMountRightAnim` under `objects/object_horse_link_child/rideable/`. Mount data is separately namespaced and encoded with SoH resource types.
- `gSaveContext.ship.youngHorseData` and `youngHorseDataValid` appended to Ship save data, reset on file load/new save and serialized through SaveManager v4 with legacy-safe defaults.

## Tasks and checks

- [x] Assets: decode the supplied MM donor, validate four 46-limb horse clips and two 38-frame player mount clips, convert player references to SoH hashes/types, test bounds and roundtrip fields, package the eight resources in a separate O2R with a reproducible converter. Native source archives stay outside git.
- [x] Actor: add third skeleton/animation variant, native young scale `.00648`, MM attachment and hoof indices, eye textures, default-off/missing-asset guards and song handling. Preserve native `.01` jump physics (root motion matches adult); use the young scale only for rider/model correction. Test actual relevant production functions and compile the whole actor.
- [x] Spawn/player/save/UI: gate child spawn/summon, prevent duplicates without removing native ranch young Epona until the riding actor is ready, preserve adult ownership/state and Master Cycle; child mount clips/offsets; all five areas and scene entry; separate child save location and defaults; visible checkbox. Exercise gates, save isolation, and native adult path in production fixtures.
- [x] Integration: run narrow tests and cumulative diagnostics; preserve original non-horse production files; formatting/diff checks; review current changes. Update continuity notes with exact passed and untested checks.
- [ ] Push: fetch the integration tip again, preserve any concurrent changes, fast-forward push without force; verify exact remote SHA and GitHub workflow state.

## Review focus

1. Missing pack or setting off must preserve vanilla child behavior and never load unavailable clips.
2. Young horse state must never overwrite adult horse location, adult quest flags, or another save file.
3. Horse-blocked polygons and automatic fence jumps must retain native collision/landing decisions.
4. Age transitions must not leave a young horse in adult scenes; mounted exits and death/reload must be safe.
5. Dynamic skin buffers and player mount references must be valid across draw, reentry and Alt paths.

## Scope note

The interruption record says Epona cosmetics had not been implemented. This urgent recovery targets rideable young Epona and preserves every existing integration feature; it does not claim the separate proposed cosmetic controls are complete.

## Verification result

The restored asset pack is byte-identical to the interrupted original. All 15 actual-donor asset tests pass. Production actor, player, spawning/ranch and v1-v4 JSON save fixtures pass; all five C units compile. The final cumulative regression runner (including the new fixtures) exited 0. clang-format 14 and diff checks pass. Independent review found no blocking or minor issue. Game runtime and native/Alt/Din visual acceptance remain untested.
