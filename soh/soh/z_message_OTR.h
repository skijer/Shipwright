#pragma once
#include "z64.h"
#include "message_data_static.h"

#ifdef __cplusplus
extern "C" {
#endif

void OTRMessage_Init(void);

MessageTableEntry* OTRMessage_Find(MessageTableEntry* table, u16 textId);

#ifdef __cplusplus
}
#endif
