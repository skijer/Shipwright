/**
 * cane_ship.cpp — the one thing Ultrahand needs from the vanilla-behavior hooks.
 *
 * Bg_Heavy_Block is thrown by handing it to its own state machine: set actor->parent for a few
 * frames and clear it, and BgHeavyBlock_Wait -> _LiftedUp -> _Fly runs the whole lift, quake,
 * NA_SE_EV_HEAVY_THROW and flight on its own (z_bg_heavy_block.c:322-390).
 *
 * The one part of that sequence which does not belong to us is Link. BgHeavyBlock_LiftedUp calls
 * Player_SetCsActionWithHaltedActors(play, &player->actor, 8) every frame it runs, because in
 * vanilla Link IS holding the pillar over his head and the game wants him locked into that pose.
 * Here he is standing several metres away pointing a cane, so freezing him is wrong and nothing
 * would release him afterwards.
 *
 * SoH already routes exactly that call through a hook for its FasterHeavyBlockLift enhancement:
 *
 *     GameInteractor_Should(VB_FREEZE_LINK_FOR_BLOCK_THROW, true, this)   // z_bg_heavy_block.c
 *
 * So the cane suppresses it, and only while one of its own throws is in the air.
 *
 * This lives in its own translation unit because it is C++ and the rest of the cane is C. The only
 * thing it asks the cane is one bool.
 */

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "functions.h"

/** Is a THROWS body mid-hand-off? Defined in cane_pacci.c. */
u8 Pacci_IsThrowing(void);
}

void RegisterCaneHeavyThrow() {
    // Unconditional: the hook has to be live whenever the cane might be, and it does nothing at all
    // unless the cane is the one throwing. FasterHeavyBlockLift registers the same hook behind its
    // own CVar and the two coexist - both only ever suppress, so whichever runs second agrees.
    COND_VB_SHOULD(VB_FREEZE_LINK_FOR_BLOCK_THROW, true, {
        if (Pacci_IsThrowing()) {
            *should = false;
        }
    });
}

static RegisterShipInitFunc initCaneHeavyThrow(RegisterCaneHeavyThrow, {});
