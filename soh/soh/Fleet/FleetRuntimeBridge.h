#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t FleetRuntime_ResolveExit(int32_t entrance, int beforeNative);
int FleetRuntime_CompleteTransition(void);
int FleetRuntime_EntrancesEnabled(void);
void FleetRuntime_CancelTransition(void);
void FleetRuntime_SetLoading(int loading);
int32_t FleetRuntime_RecoveryEntrance(void);

#ifdef __cplusplus
}
#endif
