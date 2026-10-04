/*
 * File: z_bg_toki_swd.c
 * Overlay: ovl_Bg_Toki_Swd
 * Description: Master Sword (Contains Cutscenes)
 */

#include "din_fire_sword.h"
#include "z_bg_toki_swd.h"
#include "objects/object_toki_objects/object_toki_objects.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/SwitchAge.h"
#include "soh/Enhancements/customequipment.h"
#include "overlays/actors/ovl_En_Viewer/static_story_actor.h"

#include <stdlib.h>
#include <string.h>

#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED

void BgTokiSwd_Init(Actor* thisx, PlayState* play);
void BgTokiSwd_Destroy(Actor* thisx, PlayState* play);
void BgTokiSwd_Update(Actor* thisx, PlayState* play);
void BgTokiSwd_Draw(Actor* thisx, PlayState* play);

void func_808BAF40(BgTokiSwd* this, PlayState* play);
void func_808BB0AC(BgTokiSwd* this, PlayState* play);
void func_808BB128(BgTokiSwd* this, PlayState* play);
void BgTokiSwd_TimePedestalWait(BgTokiSwd* this, PlayState* play);
void BgTokiSwd_TimePedestalCutscene(BgTokiSwd* this, PlayState* play);
void BgTokiSwd_FinishTimePedestal(BgTokiSwd* this, PlayState* play);
void func_80068DC0(PlayState* play, CutsceneContext* csCtx);

extern CutsceneData D_808BB2F0[];
extern CutsceneData D_808BB7A0[];
extern CutsceneData D_808BBD90[];
extern const size_t gMasterSwordChildCutsceneWordCount;
extern const size_t gMasterSwordAdultCutsceneWordCount;

const ActorInit Bg_Toki_Swd_InitVars = {
    ACTOR_BG_TOKI_SWD,
    ACTORCAT_PROP,
    FLAGS,
    OBJECT_TOKI_OBJECTS,
    sizeof(BgTokiSwd),
    (ActorFunc)BgTokiSwd_Init,
    (ActorFunc)BgTokiSwd_Destroy,
    (ActorFunc)BgTokiSwd_Update,
    (ActorFunc)BgTokiSwd_Draw,
    NULL,
};

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_NONE,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1 | OC2_UNK1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_NONE,
        OCELEM_ON,
    },
    { 10, 70, 0, { 0 } },
};

static CollisionCheckInfoInit sColChkInfoInit = { 10, 35, 100, MASS_IMMOVABLE };

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 25, ICHAIN_STOP),
};

// One local reload only. The next primary player init consumes pending even
// on a mismatched spawn, so reset/load/another entrance cannot replay it later.
static struct {
    PlayState* play;
    PlayState* fillPlay;
    Player* player;
    Vec3f startPos, returnPos;
    Vec3f cameraAt, cameraEye;
    s16 startYaw, returnYaw, scene, entrance;
    s16 subCamId, subCamUid;
    u16 fileNum;
    s8 room, age;
    u8 pending, cameraAttempted, skipFadeFrame, fadeOutFrame, fillAlpha;
} sTimePedestalArrival;

#define TIME_PEDESTAL_SKIP_FADE_FRAMES 8

static void BgTokiSwd_SetSkipFill(PlayState* play, u8 alpha) {
    sTimePedestalArrival.fillPlay = alpha != 0 ? play : NULL;
    sTimePedestalArrival.fillAlpha = alpha;
    play->envCtx.fillScreen = alpha != 0;
    play->envCtx.screenFillColor[0] = 255;
    play->envCtx.screenFillColor[1] = 255;
    play->envCtx.screenFillColor[2] = 255;
    play->envCtx.screenFillColor[3] = alpha;
}

static void BgTokiSwd_ClearSkipFill(PlayState* play) {
    if (sTimePedestalArrival.fillPlay != play) {
        return;
    }
    // Another transition or effect may have replaced the shared fill already.
    if (play->envCtx.screenFillColor[0] == 255 && play->envCtx.screenFillColor[1] == 255 &&
        play->envCtx.screenFillColor[2] == 255 && play->envCtx.screenFillColor[3] == sTimePedestalArrival.fillAlpha) {
        play->envCtx.fillScreen = false;
        play->envCtx.screenFillColor[3] = 0;
    }
    sTimePedestalArrival.fillPlay = NULL;
    sTimePedestalArrival.fadeOutFrame = 0;
}

void BgTokiSwd_UpdateTimePedestalFill(PlayState* play, Player* player) {
    if (play == NULL || player == NULL || player != GET_PLAYER(play) || sTimePedestalArrival.fillPlay != play ||
        sTimePedestalArrival.fadeOutFrame == 0) {
        return;
    }
    if (!play->state.running || play->transitionTrigger != TRANS_TRIGGER_OFF) {
        BgTokiSwd_ClearSkipFill(play);
        return;
    }
    // The primary player updates this once, even if the room contains several
    // pedestals or the pedestal has been removed during the exit animation.
    sTimePedestalArrival.fadeOutFrame--;
    BgTokiSwd_SetSkipFill(play, 255 * sTimePedestalArrival.fadeOutFrame / TIME_PEDESTAL_SKIP_FADE_FRAMES);
}

void BgTokiSwd_SetupAction(BgTokiSwd* this, BgTokiSwdActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void BgTokiSwd_Init(Actor* thisx, PlayState* play) {
    s32 pad;
    BgTokiSwd* this = (BgTokiSwd*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    this->actor.shape.yOffset = 800.0f;
    BgTokiSwd_SetupAction(this, func_808BAF40);
    this->localCutscene = NULL;
    this->localCutsceneStarted = false;
    this->localCutsceneFinished = false;
    this->skipFadeFrame = 0;

    if (this->actor.params == BG_TOKI_SWD_TIME_PEDESTAL) {
        // The authored grip anchor can sit inside the stump. Resolve the
        // supporting surface above it, rather than treating nearby ground as
        // part of a broad vertical interaction cylinder.
        Vec3f floorProbe = this->actor.world.pos;
        s32 floorBgId;
        floorProbe.y += 40.0f;
        this->actor.floorHeight = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &this->actor.floorPoly, &floorBgId,
                                                              &this->actor, &floorProbe);
        this->actor.floorBgId = floorBgId;
        BgTokiSwd_SetupAction(this, BgTokiSwd_TimePedestalWait);
        if (LINK_IS_ADULT) {
            this->actor.draw = NULL;
        }
    } else if (LINK_IS_ADULT) {
        if (IS_RANDO) {
            if (!CUR_UPG_VALUE(UPG_BOMB_BAG)) {
                for (size_t i = 0; i < 8; i++) {
                    if (gSaveContext.equips.buttonItems[i] == ITEM_BOMB) {
                        gSaveContext.equips.buttonItems[i] = ITEM_NONE;
                    }
                }
            }
        }
        this->actor.draw = NULL;
    } else if (IS_RANDO) {
        // don't give child link a kokiri sword if we don't have one
        uint32_t kokiriSwordBitMask = 1 << 0;
        if (!(gSaveContext.inventory.equipment & kokiriSwordBitMask)) {
            Player* player = GET_PLAYER(gPlayState);
            player->currentSwordItemId = ITEM_NONE;
            gSaveContext.equips.buttonItems[0] = ITEM_NONE;
            Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
        }
    }

    if (this->actor.params != BG_TOKI_SWD_TIME_PEDESTAL && gSaveContext.sceneLayer == 5) {
        play->roomCtx.unk_74[0] = 0xFF;
    }

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, NULL, &sColChkInfoInit);
}

void BgTokiSwd_Destroy(Actor* thisx, PlayState* play) {
    BgTokiSwd* this = (BgTokiSwd*)thisx;

    if (this->skipFadeFrame != 0) {
        BgTokiSwd_ClearSkipFill(play);
    }
    if (this->localCutscene != NULL) {
        // Actor teardown happens after the age-equipment handoff and after the
        // player is deleted. It must never request another age transition.
        if (play->csCtx.segment == this->localCutscene) {
            Player* player = GET_PLAYER(play);
            gSaveContext.seqId = (u8)NA_BGM_DISABLED;
            gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
            if (play->state.running && player != NULL) {
                // If only this actor is removed, use the native camera cleanup
                // before releasing the camera spline's backing allocation.
                play->csCtx.unk_0C = 0.0f;
                func_80068DC0(play, &play->csCtx);
                player->interactRangeActor = NULL;
                player->stateFlags1 &= ~PLAYER_STATE1_CARRYING_ACTOR;
                Player_SetCsAction(play, NULL, PLAYER_CSACTION_7);
                Environment_PlaySceneSequence(play);
            } else {
                play->csCtx.state = CS_STATE_IDLE;
                gSaveContext.cutsceneIndex = 0;
                Audio_SetCutsceneFlag(0);
            }
            gSaveContext.cutsceneTrigger = 0;
            play->csCtx.segment = NULL;
            play->csCtx.linkAction = NULL;
        }
        free(this->localCutscene);
        this->localCutscene = NULL;
    }
    Collider_DestroyCylinder(play, &this->collider);
}

void BgTokiSwd_TimePedestalWait(BgTokiSwd* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    if (!play->state.running || player == NULL) {
        return;
    }

    if (Actor_HasParent(&this->actor, play)) {
        const CutsceneData* source = LINK_IS_ADULT ? D_808BB7A0 : D_808BB2F0;
        size_t count = LINK_IS_ADULT ? gMasterSwordAdultCutsceneWordCount : gMasterSwordChildCutsceneWordCount;
        this->localCutscene = malloc(count * sizeof(CutsceneData));
        if (this->localCutscene == NULL ||
            !TimePedestalCutscene_Build(this->localCutscene, count, source, count, &this->actor.world.pos,
                                        this->actor.shape.rot.y, &this->ageSwapFrame)) {
            free(this->localCutscene);
            this->localCutscene = NULL;
            this->actor.parent = NULL;
            player->stateFlags1 &= ~PLAYER_STATE1_CARRYING_ACTOR;
            Player_SetCsAction(play, NULL, PLAYER_CSACTION_7);
            return;
        }
        this->returnPos = player->actor.world.pos;
        this->returnYaw = player->actor.shape.rot.y;
        this->localCutsceneStarted = false;
        player->interactRangeActor = &this->actor;
        this->actor.parent = NULL;
        play->csCtx.segment = this->localCutscene;
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP);
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_MASTER_SWORD);
        gSaveContext.cutsceneTrigger = 1;
        BgTokiSwd_SetupAction(this, BgTokiSwd_TimePedestalCutscene);
    } else if (play->transitionTrigger == TRANS_TRIGGER_OFF && !Play_InCsMode(play) &&
               !(player->stateFlags1 & PLAYER_STATE1_TALKING) && this->actor.floorPoly != NULL &&
               (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && player->actor.floorBgId == this->actor.floorBgId &&
               fabsf(player->actor.floorHeight - this->actor.floorHeight) < 2.0f &&
               fabsf(player->actor.world.pos.y - this->actor.floorHeight) < 4.0f &&
               (player->interactRangeActor == NULL || player->getItemId == GI_NONE)) {
        // Cover the whole stump top from every heading. The surface checks
        // above exclude the ground and sloping bark; the ceremony aligns Link.
        if (Actor_OfferGetItem(&this->actor, play, GI_NONE, 60.0f, 40.0f)) {
            // Withdraw any pending offer from these nearby static NPCs.
            // Their later ITEMACTION update also suppresses new talk offers
            // while this PROP has the sword interaction reserved.
            Actor* talkActor = player->talkActor;
            if (talkActor != NULL && talkActor->id == ACTOR_EN_VIEWER) {
                StaticStoryActorType type = StaticStoryActor_GetType(talkActor->params);
                if (type == STATIC_STORY_ACTOR_SARIA || type == STATIC_STORY_ACTOR_SKULL_KID) {
                    player->talkActor = NULL;
                    player->stateFlags2 &= ~PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER;
                }
            }
        }
    }
}

s32 BgTokiSwd_RelocateTimePedestalPlayer(PlayState* play, Player* player) {
    Actor* actor = player->interactRangeActor;
    if (actor != NULL && actor->id == ACTOR_BG_TOKI_SWD && actor->params == BG_TOKI_SWD_TIME_PEDESTAL) {
        BgTokiSwd* this = (BgTokiSwd*)actor;
        if (this->localCutscene != NULL && play->csCtx.segment == this->localCutscene) {
            Vec3f pos = { -1.0f, 70.0f, 20.0f };
            TimePedestalCutscene_TransformPoint(&pos, &actor->world.pos, actor->shape.rot.y);
            player->actor.world.pos = pos;
            player->yaw = player->actor.shape.rot.y = actor->shape.rot.y + 0x8000;
            return true;
        }
    }
    return false;
}

s32 BgTokiSwd_BeginTimePedestalArrival(PlayState* play, Player* player) {
    if (play == NULL || player == NULL || player != GET_PLAYER(play)) {
        return false;
    }
    BgTokiSwd_ClearSkipFill(play);
    sTimePedestalArrival.fillPlay = NULL;
    sTimePedestalArrival.play = NULL;
    sTimePedestalArrival.player = NULL;
    sTimePedestalArrival.subCamId = SUBCAM_NONE;
    sTimePedestalArrival.cameraAttempted = false;
    sTimePedestalArrival.skipFadeFrame = 0;
    sTimePedestalArrival.fadeOutFrame = 0;
    if (!sTimePedestalArrival.pending) {
        return false;
    }
    sTimePedestalArrival.pending = false;
    if (!play->state.running || gSaveContext.respawnFlag != 1 || gSaveContext.fileNum != sTimePedestalArrival.fileNum ||
        gSaveContext.linkAge != sTimePedestalArrival.age || play->sceneNum != sTimePedestalArrival.scene ||
        gSaveContext.entranceIndex != sTimePedestalArrival.entrance ||
        gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex != sTimePedestalArrival.room ||
        player->actor.shape.rot.y != sTimePedestalArrival.returnYaw ||
        memcmp(&player->actor.world.pos, &sTimePedestalArrival.returnPos, sizeof(Vec3f)) != 0) {
        return false;
    }
    sTimePedestalArrival.play = play;
    sTimePedestalArrival.player = player;
    player->actor.world.pos = sTimePedestalArrival.startPos;
    player->yaw = player->actor.shape.rot.y = sTimePedestalArrival.startYaw;
    return true;
}

s32 BgTokiSwd_IsTimePedestalArrival(PlayState* play, Player* player) {
    return play != NULL && player != NULL && sTimePedestalArrival.play == play &&
           sTimePedestalArrival.player == player && player == GET_PLAYER(play);
}

void BgTokiSwd_UpdateTimePedestalArrivalCamera(PlayState* play, Player* player) {
    if (!BgTokiSwd_IsTimePedestalArrival(play, player) || !play->state.running ||
        play->transitionTrigger == TRANS_TRIGGER_START || sTimePedestalArrival.cameraAttempted) {
        return;
    }
    // Player_Init runs before Play_Init initializes the main camera. Acquire
    // the exit shot on its first live update, so the scene's camera setup
    // cannot replace it with the ordinary behind-Link respawn view.
    sTimePedestalArrival.cameraAttempted = true;
    if (Play_GetActiveCamId(play) != CAM_ID_MAIN) {
        return;
    }
    s16 subCamId = Play_CreateSubCamera(play);
    if (subCamId == SUBCAM_NONE) {
        return; // Keep the animation and its skip usable if no camera is free.
    }
    sTimePedestalArrival.subCamId = subCamId;
    sTimePedestalArrival.subCamUid = Play_CameraGetUID(play, subCamId);
    func_800C0808(play, subCamId, player, CAM_SET_FREE0);
    Play_CameraSetAtEye(play, subCamId, &sTimePedestalArrival.cameraAt, &sTimePedestalArrival.cameraEye);
    Play_CameraSetFov(play, subCamId, 55.0f);
    Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
    Play_ChangeCameraStatus(play, subCamId, CAM_STAT_ACTIVE);
    Letterbox_SetSizeTarget(0x20);
}

s32 BgTokiSwd_SkipTimePedestalArrival(PlayState* play, Player* player) {
    if (!BgTokiSwd_IsTimePedestalArrival(play, player)) {
        return false;
    }
    // Read only during the live player update, after GameState_ReqPadData.
    // PadMgr consumes press edges each frame: a held departure B cannot skip
    // arrival, while a release/repress during loading is a new valid edge.
    // The arrival is the player's local exit movement, not a story cutscene.
    // Keep the global story skip scoped to the departure ceremony above; only
    // a fresh B edge skips this independent phase.
    if (sTimePedestalArrival.skipFadeFrame == 0 && !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        return false;
    }
    if (sTimePedestalArrival.skipFadeFrame < TIME_PEDESTAL_SKIP_FADE_FRAMES) {
        sTimePedestalArrival.skipFadeFrame++;
        BgTokiSwd_SetSkipFill(play, 255 * sTimePedestalArrival.skipFadeFrame / TIME_PEDESTAL_SKIP_FADE_FRAMES);
        return -1; // Hold the current animation pose until the handoff is covered.
    }
    BgTokiSwd_SetSkipFill(play, 255);
    return true;
}

s32 BgTokiSwd_EndTimePedestalArrival(PlayState* play, Player* player) {
    if (play != NULL && player != NULL && player == GET_PLAYER(play) &&
        (!play->state.running || play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        BgTokiSwd_ClearSkipFill(play);
    }
    if (!BgTokiSwd_IsTimePedestalArrival(play, player)) {
        return false;
    }
    player->actor.world.pos = sTimePedestalArrival.returnPos;
    // Keep the native animation's pedestal-facing finish. Restoring the saved
    // pre-swap yaw here visibly twists the whole character as control returns;
    // the saved position is still restored to keep the actor on safe ground.
    s16 subCamId = sTimePedestalArrival.subCamId;
    if (subCamId != SUBCAM_NONE && Play_CameraGetUID(play, subCamId) == sTimePedestalArrival.subCamUid) {
        s32 active = Play_GetActiveCamId(play) == subCamId;
        if (active && play->state.running) {
            // Resume normal camera movement from this shot, not the stale
            // spawn-camera image. Only release the camera owned by this phase.
            Play_CopyCamera(play, CAM_ID_MAIN, subCamId);
            Letterbox_SetSizeTarget(0);
        }
        Play_ClearCamera(play, subCamId);
        if (active) {
            Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        }
    }
    sTimePedestalArrival.subCamId = SUBCAM_NONE;
    if (sTimePedestalArrival.skipFadeFrame != 0 && sTimePedestalArrival.fillPlay == play) {
        sTimePedestalArrival.fadeOutFrame = TIME_PEDESTAL_SKIP_FADE_FRAMES;
    }
    sTimePedestalArrival.skipFadeFrame = 0;
    sTimePedestalArrival.play = NULL;
    sTimePedestalArrival.player = NULL;
    return true;
}

s32 BgTokiSwd_GetTimePedestalHandState(PlayState* play, Player* player) {
    if (play == NULL || player == NULL || player != GET_PLAYER(play) || !play->state.running) {
        return BG_TOKI_SWD_HAND_UNCHANGED;
    }
    if (BgTokiSwd_IsTimePedestalArrival(play, player)) {
        return LINK_IS_ADULT ? BG_TOKI_SWD_HAND_MASTER_SWORD : BG_TOKI_SWD_HAND_UNCHANGED;
    }
    if (play->csCtx.state == CS_STATE_IDLE) {
        return BG_TOKI_SWD_HAND_UNCHANGED;
    }
    Actor* actor = player->interactRangeActor;
    if (actor == NULL || actor->id != ACTOR_BG_TOKI_SWD || actor->params != BG_TOKI_SWD_TIME_PEDESTAL) {
        return BG_TOKI_SWD_HAND_UNCHANGED;
    }
    BgTokiSwd* this = (BgTokiSwd*)actor;
    if (this->localCutscene == NULL || play->csCtx.segment != this->localCutscene ||
        (this->localCutsceneFinished && play->transitionTrigger != TRANS_TRIGGER_START)) {
        return BG_TOKI_SWD_HAND_UNCHANGED;
    }
    if (LINK_IS_ADULT) {
        if (player->heldItemAction != PLAYER_IA_SWORD_CS) {
            return BG_TOKI_SWD_HAND_UNCHANGED;
        }
        // Frame 70 closes the hand without changing leftHandType. Preserve
        // that insertion cue after the ordinary PAK/custom equipment hooks.
        return player->leftHandDLists == &gPlayerLeftHandClosedDLs[LINK_AGE_ADULT] ? BG_TOKI_SWD_HAND_CLOSED
                                                                                   : BG_TOKI_SWD_HAND_MASTER_SWORD;
    }
    // The native frame-87 handoff selects this child-only Master Sword DL.
    // Never infer a ceremonial weapon from inventory or another player's pose.
    return player->leftHandDLists == &gPlayerLeftHandBgsDLs[LINK_AGE_CHILD] ? BG_TOKI_SWD_HAND_MASTER_SWORD
                                                                            : BG_TOKI_SWD_HAND_UNCHANGED;
}

s32 BgTokiSwd_GetChildSwordPullFloor(PlayState* play, Player* player, f32* floorY) {
    if (play == NULL || player == NULL || floorY == NULL || player != GET_PLAYER(play) || LINK_IS_ADULT ||
        play->csCtx.state == CS_STATE_IDLE) {
        return false;
    }
    Actor* actor = player->interactRangeActor;
    if (actor == NULL || actor->id != ACTOR_BG_TOKI_SWD || actor->params != BG_TOKI_SWD_TIME_PEDESTAL) {
        return false;
    }
    BgTokiSwd* pedestal = (BgTokiSwd*)actor;
    // The planted sword must remain aligned with the reaching hand. Once the
    // frame-87 handoff hides that sword, the held blade follows Link's limb.
    if (pedestal->localCutscene == NULL || play->csCtx.segment != pedestal->localCutscene ||
        pedestal->localCutsceneFinished || actor->draw != NULL || actor->floorPoly == NULL ||
        player->leftHandDLists != &gPlayerLeftHandBgsDLs[LINK_AGE_CHILD]) {
        return false;
    }
    *floorY = actor->floorHeight;
    return true;
}

void BgTokiSwd_FinishTimePedestal(BgTokiSwd* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    if (!play->state.running || player == NULL || this->localCutsceneFinished ||
        play->transitionTrigger == TRANS_TRIGGER_START || play->csCtx.segment != this->localCutscene) {
        return;
    }

    Vec3f ceremonyPos = player->actor.world.pos;
    s16 ceremonyYaw = player->actor.shape.rot.y;
    this->localCutsceneFinished = true;
    player->actor.world.pos = this->returnPos;
    player->actor.shape.rot.y = this->returnYaw;
    player->yaw = this->returnYaw;
    player->interactRangeActor = NULL;
    this->actor.parent = NULL;
    // Match the native terminal cutscene state without executing its destination
    // command, which would edit story flags and enter the Temple of Time.
    play->csCtx.state = CS_STATE_UNSKIPPABLE_EXEC;
    play->csCtx.linkAction = NULL;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.cutsceneTrigger = 0;
    Audio_SetCutsceneFlag(0);
    // Some Lost Woods entrances continue BGM. The sword fanfare replaced the
    // scene track, so force normal scene music to restart on the local reload.
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
    SwitchAgeWithoutProgression();
    // SwitchAge's instant reload exposes the last camera image for the entire
    // synchronous load. Finish a visible fade first, then fade back into the
    // independent exit animation. Keep the safe respawn recorded above, but
    // do not visibly snap Link back to his approach position during fade-out.
    play->transitionType = TRANS_TYPE_FADE_WHITE_FAST;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_WHITE_FAST;
    player->actor.world.pos = ceremonyPos;
    player->yaw = player->actor.shape.rot.y = ceremonyYaw;
    player->interactRangeActor = &this->actor;
    // Both a completed and a manually skipped departure get their own arrival.
    // Keep the ordinary respawn contract; only its exact next spawn can consume
    // this visual continuation, before native story/rando start-mode hooks run.
    sTimePedestalArrival.pending = play->transitionTrigger == TRANS_TRIGGER_START && gSaveContext.respawnFlag == 1;
    sTimePedestalArrival.startPos = (Vec3f){ -1.0f, 69.0f, 20.0f };
    TimePedestalCutscene_TransformPoint(&sTimePedestalArrival.startPos, &this->actor.world.pos,
                                        this->actor.shape.rot.y);
    sTimePedestalArrival.startYaw = this->actor.shape.rot.y + 0x8000;
    // Frame the far side of the stump toward Sheik and Saria in room 10.
    // These points use the same pedestal-relative coordinates as the native
    // cutscene, so moving the authored sword moves both ages' shots together.
    sTimePedestalArrival.cameraAt = (Vec3f){ -59.0f, play->linkAgeOnLoad == LINK_AGE_CHILD ? 103.0f : 118.0f, 56.0f };
    sTimePedestalArrival.cameraEye = (Vec3f){ -80.0f, play->linkAgeOnLoad == LINK_AGE_CHILD ? 132.0f : 150.0f, 165.0f };
    TimePedestalCutscene_TransformPoint(&sTimePedestalArrival.cameraAt, &this->actor.world.pos,
                                        this->actor.shape.rot.y);
    TimePedestalCutscene_TransformPoint(&sTimePedestalArrival.cameraEye, &this->actor.world.pos,
                                        this->actor.shape.rot.y);
    sTimePedestalArrival.returnPos = this->returnPos;
    sTimePedestalArrival.returnYaw = this->returnYaw;
    sTimePedestalArrival.scene = play->sceneNum;
    sTimePedestalArrival.entrance = play->nextEntranceIndex;
    sTimePedestalArrival.room = play->roomCtx.curRoom.num;
    sTimePedestalArrival.age = play->linkAgeOnLoad;
    sTimePedestalArrival.fileNum = gSaveContext.fileNum;
}

void BgTokiSwd_TimePedestalCutscene(BgTokiSwd* this, PlayState* play) {
    if (!play->state.running || GET_PLAYER(play) == NULL || this->localCutsceneFinished ||
        play->transitionTrigger == TRANS_TRIGGER_START || play->csCtx.segment != this->localCutscene) {
        if (this->skipFadeFrame != 0 && (!play->state.running || !this->localCutsceneFinished)) {
            BgTokiSwd_ClearSkipFill(play);
            this->skipFadeFrame = 0;
        }
        return;
    }
    if (play->csCtx.state != CS_STATE_IDLE) {
        this->localCutsceneStarted = true;
    }
    // Preserve the native frame-70/87 handoff, sword model, sounds and animation.
    if (Actor_HasParent(&this->actor, play)) {
        if (!LINK_IS_ADULT) {
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_PUTAWAY_STN);
        }
        this->actor.draw = LINK_IS_ADULT ? BgTokiSwd_Draw : NULL;
        this->actor.parent = NULL;
    }
    GET_PLAYER(play)->interactRangeActor = &this->actor;

    // The copied script deliberately omits the native destination command, so
    // its automatic story-skip hook never runs. Read the same setting directly:
    // that hook would also award story flags/items inappropriate for this actor.
    // B can skip either local ceremony after the native camera has started.
    if (this->localCutsceneStarted && play->csCtx.frames > 20 && play->csCtx.frames < this->ageSwapFrame &&
        (this->skipFadeFrame != 0 || CVarGetInteger(CVAR_ENHANCEMENT("TimeSavers.SkipCutscene.Story"), IS_RANDO) ||
         CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B))) {
        if (this->skipFadeFrame < TIME_PEDESTAL_SKIP_FADE_FRAMES) {
            this->skipFadeFrame++;
            BgTokiSwd_SetSkipFill(play, 255 * this->skipFadeFrame / TIME_PEDESTAL_SKIP_FADE_FRAMES);
            return;
        }
        BgTokiSwd_FinishTimePedestal(this, play);
        return;
    }

    if (this->localCutsceneStarted &&
        (play->csCtx.frames >= this->ageSwapFrame || play->csCtx.state == CS_STATE_UNSKIPPABLE_INIT ||
         play->csCtx.state == CS_STATE_IDLE)) {
        BgTokiSwd_FinishTimePedestal(this, play);
    }
}

void func_808BAF40(BgTokiSwd* this, PlayState* play) {
    if (((Flags_GetEventChkInf(EVENTCHKINF_ENTERED_MASTER_SWORD_CHAMBER)) == 0) && (gSaveContext.sceneLayer < 4) &&
        Actor_IsFacingAndNearPlayer(&this->actor, 800.0f, 0x7530) && !Play_InCsMode(play)) {
        Flags_SetEventChkInf(EVENTCHKINF_ENTERED_MASTER_SWORD_CHAMBER);
        if (GameInteractor_Should(VB_PLAY_ENTRANCE_CS, true, EVENTCHKINF_ENTERED_MASTER_SWORD_CHAMBER,
                                  gSaveContext.entranceIndex)) {
            play->csCtx.segment = D_808BBD90;
            gSaveContext.cutsceneTrigger = 1;
        }
    }

    if (!LINK_IS_ADULT || (Flags_GetEventChkInf(EVENTCHKINF_LEARNED_PRELUDE_OF_LIGHT) && !IS_RANDO) || IS_RANDO) {
        if (Actor_HasParent(&this->actor, play)) {
            if (!LINK_IS_ADULT) {
                if (GameInteractor_Should(VB_GIVE_ITEM_MASTER_SWORD, true)) {
                    Item_Give(play, ITEM_SWORD_MASTER);
                }
                play->csCtx.segment = D_808BB2F0;

                // Discover adult spawn
                Entrance_SetEntranceDiscovered(ENTR_HYRULE_FIELD_10, false);
            } else {
                play->csCtx.segment = D_808BB7A0;

                // Discover child spawn
                Entrance_SetEntranceDiscovered(ENTR_LINKS_HOUSE_CHILD_SPAWN, false);
            }
            Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP);
            Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_MASTER_SWORD);
            gSaveContext.cutsceneTrigger = 1;
            this->actor.parent = NULL;
            BgTokiSwd_SetupAction(this, func_808BB0AC);
        } else {
            Player* player = GET_PLAYER(play);
            if (Actor_IsFacingPlayer(&this->actor, 0x2000)) {
                Actor_OfferCarry(&this->actor, play);
            }
        }
    }
    if (gSaveContext.sceneLayer == 5) {
        if (play->roomCtx.unk_74[0] > 0) {
            play->roomCtx.unk_74[0]--;
        } else {
            play->roomCtx.unk_74[0] = 0;
        }
    }
}

void func_808BB0AC(BgTokiSwd* this, PlayState* play) {
    Player* player;

    // if sword has a parent it has been pulled/placed from the pedestal
    if (Actor_HasParent(&this->actor, play)) {
        if (!LINK_IS_ADULT) {
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_PUTAWAY_STN);
            this->actor.draw = NULL; // sword has been pulled, dont draw sword
        } else {
            this->actor.draw = BgTokiSwd_Draw; // sword has been placed, draw the master sword
        }
        BgTokiSwd_SetupAction(this, func_808BB128);
    } else {
        player = GET_PLAYER(play);
        player->interactRangeActor = &this->actor;
    }
}

void func_808BB128(BgTokiSwd* this, PlayState* play) {
    if (Flags_GetEnv(play, 1) && (play->roomCtx.unk_74[0] < 0xFF)) {
        play->roomCtx.unk_74[0] += 5;
    }
}

void BgTokiSwd_Update(Actor* thisx, PlayState* play) {
    BgTokiSwd* this = (BgTokiSwd*)thisx;

    this->actionFunc(this, play);
    // The custom sword sits on scene collision. Its stock body cylinder would
    // push Link out of the small stump's center and onto the edge of its top.
    if (this->actor.params != BG_TOKI_SWD_TIME_PEDESTAL) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
}

void BgTokiSwd_Draw(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    BgTokiSwd* this = (BgTokiSwd*)thisx;
    s32 pad[3];
    Gfx* selectedSword = NULL;

    // Do not draw the Master Sword in the pedestal if the player has not found it yet
    if (this->actor.params != BG_TOKI_SWD_TIME_PEDESTAL && IS_RANDO &&
        Randomizer_GetSettingValue(RSK_SHUFFLE_MASTER_SWORD) &&
        !CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    func_8002EBCC(&this->actor, play, 0);

    if (this->actor.params == BG_TOKI_SWD_TIME_PEDESTAL) {
        selectedSword = CustomEquipment_GetTimePedestalSwordDL();
    }
    Matrix_Push();
    if (selectedSword != NULL) {
        // Weapon pieces are authored at Link's 0.01 scale with their grip at
        // the origin and blade along +X. The actor uses 0.025, with a 20-unit
        // shape offset. Place the grip 40 units above the pedestal anchor and
        // turn the blade down without importing a hand or changing collision.
        Matrix_Translate(0.0f, 800.0f, 0.0f, MTXMODE_APPLY);
        Matrix_Scale(0.4f, 0.4f, 0.4f, MTXMODE_APPLY);
        Matrix_RotateZ(-M_PI / 2.0f, MTXMODE_APPLY);
    }

    gSPSegment(POLY_OPA_DISP++, 0x08,
               Gfx_TexScrollEx(play->state.gfxCtx, 0, -(play->gameplayFrames % 0x80), 32, 32, 0, -1));
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, selectedSword != NULL ? selectedSword : (Gfx*)object_toki_objects_DL_001BD0);
    if (this->actor.params == BG_TOKI_SWD_TIME_PEDESTAL && selectedSword != NULL) {
        DinFireSword_DrawPedestal(play);
    }
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}
