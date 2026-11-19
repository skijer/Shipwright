// Fleet-owned ABI v1. Included after the receiving game's z64 and CVar types.
// Function pointers belong to this receiver only. No native PlayState crosses game boundaries.
#ifndef FLEET_PRESENTATION_CONTEXT_V1_H
#define FLEET_PRESENTATION_CONTEXT_V1_H
#include <stdint.h>
#include <stddef.h>
#include <libultraship/color.h>
struct FleetPresentationContextV1 {
    uint32_t structSize;
    uint32_t version;
    const char* receiver;
    bool (*isComboMode)();
    bool (*readOwnerLevel)(const char* owner, const char* level, uint32_t* value);
    Gfx* (*setupDL)(Gfx*, uint32_t);
    Mtx* (*newMtx)(GraphicsContext*, const char*, int32_t);
    void (*matrixPush)();
    void (*matrixPop)();
    void (*matrixReplaceRotation)(MtxF*);
    void (*matrixScale)(float, float, float, uint8_t);
    void (*matrixTranslate)(float, float, float, uint8_t);
    void (*matrixRotateX)(float, uint8_t);
    void (*matrixRotateY)(float, uint8_t);
    void (*matrixRotateZ)(float, uint8_t);
    void (*matrixRotateZYX)(int16_t, int16_t, int16_t, uint8_t);
    Gfx* (*texScroll)(GraphicsContext*, uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t);
    Gfx* (*twoTexScroll)(GraphicsContext*, int32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t,
                         int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
    void (*openInterpolationChild)(const void*, int);
    void (*closeInterpolationChild)();
    Gfx* (*setFog)(PlayState*, Gfx*);
    int32_t (*cvarInteger)(const char*, int32_t);
    Color_RGB8 (*cvarColor24)(const char*, Color_RGB8);
    void (*displayList)(Gfx*, const void*);
    void (*segment)(Gfx*, int32_t, const void*);
};
typedef const FleetPresentationContextV1* (*FleetGetPresentationContextV1)();
static constexpr const char* FLEET_PRESENTATION_SERVICE = "fleet.presentation.context.v1";
#endif
