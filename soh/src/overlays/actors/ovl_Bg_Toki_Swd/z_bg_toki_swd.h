#ifndef Z_BG_TOKI_SWD_H
#define Z_BG_TOKI_SWD_H

#include <libultraship/libultra.h>
#include "global.h"

struct BgTokiSwd;

// Exact custom placement. All other parameters retain the Temple of Time actor.
#define BG_TOKI_SWD_TIME_PEDESTAL 0x4C57

typedef enum {
    BG_TOKI_SWD_HAND_UNCHANGED,
    BG_TOKI_SWD_HAND_MASTER_SWORD,
    BG_TOKI_SWD_HAND_CLOSED,
} BgTokiSwdHandState;

typedef void (*BgTokiSwdActionFunc)(struct BgTokiSwd*, PlayState*);

typedef struct BgTokiSwd {
    /* 0x0000 */ Actor actor;
    /* 0x014C */ BgTokiSwdActionFunc actionFunc;
    /* 0x0150 */ ColliderCylinder collider;
    // Used only by BG_TOKI_SWD_TIME_PEDESTAL; never written to the save.
    CutsceneData* localCutscene;
    Vec3f returnPos;
    s16 returnYaw;
    s16 ageSwapFrame;
    u8 localCutsceneStarted;
    u8 localCutsceneFinished;
    u8 skipFadeFrame;
} BgTokiSwd;

#ifdef __cplusplus
extern "C" {
#endif

void TimePedestalCutscene_TransformPoint(Vec3f* point, const Vec3f* origin, s16 yaw);
size_t TimePedestalCutscene_Build(CutsceneData* output, size_t capacity, const CutsceneData* source,
                                size_t wordCount, const Vec3f* origin, s16 yaw, s16* ageSwapFrame);
s32 BgTokiSwd_RelocateTimePedestalPlayer(PlayState* play, Player* player);
s32 BgTokiSwd_GetTimePedestalHandState(PlayState* play, Player* player);
s32 BgTokiSwd_GetChildSwordPullFloor(PlayState* play, Player* player, f32* floorY);
s32 BgTokiSwd_BeginTimePedestalArrival(PlayState* play, Player* player);
s32 BgTokiSwd_IsTimePedestalArrival(PlayState* play, Player* player);
void BgTokiSwd_UpdateTimePedestalArrivalCamera(PlayState* play, Player* player);
void BgTokiSwd_UpdateTimePedestalFill(PlayState* play, Player* player);
s32 BgTokiSwd_SkipTimePedestalArrival(PlayState* play, Player* player);
s32 BgTokiSwd_EndTimePedestalArrival(PlayState* play, Player* player);

#ifdef __cplusplus
}
#endif

#endif
