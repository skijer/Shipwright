#include "FormRegistry.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/debug/Console.h>

#include "soh/SaveManager.h"
#include "soh/ModApi/ConsoleArguments.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ModApi/Player/PlayerHookTypes.h"
#include "soh/ModApi/PlayerInput/PlayerInput.h"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/Forms/FormTransform.h"
#include "soh/ModApi/Forms/FormModel.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace {

constexpr const char* SaveSection = "forms";

struct RegisteredForm {
    std::string key;
    std::string label;
    std::string item;
    std::string modelPath;
    SOHFormDefinition definition = {};
    std::vector<SOHFormActionOverride> actions;
    std::vector<SOHFormAnimOverride> anims;
    PlayerAgeProperties body = {};
};

std::deque<RegisteredForm> sForms;
std::unordered_map<std::string, RegisteredForm*> sFormsByKey;
RegisteredForm* sActiveForm = nullptr;
std::string sPendingForm;

Player* GetPlayer() {
    return gPlayState == nullptr ? nullptr : GET_PLAYER(gPlayState);
}

bool IsActiveKind(SOHFormKind kind) {
    return sActiveForm != nullptr && sActiveForm->definition.kind == kind;
}

RegisteredForm* FindForm(const char* key) {
    if (key == nullptr) {
        return nullptr;
    }
    auto form = sFormsByKey.find(key);
    return form == sFormsByKey.end() ? nullptr : form->second;
}

bool IsFormItemOwned(const RegisteredForm* form) {
    return form->item.empty() || CustomItemRegistry_IsOwned(form->item.c_str());
}

const SOHFormDefinition* ActiveDefinition() {
    return sActiveForm == nullptr ? nullptr : &sActiveForm->definition;
}

constexpr int32_t EquipTypeCount = 4;
constexpr uint8_t FirstItemButton = 1;
constexpr uint8_t ButtonCount = 8;

struct StashedLoadout {
    std::array<bool, EquipTypeCount> hasEquip{};
    std::array<uint16_t, EquipTypeCount> equip{};
    uint8_t buttonB = ITEM_NONE;
    bool swordless = false;
    std::array<bool, ButtonCount> hasButton{};
    std::array<uint8_t, ButtonCount> buttonSlot{};
    std::array<std::string, ButtonCount> buttonKey{};
};

StashedLoadout sStash;

uint8_t SwordItemFor(uint16_t swordValue) {
    switch (swordValue) {
        case EQUIP_VALUE_SWORD_KOKIRI:
            return ITEM_SWORD_KOKIRI;
        case EQUIP_VALUE_SWORD_MASTER:
            return ITEM_SWORD_MASTER;
        case EQUIP_VALUE_SWORD_BIGGORON:
            return ITEM_SWORD_BGS;
        default:
            return ITEM_NONE;
    }
}

void ChangeSword(uint16_t swordValue) {
    Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, swordValue);
    gSaveContext.equips.buttonItems[0] = SwordItemFor(swordValue);
    if (swordValue == EQUIP_VALUE_SWORD_NONE) {
        Flags_SetInfTable(INFTABLE_SWORDLESS);
    } else {
        Flags_UnsetInfTable(INFTABLE_SWORDLESS);
    }
}

bool EnforceEquipment(const SOHFormDefinition& form) {
    if (form.resolveEquipment == nullptr) {
        return false;
    }
    bool changed = false;

    for (int32_t type = 0; type < EquipTypeCount; type++) {
        const uint16_t current = CUR_EQUIP_VALUE(type);
        const uint16_t wanted = form.resolveEquipment(type, current);
        if (wanted == current) {
            continue;
        }
        sStash.hasEquip[type] = true;
        sStash.equip[type] = current;
        if (type == EQUIP_TYPE_SWORD) {
            sStash.buttonB = gSaveContext.equips.buttonItems[0];
            sStash.swordless = Flags_GetInfTable(INFTABLE_SWORDLESS);
            ChangeSword(wanted);
        } else {
            Inventory_ChangeEquipment(static_cast<s16>(type), wanted);
        }
        changed = true;
    }
    return changed;
}

bool EnforceButtons(const RegisteredForm& form) {
    if (form.definition.allowsButtonItem == nullptr) {
        return false;
    }
    bool changed = false;

    for (uint8_t button = FirstItemButton; button < ButtonCount; button++) {
        const uint8_t item = gSaveContext.equips.buttonItems[button];
        const char* key = CustomItemRegistry_GetEquippedKey(button);
        const bool isOwnMask = key != nullptr && form.item == key;
        if (item == ITEM_NONE || isOwnMask || form.definition.allowsButtonItem(item, key)) {
            continue;
        }
        sStash.hasButton[button] = true;
        sStash.buttonSlot[button] = gSaveContext.equips.cButtonSlots[button - 1];
        sStash.buttonKey[button] = key != nullptr ? key : "";
        if (key != nullptr) {
            CustomItemRegistry_Unequip(button);
        }
        gSaveContext.equips.buttonItems[button] = ITEM_NONE;
        gSaveContext.equips.cButtonSlots[button - 1] = SLOT_NONE;
        changed = true;
    }
    return changed;
}

void RefreshEquipment(Player* player) {
    if (player != nullptr && gPlayState != nullptr) {
        Player_SetEquipmentData(gPlayState, player);
    }
}

void EnforceLoadout(Player* player) {
    if (sActiveForm == nullptr) {
        return;
    }
    const bool equipmentChanged = EnforceEquipment(sActiveForm->definition);
    const bool buttonsChanged = EnforceButtons(*sActiveForm);
    if (!equipmentChanged && !buttonsChanged) {
        return;
    }
    SPDLOG_INFO("[FormRegistry] '{}' took off what it does not allow", sActiveForm->key);
    RefreshEquipment(player);
}

void RefreshAgeProperties(Player* player) {
    if (player != nullptr) {
        player->ageProperties = Player_ResolveAgeProperties(player);
    }
}

void RestoreButton(uint8_t button) {
    if (!sStash.buttonKey[button].empty()) {
        CustomItemRegistry_Equip(button, sStash.buttonKey[button].c_str());
    } else {
        const uint8_t slot = sStash.buttonSlot[button];
        const uint8_t item = slot == SLOT_NONE ? ITEM_NONE : gSaveContext.inventory.items[slot];
        if (item == ITEM_NONE) {
            return;
        }
        gSaveContext.equips.buttonItems[button] = item;
        gSaveContext.equips.cButtonSlots[button - 1] = slot;
    }
    if (gPlayState != nullptr) {
        Interface_LoadItemIcon1(gPlayState, button);
    }
}

void RestoreLoadout(Player* player) {
    bool restored = false;

    for (int32_t type = 0; type < EquipTypeCount; type++) {
        if (!sStash.hasEquip[type]) {
            continue;
        }
        if (type == EQUIP_TYPE_SWORD) {
            Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, sStash.equip[type]);
            gSaveContext.equips.buttonItems[0] = sStash.buttonB;
            if (sStash.swordless) {
                Flags_SetInfTable(INFTABLE_SWORDLESS);
            } else {
                Flags_UnsetInfTable(INFTABLE_SWORDLESS);
            }
        } else {
            Inventory_ChangeEquipment(static_cast<s16>(type), sStash.equip[type]);
        }
        restored = true;
    }
    for (uint8_t button = FirstItemButton; button < ButtonCount; button++) {
        if (sStash.hasButton[button]) {
            RestoreButton(button);
            restored = true;
        }
    }
    sStash = {};
    if (!restored) {
        return;
    }

    SPDLOG_INFO("[FormRegistry] Gave Link back what the form took off");
    RefreshEquipment(player);
    if (gPlayState != nullptr && gSaveContext.equips.buttonItems[0] != ITEM_NONE) {
        Interface_LoadItemIcon1(gPlayState, 0);
    }
}

void ExitActiveForm() {
    if (sActiveForm == nullptr) {
        return;
    }
    RegisteredForm* leaving = sActiveForm;
    sActiveForm = nullptr;
    FormModel_SetActive(nullptr);
    PlayerInput_Release(leaving->key.c_str());
    Player* player = GetPlayer();
    if (player != nullptr && leaving->definition.onExit != nullptr) {
        leaving->definition.onExit(gPlayState, player);
    }
    RefreshAgeProperties(player);
    RestoreLoadout(player);
}

void EnterForm(RegisteredForm* form) {
    Player* player = GetPlayer();
    if (player == nullptr) {
        sPendingForm = form->key;
        return;
    }
    sPendingForm.clear();
    sActiveForm = form;
    FormModel_SetActive(&form->definition);
    if (form->definition.blockedButtons != 0) {
        PlayerInput_Block(form->key.c_str(), form->definition.blockedButtons, false);
    }
    RefreshAgeProperties(player);
    EnforceLoadout(player);
    if (form->definition.onEnter != nullptr) {
        form->definition.onEnter(gPlayState, player);
    }
}

bool IsValidDefinition(const SOHFormDefinition* definition) {
    if (definition == nullptr || definition->structSize < SOH_FORM_DEFINITION_MIN_SIZE || definition->key == nullptr ||
        definition->key[0] == '\0' || definition->label == nullptr || definition->kind > SOH_FORM_KIND_TAKEOVER) {
        SPDLOG_ERROR("[FormRegistry] Invalid form definition");
        return false;
    }
    if (sFormsByKey.contains(definition->key)) {
        SPDLOG_ERROR("[FormRegistry] Form '{}' is already registered", definition->key);
        return false;
    }
    if ((definition->actionCount != 0 && definition->actions == nullptr) ||
        (definition->animCount != 0 && definition->anims == nullptr)) {
        SPDLOG_ERROR("[FormRegistry] Form '{}' declares overrides without a table", definition->key);
        return false;
    }
    for (uint32_t i = 0; i < definition->actionCount; i++) {
        const SOHFormActionOverride& action = definition->actions[i];
        if (action.action < 0 || action.action >= SOH_PLAYER_ACTION_MAX || action.run == nullptr) {
            SPDLOG_ERROR("[FormRegistry] Form '{}' has an invalid action override {}", definition->key, i);
            return false;
        }
    }
    for (uint32_t i = 0; i < definition->animCount; i++) {
        const SOHFormAnimOverride& anim = definition->anims[i];
        if (anim.group < 0 || anim.group >= PLAYER_ANIMGROUP_MAX || anim.animType < SOH_FORM_ANIM_TYPE_ALL ||
            anim.animType >= PLAYER_ANIMTYPE_MAX || anim.anim == nullptr) {
            SPDLOG_ERROR("[FormRegistry] Form '{}' has an invalid animation override {}", definition->key, i);
            return false;
        }
    }
    return true;
}

void RunActionOverride(PlayState* play, Player* player, int32_t action, bool* consumed, bool* startedAction) {
    if (*consumed || sActiveForm == nullptr) {
        return;
    }
    for (const SOHFormActionOverride& override : sActiveForm->actions) {
        if (override.action != action) {
            continue;
        }
        const int32_t result = override.run(play, player);
        if (result == SOH_FORM_ACTION_VANILLA) {
            return;
        }
        *consumed = true;
        *startedAction = result == SOH_FORM_ACTION_STARTED;
        return;
    }
}

void ResolveAnimOverride(int32_t group, int32_t animType, LinkAnimationHeader** anim) {
    if (sActiveForm == nullptr) {
        return;
    }
    for (const SOHFormAnimOverride& override : sActiveForm->anims) {
        if (override.group == group && (override.animType == SOH_FORM_ANIM_TYPE_ALL || override.animType == animType)) {
            *anim = override.anim;
            return;
        }
    }
}

void ResolveBody(Player* player, PlayerAgeProperties** properties) {
    if (sActiveForm != nullptr && sActiveForm->definition.body != nullptr) {
        *properties = &sActiveForm->body;
    }
}

void ResolveHeight(Player* player, float* height) {
    if (sActiveForm != nullptr && sActiveForm->definition.height > 0.0f) {
        *height = sActiveForm->definition.height;
    }
}

void ResolveMotionScale(Player* player, int32_t kind, float* scale) {
    if (sActiveForm != nullptr && sActiveForm->definition.motionScale > 0.0f) {
        *scale *= sActiveForm->definition.motionScale;
    }
}

void UpdateForms() {
    Player* player = GetPlayer();
    if (player == nullptr) {
        return;
    }
    if (!sPendingForm.empty()) {
        RegisteredForm* pending = FindForm(sPendingForm.c_str());
        if (pending == nullptr || !IsFormItemOwned(pending)) {
            SPDLOG_WARN("[FormRegistry] Saved form '{}' is not available; staying as Link", sPendingForm);
            sPendingForm.clear();
        } else {
            EnterForm(pending);
        }
    }
    if (sActiveForm != nullptr && !IsFormItemOwned(sActiveForm)) {
        ExitActiveForm();
        return;
    }
    EnforceLoadout(player);
    if (IsActiveKind(SOH_FORM_KIND_LINK) && sActiveForm->definition.update != nullptr) {
        sActiveForm->definition.update(gPlayState, player);
    }
}

void DriveTakeoverFrame(GIVanillaBehavior flag, bool* should, va_list args) {
    if (!IsActiveKind(SOH_FORM_KIND_TAKEOVER)) {
        return;
    }
    *should = false;
    if (sActiveForm->definition.update != nullptr) {
        sActiveForm->definition.update(gPlayState, GetPlayer());
    }
}

void DrawTakeoverForm(Actor* actor, PlayState* play, bool* drawVanilla) {
    if (IsActiveKind(SOH_FORM_KIND_TAKEOVER) && sActiveForm->definition.draw != nullptr) {
        *drawVanilla = false;
        sActiveForm->definition.draw(play, (Player*)actor);
    }
}

void DrawLinkFormOverlay(Actor* actor, PlayState* play) {
    if (IsActiveKind(SOH_FORM_KIND_LINK) && sActiveForm->definition.draw != nullptr) {
        sActiveForm->definition.draw(play, (Player*)actor);
    }
}

std::string StashKey(const char* field, int32_t index) {
    return std::string("stash") + field + std::to_string(index);
}

void SaveStash() {
    SaveManager::Instance->SaveData("stashButtonB", sStash.buttonB);
    SaveManager::Instance->SaveData("stashSwordless", sStash.swordless);
    for (int32_t type = 0; type < EquipTypeCount; type++) {
        SaveManager::Instance->SaveData(StashKey("HasEquip", type), sStash.hasEquip[type]);
        SaveManager::Instance->SaveData(StashKey("Equip", type), sStash.equip[type]);
    }
    for (int32_t button = FirstItemButton; button < ButtonCount; button++) {
        SaveManager::Instance->SaveData(StashKey("HasButton", button), sStash.hasButton[button]);
        SaveManager::Instance->SaveData(StashKey("ButtonSlot", button), sStash.buttonSlot[button]);
        SaveManager::Instance->SaveData(StashKey("ButtonKey", button), sStash.buttonKey[button]);
    }
}

void LoadStash() {
    sStash = {};
    SaveManager::Instance->LoadData("stashButtonB", sStash.buttonB, static_cast<uint8_t>(ITEM_NONE));
    SaveManager::Instance->LoadData("stashSwordless", sStash.swordless, false);
    for (int32_t type = 0; type < EquipTypeCount; type++) {
        SaveManager::Instance->LoadData(StashKey("HasEquip", type), sStash.hasEquip[type], false);
        SaveManager::Instance->LoadData(StashKey("Equip", type), sStash.equip[type], static_cast<uint16_t>(0));
    }
    for (int32_t button = FirstItemButton; button < ButtonCount; button++) {
        SaveManager::Instance->LoadData(StashKey("HasButton", button), sStash.hasButton[button], false);
        SaveManager::Instance->LoadData(StashKey("ButtonSlot", button), sStash.buttonSlot[button],
                                        static_cast<uint8_t>(SLOT_NONE));
        SaveManager::Instance->LoadData(StashKey("ButtonKey", button), sStash.buttonKey[button], std::string());
    }
}

void ResetSaveState(bool isDebug) {
    sStash = {};
    ExitActiveForm();
    sPendingForm.clear();
}

void SaveState(SaveContext* saveContext, int sectionId, bool fullSave) {
    std::string active = sActiveForm != nullptr ? sActiveForm->key : sPendingForm;
    SaveManager::Instance->SaveData("active", active);
    SaveStash();
}

void LoadState() {
    sActiveForm = nullptr;
    SaveManager::Instance->LoadData("active", sPendingForm, std::string());
    LoadStash();
}

int32_t FormCommand(std::shared_ptr<Ship::Console> console, std::vector<std::string> args, std::string* output) {
    if (args.size() != 2) {
        if (output != nullptr) {
            *output = "Usage: form \"<key>\" | form link";
        }
        return 1;
    }
    const std::string key = ModApi_UnquoteConsoleArgument(args[1]);
    const bool toLink = key == "link";
    const bool changed = FormRegistry_SetActive(toLink ? nullptr : key.c_str());
    if (output != nullptr) {
        *output = changed ? "Form: " + key : "Unknown form " + key;
    }
    return changed ? 0 : 1;
}

} // namespace

void FormRegistry_Init() {
    FormTransform_Init();
    FormModel_Init();
    SaveManager::Instance->AddInitFunction(ResetSaveState);
    SaveManager::Instance->AddSaveFunction(SaveSection, 1, SaveState, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(SaveSection, 1, LoadState);

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerActionHandler>(RunActionOverride);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerResolveAnim>(ResolveAnimOverride);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerResolveAgeProperties>(ResolveBody);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerResolveHeight>(ResolveHeight);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnPlayerResolveMotionScale>(
        SOH_PLAYER_MOTION_STICK_SPEED, ResolveMotionScale);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(UpdateForms);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnVanillaBehavior>(VB_EXECUTE_PLAYER_ACTION_FUNC,
                                                                                       DriveTakeoverFrame);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorDraw>(ACTOR_PLAYER, DrawTakeoverForm);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorDrawEnd>(ACTOR_PLAYER, DrawLinkFormOverlay);

    Ship::Context::GetInstance()->GetConsole()->AddCommand(
        "form", { FormCommand, "Switches the player to a registered form", { { "key", Ship::ArgumentType::TEXT } } });
}

bool FormRegistry_Register(const SOHFormDefinition* definition) {
    if (!IsValidDefinition(definition)) {
        return false;
    }
    RegisteredForm& form = sForms.emplace_back();
    std::memcpy(&form.definition, definition, std::min<size_t>(definition->structSize, sizeof(form.definition)));
    form.definition.structSize = sizeof(form.definition);
    form.key = definition->key;
    form.label = definition->label;
    form.item = form.definition.item == nullptr ? "" : form.definition.item;
    form.modelPath = form.definition.modelPath == nullptr ? "" : form.definition.modelPath;
    form.definition.key = form.key.c_str();
    form.definition.label = form.label.c_str();
    form.definition.item = form.item.empty() ? nullptr : form.item.c_str();
    form.definition.modelPath = form.modelPath.empty() ? nullptr : form.modelPath.c_str();
    form.actions.assign(definition->actions, definition->actions + definition->actionCount);
    form.anims.assign(definition->anims, definition->anims + definition->animCount);
    form.definition.actions = form.actions.data();
    form.definition.anims = form.anims.data();
    if (definition->body != nullptr) {
        form.body = *definition->body;
        form.definition.body = &form.body;
    }
    sFormsByKey[form.key] = &form;
    SPDLOG_INFO("[FormRegistry] Registered form '{}'", form.key);
    return true;
}

const SOHFormDefinition* FormRegistry_Find(const char* key) {
    RegisteredForm* form = FindForm(key);
    return form == nullptr ? nullptr : &form->definition;
}

uint32_t FormRegistry_GetCount(void) {
    return static_cast<uint32_t>(sForms.size());
}

const SOHFormDefinition* FormRegistry_GetAt(uint32_t index) {
    return index < sForms.size() ? &sForms[index].definition : nullptr;
}

bool FormRegistry_SetActive(const char* key) {
    if (key == nullptr || key[0] == '\0') {
        if (sActiveForm != nullptr && FormTransform_Begin(ActiveDefinition(), nullptr)) {
            return true;
        }
        ExitActiveForm();
        sPendingForm.clear();
        return true;
    }
    RegisteredForm* form = FindForm(key);
    if (form == nullptr) {
        return false;
    }
    if (!IsFormItemOwned(form)) {
        SPDLOG_WARN("[FormRegistry] Form '{}' needs its item '{}'", form->key, form->item);
        return false;
    }
    if (form == sActiveForm) {
        return true;
    }
    if (FormTransform_Begin(ActiveDefinition(), &form->definition)) {
        return true;
    }
    ExitActiveForm();
    EnterForm(form);
    return true;
}

bool FormRegistry_ApplyActive(const char* key) {
    if (key == nullptr || key[0] == '\0') {
        ExitActiveForm();
        sPendingForm.clear();
        return true;
    }
    RegisteredForm* form = FindForm(key);
    if (form == nullptr) {
        return false;
    }
    if (form != sActiveForm) {
        ExitActiveForm();
        EnterForm(form);
    }
    return true;
}

bool FormRegistry_Toggle(const char* key) {
    return FormRegistry_IsActive(key) ? FormRegistry_SetActive(nullptr) : FormRegistry_SetActive(key);
}

const char* FormRegistry_GetActiveKey(void) {
    return sActiveForm == nullptr ? nullptr : sActiveForm->key.c_str();
}

bool FormRegistry_IsActive(const char* key) {
    return key != nullptr && sActiveForm != nullptr && sActiveForm->key == key;
}
