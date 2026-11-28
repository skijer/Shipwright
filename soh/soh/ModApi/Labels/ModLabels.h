#ifndef SOH_MOD_API_MOD_LABELS_H
#define SOH_MOD_API_MOD_LABELS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SOH_LABEL_ITEM,
    SOH_LABEL_QUEST_ITEM,
    SOH_LABEL_SCENE,
    SOH_LABEL_TIMESTAMP,
} SOHLabelKind;

typedef struct {
    uint32_t type;
    uint32_t id;
    const char* name;
} SOHTimeSplit;

#ifdef __cplusplus
extern "C" {
#endif

bool ModLabels_Set(SOHLabelKind kind, int32_t language, int32_t index, const char* name);
bool ModLabels_SetTimeSplits(const SOHTimeSplit* splits, uint32_t count);
bool ModLabels_SetTimeSplitGroup(uint32_t id, const uint32_t* members, uint32_t count);
void ModLabels_ClearTimeSplitGroups(void);
bool ModLabels_SetItemIcon(uint32_t item, uint32_t source);
void ModLabels_ApplyTimestamps(void);

#ifdef __cplusplus
}

#include <string>

// A mod quest's labels go on while it's awake; the labels from before it woke come back when it sleeps.
void ModLabels_WakeQuest(const std::string& quest);
void ModLabels_SleepQuest(const std::string& quest);
#endif

#endif
