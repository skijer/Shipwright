#include "FleetFieldTest.h"
#include "FleetForeignItems.h"
#include "FleetPortal.h"
#include "FleetRuntime.h"
#include "FleetRuntimeBridge.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
#include "soh/Enhancements/randomizer/randomizer_grotto.h"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/RandoItems/RandoItems.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

void EnItem00_DrawRandomizedItem(EnItem00* enItem00, PlayState* play);

namespace {

struct Travel {
    std::string mode;
    int32_t entrance;
    std::string passage;
};

constexpr int32_t kGrottoHoleParams = 0x04FF;
constexpr int kRainOnLinkEvery = 3;
constexpr float kRainRadius = 220.0f;
constexpr float kRainHeight = 160.0f;
constexpr int kMessageCloseFrames = 600;
constexpr size_t kShowcaseBatch = 24;
constexpr int kShowcaseDrawFrames = 45;
constexpr int kShowcaseTextFrames = 30;

enum class ShowcasePhase { NotStarted, Drawing, Texts, Done };

// Draws every foreign item in a real frame next to Link, then opens each one's get-item text until its icon draws.
struct Showcase {
    ShowcasePhase phase = ShowcasePhase::NotStarted;
    std::vector<std::string> keys;
    std::set<std::string> shownIcons;
    size_t next = 0;
    size_t texts = 0;
    int frames = 0;
};

std::optional<Travel> sTravel;
Showcase sShowcase;
std::string sTravelResult = "idle";
std::optional<int> sRainCount;
std::vector<Actor*> sRainDrops;
std::set<RandomizerCheck> sRained;
int sMessageCloseFrames = 0;
int sNudgeFrames = 0;
s8 sNudgeStickX = 0;
s8 sNudgeStickY = 0;
bool sIsDriving = false;

bool IsPlaying() {
    return gPlayState != nullptr && gGameState == &gPlayState->state && gSaveContext.gameMode == GAMEMODE_NORMAL;
}

void HoldStick() {
    if (sNudgeFrames <= 0) {
        return;
    }
    --sNudgeFrames;
    Input& input = gPlayState->state.input[0];
    input.cur.stick_x = sNudgeStickX;
    input.cur.stick_y = sNudgeStickY;
    input.rel.stick_x = sNudgeStickX;
    input.rel.stick_y = sNudgeStickY;
}

// Runs after the pad is read and before Play. Walks drive Link into water and voids on purpose, so he keeps a full
// heart bar every frame instead of reaching a game over.
void DriveTestFrame() {
    if (!IsPlaying()) {
        return;
    }
    gSaveContext.health = gSaveContext.healthCapacity;
    HoldStick();
}

void StartDriving() {
    if (sIsDriving) {
        return;
    }
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(DriveTestFrame);
    sIsDriving = true;
}

void Nudge(const nlohmann::json& input) {
    sNudgeStickX = static_cast<s8>(input.value("x", 0));
    sNudgeStickY = static_cast<s8>(input.value("y", 0));
    sNudgeFrames = input.value("frames", 0);
}

bool IsTransitioning(const PlayState* play) {
    return play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF;
}

bool IsGrottoLoad(int32_t entrance) {
    return entrance >= ENTRANCE_GROTTO_LOAD_START && entrance < ENTRANCE_GROTTO_LOAD_START + NUM_GROTTOS;
}

bool IsGrottoExit(int32_t entrance) {
    return entrance >= ENTRANCE_GROTTO_EXIT_START && entrance < ENTRANCE_GROTTO_EXIT_START + NUM_GROTTOS;
}

// Mirrors the player's scene-exit code, where every grotto leaves through ENTR_RETURN_GROTTO.
bool TakeSceneExit(PlayState* play, int32_t exit) {
    play->nextEntranceIndex = Entrance_OverrideNextIndex(static_cast<s16>(IsGrottoExit(exit) ? ENTR_RETURN_GROTTO
                                                                                             : exit));
    if (play->nextEntranceIndex == ENTR_RETURN_GROTTO) {
        gSaveContext.respawnFlag = 2;
        play->nextEntranceIndex = gSaveContext.respawn[RESPAWN_MODE_RETURN].entranceIndex;
        play->transitionType = TRANS_TYPE_FADE_WHITE;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_WHITE;
    } else if (play->nextEntranceIndex >= ENTR_RETURN_YOUSEI_IZUMI_YOKO) {
        return false;
    } else {
        gSaveContext.retainWeatherMode = 1;
        Scene_SetTransitionForNextEntrance(play);
    }
    play->transitionTrigger = TRANS_TRIGGER_START;
    return true;
}

void Warp(PlayState* play, int32_t entrance) {
    gSaveContext.respawnFlag = 0;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    play->nextEntranceIndex = static_cast<s16>(FleetRuntime_Materialize(entrance));
    play->transitionTrigger = TRANS_TRIGGER_START;
}

// Falling into a hole stores the hole as the way back and loads the grotto, as Door_Ana does.
void EnterGrottoHole(PlayState* play, int32_t grottoLoad) {
    Play_SetupRespawnPoint(play, RESPAWN_MODE_RETURN, kGrottoHoleParams);
    const int32_t composed = FleetRuntime_ResolveExit(grottoLoad, 0);
    play->nextEntranceIndex = Grotto_OverrideSpecialEntrance(static_cast<s16>(composed >= 0 ? composed : grottoLoad));
    play->transitionTrigger = TRANS_TRIGGER_START;
}

// Cross routes sit on top of the native shuffle, so Link walks through the door that natively leads to the entrance.
int32_t FindNativeDoorTo(int32_t entrance) {
    for (int32_t door = 0; door < ENTRANCE_TABLE_SIZE; door++) {
        if (door != entrance && Entrance_GetOverride(static_cast<s16>(door)) == entrance) {
            return door;
        }
    }
    return entrance;
}

bool Enter(PlayState* play, int32_t entrance) {
    if (!IsGrottoLoad(entrance)) {
        return TakeSceneExit(play, FindNativeDoorTo(entrance));
    }
    EnterGrottoHole(play, entrance);
    return true;
}

bool RunTravel(PlayState* play, const Travel& travel) {
    if (travel.mode == "field") {
        Warp(play, ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN);
        return true;
    }
    if (travel.mode == "warp") {
        Warp(play, travel.entrance);
        return true;
    }
    if (travel.mode == "enter") {
        return Enter(play, travel.entrance);
    }
    if (travel.mode == "exit") {
        return TakeSceneExit(play, travel.entrance);
    }
    if (travel.mode == "passage") {
        FleetPortal_RequestDeparture(travel.passage.c_str());
        return true;
    }
    return false;
}

bool IsForeignItem(RandomizerGet item) {
    const char* key = RandoItems_GetKey(item);
    return key != nullptr && std::strncmp(key, "fleet:", 6) == 0;
}

RandomizerInf RainFlag(RandomizerCheck check) {
    return OTRGlobals::Instance->gRandomizer->GetRandomizerInfFromCheck(check);
}

std::vector<RandomizerCheck> ListRainChecks() {
    std::vector<RandomizerCheck> foreign;
    std::vector<RandomizerCheck> native;
    auto* context = Rando::Context::GetInstance().get();
    for (int32_t id = RC_UNKNOWN_CHECK + 1; id < RC_MAX; id++) {
        const auto check = static_cast<RandomizerCheck>(id);
        const RandomizerInf flag = RainFlag(check);
        auto* location = context->GetItemLocation(check);
        if (flag == RAND_INF_MAX || Flags_GetRandomizerInf(flag) || location->HasObtained() ||
            location->GetPlacedRandomizerGet() == RG_NONE || sRained.contains(check)) {
            continue;
        }
        (IsForeignItem(location->GetPlacedRandomizerGet()) ? foreign : native).push_back(check);
    }
    foreign.insert(foreign.end(), native.begin(), native.end());
    return foreign;
}

void ClearRain(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != nullptr; actor = actor->next) {
        if (std::find(sRainDrops.begin(), sRainDrops.end(), actor) != sRainDrops.end()) {
            Actor_Kill(actor);
        }
    }
    sRainDrops.clear();
}

// Drops seed checks the way shuffled grass does: every third lands on Link and is collected, the rest only draw.
void RainItems(PlayState* play, int count) {
    ClearRain(play);
    const Vec3f feet = GET_PLAYER(play)->actor.world.pos;
    const std::vector<RandomizerCheck> checks = ListRainChecks();
    const int total = std::min<int>(count, static_cast<int>(checks.size()));
    for (int i = 0; i < total; i++) {
        const bool isOnLink = i % kRainOnLinkEvery == 0;
        const s16 angle = static_cast<s16>(i * 0x10000 / std::max(total, 1));
        const float radius = isOnLink ? 0.0f : kRainRadius;
        Vec3f pos = { feet.x + Math_SinS(angle) * radius, feet.y + (isOnLink ? 0.0f : kRainHeight),
                      feet.z + Math_CosS(angle) * radius };
        EnItem00* drop = Item_DropCollectible2(play, &pos, ITEM00_SOH_DUMMY);
        if (drop == nullptr) {
            continue;
        }
        drop->randoInf = RainFlag(checks[i]);
        drop->itemEntry = Rando::Context::GetInstance()->GetFinalGIEntry(checks[i], true, GI_NONE);
        drop->actor.draw = reinterpret_cast<ActorFunc>(EnItem00_DrawRandomizedItem);
        sRainDrops.push_back(&drop->actor);
        sRained.insert(checks[i]);
    }
    sMessageCloseFrames = kMessageCloseFrames;
}

void CloseRainMessages(PlayState* play) {
    if (sMessageCloseFrames <= 0) {
        return;
    }
    --sMessageCloseFrames;
    const u8 state = Message_GetState(&play->msgCtx);
    if (play->msgCtx.msgMode != MSGMODE_NONE && state != TEXT_STATE_CLOSING && state != TEXT_STATE_NONE) {
        Message_CloseTextbox(play);
    }
}

int CountCollected() {
    return static_cast<int>(std::count_if(sRained.begin(), sRained.end(), [](RandomizerCheck check) {
        return Flags_GetRandomizerInf(RainFlag(check));
    }));
}

void DrawNextShowcaseBatch(PlayState* play) {
    ClearRain(play);
    if (sShowcase.next == sShowcase.keys.size()) {
        sShowcase.phase = ShowcasePhase::Texts;
        sShowcase.next = 0;
        return;
    }
    const Vec3f feet = GET_PLAYER(play)->actor.world.pos;
    const size_t end = std::min(sShowcase.next + kShowcaseBatch, sShowcase.keys.size());
    for (size_t index = sShowcase.next; index < end; index++) {
        const SOHCustomItemDefinition* definition = CustomItemRegistry_Find(sShowcase.keys[index].c_str());
        const s16 angle = static_cast<s16>((index - sShowcase.next) * 0x10000 / kShowcaseBatch);
        Vec3f pos = { feet.x + Math_SinS(angle) * kRainRadius, feet.y + kRainHeight,
                      feet.z + Math_CosS(angle) * kRainRadius };
        EnItem00* drop = definition != nullptr ? Item_DropCollectible2(play, &pos, ITEM00_SOH_DUMMY) : nullptr;
        if (drop == nullptr) {
            continue;
        }
        drop->randoInf = RAND_INF_MAX;
        drop->itemEntry = definition->getItemEntry;
        drop->actor.draw = reinterpret_cast<ActorFunc>(EnItem00_DrawRandomizedItem);
        sRainDrops.push_back(&drop->actor);
    }
    sShowcase.next = end;
    sShowcase.frames = kShowcaseDrawFrames;
}

void ShowNextShowcaseText(PlayState* play) {
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        if (Message_GetState(&play->msgCtx) != TEXT_STATE_CLOSING) {
            Message_CloseTextbox(play);
        }
        return;
    }
    while (sShowcase.next < sShowcase.keys.size()) {
        const std::string& key = sShowcase.keys[sShowcase.next++];
        const char* icon = CustomItemRegistry_ResolveTexture(key.c_str(), SOH_ITEM_ICON_TEXTBOX);
        if (icon == nullptr || !sShowcase.shownIcons.insert(icon).second) {
            continue;
        }
        if (CustomItemRegistry_ShowGetItemText(play, key.c_str())) {
            ++sShowcase.texts;
            sShowcase.frames = kShowcaseTextFrames;
            return;
        }
    }
    sShowcase.phase = ShowcasePhase::Done;
}

void AdvanceShowcase(PlayState* play) {
    if (sShowcase.frames > 0) {
        --sShowcase.frames;
        return;
    }
    switch (sShowcase.phase) {
        case ShowcasePhase::Drawing:
            DrawNextShowcaseBatch(play);
            break;
        case ShowcasePhase::Texts:
            ShowNextShowcaseText(play);
            break;
        default:
            break;
    }
}

const char* ShowcasePhaseName() {
    switch (sShowcase.phase) {
        case ShowcasePhase::NotStarted:
            return "notStarted";
        case ShowcasePhase::Drawing:
            return "drawing";
        case ShowcasePhase::Texts:
            return "texts";
        case ShowcasePhase::Done:
            return "done";
        default:
            return "unknown";
    }
}

nlohmann::json Status() {
    const bool isPlaying = IsPlaying();
    nlohmann::json status = { { "playing", isPlaying },
                              { "scene", isPlaying ? static_cast<int>(gPlayState->sceneNum) : -1 },
                              { "transitioning", isPlaying && IsTransitioning(gPlayState) },
                              { "travel", sTravel ? "queued" : sTravelResult },
                              { "rained", sRained.size() },
                              { "collected", CountCollected() },
                              { "showcase",
                                { { "phase", ShowcasePhaseName() },
                                  { "keys", sShowcase.keys.size() },
                                  { "texts", sShowcase.texts } } },
                              { "slowestSaveMs", FleetRuntime_SlowestSaveMilliseconds() } };
    if (isPlaying) {
        const Player* player = GET_PLAYER(gPlayState);
        const Vec3f& pos = player->actor.world.pos;
        status["player"] = { pos.x, pos.y, pos.z };
        status["grounded"] = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
        status["free"] = !Player_InCsMode(gPlayState) && gPlayState->csCtx.state == CS_STATE_IDLE;
        status["nudging"] = sNudgeFrames > 0;
        status["msgMode"] = gPlayState->msgCtx.msgMode;
        status["pause"] = gPlayState->pauseCtx.state;
    }
    return status;
}

} // namespace

std::optional<std::string> FleetFieldTest_Request(const std::string& request, const nlohmann::json& input) {
    if (request == "TestTravel") {
        sTravel = Travel{ input.value("mode", ""), input.value("entrance", -1), input.value("passage", "") };
        sTravelResult = "queued";
    } else if (request == "TestItemRain") {
        sRainCount = input.value("count", 24);
    } else if (request == "TestNudge") {
        Nudge(input);
    } else if (request == "TestForeignShowcase") {
        sShowcase = { ShowcasePhase::Drawing, FleetForeignItems_ListKeys() };
    } else if (request != "TestStatus") {
        return std::nullopt;
    }
    if (request != "TestStatus") {
        StartDriving();
    }
    return Status().dump();
}

void FleetFieldTest_FrameEnd() {
    if (!IsPlaying()) {
        return;
    }
    PlayState* play = gPlayState;
    CloseRainMessages(play);
    if (IsTransitioning(play) || play->pauseCtx.state != 0) {
        return;
    }
    if (sShowcase.phase == ShowcasePhase::Drawing || sShowcase.phase == ShowcasePhase::Texts) {
        AdvanceShowcase(play);
        return;
    }
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        return;
    }
    if (sTravel) {
        sTravelResult = RunTravel(play, *sTravel) ? "started" : "unsupported " + sTravel->mode;
        sTravel.reset();
    }
    if (sRainCount) {
        RainItems(play, *sRainCount);
        sRainCount.reset();
    }
}
