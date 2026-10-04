# Cumulative feature continuity audit

Audited repository: `marsh6487/Shipwright`, working integration
`integration/nei-weather-static-actors` at
`0574f79cb3838fdf79a8466393bc73eda5ef2aa9`.
Scope: all 60 published branch heads returned for this repository, the previous
working-feature inventory, relevant production code and existing regressions.
The branch snapshot is in the table below. This is a source/history audit, not
runtime acceptance of every configuration or an audit of unpublished work.

## Confirmed omissions and repair

| Feature | Evidence at the audited tip | Carried-forward source |
| --- | --- | --- |
| Midna POC2/POC3 | Only POC1 behavior was integrated. The later production test fails because Midna still uses `LIGHT_POINT_GLOW`. | Merge `822f46b15dcae98b0a0564ec469a061f5106d4c4`: glow suppression, Primary/Secondary cosmetics and motes, pose selection, visible-only blink clock, movement cue spacing, idle yawn including passive NPC proximity. |
| Zora shield anchoring | The later world-space matrix test fails at the grounded origin. | Merge `5d0b8384d4885b4e1f54c041afcc432542848257`: feet anchoring on ground and the animated Zora root while swimming, including pitch and roll; retains size, color and human fallback. |

Both histories merge cleanly onto `0574f79c`. Production changes match the donor
versions, with clang-format 14 applied to the restored Midna source and tests;
all noncomment source tokens are unchanged by formatting. Other production
files remain identical to the audited integration. Original Midna tests passed
on the incomplete tip because they
predated the missing features. Restoring the newer tests reproduces both
regressions before the source merges and passes after them.

The supplied Midna POC3 archive and both game copies in the Combo integration
batch have identical SHA-256
`dbe4552da4110b845c00a78ea9b02dcc8239ec18214166120de46ddfc2f8c176`.
The archive was not the cause of the standalone glow regression. The executable
must select the POC2/POC3 resources for the corrected animation and body UVs.
Local point illumination is intentionally retained; the spherical fairy halo
is suppressed only when Midna loads. Native Navi fallback and other fairies
keep their original behavior. Combo/2Ship runtime observations do not establish
that the standalone SoH executable contains the same feature code.

## Retained work and older branch disposition

| Area | Finding |
| --- | --- |
| NEI, weather and night audio | Integrated implementations and later audio-capacity/handoff fixes are present. Older divergent weather snapshots are superseded; current rain ownership, thunder controls, placement pose/root protections and Ruto water cycle are present. |
| Static/MM actor catalogue | Expanded roster, native rendering/vertex offsets, HD eye metadata, Great Fairy mesh blinking, MMD Shop Gal eyelids/fitted feet, Lulu singing face, scene resource retention, Skull Kid/Tael/draw distance, and Anju/Kafei repairs are present. |
| PAK and equipment | Selection identity/migration and hide-back-equipment/scabbard option are present. |
| Native materials | Prelude scrolling, fountain motion, reusable caustics, and the corrected Lost Woods cache are present. The explicitly reverted first cache experiment is not restored. |
| Pedestal | Selected sword, child/adult ceremony, independent skips, fixed exit camera, prompt collision/clearance and child grip follow-up are present. Prior runtime interaction feedback remains open. |
| Cosmetics/effects | Custom heart/magic colors, dynamic transformation cosmetics, scene reentry behavior, Zora shield scale/colors, and HD spin/TorchFlame texture sampling are present. |
| New gameplay options | Contents-based chest sizing and RPG stat pickups are retained from `0574f79c`. |
| Old render-state branch | The unused `GetDrawObjectId` accessor and diagnostic crash-report experiment are not promoted as gameplay features. Current code resolves separate model/animation object requirements and retains the later actor lifecycle/render repairs. |
| Initial actor/Ruto/design branches | Early prototypes and design documents are superseded by the later implemented catalogue, native material, and Ruto water work. They are not wholesale merged over the newer implementation. |

No additional implemented gameplay feature omission was confirmed in this
published-branch audit. Asset-only work remains in its separate archives.
Unfinished custom-asset compatibility requests and active experiments are not
treated as completed features. No sacred scene/model archive was modified by
the source repair.

## Verification and prevention

- The original tip fails the new 25-baseline ancestry check for exactly Midna
  POC3 and Zora anchoring. The repaired candidate passes all 25.
- Eight real-Git checker tests pass, including divergent newer commits, merge
  parents, missing objects, invalid inventories, explicit refs and shallow clones.
- The cumulative regression runner passes: Midna draw/audio, Zora matrices and
  colors, dynamic cosmetics, RPG/stat persistence, chest sizing, pedestal
  interaction/sword/camera, full affected C syntax, and the actor/weather/audio/
  Time Gate/custom-item stabilization suite.
- Additional real-ImGui PAK/config checks and native-material scroll/profile/
  factory/cache checks pass. HD spin sampling source is retained unchanged;
  this audit does not claim a fresh spin renderer runtime test.
- Independent review found no substantive issue. GitHub's cumulative regression
  job passed on `5d934cc6`. The separate formatting job exposed three Midna files
  needing clang-format 14; the follow-up changes formatting only, with identical
  noncomment tokens and rerun Midna production diagnostics.
- `generate-builds` now requires the history inventory and production regression
  runner before archive generation and all platform builds. Full history is
  checked out. Update the inventory with subsequent integrated features.

Ancestry is a guard against omitted branches, not proof against later reverts.
Keep the behavioral regressions and source review. Build verification, exact
archive/mod-order runtime testing, and user acceptance remain separate gates.
Runtime checks still include Midna readability/blink/UVs/idle audio/Alt parity,
and Zora shield ground/swim alignment with the active model stack.

The optional [laugh-recall pack](../poc/midna-audio-swaps.md) changes one WAV only
and works without a new executable. The visual restoration needs the repaired
executable, using either the original complete POC3 pack or the laugh variant.

## Published branch snapshot

`Included` means the branch head was an ancestor of `0574f79c`. `Restored` names
one of the two missing follow-ups. `Superseded` identifies an older divergent
prototype/diagnostic whose relevant implementation was checked against the
current source, as described above.

| Branch | Head | Disposition |
| --- | --- | --- |
| `codex/combined-weather-actors-poc3` | `20869eec2656` | Included |
| `codex/concurrent-weather-base-adcdb7d` | `adcdb7d9dc13` | Included |
| `codex/concurrent-weather-poc1` | `37da4cd6dd6f` | Included |
| `codex/concurrent-weather-poc2` | `d54a4c33da15` | Included |
| `codex/fix-lost-woods-rain-ownership` | `60f0c11b5eac` | Superseded |
| `codex/fix-lost-woods-rain-ownership-red` | `60f0c11b5eac` | Superseded |
| `codex/fix-lost-woods-rain-ownership-test` | `60f0c11b5eac` | Superseded |
| `codex/fix-ruto-tread-shin-orientation` | `e6972c1e0241` | Superseded |
| `codex/fix-static-actor-render-state` | `6ba20404420e` | Superseded |
| `codex/fix-static-pose-regressions` | `60f0c11b5eac` | Superseded |
| `codex/hyrule-field-night-audio-poc5` | `3f3de1c5eef9` | Superseded |
| `codex/integrated-weather-great-fairy-no-mm` | `6b142601afaf` | Superseded |
| `codex/poc5-hallmark-fixes` | `b453c1aceccc` | Superseded |
| `codex/poc5-stabilization` | `50b407983082` | Superseded |
| `codex/poc6-actor-expansion-stable` | `2ad8fe19209e` | Included |
| `codex/poc6-actor-render-diagnostics` | `01a75b87ee46` | Included |
| `codex/poc7-weather-audio-diagnostics` | `398b22212e99` | Superseded |
| `codex/poc8-weather-audio-stabilization` | `728cbd142083` | Superseded |
| `codex/poc9-actor-adapter-stabilization` | `7e942773837d` | Included |
| `codex/poc10-ruto-zelda-stabilization` | `a678d5d3267f` | Included |
| `codex/poc11-actor-tracking-ruto-polish` | `fe46f0081603` | Included |
| `codex/poc11-final-weather-refinements` | `d72c7dc2e105` | Superseded |
| `codex/poc12-ruto-final-stabilization` | `5276faae8839` | Included |
| `codex/poc12-weather-thunder-color-fix` | `06062933b5ac` | Superseded |
| `codex/poc13-great-fairy-facial-stabilization` | `51d407e0bb65` | Included |
| `codex/poc14-final-actor-expansion` | `9c90323960b6` | Included |
| `codex/ruto-water-cycle-poc` | `3f20787d9c2b` | Superseded |
| `codex/static-story-actors-poc1` | `b1c238fa206a` | Superseded |
| `codex/static-story-actors-poc4` | `b13d157843d4` | Included |
| `design/mm-catalogue-fountain` | `7f196e03a031` | Superseded |
| `develop` | `67191665dfa7` | Included |
| `feat/anju-umbrella-kafei` | `cf1ccfa77736` | Included |
| `feat/lost-woods-materials-time-pedestal` | `bcde7a53ea26` | Included |
| `feat/reusable-water-temple-caustics` | `8db6380f677f` | Included |
| `feat/skull-kid-3ds-tael` | `e4e71eeea6e1` | Included |
| `fix/great-fairy-charcoal-blink` | `4658ac61c502` | Included |
| `fix/lulu-shop-gal-hd-blinking` | `8092059a41fe` | Included |
| `fix/mm-actor-rendering` | `c8a3186eaefd` | Included |
| `fix/mm-scene-reentry` | `9f95b9fc9b6e` | Included |
| `fix/pak-menu-selection-and-back-equipment` | `61bf5bad25fe` | Included |
| `fix/pedestal-prompt-child-grip-20260922` | `7eefc9986d4b` | Included |
| `fix/shop-gal-grounded-feet` | `d6556026f3a7` | Included |
| `fix/shop-gal-mmd-draft` | `b89e314ac8fb` | Included |
| `fix/zora-shield-anchor-20260922` | `5d0b8384d488` | Restored |
| `integration/nei-weather-static-actors` | `0574f79cb383` | Included |
| `poc/heart-magic-cosmetics` | `8481c2575973` | Included |
| `poc/lost-woods-cache-animations` | `4a16cd05ce90` | Included |
| `poc/lost-woods-rain-small-fixes` | `c433b6c7e083` | Included |
| `poc/midna-navi-soh-poc2` | `822f46b15dca` | Restored |
| `poc/pedestal-camera-build-fix` | `7a31560dfec8` | Included |
| `poc/pedestal-exit-camera` | `0b6814bf1fa0` | Included |
| `poc/pedestal-proximity-fade-sword` | `a42472291f7f` | Included |
| `poc/pedestal-stump-clearance` | `6f03c439ab83` | Included |
| `poc/soh-stat-items-20260923` | `d676a5ffd2f6` | Included |
| `poc/spin-reimagined-20260921` | `0b745bc8b9ce` | Included |
| `poc/transform-cosmetics-20260921` | `74edaf1019df` | Included |
| `probe/prelude-native-materials` | `0a4dc0641c3a` | Included |
| `test/adcdb7d-base-npc-regression` | `adcdb7d9dc13` | Included |
| `work/mm-actor-candidate` | `286a92d32d34` | Included |
| `work/mm-catalogue-fountain-recovery` | `60a53ece0684` | Included |
