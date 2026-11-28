#ifndef SOH_MOD_API_TIMERS_H
#define SOH_MOD_API_TIMERS_H

#include <stdbool.h>
#include <stdint.h>

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

bool Timers_RequestHazard(const char* owner, int32_t priority, int16_t hazard, uint8_t requiredTunic);
void Timers_ReleaseHazard(const char* owner);
int16_t Timers_GetHazard(void);

bool Timers_StartCountdown(const char* owner, uint16_t seconds);
void Timers_StopCountdown(const char* owner);
uint16_t Timers_GetCountdown(const char* owner);
bool Timers_IsCountdownRunning(const char* owner);

#ifdef __cplusplus
}

void Timers_Init();
#endif

#endif
