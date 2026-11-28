#include "ModLabels.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "soh/Enhancements/gameplaystats.h"
#include "soh/Enhancements/timesplits/TimeSplits.h"
#include "soh/ModApi/ModPermissions/ModPermissions.h"
#include "soh/ModApi/ModQuests/ModQuests.h"
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
extern char itemTimestampDisplayName[TIMESTAMP_MAX][24];

namespace {

std::map<int32_t, std::string> sTimestamps;
std::map<uint32_t, ItemMapEntry> sOriginalIcons;

struct RecordedSplit {
    uint32_t type;
    uint32_t id;
    std::string name;
};

// A quest mod's labels, latest value per label, applied only while the quest is awake.
struct QuestLabels {
    std::map<std::tuple<SOHLabelKind, int32_t, int32_t>, std::string> labels;
    std::optional<std::vector<RecordedSplit>> splits;
    bool clearsSplitGroups = false;
    std::map<uint32_t, std::vector<uint32_t>> splitGroups;
    std::map<uint32_t, uint32_t> icons;
};

struct LabelState {
    std::vector<std::string> names[7];
    char timestamps[TIMESTAMP_MAX][24];
    std::map<int32_t, std::string> modTimestamps;
    std::vector<TimeSplits::TimesplitObject> splits;
    std::vector<TimeSplits::TimesplitObject> sceneSplits;
    std::map<uint32_t, std::vector<uint32_t>> splitGroups;
    std::map<uint32_t, ItemMapEntry> icons;
};

std::map<std::string, QuestLabels> sQuestLabels;
std::map<std::string, LabelState> sStateBeforeQuest;

std::vector<std::string>* const kNameLists[7] = { &itemNamesEng,      &itemNamesGer,      &itemNamesFra, &questItemNamesEng,
                                                  &questItemNamesGer, &questItemNamesFra, &sceneNames };

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

LabelState TakeLabelState() {
    LabelState state;
    for (size_t i = 0; i < 7; i++) {
        state.names[i] = *kNameLists[i];
    }
    std::memcpy(state.timestamps, itemTimestampDisplayName, sizeof(state.timestamps));
    state.modTimestamps = sTimestamps;
    state.splits = TimeSplits::splitObjectList;
    state.sceneSplits = TimeSplits::sceneObjectList;
    state.splitGroups = TimeSplits::itemSubMenuList;
    state.icons = itemMapping;
    return state;
}

void PutLabelState(const LabelState& state) {
    for (size_t i = 0; i < 7; i++) {
        *kNameLists[i] = state.names[i];
    }
    std::memcpy(itemTimestampDisplayName, state.timestamps, sizeof(state.timestamps));
    sTimestamps = state.modTimestamps;
    TimeSplits::splitObjectList = state.splits;
    TimeSplits::sceneObjectList = state.sceneSplits;
    TimeSplits::itemSubMenuList = state.splitGroups;
    itemMapping = state.icons;
}

// The quest of the mod calling, or "" when the caller is the host or a mod without a quest.
std::string GetCallingQuest(const void* caller) {
    const ModIdentity* mod = ModPermissions_FindCaller(caller);
    const char* quest = mod != nullptr ? ModQuests_GetQuestOfMod(mod->name) : nullptr;
    return quest != nullptr ? quest : "";
}

// A quest's labels show only after the host wakes it: during its ModInit they are recorded, not written.
bool IsAppliedNow(const std::string& quest) {
    return quest.empty() || sStateBeforeQuest.contains(quest);
}

void WriteTimestamp(int32_t index, const std::string& name) {
    std::string text = name.substr(0, 19) + ":";

    snprintf(itemTimestampDisplayName[index], sizeof(itemTimestampDisplayName[index]), "%-20s", text.c_str());
}

bool IsValidLabel(SOHLabelKind kind, int32_t language, int32_t index, const char* name) {
    if ((name == nullptr) || (index < 0)) {
        return false;
    }
    if (kind == SOH_LABEL_TIMESTAMP) {
        return index < TIMESTAMP_MAX;
    }
    std::vector<std::string>* names = NamesFor(kind, language);
    return (names != nullptr) && ((size_t)index < names->size());
}

void WriteLabel(SOHLabelKind kind, int32_t language, int32_t index, const std::string& name) {
    if (kind == SOH_LABEL_TIMESTAMP) {
        sTimestamps[index] = name;
        WriteTimestamp(index, name);
        return;
    }
    (*NamesFor(kind, language))[index] = name;
}

std::vector<RecordedSplit> RecordSplits(const SOHTimeSplit* splits, uint32_t count) {
    std::vector<RecordedSplit> recorded;
    for (uint32_t i = 0; i < count; i++) {
        recorded.push_back({ splits[i].type, splits[i].id, (splits[i].name != nullptr) ? splits[i].name : "" });
    }
    return recorded;
}

void WriteSplits(const std::vector<RecordedSplit>& splits) {
    std::vector<TimeSplits::TimesplitObject> objects;
    std::vector<TimeSplits::TimesplitObject> scenes;

    for (const RecordedSplit& split : splits) {
        TimeSplits::TimesplitObject object = {};
        std::vector<TimeSplits::TimesplitObject>& current =
            (split.type == TimeSplits::SPLIT_TYPE_SCENE) ? TimeSplits::sceneObjectList : TimeSplits::splitObjectList;

        object.splitType = split.type;
        object.splitId = split.id;
        object.splitName = split.name;
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
}

void WriteSplitGroup(uint32_t id, const std::vector<uint32_t>& members) {
    if (members.empty()) {
        TimeSplits::itemSubMenuList.erase(id);
    } else {
        TimeSplits::itemSubMenuList[id] = members;
    }
}

void WriteItemIcon(uint32_t item, uint32_t source) {
    auto target = itemMapping.find(item);
    auto from = itemMapping.find(source);

    sOriginalIcons.try_emplace(item, target->second);
    sOriginalIcons.try_emplace(source, from->second);

    const ItemMapEntry& icon = sOriginalIcons.at(source);
    target->second.name = icon.name;
    target->second.nameFaded = icon.nameFaded;
    target->second.texturePath = icon.texturePath;
}

void WriteQuestLabels(const QuestLabels& quest) {
    if (quest.clearsSplitGroups) {
        TimeSplits::itemSubMenuList.clear();
    }
    for (const auto& [id, members] : quest.splitGroups) {
        WriteSplitGroup(id, members);
    }
    for (const auto& [label, name] : quest.labels) {
        WriteLabel(std::get<0>(label), std::get<1>(label), std::get<2>(label), name);
    }
    if (quest.splits.has_value()) {
        WriteSplits(*quest.splits);
    }
    for (const auto& [item, source] : quest.icons) {
        WriteItemIcon(item, source);
    }
}

} // namespace

extern "C" bool ModLabels_Set(SOHLabelKind kind, int32_t language, int32_t index, const char* name) {
    if (!IsValidLabel(kind, language, index, name)) {
        return false;
    }
    const std::string quest = GetCallingQuest(MOD_CALLER_ADDRESS());
    if (!quest.empty()) {
        sQuestLabels[quest].labels[{ kind, language, index }] = name;
    }
    if (IsAppliedNow(quest)) {
        WriteLabel(kind, language, index, name);
    }
    return true;
}

extern "C" void ModLabels_ApplyTimestamps(void) {
    for (const auto& [index, name] : sTimestamps) {
        WriteTimestamp(index, name);
    }
}

extern "C" bool ModLabels_SetTimeSplits(const SOHTimeSplit* splits, uint32_t count) {
    if ((splits == nullptr) || (count == 0)) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (splits[i].type > TimeSplits::SPLIT_TYPE_BOSS) {
            return false;
        }
    }
    const std::vector<RecordedSplit> recorded = RecordSplits(splits, count);
    const std::string quest = GetCallingQuest(MOD_CALLER_ADDRESS());
    if (!quest.empty()) {
        sQuestLabels[quest].splits = recorded;
    }
    if (IsAppliedNow(quest)) {
        WriteSplits(recorded);
    }
    return true;
}

extern "C" bool ModLabels_SetTimeSplitGroup(uint32_t id, const uint32_t* members, uint32_t count) {
    if ((count != 0) && (members == nullptr)) {
        return false;
    }
    const std::vector<uint32_t> group =
        (count == 0) ? std::vector<uint32_t>() : std::vector<uint32_t>(members, members + count);
    const std::string quest = GetCallingQuest(MOD_CALLER_ADDRESS());
    if (!quest.empty()) {
        sQuestLabels[quest].splitGroups[id] = group;
    }
    if (IsAppliedNow(quest)) {
        WriteSplitGroup(id, group);
    }
    return true;
}

extern "C" void ModLabels_ClearTimeSplitGroups(void) {
    const std::string quest = GetCallingQuest(MOD_CALLER_ADDRESS());
    if (!quest.empty()) {
        sQuestLabels[quest].clearsSplitGroups = true;
        sQuestLabels[quest].splitGroups.clear();
    }
    if (IsAppliedNow(quest)) {
        TimeSplits::itemSubMenuList.clear();
    }
}

// SoH's windows draw item's icon as source's (as SoH made source's); source == item puts item's own back
extern "C" bool ModLabels_SetItemIcon(uint32_t item, uint32_t source) {
    if (!itemMapping.contains(item) || !itemMapping.contains(source)) {
        return false;
    }
    const std::string quest = GetCallingQuest(MOD_CALLER_ADDRESS());
    if (!quest.empty()) {
        sQuestLabels[quest].icons[item] = source;
    }
    if (IsAppliedNow(quest)) {
        WriteItemIcon(item, source);
    }
    return true;
}

void ModLabels_WakeQuest(const std::string& quest) {
    sStateBeforeQuest[quest] = TakeLabelState();
    auto labels = sQuestLabels.find(quest);
    if (labels != sQuestLabels.end()) {
        WriteQuestLabels(labels->second);
    }
}

void ModLabels_SleepQuest(const std::string& quest) {
    auto state = sStateBeforeQuest.find(quest);
    if (state == sStateBeforeQuest.end()) {
        return;
    }
    PutLabelState(state->second);
    sStateBeforeQuest.erase(state);
}
