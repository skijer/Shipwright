#ifndef SOH_MOD_API_MICROPHONE_H
#define SOH_MOD_API_MICROPHONE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOH_MICROPHONE_OPEN,
    SOH_MICROPHONE_PENDING,
    SOH_MICROPHONE_DENIED,
    SOH_MICROPHONE_UNAVAILABLE,
} SOHMicrophoneStatus;

SOHMicrophoneStatus Microphone_Open(uint32_t sampleRate);
uint32_t Microphone_Read(float* samples, uint32_t capacity);
void Microphone_Close(void);

#ifdef __cplusplus
}

void Microphone_Init();
#endif

#endif
