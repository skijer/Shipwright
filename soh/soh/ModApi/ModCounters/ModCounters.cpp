#include "soh/ModApi/ModCounters/ModCounters.h"

#include <algorithm>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "soh/SaveManager.h"

namespace {

constexpr size_t SaveCounterLimit = 4096;
constexpr const char* SaveSection = "modCounters";

std::vector<ModCounter> sCounters;
std::unordered_map<std::string, int32_t> sValuesOfUnloadedMods;

ModCounter* FindCounter(const char* key) {
    if (key == nullptr) {
        return nullptr;
    }
    auto found = std::find_if(sCounters.begin(), sCounters.end(),
                              [key](const ModCounter& counter) { return counter.key == key; });
    return found != sCounters.end() ? &*found : nullptr;
}

int32_t Clamp(const ModCounter& counter, int32_t value) {
    return std::clamp(value, 0, counter.max);
}

void ResetValues() {
    for (ModCounter& counter : sCounters) {
        counter.value = 0;
    }
    sValuesOfUnloadedMods.clear();
}

void KeepValue(const std::string& key, int32_t value) {
    ModCounter* counter = FindCounter(key.c_str());
    if (counter == nullptr) {
        sValuesOfUnloadedMods[key] = value;
        return;
    }
    counter->value = Clamp(*counter, value);
}

void SaveState(const SaveContext& saveContext, int sectionId, bool fullSave) {
    std::vector<std::pair<std::string, int32_t>> entries(sValuesOfUnloadedMods.begin(), sValuesOfUnloadedMods.end());
    for (const ModCounter& counter : sCounters) {
        if (counter.value != 0) {
            entries.emplace_back(counter.key, counter.value);
        }
    }
    SaveManager::Instance->SaveArray("values", entries.size(), [&entries](size_t index) {
        SaveManager::Instance->SaveStruct("", [&entries, index]() {
            SaveManager::Instance->SaveData("key", entries[index].first);
            SaveManager::Instance->SaveData("value", entries[index].second);
        });
    });
}

void LoadState() {
    ResetValues();
    SaveManager::Instance->LoadArray("values", SaveCounterLimit, [](size_t index) {
        SaveManager::Instance->LoadStruct("", []() {
            std::string key;
            int32_t value = 0;
            SaveManager::Instance->LoadData("key", key, std::string());
            SaveManager::Instance->LoadData("value", value, 0);
            if (!key.empty()) {
                KeepValue(key, value);
            }
        });
    });
}

void ResetSaveState(bool isDebug) {
    ResetValues();
}

} // namespace

bool ModCounters_Register(const char* key, int32_t max) {
    if (key == nullptr || key[0] == '\0' || max <= 0) {
        SPDLOG_ERROR("Rejected a mod counter with no key or no range");
        return false;
    }
    ModCounter* existing = FindCounter(key);
    if (existing != nullptr) {
        existing->max = max;
        existing->value = Clamp(*existing, existing->value);
        return true;
    }
    sCounters.push_back({ key, max, 0 });
    auto unloaded = sValuesOfUnloadedMods.find(key);
    if (unloaded != sValuesOfUnloadedMods.end()) {
        sCounters.back().value = Clamp(sCounters.back(), unloaded->second);
        sValuesOfUnloadedMods.erase(unloaded);
    }
    return true;
}

int32_t ModCounters_Get(const char* key) {
    const ModCounter* counter = FindCounter(key);
    return counter != nullptr ? counter->value : 0;
}

void ModCounters_Set(const char* key, int32_t value) {
    ModCounter* counter = FindCounter(key);
    if (counter == nullptr) {
        SPDLOG_ERROR("Set the unregistered mod counter {}", key != nullptr ? key : "(null)");
        return;
    }
    counter->value = Clamp(*counter, value);
}

const std::vector<ModCounter>& ModCounters_List() {
    return sCounters;
}

void ModCounters_Init() {
    SaveManager::Instance->AddInitFunction(ResetSaveState);
    SaveManager::Instance->AddSaveFunction(SaveSection, 1, SaveState, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(SaveSection, 1, LoadState);
}
