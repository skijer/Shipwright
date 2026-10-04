#ifndef NIGHT_BGM_BRIDGE_H
#define NIGHT_BGM_BRIDGE_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// NA_BGM_DISABLED clears ownership. IDs are resolved, full-width resource IDs.
void Audio_RegisterNightBgm(uint16_t sequence);
uint8_t Audio_IsNightBgmActive(void);
#ifdef __cplusplus
}
#endif
#endif
