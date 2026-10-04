# Great Fairy and Facial Stabilization Design

## Goal

Add a static Great Fairy to the existing `En_Viewer` actor catalogue and stabilize the remaining Sheik, Impa, and Adult Ruto facial presentation without changing weather code or expanding the catalogue further.

## Baseline

The work starts from `codex/poc12-ruto-final-stabilization`. Existing Zelda tracking, Adult Ruto tracking and water presentation, catalogue parameter decoding, and all unrelated actor behavior remain intact.

## Great Fairy catalogue adapter

The Great Fairy uses OoT's native `OBJECT_DY_OBJ`, `gGreatFairySkel`, symbolic limb display lists, textures, and matching 28-limb animations so SoH alternate-resource lookup remains active for the complete model. The adapter must not embed or substitute vanilla-only Great Fairy geometry. It exposes three expanded parameters:

- `0x7E04`: sitting idle (`gGreatFairySittingAnim`)
- `0x7E14`: laying sideways (`gGreatFairyLayingSidewaysAnim`)
- `0x7E24`: looping after-spell flourish (`gGreatFairyAfterSpellAnim`)

The adapter reproduces only the model's static render contract: eye segment `0x08`, unused-compatible second eye segment `0x09`, mouth segment `0x0A`, torso tracking on limb 8, and head tracking on limb 15. It uses ordinary catalogue targeting, talk offering, and collision. It does not import fountain reward selection, healing, particles, growth, disappearance, cutscene control, spawned items, or beam behavior.

The sitting pose may use native torso-and-head tracking. The laying and flourish poses preserve their authored body silhouettes but receive conservative, clamped head-only tracking through native head limb 15. The Great Fairy has no directional eye textures, so this is head/face-bone tracking rather than pupil movement. If a pose's local head axes cannot accept the bounded rotation without contortion, that pose falls back to no tracking rather than rotating its torso or overriding its authored body animation. All three use normal blinking; the mouth remains closed unless runtime inspection proves the flourish animation requires the native open-mouth presentation. Dialogue is a safe existing Great Fairy message presented directly by the catalogue, with none of the native actor's follow-up action logic.

All three poses hover around the authored placement Y. Sitting and flourish use a smooth five-unit sinusoidal amplitude; laying uses a gentler three-unit amplitude. Collision, focus, targeting, and dialogue follow the actor's resulting world position.

## Sheik harp eyes

The existing harp pose must continue forcing closed eyes in vanilla assets. The implementation will verify that both facial segments receive `gSheikEyeShutTex`. If a replacement model ignores those segments or bakes open eyes into its head display list, the adapter will not replace the Twilight Princess head with vanilla geometry. A code change is made only if a resource-pack-compatible segmented route exists in the current asset contract; otherwise the limitation is documented as replacement-model behavior.

## Impa facial contract

The static Impa path is compared against native `Demo_Im`, including object-bank selection, the unmasked head display list, eye segments `0x08` and `0x09`, and segment `0x0C`. If the static path leaves a model-required face segment stale, Impa receives a model-specific setup isolated to her draw callback. The fix must preserve vanilla rendering, blinking, tracking, and compatible replacement models.

## Adult Ruto facial audit

Open, half, and closed eye states are verified for both the opaque grounded/surfaced draw and translucent emergence/departure draw. Both paths must bind identical eye and mouth resources to their respective display-list streams, and neither may inherit stale state from another actor. No Ruto behavior changes are included unless the audit produces a concrete failing contract.

## Testing and acceptance

Focused tests cover parameter decoding, all three Great Fairy poses, object and skeleton selection, full versus head-only tracking policy, bounded head-only rotations, face-resource selection, Sheik's forced harp eye state, Impa's face setup decision, and Ruto OPA/XLU facial parity. Tests must fail for each missing contract before implementation and pass afterward. Existing actor and Ruto water suites must remain green, `git diff --check` must pass, and the published branch must contain no weather changes.

Runtime acceptance requires the Great Fairy to render under vanilla and alternate assets without invoking fountain behavior, Sheik to close her eyes under vanilla assets and wherever a replacement model honors native eye segments, Impa to render without a garbage facial patch, and Adult Ruto to blink correctly throughout opaque and translucent phases.
