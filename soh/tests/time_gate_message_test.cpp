#include <cstdint>
#include "z64item.h"
#include "z64object.h"
#include "soh/Enhancements/randomizer/randomizerEnums.h"
#include "soh/Enhancements/item-tables/ItemTableTypes.h"
#include "tests/test_require.h"

using u16 = uint16_t;
struct Player {
    GetItemEntry getItemEntry;
};
struct PlayState {
    Player* player;
};
PlayState* gPlayState;
static bool fixtureRandomizer;
#define IS_RANDO fixtureRandomizer
#define GET_PLAYER(play) ((play)->player)
#define TEXT_RANDOMIZER_CUSTOM_ITEM 0x00F8
static int loadedMessages;
static void (*registeredMessage)(u16*, bool*);
// The fixture supplies the message renderer; production selects whether to use it.
struct CustomMessage {
    void LoadIntoFont() {
        ++loadedMessages;
    }
};
void BuildCustomItemMessage(Player*, CustomMessage&) {
}
void BuildTriforcePieceMessage(CustomMessage&) {
}
void BuildTriforceMessage(CustomMessage&) {
}
void BuildQuarterHeartMessage(CustomMessage&) {
}
void BuildDefenseUpgradeMessage(CustomMessage&) {
}
void BuildSpeedUpgradeMessage(CustomMessage&) {
}
void BuildPowerUpgradeMessage(CustomMessage&) {
}
void BuildMagicStatUpgradeMessage(CustomMessage&) {
}
void BuildCrawlSpeedUpgradeMessage(CustomMessage&) {
}
void BuildClimbSpeedUpgradeMessage(CustomMessage&) {
}
void BuildPushSpeedUpgradeMessage(CustomMessage&) {
}
namespace Rando::Traps {
void BuildIceTrapMessage(CustomMessage&, GetItemEntry) {
}
} // namespace Rando::Traps
#define COND_ID_HOOK(hook, id, condition, body)           \
    do {                                                  \
        registeredMessage = (condition) ? body : nullptr; \
    } while (0)
#include "time_gate_message_production.inc"

int main() {
    auto play = new PlayState{};
    auto player = new Player{};
    play->player = player;
    gPlayState = play;
    for (int randomizer = 0; randomizer < 2; ++randomizer) {
        fixtureRandomizer = randomizer;
        player->getItemEntry = (GetItemEntry)GET_ITEM_NONE;
        Fixture_RegisterItemMessage(); // Registration precedes approaching the chest.
        REQUIRE(registeredMessage != nullptr);
        player->getItemEntry.modIndex = MOD_RANDOMIZER;
        player->getItemEntry.getItemId = RG_TIME_GATE;
        player->getItemEntry.objectId = OBJECT_GI_MEDAL;
        u16 text = TEXT_RANDOMIZER_CUSTOM_ITEM;
        bool fromTable = true;
        loadedMessages = 0;
        registeredMessage(&text, &fromTable);
        REQUIRE(!fromTable && loadedMessages == 1);
        player->getItemEntry.getItemId = RG_PROGRESSIVE_ROCS;
        fromTable = true;
        loadedMessages = 0;
        registeredMessage(&text, &fromTable);
        REQUIRE(!fromTable && loadedMessages == 1);
        player->getItemEntry.objectId = OBJECT_INVALID;
        fromTable = true;
        loadedMessages = 0;
        registeredMessage(&text, &fromTable);
        REQUIRE(fromTable == !randomizer && loadedMessages == randomizer);
        player->getItemEntry.objectId = OBJECT_GI_MEDAL;
        player->getItemEntry.getItemId = RG_PROGRESSIVE_BOW;
        fromTable = true;
        loadedMessages = 0;
        registeredMessage(&text, &fromTable);
        REQUIRE(fromTable == !randomizer && loadedMessages == randomizer);
        player->getItemEntry.getItemId = RG_TIME_GATE;
        player->getItemEntry.modIndex = MOD_NONE;
        fromTable = true;
        loadedMessages = 0;
        registeredMessage(&text, &fromTable);
        REQUIRE(fromTable == !randomizer && loadedMessages == randomizer);
    }
    delete player;
    delete play;
    puts("PASS fixed reward messages: Time Gate/house feather in normal and rando saves; unrelated messages preserved");
}
