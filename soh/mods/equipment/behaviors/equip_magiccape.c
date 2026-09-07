/**
 * equip_magiccape.c - Magic Cape (Extended Tunic Slot 1)
 *
 * Behavior: Ganondorf's cape cloth physics attached to Link's shoulders.
 * Uses the same DLs and textures from ovl_En_Ganon_Mant (gMantDL, gMantTex, etc.)
 * with Verlet cloth simulation adapted for Link's proportions.
 *
 * Cape attaches between PLAYER_LIMB_L_SHOULDER and PLAYER_LIMB_R_SHOULDER,
 * draping down Link's back with full physics simulation.
 *
 * Included by ext_equip_behavior.c (unity build).
 */

// No extra includes - unity-built from ext_equip_behavior.c
// which inherits all from extended_equipment.c

#include "overlays/ovl_En_Ganon_Mant/ovl_En_Ganon_Mant.h"
#include "soh/ResourceManagerHelpers.h"

// ---------------------------------------------------------------------------
// Constants (adapted from EnGanonMant for Link's scale)
// ---------------------------------------------------------------------------
#define CAPE_NUM_JOINTS 12
#define CAPE_NUM_STRANDS 12
#define CAPE_JOINT_LENGTH 4.5f
#define CAPE_GRAVITY -3.0f
#define CAPE_BACK_PUSH -4.0f
#define CAPE_MIN_DIST 8.0f
#define CAPE_MIN_Y_OFFSET -200.0f // Below actor pos
#define CAPE_TEX_WIDTH 32
#define CAPE_TEX_HEIGHT 64

// ---------------------------------------------------------------------------
// Custom Items editor — live-tunable parameters (Skijer's NEI)
// ---------------------------------------------------------------------------
// Every constant above is now a DEFAULT; the real value is read once per frame
// from the `gItemEditor.Cape.*` CVars driven by the "Item Editor" tab in the
// NEI menu. The CVar names are identical in Ship and 2ship, so a preset moves
// between both games untouched. With `gItemEditor.Cape.Custom` = 0 the struct is
// filled with the vanilla defaults and the cloth behaves exactly as before.
#define CAPE_CVAR(name) "gItemEditor.Cape." name
#define CAPE_DEG_TO_RAD (M_PI / 180.0f)

typedef struct {
    // Shape
    f32 scale;       // master multiplier over length + width
    f32 jointLength; // per-joint segment length (total length = 11 * this)
    f32 width;       // multiplier over the shoulder half-span
    f32 arcSpan;     // radians the strand roots are spread over (vanilla PI)
    f32 arcBulge;    // how far the root arc bows away from the shoulder line
    f32 arcSpread;   // root arc span along the shoulder line
    // Placement (in the shoulder frame: x = bulge/back, y = up, z = shoulder line)
    f32 offX, offY, offZ;
    f32 yaw, pitch, roll; // radians, added on top of the shoulder-derived angles
    // Physics
    f32 gravity;
    f32 backPush;
    f32 minDist;     // push-away radius from the player's center
    f32 floorOffset; // lowest the cloth may hang, relative to actor Y
    f32 backSway;    // per-unit-of-speed back sway
    f32 sideSway;    // per-unit-of-speed side sway
    f32 damping;     // velocity retained per tick
    f32 velClamp;    // per-axis velocity limit
    f32 decel;       // per-tick approach-zero rate
    // Look
    u8 tinted; // 1 when the color differs from opaque white (skip the override otherwise)
    u8 r, g, b, a;
} CapeParams;

static CapeParams sCapeP;

static void MagicCape_LoadParams(void) {
    CapeParams* p = &sCapeP;

    p->scale = 1.0f;
    p->jointLength = CAPE_JOINT_LENGTH;
    p->width = 1.0f;
    p->arcSpan = M_PI;
    p->arcBulge = 1.0f;
    p->arcSpread = 1.0f;
    p->offX = p->offY = p->offZ = 0.0f;
    p->yaw = p->pitch = p->roll = 0.0f;
    p->gravity = CAPE_GRAVITY;
    p->backPush = CAPE_BACK_PUSH;
    p->minDist = CAPE_MIN_DIST;
    p->floorOffset = CAPE_MIN_Y_OFFSET;
    p->backSway = 0.3f;
    p->sideSway = 0.15f;
    p->damping = 0.8f;
    p->velClamp = 5.0f;
    p->decel = 0.1f;
    p->tinted = 0;
    p->r = p->g = p->b = p->a = 255;

    if (!CVarGetInteger(CAPE_CVAR("Custom"), 0)) {
        return;
    }

    p->scale = CVarGetFloat(CAPE_CVAR("Scale"), p->scale);
    p->jointLength = CVarGetFloat(CAPE_CVAR("Length"), p->jointLength);
    p->width = CVarGetFloat(CAPE_CVAR("Width"), p->width);
    p->arcSpan = CVarGetFloat(CAPE_CVAR("ArcSpan"), 180.0f) * CAPE_DEG_TO_RAD;
    p->arcBulge = CVarGetFloat(CAPE_CVAR("ArcBulge"), p->arcBulge);
    p->arcSpread = CVarGetFloat(CAPE_CVAR("ArcSpread"), p->arcSpread);
    p->offX = CVarGetFloat(CAPE_CVAR("OffsetX"), 0.0f);
    p->offY = CVarGetFloat(CAPE_CVAR("OffsetY"), 0.0f);
    p->offZ = CVarGetFloat(CAPE_CVAR("OffsetZ"), 0.0f);
    p->yaw = CVarGetFloat(CAPE_CVAR("Yaw"), 0.0f) * CAPE_DEG_TO_RAD;
    p->pitch = CVarGetFloat(CAPE_CVAR("Pitch"), 0.0f) * CAPE_DEG_TO_RAD;
    p->roll = CVarGetFloat(CAPE_CVAR("Roll"), 0.0f) * CAPE_DEG_TO_RAD;
    p->gravity = CVarGetFloat(CAPE_CVAR("Gravity"), p->gravity);
    p->backPush = CVarGetFloat(CAPE_CVAR("BackPush"), p->backPush);
    p->minDist = CVarGetFloat(CAPE_CVAR("MinDist"), p->minDist);
    p->floorOffset = CVarGetFloat(CAPE_CVAR("FloorOffset"), p->floorOffset);
    p->backSway = CVarGetFloat(CAPE_CVAR("BackSway"), p->backSway);
    p->sideSway = CVarGetFloat(CAPE_CVAR("SideSway"), p->sideSway);
    p->damping = CVarGetFloat(CAPE_CVAR("Damping"), p->damping);
    p->velClamp = CVarGetFloat(CAPE_CVAR("VelClamp"), p->velClamp);
    p->decel = CVarGetFloat(CAPE_CVAR("Decel"), p->decel);
    p->r = (u8)CVarGetInteger(CAPE_CVAR("ColorR"), 255);
    p->g = (u8)CVarGetInteger(CAPE_CVAR("ColorG"), 255);
    p->b = (u8)CVarGetInteger(CAPE_CVAR("ColorB"), 255);
    p->a = (u8)CVarGetInteger(CAPE_CVAR("ColorA"), 255);
    p->tinted = (p->r != 255 || p->g != 255 || p->b != 255 || p->a != 255);

    // Guard against values that would divide by zero or invert the cloth.
    if (p->scale < 0.01f) {
        p->scale = 0.01f;
    }
    if (p->jointLength < 0.01f) {
        p->jointLength = 0.01f;
    }
}

// ---------------------------------------------------------------------------
// Strand struct (same as MantStrand from z_en_ganon_mant.h)
// ---------------------------------------------------------------------------
typedef struct {
    Vec3f root;
    Vec3f joints[CAPE_NUM_JOINTS];
    Vec3f rotations[CAPE_NUM_JOINTS];
    Vec3f velocities[CAPE_NUM_JOINTS];
} CapeStrand; // no torn[] needed

// ---------------------------------------------------------------------------
// Static state
// ---------------------------------------------------------------------------
static CapeStrand sCapeStrands[CAPE_NUM_STRANDS];
static u8 sCapeMaskTex[CAPE_TEX_WIDTH * CAPE_TEX_HEIGHT];
static u8 sCapeInitialized = 0;
static u8 sCapeFrameTimer = 0;
static u8 sCapeUpdateHasRun = 0;
static f32 sCapeBaseYaw = 0.0f;

// Persistent across cape (re)inits and scene transitions: register the blended texture
// once and never unregister. Re-registering each Init/Reset cycle while the GPU pipeline
// still references the prior registration was the suspect for intermittent crashes in
// scenes with dense cutscene churn (Lon Lon, Kakariko).
static u8 sCapeTexRegistered = 0;

// On the first physics tick after Init (scene change, equip toggle, cutscene exit),
// snap every joint of every strand to its current root position so the cape doesn't
// settle from stale world coordinates left over from the previous scene.
static u8 sCapeNeedsRootSnap = 1;

// Shoulder positions captured from PostLimbDraw
static Vec3f sCapeLeftShoulderPos;
static Vec3f sCapeRightShoulderPos;
static u8 sCapeShouldersCaptured = 0;

// ---------------------------------------------------------------------------
// Physics coefficients (from EnGanonMant)
// ---------------------------------------------------------------------------
static f32 sCapeBackSwayCoeff[CAPE_NUM_JOINTS] = {
    0.0f, 1.0f, 0.5f, 0.25f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
};

static f32 sCapeSideSwayCoeff[CAPE_NUM_JOINTS] = {
    0.0f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f, 0.0f,
};

static f32 sCapeDistMult[CAPE_NUM_JOINTS] = {
    0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f,
};

// Vertex mapping (same as EnGanonMant)
#define CAPE_MAP_STRAND(n)                                                                                          \
    (n) + CAPE_NUM_JOINTS * 0, (n) + CAPE_NUM_JOINTS * 1, (n) + CAPE_NUM_JOINTS * 2, (n) + CAPE_NUM_JOINTS * 3,     \
        (n) + CAPE_NUM_JOINTS * 4, (n) + CAPE_NUM_JOINTS * 5, (n) + CAPE_NUM_JOINTS * 6, (n) + CAPE_NUM_JOINTS * 7, \
        (n) + CAPE_NUM_JOINTS * 8, (n) + CAPE_NUM_JOINTS * 9, (n) + CAPE_NUM_JOINTS * 10, (n) + CAPE_NUM_JOINTS * 11

static u16 sCapeVerticesMap[CAPE_NUM_STRANDS * CAPE_NUM_JOINTS] = {
    CAPE_MAP_STRAND(11), CAPE_MAP_STRAND(10), CAPE_MAP_STRAND(9), CAPE_MAP_STRAND(8),
    CAPE_MAP_STRAND(7),  CAPE_MAP_STRAND(6),  CAPE_MAP_STRAND(5), CAPE_MAP_STRAND(4),
    CAPE_MAP_STRAND(3),  CAPE_MAP_STRAND(2),  CAPE_MAP_STRAND(1), CAPE_MAP_STRAND(0),
};

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
static void MagicCape_Init(void) {
    if (sCapeInitialized)
        return;

    memset(sCapeStrands, 0, sizeof(sCapeStrands));
    sCapeFrameTimer = 0;
    sCapeUpdateHasRun = 0;
    sCapeShouldersCaptured = 0;
    sCapeNeedsRootSnap = 1;

    // Register the blended texture exactly once over the lifetime of the process.
    // sCapeMaskTex is static so its address is stable; the mask stays zero-filled which
    // is a no-op blend (passes the source texture through unchanged).
    if (!sCapeTexRegistered) {
        memset(sCapeMaskTex, 0, sizeof(sCapeMaskTex));
        Gfx_RegisterBlendedTexture(gMantTex, sCapeMaskTex, NULL);
        sCapeTexRegistered = 1;
    }

    sCapeInitialized = 1;
}

// ---------------------------------------------------------------------------
// Reset (when cape is unequipped)
// ---------------------------------------------------------------------------
static void MagicCape_Reset(void) {
    if (!sCapeInitialized)
        return;

    // Note: we intentionally do NOT call Gfx_UnregisterBlendedTexture here. The texture
    // registration is established once in Init and persists for the rest of the session.
    // The mask buffer is static so its lifetime is forever; the GPU can keep referencing it
    // safely across re-inits without races.
    sCapeInitialized = 0;
    sCapeShouldersCaptured = 0;
    sCapeNeedsRootSnap = 1;
}

// ---------------------------------------------------------------------------
// Capture shoulder position (called from PostLimbDraw)
// ---------------------------------------------------------------------------
static void MagicCape_CaptureShoulderPos(s32 limbIndex) {
    Vec3f origin = { 0.0f, 200.0f, 0.0f }; // Offset up from shoulder joint

    if (limbIndex == PLAYER_LIMB_L_SHOULDER) {
        Matrix_MultVec3f(&origin, &sCapeLeftShoulderPos);
        sCapeShouldersCaptured |= 1;
    } else if (limbIndex == PLAYER_LIMB_R_SHOULDER) {
        Matrix_MultVec3f(&origin, &sCapeRightShoulderPos);
        sCapeShouldersCaptured |= 2;
    }
}

// ---------------------------------------------------------------------------
// Update single strand (adapted from EnGanonMant_UpdateStrand)
// ---------------------------------------------------------------------------
static void MagicCape_UpdateStrand(Vec3f* actorPos, f32 actorRotY, Vec3f* root, Vec3f* pos, Vec3f* nextPos, Vec3f* rot,
                                   Vec3f* vel, s16 strandNum, f32 backSwayMag, f32 sideSwayMag, f32 minY) {
    s16 i;
    f32 x, y, z;
    f32 yaw;
    f32 xDiff, zDiff;
    Vec3f delta;
    Vec3f posStep;
    Vec3f backSwayOffset;
    Vec3f sideSwayOffset;

    for (i = 0; i < CAPE_NUM_JOINTS; i++, pos++, vel++, rot++, nextPos++) {
        if (i == 0) {
            pos->x = root->x;
            pos->y = root->y;
            pos->z = root->z;
        } else {
            // Decelerate
            Math_ApproachZeroF(&vel->x, 1.0f, sCapeP.decel);
            Math_ApproachZeroF(&vel->y, 1.0f, sCapeP.decel);
            Math_ApproachZeroF(&vel->z, 1.0f, sCapeP.decel);

            // Back push + sway
            delta.x = 0;
            delta.y = 0;
            delta.z = (sCapeP.backPush + (sinf((strandNum * (2 * M_PI)) / 2.1f) * backSwayMag)) * sCapeBackSwayCoeff[i];
            Matrix_RotateY(sCapeBaseYaw, MTXMODE_NEW);
            Matrix_MultVec3f(&delta, &backSwayOffset);

            // Side sway
            delta.x = cosf((strandNum * M_PI) / (CAPE_NUM_STRANDS - 1.0f)) * sideSwayMag * sCapeSideSwayCoeff[i];
            delta.z = 0;
            Matrix_MultVec3f(&delta, &sideSwayOffset);

            // Position difference
            x = ((pos->x + vel->x) - (pos - 1)->x) + (backSwayOffset.x + sideSwayOffset.x);
            y = ((pos->y + vel->y) - (pos - 1)->y) + sCapeP.gravity;
            z = ((pos->z + vel->z) - (pos - 1)->z) + (backSwayOffset.z + sideSwayOffset.z);

            // Rotation
            yaw = Math_Atan2F(z, x);
            x = -Math_Atan2F(sqrtf(SQ(x) + SQ(z)), y);
            (rot - 1)->x = x;

            // Constrained position
            delta.x = 0;
            delta.y = 0;
            delta.z = sCapeP.jointLength * sCapeP.scale;
            Matrix_RotateY(yaw, MTXMODE_NEW);
            Matrix_RotateX(x, MTXMODE_APPLY);
            Matrix_MultVec3f(&delta, &posStep);

            // Save old position
            x = pos->x;
            y = pos->y;
            z = pos->z;

            // New position
            pos->x = (pos - 1)->x + posStep.x;
            pos->y = (pos - 1)->y + posStep.y;
            pos->z = (pos - 1)->z + posStep.z;

            // Push away from actor center
            xDiff = pos->x - actorPos->x;
            zDiff = pos->z - actorPos->z;
            if (sqrtf(SQ(xDiff) + SQ(zDiff)) < (sCapeDistMult[i] * sCapeP.minDist)) {
                yaw = Math_Atan2F(zDiff, xDiff);
                delta.z = sCapeP.minDist * sCapeDistMult[i];
                delta.x = 0;
                Matrix_RotateY(yaw, MTXMODE_NEW);
                Matrix_MultVec3f(&delta, &posStep);
                pos->x = actorPos->x + posStep.x;
                pos->z = actorPos->z + posStep.z;
            }

            // Floor constraint
            if (pos->y < minY) {
                pos->y = minY;
            }

            // Velocity (80% damping by default)
            vel->x = (pos->x - x) * sCapeP.damping;
            vel->y = (pos->y - y) * sCapeP.damping;
            vel->z = (pos->z - z) * sCapeP.damping;

            // Clamp velocity
            {
                f32 clamp = sCapeP.velClamp;

                if (vel->x > clamp)
                    vel->x = clamp;
                else if (vel->x < -clamp)
                    vel->x = -clamp;
                if (vel->y > clamp)
                    vel->y = clamp;
                else if (vel->y < -clamp)
                    vel->y = -clamp;
                if (vel->z > clamp)
                    vel->z = clamp;
                else if (vel->z < -clamp)
                    vel->z = -clamp;
            }

            // Update angle
            xDiff = pos->x - nextPos->x;
            zDiff = pos->z - nextPos->z;
            (rot - 1)->y = Math_Atan2F(zDiff, xDiff);
        }
    }
    rot[11].y = rot[10].y;
    rot[11].x = rot[10].x;
}

// ---------------------------------------------------------------------------
// Update vertices (adapted from EnGanonMant_UpdateVertices)
// ---------------------------------------------------------------------------
static void MagicCape_UpdateVertices(void) {
    s16 i, j, k;
    Vtx* vtx;
    Vtx* vertices;
    CapeStrand* strand;
    Vec3f up = { 0.0f, 30.0f, 0.0f };
    Vec3f normal;

    if (sCapeFrameTimer % 2 != 0) {
        vertices = SEGMENTED_TO_VIRTUAL(gMant1Vtx);
    } else {
        vertices = SEGMENTED_TO_VIRTUAL(gMant2Vtx);
    }

    vertices = ResourceMgr_LoadVtxByName((char*)vertices);
    if (vertices == NULL) {
        return;
    }

    strand = &sCapeStrands[0];
    for (i = 0; i < CAPE_NUM_STRANDS; i++, strand++) {
        for (j = 0, k = 0; j < CAPE_NUM_JOINTS; j++, k += CAPE_NUM_JOINTS) {
            vtx = &vertices[sCapeVerticesMap[i + k]];
            vtx->n.ob[0] = strand->joints[j].x;
            vtx->n.ob[1] = strand->joints[j].y;
            vtx->n.ob[2] = strand->joints[j].z;
            Matrix_RotateY(strand->rotations[j].y, MTXMODE_NEW);
            Matrix_RotateX(strand->rotations[j].x, MTXMODE_APPLY);
            Matrix_MultVec3f(&up, &normal);
            vtx->n.n[0] = normal.x;
            vtx->n.n[1] = normal.y;
            vtx->n.n[2] = normal.z;
        }
    }
}

// ---------------------------------------------------------------------------
// Draw cape (adapted from EnGanonMant_DrawCloak + EnGanonMant_Draw)
// ---------------------------------------------------------------------------
static void MagicCape_Draw(Player* player, PlayState* play) {
    if (!sCapeInitialized)
        return;
    // Skip cape rendering while riding Epona (and other special states): the player skeleton
    // is in horse pose, shoulder limbs land in unexpected positions, and the cape produces
    // garbage geometry.
    if (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
        return;
    }
    if (sCapeShouldersCaptured != 3)
        return; // Need both shoulders

    // Item Editor: refresh the tunables once per drawn frame.
    MagicCape_LoadParams();

    // --- Physics update (runs once per frame in Draw, like original) ---
    if (sCapeUpdateHasRun) {
        Vec3f* rightPos = &sCapeRightShoulderPos;
        Vec3f* leftPos = &sCapeLeftShoulderPos;

        f32 xDiff = leftPos->x - rightPos->x;
        f32 yDiff = leftPos->y - rightPos->y;
        f32 zDiff = leftPos->z - rightPos->z;

        Vec3f midpoint;
        midpoint.x = rightPos->x + xDiff * 0.5f;
        midpoint.y = rightPos->y + yDiff * 0.5f;
        midpoint.z = rightPos->z + zDiff * 0.5f;

        f32 yaw = Math_Atan2F(zDiff, xDiff);
        f32 pitch = -Math_Atan2F(sqrtf(SQ(xDiff) + SQ(zDiff)), yDiff);
        f32 diffHalfDist = sqrtf(SQ(xDiff) + SQ(yDiff) + SQ(zDiff)) * 0.5f;

        // Item Editor: yaw/pitch/roll ride on top of the shoulder-derived frame, so the whole
        // cape (root arc included) can be re-aimed without touching the shoulder anchors.
        Matrix_RotateY(yaw + sCapeP.yaw, MTXMODE_NEW);
        Matrix_RotateX(pitch + sCapeP.pitch, MTXMODE_APPLY);
        if (sCapeP.roll != 0.0f) {
            Matrix_RotateZ(sCapeP.roll, MTXMODE_APPLY);
        }
        sCapeBaseYaw = yaw + sCapeP.yaw - M_PI / 2.0f;

        // Movement-based sway
        f32 speed = player->actor.speedXZ;
        f32 backSwayMag = speed * sCapeP.backSway;
        f32 sideSwayMag = speed * sCapeP.sideSway;
        f32 minY = player->actor.world.pos.y + sCapeP.floorOffset;
        f32 halfSpan = diffHalfDist * sCapeP.width * sCapeP.scale;

        for (s16 strandIdx = 0; strandIdx < CAPE_NUM_STRANDS; strandIdx++) {
            Matrix_Push();

            Vec3f strandOffset;
            Vec3f strandDivPos;
            // Root arc: `arcSpan` is the angle the roots are spread over (180 deg = the vanilla
            // semicircle), `arcBulge` how far the arc bows away from the shoulder line (the
            // "parabola" depth) and `arcSpread` its width along that line.
            f32 t = (strandIdx * sCapeP.arcSpan) / (CAPE_NUM_STRANDS - 1);
            strandOffset.x = sinf(t) * halfSpan * sCapeP.arcBulge + sCapeP.offX;
            strandOffset.y = sCapeP.offY;
            strandOffset.z = -cosf(t) * halfSpan * sCapeP.arcSpread + sCapeP.offZ;
            Matrix_MultVec3f(&strandOffset, &strandDivPos);
            sCapeStrands[strandIdx].root.x = midpoint.x + strandDivPos.x;
            sCapeStrands[strandIdx].root.y = midpoint.y + strandDivPos.y;
            sCapeStrands[strandIdx].root.z = midpoint.z + strandDivPos.z;

            // First physics tick after Init: collapse every joint of this strand onto the
            // current root so the cape doesn't have to settle from world (0,0,0) coords left
            // by memset, which produced a violent first frame after every scene transition.
            if (sCapeNeedsRootSnap) {
                for (s32 j = 0; j < CAPE_NUM_JOINTS; j++) {
                    sCapeStrands[strandIdx].joints[j] = sCapeStrands[strandIdx].root;
                    sCapeStrands[strandIdx].velocities[j].x = 0.0f;
                    sCapeStrands[strandIdx].velocities[j].y = 0.0f;
                    sCapeStrands[strandIdx].velocities[j].z = 0.0f;
                }
            }

            s16 nextStrandIdx = strandIdx + 1;
            if (nextStrandIdx >= CAPE_NUM_STRANDS) {
                nextStrandIdx = strandIdx - 1;
            }

            MagicCape_UpdateStrand(&player->actor.world.pos, player->actor.shape.rot.y, &sCapeStrands[strandIdx].root,
                                   sCapeStrands[strandIdx].joints, sCapeStrands[nextStrandIdx].joints,
                                   sCapeStrands[strandIdx].rotations, sCapeStrands[strandIdx].velocities, strandIdx,
                                   backSwayMag, sideSwayMag, minY);

            Matrix_Pop();
        }

        MagicCape_UpdateVertices();
        sCapeUpdateHasRun = 0;
        sCapeNeedsRootSnap = 0;
    }

    // --- Render ---
    OPEN_DISPS(play->state.gfxCtx);

    gSPInvalidateTexCache(POLY_OPA_DISP++, sCapeMaskTex);

    Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    gSPDisplayList(POLY_OPA_DISP++, gMantMaterialDL);

    // Item Editor tint: the material DL leaves its own combiner bound, and nothing guarantees it
    // reads PRIM — so when a custom color is set we re-pin a MODULATERGBA combiner on top of the
    // texture the material DL just loaded and feed it our prim color. Untinted (the default) keeps
    // the material DL's state byte-for-byte.
    if (sCapeP.tinted) {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
        if (sCapeP.a < 255) {
            gDPSetRenderMode(POLY_OPA_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
        }
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, sCapeP.r, sCapeP.g, sCapeP.b, sCapeP.a);
    }

    if (sCapeFrameTimer % 2 != 0) {
        gSPSegmentLoadRes(POLY_OPA_DISP++, 0x0C, gMant1Vtx);
    } else {
        gSPSegmentLoadRes(POLY_OPA_DISP++, 0x0C, gMant2Vtx);
    }

    gSPDisplayList(POLY_OPA_DISP++, gMantDL);

    // Restore segment 0x0C to gCullBackDList after the cape DL finishes.
    // SOH's player draw pipeline binds segment 0x0C to gCullBackDList (the backface-cull
    // dlist that the skeleton DL chain jumps to when an LOD/cull threshold is hit —
    // see pak_loader.cpp:3000 and z_player_lib.c). ExtEquip_DrawDispatch runs inside
    // Player_Draw, so anything drawn after this (Four Sword clones via SkelAnime_DrawFlexOpa,
    // IK Axe reticle, etc.) reuses that segment binding.
    //
    // If we leave 0x0C pointed at the cape vertex buffer (gMant1Vtx/gMant2Vtx — 192 bytes
    // per strand), a clone limb's gSPDisplayList(0x0C000000) cull-jump executes vertex
    // data as opcodes (the "0x62 0x70 0x55 0x50 ..." ASCII-looking pattern in the log,
    // which is s16 world-space coords from MagicCape_UpdateVertices reinterpreted as
    // RSP commands). Setting it to NULL/0 doesn't help — the cull-jump still dereferences
    // a bad address. Restoring to gCullBackDList is what the player skeleton expects.
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gCullBackDList);

    CLOSE_DISPS(play->state.gfxCtx);
}

// ---------------------------------------------------------------------------
// Cleanup: called EVERY frame from dispatch, regardless of equipped tunic.
// Handles resetting cape when tunic changes away.
// ---------------------------------------------------------------------------
static void MagicCape_Cleanup(void) {
    // Skijer 2026-07-15: the cape is no longer an ext-equipment slot — the cloth shows whenever the
    // cape is OWNED and not hidden via its kaleido upgrade-cell toggle.
    if (!ExtEquip_CapeVisible() && sCapeInitialized) {
        MagicCape_Reset();
    }
}

// ---------------------------------------------------------------------------
// PASSIVE effect (Skijer 2026-07-15): the cape HALVES all magic COSTS (commit 10a66533's MAGIC_REQ),
// so spells are castable with half the base magic. Implemented at the cost sites, not here:
// Magic_RequestChange (z_parameter.c, vanilla spells + API), ItemMagic_Consume/HasEnough
// (equip_helper.c, all custom magic items), FS_CLONE_MP_COST (Four Sword). The old refund tracker
// (recover half of spent magic) was REMOVED — it required the FULL cost upfront to cast and would
// have double-dipped with the real half cost.
// ---------------------------------------------------------------------------
// Main behavior entry (cloth physics only — called per frame when the cape is VISIBLE)
// ---------------------------------------------------------------------------
static void MagicCape_Behavior(Player* player, PlayState* play) {
    // Skip while riding Epona — pairs with the same guard in MagicCape_Draw.
    // We don't Reset here; we just stop updating, so the cape resumes naturally on dismount.
    if (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
        return;
    }

    // Skip during cutscenes
    if (player->stateFlags1 &
        (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING | PLAYER_STATE1_IN_ITEM_CS)) {
        if (sCapeInitialized) {
            MagicCape_Reset();
        }
        return;
    }

    // Initialize if needed
    if (!sCapeInitialized) {
        MagicCape_Init();
    }

    sCapeFrameTimer++;
    sCapeUpdateHasRun = 1;

    // Reset shoulder capture flags for next frame
    sCapeShouldersCaptured = 0;
}
