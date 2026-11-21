#include "BossSuperDamage.h"

#include "functions.h"
#include "macros.h"
#include "soh/ModApi/ModApi.h"

#define SPARK_SLOTS 8
#define SPARK_FADE_FRAMES 20
#define SPARK_MAX_ANCHORS 32
#define ORB_BASE_SCALE 6.0f
#define ORB_PULSE 0.12f
#define ORB_MATERIAL_DL "__OTR__overlays/ovl_Boss_Ganon2/gGanonLightOrbMaterialDL"
#define ORB_MODEL_DL "__OTR__overlays/ovl_Boss_Ganon2/gGanonLightOrbModelDL"

typedef struct {
    Actor* boss;
    s16 timer;
} SparkSlot;

static SparkSlot sSparkSlots[SPARK_SLOTS];
static const SOHBossSuperAttack* sProvider;

static const SOHBossSuperAttack* FindProvider(void) {
    if (sProvider == NULL) {
        sProvider = (const SOHBossSuperAttack*)ModApi_Get()->FindService(SOH_BOSS_SUPER_ATTACK_SERVICE);
    }
    return sProvider;
}

u8 BossSuperDamage_IsActive(PlayState* play) {
    const SOHBossSuperAttack* provider = FindProvider();
    return provider != NULL && play != NULL && provider->isStriking(play);
}

u8 BossSuperDamage_IsFormActive(PlayState* play) {
    const SOHBossSuperAttack* provider = FindProvider();
    return provider != NULL && play != NULL && provider->isFormActive(play);
}

u8 BossSuperDamage_FormAttackReaches(PlayState* play, Vec3f* targetPos, f32 range) {
    const SOHBossSuperAttack* provider = FindProvider();
    return provider != NULL && play != NULL && targetPos != NULL && provider->reaches(play, targetPos, range);
}

f32 BossSuperDamage_FormAttackRange(PlayState* play) {
    const SOHBossSuperAttack* provider = FindProvider();
    return (provider != NULL && play != NULL) ? provider->getReach(play) : 0.0f;
}

u8 BossSuperDamage_FormDamage(PlayState* play) {
    const SOHBossSuperAttack* provider = FindProvider();
    return (provider != NULL && play != NULL) ? provider->getDamage(play) : 0;
}

static SparkSlot* FindSparkSlot(Actor* boss) {
    for (s32 i = 0; i < SPARK_SLOTS; i++) {
        if (sSparkSlots[i].boss == boss) {
            return &sSparkSlots[i];
        }
    }
    return NULL;
}

// Restarting an active burst refreshes its timer instead of taking a second slot.
void BossSuperDamage_StartElectricSparks(Actor* boss, s16 durationFrames) {
    if (boss == NULL || durationFrames <= 0) {
        return;
    }
    Audio_PlayActorSound2(boss, NA_SE_EN_GANON_BODY_SPARK);
    SparkSlot* slot = FindSparkSlot(boss);
    if (slot == NULL) {
        slot = FindSparkSlot(NULL);
    }
    if (slot == NULL) {
        return;
    }
    slot->boss = boss;
    slot->timer = durationFrames;
}

// Dark Beast Ganon's transformation glow, one light orb billboarded over each anchor.
void BossSuperDamage_DrawElectricSparks(Actor* boss, PlayState* play, Vec3f* limbsPos, s32 limbCount, f32 scale) {
    if (boss == NULL || play == NULL || limbsPos == NULL || limbCount <= 0) {
        return;
    }
    SparkSlot* slot = FindSparkSlot(boss);
    if (slot == NULL) {
        return;
    }
    if (slot->timer <= 0) {
        slot->boss = NULL;
        return;
    }
    s32 alpha = (slot->timer >= SPARK_FADE_FRAMES) ? 255 : (slot->timer * 255 / SPARK_FADE_FRAMES);
    slot->timer--;
    f32 orbScale = ORB_BASE_SCALE * scale * (1.0f + ORB_PULSE * Math_SinS(play->gameplayFrames * 0x0C00));

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (u8)alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 100, 200, 255, 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)ORB_MATERIAL_DL);
    for (s32 i = 0; i < limbCount; i++) {
        Matrix_Translate(limbsPos[i].x, limbsPos[i].y, limbsPos[i].z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(orbScale, orbScale, orbScale, MTXMODE_APPLY);
        Matrix_RotateZ(i * 0.7f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)ORB_MODEL_DL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void BossSuperDamage_DrawGlowFromSpheres(Actor* boss, PlayState* play, ColliderJntSph* collider, s32 sphereCount,
                                         f32 scale) {
    Vec3f anchors[SPARK_MAX_ANCHORS];

    if (collider == NULL || sphereCount <= 0) {
        return;
    }
    sphereCount = MIN(sphereCount, SPARK_MAX_ANCHORS);
    for (s32 i = 0; i < sphereCount; i++) {
        anchors[i].x = collider->elements[i].dim.worldSphere.center.x;
        anchors[i].y = collider->elements[i].dim.worldSphere.center.y;
        anchors[i].z = collider->elements[i].dim.worldSphere.center.z;
    }
    BossSuperDamage_DrawElectricSparks(boss, play, anchors, sphereCount, scale);
}
