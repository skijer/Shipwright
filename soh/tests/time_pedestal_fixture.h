#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef float f32;
typedef struct {
    f32 x, y, z;
} Vec3f;
typedef struct {
    s16 x, y, z;
} Vec3s;
typedef struct {
    u16 x, y, z;
} Vec3us;
typedef struct {
    s32 x, y, z;
} Vec3i;
#include "z64item.h"
#include "z64cutscene.h"

typedef struct PlayState PlayState;
typedef struct Actor Actor;
#include "time_pedestal_collision_types.h"
typedef void (*ActorFunc)(Actor*, PlayState*);
typedef int Gfx;
typedef struct {
    u8 buttonItems[8];
    u8 cButtonSlots[7];
    u16 equipment;
} ItemEquips;
typedef struct {
    u8 items[24];
    s8 ammo[16];
    u16 equipment;
    u32 upgrades;
    u32 questItems;
    u8 dungeonItems[20];
    s8 dungeonKeys[19];
    s8 defenseHearts;
    s16 gsTokens;
} Inventory;
typedef struct {
    Vec3f pos;
    s16 yaw, playerParams, entranceIndex;
    s8 roomIndex, data;
    u32 tempSwchFlags, tempCollectFlags;
} RespawnData;
typedef struct {
    s32 entranceIndex, linkAge, sceneLayer, respawnFlag;
    u16 cutsceneIndex;
    u8 cutsceneTrigger, nextTransitionType, cutsceneTransitionControl;
    u8 buttonStatus[9];
    u8 fileNum;
    u8 seqId, natureAmbienceId, forcedSeqId;
    u16 dayTime;
    s16 swordHealth;
    s16 savedSceneNum;
    ItemEquips equips, childEquips, adultEquips;
    Inventory inventory;
    u16 eventChkInf[14], itemGetInf[4], infTable[30];
    RespawnData respawn[3];
    struct {
        u8 maskMemory;
    } ship;
} SaveContext;
struct Actor {
    s16 id, params;
    s8 room;
    struct {
        Vec3f pos;
        Vec3s rot;
    } world;
    struct {
        Vec3s rot;
        f32 yOffset;
        void* shadowDraw;
    } shape;
    Vec3f velocity, scale;
    f32 speedXZ;
    f32 xzDistToPlayer, yDistToPlayer;
    s16 yawTowardsPlayer;
    Actor* parent;
    ActorFunc draw;
    u16 bgCheckFlags;
    f32 floorHeight;
    void* floorPoly;
    u8 floorBgId;
    CollisionCheckInfo colChkInfo;
};
typedef struct {
    int marker;
} LinkAnimationHeader;
typedef struct {
    f32 curFrame, endFrame, animLength;
    LinkAnimationHeader* animation;
    u8 movementFlags;
} SkelAnime;
typedef struct {
    u16 unk_00, unk_02;
} struct_808551A4;
typedef struct {
    u16 sfx;
    s16 frame;
} AnimSfxEntry;
typedef struct {
    LinkAnimationHeader* unk_9C;
    LinkAnimationHeader* unk_A0;
    f32 unk_08;
} PlayerAgeProperties;
struct Player;
typedef void (*PlayerActionFunc)(struct Player*, PlayState*);
typedef struct Player {
    Actor actor;
    s16 yaw;
    u8 currentSwordItemId, csAction, prevCsAction;
    s8 itemAction, heldItemAction, nextModelGroup;
    u8 heldItemId;
    u8 leftHandType, rightHandType, sheathType, currentShield;
    Gfx** leftHandDLists;
    SkelAnime skelAnime;
    PlayerActionFunc actionFunc;
    PlayerActionFunc upperActionFunc;
    s16 unk_6AE_rotFlags;
    struct {
        s16 actionVar1;
    } av1;
    struct {
        s16 actionVar2;
    } av2;
    u32 stateFlags1, stateFlags2;
    s16 getItemId;
    u16 getItemDirection;
    f32 linearVelocity;
    Actor* interactRangeActor;
    Actor* heldActor;
    Actor* talkActor;
    const PlayerAgeProperties* ageProperties;
} Player;
typedef struct {
    u8 currentExtSword, currentExtShield, currentExtTunic, currentExtBoots;
} FixtureExtendedEquipment;
#include "mods/nei_save.h"
typedef NeiSaveData FixtureNeiSave;
typedef struct {
    s16 camDataIdx, setting, status, uid;
    Vec3f at, eye;
    f32 fov;
} Camera;
typedef struct {
    s16 state, frames;
    f32 unk_0C;
    void* segment;
    CsCmdActorCue* linkAction;
} CutsceneContext;
typedef struct {
    struct {
        u8 bButton;
    } restrictions;
} InterfaceContext;
struct PlayState {
    struct {
        void* gfxCtx;
        bool running;
        struct {
            struct {
                u16 button;
            } press, cur;
        } input[1];
    } state;
    s16 sceneNum, nextEntranceIndex, linkAgeOnLoad;
    u8 transitionTrigger, transitionType;
    struct {
        struct {
            s8 num, behaviorType2, echo;
        } curRoom;
        u8 unk_74[2];
    } roomCtx;
    struct {
        u8 seqId, natureAmbienceId;
    } sequenceCtx;
    struct {
        u8 unk_E0;
        u8 fillScreen;
        u8 screenFillColor[4];
    } envCtx;
    struct {
        int state, cursorSpecialPos;
    } pauseCtx;
    struct {
        struct {
            u32 tempSwch, tempCollect;
        } flags;
    } actorCtx;
    CutsceneContext csCtx;
    InterfaceContext interfaceCtx;
    CollisionCheckContext colChkCtx;
    int colCtx;
    u32 gameplayFrames;
    Player player;
    bool playerRemoved;
    Camera camera, subCamera;
    s16 activeCamera;
    bool subCameraAllocated;
};

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define SQ(x) ((x) * (x))
#define IS_ZERO(f) (fabsf(f) < 0.008f)
#define MASS_IMMOVABLE 0xFF
#define MASS_HEAVY 0xFE
#define _SHIFTL(v, s, w) (((u32)(v) & ((1U << (w)) - 1U)) << (s))
#define GET_PLAYER(p) ((p)->playerRemoved ? NULL : &(p)->player)
#define GET_ACTIVE_CAM(p) (&(p)->camera)
#define CAM_ID_MAIN 0
#define SUBCAM_NONE -1
#define CAM_STAT_WAIT 1
#define CAM_STAT_ACTIVE 7
#define CAM_SET_FREE0 0x21
#define LINK_AGE_ADULT 0
#define LINK_AGE_CHILD 1
#define LINK_IS_ADULT (gSaveContext.linkAge == LINK_AGE_ADULT)
#define LINK_AGE_IN_YEARS (LINK_IS_ADULT ? 17 : 5)
#define YEARS_CHILD 5
#define YEARS_ADULT 17
#define IS_RANDO fixtureRando
#define CVAR_ENHANCEMENT(name) "gEnhancements." name
#define CVAR_SETTING(name) "gSettings." name
#define OWNED_EQUIP_FLAG(type, value) (1U << ((type)*4 + (value)))
#define CHECK_OWNED_EQUIP(type, value) (gSaveContext.inventory.equipment & OWNED_EQUIP_FLAG(type, value))
#define CHECK_OWNED_EQUIP_ALT CHECK_OWNED_EQUIP
#define OWNED_EQUIP_FLAG_ALT OWNED_EQUIP_FLAG
#define ALL_EQUIP_VALUE(type) ((gSaveContext.inventory.equipment >> ((type)*4)) & 0xF)
#define CUR_UPG_VALUE(type) (gSaveContext.inventory.upgrades & 7)
#define CUR_EQUIP_VALUE(type) ((gSaveContext.equips.equipment >> ((type)*4)) & 0xF)
#define RESPAWN_MODE_DOWN 0
#define TRANS_TRIGGER_OFF 0
#define TRANS_TRIGGER_START 1
#define TRANS_TYPE_INSTANT 11
#define TRANS_TYPE_FADE_BLACK_FAST 6
#define TRANS_TYPE_FADE_WHITE_FAST 7
#define SCENE_LOST_WOODS 0x5B
#define SCENE_HYRULE_CASTLE 0x5F
#define SCENE_OUTSIDE_GANONS_CASTLE 0x64
#define ACTOR_BG_TOKI_SWD 0x6C
#define ACTOR_EN_VIEWER 0x2A
#define ENTR_CASTLE_GROUNDS_SOUTH_EXIT 0x0138
#define ENTR_HYRULE_FIELD_10 0x0282
#define ENTR_LINKS_HOUSE_CHILD_SPAWN 0x00BB
#define EVENTCHKINF_ENTERED_MASTER_SWORD_CHAMBER 0x4F
#define EVENTCHKINF_LEARNED_PRELUDE_OF_LIGHT 0x55
#define INFTABLE_SWORDLESS 0x1D0
#define RSK_SHUFFLE_MASTER_SWORD 1
#define VB_PLAY_ENTRANCE_CS 1
#define VB_GIVE_ITEM_MASTER_SWORD 2
#define VB_SET_BUTTON_ITEM_FROM_C_BUTTON_SLOT 3
#define VB_INFLICT_VOID_DAMAGE 4
#define VB_TEMP_B_SHOULD_RESTORE 5
#define VB_TEMP_B_RESTORE_SWORDLESS 6
#define VB_PLAYER_OVERRIDE_LIMB_DRAW 7
#define VB_EXECUTE_PLAYER_STARTMODE_FUNC 8
#define BTN_B 0x4000
#define BTN_A 0x8000
#define PLAYER_ACTION_HANDLER_7 7
#define PLAYER_STATE1_LOADING (1U << 0)
#define PLAYER_STATE1_START_CHANGING_HELD_ITEM (1U << 1)
#define UNK6AE_ROT_FOCUS_X 1
#define UNK6AE_ROT_UPPER_X 2
#define CHECK_BTN_ALL(state, mask) (((state) & (mask)) == (mask))
#define PAK_DL_STUB ((Gfx*)(uintptr_t)1)
enum {
    PLAYER_LIMB_L_HAND = 1,
    PLAYER_LIMB_R_HAND,
    PLAYER_LIMB_SHEATH,
    PLAYER_LIMB_WAIST,
    PLAYER_MODELTYPE_LH_OPEN,
    PLAYER_MODELTYPE_LH_CLOSED,
    PLAYER_MODELTYPE_LH_SWORD,
    PLAYER_MODELTYPE_LH_SWORD_2,
    PLAYER_MODELTYPE_LH_BGS,
    PLAYER_MODELTYPE_LH_HAMMER,
    PLAYER_MODELTYPE_LH_BOOMERANG,
    PLAYER_MODELTYPE_LH_BOTTLE,
    PLAYER_MODELTYPE_RH_OPEN,
    PLAYER_MODELTYPE_RH_CLOSED,
    PLAYER_MODELTYPE_RH_SHIELD,
    PLAYER_MODELTYPE_RH_BOW_SLINGSHOT,
    PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2,
    PLAYER_MODELTYPE_RH_OCARINA,
    PLAYER_MODELTYPE_RH_OOT,
    PLAYER_MODELTYPE_RH_HOOKSHOT,
    PLAYER_MODELTYPE_SHEATH_16,
    PLAYER_MODELTYPE_SHEATH_17,
    PLAYER_MODELTYPE_SHEATH_18,
    PLAYER_MODELTYPE_SHEATH_19,
    PLAYER_SHIELD_MAX = 4,
    PLAYER_LIMB_L_FOREARM = 0x40
};
#define BTN_ENABLED 0
#define BTN_DISABLED 0xFF
#define MOD_NONE 0
#define PAUSE_CURSOR_PAGE_LEFT 0
#define SEQ_PLAYER_BGM_MAIN 0
#define NA_BGM_STOP 0xFFFF
#define NA_BGM_DISABLED 0xFFFF
#define NA_BGM_GENERAL_SFX 0
#define NA_BGM_NO_MUSIC 0x7F
#define NATURE_ID_DISABLED 0xFF
#define NATURE_ID_NONE 0x13
#define NATURE_ID_KOKIRI_REGION 4
#define ENTR_LOST_WOODS_BRIDGE_WEST_EXIT 0x4D6
#define ENTR_LOST_WOODS_BRIDGE_EAST_EXIT 0x4DA
#define NA_BGM_MASTER_SWORD 0x53
#define NA_SE_IT_SWORD_PUTAWAY_STN 1
#define NA_SE_IT_SWORD_STICK_STN 2
#define NA_SE_VO_LI_SWORD_N 3
#define NA_SE_VO_LI_SWORD_L 4
#define NA_SE_IT_MASTER_SWORD_SWING 5
#define NA_SE_VO_LI_AUTO_JUMP 6
#define ANIMSFX_TYPE_WALKING 0
#define ANIMSFX_TYPE_GENERAL 1
#define ANIMSFX_TYPE_VOICE 2
#define ANIMSFX_TYPE_LANDING 3
#define ANIMSFX_DATA(type, frame) (frame)
#define ANIMMODE_ONCE 0
#define DECR(value) ((value) ? --(value) : 0)
#define PLAYER_STATE1_IN_CUTSCENE (1U << 29)
#define PLAYER_STATE1_IN_ITEM_CS (1U << 28)
#define PLAYER_STATE1_INPUT_DISABLED (1U << 5)
#define PLAYER_STATE1_CARRYING_ACTOR (1U << 11)
#define PLAYER_STATE1_TALKING (1U << 6)
#define PLAYER_STATE1_DEAD (1U << 7)
#define PLAYER_STATE1_CHARGING_SPIN_ATTACK (1U << 12)
#define PLAYER_STATE1_HANGING_OFF_LEDGE (1U << 13)
#define PLAYER_STATE1_CLIMBING_LEDGE (1U << 14)
#define PLAYER_STATE1_JUMPING (1U << 18)
#define PLAYER_STATE1_FREEFALL (1U << 19)
#define PLAYER_STATE1_FIRST_PERSON (1U << 20)
#define PLAYER_STATE1_CLIMBING_LADDER (1U << 21)
#define PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER (1U << 1)
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define M_PI 3.14159265358979323846
#define MTXMODE_APPLY 1
#define BGCHECKFLAG_GROUND 1
#define PLAYER_CSACTION_7 7
#define PLAYER_IA_SWORD_CS 1
#define PLAYER_IA_NONE 0
#define PLAYER_START_MODE_IDLE 13
#define PLAYER_START_MODE_TIME_TRAVEL 1
#define PLAYER_MODELGROUP_DEFAULT 0
#define PLAYER_MODELGROUP_SWORD 15
#define AGE_REQ_NONE 9
#define AGE_REQ_CHILD LINK_AGE_CHILD
#define AGE_REQ_ADULT LINK_AGE_ADULT
#define EXT_EQUIP_OWNED_SHIFT 16
#define ITEM_EXT_SWORD_1 0xE0
#define ITEM_EXT_SWORD_2 0xE1
#define ITEM_EXT_SWORD_3 0xE2
#define OPEN_DISPS(ctx) ((void)0)
#define CLOSE_DISPS(ctx) ((void)0)
#define gSPSegment(...) ((void)0)
#define gSPMatrix(...) ((void)0)
#define gSPDisplayList(unused, dl) Fixture_RecordDraw(dl)
#define osSyncPrintf(...) ((void)0)

#include "time_pedestal_actor.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_actor.h"
#include "soh/Enhancements/SwitchAge.h"

#ifdef __cplusplus
extern "C" {
#endif
extern SaveContext gSaveContext;
extern PlayState* gPlayState;
extern bool fixtureRando;
void func_80068DC0(PlayState*, CutsceneContext*);
void Environment_PlaySceneSequence(PlayState*);
void Fixture_AdultLoadRepair(void);
void Fixture_UpdatePedestalFill(PlayState*, Player*);
void Fixture_PlayerAnimationMove(PlayState*, Player*);
void Math_ApproachF(f32*, f32, f32, f32);
void AnimationContext_SetMoveActor(PlayState*, Actor*, SkelAnime*, f32);
u8 Fixture_GiveSword(PlayState*, u8);
u8 Return_Item(u8, int, u8);
u8 Inventory_DeleteEquipment(PlayState*, s16);
void GameInteractor_ExecuteOnEquipmentDelete(s16, u16);
void NeiSave_Save(SaveContext*, int, bool);
void NeiSave_Load(void);
void TradeItems_SyncWrite(void);
void TradeItems_SyncRead(void);
void Picto_SyncRead(void);
void Bottle_WheelResetTracking(void);
s32 Environment_IsForcedSequenceDisabled(void);
void Audio_PlayNatureAmbienceSequence(u8);
void Audio_PlaySceneSequence(u16);
void Audio_SetEnvReverb(s8);
extern FixtureExtendedEquipment gExtEquipState;
FixtureNeiSave* Nei_Save(void);
void ExtEquip_CleanupSlot(s16, u8);
void ExtEquip_ReloadBIcon(void);
void ExtEquip_RefreshPlayer(void);
void ExtEquip_ValidateForAgeWithoutProgression(u8);
s32 CVarGetInteger(const char*, s32);
extern u16 gEquipMasks[4], gEquipNegMasks[4];
extern u8 gEquipShifts[4];
extern CutsceneData D_808BB2F0[], D_808BB7A0[], D_808BBD90[];
extern const size_t gMasterSwordChildCutsceneWordCount, gMasterSwordAdultCutsceneWordCount;
void BgTokiSwd_Init(Actor*, PlayState*);
void BgTokiSwd_Update(Actor*, PlayState*);
void BgTokiSwd_Destroy(Actor*, PlayState*);
void BgTokiSwd_Draw(Actor*, PlayState*);
void DinFireSword_DrawPedestal(PlayState*);
void Inventory_SwapAgeEquipment(void);
void Fixture_PlayDestroyAgeHandoff(PlayState*);
void Fixture_HudRestore(PlayState*);
void func_80084BF4(PlayState*, u16);
void func_80083108(PlayState*);
void Play_PerformSave(PlayState*);
void Play_SaveSceneFlags(PlayState*);
void Save_SaveFile(void);
void Interface_LoadItemIcon1(PlayState*, u16);
void Interface_ChangeHudVisibilityMode(s16);
void func_808519EC(PlayState*, Player*, CsCmdActorCue*);
void func_80851A50(PlayState*, Player*, CsCmdActorCue*);
s32 LinkAnimation_Update(PlayState*, SkelAnime*);
void LinkAnimation_Change(PlayState*, SkelAnime*, LinkAnimationHeader*, f32, f32, f32, u8, f32);
void Fixture_PlayerStartMode(PlayState*, s32);
void Player_StartMode_TimeTravel(PlayState*, Player*);
void Player_StartMode_Idle(PlayState*, Player*);
void Player_Action_8084E9AC(Player*, PlayState*);
void Player_Action_Idle(Player*, PlayState*);
s32 Player_SetupAction(PlayState*, Player*, PlayerActionFunc, s32);
void func_8083C0E8(Player*, PlayState*);
void func_80846720(PlayState*, Player*, s32);
void Player_AnimPlayOnce(PlayState*, Player*, LinkAnimationHeader*);
LinkAnimationHeader* Player_GetIdleAnim(Player*);
s32 LinkAnimation_OnFrame(SkelAnime*, f32);
void Player_PlaySfx(Player*, u16);
void Player_PlayVoiceSfx(Player*, u16);
void Player_ProcessAnimSfxList(Player*, AnimSfxEntry*);
extern Gfx *gPlayerLeftHandBgsDLs[], *gPlayerLeftHandClosedDLs[];
extern Gfx* sPlayerRightHandClosedDLs[];
extern const char gLinkChildLeftHandHoldingMasterSwordDL[];
extern const char gLinkAdultLeftHandHoldingMasterSwordNearDL[], gLinkAdultLeftHandHoldingMasterSwordFarDL[];
void Fixture_ApplyLateHandOverrides(PlayState*, Player*, s32, Gfx**);
void Fixture_ApplyLateHandOverridesWithRot(PlayState*, Player*, s32, Gfx**, Vec3s*);
void Fixture_DrawPostHand(PlayState*, Player*, Gfx**);
void DinFireSword_Draw(PlayState*, Player*);
void BossRemains_DrawOdolwaSword(PlayState*, Player*);
Gfx* Player_ResolveLimbDLForDummyOrLocal(void*);
bool NeiArticulated_UsesSwitchHook(const Player*);
bool NeiArticulated_ApplySwitchHookHand(PlayState*, Player*, Gfx**, Gfx*);
Gfx* PakLoader_GetEquipDL(Player*, s32);
Gfx* PakLoader_GetTimePedestalSwordDL(void);
Gfx* PakLoader_GetTimePedestalHandDL(void);
Gfx* CustomEquipment_GetTimePedestalSwordDL(void);
void Fixture_RecordDraw(const void*);
void Matrix_Push(void);
void Matrix_Pop(void);
void Matrix_Translate(f32, f32, f32, u8);
void Matrix_Scale(f32, f32, f32, u8);
void Matrix_RotateZ(f32, u8);
s16 Play_CreateSubCamera(PlayState*);
s16 Play_GetActiveCamId(PlayState*);
s16 Play_ChangeCameraStatus(PlayState*, s16, s16);
Camera* Play_GetCamera(PlayState*, s16);
void Play_ClearCamera(PlayState*, s16);
s16 Play_CameraGetUID(PlayState*, s16);
s32 func_800C0808(PlayState*, s16, Player*, s16);
s32 Play_CameraSetAtEye(PlayState*, s16, Vec3f*, Vec3f*);
s32 Play_CameraSetFov(PlayState*, s16, f32);
void Play_CopyCamera(PlayState*, s16, s16);
void Letterbox_SetSizeTarget(s32);
extern const char object_toki_objects_DL_001BD0[];
u8 PakLoader_HasActiveModel(void);
u8 PakLoader_UsedCombinedDL(u8);
bool GameInteractor_InvisibleLinkActive(void);
s32 CustomEquipment_OverrideMasterSwordHand(PlayState*, Gfx**);
extern const char gCustomMasterSwordDL[], gLinkChildLeftFistNearDL[], gLinkAdultLeftHandClosedNearDL[];
Gfx* ResourceMgr_LoadGfxByName(const char*);
u8 ResourceMgr_FileAltExists(const char*);
u8 ResourceGetIsCustomByName(const char*);
u8 TransformMasks_IsTransformedAny(void);
void Fixture_BuildHandItemDL(PlayState*, Gfx**, Gfx*, Gfx*, bool);
void Actor_ProcessInitChain(Actor*, void*);
s32 Collider_InitCylinder(PlayState*, ColliderCylinder*);
s32 Collider_SetCylinder(PlayState*, ColliderCylinder*, Actor*, ColliderCylinderInit*);
void Collider_UpdateCylinder(Actor*, ColliderCylinder*);
void CollisionCheck_SetInfo(CollisionCheckInfo*, DamageTable*, CollisionCheckInfoInit*);
s32 Collider_DestroyCylinder(PlayState*, ColliderCylinder*);
void CollisionCheck_SetOC(PlayState*, CollisionCheckContext*, Collider*);
void CollisionCheck_OC_CylVsCyl(PlayState*, CollisionCheckContext*, Collider*, Collider*);
s32 CollisionCheck_Incompatible(Collider*, Collider*);
void Fixture_InitPlayerCollision(PlayState*, ColliderCylinder*);
void Inventory_ChangeEquipment(s16, u16);
s32 Flags_GetEventChkInf(s32);
s32 Flags_GetInfTable(s32);
void Flags_SetEventChkInf(s32);
void Flags_SetInfTable(s32);
void Flags_UnsetInfTable(s32);
s32 Actor_IsFacingAndNearPlayer(Actor*, f32, s16);
s32 Actor_IsFacingPlayer(Actor*, s16);
s32 Play_InCsMode(PlayState*);
s32 Actor_HasParent(Actor*, PlayState*);
void Actor_OfferCarry(Actor*, PlayState*);
s32 Actor_OfferGetItem(Actor*, PlayState*, s32, f32, f32);
s32 Player_ActionHandler_2(Player*, PlayState*);
s32 Player_UpdateUpperBody(Player*, PlayState*);
s32 func_8008F128(Player*);
void Player_Action_8084E604(Player*, PlayState*);
void Player_UpperAction_ChangeHeldItem(Player*, PlayState*);
s32 Fixture_TurnActionHandler(Player*, PlayState*);
s32 Fixture_TryTurnInPlace(PlayState*, Player*);
f32 BgCheck_EntityRaycastFloor5(PlayState*, void*, void**, s32*, Actor*, Vec3f*);
s32 Player_GetExplosiveHeld(Player*);
bool GameInteractor_Should(int, bool, ...);
void Item_Give(PlayState*, s16);
void Entrance_SetEntranceDiscovered(s16, bool);
void Audio_QueueSeqCmd(u32);
void Audio_PlayActorSound2(Actor*, u16);
s32 Flags_GetEnv(PlayState*, s16);
s32 Randomizer_GetSettingValue(s32);
void Gfx_SetupDL_25Opa(void*);
void func_8002EBCC(Actor*, PlayState*, s32);
void Math_Vec3f_Copy(Vec3f*, Vec3f*);
f32 Math_SinS(s16);
f32 Math_CosS(s16);
void Player_AnimPlayOnceAdjusted(PlayState*, Player*, LinkAnimationHeader*);
void Player_StartAnimMovement(PlayState*, Player*, s32);
void Player_InitItemAction(PlayState*, Player*, s8);
s32 Player_ActionToModelGroup(Player*, s32);
void Player_SetEquipmentData(PlayState*, Player*);
s32 Player_SetCsAction(PlayState*, Actor*, u8);
void Audio_SetCutsceneFlag(s32);
#ifdef __cplusplus
}

#include <functional>
#include <map>
#include <string>
class SaveManager {
  public:
    static SaveManager* Instance;
    std::map<std::string, uint64_t> data;
    std::string prefix;
    template <typename T> void SaveData(const char* key, T value) {
        data[prefix + key] = value;
    }
    template <typename T> void LoadData(const char* key, T& value, T fallback) {
        auto found = data.find(prefix + key);
        value = found == data.end() ? fallback : static_cast<T>(found->second);
    }
    template <typename F> void SaveArray(const char* key, size_t count, F fn) {
        auto parent = prefix;
        for (size_t i = 0; i < count; ++i) {
            prefix = parent + key + "/" + std::to_string(i) + "/";
            fn(i);
        }
        prefix = parent;
    }
    template <typename F> void LoadArray(const char* key, size_t count, F fn) {
        SaveArray(key, count, fn);
    }
};
using HOOK_ID = unsigned;
class GameInteractor {
  public:
    struct OnVanillaBehavior {};
    static GameInteractor* Instance;
    std::map<unsigned, std::function<void(bool*)>> hooks;
    unsigned next = 1;
    unsigned Add(std::function<void(bool*)> fn) {
        hooks[next] = fn;
        return next++;
    }
    template <typename T> void UnregisterGameHookForID(unsigned id) {
        hooks.erase(id);
    }
};
#define REGISTER_VB_SHOULD(flag, body) GameInteractor::Instance->Add([](bool* should) body)
#endif
