# Task 1 report: dialogue and Phantom safeguards

## Implemented

- Corrected progression selectors: Impa `708D/708E`, Adult Zelda `70FF/70FE`, and unchanged Sheik `700F/7010`.
- Added the six approved `8F20`–`8F25` messages in a C catalogue. Every game language resolves to the approved English fallback.
- Added always-on ID-specific `OnOpenText` hooks. The hook passes `gSaveContext.language`, calls `AutoFormat()` and `LoadIntoFont()`, and suppresses table lookup only when dispatch succeeds.
- Added `StaticStoryActor_CanTalk`. Phantom Ganon selects text ID zero, and the viewer exits before progression lookup, active-message handling, request processing, or talk offering.
- Added unavailable registry identities 20–22 for Keaton, Child Kafei, and Lulu with `OBJECT_INVALID` and `STATIC_ADAPTER_NONE`; parameter decoding was not extended.
- Preserved Phantom's real descriptor (`0.01` scale, `1000` shape offset, `35/100/0` collider) and did not alter placement, collision setup, attention flags, audio, MM assets, or Skull Kid's companion/display-list paths.
- Replaced the static actor test's `assert` calls with always-evaluated `REQUIRE` checks.

## Verification

- Red phase observed before implementation: the actor test failed on missing types and `CanTalk`; dialogue/talk tests failed on missing production headers.
- Focused direct builds used `cc -std=c11 -Wall -Wextra -Werror -DNDEBUG` and passed:
  `static_story_actor_test`, `static_story_dialogue_test`, `static_story_talk_test`, `static_story_ganon_test`, `static_story_mm_actor_test`, and `static_story_ruto_water_test`.
- A function-section fixture linked the real `z_en_viewer.c` and executed `EnViewerStatic_OfferTalk`; Phantom cleared stale talk state and made zero progression, message, process, offer, or close calls.
- A real-header C++ syntax probe compiled the `CustomMessage` `AutoFormat`/`LoadIntoFont` API. The production hook translation unit also passed syntax compilation with the real JSON/spdlog headers from the Linux sysroot.
- Parsed `text/nes_message_data_static/ntsc_nes_message_data_static` from the supplied OoT archive: 2,116 entries, zero collisions with `0x8F20`–`0x8F25`. Repository source has no competing text/hook IDs.
- `bash scripts/diagnostics/run_stabilization_tests.sh ../donor-inputs/mm.o2r` passed 12 standalone binaries, 10 audit tests, streamed-audio runtime, night-combat bridge, and the archived Skull Kid rewrite check (34 culls, 59 textures, 22 display lists).
- `git diff --check` passed.

Archive SHA-256: OoT `ea80d61b223ee38075fe5383b652998f3b08164e903134922f63f205d995717b`; MM `f10167e5682d74cc8da0137c524b1521f4e7ff47438b8888b63db6c7c6dc8d59`.

## Limits

`cmake` is unavailable in this checkout environment, so no full game link was run. The direct real-unit compiles and stabilization suite are CPU/build evidence only; no visual or gameplay runtime claim is made.
