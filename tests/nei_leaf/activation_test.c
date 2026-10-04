// Production ground-start/upper-action logic with fixture animation/resource,
// input, collision, audio and particle boundaries. This is not a GPU/runtime test.
#include "global.h"
#include "mods/items/logic/item_dekuleaf.c"
#include "mods/items/anim/deku_leaf/dekuleaf_anim_data.c"
#include "mods/sound_translator/mm_audio_sfx.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
CustomItemState gCustomItemState;
static PlayState play;
static Player player;
static LinkAnimationHeader clip = {{39}, gDekuLeafBlowAnimData};
static ItemInputState input;
static int magic, spent, missing, blocked, colliders, equips, unequips;
static unsigned char frameData[134];
static int frames;
s32 ItemMagic_HasEnough(PlayState* p, s16 n) { return magic >= n; }
void ItemMagic_Consume(PlayState* p, s16 n) { magic -= n; spent += n; }
u8 ResourceMgr_FileExists(const char* p) { assert(!strcmp(p, NEI_ANIM_DEKULEAF_BLOW)); return !missing; }
LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* p) { return &clip; }
void ItemInput_Update(ItemInputState* out, u8 id, Player* p, PlayState* c) { *out = input; }
u8 ItemInput_IsBlocked(Player* p, PlayState* c) { return blocked; }
u8 ItemInput_CheckDamage(Player* p, s8* prev) { return 0; }
u8 TransformMasks_IsTransformed(void) { return 0; }
MmPlayerTransformation MmPlayer_GetForm(void) { return MM_PLAYER_FORM_DEKU; }
s32 Movement_IsOnGround(Player* p) { return 1; }
u8 RocBoots_IsWorn(void) { return 0; }
void ItemEquip_PlayEquipSFXForAction(PlayState* p, Player* c, s32 a) { ++equips; }
void ItemEquip_PlayUnequipSFXForAction(PlayState* p, Player* c, s32 a) { ++unequips; }
s32 Collider_InitCylinder(PlayState* p, ColliderCylinder* c) { memset(c, 0, sizeof(*c)); return 0; }
s32 Collider_SetCylinder(PlayState* p, ColliderCylinder* c, Actor* a, ColliderCylinderInit* src) { c->base.actor = a; return 0; }
s32 CollisionCheck_SetAT(PlayState* p, CollisionCheckContext* ctx, Collider* c) { assert(c->actor == &player.actor); ++colliders; return 0; }
void Audio_StopSfxById(u32 id) {}
// Keep the real bank stop: ground Leaf can reach it before any MM playback.
// The outer audio mutex and output mixer remain fixture boundaries.
void MmSfx_Stop(u16 id) { AudioMmSfx_StopById(id); }
s32 MmSfx_PlayAtPos(u16 id, Vec3f* pos) { return 0; }
void Player_PlaySfx(Actor* p, u16 id) {}
void FX_SpawnWindBlow(PlayState* p, Vec3f* v, s16 yaw, f32 range) {}
void func_8002836C(PlayState* p, Vec3f* v, Vec3f* vel, Vec3f* accel, Color_RGBA8* prim, Color_RGBA8* env, s16 scale, s16 step, s16 life) {}
f32 Rand_ZeroFloat(f32 f) { return 0; }
f32 Rand_CenteredFloat(f32 f) { return 0; }
f32 Math_SinS(s16 a) { return sinf(a * (3.14159265358979323846f / 32768)); }
f32 Math_CosS(s16 a) { return cosf(a * (3.14159265358979323846f / 32768)); }
s16 Math_Atan2S(f32 x, f32 z) { return 0; }
s32 Math_StepToF(f32* a, f32 b, f32 c) { *a=b; return 1; }
s16 Animation_GetLastFrame(void* a) { return ((LinkAnimationHeader*)a)->common.frameCount-1; }
static void loadFrame(SkelAnime* s) {
    int frame=(int)s->curFrame;
    assert(frame>=0 && frame<39);
    memcpy(frameData, (char*)((LinkAnimationHeader*)s->animation)->segment + frame*134, 134);
    ++frames;
}
void LinkAnimation_Change(PlayState* p, SkelAnime* s, LinkAnimationHeader* a, f32 speed, f32 start, f32 end, u8 mode, f32 morph) {
    s->animation=a; s->curFrame=start; s->endFrame=end; s->playSpeed=speed; s->animLength=a->common.frameCount; loadFrame(s);
}
void LinkAnimation_PlayOnce(PlayState* p, SkelAnime* s, LinkAnimationHeader* a) {
    LinkAnimation_Change(p,s,a,1,0,Animation_GetLastFrame(a),ANIMMODE_ONCE,0);
}
void LinkAnimation_AnimateFrame(PlayState* p, SkelAnime* s) { loadFrame(s); }
#undef R_UPDATE_RATE
#define R_UPDATE_RATE 3
#include "leaf_native_once.inc"
s32 LinkAnimation_Update(PlayState* p, SkelAnime* s) { return LinkAnimation_Once(p,s); }
static void reset(void) {
    memset(&play,0,sizeof(play)); memset(&player,0,sizeof(player));
    memset(&gCustomItemState,0,sizeof(gCustomItemState));
    sDekuLeafColInitialized=0; sDekuLeafBlowEffectFired=0;
    input=(ItemInputState){.wasEquipped=1,.isPressed=1,.isHeld=1};
    magic=30; spent=missing=blocked=colliders=equips=unequips=frames=0;
}
int main(void) {
    puts("Starting first ground Leaf with the MM bank engine still uninitialized");
    fflush(stdout);
    reset(); Handle_DekuLeaf(&player,&play);
    assert(dlBlowing && dlActive && equips==1 && frames==1 && spent==0);
    input.isPressed=0;
    for(int i=0; i<30 && dlActive; ++i) {
        ++play.gameplayFrames;
        Handle_DekuLeaf(&player,&play);
        Player_UpperAction_DekuLeaf(&player,&play);
    }
    assert(!dlActive && !dlBlowing && !(player.stateFlags1 & PLAYER_STATE1_INPUT_DISABLED));
    assert(spent==3 && colliders==6 && unequips==1);
    input.isPressed=1; Handle_DekuLeaf(&player,&play); assert(dlBlowing);
    blocked=1; Handle_DekuLeaf(&player,&play); assert(!dlActive);
    reset(); missing=1; Handle_DekuLeaf(&player,&play); assert(!dlActive && !dlBlowing && !equips && !spent);
    reset(); magic=0; Handle_DekuLeaf(&player,&play); assert(!dlActive && !frames);
    puts("PASS: ground start, full gust, completion, repeat, interruption, missing clip, insufficient magic");
}
