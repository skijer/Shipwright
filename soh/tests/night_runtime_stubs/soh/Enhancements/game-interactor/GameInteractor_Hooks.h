#pragma once
#include <functional>
#include <vector>
class GameInteractor {
  public:
    struct OnGameFrameUpdate {};
    struct OnPlayDestroy {};
    static GameInteractor* Instance;
    template <class Hook> static auto& Hooks() {
        static std::vector<std::function<void()>> hooks;
        return hooks;
    }
    template <class Hook, class F> void RegisterGameHook(F callback) {
        Hooks<Hook>().emplace_back(callback);
    }
};
