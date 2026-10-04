# MM dialogue and Phantom safeguard evidence

Task 1 adds the approved C dialogue catalogue and `OnOpenText` dispatch for `8F20` Treasure Shop Gal, `8F21` Skull Kid, `8F22` Keaton, `8F23` Happy Mask Salesman, `8F24` Child Kafei, and `8F25` Lulu. The hook passes the current language, uses English fallback for every language, runs `AutoFormat` and `LoadIntoFont`, and disables message-table loading only after successful dispatch.

Selectors now use Impa `708D/708E`, Adult Zelda `70FF/70FE`, and Sheik `700F/7010`. Phantom selects zero and the real viewer talk function exits before progression, message-state, process, and offer calls. Its `0.01` scale, `1000` shape offset, and `35/100/0` collider remain covered. Keaton, Child Kafei, and Lulu are registry identities 20–22 with `OBJECT_INVALID`, no adapter, unavailable status, and no parameter decode.

Focused `-DNDEBUG -Werror` C builds passed actor, dialogue, extracted talk, actual `z_en_viewer.c` talk wiring, Ganon descriptor, MM actor, and Ruto-water tests. Real `CustomMessage` API and production hook translation units passed syntax compilation. The OoT archive table parser read 2,116 entries and found no `8F20`–`8F25` collisions. `run_stabilization_tests.sh ../donor-inputs/mm.o2r` passed all 12 binaries, 10 audit cases, streamed-audio and night-combat checks, plus the 22-display-list Skull Kid archive check.

Archive SHA-256 values: OoT `ea80d61b223ee38075fe5383b652998f3b08164e903134922f63f205d995717b`; MM `f10167e5682d74cc8da0137c524b1521f4e7ff47438b8888b63db6c7c6dc8d59`.

No full game link or visual/runtime test was performed because `cmake` is absent from the worker environment. The hook translation unit compiled with the real project and Linux-sysroot dependency headers.
