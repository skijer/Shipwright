#ifndef SOH_MOD_API_MOD_QUESTS_H
#define SOH_MOD_API_MOD_QUESTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <string>
extern "C" {
#endif

// The first time the quest is registered, the Original Quest files already there that belong to no mod quest become
// its files: files made before the mod registered it, while its world was all there was.
#define SOH_MOD_QUEST_ADOPTS_ORIGINAL_FILES (1u << 0)

#define SOH_MOD_QUEST_KEY_SIZE 32

// A quest a mod adds to the file select's quest menu, beside Original Quest, Master Quest, Randomizer and Boss Rush.
// Its files play as Original Quest and remember the quest by key, so they open only while the mod is loaded.
typedef struct {
    uint32_t structSize;
    const char* key;   // stored in the save; letters, digits, '_' and '-', shorter than SOH_MOD_QUEST_KEY_SIZE
    const char* name;  // read out on the quest menu, and drawn there when there is no logo
    const char* label; // on the file's button, like "MQ" or "RAND": up to 4 capitals or digits (NULL: "MOD")
    uint32_t flags;    // SOH_MOD_QUEST_*
    // The archive in the mods folder that holds the quest's world (scenes, text, audio), e.g. "MyQuest.o2r". It
    // covers the game's files only while a file of this quest is played; otherwise it sits under them, so only its
    // own new files show. NULL: the quest plays in the game's own world.
    const char* archive;
    // A 160x160 RGBA32 texture drawn on the quest menu in place of the Ocarina of Time logo, e.g. a path of the
    // quest's archive that the game doesn't have. NULL: the Ocarina of Time logo with the name under it.
    const char* logo;
} SOHModQuest;

#define SOH_MOD_QUEST_MIN_SIZE (offsetof(SOHModQuest, flags) + sizeof(((SOHModQuest*)0)->flags))

bool ModQuests_Register(const SOHModQuest* quest);
// The mod quest of the file being played, or NULL for the game's own quests.
const char* ModQuests_GetActive(void);
// The mod quest whose world covers the game's files now, or NULL for the game's own world.
const char* ModQuests_GetWorld(void);

// File select. The quest menu shows mod quest i as quest QUEST_MOD + i.
uint32_t ModQuests_Count(void);
const char* ModQuests_GetName(uint32_t index);
// The logo as a texture name for the file select's draw calls, or NULL when the quest has none.
const char* ModQuests_GetLogo(uint32_t index);
// A file's mod quest (its SaveFileMetaInfo modQuest, "" for none): it may open while its mod is loaded.
bool ModQuests_IsFileAllowed(const char* modQuest);
// The label on the file's button, or NULL for a file of the game's own quests.
const char* ModQuests_GetFileLabel(const char* modQuest);
void ModQuests_Choose(uint32_t index);
// One of the game's own quests was chosen, or a randomizer seed is about to be made, outside play: the game's world
// goes on top (a seed is made from the game's scenes and tables; Boss Rush plays them without a file).
void ModQuests_UseGameWorld(void);
// A new file: a mod quest's when modQuest is true, else one of the game's own quests.
void ModQuests_ApplyToNewFile(bool modQuest);

#ifdef __cplusplus
}

void ModQuests_Init();
// Once the mods are loaded: puts the world of the last quest played back on top.
void ModQuests_RestoreWorld();
// Before the file select reads the files: adopts Original Quest files for quests that take them.
void ModQuests_AdoptFiles();
// After a file's save sections are read: puts the file's world on top of the game's files.
void ModQuests_OnFileLoaded();
// The flag a quest mod's hooks run under: true while its world covers the game's files. NULL for other mods.
const bool* ModQuests_GetWorldFlag(const std::string& mod);
#endif

#endif
