# Din Fire Weapons Combined POC Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans; implement inline and obtain one fresh final review.

**Goal:** Add an opt-in matching fire sword and deliver it with the HD shield and charging-only Shield SFX in one final build and archive.

**Architecture:** Preserve the existing Din sword/hilt. Draw a close-fitting hot blade layer and translucent animated flame shell from the gameplay left-hand matrix, using private child/adult meshes derived from each actual equipment pack. Whole-image named resources carry 1024px weapon textures; stable native logical UVs animate toward the tip.

**Tech Stack:** C/GBI, existing player limb hooks and cosmetics, Python/Pillow, o2r resources.

**Spec:** User explicitly requested fire-sword implementation with the shield updates, no further separate pushes. Shield code had already reached commit918c3253 before that instruction. The next push must contain the complete sword and preserve those shield changes. Shield SFX remains the exact charging-only Fire Arrow cue, default off, labeled "Shield SFX".

## Global Constraints

- Base: 918c32539d0e5cf1edec7eaf9662db6516ae3762; all prior cumulative work remains.
- Sword is visual by default. User subsequently requested opt-in Fire Damage: direct sword hits use the native Fire Arrow damage table; reach, swings and stowed equipment remain unchanged. Charged spin-wave actors retain their original behavior.
- Preserve source equipment archives and all existing shield resources unchanged.
- Respect transformed/hidden/dead/underwater/cutscene states, other weapon owners, PAK overrides, and time-pedestal ceremonies.
- Distinct default-off Din Fire Sword checkbox; separate core/outer cosmetics with shield-matching defaults.
- Shader has no256px cap; paired sword textures1024px, shield/icon256px.
- No more pushes until sword and shield checks/review finish. Runtime remains a POC.

## Review Focus

- Child/adult alignment derives from the correct source meshes; hot layers must avoid z-fighting.
- Left-hand hook must skip alternate weapon owners and ceremonies, and keep the limb matrix intact.
- Opaque hot core and translucent flame shell must restore render state and preserve depth behavior.
- Missing assets and disabled options fall back to the unchanged original sword.
- Pause, duplicate draws, scene/age changes and reflective passes must not change animation unexpectedly.

## Task 1: Sword renderer and controls

**Files:** new `soh/include/din_fire_sword.h`, `soh/src/code/din_fire_sword.c`, `soh/tests/din_fire_sword_test.c`, `scripts/diagnostics/run_din_fire_sword_tests.py`; player update/reset/left-hand hooks; UI and cosmetic entries.

**Interfaces:** `DinFireSword_Reset(void)`, `DinFireSword_Update(PlayState*, Player*)`, `DinFireSword_Draw(PlayState*, Player*)`. Known child/adult geometry is extracted offline (child POC2 includes the longer blade). Full/broken BGS use user-approved approximate vanilla proportions. No arbitrary runtime mesh detection is claimed. Draw consumes the current gameplay left-hand matrix, never changes collision/equipment state, and is called only for the ordinary sword hand after weapon ownership decisions.

- [ ] Write a real-C fixture exercising default-off/Alt/age/action/eligibility, missing dependencies, exact HD texture handles, balanced matrices, matching colors, pause/context changes, and render idempotence. Run before implementation; expected missing renderer failure.
- [ ] Implement the minimal renderer and hooks. Pass `__OTR__` aligned names for both textures. Opaque core uses actual blade silhouette; translucent shell uses the expanded private mesh.
- [ ] Add default-off Din Fire Sword option and Din Fire Sword Core/Outer cosmetics.
- [ ] Run the fixture; expected all cases pass. Add to cumulative gate.

## Task 2: Combined asset candidate

**Files:** `scripts/assets/build_din_fire_weapons.py`, independent validation and offline preview tooling.

**Interfaces:** inputs are the validated HD256 shield plus immutable child/adult equipment packs and full-resolution core/rim art. Output includes shield resources byte-for-byte plus eighteen private sword resources and a manifest.

- [ ] Derive private child/adult hot-core and flame-shell vertices from each packed blade; author S across width and T along local+X toward the tip.
- [ ] Encode1024px grayscale intensity as whole-image RGBA with intensity alpha, flags3/unit scales.
- [ ] Independently check dependencies, indices, original archive/resource hashes, all shield bytes, and unchanged hilt/grip/stowed resources.
- [ ] Render both ages from actual candidate geometry and texture pixels; inspect for recognizable blade/fire and preserved hilt.

## Task 3: Single final push and handoff

- [ ] Run shield/sword fixtures and the cumulative regression gate. Expected all26 historical baselines retained and existing checks pass, known skip recorded.
- [ ] Fresh review of the final source and artifact boundary. Resolve important findings before pushing.
- [ ] Fast-forward cumulative branch once with the sword on top of the shield update; verify exact remote tree and resulting build.
- [ ] Deliver one combined archive, reproduction/evidence bundle, install instructions, and honest build/runtime status.

## Pre-push evidence

Cumulative regression suite passed with all26 required baselines retained and one pre-existing skipped test. Real player translation units compile. Independent archive validation passed for all18 sword resources, four profiles,1024px textures and byte-preserved HD256 shield resources. Offline geometry/pixel previews inspected. Fresh review found and resolved guarding visibility (`heldItemAction`) and crouch-stab fire toggle restoration. Ordinary and fidget idles share the hand attachment without an animation whitelist. Runtime appearance, sound and enemy reactions remain unverified.

## Follow-up: visible custom pedestal and formatting gate

User requested fire on the Room10 custom pedestal after db3f11fc, noting the absent adult sword must stay absent. The actor now calls the shared adult fire layers only when its custom selected sword is drawn, before its existing matrix pop. No standalone flame actor, actor-visibility changes, progression changes or native Temple of Time hook. Arbitrary replacement proportions remain approximate. Fresh reviewer found no blocker; actor and renderer fixtures pass. clang-format14 is applied with the repository's complete source selection, including inherited formatting violations.

## Follow-up: child Din flames suppressed by PAK slot mixes

Runtime report on df2e464: child Din's sword is visible, but its fire is absent. The supplied log confirms the combined archive and child equipment archive load, and that other equipment slot overrides remain after Sword0 is reset. `PakLoader_HasActiveModel` includes any nonempty slot mix, so its blanket exclusion prevented the fire hook even when a different slot owned the override. Remove that exclusion from the hand hook and optional damage eligibility without changing selected equipment. The existing Odolwa/Goht exclusions now live in shared eligibility so they also protect rendering when a PAK sword bypasses the ordinary hand-type rewrite.

The actual post-hand hook regression and child damage regression both failed before the fix and pass afterward. Review identified the Boss Remains interaction; independent Odolwa/Goht rendering tests reproduced it and now pass. The cumulative suite passed with all26 historical baselines retained and its one existing skip. The complete clang-format14 selection passed; the final ownership adjustment also passed the real sword fixture and formatting check. Fresh review found no remaining blocker. No asset changes are needed; reuse the combined archive. Runtime visibility after this code fix still requires user confirmation.
