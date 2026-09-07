/**
 * remote_bomb_ship.cpp — the one thing the Remote Bomb rune needs from the vanilla-behavior hooks.
 *
 * A carried actor is thrown by A, B or ANY C button (Player_ActionHandler_9), so the slate's own
 * button would lob the bomb instead of setting it off. That call already goes through a hook:
 *
 *     GameInteractor_Should(VB_THROW_OR_PUT_DOWN_HELD_ITEM, ..., sControlInput)   // z_player.c
 *
 * so the rune suppresses it for its own button only. A and B still throw the bomb normally.
 *
 * This lives in its own translation unit because it is C++ and the rest of the rune is C.
 */

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"

/** Is the rune's own bomb in Link's hands? Defined in mods/actors/remote_bomb.c. */
u8 RemoteBomb_IsHeld(void);

/** Every C button currently holding the slate. Defined in mods/items/logic/item_sheikah_slate.c. */
u16 Slate_EquippedButtonMask(void);
}

void RegisterRemoteBombThrow() {
    // Unconditional, like the cane's: it does nothing at all unless the rune is holding its bomb,
    // and it only ever suppresses, so it agrees with anything else registered on the same hook.
    COND_VB_SHOULD(VB_THROW_OR_PUT_DOWN_HELD_ITEM, true, {
        if (!*should) {
            return;
        }

        Input* input = va_arg(args, Input*);

        if (RemoteBomb_IsHeld() && (input->press.button & Slate_EquippedButtonMask())) {
            *should = false;
        }
    });
}

static RegisterShipInitFunc initRemoteBombThrow(RegisterRemoteBombThrow, {});
