#ifndef YOUNG_EPONA_H
#define YOUNG_EPONA_H

#include "global.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENHORSE_YOUNG_PARAM 0x4000

s32 Horse_CanSpawnYoung(s32 scene);
s32 Horse_YoungEponaAssetsAvailable(void);
s32 Horse_CanUseYoungEpona(void);
Actor* Horse_FindYoungEpona(PlayState* play);
HorseData* Horse_GetActorSaveData(Actor* actor);
void Horse_SaveYoungEpona(PlayState* play, Actor* actor);
s32 Horse_TrySummonYoungEpona(PlayState* play);

#ifdef __cplusplus
}
#endif
#endif
