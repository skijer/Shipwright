#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/Forms/FormRegistry.h"
#include <nlohmann/json.hpp>

extern "C" {
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace {

// Every custom item shares one item action and one item id, and vanilla clients have neither: they go by key.
s8 ToVanillaItemAction(s8 itemAction) {
    return itemAction == PLAYER_IA_CUSTOM ? PLAYER_IA_NONE : itemAction;
}

u8 ToVanillaItem(u8 item) {
    return item == ITEM_CUSTOM ? ITEM_NONE : item;
}

nlohmann::json PrepUnboundPlayerState(const Player* player) {
    nlohmann::json state = nlohmann::json::object();
    const char* formKey = FormRegistry_GetActiveKey();
    if (formKey != nullptr) {
        state["form"] = formKey;
    }
    const char* heldKey = player->heldItemAction == PLAYER_IA_CUSTOM ? CustomItemRegistry_GetHeldKey() : nullptr;
    if (heldKey != nullptr) {
        state["held"] = heldKey;
    }
    std::vector<std::string> wornKeys = CustomItemRegistry_ListWorn();
    if (!wornKeys.empty()) {
        state["worn"] = wornKeys;
    }
    return state;
}

void SetWornCustomItems(AnchorClient& client, std::vector<std::string> wornKeys) {
    if (wornKeys == client.wornCustomItemKeys) {
        return;
    }
    client.wornCustomItemKeys = std::move(wornKeys);
    client.wornCustomItemKeyPointers.clear();
    for (const std::string& key : client.wornCustomItemKeys) {
        client.wornCustomItemKeyPointers.push_back(key.c_str());
    }
}

u8 ToKnownMask(u8 mask) {
    return mask < PLAYER_MASK_MAX ? mask : PLAYER_MASK_NONE;
}

} // namespace

/**
 * PLAYER_UPDATE
 *
 * Contains real-time data necessary to update other clients in the same scene as the player
 *
 * Sent every frame to other clients within the same scene
 *
 * Note: This packet is sent _a lot_, so please do not include any unnecessary data in it
 */

void Anchor::SendPacket_PlayerUpdate() {
    if (!IsSaveLoaded()) {
        return;
    }

    uint32_t currentPlayerCount = 0;
    for (auto& [clientId, client] : clients) {
        if (client.sceneNum == gPlayState->sceneNum && client.online && client.isSaveLoaded && !client.self) {
            currentPlayerCount++;
        }
    }
    if (currentPlayerCount == 0) {
        return;
    }

    Player* player = GET_PLAYER(gPlayState);
    nlohmann::json payload;

    payload["type"] = PLAYER_UPDATE;
    payload["sceneNum"] = gPlayState->sceneNum;
    payload["entranceIndex"] = gSaveContext.entranceIndex;
    payload["linkAge"] = gSaveContext.linkAge;
    payload["posRot"]["pos"] = player->actor.world.pos;
    payload["posRot"]["rot"] = player->actor.shape.rot;
    std::vector<int> jointArray;
    for (size_t i = 0; i < 24; i++) {
        Vec3s joint = player->skelAnime.jointTable[i];
        jointArray.push_back(joint.x);
        jointArray.push_back(joint.y);
        jointArray.push_back(joint.z);
    }
    payload["prevTransl"] = player->skelAnime.prevTransl;
    payload["movementFlags"] = player->skelAnime.movementFlags;
    payload["jointTable"] = jointArray;
    payload["upperLimbRot"] = player->upperLimbRot;
    payload["currentBoots"] = player->currentBoots;
    payload["currentShield"] = player->currentShield;
    payload["currentTunic"] = player->currentTunic;
    payload["stateFlags1"] = player->stateFlags1;
    payload["stateFlags2"] = player->stateFlags2 & ~PLAYER_STATE2_DISABLE_DRAW;
    payload["buttonItem0"] = ToVanillaItem(gSaveContext.equips.buttonItems[0]);
    payload["itemAction"] = ToVanillaItemAction(player->itemAction);
    payload["heldItemAction"] = ToVanillaItemAction(player->heldItemAction);
    payload["modelGroup"] = player->modelGroup;
    payload["currentMask"] = player->currentMask;
    nlohmann::json unboundState = PrepUnboundPlayerState(player);
    if (!unboundState.empty()) {
        payload["unbound"] = unboundState;
    }
    payload["invincibilityTimer"] = player->invincibilityTimer;
    payload["unk_862"] = player->unk_862;
    payload["unk_85C"] = player->unk_85C;
    payload["actionVar1"] = player->av1.actionVar1;
    payload["quiet"] = true;

    for (auto& [clientId, client] : clients) {
        if (client.sceneNum == gPlayState->sceneNum && client.online && client.isSaveLoaded && !client.self) {
            payload["targetClientId"] = clientId;
            SendJsonToRemote(payload);
        }
    }
}

void Anchor::HandlePacket_PlayerUpdate(nlohmann::json payload) {
    uint32_t clientId = payload["clientId"].get<uint32_t>();

    if (clients.contains(clientId)) {
        auto& client = clients[clientId];

        if (client.linkAge != payload.value("linkAge", (s32)LINK_AGE_ADULT)) {
            shouldRefreshActors = true;
        }

        client.sceneNum = payload.value("sceneNum", (s16)SCENE_ID_MAX);
        client.entranceIndex = payload.value("entranceIndex", (s32)0);
        client.linkAge = payload.value("linkAge", (s32)LINK_AGE_ADULT);
        client.posRot = payload.value("posRot", PosRot{ 0 });
        std::vector<int> jointArray = payload.value("jointTable", std::vector<int>{});
        jointArray.resize(24 * 3); // Ensure it has enough elements, in case of missing data
        for (int i = 0; i < 24; i++) {
            client.jointTable[i].x = jointArray[i * 3];
            client.jointTable[i].y = jointArray[i * 3 + 1];
            client.jointTable[i].z = jointArray[i * 3 + 2];
        }
        client.movementFlags = payload.value("movementFlags", (u8)0);
        client.prevTransl = payload.value("prevTransl", Vec3s{ 0 });
        client.upperLimbRot = payload.value("upperLimbRot", Vec3s{ 0 });
        client.currentBoots = payload.value("currentBoots", (s8)0);
        client.currentShield = payload.value("currentShield", (s8)0);
        client.currentTunic = payload.value("currentTunic", (s8)0);
        client.stateFlags1 = payload.value("stateFlags1", (u32)0);
        client.stateFlags2 = payload.value("stateFlags2", (u32)0);
        client.buttonItem0 = payload.value("buttonItem0", (u8)0);
        client.itemAction = payload.value("itemAction", (s8)0);
        client.heldItemAction = payload.value("heldItemAction", (s8)0);
        client.modelGroup = payload.value("modelGroup", (u8)0);
        const nlohmann::json unboundState = payload.value("unbound", nlohmann::json::object());
        client.formKey = unboundState.value("form", std::string());
        client.heldCustomItemKey = unboundState.value("held", std::string());
        if (!client.heldCustomItemKey.empty()) {
            client.heldItemAction = PLAYER_IA_CUSTOM;
        }
        SetWornCustomItems(client, unboundState.value("worn", std::vector<std::string>{}));
        client.currentMask = ToKnownMask(payload.value("currentMask", (u8)PLAYER_MASK_NONE));
        client.invincibilityTimer = payload.value("invincibilityTimer", (s8)0);
        client.unk_862 = payload.value("unk_862", (s16)0);
        client.unk_85C = payload.value("unk_85C", (f32)0);
        client.actionVar1 = payload.value("actionVar1", (s8)0);
    }
}
