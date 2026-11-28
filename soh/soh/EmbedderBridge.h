#pragma once

#ifdef __EMSCRIPTEN__
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void Soh_InitEmbedderBridge(void);
void Soh_EmbedderAfterFrame(void);
void Soh_EmbedderQuit(void);
void Soh_EmbedderError(const char* message);
void Soh_EmbedderReportFile(const char* path);
void Soh_EmbedderCountDraws(size_t draws);
void Soh_EmbedderCountAudioDrop(void);
#ifdef __cplusplus
}
#endif
#endif
