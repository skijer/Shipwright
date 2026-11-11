// Form strength as an offset on Link's own level, the same rule 2Ship's oot_collectables uses.
// Level -1 is the rando's missing grab, 0 lifts small things and each strength upgrade adds one.
// A form whose total reaches 0 grabs without the rando's grab ability; one below 0 never grabs.
#ifndef Z64STRENGTH_H
#define Z64STRENGTH_H

#include <stdarg.h>
#include <stdbool.h>

#include "functions.h"
#include "macros.h"
#include "variables.h"

#define Z64STRENGTH_OFFSET_DEKU (-1)
#define Z64STRENGTH_OFFSET_HUMAN 0
#define Z64STRENGTH_OFFSET_ZORA 1
#define Z64STRENGTH_OFFSET_GORON 2

static inline bool Z64Strength_HasGrab(void) {
    return !IS_RANDO || Flags_GetRandomizerInf(RAND_INF_CAN_GRAB);
}

static inline s32 Z64Strength_GetEffective(s32 formOffset) {
    const s32 level = CUR_UPG_VALUE(UPG_STRENGTH) - (Z64Strength_HasGrab() ? 0 : 1);
    return level + formOffset;
}

// VB_USE_STRENGTH_UPGRADE: the returned strength replaces the vanilla upgrade read.
static inline void Z64Strength_ResolveUpgrade(bool* should, va_list args, s32 formOffset) {
    s32* strength = va_arg(args, s32*);
    *strength = CLAMP(Z64Strength_GetEffective(formOffset), PLAYER_STR_NONE, PLAYER_STR_GOLD_G);
    *should = false;
}

// VB_PREVENT_STRENGTH has two callers: the lift (Link already carrying) and the block push (flag set just before).
// The rando handler ran first and cleared the push flag when it prevented; a skipped check restores it.
static inline void Z64Strength_ResolvePrevent(bool* should, s32 formOffset) {
    Player* player = GET_PLAYER(gPlayState);
    const s32 effective = Z64Strength_GetEffective(formOffset);
    const bool isLifting = (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) && player->interactRangeActor != NULL;

    if (effective < 0) {
        player->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
        *should = true;
        return;
    }
    if (Z64Strength_HasGrab()) {
        return;
    }
    if (isLifting) {
        const s16 id = player->interactRangeActor->id;
        *should = (id == ACTOR_EN_BOMBF || id == ACTOR_EN_KUSA) && effective == 0;
        return;
    }
    player->stateFlags2 |= PLAYER_STATE2_MOVING_DYNAPOLY;
    *should = false;
}

#endif
