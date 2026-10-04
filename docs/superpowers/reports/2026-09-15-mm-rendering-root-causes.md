# Skull Kid and HMS rendering corrections

Smoke-test log identifies work/mm-actor-candidate commit286a92d. User screenshots show scrambled Skull Kid triangles and missing HMS head/limb surfaces with backpack and hands visible. Keaton and Lulu render correctly; Kafei is visible in the supplied screenshot, so earlier invisibility is not declared resolved or assigned a cause.

## Demonstrated defects

1. The private Skull Kid patcher treated G_VTX_OTR_HASH w1 as unused. Fast's actual gfx_vtx_hash_handler_custom adds it as a byte offset. The donor contains151 vertex loads across40 object_stk display lists;111 have nonzero offsets. The old patcher repeatedly selected the array start. The correction retains byte offsets and checks alignment, remaining extent and pointer-addition overflow before binding.
2. The previous segment0C-to-culling interpretation was wrong for these actors. MM EnOsn_Draw and DmStk_Draw call Scene_SetRenderModeXlu(play,0,1) in their opaque branches. That table consists of four ENDDL entries; indexed calls0/2 terminate without changing state. Skull Kid now resolves its private calls to that stable table. HMS binds the same table immediately before drawing rather than inheriting another actor's0C state.
3. Skull Kid's native opaque branch also synchronizes the pipeline and sets environment RGBA to255. The adapter now does so explicitly, avoiding inherited material color/opacity.

Source references: libultraship pinned c57da1b4afa775b24b58b2adf93d63d3b561bb65, src/fast/interpreter.cpp gfx_vtx_hash_handler_custom and gfx_dl_index_handler; native zeldaret/mm src/overlays/actors/ovl_En_Osn/z_en_osn.c, ovl_Dm_Stk/z_dm_stk.c and src/code/z_scene_proc.c retrieved September15,2026. The previous reports and comments calling these commands culls are superseded by this source-backed correction.

## Verification

`python3 scripts/diagnostics/run_mm_rendering_regression.py ../donor-inputs/mm.o2r`

Passes actual151 vertex-load pointer/byte comparisons,111 nonzero offsets, bounds tests, production viewer function fixtures, actual four-command opaque table and full viewer translation-unit syntax checking against game headers. Native matrix segment0D remains owned by flex drawing; HMS face segments08/09 remain bound. The fixture verifies segment0C precedes HMS skeleton drawing and Skull opaque pipe-sync/environment packets precede drawing. Existing ordinary actor and Kafei lifecycle checks remain in that fixture.

The revised vertex-offset assertion fails against the pre-fix patcher. The revised HMS draw-boundary assertion fails when the fixture uses the pre-fix viewer. Both pass after corrections. An independent bounded review checked native render setup, actual archive segment references, flex matrix-slot reservation for Skull's substituted head, and retained private vertex ownership; no blocking defect found. A real normal-build smoke test is still required. Headless checks do not establish visible rendering, Alt-pack appearance, multiple-instance/re-entry behavior, or resolve Kafei's earlier intermittent report.

No changes to Keaton/Lulu behavior, actor placement, Phantom model Y anchor, fountain assets, audio, or global MM player rendering are included.
