#include "ModStorage.h"

#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "soh/SaveManager.h"

namespace {

constexpr size_t KeyLimit = 256;
constexpr size_t ValueLimit = 65536;
constexpr size_t EntryLimit = 4096;
constexpr const char* SaveSection = "modStorage";

using Entries = std::map<std::string, std::string>;
std::map<std::string, Entries> sStorage;

bool ValidKey(const char* key) {
    return key != nullptr && key[0] != '\0' && strnlen(key, KeyLimit) < KeyLimit;
}

const std::string* Find(const char* mod, const char* key) {
    if (!ValidKey(mod) || !ValidKey(key)) {
        return nullptr;
    }
    auto owner = sStorage.find(mod);
    if (owner == sStorage.end()) {
        return nullptr;
    }
    auto entry = owner->second.find(key);
    return entry == owner->second.end() ? nullptr : &entry->second;
}

size_t Count() {
    size_t result = 0;
    for (const auto& owner : sStorage) {
        result += owner.second.size();
    }
    return result;
}

char HexDigit(uint8_t value) {
    return value < 10 ? (char)('0' + value) : (char)('a' + value - 10);
}

int HexValue(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

std::string Encode(const std::string& value) {
    std::string result;
    result.reserve(value.size() * 2);
    for (uint8_t byte : value) {
        result.push_back(HexDigit(byte >> 4));
        result.push_back(HexDigit(byte & 15));
    }
    return result;
}

bool Decode(const std::string& value, std::string& result) {
    if ((value.size() & 1) != 0 || value.size() > ValueLimit * 2) {
        return false;
    }
    result.clear();
    result.reserve(value.size() / 2);
    for (size_t index = 0; index < value.size(); index += 2) {
        int high = HexValue(value[index]);
        int low = HexValue(value[index + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        result.push_back((char)((high << 4) | low));
    }
    return true;
}

void Reset(bool isDebug) {
    sStorage.clear();
}

void Save(SaveContext* saveContext, int sectionId, bool fullSave) {
    struct Entry {
        const std::string* mod;
        const std::string* key;
        const std::string* value;
    };
    std::vector<Entry> entries;
    entries.reserve(Count());
    for (const auto& [mod, values] : sStorage) {
        for (const auto& [key, value] : values) {
            entries.push_back({ &mod, &key, &value });
        }
    }
    SaveManager::Instance->SaveArray("entries", entries.size(), [&entries](size_t index) {
        SaveManager::Instance->SaveStruct("", [&entries, index]() {
            SaveManager::Instance->SaveData("mod", *entries[index].mod);
            SaveManager::Instance->SaveData("key", *entries[index].key);
            SaveManager::Instance->SaveData("value", Encode(*entries[index].value));
        });
    });
}

void Load() {
    sStorage.clear();
    SaveManager::Instance->LoadArray("entries", EntryLimit, [](size_t) {
        SaveManager::Instance->LoadStruct("", []() {
            std::string mod;
            std::string key;
            std::string encoded;
            std::string value;
            SaveManager::Instance->LoadData("mod", mod, std::string());
            SaveManager::Instance->LoadData("key", key, std::string());
            SaveManager::Instance->LoadData("value", encoded, std::string());
            if (ValidKey(mod.c_str()) && ValidKey(key.c_str()) && Decode(encoded, value)) {
                sStorage[mod][key] = std::move(value);
            }
        });
    });
}

} // namespace

void ModStorage_Init() {
    SaveManager::Instance->AddInitFunction(Reset);
    SaveManager::Instance->AddSaveFunction(SaveSection, 1, Save, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(SaveSection, 1, Load);
}

bool ModStorage_Has(const char* mod, const char* key) {
    return Find(mod, key) != nullptr;
}

uint32_t ModStorage_GetSize(const char* mod, const char* key) {
    const auto* value = Find(mod, key);
    return value == nullptr ? 0 : (uint32_t)value->size();
}

uint32_t ModStorage_Get(const char* mod, const char* key, void* output, uint32_t capacity) {
    const auto* value = Find(mod, key);
    if (value == nullptr || output == nullptr || capacity == 0) {
        return 0;
    }
    uint32_t size = value->size() < capacity ? (uint32_t)value->size() : capacity;
    memcpy(output, value->data(), size);
    return size;
}

bool ModStorage_Set(const char* mod, const char* key, const void* data, uint32_t size) {
    if (!ValidKey(mod) || !ValidKey(key) || size > ValueLimit || (size != 0 && data == nullptr) ||
        (Find(mod, key) == nullptr && Count() >= EntryLimit)) {
        return false;
    }
    auto& value = sStorage[mod][key];
    if (size == 0) {
        value.clear();
    } else {
        value.assign((const char*)data, size);
    }
    return true;
}

bool ModStorage_Remove(const char* mod, const char* key) {
    if (Find(mod, key) == nullptr) {
        return false;
    }
    auto& entries = sStorage[mod];
    entries.erase(key);
    if (entries.empty()) {
        sStorage.erase(mod);
    }
    return true;
}

uint32_t ModStorage_GetString(const char* mod, const char* key, char* output, uint32_t capacity) {
    if (output == nullptr || capacity == 0) {
        return 0;
    }
    uint32_t size = ModStorage_Get(mod, key, output, capacity - 1);
    output[size] = '\0';
    return size;
}

bool ModStorage_SetString(const char* mod, const char* key, const char* value) {
    return value != nullptr && ModStorage_Set(mod, key, value, (uint32_t)strlen(value));
}
