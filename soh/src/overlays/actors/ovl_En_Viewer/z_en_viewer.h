#ifndef Z_EN_VIEWER_H
#define Z_EN_VIEWER_H

#include <libultraship/libultra.h>
#include "global.h"
#include "static_story_ruto_water.h"

struct EnViewer;

typedef void (*EnViewerActionFunc)(struct EnViewer*, PlayState*);
typedef void (*EnViewerDrawFunc)(struct EnViewer*, PlayState*);
typedef void (*EnViewerInitAnimFunc)(struct EnViewer*, PlayState*, void*, AnimationHeader*);

typedef enum {
    /* 0 */ ENVIEWER_TYPE_0_HORSE_ZELDA,
    /* 1 */ ENVIEWER_TYPE_1_IMPA,
    /* 2 */ ENVIEWER_TYPE_2_ZELDA,
    /* 3 */ ENVIEWER_TYPE_3_GANONDORF,
    /* 4 */ ENVIEWER_TYPE_4_HORSE_GANONDORF,
    /* 5 */ ENVIEWER_TYPE_5_GANONDORF,
    /* 6 */ ENVIEWER_TYPE_6_HORSE_GANONDORF,
    /* 7 */ ENVIEWER_TYPE_7_GANONDORF,
    /* 8 */ ENVIEWER_TYPE_8_GANONDORF,
    /* 9 */ ENVIEWER_TYPE_9_GANONDORF
} EnViewerType;

typedef enum {
    /* 0 */ ENVIEWER_DRAW_GANONDORF,
    /* 1 */ ENVIEWER_DRAW_HORSE,
    /* 2 */ ENVIEWER_DRAW_ZELDA,
    /* 3 */ ENVIEWER_DRAW_IMPA,
    /* 4 */ ENVIEWER_DRAW_STATIC_CHILD_MALON,
    /* 5 */ ENVIEWER_DRAW_STATIC_SARIA,
    /* 6 */ ENVIEWER_DRAW_STATIC_SHEIK,
    /* 7 */ ENVIEWER_DRAW_STATIC_ADULT_RUTO,
    /* 8 */ ENVIEWER_DRAW_STATIC_CHILD_RUTO,
    /* 9 */ ENVIEWER_DRAW_STATIC_KOKIRI_GIRL,
    /* 10 */ ENVIEWER_DRAW_STATIC_FADO,
    /* 11 */ ENVIEWER_DRAW_STATIC_ADULT_MALON
} EnViewerDrawType;

typedef enum {
    /* 0 */ ENVIEWER_SHADOW_NONE,
    /* 1 */ ENVIEWER_SHADOW_CIRCLE,
    /* 2 */ ENVIEWER_SHADOW_HORSE
} EnViewerShadowType;

typedef struct {
    /* 0x00 */ s16 skeletonObject;
    /* 0x02 */ s16 animObject;
    /* 0x04 */ u8 scale;   // divided by 100.0f
    /* 0x05 */ s8 yOffset; // multiplied by 100
    /* 0x06 */ u8 shadowType;
    /* 0x07 */ u8 shadowScale;
    /* 0x08 */ u8 drawType;
    /* 0x0C */ void* skeletonHeaderSeg;
    /* 0x10 */ AnimationHeader* anim;
} EnViewerInitData; // size = 0x14

typedef struct {
    /* 0x00 */ Vec3f startPos;
    /* 0x0C */ Vec3f endPos;
    /* 0x18 */ Vec3f pos;
    /* 0x24 */ f32 lerpFactorSpeed;
    /* 0x28 */ f32 scale;
    /* 0x2C */ f32 lerpFactor;
    /* 0x30 */ u8 state;
    u32 epoch;
} EnViewerFireEffect; // size = 0x34

typedef struct {
    uint8_t staticMode;
    uint8_t type;
    uint8_t pose;
    uint8_t initialized;
    uint8_t eyeIndex;
    uint8_t talking;
    uint8_t tracking;
    int16_t blinkTimer;
    uint8_t luluHdBlinkPhase;
    int16_t luluHdBlinkTimer;
    int16_t objectSlots[4];
    NpcInteractInfo interactInfo;
    ColliderCylinder collider;
    StaticRutoWaterState rutoWater;
    StaticRutoWaterPhase previousRutoWaterPhase;
    uint8_t danceStep;
    uint8_t diagnosticDrawLogged;
    uint16_t greatFairyHoverPhase;
    f32 kokiriLegFrame;
    uint16_t mmHoverPhase;
    uint16_t tatlPulsePhase;
    uint8_t tatlInitialized;
    SkelAnime tatlSkelAnime;
    FlexSkeletonHeader tatlSkeleton;
    StandardLimb tatlLimbs[6];
    void* tatlLimbPtrs[6];
    Gfx* skullKidHeadDL;
    Gfx* skullKidEyesDL;
    Gfx* skullKidMaskDL;
    Gfx* skullKidLimbDLs[22];
    void* mmEyeTextures[8];
    void* mmMouthTextures[4];
    void* mmResourceOwner;
    Gfx* anjuUmbrellaDL;
    const struct MmAnjuDisplayLists* anjuModel;
    uint8_t anjuBlinkPhase;
    int16_t anjuBlinkTimer;
    const int16_t* mmPlayerFrames;
    uint16_t mmPlayerFrameCount;
    uint16_t mmAppearance;
} EnViewerStaticState;

typedef struct EnViewer {
    /* 0x0000 */ Actor actor;
    /* 0x014C */ Skin skin;
    /* 0x01DC */ s8 animObjBankIndex;
    /* 0x01DD */ u8 drawFuncIndex;
    /* 0x01E0 */ EnViewerActionFunc actionFunc;
    /* 0x01E4 */ u8 unused;
    /* 0x01E5 */ u8 state;
    /* 0x01E6 */ u8 isVisible;
    EnViewerStaticState staticState;
    /* 0x01E8 */ EnViewerFireEffect fireEffects[20];
} EnViewer; // size = 0x05F8

#endif
