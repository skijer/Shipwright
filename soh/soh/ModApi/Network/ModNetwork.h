#ifndef SOH_MOD_API_NETWORK_H
#define SOH_MOD_API_NETWORK_H

#include <stdbool.h>
#include <stdint.h>

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// Contract: soh/tools/mods-api/NETWORK.md. Travels over Anchor.
#define SOH_NET_TO_ROOM 0u
#define SOH_NET_TO_TEAM 0xFFFFFFFFu
#define SOH_NET_MAX_PAYLOAD 1024u
#define SOH_NET_MAX_CHANNEL 64u

typedef enum {
    SOH_NET_EVENT,
    SOH_NET_PEER_STATE,
    SOH_NET_PEER_STATE_CLEARED,
    SOH_NET_ROOM_STATE,
    SOH_NET_ROOM_STATE_CLEARED,
} SOHNetMessageKind;

typedef struct {
    uint32_t structSize;
    SOHNetMessageKind kind;
    uint32_t from;
    const void* data;
    uint32_t size;
} SOHNetMessage;

// Runs on the game thread; `data` is only valid during the call.
typedef void (*SOHNetMessageFunc)(const SOHNetMessage* message);

typedef struct {
    uint32_t structSize;
    uint32_t clientId;
    char name[32];
    Color_RGB8 color;
    bool isSelf;
    bool isHost;
    bool isOnline;
    bool isSaveLoaded;
    bool isSameTeam;
    int16_t sceneNum;
    // The peer's dummy in the local scene, NULL when it is elsewhere. Re-read every frame.
    Actor* actor;
} SOHNetPeer;

bool ModNetwork_RegisterChannel(const char* channel, uint16_t version, SOHNetMessageFunc onMessage);
bool ModNetwork_Send(const char* channel, uint32_t to, const void* data, uint32_t size);

bool ModNetwork_SetPeerState(const char* channel, const void* data, uint32_t size);
void ModNetwork_ClearPeerState(const char* channel);
uint32_t ModNetwork_GetPeerState(uint32_t clientId, const char* channel, void* output, uint32_t capacity);

bool ModNetwork_SetRoomState(const char* channel, const void* data, uint32_t size);
void ModNetwork_ClearRoomState(const char* channel);
uint32_t ModNetwork_GetRoomState(const char* channel, void* output, uint32_t capacity);

bool ModNetwork_IsConnected(void);
bool ModNetwork_IsHost(void);
uint32_t ModNetwork_GetOwnClientId(void);
uint32_t ModNetwork_GetPeerCount(void);
bool ModNetwork_GetPeerAt(uint32_t index, SOHNetPeer* peer);
bool ModNetwork_FindPeer(uint32_t clientId, SOHNetPeer* peer);
uint32_t ModNetwork_GetPeerIdForActor(const Actor* actor);

bool ModNetwork_RequestTeleport(uint32_t clientId);
bool ModNetwork_SummonPeer(uint32_t clientId);
bool ModNetwork_StartRoomCountdown(const char* owner, uint16_t seconds);
void ModNetwork_StopRoomCountdown(const char* owner);

#ifdef __cplusplus
}

#include <string>

void ModNetwork_Init();
void ModNetwork_SetCurrentMod(const std::string& name);
void ModNetwork_Deliver(uint32_t from, const std::string& channel, uint16_t version, const std::string& kind,
                        const std::string& base64Data);
void ModNetwork_OnPeerOnlineChange(uint32_t clientId, bool online);
#endif

#endif
