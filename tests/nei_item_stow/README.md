# Custom item put-away and roll audio regression

The preserved presentation checkpoint is `34815ab4f`; gameplay fixes are isolated
on `fix/nei-item-stow-poc1-20260927` and included in the same GI PR update.

## Behavior

- An assigned but inactive Ball and Chain no longer calls its unequip routine
  when another button is pressed. This removes the change-arms sound during
  rolls and avoids resetting shared timers, pose and animation speed.
- Native A-button and scripted put-away recognize actual custom held state,
  including a lit lantern carried while the native item action is empty.
- Accepted stow and completion of the native item change release the existing
  camera, collider, trail, charge and sound ownership through item callbacks.
- Explicit lantern stow persists until the next lantern use. Captured fire,
  pocket lighting and green-fire healing continue; the in-hand Poe lens ends.
- Native rod initialization synchronizes the private equip state, so cancellation
  still works after the original button press and equip animation have finished.
- Stowed Ball and Chain/Whip hold activation waits for button release before
  rearming. Whip swing/launch momentum, beetle remote control, leaf gliding,
  spells and toggles retain their separate action handling.

## Verification

```sh
python3 tests/nei_item_stow/run_ballchain_tests.py
python3 tests/nei_item_stow/run_lantern_rod_tests.py
python3 tests/nei_item_stow/run_stow_tests.py
bash scripts/diagnostics/run_cumulative_regressions.sh
```

The focused fixtures execute production input/handler functions and native
stow decision paths. Engine boundaries are supplied by fixtures; these are
source-level regression checks, not an executable game session. Full player
unity syntax and the existing cumulative regression gate are also checked.

All three focused runners and the complete cumulative gate passed on the final
candidate. Optional `--baseline-negative` runs for lantern/rods and the player
stow contract reject the original published behavior at `bea6f438f`. The
Ball and Chain test likewise reproduced the inactive-item audio failure before
its guard was applied. Independent review found no blocking issues, including
the release/reassignment case where no custom handler polls the intervening input.

Runtime acceptance remains pending: stand still and put away each affected
tool, wait, then re-use it; roll with an inactive Ball and Chain on B/C/D-pad;
confirm real Ball and Chain impacts, whip swing releases, rod charging and
lantern catch/fire variants retain their normal behavior. Master is unchanged.

## Shared put-away sound ownership (2026-09-28)

The runtime report showed two outgoing sounds: immediate custom cleanup and
later native item-change completion. Both now share transition-scoped sound
ownership keyed to the outgoing action. Native initialization and a fresh
custom equip reset only that item's state. Incoming equip sounds are separate.
A custom completion before a later stow (digging, landing, empty aim) remains
an independent sound; only same-update early cleanup joins a new transition.

The 14 routed tools are Fire/Ice/Light Rod, Shovel, Switch Hook, Mogma Mitts,
Whip, Gust Jar, Deku Leaf, Dominion Rod, Ball and Chain, Beetle, Bomb Arrows,
and Cane of Somaria (including its shared cane modes).

Lantern cleanup is silent and retains native fallback. Spinner's homing-windup
sword sound is an attack cue, not stow, and remains unchanged. Time Gate,
Minish Cap, Feather/Cape and the three spells have no competing custom stow
emitter. Standalone Slate/Seasons draw toggles and Wand wind toggles remain
independent cues rather than acquiring another native item's sound ownership.

Tests execute real rod/Mitts teardown, native FinishItemChange and shared sound
helpers, with early/late cleanup, delayed completion, swaps, repeated cycles,
native-only inactive fallback, exact ordinary sword/bow sound IDs, distinct
source keys, and completion followed by a later stow. Native InitItemAction is
also exercised to verify outgoing ownership survives a swap and resets on
re-equip. Ball-and-Chain impact/roll and lantern persistence fixtures remain.
These are compiled source fixtures, not an in-game audio acceptance test.
