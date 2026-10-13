#ifndef SOH_MOD_API_MOD_LABELS_H
#define SOH_MOD_API_MOD_LABELS_H

#include <stdbool.h>
#include <stdint.h>

// The names SoH's own windows (save editor, gameplay stats, time splits) show
typedef enum {
    SOH_LABEL_ITEM,       // an item id; per language
    SOH_LABEL_QUEST_ITEM, // a quest item; per language
    SOH_LABEL_SCENE,      // a scene id
    SOH_LABEL_TIMESTAMP,  // a gameplay stats timestamp (an item id, or TIMESTAMP_DEFEAT_GOHMA onward)
} SOHLabelKind;

typedef struct {
    uint32_t type;     // SplitType: item, upgrade, equipment, quest, boss, entrance, misc
    uint32_t id;       // what the split waits for: an item, a boss actor, an entrance...
    const char* name;  // the label
    const char* image; // an ImGui texture name, as SoH's own splits use
    float tint[4];     // RGBA, 0 to 1
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
