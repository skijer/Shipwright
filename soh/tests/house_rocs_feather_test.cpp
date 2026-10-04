// Exercise the production actor and native item-offer boundary. Rendering,
// actor allocation, item metadata and hook dispatch are engine boundaries.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "mods/extended_inventory.h"
#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"
#include "test_require.h"

static std::function<void()> sceneHook;
class GameInteractor {
  public:
    struct OnSceneSpawnActors {
        using fn = std::function<void()>;
    };
    static GameInteractor* Instance;
    template <typename H> uint32_t RegisterGameHook(typename H::fn callback) {
        sceneHook = callback;
        return 1;
    }
};
static GameInteractor interactor;
GameInteractor* GameInteractor::Instance = &interactor;

static NeiSaveData gNeiSave;
static PlayState play;
static Player player;
static std::vector<Actor*> allocations;
static int spawnCalls, registrations, drawCalls;
static bool failSpawn;
static ActorDB::Entry actorEntry;
ActorDB* ActorDB::Instance;
ActorDB::ActorDB() = default;
ActorDB::Entry::Entry() {
    entry = {};
}
ActorDB::Entry& ActorDB::AddEntry(const ActorDBInit& init) {
    registrations++;
    actorEntry.entry.id = 0x4000;
    actorEntry.entry.category = init.category;
    actorEntry.entry.flags = init.flags;
    actorEntry.entry.objectId = init.objectId;
    actorEntry.entry.instanceSize = init.instanceSize;
    actorEntry.entry.init = init.init;
    actorEntry.entry.destroy = init.destroy;
    actorEntry.entry.update = init.update;
    actorEntry.entry.draw = init.draw;
    return actorEntry;
}

extern "C" {
SaveContext gSaveContext;
PlayState* gPlayState;
float iceTrapScale;
void GameInteractor_ExecuteOnActorKill(Actor*) {
}
s32 Player_GetExplosiveHeld(Player*) {
    return -1;
}
s32 Player_ActionToMagicSpell(Player*, s32) {
    return -1;
}
uint8_t ExtInv_GetItemSlot(uint16_t item) {
    // Item-registry boundary: both existing Roc tiers share slot 24.
    REQUIRE(item == ITEM_ROCS_FEATHER_SKIJER || item == ITEM_ROCS_CAPE);
    return 24;
}
u8 Message_GetState(MessageContext* context) {
    return context->msgMode == MSGMODE_NONE ? TEXT_STATE_NONE : TEXT_STATE_DONE;
}
void func_8002EBCC(Actor*, PlayState*, s32) {
}
void func_8002ED80(Actor*, PlayState*, s32) {
}
void GetItem_Draw(PlayState*, s16) {
    REQUIRE(false);
}
// This fixture supplies its own unrelated FeatherDraw callback. The real NEI
// renderer's bindings/fallback are exercised by tests/nei_gi/presentation_test.cpp.
bool NeiGi_Draw(PlayState*, GetItemEntry*) {
    return false;
}
Actor* Actor_Spawn(ActorContext* context, PlayState* state, s16 id, f32 x, f32 y, f32 z, s16 rotX, s16 rotY, s16 rotZ,
                   s16 params) {
    spawnCalls++;
    REQUIRE(id == actorEntry.entry.id);
    REQUIRE(id != ACTOR_EN_ITEM00 && id != ACTOR_EN_BOX);
    REQUIRE(actorEntry.entry.objectId == OBJECT_GAMEPLAY_KEEP);
    if (failSpawn)
        return nullptr;
    Actor* actor = static_cast<Actor*>(std::calloc(1, actorEntry.entry.instanceSize));
    REQUIRE(actor != nullptr);
    actor->id = id;
    actor->category = actorEntry.entry.category;
    actor->flags = actorEntry.entry.flags;
    actor->update = actorEntry.entry.update;
    actor->draw = actorEntry.entry.draw;
    actor->world.pos = { x, y, z };
    actor->home.pos = actor->world.pos;
    actor->shape.rot = { rotX, rotY, rotZ };
    actor->params = params;
    actor->next = context->actorLists[actor->category].head;
    context->actorLists[actor->category].head = actor;
    allocations.push_back(actor);
    actorEntry.entry.init(actor, state);
    return actor;
}
}

static void FeatherDraw(PlayState* state, GetItemEntry* entry) {
    REQUIRE(state == &play && entry->itemId == RG_PROGRESSIVE_ROCS);
    drawCalls++;
}
namespace Rando {
struct FixtureItem {
    GetItemEntry GetGIEntry_Copy() const {
        GetItemEntry entry = GET_ITEM(RG_PROGRESSIVE_ROCS, OBJECT_GI_BOMB_2, GID_RUPEE_BLUE, 0xF8, 0x80,
                                      CHEST_ANIM_LONG, ITEM_CATEGORY_MAJOR, MOD_RANDOMIZER, RG_PROGRESSIVE_ROCS);
        entry.drawFunc = FeatherDraw;
        return entry;
    }
};
struct StaticData {
    static FixtureItem& RetrieveItem(RandomizerGet id) {
        REQUIRE(id == RG_PROGRESSIVE_ROCS);
        static FixtureItem item;
        return item;
    }
};
} // namespace Rando

// Native offer/range/parent logic and NEI accessors, copied by the runner.
#include "house_rocs_feather_native.inc"
// Complete new production translation unit, with only its includes omitted.
#include "house_rocs_feather_production.inc"

static void Reset(s16 scene = SCENE_LINKS_HOUSE, uint16_t item = ITEM_NONE) {
    for (Actor* actor : allocations)
        std::free(actor);
    allocations.clear();
    play = {};
    player = {};
    gSaveContext = {};
    gNeiSave = {};
    std::fill(std::begin(gNeiSave.ownedItems), std::end(gNeiSave.ownedItems), ITEM_NONE);
    Nei_SetOwnedItem(24, item);
    gPlayState = &play;
    play.sceneNum = scene;
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    player.getItemEntry = GET_ITEM_NONE;
    player.actor.world.pos = { 0.0f, 0.0f, 100.0f };
    spawnCalls = drawCalls = 0;
    failSpawn = false;
}

static Actor* Spawn() {
    sceneHook();
    REQUIRE(allocations.size() == 1);
    return allocations.back();
}

static void Tick(Actor* actor, float x = 0.0f, float y = 0.0f, float z = 0.0f) {
    player.actor.world.pos = { x, y, z };
    actor->xzDistToPlayer = std::hypot(x - actor->world.pos.x, z - actor->world.pos.z);
    actor->yDistToPlayer = y - actor->world.pos.y;
    if (actor->update != nullptr)
        actor->update(actor, &play);
}

static void ClearOffer() {
    player.getItemId = GI_NONE;
    player.getItemEntry = GET_ITEM_NONE;
    player.interactRangeActor = nullptr;
}

int main() {
    static ActorDB database;
    ActorDB::Instance = &database;
    ShipInit::InitAll();
    REQUIRE(sceneHook && "Link's House collectible hook must be registered");

    Reset(SCENE_KOKIRI_FOREST);
    sceneHook();
    REQUIRE(spawnCalls == 0);
    gPlayState = nullptr;
    sceneHook();
    REQUIRE(spawnCalls == 0);
    for (uint16_t owned : { ITEM_ROCS_FEATHER_SKIJER, ITEM_ROCS_CAPE }) {
        Reset(SCENE_LINKS_HOUSE, owned);
        sceneHook();
        REQUIRE(spawnCalls == 0 && Nei_GetOwnedItem(24) == owned);
    }

    Reset();
    Actor* actor = Spawn();
    REQUIRE(actor->world.pos.x == 0.0f && actor->world.pos.z == 0.0f);
    // Tabletop is Y21, mesh minimum is -91 and NEI draw applies another 0.5.
    REQUIRE(actor->world.pos.y - 91.0f * actor->scale.y * 0.5f > 21.0f);
    REQUIRE(actor->world.pos.y + 82.0f * actor->scale.y * 0.5f < 50.0f);
    REQUIRE(actor->draw != nullptr);
    actor->draw(actor, &play);
    REQUIRE(drawCalls == 1);
    sceneHook();
    REQUIRE(spawnCalls == 1 && registrations == 1);
    REQUIRE(Nei_GetOwnedItem(24) == ITEM_NONE);

    Tick(actor, 80.0f);
    REQUIRE(player.getItemId == GI_NONE);
    Tick(actor, 0.0f, 150.0f);
    REQUIRE(player.getItemId == GI_NONE);
    Tick(actor, 20.0f);
    REQUIRE(player.getItemId == RG_PROGRESSIVE_ROCS);
    REQUIRE(player.getItemEntry.modIndex == MOD_RANDOMIZER);
    REQUIRE(player.getItemEntry.itemId == RG_PROGRESSIVE_ROCS);
    REQUIRE(player.getItemEntry.getItemFrom == ITEM_FROM_FREESTANDING);
    REQUIRE(player.getItemEntry.collectable && player.getItemEntry.gi > 0);
    REQUIRE(player.interactRangeActor == actor && actor->parent == nullptr);
    REQUIRE(Nei_GetOwnedItem(24) == ITEM_NONE); // Offer must not grant directly.
    REQUIRE((player.stateFlags1 & PLAYER_STATE1_GETTING_ITEM) == 0);
    ClearOffer();

    for (int guard = 0; guard < 8; ++guard) {
        play.pauseCtx.state = guard == 0;
        play.pauseCtx.debugState = guard == 1;
        play.csCtx.state = guard == 2 ? 1 : CS_STATE_IDLE;
        play.transitionTrigger = guard == 3 ? TRANS_TRIGGER_START : TRANS_TRIGGER_OFF;
        play.gameOverCtx.state = guard == 4 ? GAMEOVER_DEATH_START : GAMEOVER_INACTIVE;
        play.msgCtx.msgMode = guard == 5 ? MSGMODE_TEXT_DISPLAYING : MSGMODE_NONE;
        player.csAction = guard == 6 ? 1 : 0;
        player.stateFlags1 = guard == 7 ? PLAYER_STATE1_IN_CUTSCENE : 0;
        Tick(actor);
        REQUIRE(player.getItemId == GI_NONE && Nei_GetOwnedItem(24) == ITEM_NONE);
    }
    Reset();
    actor = Spawn();
    player.stateFlags1 = PLAYER_STATE1_TALKING;
    Tick(actor);
    REQUIRE(player.getItemId == GI_NONE);
    player.stateFlags1 = 0;
    Tick(actor);
    REQUIRE(player.getItemId == RG_PROGRESSIVE_ROCS);

    // The player's native handler accepts on a later frame. Acceptance is not
    // ownership: keep one hidden actor until the native receipt populates NEI.
    actor->parent = &player.actor;
    player.stateFlags1 = PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_CUTSCENE;
    Tick(actor);
    REQUIRE(actor->draw == nullptr && actor->update != nullptr);
    REQUIRE(Nei_GetOwnedItem(24) == ITEM_NONE);
    sceneHook();
    REQUIRE(spawnCalls == 1);
    actor->parent = nullptr;
    player.stateFlags1 = 0;
    ClearOffer();
    Tick(actor);
    REQUIRE(player.getItemId == GI_NONE && actor->draw == nullptr);

    // A normal native receipt of the table's queued entry fills the NEI slot.
    Fixture_ReceiveRandomizedCheck(Rando::StaticData::RetrieveItem(RG_PROGRESSIVE_ROCS).GetGIEntry_Copy());
    Tick(actor);
    REQUIRE(actor->update == nullptr && actor->draw == nullptr);
    REQUIRE(Nei_GetOwnedItem(24) == ITEM_ROCS_FEATHER_SKIJER);
    NeiSaveData saved = gNeiSave;
    Reset(); // A new actor context cannot serve as the persistence flag.
    gNeiSave = saved;
    sceneHook();
    REQUIRE(spawnCalls == 0);

    // The table's ownership guard is local. Receiving a normal randomized
    // check after this fixed pickup must still perform the progressive upgrade.
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    GetItemEntry check = Rando::StaticData::RetrieveItem(RG_PROGRESSIVE_ROCS).GetGIEntry_Copy();
    Fixture_ReceiveRandomizedCheck(check);
    REQUIRE(Nei_GetOwnedItem(24) == ITEM_ROCS_CAPE);
    sceneHook();
    REQUIRE(spawnCalls == 0);

    // A check can also be the first source. Its feather suppresses only the
    // extra table actor, while the next ordinary check still grants the cape.
    Reset();
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    actor = Spawn();
    Fixture_ReceiveRandomizedCheck(check);
    REQUIRE(Nei_GetOwnedItem(24) == ITEM_ROCS_FEATHER_SKIJER);
    Tick(actor);
    REQUIRE(actor->update == nullptr && player.getItemId == GI_NONE);
    Fixture_ReceiveRandomizedCheck(check);
    REQUIRE(Nei_GetOwnedItem(24) == ITEM_ROCS_CAPE);

    // Acquiring feather/cape elsewhere while the display is waiting ends it.
    for (uint16_t owned : { ITEM_ROCS_FEATHER_SKIJER, ITEM_ROCS_CAPE }) {
        Reset();
        actor = Spawn();
        Nei_SetOwnedItem(24, owned);
        Tick(actor);
        REQUIRE(actor->update == nullptr && player.getItemId == GI_NONE);
        REQUIRE(Nei_GetOwnedItem(24) == owned);
    }
    Reset();
    failSpawn = true;
    sceneHook();
    REQUIRE(allocations.empty());
    failSpawn = false;
    actor = Spawn();
    REQUIRE(actor->draw != nullptr && registrations == 1);
    Reset(SCENE_KOKIRI_FOREST);
    sceneHook();
    REQUIRE(allocations.empty());
    Reset();
    Spawn();
    REQUIRE(registrations == 1);
    Reset();
    std::puts("PASS: house feather scene/ownership/placement/draw/offer/guards/handoff/reentry/rando checks");
}
