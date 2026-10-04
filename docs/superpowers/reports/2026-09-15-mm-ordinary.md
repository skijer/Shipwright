# HMS, Keaton and Lulu implementation checkpoint

Implemented HMS three poses, Keaton three poses and Lulu four poses, with exact donor skeleton/clip/face contracts. Lulu singing is animation-only. Child Kafei remains reserved and unavailable.

The typed loader validates actual resource classes, skeleton children, bounds and pointer identity, and retains selected resources per viewer. Initialization failure and repeated destruction are safe; the outer viewer update now skips OoT object segment6 for MM actors. Ordinary MM drawing establishes opaque white environment color. Existing Skull Kid/Tatl, Phantom model-only lift, dialogue and legacy actor paths are preserved.

Verification on this source:
- `source .superpowers/sdd/mm-recovery/build-env.sh`
- `python3 docs/superpowers/reports/appendices/mm_ordinary_verify.py ../build-mm-catalogue ../donor-inputs/mm.o2r` — exit0: actual archive20loads (10poses withAlt off/on), sevenfaceassets, negative type/bounds/ownership checks; compiled production viewer init/update/draw/free10poses, independentstate, roots, facepackets, failurecleanup and segment6guard.
- `bash scripts/diagnostics/run_stabilization_tests.sh ../donor-inputs/mm.o2r` — exit0, including existing audio/weather/Skull regressions.
- Changed loader and viewer objects compiled against normal game headers. Independent task review: specificationPASS, qualityPASS, no actionable serious findings.
- `git diff --check` — clean.

No in-game visual claim. Actual replacement-pack visuals remain untested. The optional localLinux full build was stopped in favor of the normal Windows CI deliverable; fullWindows compilation/packaging is tracked separately.

MM archive SHA256 f10167e5682d74cc8da0137c524b1521f4e7ff47438b8888b63db6c7c6dc8d59; supplied originals unchanged.
