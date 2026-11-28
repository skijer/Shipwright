#ifndef SOH_MOD_API_MOD_QUESTS_H
#define SOH_MOD_API_MOD_QUESTS_H

#include <stdbool.h>
#include <stdint.h>

#define MOD_QUEST_KEY_SIZE 32

#ifdef __cplusplus
extern "C" {
#endif

// The quest menu shows mod quest i as quest QUEST_MOD + i.
uint32_t ModQuests_Count(void);
const char* ModQuests_GetName(uint32_t index);
// An "__OTR__" texture path for the file select's draw calls, or NULL when the quest has no logo.
const char* ModQuests_GetLogo(uint32_t index);
// fileQuest is a file's SaveFileMetaInfo::modQuest, "" for the game's own quests.
bool ModQuests_CanOpenFile(const char* fileQuest);
const char* ModQuests_GetFileLabel(const char* fileQuest);

void ModQuests_ChooseModQuest(uint32_t index);
void ModQuests_ChooseGameQuest(void);
void ModQuests_StartNewFile(bool isModQuest);
void ModQuests_LeaveFile(void);
// Both assume no scene is loaded.
void ModQuests_ShowTitleWorld(void);
void ModQuests_ShowGameWorld(void);

const char* ModQuests_GetActive(void);
// A path only a sleeping world's archives provide, such as a text override that must not leak into other worlds.
bool ModQuests_IsPathAsleep(const char* path);

#ifdef __cplusplus
}

#include <string>
#include <vector>

struct ModQuestDeclaration {
    std::string key;
    std::string name;
    std::string label;
    bool isRomhack;
    std::vector<std::string> worldArchives;
    std::string logo;
    bool adoptsOriginalFiles;
};

struct ModQuestWorld {
    std::string key;
    std::string name;
};

void ModQuests_Init();
bool ModQuests_Declare(const ModQuestDeclaration& declaration, const std::string& owner,
                       const std::string& ownArchivePath);
void ModQuests_FinishLoading();
void ModQuests_AdoptOriginalFiles();
void ModQuests_EnterLoadedFile();
const bool* ModQuests_GetAwakeFlag(const std::string& mod);
// The key of the quest the mod declared and the host took, or NULL.
const char* ModQuests_GetQuestOfMod(const std::string& mod);
// The title screen setting's choices (last played, the game's, each romhack's), empty without a romhack.
std::vector<ModQuestWorld> ModQuests_ListTitleChoices();
std::string ModQuests_GetTitleChoice();
void ModQuests_SetTitleChoice(const std::string& key);
#endif

#endif
