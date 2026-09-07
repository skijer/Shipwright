// FleetWarpBoot.cpp — UNIFIED cross-game warp arrival pipeline (OoT side) + combo-pair save
// validation + title-screen temp cleanup + Temple of Time fleet-hole spawner.
//
// EVERY warp addressed to OoT lands here (cold boot AND in-gameplay). The pipeline always:
//   1. force-opens the paired save slot from disk (creating it if missing),
//   2. applies the FleetSync temp overlays (own full anchor + shared player state),
//   3. sets EXPLICIT destination overrides (entrance/cutscene/respawn) LAST,
//   4. boots a fresh Play_Init.
// One code path, overrides always after any load = no stale cutsceneIndex (the old
// Scene_CommandAlternateHeaderList crash) and no load-machine stomping our respawn data.
//
// Runs every frame via GameInteractor::OnGameFrameUpdate (fires in ALL gamestates, and keeps
// firing while the game is combo-frozen).

#include "FleetShipCombo.h"
#include "FleetSync.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include "soh/ResourceManagerHelpers.h" // ResourceMgr_FileExists: is the waiting room actually packed?

#include <cstring>
#include <exception> // watchdog guards: std::exception
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h> // CVar: gFleetCombo.LastSaved* for boot-resume

extern "C" {
#include <z64.h>
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
extern GameState* gGameState;
extern SaveContext gSaveContext;
void Sram_OpenSave(void);
void Sram_InitSave(FileChooseContext* fileChooseCtx);
void Play_Init(GameState* state);
void FileChoose_Init(GameState* state);
void FileChoose_LoadGame(GameState* thisx); // SM_LOAD_GAME handler: load a slot at its OWN entrance
// Defined extern "C" in SaveManager.cpp, but SaveManager.h only declares it for C callers.
SaveFileMetaInfo* Save_GetSaveMetaInfo(int fileNum);
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId); // z_scene.c (not in a header)
// custom_items_common.c (unity-built into z_player.c): re-arm the Lost Woods trigger after an
// arrival so we don't instantly ping-pong back.
void FleetWarp_NotifyArrived(void);
// Same file: drives the sending fade when the PLAYER update isn't running it (see the watchdog note
// there). Called unconditionally from this tick, which fires in every gamestate.
void FleetWarp_SendFadeWatchdog(void);
// Same file: starts a RESUME hand-off to MM (fade out here, land in MM's own save).
void FleetWarp_StartResumeToMm(void);
void GameInteractor_ExecuteOnLoadGame(int32_t fileNum); // mods hook their per-file init on this
}

namespace {

// Locally cached pending warp (ConsumePendingWarp is one-shot, but the cold-boot path may need
// several frames to walk the gamestates: title -> file select -> load/boot).
bool sPending = false;
int sScene = 0;

// =================================================================================================
// WATCHDOGS — a warp must never leave the player stuck (mirror of the MM side)
// =================================================================================================
// Every stage of the arrival waits for a state the game normally reaches on its own: a gamestate to
// settle, a transition FSM to pick up a trigger, a hook to fire. When one of them doesn't happen,
// the player is left looking at a black screen with no error — and we can't reproduce it here. So
// each stage carries a deadline and a recovery, and every recovery logs a distinct [FleetWatchdog]
// line, which names the culprit in a stuck player's log.
constexpr int kPendingWarnFrames = 600;   // 10s of a warp we consumed but could not apply yet
constexpr int kPendingForceFrames = 1200; // 20s -> take the in-game path regardless of gameMode
constexpr int kArmedTransFrames = 40;     // frames an armed arrival transition may fail to start
constexpr int kArmedMaxRetries = 3;       // re-arms before we boot the destination outright
constexpr int kFadeStuckFrames = 120;     // 2s of leftover send fade with no warp in progress

int sPendingFrames = 0;
// RESUME hand-off: set when a combo file whose last save was in MM is loaded, consumed once OoT is
// actually in gameplay. See FleetCombo_QueueResumeToMm.
bool sResumeQueued = false;
int sResumeWaitFrames = 0;
constexpr int kResumeMaxWaitFrames = 3600; // 60s for OoT to reach gameplay before we give up
bool sArrivalArmed = false;                // ExecuteWarpInGame armed a transition; is it actually running?
int sArmedFrames = 0;
int sArmedRetries = 0;
s16 sArmedEntrance = 0;
int sFadeStuckFrames = 0;

// The Temple of Time EXTERIOR fleet hole (Door_Ana). Must match the MM South Clock Town pairing.
// Position captured by the user in-game (dev Player tab, room index 1): y=-40 IS solid floor here
// (an earlier assumption that it was void was wrong). Link pops OUT of this exact spot on arrival
// (grotto-return respawn). The hole ALWAYS spawns on scene load (user directive); re-entry right
// after an arrival is suppressed by the FleetSync GRAB COOLDOWN (visible but inert — z_door_ana.c).
constexpr float kTotHoleX = 464.523f;
constexpr float kTotHoleY = -40.0f;
constexpr float kTotHoleZ = 1651.472f;
constexpr s16 kTotHoleYaw = 15278; // Link's facing when he rises out of the hole (user capture)
constexpr u8 kTotHoleRoom = 1;     // ToT INTERIOR room index the spot lives in (Master Sword chamber)

// The hole lives in the Temple of Time INTERIOR (SCENE_TEMPLE_OF_TIME) — the user's captured spot is
// the Master Sword pedestal chamber (room 1), NOT the exterior courtyard.
// =================================================================================================
// LIMBO — the inactive game is PARKED, not frozen (mirror of the MM side; see FleetWarpArrival.cpp)
// =================================================================================================
// Before handing the game to MM, OoT walks Link into a sealed custom room ("fleet_scene" in
// soh.o2r: floor, walls, no exits, no music, time speed 0) and only THEN flips. Parked, it keeps
// running normally with input blocked; becoming active again is an ordinary in-game transition
// out of the room to wherever MM sent us. This replaces the FrameAdvance freeze, which left the
// inactive game half-alive (unfinished transitions, stale framebuffer, unfinished saves).
//
// The room hijacks SCENE_TEST01: an unused test map whose scene-table row is repointed at our
// custom scene at init, reached through its own vanilla entrance ENTR_TEST01_0.
constexpr s32 kLimboSceneId = SCENE_TEST01;
constexpr s16 kLimboEntrance = ENTR_TEST01_0;
constexpr int kLimboWaitMaxFrames = 300; // 5s to reach the room before we flip anyway (old behaviour)

struct LimboReturnState {
    bool valid = false;
    s32 entranceIndex = 0;
    s32 cutsceneIndex = 0;
    u16 nextCutsceneIndex = 0xFFEF;
    s32 respawnFlag = 0;
    s16 savedSceneNum = 0;
    u16 dayTime = 0; // frozen while parked (room time speed 0) and restored on the way out
};
LimboReturnState sLimboReturn;

// "Heading into the room": set when the limbo transition starts, cleared once the room is loaded
// (or after kLimboWaitMaxFrames). While set, the game is NOT suspended even though it is already
// inactive (the flip happens the same frame the transition starts), and the frozen-game guard in
// custom_items_common.c leaves its transition trigger alone -- otherwise the handover would freeze
// it mid-transition, the exact half-alive state the waiting room exists to abolish.
bool sLimboInFlight = false;
int sLimboInFlightFrames = 0;
// The warp we owe MM once we are parked (RequestWarp arguments, held across the room load).
int sLimboWarpScene = 0;
float sLimboWarpX = 0.0f, sLimboWarpY = 0.0f, sLimboWarpZ = 0.0f;
int sLimboWarpRotY = 0;
int sLimboWarpSlot = 0;

bool LimboInRoom() {
    return gPlayState != NULL && gPlayState->sceneNum == kLimboSceneId;
}

// Is the waiting room actually in an archive? Booting SCENE_TEST01 without it makes soh's loader
// fall back to Dodongo's Cavern (its "unable to load scene" default) — the player would be parked
// in a dungeon. A soh.o2r packed before the asset existed does exactly that. Checked once, on first
// use; if missing, limbo turns itself off and every caller flips in place as before, saying why.
bool sLimboAvailable = false;
bool sLimboChecked = false;
bool LimboAvailable() {
    if (!sLimboChecked) {
        sLimboChecked = true;
        sLimboAvailable = ResourceMgr_FileExists("scenes/shared/fleet_scene/fleet_scene") &&
                          ResourceMgr_FileExists("scenes/shared/fleet_scene/fleet_scene_room_0") &&
                          ResourceMgr_FileExists("scenes/shared/fleet_scene/fleet_scene_col");
        if (!sLimboAvailable) {
            SPDLOG_ERROR("[FleetLimbo] fleet_scene is NOT in any archive — soh.o2r was packed before the asset existed "
                         "(run the GenerateSohOtr target). Waiting room disabled; inactive OoT will freeze in place.");
        }
    }
    return sLimboAvailable;
}

void LimboStashReturnState() {
    sLimboReturn.valid = true;
    sLimboReturn.entranceIndex = gSaveContext.entranceIndex;
    sLimboReturn.cutsceneIndex = gSaveContext.cutsceneIndex;
    sLimboReturn.nextCutsceneIndex = gSaveContext.nextCutsceneIndex;
    sLimboReturn.respawnFlag = gSaveContext.respawnFlag;
    sLimboReturn.savedSceneNum = gSaveContext.savedSceneNum;
    sLimboReturn.dayTime = gSaveContext.dayTime;
}

void LimboSetSaveToRoom() {
    gSaveContext.entranceIndex = kLimboEntrance;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.nextCutsceneIndex = 0xFFEF;
    gSaveContext.respawnFlag = 0; // spawn from the room's own spawn, never a stale respawn point
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.nextDayTime = 0xFFFF;
}

// Start walking into the room from live gameplay (still the ACTIVE game). INSTANT because the send
// fade is already at full black. The flip happens once we are inside (FleetWarpBoot_Tick).
void LimboEnterFromGameplay() {
    if (gPlayState == NULL || !LimboAvailable()) {
        return; // no room to go to: the wait in FleetWarpBoot_Tick expires at once and flips in place
    }
    LimboStashReturnState();
    LimboSetSaveToRoom();
    gPlayState->nextEntranceIndex = kLimboEntrance;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_INSTANT;
    sLimboInFlight = true;
    sLimboInFlightFrames = 0;
    SPDLOG_INFO("[FleetLimbo] OoT heading into the waiting room (was at entrance {:#06x})",
                (int)sLimboReturn.entranceIndex);
}

// Repoint the unused test scene at our custom room. Runs once at init.
void LimboInstallScene() {
    SceneTableEntry* entry = &gSceneTable[kLimboSceneId];
    entry->sceneFile.vromStart = 0;
    entry->sceneFile.vromEnd = 0;
    entry->sceneFile.fileName = (char*)"fleet_scene"; // -> scenes/shared/fleet_scene/fleet_scene (soh.o2r)
    entry->titleFile.vromStart = 0;
    entry->titleFile.vromEnd = 0;
    entry->titleFile.fileName = NULL;
    entry->config = SDC_DEFAULT;
    SPDLOG_INFO("[FleetLimbo] waiting room installed over SCENE_TEST01 (entrance {:#06x})", (int)kLimboEntrance);
}

bool IsTotHoleScene(s32 sceneNum) {
    return sceneNum == SCENE_TEMPLE_OF_TIME;
}

// Pick the destination entrance for a scene id + set the grotto-return respawn if applicable.
// For the ToT hole it sets respawnFlag=2 + RESPAWN_MODE_RETURN so Link pops OUT of the counterpart
// hole (user's locked decision). The caller must NOT clear respawnFlag after this returns.
s16 FleetSelectDestination(int scene) {
    switch (scene) {
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_DAY:   // fleet hole home (MM may send any ToT id; they
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_NIGHT: // all resolve to the INTERIOR pedestal chamber)
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS:
        case SCENE_TEMPLE_OF_TIME: {
            // GROTTO POP-OUT: Link rises out of the ground at the hole spot the user captured inside
            // the Temple of Time INTERIOR (464.523, -40, 1651.472, room 1 = Master Sword chamber).
            // Scene AND position are BOTH the interior — matching them keeps Link on solid floor.
            // respawnFlag=2 -> Play_Init reads respawn[RETURN] (room 1, pos, yaw, grotto start mode).
            s16 entrance = ENTR_TEMPLE_OF_TIME_ENTRANCE; // SCENE_TEMPLE_OF_TIME interior, spawn 0
            gSaveContext.respawnFlag = 2;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].entranceIndex = entrance;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].roomIndex = kTotHoleRoom;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].pos.x = kTotHoleX;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].pos.y = kTotHoleY;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].pos.z = kTotHoleZ;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].yaw = kTotHoleYaw;
            // 0x04FF = params 0xFF + start mode 4 (PLAYER_START_MODE_GROTTO) — the exact value
            // vanilla Door_Ana passes to Play_SetupRespawnPoint. OoT has no PLAYER_PARAMS macro.
            gSaveContext.respawn[RESPAWN_MODE_RETURN].playerParams = 0x04FF;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].data = 0;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].tempSwchFlags = 0;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].tempCollectFlags = 0;
            return entrance;
        }
        case SCENE_MARKET_DAY:
            return ENTR_MARKET_DAY_OUTSIDE_HAPPY_MASK_SHOP;
        case SCENE_LOST_WOODS:
        default:
            return ENTR_LOST_WOODS_SOUTH_EXIT;
    }
}

// SEAMLESS in-game arrival (spiritual_stones.cpp ExecuteWarp style): OoT is ALREADY in gameplay
// with its save loaded — just apply the shared overlay and start a NORMAL scene transition to the
// destination. NO Play_Init reboot (that lost HUD/button/magic state = the "broken status") and NO
// disk reload (the save-sync signal keeps the paired file consistent separately). The flip's black
// covers the INSTANT-out; the FADE_BLACK-in reveals the destination.
void ExecuteWarpInGame(int slot) {
    sLimboReturn.valid = false; // leaving the waiting room (or never in it): the stash is spent
    FleetSync_BeginSwapTrace("arrival: MM -> OoT (in-game)");
    FleetSync_SwapTrace("B1. ApplyArrival enter");
    FleetSync_ApplyArrival(slot); // shared player-state overlay only
    FleetSync_SwapTrace("B2. ApplyArrival done");

    gSaveContext.respawnFlag = 0;
    s16 entrance = FleetSelectDestination(sScene);

    gSaveContext.entranceIndex = entrance;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.nextCutsceneIndex = 0xFFEF;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = entrance; // void-out anchor (never -1)
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = 0;

    gPlayState->nextEntranceIndex = entrance;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_INSTANT;         // no fade-out (the flip is already black)
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK; // fade-in reveal at the destination

    // Under watch from here: if the FSM never picks this trigger up (something else squashed it, the
    // player update isn't running) the arrival silently never happens and the warp is already spent.
    sArrivalArmed = true;
    sArmedFrames = 0;
    sArmedEntrance = entrance;
    // Destination armed: the black curtain the DEPARTING game left up can come down now (the
    // transition out of the room is INSTANT and the destination fades in from black).
    FleetShipCombo_SetSendFadeAlpha(0);

    FleetSync_SwapTrace("B3. arrival transition armed");
    FleetWarp_NotifyArrived();
    sPending = false;
    sPendingFrames = 0;
    // NO SET_NEXT_GAMESTATE — the transition FSM loads the scene; respawnFlag survives to Play_Init.
}

// Destination overrides + fresh boot. Runs AFTER the slot load + FleetSync overlays so nothing
// can stomp these values before Play_Init consumes them.
void ApplyDestinationAndBoot(GameState* state, int slot) {
    FleetSync_ApplyArrival(slot);

    // "START SAVE FILE" defaults — the exact block FileChoose_LoadGame applies after Sram_OpenSave
    // (z_file_choose.c). Skipping it booted a half-initialized file: disabled buttons, broken magic
    // meter/HUD alphas, stale timers ("broken status"). Destination overrides come AFTER.
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.respawnFlag = 0;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gSaveContext.dogParams = 0;
    gSaveContext.timerState = TIMER_STATE_OFF;
    gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
    gSaveContext.eventInf[0] = 0;
    gSaveContext.eventInf[1] = 0;
    gSaveContext.eventInf[2] = 0;
    gSaveContext.eventInf[3] = 0;
    gSaveContext.prevHudVisibilityMode = 0x32; // HUD visibility alpha
    gSaveContext.nayrusLoveTimer = 0;
    gSaveContext.healthAccumulator = 0;
    gSaveContext.magicState = MAGIC_STATE_IDLE;
    gSaveContext.prevMagicState = MAGIC_STATE_IDLE;
    gSaveContext.forcedSeqId = NA_BGM_GENERAL_SFX;
    gSaveContext.skyboxTime = 0;
    gSaveContext.nextTransitionType = TRANS_NEXT_TYPE_DEFAULT;
    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.chamberCutsceneNum = 0;
    gSaveContext.nextDayTime = 0xFFFF;
    gSaveContext.retainWeatherMode = 0;
    for (int buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.buttonStatus); buttonIndex++) {
        gSaveContext.buttonStatus[buttonIndex] = BTN_ENABLED;
    }
    gSaveContext.forceRisingButtonAlphas = gSaveContext.nextHudVisibilityMode = gSaveContext.hudVisibilityMode =
        gSaveContext.hudVisibilityModeTimer = gSaveContext.magicCapacity = 0;
    gSaveContext.magicFillTarget = gSaveContext.magic; // boot-time magic refill animation
    gSaveContext.magic = 0;
    gSaveContext.magicLevel = gSaveContext.magic;
    gSaveContext.naviTimer = 0;

    gSaveContext.respawnFlag = 0;
    s16 entrance = FleetSelectDestination(sScene); // natural entrance (no void); grotto only where safe
    gSaveContext.entranceIndex = entrance;
    gSaveContext.cutsceneIndex = 0; // NEVER inherit a stale scene-setup selector (crash C1)
    gSaveContext.nextCutsceneIndex = 0xFFEF;
    // Void-out anchor = the DESTINATION entrance (NEVER ENTR_LOAD_OPENING = -1: a void-out with
    // that sentinel garbage-loads entrance 0 = Inside the Deku Tree, in an inescapable loop).
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = entrance;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = 0;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gSaveContext.dogParams = 0;
    gSaveContext.timerState = TIMER_STATE_OFF;
    gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
    gSaveContext.nextDayTime = 0xFFFF;

    gSaveContext.gameMode = GAMEMODE_NORMAL;                // NEVER inherit the title-demo mode (no pause +
                                                            // loading zones cycling demo scenes like Dodongo's)
    GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum); // mods' per-file init (rando etc.)
    FleetWarp_NotifyArrived(); // arms the proximity trigger so we don't instantly flip back
    sPending = false;
    sPendingFrames = 0;
    sArrivalArmed = false;              // this path boots Play_Init directly — no transition to watch
    FleetShipCombo_SetSendFadeAlpha(0); // destination booting: lower the curtain the peer left up
    FleetSync_SwapTrace("B4. booting Play_Init at the destination (cold-boot path)");
    state->running = false;
    SET_NEXT_GAMESTATE(state, Play_Init, PlayState);
}

// NOTE: OoT's boot auto-resume was REMOVED (2026-07-23, user request). OoT's title/file select is
// the combo ENTRY POINT now — you pick your file (or the "COMBO" quest) there, so OoT must NOT
// auto-load its last-saved slot on boot. Cross-game WARP arrivals still auto-load (the sPending
// path below); only the no-warp boot-resume is gone, leaving OoT at its file select.

void FleetWarpBoot_Tick() {
    if (FleetShipCombo_GetActiveGame() < 0) {
        return; // combo not running
    }
    if (gGameState == NULL) {
        return;
    }

    // One-shot per process: reaching the title/file-select after launch wipes the temp file.
    // (No gPlayState check: OoT's title screen runs INSIDE a PlayState — the title demo.)
    if (gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN || gSaveContext.gameMode == GAMEMODE_FILE_SELECT) {
        FleetSync_OnTitleScreen();
    }

    // WATCHDOG — the send fade is ramped from the PLAYER update, which stops running in plenty of
    // ordinary situations. This hook doesn't, so it drives the fade whenever it sees it stall.
    FleetWarp_SendFadeWatchdog();

    // ---- WAITING-ROOM UPKEEP ----
    if (LimboInRoom() && sLimboReturn.valid) {
        // Time stands still while parked (the room's own time speed is 0; this is the backstop).
        gSaveContext.dayTime = sLimboReturn.dayTime;
    }

    // ---- IN-FLIGHT BOOKKEEPING (OoT -> waiting room, already inactive) ----
    // The send path flipped the moment Link started walking into the room; here we only notice
    // when the room is up (bounded, so a room that never loads cannot keep an inactive game ticking
    // forever -- it then falls back to the old freeze).
    if (sLimboInFlight) {
        if (LimboInRoom() && gPlayState != NULL && gPlayState->transitionMode == TRANS_MODE_OFF) {
            sLimboInFlight = false;
            sLimboInFlightFrames = 0;
            SPDLOG_INFO("[FleetLimbo] OoT parked in the waiting room");
        } else if (++sLimboInFlightFrames > kLimboWaitMaxFrames) {
            sLimboInFlight = false;
            sLimboInFlightFrames = 0;
            SPDLOG_ERROR("[FleetLimbo] waiting room not reached after {} frames (scene={:#x} mode={}) -- giving up, "
                         "inactive OoT falls back to the freeze",
                         kLimboWaitMaxFrames, gPlayState ? (int)gPlayState->sceneNum : -1,
                         gPlayState ? (int)gPlayState->transitionMode : -1);
        }
    }

    // RESUME HAND-OFF — the loaded combo file was last saved in MM, so walk the player across.
    // Waits for real gameplay on purpose: doing it any earlier means the departure would serialise a
    // half-loaded save, and the arrival on MM's side would take the cold-boot branch. Here, it is
    // the exact same path as stepping through the portal.
    if (sResumeQueued) {
        if (gPlayState != NULL && gSaveContext.gameMode == GAMEMODE_NORMAL && gSaveContext.fileNum >= 0 &&
            gSaveContext.fileNum <= 2 && !sPending) {
            sResumeQueued = false;
            sResumeWaitFrames = 0;
            SPDLOG_INFO("[FleetCombo] file last saved in MM -> handing the player over (resume warp, slot {})",
                        (int)gSaveContext.fileNum);
            FleetWarp_StartResumeToMm();
        } else if (++sResumeWaitFrames > kResumeMaxWaitFrames) {
            // OoT never reached a state we could hand off from. Drop it rather than fire the warp
            // later at a random moment — the player is already playing OoT, and the portal works.
            sResumeQueued = false;
            sResumeWaitFrames = 0;
            SPDLOG_WARN("[FleetCombo] resume hand-off dropped: OoT never reached gameplay (gameMode={} play={})",
                        (int)gSaveContext.gameMode, (int)(gPlayState != NULL));
        }
    }

    // WATCHDOG — an armed arrival transition that never starts. The trigger can be squashed by our
    // own frozen-game guard or by anything else that writes transitionTrigger in the same frame;
    // when that happens the warp is already consumed and nothing will retry it, so OoT just stays
    // where it was with the screen black. Re-arm, then boot the destination outright.
    if (sArrivalArmed && gPlayState != NULL) {
        if (gPlayState->transitionMode != TRANS_MODE_OFF) {
            sArrivalArmed = false; // the FSM took it: the scene load is under way
            sArmedFrames = 0;
            sArmedRetries = 0;
        } else if (++sArmedFrames > kArmedTransFrames) {
            sArmedFrames = 0;
            if (++sArmedRetries > kArmedMaxRetries) {
                SPDLOG_ERROR("[FleetWatchdog] armed arrival transition never started -> booting entrance {:#06x}",
                             (int)(u16)sArmedEntrance);
                sArrivalArmed = false;
                sArmedRetries = 0;
                gSaveContext.entranceIndex = sArmedEntrance;
                gPlayState->state.running = false;
                SET_NEXT_GAMESTATE(&gPlayState->state, Play_Init, PlayState);
            } else {
                SPDLOG_WARN("[FleetWatchdog] arrival transition did not start (try {}/{}) -> re-arming", sArmedRetries,
                            kArmedMaxRetries);
                gPlayState->nextEntranceIndex = sArmedEntrance;
                gPlayState->transitionTrigger = TRANS_TRIGGER_START;
                gPlayState->transitionType = TRANS_TYPE_INSTANT;
            }
        }
    } else if (gPlayState == NULL) {
        sArrivalArmed = false; // gamestate changed under us: the load is happening
    }

    // WATCHDOG — leftover send fade. The fade is a black overlay the host draws over whichever game
    // is on screen; left up with no warp in flight it reads as a hard freeze (the game is running,
    // the screen is black). Only the active game may clear it.
    if (FleetShipCombo_IsThisGameActive() && !sPending && FleetShipCombo_GetSendFadeAlpha() != 0) {
        if (++sFadeStuckFrames > kFadeStuckFrames) {
            sFadeStuckFrames = 0;
            SPDLOG_ERROR("[FleetWatchdog] send-fade left at alpha {} with no warp in progress -> cleared",
                         FleetShipCombo_GetSendFadeAlpha());
            FleetShipCombo_SetSendFadeAlpha(0);
        }
    } else {
        sFadeStuckFrames = 0;
    }

    if (!sPending) {
        int scene = 0, rotY = 0;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (FleetShipCombo_ConsumePendingWarp(&scene, &x, &y, &z, &rotY)) {
            FleetSync_BeginSwapTrace("warp addressed to OoT consumed");
            FleetSync_SwapTrace("B0. warp consumed — OoT is the active game now");
            sPending = true;
            sPendingFrames = 0;
            sScene = scene;
        }
    }
    if (!sPending) {
        sPendingFrames = 0;
        return; // no cross-game warp -> leave OoT at its title/file select (the combo entry point)
    }
    sPendingFrames++;

    int slot = FleetShipCombo_GetWarpSaveFile();
    if (slot < 0 || slot > 2) {
        slot = 0;
    }

    // WATCHDOG — the warp is consumed but no branch below has been able to act on it. Every branch
    // is gated on a gameMode/gamestate combination, so a mode we didn't anticipate (a cutscene mode,
    // a game-over, a state left over from the previous warp) means the player sits in a game that
    // will never arrive, with MM already frozen waiting for it. Say so, then take the in-game path
    // anyway: it only needs a live PlayState, not a particular mode.
    if (sPendingFrames == kPendingWarnFrames) {
        SPDLOG_WARN("[FleetWatchdog] warp pending {} frames and still unapplied (gameMode={} play={}) — "
                    "waiting for a state it can boot from",
                    sPendingFrames, (int)gSaveContext.gameMode, (int)(gPlayState != NULL));
    }
    if (sPendingFrames > kPendingForceFrames && gPlayState != NULL) {
        SPDLOG_ERROR("[FleetWatchdog] warp still pending after {} frames (gameMode={}) -> forcing the in-game "
                     "arrival regardless of game mode",
                     sPendingFrames, (int)gSaveContext.gameMode);
        gSaveContext.gameMode = GAMEMODE_NORMAL;
        gSaveContext.fileNum = slot;
        ExecuteWarpInGame(slot);
        return;
    }

    // REAL GAMEPLAY (already in a scene, save loaded): seamless in-game transition — NO reboot,
    // NO disk reload. This is the common case (flipping between two running games; parked in the
    // waiting room counts). Not while a transition is still running, though (the room's own fade-in,
    // a load in flight): arming ours on top of a live one makes the FSM read it as the completion of
    // its own. sPending keeps the warp; the next idle frame takes it.
    if (gPlayState != NULL && gSaveContext.gameMode == GAMEMODE_NORMAL) {
        if (gPlayState->transitionMode != TRANS_MODE_OFF) {
            return;
        }
        gSaveContext.fileNum = slot;
        ExecuteWarpInGame(slot);
        return;
    }

    // TITLE DEMO with the paired save already on disk: reboot straight into the destination
    // (full FileChoose_LoadGame "start save file" defaults) — the title demo can't do an in-game
    // transition. A MISSING pair still routes through the file select below (Sram_InitSave needs a
    // real FileChooseContext).
    if (Save_GetSaveMetaInfo(slot)->valid && gGameState != NULL) {
        gSaveContext.fileNum = slot;
        Sram_OpenSave(); // reload the slot from disk; FleetSync anchor/shared overlay next
        ApplyDestinationAndBoot(gGameState, slot);
        return;
    }

    // COLD BOOT: title -> file select -> validate/create pair -> load -> unified pipeline.
    if (gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN) {
        gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
        gGameState->running = false;
        SET_NEXT_GAMESTATE(gGameState, FileChoose_Init, FileChooseContext);
        return;
    }
    if (gSaveContext.gameMode != GAMEMODE_FILE_SELECT) {
        return; // logo/other boot states -> wait; we'll land at title/file select
    }
    if (gPlayState != NULL) {
        return; // the old PlayState (title demo) is still tearing down: gGameState isn't the
                // FileChooseContext yet — casting now would scribble on a dying gamestate
    }

    FileChooseContext* fc = (FileChooseContext*)gGameState;
    gSaveContext.fileNum = slot;

    // COMBO-PAIR VALIDATION: if OoT's relative of the MM file doesn't exist, CREATE it now
    // (fresh vanilla save persisted to disk) so OoT file_N <-> MM file_N always exists.
    if (!Save_GetSaveMetaInfo(slot)->valid) {
        static const u8 kDefaultName[8] = { 21, 44, 43, 46, 62, 62, 62, 62 }; // "LINK"
        memcpy(Save_GetSaveMetaInfo(slot)->playerName, kDefaultName, 8);
        fc->buttonIndex = (s16)slot;
        fc->n64ddFlag = 0;
        Sram_InitSave(fc);
    }

    gSaveContext.gameMode = GAMEMODE_NORMAL;
    Sram_OpenSave();
    ApplyDestinationAndBoot(gGameState, slot);
}

// ---------------------------------------------------------------------------------------------
// Temple of Time fleet hole: spawn a real Door_Ana (open grotto) at the warp spot. Needs
// OBJECT_GAMEPLAY_FIELD_KEEP (gGrottoDL) which is NOT resident indoors -> Object_Spawn it,
// then retry Actor_Spawn each frame until it sticks. The spawned actor is registered with
// FleetSync so z_door_ana.c's fall commit redirects to the cross-game flip.
// ---------------------------------------------------------------------------------------------
void FleetHoleSpawnTick() {
    static Actor* sHole = nullptr;
    static s16 sSceneWithHole = -1;
    static u32 sLastFrameCount = 0;

#ifdef COMBO_BUILD
    // ComboShip has no fade/flip pipeline behind this hole: Link would sink into it and void out.
    return;
#endif
    if (FleetShipCombo_GetActiveGame() < 0 || gPlayState == NULL) {
        sHole = nullptr;
        sSceneWithHole = -1;
        return;
    }
    // Scene-load detection (mailbox_actor.c pattern): a fresh PlayState re-inits state.frames to 0,
    // so a frame-counter REWIND means a reload even when sceneNum is unchanged AND even when the
    // new PlayState reuses the same arena address (a pointer compare misses that).
    s32 sceneLoaded = (gPlayState->sceneNum != sSceneWithHole) || (gPlayState->state.frames < sLastFrameCount);
    sLastFrameCount = gPlayState->state.frames;
    if (sceneLoaded) {
        sHole = nullptr;
        sSceneWithHole = -1;
    }
    if (!IsTotHoleScene(gPlayState->sceneNum)) {
        return;
    }
    if (sHole != nullptr) {
        return; // already placed this scene
    }
    if (Object_GetIndex(&gPlayState->objectCtx, OBJECT_GAMEPLAY_FIELD_KEEP) < 0) {
        Object_Spawn(&gPlayState->objectCtx, OBJECT_GAMEPLAY_FIELD_KEEP);
        return; // object not resident this frame yet -> retry next frame (Actor_Spawn would fail)
    }
    // FLEET_HOLE_PARAM marks this Door_Ana as ours (z_door_ana.c makes it a pure visual — never
    // grabs/warps). The FleetWarp proximity trigger runs the cross-game flip.
    sHole = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_DOOR_ANA, kTotHoleX, kTotHoleY, kTotHoleZ, 0, 0, 0,
                        FLEET_HOLE_PARAM);
    if (sHole != nullptr) {
        sHole->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
        sHole->room = -1; // global actor: survives room transitions (hall <-> pedestal chamber)
        sSceneWithHole = gPlayState->sceneNum;
        SPDLOG_INFO("[FleetSync] ToT fleet hole spawned at ({},{},{})", kTotHoleX, kTotHoleY, kTotHoleZ);
    } else {
        SPDLOG_WARN("[FleetSync] ToT fleet hole Actor_Spawn FAILED (retrying)");
    }
}

// ---------------------------------------------------------------------------------------------
// Creación programática del save combo de OoT (botón "Crear saves" del Fleet Shared).
// Sram_InitSave necesita un FileChooseContext REAL, así que la petición queda pendiente hasta
// estar en file select (desde el title se fuerza el paso a file select, como el warp boot).
// Con questType[slot] = QUEST_RANDOMIZER, Sram_InitSave llama Randomizer_InitSaveFile y el save
// nace con la seed combo ya generada. Overwrite incondicional del slot.
// ---------------------------------------------------------------------------------------------
int sPendingCreateSlot = -1;
u8 sPendingCreateName[8];

void FleetCreateSaveTick() {
    if (sPendingCreateSlot < 0 || FleetShipCombo_GetActiveGame() < 0 || gGameState == NULL) {
        return;
    }
    if (gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN) {
        gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
        gGameState->running = false;
        SET_NEXT_GAMESTATE(gGameState, FileChoose_Init, FileChooseContext);
        return;
    }
    if (gSaveContext.gameMode != GAMEMODE_FILE_SELECT || gPlayState != NULL) {
        return; // en gameplay no hay FileChooseContext: espera a que el usuario salga al title
    }

    int slot = sPendingCreateSlot;
    sPendingCreateSlot = -1;

    FileChooseContext* fc = (FileChooseContext*)gGameState;
    gSaveContext.fileNum = (s16)slot;
    memcpy(Save_GetSaveMetaInfo(slot)->playerName, sPendingCreateName, 8);
    fc->buttonIndex = (s16)slot;
    fc->n64ddFlag = 0;
    fc->questType[slot] = QUEST_RANDOMIZER;
    Sram_InitSave(fc);
    SPDLOG_INFO("[FleetCombo] save OoT combo creado en File {} (quest RANDOMIZER, overwrite)", slot + 1);
}

// A throw out of the warp tick would skip the rest of it — including the watchdogs that exist to
// recover a stuck warp — with nothing logged. Swallow + log; next frame tries again.
template <typename Fn> void GuardedTick(const char* what, Fn&& fn) {
    try {
        fn();
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[FleetWatchdog] {} threw: {} — frame skipped, watchdogs still armed", what, e.what());
    } catch (...) { SPDLOG_ERROR("[FleetWatchdog] {} threw a non-std exception — frame skipped", what); }
}

void RegisterFleetWarpBoot() {
#ifdef COMBO_BUILD
    // ComboShip owns arrivals and departures (scene seams + resume); the limbo scene and the
    // two-process warp pipeline must not be installed on top of it.
    return;
#endif
    LimboInstallScene(); // patch the scene table before anything can boot a scene
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(
        []() { GuardedTick("FleetWarpBoot_Tick", FleetWarpBoot_Tick); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(FleetHoleSpawnTick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(FleetCreateSaveTick);
}

} // namespace

// ---- C-callable limbo API (declared in FleetShipCombo.h) ----
extern "C" {

// The send path calls this INSTEAD of RequestWarp: it records the warp we owe MM, walks Link into
// the waiting room, and FleetWarpBoot_Tick flips once he is inside (or after the deadline).
void FleetLimbo_DepartToMm(int scene, float x, float y, float z, int rotY, int saveFile) {
    sLimboWarpScene = scene;
    sLimboWarpX = x;
    sLimboWarpY = y;
    sLimboWarpZ = z;
    sLimboWarpRotY = rotY;
    sLimboWarpSlot = saveFile;
    if (gPlayState == NULL || gSaveContext.gameMode != GAMEMODE_NORMAL) {
        // Nothing to park (title/file select): flip right away. The curtain stays up; MM lowers it
        // once its destination is armed, same as every other hand-over.
        FleetShipCombo_SetSendFadeAlpha(255);
        FleetShipCombo_RequestWarp(1 /*MM*/, scene, x, y, z, rotY, saveFile);
        return;
    }
    // Start walking into the room AND hand over in the same frame: the room finishes loading in
    // the background (sLimboInFlight keeps this game ticking although it is already inactive), while
    // MM gets the player right away. The black curtain (send fade) stays UP across the flip on
    // purpose -- it is the ARRIVING game that lowers it, the moment its destination transition is
    // armed, so the player never sees the waiting room on either side.
    LimboEnterFromGameplay(); // no-op if the room is unavailable -> plain flip in place (old behaviour)
    FleetShipCombo_SetSendFadeAlpha(255);
    FleetSync_SwapTrace("A5. RequestWarp enter (heading into the waiting room; after this MM is the active game)");
    FleetShipCombo_RequestWarp(1 /*MM*/, scene, x, y, z, rotY, saveFile);
    FleetSync_SwapTrace("A6. RequestWarp done -- OoT is parking");
}

// True while this game is walking into the waiting room (already inactive). The frozen-game guard in
// custom_items_common.c must not squash that transition's trigger.
int FleetLimbo_InFlight(void) {
    return sLimboInFlight ? 1 : 0;
}

int FleetShipCombo_IsGameSuspended(void) {
#ifdef COMBO_BUILD
    return 0; // ComboShip parks the dormant game by not running its loop at all; nothing to freeze here
#else
    if (FleetShipCombo_IsThisGameActive()) {
        return 0;
    }
    if (sLimboInFlight) {
        return 0; // still walking into the room: the transition must be allowed to finish
    }
    return LimboInRoom() ? 0 : 1; // parked = keep running; not parked = the old freeze (fallback)
#endif
}

int FleetShipCombo_IsParkedInLimbo(void) {
    return LimboInRoom() ? 1 : 0;
}

// Wrap a save write done while parked so the file records the player's REAL place, never the room.
void FleetShipCombo_LimboSaveShadowBegin(void) {
    if (!LimboInRoom() || !sLimboReturn.valid) {
        return;
    }
    gSaveContext.entranceIndex = sLimboReturn.entranceIndex;
    gSaveContext.cutsceneIndex = sLimboReturn.cutsceneIndex;
    gSaveContext.savedSceneNum = sLimboReturn.savedSceneNum;
}
void FleetShipCombo_LimboSaveShadowEnd(void) {
    if (!LimboInRoom() || !sLimboReturn.valid) {
        return;
    }
    gSaveContext.entranceIndex = kLimboEntrance;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.savedSceneNum = kLimboSceneId;
}

} // extern "C"

// Queue the RESUME hand-off (see FleetShipCombo.h). Called when a combo file is loaded; only arms
// when the last save of this combo was made in MM.
void FleetCombo_QueueResumeToMm(void) {
    if (FleetShipCombo_GetActiveGame() < 0) {
        return; // no combo running
    }
    if (CVarGetInteger("gFleetCombo.LastSavedGame", -1) != 1) {
        return; // last save was in OoT (or there is none yet) — stay here
    }
    sResumeQueued = true;
    sResumeWaitFrames = 0;
    SPDLOG_INFO("[FleetCombo] resume hand-off queued (last save was in MM)");
}

// Encola la creación del save combo de OoT (name ya codificado al charset del file select).
void FleetCombo_RequestCreateSave(int slot, const unsigned char name[8]) {
    if (slot < 0 || slot > 2) {
        return;
    }
    memcpy(sPendingCreateName, name, 8);
    sPendingCreateSlot = slot;
}

static RegisterShipInitFunc initFleetWarpBoot(RegisterFleetWarpBoot, {});
