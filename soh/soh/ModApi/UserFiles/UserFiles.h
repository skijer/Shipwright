#ifndef SOH_MOD_API_USER_FILES_H
#define SOH_MOD_API_USER_FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t structSize;
    uint8_t* data;
    uint64_t size;
    char name[256];
} SOHUserFile;

#define SOH_USER_FILE_MIN_SIZE (offsetof(SOHUserFile, name) + sizeof(((SOHUserFile*)0)->name))

bool UserFiles_Pick(const char* title, const char* filterName, const char* filterPatterns, SOHUserFile* file);
void UserFiles_Free(SOHUserFile* file);
bool UserFiles_SaveToMods(const char* fileName, const void* data, uint64_t size);
bool UserFiles_MountArchive(const char* title);

#ifdef __cplusplus
}

#include <filesystem>

void UserFiles_ApplyPending(const std::filesystem::path& modsDirectory);
#endif

#endif
