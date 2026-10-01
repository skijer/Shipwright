#ifndef SOH_MOD_API_O2R_EXTRACTOR_H
#define SOH_MOD_API_O2R_EXTRACTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t structSize;
    const char* romPath;
    const char* assetsDir;
    const char* xmlDir;
    const char* configPath;
    const char* filelistDir;
    const char* outputPath;
} SOHO2rExtractRequest;

#define SOH_O2R_EXTRACT_REQUEST_MIN_SIZE \
    (offsetof(SOHO2rExtractRequest, outputPath) + sizeof(((SOHO2rExtractRequest*)0)->outputPath))

uint32_t O2rExtractor_ExportFiles(const char* searchMask, const char* destinationDir);
bool O2rExtractor_Run(const SOHO2rExtractRequest* request);

#ifdef __cplusplus
}
#endif

#endif
