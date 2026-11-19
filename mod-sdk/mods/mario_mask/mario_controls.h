#ifndef MARIO_CONTROLS_H
#define MARIO_CONTROLS_H

#include <stdint.h>

#include "mario_host.h"

typedef enum MarioAction {
    MARIO_ACTION_JUMP,
    MARIO_ACTION_PUNCH,
    MARIO_ACTION_INTERACT,
    MARIO_ACTION_CROUCH,
    MARIO_ACTION_TARGET,
    MARIO_ACTION_DETRANSFORM,
    MARIO_ACTION_FIRST_PERSON,
    MARIO_ACTION_ITEM,
    MARIO_ACTION_COUNT
} MarioAction;

typedef struct MarioBindings {
    uint32_t buttons[MARIO_ACTION_COUNT];
} MarioBindings;

void MarioControls_ReadBindings(MarioBindings* bindings);
void MarioControls_RegisterMenu(const SOHModApi* api);

#endif
