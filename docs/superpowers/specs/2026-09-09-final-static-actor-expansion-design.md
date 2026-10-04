# Final Static Actor Expansion Design

## Goal

Finish the static `En_Viewer` actor catalogue with five presentation-only additions: the Majora's Mask Bomb Shop Lady, adult human Ganondorf, Phantom Ganon, Skull Kid with decorative Tatl, and the Happy Mask Salesman. The actors must remain safe static scene dressing, preserve resource-pack parity, and omit native gameplay or cutscene machinery.

## Scope and Parameter Map

The expanded parameter prefix remains `0x7E00`. The high byte selects the actor and the low nibble selects its pose, matching the existing catalogue decoder.

| Parameter | Actor | Presentation |
|---|---|---|
| `0x7E05` | MM Bomb Shop Lady | Full-body idle |
| `0x7E15` | MM Bomb Shop Lady | Idle while holding bag |
| `0x7E25` | MM Bomb Shop Lady | Sway loop |
| `0x7E06` | Adult Ganondorf | Human standing idle, without dynamic battle cape |
| `0x7E07` | Phantom Ganon | Neutral floating idle |
| `0x7E08` | Skull Kid and Tatl | Reclining, crossed-leg hover |
| `0x7E18` | Skull Kid and Tatl | Upright, arms-crossed hover |
| `0x7E09` | Happy Mask Salesman | Cheerful idle |
| `0x7E19` | Happy Mask Salesman | Hands clasped |
| `0x7E29` | Happy Mask Salesman | Theatrical arms-out pose |

Beast Ganon is explicitly excluded. `0x7E06` uses the normal adult human `gGanondorfSkel` and never invokes transformation logic.

## Implementation Order

Implementation and review proceed in this dependency order:

1. MM Bomb Shop Lady, proving the archive-scoped MM full-skeleton adapter.
2. Skull Kid and Tatl, extending that adapter with pose-local hovering and a second decorative skeleton.
3. Phantom Ganon, establishing the first new OoT-backed special render contract.
4. Adult human Ganondorf, reusing the proven viewer face contract without Beast Ganon or dynamic cape state.
5. Happy Mask Salesman, completing the catalogue on the already-proven MM adapter and facial-segment path.

Each stage must leave its focused tests green before the next actor begins.

## Architecture

### Catalogue Integration

Each actor receives a normal `StaticStoryActorDefinition` and pose descriptors. OoT-backed Ganondorf and Phantom Ganon use the ordinary object-slot loading path. The three MM-backed entries use one small MM adapter boundary responsible for loading skeletons, animations, and display-list resources from the mounted user-produced `mm.o2r` through the existing `MmAssets_*` archive-scoped loader.

The MM boundary must not expose raw resource lookup throughout `z_en_viewer.c`. It should provide actor-specific initialization, update, and draw entry points so archive ownership and failure handling remain localized. This also prevents shared paths such as `objects/gameplay_keep` from accidentally resolving to OoT resources when an MM skeleton expects MM limb display lists.

### MM Resource Ownership

The build already requires a user-generated `mm.o2r`; this feature redistributes no Majora's Mask assets. All MM skeletons, animations, textures, and limb display lists are resolved from that mounted archive or compatible MM override archives.

Each MM actor owns its loaded skeleton state for the lifetime of its `En_Viewer` instance. Skull Kid additionally owns a second `SkelAnime` for Tatl. Destroy logic releases or tears down both animation states without changing global companion or scene state.

Missing, malformed, or incompatible MM resources fail closed: the placement remains non-drawing, non-targetable, non-colliding, and non-talkable. It must not substitute an OoT resource with the same path or leave a partially initialized actor alive.

## Actor Contracts

### Bomb Shop Lady

The adapter uses `object_bba`'s complete 18-limb `gBombShopLadySkel`, including legs and feet. Its three poses use `gBombShopLadyIdleAnim`, `gBombShopLadyIdleHoldingBagAnim`, and `gBombShopLadySwayAnim` respectively.

The neutral idle may use conservative head-and-torso tracking. The holding-bag and sway poses remain animation-authored to avoid contorting the prop relationship. The model's fixed eye presentation is preserved; no synthetic blink flipbook is introduced.

### Adult Human Ganondorf

The adapter uses OoT's `gGanondorfSkel`, `gGanondorfStandIdleAnim`, `gGanondorfNormalEyeTex`, and the existing separate `gGanondorfEyesDL` face contract. It is a single standing-idle presentation with no tracking.

The native dynamic cape actor is omitted. `En_Ganon_Mant` has large mutable state and the existing viewer code stores its instance globally, which is unsafe for arbitrary catalogue multiplicity. The body model, armor, facial presentation, and cape fastening remain; only the separately simulated battle cloth is absent. No boss AI, magic, transformation, combat effects, Zelda dependency, or fight state is initialized.

### Phantom Ganon

The adapter uses OoT's `OBJECT_GND`, `gPhantomGanonSkel`, and `gPhantomGanonNeutralAnim`. It loops in place with no tracking, horse, projectiles, battle AI, or encounter effects.

Its special segment `0x08` receives an explicit harmless display-list value suitable for the neutral model. It must never inherit stale segment state from another draw call.

### Skull Kid and Tatl

Both Skull Kid poses use MM's `gSkullKidSkel` and always render Majora's Mask through the native head/post-limb contract. The reclining entry uses `gSkullKidRecliningFloatAnim`; the upright entry uses `gSkullKidFloatingArmsCrossedAnim`. Neither pose applies head tracking.

Skull Kid hovers relative to the authored Prelude Y coordinate using the native Clock Tower cadence: a phase increment equivalent to `0x4B0` and approximately `±10` world-unit vertical displacement. Placement never drifts because every update derives Y from the stored authored position.

Tatl is an embedded decorative secondary skeleton, not a spawned `En_Elf`. It uses MM's six-limb fairy skeleton and idle animation from archive-scoped `gameplay_keep` resources. Tatl receives native-inspired translucent rendering, warm Tatl colors, a triangular outer-alpha pulse, and subtle wing/body scaling. She has no companion AI, navigation, hints, dialogue, collision, targeting, save state, or scene-affecting point light.

Tatl appears in both entries. Her orbit center is pose-specific: above and behind the shoulder/mask for the reclining pose, and beside the raised shoulder for the upright pose. Her position is derived from the Skull Kid placement plus the same hover displacement, then augmented with a small local orbit. This keeps the pair visually attached without allowing cumulative drift or mesh intersection.

### Happy Mask Salesman

The adapter uses MM `object_osn`, `gHappyMaskSalesmanSkel`, and the native backpack-bearing model. The three loops use `gHappyMaskSalesmanIdleAnim`, `gHappyMaskSalesmanHandsClaspedAnim`, and `gHappyMaskSalesmanArmsOutAnim`.

The idle uses a stable cheerful face and may use conservative head-and-torso tracking. Hands-clasped and arms-out remain animation-authored with tracking disabled. Facial segments must be assigned explicitly on every draw so they cannot inherit stale state. No cutscene controller, inventory exchange, transformation logic, or scripted movement is retained.

## Interaction and Rendering Rules

All five entries use the established static-actor talk/collision contract only after successful initialization. Dialogue remains a safe existing-message assignment and does not invoke native actor action functions, item rewards, cutscene triggers, or progression mutations. Tracking is enabled only for the explicitly approved neutral poses.

OoT actors preserve ordinary OoT resource-pack overrides through symbolic asset paths. MM actors preserve parity with replacements supplied through the mounted MM archive/override stack. The implementation must not cache resolved display-list pointers globally in a way that prevents runtime resource replacement or crosses actor instances.

All draw-specific segments are assigned on every relevant OPA or XLU path before skeleton drawing. Tatl uses the translucent path; other actors use the path required by their native model contract. Rendering one catalogue entry must not change the next actor's face, eye, texture, or segment state.

## Error Handling

- Reject unknown poses during parameter decoding instead of indexing beyond a descriptor table.
- Treat unavailable OoT object slots or MM resources as a recoverable, invisible placement rather than dereferencing partial state.
- Initialize interaction flags only after every required skeleton and animation succeeds.
- Destroy only animation states that completed initialization.
- Keep Tatl failure atomic with Skull Kid: if either required presentation cannot initialize safely, suppress the paired entry.
- Log one concise diagnostic for an unavailable MM resource set; do not emit a message every frame.

## Testing

Focused host tests will cover:

- exact parameter decoding and rejection of undefined poses;
- object/archive routing for all five actor types;
- Bomb Shop Lady and Happy Mask Salesman pose-to-animation mappings;
- adult Ganondorf's human skeleton, standing animation, normal eye contract, and explicit absence of Beast Ganon/transformation/cape spawning;
- Phantom Ganon's neutral animation and explicit segment `0x08` contract;
- Skull Kid pose selection, mask attachment, hover amplitude, authored-Y stability, and disabled tracking;
- Tatl presence in both poses, second-skeleton ownership, pose-specific anchor selection, color/alpha pulse bounds, archive-scoped `gameplay_keep` resolution, and atomic failure;
- pose-gated tracking for the Bomb Shop Lady and Happy Mask Salesman;
- safe unavailable-resource behavior and initialized-only destruction;
- no regression in existing Great Fairy, Zelda, Sheik, Impa, or Ruto adapters.

Integration verification will compile the actual overlay on Linux and Windows, run the complete focused actor test suite with strict warnings, and check the diff for weather or unrelated catalogue changes. Runtime acceptance requires testing vanilla OoT assets, the intended OoT replacement stack, stock `mm.o2r`, and at least one compatible MM override stack. Each parameter must animate indefinitely, survive room reloads, support multiple simultaneous placements, and leave no stale face/eye/display-list state on adjacent actors.

## Explicit Non-Goals

- Beast Ganon or any transformation sequence
- Ganondorf's dynamic battle cape, combat AI, magic, or boss effects
- Phantom Ganon combat, horse, projectiles, or encounter scripting
- Skull Kid AI, moon/cutscene state, playable interactions, or Tatl companion behavior
- Happy Mask Salesman trade logic or cutscenes
- Bomb Shop Lady schedules, shop logic, or knockdown sequences
- New dialogue-pool work
- Weather changes
