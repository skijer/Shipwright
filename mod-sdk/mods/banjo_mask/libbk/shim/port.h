#ifndef LIBBK_PORT_H
#define LIBBK_PORT_H

#include <stddef.h>
#include <stdint.h>
#include "events.h"

char* ResourceMgr_LoadByAssetId(uint32_t assetId);
char* ResourceMgr_ReloadByAssetId(uint32_t assetId);
size_t ResourceMgr_GetResourceSize(uint32_t assetId);
void FrameInterpolation_RecordMatrixToMtx(void* matrix, const float source[4][4]);
int GraphicsPatches_IsLodDisabled(void);

#define BK_LOG_WARN(...) ((void)0)
#define BK_LOG_DEBUG(...) ((void)0)
#define BK_LOG_INFO(...) ((void)0)
#define BK_LOG_ERROR(...) ((void)0)

#endif
