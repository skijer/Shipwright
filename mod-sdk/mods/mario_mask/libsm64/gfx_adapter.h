#pragma once

#include "decomp/engine/graph_node.h"
#include "libsm64.h"

// Commented out in gbi.h - Replaced here
#define gDPPipeSync(pkt) ((void)0)
#define gSPSetGeometryMode(pkt, word) ((void)0)
#define gSPClearGeometryMode(pkt, word) ((void)0)
#define gDPFillRectangle(pkt, ulx, uly, lrx, lry) ((void)0)
#define gSPViewport(pkt, v) ((void)0)
extern void gSPMatrix(void* pkt, Mtx* m, uint8_t flags);
extern void gSPDisplayList(void* pkt, struct DisplayListNode* dl);

extern void gfx_adapter_bind_output_buffers(struct SM64MarioGeometryBuffers* outBuffers);
