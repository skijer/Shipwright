#pragma once
#include "z64.h"
#include "message_data_static.h"

#ifdef __cplusplus
extern "C" {
#endif

void OTRMessage_Init(void);

// SOH [Unbound] rebuilds the tables from the archives mounted now (a mod quest's world changed).
void OTRMessage_Reload(void);

MessageTableEntry* OTRMessage_Find(MessageTableEntry* table, u16 textId);

#ifdef __cplusplus
}
#endif
