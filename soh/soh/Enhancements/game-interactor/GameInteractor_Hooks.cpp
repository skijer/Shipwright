#include "GameInteractor_Hooks.h"

// MARK: - Gameplay

void GameInteractor_ExecuteOnZTitleInit(void* gameState) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnZTitleInit>(gameState);
}

void GameInteractor_ExecuteOnZTitleUpdate(void* gameState) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnZTitleUpdate>(gameState);
}

void GameInteractor_ExecuteOnLoadGame(int32_t fileNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLoadGame>(fileNum);
}

void GameInteractor_ExecuteOnExitGame(int32_t fileNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnExitGame>(fileNum);
}

void GameInteractor_ExecuteOnGameStateMainStart() {
    // Cleanup all hooks at the start of each frame
    GameInteractor::Instance->RemoveAllQueuedHooks();

    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGameStateMainStart>();
}

void GameInteractor_ExecuteOnGameFrameUpdate() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGameFrameUpdate>();
}

void GameInteractor_ExecuteOnCameraState(PlayState* play) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCameraState>(play);
}

void GameInteractor_ExecuteOnItemReceiveHooks(GetItemEntry itemEntry) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnItemReceive>(itemEntry);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnItemReceive>(itemEntry);
}

void GameInteractor_ExecuteOnEquipmentDelete(int16_t equipmentType, uint16_t equipValue) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnEquipmentDelete>(equipmentType, equipValue);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnEquipmentDelete>(equipmentType, equipValue);
}

void GameInteractor_ExecuteOnSaleEndHooks(GetItemEntry itemEntry) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSaleEnd>(itemEntry);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSaleEnd>(itemEntry);
}

void GameInteractor_ExecuteOnTransitionEndHooks(int16_t sceneNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnTransitionEnd>(sceneNum);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnTransitionEnd>(sceneNum, sceneNum);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnTransitionEnd>(sceneNum);
}

void GameInteractor_ExecuteOnSceneInit(int16_t sceneNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneInit>(sceneNum);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnSceneInit>(sceneNum, sceneNum);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSceneInit>(sceneNum);
}

void GameInteractor_ExecuteAfterSceneCommands(int16_t sceneNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::AfterSceneCommands>(sceneNum);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::AfterSceneCommands>(sceneNum, sceneNum);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::AfterSceneCommands>(sceneNum);
}

void GameInteractor_ExecuteOnSceneFlagSet(int16_t sceneNum, int16_t flagType, int16_t flag) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneFlagSet>(sceneNum, flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSceneFlagSet>(sceneNum, flagType, flag);
}

void GameInteractor_ExecuteOnSceneFlagUnset(int16_t sceneNum, int16_t flagType, int16_t flag) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneFlagUnset>(sceneNum, flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSceneFlagUnset>(sceneNum, flagType, flag);
}

void GameInteractor_ExecuteOnFlagSet(int16_t flagType, int16_t flag) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnFlagSet>(flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnFlagSet>(flagType, flag);
}

void GameInteractor_ExecuteOnFlagUnset(int16_t flagType, int16_t flag) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnFlagUnset>(flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnFlagUnset>(flagType, flag);
}

void GameInteractor_ExecuteOnSceneSpawnActors() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneSpawnActors>();
}

void GameInteractor_ExecuteOnLinkSkeletonInit() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLinkSkeletonInit>();
}

void GameInteractor_ExecuteOnLinkEquipmentChange() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLinkEquipmentChange>();
}

void GameInteractor_ExecuteOnPlayerUpdate() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerUpdate>();
}

void GameInteractor_ExecuteOnPlayerResolveItemAction(int32_t item, int8_t* itemAction) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveItemAction>(item, itemAction);
}

void GameInteractor_ExecuteOnPlayerResolveItemActionInit(int8_t itemAction, PlayerItemActionInitFunc* init) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveItemActionInit>(itemAction, init);
}

void GameInteractor_ExecuteOnPlayerResolveItemActionUpdate(int8_t itemAction, UpperActionFunc* update) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveItemActionUpdate>(itemAction, update);
}

void GameInteractor_ExecuteOnPlayerResolveModelGroup(Player* player, int32_t itemAction, int32_t* modelGroup) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveModelGroup>(player, itemAction, modelGroup);
}

bool GameInteractor_ExecuteOnPlayerActionHandler(PlayState* play, Player* player, int32_t action, bool* startedAction) {
    bool consumed = false;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerActionHandler>(play, player, action, &consumed,
                                                                                  startedAction);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnPlayerActionHandler>(action, play, player, action,
                                                                                       &consumed, startedAction);
    return consumed;
}

void GameInteractor_ExecuteOnPlayerResolveAnim(int32_t group, int32_t animType, LinkAnimationHeader** anim) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveAnim>(group, animType, anim);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnPlayerResolveAnim>(group, group, animType, anim);
}

void GameInteractor_ExecuteOnPlayerResolveAnimSite(int32_t site, int32_t index, LinkAnimationHeader** anim) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveAnimSite>(site, index, anim);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnPlayerResolveAnimSite>(site, site, index, anim);
}

void GameInteractor_ExecuteOnPlayerResolveAgeProperties(Player* player, PlayerAgeProperties** properties) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveAgeProperties>(player, properties);
}

void GameInteractor_ExecuteOnPlayerResolveMotionScale(Player* player, int32_t kind, float* scale) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveMotionScale>(player, kind, scale);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnPlayerResolveMotionScale>(kind, player, kind, scale);
}

void GameInteractor_ExecuteOnPlayerResolveHeight(Player* player, float* height) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveHeight>(player, height);
}

void GameInteractor_ExecuteOnPlayerFilterInput(Player* player, Input* input) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerFilterInput>(player, input);
}

void GameInteractor_ExecuteOnPlayerPostLimbDraw(PlayState* play, Player* player, int32_t limbIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerPostLimbDraw>(play, player, limbIndex);
}

void GameInteractor_ExecuteOnPlayerResolveFaceTextures(const char** eyes, const char** mouth) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveFaceTextures>(eyes, mouth);
}

void GameInteractor_ExecuteOnPlayerResolveLimbDraw(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList,
                                                   Vec3f* pos) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveLimbDraw>(player, limbIndex, dList, limbDList,
                                                                                    pos);
}

bool GameInteractor_ExecuteOnResolveItemGive(PlayState* play, uint8_t* item) {
    bool handled = false;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnResolveItemGive>(play, item, &handled);
    return handled;
}

bool GameInteractor_ExecuteOnGetItemDraw(PlayState* play, GetItemEntry* entry) {
    bool handled = false;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGetItemDraw>(play, entry, &handled);
    return handled;
}

void GameInteractor_ExecuteOnGetItemDrawPost(PlayState* play, GetItemEntry* entry) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGetItemDrawPost>(play, entry);
}

void GameInteractor_ExecuteOnBgCheckRaycastFloor(CollisionContext* colCtx, Vec3f* pos, Actor* actor,
                                                 CollisionPoly** poly, int32_t* bgId, float* floorY) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnBgCheckRaycastFloor>(colCtx, pos, actor, poly, bgId,
                                                                                  floorY);
}

void GameInteractor_ExecuteOnBgCheckLineTest(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* hitPos,
                                             CollisionPoly** poly, int32_t* bgId, Actor* actor, uint32_t flags,
                                             bool* result) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnBgCheckLineTest>(colCtx, posA, posB, hitPos, poly, bgId,
                                                                              actor, flags, result);
}

void GameInteractor_ExecuteOnBgCheckResolveWallFlags(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId,
                                                     uint32_t* flags) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnBgCheckResolveWallFlags>(colCtx, poly, bgId, flags);
}

void GameInteractor_ExecuteOnActorResolveBgCheckFlags(Actor* actor, int32_t* flags) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorResolveBgCheckFlags>(actor, flags);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorResolveBgCheckFlags>(actor->id, actor, flags);
}

void GameInteractor_ExecuteOnCameraResolveView(Camera* camera, Vec3f* eye, Vec3f* at, Vec3f* up) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCameraResolveView>(camera, eye, at, up);
}

void GameInteractor_ExecuteOnSetDoAction(uint16_t action) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSetDoAction>(action);
}

void GameInteractor_ExecuteOnPlayerSfx(u16 sfxId) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerSfx>(sfxId);
}

void GameInteractor_ExecuteOnOcarinaSongAction() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOcarinaSongAction>();
}

void GameInteractor_ExecuteOnOcarinaNote(uint8_t note, float modulator, int8_t bend) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOcarinaNote>(note, modulator, bend);
}

void GameInteractor_ExecuteOnOcarinaPlaybackNote(uint8_t note, float modulator) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOcarinaPlaybackNote>(note, modulator);
}

void GameInteractor_ExecuteOnCuccoOrChickenHatch() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCuccoOrChickenHatch>();
}

void GameInteractor_ExecuteOnShopSlotChangeHooks(uint8_t cursorIndex, int16_t price) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnShopSlotChange>(cursorIndex, price);
}

void GameInteractor_ExecuteOnDungeonKeyUsedHooks(uint16_t mapIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnDungeonKeyUsed>(mapIndex);
}

bool GameInteractor_ShouldActorInit(void* actor) {
    bool result = true;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::ShouldActorInit>(actor, &result);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::ShouldActorInit>(((Actor*)actor)->id, actor, &result);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::ShouldActorInit>((uintptr_t)actor, actor, &result);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::ShouldActorInit>(actor, &result);
    return result;
}

void GameInteractor_ExecuteOnActorInit(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorInit>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorInit>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorInit>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorInit>(actor);
}

void GameInteractor_ExecuteOnActorSpawn(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorSpawn>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorSpawn>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorSpawn>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorSpawn>(actor);
}

bool GameInteractor_ShouldActorUpdate(void* actor) {
    bool result = true;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::ShouldActorUpdate>(actor, &result);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::ShouldActorUpdate>(((Actor*)actor)->id, actor, &result);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::ShouldActorUpdate>((uintptr_t)actor, actor, &result);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::ShouldActorUpdate>(actor, &result);
    return result;
}

void GameInteractor_ExecuteOnActorUpdate(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorUpdate>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorUpdate>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorUpdate>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorUpdate>(actor);
}

bool GameInteractor_ExecuteOnActorTalk(Actor* actor, PlayState* play) {
    bool continueVanilla = true;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorTalk>(actor, play, &continueVanilla);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorTalk>(actor->id, actor, play, &continueVanilla);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorTalk>((uintptr_t)actor, actor, play,
                                                                              &continueVanilla);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorTalk>(actor, play, &continueVanilla);
    return continueVanilla;
}

bool GameInteractor_ExecuteOnActorPlaySfx(Actor* actor, int32_t kind, uint16_t* sfxId) {
    bool handled = false;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorPlaySfx>(actor, kind, sfxId, &handled);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorPlaySfx>(actor->id, actor, kind, sfxId,
                                                                                &handled);
    return handled;
}

bool GameInteractor_ExecuteOnActorDraw(Actor* actor, PlayState* play) {
    bool drawVanilla = true;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorDraw>(actor, play, &drawVanilla);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorDraw>(actor->id, actor, play, &drawVanilla);
    return drawVanilla;
}

void GameInteractor_ExecuteOnActorDrawEnd(Actor* actor, PlayState* play) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorDrawEnd>(actor, play);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorDrawEnd>(actor->id, actor, play);
}

void GameInteractor_ExecuteOnActorResolveGrayscale(Actor* actor, PlayState* play, Color_RGBA8* grayscale) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorResolveGrayscale>(actor, play, grayscale);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorResolveGrayscale>(actor->id, actor, play,
                                                                                         grayscale);
}

void GameInteractor_ExecuteOnRoomResolveGrayscale(PlayState* play, Room* room, Color_RGBA8* grayscale) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnRoomResolveGrayscale>(play, room, grayscale);
}

void GameInteractor_ExecuteOnActorResolveMotionScale(Actor* actor, float* scale) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorResolveMotionScale>(actor, scale);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorResolveMotionScale>(actor->id, actor, scale);
}

void GameInteractor_ExecuteOnActorResolvePlayerRelation(Actor* actor, float* xzDist, float* yDist,
                                                        int16_t* yawTowards) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorResolvePlayerRelation>(actor, xzDist, yDist,
                                                                                         yawTowards);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorResolvePlayerRelation>(actor->id, actor, xzDist,
                                                                                              yDist, yawTowards);
}

void GameInteractor_ExecuteOnCollisionRegisterAC(Collider* collider) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCollisionRegisterAC>(collider);
}

void GameInteractor_ExecuteOnResolveSwordDamage(PlayState* play, int32_t dmgFlags, uint8_t* damage) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnResolveSwordDamage>(play, dmgFlags, damage);
}

void GameInteractor_ExecuteOnCollisionResolveDamage(Actor* victim, ColliderInfo* attack, float* damage) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCollisionResolveDamage>(victim, attack, damage);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnCollisionResolveDamage>(victim->id, victim, attack,
                                                                                          damage);
}

void GameInteractor_ExecuteOnResolveCustomGetItem(Actor* actor, PlayState* play, GetItemEntry* entry,
                                                  const char** key) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnResolveCustomGetItem>(actor, play, entry, key);
}

void GameInteractor_ExecuteOnActorKill(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorKill>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorKill>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorKill>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorKill>(actor);
}

void GameInteractor_ExecuteOnActorDestroy(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorDestroy>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorDestroy>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorDestroy>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorDestroy>(actor);
}

void GameInteractor_ExecuteOnEnemyDefeat(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnEnemyDefeat>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnEnemyDefeat>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnEnemyDefeat>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnEnemyDefeat>(actor);
}

void GameInteractor_ExecuteOnBossDefeat(void* actor) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnBossDefeat>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnBossDefeat>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnBossDefeat>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnBossDefeat>(actor);
}

void GameInteractor_ExecuteOnTimestamp(u8 item) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnTimestamp>(item);
}

void GameInteractor_ExecuteOnPlayerBonk() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerBonk>();
}

void GameInteractor_ExecuteOnPlayerSetModels(Player* player, u8 modelGroup) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerSetModels>(player, modelGroup);
}

void GameInteractor_ExecuteOnPlayerHealthChange(int16_t amount) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerHealthChange>(amount);
}

void GameInteractor_ExecuteOnPlayerBottleUpdate(int16_t contents) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerBottleUpdate>(contents);
}

void GameInteractor_ExecuteOnPlayerHoldUpShield() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerHoldUpShield>();
}

void GameInteractor_ExecuteOnPlayerShieldBlocked(PlayState* play, Player* player, Actor* attacker) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerShieldBlocked>(play, player, attacker);
}

void GameInteractor_ExecuteOnAudioTablesReady() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnAudioTablesReady>();
}

void GameInteractor_ExecuteOnPlayerFirstPersonControl(Player* player) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerFirstPersonControl>(player);
}

void GameInteractor_ExecuteOnPlayerShieldControl(float* sp50, float* sp54) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerShieldControl>(sp50, sp54);
}

void GameInteractor_ExecuteOnPlayerProcessStick() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerProcessStick>();
}

void GameInteractor_ExecuteOnPlayDestroy() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDestroy>();
}

void GameInteractor_ExecuteOnPlayDrawBegin() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDrawBegin>();
}

void GameInteractor_ExecuteOnPlayDrawEnd() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDrawEnd>();
}

bool GameInteractor_Should(GIVanillaBehavior flag, u32 result, ...) {
    // Only the external function can use the Variadic Function syntax
    // To pass the va args to the next caller must be done using va_list and reading the args into it
    // Because there can be N subscribers registered to each template call, the subscribers will be responsible for
    // creating a copy of this va_list to avoid incrementing the original pointer between calls
    va_list args;
    va_start(args, result);

    // Because of default argument promotion, even though our incoming "result" is just a bool, it needs to be typed as
    // an int to be permitted to be used in `va_start`, otherwise it is undefined behavior.
    // Here we downcast back to a bool for our actual hook handlers
    bool boolResult = static_cast<bool>(result);

    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnVanillaBehavior>(flag, &boolResult, args);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnVanillaBehavior>(flag, flag, &boolResult, args);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnVanillaBehavior>(flag, &boolResult, args);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnModVanillaBehavior>(flag, flag, &boolResult, args);

    va_end(args);
    return boolResult;
}

// MARK: -  Save Files

void GameInteractor_ExecuteOnSaveFile(int32_t fileNum, int32_t sectionID) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSaveFile>(fileNum, sectionID);
}

void GameInteractor_ExecuteOnLoadFile(int32_t fileNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLoadFile>(fileNum);
}

void GameInteractor_ExecuteOnDeleteFile(int32_t fileNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnDeleteFile>(fileNum);
}

// MARK: - Dialog

void GameInteractor_ExecuteOnDialogMessage() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnDialogMessage>();
}

void GameInteractor_ExecuteOnMessageResolveItemIcon(uint16_t itemId, const char** iconPath) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnMessageResolveItemIcon>(itemId, iconPath);
}

void GameInteractor_ExecuteOnPresentTitleCard() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPresentTitleCard>();
}

void GameInteractor_ExecuteOnResolveEnvHazard(PlayState* play, s16* hazard) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnResolveEnvHazard>(play, hazard);
}

void GameInteractor_ExecuteOnMagicResolveCost(s16* amount) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnMagicResolveCost>(amount);
}

void GameInteractor_ExecuteOnInterfaceUpdate() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnInterfaceUpdate>();
}

void GameInteractor_ExecuteOnInterfaceDrawEnd(PlayState* play) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnInterfaceDrawEnd>(play);
}

void GameInteractor_ExecuteOnInterfaceResolveButtonIcon(PlayState* play, uint8_t button, uint16_t item,
                                                        const char** iconPath) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnInterfaceResolveButtonIcon>(play, button, item, iconPath);
}

void GameInteractor_ExecuteOnKaleidoscopeUpdate(int16_t inDungeonScene) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoscopeUpdate>(inDungeonScene);
}

void GameInteractor_ExecuteOnKaleidoItemCursor(PlayState* play, uint16_t* item, uint16_t* slot) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoItemCursor>(play, item, slot);
}

void GameInteractor_ExecuteOnKaleidoItemDraw(PlayState* play, int32_t slot, int32_t item, Vtx* vertices) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoItemDraw>(play, slot, item, vertices);
}

void GameInteractor_ExecuteOnKaleidoResolveItemIcon(PlayState* play, uint16_t item, const char** iconPath) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoResolveItemIcon>(play, item, iconPath);
}

void GameInteractor_ExecuteOnKaleidoItemEquip(PlayState* play, uint8_t button, uint16_t item, bool* handled) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoItemEquip>(play, button, item, handled);
}

void GameInteractor_ExecuteOnKaleidoInput(PlayState* play, Input* input, bool* handled) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoInput>(play, input, handled);
}

void GameInteractor_ExecuteOnKaleidoResolveName(PlayState* play, uint16_t item, const char** namePath) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoResolveName>(play, item, namePath);
}

void GameInteractor_ExecuteOnKaleidoDebugEditor(PlayState* play, bool* handled) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoDebugEditor>(play, handled);
}

// MARK: - Main Menu

void GameInteractor_ExecuteOnPresentFileSelect() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPresentFileSelect>();
}

void GameInteractor_ExecuteOnUpdateFileSelectSelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileSelectSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileSelectConfirmationSelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileSelectConfirmationSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileCopySelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileCopySelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileCopyConfirmationSelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileCopyConfirmationSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileEraseSelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileEraseSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileEraseConfirmationSelection(uint16_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileEraseConfirmationSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileAudioSelection(uint8_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileAudioSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileTargetSelection(uint8_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileTargetSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileLanguageSelection(uint8_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileLanguageSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileQuestSelection(uint8_t questIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileQuestSelection>(questIndex);
}

void GameInteractor_ExecuteOnUpdateFileBossRushOptionSelection(uint8_t optionIndex, uint8_t optionValue) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileBossRushOptionSelection>(optionIndex,
                                                                                                optionValue);
}

void GameInteractor_ExecuteOnUpdateFileRandomizerOptionSelection(uint8_t optionIndex) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileRandomizerOptionSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileNameSelection(int16_t charCode) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileNameSelection>(charCode);
}

void GameInteractor_ExecuteOnFileChooseMain(void* gameState) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnFileChooseMain>(gameState);
}

// MARK: - Game

void GameInteractor_ExecuteOnSetGameLanguage() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSetGameLanguage>();
}

// MARK: - System

void GameInteractor_RegisterOnAssetAltChange(void (*fn)(void)) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnAssetAltChange>(fn);
}

// MARK: Pause Menu

void GameInteractor_ExecuteOnKaleidoUpdate() {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoUpdate>();
}

// MARK: Messages
void GameInteractor_ExecuteOnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOpenText>(textId, loadFromMessageTable);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnOpenText>(*textId, textId, loadFromMessageTable);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnOpenText>(textId, loadFromMessageTable);
}

// Mark: Audio
void GameInteractor_ExecuteOnSeqPlayerInit(int32_t playerIdx, int32_t seqId) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSeqPlayerInit>(playerIdx, seqId);
}

// MARK: - Rando
void GameInteractor_ExecuteOnRandoEntranceDiscovered(u16 entranceIndex, u8 isReversedEntrance) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnRandoEntranceDiscovered>(entranceIndex,
                                                                                      isReversedEntrance);
}
