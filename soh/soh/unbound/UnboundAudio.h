#pragma once
#include <stdint.h>

#ifdef __cplusplus
#include <string>

namespace SOH::Unbound {
uint16_t SequenceIdForPath(const std::string& path);
} // namespace SOH::Unbound

extern "C" {
#endif

struct PlayState;
// Forces the next Environment_PlaySceneSequence to queue when only the effective song changed: its same-seq gate
// compares vanilla u8 ids and would otherwise keep the previous scene's audio playing.
void Unbound_BindSceneSong(struct PlayState* play, uint16_t songSeqId);
uint16_t Unbound_SequenceIdForPath(const char* path);
uint16_t Unbound_RegisterSequence(const char* path);
int32_t Unbound_RegisterSoundFont(const char* path);

#ifdef __cplusplus
}
#endif
