// Replace only this frame's quest-background texture commands, after Kaleido has drawn.
// Both games use fifteen IA8 panels of 80 x 32; geometry and other pause pages stay native.
static const ALIGN_ASSET(2) char sMmQuestPanels[15][96]={
    "__OTR__icon_item_jpn_static/gPauseQuestStatus00ENGTex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus01Tex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus02Tex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus03Tex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus04Tex",
    "__OTR__icon_item_jpn_static/gPauseQuestStatus10ENGTex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus11Tex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus12Tex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus13Tex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus14Tex",
    "__OTR__icon_item_jpn_static/gPauseQuestStatus20ENGTex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus21Tex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus22Tex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus23Tex",
    "__OTR__icon_item_static_yar/gPauseQuestStatus24Tex"
};
static int MmQuestPanelIndex(const char* path) {
    if(path==NULL || strncmp(path,"__OTR__textures/icon_item_",sizeof("__OTR__textures/icon_item_")-1)!=0)return -1;
    const char* name=strrchr(path,'/');
    if(name==NULL || strncmp(++name,"gPauseQuestStatus",sizeof("gPauseQuestStatus")-1)!=0)return -1;
    int row=name[17]-'0',column=name[18]-'0';
    if(row<0 || row>2 || column<0 || column>4)return -1;
    const char* suffix=name+19;
    if(strcmp(suffix,"Tex") && strcmp(suffix,"ENGTex") && strcmp(suffix,"GERTex") &&
       strcmp(suffix,"FRATex") && strcmp(suffix,"JPNTex"))return -1;
    return row*5+column;
}
static void DrawMmQuestBackground(PlayState* play) {
    if(play->pauseCtx.state==0 || play->gameOverCtx.state!=GAMEOVER_INACTIVE ||
       !ModLayout_IsActive(SOH_LAYOUT_COLLECTABLES) || !sApi->HasResource(sMmQuestPanels[0]))return;
    Gfx* end=play->state.gfxCtx->polyOpa.p;
    for(Gfx* command=play->state.gfxCtx->polyOpa.bufp;command<end;++command) {
        if(((command->words.w0>>24)&255)!=G_SETTIMG)continue;
        // Segmented addresses aren't host strings. Native CPU texture commands contain host pointers.
        if((uintptr_t)command->words.w1<=UINT32_MAX)continue;
        char* path=(char*)(uintptr_t)command->words.w1;
        if(!ResourceMgr_OTRSigCheck(path))continue;
        int panel=MmQuestPanelIndex(path);
        if(panel>=0)command->words.w1=(uintptr_t)sMmQuestPanels[panel];
    }
}
