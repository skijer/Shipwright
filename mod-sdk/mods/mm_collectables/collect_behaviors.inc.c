// Kept local to this mod: persistent Elegy shells, Twin Link, and the two travel songs.
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include <stdio.h>

typedef struct {
    Actor actor;
    ColliderCylinder collider;
    Vec3s pose[PLAYER_LIMB_MAX+1];
    s16 cooldown;
} CollectActor;
static s16 sShellActorId=-1,sTwinActorId=-1;
static Actor* sShells[4];
static Actor* sTwin;
static Player sTwinTemplate;
static int sTwinOrder; // 0 follow, 1 wait, 2 attack
static int sCollectPrompt; // 1 Soaring, 2 Fugue, 3 orders
static int sSoaringPage;
static bool sLandingPending;
static int sPendingSong=-1,sPendingSongFrames;
static const ALIGN_ASSET(2) char sElegyModels[4][80]={
    "__OTR__objects/gameplay_keep/gElegyShellHumanDL","__OTR__objects/gameplay_keep/gElegyShellDekuDL",
    "__OTR__objects/gameplay_keep/gElegyShellGoronDL","__OTR__objects/gameplay_keep/gElegyShellZoraDL"
};
static ColliderCylinderInit sCollectBody={
    {COLTYPE_NONE,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_ON|OC1_TYPE_ALL,OC2_TYPE_1,COLSHAPE_CYLINDER},
    {ELEMTYPE_UNK0,{DMG_SLASH_MASTER,0,2},{0,0,0},TOUCH_ON|TOUCH_SFX_NORMAL,BUMP_NONE,OCELEM_ON},
    {20,55,0,{0,0,0}}
};
static void CollectActorInit(Actor* actor,PlayState* play) {
    CollectActor* self=(CollectActor*)actor;
    Actor_SetScale(actor,.01f);actor->gravity=-1.0f;
    actor->flags|=ACTOR_FLAG_CAN_PRESS_SWITCHES;
    actor->colChkInfo.mass=MASS_IMMOVABLE;
    Collider_InitCylinder(play,&self->collider);
    Collider_SetCylinder(play,&self->collider,actor,&sCollectBody);
    ActorShape_Init(&actor->shape,0,ActorShadow_DrawCircle,20);
}
static void CollectActorDestroy(Actor* actor,PlayState* play) {
    Collider_DestroyCylinder(play,&((CollectActor*)actor)->collider);
    if(actor==sTwin)sTwin=NULL;
    for(int i=0;i<4;++i)if(sShells[i]==actor)sShells[i]=NULL;
}
static void CollectActorUpdate(Actor* actor,PlayState* play) {
    CollectActor* self=(CollectActor*)actor;
    if(actor->id==sTwinActorId) {
        if(Remains_GetWorn()!=REMAINS_TWINMOLD) {Actor_Kill(actor);return;}
        Player* player=GET_PLAYER(play);
        Vec3f goal=player->actor.world.pos;
        goal.x+=Math_CosS(player->actor.shape.rot.y)*65;
        goal.z-=Math_SinS(player->actor.shape.rot.y)*65;
        Actor* enemy=sTwinOrder==2?RemainsAlly_FindNearestEnemy(play,&actor->world.pos,1200):NULL;
        if(enemy!=NULL)goal=enemy->world.pos;
        f32 distance=Math_Vec3f_DistXZ(&actor->world.pos,&goal);
        actor->speedXZ=sTwinOrder==1?0:distance>40?MIN(distance*.08f,7):0;
        if(actor->speedXZ>0)actor->shape.rot.y=actor->world.rot.y=Math_Vec3f_Yaw(&actor->world.pos,&goal);
        if(self->cooldown>0)--self->cooldown;
        if(enemy!=NULL && distance<70 && self->cooldown==0) {
            CollisionCheck_SetAT(play,&play->colChkCtx,&self->collider.base);self->cooldown=18;
        }
    }
    Actor_MoveXZGravity(actor);
    Actor_UpdateBgCheckInfo(play,actor,20,20,60,5);
    Collider_UpdateCylinder(actor,&self->collider);
    CollisionCheck_SetOC(play,&play->colChkCtx,&self->collider.base);
}
static void ShellDraw(Actor* actor,PlayState* play) {
    int form=actor->params&3;
    if(!sApi->HasResource(sElegyModels[form]))return;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++,(Gfx*)sElegyModels[form]);
    CLOSE_DISPS(play->state.gfxCtx);
}
static void TwinDraw(Actor* actor,PlayState* play) {
    Player* player=GET_PLAYER(play);CollectActor* self=(CollectActor*)actor;
    if(player->skelAnime.skeleton==NULL)return;
    sTwinTemplate=*player;
    memcpy(self->pose,player->skelAnime.jointTable,sizeof(Vec3s)*(player->skelAnime.limbCount+1));
    if(sTwinOrder==2 && self->cooldown>0) {
        AnimationContext_SetLoadFrame(play,(LinkAnimationHeader*)gPlayerAnim_link_fighter_Lside_kiru,
            (18-self->cooldown)%12,player->skelAnime.limbCount,self->pose);
    }
    sTwinTemplate.actor.world.pos=actor->world.pos;sTwinTemplate.actor.shape.rot=actor->shape.rot;
    sTwinTemplate.skelAnime.jointTable=self->pose;sTwinTemplate.heldActor=NULL;sTwinTemplate.meleeWeaponState=0;
    Matrix_Push();
    Matrix_SetTranslateRotateYXZ(actor->world.pos.x,actor->world.pos.y,actor->world.pos.z,&actor->shape.rot);
    Matrix_Scale(player->actor.scale.x,player->actor.scale.y,player->actor.scale.z,MTXMODE_APPLY);
    Player_DrawImpl(play,sTwinTemplate.skelAnime.skeleton,self->pose,sTwinTemplate.skelAnime.dListCount,
        0,sTwinTemplate.currentTunic,sTwinTemplate.currentBoots,sTwinTemplate.actor.shape.face,
        Player_OverrideLimbDrawGameplayDefault,NULL,&sTwinTemplate.actor);
    Matrix_Pop();
}
static void SummonElegy(void) {
    PlayState* play=gPlayState;Player* player=GET_PLAYER(play);int form=0;
    const char* active=sApi->GetActiveForm();
    if(active!=NULL) {if(strstr(active,"deku"))form=1;else if(strstr(active,"goron"))form=2;else if(strstr(active,"zora"))form=3;}
    if(sShells[form]!=NULL)Actor_Kill(sShells[form]);
    sShells[form]=Actor_Spawn(&play->actorCtx,play,sShellActorId,player->actor.world.pos.x,
        player->actor.world.pos.y,player->actor.world.pos.z,0,player->actor.shape.rot.y,0,form);
}
typedef struct {const char* name; s16 entrance;Vec3f pos;u8 room;} OwlDestination;
// Positions read from the vanilla owl actor lists in the user's oot.o2r, not guessed map coordinates.
static const OwlDestination sOwlDestinations[]={
    {"Lake Hylia",ENTR_LAKE_HYLIA_NORTH_EXIT,{-3080,-985,6067},0},
    {"Death Mountain Trail",ENTR_DEATH_MOUNTAIN_TRAIL_BOTTOM_EXIT,{-42,3347,-4409},0},
    {"Hyrule Field - Kokiri",ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN,{4284,182,7877},0},
    {"Zora's River",ENTR_ZORAS_RIVER_WEST_EXIT,{-1397,260,426},0},
    {"Gerudo Valley",ENTR_GERUDO_VALLEY_EAST_EXIT,{14,242,-264},0},
    {"Desert Colossus",ENTR_DESERT_COLOSSUS_EAST_EXIT,{-1456,811,425},0},
    {"Hyrule Castle",ENTR_CASTLE_GROUNDS_SOUTH_EXIT,{-146,1619,2961},0},
    {"Lost Woods",ENTR_LOST_WOODS_SOUTH_EXIT,{955,210,-798},2}
};
static void ShowSoaringPage(void) {
    static char text[256];int first=sSoaringPage*2;
    snprintf(text,sizeof(text),"Song of Soaring\x1C%%g%s&%s&%s%%w",sOwlDestinations[first].name,
        sOwlDestinations[first+1].name,sSoaringPage<3?"More owl spots":"Cancel");
    sCollectPrompt=sApi->ShowTextbox(gPlayState,text,false)?1:0;
}
static void WarpTo(s16 entrance,const OwlDestination* owl) {
    PlayState* play=gPlayState;
    if(owl!=NULL) {
        RespawnData* dest=&gSaveContext.respawn[RESPAWN_MODE_TOP];
        dest->entranceIndex=entrance;dest->roomIndex=owl->room;dest->pos=owl->pos;
        dest->pos.z+=70;dest->yaw=0;dest->playerParams=0xDFF;
        gSaveContext.respawnFlag=3;sLandingPending=true;
    }
    play->nextEntranceIndex=entrance;play->transitionTrigger=TRANS_TRIGGER_START;
    play->transitionType=TRANS_TYPE_FADE_BLACK;gSaveContext.nextTransitionType=TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextCutsceneIndex=0xFFEF;
}
static void ApplyRecognizedSong(int index) {
    if(index==3)SummonElegy();
    else if(index==6) {sSoaringPage=0;ShowSoaringPage();}
    else if(index==7) {
        sCollectPrompt=sApi->ShowTextbox(gPlayState,"Fugue of Home\x1C%gChild spawn&Adult spawn&Cancel%w",false)?2:0;
    }
}
static void QueueRecognizedSong(int index) {
    sPendingSong=index;sPendingSongFrames=8;
    gPlayState->msgCtx.ocarinaMode=OCARINA_MODE_04;
    AudioOcarina_SetInstrument(0);
}
static void CollectKaleidoInput(PlayState* play,Input* input,bool* handled) {
    if(play->pauseCtx.state!=6 || play->pauseCtx.pageIndex!=PAUSE_QUEST ||
        !ModLayout_IsActive(SOH_LAYOUT_COLLECTABLES) || play->pauseCtx.cursorPoint[PAUSE_QUEST]!=3 ||
        !(input->press.button&BTN_A) || !sApi->IsCustomItemOwned("nei.mm_remains_twinmold"))return;
    *handled=true;play->pauseCtx.unk_1E4=10;
    sCollectPrompt=sApi->ShowTextbox(play,"Twin Link orders\x1C%gFollow Link&Hold position&Attack nearby enemies%w",false)?3:0;
    if(!sCollectPrompt)play->pauseCtx.unk_1E4=0;
}
static void CollectBehaviorsTick(void) {
    PlayState* play=gPlayState;if(play==NULL)return;
    if(sPendingSong>=0 && --sPendingSongFrames<=0) {
        int song=sPendingSong;sPendingSong=-1;Message_CloseTextbox(play);ApplyRecognizedSong(song);
    }
    if(sLandingPending && play->transitionTrigger==TRANS_TRIGGER_OFF && play->transitionMode==TRANS_MODE_OFF) {
        Player* player=GET_PLAYER(play);Vec3f probe=player->actor.world.pos;probe.y+=100;
        CollisionPoly* poly=NULL;s32 bg;f32 floor=BgCheck_EntityRaycastFloor5(play,&play->colCtx,&poly,&bg,&player->actor,&probe);
        if(poly!=NULL && floor>BGCHECK_Y_MIN)player->actor.world.pos.y=floor;
        sLandingPending=false;
    }
    if(play->pauseCtx.state==0 && Remains_GetWorn()==REMAINS_TWINMOLD && sTwin==NULL) {
        Player* player=GET_PLAYER(play);Vec3f pos=player->actor.world.pos;pos.x+=60;
        sTwin=Actor_Spawn(&play->actorCtx,play,sTwinActorId,pos.x,pos.y,pos.z,0,player->actor.shape.rot.y,0,0);
    }
    if(!sCollectPrompt)return;
    if(Message_GetState(&play->msgCtx)==TEXT_STATE_NONE) {
        if(sCollectPrompt==3)play->pauseCtx.unk_1E4=0;
        sCollectPrompt=0;return;
    }
    if(Message_GetState(&play->msgCtx)!=TEXT_STATE_CHOICE || !Message_ShouldAdvance(play))return;
    int prompt=sCollectPrompt,choice=play->msgCtx.choiceIndex;sCollectPrompt=0;
    Message_CloseTextbox(play);
    if(prompt==1) {
        if(choice==2) {if(sSoaringPage<3){++sSoaringPage;ShowSoaringPage();}}
        else {const OwlDestination* target=&sOwlDestinations[sSoaringPage*2+choice];WarpTo(target->entrance,target);}
    } else if(prompt==2 && choice<2) {
        s16 spawn=choice?ENTR_TEMPLE_OF_TIME_WARP_PAD:ENTR_LINKS_HOUSE_CHILD_SPAWN;
        if(IS_RANDO)spawn=Entrance_PeekNextIndexOverride(spawn);
        WarpTo(spawn,NULL); // Never change linkAge or linkAgeOnLoad.
    } else if(prompt==3) {sTwinOrder=choice;play->pauseCtx.unk_1E4=0;}
}
static void CollectBehaviorsScene(int16_t scene) {
    sTwin=NULL;memset(sShells,0,sizeof(sShells));sCollectPrompt=0;sPendingSong=-1;
}
static void RegisterCollectBehaviors(void) {
    if(!SOH_MOD_API_HAS(sApi,RegisterActor) || !SOH_MOD_API_HAS(sApi,ShowTextbox) ||
       !SOH_MOD_API_HAS(sApi,GetActiveForm))return;
    SOHActorDefinition a={0};a.structSize=sizeof(a);a.key="nei.elegy.shell";a.description="Elegy switch shell";
    a.category=ACTORCAT_PROP;a.actorFlags=ACTOR_FLAG_UPDATE_CULLING_DISABLED|ACTOR_FLAG_CAN_PRESS_SWITCHES;
    a.objectId=OBJECT_GAMEPLAY_KEEP;a.instanceSize=sizeof(CollectActor);
    a.init=CollectActorInit;a.destroy=CollectActorDestroy;a.update=CollectActorUpdate;a.draw=ShellDraw;
    sShellActorId=sApi->RegisterActor(&a);
    a.key="nei.twin_link";a.description="Twinmold's Twin Link";a.category=ACTORCAT_NPC;a.draw=TwinDraw;
    sTwinActorId=sApi->RegisterActor(&a);
    SOH_REGISTER_HOOK(sApi,OnGameFrameUpdate,CollectBehaviorsTick);
    SOH_REGISTER_HOOK(sApi,OnSceneInit,CollectBehaviorsScene);
    SOH_REGISTER_HOOK(sApi,OnKaleidoInput,CollectKaleidoInput);
}
