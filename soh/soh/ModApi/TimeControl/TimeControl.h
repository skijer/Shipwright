#ifndef SOH_MOD_API_TIME_CONTROL_H
#define SOH_MOD_API_TIME_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool TimeControl_Request(const char* owner, int32_t priority, float worldSpeed, bool freezeClock);
void TimeControl_Release(const char* owner);
float TimeControl_GetWorldSpeed(void);

#ifdef __cplusplus
}

void TimeControl_Init();
#endif

#endif
