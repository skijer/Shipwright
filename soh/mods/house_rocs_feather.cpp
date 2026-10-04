// A freestanding NEI feather on Link's House table. Its existing NEI ownership
// is the collection flag; no scene resource, chest or collectible flag is used.
#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "mods/extended_inventory.h"
extern PlayState* gPlayState;
}

namespace {

// Native link_home_sceneCollisionHeader_000E4C polygons 116..123 form the
// tabletop at (0,21,0), radius 20. link_home_room_0Vtx_000F10 agrees. The NEI
// feather's mesh Y bounds are -91..82; actor scale .2 times its custom draw's
// .5 keeps the lowest vertex at 22.4, just above the unchanged tabletop.
constexpr Vec3f kFeatherPosition = { 0.0f, 31.5f, 0.0f };
constexpr float kFeatherScale = 0.2f;
constexpr float kPickupXZRange = 28.0f;
constexpr float kPickupYRange = 50.0f;

struct HouseRocsFeather {
    Actor actor;
    GetItemEntry entry;
    bool handoffStarted;
};

s16 sActorId = -1;

bool NeedsFeather() {
    // RG_PROGRESSIVE_ROCS would grant a cape on its second receipt. Only an
    // empty slot may collect this placement, including after save/load.
    return ExtInv_GetSlotItem(SLOT_ROCS) == ITEM_NONE;
}

void FeatherInit(Actor* actor, PlayState*) {
    if (!NeedsFeather()) {
        Actor_Kill(actor);
        return;
    }
    auto* feather = reinterpret_cast<HouseRocsFeather*>(actor);
    feather->entry = Rando::StaticData::RetrieveItem(RG_PROGRESSIVE_ROCS).GetGIEntry_Copy();
    feather->entry.getItemFrom = ITEM_FROM_FREESTANDING;
    feather->handoffStarted = false;
    Actor_SetScale(actor, kFeatherScale);
}

void FeatherUpdate(Actor* actor, PlayState* play) {
    auto* feather = reinterpret_cast<HouseRocsFeather*>(actor);
    if (!NeedsFeather()) {
        Actor_Kill(actor);
        return;
    }
    if (Actor_HasParent(actor, play)) {
        // Acceptance precedes the native overhead animation's inventory write.
        // Keep a hidden actor until that write, so a repeated scene hook cannot
        // create another offer during the handoff. Never grant directly here.
        feather->handoffStarted = true;
        actor->draw = nullptr;
    }
    if (feather->handoffStarted) {
        return;
    }

    Player* player = GET_PLAYER(play);
    if (player == nullptr || play->pauseCtx.state != 0 || play->pauseCtx.debugState != 0 ||
        play->gameOverCtx.state != GAMEOVER_INACTIVE || play->csCtx.state != CS_STATE_IDLE ||
        play->transitionTrigger != TRANS_TRIGGER_OFF || Player_InCsMode(play) ||
        (player->stateFlags1 & (PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM)) ||
        Message_GetState(&play->msgCtx) != TEXT_STATE_NONE) {
        return;
    }
    GiveItemEntryFromActor(actor, play, feather->entry, kPickupXZRange, kPickupYRange);
}

void FeatherDraw(Actor* actor, PlayState* play) {
    auto* feather = reinterpret_cast<HouseRocsFeather*>(actor);
    func_8002EBCC(actor, play, 0);
    func_8002ED80(actor, play, 0);
    GetItemEntry_Draw(play, feather->entry);
}

void SpawnHouseFeather() {
    PlayState* play = gPlayState;
    if (play == nullptr || play->sceneNum != SCENE_LINKS_HOUSE || !NeedsFeather() || ActorDB::Instance == nullptr) {
        return;
    }
    if (sActorId < 0) {
        ActorDBInit init;
        init.name = "LinkHouseRocsFeather";
        init.desc = "NEI Roc's Feather on Link's House table";
        init.category = ACTORCAT_MISC;
        init.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
        init.objectId = OBJECT_GAMEPLAY_KEEP;
        init.instanceSize = sizeof(HouseRocsFeather);
        init.init = FeatherInit;
        init.update = FeatherUpdate;
        init.draw = FeatherDraw;
        sActorId = ActorDB::Instance->AddEntry(init).entry.id;
    }
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != nullptr; actor = actor->next) {
        if (actor->id == sActorId && actor->update != nullptr) {
            return;
        }
    }
    Actor_Spawn(&play->actorCtx, play, sActorId, kFeatherPosition.x, kFeatherPosition.y, kFeatherPosition.z, 0, 0, 0,
                0);
}

void Register() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneSpawnActors>(SpawnHouseFeather);
}

static RegisterShipInitFunc sHouseFeatherInit(Register);

} // namespace
