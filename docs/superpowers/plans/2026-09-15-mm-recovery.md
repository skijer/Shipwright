# Approved MM catalogue and fountain implementation

User approved implementation. Previous unpublished checkout was pruned; reconstruct source from baseline 0a4dc0641. Do not repeat content/design approval gates. Implement serial batches, review each, preserve recoverable commits.

## Global constraints
- Preserve existing params, placements, collision, Skull Kid private display-list/vertex safeguards and native fairy companion.
- Phantom Ganon model-only shapeYOffset1000 at scale0.01 remains a10-world-unit lift; never change authored world Y.
- Generalized night music excluded; existing Hyrule Field/audio/weather fixes remain.
- Lulu singing is animation-only, no audio/ocarina/quest state.
- Fountain means MM animation on existing pasted water, retaining geometry/art/UV/placement. No new spray surfaces or global Lake remap.
- No original archive edits. Normal game build, no diagnostic-only game binary. Never claim visual runtime from CPU tests.

## Task 1: Dialogue and Phantom safeguards
Implement all approved dialogue in one batch. Existing actor selectors: Impa708D before metZelda/708E after; Adult Zelda70FF/70FE; Sheik700F/7010 unchanged. Phantom selects0 and cannot offer/process talk, including existing talking-state branch. Add pure CanTalk(type) and early viewer guard; preserve shape/collider/attention.

Six isolated custom IDs (recheck collision against repository and supplied OOT table):
| ID | Actor | Exact text |
| --- | --- | --- |
| 0x8F20 | Treasure Chest Shop Gal | Finding out what's inside is half the fun! |
| 0x8F21 | Skull Kid | That won't do you any good. Hee, hee. |
| 0x8F22 | Keaton | We Keatons can recognize our own by the sheen of our tails. |
| 0x8F23 | Happy Mask Salesman | You've met with a terrible fate, haven't you? |
| 0x8F24 | Child Kafei | I've made a promise to Anju. |
| 0x8F25 | Lulu | Pleased to meet you. I'm Lulu. |

Use C-compatible catalogue beside static_story_actor.c and existing CustomMessage/GameInteractor OnOpenText ID hooks. Current language passed to dispatch; English fallback every language. AutoFormat + LoadIntoFont, ordinary END only; no imported MM controls/followups. Hook executes before message-table lookup and disables table loading after successful dispatch. Verify real header/API compile and hook boundary tests. Append identities Keaton20/Kafei21/Lulu22 but keep unavailable with OBJECT_INVALID/no adapter and no new param decode until real adapters exist.

Migrate actor assert checks to REQUIRE so NDEBUG still executes them, preserving old checks. Test all16 progression combinations, NULL/invalid, exact text, hook dispatch, nonzero language, Phantom real descriptor scale/offset/collider and production talk function via extracted behavioral fixture. No test-only rewritten talk logic. Focused tests then stabilization once. Record actual commands/results; local exact-path commit.

## Task 2: Ordinary HMS/Keaton/Lulu
HMS19 params7E09/19/29, Keaton20 params7E0A/1A/2A, Lulu22 params7E0C/1C/2C/3C. Kafei21 reserved disabled. Preserve roots and ordinary loops, Lulu87-frame raw look-around loop pending visual seam evidence.
HMS object_osn/gHappyMaskSalesmanSkel flex18/17; Idle/HandsClasped/ArmsOut Anim29frames each; eyes08 ClosedHappy, mouth09 Smile; head11 tracking idle only.
Keaton object_kitan/gKeatonSkel flex20/20; Idle36/Chuckle36/Celebrate30; no face segments/tracking.
Lulu object_zov/gLuluSkel flex22/21; LookDown30/LookLeftLoop30/SingLoop72/LookAround87; eyes09 Open/Half/Closed, mouth08 Closed/Open; torso11/head12 tracking quiet only. Singing open eyes/mouth, no audio. LookAround face sequence derive actual donor evidence; never zero root.
Typed resource validation before raw casts, actual child kinds and pointer ownership, per-instance faces/joints, safe init/destroy. Real archive integration + compiled production viewer lifecycle tests. Update parameter docs. Do not enable missing-resource placeholders.

## Task 3: Isolated Kafei
Params7E0B/1B. Scoped MM Link wrappers objects/gameplay_keep/gPlayerAnim_link_normal_wait_free (89 frames) and gPlayerAnim_al_yareyare (48); OPAM misc/link_animetion/<same>_Data,67s16/frame=22Vec3s+separate appearance. Never overflow22-joint table with appearance or invoke Player/Link hooks/queue/global animation factory.
Bounded private wrapper parse + existing PlayerAnimation factory after payload size validation; retained immutable clip, per-instance cursor/joints. Native LOOP delta speed*(R_UPDATE_RATE*.5), integer frame samples, safe wrap. InitFlex(NULL animation) storage; DrawFlexLod18,lod0. Root local FLOAT pos once*11/17; world/home unchanged. Neutral face fallback, eyes08 eight slots/mouth09 four; no pendant.
Actual Kafei OSKL header misleading Standard tags: exact tuple(1,1,21,18,1,21),983 bytes; all21 actual OSLB children LOD with18 near/far slots and acyclic complete hierarchy. Validate actual typed children, finite known donor exception, no broad relaxed validation.

## Task 4: Skull Kid texture metadata and Tatl
Preserve private nestedDL/vertices/culls/stable buffers. Raw ImageData texture replacement currently loses metadata. Use existing G_SETTIMG_OTR_FILEPATH opcode25 and ResourceManager CacheExternalResource with stable private alias/base+alt alias immutable full Fast::Texture snapshots. Select validated exact canonical texture override/private MM fallback; alt/canonical override/base fallback. Geometry stays private. Retain alias strings/pixels/resources process-lived and republish evicted aliases before draw. No renderer rewrite.
Tatl native skeleton NORMAL14; glow callback index8, zero local transform, DL applies billboard segment01 and branch08. Add real7-argument OverrideLimbDraw to existing SkelAnime_Draw: preserve world origin, Matrix_Translate NEW, absolute scale .012*(1+.1*sin phase)*(actor.scale.x*124.99999)*existing presentationScale. Returnfalse; wings/anchor/stable08 unchanged. Test real callback matrix math and actual ResourceManager/renderer handler, base/Alt toggles/lifetime/privategeometry. Stock missing pixels may have additional cause: visual acceptance outstanding until observed.

## Task 5: Portable MM fountain animation
Per-material nativeAnimation metadata version1, binding material-motion, bounded source mm.bg_keikoku_spr.lower_a/lower_b/central and logical dimensions32/64. Absent metadata preserves old behavior, malformed explicit declaration fails closed. Direct single MM donor chains object_keikoku_obj_DL_000100/300/500 map64 variants.
Native64 second-layer Y rates -40/+40/+20 quartertexel/frame; adapted32 rates -20/+20/+10 preserve normalized phase/cadence. First layer stationary, gameplayFrames, phase0. Extend stable12-command profile buffers safely, strict dualtile extent/wrap/insertion validation; no geometry/texture/UV edits.
R5 six Lake-chain water resources are flat night70/78+day100 and raised night77/85+day107, four physical fountains;18 adjacent stone rows remain unchanged. Explicit binding flat lower_a32/raised central32. Provide COPY-producing postprocessor, portable geometric/material association not scene/path whitelist; retain supplied originals. Existing raw metadata lacks association; require verified complete MM stone+water topology pairing, reject ambiguous/incomplete groups, support merged day copies and retextures. Tests normalized phase/periods, stable buffers, negative malformed metadata/commands, unrelatedLake, renamed/translated/copied exports, archive payload equality exceptedits.json.

## Task 6: Integrated verification and handoff
Independent review after each batch, final broad review, fix concrete findings. Full normal build and tests where supported. Runtime visual checks only if actually observed. Preserve reviewed work outside transient workspace on isolated development branch; never merge/change user's baseline automatically.
