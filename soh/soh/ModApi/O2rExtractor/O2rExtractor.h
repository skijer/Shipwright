#ifndef SOH_MOD_API_O2R_EXTRACTOR_H
#define SOH_MOD_API_O2R_EXTRACTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returning false stops the walk. The data lives only for the call.
typedef bool (*SOHArchiveFileFunc)(const char* path, const uint8_t* data, uint64_t size, void* userData);
typedef bool (*SOHArchivePathFunc)(const char* path, void* userData);

uint32_t O2rExtractor_ExportFiles(const char* searchMask, const char* destinationDir);
uint32_t O2rExtractor_ReadFiles(const char* searchMask, SOHArchiveFileFunc visit, void* userData);
uint32_t O2rExtractor_ReadGameFiles(const char* searchMask, SOHArchiveFileFunc visit, void* userData);
uint32_t O2rExtractor_ListFiles(const char* searchMask, SOHArchivePathFunc visit, void* userData);

#ifdef __cplusplus
}

#include <filesystem>
#include <string>

uint32_t O2rExtractor_ExportMatching(const std::string& searchMask, const std::filesystem::path& destination);
#endif

#endif
