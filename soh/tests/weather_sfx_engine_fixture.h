#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void WeatherSfxEngine_Reset(void);
void WeatherSfxEngine_Start(void);
void WeatherSfxEngine_StartDenseFlameHub(void);
void WeatherSfxEngine_StartSaturatedEnvironment(void);
int WeatherSfxEngine_IsNaviPlaying(void);
void WeatherSfxEngine_RequirePlaying(void);
void WeatherSfxEngine_Refresh(void);
#ifdef __cplusplus
}
#endif
