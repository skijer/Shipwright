#include "ModLabels.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "soh/Enhancements/gameplaystats.h"
#include "soh/Enhancements/timesplits/TimeSplits.h"
#include "soh/SohGui/ImGuiUtils.h"

extern "C" {
#include "z64.h"
}

extern std::vector<std::string> itemNamesEng;
extern std::vector<std::string> itemNamesGer;
extern std::vector<std::string> itemNamesFra;
extern std::vector<std::string> questItemNamesEng;
extern std::vector<std::string> questItemNamesGer;
extern std::vector<std::string> questItemNamesFra;
extern std::vector<std::string> sceneNames;
extern char itemTimestampDisplayName[TIMESTAMP_MAX][21];
extern std::vector<SplitObject> splitObjectList;
extern std::map<uint32_t, std::vector<uint32_t>> popupList;

namespace {

// The gameplay stats set their timestamp names when their window starts; a mod's are kept to be put back after
std::map<int32_t, std::string> sTimestamps;
// itemMapping entries as SoH made them, saved the first time a mod changes one
std::map<uint32_t, ItemMapEntry> sOriginalIcons;

std::vector<std::string>* NamesFor(SOHLabelKind kind, int32_t language) {
    switch (kind) {
        case SOH_LABEL_ITEM:
            return (language == LANGUAGE_GER)   ? &itemNamesGer
                   : (language == LANGUAGE_FRA) ? &itemNamesFra
                                                : &itemNamesEng;
        case SOH_LABEL_QUEST_ITEM:
            return (language == LANGUAGE_GER)   ? &questItemNamesGer
                   : (language == LANGUAGE_FRA) ? &questItemNamesFra
                                                : &questItemNamesEng;
        case SOH_LABEL_SCENE:
            return &sceneNames;
        default:
            return nullptr;
    }
}

// SetupDisplayNames' layout: "Name:" left-aligned in 20
void WriteTimestamp(int32_t index, const std::string& name) {
    std::string text = name.substr(0, 19) + ":";

    snprintf(itemTimestampDisplayName[index], sizeof(itemTimestampDisplayName[index]), "%-20s", text.c_str());
}

} // namespace

extern "C" bool ModLabels_Set(SOHLabelKind kind, int32_t language, int32_t index, const char* name) {
    if ((name == nullptr) || (index < 0)) {
        return false;
    }
    if (kind == SOH_LABEL_TIMESTAMP) {
        if (index >= TIMESTAMP_MAX) {
            return false;
        }
        sTimestamps[index] = name;
        WriteTimestamp(index, name);
        return true;
    }
    std::vector<std::string>* names = NamesFor(kind, language);
    if ((names == nullptr) || ((size_t)index >= names->size())) {
        return false;
    }
    (*names)[index] = name;
    return true;
}

extern "C" void ModLabels_ApplyTimestamps(void) {
    for (const auto& [index, name] : sTimestamps) {
        WriteTimestamp(index, name);
    }
}

// Replaces SoH's list; a split already in it (same type and id) keeps its times
extern "C" bool ModLabels_SetTimeSplits(const SOHTimeSplit* splits, uint32_t count) {
    std::vector<SplitObject> list;

    if ((splits == nullptr) || (count == 0)) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        const SOHTimeSplit& split = splits[i];
        SplitObject object = {};

        if (split.type > SPLIT_TYPE_MISC) {
            return false;
        }
        object.splitType = split.type;
        object.splitID = split.id;
        object.splitName = (split.name != nullptr) ? split.name : "";
        object.splitImage = (split.image != nullptr) ? split.image : "";
        object.splitTint = ImVec4(split.tint[0], split.tint[1], split.tint[2], split.tint[3]);
        for (const SplitObject& current : splitObjectList) {
            if ((current.splitType == object.splitType) && (current.splitID == object.splitID)) {
                object.splitTimeCurrent = current.splitTimeCurrent;
                object.splitTimeBest = current.splitTimeBest;
                object.splitTimePreviousBest = current.splitTimePreviousBest;
                object.splitTimeStatus = current.splitTimeStatus;
                object.splitSkullTokenCount = current.splitSkullTokenCount;
                break;
            }
        }
        list.push_back(object);
    }
    splitObjectList = list;
    return true;
}

// The splits a split's popup offers to pick from (SoH's bottles, swords...); no members removes the group
extern "C" bool ModLabels_SetTimeSplitGroup(uint32_t id, const uint32_t* members, uint32_t count) {
    if ((count != 0) && (members == nullptr)) {
        return false;
    }
    if (count == 0) {
        popupList.erase(id);
    } else {
        popupList[id] = std::vector<uint32_t>(members, members + count);
    }
    return true;
}

extern "C" void ModLabels_ClearTimeSplitGroups(void) {
    popupList.clear();
}

// SoH's windows draw item's icon as source's (as SoH made source's); source == item puts item's own back
extern "C" bool ModLabels_SetItemIcon(uint32_t item, uint32_t source) {
    auto target = itemMapping.find(item);
    auto from = itemMapping.find(source);

    if ((target == itemMapping.end()) || (from == itemMapping.end())) {
        return false;
    }
    sOriginalIcons.try_emplace(item, target->second);
    sOriginalIcons.try_emplace(source, from->second);

    const ItemMapEntry& icon = sOriginalIcons.at(source);
    target->second.name = icon.name;
    target->second.nameFaded = icon.nameFaded;
    target->second.texturePath = icon.texturePath;
    return true;
}
