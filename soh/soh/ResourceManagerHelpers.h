#pragma once

#include "libultraship/libultra/types.h"

#define GAME_REGION_NTSC 0
#define GAME_REGION_PAL 1
#define GAME_REGION_UNKNOWN 2

#define GAME_PLATFORM_N64 0
#define GAME_PLATFORM_GC 1
#define GAME_PLATFORM_UNKNOWN 2

#ifdef __cplusplus
#include <memory>

namespace Ship {
class IResource;
} // namespace Ship

std::shared_ptr<Ship::IResource> ResourceMgr_GetResourceByNameHandlingMQ(const char* path);

extern "C" {
#endif // __cplusplus
#include "z64animation.h"
#include "z64audio.h"
#include "z64bgcheck.h"
uint32_t ResourceMgr_IsGameMasterQuest();
uint32_t ResourceMgr_IsSceneMasterQuest(s16 sceneNum);
uint32_t ResourceMgr_GameHasMasterQuest();
uint32_t ResourceMgr_GameHasOriginal();
uint32_t ResourceMgr_GetNumGameVersions();
uint32_t ResourceMgr_GetGameVersion(int index);
uint32_t ResourceMgr_GetGamePlatform(int index);
uint32_t ResourceMgr_GetGameRegion(int index);
bool ResourceMgr_IsPalLoaded();
void ResourceMgr_LoadDirectory(const char* resName);
void ResourceMgr_UnloadResource(const char* resName);
char** ResourceMgr_ListFiles(const char* searchMask, int* resultSize);
uint8_t ResourceMgr_FileExists(const char* resName);
uint8_t ResourceMgr_FileAltExists(const char* resName);
void ResourceMgr_UnloadOriginalWhenAltExists(const char* resName);
uint8_t ResourceMgr_TexIsRaw(const char* texPath);
uint8_t ResourceMgr_ResourceIsBackground(char* texPath);
char* ResourceMgr_LoadJPEG(char* data, size_t dataSize);
uint16_t ResourceMgr_LoadTexWidthByName(char* texPath);
uint16_t ResourceMgr_LoadTexHeightByName(char* texPath);
char* ResourceMgr_LoadTexOrDListByName(const char* filePath);
char* ResourceMgr_LoadPlayerAnimByName(const char* animPath);
// Wraps a raw misc/link_animetion/ PlayerAnimation resource in a runtime
// LinkAnimationHeader (frameCount + data pointer) so it can be passed to
// Player_AnimPlayLoop / Player_AnimPlayOnce. Returns NULL if the path doesn't
// exist or the resource isn't a PlayerAnimation. Cached by path.
LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
// Same, but every frame keeps frame 0's root translation, so the clip animates
// in place instead of walking the body across the floor. Use this for any clip
// installed into OOT's own animation tables: OOT integrates baked root motion on
// top of linearVelocity, and the imported MHR clips carry a lot of it.
// stripY also pins the vertical root (OOT then owns floor height).
LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlace(const char* animPath, uint8_t stripY);
// Same, plus a forced frame count (0 = keep the clip's own length). OOT's
// locomotion blends walk and run at fixed frame RATIOS against a hard-coded
// 29-frame length, so imported cycles have to be resampled to match or the two
// clips get sampled out of phase and limbs snap to wrong angles mid-stride.
LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(const char* animPath, uint8_t stripY,
                                                                        int16_t targetFrames);
// Same, plus an inclusive sub-range (-1/-1 = the whole clip). Cuts one packed clip
// into several engine slots — Gerudo's guard is one 46-frame flourish served to
// OOT's defense / defense_wait / defense_end as 1-20 / 21-30 / 31-45 — and, paired
// with targetFrames, sets playback speed: asking a 20-frame range for 10 frames
// makes it run at double speed in slots OOT always plays at 1.0.
LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(const char* animPath, uint8_t stripY,
                                                                    int16_t firstFrame, int16_t lastFrame,
                                                                    int16_t targetFrames);
AnimationHeaderCommon* ResourceMgr_LoadAnimByName(const char* path);
char* ResourceMgr_GetNameByCRC(uint64_t crc, char* alloc);
Gfx* ResourceMgr_LoadGfxByCRC(uint64_t crc);
Gfx* ResourceMgr_LoadGfxByName(const char* path);
uint8_t ResourceMgr_FileIsCustomByName(const char* path);
void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction);
void ResourceMgr_PatchCustomGfxByName(const char* path, const char* patchName, int index, Gfx instruction);
void ResourceMgr_UnpatchGfxByName(const char* path, const char* patchName);
char* ResourceMgr_LoadArrayByNameAsVec3s(const char* path);
Vtx* ResourceMgr_LoadVtxByCRC(uint64_t crc);
Vtx* ResourceMgr_LoadVtxByName(char* path);
SoundFont* ResourceMgr_LoadAudioSoundFontByName(const char* path);
SequenceData ResourceMgr_LoadSeqByName(const char* path);
SequenceData* ResourceMgr_LoadSeqPtrByName(const char* path);
SoundFontSample* ResourceMgr_LoadAudioSample(const char* path);
CollisionHeader* ResourceMgr_LoadColByName(const char* path);
bool ResourceMgr_IsAltAssetsEnabled();
SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
void ResourceMgr_UnregisterSkeleton(SkelAnime* skelAnime);
void ResourceMgr_ClearSkeletons();
s32* ResourceMgr_LoadCSByName(const char* path);
int ResourceMgr_OTRSigCheck(char* imgData);
char* ResourceMgr_GetResourceDataByNameHandlingMQ(const char* path);
#ifdef __cplusplus
}
#endif // __cplusplus
