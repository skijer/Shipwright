#include "ModLabels.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <optional>
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
extern char itemTimestampDisplayName[TIMESTAMP_MAX][24]; // as gameplaystats.cpp defines it (MSVC links by type)

namespace {

// The gameplay stats set their timestamp names when their window starts; a mod's are kept to be put back after
std::map<int32_t, std::string> sTimestamps;
// itemMapping entries as SoH made them, saved the first time a mod changes one
std::map<uint32_t, ItemMapEntry> sOriginalIcons;

// Everything a mod can relabel, to swap the game's labels with a mod quest's world (ModQuests)
struct Labels {
    std::vector<std::string> names[7];
    char timestamps[TIMESTAMP_MAX][24];
    std::map<int32_t, std::string> modTimestamps;
    std::vector<TimeSplits::TimesplitObject> splits;
    std::vector<TimeSplits::TimesplitObject> sceneSplits;
    std::map<uint32_t, std::vector<uint32_t>> splitGroups;
};

// The labels before the first mod changed one
std::optional<Labels> sGameLabels;
// A quest world's labels while another world is played, by quest key
std::map<std::string, Labels> sWorldLabels;

std::vector<std::string>* const kNameLists[7] = { &itemNamesEng,      &itemNamesGer,      &itemNamesFra, &questItemNamesEng,
                                                  &questItemNamesGer, &questItemNamesFra, &sceneNames };

Labels Take() {
    Labels labels;
    for (size_t i = 0; i < 7; i++) {
        labels.names[i] = *kNameLists[i];
    }
    std::memcpy(labels.timestamps, itemTimestampDisplayName, sizeof(labels.timestamps));
    labels.modTimestamps = sTimestamps;
    labels.splits = TimeSplits::splitObjectList;
    labels.sceneSplits = TimeSplits::sceneObjectList;
    labels.splitGroups = TimeSplits::itemSubMenuList;
    return labels;
}

void Put(const Labels& labels) {
    for (size_t i = 0; i < 7; i++) {
        *kNameLists[i] = labels.names[i];
    }
    std::memcpy(itemTimestampDisplayName, labels.timestamps, sizeof(labels.timestamps));
    sTimestamps = labels.modTimestamps;
    TimeSplits::splitObjectList = labels.splits;
    TimeSplits::sceneObjectList = labels.sceneSplits;
    TimeSplits::itemSubMenuList = labels.splitGroups;
}

void KeepGameLabels() {
    if (!sGameLabels.has_value()) {
        sGameLabels = Take();
    }
}

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
    KeepGameLabels();
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

// Replaces the splits SoH offers (its item and boss list, and its scene list); a split already offered (same type and
// id) keeps its times
extern "C" bool ModLabels_SetTimeSplits(const SOHTimeSplit* splits, uint32_t count) {
    std::vector<TimeSplits::TimesplitObject> objects;
    std::vector<TimeSplits::TimesplitObject> scenes;

    if ((splits == nullptr) || (count == 0)) {
        return false;
    }
    KeepGameLabels();
    for (uint32_t i = 0; i < count; i++) {
        const SOHTimeSplit& split = splits[i];
        TimeSplits::TimesplitObject object = {};
        std::vector<TimeSplits::TimesplitObject>& current =
            (split.type == TimeSplits::SPLIT_TYPE_SCENE) ? TimeSplits::sceneObjectList : TimeSplits::splitObjectList;

        if (split.type > TimeSplits::SPLIT_TYPE_BOSS) {
            return false;
        }
        object.splitType = split.type;
        object.splitId = split.id;
        object.splitName = (split.name != nullptr) ? split.name : "";
        object.splitStatus = TimeSplits::SPLIT_INACTIVE;
        for (const TimeSplits::TimesplitObject& old : current) {
            if ((old.splitType == object.splitType) && (old.splitId == object.splitId)) {
                object.splitCurrentTime = old.splitCurrentTime;
                object.splitPreviousBest = old.splitPreviousBest;
                object.splitStatus = old.splitStatus;
                break;
            }
        }
        ((split.type == TimeSplits::SPLIT_TYPE_SCENE) ? scenes : objects).push_back(object);
    }
    TimeSplits::splitObjectList = objects;
    TimeSplits::sceneObjectList = scenes;
    return true;
}

// The items an item split's popup offers to pick from (SoH's bottles, swords...); no members removes the group
extern "C" bool ModLabels_SetTimeSplitGroup(uint32_t id, const uint32_t* members, uint32_t count) {
    if ((count != 0) && (members == nullptr)) {
        return false;
    }
    KeepGameLabels();
    if (count == 0) {
        TimeSplits::itemSubMenuList.erase(id);
    } else {
        TimeSplits::itemSubMenuList[id] = std::vector<uint32_t>(members, members + count);
    }
    return true;
}

extern "C" void ModLabels_ClearTimeSplitGroups(void) {
    KeepGameLabels();
    TimeSplits::itemSubMenuList.clear();
}

// A quest's world is left: its labels are kept and the game's put back
void ModLabels_LeaveWorld(const std::string& quest) {
    if (sGameLabels.has_value()) {
        sWorldLabels[quest] = Take();
        Put(*sGameLabels);
    }
}

// A quest's world comes back on top: the labels its mods set come back
void ModLabels_EnterWorld(const std::string& quest) {
    auto labels = sWorldLabels.find(quest);
    if (labels != sWorldLabels.end()) {
        Put(labels->second);
    }
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
