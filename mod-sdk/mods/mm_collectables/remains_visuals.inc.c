static const ALIGN_ASSET(2) char sCraterDL[3][80]={
 "__OTR__objects/gameplay_keep/gameplay_keep_DL_06BFE0",
 "__OTR__objects/gameplay_keep/gameplay_keep_DL_06C098",
 "__OTR__objects/gameplay_keep/gameplay_keep_DL_06C1E8"
};
static const ALIGN_ASSET(2) char sCraterTex[]="__OTR__objects/gameplay_keep/gameplay_keep_Tex_06C350";
static void DrawGroundCrater(PlayState* play) {
 if(sCraterTimer<=0 || !sApi->HasResource(sCraterDL[0]))return;
 --sCraterTimer;
 Matrix_Push();Matrix_Translate(sCraterPos.x,sCraterPos.y+.3f,sCraterPos.z,MTXMODE_NEW);
 Matrix_Scale(.025f,.025f,.025f,MTXMODE_APPLY);
 OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Xlu(play->state.gfxCtx);
 Gfx* material=Graph_Alloc(play->state.gfxCtx,3*sizeof(Gfx));
 gDPSetPrimColor(&material[0],0,0x80,80,60,40,MIN(sCraterTimer*8,255));
 gDPSetEnvColor(&material[1],40,30,20,MIN(sCraterTimer*8,255));
 gSPEndDisplayList(&material[2]);
 gSPSegment(POLY_XLU_DISP++,0x08,(uintptr_t)material);
 gSPSegment(POLY_XLU_DISP++,0x09,(uintptr_t)material);
 gSPSegment(POLY_XLU_DISP++,0x0A,(uintptr_t)Gfx_TexScroll(play->state.gfxCtx,0,0,32,32));
 gDPSetPrimColor(POLY_XLU_DISP++,0,0,80,60,40,MIN(sCraterTimer*8,255));
 gDPSetEnvColor(POLY_XLU_DISP++,0,0,0,MIN(sCraterTimer*8,255));
 gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
 for(int i=0;i<3;++i)gSPDisplayList(POLY_XLU_DISP++,(Gfx*)sCraterDL[i]);
 CLOSE_DISPS(play->state.gfxCtx);Matrix_Pop();
}
// Gyorg's funnel is MM's En_Water_Effect primary spray, re-emitted every 14 frames while held, as NEI's 2ship
// branch spawns it. Each burst replays func_80A5A534; while held it is glued to the mask and kept a cone.
#define FUNNEL_BURST_COUNT 8
#define FUNNEL_RESPAWN_FRAMES 14
#define FUNNEL_LAYER_COUNT 3
#define FUNNEL_ACTOR_SCALE (10 * 0.0002f)
#define FUNNEL_MIN_LENGTH 12.0f
#define FUNNEL_MAX_RADIUS 3.07f
#define FUNNEL_UPRIGHT_PITCH 0x4000

typedef struct {
    bool isAlive;
    s16 timer;
    f32 length[FUNNEL_LAYER_COUNT];
    f32 growth[FUNNEL_LAYER_COUNT];
    f32 radius[FUNNEL_LAYER_COUNT];
    f32 alpha[FUNNEL_LAYER_COUNT];
    f32 life;
} FunnelBurst;

typedef struct {
    const char* dl;
    f32 growthDecay;
    f32 radiusStep;
    f32 startAlpha;
    f32 fadeStep;
    s8 scrollStepY;
    const u8 (*flicker)[5];
} FunnelLayer;

// object_water_effect's texture animations 0xDE0, 0xE0C and 0xE40, read from 2ship's mm.o2r.
static const u8 sFunnelFlickerMid[2][5] = { { 255, 255, 255, 200, 128 }, { 255, 255, 255, 255, 128 } };
static const u8 sFunnelFlickerOuter[2][5] = { { 255, 255, 255, 100, 128 }, { 255, 255, 255, 255, 128 } };
static const FunnelLayer sFunnelLayers[FUNNEL_LAYER_COUNT] = {
    { "__OTR__objects/object_water_effect/object_water_effect_DL_000420", -0.017f, 0.025f, 255.0f, 26.0f, 10, NULL },
    { "__OTR__objects/object_water_effect/object_water_effect_DL_000730", -0.018f, 0.075f, 200.0f, 26.0f, 8,
      sFunnelFlickerMid },
    { "__OTR__objects/object_water_effect/object_water_effect_DL_000A48", -0.019f, 0.1f, 255.0f, 24.0f, 10,
      sFunnelFlickerOuter },
};

static FunnelBurst sFunnelBursts[FUNNEL_BURST_COUNT];
static Vec3f sFunnelPos;
static s16 sFunnelYaw;
static s16 sFunnelPitch;

static void SpawnFunnelBurst(void) {
    for (s32 i = 0; i < FUNNEL_BURST_COUNT; i++) {
        FunnelBurst* burst = &sFunnelBursts[i];
        if (burst->isAlive) {
            continue;
        }
        burst->isAlive = true;
        burst->timer = -3;
        burst->life = 191.25f;
        for (s32 layer = 0; layer < FUNNEL_LAYER_COUNT; layer++) {
            burst->length[layer] = 0.0f;
            burst->growth[layer] = 0.2f;
            burst->radius[layer] = 1.0f;
            burst->alpha[layer] = sFunnelLayers[layer].startAlpha;
        }
        return;
    }
}

static void HoldFunnelShape(FunnelBurst* burst) {
    for (s32 layer = 0; layer < FUNNEL_LAYER_COUNT; layer++) {
        burst->length[layer] = MAX(burst->length[layer], FUNNEL_MIN_LENGTH);
        burst->radius[layer] = MIN(burst->radius[layer], FUNNEL_MAX_RADIUS);
    }
}

static void AgeFunnelBurst(FunnelBurst* burst) {
    if (burst->life < 1.0f) {
        burst->isAlive = false;
        return;
    }
    if (++burst->timer < 0) {
        return;
    }
    for (s32 layer = 0; layer < FUNNEL_LAYER_COUNT; layer++) {
        burst->length[layer] += burst->growth[layer];
        burst->growth[layer] += sFunnelLayers[layer].growthDecay;
        if (burst->length[layer] <= 0.0f) {
            burst->length[layer] = 0.0f;
            burst->growth[layer] = 0.0f;
        }
        Math_ApproachF(&burst->radius[layer], 20.0f, 1.0f, sFunnelLayers[layer].radiusStep);
        if (burst->timer >= 24) {
            Math_ApproachZeroF(&burst->alpha[layer], 1.0f, sFunnelLayers[layer].fadeStep);
        }
    }
    if (burst->timer >= 50) {
        Math_ApproachZeroF(&burst->life, 1.0f, 3.75f);
    }
}

// Released bursts keep their last pose and flatten out, as MM's do once nothing re-aims them.
static void UpdateWhirlpoolFunnel(PlayState* play, Player* player) {
    if (sIsWhirlpoolActive) {
        sFunnelPos = sWhirlpoolPos;
        sFunnelYaw = player->actor.shape.rot.y;
        sFunnelPitch = FUNNEL_UPRIGHT_PITCH + player->actor.shape.rot.x;
        if ((play->gameplayFrames % FUNNEL_RESPAWN_FRAMES) == 0) {
            SpawnFunnelBurst();
        }
    }
    for (s32 i = 0; i < FUNNEL_BURST_COUNT; i++) {
        FunnelBurst* burst = &sFunnelBursts[i];
        if (!burst->isAlive) {
            continue;
        }
        if (sIsWhirlpoolActive) {
            HoldFunnelShape(burst);
        }
        AgeFunnelBurst(burst);
    }
}

static Gfx* FunnelFlicker(PlayState* play, const u8 (*flicker)[5]) {
    Gfx* list = Graph_Alloc(play->state.gfxCtx, 2 * sizeof(Gfx));
    const u8* color = flicker[play->gameplayFrames % 2];

    gDPSetPrimColor(&list[0], 0, color[4], color[0], color[1], color[2], color[3]);
    gSPEndDisplayList(&list[1]);
    return list;
}

static Gfx* FunnelScroll(PlayState* play, s8 stepY) {
    return Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 64, 64, 1, 0, -(s32)(play->gameplayFrames * stepY), 64, 64);
}

static void DrawFunnelLayer(PlayState* play, const FunnelBurst* burst, s32 layer) {
    const FunnelLayer* source = &sFunnelLayers[layer];

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    if (source->flicker != NULL) {
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)FunnelFlicker(play, source->flicker));
        gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)FunnelScroll(play, source->scrollStepY));
    } else {
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)FunnelScroll(play, source->scrollStepY));
    }
    Matrix_Push();
    Matrix_Scale(burst->radius[layer], burst->length[layer], burst->radius[layer], MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (u8)burst->alpha[layer]);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)source->dl);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGyorgWhirlpool(PlayState* play) {
    if (!sApi->HasResource(sFunnelLayers[0].dl)) {
        return;
    }
    for (s32 i = 0; i < FUNNEL_BURST_COUNT; i++) {
        const FunnelBurst* burst = &sFunnelBursts[i];
        if (!burst->isAlive) {
            continue;
        }
        Matrix_Push();
        Matrix_Translate(sFunnelPos.x, sFunnelPos.y, sFunnelPos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(sFunnelYaw), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(sFunnelPitch), MTXMODE_APPLY);
        Matrix_Scale(FUNNEL_ACTOR_SCALE, FUNNEL_ACTOR_SCALE, FUNNEL_ACTOR_SCALE, MTXMODE_APPLY);
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetEnvColor(POLY_XLU_DISP++, 165, 235, 255, 128);
        CLOSE_DISPS(play->state.gfxCtx);
        for (s32 layer = 0; layer < FUNNEL_LAYER_COUNT; layer++) {
            if (burst->alpha[layer] > 1.0f) {
                DrawFunnelLayer(play, burst, layer);
            }
        }
        Matrix_Pop();
    }
}

static void ForgetWhirlpoolFunnel(void) {
    memset(sFunnelBursts, 0, sizeof(sFunnelBursts));
}
