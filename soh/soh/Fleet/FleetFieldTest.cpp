#include "FleetFieldTest.h"
#include "FleetForeignItems.h"
#include "FleetPortal.h"
#include "FleetItemDraw.h"
#include "FleetRuntime.h"
#include "FleetRuntimeBridge.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/audio/AudioCollection.h"
#include "soh/Enhancements/audio/AudioEditor.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
#include "soh/Enhancements/randomizer/randomizer_grotto.h"
#include "soh/Enhancements/randomizer/static_data.h"
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

constexpr float kPhotoDistance = 60.0f;
constexpr float kPhotoScale = 0.25f;
constexpr int kPhotoSettleFrames = 2;

// One item at a time in front of the camera, drawn by Fleet's own item drawing, for fleet-test to photograph.
struct Photo {
    std::string owner;
    std::string item;
    bool isQueued = false;
    int frames = 0;
    u32 serial = 0;
};

Photo sPhoto;
bool sIsPhotoHooked = false;

Vec3f FindPhotoSpot(PlayState* play) {
    const Vec3f& eye = play->view.eye;
    const Vec3f& at = play->view.lookAt;
    const Vec3f toward = { at.x - eye.x, at.y - eye.y, at.z - eye.z };
    const float length = std::max(1.0f, sqrtf(toward.x * toward.x + toward.y * toward.y + toward.z * toward.z));
    return { eye.x + toward.x / length * kPhotoDistance, eye.y + toward.y / length * kPhotoDistance,
             eye.z + toward.z / length * kPhotoDistance };
}

// OPEN_DISPS redeclares these inside the function; repeating them here as C keeps the link undecorated.
extern "C" {
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

// The corner square tells fleet-test which request this frame shows: in turbo the framebuffer can lag behind.
void DrawPhotoMarker(PlayState* play) {
    const u8 red = static_cast<u8>((sPhoto.serial & 31) << 3);
    const u8 green = static_cast<u8>(((sPhoto.serial >> 5) & 31) << 3);
    const u16 colour = GPACK_RGBA5551(red, green, 248, 1);
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_FILL);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(OVERLAY_DISP++, (colour << 16) | colour);
    gDPFillRectangle(OVERLAY_DISP++, 0, 0, 7, 7);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Drawn with the world and not as an actor, so nothing can collect it or open a textbox.
// Lit like an actor at the spot; the lights left by the last drawn actor could be anything.
void LightPhotoSpot(PlayState* play, Vec3f& spot) {
    Lights* lights = LightContext_NewLights(&play->lightCtx, play->state.gfxCtx);
    Lights_BindAll(lights, play->lightCtx.listHead, &spot);
    Lights_Draw(lights, play->state.gfxCtx);
}

void DrawPhotoItemInWorld() {
    if (sPhoto.item.empty() || !IsPlaying()) {
        return;
    }
    PlayState* play = gPlayState;
    Vec3f spot = FindPhotoSpot(play);
    LightPhotoSpot(play, spot);
    Matrix_Push();
    Matrix_Translate(spot.x, spot.y, spot.z, MTXMODE_NEW);
    Matrix_Scale(kPhotoScale, kPhotoScale, kPhotoScale, MTXMODE_APPLY);
    FleetItemDraw_DrawForeign(play, sPhoto.owner.c_str(), sPhoto.item.c_str());
    Matrix_Pop();
    DrawPhotoMarker(play);
}

// The camera looks at Link's head, so he would stand right behind every photographed item.
void HidePlayerDuringPhoto(PlayState* play) {
    static ActorFunc hiddenDraw = nullptr;
    Player* player = GET_PLAYER(play);
    if (player == nullptr) {
        return;
    }
    const bool isPhotographing = !sPhoto.item.empty();
    if (isPhotographing && player->actor.draw != nullptr) {
        hiddenDraw = player->actor.draw;
        player->actor.draw = nullptr;
    } else if (!isPhotographing && hiddenDraw != nullptr && player->actor.draw == nullptr) {
        player->actor.draw = hiddenDraw;
        hiddenDraw = nullptr;
    }
}

// A textbox left open by a delivery would cover every photo, and a hidden game has nobody to close it.
void AdvancePhoto(PlayState* play) {
    const bool isTextboxOpen = play->msgCtx.msgMode != MSGMODE_NONE;
    if (!sPhoto.item.empty() && isTextboxOpen && Message_GetState(&play->msgCtx) != TEXT_STATE_CLOSING) {
        Message_CloseTextbox(play);
    }
    if (!sIsPhotoHooked) {
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(DrawPhotoItemInWorld);
        sIsPhotoHooked = true;
    }
    HidePlayerDuringPhoto(play);
    if (sPhoto.isQueued) {
        sPhoto.isQueued = false;
        sPhoto.frames = 0;
    } else if (!sPhoto.item.empty()) {
        sPhoto.frames++;
    }
}

enum class ShowPhase { Idle, Queued, Started, Showing, Done, Unknown };

// A real get-item for fleet-test's review: Link holds the item up with its own textbox, as from a chest.
struct GetItemShow {
    std::string item;
    std::string owner;
    std::string label;
    ShowPhase phase = ShowPhase::Idle;
    int quietFrames = 0;
};

constexpr int kShowQuietFrames = 10;
constexpr const char* kShowGame = "oot";

GetItemShow sShow;
// Each item is shown against the save as it stood before, so earlier gifts cannot turn later ones into junk.
SaveContext sSaveBeforeShow;

bool IsShowingGetItem(PlayState* play) {
    const Player* player = GET_PLAYER(play);
    return (player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) || player->getItemId != GI_NONE ||
           play->msgCtx.msgMode != MSGMODE_NONE;
}

// As a check holding the item would hand it over: this game's own, or the other game's through Fleet.
std::optional<GetItemEntry> FindShowEntry() {
    if (sShow.owner != kShowGame) {
        const std::string key = FleetForeignItems_RegisterLoose(sShow.owner, sShow.item, sShow.label);
        const SOHCustomItemDefinition* foreign = CustomItemRegistry_Find(key.c_str());
        return foreign != nullptr ? std::optional<GetItemEntry>(foreign->getItemEntry) : std::nullopt;
    }
    const auto found = Rando::StaticData::itemNameToEnum.find(sShow.item);
    if (found == Rando::StaticData::itemNameToEnum.end()) {
        return std::nullopt;
    }
    return Rando::StaticData::RetrieveItem(found->second).GetGIEntry_Copy();
}

void StartGetItemShow(PlayState* play) {
    const std::optional<GetItemEntry> entry = FindShowEntry();
    if (!entry) {
        sShow.phase = ShowPhase::Unknown;
        return;
    }
    if (IsShowingGetItem(play) || Player_InCsMode(play)) {
        return;
    }
    sSaveBeforeShow = gSaveContext;
    if (GiveItemEntryWithoutActor(play, *entry)) {
        sShow.phase = ShowPhase::Started;
    }
}

// The clock keeps running: a save restored with an earlier time would rewind the day.
void RestoreSaveKeepingTime() {
    const u16 dayTime = gSaveContext.dayTime;
    const s32 nightFlag = gSaveContext.nightFlag;
    const s32 totalDays = gSaveContext.totalDays;
    const s32 bgsDayCount = gSaveContext.bgsDayCount;
    const u16 skyboxTime = gSaveContext.skyboxTime;
    gSaveContext = sSaveBeforeShow;
    gSaveContext.dayTime = dayTime;
    gSaveContext.nightFlag = nightFlag;
    gSaveContext.totalDays = totalDays;
    gSaveContext.bgsDayCount = bgsDayCount;
    gSaveContext.skyboxTime = skyboxTime;
}

// Finished once Link has been free of the item and its textbox for a moment.
void AdvanceGetItemShow(PlayState* play) {
    switch (sShow.phase) {
        case ShowPhase::Queued:
            StartGetItemShow(play);
            break;
        case ShowPhase::Started:
        case ShowPhase::Showing:
            if (IsShowingGetItem(play)) {
                sShow.phase = ShowPhase::Showing;
                sShow.quietFrames = 0;
            } else if (++sShow.quietFrames >= kShowQuietFrames) {
                RestoreSaveKeepingTime();
                sShow.phase = ShowPhase::Done;
            }
            break;
        case ShowPhase::Idle:
        case ShowPhase::Done:
        case ShowPhase::Unknown:
            break;
        default:
            sShow.phase = ShowPhase::Unknown;
            break;
    }
}

bool IsShowActive() {
    return sShow.phase == ShowPhase::Started || sShow.phase == ShowPhase::Showing;
}

// The review reads each get-item text whole, as Ship's instant text speed shows it.
void FillTextDuringShows() {
    static bool isHooked = false;
    if (isHooked) {
        return;
    }
    REGISTER_VB_SHOULD(VB_TEXT_CRAWL_FASTER, {
        if (IsShowActive()) {
            gPlayState->msgCtx.textDrawPos = gPlayState->msgCtx.decodedTextLen + 1;
            *should = true;
        }
    });
    isHooked = true;
}

const char* ShowPhaseName() {
    switch (sShow.phase) {
        case ShowPhase::Idle:
            return "idle";
        case ShowPhase::Queued:
            return "queued";
        case ShowPhase::Started:
            return "started";
        case ShowPhase::Showing:
            return "showing";
        case ShowPhase::Done:
            return "done";
        case ShowPhase::Unknown:
            return "unknown";
        default:
            return "invalid";
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
                              { "photo",
                                { { "item", sPhoto.item },
                                  { "marked", true },
                                  { "ready", !sPhoto.isQueued && sPhoto.frames >= kPhotoSettleFrames && IsPlaying() &&
                                                gPlayState->msgCtx.msgMode == MSGMODE_NONE } } },
                              { "getItem", { { "item", sShow.item }, { "phase", ShowPhaseName() } } },
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

static bool sIsTurbo = false;

bool FleetFieldTest_IsTurbo() {
    return sIsTurbo;
}

std::optional<std::string> FleetFieldTest_Request(const std::string& request, const nlohmann::json& input) {
    if (request == "TestTurbo") {
        sIsTurbo = input.value("on", false);
        return nlohmann::json{ { "turbo", sIsTurbo } }.dump();
    }
    if (request == "TestItemPhoto") {
        sPhoto = { input.value("owner", std::string{}), input.value("item", std::string{}), true, 0,
                   input.value("serial", 0u) };
        return nlohmann::json{ { "queued", sPhoto.item } }.dump();
    }
    if (request == "TestGetItem") {
        FillTextDuringShows();
        const std::string item = input.value("item", std::string{});
        sShow = { item, input.value("owner", std::string(kShowGame)), input.value("label", item), ShowPhase::Queued, 0 };
        return Status().dump();
    }
    if (request == "TestItemAssets") {
        auto assets = nlohmann::json::object();
        for (const auto& item : input.value("items", nlohmann::json::array())) {
            if (item.is_string()) {
                assets[item.get<std::string>()] = FleetItemDraw_DescribeIcon(item.get<std::string>().c_str());
            }
        }
        return nlohmann::json{ { "assets", assets } }.dump();
    }
    // Overwrites the player's music choices when "randomize" is set: run it on a test profile.
    if (request == "TestAudioPool") {
        if (input.value("randomize", false)) {
            AudioEditor_RandomizeAll();
        }
        return nlohmann::json{ { "forbidden", AudioCollection::Instance->ListForbiddenReplacements() } }.dump();
    }
    if (request == "TestOwns") {
        return nlohmann::json{ { "owned", FleetRuntime_ReadOwnership(input.value("items", nlohmann::json::array())) } }
            .dump();
    }
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
    AdvancePhoto(play);
    AdvanceGetItemShow(play);
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
