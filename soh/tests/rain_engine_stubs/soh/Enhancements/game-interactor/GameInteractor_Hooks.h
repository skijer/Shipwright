#pragma once
struct GameInteractor {
    struct OnGameFrameUpdate {};
    struct OnPlayDestroy {};
    struct OnExitGame {};
    static GameInteractor* Instance;
    template <class Hook, class Callback> void RegisterGameHook(Callback) {
    }
};
