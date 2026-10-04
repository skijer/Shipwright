# Lost Woods rain and small fixes candidate

Cor approved the rain probe and requested that the Time Gate chest, Treasure
Gal eyes, and child Ruto face regression move together. This candidate bundles
those changes; none is declared visually accepted by offline tests.

## Code scope

- Native weather actors register independent rain requests. One leaving or being
  destroyed cannot clear another's rain. Scene entry/exit resets requests, and
  revisiting persistent actor placements does not accumulate duplicates.
- An explicit policy from the loaded Lost Woods scene's owning archive requests
  continuous density 25 throughout normal child/adult scene layers. It supersedes
  the optional global intermittent cycle. The policy is read once on scene load,
  from the exact normal/Alt resource's archive; unrelated archives cannot opt in
  vanilla Lost Woods.
- Resolve rain before the existing particle-count update. Sky restoration cannot
  erase active intent. Explicit cutscene weather commands retain precedence.
  Underwater rendering retains its native gate; logical rain is independent.
- Reconcile lightning and overcast after competing scripted writers. Finish an
  active flash through LAST before releasing it; reacquire requested thunder
  after Song of Storms cleanup. Preserve private rain audio and live controls.
- Log policy binding, room/owner/source changes and rain drawing gates through the
  regular game logger (`[rain-probe]`). Source codes: 0 none, 1 placed actor,
  2 optional outdoor cycle, 3 authored scene. Camera logs distinguish main eye,
  water height and rendered-view eye height.
- Give the verified adult Zelda/Lullaby chest Time Gate in normal and randomizer
  saves. Preserve switch 32 and treasure flag 5. Existing collected chests remain
  collected. Handle extended-item obtainability safely before vanilla lookups.
- Bind child Ruto's mouth to native segment 0x09 instead of binding a second eye.
  See the separate chest and child Ruto evidence notes for exact scope.

## Companion asset candidates

Use the new executable with `Lost_Woods_Continuous_Rain_POC3.prelude.o2r` in place
of the preceding Lost Woods candidate. Its only added entry is
`custom/prelude/spot10_scene/weather.json`. Every one of POC2's **6,412 existing
entry payloads** is byte-identical, preserving parity, caustics and the adult
room-10 doorway correction. Old builds ignore the new policy.

Choose one Shop Gal alternative, replacing the corresponding active archive:
`MMShopGal_OG_EyeContrast_POC1.o2r` or `MMShopGal_GOTH_EyeContrast_POC1.o2r`.
Only the eye material in four head lists changes. All textures, geometry,
animations and blink geometry remain byte-identical; final material state is
restored for subsequent limbs. Do not load both variants simultaneously.

| Candidate | SHA-256 |
| --- | --- |
| Lost Woods POC3 | `988c7c0a49e43acdc941894de9ba391f32831a302205e6699251ea80d83979d6` |
| Shop Gal original | `ca136d78e623e471a7da1134da28f66af7cd0c3bedcc6b0cac70e2b6fb0c76b2` |
| Shop Gal goth | `1b232ed667711d59b4bf5fb80ce8535b47d685cbe4d478a22bb3637d40712285` |

## Verification and review

The configured `scripts/diagnostics/run_stabilization_tests.sh` passes, including
real weather audio, actor/environment/lightning functions, parser and owning-
archive binding tests, actual scene-init argument compilation, chest/receipt
and bounds checks, child Ruto's nine pose/blink combinations, and existing
stabilization coverage. Set `SOH_TEST_JSON_INCLUDE` if nlohmann/json.hpp is not
on the compiler's default include path. Full environment/cutscene and policy
translation-unit syntax checks also passed. Existing viewer warnings remain.

Fresh review reproduced and then verified fixes for scripted lightning cleanup,
mid-flash release, thunder reacquisition, overcast reacquisition, the actual
SaveContext scene-layer field, and Time Gate's extended-inventory bounds. Final
review finds no unresolved correctness issue for candidate publication.

This commit does not establish a full executable build or runtime acceptance.
The separate branch's build and the user-facing handoff record those results.

## Runtime acceptance still required

- Lost Woods child/adult, Alt on/off, global rain off/persistent/intermittent;
  0→9 pool, submerge/surface, 3→4→7→8→10 and return, stationary room 10,
  revisits, pause, reset/re-entry and pedestal age swap.
- Rain/BGM/SFX coexistence, volume and thunder controls; leave the authored zone
  and confirm its rain does not follow into unrelated scenes.
- Adult Zelda: play Zelda's Lullaby, open the still-uncollected chest, verify
  Time Gate model/text/use and save/reload. The accepted R4.1 placement exists in
  the Alt adult header; normal-save support does not add a missing scene actor.
- Shop Gal original/goth at normal camera distance and first-person, blinking,
  dark/bright lighting; child Ruto with the installed normal/Alt pack and all
  three poses. The pictured editor preview is not verified by the fork tests.

Pedestal exit polish and the child's downward sword grip remain open. Loading
optimization stays shelved; the missing-dungeon-geometry report is unresolved.
