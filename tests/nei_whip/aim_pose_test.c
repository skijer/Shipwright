// Actual whip release and upper action; engine animation/audio/camera boundaries are fixtures.
#include "global.h"
#include "mods/items/helpers/camera_helper.h"
extern s32 Player_IsZTargeting(Player* p);
#include "mods/items/logic/item_whip.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>
CustomItemState gCustomItemState;
f32 gSfxDefaultFreqAndVolScale = 1.0f;
s8 gSfxDefaultReverb;
static PlayState play;
static Player player;
static Vec3s upper[PLAYER_LIMB_MAX], mainJoints[PLAYER_LIMB_MAX];
static int copies, loops, oneShots;
static const char* selected;
static int ztarget;
void LinkAnimation_PlayLoop(PlayState* ctx, SkelAnime* s, LinkAnimationHeader* animation) {
    selected = (const char*)animation;
    ++loops;
    s->animation = animation;
    // Distinctive loaded pose samples, making same-frame upper-body copy observable.
    s->jointTable[PLAYER_LIMB_R_SHOULDER] = (Vec3s){100, 200, 300};
    s->jointTable[PLAYER_LIMB_R_HAND] = (Vec3s){400, 500, 600};
}
void LinkAnimation_PlayOnce(PlayState* ctx, SkelAnime* s, LinkAnimationHeader* animation) {
    selected = (const char*)animation;
    ++oneShots;
}
s32 LinkAnimation_Update(PlayState* ctx, SkelAnime* s) { return 1; }
void ExtPlayer_CopyUpperBody(PlayState* ctx, Player* p) {
    ++copies;
    // The real helper queues the engine's existing upper-body mask.
    for (int i = PLAYER_LIMB_UPPER; i < PLAYER_LIMB_MAX; ++i) p->skelAnime.jointTable[i] = p->upperSkelAnime.jointTable[i];
}
s32 Player_IsZTargeting(Player* p) { return ztarget; }
void FirstPerson_Init(Player* p, PlayState* ctx) { p->stateFlags1 |= PLAYER_STATE1_FIRST_PERSON; }
void FirstPerson_Update(Player* p, PlayState* ctx) {}
void FirstPerson_Exit(Player* p, PlayState* ctx) { p->stateFlags1 &= ~PLAYER_STATE1_FIRST_PERSON; }
s16 FirstPerson_GetAimYaw(Player* p) { return p->actor.focus.rot.y; }
s16 FirstPerson_GetAimPitch(Player* p) { return p->actor.focus.rot.x; }
s16 Math_Atan2S(f32 x, f32 y) { return 0; }
void Audio_PlaySoundGeneral(u16 id, Vec3f* pos, u8 token, f32* frequency, f32* volume, s8* reverb) {}
static void reset(void) {
    memset(&play, 0, sizeof(play)); memset(&player, 0, sizeof(player));
    memset(&gCustomItemState, 0, sizeof(gCustomItemState));
    memset(upper, 0, sizeof(upper)); memset(mainJoints, 0, sizeof(mainJoints));
    player.upperSkelAnime.jointTable = upper; player.skelAnime.jointTable = mainJoints;
    whipActive = 1; whipState = WHIP_STATE_EQUIP; whipFirstPerson = 1;
    player.stateFlags1 = PLAYER_STATE1_FIRST_PERSON;
    player.actor.focus.rot = (Vec3s){-0x1800, 0x3000, 0};
    player.bodyPartsPos[PLAYER_BODYPART_R_HAND] = (Vec3f){4, 20, 7};
    sWhipAnimState = WHIP_STATE_EQUIP; selected = NULL;
    copies = loops = oneShots = ztarget = 0;
}
int main(void) {
    reset();
    ItemInputState input = {.isPressed=1};
    WhipStateEquip(&player, &play, &input);
    assert(whipState == WHIP_STATE_EXTENDING && !whipFirstPerson);
    assert(!(player.stateFlags1 & PLAYER_STATE1_FIRST_PERSON));
    assert(whipExtendYaw == 0x3000 && whipExtendPitch == -0x1800);
    assert(whipTipPos.x == 4 && whipTipPos.y == 20 && whipTipPos.z == 7 && whipTimer == WHIP_TIMER_MAX);
    assert(selected && !strcmp(selected, gPlayerAnim_link_hook_wait));
    assert(copies == 1 && mainJoints[PLAYER_LIMB_R_SHOULDER].x == 100);
    assert(player.upperLimbRot.x == -0x1800 && player.upperLimbRot.y == 0 && player.upperLimbRot.z == 0);
    for (int state=WHIP_STATE_EXTENDING; state<=WHIP_STATE_RETRACTING; ++state) {
        if (state==WHIP_STATE_ATTACHED || state==WHIP_STATE_SWINGING) continue;
        whipState=state;
        for (int frame=0; frame<50; ++frame) {
            assert(Player_UpperAction_Whip(&player, &play)==1);
            assert(!strcmp(selected, gPlayerAnim_link_hook_wait));
            assert(player.upperLimbRot.x==-0x1800 && oneShots==0);
        }
    }
    // Inactive / equip / launched states do not keep imposing the lash aim.
    whipActive=0; player.upperLimbRot.x=123; assert(Player_UpperAction_Whip(&player,&play)==0);
    assert(player.upperLimbRot.x==123);
    whipActive=1; whipState=WHIP_STATE_EQUIP; Player_UpperAction_Whip(&player,&play);
    assert(player.upperLimbRot.x==123 && !strcmp(selected,gPlayerAnim_link_boom_throw_waitR));
    whipState=WHIP_STATE_LAUNCHED; assert(Player_UpperAction_Whip(&player,&play)==0);
    assert(player.upperLimbRot.x==123);
    puts("PASS: same-frame aimed release, raised lash hold, gameplay release state, upper-pose ownership");
}
