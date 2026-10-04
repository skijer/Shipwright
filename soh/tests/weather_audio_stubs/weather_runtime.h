#pragma once
#include <cstdint>
#include "libultraship/bridge/consolevariablebridge.h"

struct Color_RGB8 {
    uint8_t r, g, b;
};
struct PlayState {
    int16_t sceneNum;
    uint8_t skyboxId;
    struct {
        uint8_t state;
    } csCtx;
    struct {
        struct {
            int8_t num;
        } curRoom;
    } roomCtx;
    struct {
        uint8_t indoors;
        uint8_t skyboxDisabled;
        uint8_t gloomySkyMode;
        uint8_t lightningMode;
        uint8_t unk_EE[4];
    } envCtx;
};
#define SKYBOX_NORMAL_SKY 1
#define CS_STATE_IDLE 0
#define LIGHTNING_MODE_OFF 0
#define LIGHTNING_MODE_ON 1
#define LIGHTNING_MODE_LAST 2
#define LIGHTNING_STRIKE_WAIT 0
struct LightningStrike {
    uint8_t state;
};
extern "C" {
extern LightningStrike gLightningStrike;
Color_RGB8 CVarGetColor24(const char*, Color_RGB8);
int16_t Rand_S16Offset(int16_t base, int16_t range);
uint8_t Audio_IsNatureRainEnabled(void);
}

// Hook registration is the external boundary; tests drive Update/Reset directly.
struct GameInteractor {
    struct OnGameFrameUpdate {};
    struct OnPlayDestroy {};
    struct OnExitGame {};
    static GameInteractor* Instance;
    template <class Hook, class Callback> void RegisterGameHook(Callback) {
    }
};
