# NEI item hint-name candidate — 2026-09-28

## Baseline and scope

- SoH baseline: published GI PR #17 head `c77c18587a976f6d6cb5c8f91f27593286469218`.
- Isolated candidate: `poc/nei-hint-name-fallback-20260928`.
- User requested that the fix accompany the upcoming GI merge. Keep PR #17 in draft until its runtime acceptance; inclusion here is not master promotion.
- ComboShip port baseline: `4a42c2525e855e4eba3547de4c5f93a6afcd1eb8`; prepared on the same candidate branch name in the ComboShip repository for the later GI integration.

Ordinary item-area hints can insert the literal `No Hint` because many NEI item registrations have `RHT_NONE`, despite having valid localized display names. `Hint::GetItemHintText` now supplies the item's existing article and name when its dedicated hint mapping is absent.

Dedicated hint wording and clarity variants, mysterious hints, ice-trap disguises, non-item sentinels, and ComboShip's foreign-item/trap handling retain their existing paths. The shared `RHT_NONE` table entry is unchanged. No item IDs, hint IDs, placement logic, seed/save formats, models, textures, or effects change.

## Evidence

- The focused regression failed before the production edit on the exact Spinner/Market sentence.
- SoH and the ComboShip port each pass 753 ordinary item/clarity cases across English, German, and French, plus independent literal checks for the Market sentence, articles, language order, absent translations, dedicated clues, mysterious mode, sentinel isolation, per-slot text lifetime, and trap disguises.
- The fixture uses real `Item`, `HintText`, `CustomMessage`, and `Text` definitions and extracted production methods. Seed storage and final text-box formatting are substituted; this is not a game-runtime test.
- SoH and ComboShip ASan/UBSan passed with local LeakSanitizer disabled because the sandbox does not support its process inspection.
- The full SoH cumulative GI regression gate passed. The first local attempt lacked `nlohmann/json.hpp`; rerunning with the already available nlohmann/spdlog include directories completed successfully. Existing legacy C pointer-conversion warnings remain.
- Independent read-only review found no Critical or Important issues.
- Direct full-translation-unit syntax checking is unavailable locally because the ImGui development header is absent. CI full builds remain the compilation gate for the complete application.

## Repeatable checks

```sh
python3 -B scripts/diagnostics/run_hint_item_name_tests.py
ASAN_OPTIONS=detect_leaks=0 python3 -B scripts/diagnostics/run_hint_item_name_tests.py --sanitize
bash scripts/diagnostics/run_cumulative_regressions.sh
```

The SoH cumulative gate and ComboShip build-artifacts regression gate include the focused runner. The standalone runner also accepts `--source-root /path/to/ComboShip` to exercise the same tests against a ported checkout. The complete ComboShip regression gate and both games' full application builds were not run locally.

## Runtime acceptance and recovery

In the combined GI runtime build, reopen a previously affected gossip stone and confirm that it names the actual placed item. Check a normal mapped item and an ice-trap disguise; during the ComboShip GI port, also check an MM item and a foreign trap hinted from OoT. Confirm the behavior after reloading the save.

Runtime is untested and the candidate is not promoted. Revert the isolated hint commit to restore the prior behavior; the baseline GI commit and concurrent GI feedback work are preserved.
