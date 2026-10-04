#include "overlays/actors/ovl_Arms_Hook/z_arms_hook.h"
#include "mods/items/helpers/target_select_helper.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Player player;
static PlayState play;
static ArmsHook hook;
static Actor target;
static u8 variant = 4;
static ColliderCylinder playerCollider, targetCollider;
Vec3f gSfxDefaultPos;
f32 gSfxDefaultFreqAndVolScale;
s8 gSfxDefaultReverb;
void ArmsHook_Wait(ArmsHook*, PlayState*);
void ArmsHook_SwitchSwap(ArmsHook*, PlayState*);
void ArmsHook_Shoot(ArmsHook* h, PlayState* p) {
}
u8 Nei_ArmsHookVariant(Player* p) {
    return variant;
}
float CVarGetFloat(const char* name, float fallback) {
    return fallback;
}
void Audio_PlaySoundGeneral(u16 id, Vec3f* pos, u8 token, f32* freq, f32* volume, s8* reverb) {
}
void Audio_StopSfxByPos(Vec3f* pos) {
}
void Audio_StopSfxById(u32 id) {
}
void Actor_SetColorFilter(Actor* a, s16 flag, s16 intensity, s16 xlu, s16 duration) {
}
void Math_Vec3f_Copy(Vec3f* dst, Vec3f* src) {
    *dst = *src;
}
f32 Math_SinS(s16 angle) {
    return sinf(angle * (3.14159265358979323846f / 32768.0f));
}
f32 Math_CosS(s16 angle) {
    return cosf(angle * (3.14159265358979323846f / 32768.0f));
}
s16 Math_Atan2S(f32 x, f32 y) {
    return (s16)(atan2f(y, x) * (32768.0f / 3.14159265358979323846f));
}
void Actor_SetProjectileSpeed(Actor* a, f32 speed) {
    a->speedXZ = Math_CosS(a->world.rot.x) * speed;
    a->velocity.y = -Math_SinS(a->world.rot.x) * speed;
}
static void alive(Actor* a, PlayState* p) {
}
