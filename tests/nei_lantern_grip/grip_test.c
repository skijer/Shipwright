// Execute the real final player hand-override order with resource/PAK boundaries.
#include "global.h"
#include "mods/items/custom_items.h"
#include "mods/pak_loader/pak_loader.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
CustomItemState gCustomItemState;
SaveContext gSaveContext;
static PlayState play;
static Player player;
static Gfx opened[1], nativeHand[1], pakOpen[1], pakFist[1], altFist[1];
Gfx* gPlayerLeftHandClosedDLs[] = {(Gfx*)gLinkAdultLeftHandClosedNearDL, (Gfx*)gLinkChildLeftFistNearDL,
                                        (Gfx*)gLinkAdultLeftHandClosedFarDL, (Gfx*)gLinkChildLeftFistFarDL};
static Gfx* sPlayerRightHandClosedDLs[] = {nativeHand,nativeHand,nativeHand,nativeHand};
static int sDListsLodOffset, sLeftHandType, sRightHandType, pak, alt, requested;
#define VB_PLAYER_OVERRIDE_LIMB_DRAW 0
static s32 GameInteractor_Should(int flag,int value,...) { return value; }
static s32 GameInteractor_InvisibleLinkActive(void) { return 0; }
u8 PakLoader_HasActiveModel(void) { return pak; }
Gfx* PakLoader_GetEquipDL(Player* p,s32 limb) { return pakOpen; }
Gfx* PakLoader_GetDLOverride(const char* path) {
    assert(!strcmp(path,gLinkAdultLeftHandClosedNearDL) || !strcmp(path,gLinkAdultLeftHandClosedFarDL));
    return pak ? pakFist : NULL;
}
static Gfx* Player_ResolveLimbDLForDummyOrLocal(void* path) {
    assert(path==gPlayerLeftHandClosedDLs[gSaveContext.linkAge+sDListsLodOffset]);
    ++requested; return alt ? altFist : nativeHand;
}
u8 TransformMasks_IsTransformedAny(void) { return 0; }
static s32 NeiArticulated_UsesSwitchHook(Player* p) { return 0; }
static void NeiArticulated_ApplySwitchHookHand(PlayState* c, Player* p, Gfx** dl, Gfx* hand) {}
static void Player_ApplyTimePedestalSword(PlayState* c, Player* p, s32 limb, Gfx** dl, Vec3s* rot) {}
static void Player_ApplyBackEquipmentVisibility(s32 limb,Gfx** dl) {}
#include "grip_tail.inc"
static void reset(void) {
    memset(&player,0,sizeof(player)); memset(&play,0,sizeof(play));
    memset(&gCustomItemState,0,sizeof(gCustomItemState)); memset(&gSaveContext,0,sizeof(gSaveContext));
    player.actor.scale=(Vec3f){.01f,.01f,.01f}; player.heldItemAction=PLAYER_IA_LANTERN;
    gSaveContext.equips.buttonItems[1]=ITEM_LANTERN; gCustomItemState.lanternEquipped=1;
    pak=alt=requested=sDListsLodOffset=0;
}
int main(void) {
    for(int age=0;age<=1;++age) for(int lod=0;lod<=2;lod+=2) for(int usePak=0;usePak<=1;++usePak) for(int useAlt=0;useAlt<=1;++useAlt) {
        reset(); gSaveContext.linkAge=age; sDListsLodOffset=lod; pak=usePak; alt=useAlt;
        Player before=player; Gfx* dl=opened;
        Fixture_Apply(&play,&player,PLAYER_LIMB_L_HAND,&dl);
        assert(dl==(pak ? pakFist : alt ? altFist : nativeHand));
        assert(sLeftHandType==PLAYER_MODELTYPE_LH_CLOSED);
        assert(!memcmp(&before,&player,sizeof(player)));
    }
    reset(); Gfx* dl=opened; gCustomItemState.lanternEquipped=0;
    Fixture_Apply(&play,&player,PLAYER_LIMB_L_HAND,&dl); assert(dl==opened);
    reset(); dl=opened; gSaveContext.equips.buttonItems[1]=ITEM_NONE;
    Fixture_Apply(&play,&player,PLAYER_LIMB_L_HAND,&dl); assert(dl==opened);
    reset(); dl=opened; player.heldItemAction=PLAYER_IA_SWORD_MASTER;
    Fixture_Apply(&play,&player,PLAYER_LIMB_L_HAND,&dl); assert(dl==opened);
    reset(); dl=opened; Fixture_Apply(&play,&player,PLAYER_LIMB_R_HAND,&dl); assert(dl==opened);
    reset(); dl=opened; gCustomItemState.lanternEquipped=0; gCustomItemState.lanternSwinging=1;
    Fixture_Apply(&play,&player,PLAYER_LIMB_L_HAND,&dl); assert(dl==nativeHand);
    puts("PASS: lantern native/Alt/PAK closed left grip, both ages/LODs, state isolation, unchanged player pose");
}
