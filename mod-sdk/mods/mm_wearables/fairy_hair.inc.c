// Great Fairy hair physics, adapted from MM and the NEI port.
typedef struct {
    Vec3f pos;   // 0x00 - world position
    Vec3f vel;   // 0x0C - velocity
    s16 yaw;     // 0x18
    s16 pitch;   // 0x1A
} FairyHairLink; // size = 0x1C

static FairyHairLink sFairyHairStrands[3][3]; // 3 strands × 3 links
static s32 sFairyHairInited = 0;
static s32 sFairyHairActivated = 0;             // 1 = in Great Fairy Fountain (strands float up + particles)
static u32 sHairLastPhysicsFrame = 0xFFFFFFFFu; // Guard against double-draw per frame

// Strand root positions in head model space (from MM D_801C0C0C)
static Vec3f sHairRootPos[] = {
    { 174.0f, -1269.0f, -1.0f },
    { 401.0f, -729.0f, -701.0f },
    { 401.0f, -729.0f, 699.0f },
};

// Strand gravity targets (from MM D_801C0C30)
static Vec3f sHairTargetPos[] = {
    { 74.0f, -1269.0f, -1.0f },
    { 301.0f, -729.0f, -701.0f },
    { 301.0f, -729.0f, 699.0f },
};

// Chain constraint params (from MM D_801C0C54)
typedef struct {
    f32 length;    // 0x00
    s16 rotY;      // 0x04
    s16 rotX;      // 0x06
    Vec3f target;  // 0x08
    f32 maxLength; // 0x14
    s16 maxYaw;    // 0x18
    s16 maxPitch;  // 0x1A
} HairChainParam;  // size = 0x1C

static HairChainParam sHairChainParams[] = {
    { 0.0f, 0x0000, (s16)0x8000, { 0.0f, 0.0f, 0.0f }, 0.0f, 0x0000, 0x0000 },
    { 16.8f, 0x0000, 0x0000, { 0.0f, 0.0f, 0.0f }, 20.0f, 0x1388, 0x1388 },
    { 30.0f, 0x0000, 0x0000, { 0.0f, 0.0f, 0.0f }, 20.0f, 0x1F40, 0x2EE0 },
};

// D_801C0C00: offset for chain constraint target computation
static Vec3f sHairTargetOffset = { 0.0f, 20.0f, 0.0f };

// Initialize all strand links to a position (from MM func_80127B64)
static void FairyHair_Init(Vec3f* headPos) {
    for (s32 s = 0; s < 3; s++) {
        for (s32 i = 0; i < 3; i++) {
            Math_Vec3f_Copy(&sFairyHairStrands[s][i].pos, headPos);
            sFairyHairStrands[s][i].vel.x = 0.0f;
            sFairyHairStrands[s][i].vel.y = 0.0f;
            sFairyHairStrands[s][i].vel.z = 0.0f;
            sFairyHairStrands[s][i].yaw = 0;
            sFairyHairStrands[s][i].pitch = 0;
        }
    }
    sFairyHairInited = 1;
}

// Spawn sparkle particles when activated (from MM Player_DrawStrayFairyParticles)
static void FairyHair_SpawnParticles(PlayState* play, Vec3f* pos) {
    Vec3f sparkVel = { 0.0f, 0.3f, 0.0f };
    Vec3f sparkAccel = { 0.0f, -0.025f, 0.0f };
    Color_RGBA8 primColor = { 250, 100, 100, 0 };
    Color_RGBA8 envColor = { 0, 0, 100, 0 };
    Vec3f sparkPos;

    sparkVel.y = Rand_ZeroFloat(0.07f) + -0.1f;
    sparkAccel.y = Rand_ZeroFloat(0.1f) + 0.04f;

    f32 sign = (Rand_ZeroOne() < 0.5f) ? -1.0f : 1.0f;
    sparkVel.x = (Rand_ZeroFloat(0.2f) + 0.1f) * sign;
    sparkAccel.x = 0.1f * sign;

    sign = (Rand_ZeroOne() < 0.5f) ? -1.0f : 1.0f;
    sparkVel.z = (Rand_ZeroFloat(0.2f) + 0.1f) * sign;
    sparkAccel.z = 0.1f * sign;

    sparkPos.x = pos->x;
    sparkPos.y = Rand_ZeroFloat(15.0f) + pos->y;
    sparkPos.z = pos->z;

    EffectSsKiraKira_SpawnDispersed(play, &sparkPos, &sparkVel, &sparkAccel, &primColor, &envColor, -50, 11);
}

// Port of MM func_80127DA4 (z_player_lib.c:3437-3542)
// OOT Math_Atan2S takes (x, y), the same argument order as MM Math_Atan2S_XY.
static void FairyHair_UpdateStrand(PlayState* play, FairyHairLink arg1[], HairChainParam arg2[], s32 arg3, Vec3f* arg4,
                                   Vec3f* arg5, u32* arg6) {
    FairyHairLink* phi_s1 = &arg1[1];
    Vec3f spB0;
    Vec3f spA4;
    f32 f22;
    f32 f28;
    f32 f24;
    f32 f20;
    f32 f0;
    f32 sp8C = -1.0f;
    s32 i;
    s16 s0;
    s16 s2;

    Math_Vec3f_Copy(&arg1->pos, arg4);
    Math_Vec3f_Diff(arg5, arg4, &spB0);
    // Preserve MM XY argument order; the NEI swap reversed these angles.
    arg1->yaw = Math_Atan2S(spB0.z, spB0.x);
    arg1->pitch = Math_Atan2S(sqrtf(SQ(spB0.x) + SQ(spB0.z)), spB0.y);
    i = 1;
    arg2++;

    while (i < arg3) {

        if (sFairyHairActivated) {
            if (*arg6 & 0x20) {
                sp8C = -0.2f;
            } else {
                sp8C = 0.2f;
            }

            *arg6 += 0x16;
            if (!(*arg6 & 1)) {
                FairyHair_SpawnParticles(play, &phi_s1->pos);
            }
        }
        Math_Vec3f_Sum(&phi_s1->pos, &phi_s1->vel, &phi_s1->pos);

        f0 = Math_Vec3f_DistXYZAndStoreDiff(&arg1->pos, &phi_s1->pos, &spB0);
        f28 = f0 - arg2->length;
        if (f0 == 0.0f) {
            spB0.x = 0.0f;
            spB0.y = arg2->length;
            spB0.z = 0.0f;
        }
        f20 = sqrtf(SQ(spB0.x) + SQ(spB0.z));

        if (f20 > 4.0f) {
            phi_s1->yaw = Math_Atan2S(spB0.z, spB0.x);
            s2 = phi_s1->yaw - arg1->yaw;

            if (ABS(s2) > 0x4000) {
                phi_s1->yaw = (s16)(phi_s1->yaw + 0x8000);
                f20 = -f20;
            }
        }

        phi_s1->pitch = Math_Atan2S(f20, spB0.y);

        s2 = phi_s1->yaw - arg1->yaw;
        s2 = CLAMP(s2, -arg2->maxYaw, arg2->maxYaw);
        phi_s1->yaw = arg1->yaw + s2;

        s0 = phi_s1->pitch - arg1->pitch;
        s0 = CLAMP(s0, -arg2->maxPitch, arg2->maxPitch);
        phi_s1->pitch = arg1->pitch + s0;

        f20 = Math_CosS(phi_s1->pitch) * arg2->length;
        spA4.x = Math_SinS(phi_s1->yaw) * f20;
        spA4.z = Math_CosS(phi_s1->yaw) * f20;
        spA4.y = Math_SinS(phi_s1->pitch) * arg2->length;
        Math_Vec3f_Sum(&arg1->pos, &spA4, &phi_s1->pos);
        phi_s1->vel.x *= 0.9f;
        phi_s1->vel.z *= 0.9f;

        f22 = Math_CosS(s0) * f28;
        f24 = Math_SinS(s0) * f28;
        phi_s1->vel.y += sp8C;

        if (sFairyHairActivated) {
            phi_s1->vel.y = CLAMP(phi_s1->vel.y, -0.8f, 0.8f);
        } else {
            f20 = Math_SinS(arg1->pitch);
            phi_s1->vel.y += (((f22 * Math_CosS(arg1->pitch)) + (f24 * f20)) * 0.2f);
            phi_s1->vel.y = CLAMP(phi_s1->vel.y, -2.0f, 4.0f);
        }

        f20 = (f24 * Math_CosS(arg1->pitch)) - (Math_SinS(arg1->pitch) * f22);
        f22 = Math_CosS(s2) * f20;
        f24 = Math_SinS(s2) * f20;

        f20 = Math_SinS(arg1->yaw);

        phi_s1->vel.x += (((f24 * Math_CosS(arg1->yaw)) - (f22 * f20)) * 0.1f);
        phi_s1->vel.x = CLAMP(phi_s1->vel.x, -4.0f, 4.0f);

        f20 = Math_SinS(arg1->yaw);

        phi_s1->vel.z += (((f22 * Math_CosS(arg1->yaw)) + (f24 * f20)) * -0.1f);
        phi_s1->vel.z = CLAMP(phi_s1->vel.z, -4.0f, 4.0f);

        arg1++;
        phi_s1++;
        i++;
        arg2++;
    }
}

// Convert strand angles to Mtx for segment 0x0B (from MM func_80128388)
// Port of MM func_80128388 (z_player_lib.c:3545-3566)
static void FairyHair_ComputeStrandMatrices(FairyHairLink arg0[], HairChainParam arg1[], s32 arg2, Mtx** arg3) {
    FairyHairLink* phi_s1 = &arg0[1];
    Vec3f sp58;
    Vec3s sp50;
    s32 i;

    sp58.y = 0.0f;
    sp58.z = 0.0f;
    sp50.x = 0;

    for (i = 1; i < arg2; i++) {
        sp58.x = arg1->length * 100.0f;
        sp50.z = arg1->rotX + (s16)(phi_s1->pitch - arg0->pitch);
        sp50.y = arg1->rotY + (s16)(phi_s1->yaw - arg0->yaw);
        Matrix_TranslateRotateZYX(&sp58, &sp50);
        Matrix_ToMtx(*arg3, (char*)__FILE__, __LINE__);
        (*arg3)++;
        arg0++;
        phi_s1++;
        arg1++;
    }
}

// Full draw: compute all 3 strands' matrices for segment 0x0B (from MM Player_DrawGreatFairysMask)
static void FairyHair_ComputeMatrices(PlayState* play, Player* player, Mtx* mtxBuffer) {
    Vec3f rootWorld, targetWorld;
    // Use play->gameplayFrames directly, like MM does (sp6C = play->gameplayFrames)
    u32 frame = play->gameplayFrames;

    // Only run physics simulation ONCE per game frame.
    // SoH may call player draw multiple times per frame (reflections, pause, etc.)
    // which would cause double-speed physics if not guarded.
    s32 doPhysics = (play->gameplayFrames != sHairLastPhysicsFrame);
    if (doPhysics) {
        sHairLastPhysicsFrame = play->gameplayFrames;
    }

    if (!sFairyHairInited) FairyHair_Init(&player->bodyPartsPos[PLAYER_BODYPART_HEAD]);
    sFairyHairActivated = (play->sceneNum == SCENE_GREAT_FAIRYS_FOUNTAIN_MAGIC ||
                           play->sceneNum == SCENE_GREAT_FAIRYS_FOUNTAIN_SPELLS);

    // Update constraint targets from model matrix
    Matrix_MultVec3f(&sHairTargetOffset, &sHairChainParams[1].target);
    {
        Vec3f* head = &player->bodyPartsPos[PLAYER_BODYPART_HEAD];
        Vec3f* waist = &player->bodyPartsPos[PLAYER_BODYPART_WAIST];
        sHairChainParams[2].target.x = head->x + (waist->x - head->x) * 0.2f;
        sHairChainParams[2].target.y = head->y + (waist->y - head->y) * 0.2f;
        sHairChainParams[2].target.z = head->z + (waist->z - head->z) * 0.2f;
    }

    for (s32 i = 0; i < 3; i++) {
        Matrix_MultVec3f(&sHairRootPos[i], &rootWorld);
        Matrix_MultVec3f(&sHairTargetPos[i], &targetWorld);

        if (doPhysics) {
            FairyHair_UpdateStrand(play, sFairyHairStrands[i], sHairChainParams, 3, &rootWorld, &targetWorld, &frame);
            frame += 11;
        }

        Matrix_Push();
        Matrix_Translate(sHairRootPos[i].x, sHairRootPos[i].y, sHairRootPos[i].z, MTXMODE_APPLY);
        FairyHair_ComputeStrandMatrices(sFairyHairStrands[i], sHairChainParams, 3, &mtxBuffer);
        Matrix_Pop();
    }
}

