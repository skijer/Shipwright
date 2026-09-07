#ifndef FILE_SELECT_ENHANCEMENTS_H
#define FILE_SELECT_ENHANCEMENTS_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif
const char* SohFileSelect_GetSettingText(u8 optionIndex, u8 language);
// Same option indices (RSM_*) but combo-worded ("Start Combo" / "Generate Combo Seed" / ...), used
// when the selected quest is QUEST_OOTXMM so the reused rando sub-screen reads as the OoT x MM combo.
const char* SohFileSelect_GetComboSettingText(u8 optionIndex, u8 language);
void SohFileSelect_ShowPresetModal();
#ifdef __cplusplus
};
#endif

typedef enum {
    RSM_START_RANDOMIZER,
    RSM_GENERATE_RANDOMIZER,
    RSM_OPEN_RANDOMIZER_SETTINGS,
    RSM_GENERATING,
    RSM_NO_RANDOMIZER_GENERATED,
    RSM_MAX,
} RandomizerSettingsMenuEnums;

// COMBO (QUEST_OOTXMM) reuses the rando settings sub-screen but with 4 selectable options (adds
// "Load Combo Seed"). Indices 0..CBO_OPEN_SETTINGS are the selectable rows; CBO_GENERATING/CBO_NO_SEED
// are status/hint strings. SohFileSelect_GetComboSettingText is indexed by these.
typedef enum {
    CBO_START,         // 0  create the OoT + MM save pair from the ready seed
    CBO_GENERATE,      // 1  generate a fresh combo seed
    CBO_LOAD_SEED,     // 2  load a .fleet seed (seed-only; Start then bakes it)
    CBO_OPEN_SETTINGS, // 3  open the shared combo settings (knobs)
    CBO_GENERATING,    // 4  status
    CBO_NO_SEED,       // 5  hint
    CBO_MAX,
} ComboSettingsMenuEnums;

#endif
