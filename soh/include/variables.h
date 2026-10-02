#ifndef VARIABLES_H
#define VARIABLES_H

#include "z64.h"
#include "segment_symbols.h"

#ifdef __cplusplus
extern "C"
{
#endif

	extern HOST_DATA u32 osTvType;
	extern HOST_DATA u32 osRomBase;
	extern HOST_DATA u32 osResetType;
	extern HOST_DATA u32 osMemSize;
	extern HOST_DATA u8 osAppNmiBuffer[0x40];

	extern HOST_DATA u8 D_80009320[];
	extern HOST_DATA u8 D_800093F0[];
	extern HOST_DATA s8 D_80009430;
	extern HOST_DATA u32 D_80009460;
	extern HOST_DATA u32 gDmaMgrDmaBuffSize;
	extern HOST_DATA vu8 gViConfigUseDefault;
	extern HOST_DATA u8 gViConfigAdditionalScanLines;
	extern HOST_DATA u32 gViConfigFeatures;
	extern HOST_DATA f32 gViConfigXScale;
	extern HOST_DATA f32 gViConfigYScale;
	extern HOST_DATA OSPiHandle* gCartHandle;
	extern HOST_DATA u32 __osPiAccessQueueEnabled;
	extern HOST_DATA OSViMode osViModePalLan1;
	extern HOST_DATA s32 osViClock;
	extern HOST_DATA u32 __osShutdown;
	extern HOST_DATA OSHWIntr __OSGlobalIntMask;
	extern HOST_DATA OSThread* __osThreadTail[];
	extern HOST_DATA OSThread* __osRunQueue;
	extern HOST_DATA OSThread* __osActiveQueue;
	extern HOST_DATA OSThread* __osRunningThread;
	extern HOST_DATA OSThread* __osFaultedThread;
	extern HOST_DATA OSPiHandle* __osPiTable;
	extern HOST_DATA OSPiHandle* __osCurrentHandle[];
	extern HOST_DATA OSTimer* __osTimerList;
	extern HOST_DATA OSViMode osViModeNtscLan1;
	extern HOST_DATA OSViMode osViModeMpalLan1;
	extern HOST_DATA OSViContext* __osViCurr;
	extern HOST_DATA OSViContext* __osViNext;
	extern HOST_DATA OSViMode osViModeFpalLan1;
	extern HOST_DATA u32 __additional_scanline;
	extern HOST_DATA const char gBuildVersion[];
	extern HOST_DATA u16 gBuildVersionMajor;
	extern HOST_DATA u16 gBuildVersionMinor;
	extern HOST_DATA u16 gBuildVersionPatch;
	extern HOST_DATA const char gGitBranch[];
	extern HOST_DATA const char gGitCommitHash[];
	extern HOST_DATA u8 gGitCommitTag[];
	extern HOST_DATA u8 gBuildTeam[];
	extern HOST_DATA const char gBuildForkName[];
	extern HOST_DATA const char gBuildForkVersion[];
	extern HOST_DATA u8 gBuildDate[];
	extern HOST_DATA u8 gBuildMakeOption[];
	extern HOST_DATA OSMesgQueue gPiMgrCmdQ;
	extern HOST_DATA OSViMode gViConfigMode;
	extern HOST_DATA u8 D_80013960;
	extern HOST_DATA OSMesgQueue __osPiAccessQueue;
	extern HOST_DATA OSPiHandle __Dom1SpeedParam;
	extern HOST_DATA OSPiHandle __Dom2SpeedParam;
	extern HOST_DATA OSTime __osCurrentTime;
	extern HOST_DATA u32 __osBaseCounter;
	extern HOST_DATA u32 __osViIntrCount;
	extern HOST_DATA u32 __osTimerCounter;
	extern HOST_DATA DmaEntry gDmaDataTable[0x60C];
	extern HOST_DATA u64 D_801120C0[];
	extern HOST_DATA u8 D_80113070[];
	extern HOST_DATA u64 gJpegUCode[];
	extern HOST_DATA EffectSsOverlay gEffectSsOverlayTable[EFFECT_SS_TYPE_MAX];
	extern HOST_DATA Gfx D_80116280[];
	extern HOST_DATA s32 gDbgCamEnabled;
	extern HOST_DATA GameStateOverlay gGameStateOverlayTable[6];
	extern HOST_DATA u8 gWeatherMode;
	extern HOST_DATA u8 D_8011FB34;
	extern HOST_DATA u8 D_8011FB38;
	extern HOST_DATA u8 gSkyboxBlendingEnabled;
	extern HOST_DATA u16 gTimeIncrement;
	extern HOST_DATA struct_8011FC1C D_8011FC1C[][9];
	extern HOST_DATA SkyboxFile gSkyboxFiles[];
	extern HOST_DATA s32 gZeldaArenaLogSeverity;
	extern HOST_DATA MapData gMapDataTable;
	extern HOST_DATA s16 gSpoilingItems[3];
	extern HOST_DATA s16 gSpoilingItemReverts[3];
	extern HOST_DATA FlexSkeletonHeader* gPlayerSkelHeaders[2];
	extern HOST_DATA u8 gPlayerModelTypes[PLAYER_MODELGROUP_MAX][PLAYER_MODELGROUPENTRY_MAX];
	extern HOST_DATA Gfx* gPlayerLeftHandBgsDLs[];
	extern HOST_DATA Gfx* gPlayerLeftHandOpenDLs[];
	extern HOST_DATA Gfx* gPlayerLeftHandClosedDLs[];
	extern HOST_DATA Gfx* gPlayerLeftHandBoomerangDLs[];
	extern HOST_DATA Gfx gCullBackDList[];
	extern HOST_DATA Gfx gCullFrontDList[];
	extern HOST_DATA Gfx gEmptyDL[];
	extern HOST_DATA u32 gBitFlags[32];
	extern HOST_DATA u16 gEquipMasks[4];
	extern HOST_DATA u16 gEquipNegMasks[4];
	extern HOST_DATA u32 gUpgradeMasks[8];
	extern HOST_DATA u32 gUpgradeNegMasks[8];
	extern HOST_DATA u8 gEquipShifts[4];
	extern HOST_DATA u8 gUpgradeShifts[8];
	extern HOST_DATA u16 gUpgradeCapacities[8][4];
	extern HOST_DATA u32 gGsFlagsMasks[4];
	extern HOST_DATA u32 gGsFlagsShifts[4];
	extern HOST_DATA void* gItemIcons[158];
	extern HOST_DATA u8 gItemAgeReqs[];
	extern HOST_DATA u8 gSlotAgeReqs[];
	extern HOST_DATA u8 gItemSlots[56];
	extern HOST_DATA void (*gSceneCmdHandlers[SCENE_CMD_ID_MAX])(PlayState*, SceneCmd*);
	extern HOST_DATA s16 gLinkObjectIds[2];
	extern HOST_DATA u32 gObjectTableSize;
	extern HOST_DATA RomFile gObjectTable[OBJECT_ID_MAX];
	extern HOST_DATA EntranceInfo* gEntranceTable; // SOH [Unbound] backed by SceneDB; ENTR_MAX is the vanilla count only
	extern HOST_DATA u16 gSramSlotOffsets[];
	// 4 16-colors palettes
	extern HOST_DATA u64 gMojiFontTLUTs[4][4]; // original name: "moji_tlut"
	extern HOST_DATA u64 gMojiFontTex[]; // original name: "font_ff"
	extern HOST_DATA KaleidoMgrOverlay gKaleidoMgrOverlayTable[KALEIDO_OVL_MAX];
	extern HOST_DATA KaleidoMgrOverlay* gKaleidoMgrCurOvl;
	extern HOST_DATA u8 gBossMarkState;
	extern HOST_DATA void* gDebugCutsceneScript;
	extern HOST_DATA s32 gScreenWidth;
	extern HOST_DATA s32 gScreenHeight;
	extern HOST_DATA Mtx gMtxClear;
	extern HOST_DATA MtxF gMtxFClear;
	extern HOST_DATA u32 gIsCtrlr2Valid;
	extern HOST_DATA vu32 gIrqMgrResetStatus;
	extern HOST_DATA volatile OSTime gIrqMgrRetraceTime;
	extern HOST_DATA s16* gWaveSamples[9];
	extern HOST_DATA f32 gBendPitchOneOctaveFrequencies[256];
	extern HOST_DATA f32 gBendPitchTwoSemitonesFrequencies[256];
	extern HOST_DATA f32 gNoteFrequencies[];
	extern HOST_DATA u8 gDefaultShortNoteVelocityTable[16];
	extern HOST_DATA u8 gDefaultShortNoteGateTimeTable[16];
	extern HOST_DATA AdsrEnvelope gDefaultEnvelope[4];
	extern HOST_DATA NoteSubEu gZeroNoteSub;
	extern HOST_DATA NoteSubEu gDefaultNoteSub;
	extern HOST_DATA u16 gHeadsetPanQuantization[64];
	extern HOST_DATA s16 D_8012FBA8[];
	extern HOST_DATA f32 gHeadsetPanVolume[128];
	extern HOST_DATA f32 gStereoPanVolume[128];
	extern HOST_DATA f32 gDefaultPanVolume[128];
	extern HOST_DATA s16 sLowPassFilterData[16 * 8];
	extern HOST_DATA s16 sHighPassFilterData[15 * 8];
	extern HOST_DATA s32 gAudioContextInitalized;
	extern HOST_DATA u8 gIsLargeSoundBank[7];
	extern HOST_DATA u8 gChannelsPerBank[4][7];
	extern HOST_DATA u8 gUsedChannelsPerBank[4][7];
	extern HOST_DATA u8 gMorphaTransposeTable[16];
	extern HOST_DATA u8* gFrogsSongPtr;
	extern HOST_DATA OcarinaNote* gScarecrowCustomSongPtr;
	extern HOST_DATA u8* gScarecrowSpawnSongPtr;
	extern HOST_DATA OcarinaSongInfo gOcarinaSongNotes[];
	extern HOST_DATA SoundParams* gSoundParams[7];
	extern HOST_DATA char D_80133390[];
	extern HOST_DATA char D_80133398[];
	extern HOST_DATA SoundBankEntry* gSoundBanks[7];
	extern HOST_DATA u8 gSfxChannelLayout;
	extern HOST_DATA Vec3f gSfxDefaultPos;
	extern HOST_DATA f32 gSfxDefaultFreqAndVolScale;
	extern HOST_DATA s8 gSfxDefaultReverb;
	extern HOST_DATA u8 D_801333F0;
	extern HOST_DATA u8 gAudioSfxSwapOff;
	extern HOST_DATA u8 D_80133408;
	extern HOST_DATA u8 D_8013340C;
	extern HOST_DATA u8 gAudioSpecId;
	extern HOST_DATA u8 D_80133418;
	extern HOST_DATA AudioSpec gAudioSpecs[18];
	extern HOST_DATA s32 gOverlayLogSeverity;
	extern HOST_DATA s32 gSystemArenaLogSeverity;
	extern HOST_DATA u8 __osPfsInodeCacheBank;
	extern HOST_DATA s32 __osPfsLastChannel;
	extern HOST_DATA u8 gWalkSpeedToggle;
	extern HOST_DATA f32 iceTrapScale;
	extern HOST_DATA f32 triforcePieceScale;
	extern HOST_DATA f32 mysteryItemScale;

	extern HOST_DATA const s16 D_8014A6C0[];
#define gTatumsPerBeat (D_8014A6C0[1])
	extern HOST_DATA const AudioContextInitSizes D_8014A6C4;
	extern HOST_DATA s16 gOcarinaSongItemMap[];
	extern HOST_DATA u8 D_80155F50[];
	extern HOST_DATA u8 D_80157580[];
	extern HOST_DATA u8 D_801579A0[];
	extern HOST_DATA u64 gJpegUCodeData[];

	extern HOST_DATA SaveContext gSaveContext;
	extern HOST_DATA PlayState* gPlayState;
	extern HOST_DATA GameInfo* gGameInfo;
	extern HOST_DATA u16 D_8015FCC0;
	extern HOST_DATA u16 D_8015FCC2;
	extern HOST_DATA u16 D_8015FCC4;
	extern HOST_DATA u8 D_8015FCC8;
	extern HOST_DATA u8 gCustomLensFlareOn;
	extern HOST_DATA Vec3f gCustomLensFlarePos;
	extern HOST_DATA s16 gLensFlareScale;
	extern HOST_DATA f32 gLensFlareColorIntensity;
	extern HOST_DATA s16 gLensFlareScreenFillAlpha;
	extern HOST_DATA LightningStrike gLightningStrike;
	extern HOST_DATA MapData* gMapData;
	extern HOST_DATA f32 gBossMarkScale;
	extern HOST_DATA PauseMapMarksData* gLoadedPauseMarkDataTable;
	extern HOST_DATA s32 gTrnsnUnkState;
	extern HOST_DATA Color_RGBA8_u32 gVisMonoColor;
	extern HOST_DATA PreNmiBuff* gAppNmiBufferPtr;
	extern HOST_DATA SchedContext gSchedContext;
	extern HOST_DATA PadMgr gPadMgr;
	extern HOST_DATA uintptr_t gSegments[NUM_SEGMENTS];
	extern HOST_DATA volatile OSTime D_8016A520;
	extern HOST_DATA volatile OSTime D_8016A528;
	extern HOST_DATA volatile OSTime D_8016A530;
	extern HOST_DATA volatile OSTime D_8016A538;
	extern HOST_DATA volatile OSTime D_8016A540;
	extern HOST_DATA volatile OSTime D_8016A548;
	extern HOST_DATA volatile OSTime D_8016A550;
	extern HOST_DATA volatile OSTime D_8016A558;
	extern HOST_DATA volatile OSTime gRSPAudioTotalTime;
	extern HOST_DATA volatile OSTime gRSPGFXTotalTime;
	extern HOST_DATA volatile OSTime gRSPOtherTotalTime;
	extern HOST_DATA volatile OSTime gRDPTotalTime;
	extern HOST_DATA FaultThreadStruct gFaultStruct;

	extern HOST_DATA ActiveSound gActiveSounds[7][MAX_CHANNELS_PER_BANK]; // total size = 0xA8
	extern HOST_DATA u8 gSoundBankMuted[];
	extern HOST_DATA u8 D_801333F0;
	extern HOST_DATA u8 gAudioSfxSwapOff;
	extern HOST_DATA u16 gAudioSfxSwapSource[10];
	extern HOST_DATA u16 gAudioSfxSwapTarget[10];
	extern HOST_DATA u8 gAudioSfxSwapMode[10];
	extern HOST_DATA ActiveSequence gActiveSeqs[4];
	extern HOST_DATA AudioContext gAudioContext;
	extern HOST_DATA void(*D_801755D0)(void);

	extern HOST_DATA u32 __osMalloc_FreeBlockTest_Enable;
	extern HOST_DATA Arena gSystemArena;
	extern HOST_DATA OSPifRam __osPifInternalBuff;
	extern HOST_DATA u8 __osContLastPoll;
	extern HOST_DATA u8 __osMaxControllers;
	extern HOST_DATA __OSInode __osPfsInodeCache;
	extern HOST_DATA OSPifRam gPifMempakBuf;
	extern HOST_DATA u16 gZBuffer[SCREEN_HEIGHT][SCREEN_WIDTH]; // 0x25800 bytes
	extern HOST_DATA u64 gGfxSPTaskOutputBuffer[0x3000]; // 0x18000 bytes
	extern HOST_DATA u8 gGfxSPTaskYieldBuffer[OS_YIELD_DATA_SIZE]; // 0xC00 bytes
	extern HOST_DATA u8 gGfxSPTaskStack[0x400]; // 0x400 bytes
	extern HOST_DATA GfxPool gGfxPools[2]; // 0x24820 bytes
	extern HOST_DATA u8* gAudioHeap;
	extern HOST_DATA u8* gSystemHeap;
	extern HOST_DATA GameState* gGameState;

#ifdef __cplusplus
};
#endif

#endif
