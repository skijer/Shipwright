/**
 * wand_shadow.c — Shadow Scepter (Skijer's NEI).
 *
 * A black Boe, thrown at the nearest enemy, which it stuns and then disperses into.
 *
 * The Boe is Majora's Mask's En_Mkk, and it is unusually cheap to borrow: it has no skeleton and no
 * animations at all, just billboarded display lists. So there is no actor to register, no ActorDB
 * entry and no skeleton loader — the bolt is wand state that draws three MM display lists by path.
 */

#include "align_asset_macro.h"

#define dgBlackBoeBodyMaterialDL "__OTR__objects/object_mkk/gBlackBoeBodyMaterialDL"
static const ALIGN_ASSET(2) char gBlackBoeBodyMaterialDL[] = dgBlackBoeBodyMaterialDL;
#define dgBlackBoeBodyModelDL "__OTR__objects/object_mkk/gBlackBoeBodyModelDL"
static const ALIGN_ASSET(2) char gBlackBoeBodyModelDL[] = dgBlackBoeBodyModelDL;
#define dgBlackBoeEndDL "__OTR__objects/object_mkk/gBlackBoeEndDL"
static const ALIGN_ASSET(2) char gBlackBoeEndDL[] = dgBlackBoeEndDL;

#define SHADOW_SEEK_RANGE 460.0f
#define SHADOW_SPAWN_DIST 30.0f
#define SHADOW_SPAWN_HEIGHT 25.0f
#define SHADOW_SPEED 11.0f
#define SHADOW_HIT_RADIUS 22.0f
#define SHADOW_LIFE_FRAMES 90
#define SHADOW_STUN_FRAMES 120
#define SHADOW_SCALE 0.014f

// Actor_SetColorFilter's blue/stun tint. SoH's headers carry no COLORFILTER_* names, so the flags
// are spelled out the way the trutefel enemies do: 0x0000 blue, 0x4000 red, 0x8000 grey.
#define SHADOW_FILTER_BLUE 0x0000
#define SHADOW_FILTER_STRENGTH 0xF8

static const u8 sShadowEnemyCats[2] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };

static struct {
    Vec3f pos;
    Actor* target;
    s16 yaw;
    s16 life;
    u8 active;
} sShadowBolt;

extern u8 ResourceMgr_FileExists(const char* resName);

// mm.o2r may not be mounted. The bolt still flies and still stuns — it just has nothing to draw.
static u8 WandShadow_HasModel(void) {
    static u8 sChecked = 0;
    static u8 sPresent = 0;

    if (!sChecked) {
        sChecked = 1;
        sPresent = ResourceMgr_FileExists(gBlackBoeBodyModelDL);
    }
    return sPresent;
}

// A frozen enemy is the Deku Nut effect: the freeze stops its update, the colour filter is what
// makes it read as stunned. Same recipe as the Divine Shield's parry.
static void WandShadow_Stun(Actor* enemy) {
    enemy->freezeTimer = SHADOW_STUN_FRAMES;
    Actor_SetColorFilter(enemy, SHADOW_FILTER_BLUE, SHADOW_FILTER_STRENGTH, 0, SHADOW_STUN_FRAMES);
}

void WandShadow_Forget(void) {
    sShadowBolt.active = 0;
    sShadowBolt.target = NULL;
}

// Re-acquired every frame rather than trusted: the enemy the bolt left Link chasing can die, or be
// killed by something else, before the bolt arrives.
void WandShadow_Tick(PlayState* play) {
    Actor* hit;

    if (!sShadowBolt.active) {
        return;
    }
    if (--sShadowBolt.life <= 0) {
        WandShadow_Forget();
        return;
    }

    if ((sShadowBolt.target != NULL) && (sShadowBolt.target->update != NULL)) {
        sShadowBolt.yaw = Math_Vec3f_Yaw(&sShadowBolt.pos, &sShadowBolt.target->world.pos);
    }
    sShadowBolt.pos.x += Math_SinS(sShadowBolt.yaw) * SHADOW_SPEED;
    sShadowBolt.pos.z += Math_CosS(sShadowBolt.yaw) * SHADOW_SPEED;

    hit = TargetSelect_FindNearest(play, sShadowEnemyCats, ARRAY_COUNT(sShadowEnemyCats), NULL, &sShadowBolt.pos,
                                   SHADOW_HIT_RADIUS);
    if (hit != NULL) {
        WandShadow_Stun(hit);
        Audio_PlaySoundGeneral(NA_SE_EN_GANON_DARKWAVE, &sShadowBolt.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        WandShadow_Forget();
    }
}

void WandShadow_Draw(PlayState* play) {
    if (!sShadowBolt.active || !WandShadow_HasModel()) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(sShadowBolt.pos.x, sShadowBolt.pos.y, sShadowBolt.pos.z, MTXMODE_NEW);
    // The Boe is a flat billboard in MM too — its own draw replaces the rotation the same way.
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(SHADOW_SCALE, SHADOW_SCALE, SHADOW_SCALE, MTXMODE_APPLY);

    // The MM material lists reach for segment 8; pointing it at an empty list is what keeps them
    // from running whatever the scene happened to leave there.
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)gEmptyDL);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBlackBoeBodyMaterialDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBlackBoeBodyModelDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBlackBoeEndDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

u8 WandShadow_Cast(Player* player, PlayState* play) {
    s16 yaw = player->actor.shape.rot.y;

    if (sShadowBolt.active) {
        return 0; // one bolt at a time
    }

    sShadowBolt.pos.x = player->actor.world.pos.x + (Math_SinS(yaw) * SHADOW_SPAWN_DIST);
    sShadowBolt.pos.y = player->actor.world.pos.y + SHADOW_SPAWN_HEIGHT;
    sShadowBolt.pos.z = player->actor.world.pos.z + (Math_CosS(yaw) * SHADOW_SPAWN_DIST);

    // No enemy in reach is not a failed cast: the bolt flies off Link's nose and fades on its timer.
    sShadowBolt.target = TargetSelect_FindNearest(play, sShadowEnemyCats, ARRAY_COUNT(sShadowEnemyCats), NULL,
                                                  &player->actor.world.pos, SHADOW_SEEK_RANGE);
    sShadowBolt.yaw = yaw;
    sShadowBolt.life = SHADOW_LIFE_FRAMES;
    sShadowBolt.active = 1;

    // NOT NA_SE_EN_GANON_DARKWAVE_M: vanilla only ever plays that one as `- SFX_FLAG`, the
    // continuous variant re-issued every frame, so started raw it would never stop.
    Audio_PlaySoundGeneral(NA_SE_IT_SHIELD_REFLECT_MG, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return 1;
}
