# Stat items integration candidate

| Field | Record |
|---|---|
| Baseline | `integration/nei-weather-static-actors` at `d676a5ffd2f61677e45190259165005d51e17df3`; existing build workflows passed. Prior runtime acceptance is not inferred from those builds. |
| Candidate | `poc/soh-stat-items-20260923`, based directly on that integration commit. |
| Donor | HarbourMasters/Shipwright PR [#6760](https://github.com/HarbourMasters/Shipwright/pull/6760), by Jepvid; head `c823750b1ab24ef468685ac45f40cda11fd7c92e`. Only the PR delta from `eca767dcbfa925cb81ad094fbee9a8d1002a17fb` is ported. |
| Scope | Optional randomizer stat pickups, quarter hearts, settings, item pool/logic, save counters, models/icons, messages and tracker. |
| Preservation | Existing NEI items and forms, spiritual-stone effects, weather/static actors, Midna audio, pedestal fixes and chest-size behavior remain on the integration base. No libultraship update, scene/archive consolidation or ComboShip change. |
| Configuration | Randomizer → Stat Upgrades; existing packs and load order. Test both vanilla and the user's usual Alt Assets configuration for the new models/icons. |
| Status | Implemented on the latest full integration stack. Local focused regression checks pass; full executable/package status is recorded by GitHub Actions. The user requested merging onto integration for combined testing; in-game acceptance remains pending. |
| Recovery | Keep the baseline branch/commit above; the candidate is separate and has no donor-history merge parent. |

## Port decisions

- Use the integration branch's existing option/menu and tracker APIs. Append new
  item and setting IDs so existing serialized IDs keep their meanings.
- Persist all eight new counters, default missing saved fields to zero, and reset
  counters when creating a new seed in the same process.
- Default missing Anchor stat counters to zero so older client snapshots remain
  readable; verify current snapshots round-trip without losing their counters.
- Read magic configuration from the loaded seed, enforce its logical threshold,
  give the magic stat item a distinct serialized name, and suppress ordinary
  progressive magic in both of the host's pool insertion paths.
- Apply quarter-heart health in both live receipt and seed logic. Avoid native
  inventory-slot lookups for stat tracker IDs.
- Register crawl behavior again when a randomizer save is loaded. Apply the speed
  multiplier once through shared movement calculation. Preserve original integer
  block-push math while the corresponding stat option is off.
- Apply power to each collision's damage contribution, before accumulation, so
  multiple hitbox elements do not multiply previously accumulated damage again.

## Verification and runtime handoff

Focused tests execute the production stat callbacks, pickup cases, health/magic
logic, tracker inventory expressions, new-file resets, push formulas, and collision
damage routine. Undefined-behavior sanitization covers the new tracker IDs. The
existing chest-size and Time Gate tests remain enabled alongside the new suite in
CI. Model XML/reference validation does not prove their rendered appearance.

For combined runtime testing, use a fresh seed with stats enabled and compare with a seed
where all stat options are disabled:

1. Collect a stat item; inspect its model, message, tracker count and effect. Save,
   reload, then return to file selection and create another seed to check reset.
2. Check ordinary running/swimming and locking on, crawl/climb/block pushing,
   incoming damage and sword hits, including an NEI form and a spiritual stone.
3. Collect incremental magic and a quarter heart; check meter capacity, refill,
   health display and progression availability. Include Mask Quest shuffle when
   checking that magic stats replace progressive magic.
4. Check the Time Gate chest and chest-size option, then inspect one new model/icon
   with the usual Alt Assets packs enabled.

These runtime checks are unperformed here. The user's explicit request is to merge
the combined changes onto integration for testing. That merge does not establish
runtime acceptance; the original baseline remains recoverable at the commit above.
