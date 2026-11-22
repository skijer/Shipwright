#ifndef BANJO_CONTROLS_H
#define BANJO_CONTROLS_H

#include <stdint.h>

#include "banjo_host.h"

typedef enum BanjoAction {
    BANJO_ACTION_JUMP,
    BANJO_ACTION_ATTACK,
    BANJO_ACTION_INTERACT,
    BANJO_ACTION_CROUCH,
    BANJO_ACTION_TARGET,
    BANJO_ACTION_DETRANSFORM,
    BANJO_ACTION_FIRST_PERSON,
    BANJO_ACTION_EGGS,
    BANJO_ACTION_TALON_TROT,
    BANJO_ACTION_WONDER_WING,
    BANJO_ACTION_ITEM,
    BANJO_ACTION_COUNT
} BanjoAction;

typedef struct BanjoBindings {
    uint32_t buttons[BANJO_ACTION_COUNT];
} BanjoBindings;

void BanjoControls_ReadBindings(BanjoBindings* bindings);
void BanjoControls_RegisterMenu(const SOHModApi* api);

#endif
