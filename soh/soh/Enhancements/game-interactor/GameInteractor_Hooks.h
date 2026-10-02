#pragma once

#include "vanilla-behavior/GIVanillaBehavior.h"
#include "GameInteractor.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
// MARK: - Gameplay
void GameInteractor_ExecuteOnZTitleInit(void* gameState);
void GameInteractor_ExecuteOnZTitleUpdate(void* gameState);
void GameInteractor_ExecuteOnLoadGame(int32_t fileNum);
void GameInteractor_ExecuteOnExitGame(int32_t fileNum);
void GameInteractor_ExecuteOnGameStateMainStart();
void GameInteractor_ExecuteOnGameFrameUpdate();
void GameInteractor_ExecuteOnCameraState(PlayState* play);
void GameInteractor_ExecuteOnItemReceiveHooks(GetItemEntry itemEntry);
void GameInteractor_ExecuteOnEquipmentDelete(int16_t equipmentType, uint16_t equipValue);
void GameInteractor_ExecuteOnSaleEndHooks(GetItemEntry itemEntry);
void GameInteractor_ExecuteOnTransitionEndHooks(int16_t sceneNum);
void GameInteractor_ExecuteOnSceneInit(int16_t sceneNum);
void GameInteractor_ExecuteAfterSceneCommands(int16_t sceneNum);
void GameInteractor_ExecuteOnSceneFlagSet(int16_t sceneNum, int16_t flagType, int16_t flag);
void GameInteractor_ExecuteOnSceneFlagUnset(int16_t sceneNum, int16_t flagType, int16_t flag);
void GameInteractor_ExecuteOnFlagSet(int16_t flagType, int16_t flag);
void GameInteractor_ExecuteOnFlagUnset(int16_t flagType, int16_t flag);
void GameInteractor_ExecuteOnSceneSpawnActors();
void GameInteractor_ExecuteOnLinkSkeletonInit();
void GameInteractor_ExecuteOnLinkEquipmentChange();
void GameInteractor_ExecuteOnPlayerUpdate();
void GameInteractor_ExecuteOnPlayerResolveItemAction(int32_t item, int8_t* itemAction);
void GameInteractor_ExecuteOnPlayerResolveItemActionInit(int8_t itemAction, PlayerItemActionInitFunc* init);
void GameInteractor_ExecuteOnPlayerResolveItemActionUpdate(int8_t itemAction, UpperActionFunc* update);
void GameInteractor_ExecuteOnPlayerResolveModelGroup(Player* player, int32_t itemAction, int32_t* modelGroup);
bool GameInteractor_ExecuteOnPlayerActionHandler(PlayState* play, Player* player, int32_t action, bool* startedAction);
void GameInteractor_ExecuteOnPlayerResolveAnim(int32_t group, int32_t animType, LinkAnimationHeader** anim);
void GameInteractor_ExecuteOnPlayerResolveAnimSite(int32_t site, int32_t index, LinkAnimationHeader** anim);
void GameInteractor_ExecuteOnPlayerResolveAgeProperties(Player* player, PlayerAgeProperties** properties);
void GameInteractor_ExecuteOnPlayerResolveMotionScale(Player* player, int32_t kind, float* scale);
void GameInteractor_ExecuteOnPlayerResolveHeight(Player* player, float* height);
void GameInteractor_ExecuteOnPlayerFilterInput(Player* player, Input* input);
void GameInteractor_ExecuteOnPlayerPostLimbDraw(PlayState* play, Player* player, int32_t limbIndex);
void GameInteractor_ExecuteOnPlayerResolveFaceTextures(const char** eyes, const char** mouth);
void GameInteractor_ExecuteOnPlayerResolveLimbDraw(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList,
                                                   Vec3f* pos);
bool GameInteractor_ExecuteOnResolveItemGive(PlayState* play, uint8_t* item);
bool GameInteractor_ExecuteOnGetItemDraw(PlayState* play, GetItemEntry* entry);
void GameInteractor_ExecuteOnGetItemDrawPost(PlayState* play, GetItemEntry* entry);
void GameInteractor_ExecuteOnBgCheckRaycastFloor(CollisionContext* colCtx, Vec3f* pos, Actor* actor,
                                                 CollisionPoly** poly, int32_t* bgId, float* floorY);
void GameInteractor_ExecuteOnBgCheckLineTest(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* hitPos,
                                             CollisionPoly** poly, int32_t* bgId, Actor* actor, uint32_t flags,
                                             bool* result);
void GameInteractor_ExecuteOnBgCheckResolveWallFlags(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId,
                                                     uint32_t* flags);
void GameInteractor_ExecuteOnActorResolveBgCheckFlags(Actor* actor, int32_t* flags);
void GameInteractor_ExecuteOnCameraResolveView(Camera* camera, Vec3f* eye, Vec3f* at, Vec3f* up);
void GameInteractor_ExecuteOnSetDoAction(uint16_t action);
void GameInteractor_ExecuteOnPlayerSfx(u16 sfxId);
void GameInteractor_ExecuteOnOcarinaSongAction();
void GameInteractor_ExecuteOnOcarinaNote(uint8_t note, float modulator, int8_t bend);
void GameInteractor_ExecuteOnOcarinaPlaybackNote(uint8_t note, float modulator);
void GameInteractor_ExecuteOnCuccoOrChickenHatch();
bool GameInteractor_ShouldActorInit(void* actor);
void GameInteractor_ExecuteOnActorInit(void* actor);
void GameInteractor_ExecuteOnActorSpawn(void* actor);
bool GameInteractor_ShouldActorUpdate(void* actor);
void GameInteractor_ExecuteOnActorUpdate(void* actor);
bool GameInteractor_ExecuteOnActorTalk(Actor* actor, PlayState* play);
bool GameInteractor_ExecuteOnActorPlaySfx(Actor* actor, int32_t kind, uint16_t* sfxId);
bool GameInteractor_ExecuteOnActorDraw(Actor* actor, PlayState* play);
void GameInteractor_ExecuteOnActorDrawEnd(Actor* actor, PlayState* play);
void GameInteractor_ExecuteOnActorResolveGrayscale(Actor* actor, PlayState* play, Color_RGBA8* grayscale);
void GameInteractor_ExecuteOnRoomResolveGrayscale(PlayState* play, Room* room, Color_RGBA8* grayscale);
void GameInteractor_ExecuteOnActorResolveMotionScale(Actor* actor, float* scale);
void GameInteractor_ExecuteOnActorResolvePlayerRelation(Actor* actor, float* xzDist, float* yDist, int16_t* yawTowards);
void GameInteractor_ExecuteOnCollisionRegisterAC(Collider* collider);
void GameInteractor_ExecuteOnResolveSwordDamage(PlayState* play, int32_t dmgFlags, uint8_t* damage);
void GameInteractor_ExecuteOnCollisionResolveDamage(Actor* victim, ColliderInfo* attack, float* damage);
void GameInteractor_ExecuteOnResolveCustomGetItem(Actor* actor, PlayState* play, GetItemEntry* entry, const char** key);
void GameInteractor_ExecuteOnActorKill(void* actor);
void GameInteractor_ExecuteOnActorDestroy(void* actor);
void GameInteractor_ExecuteOnEnemyDefeat(void* actor);
void GameInteractor_ExecuteOnBossDefeat(void* actor);
void GameInteractor_ExecuteOnTimestamp(u8 item);
void GameInteractor_ExecuteOnPlayerBonk();
void GameInteractor_ExecuteOnPlayerSetModels(Player* player, u8 modelGroup);
void GameInteractor_ExecuteOnPlayerHealthChange(int16_t amount);
void GameInteractor_ExecuteOnPlayerBottleUpdate(int16_t contents);
void GameInteractor_ExecuteOnPlayerHoldUpShield();
void GameInteractor_ExecuteOnPlayerShieldBlocked(PlayState* play, Player* player, Actor* attacker);
void GameInteractor_ExecuteOnAudioTablesReady();
void GameInteractor_ExecuteOnPlayerFirstPersonControl(Player* player);
void GameInteractor_ExecuteOnPlayerShieldControl(float* sp50, float* sp54);
void GameInteractor_ExecuteOnPlayerProcessStick();
void GameInteractor_ExecuteOnShopSlotChangeHooks(uint8_t cursorIndex, int16_t price);
void GameInteractor_ExecuteOnDungeonKeyUsedHooks(uint16_t mapIndex);
void GameInteractor_ExecuteOnPlayDestroy();
void GameInteractor_ExecuteOnPlayDrawBegin();
void GameInteractor_ExecuteOnPlayDrawEnd();
bool GameInteractor_Should(GIVanillaBehavior flag, uint32_t result, ...);

// MARK: -  Save Files
void GameInteractor_ExecuteOnSaveFile(int32_t fileNum, int32_t sectionID);
void GameInteractor_ExecuteOnLoadFile(int32_t fileNum);
void GameInteractor_ExecuteOnDeleteFile(int32_t fileNum);

// MARK: - Dialog
void GameInteractor_ExecuteOnDialogMessage();
void GameInteractor_ExecuteOnMessageResolveItemIcon(uint16_t itemId, const char** iconPath);
void GameInteractor_ExecuteOnPresentTitleCard();
void GameInteractor_ExecuteOnResolveEnvHazard(PlayState* play, s16* hazard);
void GameInteractor_ExecuteOnMagicResolveCost(s16* amount);
void GameInteractor_ExecuteOnInterfaceUpdate();
void GameInteractor_ExecuteOnInterfaceDrawEnd(PlayState* play);
void GameInteractor_ExecuteOnInterfaceResolveButtonIcon(PlayState* play, uint8_t button, uint16_t item,
                                                        const char** iconPath);
void GameInteractor_ExecuteOnKaleidoscopeUpdate(int16_t inDungeonScene);
void GameInteractor_ExecuteOnKaleidoItemCursor(PlayState* play, uint16_t* item, uint16_t* slot);
void GameInteractor_ExecuteOnKaleidoItemDraw(PlayState* play, int32_t slot, int32_t item, Vtx* vertices);
void GameInteractor_ExecuteOnKaleidoResolveItemIcon(PlayState* play, uint16_t item, const char** iconPath);
void GameInteractor_ExecuteOnKaleidoItemEquip(PlayState* play, uint8_t button, uint16_t item, bool* handled);
void GameInteractor_ExecuteOnKaleidoInput(PlayState* play, Input* input, bool* handled);
void GameInteractor_ExecuteOnKaleidoResolveName(PlayState* play, uint16_t item, const char** namePath);
void GameInteractor_ExecuteOnKaleidoDebugEditor(PlayState* play, bool* handled);

// MARK: - Main Menu
void GameInteractor_ExecuteOnPresentFileSelect();
void GameInteractor_ExecuteOnUpdateFileSelectSelection(uint16_t optionIndex);
void GameInteractor_ExecuteOnUpdateFileSelectConfirmationSelection(uint16_t optionIndex);
void GameInteractor_ExecuteOnUpdateFileCopySelection(uint16_t optionIndex);
void GameInteractor_ExecuteOnUpdateFileCopyConfirmationSelection(uint16_t optionIndex);
void GameInteractor_ExecuteOnUpdateFileEraseSelection(uint16_t optionIndex);
void GameInteractor_ExecuteOnUpdateFileEraseConfirmationSelection(uint16_t optionIndex);
void GameInteractor_ExecuteOnUpdateFileAudioSelection(uint8_t optionIndex);
void GameInteractor_ExecuteOnUpdateFileTargetSelection(uint8_t optionIndex);
void GameInteractor_ExecuteOnUpdateFileLanguageSelection(uint8_t optionIndex);
void GameInteractor_ExecuteOnUpdateFileQuestSelection(uint8_t questIndex);
void GameInteractor_ExecuteOnUpdateFileBossRushOptionSelection(uint8_t optionIndex, uint8_t optionValue);
void GameInteractor_ExecuteOnUpdateFileRandomizerOptionSelection(uint8_t optionIndex);
void GameInteractor_ExecuteOnUpdateFileNameSelection(int16_t charCode);
void GameInteractor_ExecuteOnFileChooseMain(void* gameState);

// MARK: - Game
void GameInteractor_ExecuteOnSetGameLanguage();

// MARK: - System
void GameInteractor_RegisterOnAssetAltChange(void (*fn)(void));

// Mark: - Pause Menu
void GameInteractor_ExecuteOnKaleidoUpdate();

// MARK: - Messages
void GameInteractor_ExecuteOnOpenText(uint16_t* textId, bool* loadFromMessageTable);

// Mark: - Audio
void GameInteractor_ExecuteOnSeqPlayerInit(int32_t playerIdx, int32_t seqId);

// MARK: - Rando
void GameInteractor_ExecuteOnRandoEntranceDiscovered(u16 entranceIndex, u8 isReversedEntrance);

#ifdef __cplusplus
}
#endif
