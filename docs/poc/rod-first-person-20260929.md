# Final first-person rod correction, 2026-09-29

Baseline: accepted SoH GI candidate `5fadb2d07f0e768974dd62c278220245d1539aaf` (PR build `d730a94`). Cor reports that Deku Leaf no longer crashes. Third-person rod alignment is accepted on another model; Din's wrist orientation is separate model work.

Scope: suppress only the held Fire/Ice/Light model and attached tip effects while the respective first-person aiming flag is set. The gate covers replacement and legacy fallback draws. Projectile/trail rendering continues after it; aiming controls and reticle are untouched. Remote visual sync already clears these local first-person flags.

Regression: real object drawers tested for both ages, all three elements, available/missing held assets, captured/missing wrist, camera transition/settled aim, single-shot visibility, and return to third person. The new first-person assertion failed on the accepted baseline and passes after the draw gate. Existing wrist/grip/projectile dispatch checks pass.

Runtime status: the Leaf fix is user-reported passing; first-person hiding is source verified and awaits the next build. All previously accepted effects and third-person transforms remain.

Independent review caught the shared Four Sword clone state: guards now suppress only GET_PLAYER(play). The added nonlocal-player fixture fails before that restriction and verifies clone held models remain visible during local aiming.
