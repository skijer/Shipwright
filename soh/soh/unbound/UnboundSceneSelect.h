#pragma once
#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// The list lives until the next call; its length is written to `outCount`.
BetterSceneSelectEntry* UnboundSceneSelect_BuildList(const BetterSceneSelectEntry* vanilla, s32 vanillaCount,
                                                     void (*loadFunc)(struct SelectContext*, s32), s32* outCount);

#ifdef __cplusplus
}
#endif
