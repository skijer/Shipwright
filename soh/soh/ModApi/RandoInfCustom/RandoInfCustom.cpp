#include "soh/ModApi/RandoInfCustom/RandoInfCustom.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "soh/SaveManager.h"

namespace {

constexpr size_t SaveFlagLimit = 4096;
constexpr const char* SaveSection = "randoInfCustom";

std::vector<std::string> sKeys;
std::unordered_map<std::string, uint16_t> sKeysToFlags;
std::array<std::vector<bool>, SOH_RANDO_INF_BANK_MAX> sBanks;
std::vector<std::string> sKeysOfUnloadedMods;

bool IsBankValid(SOHRandoInfBank bank) {
    return static_cast<int>(bank) >= 0 && bank < SOH_RANDO_INF_BANK_MAX;
}

bool IsFlagValid(uint16_t flag) {
    return flag != RANDO_INF_CUSTOM_NONE && flag <= sKeys.size();
}

void ClaimUnresolvedKey(const std::string& key, uint16_t flag) {
    for (size_t index = 0; index < sKeysOfUnloadedMods.size(); index++) {
        if (sKeysOfUnloadedMods[index] != key) {
            continue;
        }
        sKeysOfUnloadedMods.erase(sKeysOfUnloadedMods.begin() + index);
        sBanks[SOH_RANDO_INF_SAVE][flag - 1] = true;
        return;
    }
}

void SaveState(SaveContext* saveContext, int sectionId, bool fullSave) {
    std::vector<std::string> set = sKeysOfUnloadedMods;
    for (uint16_t flag = 1; flag <= sKeys.size(); flag++) {
        if (sBanks[SOH_RANDO_INF_SAVE][flag - 1]) {
            set.push_back(sKeys[flag - 1]);
        }
    }
    SaveManager::Instance->SaveArray("set", set.size(),
                                     [&set](size_t index) { SaveManager::Instance->SaveData("", set[index]); });
}

void LoadState() {
    RandoInfCustom_ClearBank(SOH_RANDO_INF_SAVE);
    sKeysOfUnloadedMods.clear();
    SaveManager::Instance->LoadArray("set", SaveFlagLimit, [](size_t index) {
        std::string key;
        SaveManager::Instance->LoadData("", key, std::string());
        if (key.empty()) {
            return;
        }
        uint16_t flag = RandoInfCustom_Find(key.c_str());
        if (flag == RANDO_INF_CUSTOM_NONE) {
            sKeysOfUnloadedMods.push_back(std::move(key));
            return;
        }
        sBanks[SOH_RANDO_INF_SAVE][flag - 1] = true;
    });
}

void ResetSaveState(bool isDebug) {
    RandoInfCustom_ClearBank(SOH_RANDO_INF_SAVE);
    sKeysOfUnloadedMods.clear();
}

} // namespace

uint16_t RandoInfCustom_Register(const char* key) {
    if (key == nullptr || key[0] == '\0') {
        SPDLOG_ERROR("Rejected a custom randomizer flag with no key");
        return RANDO_INF_CUSTOM_NONE;
    }
    std::string name = key;
    auto found = sKeysToFlags.find(name);
    if (found != sKeysToFlags.end()) {
        return found->second;
    }
    if (sKeys.size() >= UINT16_MAX - 1) {
        SPDLOG_ERROR("Ran out of custom randomizer flags registering {}", name);
        return RANDO_INF_CUSTOM_NONE;
    }

    sKeys.push_back(name);
    uint16_t flag = static_cast<uint16_t>(sKeys.size());
    sKeysToFlags[name] = flag;
    for (std::vector<bool>& bank : sBanks) {
        bank.resize(sKeys.size(), false);
    }
    ClaimUnresolvedKey(name, flag);
    return flag;
}

uint16_t RandoInfCustom_Find(const char* key) {
    if (key == nullptr) {
        return RANDO_INF_CUSTOM_NONE;
    }
    auto found = sKeysToFlags.find(key);
    return found != sKeysToFlags.end() ? found->second : RANDO_INF_CUSTOM_NONE;
}

const char* RandoInfCustom_GetKey(uint16_t flag) {
    return IsFlagValid(flag) ? sKeys[flag - 1].c_str() : nullptr;
}

uint16_t RandoInfCustom_GetCount(void) {
    return static_cast<uint16_t>(sKeys.size());
}

bool RandoInfCustom_Get(SOHRandoInfBank bank, uint16_t flag) {
    if (!IsBankValid(bank) || !IsFlagValid(flag)) {
        return false;
    }
    return sBanks[bank][flag - 1];
}

void RandoInfCustom_Set(SOHRandoInfBank bank, uint16_t flag, bool state) {
    if (!IsBankValid(bank) || !IsFlagValid(flag)) {
        return;
    }
    sBanks[bank][flag - 1] = state;
}

void RandoInfCustom_ClearBank(SOHRandoInfBank bank) {
    if (!IsBankValid(bank)) {
        return;
    }
    sBanks[bank].assign(sKeys.size(), false);
}

bool RandoInfCustom_GetByKey(const char* key) {
    return RandoInfCustom_Get(SOH_RANDO_INF_SAVE, RandoInfCustom_Find(key));
}

void RandoInfCustom_SetByKey(const char* key, bool state) {
    RandoInfCustom_Set(SOH_RANDO_INF_SAVE, RandoInfCustom_Register(key), state);
}

void RandoInfCustom_Init() {
    SaveManager::Instance->AddInitFunction(ResetSaveState);
    SaveManager::Instance->AddSaveFunction(SaveSection, 1, SaveState, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(SaveSection, 1, LoadState);
}
