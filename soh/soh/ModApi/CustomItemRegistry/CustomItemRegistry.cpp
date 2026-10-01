#include "CustomItemRegistry.h"

#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>
#include <libultraship/bridge.h>
#include <ship/Context.h>
#include <ship/debug/Console.h>

#include "soh/SaveManager.h"
#include "soh/cvar_prefixes.h"
#include "soh/ModApi/ConsoleArguments.h"
#include "soh/ModApi/RandoItems/RandoItems.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
void Player_InitItemAction(PlayState* play, Player* player, s8 itemAction);
void Player_InitItemActionWithAnim(PlayState* play, Player* player, s8 itemAction);
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

namespace {

constexpr uint16_t RuntimeIdBase = 0x400;
constexpr size_t ButtonCount = 8;
constexpr size_t AgeCount = 2;
constexpr size_t SaveItemLimit = 4096;
constexpr const char* SaveSection = "customItems";

using ButtonKeys = std::array<std::string, ButtonCount>;

struct RegisteredCustomItem {
    std::string owner;
    std::string key;
    std::string iconPath;
    std::string namePath;
    std::string pauseText;
    std::string getItemText;
    std::string heldModelPath;
    std::string getItemModelPath;
    std::string firstPersonModelPath;
    std::string logicKey;
    std::string hintKey;
    std::string progressionGroup;
    std::string poolCountOption;
    SOHCustomItemRandomizer randomizer = {};
    SOHCustomItemDefinition definition = {};
    uint16_t runtimeId = 0;
};

std::string sCurrentMod;
std::unordered_map<std::string, std::unique_ptr<RegisteredCustomItem>> sCustomItems;
std::unordered_map<uint16_t, RegisteredCustomItem*> sRuntimeItems;
std::unordered_map<uint16_t, RegisteredCustomItem*> sReplacements;
std::unordered_set<std::string> sOwnedItems;
std::unordered_map<Actor*, std::string> sActorItems;
std::unordered_set<Actor*> sDroppedItems;
std::array<ButtonKeys, AgeCount> sEquippedItems;
std::string sActiveItem;
std::string sHeldItem;
uint32_t sHeldUpdateFrame = 0;
constexpr uint32_t HeldUpdateGraceFrames = 4;
std::string sPauseItem;
std::string sPendingGetItem;
std::string sMessageItem;
uint16_t sNextRuntimeId = RuntimeIdBase;

const SOHCustomItemDefinition* FindDefinition(const std::string& key) {
    auto item = sCustomItems.find(key);
    return item == sCustomItems.end() ? nullptr : &item->second->definition;
}

void SetString(const char*& target, std::string& storage, const char* value) {
    storage = value == nullptr ? "" : value;
    target = storage.empty() ? nullptr : storage.c_str();
}

bool IsReplacement(const SOHCustomItemDefinition* definition) {
    return (definition->flags & SOH_CUSTOM_ITEM_REPLACES_VANILLA) != 0;
}

ButtonKeys& CurrentAgeButtons() {
    return sEquippedItems[LINK_IS_CHILD ? LINK_AGE_CHILD : LINK_AGE_ADULT];
}

ItemEquips& EquipsForAge(size_t age) {
    if (age == (size_t)gSaveContext.linkAge) {
        return gSaveContext.equips;
    }
    return age == LINK_AGE_CHILD ? gSaveContext.childEquips : gSaveContext.adultEquips;
}

void SwapOutOfOtherButtons(uint8_t targetButton, const char* key) {
    ButtonKeys& keys = CurrentAgeButtons();
    ItemEquips& equips = gSaveContext.equips;
    for (uint8_t other = 1; other < ButtonCount; ++other) {
        if (other == targetButton || equips.buttonItems[other] != ITEM_CUSTOM || keys[other] != key) {
            continue;
        }
        equips.buttonItems[other] = equips.buttonItems[targetButton];
        equips.cButtonSlots[other - 1] = equips.cButtonSlots[targetButton - 1];
        keys[other] = equips.buttonItems[other] == ITEM_CUSTOM ? keys[targetButton] : std::string();
        if (equips.buttonItems[other] == ITEM_NONE) {
            equips.cButtonSlots[other - 1] = SLOT_NONE;
        } else if (gPlayState != nullptr) {
            Interface_LoadItemIcon2(gPlayState, other);
        }
    }
}

void SyncButton(size_t age, size_t button) {
    const std::string& key = sEquippedItems[age][button];
    ItemEquips& equips = EquipsForAge(age);
    if (key.empty() || FindDefinition(key) == nullptr) {
        if (equips.buttonItems[button] == ITEM_CUSTOM) {
            equips.buttonItems[button] = ITEM_NONE;
        }
        return;
    }
    equips.buttonItems[button] = ITEM_CUSTOM;
}

void ResetSaveState(bool isDebug) {
    sOwnedItems.clear();
    sActiveItem.clear();
    sHeldItem.clear();
    sPauseItem.clear();
    sPendingGetItem.clear();
    sMessageItem.clear();
    for (auto& buttons : sEquippedItems) {
        for (auto& key : buttons) {
            key.clear();
        }
    }
}

const char* EquippedSaveKey(size_t age) {
    return age == LINK_AGE_CHILD ? "equippedChild" : "equippedAdult";
}

void SaveState(SaveContext* saveContext, int sectionId, bool fullSave) {
    std::vector<std::string> owned(sOwnedItems.begin(), sOwnedItems.end());
    SaveManager::Instance->SaveArray("owned", owned.size(),
                                     [&owned](size_t index) { SaveManager::Instance->SaveData("", owned[index]); });
    for (size_t age = 0; age < AgeCount; ++age) {
        SaveManager::Instance->SaveArray(EquippedSaveKey(age), ButtonCount, [age](size_t index) {
            bool holdsCustomItem = EquipsForAge(age).buttonItems[index] == ITEM_CUSTOM;
            SaveManager::Instance->SaveData("", holdsCustomItem ? sEquippedItems[age][index] : std::string());
        });
    }
}

void LoadState() {
    sOwnedItems.clear();
    SaveManager::Instance->LoadArray("owned", SaveItemLimit, [](size_t index) {
        std::string key;
        SaveManager::Instance->LoadData("", key, std::string());
        if (!key.empty()) {
            sOwnedItems.insert(std::move(key));
        }
    });
    for (size_t age = 0; age < AgeCount; ++age) {
        SaveManager::Instance->LoadArray(EquippedSaveKey(age), ButtonCount, [age](size_t index) {
            SaveManager::Instance->LoadData("", sEquippedItems[age][index], std::string());
            SyncButton(age, index);
        });
    }
}

void RegisterSaveState() {
    SaveManager::Instance->AddInitFunction(ResetSaveState);
    SaveManager::Instance->AddSaveFunction(SaveSection, 1, SaveState, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(SaveSection, 1, LoadState);
}

uint16_t ParseVanillaItem(const std::string& argument) {
    char* end = nullptr;
    const long item = std::strtol(argument.c_str(), &end, 0);
    if (end == argument.c_str() || *end != '\0' || item <= 0 || item > UINT16_MAX) {
        return 0;
    }
    return (uint16_t)item;
}

int32_t DropCommand(std::shared_ptr<Ship::Console> console, std::vector<std::string> args, std::string* output) {
    if (args.size() == 2) {
        const uint16_t item = ParseVanillaItem(args[1]);
        const bool dropped = item != 0 && VanillaItems_Drop(gPlayState, item);
        if (output != nullptr) {
            *output = dropped ? "Dropped item " + args[1] : "No custom item takes that item's place, or no game";
        }
        return dropped ? 0 : 1;
    }
    if (args.size() != 3 || args[1] != "custom_item") {
        if (output != nullptr) {
            *output = "Usage: drop custom_item \"<key>\" | drop <vanilla item id>";
        }
        return 1;
    }
    const std::string key = ModApi_UnquoteConsoleArgument(args[2]);
    const auto* definition = CustomItemRegistry_Find(key.c_str());
    if (definition == nullptr || gPlayState == nullptr) {
        if (output != nullptr) {
            *output = definition == nullptr ? "Unknown custom item" : "No active game";
        }
        return 1;
    }
    Player* player = GET_PLAYER(gPlayState);
    Vec3f position = player->actor.world.pos;
    position.x += Math_SinS(player->actor.shape.rot.y) * 60.0f;
    position.y += 20.0f;
    position.z += Math_CosS(player->actor.shape.rot.y) * 60.0f;
    EnItem00* drop = Item_DropCollectible2(gPlayState, &position, ITEM00_SOH_CUSTOM);
    if (drop == nullptr || !CustomItemRegistry_AttachDrop(&drop->actor, definition->key)) {
        if (output != nullptr) {
            *output = "Could not spawn custom item";
        }
        return 1;
    }
    if (output != nullptr) {
        *output = "Dropped " + key;
    }
    return 0;
}

const std::array<const char*, ButtonCount> ButtonNames = { "b",   "cleft", "cdown", "cright",
                                                           "dup", "ddown", "dleft", "dright" };

int32_t EquipCommand(std::shared_ptr<Ship::Console> console, std::vector<std::string> args, std::string* output) {
    const auto name = args.size() == 4 && args[1] == "custom_item"
                          ? std::find(ButtonNames.begin(), ButtonNames.end(), args[3])
                          : ButtonNames.end();
    if (name == ButtonNames.end()) {
        if (output != nullptr) {
            *output = "Usage: equip custom_item \"<key>\" <b|cleft|cdown|cright|dup|ddown|dleft|dright>";
        }
        return 1;
    }
    const std::string key = ModApi_UnquoteConsoleArgument(args[2]);
    bool equipped = gPlayState != nullptr &&
                    CustomItemRegistry_Equip(static_cast<uint8_t>(name - ButtonNames.begin()), key.c_str());
    if (output != nullptr) {
        *output = equipped ? "Equipped " + key : "Not owned, wrong age, button not allowed, or no active game";
    }
    return equipped ? 0 : 1;
}

void RegisterConsole() {
    Ship::Context::GetInstance()->GetConsole()->AddCommand("equip", { EquipCommand,
                                                                      "Equips an owned custom item on a button",
                                                                      { { "custom_item", Ship::ArgumentType::TEXT },
                                                                        { "key", Ship::ArgumentType::TEXT },
                                                                        { "button", Ship::ArgumentType::TEXT } } });
    Ship::Context::GetInstance()->GetConsole()->AddCommand(
        "give", { [](std::shared_ptr<Ship::Console>, std::vector<std::string> args, std::string* output) -> int32_t {
                     if (args.size() == 2) {
                         const uint16_t item = ParseVanillaItem(args[1]);
                         const bool given = item != 0 && VanillaItems_Give(gPlayState, item);
                         if (output != nullptr) {
                             *output = given ? "Given item " + args[1] : "Unknown item id or no active game";
                         }
                         return given ? 0 : 1;
                     }
                     if (args.size() != 3) {
                         if (output != nullptr) {
                             *output = "Usage: give custom_item \"<key>\" | give <vanilla item id>";
                         }
                         return 1;
                     }
                     const std::string key = ModApi_UnquoteConsoleArgument(args[2]);
                     bool handled = false;
                     bool given = false;
                     GameInteractor::Instance->ExecuteHooks<GameInteractor::OnConsoleGive>(
                         gPlayState, args[1].c_str(), key.c_str(), &handled, &given);
                     if (!handled && args[1] == "custom_item") {
                         given = CustomItemRegistry_Give(gPlayState, key.c_str());
                     }
                     if (output != nullptr) {
                         *output = given ? "Given " + key : "Unknown custom item or no active game";
                     }
                     return given ? 0 : 1;
                 },
                  "Gives a registered custom item, or a vanilla item by id",
                  { { "custom_item|item id", Ship::ArgumentType::TEXT }, { "key", Ship::ArgumentType::TEXT, true } } });
    Ship::Context::GetInstance()->GetConsole()->AddCommand(
        "drop", { DropCommand,
                  "Drops an item in front of Link, by custom key or vanilla item id",
                  { { "custom_item|item id", Ship::ArgumentType::TEXT }, { "key", Ship::ArgumentType::TEXT, true } } });
}

SOHItemIconSurface ButtonIconSurface(uint8_t button) {
    if (button == 0) {
        return SOH_ITEM_ICON_B_BUTTON;
    }
    return button < 4 ? SOH_ITEM_ICON_C_BUTTON : SOH_ITEM_ICON_DPAD;
}

void ResolveReplacementTexture(uint16_t item, SOHItemIconSurface surface, const char** path) {
    const auto* replacement = CustomItemRegistry_FindReplacement(item);
    if (replacement == nullptr) {
        return;
    }
    const char* resolved = CustomItemRegistry_ResolveTexture(replacement->key, surface);
    if (resolved != nullptr) {
        *path = resolved;
    }
}

void DrawReplacementGetItem(PlayState* play, GetItemEntry* entry, bool* handled) {
    if (*handled || entry->modIndex != MOD_NONE) {
        return;
    }
    const auto* replacement = CustomItemRegistry_FindReplacement(entry->itemId);
    if (replacement == nullptr || replacement->getItemEntry.drawFunc == nullptr) {
        return;
    }
    GetItemEntry drawEntry = replacement->getItemEntry;
    drawEntry.drawFunc(play, &drawEntry);
    *handled = true;
}

void ResolveReplacementItemAction(int32_t item, int8_t* itemAction) {
    if (item < 0 || item > ITEM_CLAIM_CHECK) {
        return;
    }
    const auto* replacement = CustomItemRegistry_FindReplacement((uint16_t)item);
    if (replacement != nullptr && (replacement->init != nullptr || replacement->update != nullptr)) {
        *itemAction = PLAYER_IA_CUSTOM;
    }
}

void RegisterReplacementHooks() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnInterfaceResolveButtonIcon>(
        [](PlayState*, uint8_t button, uint16_t item, const char** iconPath) {
            ResolveReplacementTexture(item, ButtonIconSurface(button), iconPath);
        });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnMessageResolveItemIcon>(
        [](uint16_t item, const char** iconPath) { ResolveReplacementTexture(item, SOH_ITEM_ICON_TEXTBOX, iconPath); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoResolveItemIcon>(
        [](PlayState*, uint16_t item, const char** iconPath) {
            ResolveReplacementTexture(item, SOH_ITEM_ICON_INVENTORY, iconPath);
        });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoResolveName>(
        [](PlayState*, uint16_t item, const char** namePath) {
            ResolveReplacementTexture(item, SOH_ITEM_NAME_TEXTURE, namePath);
        });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGetItemDraw>(DrawReplacementGetItem);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerResolveItemAction>(ResolveReplacementItemAction);
}

} // namespace

static void DrawModelPath(PlayState* play, const char* path, bool setupOpa) {
    OPEN_DISPS(play->state.gfxCtx);
    if (setupOpa) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
    }
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)path);
    CLOSE_DISPS(play->state.gfxCtx);
}

namespace {

const SOHCustomItemDefinition* GetShownItem() {
    return FindDefinition(sPendingGetItem.empty() ? sMessageItem : sPendingGetItem);
}

void DrawShownGetItemModel(PlayState* play, GetItemEntry* entry) {
    const SOHCustomItemDefinition* definition = GetShownItem();
    if (definition != nullptr && definition->getItemModelPath != nullptr) {
        DrawModelPath(play, definition->getItemModelPath, true);
    }
}

void BuildGetItemText(uint16_t* textId, bool* loadFromMessageTable) {
    const SOHCustomItemDefinition* definition = FindDefinition(sMessageItem);
    if (definition == nullptr || definition->getItemText == nullptr) {
        return;
    }
    CustomMessage message(definition->getItemText, TEXTBOX_TYPE_BLUE);
    message.AutoFormat(ITEM_CUSTOM);
    message.LoadIntoFont();
    *loadFromMessageTable = false;
}

std::string sModMessageText;
bool sIsModMessageAutoFormatted = false;

void BuildModMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage message(sModMessageText);
    if (sIsModMessageAutoFormatted) {
        message.AutoFormat();
    } else {
        message.Format();
    }
    message.LoadIntoFont();
    *loadFromMessageTable = false;
}

void PutAwayHeldItem(PlayState* play, Player* player) {
    if (sHeldItem.empty()) {
        return;
    }
    const SOHCustomItemDefinition* definition = FindDefinition(sHeldItem);
    sHeldItem.clear();
    if (definition != nullptr && definition->putAway != nullptr && player != nullptr) {
        definition->putAway(player, play);
    }
    if (play == nullptr || player == nullptr || player->heldItemAction != PLAYER_IA_CUSTOM) {
        return;
    }
    player->heldItemId = ITEM_NONE;
    player->nextModelGroup = Player_ActionToModelGroup(player, PLAYER_IA_NONE);
    Player_InitItemActionWithAnim(play, player, PLAYER_IA_NONE);
}

void PutAwayStalledHeldItem() {
    if (sHeldItem.empty() || gPlayState == nullptr) {
        return;
    }
    const SOHCustomItemDefinition* definition = FindDefinition(sHeldItem);
    if (definition == nullptr || definition->update == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player->heldItemAction != PLAYER_IA_CUSTOM ||
        (gPlayState->state.frames - sHeldUpdateFrame) > HeldUpdateGraceFrames) {
        PutAwayHeldItem(gPlayState, player);
    }
}

void PutAwayOnItemActionChange(int8_t itemAction, PlayerItemActionInitFunc* init) {
    if (itemAction != PLAYER_IA_CUSTOM && gPlayState != nullptr) {
        PutAwayHeldItem(gPlayState, GET_PLAYER(gPlayState));
    }
}

void DrawNoGetItemModel(PlayState* play, GetItemEntry* entry) {
}

void CompleteGetItemEntry(GetItemEntry& entry) {
    if (entry.objectId == OBJECT_INVALID) {
        entry.objectId = OBJECT_GI_BOMB_2;
    }
    if (entry.gi == 0) {
        entry.gi = 1;
    }
    if (entry.field == 0) {
        entry.field = 0x80;
    }
    if (entry.getItemCategory == ITEM_CATEGORY_JUNK) {
        entry.getItemCategory = ITEM_CATEGORY_MAJOR;
    }
    if (entry.drawFunc == nullptr) {
        entry.drawFunc = DrawNoGetItemModel;
    }
    entry.collectable = true;
    entry.drawItemId = ITEM_CUSTOM;
}

void ApplyDataDrivenDefaults(SOHCustomItemDefinition& definition) {
    if (definition.getItemText != nullptr && definition.getItemEntry.textId == 0) {
        definition.getItemEntry.textId = TEXT_MOD_ITEM_GET_ITEM;
    }
    if (definition.getItemModelPath != nullptr && definition.getItemEntry.drawFunc == nullptr) {
        definition.getItemEntry.drawFunc = DrawShownGetItemModel;
    }
    CompleteGetItemEntry(definition.getItemEntry);
}

bool IsValidDefinition(const SOHCustomItemDefinition* definition) {
    if (definition == nullptr || definition->structSize < SOH_CUSTOM_ITEM_DEFINITION_MIN_SIZE ||
        definition->key == nullptr || definition->key[0] == '\0') {
        SPDLOG_ERROR("[CustomItemRegistry] Mod '{}' supplied an invalid custom item", sCurrentMod);
        return false;
    }
    if ((definition->flags & SOH_CUSTOM_ITEM_EQUIPPABLE) != 0 &&
        (definition->iconPath == nullptr || definition->iconPath[0] == '\0')) {
        SPDLOG_ERROR("[CustomItemRegistry] Equippable custom item '{}' requires an icon path", definition->key);
        return false;
    }
    if (definition->ageRequirement > SOH_CUSTOM_ITEM_AGE_ADULT) {
        SPDLOG_ERROR("[CustomItemRegistry] Custom item '{}' has unknown age requirement {}", definition->key,
                     definition->ageRequirement);
        return false;
    }
    if (sCustomItems.contains(definition->key)) {
        SPDLOG_ERROR("[CustomItemRegistry] Mod '{}' tried to register duplicate custom item '{}'", sCurrentMod,
                     definition->key);
        return false;
    }
    if (!IsReplacement(definition)) {
        return true;
    }
    if (definition->structSize < SOH_CUSTOM_ITEM_REPLACEMENT_MIN_SIZE || definition->replacesItem > ITEM_CLAIM_CHECK) {
        SPDLOG_ERROR("[CustomItemRegistry] Custom item '{}' must replace an inventory item", definition->key);
        return false;
    }
    auto replaced = sReplacements.find(definition->replacesItem);
    if (replaced != sReplacements.end()) {
        SPDLOG_ERROR("[CustomItemRegistry] Custom item '{}' cannot replace item {}: '{}' already does", definition->key,
                     definition->replacesItem, replaced->second->key);
        return false;
    }
    return true;
}

} // namespace

void CustomItemRegistry_Init() {
    RegisterSaveState();
    RegisterConsole();
    RegisterReplacementHooks();
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(TEXT_MOD_ITEM_GET_ITEM,
                                                                                BuildGetItemText);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(TEXT_MOD_MESSAGE, BuildModMessage);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerResolveItemActionInit>(
        PutAwayOnItemActionChange);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(PutAwayStalledHeldItem);
}

bool CustomItemRegistry_Register(const SOHCustomItemDefinition* definition) {
    if (!IsValidDefinition(definition)) {
        return false;
    }
    if (sNextRuntimeId == UINT16_MAX) {
        SPDLOG_ERROR("[CustomItemRegistry] Runtime item id space is exhausted");
        return false;
    }

    auto item = std::make_unique<RegisteredCustomItem>();
    item->owner = sCurrentMod;
    item->key = definition->key;
    item->runtimeId = sNextRuntimeId;
    std::memcpy(&item->definition, definition, std::min<size_t>(definition->structSize, sizeof(item->definition)));
    item->definition.structSize = sizeof(item->definition);
    item->definition.key = item->key.c_str();
    item->definition.getItemEntry.itemId = ITEM_CUSTOM;
    item->definition.getItemEntry.getItemId = GI_CUSTOM;
    SetString(item->definition.iconPath, item->iconPath, definition->iconPath);
    SetString(item->definition.namePath, item->namePath, definition->namePath);
    SetString(item->definition.pauseText, item->pauseText, definition->pauseText);
    SetString(item->definition.getItemText, item->getItemText, item->definition.getItemText);
    SetString(item->definition.heldModelPath, item->heldModelPath, item->definition.heldModelPath);
    SetString(item->definition.getItemModelPath, item->getItemModelPath, item->definition.getItemModelPath);
    SetString(item->definition.firstPersonModelPath, item->firstPersonModelPath, item->definition.firstPersonModelPath);
    ApplyDataDrivenDefaults(item->definition);
    item->definition.randomizer = nullptr;

    if (definition->randomizer != nullptr) {
        if (definition->randomizer->structSize < SOH_CUSTOM_ITEM_RANDOMIZER_MIN_SIZE) {
            SPDLOG_ERROR("[CustomItemRegistry] Custom item '{}' supplied invalid randomizer data", item->key);
            return false;
        }

        std::memcpy(&item->randomizer, definition->randomizer,
                    std::min<size_t>(definition->randomizer->structSize, sizeof(item->randomizer)));
        item->randomizer.structSize = sizeof(item->randomizer);
        SetString(item->randomizer.logicKey, item->logicKey, item->randomizer.logicKey);
        SetString(item->randomizer.hintKey, item->hintKey, item->randomizer.hintKey);
        SetString(item->randomizer.progressionGroup, item->progressionGroup, item->randomizer.progressionGroup);
        SetString(item->randomizer.poolCountOption, item->poolCountOption, item->randomizer.poolCountOption);
        item->definition.randomizer = &item->randomizer;
    }

    auto* registered = item.get();
    sRuntimeItems.emplace(registered->runtimeId, registered);
    sCustomItems.emplace(registered->key, std::move(item));
    ++sNextRuntimeId;
    if (registered->definition.randomizer != nullptr) {
        RandoItems_Register(registered->key.c_str());
    }
    if (IsReplacement(&registered->definition)) {
        sReplacements.emplace(registered->definition.replacesItem, registered);
    }
    for (size_t age = 0; age < AgeCount; ++age) {
        for (size_t button = 0; button < ButtonCount; ++button) {
            if (sEquippedItems[age][button] == registered->key) {
                SyncButton(age, button);
            }
        }
    }
    SPDLOG_INFO("[CustomItemRegistry] Mod '{}' registered custom item '{}' as {}", registered->owner, registered->key,
                registered->runtimeId);
    return true;
}

const SOHCustomItemDefinition* CustomItemRegistry_Find(const char* key) {
    return key == nullptr ? nullptr : FindDefinition(key);
}

const GetItemEntry* CustomItemRegistry_FindGetItemEntry(const char* key) {
    const auto* definition = CustomItemRegistry_Find(key);
    return definition == nullptr ? nullptr : &definition->getItemEntry;
}

uint16_t CustomItemRegistry_GetRuntimeId(const char* key) {
    if (key == nullptr) {
        return 0;
    }
    auto item = sCustomItems.find(key);
    return item == sCustomItems.end() ? 0 : item->second->runtimeId;
}

const SOHCustomItemDefinition* CustomItemRegistry_FindByRuntimeId(uint16_t runtimeId) {
    auto item = sRuntimeItems.find(runtimeId);
    return item == sRuntimeItems.end() ? nullptr : &item->second->definition;
}

uint32_t CustomItemRegistry_GetCount(void) {
    return (uint32_t)sCustomItems.size();
}

const SOHCustomItemDefinition* CustomItemRegistry_GetAt(uint32_t index) {
    if (index >= sRuntimeItems.size()) {
        return nullptr;
    }
    return CustomItemRegistry_FindByRuntimeId((uint16_t)(RuntimeIdBase + index));
}

const SOHCustomItemDefinition* CustomItemRegistry_FindReplacement(uint16_t vanillaItem) {
    auto replacement = sReplacements.find(vanillaItem);
    return replacement == sReplacements.end() ? nullptr : &replacement->second->definition;
}

bool CustomItemRegistry_IsAgeAllowed(const char* key) {
    const auto* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr) {
        return false;
    }
    if (CVarGetInteger(CVAR_CHEAT("TimelessEquipment"), 0)) {
        return true;
    }
    switch (definition->ageRequirement) {
        case SOH_CUSTOM_ITEM_AGE_ANY:
            return true;
        case SOH_CUSTOM_ITEM_AGE_CHILD:
            return LINK_IS_CHILD;
        case SOH_CUSTOM_ITEM_AGE_ADULT:
            return LINK_IS_ADULT;
        default:
            SPDLOG_ERROR("[CustomItemRegistry] Custom item '{}' has unknown age requirement {}", definition->key,
                         definition->ageRequirement);
            return false;
    }
}

bool CustomItemRegistry_IsPauseItemAgeAllowed(void) {
    const auto* definition = CustomItemRegistry_GetPauseItem();
    return definition != nullptr && CustomItemRegistry_IsAgeAllowed(definition->key);
}

bool CustomItemRegistry_SetOwned(const char* key, bool owned) {
    if (key == nullptr || key[0] == '\0') {
        return false;
    }
    const auto* definition = CustomItemRegistry_Find(key);
    if (definition != nullptr && CustomItemRegistry_IsVanillaUpgrade(definition)) {
        SPDLOG_ERROR("[CustomItemRegistry] '{}' upgrades a vanilla item; its ownership is that item's", key);
        return false;
    }
    const bool wasOwned = sOwnedItems.contains(key);
    if (owned) {
        sOwnedItems.insert(key);
    } else {
        sOwnedItems.erase(key);
    }
    if (definition != nullptr && owned != wasOwned) {
        SOHCustomItemStateFunc callback = owned ? definition->onAcquire : definition->onRemove;
        if (callback != nullptr) {
            callback(key);
        }
    }
    return true;
}

void CustomItemRegistry_ReceiveGetItem(const char* key) {
    CustomItemRegistry_SetOwned(key, true);

    const auto* definition = CustomItemRegistry_Find(key);

    if (definition != nullptr && definition->onReceive != nullptr) {
        definition->onReceive(key);
    }
}

bool CustomItemRegistry_IsOwned(const char* key) {
    const auto* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr) {
        return false;
    }
    if (CustomItemRegistry_IsVanillaUpgrade(definition)) {
        return INV_CONTENT(definition->replacesItem) == definition->replacesItem;
    }
    return sOwnedItems.contains(key);
}

bool CustomItemRegistry_Equip(uint8_t button, const char* key) {
    const auto* definition = CustomItemRegistry_Find(key);
    if (button >= ButtonCount || definition == nullptr || CustomItemRegistry_IsVanillaUpgrade(definition) ||
        !CustomItemRegistry_IsOwned(key) || !CustomItemRegistry_IsAgeAllowed(key) ||
        (definition->flags & SOH_CUSTOM_ITEM_EQUIPPABLE) == 0) {
        return false;
    }
    if (button == 0 && (definition->flags & SOH_CUSTOM_ITEM_B_BUTTON) == 0) {
        return false;
    }
    if (button >= 1 && button <= 3 && (definition->flags & SOH_CUSTOM_ITEM_C_BUTTON) == 0) {
        return false;
    }
    if (button >= 4 && (definition->flags & SOH_CUSTOM_ITEM_DPAD) == 0) {
        return false;
    }
    if (button != 0) {
        SwapOutOfOtherButtons(button, key);
        gSaveContext.equips.cButtonSlots[button - 1] = SLOT_NONE;
    }
    CurrentAgeButtons()[button] = key;
    SyncButton(gSaveContext.linkAge, button);
    return true;
}

void CustomItemRegistry_CopyEquippedKey(uint8_t fromButton, uint8_t toButton) {
    if (fromButton >= ButtonCount || toButton >= ButtonCount) {
        return;
    }
    CurrentAgeButtons()[toButton] = CurrentAgeButtons()[fromButton];
}

void CustomItemRegistry_Unequip(uint8_t button) {
    if (button >= ButtonCount) {
        return;
    }
    CurrentAgeButtons()[button].clear();
    SyncButton(gSaveContext.linkAge, button);
}

const char* CustomItemRegistry_GetEquippedKey(uint8_t button) {
    if (button >= ButtonCount || gSaveContext.equips.buttonItems[button] != ITEM_CUSTOM ||
        CurrentAgeButtons()[button].empty()) {
        return nullptr;
    }
    return CurrentAgeButtons()[button].c_str();
}

uint16_t CustomItemRegistry_GetEquippedRuntimeId(uint8_t button) {
    return CustomItemRegistry_GetRuntimeId(CustomItemRegistry_GetEquippedKey(button));
}

bool CustomItemRegistry_PressButtonItem(PlayState* play, Player* player, uint8_t button, uint16_t item) {
    const SOHCustomItemDefinition* definition = item == ITEM_CUSTOM
                                                    ? CustomItemRegistry_Find(CustomItemRegistry_GetEquippedKey(button))
                                                    : CustomItemRegistry_FindReplacement(item);
    if (definition == nullptr || !CustomItemRegistry_IsAgeAllowed(definition->key) ||
        (definition->canUse != nullptr && !definition->canUse(player, play))) {
        return false;
    }
    if ((definition->flags & SOH_CUSTOM_ITEM_INSTANT) != 0) {
        if (definition->init != nullptr) {
            definition->init(play, player);
        }
        return false;
    }
    sActiveItem = definition->key;
    if (player->heldItemAction != PLAYER_IA_CUSTOM || sHeldItem.empty()) {
        return true;
    }
    if (sHeldItem == sActiveItem) {
        if (definition->use != nullptr) {
            definition->use(player, play);
        }
        return true;
    }
    Player_InitItemAction(play, player, PLAYER_IA_CUSTOM);
    return false;
}

bool CustomItemRegistry_SelectPauseItem(const char* key) {
    if (key == nullptr || FindDefinition(key) == nullptr || !CustomItemRegistry_IsOwned(key)) {
        sPauseItem.clear();
        return false;
    }
    sPauseItem = key;
    return true;
}

const SOHCustomItemDefinition* CustomItemRegistry_GetPauseItem(void) {
    return FindDefinition(sPauseItem);
}

void CustomItemRegistry_PreparePauseMessage(void) {
    sMessageItem = sPauseItem;
}

const SOHCustomItemDefinition* CustomItemRegistry_GetMessageItem(void) {
    return FindDefinition(sMessageItem);
}

const SOHCustomItemDefinition* CustomItemRegistry_GetActive(void) {
    return FindDefinition(sActiveItem);
}

const char* CustomItemRegistry_GetHeldKey(void) {
    return sHeldItem.empty() ? nullptr : sHeldItem.c_str();
}

bool CustomItemRegistry_SetIconPath(const char* key, const char* path) {
    auto item = sCustomItems.find(key == nullptr ? "" : key);
    if (item == sCustomItems.end()) {
        return false;
    }
    SetString(item->second->definition.iconPath, item->second->iconPath, path);
    return true;
}

bool CustomItemRegistry_SetText(const char* key, SOHItemTextKind kind, const char* text) {
    auto item = sCustomItems.find(key == nullptr ? "" : key);
    if (item == sCustomItems.end()) {
        return false;
    }
    switch (kind) {
        case SOH_ITEM_TEXT_GET_ITEM:
            SetString(item->second->definition.getItemText, item->second->getItemText, text);
            if (item->second->definition.getItemText != nullptr && item->second->definition.getItemEntry.textId == 0) {
                item->second->definition.getItemEntry.textId = TEXT_MOD_ITEM_GET_ITEM;
            }
            return true;
        case SOH_ITEM_TEXT_PAUSE:
            SetString(item->second->definition.pauseText, item->second->pauseText, text);
            return true;
        default:
            SPDLOG_ERROR("[CustomItemRegistry] Custom item '{}' asked for unknown text {}", item->second->key,
                         (int)kind);
            return false;
    }
}

uint8_t CustomItemRegistry_GetActiveMeleeWeapon(void) {
    const auto* definition = FindDefinition(sActiveItem);
    return definition == nullptr ? 0 : definition->meleeWeapon;
}

void CustomItemRegistry_InitHeld(PlayState* play, Player* player) {
    if (sHeldItem != sActiveItem) {
        PutAwayHeldItem(play, player);
    }
    sHeldItem = sActiveItem;
    sHeldUpdateFrame = play->state.frames;
    const auto* definition = FindDefinition(sHeldItem);
    if (definition != nullptr && definition->init != nullptr) {
        definition->init(play, player);
    }
}

int32_t CustomItemRegistry_UpdateHeld(Player* player, PlayState* play) {
    const auto* definition = FindDefinition(sHeldItem);
    sHeldUpdateFrame = play->state.frames;
    return definition != nullptr && definition->update != nullptr ? definition->update(player, play) : 0;
}

void CustomItemRegistry_DrawHeld(Player* player, PlayState* play) {
    const auto* definition = FindDefinition(sHeldItem);
    if (definition == nullptr) {
        return;
    }
    if (definition->drawHeld != nullptr) {
        definition->drawHeld(player, play);
    } else if (definition->heldModelPath != nullptr) {
        DrawModelPath(play, definition->heldModelPath, false);
    }
}

const char* CustomItemRegistry_GetFirstPersonModel(void) {
    const auto* definition = FindDefinition(sHeldItem);
    return definition == nullptr ? nullptr : definition->firstPersonModelPath;
}

void CustomItemRegistry_HoldHeld(void) {
    if (!sHeldItem.empty() && gPlayState != nullptr) {
        sHeldUpdateFrame = gPlayState->state.frames;
    }
}

bool CustomItemRegistry_AttachDrop(Actor* actor, const char* key) {
    if (!CustomItemRegistry_BindActor(actor, key)) {
        return false;
    }
    sDroppedItems.insert(actor);
    return true;
}

bool CustomItemRegistry_BindActor(Actor* actor, const char* key) {
    if (actor == nullptr || CustomItemRegistry_Find(key) == nullptr) {
        return false;
    }
    sActorItems[actor] = key;
    return true;
}

void CustomItemRegistry_UnbindActor(Actor* actor) {
    sActorItems.erase(actor);
    sDroppedItems.erase(actor);
}

const char* CustomItemRegistry_GetActorItem(Actor* actor) {
    auto item = sActorItems.find(actor);
    return item == sActorItems.end() ? nullptr : item->second.c_str();
}

bool CustomItemRegistry_ShowTextbox(PlayState* play, const char* text, bool autoFormat) {
    if (play == nullptr || text == nullptr) {
        return false;
    }
    sModMessageText = text;
    sIsModMessageAutoFormatted = autoFormat;
    Message_StartTextbox(play, TEXT_MOD_MESSAGE, nullptr);
    return true;
}

bool CustomItemRegistry_Give(PlayState* play, const char* key) {

    const auto* definition = CustomItemRegistry_Find(key);
    if (play == nullptr || definition == nullptr) {
        return false;
    }
    sPendingGetItem = definition->key;
    if (!GiveItemEntryWithoutActor(play, definition->getItemEntry)) {
        sPendingGetItem.clear();
        return false;
    }
    sMessageItem = definition->key;
    return true;
}

const char* CustomItemRegistry_ResolveTexture(const char* key, SOHItemIconSurface surface) {
    const auto* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr || (definition->disabledIconSurfaces & surface) != 0) {
        return nullptr;
    }
    const char* path = surface == SOH_ITEM_NAME_TEXTURE ? definition->namePath : definition->iconPath;
    return path != nullptr && path[0] != '\0' ? path : nullptr;
}

bool CustomItemRegistry_IsEditorVisible(const char* key) {
    const auto* definition = CustomItemRegistry_Find(key);
    return definition != nullptr && (definition->presentationFlags & SOH_ITEM_HIDE_FROM_SAVE_EDITOR) == 0;
}

bool CustomItemRegistry_IsCustomGetItem(const GetItemEntry* entry) {
    return entry != nullptr && entry->modIndex == MOD_NONE && entry->itemId == ITEM_CUSTOM &&
           entry->getItemId == GI_CUSTOM;
}

bool CustomItemRegistry_HasMessageItemIcon(void) {
    const SOHCustomItemDefinition* definition = CustomItemRegistry_GetMessageItem();
    return definition != nullptr &&
           CustomItemRegistry_ResolveTexture(definition->key, SOH_ITEM_ICON_TEXTBOX) != nullptr;
}

bool CustomItemRegistry_PrepareGetItem(Actor* actor, PlayState* play, GetItemEntry* entry) {
    if (entry == nullptr) {
        return true;
    }
    if (!CustomItemRegistry_IsCustomGetItem(entry)) {
        sMessageItem.clear();
        const auto* replacement =
            entry->modIndex == MOD_NONE ? CustomItemRegistry_FindReplacement(entry->itemId) : nullptr;
        if (replacement != nullptr && replacement->getItemEntry.textId != 0) {
            entry->textId = replacement->getItemEntry.textId;
        }
        return true;
    }
    const char* key = actor != nullptr          ? CustomItemRegistry_GetActorItem(actor)
                      : sPendingGetItem.empty() ? nullptr
                                                : sPendingGetItem.c_str();
    GameInteractor_ExecuteOnResolveCustomGetItem(actor, play, entry, &key);
    const auto* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr) {
        return false;
    }
    sPendingGetItem = definition->key;
    sMessageItem = definition->key;
    *entry = definition->getItemEntry;
    return true;
}

const SOHCustomItemDefinition* CustomItemRegistry_GetPendingGetItem(void) {
    return FindDefinition(sPendingGetItem);
}

void CustomItemRegistry_ClearPendingGetItem(void) {
    sPendingGetItem.clear();
}

bool CustomItemRegistry_UpdateDrop(Actor* actor, PlayState* play) {
    if (!sDroppedItems.contains(actor)) {
        return false;
    }
    const char* key = CustomItemRegistry_GetActorItem(actor);
    auto definition = CustomItemRegistry_Find(key);
    if (definition == nullptr) {
        Actor_Kill(actor);
        return true;
    }
    if (Actor_HasParent(actor, play)) {
        Actor_Kill(actor);
    } else {
        GiveItemEntryFromActorWithFixedRange(actor, play, definition->getItemEntry);
    }
    return true;
}

bool CustomItemRegistry_DrawDrop(Actor* actor, PlayState* play) {
    if (!sDroppedItems.contains(actor)) {
        return false;
    }
    const auto* definition = CustomItemRegistry_Find(CustomItemRegistry_GetActorItem(actor));
    if (definition == nullptr) {
        return true;
    }
    constexpr f32 dropModelScale = 10.0f;
    Matrix_Scale(dropModelScale, dropModelScale, dropModelScale, MTXMODE_APPLY);
    func_8002EBCC(actor, play, 0);
    func_8002ED80(actor, play, 0);
    if (definition->getItemEntry.drawFunc == DrawShownGetItemModel) {
        DrawModelPath(play, definition->getItemModelPath, true);
    } else {
        GetItemEntry_Draw(play, definition->getItemEntry);
    }
    return true;
}

void CustomItemRegistry_RemoveDrop(Actor* actor) {
    CustomItemRegistry_UnbindActor(actor);
}

void CustomItemRegistry_SetCurrentMod(const std::string& name) {
    sCurrentMod = name;
}

void CustomItemRegistry_ClearCurrentMod() {
    sCurrentMod.clear();
}
