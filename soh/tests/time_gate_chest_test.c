#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "tests/test_require.h"
#include "src/overlays/actors/ovl_Bg_Toki_Swd/z_bg_toki_swd.h"
#include "src/overlays/actors/ovl_En_Box/z_en_box.h"
#include "src/overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h"
#include "objects/object_box/object_box.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "mods/extended_inventory.h"
#include "vt.h"

SaveContext gSaveContext;
f32 iceTrapScale;
f32 gSfxDefaultFreqAndVolScale;
s8 gSfxDefaultReverb;
Vec3f gSfxDefaultPos;
static int randoLookups, treasureEvents;
static bool knownRandoChest;
static int customGives, vanillaGives, vanillaFallbacks;
static u16 receiptText;
static GetItemEntry receivedEntry;
static u16 ownedTimeGate;

uint16_t Nei_GetOwnedItem(uint8_t slot) {
    return slot == SLOT_TIME_GATE ? ownedTimeGate : ITEM_NONE;
}
s32 Flags_GetItemGetInf(s32 flag) {
    return 0;
}
s32 Flags_GetInfTable(s32 flag) {
    return 0;
}
u8 Bottle_HasFreeSlot(void) {
    return 0;
}

void EnBox_Open(EnBox* box, PlayState* play) {
}
void EnBox_FallOnSwitchFlag(EnBox* box, PlayState* play) {
}
void EnBox_AppearOnRoomClear(EnBox* box, PlayState* play) {
}
void func_809C9700(EnBox* box, PlayState* play) {
}
void EnBox_UpdateTexture(EnBox* box, PlayState* play) {
}
void Actor_ProcessInitChain(Actor* actor, InitChainEntry* entries) {
}
void DynaPolyActor_Init(DynaPolyActor* actor, s32 flags) {
}
void CollisionHeader_GetVirtual(void* source, CollisionHeader** dest) {
    *dest = NULL;
}
s32 DynaPoly_SetBgActor(PlayState* play, DynaCollisionContext* ctx, Actor* actor, CollisionHeader* header) {
    return 0;
}
void func_8003ECA8(PlayState* play, DynaCollisionContext* ctx, s32 id) {
}
void func_8003EBF8(PlayState* play, DynaCollisionContext* ctx, s32 id) {
}
void func_8003EC50(PlayState* play, DynaCollisionContext* ctx, s32 id) {
}
s16 Animation_GetLastFrame(void* animation) {
    return 100;
}
s32 SkelAnime_Init(PlayState* play, SkelAnime* skel, SkeletonHeader* header, AnimationHeader* anim, Vec3s* joints,
                   Vec3s* morphs, s32 count) {
    return 0;
}
void Animation_Change(SkelAnime* skel, AnimationHeader* anim, f32 speed, f32 start, f32 end, u8 mode, f32 morph) {
    skel->curFrame = start;
}
f32 Rand_ZeroOne(void) {
    return 0.75f;
}
s32 Flags_GetClear(PlayState* play, s32 flag) {
    return 0;
}
s32 Flags_GetSwitch(PlayState* play, s32 flag) {
    return flag < 32 ? (play->actorCtx.flags.swch & (1u << flag))
                     : (play->actorCtx.flags.tempSwch & (1u << (flag - 32)));
}
void Actor_SetClosestSecretDistance(Actor* actor, PlayState* play) {
}
s32 OnePointCutscene_Attention(PlayState* play, Actor* actor) {
    return 0;
}
s32 func_8005B198(void) {
    return ACTORCAT_CHEST;
}
Actor* Actor_Spawn(ActorContext* ctx, PlayState* play, s16 id, f32 x, f32 y, f32 z, s16 rx, s16 ry, s16 rz,
                   s16 params) {
    return NULL;
}
Actor* Actor_SpawnAsChild(ActorContext* ctx, Actor* parent, PlayState* play, s16 id, f32 x, f32 y, f32 z, s16 rx,
                          s16 ry, s16 rz, s16 params) {
    return NULL;
}
void Audio_PlayFanfare(u16 id) {
}
void Audio_PlayFanfare_Rando(GetItemEntry entry) {
}
void Audio_PlaySoundGeneral(u16 id, Vec3f* pos, u8 token, f32* frequency, f32* volume, s8* reverb) {
}
void Actor_WorldToActorCoords(Actor* actor, Vec3f* result, Vec3f* position) {
    *result = (Vec3f){ 0, 0, -20 };
}
s32 Player_IsFacingActor(Actor* actor, s16 angle, PlayState* play) {
    return 1;
}
s32 Player_GetExplosiveHeld(Player* player) {
    return -1;
}
u8 MmForm_IsZoraSwimming(Player* player) {
    return 0;
}
void Player_Action_8084E6D4(Player* player, PlayState* play) {
}
void Player_SetupActionPreserveAnimMovement(PlayState* play, Player* player, PlayerActionFunc action, s32 flags) {
}
void Player_SetPendingFlag(Player* player, PlayState* play) {
}
void Message_StartTextbox(PlayState* play, u16 text, Actor* actor) {
    receiptText = text;
}
int32_t CVarGetInteger(const char* name, int32_t fallback) {
    return fallback;
}
u8 Item_Give(PlayState* play, u8 item) {
    ++vanillaGives;
    return item;
}
u16 Randomizer_Item_Give(PlayState* play, GetItemEntry entry) {
    receivedEntry = entry;
    ++customGives;
    return RG_NONE;
}
void GameInteractor_ExecuteOnSceneFlagSet(s16 scene, s16 type, s16 flag) {
    ++treasureEvents;
}
void osSyncPrintfUnused(const char* fmt, ...) {
}
void lusprintf(const char* file, int32_t line, int32_t level, const char* format, ...) {
}

GetItemEntry ItemTable_RetrieveEntry(s16 table, s16 item) {
    if (table == MOD_RANDOMIZER) {
        REQUIRE(item == RG_TIME_GATE);
        return (GetItemEntry)GET_ITEM(ITEM_TIME_GATE, OBJECT_GI_MEDAL, 0, 0xF8, 0x80, CHEST_ANIM_LONG,
                                      ITEM_CATEGORY_MAJOR, MOD_RANDOMIZER, RG_TIME_GATE);
    }
    REQUIRE(table == MOD_NONE);
    GetItemEntry entry = GET_ITEM(ITEM_HEART_CONTAINER, OBJECT_GI_HEARTS, 0, 0, 0x80, CHEST_ANIM_LONG,
                                  ITEM_CATEGORY_HEALTH, MOD_NONE, GI_HEART_CONTAINER);
    entry.getItemId = item;
    return entry;
}
RandomizerCheck Randomizer_GetCheckFromActor(s16 id, s16 scene, s16 params) {
    ++randoLookups;
    return knownRandoChest ? RC_KF_KOKIRI_SWORD_CHEST : RC_UNKNOWN_CHECK;
}
GetItemEntry Randomizer_GetItemFromKnownCheck(RandomizerCheck check, GetItemID original) {
    return (GetItemEntry)GET_ITEM(ITEM_BOW, OBJECT_GI_BOW, 0, 0, 0x80, CHEST_ANIM_LONG, ITEM_CATEGORY_MAJOR,
                                  MOD_RANDOMIZER, RG_PROGRESSIVE_BOW);
}
GetItemEntry ItemTable_Retrieve(s16 item) {
    ++vanillaFallbacks;
    return ItemTable_RetrieveEntry(MOD_NONE, item);
}

#include "time_gate_chest_production.inc"

static EnBox AuthoredChest(void) {
    EnBox chest = { 0 };
    chest.dyna.actor.id = ACTOR_EN_BOX;
    chest.dyna.actor.params = (s16)0xB7A5;
    chest.dyna.actor.room = 0;
    chest.dyna.actor.category = ACTORCAT_CHEST;
    chest.dyna.actor.home.pos = (Vec3f){ -1582, 220, 1961 };
    chest.dyna.actor.world.pos = chest.dyna.actor.home.pos;
    chest.dyna.actor.home.rot = (Vec3s){ 4316, 21604, 32 };
    chest.dyna.actor.world.rot = chest.dyna.actor.home.rot;
    chest.dyna.actor.xzDistToPlayer = 20;
    return chest;
}

static void Reset(PlayState* play, Player* player, bool randomizer) {
    memset(play, 0, sizeof(*play));
    memset(player, 0, sizeof(*player));
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    gSaveContext.linkAge = LINK_AGE_ADULT;
    gSaveContext.ship.quest.id = randomizer ? QUEST_RANDOMIZER : QUEST_NORMAL;
    play->sceneNum = SCENE_HYRULE_FIELD;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].head = &player->actor;
    randoLookups = treasureEvents = 0;
    customGives = vanillaGives = vanillaFallbacks = 0;
    receiptText = 0;
    ownedTimeGate = ITEM_NONE;
    knownRandoChest = false;
}

int main(void) {
    PlayState* play = calloc(1, sizeof(*play));
    Player* player = calloc(1, sizeof(*player));
    REQUIRE(play && player);
    for (int randomizer = 0; randomizer < 2; ++randomizer) {
        Reset(play, player, randomizer);
        // Native chest animation asks this before receipt. Extended item IDs
        // must never index the vanilla 56-slot / 18-extra-item tables.
        REQUIRE(Item_CheckObtainability(ITEM_TIME_GATE) == ITEM_NONE);
        ownedTimeGate = ITEM_TIME_GATE;
        REQUIRE(Item_CheckObtainability(ITEM_TIME_GATE) == ITEM_TIME_GATE);
        ownedTimeGate = ITEM_NONE;
        REQUIRE(Item_CheckObtainability(ITEM_BOW) == ITEM_NONE);
        gSaveContext.inventory.items[SLOT_STICK] = ITEM_STICK;
        REQUIRE(Item_CheckObtainability(ITEM_STICK) == ITEM_STICK);
        EnBox chest = AuthoredChest();
        EnBox_Init(&chest.dyna.actor, play);
        REQUIRE(chest.getItemEntry.itemId == ITEM_TIME_GATE);
        REQUIRE(chest.getItemEntry.modIndex == MOD_RANDOMIZER);
        REQUIRE(chest.getItemEntry.getItemId == RG_TIME_GATE);
        REQUIRE(randoLookups == 0);
        REQUIRE(chest.actionFunc == EnBox_AppearOnSwitchFlag);
        REQUIRE(chest.switchFlag == 32 && chest.dyna.actor.params == (s16)0xB7A5);
        EnBox_AppearOnSwitchFlag(&chest, play);
        REQUIRE(chest.actionFunc == EnBox_AppearOnSwitchFlag);
        REQUIRE(player->getItemId == GI_NONE && play->actorCtx.flags.chest == 0);
        play->actorCtx.flags.tempSwch = 1; // Existing Lullaby actor sets switch 32.
        EnBox_AppearOnSwitchFlag(&chest, play);
        REQUIRE(chest.actionFunc == EnBox_AppearInit);
        for (int frame = 0; frame < 64; ++frame)
            chest.actionFunc(&chest, play);
        REQUIRE(chest.actionFunc == EnBox_WaitOpen);
        REQUIRE(player->getItemEntry.itemId == ITEM_TIME_GATE);
        REQUIRE(player->getItemEntry.getItemId == -RG_TIME_GATE);
        REQUIRE(player->getItemId == -RG_TIME_GATE);
        REQUIRE(player->getItemEntry.getItemFrom == ITEM_FROM_CHEST);
        REQUIRE(chest.getItemEntry.getItemId == RG_TIME_GATE); // Offer copy only.
        REQUIRE(player->interactRangeActor == &chest.dyna.actor);
        REQUIRE(play->actorCtx.flags.chest == 0); // Merely approaching never collects.
        func_8083A434(play, player);
        REQUIRE(player->getItemId == RG_TIME_GATE && player->getItemEntry.getItemId == RG_TIME_GATE);
        Fixture_ReceiveItem(play, player);
        REQUIRE(customGives == 1 && vanillaGives == 0 && vanillaFallbacks == 0);
        REQUIRE(receivedEntry.getItemId == RG_TIME_GATE && receivedEntry.itemId == ITEM_TIME_GATE);
        REQUIRE(receiptText == 0xF8);
        Fixture_ReceiveItem(play, player);
        REQUIRE(customGives == 1); // The item-receipt guard prevents a second grant.
        chest.unk_1F4 = 1;
        EnBox_WaitOpen(&chest, play);
        REQUIRE(play->actorCtx.flags.chest == (1u << 5) && treasureEvents == 1);
        EnBox reentered = AuthoredChest();
        EnBox_Init(&reentered.dyna.actor, play);
        REQUIRE(reentered.actionFunc == EnBox_Open && reentered.skelanime.curFrame == 100);
        REQUIRE(treasureEvents == 1);

        // Every discriminator is needed to avoid stealing a normal chest's reward.
        for (int variation = 0; variation < 8; ++variation) {
            Reset(play, player, randomizer);
            EnBox other = AuthoredChest();
            switch (variation) {
                case 0:
                    gSaveContext.linkAge = LINK_AGE_CHILD;
                    break;
                case 1:
                    play->sceneNum = SCENE_GROTTOS;
                    break;
                case 2:
                    other.dyna.actor.params ^= 1;
                    break;
                case 3:
                    other.dyna.actor.world.rot.z = 31;
                    break;
                case 4:
                    other.dyna.actor.home.pos.x += 1;
                    break;
                case 5:
                    other.dyna.actor.home.pos.y += 1;
                    break;
                case 6:
                    other.dyna.actor.home.pos.z += 1;
                    break;
                case 7:
                    other.dyna.actor.room = 1;
                    break;
            }
            EnBox_Init(&other.dyna.actor, play);
            REQUIRE(other.getItemEntry.modIndex == MOD_NONE);
            REQUIRE(other.getItemEntry.itemId == ITEM_HEART_CONTAINER);
            REQUIRE(randoLookups == randomizer);
        }

        Reset(play, player, randomizer);
        EnBox ordinary = AuthoredChest();
        ordinary.dyna.actor.home.pos.x = 0;
        knownRandoChest = true;
        EnBox_Init(&ordinary.dyna.actor, play);
        REQUIRE(ordinary.getItemEntry.itemId == (randomizer ? ITEM_BOW : ITEM_HEART_CONTAINER));
        EnBox_WaitOpen(&ordinary, play);
        REQUIRE(player->getItemId == -GI_HEART_CONTAINER);
        REQUIRE(player->getItemEntry.itemId != ITEM_TIME_GATE);
    }
    free(player);
    free(play);
    puts("PASS Time Gate chest: exact adult placement, both rando modes, switch, offer/receipt, collection/re-entry, "
         "ordinary chests");
    return 0;
}
