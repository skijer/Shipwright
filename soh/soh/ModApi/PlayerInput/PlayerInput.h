#ifndef SOH_MOD_API_PLAYER_INPUT_H
#define SOH_MOD_API_PLAYER_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool PlayerInput_Block(const char* owner, uint16_t buttons, bool blockStick);
void PlayerInput_Release(const char* owner);
uint16_t PlayerInput_GetBlockedButtons(void);
bool PlayerInput_IsStickBlocked(void);

#ifdef __cplusplus
}

void PlayerInput_Init();
#endif

#endif
