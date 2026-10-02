/**
 * Hook Table
 *
 * DEFINE_HOOK arguments:
 *    - Argument 1: Name of the hook
 *    - Argument 2: Function type that the hook uses
 */
DEFINE_HOOK(OnZTitleInit, (void* gameState));
DEFINE_HOOK(OnZTitleUpdate, (void* gameState));
DEFINE_HOOK(OnLoadGame, (int32_t fileNum));
DEFINE_HOOK(OnExitGame, (int32_t fileNum));
DEFINE_HOOK(OnGameStateMainStart, ());
DEFINE_HOOK(OnGameFrameUpdate, ());
DEFINE_HOOK(OnSaveEditorTabs, (SaveContext * save));
DEFINE_HOOK(OnSaveEditorInventory, (SaveContext * save));
DEFINE_HOOK(OnSaveEditorItemPicker, (SaveContext * save, int32_t slot, bool restricted));
DEFINE_HOOK(OnSaveEditorItemEligibility, (int32_t slot, uint16_t item, bool* allowed));
DEFINE_HOOK(OnConsoleGive, (PlayState * play, const char* type, const char* key, bool* handled, bool* success));
DEFINE_HOOK(OnCameraState, (PlayState * play));
DEFINE_HOOK(OnItemReceive, (GetItemEntry itemEntry));
DEFINE_HOOK(OnResolveItemGive, (PlayState * play, uint8_t* item, bool* handled));
DEFINE_HOOK(OnEquipmentDelete, (int16_t equipmentType, uint16_t equipValue));
DEFINE_HOOK(OnSaleEnd, (GetItemEntry itemEntry));
DEFINE_HOOK(OnTransitionEnd, (int16_t sceneNum));
DEFINE_HOOK(OnSceneInit, (int16_t sceneNum));
DEFINE_HOOK(AfterSceneCommands, (int16_t sceneNum));
DEFINE_HOOK(OnSceneFlagSet, (int16_t sceneNum, int16_t flagType, int16_t flag));
DEFINE_HOOK(OnSceneFlagUnset, (int16_t sceneNum, int16_t flagType, int16_t flag));
DEFINE_HOOK(OnFlagSet, (int16_t flagType, int16_t flag));
DEFINE_HOOK(OnFlagUnset, (int16_t flagType, int16_t flag));
DEFINE_HOOK(OnSceneSpawnActors, ());
DEFINE_HOOK(OnLinkSkeletonInit, ());
DEFINE_HOOK(OnLinkEquipmentChange, ());
DEFINE_HOOK(OnPlayerUpdate, ());
DEFINE_HOOK(OnPlayerResolveItemAction, (int32_t item, int8_t* itemAction));
DEFINE_HOOK(OnPlayerResolveItemActionInit, (int8_t itemAction, PlayerItemActionInitFunc* init));
DEFINE_HOOK(OnPlayerResolveItemActionUpdate, (int8_t itemAction, UpperActionFunc* update));
DEFINE_HOOK(OnPlayerResolveModelGroup, (Player * player, int32_t itemAction, int32_t* modelGroup));
DEFINE_HOOK(OnPlayerActionHandler,
            (PlayState * play, Player* player, int32_t action, bool* consumed, bool* startedAction));
DEFINE_HOOK(OnPlayerResolveAnim, (int32_t group, int32_t animType, LinkAnimationHeader** anim));
DEFINE_HOOK(OnPlayerResolveAnimSite, (int32_t site, int32_t index, LinkAnimationHeader** anim));
DEFINE_HOOK(OnPlayerResolveAgeProperties, (Player * player, PlayerAgeProperties** properties));
DEFINE_HOOK(OnPlayerResolveMotionScale, (Player * player, int32_t kind, float* scale));
DEFINE_HOOK(OnPlayerResolveHeight, (Player * player, float* height));
DEFINE_HOOK(OnPlayerFilterInput, (Player * player, Input* input));
DEFINE_HOOK(OnPlayerPostLimbDraw, (PlayState * play, Player* player, int32_t limbIndex));
DEFINE_HOOK(OnPlayerResolveFaceTextures, (const char** eyes, const char** mouth));
DEFINE_HOOK(OnPlayerResolveLimbDraw, (Player * player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos));
DEFINE_HOOK(OnGetItemDraw, (PlayState * play, GetItemEntry* entry, bool* handled));
DEFINE_HOOK(OnGetItemDrawPost, (PlayState * play, GetItemEntry* entry));
DEFINE_HOOK(OnBgCheckRaycastFloor,
            (CollisionContext * colCtx, Vec3f* pos, Actor* actor, CollisionPoly** poly, int32_t* bgId, float* floorY));
DEFINE_HOOK(OnBgCheckLineTest, (CollisionContext * colCtx, Vec3f* posA, Vec3f* posB, Vec3f* hitPos,
                                CollisionPoly** poly, int32_t* bgId, Actor* actor, uint32_t flags, bool* result));
DEFINE_HOOK(OnBgCheckResolveWallFlags, (CollisionContext * colCtx, CollisionPoly* poly, int32_t bgId, uint32_t* flags));
DEFINE_HOOK(OnActorResolveBgCheckFlags, (Actor * actor, int32_t* flags));
DEFINE_HOOK(OnCameraResolveView, (Camera * camera, Vec3f* eye, Vec3f* at, Vec3f* up));
DEFINE_HOOK(OnSetDoAction, (uint16_t action));
DEFINE_HOOK(OnPlayerSfx, (u16 sfxId));
DEFINE_HOOK(OnOcarinaSongAction, ());
DEFINE_HOOK(OnOcarinaNote, (uint8_t note, float modulator, int8_t bend));
DEFINE_HOOK(OnOcarinaPlaybackNote, (uint8_t note, float modulator));
DEFINE_HOOK(OnCuccoOrChickenHatch, ());
DEFINE_HOOK(OnShopSlotChange, (uint8_t cursorIndex, int16_t price));
DEFINE_HOOK(OnDungeonKeyUsed, (uint16_t mapIndex));
DEFINE_HOOK(ShouldActorInit, (void* actor, bool* result));
DEFINE_HOOK(OnActorInit, (void* actor));
DEFINE_HOOK(OnActorSpawn, (void* actor));
DEFINE_HOOK(ShouldActorUpdate, (void* actor, bool* result));
DEFINE_HOOK(OnActorUpdate, (void* actor));
DEFINE_HOOK(OnActorTalk, (Actor * actor, PlayState* play, bool* continueVanilla));
DEFINE_HOOK(OnActorPlaySfx, (Actor * actor, int32_t kind, uint16_t* sfxId, bool* handled));
DEFINE_HOOK(OnActorDraw, (Actor * actor, PlayState* play, bool* drawVanilla));
DEFINE_HOOK(OnActorDrawEnd, (Actor * actor, PlayState* play));
DEFINE_HOOK(OnActorResolveGrayscale, (Actor * actor, PlayState* play, Color_RGBA8* grayscale));
DEFINE_HOOK(OnRoomResolveGrayscale, (PlayState * play, Room* room, Color_RGBA8* grayscale));
DEFINE_HOOK(OnActorResolveMotionScale, (Actor * actor, float* scale));
DEFINE_HOOK(OnActorResolvePlayerRelation, (Actor * actor, float* xzDist, float* yDist, int16_t* yawTowards));
DEFINE_HOOK(OnCollisionRegisterAC, (Collider * collider));
DEFINE_HOOK(OnResolveSwordDamage, (PlayState * play, int32_t dmgFlags, uint8_t* damage));
DEFINE_HOOK(OnCollisionResolveDamage, (Actor * victim, ColliderInfo* attack, float* damage));
DEFINE_HOOK(OnResolveCustomGetItem, (Actor * actor, PlayState* play, GetItemEntry* entry, const char** key));
DEFINE_HOOK(OnActorKill, (void* actor));
DEFINE_HOOK(OnActorDestroy, (void* actor));
DEFINE_HOOK(OnEnemyDefeat, (void* actor));
DEFINE_HOOK(OnBossDefeat, (void* actor));
DEFINE_HOOK(OnTimestamp, (u8 item));
DEFINE_HOOK(OnPlayerBonk, ());
DEFINE_HOOK(OnPlayerSetModels, (Player * player, u8 modelGroup));
DEFINE_HOOK(OnPlayerHealthChange, (int16_t amount));
DEFINE_HOOK(OnPlayerBottleUpdate, (int16_t contents));
DEFINE_HOOK(OnPlayerHoldUpShield, ());
DEFINE_HOOK(OnPlayerShieldBlocked, (PlayState * play, Player* player, Actor* attacker));
DEFINE_HOOK(OnPlayerFirstPersonControl, (Player * player));
DEFINE_HOOK(OnPlayerProcessStick, ());
DEFINE_HOOK(OnPlayerShieldControl, (float* sp50, float* sp54));
DEFINE_HOOK(OnPlayDestroy, ());
DEFINE_HOOK(OnPlayDrawBegin, ());
DEFINE_HOOK(OnPlayDrawEnd, ());
DEFINE_HOOK(OnVanillaBehavior, (GIVanillaBehavior flag, bool* result, va_list originalArgs));
DEFINE_HOOK(OnModVanillaBehavior, (GIVanillaBehavior flag, bool* result, va_list originalArgs));
DEFINE_HOOK(OnSaveFile, (int32_t fileNum, int32_t sectionID));
DEFINE_HOOK(OnLoadFile, (int32_t fileNum));
DEFINE_HOOK(OnDeleteFile, (int32_t fileNum));

DEFINE_HOOK(OnDialogMessage, ());
DEFINE_HOOK(OnMessageResolveItemIcon, (uint16_t itemId, const char** iconPath));
DEFINE_HOOK(OnPresentTitleCard, ());
DEFINE_HOOK(OnResolveEnvHazard, (PlayState * play, int16_t* hazard));
DEFINE_HOOK(OnMagicResolveCost, (int16_t * amount));
DEFINE_HOOK(OnInterfaceUpdate, ());
DEFINE_HOOK(OnInterfaceDrawEnd, (PlayState * play));
DEFINE_HOOK(OnInterfaceResolveButtonIcon, (PlayState * play, uint8_t button, uint16_t item, const char** iconPath));
DEFINE_HOOK(OnKaleidoscopeUpdate, (int16_t inDungeonScene));
DEFINE_HOOK(OnKaleidoItemCursor, (PlayState * play, uint16_t* item, uint16_t* slot));
DEFINE_HOOK(OnKaleidoItemDraw, (PlayState * play, int32_t slot, int32_t item, Vtx* vertices));
DEFINE_HOOK(OnKaleidoResolveItemIcon, (PlayState * play, uint16_t item, const char** iconPath));
DEFINE_HOOK(OnKaleidoItemEquip, (PlayState * play, uint8_t button, uint16_t item, bool* handled));
DEFINE_HOOK(OnKaleidoInput, (PlayState * play, Input* input, bool* handled));
DEFINE_HOOK(OnKaleidoResolveName, (PlayState * play, uint16_t item, const char** namePath));
DEFINE_HOOK(OnKaleidoDebugEditor, (PlayState * play, bool* handled));

DEFINE_HOOK(OnPresentFileSelect, ());
DEFINE_HOOK(OnUpdateFileSelectSelection, (uint16_t optionIndex));
DEFINE_HOOK(OnUpdateFileSelectConfirmationSelection, (uint16_t optionIndex));
DEFINE_HOOK(OnUpdateFileCopySelection, (uint16_t optionIndex));
DEFINE_HOOK(OnUpdateFileCopyConfirmationSelection, (uint16_t optionIndex));
DEFINE_HOOK(OnUpdateFileEraseSelection, (uint16_t optionIndex));
DEFINE_HOOK(OnUpdateFileEraseConfirmationSelection, (uint16_t optionIndex));
DEFINE_HOOK(OnUpdateFileAudioSelection, (uint8_t optionIndex));
DEFINE_HOOK(OnUpdateFileTargetSelection, (uint8_t optionIndex));
DEFINE_HOOK(OnUpdateFileLanguageSelection, (uint8_t optionIndex));
DEFINE_HOOK(OnUpdateFileQuestSelection, (uint8_t questIndex));
DEFINE_HOOK(OnUpdateFileBossRushOptionSelection, (uint8_t optionIndex, uint8_t optionValue));
DEFINE_HOOK(OnUpdateFileRandomizerOptionSelection, (uint8_t optionIndex));
DEFINE_HOOK(OnUpdateFileNameSelection, (int16_t charCode));
DEFINE_HOOK(OnFileChooseMain, (void* gameState));
DEFINE_HOOK(OnGenerationCompletion, ());

DEFINE_HOOK(OnSetGameLanguage, ());
DEFINE_HOOK(OnAssetAltChange, ());
DEFINE_HOOK(OnKaleidoUpdate, ());

// Messages
DEFINE_HOOK(OnOpenText, (uint16_t * textId, bool* loadFromMessageTable));

// Audio
DEFINE_HOOK(OnSeqPlayerInit, (int32_t playerIdx, int32_t seqId));
DEFINE_HOOK(OnAudioTablesReady, ());

// Rando
DEFINE_HOOK(OnRandoSetCheckStatus, (RandomizerCheck rc, RandomizerCheckStatus status));
DEFINE_HOOK(OnRandoSetIsSkipped, (RandomizerCheck rc, bool isSkipped));
DEFINE_HOOK(OnRandoEntranceDiscovered, (u16 entranceIndex, u8 isReversedEntrance));
