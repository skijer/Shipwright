#include "TimeControl.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace {

constexpr u8 FreezeRefreshFrames = 3;
constexpr size_t MaxTrackedHurtboxes = 64;
constexpr std::array<u8, 7> StoppableCategories = { ACTORCAT_SWITCH, ACTORCAT_BG,   ACTORCAT_EXPLOSIVE, ACTORCAT_NPC,
                                                    ACTORCAT_ENEMY,  ACTORCAT_MISC, ACTORCAT_BOSS };

struct TimeClaim {
    int32_t priority;
    float worldSpeed;
    bool freezeClock;
};

struct TrackedHurtbox {
    Actor* actor;
    Collider* collider;
};

std::map<std::string, TimeClaim> sClaims;
std::optional<TimeClaim> sWinningClaim;
std::vector<TrackedHurtbox> sHurtboxes;
std::optional<u16> sClockSpeedBeforeHold;
bool sWasStopped = false;

float WinningSpeed() {
    return sWinningClaim ? std::clamp(sWinningClaim->worldSpeed, 0.0f, 1.0f) : 1.0f;
}

bool IsStopped() {
    return sWinningClaim && WinningSpeed() <= 0.0f;
}

bool IsSlowed() {
    return sWinningClaim && WinningSpeed() > 0.0f && WinningSpeed() < 1.0f;
}

bool IsExempt(Actor* actor) {
    if (actor == nullptr || gPlayState == nullptr) {
        return true;
    }
    if (actor->category == ACTORCAT_PLAYER || actor->category == ACTORCAT_ITEMACTION) {
        return true;
    }
    return actor->parent == &GET_PLAYER(gPlayState)->actor;
}

void PickWinningClaim() {
    sWinningClaim.reset();
    for (const auto& [owner, claim] : sClaims) {
        if (!sWinningClaim || claim.priority > sWinningClaim->priority) {
            sWinningClaim = claim;
        }
    }
}

void TrackHurtbox(Collider* collider) {
    if (collider == nullptr || IsStopped() || IsExempt(collider->actor)) {
        return;
    }
    for (auto& tracked : sHurtboxes) {
        if (tracked.collider == collider) {
            tracked.actor = collider->actor;
            return;
        }
    }
    if (sHurtboxes.size() < MaxTrackedHurtboxes) {
        sHurtboxes.push_back({ collider->actor, collider });
    }
}

bool ReregisterHurtboxesWasHit(PlayState* play, Actor* actor) {
    bool wasHit = false;
    for (size_t index = 0; index < sHurtboxes.size(); ++index) {
        Collider* collider = sHurtboxes[index].collider;
        if (sHurtboxes[index].actor != actor || collider->actor != actor) {
            continue;
        }
        if (collider->acFlags & AC_HIT) {
            wasHit = true;
        } else if (collider->acFlags & AC_ON) {
            CollisionCheck_SetAC(play, &play->colChkCtx, collider);
        }
    }
    return wasHit;
}

void ForEachStoppableActor(PlayState* play, void (*visit)(PlayState*, Actor*)) {
    for (u8 category : StoppableCategories) {
        for (Actor* actor = play->actorCtx.actorLists[category].head; actor != nullptr; actor = actor->next) {
            if (!IsExempt(actor)) {
                visit(play, actor);
            }
        }
    }
}

void HoldActorStopped(PlayState* play, Actor* actor) {
    if (actor->sfx != 0) {
        actor->sfx = 0;
        Audio_StopSfxByPos(&actor->projectedPos);
    }
    actor->freezeTimer = ReregisterHurtboxesWasHit(play, actor) ? 0 : FreezeRefreshFrames;
}

void ReleaseActor(PlayState* play, Actor* actor) {
    actor->freezeTimer = 0;
}

void KeepSlowedActorHittable(PlayState* play, Actor* actor) {
    if (actor->freezeTimer > 1 && ReregisterHurtboxesWasHit(play, actor)) {
        actor->freezeTimer = 0;
    }
}

int16_t StutterFrames() {
    return static_cast<int16_t>(std::max(1, static_cast<int>(1.0f / WinningSpeed()) - 1));
}

void HoldDayClock(bool isHeld) {
    if (isHeld) {
        if (!sClockSpeedBeforeHold) {
            sClockSpeedBeforeHold = gTimeIncrement;
        }
        gTimeIncrement = 0;
        return;
    }
    if (sClockSpeedBeforeHold) {
        gTimeIncrement = *sClockSpeedBeforeHold;
        sClockSpeedBeforeHold.reset();
    }
}

void ApplyWorldSpeed() {
    if (gPlayState == nullptr) {
        return;
    }
    bool isStopped = IsStopped();
    if (isStopped) {
        ForEachStoppableActor(gPlayState, HoldActorStopped);
    } else if (sWasStopped) {
        ForEachStoppableActor(gPlayState, ReleaseActor);
        sHurtboxes.clear();
    }
    if (IsSlowed()) {
        ForEachStoppableActor(gPlayState, KeepSlowedActorHittable);
    }
    sWasStopped = isStopped;
    HoldDayClock(sWinningClaim && sWinningClaim->freezeClock);
}

void StutterSlowedActor(void* actor) {
    Actor* updated = static_cast<Actor*>(actor);
    if (IsSlowed() && !IsExempt(updated)) {
        updated->freezeTimer = StutterFrames();
    }
}

void StopStoppedActorMotion(Actor* actor, float* scale) {
    if (IsStopped() && !IsExempt(actor)) {
        *scale = 0.0f;
    }
}

void DropClaimsOnSceneChange(int16_t sceneNum) {
    sClaims.clear();
    sWinningClaim.reset();
    sHurtboxes.clear();
    sWasStopped = false;
    HoldDayClock(false);
}

} // namespace

void TimeControl_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(ApplyWorldSpeed);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(StutterSlowedActor);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorResolveMotionScale>(StopStoppedActorMotion);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnCollisionRegisterAC>(TrackHurtbox);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(DropClaimsOnSceneChange);
}

bool TimeControl_Request(const char* owner, int32_t priority, float worldSpeed, bool freezeClock) {
    if (owner == nullptr || owner[0] == '\0') {
        return false;
    }
    sClaims[owner] = { priority, worldSpeed, freezeClock };
    PickWinningClaim();
    ApplyWorldSpeed();
    return true;
}

void TimeControl_Release(const char* owner) {
    if (owner == nullptr || sClaims.erase(owner) == 0) {
        return;
    }
    PickWinningClaim();
    ApplyWorldSpeed();
}

float TimeControl_GetWorldSpeed(void) {
    return WinningSpeed();
}
