#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "expansions/sw97/sw97_config.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "mods/items/custom_items.h"
#include "mods/items/logic/item_bombarrows.h"
#include "mods/items/logic/twilight_upgrade.h"
#include "mods/extended_inventory.h" // Sw97_Element* — shared ordering/ownership (Skijer's NEI)
#include "mods/extended_player.h"    // ExtPlayer_GetItemAction — element -> PLAYER_IA_*

s32 func_808351D4(Player* thisx, PlayState* play); // Arrow nocked
void EnArrow_Init(Actor* thisx, PlayState* play);
// NOTE: parameter must NOT be named `this` — even inside `extern "C"`,
// `this` is a reserved keyword in C++ TUs and the parse fails before the
// `extern "C"` linkage takes effect. Use a different param name.
s32 Player_UpperAction_BombArrows(Player* thisx, PlayState* play);

extern PlayState* gPlayState;
}

#define CVAR_ARROW_CYCLE_NAME CVAR_ENHANCEMENT("BowArrowCycle")
#define CVAR_ARROW_CYCLE_DEFAULT 0
#define CVAR_ARROW_CYCLE_VALUE CVarGetInteger(CVAR_ARROW_CYCLE_NAME, CVAR_ARROW_CYCLE_DEFAULT)

// NEI extension CVar — when enabled, adds: L button (prev direction), SW97
// elemental arrow cycling, and slingshot support. Coexists with vanilla
// BowArrowCycle: when only the vanilla CVar is on, behavior is unchanged
// (R-only, vanilla arrows). Either CVar enables the unified registration.
#define CVAR_NEI_AIM_CYCLE_NAME CVAR_ENHANCEMENT("NeiAimCycle")
#define CVAR_NEI_AIM_CYCLE_VALUE CVarGetInteger(CVAR_NEI_AIM_CYCLE_NAME, 0)
#define EITHER_ARROW_CYCLE_VALUE (CVAR_ARROW_CYCLE_VALUE || CVAR_NEI_AIM_CYCLE_VALUE)

static const s16 sMagicArrowCosts[] = { 4, 4, 8 };

static const PlayerItemAction sArrowCycleOrder[] = {
    PLAYER_IA_BOW,
    PLAYER_IA_BOW_FIRE,
    PLAYER_IA_BOW_ICE,
    PLAYER_IA_BOW_LIGHT,
};

static bool IsHoldingBow(Player* player) {
    return player->heldItemAction >= PLAYER_IA_BOW && player->heldItemAction <= PLAYER_IA_BOW_LIGHT;
}

static bool IsHoldingMagicBow(Player* player) {
    return player->heldItemAction >= PLAYER_IA_BOW_FIRE && player->heldItemAction <= PLAYER_IA_BOW_LIGHT;
}

static bool IsAimingBow(Player* player) {
    return IsHoldingBow(player) && ((player->unk_6AD == 2) || (player->upperActionFunc == func_808351D4));
}

static bool HasArrowType(PlayerItemAction itemAction) {
    switch (itemAction) {
        case PLAYER_IA_BOW:
            return true;
        case PLAYER_IA_BOW_FIRE:
            return INV_CONTENT(ITEM_ARROW_FIRE) == ITEM_ARROW_FIRE && gSaveContext.magic >= sMagicArrowCosts[0];
        case PLAYER_IA_BOW_ICE:
            return INV_CONTENT(ITEM_ARROW_ICE) == ITEM_ARROW_ICE && gSaveContext.magic >= sMagicArrowCosts[1];
        case PLAYER_IA_BOW_LIGHT:
            return INV_CONTENT(ITEM_ARROW_LIGHT) == ITEM_ARROW_LIGHT && gSaveContext.magic >= sMagicArrowCosts[2];
        default:
            return false;
    }
}

static s32 GetBowItemForArrow(PlayerItemAction itemAction) {
    switch (itemAction) {
        case PLAYER_IA_BOW_FIRE:
            return ITEM_BOW_ARROW_FIRE;
        case PLAYER_IA_BOW_ICE:
            return ITEM_BOW_ARROW_ICE;
        case PLAYER_IA_BOW_LIGHT:
            return ITEM_BOW_ARROW_LIGHT;
        default:
            return ITEM_BOW;
    }
}

static ArrowType GetArrowTypeForArrow(s8 itemAction) {
    switch (itemAction) {
        case PLAYER_IA_BOW_FIRE:
            return ARROW_FIRE;
        case PLAYER_IA_BOW_ICE:
            return ARROW_ICE;
        case PLAYER_IA_BOW_LIGHT:
            return ARROW_LIGHT;
        default:
            return ARROW_NORMAL;
    }
}

static bool CanCycleArrows() {
    Player* player = GET_PLAYER(gPlayState);

    return (LINK_IS_ADULT || CVarGetInteger(CVAR_CHEAT("TimelessEquipment"), 0)) && !gSaveContext.minigameState &&
           gPlayState->sceneNum != SCENE_SHOOTING_GALLERY && !(player->stateFlags1 & PLAYER_STATE1_ON_HORSE) &&
           player->rideActor == NULL && INV_CONTENT(SLOT_BOW) == ITEM_BOW &&
           (INV_CONTENT(ITEM_ARROW_FIRE) == ITEM_ARROW_FIRE || INV_CONTENT(ITEM_ARROW_ICE) == ITEM_ARROW_ICE ||
            INV_CONTENT(ITEM_ARROW_LIGHT) == ITEM_ARROW_LIGHT);
}

static s8 GetNextArrowType(s8 currentArrowType) {
    int currentIndex = 0;
    for (int i = 0; i < (int)ARRAY_COUNT(sArrowCycleOrder); i++) {
        if (sArrowCycleOrder[i] == currentArrowType) {
            currentIndex = i;
            break;
        }
    }

    for (int offset = 1; offset <= (int)ARRAY_COUNT(sArrowCycleOrder); offset++) {
        int nextIndex = (currentIndex + offset) % ARRAY_COUNT(sArrowCycleOrder);
        if (HasArrowType(sArrowCycleOrder[nextIndex])) {
            return sArrowCycleOrder[nextIndex];
        }
    }

    return PLAYER_IA_BOW;
}

// NEI extension: same as GetNextArrowType but walks backward through the cycle.
static s8 GetPrevArrowType(s8 currentArrowType) {
    int count = (int)ARRAY_COUNT(sArrowCycleOrder);
    int currentIndex = 0;
    for (int i = 0; i < count; i++) {
        if (sArrowCycleOrder[i] == currentArrowType) {
            currentIndex = i;
            break;
        }
    }

    for (int offset = 1; offset <= count; offset++) {
        int prevIndex = ((currentIndex - offset) % count + count) % count;
        if (HasArrowType(sArrowCycleOrder[prevIndex])) {
            return sArrowCycleOrder[prevIndex];
        }
    }

    return PLAYER_IA_BOW;
}

static void UpdateButtonAlpha(s16 flashAlpha, bool isButtonBow, u16* buttonAlpha) {
    if (isButtonBow) {
        *buttonAlpha = flashAlpha;
    }
}

static void UpdateEquippedBow(PlayState* play, s8 arrowType) {
    s32 bowItem = GetBowItemForArrow((PlayerItemAction)arrowType);
    bool dpadEnabled = CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0);
    s32 maxButton = dpadEnabled ? 7 : 3;

    for (s32 i = 1; i <= maxButton; i++) {
        if ((gSaveContext.equips.buttonItems[i] == ITEM_BOW) ||
            (gSaveContext.equips.buttonItems[i] >= ITEM_BOW_ARROW_FIRE &&
             gSaveContext.equips.buttonItems[i] <= ITEM_BOW_ARROW_LIGHT)) {
            gSaveContext.equips.buttonItems[i] = bowItem;
            gSaveContext.equips.cButtonSlots[i - 1] = SLOT_BOW;

            if (i <= 3) {
                Interface_LoadItemIcon1(play, i);
            }

            gSaveContext.buttonStatus[i] = BTN_ENABLED;
        }
    }
}

bool ArrowCycleMain() {
    if (gPlayState == nullptr || !CanCycleArrows()) {
        return false;
    }

    Player* player = GET_PLAYER(gPlayState);
    if (player->heldActor != NULL && player->heldActor->id == ACTOR_EN_ARROW) {
        if (IsHoldingMagicBow(player) && gSaveContext.magicState != MAGIC_STATE_IDLE && player->heldActor == NULL) {
            Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            return true;
        }

        // reset magic state to IDLE before cycling to prevent error sound
        gSaveContext.magicState = MAGIC_STATE_IDLE;

        s8 nextArrow = GetNextArrowType(player->heldItemAction);
        player->heldItemAction = nextArrow;
        player->itemAction = nextArrow;
        Actor* arrow = player->heldActor;

        if (arrow->child != NULL) {
            Actor_Kill(arrow->child);
            arrow->child = NULL;
        }
        arrow->params = GetArrowTypeForArrow(nextArrow);
        EnArrow_Init(arrow, gPlayState);
        UpdateEquippedBow(gPlayState, nextArrow);
        return true;
    }
    return false;
}

// =============================================================================
// NEI extensions — L button (prev), SW97 arrow cycling, slingshot support.
// All gated on CVAR_NEI_AIM_CYCLE_VALUE. The vanilla ArrowCycleMain above
// stays as-is for users with only BowArrowCycle on.
// =============================================================================

// Vanilla-arrows reverse direction (mirror of ArrowCycleMain with GetPrev).
static bool ArrowCycleMainPrev() {
    if (gPlayState == nullptr || !CanCycleArrows()) {
        return false;
    }

    Player* player = GET_PLAYER(gPlayState);
    if (player->heldActor != NULL && player->heldActor->id == ACTOR_EN_ARROW) {
        if (IsHoldingMagicBow(player) && gSaveContext.magicState != MAGIC_STATE_IDLE && player->heldActor == NULL) {
            Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            return true;
        }

        gSaveContext.magicState = MAGIC_STATE_IDLE;

        s8 prevArrow = GetPrevArrowType(player->heldItemAction);
        player->heldItemAction = prevArrow;
        player->itemAction = prevArrow;
        Actor* arrow = player->heldActor;

        if (arrow->child != NULL) {
            Actor_Kill(arrow->child);
            arrow->child = NULL;
        }
        arrow->params = GetArrowTypeForArrow(prevArrow);
        EnArrow_Init(arrow, gPlayState);
        UpdateEquippedBow(gPlayState, prevArrow);
        return true;
    }
    return false;
}

// Skijer's NEI: this file used to carry a FOURTH private copy of "which elemental arrow is this"
// — its own cycle-order array, its own CHECK_QUEST_ITEM ownership table and its own item→IA switch,
// all keyed on the six ITEM_SW97_ARROW_* ids. The element is a flag now, and ordering/ownership come
// from the shared Sw97_Element* helpers, so the kaleido wheel and this R/L cycle can no longer drift
// apart. Bomb Arrows is SW97_ELEM_BOMB, the last entry, exactly as before.

// Which weapon the held button is, for the flag lookup: 0 bow, 1 slingshot, -1 neither.
static s32 GetHeldWeaponIsSling(Player* player) {
    if (player->heldItemButton < 0 || player->heldItemButton >= (s32)ARRAY_COUNT(gSaveContext.equips.buttonItems)) {
        return -1;
    }
    u8 item = gSaveContext.equips.buttonItems[player->heldItemButton];
    if (Sw97_IsBowItem(item)) {
        return 0;
    }
    if (Sw97_IsSlingItem(item)) {
        return 1;
    }
    return -1;
}

// Both bow and slingshot — vanilla cheat only handled bow, NEI extends to
// slingshot since SW97 arrows are shared (see z_player.c:2873). Range
// extended through PLAYER_IA_BOW_0E so SW97 dark/soul/wind arrows are also
// recognized as "holding bow" for cycle activation.
static bool IsHoldingBowOrSlingshot(Player* player) {
    return (player->heldItemAction >= PLAYER_IA_BOW && player->heldItemAction <= PLAYER_IA_BOW_0E) ||
           player->heldItemAction == PLAYER_IA_SLINGSHOT;
}

static bool IsAimingBowOrSlingshot(Player* player) {
    return IsHoldingBowOrSlingshot(player) && ((player->unk_6AD == 2) || (player->upperActionFunc == func_808351D4));
}

// Bomb arrows is a custom item with its own aim flow — it's NOT in the
// IsHoldingBow range. Detect it via baActive (set by Handle_BombArrows in
// the sustained-aim state).
static bool IsAimingBombArrows(Player* player) {
    return player->heldItemAction == PLAYER_IA_BOMB_ARROWS && baActive;
}

// NEI vanilla-arrow cycle — same as ArrowCycleMain/Prev but skips the
// BowArrowCycle CVar gate (since NEI is its own gate) and relaxes the
// LINK_IS_ADULT requirement only when held arrow is a vanilla magic arrow
// (adult-only by design). Used when NEI is on without BowArrowCycle.
static bool NeiVanillaArrowCycle(s32 direction) {
    if (gPlayState == nullptr) {
        return false;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player->heldActor == NULL || player->heldActor->id != ACTOR_EN_ARROW) {
        return false;
    }
    // Need vanilla bow (adult only) AND at least one elemental arrow owned.
    if (!LINK_IS_ADULT || INV_CONTENT(SLOT_BOW) != ITEM_BOW) {
        return false;
    }
    bool hasAny = INV_CONTENT(ITEM_ARROW_FIRE) == ITEM_ARROW_FIRE || INV_CONTENT(ITEM_ARROW_ICE) == ITEM_ARROW_ICE ||
                  INV_CONTENT(ITEM_ARROW_LIGHT) == ITEM_ARROW_LIGHT;
    if (!hasAny) {
        return false;
    }

    gSaveContext.magicState = MAGIC_STATE_IDLE;
    s8 nextArrow = direction > 0 ? GetNextArrowType(player->heldItemAction) : GetPrevArrowType(player->heldItemAction);
    if (nextArrow == player->heldItemAction) {
        return false;
    }

    player->heldItemAction = nextArrow;
    player->itemAction = nextArrow;
    Actor* arrow = player->heldActor;
    if (arrow->child != NULL) {
        Actor_Kill(arrow->child);
        arrow->child = NULL;
    }
    arrow->params = GetArrowTypeForArrow(nextArrow);
    EnArrow_Init(arrow, gPlayState);
    UpdateEquippedBow(gPlayState, nextArrow);
    return true;
}

// heldItemAction still has to follow the element after a cycle, or Player_HoldsBow stops matching
// and the bow visually unequips. It is no longer computed here though: ExtPlayer_GetItemAction reads
// the flag we just set, so asking IT is what keeps this in step with the pause menu and the shot
// decode. (The bow's element -> PLAYER_IA_BOW_FIRE..0E mapping lives there, once.)
static s8 GetSw97PlayerItemAction(Player* player) {
    if (player->heldItemButton < 0 || player->heldItemButton >= (s32)ARRAY_COUNT(gSaveContext.equips.buttonItems)) {
        return PLAYER_IA_BOW;
    }
    return (s8)ExtPlayer_GetItemAction(gSaveContext.equips.buttonItems[player->heldItemButton]);
}

// Cycle SW97 arrows AND bomb arrows on the held C-button. The active aim is
// either vanilla bow flow (SW97 held → EnArrow as heldActor) or custom bomb
// arrows flow (no heldActor, baActive=1). The cycle has to handle 4
// transition types:
//   SW97  → SW97 : update EnArrow params + heldItemAction (existing)
//   SW97  → BOMB : kill EnArrow, set BOMB action, enter bomb arrows aim
//   BOMB  → SW97 : exit bomb arrows, spawn EnArrow as held, set BOW action
//   BOMB  → BOMB : impossible (only one bomb arrows position)
// Returns true if applied (so the caller skips the vanilla fallback).
static bool NeiSW97ArrowCycle(s32 direction) {
    if (gPlayState == nullptr) {
        return false;
    }
    Player* player = GET_PLAYER(gPlayState);

    // Aim state — either vanilla bow with SW97 OR custom bomb arrows.
    bool inBowAim =
        IsAimingBowOrSlingshot(player) && player->heldActor != NULL && player->heldActor->id == ACTOR_EN_ARROW;
    bool inBombAim = IsAimingBombArrows(player);
    if (!inBowAim && !inBombAim) {
        return false;
    }

    s32 isSling = GetHeldWeaponIsSling(player);
    if (isSling < 0) {
        return false; // the held button is not a bow/slingshot
    }
    if (!SW97_MEDALLIONS_ENABLED()) {
        return false;
    }

    u8 currentElem = Sw97_GetElement((u8)isSling);
    u8 nextElem = Sw97_ElementNeighbor((u8)isSling, currentElem, direction);
    if (nextElem == currentElem) {
        return false; // nothing else owned to cycle to
    }

    bool nextIsBomb = (nextElem == SW97_ELEM_BOMB);
    bool currIsBomb = (currentElem == SW97_ELEM_BOMB);

    // Common update — the FLAG plus the icon refresh. Note what is gone: the buttonItems[] write.
    // The button keeps its weapon; only the primed element changes.
    s32 button = player->heldItemButton;
    Sw97_SetElement((u8)isSling, nextElem);
    if (button <= 3) {
        Interface_LoadItemIcon1(gPlayState, button);
    }
    gSaveContext.buttonStatus[button] = BTN_ENABLED;

    if (!currIsBomb && !nextIsBomb) {
        // SW97 → SW97: re-init the held EnArrow with new params (existing path).
        // Only update heldItemAction for BOW — slingshot keeps PLAYER_IA_SLINGSHOT
        // because the slingshot has its own action distinct from the bow. Forcing
        // PLAYER_IA_BOW_* on a child holding slingshot makes Player_HoldsSlingshot
        // return false → the slingshot model disappears (this was the user's
        // "cycling unequips slingshot" report).
        if (player->heldItemAction != PLAYER_IA_SLINGSHOT) {
            s8 newAction = GetSw97PlayerItemAction(player);
            player->heldItemAction = newAction;
            player->itemAction = newAction;
        }

        Actor* arrow = player->heldActor;
        if (arrow->child != NULL) {
            Actor_Kill(arrow->child);
            arrow->child = NULL;
        }
        arrow->params = (nextElem == SW97_ELEM_NONE)
                            ? (isSling ? ARROW_SEED : ARROW_NORMAL)
                            : ((isSling ? ARROW_SEED_FIRE : ARROW_SW97_FIRE) + (nextElem - SW97_ELEM_FIRE));
        EnArrow_Init(arrow, gPlayState);
        return true;
    }

    if (!currIsBomb && nextIsBomb) {
        // SW97 → BOMB: kill the EnArrow, swap to bomb arrows aim.
        if (player->heldActor != NULL) {
            Actor_Kill(player->heldActor);
            player->heldActor = NULL;
        }
        if (player->actor.child != NULL) {
            // Whatever child the arrow had (or the arrow itself) — clear so
            // vanilla bow code doesn't keep referencing it.
            player->actor.child = NULL;
        }

        player->heldItemAction = PLAYER_IA_BOMB_ARROWS;
        player->itemAction = PLAYER_IA_BOMB_ARROWS;
        // Force the upper action to bomb arrows so the player update loop
        // queries the new anim state machine next frame.
        player->upperActionFunc = Player_UpperAction_BombArrows;

        BombArrows_EnterFromCycle(player, gPlayState);
        return true;
    }

    if (currIsBomb && !nextIsBomb) {
        // BOMB → SW97: tear down bomb arrows, hand back to vanilla bow/slingshot.
        BombArrows_ExitFromCycle(player, gPlayState);

        // For child, the underlying weapon is the slingshot — keep that as
        // heldItemAction so Player_HoldsSlingshot stays true. Only adult Link
        // uses the bow action with SW97 arrows.
        // GetSw97PlayerItemAction already returns PLAYER_IA_SLINGSHOT when the button holds one, so
        // the old LINK_IS_ADULT branch is redundant — the button is the source of truth.
        s8 newAction = GetSw97PlayerItemAction(player);
        player->heldItemAction = newAction;
        player->itemAction = newAction;
        // Restore vanilla bow/slingshot's upper action (arrow-nocked handler).
        player->upperActionFunc = func_808351D4;

        // Vanilla aim wants an EnArrow as the held actor. Spawn one now
        // so the bow/slingshot code sees a valid arrow on the very next frame.
        s32 spawnParams = (nextElem == SW97_ELEM_NONE)
                              ? (isSling ? ARROW_SEED : ARROW_NORMAL)
                              : ((isSling ? ARROW_SEED_FIRE : ARROW_SW97_FIRE) + (nextElem - SW97_ELEM_FIRE));
        Actor* arrow = Actor_SpawnAsChild(&gPlayState->actorCtx, &player->actor, gPlayState, ACTOR_EN_ARROW,
                                          player->actor.world.pos.x, player->actor.world.pos.y,
                                          player->actor.world.pos.z, 0, 0, 0, spawnParams);
        if (arrow != NULL) {
            player->heldActor = arrow;
            arrow->parent = &player->actor;
        }
        return true;
    }

    // Shouldn't reach here — bomb → bomb has no valid next item.
    return false;
}

void RegisterArrowCycle() {
    // R press: cycle to NEXT arrow type. SW97 detection takes priority when
    // NEI CVar is on (SW97 held → cycle SW97; otherwise fall back to vanilla).
    // L press: only handled when NEI CVar is on. Cycle to PREVIOUS arrow type.
    // Suppress the consumed button from the input stream so shield/etc. don't
    // also trigger off the same press.
    COND_VB_SHOULD(VB_EXECUTE_PLAYER_ACTION_FUNC, EITHER_ARROW_CYCLE_VALUE, {
        Player* player = (Player*)va_arg(args, void*);
        Input* input = (Input*)va_arg(args, void*);

        bool nei = CVAR_NEI_AIM_CYCLE_VALUE;
        // NEI mode: also let the cycle fire while aiming custom bomb arrows,
        // so R/L can rotate bombs → SW97 (and vice-versa).
        bool aiming = nei ? (IsAimingBowOrSlingshot(player) || IsAimingBombArrows(player)) : IsAimingBow(player);
        if (!aiming) {
            return;
        }

        bool rPressed = CHECK_BTN_ANY(input->press.button, BTN_R);
        bool lPressed = nei && CHECK_BTN_ANY(input->press.button, BTN_L);

        if (rPressed) {
            bool handled = false;
            if (nei) {
                // NEI mode: try SW97 + bomb arrows first.
                handled = NeiSW97ArrowCycle(1);
            }
            // Vanilla fallback fires when SW97/bomb didn't apply — covers
            // the "holding vanilla bow with vanilla elemental arrows" case
            // even when NEI is on. Without this fallback, vanilla cycling
            // dies for users who have both CVars enabled.
            if (!handled && CVAR_ARROW_CYCLE_VALUE) {
                ArrowCycleMain();
            }
            // ALWAYS consume R while aiming bow/slingshot, even if no cycle
            // applied. Otherwise R falls through to the shield action.
            input->cur.button &= ~BTN_R;
            input->press.button &= ~BTN_R;
        }

        if (lPressed) {
            bool handled = false;
            if (nei) {
                handled = NeiSW97ArrowCycle(-1);
            }
            // Vanilla cycle in reverse — same fallback rationale as R above.
            if (!handled && CVAR_ARROW_CYCLE_VALUE) {
                ArrowCycleMainPrev();
            }
            input->cur.button &= ~BTN_L;
            input->press.button &= ~BTN_L;
        }
    });

    // don't consume magic on draw, but check if we have enough to fire
    COND_VB_SHOULD(VB_PLAYER_ARROW_MAGIC_CONSUMPTION, EITHER_ARROW_CYCLE_VALUE, {
        Player* player = va_arg(args, Player*);
        int32_t magicArrowType = va_arg(args, int32_t);
        int32_t* arrowType = va_arg(args, int32_t*);

        if (gSaveContext.magic < sMagicArrowCosts[magicArrowType]) {
            *arrowType = ARROW_NORMAL;
        } else {
            *should = false;
        }
    });

    COND_VB_SHOULD(VB_EN_ARROW_MAGIC_CONSUMPTION, EITHER_ARROW_CYCLE_VALUE, {
        EnArrow* arrow = va_arg(args, EnArrow*);

        if (arrow->actor.params < ARROW_FIRE || arrow->actor.params > ARROW_LIGHT) {
            return;
        }

        int32_t magicArrowType = arrow->actor.params - ARROW_FIRE;
        Magic_RequestChange(gPlayState, sMagicArrowCosts[magicArrowType], MAGIC_CONSUME_NOW);
    });
}

static RegisterShipInitFunc initFunc(RegisterArrowCycle, { CVAR_ARROW_CYCLE_NAME, CVAR_NEI_AIM_CYCLE_NAME });
