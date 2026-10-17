#include "ModNetwork.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ModApi/ModPermissions/ModPermissions.h"
#include "soh/ModApi/Timers/Timers.h"
#include "soh/Network/Anchor/Anchor.h"

extern "C" {
#include "variables.h"
}

namespace {

using Bytes = std::vector<uint8_t>;
using Clock = std::chrono::steady_clock;

constexpr const char* KindEvent = "event";
constexpr const char* KindPeerState = "peer";
constexpr const char* KindPeerClear = "peer_clear";
constexpr const char* KindRoomState = "room";
constexpr const char* KindRoomClear = "room_clear";
constexpr const char* KindCountdown = "countdown";
constexpr const char* KindCountdownStop = "countdown_stop";
constexpr const char* CountdownOwnerPrefix = "net.";
constexpr const char* Base64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Same budget as Harpoon's ROOM.BROADCAST_EVENT share, so a mod behaves the same on either relay.
constexpr double MessagesPerSecond = 10.0;
constexpr size_t MaxQueuedMessages = 64;

struct Channel {
    std::string owner;
    uint16_t version;
    SOHNetMessageFunc onMessage;
};

struct OutgoingMessage {
    nlohmann::json packet;
    std::string stateKey;
};

std::string sCurrentMod;
std::unordered_map<std::string, Channel> sChannels;
std::map<std::string, Bytes> sOwnPeerStates;
std::map<uint32_t, std::map<std::string, Bytes>> sPeerStates;
std::map<std::string, Bytes> sRoomStates;
std::set<uint32_t> sPresentPeers;
std::deque<OutgoingMessage> sOutgoing;
double sTokens = MessagesPerSecond;
Clock::time_point sLastFlush;
bool sWasConnected = false;

Anchor* Backend() {
    return Anchor::Instance;
}

bool IsConnected() {
    return Backend() != nullptr && Backend()->isEnabled && Backend()->isConnected;
}

bool IsHost() {
    return IsConnected() && Backend()->ownClientId != 0 &&
           Backend()->roomState.ownerClientId == Backend()->ownClientId;
}

std::string OwnTeamId() {
    return CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
}

std::string EncodeBase64(const Bytes& data) {
    std::string text;
    text.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        const bool hasSecond = i + 1 < data.size();
        const bool hasThird = i + 2 < data.size();
        uint32_t chunk = data[i] << 16;
        if (hasSecond) {
            chunk |= data[i + 1] << 8;
        }
        if (hasThird) {
            chunk |= data[i + 2];
        }
        text.push_back(Base64Alphabet[(chunk >> 18) & 63]);
        text.push_back(Base64Alphabet[(chunk >> 12) & 63]);
        text.push_back(hasSecond ? Base64Alphabet[(chunk >> 6) & 63] : '=');
        text.push_back(hasThird ? Base64Alphabet[chunk & 63] : '=');
    }
    return text;
}

bool DecodeBase64(const std::string& text, Bytes& data) {
    data.clear();
    uint32_t buffer = 0;
    int32_t bits = 0;
    for (char symbol : text) {
        if (symbol == '=') {
            break;
        }
        const char* found = std::strchr(Base64Alphabet, symbol);
        if (symbol == '\0' || found == nullptr) {
            return false;
        }
        buffer = (buffer << 6) | static_cast<uint32_t>(found - Base64Alphabet);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            data.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
            buffer &= (1u << bits) - 1;
        }
    }
    return data.size() <= SOH_NET_MAX_PAYLOAD;
}

bool IsValidPayload(const void* data, uint32_t size) {
    return size <= SOH_NET_MAX_PAYLOAD && (size == 0 || data != nullptr);
}

Bytes ToBytes(const void* data, uint32_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    return size == 0 ? Bytes() : Bytes(bytes, bytes + size);
}

uint32_t CopyOut(const Bytes& data, void* output, uint32_t capacity) {
    if (output != nullptr) {
        std::memcpy(output, data.data(), std::min<size_t>(data.size(), capacity));
    }
    return static_cast<uint32_t>(data.size());
}

const Channel* FindChannel(const std::string& channel) {
    auto found = sChannels.find(channel);
    return found == sChannels.end() ? nullptr : &found->second;
}

nlohmann::json BuildPacket(const std::string& channel, uint16_t version, const char* kind, const Bytes& data,
                           uint32_t to) {
    nlohmann::json packet;
    packet["type"] = Anchor::MOD_PACKET;
    packet["channel"] = channel;
    packet["ver"] = version;
    packet["kind"] = kind;
    packet["data"] = EncodeBase64(data);
    packet["quiet"] = true;
    if (to == SOH_NET_TO_TEAM) {
        packet["targetTeamId"] = OwnTeamId();
    } else if (to != SOH_NET_TO_ROOM) {
        packet["targetClientId"] = to;
    }
    return packet;
}

std::string StateKey(const char* scope, const std::string& channel, uint32_t to) {
    return std::string(scope) + ":" + channel + ":" + std::to_string(to);
}

void DropOldestEvent() {
    auto oldestEvent = std::find_if(sOutgoing.begin(), sOutgoing.end(),
                                    [](const OutgoingMessage& message) { return message.stateKey.empty(); });
    sOutgoing.erase(oldestEvent != sOutgoing.end() ? oldestEvent : sOutgoing.begin());
}

// A state still waiting in the queue is replaced in place: only its latest value is worth sending.
void Enqueue(nlohmann::json packet, std::string stateKey) {
    if (!stateKey.empty()) {
        for (OutgoingMessage& message : sOutgoing) {
            if (message.stateKey == stateKey) {
                message.packet = std::move(packet);
                return;
            }
        }
    }
    sOutgoing.push_back({ std::move(packet), std::move(stateKey) });
    if (sOutgoing.size() > MaxQueuedMessages) {
        SPDLOG_WARN("[ModNetwork] Outgoing queue is full; dropping the oldest event");
        DropOldestEvent();
    }
}

void EnqueuePeerState(const std::string& channel, const Bytes& data, uint32_t to) {
    const Channel* registered = FindChannel(channel);
    if (registered != nullptr) {
        Enqueue(BuildPacket(channel, registered->version, KindPeerState, data, to),
                StateKey(KindPeerState, channel, to));
    }
}

void EnqueueRoomState(const std::string& channel, const Bytes& data, uint32_t to) {
    const Channel* registered = FindChannel(channel);
    if (registered != nullptr) {
        Enqueue(BuildPacket(channel, registered->version, KindRoomState, data, to),
                StateKey(KindRoomState, channel, to));
    }
}

void FlushOutgoing() {
    const Clock::time_point now = Clock::now();
    const double elapsed = std::chrono::duration<double>(now - sLastFlush).count();
    sLastFlush = now;
    sTokens = std::min(MessagesPerSecond, sTokens + elapsed * MessagesPerSecond);
    while (sTokens >= 1.0 && !sOutgoing.empty()) {
        Backend()->SendJsonToRemote(sOutgoing.front().packet);
        sOutgoing.pop_front();
        sTokens -= 1.0;
    }
}

void Notify(const std::string& channel, SOHNetMessageKind kind, uint32_t from, const Bytes& data) {
    const Channel* registered = FindChannel(channel);
    if (registered == nullptr) {
        return;
    }
    const SOHNetMessage message = { sizeof(SOHNetMessage), kind, from, data.empty() ? nullptr : data.data(),
                                    static_cast<uint32_t>(data.size()) };
    registered->onMessage(&message);
}

void SendStatesTo(uint32_t clientId) {
    for (const auto& [channel, data] : sOwnPeerStates) {
        EnqueuePeerState(channel, data, clientId);
    }
    if (!IsHost()) {
        return;
    }
    for (const auto& [channel, data] : sRoomStates) {
        EnqueueRoomState(channel, data, clientId);
    }
}

void ForgetPeerStates(uint32_t clientId) {
    auto peer = sPeerStates.find(clientId);
    if (peer == sPeerStates.end()) {
        return;
    }
    const std::map<std::string, Bytes> forgotten = std::move(peer->second);
    sPeerStates.erase(peer);
    for (const auto& [channel, data] : forgotten) {
        Notify(channel, SOH_NET_PEER_STATE_CLEARED, clientId, Bytes());
    }
}

void ForgetRoom() {
    const std::set<uint32_t> peers = std::move(sPresentPeers);
    sPresentPeers.clear();
    for (uint32_t clientId : peers) {
        ForgetPeerStates(clientId);
        GameInteractor::Instance->ExecuteHooks<GameInteractor::OnNetPeerChanged>(clientId, false);
    }
    while (!sPeerStates.empty()) {
        ForgetPeerStates(sPeerStates.begin()->first);
    }
    const std::map<std::string, Bytes> forgotten = std::move(sRoomStates);
    sRoomStates.clear();
    for (const auto& [channel, data] : forgotten) {
        Notify(channel, SOH_NET_ROOM_STATE_CLEARED, 0, Bytes());
    }
    sOutgoing.clear();
}

void UpdateConnection() {
    const bool connected = IsConnected();
    if (connected != sWasConnected) {
        sWasConnected = connected;
        if (!connected) {
            ForgetRoom();
        }
        GameInteractor::Instance->ExecuteHooks<GameInteractor::OnNetConnectionChanged>(connected);
    }
    if (connected) {
        FlushOutgoing();
    }
}

std::string CountdownOwner(const std::string& owner) {
    return CountdownOwnerPrefix + owner;
}

void ApplyCountdown(const char* kind, const std::string& owner, const Bytes& data) {
    if (std::strcmp(kind, KindCountdownStop) == 0) {
        Timers_StopCountdown(CountdownOwner(owner).c_str());
        return;
    }
    if (data.size() < sizeof(uint16_t)) {
        return;
    }
    const uint16_t seconds = static_cast<uint16_t>(data[0] | (data[1] << 8));
    Timers_StartCountdown(CountdownOwner(owner).c_str(), seconds);
}

void DeliverRoomState(uint32_t from, const std::string& channel, bool isCleared, const Bytes& data) {
    if (from != Backend()->roomState.ownerClientId) {
        return;
    }
    if (isCleared) {
        sRoomStates.erase(channel);
        Notify(channel, SOH_NET_ROOM_STATE_CLEARED, from, Bytes());
        return;
    }
    sRoomStates[channel] = data;
    Notify(channel, SOH_NET_ROOM_STATE, from, data);
}

void DeliverPeerState(uint32_t from, const std::string& channel, bool isCleared, const Bytes& data) {
    if (isCleared) {
        sPeerStates[from].erase(channel);
        Notify(channel, SOH_NET_PEER_STATE_CLEARED, from, Bytes());
        return;
    }
    sPeerStates[from][channel] = data;
    Notify(channel, SOH_NET_PEER_STATE, from, data);
}

void FillPeer(const AnchorClient& client, SOHNetPeer* peer) {
    const bool isInLocalScene = gPlayState != nullptr && client.sceneNum == gPlayState->sceneNum;
    *peer = {};
    peer->structSize = sizeof(SOHNetPeer);
    peer->clientId = client.clientId;
    std::strncpy(peer->name, client.name.c_str(), sizeof(peer->name) - 1);
    peer->color = client.color;
    peer->isSelf = client.self;
    peer->isHost = client.clientId == Backend()->roomState.ownerClientId;
    peer->isOnline = client.online;
    peer->isSaveLoaded = client.isSaveLoaded;
    peer->isSameTeam = client.teamId == OwnTeamId();
    peer->sceneNum = client.sceneNum;
    peer->actor = isInLocalScene && client.player != nullptr ? &client.player->actor : nullptr;
}

bool CanFillPeer(const SOHNetPeer* peer) {
    return peer != nullptr && peer->structSize >= sizeof(SOHNetPeer) && IsConnected();
}

bool MayUseNetwork(const ModIdentity* mod) {
    if (mod == nullptr) {
        SPDLOG_ERROR("[ModNetwork] Only a mod can use the network");
        return false;
    }
    if (!ModPermissions_HasDeclared(*mod, ModPermission::Network)) {
        SPDLOG_ERROR("[ModNetwork] Mod '{}' did not declare the 'network' permission", mod->name);
        return false;
    }
    return true;
}

} // namespace

extern "C" bool ModNetwork_RegisterChannel(const char* channel, uint16_t version, SOHNetMessageFunc onMessage) {
    if (!MayUseNetwork(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()))) {
        return false;
    }
    if (channel == nullptr || channel[0] == '\0' || std::strlen(channel) >= SOH_NET_MAX_CHANNEL ||
        onMessage == nullptr) {
        SPDLOG_ERROR("[ModNetwork] Mod '{}' registered an invalid channel", sCurrentMod);
        return false;
    }
    const Channel* existing = FindChannel(channel);
    if (existing != nullptr) {
        SPDLOG_ERROR("[ModNetwork] Mod '{}' tried to register '{}', which mod '{}' already owns", sCurrentMod,
                     channel, existing->owner);
        return false;
    }
    sChannels[channel] = { sCurrentMod, version, onMessage };
    return true;
}

extern "C" bool ModNetwork_Send(const char* channel, uint32_t to, const void* data, uint32_t size) {
    const Channel* registered = channel == nullptr ? nullptr : FindChannel(channel);
    if (registered == nullptr || !IsConnected() || !IsValidPayload(data, size)) {
        return false;
    }
    Enqueue(BuildPacket(channel, registered->version, KindEvent, ToBytes(data, size), to), std::string());
    return true;
}

extern "C" bool ModNetwork_SetPeerState(const char* channel, const void* data, uint32_t size) {
    if (channel == nullptr || FindChannel(channel) == nullptr || !IsValidPayload(data, size)) {
        return false;
    }
    Bytes& state = sOwnPeerStates[channel];
    state = ToBytes(data, size);
    if (IsConnected()) {
        EnqueuePeerState(channel, state, SOH_NET_TO_ROOM);
    }
    return true;
}

extern "C" void ModNetwork_ClearPeerState(const char* channel) {
    const Channel* registered = channel == nullptr ? nullptr : FindChannel(channel);
    if (registered == nullptr || sOwnPeerStates.erase(channel) == 0 || !IsConnected()) {
        return;
    }
    Enqueue(BuildPacket(channel, registered->version, KindPeerClear, Bytes(), SOH_NET_TO_ROOM),
            StateKey(KindPeerState, channel, SOH_NET_TO_ROOM));
}

extern "C" uint32_t ModNetwork_GetPeerState(uint32_t clientId, const char* channel, void* output, uint32_t capacity) {
    if (channel == nullptr) {
        return 0;
    }
    const bool isOwnState = IsConnected() && clientId == Backend()->ownClientId;
    const std::map<std::string, Bytes>* states = nullptr;
    if (isOwnState) {
        states = &sOwnPeerStates;
    } else if (auto peer = sPeerStates.find(clientId); peer != sPeerStates.end()) {
        states = &peer->second;
    }
    if (states == nullptr) {
        return 0;
    }
    auto state = states->find(channel);
    return state == states->end() ? 0 : CopyOut(state->second, output, capacity);
}

extern "C" bool ModNetwork_SetRoomState(const char* channel, const void* data, uint32_t size) {
    if (channel == nullptr || FindChannel(channel) == nullptr || !IsValidPayload(data, size) || !IsHost()) {
        return false;
    }
    Bytes& state = sRoomStates[channel];
    state = ToBytes(data, size);
    EnqueueRoomState(channel, state, SOH_NET_TO_ROOM);
    return true;
}

extern "C" void ModNetwork_ClearRoomState(const char* channel) {
    const Channel* registered = channel == nullptr ? nullptr : FindChannel(channel);
    if (registered == nullptr || !IsHost() || sRoomStates.erase(channel) == 0) {
        return;
    }
    Enqueue(BuildPacket(channel, registered->version, KindRoomClear, Bytes(), SOH_NET_TO_ROOM),
            StateKey(KindRoomState, channel, SOH_NET_TO_ROOM));
}

extern "C" uint32_t ModNetwork_GetRoomState(const char* channel, void* output, uint32_t capacity) {
    if (channel == nullptr) {
        return 0;
    }
    auto state = sRoomStates.find(channel);
    return state == sRoomStates.end() ? 0 : CopyOut(state->second, output, capacity);
}

extern "C" bool ModNetwork_IsConnected(void) {
    return IsConnected();
}

extern "C" bool ModNetwork_IsHost(void) {
    return IsHost();
}

extern "C" uint32_t ModNetwork_GetOwnClientId(void) {
    return IsConnected() ? Backend()->ownClientId : 0;
}

extern "C" uint32_t ModNetwork_GetPeerCount(void) {
    return IsConnected() ? static_cast<uint32_t>(Backend()->clients.size()) : 0;
}

extern "C" bool ModNetwork_GetPeerAt(uint32_t index, SOHNetPeer* peer) {
    if (!CanFillPeer(peer) || index >= Backend()->clients.size()) {
        return false;
    }
    auto client = Backend()->clients.begin();
    std::advance(client, index);
    FillPeer(client->second, peer);
    return true;
}

extern "C" bool ModNetwork_FindPeer(uint32_t clientId, SOHNetPeer* peer) {
    if (!CanFillPeer(peer)) {
        return false;
    }
    auto client = Backend()->clients.find(clientId);
    if (client == Backend()->clients.end()) {
        return false;
    }
    FillPeer(client->second, peer);
    return true;
}

extern "C" uint32_t ModNetwork_GetPeerIdForActor(const Actor* actor) {
    return actor != nullptr && Backend() != nullptr ? Backend()->GetDummyPlayerClientId(actor) : 0;
}

extern "C" bool ModNetwork_RequestTeleport(uint32_t clientId) {
    if (!MayUseNetwork(ModPermissions_FindCaller(MOD_CALLER_ADDRESS())) || !IsConnected() || !Backend()->CanTeleportTo(clientId)) {
        return false;
    }
    Backend()->SendPacket_RequestTeleport(clientId);
    return true;
}

extern "C" bool ModNetwork_SummonPeer(uint32_t clientId) {
    if (!MayUseNetwork(ModPermissions_FindCaller(MOD_CALLER_ADDRESS())) || !IsHost() || !Backend()->IsSaveLoaded()) {
        return false;
    }
    auto client = Backend()->clients.find(clientId);
    if (client == Backend()->clients.end() || client->second.self || !client->second.online ||
        !client->second.isSaveLoaded) {
        return false;
    }
    Backend()->SendPacket_Summon(clientId);
    return true;
}

extern "C" bool ModNetwork_StartRoomCountdown(const char* owner, uint16_t seconds) {
    if (!MayUseNetwork(ModPermissions_FindCaller(MOD_CALLER_ADDRESS())) || owner == nullptr || owner[0] == '\0' || std::strlen(owner) >= SOH_NET_MAX_CHANNEL || !IsHost() ||
        !Timers_StartCountdown(CountdownOwner(owner).c_str(), seconds)) {
        return false;
    }
    const Bytes data = { static_cast<uint8_t>(seconds & 0xFF), static_cast<uint8_t>(seconds >> 8) };
    Enqueue(BuildPacket(owner, 0, KindCountdown, data, SOH_NET_TO_ROOM), StateKey(KindCountdown, owner, 0));
    return true;
}

extern "C" void ModNetwork_StopRoomCountdown(const char* owner) {
    if (owner == nullptr || !IsHost()) {
        return;
    }
    Timers_StopCountdown(CountdownOwner(owner).c_str());
    Enqueue(BuildPacket(owner, 0, KindCountdownStop, Bytes(), SOH_NET_TO_ROOM), StateKey(KindCountdown, owner, 0));
}

void ModNetwork_Init() {
    sLastFlush = Clock::now();
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(UpdateConnection);
}

void ModNetwork_SetCurrentMod(const std::string& name) {
    sCurrentMod = name;
}

void ModNetwork_Deliver(uint32_t from, const std::string& channel, uint16_t version, const std::string& kind,
                        const std::string& base64Data) {
    Bytes data;
    if (!DecodeBase64(base64Data, data)) {
        SPDLOG_WARN("[ModNetwork] Dropped a malformed '{}' message on '{}'", kind, channel);
        return;
    }
    if (kind == KindCountdown || kind == KindCountdownStop) {
        if (from == Backend()->roomState.ownerClientId) {
            ApplyCountdown(kind.c_str(), channel, data);
        }
        return;
    }
    const Channel* registered = FindChannel(channel);
    if (registered == nullptr || registered->version != version) {
        return;
    }
    if (kind == KindEvent) {
        Notify(channel, SOH_NET_EVENT, from, data);
    } else if (kind == KindPeerState || kind == KindPeerClear) {
        DeliverPeerState(from, channel, kind == KindPeerClear, data);
    } else if (kind == KindRoomState || kind == KindRoomClear) {
        DeliverRoomState(from, channel, kind == KindRoomClear, data);
    } else {
        SPDLOG_WARN("[ModNetwork] Unknown message kind '{}' on '{}'", kind, channel);
    }
}

void ModNetwork_OnPeerOnlineChange(uint32_t clientId, bool online) {
    const bool isKnown = sPresentPeers.contains(clientId);
    if (online == isKnown) {
        return;
    }
    if (online) {
        sPresentPeers.insert(clientId);
        SendStatesTo(clientId);
    } else {
        sPresentPeers.erase(clientId);
        ForgetPeerStates(clientId);
    }
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnNetPeerChanged>(clientId, online);
}
