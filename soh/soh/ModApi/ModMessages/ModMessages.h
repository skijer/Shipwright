#ifndef SOH_MOD_API_MOD_MESSAGES_H
#define SOH_MOD_API_MOD_MESSAGES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOH_MOD_MESSAGE_INVALID 0xFFFF

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* text;
    const char* textGerman;
    const char* textFrench;
    uint8_t textboxType;
    uint8_t textboxPosition;
    bool autoFormat;
} SOHModMessage;

uint16_t ModMessages_Register(const SOHModMessage* message);
uint16_t ModMessages_GetId(const char* key);
uint8_t ModMessages_RegisterNaviHint(const char* actorKey, const char* hint);

#ifdef __cplusplus
}
#endif

#endif
