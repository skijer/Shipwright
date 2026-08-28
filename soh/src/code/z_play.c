#include "global.h"
#include "vt.h"

#include <string.h>

#include "soh/Enhancements/gameconsole.h"
#include "soh/frame_interpolation.h"
#include <overlays/actors/ovl_En_Niw/z_en_niw.h>
#include <overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h>
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/SaveManager.h"
#include "soh/framebuffer_effects.h"

#include <libultraship/libultraship.h>

#include <time.h>
#include <assert.h>

TransitionUnk sTrnsnUnk;
s32 gTrnsnUnkState;
VisMono gPlayVisMono;
Color_RGBA8_u32 gVisMonoColor;

FaultClient D_801614B8;

s16 sTransitionFillTimer;

void* gDebugCutsceneScript = NULL;
UNK_TYPE D_8012D1F4 = 0; // unused

Input* D_8012D1F8 = NULL;

PlayState* gPlayState;
s16 firstInit = 0;
s16 gEnPartnerId;

void Play_SpawnScene(PlayState* play, s32 sceneId, s32 spawn);
void Actor_Draw(PlayState* play, Actor* actor);

#define LOCAL_MP_PLAYER_COUNT_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.PlayerCount")
#define LOCAL_MP_DEBUG_MARKERS_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.DebugMarkers")
#define LOCAL_MP_DISABLED_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.Disable")
#define LOCAL_MP_SPLITSCREEN_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreen")
#define LOCAL_MP_SPLITSCREEN_VERTICAL_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreenVertical")
#define LOCAL_MP_SPLITSCREEN_PERF_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreenPerformanceMode")
#define LOCAL_MP_SPLITSCREEN_AGGRESSIVE_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.SplitScreenAggressiveMode")
#define LOCAL_MP_TELEPORT_TARGET_PORT_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.TeleportTargetPlayer")
#define LOCAL_MP_TELEPORT_REQUEST_CVAR CVAR_ENHANCEMENT("LocalMultiplayer.TeleportAllToSelectedRequest")

typedef struct LocalMPCameraState {
    Vec3f midpoint;
    f32 maxRadius;
    f32 heightSpan;
    f32 forwardX;
    f32 forwardZ;
    s32 playerCount;
    u8 hasForward;
    u8 initialized;
    Vec3f smoothedAt;
    f32 smoothedDist;
    f32 smoothedEyeLift;
    s16 smoothedYaw;
} LocalMPCameraState;

static LocalMPCameraState sLocalMPCameraState;
static s16 sLocalMPSplitCamIds[3] = { SUBCAM_NONE, SUBCAM_NONE, SUBCAM_NONE };
static u16 sMergedStartPrevCur = 0;
static s32 sLocalMPLastTeleportRequest = 0;

static void LocalMP_DrawDebugMarkers(PlayState* play);

static s32 LocalMP_GetDesiredPlayerCount(void) {
    if (CVarGetInteger(LOCAL_MP_DISABLED_CVAR, 0)) {
        return 1;
    }

    s32 desiredPlayers = CVarGetInteger(LOCAL_MP_PLAYER_COUNT_CVAR, 2);

    if (desiredPlayers < 1) {
        desiredPlayers = 1;
    } else if (desiredPlayers > 4) {
        desiredPlayers = 4;
    }

    return desiredPlayers;
}

static Player* LocalMP_FindPlayerByPort(PlayState* play, s32 controllerPort) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;

    while (actor != NULL) {
        if ((actor->id == ACTOR_PLAYER) && (actor->update != NULL)) {
            Player* player = (Player*)actor;

            if (player->controllerPort == controllerPort) {
                return player;
            }
        }

        actor = actor->next;
    }

    return NULL;
}

static void LocalMP_HandleTeleportAllToSelected(PlayState* play) {
    s32 request = CVarGetInteger(LOCAL_MP_TELEPORT_REQUEST_CVAR, 0);
    s32 targetPort;
    Player* targetPlayer;
    Vec3f targetPos;
    s16 targetYaw;
    s32 targetRoom;
    Actor* actor;

    if (request == sLocalMPLastTeleportRequest) {
        return;
    }

    if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->roomCtx.status != 0) || Play_InCsMode(play) ||
        (play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0)) {
        return;
    }

    if (LocalMP_GetDesiredPlayerCount() <= 1) {
        return;
    }

    targetPort = CVarGetInteger(LOCAL_MP_TELEPORT_TARGET_PORT_CVAR, 1);

    if (targetPort < 1) {
        targetPort = 1;
    } else if (targetPort > 4) {
        targetPort = 4;
    }

    targetPlayer = LocalMP_FindPlayerByPort(play, targetPort);
    if ((targetPlayer == NULL) || (targetPlayer->actor.update == NULL)) {
        targetPlayer = LocalMP_FindPlayerByPort(play, 1);
    }

    if ((targetPlayer == NULL) || (targetPlayer->actor.update == NULL)) {
        return;
    }

    targetPos = targetPlayer->actor.world.pos;
    targetYaw = targetPlayer->actor.shape.rot.y;
    targetRoom = targetPlayer->actor.room;

    actor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;
    while (actor != NULL) {
        if ((actor->id == ACTOR_PLAYER) && (actor->update != NULL)) {
            Player* player = (Player*)actor;

            actor->world.pos = targetPos;
            actor->home.pos = targetPos;
            actor->prevPos = targetPos;
            actor->shape.rot.y = targetYaw;
            actor->world.rot.y = targetYaw;
            actor->velocity.x = 0.0f;
            actor->velocity.y = 0.0f;
            actor->velocity.z = 0.0f;
            actor->speedXZ = 0.0f;
            actor->room = targetRoom;
            player->yaw = targetYaw;
            player->linearVelocity = 0.0f;
            player->pushedSpeed = 0.0f;
            player->fallStartHeight = targetPos.y;
        }

        actor = actor->next;
    }

    sLocalMPLastTeleportRequest = request;
}

static void LocalMP_ReorderPlayerList(PlayState* play, Player* mainPlayer) {
    Actor* head;
    Actor* prev;
    Actor* next;

    if (mainPlayer == NULL) {
        return;
    }

    head = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;

    if ((head == NULL) || (head == &mainPlayer->actor)) {
        return;
    }

    prev = mainPlayer->actor.prev;
    next = mainPlayer->actor.next;

    if (prev != NULL) {
        prev->next = next;
    }

    if (next != NULL) {
        next->prev = prev;
    }

    mainPlayer->actor.prev = NULL;
    mainPlayer->actor.next = head;
    head->prev = &mainPlayer->actor;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].head = &mainPlayer->actor;
}

static Player* LocalMP_SpawnPlayer(PlayState* play, Player* mainPlayer, s32 controllerPort) {
    static Vec3f sSpawnOffsets[] = {
        { 40.0f, 0.0f, 0.0f },
        { -40.0f, 0.0f, 40.0f },
        { 0.0f, 0.0f, -40.0f },
    };
    Vec3f spawnOffset;
    Actor* actor;
    Player* player;

    if ((mainPlayer == NULL) || (controllerPort < 2) || (controllerPort > 4)) {
        return NULL;
    }

    spawnOffset = sSpawnOffsets[controllerPort - 2];

    actor = Actor_Spawn(&play->actorCtx, play, ACTOR_PLAYER, mainPlayer->actor.world.pos.x + spawnOffset.x,
                        mainPlayer->actor.world.pos.y + spawnOffset.y, mainPlayer->actor.world.pos.z + spawnOffset.z,
                        mainPlayer->actor.shape.rot.x, mainPlayer->actor.shape.rot.y, mainPlayer->actor.shape.rot.z,
                        mainPlayer->actor.params | 0x8000);

    if (actor == NULL) {
        return NULL;
    }

    player = (Player*)actor;
    player->controllerPort = controllerPort;
    player->inputEnabled = true;
    player->isSecondPlayer = true;
    player->linkedPlayer = mainPlayer;
    Magic_InitForPort(controllerPort);
    player->actor.room = mainPlayer->actor.room;
    player->yaw = mainPlayer->yaw;

    if (controllerPort == 2) {
        player->currentTunic = PLAYER_TUNIC_GORON;
    } else if (controllerPort == 3) {
        player->currentTunic = PLAYER_TUNIC_ZORA;
    } else {
        player->currentTunic = PLAYER_TUNIC_KOKIRI;
    }

    return player;
}

static void LocalMP_EnsurePlayers(PlayState* play) {
    Player* mainPlayer;
    bool seenPorts[5] = { false, false, false, false, false };
    Actor* actor;
    s32 desiredPlayers;
    s32 controllerPort;

    desiredPlayers = LocalMP_GetDesiredPlayerCount();

    // Keep the player list stable while rooms/transitions are in-flight.
    if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->roomCtx.status != 0) || Play_InCsMode(play)) {
        return;
    }

    if (desiredPlayers <= 1) {
        Actor* nextActor;

        actor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;
        while (actor != NULL) {
            nextActor = actor->next;

            if ((actor->id == ACTOR_PLAYER) && (actor != play->actorCtx.actorLists[ACTORCAT_PLAYER].head)) {
                Player* player = (Player*)actor;
                if ((player->controllerPort >= 2) && (player->controllerPort <= 4)) {
                    Actor_Kill(actor);
                }
            }

            actor = nextActor;
        }

        sLocalMPCameraState.playerCount = 1;
        sLocalMPCameraState.initialized = false;
        return;
    }

    mainPlayer = LocalMP_FindPlayerByPort(play, 1);
    if (mainPlayer == NULL) {
        mainPlayer = GET_PLAYER(play);
    }

    if (mainPlayer == NULL) {
        return;
    }

    mainPlayer->controllerPort = 1;
    mainPlayer->inputEnabled = true;
    mainPlayer->isSecondPlayer = false;
    mainPlayer->linkedPlayer = NULL;
    seenPorts[1] = true;

    actor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;
    while (actor != NULL) {
        Actor* nextActor = actor->next;

        if ((actor->id == ACTOR_PLAYER) && (actor != &mainPlayer->actor)) {
            Player* player = (Player*)actor;
            controllerPort = player->controllerPort;

            if ((controllerPort < 2) || (controllerPort > desiredPlayers) || (controllerPort > 4) ||
                seenPorts[controllerPort]) {
                Actor_Kill(actor);
            } else {
                seenPorts[controllerPort] = true;
                player->isSecondPlayer = true;
                player->inputEnabled = true;
                player->linkedPlayer = mainPlayer;
                player->actor.room = mainPlayer->actor.room;
            }
        }

        actor = nextActor;
    }

    for (controllerPort = 2; controllerPort <= desiredPlayers; controllerPort++) {
        if (!seenPorts[controllerPort]) {
            Player* spawned = LocalMP_SpawnPlayer(play, mainPlayer, controllerPort);

            if (spawned != NULL) {
                seenPorts[controllerPort] = true;
            }
        }
    }

    LocalMP_ReorderPlayerList(play, mainPlayer);
}

static s32 LocalMP_UpdateMidpointState(PlayState* play) {
    Vec3f sum = { 0.0f, 0.0f, 0.0f };
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;
    f32 forwardX = 0.0f;
    f32 forwardZ = 0.0f;
    f32 forwardMag;
    f32 minY = 32767.0f;
    f32 maxY = -32767.0f;
    s32 count = 0;

    while (actor != NULL) {
        if ((actor->id == ACTOR_PLAYER) && (actor->update != NULL)) {
            Player* player = (Player*)actor;

            if ((player->controllerPort >= 1) && (player->controllerPort <= 4)) {
                sum.x += actor->world.pos.x;
                sum.y += actor->world.pos.y;
                sum.z += actor->world.pos.z;
                forwardX += Math_SinS(actor->shape.rot.y);
                forwardZ += Math_CosS(actor->shape.rot.y);

                if (actor->world.pos.y < minY) {
                    minY = actor->world.pos.y;
                }
                if (actor->world.pos.y > maxY) {
                    maxY = actor->world.pos.y;
                }

                count++;
            }
        }

        actor = actor->next;
    }

    sLocalMPCameraState.playerCount = count;

    if (count < 2) {
        sLocalMPCameraState.maxRadius = 0.0f;
        sLocalMPCameraState.heightSpan = 0.0f;
        sLocalMPCameraState.hasForward = false;
        return false;
    }

    sLocalMPCameraState.midpoint.x = sum.x / count;
    sLocalMPCameraState.midpoint.y = sum.y / count;
    sLocalMPCameraState.midpoint.z = sum.z / count;
    sLocalMPCameraState.maxRadius = 0.0f;
    sLocalMPCameraState.heightSpan = maxY - minY;
    forwardMag = sqrtf(SQ(forwardX) + SQ(forwardZ));

    if (forwardMag > 0.1f) {
        sLocalMPCameraState.forwardX = forwardX / forwardMag;
        sLocalMPCameraState.forwardZ = forwardZ / forwardMag;
        sLocalMPCameraState.hasForward = true;
    } else {
        sLocalMPCameraState.forwardX = 0.0f;
        sLocalMPCameraState.forwardZ = 0.0f;
        sLocalMPCameraState.hasForward = false;
    }

    actor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;
    while (actor != NULL) {
        if ((actor->id == ACTOR_PLAYER) && (actor->update != NULL)) {
            Player* player = (Player*)actor;

            if ((player->controllerPort >= 1) && (player->controllerPort <= 4)) {
                f32 dx = actor->world.pos.x - sLocalMPCameraState.midpoint.x;
                f32 dz = actor->world.pos.z - sLocalMPCameraState.midpoint.z;
                f32 radius = sqrtf(SQ(dx) + SQ(dz));

                if (radius > sLocalMPCameraState.maxRadius) {
                    sLocalMPCameraState.maxRadius = radius;
                }
            }
        }

        actor = actor->next;
    }

    return true;
}

static bool LocalMP_IsSplitScreenEnabled(void) {
    return !CVarGetInteger(LOCAL_MP_DISABLED_CVAR, 0) && CVarGetInteger(LOCAL_MP_SPLITSCREEN_CVAR, 1);
}

static bool LocalMP_IsVerticalSplitScreenEnabled(void) {
    return LocalMP_IsSplitScreenEnabled() && CVarGetInteger(LOCAL_MP_SPLITSCREEN_VERTICAL_CVAR, 1);
}

static bool LocalMP_IsSplitScreenPerformanceModeEnabled(void) {
    return LocalMP_IsSplitScreenEnabled() && CVarGetInteger(LOCAL_MP_SPLITSCREEN_PERF_CVAR, 1);
}

static bool LocalMP_IsSplitScreenAggressiveModeEnabled(void) {
    return LocalMP_IsSplitScreenPerformanceModeEnabled() && CVarGetInteger(LOCAL_MP_SPLITSCREEN_AGGRESSIVE_CVAR, 0);
}

static s32 LocalMP_CollectPlayersByPort(PlayState* play, Player* players[4]) {
    Player* portPlayers[5] = { NULL, NULL, NULL, NULL, NULL };
    Actor* actor;
    s32 port;
    s32 count = 0;

    memset(players, 0, sizeof(Player*) * 4);

    actor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;
    while (actor != NULL) {
        if ((actor->id == ACTOR_PLAYER) && (actor->update != NULL)) {
            Player* player = (Player*)actor;
            port = player->controllerPort;

            if ((port >= 1) && (port <= 4) && (portPlayers[port] == NULL)) {
                portPlayers[port] = player;
            }
        }

        actor = actor->next;
    }

    if ((portPlayers[1] == NULL) && (GET_PLAYER(play) != NULL) && (GET_PLAYER(play)->actor.update != NULL)) {
        portPlayers[1] = GET_PLAYER(play);
    }

    for (port = 1; port <= 4; port++) {
        if (portPlayers[port] != NULL) {
            players[count++] = portPlayers[port];
        }
    }

    return count;
}

static void LocalMP_ClearSplitScreenCameras(PlayState* play) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sLocalMPSplitCamIds); i++) {
        s16 camId = sLocalMPSplitCamIds[i];

        if ((camId >= SUBCAM_FIRST) && (camId < NUM_CAMS) && (play->cameraPtrs[camId] != NULL)) {
            Play_ClearCamera(play, camId);
        }

        sLocalMPSplitCamIds[i] = SUBCAM_NONE;
    }
}

// Returns true for camera settings driven by P1's item/animation state that should not be forced
// onto split-screen cameras (those cameras track their own players independently).
static bool LocalMP_IsCameraSettingPlayerAnimation(s16 setting) {
    return (setting == CAM_SET_TURN_AROUND) ||  // bottle use, first-person item animations
           (setting == CAM_SET_SLOW_CHEST_CS);  // big-chest opening cutscene
}

static void LocalMP_SyncSplitCamera(Camera* splitCam, Camera* mainCam, Player* player) {
    if ((splitCam == NULL) || (mainCam == NULL) || (player == NULL)) {
        return;
    }

    if (splitCam->player != player) {
        Camera_InitPlayerSettings(splitCam, player);
    }

    if ((mainCam->camDataIdx >= 0) && (splitCam->camDataIdx != mainCam->camDataIdx)) {
        Camera_ChangeDataIdx(splitCam, mainCam->camDataIdx);
    } else if ((splitCam->setting != mainCam->setting) &&
               !LocalMP_IsCameraSettingPlayerAnimation(mainCam->setting)) {
        // Don't propagate player-item-animation camera modes; each player's camera should stay
        // in its own movement mode while another player is using an item.
        Camera_ChangeSetting(splitCam, mainCam->setting);
    }
}

static void LocalMP_GetSplitViewport(s32 playerCount, s32 playerIndex, Viewport* viewport) {
    s32 halfWidth = SCREEN_WIDTH / 2;
    s32 halfHeight = SCREEN_HEIGHT / 2;

    if (playerCount <= 1) {
        viewport->topY = 0;
        viewport->bottomY = SCREEN_HEIGHT;
        viewport->leftX = 0;
        viewport->rightX = SCREEN_WIDTH;
        return;
    }

    if (playerCount == 2) {
        if (LocalMP_IsVerticalSplitScreenEnabled()) {
            viewport->topY = 0;
            viewport->bottomY = SCREEN_HEIGHT;
            viewport->leftX = (playerIndex == 0) ? 0 : halfWidth;
            viewport->rightX = (playerIndex == 0) ? halfWidth : SCREEN_WIDTH;
        } else {
            viewport->topY = (playerIndex == 0) ? 0 : halfHeight;
            viewport->bottomY = (playerIndex == 0) ? halfHeight : SCREEN_HEIGHT;
            viewport->leftX = 0;
            viewport->rightX = SCREEN_WIDTH;
        }
        return;
    }

    if (playerCount == 3) {
        if (playerIndex == 0) {
            viewport->topY = 0;
            viewport->bottomY = halfHeight;
            viewport->leftX = 0;
            viewport->rightX = SCREEN_WIDTH;
        } else if (playerIndex == 1) {
            viewport->topY = halfHeight;
            viewport->bottomY = SCREEN_HEIGHT;
            viewport->leftX = 0;
            viewport->rightX = halfWidth;
        } else {
            viewport->topY = halfHeight;
            viewport->bottomY = SCREEN_HEIGHT;
            viewport->leftX = halfWidth;
            viewport->rightX = SCREEN_WIDTH;
        }
        return;
    }

    viewport->topY = (playerIndex < 2) ? 0 : halfHeight;
    viewport->bottomY = (playerIndex < 2) ? halfHeight : SCREEN_HEIGHT;
    viewport->leftX = ((playerIndex % 2) == 0) ? 0 : halfWidth;
    viewport->rightX = ((playerIndex % 2) == 0) ? halfWidth : SCREEN_WIDTH;
}

static bool LocalMP_IsSplitScreenRenderAllowed(PlayState* play) {
    if (!LocalMP_IsSplitScreenEnabled()) {
        return false;
    }

    if ((play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0)) {
        return false;
    }

    if (Play_InCsMode(play)) {
        return false;
    }

    if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF)) {
        return false;
    }

    if ((R_PAUSE_MENU_MODE != 0) || (gTrnsnUnkState != 0)) {
        return false;
    }

    if (gDbgCamEnabled) {
        return false;
    }

    return true;
}

static void LocalMP_EnsureSplitScreenCameras(PlayState* play) {
    Player* players[4];
    Camera* mainCam;
    s32 playerCount;
    s32 i;

    if (!LocalMP_IsSplitScreenRenderAllowed(play)) {
        LocalMP_ClearSplitScreenCameras(play);
        return;
    }

    playerCount = LocalMP_CollectPlayersByPort(play, players);
    if (playerCount <= 1) {
        LocalMP_ClearSplitScreenCameras(play);
        return;
    }

    if (playerCount > 4) {
        playerCount = 4;
    }

    mainCam = play->cameraPtrs[MAIN_CAM];
    if (mainCam == NULL) {
        LocalMP_ClearSplitScreenCameras(play);
        return;
    }

    for (i = 1; i < playerCount; i++) {
        s32 slot = i - 1;
        s16 camId = sLocalMPSplitCamIds[slot];
        Camera* splitCam;
        bool createdCamera = false;

        if ((camId < SUBCAM_FIRST) || (camId >= NUM_CAMS) || (play->cameraPtrs[camId] == NULL)) {
            camId = Play_CreateSubCamera(play);
            sLocalMPSplitCamIds[slot] = camId;
            createdCamera = true;
        }

        if ((camId < SUBCAM_FIRST) || (camId >= NUM_CAMS) || (play->cameraPtrs[camId] == NULL)) {
            sLocalMPSplitCamIds[slot] = SUBCAM_NONE;
            continue;
        }

        if (createdCamera) {
            Play_CopyCamera(play, camId, MAIN_CAM);
        }

        splitCam = play->cameraPtrs[camId];

        LocalMP_SyncSplitCamera(splitCam, mainCam, players[i]);

        Camera_ChangeStatus(splitCam, CAM_STAT_ACTIVE);
    }

    for (i = playerCount - 1; i < ARRAY_COUNT(sLocalMPSplitCamIds); i++) {
        s16 camId = sLocalMPSplitCamIds[i];

        if ((camId >= SUBCAM_FIRST) && (camId < NUM_CAMS) && (play->cameraPtrs[camId] != NULL)) {
            Play_ClearCamera(play, camId);
        }

        sLocalMPSplitCamIds[i] = SUBCAM_NONE;
    }
}

static void LocalMP_SetViewForCamera(PlayState* play, Camera* camera, Viewport* viewport) {
    if ((play->sceneNum == SCENE_HYRULE_FIELD) && (camera->fov < 59.0f)) {
        View_SetScale(&play->view, 0.79f);
    } else {
        View_SetScale(&play->view, 1.0f);
    }

    play->view.fovy = camera->fov;
    func_800AA358(&play->view, &camera->eye, &camera->at, &camera->up);
    View_SetViewport(&play->view, viewport);
    func_800AA460(&play->view, play->view.fovy, play->view.zNear, play->lightCtx.fogFar);
    func_800AAA50(&play->view, 15);
    // Keep OVERLAY-space world effects (hookshot reticle, lock-on indicators) in the same split viewport.
    func_800AB560(&play->view);
}

static f32 LocalMP_DotVec3f(const Vec3f* a, const Vec3f* b) {
    return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

static void LocalMP_WorldToAudioSpace(const Camera* camera, const Vec3f* worldPos, Vec3f* outAudioPos) {
    Vec3f forward;
    Vec3f right;
    Vec3f up;
    Vec3f delta;
    f32 mag;

    if ((camera == NULL) || (worldPos == NULL) || (outAudioPos == NULL)) {
        if (outAudioPos != NULL) {
            outAudioPos->x = 0.0f;
            outAudioPos->y = 0.0f;
            outAudioPos->z = 0.0f;
        }
        return;
    }

    forward.x = camera->at.x - camera->eye.x;
    forward.y = camera->at.y - camera->eye.y;
    forward.z = camera->at.z - camera->eye.z;

    mag = sqrtf(SQ(forward.x) + SQ(forward.y) + SQ(forward.z));
    if (mag < 0.001f) {
        forward.x = 0.0f;
        forward.y = 0.0f;
        forward.z = 1.0f;
        mag = 1.0f;
    }

    forward.x /= mag;
    forward.y /= mag;
    forward.z /= mag;

    right.x = (forward.y * camera->up.z) - (forward.z * camera->up.y);
    right.y = (forward.z * camera->up.x) - (forward.x * camera->up.z);
    right.z = (forward.x * camera->up.y) - (forward.y * camera->up.x);

    mag = sqrtf(SQ(right.x) + SQ(right.y) + SQ(right.z));
    if (mag < 0.001f) {
        Vec3f fallbackUp = { 0.0f, 1.0f, 0.0f };

        if (fabsf(forward.y) > 0.99f) {
            fallbackUp.x = 1.0f;
            fallbackUp.y = 0.0f;
            fallbackUp.z = 0.0f;
        }

        right.x = (forward.y * fallbackUp.z) - (forward.z * fallbackUp.y);
        right.y = (forward.z * fallbackUp.x) - (forward.x * fallbackUp.z);
        right.z = (forward.x * fallbackUp.y) - (forward.y * fallbackUp.x);
        mag = sqrtf(SQ(right.x) + SQ(right.y) + SQ(right.z));
    }

    if (mag < 0.001f) {
        right.x = 1.0f;
        right.y = 0.0f;
        right.z = 0.0f;
        mag = 1.0f;
    }

    right.x /= mag;
    right.y /= mag;
    right.z /= mag;

    up.x = (right.y * forward.z) - (right.z * forward.y);
    up.y = (right.z * forward.x) - (right.x * forward.z);
    up.z = (right.x * forward.y) - (right.y * forward.x);

    mag = sqrtf(SQ(up.x) + SQ(up.y) + SQ(up.z));
    if (mag < 0.001f) {
        up.x = camera->up.x;
        up.y = camera->up.y;
        up.z = camera->up.z;
        mag = sqrtf(SQ(up.x) + SQ(up.y) + SQ(up.z));
    }

    if (mag < 0.001f) {
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        mag = 1.0f;
    }

    up.x /= mag;
    up.y /= mag;
    up.z /= mag;

    delta.x = worldPos->x - camera->eye.x;
    delta.y = worldPos->y - camera->eye.y;
    delta.z = worldPos->z - camera->eye.z;

    outAudioPos->x = LocalMP_DotVec3f(&delta, &right);
    outAudioPos->y = LocalMP_DotVec3f(&delta, &up);
    outAudioPos->z = LocalMP_DotVec3f(&delta, &forward);
}

static s32 LocalMP_GetClosestPlayerSlotForPos(Player* players[4], s32 playerCount, const Vec3f* worldPos) {
    s32 bestSlot = 0;
    f32 bestDistSq = FLT_MAX;
    s32 i;

    if ((players == NULL) || (worldPos == NULL) || (playerCount <= 0)) {
        return 0;
    }

    if (playerCount > 4) {
        playerCount = 4;
    }

    for (i = 0; i < playerCount; i++) {
        if (players[i] != NULL) {
            f32 dx = worldPos->x - players[i]->actor.world.pos.x;
            f32 dy = worldPos->y - players[i]->actor.world.pos.y;
            f32 dz = worldPos->z - players[i]->actor.world.pos.z;
            f32 distSq = SQ(dx) + SQ(dy) + SQ(dz);

            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestSlot = i;
            }
        }
    }

    return bestSlot;
}

static void LocalMP_ProcessActorAudio(PlayState* play, Player* players[4], Camera* cameras[4], s32 playerCount) {
    ActorListEntry* actorListEntry = &play->actorCtx.actorLists[0];
    s32 i;

    if ((play == NULL) || (players == NULL) || (cameras == NULL) || (playerCount <= 0)) {
        return;
    }

    if (playerCount > 4) {
        playerCount = 4;
    }

    for (i = 0; i < ARRAY_COUNT(play->actorCtx.actorLists); i++, actorListEntry++) {
        Actor* actor = actorListEntry->head;
        s32 sanity = 0;

        while ((actor != NULL) && (sanity < 2000)) {
            if ((actor->update != NULL) && (actor->sfx != 0)) {
                s32 playerSlot = LocalMP_GetClosestPlayerSlotForPos(players, playerCount, &actor->world.pos);
                Camera* listenerCamera = cameras[playerSlot];

                if (listenerCamera == NULL) {
                    listenerCamera = cameras[0];
                }

                LocalMP_WorldToAudioSpace(listenerCamera, &actor->world.pos, &actor->projectedPos);
                Actor_PlaySfx(actor);
            }

            actor = actor->next;
            sanity++;
        }
    }
}

static void LocalMP_DrawPlayerActorsOnly(PlayState* play) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;

    while (actor != NULL) {
        Actor* next = actor->next;

        if ((actor->update != NULL) && (actor->draw != NULL)) {
            Actor_Draw(play, actor);
        }

        actor = next;
    }
}

static bool LocalMP_DrawSplitScreen(PlayState* play) {
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Player* players[4];
    Camera* cameras[4];
    View mainView = play->view;
    MtxF mainViewProjection = play->viewProjectionMtxF;
    MtxF mainBillboardMtxF = play->billboardMtxF;
    Mtx* mainBillboardMtx = play->billboardMtx;
    Lights* lights;
    Vec3f sunPos;
    s32 playerCount;
    s32 i;
    bool performanceMode = LocalMP_IsSplitScreenPerformanceModeEnabled();
    bool aggressiveMode = LocalMP_IsSplitScreenAggressiveModeEnabled();

    if (!LocalMP_IsSplitScreenRenderAllowed(play)) {
        return false;
    }

    playerCount = LocalMP_CollectPlayersByPort(play, players);
    if (playerCount <= 1) {
        return false;
    }

    if (playerCount > 4) {
        playerCount = 4;
    }

    cameras[0] = play->cameraPtrs[MAIN_CAM];
    if (cameras[0] == NULL) {
        return false;
    }

    for (i = 1; i < playerCount; i++) {
        s16 camId = sLocalMPSplitCamIds[i - 1];

        if ((camId < SUBCAM_FIRST) || (camId >= NUM_CAMS) || (play->cameraPtrs[camId] == NULL)) {
            return false;
        }

        cameras[i] = play->cameraPtrs[camId];
        LocalMP_SyncSplitCamera(cameras[i], cameras[0], players[i]);
    }

    if (!aggressiveMode && ((HREG(80) != 10) || (HREG(83) != 0))) {
        if (play->skyboxId && (play->skyboxId != SKYBOX_UNSET_1D) && !play->envCtx.skyboxDisabled) {
            if ((play->skyboxId == SKYBOX_NORMAL_SKY) || (play->skyboxId == SKYBOX_CUTSCENE_MAP)) {
                Environment_UpdateSkybox(play, play->skyboxId, &play->envCtx, &play->skyboxCtx);
                SkyboxDraw_Draw(&play->skyboxCtx, play->state.gfxCtx, play->skyboxId, play->envCtx.skyboxBlend,
                                play->view.eye.x, play->view.eye.y, play->view.eye.z);
            } else if (play->skyboxCtx.unk_140 == 0) {
                SkyboxDraw_Draw(&play->skyboxCtx, play->state.gfxCtx, play->skyboxId, 0, play->view.eye.x,
                                play->view.eye.y, play->view.eye.z);
            }
        }
    }

    if (!aggressiveMode && ((HREG(80) != 10) || (HREG(90) & 2))) {
        if (!play->envCtx.sunMoonDisabled) {
            Environment_DrawSunAndMoon(play);
        }
    }

    if (!aggressiveMode && ((HREG(80) != 10) || (HREG(90) & 1))) {
        Environment_DrawSkyboxFilters(play);
    }

    if (!aggressiveMode && ((HREG(80) != 10) || (HREG(90) & 4))) {
        Environment_UpdateLightningStrike(play);
        Environment_DrawLightning(play, 0);
    }

    if (!aggressiveMode && ((HREG(80) != 10) || (HREG(90) & 8))) {
        lights = LightContext_NewLights(&play->lightCtx, play->state.gfxCtx);
        Lights_BindAll(lights, play->lightCtx.listHead, NULL);
        Lights_Draw(lights, play->state.gfxCtx);
    }

    Actor_SetSfxUpdatesSuppressed(true);

    for (i = 0; i < playerCount; i++) {
        Viewport viewport;
        Camera* camera = cameras[i];
        bool skipSecondaryViewportEffects = performanceMode && (i > 0);
        bool skipViewportEffects = skipSecondaryViewportEffects || aggressiveMode;
        bool drawPlayersOnly = aggressiveMode && (i > 0);

        // Keep frame interpolation history isolated per viewport so higher-FPS interpolation
        // does not cross-match matrices between split-screen players.
        FrameInterpolation_RecordOpenChild("LocalMPSplitViewport", i);

        LocalMP_GetSplitViewport(playerCount, i, &viewport);
        LocalMP_SetViewForCamera(play, camera, &viewport);

        if ((R_PAUSE_MENU_MODE <= 1) && CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0)) {
            gSPSetExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
            gSPSetExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
            gSPMatrix(POLY_OPA_DISP++, play->view.projectionFlippedPtr, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
            gSPMatrix(POLY_XLU_DISP++, play->view.projectionFlippedPtr, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
            gSPMatrix(POLY_OPA_DISP++, play->view.viewingPtr, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
            gSPMatrix(POLY_XLU_DISP++, play->view.viewingPtr, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
        }

        Matrix_MtxToMtxF(&play->view.viewing, &play->billboardMtxF);
        Matrix_MtxToMtxF(&play->view.projection, &play->viewProjectionMtxF);
        Matrix_Mult(&play->viewProjectionMtxF, MTXMODE_NEW);
        Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
        Matrix_Get(&play->viewProjectionMtxF);
        play->billboardMtxF.mf[0][3] = play->billboardMtxF.mf[1][3] = play->billboardMtxF.mf[2][3] =
            play->billboardMtxF.mf[3][0] = play->billboardMtxF.mf[3][1] = play->billboardMtxF.mf[3][2] = 0.0f;
        Matrix_Transpose(&play->billboardMtxF);
        play->billboardMtx =
            Matrix_MtxFToMtx(MATRIX_CHECKFLOATS(&play->billboardMtxF), Graph_Alloc(play->state.gfxCtx, sizeof(Mtx)));

        gSPSegment(POLY_OPA_DISP++, 0x01, play->billboardMtx);

        if (i == 0) {
            mainView = play->view;
            mainViewProjection = play->viewProjectionMtxF;
            mainBillboardMtxF = play->billboardMtxF;
            mainBillboardMtx = play->billboardMtx;
        }

        if ((HREG(80) != 10) || (HREG(84) != 0)) {
            if (VREG(94) == 0) {
                s32 roomDrawFlags;

                if (HREG(80) != 10) {
                    roomDrawFlags = 3;
                } else {
                    roomDrawFlags = HREG(84);
                }

                if (aggressiveMode) {
                    roomDrawFlags &= 1;
                }

                Scene_Draw(play);
                if (OTRRoom_AreaPersistenceEnabled()) {
                    s32 retainedRoomCount = OTRRoom_GetRetainedRoomCount();

                    if (retainedRoomCount > 0) {
                        s32 retainedRoomIndex;

                        for (retainedRoomIndex = 0; retainedRoomIndex < retainedRoomCount; retainedRoomIndex++) {
                            Room* retainedRoom = OTRRoom_GetRetainedRoom(retainedRoomIndex);

                            if ((retainedRoom != NULL) && (retainedRoom->num >= 0)) {
                                Room_Draw(play, retainedRoom, roomDrawFlags & 3);
                            }
                        }
                    } else {
                        Room_Draw(play, &play->roomCtx.curRoom, roomDrawFlags & 3);
                        if (!skipViewportEffects) {
                            Room_Draw(play, &play->roomCtx.prevRoom, roomDrawFlags & 3);
                        }
                    }
                } else {
                    Room_Draw(play, &play->roomCtx.curRoom, roomDrawFlags & 3);
                    if (!skipViewportEffects) {
                        Room_Draw(play, &play->roomCtx.prevRoom, roomDrawFlags & 3);
                    }
                }
            }
        }

        if ((HREG(80) != 10) || (HREG(83) != 0)) {
            if (!skipViewportEffects && (play->skyboxCtx.unk_140 != 0) &&
                (camera->setting != CAM_SET_PREREND_FIXED)) {
                Vec3f quakeOffset;

                Camera_GetSkyboxOffset(&quakeOffset, camera);
                SkyboxDraw_Draw(&play->skyboxCtx, play->state.gfxCtx, play->skyboxId, 0, play->view.eye.x + quakeOffset.x,
                                play->view.eye.y + quakeOffset.y, play->view.eye.z + quakeOffset.z);
            }
        }

        if (!skipViewportEffects && (play->envCtx.unk_EE[1] != 0)) {
            Environment_DrawRain(play, &play->view, play->state.gfxCtx);
        }

        if ((HREG(80) != 10) || (HREG(84) != 0)) {
            Environment_FillScreen(play->state.gfxCtx, 0, 0, 0, play->unk_11E18, FILL_SCREEN_OPA);
        }

        if ((HREG(80) != 10) || (HREG(85) != 0)) {
            if (drawPlayersOnly) {
                LocalMP_DrawPlayerActorsOnly(play);
            } else {
                func_800315AC(play, &play->actorCtx);
            }
        }

        if (!skipViewportEffects && ((HREG(80) != 10) || (HREG(86) != 0))) {
            if (!play->envCtx.sunMoonDisabled) {
                sunPos.x = play->view.eye.x + play->envCtx.sunPos.x;
                sunPos.y = play->view.eye.y + play->envCtx.sunPos.y;
                sunPos.z = play->view.eye.z + play->envCtx.sunPos.z;
                Environment_DrawSunLensFlare(play, &play->envCtx, &play->view, play->state.gfxCtx, sunPos, 0);
            }
            Environment_DrawCustomLensFlare(play);
        }

        FrameInterpolation_RecordCloseChild();
    }

    Actor_SetSfxUpdatesSuppressed(false);
    LocalMP_ProcessActorAudio(play, players, cameras, playerCount);

    play->view = mainView;
    play->viewProjectionMtxF = mainViewProjection;
    play->billboardMtxF = mainBillboardMtxF;
    play->billboardMtx = mainBillboardMtx;
    play->view.unk_124 = 0;

    SET_FULLSCREEN_VIEWPORT(&play->view);
    func_800AA460(&play->view, play->view.fovy, play->view.zNear, play->lightCtx.fogFar);
    func_800AAA50(&play->view, 15);
    func_800AB560(&play->view);

    if ((HREG(80) != 10) || (HREG(87) != 0)) {
        if (MREG(64) != 0) {
            Environment_FillScreen(play->state.gfxCtx, MREG(65), MREG(66), MREG(67), MREG(68),
                                   FILL_SCREEN_OPA | FILL_SCREEN_XLU);
        }

        switch (play->envCtx.fillScreen) {
            case 1:
                Environment_FillScreen(play->state.gfxCtx, play->envCtx.screenFillColor[0], play->envCtx.screenFillColor[1],
                                       play->envCtx.screenFillColor[2], play->envCtx.screenFillColor[3],
                                       FILL_SCREEN_OPA | FILL_SCREEN_XLU);
                break;
            default:
                break;
        }
    }

    if ((HREG(80) != 10) || (HREG(88) != 0)) {
        if (play->envCtx.sandstormState != SANDSTORM_OFF) {
            Environment_DrawSandstorm(play, play->envCtx.sandstormState);
        }
    }

    if (!aggressiveMode) {
        LocalMP_DrawDebugMarkers(play);
    }

    if ((HREG(80) != 10) || (HREG(93) != 0)) {
        DebugDisplay_DrawObjects(play);
    }

    play->viewProjectionMtxF = mainViewProjection;
    play->billboardMtxF = mainBillboardMtxF;
    play->billboardMtx = mainBillboardMtx;

    return true;
}

static void LocalMP_FinishSplitScreenCameras(PlayState* play) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sLocalMPSplitCamIds); i++) {
        s16 camId = sLocalMPSplitCamIds[i];

        if ((camId >= SUBCAM_FIRST) && (camId < NUM_CAMS) && (play->cameraPtrs[camId] != NULL)) {
            Camera_Finish(play->cameraPtrs[camId]);
        }
    }
}

static void LocalMP_UpdateSharedCamera(PlayState* play) {
    static s16 sSharedCamLastScene = -1;
    Camera* camera;
    Player* mainPlayer;
    f32 forwardLookAhead;
    f32 eyeLift;
    f32 yawStepScale;
    Vec3f at;
    Vec3f eye;
    f32 cameraDist;
    f32 atHeight;
    s16 yaw;

    if (sSharedCamLastScene != play->sceneNum) {
        sSharedCamLastScene = play->sceneNum;
        sLocalMPCameraState.initialized = false;
    }

    if (LocalMP_IsSplitScreenEnabled()) {
        sLocalMPCameraState.initialized = false;
        return;
    }

    if (LocalMP_GetDesiredPlayerCount() <= 1) {
        sLocalMPCameraState.initialized = false;
        return;
    }

    if ((play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0) || Play_InCsMode(play)) {
        return;
    }

    if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF) ||
        (play->roomCtx.status != 0)) {
        sLocalMPCameraState.initialized = false;
        return;
    }

    camera = play->cameraPtrs[MAIN_CAM];
    if ((camera == NULL) || (play->activeCamera != MAIN_CAM) || (camera->setting == CAM_SET_PREREND_FIXED)) {
        sLocalMPCameraState.initialized = false;
        return;
    }

    // Keep mode-specific camera behavior (like hookshot) from being overridden.
    if (camera->mode == CAM_MODE_HOOKSHOT) {
        sLocalMPCameraState.initialized = false;
        return;
    }

    if (!LocalMP_UpdateMidpointState(play)) {
        sLocalMPCameraState.initialized = false;
        return;
    }

    mainPlayer = LocalMP_FindPlayerByPort(play, 1);
    if (mainPlayer == NULL) {
        mainPlayer = GET_PLAYER(play);
    }

    cameraDist = 180.0f + (sLocalMPCameraState.maxRadius * 1.8f);
    cameraDist = CLAMP(cameraDist, 190.0f, 560.0f);

    atHeight = 26.0f + (sLocalMPCameraState.heightSpan * 0.35f);
    eyeLift = 84.0f + (sLocalMPCameraState.heightSpan * 0.2f) + (sLocalMPCameraState.maxRadius * 0.25f);
    eyeLift = CLAMP(eyeLift, 84.0f, 190.0f);

    if (sLocalMPCameraState.hasForward) {
        yaw = Math_Atan2S(sLocalMPCameraState.forwardX, sLocalMPCameraState.forwardZ);
    } else if (mainPlayer != NULL) {
        yaw = mainPlayer->actor.shape.rot.y;
    } else {
        yaw = camera->inputDir.y;
    }

    forwardLookAhead = sLocalMPCameraState.hasForward ? CLAMP(sLocalMPCameraState.maxRadius * 0.25f, 8.0f, 40.0f)
                                                      : 0.0f;

    at.x = sLocalMPCameraState.midpoint.x + (sLocalMPCameraState.forwardX * forwardLookAhead);
    at.y = sLocalMPCameraState.midpoint.y + atHeight;
    at.z = sLocalMPCameraState.midpoint.z + (sLocalMPCameraState.forwardZ * forwardLookAhead);

    if (!sLocalMPCameraState.initialized) {
        sLocalMPCameraState.smoothedAt = at;
        sLocalMPCameraState.smoothedDist = cameraDist;
        sLocalMPCameraState.smoothedEyeLift = eyeLift;
        sLocalMPCameraState.smoothedYaw = yaw;
        sLocalMPCameraState.initialized = true;

        // Scene loads/mode handoffs can produce large one-frame jumps.
        FrameInterpolation_DontInterpolateCamera();
    } else {
        Math_SmoothStepToF(&sLocalMPCameraState.smoothedAt.x, at.x, 0.18f, 24.0f, 0.1f);
        Math_SmoothStepToF(&sLocalMPCameraState.smoothedAt.y, at.y, 0.2f, 12.0f, 0.1f);
        Math_SmoothStepToF(&sLocalMPCameraState.smoothedAt.z, at.z, 0.18f, 24.0f, 0.1f);
        Math_SmoothStepToF(&sLocalMPCameraState.smoothedDist, cameraDist, 0.12f, 14.0f, 0.1f);
        Math_SmoothStepToF(&sLocalMPCameraState.smoothedEyeLift, eyeLift, 0.12f, 8.0f, 0.1f);

        yawStepScale = CLAMP(220.0f + (sLocalMPCameraState.maxRadius * 3.0f), 220.0f, 900.0f);
        Math_ScaledStepToS(&sLocalMPCameraState.smoothedYaw, yaw, yawStepScale);
    }

    at = sLocalMPCameraState.smoothedAt;

    eye.x = at.x - Math_SinS(sLocalMPCameraState.smoothedYaw) * sLocalMPCameraState.smoothedDist;
    eye.y = at.y + sLocalMPCameraState.smoothedEyeLift;
    eye.z = at.z - Math_CosS(sLocalMPCameraState.smoothedYaw) * sLocalMPCameraState.smoothedDist;

    Play_CameraSetAtEye(play, MAIN_CAM, &at, &eye);
}

static void LocalMP_DrawDebugMarkers(PlayState* play) {
    static Color_RGBA8 sPortColors[] = {
        { 255, 255, 255, 200 },
        { 255, 96, 96, 200 },
        { 96, 128, 255, 200 },
        { 255, 220, 64, 200 },
    };
    Actor* actor;

    if (!CVarGetInteger(LOCAL_MP_DEBUG_MARKERS_CVAR, 0)) {
        return;
    }

    if (LocalMP_GetDesiredPlayerCount() <= 1) {
        return;
    }

    if (!LocalMP_UpdateMidpointState(play)) {
        return;
    }

    DebugDisplay_AddObject(sLocalMPCameraState.midpoint.x, sLocalMPCameraState.midpoint.y + 10.0f,
                           sLocalMPCameraState.midpoint.z, 0, 0, 0, 8.0f, 8.0f, 8.0f, 255, 80, 80, 210, 2,
                           play->state.gfxCtx);

    actor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;
    while (actor != NULL) {
        if ((actor->id == ACTOR_PLAYER) && (actor->update != NULL)) {
            Player* player = (Player*)actor;

            if ((player->controllerPort >= 1) && (player->controllerPort <= 4)) {
                Color_RGBA8 color = sPortColors[player->controllerPort - 1];

                DebugDisplay_AddObject(actor->world.pos.x, actor->world.pos.y + 70.0f, actor->world.pos.z, 0, 0, 0,
                                       3.0f, 3.0f, 3.0f, color.r, color.g, color.b, color.a, 0, play->state.gfxCtx);
            }
        }

        actor = actor->next;
    }
}

// This macro prints the number "1" with a file and line number if R_ENABLE_PLAY_LOGS is enabled.
// For example, it can be used to trace the play state execution at a high level.
// SOHTODO: Revert log statements everywhere back to authentic, and deal with dynamic line/file names via macro
#define PLAY_LOG(line)                                  \
    do {                                                \
        if (1 & HREG(63)) {                             \
            LOG_NUM("1", 1 /*, "../z_play.c", line */); \
        }                                               \
    } while (0)

void enableBetaQuest();
void disableBetaQuest();

void OTRPlay_SpawnScene(PlayState* play, s32 sceneId, s32 spawn);

void Play_RequestViewpointBgCam(PlayState* play) {
    Camera_ChangeDataIdx(GET_ACTIVE_CAM(play), play->unk_1242B - 1);
}

void Play_SetViewpoint(PlayState* play, s16 viewpoint) {
    assert(viewpoint == 1 || viewpoint == 2);

    play->unk_1242B = viewpoint;

    if ((YREG(15) != 0x10) && (gSaveContext.cutsceneIndex < 0xFFF0)) {
        Audio_PlaySoundGeneral((viewpoint == 1) ? NA_SE_SY_CAMERA_ZOOM_DOWN : NA_SE_SY_CAMERA_ZOOM_UP, &gSfxDefaultPos,
                               4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }

    Play_RequestViewpointBgCam(play);
}

/**
 * @return true if the currently set viewpoint is the same as the one provided in the argument
 */
s32 Play_CheckViewpoint(PlayState* play, s16 viewpoint) {
    return (viewpoint == play->unk_1242B);
}

/**
 * If the scene is a shop, set the viewpoint that will set the bgCamIndex
 * to toggle the camera into a "browsing item selection" setting.
 */
void Play_SetShopBrowsingViewpoint(PlayState* play) {
    osSyncPrintf("Game_play_shop_pr_vr_switch_set()\n");

    if (YREG(15) == 0x10) {
        play->unk_1242B = 2;
    }
}

void Gameplay_SetupTransition(PlayState* play, s32 transitionType) {
    TransitionContext* transitionCtx = &play->transitionCtx;

    memset(transitionCtx, 0, sizeof(TransitionContext));

    transitionCtx->transitionType = transitionType;

    // Circle Transition Types
    if ((transitionCtx->transitionType >> 5) == 1) {
        transitionCtx->init = TransitionCircle_Init;
        transitionCtx->destroy = TransitionCircle_Destroy;
        transitionCtx->start = TransitionCircle_Start;
        transitionCtx->isDone = TransitionCircle_IsDone;
        transitionCtx->draw = TransitionCircle_Draw;
        transitionCtx->update = TransitionCircle_Update;
        transitionCtx->setType = TransitionCircle_SetType;
        transitionCtx->setColor = TransitionCircle_SetColor;
        transitionCtx->setEnvColor = TransitionCircle_SetEnvColor;
    } else {
        switch (transitionCtx->transitionType) {
            case TRANS_TYPE_TRIFORCE:
                transitionCtx->init = TransitionTriforce_Init;
                transitionCtx->destroy = TransitionTriforce_Destroy;
                transitionCtx->start = TransitionTriforce_Start;
                transitionCtx->isDone = TransitionTriforce_IsDone;
                transitionCtx->draw = TransitionTriforce_Draw;
                transitionCtx->update = TransitionTriforce_Update;
                transitionCtx->setType = TransitionTriforce_SetType;
                transitionCtx->setColor = TransitionTriforce_SetColor;
                transitionCtx->setEnvColor = NULL;
                break;

            case TRANS_TYPE_WIPE:
            case TRANS_TYPE_WIPE_FAST:
                transitionCtx->init = TransitionWipe_Init;
                transitionCtx->destroy = TransitionWipe_Destroy;
                transitionCtx->start = TransitionWipe_Start;
                transitionCtx->isDone = TransitionWipe_IsDone;
                transitionCtx->draw = TransitionWipe_Draw;
                transitionCtx->update = TransitionWipe_Update;
                transitionCtx->setType = TransitionWipe_SetType;
                transitionCtx->setColor = TransitionWipe_SetColor;
                transitionCtx->setEnvColor = NULL;
                break;

            case TRANS_TYPE_FADE_BLACK:
            case TRANS_TYPE_FADE_WHITE:
            case TRANS_TYPE_FADE_BLACK_FAST:
            case TRANS_TYPE_FADE_WHITE_FAST:
            case TRANS_TYPE_FADE_BLACK_SLOW:
            case TRANS_TYPE_FADE_WHITE_SLOW:
            case TRANS_TYPE_FADE_WHITE_CS_DELAYED:
            case TRANS_TYPE_FADE_WHITE_INSTANT:
            case TRANS_TYPE_FADE_GREEN:
            case TRANS_TYPE_FADE_BLUE:
                transitionCtx->init = TransitionFade_Init;
                transitionCtx->destroy = TransitionFade_Destroy;
                transitionCtx->start = TransitionFade_Start;
                transitionCtx->isDone = TransitionFade_IsDone;
                transitionCtx->draw = TransitionFade_Draw;
                transitionCtx->update = TransitionFade_Update;
                transitionCtx->setType = TransitionFade_SetType;
                transitionCtx->setColor = TransitionFade_SetColor;
                transitionCtx->setEnvColor = NULL;
                break;

            case TRANS_TYPE_FILL_WHITE2:
            case TRANS_TYPE_FILL_WHITE:
                play->transitionMode = TRANS_MODE_FILL_WHITE_INIT;
                break;

            case TRANS_TYPE_INSTANT:
                play->transitionMode = TRANS_MODE_INSTANT;
                break;

            case TRANS_TYPE_FILL_BROWN:
                play->transitionMode = TRANS_MODE_FILL_BROWN_INIT;
                break;

            case TRANS_TYPE_SANDSTORM_PERSIST:
                play->transitionMode = TRANS_MODE_SANDSTORM_INIT;
                break;

            case TRANS_TYPE_SANDSTORM_END:
                play->transitionMode = TRANS_MODE_SANDSTORM_END_INIT;
                break;

            case TRANS_TYPE_CS_BLACK_FILL:
                play->transitionMode = TRANS_MODE_CS_BLACK_FILL_INIT;
                break;

            default:
                Fault_AddHungupAndCrash(__FILE__, __LINE__);
                break;
        }
    }
}

void func_800BC88C(PlayState* play) {
    play->transitionCtx.transitionType = -1;
}

Gfx* Play_SetFog(PlayState* play, Gfx* gfx) {
    return Gfx_SetFog2(gfx, play->lightCtx.fogColor[0], play->lightCtx.fogColor[1], play->lightCtx.fogColor[2], 0,
                       play->lightCtx.fogNear, 1000);
}

void Play_Destroy(GameState* thisx) {
    PlayState* play = (PlayState*)thisx;
    Player* player = GET_PLAYER(play);

    GameInteractor_ExecuteOnPlayDestroy();
    LocalMP_ClearSplitScreenCameras(play);

    play->state.gfxCtx->callback = NULL;
    play->state.gfxCtx->callbackParam = 0;

    SREG(91) = 0;
    R_PAUSE_MENU_MODE = 0;

    PreRender_Destroy(&play->pauseBgPreRender);
    Effect_DeleteAll(play);
    EffectSs_ClearAll(play);
    CollisionCheck_DestroyContext(play, &play->colChkCtx);

    if (gTrnsnUnkState == 3) {
        TransitionUnk_Destroy(&sTrnsnUnk);
        gTrnsnUnkState = 0;
    }

    if (play->transitionMode == TRANS_MODE_INSTANCE_RUNNING) {
        play->transitionCtx.destroy(&play->transitionCtx.data);
        func_800BC88C(play);
        play->transitionMode = TRANS_MODE_OFF;
    }

    ShrinkWindow_Destroy();
    TransitionFade_Destroy(&play->transitionFade);
    VisMono_Destroy(&gPlayVisMono);

    if (gSaveContext.linkAge != play->linkAgeOnLoad) {
        Inventory_SwapAgeEquipment();
        Player_SetEquipmentData(play, player);
    }

    func_80031C3C(&play->actorCtx, play);
    func_80110990(play);
    KaleidoScopeCall_Destroy(play);
    KaleidoManager_Destroy();
    ZeldaArena_Cleanup();

    Fault_RemoveClient(&D_801614B8);

    disableBetaQuest();

    gPlayState = NULL;
}

u8 CheckStoneCount() {
    u8 stoneCount = 0;

    if (CHECK_QUEST_ITEM(QUEST_KOKIRI_EMERALD)) {
        stoneCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_GORON_RUBY)) {
        stoneCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_ZORA_SAPPHIRE)) {
        stoneCount++;
    }

    return stoneCount;
}

u8 CheckMedallionCount() {
    u8 medallionCount = 0;

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_FOREST)) {
        medallionCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_FIRE)) {
        medallionCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_WATER)) {
        medallionCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_SHADOW)) {
        medallionCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_SPIRIT)) {
        medallionCount++;
    }

    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_LIGHT)) {
        medallionCount++;
    }

    return medallionCount;
}

u8 CheckDungeonCount() {
    u8 dungeonCount = 0;

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_DEKU_TREE_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_DODONGOS_CAVERN_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_JABU_JABUS_BELLY_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_FOREST_TEMPLE_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_FIRE_TEMPLE_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetEventChkInf(EVENTCHKINF_USED_WATER_TEMPLE_BLUE_WARP)) {
        dungeonCount++;
    }

    if (Flags_GetRandomizerInf(RAND_INF_DUNGEONS_DONE_SPIRIT_TEMPLE)) {
        dungeonCount++;
    }

    if (Flags_GetRandomizerInf(RAND_INF_DUNGEONS_DONE_SHADOW_TEMPLE)) {
        dungeonCount++;
    }

    return dungeonCount;
}

u8 CheckBridgeRewardCount() {
    u8 bridgeRewardCount = 0;

    switch (Randomizer_GetSettingValue(RSK_BRIDGE_OPTIONS)) {
        case RO_BRIDGE_WILDCARD_REWARD:
            if (Flags_GetRandomizerInf(RAND_INF_GREG_FOUND)) {
                bridgeRewardCount += 1;
            }
            break;
        case RO_BRIDGE_GREG_REWARD:
            if (Flags_GetRandomizerInf(RAND_INF_GREG_FOUND)) {
                bridgeRewardCount += 1;
            }
            break;
    }
    return bridgeRewardCount;
}

u8 CheckLACSRewardCount() {
    u8 lacsRewardCount = 0;

    switch (Randomizer_GetSettingValue(RSK_LACS_OPTIONS)) {
        case RO_LACS_WILDCARD_REWARD:
            if (Flags_GetRandomizerInf(RAND_INF_GREG_FOUND)) {
                lacsRewardCount += 1;
            }
            break;
        case RO_LACS_GREG_REWARD:
            if (Flags_GetRandomizerInf(RAND_INF_GREG_FOUND)) {
                lacsRewardCount += 1;
            }
            break;
    }
    return lacsRewardCount;
}

void Play_Init(GameState* thisx) {
    PlayState* play = (PlayState*)thisx;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    uintptr_t zAlloc;
    uintptr_t zAllocAligned;
    size_t zAllocSize;
    Player* player;
    s32 playerStartBgCamIndex;
    s32 i;
    u8 baseSceneLayer;
    s32 pad[2];

    enableBetaQuest();

    // Properly initialize the frame counter so it doesn't use garbage data
    if (!firstInit) {
        play->gameplayFrames = 0;
        firstInit = 1;
    }

    // Invalid entrance, so immediately exit the game to opening title
    if (gSaveContext.entranceIndex == ENTR_LOAD_OPENING) {
        gSaveContext.entranceIndex = 0;
        play->state.running = false;
        SET_NEXT_GAMESTATE(&play->state, Opening_Init, OpeningContext);
        GameInteractor_ExecuteOnExitGame(gSaveContext.fileNum);
        return;
    }

    gPlayState = play;

    SystemArena_Display();

    // OTRTODO allocate double the normal amount of memory
    // This is to avoid some parts of the game, like loading actors, causing OoM
    // This is potionally unavoidable due to struct size differences, but is x2 the right amount?
    GameState_Realloc(&play->state, 0x1D4790 * 2);
    KaleidoManager_Init(play);
    View_Init(&play->view, gfxCtx);
    Audio_SetExtraFilter(0);
    Quake_Init();

    for (i = 0; i < ARRAY_COUNT(play->cameraPtrs); i++) {
        play->cameraPtrs[i] = NULL;
    }

    Camera_Init(&play->mainCamera, &play->view, &play->colCtx, play);
    Camera_ChangeStatus(&play->mainCamera, CAM_STAT_ACTIVE);

    for (i = 0; i < 3; i++) {
        Camera_Init(&play->subCameras[i], &play->view, &play->colCtx, play);
        Camera_ChangeStatus(&play->subCameras[i], CAM_STAT_UNK100);
    }

    play->cameraPtrs[MAIN_CAM] = &play->mainCamera;
    play->cameraPtrs[MAIN_CAM]->uid = 0;
    play->activeCamera = MAIN_CAM;
    func_8005AC48(&play->mainCamera, 0xFF);
    // Sram_Init(this, &this->sramCtx);
    Regs_InitData(play);
    Message_Init(play);
    GameOver_Init(play);
    SoundSource_InitAll(play);
    Effect_InitContext(play);
    EffectSs_InitInfo(play, 0x55);
    CollisionCheck_InitContext(play, &play->colChkCtx);
    AnimationContext_Reset(&play->animationCtx);
    func_8006450C(play, &play->csCtx);

    if (gSaveContext.nextCutsceneIndex != 0xFFEF) {
        gSaveContext.cutsceneIndex = gSaveContext.nextCutsceneIndex;
        gSaveContext.nextCutsceneIndex = 0xFFEF;
    }

    if (gSaveContext.cutsceneIndex == 0xFFFD) {
        gSaveContext.cutsceneIndex = 0;
    }

    if (gSaveContext.nextDayTime != 0xFFFF) {
        gSaveContext.dayTime = gSaveContext.nextDayTime;
        gSaveContext.skyboxTime = gSaveContext.nextDayTime;
    }

    if (gSaveContext.dayTime > 0xC000 || gSaveContext.dayTime < 0x4555) {
        gSaveContext.nightFlag = 1;
    } else {
        gSaveContext.nightFlag = 0;
    }

    Cutscene_HandleConditionalTriggers(play);

    if (gSaveContext.gameMode != GAMEMODE_NORMAL || gSaveContext.cutsceneIndex >= 0xFFF0) {
        gSaveContext.nayrusLoveTimer = 0;
        Magic_Reset(play);
        gSaveContext.sceneSetupIndex = SCENE_LAYER_CUTSCENE_FIRST + (gSaveContext.cutsceneIndex & 0xF);
    } else if (!LINK_IS_ADULT && IS_DAY) {
        gSaveContext.sceneSetupIndex = SCENE_LAYER_CHILD_DAY;
    } else if (!LINK_IS_ADULT && !IS_DAY) {
        gSaveContext.sceneSetupIndex = SCENE_LAYER_CHILD_NIGHT;
    } else if (LINK_IS_ADULT && IS_DAY) {
        gSaveContext.sceneSetupIndex = SCENE_LAYER_ADULT_DAY;
    } else {
        gSaveContext.sceneSetupIndex = SCENE_LAYER_ADULT_NIGHT;
    }

    // save the base scene layer (before accounting for the special cases below) to use later for the transition type
    baseSceneLayer = gSaveContext.sceneSetupIndex;

    if ((gEntranceTable[((void)0, gSaveContext.entranceIndex)].scene == SCENE_HYRULE_FIELD) && !LINK_IS_ADULT &&
        !IS_CUTSCENE_LAYER) {
        if (CHECK_QUEST_ITEM(QUEST_KOKIRI_EMERALD) && CHECK_QUEST_ITEM(QUEST_GORON_RUBY) &&
            CHECK_QUEST_ITEM(QUEST_ZORA_SAPPHIRE)) {
            gSaveContext.sceneSetupIndex = 1;
        } else {
            gSaveContext.sceneSetupIndex = 0;
        }
    } else if ((gEntranceTable[((void)0, gSaveContext.entranceIndex)].scene == SCENE_KOKIRI_FOREST) && LINK_IS_ADULT &&
               !IS_CUTSCENE_LAYER) {
        gSaveContext.sceneSetupIndex = (Flags_GetEventChkInf(EVENTCHKINF_USED_FOREST_TEMPLE_BLUE_WARP)) ? 3 : 2;
    }

    Play_SpawnScene(
        play, gEntranceTable[((void)0, gSaveContext.entranceIndex) + ((void)0, gSaveContext.sceneSetupIndex)].scene,
        gEntranceTable[((void)0, gSaveContext.sceneSetupIndex) + ((void)0, gSaveContext.entranceIndex)].spawn);

    osSyncPrintf("\nSCENE_NO=%d COUNTER=%d\n", ((void)0, gSaveContext.entranceIndex), gSaveContext.sceneSetupIndex);

#if 0
    // When entering Gerudo Valley in the credits, trigger the GC emulator to play the ending movie.
    // The emulator constantly checks whether PC is 0x81000000, so this works even though it's not a valid address.
    if ((gEntranceTable[((void)0, gSaveContext.save.entranceIndex)].sceneId == SCENE_GERUDO_VALLEY) &&
        gSaveContext.sceneLayer == 6) {
        PRINTF("エンディングはじまるよー\n"); // "The ending starts"
        ((void (*)(void))0x81000000)();
        PRINTF("出戻り？\n"); // "Return?"
    }
#endif

    Cutscene_HandleEntranceTriggers(play);
    KaleidoScopeCall_Init(play);
    func_801109B0(play);

    if (gSaveContext.nextDayTime != 0xFFFF) {
        if (gSaveContext.nextDayTime == 0x8001) {
            gSaveContext.totalDays++;
            gSaveContext.bgsDayCount++;
            gSaveContext.dogIsLost = true;

            if (Inventory_ReplaceItem(play, ITEM_WEIRD_EGG, ITEM_CHICKEN) || Inventory_HatchPocketCucco(play)) {
                GameInteractor_ExecuteOnCuccoOrChickenHatch();
                Message_StartTextbox(play, 0x3066, NULL);
            }

            gSaveContext.nextDayTime = 0xFFFE;
        } else {
            gSaveContext.nextDayTime = 0xFFFD;
        }
    }

    SREG(91) = -1;
    R_PAUSE_MENU_MODE = 0;
    PreRender_Init(&play->pauseBgPreRender);
    PreRender_SetValuesSave(&play->pauseBgPreRender, SCREEN_WIDTH, SCREEN_HEIGHT, NULL, NULL, NULL);
    PreRender_SetValues(&play->pauseBgPreRender, SCREEN_WIDTH, SCREEN_HEIGHT, NULL, NULL);
    gTrnsnUnkState = 0;
    play->transitionMode = TRANS_MODE_OFF;
    FrameAdvance_Init(&play->frameAdvCtx);
    Rand_Seed((u32)osGetTime());
    Matrix_Init(&play->state);
    play->state.main = Play_Main;
    play->state.destroy = Play_Destroy;
    play->transitionTrigger = TRANS_TRIGGER_END;
    play->unk_11E16 = 0xFF;
    play->unk_11E18 = 0;
    play->unk_11DE9 = false;

    if (gSaveContext.gameMode != GAMEMODE_TITLE_SCREEN) {
        if (gSaveContext.nextTransitionType == TRANS_NEXT_TYPE_DEFAULT) {
            play->transitionType = ENTRANCE_INFO_END_TRANS_TYPE(
                gEntranceTable[((void)0, gSaveContext.entranceIndex) + baseSceneLayer].field); // Fade In
        } else {
            play->transitionType = gSaveContext.nextTransitionType;
            gSaveContext.nextTransitionType = TRANS_NEXT_TYPE_DEFAULT;
        }
    } else {
        play->transitionType = TRANS_TYPE_FADE_BLACK_SLOW;
    }

    ShrinkWindow_Init();
    TransitionFade_Init(&play->transitionFade);
    TransitionFade_SetType(&play->transitionFade, 3);
    TransitionFade_SetColor(&play->transitionFade, RGBA8(160, 160, 160, 255));
    TransitionFade_Start(&play->transitionFade);
    VisMono_Init(&gPlayVisMono);
    gVisMonoColor.a = 0;
    Flags_UnsetAllEnv(play);

    osSyncPrintf("ZELDA ALLOC SIZE=%x\n", THA_GetSize(&play->state.tha));
    zAllocSize = THA_GetSize(&play->state.tha);
    zAlloc = (uintptr_t)GAMESTATE_ALLOC_MC(&play->state, zAllocSize);
    zAllocAligned = (zAlloc + 8) & ~0xF;
    ZeldaArena_Init((void*)zAllocAligned, zAllocSize - (zAllocAligned - zAlloc));
    // "Zelda Heap"
    osSyncPrintf("ゼルダヒープ %08x-%08x\n", zAllocAligned,
                 (u8*)zAllocAligned + zAllocSize - (s32)(zAllocAligned - zAlloc));

    Fault_AddClient(&D_801614B8, ZeldaArena_Display, NULL, NULL);

    // In order to keep masks equipped on first load, we need to pre-set the age reqs for the item and slot
    if (CVarGetInteger(CVAR_ENHANCEMENT("AdultMasks"), 0) || CVarGetInteger(CVAR_CHEAT("TimelessEquipment"), 0)) {
        for (int i = ITEM_MASK_KEATON; i <= ITEM_MASK_TRUTH; i += 1) {
            gItemAgeReqs[i] = AGE_REQ_NONE;
        }
        if (INV_CONTENT(ITEM_TRADE_CHILD) >= ITEM_MASK_KEATON && INV_CONTENT(ITEM_TRADE_CHILD) <= ITEM_MASK_TRUTH) {
            gSlotAgeReqs[SLOT_TRADE_CHILD] = AGE_REQ_NONE;
        }
    } else {
        for (int i = ITEM_MASK_KEATON; i <= ITEM_MASK_TRUTH; i += 1) {
            gItemAgeReqs[i] = AGE_REQ_CHILD;
        }
        gSlotAgeReqs[SLOT_TRADE_CHILD] = AGE_REQ_CHILD;
    }

    // Handle Rocs Feather requirement
    gItemAgeReqs[ITEM_ROCS_FEATHER] = AGE_REQ_NONE;
    gSlotAgeReqs[SLOT_NAYRUS_LOVE] = AGE_REQ_NONE;

    func_800304DC(play, &play->actorCtx, play->linkActorEntry);

    while (!func_800973FC(play, &play->roomCtx)) {
        ; // Empty Loop
    }

    player = GET_PLAYER(play);
    Camera_InitPlayerSettings(&play->mainCamera, player);
    Camera_ChangeMode(&play->mainCamera, CAM_MODE_NORMAL);

    // OTRTODO: Bounds check cameraDataList to guard against scenes spawning the player with
    // an out of bounds background camera index. This requires adding an extra field to the
    // CollisionHeader struct to save the length of cameraDataList.
    // Fixes Dodongo's Cavern blue warp crash.
    {
        CollisionHeader* colHeader = BgCheck_GetCollisionHeader(&play->colCtx, BGCHECK_SCENE);

        u8 camId = player->actor.params & 0xFF;
        // If the player's start cam is out of bounds, set it to 0xFF so it isn't used.
        if (colHeader != NULL && (camId != 0xFF) && (camId >= colHeader->cameraDataListLen)) {
            player->actor.params |= 0xFF;
        }
    }

    playerStartBgCamIndex = player->actor.params & 0xFF;
    if (playerStartBgCamIndex != 0xFF) {
        osSyncPrintf("player has start camera ID (" VT_FGCOL(BLUE) "%d" VT_RST ")\n", playerStartBgCamIndex);
        Camera_ChangeDataIdx(&play->mainCamera, playerStartBgCamIndex);
    }

    if (YREG(15) == 32) {
        play->unk_1242B = 2;
    } else if (YREG(15) == 16) {
        play->unk_1242B = 1;
    } else {
        play->unk_1242B = 0;
    }
    Interface_SetSceneRestrictions(play);
    Environment_PlaySceneSequence(play);
    gSaveContext.seqId = play->sequenceCtx.seqId;
    gSaveContext.natureAmbienceId = play->sequenceCtx.natureAmbienceId;
    func_8002DF18(play, GET_PLAYER(play));
    AnimationContext_Update(play, &play->animationCtx);
    gSaveContext.respawnFlag = 0;

    // #region SOH [Stats]
    if (gSaveContext.ship.stats.sceneNum != gPlayState->sceneNum) {
        u16 idx = gSaveContext.ship.stats.tsIdx;
        gSaveContext.ship.stats.sceneTimestamps[idx].sceneTime = gSaveContext.ship.stats.sceneTimer / 2;
        gSaveContext.ship.stats.sceneTimestamps[idx].roomTime = gSaveContext.ship.stats.roomTimer / 2;
        gSaveContext.ship.stats.sceneTimestamps[idx].scene = gSaveContext.ship.stats.sceneNum;
        gSaveContext.ship.stats.sceneTimestamps[idx].room = gSaveContext.ship.stats.roomNum;
        gSaveContext.ship.stats.sceneTimestamps[idx].isRoom =
            gPlayState->sceneNum == gSaveContext.ship.stats.sceneTimestamps[idx].scene &&
            gPlayState->roomCtx.curRoom.num != gSaveContext.ship.stats.sceneTimestamps[idx].room;
        gSaveContext.ship.stats.tsIdx++;
        gSaveContext.ship.stats.sceneTimer = 0;
        gSaveContext.ship.stats.roomTimer = 0;
    } else if (gSaveContext.ship.stats.roomNum != gPlayState->roomCtx.curRoom.num) {
        u16 idx = gSaveContext.ship.stats.tsIdx;
        gSaveContext.ship.stats.sceneTimestamps[idx].roomTime = gSaveContext.ship.stats.roomTimer / 2;
        gSaveContext.ship.stats.sceneTimestamps[idx].scene = gSaveContext.ship.stats.sceneNum;
        gSaveContext.ship.stats.sceneTimestamps[idx].room = gSaveContext.ship.stats.roomNum;
        gSaveContext.ship.stats.sceneTimestamps[idx].isRoom =
            gPlayState->sceneNum == gSaveContext.ship.stats.sceneTimestamps[idx].scene &&
            gPlayState->roomCtx.curRoom.num != gSaveContext.ship.stats.sceneTimestamps[idx].room;
        gSaveContext.ship.stats.tsIdx++;
        gSaveContext.ship.stats.roomTimer = 0;
    }

    gSaveContext.ship.stats.sceneNum = gPlayState->sceneNum;
    gSaveContext.ship.stats.roomNum = gPlayState->roomCtx.curRoom.num;
    // #endregion

#if 0
    if (R_USE_DEBUG_CUTSCENE) {
        static u64 sDebugCutsceneScriptBuf[0xA00];

        gDebugCutsceneScript = sDebugCutsceneScriptBuf;
        PRINTF("\nkawauso_data=[%x]", gDebugCutsceneScript);

        // This hardcoded ROM address extends past the end of the ROM file.
        // Presumably the ROM was larger at a previous point in development when this debug feature was used.
        DmaMgr_DmaRomToRam(0x03FEB000, gDebugCutsceneScript, sizeof(sDebugCutsceneScriptBuf));
    }
#endif

    if (CVarGetInteger(CVAR_ENHANCEMENT("IvanCoopModeEnabled"), 0)) {
        CVarSetInteger(CVAR_ENHANCEMENT("IvanCoopModeEnabled"), 0);
    }

    sLocalMPLastTeleportRequest = CVarGetInteger(LOCAL_MP_TELEPORT_REQUEST_CVAR, 0);

    // nextEntranceIndex was not initialized, so the previous value was carried over during soft resets.
    gPlayState->nextEntranceIndex = gSaveContext.entranceIndex;
}

void Play_Update(PlayState* play) {
    Input* input = play->state.input;
    s32 isPaused;
    s32 pad1;

    if ((SREG(1) < 0) || (DREG(0) != 0)) {
        SREG(1) = 0;
        ZeldaArena_Display();
    }

    if ((HREG(80) == 18) && (HREG(81) < 0)) {
        u32 i;
        s32 pad2;

        HREG(81) = 0;
        osSyncPrintf("object_exchange_rom_address %u\n", gObjectTableSize);
        osSyncPrintf("RomStart RomEnd   Size\n");

        for (i = 0; i < gObjectTableSize; i++) {
            ptrdiff_t size = gObjectTable[i].vromEnd - gObjectTable[i].vromStart;

            osSyncPrintf("%08x-%08x %08x(%8.3fKB)\n", gObjectTable[i].vromStart, gObjectTable[i].vromEnd, size,
                         size / 1024.0f);
        }

        osSyncPrintf("\n");
    }

    if ((HREG(81) == 18) && (HREG(82) < 0)) {
        HREG(82) = 0;
        // ActorOverlayTable_LogPrint();
    }

    if (CVarGetInteger(CVAR_SETTING("FreeLook.Enabled"), 0) && Player_InCsMode(play)) {
        play->manualCamera = false;
    }

    gSegments[4] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[play->objectCtx.mainKeepIndex].segment);
    gSegments[5] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[play->objectCtx.subKeepIndex].segment);
    gSegments[2] = VIRTUAL_TO_PHYSICAL(play->sceneSegment);

    if (FrameAdvance_Update(&play->frameAdvCtx, &input[1])) {
        if ((play->transitionMode == TRANS_MODE_OFF) && (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
            play->transitionMode = TRANS_MODE_SETUP;
        }

        // #region SOH [Stats] Gameplay stats: Count button presses
        if (!gSaveContext.ship.stats.gameComplete) {
            if (CHECK_BTN_ALL(input[0].press.button, BTN_A)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_A]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_B)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_B]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_CUP)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_CUP]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_CRIGHT)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_CRIGHT]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_CLEFT)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_CLEFT]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_CDOWN)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_CDOWN]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_DUP)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_DUP]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_DRIGHT)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_DRIGHT]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_DDOWN)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_DDOWN]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_DLEFT)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_DLEFT]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_L)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_L]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_R)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_R]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_Z)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_Z]++;
            }
            if (CHECK_BTN_ALL(input[0].press.button, BTN_START)) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_START]++;
            }

            // Start RTA timing on first non-c-up input after intro cutscene
            if (!gSaveContext.ship.stats.firstInput && !Player_InCsMode(play) &&
                ((input[0].press.button && input[0].press.button != 0x8) || input[0].rel.stick_x != 0 ||
                 input[0].rel.stick_y != 0)) {
                gSaveContext.ship.stats.firstInput = GetUnixTimestamp();
            }
        }
        // #endregion

        if (gTrnsnUnkState != 0) {
            switch (gTrnsnUnkState) {
                case 2:
                    if (TransitionUnk_Init(&sTrnsnUnk, 10, 7) == NULL) {
                        osSyncPrintf("fbdemo_init呼出し失敗！\n"); // "fbdemo_init call failed!"
                        gTrnsnUnkState = 0;
                    } else {
                        sTrnsnUnk.zBuffer = (u16*)gZBuffer;
                        gTrnsnUnkState = 3;
                        R_UPDATE_RATE = 1;
                    }
                    break;
                case 3:
                    func_800B23E8(&sTrnsnUnk);
                    break;
            }
        }

        if ((u32)play->transitionMode != TRANS_MODE_OFF) {
            switch (play->transitionMode) {
                case TRANS_MODE_SETUP:
                    if (play->transitionTrigger != TRANS_TRIGGER_END) {
                        s16 sceneLayer = 0;
                        Interface_ChangeAlpha(1);

                        if (gSaveContext.cutsceneIndex >= 0xFFF0) {
                            sceneLayer = SCENE_LAYER_CUTSCENE_FIRST + (gSaveContext.cutsceneIndex & 0xF);
                        }

                        // fade out bgm if "continue bgm" flag is not set
                        if (!(gEntranceTable[play->nextEntranceIndex + sceneLayer].field &
                              ENTRANCE_INFO_CONTINUE_BGM_FLAG)) {
                            // "Sound initalized. 111"
                            osSyncPrintf("\n\n\nサウンドイニシャル来ました。111");
                            if ((play->transitionType < TRANS_TYPE_MAX) && !Environment_IsForcedSequenceDisabled()) {
                                // "Sound initalized. 222"
                                osSyncPrintf("\n\n\nサウンドイニシャル来ました。222");
                                func_800F6964(0x14);
                                gSaveContext.seqId = (u8)NA_BGM_DISABLED;
                                gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
                            }
                        }
                    }

                    if (!R_TRANS_DBG_ENABLED) {
                        Gameplay_SetupTransition(play, play->transitionType);
                    } else {
                        Gameplay_SetupTransition(play, R_TRANS_DBG_TYPE);
                    }

                    if (play->transitionMode >= TRANS_MODE_FILL_WHITE_INIT) {
                        // non-instance modes break out of this switch
                        break;
                    }
                    FALLTHROUGH;
                case TRANS_MODE_INSTANCE_INIT:
                    play->transitionCtx.init(&play->transitionCtx.data);

                    // Circle Transition Types
                    if ((play->transitionCtx.transitionType >> 5) == 1) {
                        play->transitionCtx.setType(&play->transitionCtx.data,
                                                    play->transitionCtx.transitionType | TC_SET_PARAMS);
                    }

                    gSaveContext.transWipeSpeed = 14;

                    if ((play->transitionCtx.transitionType == TRANS_TYPE_WIPE_FAST) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FILL_WHITE2)) {
                        //! @bug TRANS_TYPE_FILL_WHITE2 will never reach this code.
                        //! It is a non-instance type transition which doesn't run this case.
                        gSaveContext.transWipeSpeed = 28;
                    }

                    gSaveContext.transFadeDuration = 60;

                    if ((play->transitionCtx.transitionType == TRANS_TYPE_FADE_BLACK_FAST) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_FAST)) {
                        gSaveContext.transFadeDuration = 20;
                    } else if ((play->transitionCtx.transitionType == TRANS_TYPE_FADE_BLACK_SLOW) ||
                               (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_SLOW)) {
                        gSaveContext.transFadeDuration = 150;
                    } else if (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_INSTANT) {
                        gSaveContext.transFadeDuration = 2;
                    }

                    if ((play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_FAST) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_SLOW) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_CS_DELAYED) ||
                        (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_INSTANT)) {
                        play->transitionCtx.setColor(&play->transitionCtx.data, RGBA8(160, 160, 160, 255));

                        if (play->transitionCtx.setEnvColor != NULL) {
                            play->transitionCtx.setEnvColor(&play->transitionCtx.data, RGBA8(160, 160, 160, 255));
                        }
                    } else if (play->transitionCtx.transitionType == TRANS_TYPE_FADE_GREEN) {
                        play->transitionCtx.setColor(&play->transitionCtx.data, RGBA8(140, 140, 100, 255));

                        if (play->transitionCtx.setEnvColor != NULL) {
                            play->transitionCtx.setEnvColor(&play->transitionCtx.data, RGBA8(140, 140, 100, 255));
                        }
                    } else if (play->transitionCtx.transitionType == TRANS_TYPE_FADE_BLUE) {
                        play->transitionCtx.setColor(&play->transitionCtx.data, RGBA8(70, 100, 110, 255));

                        if (play->transitionCtx.setEnvColor != NULL) {
                            play->transitionCtx.setEnvColor(&play->transitionCtx.data, RGBA8(70, 100, 110, 255));
                        }
                    } else {
                        play->transitionCtx.setColor(&play->transitionCtx.data, RGBA8(0, 0, 0, 0));

                        if (play->transitionCtx.setEnvColor != NULL) {
                            play->transitionCtx.setEnvColor(&play->transitionCtx.data, RGBA8(0, 0, 0, 0));
                        }
                    }

                    if (play->transitionTrigger == TRANS_TRIGGER_END) {
                        play->transitionCtx.setType(&play->transitionCtx.data, 1);
                    } else {
                        play->transitionCtx.setType(&play->transitionCtx.data, 2);
                    }

                    play->transitionCtx.start(&play->transitionCtx);

                    if (play->transitionCtx.transitionType == TRANS_TYPE_FADE_WHITE_CS_DELAYED) {
                        play->transitionMode = TRANS_MODE_INSTANCE_WAIT;
                    } else {
                        play->transitionMode = TRANS_MODE_INSTANCE_RUNNING;
                    }
                    break;

                case TRANS_MODE_INSTANCE_RUNNING:
                    if (play->transitionCtx.isDone(&play->transitionCtx.data)) {
                        if (play->transitionCtx.transitionType >= TRANS_TYPE_MAX) {
                            if (play->transitionTrigger == TRANS_TRIGGER_END) {
                                play->transitionCtx.destroy(&play->transitionCtx.data);
                                func_800BC88C(play);
                                play->transitionMode = TRANS_MODE_OFF;
                            }
                        } else if (play->transitionTrigger != TRANS_TRIGGER_END) {
                            play->state.running = false;

                            if (gSaveContext.gameMode != GAMEMODE_FILE_SELECT) {
                                SET_NEXT_GAMESTATE(&play->state, Play_Init, PlayState);
                                gSaveContext.entranceIndex = play->nextEntranceIndex;

                                if (gSaveContext.minigameState == 1) {
                                    gSaveContext.minigameState = 3;
                                }
                            } else {
                                SET_NEXT_GAMESTATE(&play->state, FileChoose_Init, FileChooseContext);
                            }
                        } else {
                            play->transitionCtx.destroy(&play->transitionCtx.data);
                            func_800BC88C(play);
                            play->transitionMode = TRANS_MODE_OFF;

                            if (gTrnsnUnkState == 3) {
                                TransitionUnk_Destroy(&sTrnsnUnk);
                                gTrnsnUnkState = 0;
                                R_UPDATE_RATE = 3;
                            }

                            // Transition end for standard transitions
                            GameInteractor_ExecuteOnTransitionEndHooks(play->sceneNum);
                        }

                        play->transitionTrigger = TRANS_TRIGGER_OFF;
                    } else {
                        play->transitionCtx.update(&play->transitionCtx.data, R_UPDATE_RATE);
                    }
                    break;
            }

            // update non-instance transitions
            switch (play->transitionMode) {
                case TRANS_MODE_FILL_WHITE_INIT:
                    sTransitionFillTimer = 0;
                    play->envCtx.fillScreen = true;
                    play->envCtx.screenFillColor[0] = 160;
                    play->envCtx.screenFillColor[1] = 160;
                    play->envCtx.screenFillColor[2] = 160;

                    if (play->transitionTrigger != TRANS_TRIGGER_END) {
                        play->envCtx.screenFillColor[3] = 0;
                        play->transitionMode = TRANS_MODE_FILL_IN;
                    } else {
                        play->envCtx.screenFillColor[3] = 255;
                        play->transitionMode = TRANS_MODE_FILL_OUT;
                    }
                    break;

                case TRANS_MODE_FILL_IN:
                    play->envCtx.screenFillColor[3] = (sTransitionFillTimer / 20.0f) * 255.0f;

                    if (sTransitionFillTimer >= 20) {
                        play->state.running = false;
                        SET_NEXT_GAMESTATE(&play->state, Play_Init, PlayState);
                        gSaveContext.entranceIndex = play->nextEntranceIndex;
                        play->transitionTrigger = TRANS_TRIGGER_OFF;
                        play->transitionMode = TRANS_MODE_OFF;
                    } else {
                        sTransitionFillTimer++;
                    }
                    break;

                case TRANS_MODE_FILL_OUT:
                    play->envCtx.screenFillColor[3] = (1 - sTransitionFillTimer / 20.0f) * 255.0f;

                    if (sTransitionFillTimer >= 20) {
                        gTrnsnUnkState = 0;
                        R_UPDATE_RATE = 3;
                        play->transitionTrigger = TRANS_TRIGGER_OFF;
                        play->transitionMode = TRANS_MODE_OFF;
                        play->envCtx.fillScreen = false;
                    } else {
                        sTransitionFillTimer++;
                    }
                    break;

                case TRANS_MODE_FILL_BROWN_INIT:
                    sTransitionFillTimer = 0;
                    play->envCtx.fillScreen = true;
                    play->envCtx.screenFillColor[0] = 170;
                    play->envCtx.screenFillColor[1] = 160;
                    play->envCtx.screenFillColor[2] = 150;

                    if (play->transitionTrigger != TRANS_TRIGGER_END) {
                        play->envCtx.screenFillColor[3] = 0;
                        play->transitionMode = TRANS_MODE_FILL_IN;
                    } else {
                        play->envCtx.screenFillColor[3] = 255;
                        play->transitionMode = TRANS_MODE_FILL_OUT;
                    }
                    break;

                case TRANS_MODE_INSTANT:
                    if (play->transitionTrigger != TRANS_TRIGGER_END) {
                        play->state.running = 0;
                        SET_NEXT_GAMESTATE(&play->state, Play_Init, PlayState);
                        gSaveContext.entranceIndex = play->nextEntranceIndex;
                        play->transitionTrigger = TRANS_TRIGGER_OFF;
                        play->transitionMode = TRANS_MODE_OFF;
                    } else {
                        gTrnsnUnkState = 0;
                        R_UPDATE_RATE = 3;
                        play->transitionTrigger = TRANS_TRIGGER_OFF;
                        play->transitionMode = TRANS_MODE_OFF;
                    }
                    break;

                case TRANS_MODE_INSTANCE_WAIT:
                    if (gSaveContext.cutsceneTransitionControl != 0) {
                        play->transitionMode = TRANS_MODE_INSTANCE_RUNNING;
                    }
                    break;

                case TRANS_MODE_SANDSTORM_INIT:
                    if (play->transitionTrigger != TRANS_TRIGGER_END) {
                        play->envCtx.sandstormState = SANDSTORM_FILL;
                        play->transitionMode = TRANS_MODE_SANDSTORM;
                    } else {
                        play->envCtx.sandstormState = SANDSTORM_UNFILL;
                        play->envCtx.sandstormPrimA = 255;
                        play->envCtx.sandstormEnvA = 255;
                        play->transitionMode = TRANS_MODE_SANDSTORM;
                    }
                    break;

                case TRANS_MODE_SANDSTORM:
                    Audio_PlaySoundGeneral(NA_SE_EV_SAND_STORM - SFX_FLAG, &gSfxDefaultPos, 4,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultReverb);

                    if (play->transitionTrigger == TRANS_TRIGGER_END) {
                        if (play->envCtx.sandstormPrimA < 110) {
                            gTrnsnUnkState = 0;
                            R_UPDATE_RATE = 3;
                            play->transitionTrigger = TRANS_TRIGGER_OFF;
                            play->transitionMode = TRANS_MODE_OFF;

                            // Transition end for sandstorm effect (delayed until effect is finished)
                            GameInteractor_ExecuteOnTransitionEndHooks(play->sceneNum);
                        }
                    } else {
                        if (play->envCtx.sandstormEnvA == 255) {
                            play->state.running = false;
                            SET_NEXT_GAMESTATE(&play->state, Play_Init, PlayState);
                            gSaveContext.entranceIndex = play->nextEntranceIndex;
                            play->transitionTrigger = TRANS_TRIGGER_OFF;
                            play->transitionMode = TRANS_MODE_OFF;
                        }
                    }
                    break;

                case TRANS_MODE_SANDSTORM_END_INIT:
                    if (play->transitionTrigger == TRANS_TRIGGER_END) {
                        play->envCtx.sandstormState = SANDSTORM_DISSIPATE;
                        play->envCtx.sandstormPrimA = 255;
                        play->envCtx.sandstormEnvA = 255;
                        // "It's here!!!!!!!!!"
                        LOG_STRING("来た!!!!!!!!!!!!!!!!!!!!!");
                        play->transitionMode = TRANS_MODE_SANDSTORM_END;
                    } else {
                        play->transitionMode = TRANS_MODE_SANDSTORM_INIT;
                    }
                    break;

                case TRANS_MODE_SANDSTORM_END:
                    Audio_PlaySoundGeneral(NA_SE_EV_SAND_STORM - SFX_FLAG, &gSfxDefaultPos, 4,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultReverb);

                    if (play->transitionTrigger == TRANS_TRIGGER_END) {
                        if (play->envCtx.sandstormPrimA <= 0) {
                            gTrnsnUnkState = 0;
                            R_UPDATE_RATE = 3;
                            play->transitionTrigger = TRANS_TRIGGER_OFF;
                            play->transitionMode = TRANS_MODE_OFF;

                            // Transition end for sandstorm effect (delayed until effect is finished)
                            GameInteractor_ExecuteOnTransitionEndHooks(play->sceneNum);
                        }
                    }
                    break;

                case TRANS_MODE_CS_BLACK_FILL_INIT:
                    sTransitionFillTimer = 0;
                    play->envCtx.fillScreen = true;
                    play->envCtx.screenFillColor[0] = 0;
                    play->envCtx.screenFillColor[1] = 0;
                    play->envCtx.screenFillColor[2] = 0;
                    play->envCtx.screenFillColor[3] = 255;
                    play->transitionMode = TRANS_MODE_CS_BLACK_FILL;
                    break;

                case TRANS_MODE_CS_BLACK_FILL:
                    if (gSaveContext.cutsceneTransitionControl != 0) {
                        play->envCtx.screenFillColor[3] = gSaveContext.cutsceneTransitionControl;

                        if (gSaveContext.cutsceneTransitionControl <= 100) {
                            gTrnsnUnkState = 0;
                            R_UPDATE_RATE = 3;
                            play->transitionTrigger = TRANS_TRIGGER_OFF;
                            play->transitionMode = TRANS_MODE_OFF;
                        }
                    }
                    break;
            }
        }

        PLAY_LOG(3533);

        if (1 && (gTrnsnUnkState != 3)) {
            PLAY_LOG(3542);

            if ((gSaveContext.gameMode == GAMEMODE_NORMAL) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
                (play->gameOverCtx.state == GAMEOVER_INACTIVE)) {
                KaleidoSetup_Update(play);
            }

            PLAY_LOG(3551);
            isPaused = (play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0);

            PLAY_LOG(3555);
            AnimationContext_Reset(&play->animationCtx);

            PLAY_LOG(3561);
            Object_UpdateBank(&play->objectCtx);

            PLAY_LOG(3577);

            if (!isPaused && (IREG(72) == 0)) {
                PLAY_LOG(3580);

                play->gameplayFrames++;
                func_800AA178(true);

                // Gameplay stat tracking
                if (!gSaveContext.ship.stats.gameComplete &&
                    (!IS_BOSS_RUSH || !gSaveContext.ship.quest.data.bossRush.isPaused)) {
                    gSaveContext.ship.stats.playTimer++;
                    gSaveContext.ship.stats.sceneTimer++;
                    gSaveContext.ship.stats.roomTimer++;

                    if (CVarGetInteger(CVAR_ENHANCEMENT("MMBunnyHood"), BUNNY_HOOD_VANILLA) != BUNNY_HOOD_VANILLA &&
                        Player_GetMask(play) == PLAYER_MASK_BUNNY) {
                        gSaveContext.ship.stats.count[COUNT_TIME_BUNNY_HOOD]++;
                    }
                }

                if (play->actorCtx.freezeFlashTimer && (play->actorCtx.freezeFlashTimer-- < 5)) {
                    if (GameInteractor_Should(VB_FLASH_SCREEN_FOR_FINISHING_BLOW, true)) {
                        osSyncPrintf("FINISH=%d\n", play->actorCtx.freezeFlashTimer);

                        if ((play->actorCtx.freezeFlashTimer > 0) && ((play->actorCtx.freezeFlashTimer % 2) != 0)) {
                            play->envCtx.fillScreen = true;
                            play->envCtx.screenFillColor[0] = play->envCtx.screenFillColor[1] =
                                play->envCtx.screenFillColor[2] = 150;
                            play->envCtx.screenFillColor[3] = 80;
                        } else {
                            play->envCtx.fillScreen = false;
                        }
                    }
                } else {
                    PLAY_LOG(3606);
                    func_800973FC(play, &play->roomCtx);

                    PLAY_LOG(3612);
                    CollisionCheck_AT(play, &play->colChkCtx);

                    PLAY_LOG(3618);
                    CollisionCheck_OC(play, &play->colChkCtx);

                    PLAY_LOG(3624);
                    CollisionCheck_Damage(play, &play->colChkCtx);

                    PLAY_LOG(3631);
                    CollisionCheck_ClearContext(play, &play->colChkCtx);

                    PLAY_LOG(3637);

                    if (!play->unk_11DE9) {
                        LocalMP_EnsurePlayers(play);
                        LocalMP_HandleTeleportAllToSelected(play);
                        Actor_UpdateAll(play, &play->actorCtx);
                    }

                    PLAY_LOG(3643);
                    func_80064558(play, &play->csCtx);

                    PLAY_LOG(3648);
                    func_800645A0(play, &play->csCtx);

                    PLAY_LOG(3651);
                    Effect_UpdateAll(play);

                    PLAY_LOG(3657);
                    EffectSs_UpdateAll(play);

                    PLAY_LOG(3662);
                }
            } else {
                func_800AA178(false);
            }

            PLAY_LOG(3672);
            func_80095AA0(play, &play->roomCtx.curRoom, &input[1], 0);

            PLAY_LOG(3675);
            func_80095AA0(play, &play->roomCtx.prevRoom, &input[1], 1);

            PLAY_LOG(3677);

            if (play->unk_1242B != 0) {
                if (CHECK_BTN_ALL(input[0].press.button, BTN_CUP)) {
                    if ((play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0)) {
                        // "Changing viewpoint is prohibited due to the kaleidoscope"
                        osSyncPrintf(VT_FGCOL(CYAN) "カレイドスコープ中につき視点変更を禁止しております\n" VT_RST);
                    } else if (Player_InCsMode(play)) {
                        // "Changing viewpoint is prohibited during the cutscene"
                        osSyncPrintf(VT_FGCOL(CYAN) "デモ中につき視点変更を禁止しております\n" VT_RST);
                    } else if (YREG(15) == 0x10) {
                        Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    } else {
                        // C-Up toggle for houses, move between pivot camera and fixed camera
                        // Toggle viewpoint between VIEWPOINT_LOCKED and VIEWPOINT_PIVOT
                        Play_SetViewpoint(play, play->unk_1242B ^ 3);
                    }
                }

                Play_RequestViewpointBgCam(play);
            }

            PLAY_LOG(3708);
            SkyboxDraw_Update(&play->skyboxCtx);

            PLAY_LOG(3716);

            if ((play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0)) {
                PLAY_LOG(3721);
                KaleidoScopeCall_Update(play);
            } else if (play->gameOverCtx.state != GAMEOVER_INACTIVE) {
                PLAY_LOG(3727);
                GameOver_Update(play);
            } else {
                PLAY_LOG(3733);
                Message_Update(play);
            }

            PLAY_LOG(3737);

            PLAY_LOG(3742);
            Interface_Update(play);

            PLAY_LOG(3765);
            AnimationContext_Update(play, &play->animationCtx);

            PLAY_LOG(3771);
            SoundSource_UpdateAll(play);

            PLAY_LOG(3777);
            ShrinkWindow_Update(R_UPDATE_RATE);

            PLAY_LOG(3783);
            TransitionFade_Update(&play->transitionFade, R_UPDATE_RATE);
        } else {
            goto skip;
        }
    }

    PLAY_LOG(3799);

skip:
    PLAY_LOG(3801);

    GameInteractor_ExecuteOnCameraState(play);
    LocalMP_EnsureSplitScreenCameras(play);

    if (!isPaused || gDbgCamEnabled) {
        s32 i;

        play->nextCamera = play->activeCamera;

        PLAY_LOG(3806);

        for (i = 0; i < NUM_CAMS; i++) {
            if ((i != play->nextCamera) && (play->cameraPtrs[i] != NULL)) {
                PLAY_LOG(3809);
                Camera_Update(play->cameraPtrs[i]);
            }
        }

        Camera_Update(play->cameraPtrs[play->nextCamera]);
        LocalMP_UpdateSharedCamera(play);

        PLAY_LOG(3814);
    }

    PLAY_LOG(3816);
    Environment_Update(play, &play->envCtx, &play->lightCtx, &play->pauseCtx, &play->msgCtx, &play->gameOverCtx,
                       play->state.gfxCtx);
}

void Play_DrawOverlayElements(PlayState* play) {
    if ((play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0)) {
        KaleidoScopeCall_Draw(play);
    }

    if (gSaveContext.gameMode == GAMEMODE_NORMAL) {
        Interface_Draw(play);
    }

    Message_Draw(play);

    if (play->gameOverCtx.state != GAMEOVER_INACTIVE) {
        GameOver_FadeInLights(play);
    }
}

void Play_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Lights* sp228;
    Vec3f sp21C;

    // #region SOH [Port] Frame buffer effects for pause menu
    // Track render size when paused and that a copy was performed
    static u32 lastPauseWidth;
    static u32 lastPauseHeight;
    static bool lastAltAssets;
    static bool hasCapturedPauseBuffer;
    bool recapturePauseBuffer = false;

    // If the size has changed, alt assets toggled, or dropped frames leading to the buffer not being copied,
    // set the prerender state back to setup to copy a new frame.
    // This requires not rendering kaleido during this copy to avoid kaleido itself being copied too.
    if ((R_PAUSE_MENU_MODE == 2 || R_PAUSE_MENU_MODE == 3) &&
        (lastPauseWidth != OTRGetGameRenderWidth() || lastPauseHeight != OTRGetGameRenderHeight() ||
         lastAltAssets != ResourceMgr_IsAltAssetsEnabled() || !hasCapturedPauseBuffer)) {
        R_PAUSE_MENU_MODE = 1;
        recapturePauseBuffer = true;
    }
    // #endregion

    OPEN_DISPS(gfxCtx);

    gSegments[4] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[play->objectCtx.mainKeepIndex].segment);
    gSegments[5] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[play->objectCtx.subKeepIndex].segment);
    gSegments[2] = VIRTUAL_TO_PHYSICAL(play->sceneSegment);

    gSPSegment(POLY_OPA_DISP++, 0x00, NULL);
    gSPSegment(POLY_XLU_DISP++, 0x00, NULL);
    gSPSegment(OVERLAY_DISP++, 0x00, NULL);

    gSPSegment(POLY_OPA_DISP++, 0x04, play->objectCtx.status[play->objectCtx.mainKeepIndex].segment);
    gSPSegment(POLY_XLU_DISP++, 0x04, play->objectCtx.status[play->objectCtx.mainKeepIndex].segment);
    gSPSegment(OVERLAY_DISP++, 0x04, play->objectCtx.status[play->objectCtx.mainKeepIndex].segment);

    gSPSegment(POLY_OPA_DISP++, 0x05, play->objectCtx.status[play->objectCtx.subKeepIndex].segment);
    gSPSegment(POLY_XLU_DISP++, 0x05, play->objectCtx.status[play->objectCtx.subKeepIndex].segment);
    gSPSegment(OVERLAY_DISP++, 0x05, play->objectCtx.status[play->objectCtx.subKeepIndex].segment);

    gSPSegment(POLY_OPA_DISP++, 0x02, play->sceneSegment);
    gSPSegment(POLY_XLU_DISP++, 0x02, play->sceneSegment);
    gSPSegment(OVERLAY_DISP++, 0x02, play->sceneSegment);

    Gfx_SetupFrame(gfxCtx, 0, 0, 0);

    if ((HREG(80) != 10) || (HREG(82) != 0)) {
        GameInteractor_ExecuteOnPlayDrawBegin();

        POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);
        POLY_XLU_DISP = Play_SetFog(play, POLY_XLU_DISP);

        func_800AA460(&play->view, play->view.fovy, play->view.zNear, play->lightCtx.fogFar);
        func_800AAA50(&play->view, 15);

        // Flip the projections and invert culling for the OPA and XLU display buffers
        // These manage the world and effects when we are not drawing kaleido
        if (R_PAUSE_MENU_MODE <= 1 && CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0)) {
            gSPSetExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
            gSPSetExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
            gSPMatrix(POLY_OPA_DISP++, play->view.projectionFlippedPtr, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
            gSPMatrix(POLY_XLU_DISP++, play->view.projectionFlippedPtr, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
            gSPMatrix(POLY_OPA_DISP++, play->view.viewingPtr, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
            gSPMatrix(POLY_XLU_DISP++, play->view.viewingPtr, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
        }

        // The billboard matrix temporarily stores the viewing matrix
        Matrix_MtxToMtxF(&play->view.viewing, &play->billboardMtxF);
        Matrix_MtxToMtxF(&play->view.projection, &play->viewProjectionMtxF);
        Matrix_Mult(&play->viewProjectionMtxF, MTXMODE_NEW);
        // The billboard is still a viewing matrix at this stage
        Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
        Matrix_Get(&play->viewProjectionMtxF);
        play->billboardMtxF.mf[0][3] = play->billboardMtxF.mf[1][3] = play->billboardMtxF.mf[2][3] =
            play->billboardMtxF.mf[3][0] = play->billboardMtxF.mf[3][1] = play->billboardMtxF.mf[3][2] = 0.0f;
        // This transpose is where the viewing matrix is properly converted into a billboard matrix
        Matrix_Transpose(&play->billboardMtxF);
        play->billboardMtx =
            Matrix_MtxFToMtx(MATRIX_CHECKFLOATS(&play->billboardMtxF), Graph_Alloc(gfxCtx, sizeof(Mtx)));

        gSPSegment(POLY_OPA_DISP++, 0x01, play->billboardMtx);

        if ((HREG(80) != 10) || (HREG(92) != 0)) {
            Gfx* gfxP;
            Gfx* sp1CC = POLY_OPA_DISP;

            gfxP = Graph_GfxPlusOne(sp1CC);
            gSPDisplayList(OVERLAY_DISP++, gfxP);
            gSPGrayscale(gfxP++, false);

            if ((play->transitionMode == TRANS_MODE_INSTANCE_RUNNING) ||
                (play->transitionMode == TRANS_MODE_INSTANCE_WAIT) ||
                (play->transitionCtx.transitionType >= TRANS_TYPE_MAX)) {
                View view;

                View_Init(&view, gfxCtx);
                view.flags = 2 | 8;

                SET_FULLSCREEN_VIEWPORT(&view);

                func_800AB9EC(&view, 15, &gfxP);
                play->transitionCtx.draw(&play->transitionCtx.data, &gfxP);
            }

            TransitionFade_Draw(&play->transitionFade, &gfxP);

            if (gVisMonoColor.a > 0) {
                gPlayVisMono.vis.primColor.rgba = gVisMonoColor.rgba;
                VisMono_Draw(&gPlayVisMono, &gfxP);
            }

            gSPEndDisplayList(gfxP++);
            Graph_BranchDlist(sp1CC, gfxP);
            POLY_OPA_DISP = gfxP;
        }

        if (gTrnsnUnkState == 3) {
            Gfx* sp88 = POLY_OPA_DISP;

            TransitionUnk_Draw(&sTrnsnUnk, &sp88);
            POLY_OPA_DISP = sp88;
            goto Play_Draw_DrawOverlayElements;
        }

        PreRender_SetValues(&play->pauseBgPreRender, SCREEN_WIDTH, SCREEN_HEIGHT, gfxCtx->curFrameBuffer, gZBuffer);

        if (R_PAUSE_MENU_MODE == 2) {
            // Wait for the previous frame's display list to be processed,
            // so that `pauseBgPreRender.fbufSave` and `pauseBgPreRender.cvgSave` are filled with the appropriate
            // content and can be used by `PreRender_ApplyFilters` below.
            MsgEvent_SendNullTask();

            PreRender_Calc(&play->pauseBgPreRender);

            R_PAUSE_MENU_MODE = 3;
        } else if (R_PAUSE_MENU_MODE >= 4) {
            R_PAUSE_MENU_MODE = 0;
        }

        if (R_PAUSE_MENU_MODE == 3) {
            Gfx* gfxP = POLY_OPA_DISP;

            // SOH [Port] Draw game framebuffer using our custom handling
            // func_800C24BC(&play->pauseBgPreRender, &gfxP);
            FB_DrawFromFramebuffer(&gfxP, gPauseFrameBuffer, 255);
            POLY_OPA_DISP = gfxP;

            goto Play_Draw_DrawOverlayElements;
        }

        if (LocalMP_DrawSplitScreen(play)) {
            GameInteractor_ExecuteOnPlayDrawEnd();
            goto Play_Draw_DrawOverlayElements;
        }

        if ((HREG(80) != 10) || (HREG(83) != 0)) {
            if (play->skyboxId && (play->skyboxId != SKYBOX_UNSET_1D) && !play->envCtx.skyboxDisabled) {
                if ((play->skyboxId == SKYBOX_NORMAL_SKY) || (play->skyboxId == SKYBOX_CUTSCENE_MAP)) {
                    Environment_UpdateSkybox(play, play->skyboxId, &play->envCtx, &play->skyboxCtx);
                    SkyboxDraw_Draw(&play->skyboxCtx, gfxCtx, play->skyboxId, play->envCtx.skyboxBlend,
                                    play->view.eye.x, play->view.eye.y, play->view.eye.z);
                } else if (play->skyboxCtx.unk_140 == 0) {
                    SkyboxDraw_Draw(&play->skyboxCtx, gfxCtx, play->skyboxId, 0, play->view.eye.x, play->view.eye.y,
                                    play->view.eye.z);
                }
            }
        }

        if ((HREG(80) != 10) || (HREG(90) & 2)) {
            if (!play->envCtx.sunMoonDisabled) {
                Environment_DrawSunAndMoon(play);
            }
        }

        if ((HREG(80) != 10) || (HREG(90) & 1)) {
            Environment_DrawSkyboxFilters(play);
        }

        if ((HREG(80) != 10) || (HREG(90) & 4)) {
            Environment_UpdateLightningStrike(play);
            Environment_DrawLightning(play, 0);
        }

        if ((HREG(80) != 10) || (HREG(90) & 8)) {
            sp228 = LightContext_NewLights(&play->lightCtx, gfxCtx);
            Lights_BindAll(sp228, play->lightCtx.listHead, NULL);
            Lights_Draw(sp228, gfxCtx);
        }

        if ((HREG(80) != 10) || (HREG(84) != 0)) {
            if (VREG(94) == 0) {
                s32 roomDrawFlags;

                if (HREG(80) != 10) {
                    roomDrawFlags = 3;
                } else {
                    roomDrawFlags = HREG(84);
                }
                Scene_Draw(play);
                if (OTRRoom_AreaPersistenceEnabled()) {
                    s32 retainedRoomCount = OTRRoom_GetRetainedRoomCount();

                    if (retainedRoomCount > 0) {
                        s32 retainedRoomIndex;

                        for (retainedRoomIndex = 0; retainedRoomIndex < retainedRoomCount; retainedRoomIndex++) {
                            Room* retainedRoom = OTRRoom_GetRetainedRoom(retainedRoomIndex);

                            if ((retainedRoom != NULL) && (retainedRoom->num >= 0)) {
                                Room_Draw(play, retainedRoom, roomDrawFlags & 3);
                            }
                        }
                    } else {
                        Room_Draw(play, &play->roomCtx.curRoom, roomDrawFlags & 3);
                        Room_Draw(play, &play->roomCtx.prevRoom, roomDrawFlags & 3);
                    }
                } else {
                    Room_Draw(play, &play->roomCtx.curRoom, roomDrawFlags & 3);
                    Room_Draw(play, &play->roomCtx.prevRoom, roomDrawFlags & 3);
                }
            }
        }

        if ((HREG(80) != 10) || (HREG(83) != 0)) {
            if ((play->skyboxCtx.unk_140 != 0) && (GET_ACTIVE_CAM(play)->setting != CAM_SET_PREREND_FIXED)) {
                Vec3f quakeOffset;

                Camera_GetSkyboxOffset(&quakeOffset, GET_ACTIVE_CAM(play));
                SkyboxDraw_Draw(&play->skyboxCtx, gfxCtx, play->skyboxId, 0, play->view.eye.x + quakeOffset.x,
                                play->view.eye.y + quakeOffset.y, play->view.eye.z + quakeOffset.z);
            }
        }

        if (play->envCtx.unk_EE[1] != 0) {
            Environment_DrawRain(play, &play->view, gfxCtx);
        }

        if ((HREG(80) != 10) || (HREG(84) != 0)) {
            Environment_FillScreen(gfxCtx, 0, 0, 0, play->unk_11E18, FILL_SCREEN_OPA);
        }

        if ((HREG(80) != 10) || (HREG(85) != 0)) {
            func_800315AC(play, &play->actorCtx);
        }

        if ((HREG(80) != 10) || (HREG(86) != 0)) {
            if (!play->envCtx.sunMoonDisabled) {
                sp21C.x = play->view.eye.x + play->envCtx.sunPos.x;
                sp21C.y = play->view.eye.y + play->envCtx.sunPos.y;
                sp21C.z = play->view.eye.z + play->envCtx.sunPos.z;
                Environment_DrawSunLensFlare(play, &play->envCtx, &play->view, gfxCtx, sp21C, 0);
            }
            Environment_DrawCustomLensFlare(play);
        }

        if ((HREG(80) != 10) || (HREG(87) != 0)) {
            if (MREG(64) != 0) {
                Environment_FillScreen(gfxCtx, MREG(65), MREG(66), MREG(67), MREG(68),
                                       FILL_SCREEN_OPA | FILL_SCREEN_XLU);
            }

            switch (play->envCtx.fillScreen) {
                case 1:
                    Environment_FillScreen(gfxCtx, play->envCtx.screenFillColor[0], play->envCtx.screenFillColor[1],
                                           play->envCtx.screenFillColor[2], play->envCtx.screenFillColor[3],
                                           FILL_SCREEN_OPA | FILL_SCREEN_XLU);
                    break;
                default:
                    break;
            }
        }

        if ((HREG(80) != 10) || (HREG(88) != 0)) {
            if (play->envCtx.sandstormState != SANDSTORM_OFF) {
                Environment_DrawSandstorm(play, play->envCtx.sandstormState);
            }
        }

        LocalMP_DrawDebugMarkers(play);

        if ((HREG(80) != 10) || (HREG(93) != 0)) {
            DebugDisplay_DrawObjects(play);
        }

        if ((R_PAUSE_MENU_MODE == 1) || (gTrnsnUnkState == 1)) {
            Gfx* gfxP = OVERLAY_DISP;

            // Copy the frame buffer contents at this point in the display list to the zbuffer
            // The zbuffer must then stay untouched until unpausing
            play->pauseBgPreRender.fbuf = gfxCtx->curFrameBuffer;
            play->pauseBgPreRender.fbufSave = (u16*)gZBuffer;
            // SOH [Port] Use our custom copy method instead of the prerender system
            // func_800C1F20(&play->pauseBgPreRender, &gfxP);
            if (R_PAUSE_MENU_MODE == 1) {
                play->pauseBgPreRender.cvgSave = (u8*)gfxCtx->curFrameBuffer;
                // func_800C20B4(&play->pauseBgPreRender, &gfxP);
                R_PAUSE_MENU_MODE = 2;

                // #region SOH [Port] Custom handling for pause prerender background capture
                lastPauseWidth = OTRGetGameRenderWidth();
                lastPauseHeight = OTRGetGameRenderHeight();
                lastAltAssets = ResourceMgr_IsAltAssetsEnabled();
                hasCapturedPauseBuffer = false;

                FB_CopyToFramebuffer(&gfxP, 0, gPauseFrameBuffer, false, &hasCapturedPauseBuffer);

                // Set the state back to ready after the recapture is done
                if (recapturePauseBuffer) {
                    R_PAUSE_MENU_MODE = 3;
                }
                // #endregion
            } else {
                gTrnsnUnkState = 2;
            }
            OVERLAY_DISP = gfxP;
            play->unk_121C7 = 2;
            SREG(33) |= 1;

            // SOH [Port] Continue to render the post world for pausing to avoid flashing the HUD
            if (gTrnsnUnkState == 2) {
                goto Play_Draw_skip;
            }
        }

        // Draw Enhancements that need to be placed in the world. This happens before the PostWorldDraw
        // so that they aren't drawn when the pause menu is up (e.g. collision viewer, actor name tags)
        GameInteractor_ExecuteOnPlayDrawEnd();

    Play_Draw_DrawOverlayElements:
        if ((HREG(80) != 10) || (HREG(89) != 0)) {
            Play_DrawOverlayElements(play);
        }

        // Reset the inverted culling
        if (CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0)) {
            gSPClearExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
            gSPClearExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
        }
    }

Play_Draw_skip:

    if (play->view.unk_124 != 0) {
        Camera_Update(GET_ACTIVE_CAM(play));
        func_800AB944(&play->view);
        play->view.unk_124 = 0;
        if (play->skyboxId && (play->skyboxId != SKYBOX_UNSET_1D) && !play->envCtx.skyboxDisabled) {
            SkyboxDraw_UpdateMatrix(&play->skyboxCtx, play->view.eye.x, play->view.eye.y, play->view.eye.z);
        }
    }

    Camera_Finish(GET_ACTIVE_CAM(play));
    LocalMP_FinishSplitScreenCameras(play);

    CLOSE_DISPS(gfxCtx);

    Interface_DrawTotalGameplayTimer(play);
}

time_t Play_GetRealTime() {
    time_t t1, t2;
    struct tm* tms;
    time(&t1);
    tms = localtime(&t1);
    tms->tm_hour = 0;
    tms->tm_min = 0;
    tms->tm_sec = 0;
    t2 = mktime(tms);
    return t1 - t2;
}

void Play_Main(GameState* thisx) {
    PlayState* play = (PlayState*)thisx;

    if (play->envCtx.unk_EE[2] == 0 && CVarGetInteger(CVAR_GENERAL("LetItSnow"), 0)) {
        play->envCtx.unk_EE[3] = 64;
        Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_OBJECT_KANKYO, 0, 0, 0, 0, 0, 0, 3);
    }

    D_8012D1F8 = &play->state.input[0];

    /* Route START from any controller into input[0] because pause/menu code
     * only checks pad 1. Also synthesize a one-frame START press from cur
     * button rising edges for input backends that miss press for START. */
    {
        u16 mergedStartCur = 0;
        u16 mergedStartPress = 0;

        for (s32 _i = 0; _i < ARRAY_COUNT(play->state.input); _i++) {
            mergedStartCur |= play->state.input[_i].cur.button & BTN_START;
            mergedStartPress |= play->state.input[_i].press.button & BTN_START;
        }

        if ((mergedStartCur & BTN_START) && !(sMergedStartPrevCur & BTN_START)) {
            mergedStartPress |= BTN_START;
        }

        play->state.input[0].cur.button |= mergedStartCur & BTN_START;
        play->state.input[0].press.button |= mergedStartPress & BTN_START;
        sMergedStartPrevCur = mergedStartCur & BTN_START;
    }

    DebugDisplay_Init();

    PLAY_LOG(4556);

    if ((HREG(80) == 10) && (HREG(94) != 10)) {
        HREG(81) = 1;
        HREG(82) = 1;
        HREG(83) = 1;
        HREG(84) = 3;
        HREG(85) = 1;
        HREG(86) = 1;
        HREG(87) = 1;
        HREG(88) = 1;
        HREG(89) = 1;
        HREG(90) = 15;
        HREG(91) = 1;
        HREG(92) = 1;
        HREG(93) = 1;
        HREG(94) = 10;
    }

    if ((HREG(80) != 10) || (HREG(81) != 0)) {
        Play_Update(play);
    }

    PLAY_LOG(4583);

    FrameInterpolation_StartRecord();
    Play_Draw(play);
    FrameInterpolation_StopRecord();

    PLAY_LOG(4587);

    if (CVarGetInteger(CVAR_CHEAT("TimeSync"), 0)) {
        const int maxRealDaySeconds = 86400;
        const int maxInGameDayTicks = 65536;

        int secs = (int)Play_GetRealTime();
        float percent = (float)secs / (float)maxRealDaySeconds;

        int newIngameTime = maxInGameDayTicks * percent;

        gSaveContext.dayTime = newIngameTime;
    }
}

u8 PlayerGrounded(Player* player) {
    return player->actor.bgCheckFlags & 1;
}

// original name: "Game_play_demo_mode_check"
s32 Play_InCsMode(PlayState* play) {
    return (play->csCtx.state != CS_STATE_IDLE) || Player_InCsMode(play);
}

f32 func_800BFCB8(PlayState* play, MtxF* mf, Vec3f* pos) {
    CollisionPoly poly;
    f32 temp1;
    f32 temp2;
    f32 temp3;
    f32 floorY = BgCheck_AnyRaycastFloor1(&play->colCtx, &poly, pos);

    if (floorY > BGCHECK_Y_MIN) {
        f32 nx = COLPOLY_GET_NORMAL(poly.normal.x);
        f32 ny = COLPOLY_GET_NORMAL(poly.normal.y);
        f32 nz = COLPOLY_GET_NORMAL(poly.normal.z);
        s32 pad[5];

        temp1 = sqrtf(1.0f - SQ(nx));

        if (temp1 != 0.0f) {
            temp2 = ny * temp1;
            temp3 = -nz * temp1;
        } else {
            temp3 = 0.0f;
            temp2 = 0.0f;
        }

        mf->xx = temp1;
        mf->yx = -nx * temp2;
        mf->zx = nx * temp3;
        mf->xy = nx;
        mf->yy = ny;
        mf->zy = nz;
        mf->yz = temp3;
        mf->zz = temp2;
        mf->wx = 0.0f;
        mf->wy = 0.0f;
        mf->xz = 0.0f;
        mf->wz = 0.0f;
        mf->xw = pos->x;
        mf->yw = floorY;
        mf->zw = pos->z;
        mf->ww = 1.0f;
    } else {
        mf->xy = 0.0f;
        mf->zx = 0.0f;
        mf->yx = 0.0f;
        mf->xx = 0.0f;
        mf->wz = 0.0f;
        mf->xz = 0.0f;
        mf->wy = 0.0f;
        mf->wx = 0.0f;
        mf->zz = 0.0f;
        mf->yz = 0.0f;
        mf->zy = 0.0f;
        mf->yy = 1.0f;
        mf->xw = pos->x;
        mf->yw = pos->y;
        mf->zw = pos->z;
        mf->ww = 1.0f;
    }

    return floorY;
}

void* Play_LoadFile(PlayState* play, RomFile* file) {
    size_t size;
    void* allocp;

    size = file->vromEnd - file->vromStart;
    allocp = GAMESTATE_ALLOC_MC(&play->state, size);
    DmaMgr_SendRequest1(allocp, file->vromStart, size, __FILE__, __LINE__);

    return allocp;
}

void Play_InitEnvironment(PlayState* play, s16 skyboxId) {
    Skybox_Init(&play->state, &play->skyboxCtx, skyboxId);
    Environment_Init(play, &play->envCtx, 0);
}

void Play_InitScene(PlayState* play, s32 spawn) {
    play->curSpawn = spawn;

    play->linkActorEntry = NULL;
    play->unk_11DFC = NULL;
    play->setupEntranceList = NULL;
    play->setupExitList = NULL;
    play->cUpElfMsgs = NULL;
    play->setupPathList = NULL;

    play->numSetupActors = 0;

    Object_InitBank(play, &play->objectCtx);
    LightContext_Init(play, &play->lightCtx);
    TransitionActor_InitContext(&play->state, &play->transiActorCtx);
    func_80096FD4(play, &play->roomCtx.curRoom);
    YREG(15) = 0;
    gSaveContext.worldMapArea = 0;
    Scene_ExecuteCommands(play, play->sceneSegment);
    Play_InitEnvironment(play, play->skyboxId);
}

void Play_SpawnScene(PlayState* play, s32 sceneId, s32 spawn) {
    uint8_t mqMode = CVarGetInteger(CVAR_GENERAL("BetterDebugWarpScreenMQMode"), WARP_MODE_OVERRIDE_OFF);
    int16_t mqModeScene = CVarGetInteger(CVAR_GENERAL("BetterDebugWarpScreenMQModeScene"), -1);
    if (mqMode != WARP_MODE_OVERRIDE_OFF && sceneId != mqModeScene) {
        CVarClear(CVAR_GENERAL("BetterDebugWarpScreenMQMode"));
        CVarClear(CVAR_GENERAL("BetterDebugWarpScreenMQModeScene"));
    }

    OTRPlay_SpawnScene(play, sceneId, spawn);
}

void func_800C016C(PlayState* play, Vec3f* src, Vec3f* dest) {
    f32 w;

    Matrix_Mult(&play->viewProjectionMtxF, MTXMODE_NEW);
    Matrix_MultVec3f(src, dest);

    w = play->viewProjectionMtxF.ww + (play->viewProjectionMtxF.wx * src->x + play->viewProjectionMtxF.wy * src->y +
                                       play->viewProjectionMtxF.wz * src->z);

    dest->x = (SCREEN_WIDTH / 2) + ((dest->x / w) * (SCREEN_WIDTH / 2));
    dest->y = (SCREEN_HEIGHT / 2) - ((dest->y / w) * (SCREEN_HEIGHT / 2));
}

s16 Play_CreateSubCamera(PlayState* play) {
    s16 i;

    for (i = SUBCAM_FIRST; i < NUM_CAMS; i++) {
        if (play->cameraPtrs[i] == NULL) {
            break;
        }
    }

    if (i == NUM_CAMS) {
        osSyncPrintf(VT_COL(RED, WHITE) "camera control: error: fulled sub camera system area\n" VT_RST);
        return SUBCAM_NONE;
    }

    osSyncPrintf("camera control: " VT_BGCOL(CYAN) " " VT_COL(WHITE, BLUE) " create new sub camera [%d] " VT_BGCOL(
                     CYAN) " " VT_RST "\n",
                 i);

    play->cameraPtrs[i] = &play->subCameras[i - SUBCAM_FIRST];
    Camera_Init(play->cameraPtrs[i], &play->view, &play->colCtx, play);
    play->cameraPtrs[i]->thisIdx = i;

    return i;
}

s16 Play_GetActiveCamId(PlayState* play) {
    return play->activeCamera;
}

s16 Play_ChangeCameraStatus(PlayState* play, s16 camId, s16 status) {
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;

    if (status == CAM_STAT_ACTIVE) {
        play->activeCamera = camIdx;
    }

    return Camera_ChangeStatus(play->cameraPtrs[camIdx], status);
}

void Play_ClearCamera(PlayState* play, s16 camId) {
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;

    if (camIdx == MAIN_CAM) {
        osSyncPrintf(VT_COL(RED, WHITE) "camera control: error: never clear camera !!\n" VT_RST);
    }

    if (play->cameraPtrs[camIdx] != NULL) {
        Camera_ChangeStatus(play->cameraPtrs[camIdx], CAM_STAT_UNK100);
        play->cameraPtrs[camIdx] = NULL;
        osSyncPrintf("camera control: " VT_BGCOL(CYAN) " " VT_COL(WHITE, BLUE) " clear sub camera [%d] " VT_BGCOL(
                         CYAN) " " VT_RST "\n",
                     camIdx);
    } else {
        osSyncPrintf(VT_COL(RED, WHITE) "camera control: error: camera No.%d already cleared\n" VT_RST, camIdx);
    }
}

void Play_ClearAllSubCameras(PlayState* play) {
    s16 i;

    for (i = SUBCAM_FIRST; i < NUM_CAMS; i++) {
        if (play->cameraPtrs[i] != NULL) {
            Play_ClearCamera(play, i);
        }
    }

    play->activeCamera = MAIN_CAM;
}

Camera* Play_GetCamera(PlayState* play, s16 camId) {
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;

    return play->cameraPtrs[camIdx];
}

s32 Play_CameraSetAtEye(PlayState* play, s16 camId, Vec3f* at, Vec3f* eye) {
    s32 ret = 0;
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;
    Camera* camera = play->cameraPtrs[camIdx];
    Player* player;

    ret |= Camera_SetParam(camera, 1, at);
    ret <<= 1;
    ret |= Camera_SetParam(camera, 2, eye);

    camera->dist = Math3D_Vec3f_DistXYZ(at, eye);

    player = camera->player;
    if (player != NULL) {
        camera->posOffset.x = at->x - player->actor.world.pos.x;
        camera->posOffset.y = at->y - player->actor.world.pos.y;
        camera->posOffset.z = at->z - player->actor.world.pos.z;
    } else {
        camera->posOffset.x = camera->posOffset.y = camera->posOffset.z = 0.0f;
    }

    camera->atLERPStepScale = 0.01f;

    return ret;
}

s32 Play_CameraSetAtEyeUp(PlayState* play, s16 camId, Vec3f* at, Vec3f* eye, Vec3f* up) {
    s32 ret = 0;
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;
    Camera* camera = play->cameraPtrs[camIdx];
    Player* player;

    ret |= Camera_SetParam(camera, 1, at);
    ret <<= 1;
    ret |= Camera_SetParam(camera, 2, eye);
    ret <<= 1;
    ret |= Camera_SetParam(camera, 4, up);

    camera->dist = Math3D_Vec3f_DistXYZ(at, eye);

    player = camera->player;
    if (player != NULL) {
        camera->posOffset.x = at->x - player->actor.world.pos.x;
        camera->posOffset.y = at->y - player->actor.world.pos.y;
        camera->posOffset.z = at->z - player->actor.world.pos.z;
    } else {
        camera->posOffset.x = camera->posOffset.y = camera->posOffset.z = 0.0f;
    }

    camera->atLERPStepScale = 0.01f;

    return ret;
}

s32 Play_CameraSetFov(PlayState* play, s16 camId, f32 fov) {
    s32 ret = Camera_SetParam(play->cameraPtrs[camId], 0x20, &fov) & 1;

    return ret;
}

s32 Play_SetCameraRoll(PlayState* play, s16 camId, s16 roll) {
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;
    Camera* camera = play->cameraPtrs[camIdx];

    camera->roll = roll;

    return 1;
}

void Play_CopyCamera(PlayState* play, s16 camId1, s16 camId2) {
    s16 camIdx2 = (camId2 == SUBCAM_ACTIVE) ? play->activeCamera : camId2;
    s16 camIdx1 = (camId1 == SUBCAM_ACTIVE) ? play->activeCamera : camId1;

    Camera_Copy(play->cameraPtrs[camIdx1], play->cameraPtrs[camIdx2]);
}

s32 func_800C0808(PlayState* play, s16 camId, Player* player, s16 setting) {
    Camera* camera;
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;

    camera = play->cameraPtrs[camIdx];
    Camera_InitPlayerSettings(camera, player);
    return Camera_ChangeSetting(camera, setting);
}

s32 Play_CameraChangeSetting(PlayState* play, s16 camId, s16 setting) {
    return Camera_ChangeSetting(Play_GetCamera(play, camId), setting);
}

void func_800C08AC(PlayState* play, s16 camId, s16 arg2) {
    s16 camIdx = (camId == SUBCAM_ACTIVE) ? play->activeCamera : camId;
    s16 i;

    Play_ClearCamera(play, camIdx);

    for (i = SUBCAM_FIRST; i < NUM_CAMS; i++) {
        if (play->cameraPtrs[i] != NULL) {
            osSyncPrintf(
                VT_COL(RED, WHITE) "camera control: error: return to main, other camera left. %d cleared!!\n" VT_RST,
                i);
            Play_ClearCamera(play, i);
        }
    }

    if (arg2 <= 0) {
        Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_ACTIVE);
        play->cameraPtrs[MAIN_CAM]->childCamIdx = play->cameraPtrs[MAIN_CAM]->parentCamIdx = SUBCAM_FREE;
    } else {
        OnePointCutscene_Init(play, 1020, arg2, NULL, MAIN_CAM);
    }
}

s16 Play_CameraGetUID(PlayState* play, s16 camId) {
    Camera* camera = play->cameraPtrs[camId];

    if (camera != NULL) {
        return camera->uid;
    } else {
        return -1;
    }
}

s16 func_800C09D8(PlayState* play, s16 camId, s16 arg2) {
    Camera* camera = play->cameraPtrs[camId];

    if (camera != NULL) {
        return 0;
    } else if (camera->uid != arg2) {
        return 0;
    } else if (camera->status != CAM_STAT_ACTIVE) {
        return 2;
    } else {
        return 1;
    }
}

void Play_SaveSceneFlags(PlayState* play) {
    SavedSceneFlags* savedSceneFlags = &gSaveContext.sceneFlags[play->sceneNum];

    savedSceneFlags->chest = play->actorCtx.flags.chest;
    savedSceneFlags->swch = play->actorCtx.flags.swch;
    savedSceneFlags->clear = play->actorCtx.flags.clear;
    savedSceneFlags->collect = play->actorCtx.flags.collect;
}

void Play_SetRespawnData(PlayState* play, s32 respawnMode, s16 entranceIndex, s32 roomIndex, s32 playerParams,
                         Vec3f* pos, s16 yaw) {
    RespawnData* respawnData = &gSaveContext.respawn[respawnMode];

    respawnData->entranceIndex = entranceIndex;
    respawnData->roomIndex = roomIndex;
    respawnData->pos = *pos;
    respawnData->yaw = yaw;
    respawnData->playerParams = playerParams;
    respawnData->tempSwchFlags = play->actorCtx.flags.tempSwch;
    respawnData->tempCollectFlags = play->actorCtx.flags.tempCollect;
}

void Play_SetupRespawnPoint(PlayState* play, s32 respawnMode, s32 playerParams) {
    Player* player = GET_PLAYER(play);
    s32 entranceIndex;
    s8 roomIndex;

    if ((play->sceneNum != SCENE_FAIRYS_FOUNTAIN) && (play->sceneNum != SCENE_GROTTOS)) {
        roomIndex = play->roomCtx.curRoom.num;
        entranceIndex = gSaveContext.entranceIndex;
        Play_SetRespawnData(play, respawnMode, entranceIndex, roomIndex, playerParams, &player->actor.world.pos,
                            player->actor.shape.rot.y);
    }
}

void Play_TriggerVoidOut(PlayState* play) {
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempSwchFlags = play->actorCtx.flags.tempSwch;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempCollectFlags = play->actorCtx.flags.tempCollect;
    gSaveContext.respawnFlag = 1;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->nextEntranceIndex = gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
}

void Play_LoadToLastEntrance(PlayState* play) {
    gSaveContext.respawnFlag = -1;
    play->transitionTrigger = TRANS_TRIGGER_START;

    if ((play->sceneNum == SCENE_GANONS_TOWER_COLLAPSE_INTERIOR) ||
        (play->sceneNum == SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR) ||
        (play->sceneNum == SCENE_INSIDE_GANONS_CASTLE_COLLAPSE) || (play->sceneNum == SCENE_GANON_BOSS)) {
        play->nextEntranceIndex = ENTR_GANONS_TOWER_COLLAPSE_EXTERIOR_0;
        Item_Give(play, ITEM_SWORD_MASTER);
    } else if ((gSaveContext.entranceIndex == ENTR_HYRULE_FIELD_11) ||
               (gSaveContext.entranceIndex == ENTR_HYRULE_FIELD_12) ||
               (gSaveContext.entranceIndex == ENTR_HYRULE_FIELD_13) ||
               (gSaveContext.entranceIndex == ENTR_HYRULE_FIELD_15)) {
        play->nextEntranceIndex = ENTR_HYRULE_FIELD_CENTER_EXIT;
    } else {
        play->nextEntranceIndex = gSaveContext.entranceIndex;
    }

    play->transitionType = TRANS_TYPE_FADE_BLACK;
}

void Play_TriggerRespawn(PlayState* play) {
    Play_SetupRespawnPoint(play, RESPAWN_MODE_DOWN, 0xDFF);
    Play_LoadToLastEntrance(play);
}

s32 func_800C0CB8(PlayState* play) {
    return (play->roomCtx.curRoom.meshHeader->base.type != 1) && (YREG(15) != 0x20) && (YREG(15) != 0x30) &&
           (YREG(15) != 0x40) && (play->sceneNum != SCENE_CASTLE_COURTYARD_GUARDS_DAY);
}

s32 FrameAdvance_IsEnabled(PlayState* play) {
    return !!play->frameAdvCtx.enabled;
}

s32 func_800C0D34(PlayState* play, Actor* actor, s16* yaw) {
    TransitionActorEntry* transitionActor;
    s32 frontRoom;

    if (actor->category != ACTORCAT_DOOR) {
        return 0;
    }

    transitionActor = &play->transiActorCtx.list[(u16)actor->params >> 10];
    frontRoom = transitionActor->sides[0].room;

    if (frontRoom == transitionActor->sides[1].room) {
        return 0;
    }

    if (frontRoom == actor->room) {
        *yaw = actor->shape.rot.y;
    } else {
        *yaw = actor->shape.rot.y + 0x8000;
    }

    return 1;
}

s32 func_800C0DB4(PlayState* play, Vec3f* pos) {
    WaterBox* waterBox;
    CollisionPoly* poly;
    Vec3f waterSurfacePos;
    s32 bgId;

    waterSurfacePos = *pos;

    if (WaterBox_GetSurface1(play, &play->colCtx, waterSurfacePos.x, waterSurfacePos.z, &waterSurfacePos.y,
                             &waterBox) == true &&
        pos->y < waterSurfacePos.y &&
        BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &waterSurfacePos) != BGCHECK_Y_MIN) {
        return true;
    } else {
        return false;
    }
}

void Play_PerformSave(PlayState* play) {
    if (play != NULL && gSaveContext.fileNum != 0xFF) {
        Play_SaveSceneFlags(play);
        gSaveContext.savedSceneNum = play->sceneNum;

        // Track values from temp B
        uint8_t prevB = gSaveContext.equips.buttonItems[0];
        uint8_t prevStatus = gSaveContext.buttonStatus[0];

        // Replicate the B button restore from minigames/epona that kaleido does
        if (gSaveContext.equips.buttonItems[0] == ITEM_SLINGSHOT || gSaveContext.equips.buttonItems[0] == ITEM_BOW ||
            gSaveContext.equips.buttonItems[0] == ITEM_BOMBCHU ||
            gSaveContext.equips.buttonItems[0] == ITEM_FISHING_POLE ||
            (gSaveContext.equips.buttonItems[0] == ITEM_NONE && !Flags_GetInfTable(INFTABLE_SWORDLESS))) {

            gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];
            Interface_RandoRestoreSwordless();
        }

        Save_SaveFile();

        // Restore temp B values back
        gSaveContext.equips.buttonItems[0] = prevB;
        gSaveContext.buttonStatus[0] = prevStatus;
    }
}
