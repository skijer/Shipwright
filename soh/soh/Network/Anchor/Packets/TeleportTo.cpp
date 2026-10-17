#include "soh/Network/Anchor/Anchor.h"
#include <nlohmann/json.hpp>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"

extern "C" {
#include "macros.h"
extern PlayState* gPlayState;
}

/**
 * TELEPORT_TO
 *
 * See REQUEST_TELEPORT for more information, this is the second part of the process.
 */

static nlohmann::json PrepTeleportTo(uint32_t clientId) {
    Player* player = GET_PLAYER(gPlayState);

    nlohmann::json payload;
    payload["type"] = Anchor::TELEPORT_TO;
    payload["targetClientId"] = clientId;
    payload["entranceIndex"] = gSaveContext.entranceIndex;
    payload["roomIndex"] = gPlayState->roomCtx.curRoom.num;
    payload["posRot"] = player->actor.world;
    return payload;
}

void Anchor::SendPacket_TeleportTo(uint32_t clientId) {
    if (!IsSaveLoaded()) {
        return;
    }

    SendJsonToRemote(PrepTeleportTo(clientId));
}

/**
 * Summon: the room owner sends an unrequested TELEPORT_TO, which only the room owner may do.
 */
void Anchor::SendPacket_Summon(uint32_t clientId) {
    if (!IsSaveLoaded()) {
        return;
    }

    nlohmann::json payload = PrepTeleportTo(clientId);
    payload["summon"] = true;
    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_TeleportTo(nlohmann::json payload) {
    if (!IsSaveLoaded()) {
        return;
    }

    const bool isSummon = payload.value("summon", false);
    if (isSummon && payload.at("clientId").get<uint32_t>() != roomState.ownerClientId) {
        return;
    }

    s32 entranceIndex = payload.at("entranceIndex").get<s32>();
    s16 roomIndex = payload.at("roomIndex").get<s16>(); // SOH [Unbound]

    if (entranceIndex < 0 || roomIndex < 0) {
        SPDLOG_ERROR("[Anchor] TELEPORT_TO: invalid entranceIndex {} or roomIndex {}", entranceIndex, (int)roomIndex);
        return;
    }

    PosRot posRot = payload.at("posRot").get<PosRot>();

    gPlayState->nextEntranceIndex = entranceIndex;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_INSTANT;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = entranceIndex;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = roomIndex;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].pos = posRot.pos;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].yaw = posRot.rot.y;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = 0xDFF;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;
    gSaveContext.respawnFlag = 1;
    static HOOK_ID hookId = 0;
    hookId = REGISTER_VB_SHOULD(VB_INFLICT_VOID_DAMAGE, {
        *should = false;
        GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnVanillaBehavior>(hookId);
    });
}
