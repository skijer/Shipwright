#include "../../sm64_build.h"
#include <string.h>
#include "libultra_internal.h"
#include "../include/macros.h"

s32 osPiStartDma(UNUSED OSIoMesg* mb, UNUSED s32 priority, UNUSED s32 direction, uintptr_t devAddr, void* vAddr,
                 size_t nbytes, UNUSED OSMesgQueue* mq) {
    memcpy(vAddr, (const void*)devAddr, nbytes);
    return 0;
}

void osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msgBuf, s32 count) {
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msgBuf;
}

s32 osSendMesg(UNUSED OSMesgQueue* mq, UNUSED OSMesg msg, UNUSED s32 flag) {
    return 0;
}

s32 osRecvMesg(UNUSED OSMesgQueue* mq, UNUSED OSMesg* msg, UNUSED s32 flag) {
    return 0;
}

void osWritebackDCacheAll(void) {
}

void osWritebackDCache(UNUSED void* a, UNUSED size_t b) {
}

void osInvalDCache(UNUSED void* a, UNUSED size_t b) {
}

s32 osAiSetFrequency(u32 freq) {
    const u32 videoClock = 0x02E6D354;
    const u32 divider = (u32)(videoClock / (float)freq + .5f);

    if (divider < 0x84) {
        return -1;
    }
    return videoClock / (s32)divider;
}

OSPiHandle* osDriveRomInit(void) {
    static OSPiHandle handle;

    return &handle;
}
