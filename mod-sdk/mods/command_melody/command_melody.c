#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/ModApi/ModApi.h"
#include "partner_control.h"

#define COMMAND_SONG "nei.mm_song_command_melody"
#define MAX_PARTNERS 8
static const SOHModApi* sApi;
static NeiPartnerV1 sPartners[MAX_PARTNERS];
static s32 sCount, sActive = -1;
static u8 sNotes[8], sNoteCount, sPending, sRelease;
static u16 sPendingTimeout, sNoteAge;
static u8 sLastPitch = OCARINA_PITCH_NONE;
static s16 sCamera = CAM_ID_NONE, sParentCamera = CAM_ID_MAIN, sYaw;
static Input sInput;
static s16 sHealth;
static Vec3f sEye;
static ActorFunc sPlayerDraw;
static s8 sInvincibility;
static u8 sShadowAlpha;
static bool sWasMidair,sTracking;
static const char* sHooks[] = { "OnOcarinaNote", "OnGameFrameUpdate", "OnPlayerFilterInput", "OnSceneInit",
                               "OnLoadGame", "OnExitGame", "OnInterfaceDrawEnd" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sHooks, ARRAY_COUNT(sHooks) };
s32 Player_SetupAction(PlayState*, Player*, PlayerActionFunc, s32);
void func_80839FFC(Player*, PlayState*);
LinkAnimationHeader* Player_GetIdleAnim(Player*);
void Player_AnimPlayLoop(PlayState*,Player*,LinkAnimationHeader*);

static bool Exists(PlayState* play, const Actor* wanted) {
    if (!wanted) return false;
    for (s32 cat=0; cat<ACTORCAT_MAX; ++cat)
        for (Actor* a=play->actorCtx.actorLists[cat].head; a; a=a->next)
            if (a==wanted) return a->update != NULL;
    return false;
}

static bool RegisterPartner(const NeiPartnerV1* partner) {
    if (!partner || partner->size < offsetof(NeiPartnerV1,flags) || !partner->key || !partner->name ||
        !partner->actor || !partner->begin || !partner->end || !partner->input) return false;
    NeiPartnerV1 copy={0};memcpy(&copy,partner,MIN(partner->size,sizeof(copy)));
    for (s32 i=0;i<sCount;++i) if (!strcmp(partner->key,sPartners[i].key)) { sPartners[i]=copy; return true; }
    if (sCount == MAX_PARTNERS) return false;
    sPartners[sCount++]=copy;
    return true;
}

static void HoldLink(Player* player, PlayState* play) {
    player->linearVelocity = player->actor.speedXZ = 0;
    player->actor.velocity.x = player->actor.velocity.z = 0;
    if (sTracking && sActive>=0) {
        Actor* actor=sPartners[sActive].actor(play);
        if (Exists(play,actor)) {
            player->actor.world.pos=actor->world.pos;player->actor.prevPos=actor->world.pos;
            player->actor.parent=actor;player->stateFlags3|=PLAYER_STATE3_MIDAIR;
            player->actor.draw=NULL;player->actor.shape.shadowAlpha=0;
            player->invincibilityTimer=-40;
        }
    }
    LinkAnimation_Update(play,&player->skelAnime);
}

static void End(PlayState* play, bool restore) {
    s32 active=sActive;
    sActive=-1;
    sRelease=2;
    if (play && active>=0) sPartners[active].end(play);
    if (play && sCamera != CAM_ID_NONE && sCamera < NUM_CAMS && play->cameraPtrs[sCamera]) {
        if (restore && play->activeCamera==sCamera && play->cameraPtrs[sParentCamera])
            Play_ChangeCameraStatus(play,sParentCamera,CAM_STAT_ACTIVE);
        Play_ClearCamera(play,sCamera);
    }
    sCamera=CAM_ID_NONE;
    if (play && restore) {
        Player* player=GET_PLAYER(play);
        if (player && sTracking) {
            player->actor.parent=NULL;
            if (!sWasMidair) player->stateFlags3&=~PLAYER_STATE3_MIDAIR;
            if (!player->actor.draw) player->actor.draw=sPlayerDraw;
            player->actor.shape.shadowAlpha=sShadowAlpha;player->invincibilityTimer=sInvincibility;
        }
        if (player && player->actionFunc==HoldLink) {
            func_80839FFC(player,play);
            Player_AnimPlayLoop(play,player,Player_GetIdleAnim(player));
        }
    }
    sTracking=false;
}

static bool Begin(PlayState* play, s32 index) {
    Player* player=GET_PLAYER(play);
    if (index<0 || index>=sCount || player->rideActor || player->heldActor || player->actor.parent ||
        (!(sPartners[index].flags&NEI_PARTNER_TRACK_PLAYER) && !(player->actor.bgCheckFlags&BGCHECKFLAG_GROUND)) ||
        !sPartners[index].begin(play)) return false;
    sParentCamera=Play_GetActiveCamId(play);
    sCamera=Play_CreateSubCamera(play);
    if (sCamera==CAM_ID_NONE) { sPartners[index].end(play); return false; }
    Camera* main=Play_GetCamera(play,sParentCamera);
    sYaw=Math_Vec3f_Yaw(&main->eye,&main->at);
    sEye=main->eye;
    Play_CopyCamera(play,sCamera,sParentCamera);
    Play_CameraChangeSetting(play,sCamera,CAM_SET_FREE0);
    Play_ChangeCameraStatus(play,sParentCamera,CAM_STAT_WAIT);
    Play_ChangeCameraStatus(play,sCamera,CAM_STAT_ACTIVE);
    sActive=index;
    sTracking=(sPartners[index].flags&NEI_PARTNER_TRACK_PLAYER)!=0;
    sPlayerDraw=player->actor.draw;sInvincibility=player->invincibilityTimer;
    sShadowAlpha=player->actor.shape.shadowAlpha;
    sWasMidair=(player->stateFlags3&PLAYER_STATE3_MIDAIR)!=0;
    sHealth=gSaveContext.health;
    memset(&sInput,0,sizeof(sInput));
    Player_SetupAction(play,player,HoldLink,1);
    Player_AnimPlayLoop(play,player,Player_GetIdleAnim(player));
    return true;
}

static void Notes(u8 note, f32 modulator, s8 bend) {
    if (!gPlayState || sActive>=0) return;
    if (note==OCARINA_PITCH_NONE) {sLastPitch=note;return;}
    if (note==sLastPitch) return;
    sLastPitch=note;
    static const u8 melody[8]={2,11,2,9,2,11,2,9};
    memmove(sNotes,sNotes+1,7); sNotes[7]=note;
    sNoteAge=0;
    if (sNoteCount<8) ++sNoteCount;
    if (sNoteCount==8 && !memcmp(sNotes,melody,8) && sApi->IsCustomItemOwned(COMMAND_SONG)) {
        sPending=12; sNoteCount=0; sPendingTimeout=120;
        // mm_collectables owns recognition, chime and the native ocarina close.
    }
}

static void Filter(Player* player, Input* input) {
    if (sActive>=0) {
        // Read the original controller: other input blockers may already have filtered this copy.
        if (gPlayState) sInput=gPlayState->state.input[0];
    }
    if (sActive>=0 || sRelease) {
        if (sRelease) --sRelease;
        uint32_t start = input->press.button & BTN_START;
        memset(input,0,sizeof(*input));
        input->press.button=start; // Pause remains available.
    }
}

static void Tick(void) {
    PlayState* play=gPlayState;
    if (!play) return;
    Player* player=GET_PLAYER(play);
    if (!player || !player->actor.update) return;
    if (sNoteCount && ++sNoteAge>120) sNoteCount=0;
    if (sActive>=0) {
        Actor* partner=sPartners[sActive].actor(play);
        if (!Exists(play,partner) || player->actionFunc!=HoldLink || gSaveContext.health<sHealth ||
            (!sTracking && !(player->actor.bgCheckFlags&BGCHECKFLAG_GROUND)) ||
            (player->stateFlags1 & (PLAYER_STATE1_DEAD|PLAYER_STATE1_TALKING|PLAYER_STATE1_GETTING_ITEM|PLAYER_STATE1_IN_CUTSCENE)) ||
            play->transitionTrigger!=TRANS_TRIGGER_OFF || play->csCtx.state!=CS_STATE_IDLE ||
            play->activeCamera!=sCamera || (!sTracking && Actor_WorldDistXYZToActor(partner,&player->actor)>650)) {
            End(play,true); return;
        }
        if (play->pauseCtx.state) return;
        if (sInput.press.button&BTN_CUP) { End(play,true); return; }
        if (sInput.cur.button&BTN_CLEFT) sYaw+=0x300;
        if (sInput.cur.button&BTN_CRIGHT) sYaw-=0x300;
        sPartners[sActive].input(play,&sInput,sYaw);
        Vec3f at=partner->world.pos;
        at.y+=45;
        Vec3f desired=at;
        desired.x-=Math_SinS(sYaw)*170; desired.z-=Math_CosS(sYaw)*170; desired.y+=55;
        Vec3f hit; CollisionPoly* poly; s32 bg;
        if (BgCheck_EntityLineTest1(&play->colCtx,&at,&desired,&hit,&poly,true,true,true,true,&bg)) {
            desired=hit;
            desired.x+=(at.x-hit.x)*.1f; desired.y+=(at.y-hit.y)*.1f; desired.z+=(at.z-hit.z)*.1f;
        }
        Math_ApproachF(&sEye.x,desired.x,.3f,40); Math_ApproachF(&sEye.y,desired.y,.3f,40);
        Math_ApproachF(&sEye.z,desired.z,.3f,40);
        Play_CameraSetAtEye(play,sCamera,&at,&sEye);
        sInput.press.button=sInput.rel.button=0;
    } else if (sPending) {
        if (!sPendingTimeout || --sPendingTimeout==0) { sPending=0; return; }
        if (--sPending) return;
        if (play->pauseCtx.state || play->msgCtx.msgMode!=MSGMODE_NONE || play->csCtx.state!=CS_STATE_IDLE ||
            player->stateFlags1&(PLAYER_STATE1_IN_CUTSCENE|PLAYER_STATE1_DEAD|PLAYER_STATE1_TALKING)) {
            sPending=2; return;
        }
        Actor* focus=player->focusActor;
        s32 chosen=-1;
        f32 distance=600;
        for (s32 i=0;i<sCount;++i) {
            Actor* actor=sPartners[i].actor(play);
            if (!Exists(play,actor)) continue;
            if (actor==focus) { chosen=i; break; }
            f32 d=Actor_WorldDistXYZToActor(&player->actor,actor);
            if (d<distance) { distance=d; chosen=i; }
        }
        if (chosen>=0) Begin(play,chosen);
    }
}

static void Reset(void) { sActive=-1; sCamera=CAM_ID_NONE; sTracking=false;sPending=sNoteCount=sRelease=0; sNoteAge=sPendingTimeout=0; sLastPitch=OCARINA_PITCH_NONE; }
static void Scene(s16 scene) { Reset(); }
static void Load(s32 file) { Reset(); }
static void Exit(s32 file) { Reset(); }

static void Hint(void) {
    PlayState* play=gPlayState;
    if (sActive<0 || !play || play->pauseCtx.state) return;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx* branch=POLY_OPA_DISP;
    Gfx* list=Graph_GfxPlusOne(branch);
    gSPDisplayList(OVERLAY_DISP++,list);
    GfxPrint printer; GfxPrint_Init(&printer); GfxPrint_Open(&printer,list);
    GfxPrint_SetColor(&printer,255,245,190,255); GfxPrint_SetPosPx(&printer,20,192);
    GfxPrint_Printf(&printer,"%s: Stick Move  A Jump  B Action",sPartners[sActive].name);
    GfxPrint_SetPosPx(&printer,20,204);
    GfxPrint_Printf(&printer,"C Left/Right: Camera  C Up: Return");
    list=GfxPrint_Close(&printer); GfxPrint_Destroy(&printer);
    gSPEndDisplayList(list++); Graph_BranchDlist(branch,list); POLY_OPA_DISP=list;
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) { sApi=api; }
SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) { return &sRequirements; }
static bool BeginByKey(const char*key) {
    PlayState*play=gPlayState;if(!play||!key||sActive>=0||play->pauseCtx.state||play->csCtx.state!=CS_STATE_IDLE||
        play->msgCtx.msgMode!=MSGMODE_NONE||play->transitionTrigger!=TRANS_TRIGGER_OFF)return false;
    for(s32 i=0;i<sCount;++i)if(!strcmp(key,sPartners[i].key))return Begin(play,i);
    return false;
}
static void EndControl(void){if(sActive>=0)End(gPlayState,true);}
SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi,PublishService)) return;
    if (SOH_MOD_API_HAS(sApi,SetCustomItemText)) {
        sApi->SetCustomItemText(COMMAND_SONG,SOH_ITEM_TEXT_GET_ITEM,
            "You learned the %rCommand Melody%w!&Play it to control a nearby&companion. Target one to choose.^"
            "%b\x9F %y\xA7 %b\x9F %y\xA8 %b\x9F %y\xA7 %b\x9F %y\xA8%w^"
            "Use the Stick to move, %b\x9F%w to jump,&and %g\xA0%w to attack. Press %y\xA5%w&to return to Link.");
        sApi->SetCustomItemText(COMMAND_SONG,SOH_ITEM_TEXT_PAUSE,
            "%rCommand Melody&%b\x9F %y\xA7 %b\x9F %y\xA8 %b\x9F %y\xA7 %b\x9F %y\xA8%w&Control a nearby companion.");
    }
    sApi->PublishService("nei.partner.register.v1",(void*)RegisterPartner);
    sApi->PublishService("nei.partner.begin.v1",(void*)BeginByKey);
    sApi->PublishService("nei.partner.end.v1",(void*)EndControl);
    SOH_REGISTER_HOOK(sApi,OnOcarinaNote,Notes);
    SOH_REGISTER_HOOK(sApi,OnPlayerFilterInput,Filter);
    SOH_REGISTER_HOOK(sApi,OnGameFrameUpdate,Tick);
    SOH_REGISTER_HOOK(sApi,OnSceneInit,Scene);
    SOH_REGISTER_HOOK(sApi,OnLoadGame,Load);
    SOH_REGISTER_HOOK(sApi,OnExitGame,Exit);
    SOH_REGISTER_HOOK(sApi,OnInterfaceDrawEnd,Hint);
}
