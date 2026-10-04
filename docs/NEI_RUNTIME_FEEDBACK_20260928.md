# NEI runtime feedback candidate — 2026-09-28

Latest revision: the user accepted review04 releases, then requested Fire charge use the accepted Light charge animation with only a readable fire texture substituted. The final candidate retains the previous combined GI/held/hint work, restores Light release, fixes local Fire/Ice interpolation identity and impact heading, and includes the cold MM-stop Leaf completion fix. The new Fire charge uses the exact recovered Light geometry and the existing private fire crest artwork. See `docs/poc/rod-runtime-revision-20260928.md` for the current record; the revision03 descriptions below are historical and superseded where they conflict.


Baseline: GI17 published head `c77c18587a976f6d6cb5c8f91f27593286469218`; local mirror `68c198b4c` with identical tree `3669ec1bedc12fc1118f48a392af38cb400e9874`.
Preview checkout: `poc/nei-runtime-feedback-20260928`, isolated from the performance candidate. User approved revision03 on2026-09-28 at03:02 America/Chicago after reviewing the video/still. This accepts the demonstrated appearance and authorizes the pending runtime-build push; it does not establish in-game proof.

Publication candidate: `poc/nei-feedback-runtime-approved-20260928`, based on current published GI PR17 head `2e9c38cc5c16fa0998ee02adf64649d9592f478a`. That head already contains the hint-name fix, which is retained unchanged. The approved feedback is applied on top for the same GI PR/head branch. Performance PR18 and ComboShip PAK PR23 remain separate, with ComboShip GI promotion after runtime review.

## Scope and preservation

- Fire projectile review02 is explicitly accepted and frozen. Ice review03 changes its head/wake presentation; both retain three shots per set, native spread/trajectory, charge timing, damage and collision.
- Light projectile and Ice/Light charging geometry/color/alpha/UV output match approved baseline over 180 frames. Their gameplay is untouched.
- Fire charge gathers at the actual held tip with sparse embers; enclosing body surface removed.
- Review01 projectile/ring designs were rejected. Review02's Fire/Ice wall designs were also rejected; its Light wall is retained without implying acceptance. Review03 retains the native wall envelope (192 units high, 0.849× radius with original5% pulse) and gives Fire/Ice new private flow textures sampled in two independently scrolling layers. No Henriko or medallion flame texture is used. Gameplay radius/timing are unchanged.
- Ice now has a compact luminous head and a textured frost history wake for every shot, with small crystalline fragments. Side histories reconstruct the original center history using the authored±5460 yaw; local/remote positions, state buffers, networking and updates are unchanged. Remote head rendering follows the reconstructed direction, including stopped duplicate samples. Trails draw before heads. Fire's accepted surface/trail and Light remain unchanged.
- Attack resources live in `objects/nei_rod_attack`; source artwork and reproducible packaging live in `tools/nei_feedback_preview`. Original charge/portal resources are untouched. Missing attack textures fall back to colored translucent geometry rather than opaque white walls.
- Switch Hook swaps on firing; fresh target acquisition, native reach bound and charge rules remain enforced. Other hooks retain their launch behavior.
- Whip uses native forward reach with captured aim during release/extension/retraction, including the first third-person frame. Gameplay tip coordinates and camera transitions are unchanged.
- Time Gate item model suppressed throughout active cast, dialogue and cancellation. Portal/state logic unchanged.
- Lantern uses the selected model's closed left hand with native/Alt age/LOD fallback; accepted model placement and wrist transform unchanged.

## Evidence and limits

The earlier combined feedback cumulative gate passed, including lantern and corrected Switch Hook checks. Revision03 has focused effect, material, source-preservation and actual-object dispatch checks; full game link/runtime remains unperformed. The effect budget exercises15 projectile heads,15 Ice wakes (5 Fire/Light wakes), charge and released spin together across3 camera bases and180 frames. Offline previews sample production geometry/materials. Whip/lantern previews use actual source animation/player geometry with studio rendering. They are not in-game captures.

Deku Leaf first-ground-activation freeze/crash was unresolved in the published feedback build. Follow-up log `(2)` and the user's animation-completion timing led to a reproducible MM sound-stop infinite loop before first playback. This local candidate adds a readiness guard and links the real bank engine into the Leaf regression test; the previous no-op audio boundary hid the failure. Source tests pass, but runtime confirmation is pending. See `tests/nei_leaf/README.md` and `docs/poc/deku-leaf-cold-stop-20260928.md`.

Full game build/runtime, active user PAK configuration, first-person camera transition and crash reproduction remain unverified. Preview acceptance now includes the Fire projectile and revision03 Fire/Ice release plus Ice wake. Passing source checks and preview approval do not imply in-game acceptance or master promotion.

## Review corrections

- Review02 emitted the original shared center history. Review03 reconstructs each Ice shot's own history; Fire/Light keep the original shared history.
- Auto Switch Hook validates full 3D distance from the hook origin before spending a charge, preventing far-vertical-target swaps introduced by bypassing projectile travel.

## Reproduction

Run `scripts/diagnostics/run_cumulative_regressions.sh` with the pinned libultraship/ZAPDTR dependencies available. Standalone runners live under `tests/nei_used_fx`, `tests/nei_leaf`, `tests/nei_whip`, `tests/nei_lantern_grip`, and `scripts/diagnostics/run_{switch_hook_instant,time_gate_visibility}_tests.py`.

Rod preview: compile `tools/nei_feedback_preview/export.cpp` with C++20 and `-Isoh`, export its binary frames, then pass that file and an output directory to `tools/nei_feedback_preview/render.py`.

Before accepting a distributable build: reproduce first-use ground Leaf; inspect three-shot Fire/Ice plus all charged releases; release Whip level/up/down and return to normal actions; compare manual/automatic Switch Hook valid/invalid targets; hold/cancel Time Gate dialogue; inspect lantern grip with the user's PAK and native child/adult models.

## Review 02 draw-path investigation

GI17's player/custom-item/rod path reaches the Ice sampler; its design explicitly emitted a main crystal, orbiting crystals and trailing shards. Missing `ice_fracture` falls back to geometry, not an invisible mesh. Native particle rendering is a separate path, so the reported clip does not alone identify whether the custom mesh drew or which binary/archive was loaded.

A concrete visual overlap was found: Ice emits six native clumps per projectile per update, alongside custom geometry. The revision collapses only these obsolete flight-particle sizes to zero, retaining RNG consumption, spawn/lifetime behavior and all impact/frozen-enemy effects. Fire's superseded flight sparkle cloud is likewise zero-sized. Native effect implementations and shared textures are untouched. A production emission comparison verifies all inputs except visual size remain identical.

Real compiled Fire/Ice/Light object drawers now have dispatch tests covering local active sets, remote three-shot sync, missing held resources, absent wrist capture and inactive states. Fire object drawing no longer includes or references gameplay_keep flame display lists. Custom attack surfaces reference only `objects/nei_rod_attack`.

Review 02 validation: effect suite, new full-object dispatch cases, material provenance/alpha checks and flight-emission comparison pass. Exact user binary provenance and live GPU/Alt/PAK acceptance remain unverified. No push.

## Review03 reference and timing correction

Recovered `Medallion_Arrow_Textures.png` and `Magic_Overlays_Texture_Audit.html` from2026-09-25. The actual sheet and source show broad flowing shapes with independent scrolling samples. The original external inspiration was not recovered; do not invent one. This revision follows those visible material/motion principles, using original new release fields rather than reusing the forbidden Henriko flame.

The review02 preview was a slowed fixture and did not show native disappearance. Native `Player_Action_808502D0` checks the melee window before animation advancement; animation completion clears melee before custom-item drawing. `link_normal_Wrolling_kiru` has17frames and endFrame16. At normal20Hz and native animation speed, the large rod wall renders16updates (0.80s): radius80,110,...500,500. The preview now uses that window, with blank intervals separating repeats. This is a source-derived timing fixture, not a captured runtime proof; interruptions or custom animations can shorten/change the window.

Preview03 uses native15-unit projectile speed,30-degree yaw spread, six history samples and0.2approach toward scale2. Camera follows the center projectile; the volley panels show the first12updates. The right-hand one-head detail is deliberately held at mature scale for inspection. The PNG combines a mature volley and near-full wall and explicitly labels that comparison.

Accepted Fire geometry/color/alpha/UV are compared against a frozen review02 policy for180frames, four scales and three directions; its resource hash is pinned. Existing Light projectile and Ice/Light charge preservation remains covered. Budget maxima across camera bases: Fire39,024vertexbytes/1,710commands; Ice70,272/2,356; Light70,544/2,208, all under the existing90,000byte/2,400command limit. No render performance improvement or live GPU acceptance is inferred from this fixture.

Final revision03 checks exited0: `tests/nei_used_fx/run_tests.py`, `tests/nei_held/run_hand_fit_tests.py`, `scripts/diagnostics/check_time_pedestal_syntax.py`, and `git diff --check`. Changed C/C++ files were formatted with clang-format14. The preview was inspected as a still and at five release/termination frames; MP4 is1500×1000,20fps,180frames/9s. The user subsequently accepted Fire/Ice release and Ice-wake appearance in that preview. Runtime acceptance remains pending.

Publication packaging: five authoring PNGs are explicitly included despite the repository-wide PNG ignore rule; their native resource hashes remain unchanged. The source-preservation test uses published `c77c18587a976f6d6cb5c8f91f27593286469218` instead of its identical local-only mirror, so a fresh CI checkout can execute it.

Publication validation: the full cumulative regression gate exited0 on the combined hint-plus-feedback tree. The exact repository clang-format14 pass completed; its additional edits are formatting only, including two lines in the inherited hint fix. Approved rod policy/presentation and all five source/five packed textures match the reviewed checkout byte for byte. Dependencies remain pinned. Full Windows/Linux application builds are delegated to PR17 CI after the authorized push.
