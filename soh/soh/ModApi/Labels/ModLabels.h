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
#endif

#endif
