#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "functions.h"
#include "macros.h"
#include "align_asset_macro.h"
#include "variables.h"
#include "message_data_textbox_types.h"
#include "overlays/actors/ovl_Boss_Ganon2/z_boss_ganon2.h"
#include "objects/object_ganon/object_ganon.h"
#include "objects/object_gnd/object_gnd.h"
#include "objects/object_fhg/object_fhg.h"
#include "objects/object_ganon_anime1/object_ganon_anime1.h"
#include "objects/object_ganon_anime2/object_ganon_anime2.h"
#include "objects/object_fd/object_fd.h"
#include "objects/object_efc_star_field/object_efc_star_field.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/ModMessages/ModMessages.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "z64boss.h"

#define DANCE_CVAR "gMods.CucuiGanon.DanceEnabled"
#define SHOWCASE_CVAR "gMods.CucuiGanon.PlayFullM5"
#define DANCE_LIMBS 25
#define DESCENT_FRAMES 60
#define LOOP_FRAMES 576
#define BLEND_START 40
#define MUSIC_ENTRY_FRAME 274
#define MUSIC_LOOP_FRAME 312
#define LYRIC_BOX_ALPHA 85
#define ARENA_CENTER_X -200.0f
#define ARENA_CENTER_Z -200.0f
#define ARENA_HALF_WIDTH 735.0f
#define ARENA_SAFE_RADIUS 735.0f
#define DASH_TRIGGER_DISTANCE 390.0f
#define DASH_CONTACT_DISTANCE 72.0f
#define PHANTOM_COUNT 4
#define PHANTOM_KEY "skijer.cucui_ganon.phantom"
#define WARP_KEY "skijer.cucui_ganon.blue_warp"
#define BOSS_HINT_KEY "skijer.cucui_ganon.boss"
#define BOSS_KEY "skijer.cucui_ganon"
#define BOSS_MAX_HEALTH 80
#define BOSS_PHASE2_HEALTH 40
#define BACK_DAMAGE 1
#define RETURN_DAMAGE 5
#define STUN_DURATION 120
#define BALL_DAMAGE 32
#define DASH_DAMAGE 32
#define SPIN_DAMAGE 24
#define ROCK_DAMAGE 16
#define BOSS_HURT_RADIUS 45
#define BOSS_HURT_HEIGHT 155
#define DASH_WINDUP 18
#define TURN_HIT_RADIUS 115.0f
#define BALL_SWORD_RADIUS 55.0f
#define BALL_LINK_HURT_RADIUS 20.0f
#define BALL_BOSS_HIT_RADIUS 35.0f
#define SPOT_HIT_RADIUS 72.0f
#define PUSH_HIT_RADIUS 220.0f

static const ALIGN_ASSET(2) char sIntroMusic[] = "custom/music/Cucui-Ganon-Intro_bgm";
static const ALIGN_ASSET(2) char sEntryMusic[] = "custom/music/Cucui-Ganon-Entry_bgm";
static const ALIGN_ASSET(2) char sMutedMusic[] = "custom/music/Cucui-Ganon-Loop-Muted_bgm";
static const ALIGN_ASSET(2) char sFullMusic[] = "custom/music/Cucui-Ganon-Loop-Full_bgm";
static const ALIGN_ASSET(2) char sBossTitleTex[] = "__OTR__textures/cucui_ganon/gCucuiGanonTitleCardENGTex";
static const ALIGN_ASSET(2) char sWarpPortalDL[] = "__OTR__objects/object_warp1/gWarpPortalDL";

typedef struct {
    Vec3f root;
    f32 footY;
    Vec3f feet[2];
    Vec3s joints[DANCE_LIMBS];
} GanondorfDancePose;

#include "ganondorf_dance_poses.inc"
#include "ganondorf_boss_poses.inc"
#include "ganondorf_native_base.inc"
#include "ganondorf_grounding.inc"
#include "ganondorf_lyrics.inc"

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate", "OnInterfaceDrawEnd", "OnActorUpdate", "OnActorDrawEnd", "OnActorDestroy", "OnAudioTablesReady",
                                               "OnCollisionResolveDamage" };
static const SOHModRequirements sRequirements = {
    sizeof(SOHModRequirements), sRequiredHooks, ARRAY_COUNT(sRequiredHooks),
};

// Ganon's slot in his own arena; NULL logic keeps the vanilla Ganon condition, which is stricter.
static const SOHBossDefinition sCucuiBoss = {
    sizeof(SOHBossDefinition),
    BOSS_KEY,
    "Cucui Ganon",
    NULL,
    SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_GANON),
    SOH_BOSS_SLOT_GANON,
    NULL,
    DANCE_CVAR,
};

static const SOHModApi* sApi;
static BossGanon2* sBoss;
static BossGanon2ActionFunc sOriginalAction;
static Vec3f sOriginalPosition;
static Vec3s sOriginalRotation;
static f32 sOriginalYOffset;
static Vec3s sInitialJoints[DANCE_LIMBS + 1];
static Vec3f sAnchor;
static uint32_t sFrame;
static bool sPlaying;
static bool sMusicStarted;
static uint32_t sMusicFrame;
static uint32_t sMoveFrame;
static s32 sPreviousMoveFrame = -1;
static uint32_t sDrawMoveFrame;
static uint32_t sMoveCount;
static int sMove;
static int sHealth;
static int sStunTimer;
static int sStunStage;
static int sFinalStage, sFinalTimer;
static s16 sFinalCamera;
static int sLinkStatusTimer, sLinkStatusElement;
static Vec3f sDashDirection;
static f32 sDashRemaining;
static Vec3f sWaveOrigin, sWaveDirection;
static int sWaveTimer, sWaveStep;
static f32 sChargeAmount;
static int sChargeElement;
static bool sIdleCharged;
static int sReflectedHits;
static int sShotSerial;
static Actor* sCrew[PHANTOM_COUNT];
static int sExtraVolleyClock;
static int sStepCount;
static int sPhase;
static ColliderCylinder sBossHurtbox;
static bool sBossHurtboxReady;
static u8 sSavedBodyAC, sSavedBodyOC, sSavedWeaponAC, sSavedWeaponAT, sSavedWeaponOC;
static int sVolleyRemaining, sVolleyTimer, sVolleyHand;
static int sSpotlightChaseAge;
static int sSummonTimer;
static int sPhantomsAlive;
static bool sEnraged;
static s16 sPhantomActorId = -1;
static s16 sWarpActorId = -1;
static u8 sBossNaviHintId = 0xFF;
static int sLyricCurrent = -1;
static uint32_t sLyricStartFrame;
static bool sTitleShown;
static f32 sStableRootY;
static ActorShadowFunc sOriginalShadowDraw;
static f32 sOriginalShadowScale;
static u8 sOriginalShadowAlpha;
static int sProjectileHitCooldown;
static bool sShowcase;
static Vec3s sPreviousJoints[DANCE_LIMBS + 1];
static Vec3f sReturnStart;
static uint32_t sAttackCycle;
static uint32_t sDashElapsed;
static int sDashCooldown;
static PlayState* sLightingPlay;
static f32 sArenaFloorY;
static bool sPostIntroSpin;
static int sStageLightRamp;
static LightInfo sStageLightInfo[2];
static LightNode* sStageLightNodes[2];
static Vtx sSpotlightVtx[2][17];
static bool sSpotlightVtxReady;
static Vtx sDashConeVtx[9];
static Vec3f sSpotlightTarget;
static bool sSpotlightChasing;
static int sSpotlightStunCooldown;
static bool sChaseStrikePending;
static int sSummonedCount;

enum { MOVE_INTRO, MOVE_IDLE, MOVE_IMMUNE, MOVE_DASH, MOVE_SPIN, MOVE_SPIN_REVERSE, MOVE_RETURN_SPIN,
       MOVE_SALSA_2, MOVE_SALSA_1, MOVE_SEXY_TURN, MOVE_BACK, MOVE_FORWARD, MOVE_M5,
       MOVE_CHARGED_TURN, MOVE_OVERHEAD };
enum { ELEMENT_EXPLOSION, ELEMENT_ICE, ELEMENT_FIRE, ELEMENT_ELECTRIC };
static const Color_RGBA8 sElementColors[4] = {{255,130,30,255},{90,200,255,255},{255,35,25,255},{255,245,60,255}};

typedef struct {
    Vec3f pos;
    Vec3f velocity;
    int life;
    int returning;
    int kind;
    int targetPhantom;
    bool fast;
    int element;
    Vec3f start;
    Vec3f target;
    int age;
    int travelFrames;
    f32 arcHeight;
    f32 lateralBend;
    int ownerPhantom;
} StarBall;
static StarBall sStarBalls[32];

typedef struct {
    Vec3f pos, velocity;
    int life, kind;
    float scale;
} DanceRock;
static DanceRock sRocks[12];

static bool MoveBeat(int frame) {
    return sPreviousMoveFrame < frame && sMoveFrame >= (uint32_t)frame;
}

typedef struct {
    Actor actor;
    SkelAnime skelAnime;
    ColliderCylinder collider;
    int index;
    int life;
    int danceMode;
    int danceFrame;
    int contactCooldown;
    Vec3f handPos[2];
} CucuiPhantom;

typedef struct { f32 footY; Vec3s joints[25]; } PhantomDancePose;
#include "PhantomGlanceA_poses.inc"
#include "PhantomGlanceB_poses.inc"
#include "PhantomSalsa_poses.inc"
#include "PhantomSideTurn_poses.inc"
#include "PhantomSpin_poses.inc"
#include "PhantomDash_poses.inc"

typedef struct {
    Actor actor;
    int age;
    bool used;
} CucuiBlueWarp;

static void BlueWarpInit(Actor* actor, PlayState* play) {
    CucuiBlueWarp* warp = (CucuiBlueWarp*)actor;
    warp->age = 0;
    warp->used = false;
    ActorShape_Init(&actor->shape, 0.0f, NULL, 0.0f);
    Actor_SetScale(actor, 1.0f);
    Audio_PlayActorSound2(actor, NA_SE_EV_WARP_HOLE);
}

static void BlueWarpUpdate(Actor* actor, PlayState* play) {
    CucuiBlueWarp* warp = (CucuiBlueWarp*)actor;
    Player* player = GET_PLAYER(play);
    f32 dx = player->actor.world.pos.x - actor->world.pos.x;
    f32 dz = player->actor.world.pos.z - actor->world.pos.z;
    if (play->sceneNum != SCENE_GANON_BOSS || !sPlaying || sFinalStage != 2) {
        Actor_Kill(actor);
        return;
    }
    if (warp->age < 30) warp->age++;
    if (!warp->used && warp->age >= 30 && dx * dx + dz * dz < 64.0f * 64.0f &&
        fabsf(player->actor.world.pos.y - actor->world.pos.y) < 60.0f) {
        warp->used = true;
        Z64Boss_RollCredits(play);
        Audio_PlayActorSound2(actor, NA_SE_EV_LINK_WARP_IN);
    }
}

static void BlueWarpDraw(Actor* actor, PlayState* play) {
    CucuiBlueWarp* warp = (CucuiBlueWarp*)actor;
    u8 alpha = (u8)(warp->age * 8 > 255 ? 255 : warp->age * 8);
    u32 scroll = play->state.frames * 10;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 195, 235, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 105, 255, 255);
    Matrix_Translate(actor->world.pos.x, actor->world.pos.y + 1.0f, actor->world.pos.z, MTXMODE_NEW);
    gSPSegment(POLY_XLU_DISP++, 0x0A, (uintptr_t)MATRIX_NEWMTX(play->state.gfxCtx));
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, scroll & 0xFF, -((s16)(scroll * 2) & 511),
                                0x100, 0x100, 1, scroll & 0xFF, -((s16)(scroll * 2) & 511),
                                0x100, 0x100));
    Matrix_Push();
    Matrix_Translate(0.0f, 15.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)MATRIX_NEWMTX(play->state.gfxCtx));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sWarpPortalDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ConfigureBallArc(StarBall* ball, Vec3f target) {
    f32 dx = target.x - ball->pos.x;
    f32 dz = target.z - ball->pos.z;
    f32 distance = sqrtf(dx * dx + dz * dz);
    ball->fast = (++sShotSerial % 3) == 0;
    ball->element = (int)Rand_ZeroFloat(4.0f);
    ball->targetPhantom = -1;
    ball->start = ball->pos;
    ball->target = target;
    ball->age = 0;
    ball->lateralBend = 0.0f;
    ball->ownerPhantom = -1;
    ball->travelFrames = (int)(distance / (ball->fast ? 13.0f : 9.0f));
    if (ball->travelFrames < 12) ball->travelFrames = 12;
    if (ball->travelFrames > 75) ball->travelFrames = 75;
    ball->arcHeight = 25.0f + distance * 0.055f;
    if (ball->arcHeight > 65.0f) ball->arcHeight = 65.0f;
    ball->life = ball->travelFrames + 7;
}

static Vec3f BallArcPoint(const StarBall* ball, f32 t) {
    f32 dx = ball->target.x - ball->start.x;
    f32 dz = ball->target.z - ball->start.z;
    f32 length = sqrtf(dx * dx + dz * dz);
    f32 lateral = 4.0f * ball->lateralBend * t * (1.0f - t);
    if (length < 1.0f) length = 1.0f;
    return (Vec3f){ ball->start.x + dx * t - dz / length * lateral,
                    ball->start.y + (ball->target.y - ball->start.y) * t +
                        4.0f * ball->arcHeight * t * (1.0f - t),
                    ball->start.z + dz * t + dx / length * lateral };
}

static void KeepInsideArena(Vec3f* pos) {
    f32 x = pos->x - ARENA_CENTER_X;
    f32 z = pos->z - ARENA_CENTER_Z;
    f32 radius;
    if (x > ARENA_HALF_WIDTH) x = ARENA_HALF_WIDTH;
    if (x < -ARENA_HALF_WIDTH) x = -ARENA_HALF_WIDTH;
    if (z > ARENA_HALF_WIDTH) z = ARENA_HALF_WIDTH;
    if (z < -ARENA_HALF_WIDTH) z = -ARENA_HALF_WIDTH;
    radius = sqrtf(x * x + z * z);
    if (radius > ARENA_SAFE_RADIUS) {
        x *= ARENA_SAFE_RADIUS / radius;
        z *= ARENA_SAFE_RADIUS / radius;
    }
    pos->x = ARENA_CENTER_X + x;
    pos->z = ARENA_CENTER_Z + z;
}

static f32 FindArenaFloorY(BossGanon2* boss, PlayState* play) {
    CollisionPoly* floor = NULL;
    s32 bgId;
    Vec3f probe = { ARENA_CENTER_X, 1500.0f, ARENA_CENTER_Z };
    f32 floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &floor, &bgId, &boss->actor, &probe);
    // The native fire ring is drawn at Y=1086. Use that as a safe fallback.
    return floor != NULL && floorY > BGCHECK_Y_MIN && fabsf(floorY - 1086.0f) < 70.0f ? floorY : 1086.0f;
}

static void KeepOnArenaFloor(BossGanon2* boss, PlayState* play, Vec3f previous) {
    CollisionPoly* floor = NULL;
    s32 bgId;
    Vec3f probe = { boss->actor.world.pos.x, sArenaFloorY + 90.0f, boss->actor.world.pos.z };
    f32 floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &floor, &bgId, &boss->actor, &probe);
    if (floor == NULL || floorY <= BGCHECK_Y_MIN || fabsf(floorY - sArenaFloorY) > 55.0f) {
        boss->actor.world.pos.x = previous.x;
        boss->actor.world.pos.z = previous.z;
    } else {
        boss->actor.floorPoly = floor;
        boss->actor.floorHeight = floorY;
    }
}

static void RemoveStageLights(void) {
    int i;
    if (sLightingPlay != NULL && sLightingPlay == gPlayState) {
        for (i = 0; i < 2; i++) {
            if (sStageLightNodes[i] != NULL) {
                LightContext_RemoveLight(sLightingPlay, &sLightingPlay->lightCtx, sStageLightNodes[i]);
            }
        }
    }
    sStageLightNodes[0] = NULL;
    sStageLightNodes[1] = NULL;
    sStageLightRamp = 0;
}

static void RestoreArenaLighting(void) {
    RemoveStageLights();
    sLightingPlay = NULL;
}

static void BrightenArena(PlayState* play) {
    // Keep the arena's own ambient light and fog; only actor-local lights are added.
    sLightingPlay = play;
}

static void UpdateStageLights(BossGanon2* boss, PlayState* play) {
    f32 x = sSpotlightChasing ? sSpotlightTarget.x : boss->actor.world.pos.x;
    f32 y = sSpotlightChasing ? sArenaFloorY : boss->actor.world.pos.y;
    f32 z = sSpotlightChasing ? sSpotlightTarget.z : boss->actor.world.pos.z;
    s16 yaw = boss->actor.shape.rot.y;
    f32 sx = Math_SinS(yaw);
    f32 cz = Math_CosS(yaw);
    f32 fade = (f32)sStageLightRamp / 20.0f;

    if (sStageLightRamp == 0) return;
    if (sStageLightNodes[0] == NULL) {
        Lights_PointNoGlowSetInfo(&sStageLightInfo[0], x, y + 120.0f, z, 255, 217, 170, 185);
        sStageLightNodes[0] = LightContext_InsertLight(play, &play->lightCtx, &sStageLightInfo[0]);
    }
    if (sStageLightNodes[1] == NULL) {
        Lights_PointNoGlowSetInfo(&sStageLightInfo[1], x, y + 120.0f, z, 172, 175, 255, 185);
        sStageLightNodes[1] = LightContext_InsertLight(play, &play->lightCtx, &sStageLightInfo[1]);
    }
    if (fade > 1.0f) fade = 1.0f;
    // Narrow sources illuminate the model, not the entire battle arena.
    Lights_PointNoGlowSetInfo(&sStageLightInfo[0], x - 48.0f * cz + 22.0f * sx,
                              y + 115.0f, z + 48.0f * sx + 22.0f * cz,
                              (u8)(255 * fade), (u8)(222 * fade), (u8)(186 * fade), 185);
    Lights_PointNoGlowSetInfo(&sStageLightInfo[1], x + 48.0f * cz - 22.0f * sx,
                              y + 115.0f, z - 48.0f * sx - 22.0f * cz,
                              (u8)(180 * fade), (u8)(185 * fade), (u8)(255 * fade), 185);
}

static void InitSpotlightVtx(void) {
    static const u8 colors[2][3] = { { 255, 225, 190 }, { 190, 200, 255 } };
    int cone, ring, side;
    for (cone = 0; cone < 2; cone++) {
        f32 sign = cone == 0 ? -1.0f : 1.0f;
        Vtx* v = sSpotlightVtx[cone];
        v[0].v.ob[0] = (s16)(sign * 125.0f);
        v[0].v.ob[1] = 330;
        v[0].v.ob[2] = (s16)(sign * 65.0f);
        v[0].v.cn[0] = colors[cone][0];
        v[0].v.cn[1] = colors[cone][1];
        v[0].v.cn[2] = colors[cone][2];
        v[0].v.cn[3] = 8;
        for (ring = 0; ring < 2; ring++) {
            f32 centerX = ring == 0 ? sign * 62.5f : 0.0f;
            f32 centerZ = ring == 0 ? sign * 32.5f : 0.0f;
            f32 radius = ring == 0 ? 43.0f : 86.0f;
            for (side = 0; side < 8; side++) {
                int index = 1 + ring * 8 + side;
                f32 angle = side * ((f32)M_PI / 4.0f);
                v[index].v.ob[0] = (s16)(centerX + cosf(angle) * radius);
                v[index].v.ob[1] = ring == 0 ? 165 : 2;
                v[index].v.ob[2] = (s16)(centerZ + sinf(angle) * radius);
                v[index].v.cn[0] = colors[cone][0];
                v[index].v.cn[1] = colors[cone][1];
                v[index].v.cn[2] = colors[cone][2];
                v[index].v.cn[3] = ring == 0 ? 28 : 4;
            }
        }
    }
    sDashConeVtx[0].v.ob[0] = 0;
    sDashConeVtx[0].v.ob[1] = 95;
    sDashConeVtx[0].v.ob[2] = 185;
    sDashConeVtx[0].v.cn[0] = 205;
    sDashConeVtx[0].v.cn[1] = 220;
    sDashConeVtx[0].v.cn[2] = 255;
    sDashConeVtx[0].v.cn[3] = 170;
    for (side = 0; side < 8; side++) {
        f32 angle = side * ((f32)M_PI / 4.0f);
        Vtx* v = &sDashConeVtx[side + 1];
        v->v.ob[0] = (s16)(cosf(angle) * 85.0f);
        v->v.ob[1] = (s16)(90.0f + sinf(angle) * 65.0f);
        v->v.ob[2] = 25;
        v->v.cn[0] = 160;
        v->v.cn[1] = 195;
        v->v.cn[2] = 255;
        v->v.cn[3] = 85;
    }
    sSpotlightVtxReady = true;
}

// Evaluate the drawn OoT hierarchy, including root translation, for hands and feet.
static void EvaluateGanondorfBones(BossGanon2* boss, MtxF bones[25]) {
    int i;
    Matrix_Push();
    for (i = 0; i < 25; i++) {
        Vec3s a = boss->skelAnime.jointTable[i+1];
        if (sGroundParents[i] < 0) {
            Vec3s root = boss->skelAnime.jointTable[0];
            Matrix_Translate(root.x, root.y + boss->actor.shape.yOffset, root.z, MTXMODE_NEW);
        } else {
            Vec3f p = sGroundOffsets[i];
            Matrix_Put(&bones[sGroundParents[i]]);
            Matrix_Translate(p.x,p.y,p.z,MTXMODE_APPLY);
        }
        Matrix_RotateZYX(a.x,a.y,a.z,MTXMODE_APPLY);
        Matrix_Get(&bones[i]);
    }
    Matrix_Pop();
}

static Vec3f PoseLimbWorldPoint(BossGanon2* boss, int limb, Vec3f local) {
    MtxF bones[25];
    Vec3f posed;
    f32 x, z;
    EvaluateGanondorfBones(boss, bones);
    Matrix_Push();
    Matrix_Put(&bones[limb]);
    Matrix_MultVec3f(&local, &posed);
    Matrix_Pop();
    x = posed.x * boss->actor.scale.x;
    z = posed.z * boss->actor.scale.z;
    return (Vec3f){ boss->actor.world.pos.x + x * Math_CosS(boss->actor.shape.rot.y) +
                    z * Math_SinS(boss->actor.shape.rot.y),
                    boss->actor.world.pos.y + posed.y * boss->actor.scale.y,
                    boss->actor.world.pos.z - x * Math_SinS(boss->actor.shape.rot.y) +
                    z * Math_CosS(boss->actor.shape.rot.y) };
}

static f32 PoseSoleHeight(BossGanon2* boss) {
    MtxF bones[25];
    f32 lowest = 100000;
    int foot, j;
    EvaluateGanondorfBones(boss, bones);
    Matrix_Push();
    for (foot = 0; foot < 2; foot++) {
        int limb = foot == 0 ? 21 : 24;
        const Vec3f* verts = foot == 0 ? sFootVertices21 : sFootVertices24;
        int count = foot == 0 ? ARRAY_COUNT(sFootVertices21) : ARRAY_COUNT(sFootVertices24);
        Matrix_Put(&bones[limb]);
        for (j = 0; j < count; j++) {
            Vec3f result;
            Matrix_MultVec3f((Vec3f*)&verts[j], &result);
            if (result.y < lowest) lowest = result.y;
        }
    }
    Matrix_Pop();
    return lowest * boss->actor.scale.y;
}

static void ApplyLinkElement(PlayState* play, int element, Vec3f pos) {
    Player* player = GET_PLAYER(play);
    Vec3f zero = {0};
    int i;
    if (element == ELEMENT_EXPLOSION) {
        EffectSsBomb2_SpawnLayered(play,&pos,&zero,&zero,100,25);
        EffectSsBlast_SpawnWhiteShockwave(play,&pos,&zero,&zero);
        Audio_PlayActorSound2(&player->actor, NA_SE_IT_BOMB_EXPLOSION);
    } else if (element == ELEMENT_FIRE) {
        for (i = 0; i < PLAYER_BODYPART_MAX; i++) player->bodyFlameTimers[i] = 50 + i*2;
        player->bodyIsBurning = true;
        EffectSsGFire_Spawn(play,&pos);
    } else {
        sLinkStatusElement = element;
        sLinkStatusTimer = element == ELEMENT_ICE ? 40 : 30;
        player->actor.freezeTimer = 2;
        if (element == ELEMENT_ICE)
            EffectSsEnIce_SpawnFlyingVec3f(play,&player->actor,&pos,180,230,255,255,50,100,255,1.2f);
        else player->bodyShockTimer = 30;
    }
}

static void UpdateLinkStatus(PlayState* play) {
    Player* player = GET_PLAYER(play);
    if (sLinkStatusTimer <= 0) return;
    player->actor.freezeTimer = 2;
    if (sLinkStatusElement == ELEMENT_ELECTRIC) player->bodyShockTimer = 3;
    if (--sLinkStatusTimer == 0) {
        player->actor.freezeTimer = 0;
        if (sLinkStatusElement == ELEMENT_ICE) EffectSsIcePiece_SpawnBurst(play,&player->actor.world.pos,0.01f);
    }
}

static void SpawnStepRocks(BossGanon2* boss, PlayState* play) {
    int i, spawned = 0;
    Vec3f foot = boss->actor.world.pos;
    foot.y = sArenaFloorY + 8;
    func_80033480(play, &foot, 45, 5, 160, 15, 1);
    Audio_PlayActorSound2(&boss->actor, NA_SE_EN_VALVAISA_LAND);
    for (i = 0; i < ARRAY_COUNT(sRocks) && spawned < 3; i++) {
        DanceRock* rock = &sRocks[i];
        float angle;
        if (rock->life > 0) continue;
        angle = BINANG_TO_RAD(boss->actor.shape.rot.y) + spawned * (2 * M_PI / 3) + sStepCount * 0.7f;
        rock->pos = foot;
        rock->pos.x += sinf(angle) * 48;
        rock->pos.z += cosf(angle) * 48;
        rock->velocity = (Vec3f){sinf(angle) * 4, 12 + spawned * 2, cosf(angle) * 4};
        rock->kind = (sStepCount + spawned) & 1;
        rock->scale = rock->kind ? 0.025f : 0.008f;
        rock->life = 60;
        spawned++;
    }
    sStepCount++;
}

static void UpdateRockWave(BossGanon2* boss, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f zero = {0};
    Vec3f pos;
    int i;
    if (sWaveTimer <= 0) return;
    if (--sWaveTimer % 4 != 0) return;
    sWaveStep++;
    pos = (Vec3f){sWaveOrigin.x+sWaveDirection.x*sWaveStep*65,
                  sArenaFloorY+8,sWaveOrigin.z+sWaveDirection.z*sWaveStep*65};
    KeepInsideArena(&pos);
    EffectSsBlast_SpawnWhiteShockwave(play,&pos,&zero,&zero);
    EffectSsBomb2_SpawnLayered(play,&pos,&zero,&zero,70,15);
    func_80033480(play,&pos,50,5,140,10,1);
    Audio_PlayActorSound2(&boss->actor,NA_SE_IT_BOMB_EXPLOSION);
    for (i = 0; i < ARRAY_COUNT(sRocks); i++) {
        if (sRocks[i].life > 0) continue;
        sRocks[i] = (DanceRock){pos,{sWaveDirection.x*2,8,sWaveDirection.z*2},35,sWaveStep&1,
                                (sWaveStep&1)?0.025f:0.008f};
        break;
    }
    if (sProjectileHitCooldown == 0) {
        float dx=player->actor.world.pos.x-pos.x, dz=player->actor.world.pos.z-pos.z;
        if (dx*dx+dz*dz < 65*65 && fabsf(player->actor.world.pos.y-sArenaFloorY)<80) {
            Actor_SetPlayerKnockbackLargeNoDamage(play,&boss->actor,12,boss->actor.shape.rot.y,6);
            play->damagePlayer(play,-BALL_DAMAGE);
            sProjectileHitCooldown=25;
        }
    }
}

static void UpdateDanceRocks(BossGanon2* boss, PlayState* play) {
    Player* player = GET_PLAYER(play);
    int i;
    for (i = 0; i < ARRAY_COUNT(sRocks); i++) {
        DanceRock* rock = &sRocks[i];
        float dx, dy, dz;
        if (rock->life <= 0) continue;
        rock->life--;
        rock->pos.x += rock->velocity.x;
        rock->pos.y += rock->velocity.y;
        rock->pos.z += rock->velocity.z;
        rock->velocity.y -= 0.85f;
        KeepInsideArena(&rock->pos);
        dx = player->actor.world.pos.x - rock->pos.x;
        dy = player->actor.world.pos.y + 35 - rock->pos.y;
        dz = player->actor.world.pos.z - rock->pos.z;
        if (sProjectileHitCooldown == 0 && dx*dx + dz*dz < 45*45 && fabsf(dy) < 60) {
            Actor_SetPlayerKnockbackLargeNoDamage(play, &boss->actor, 8, (s16)(atan2f(dx, dz) * (0x8000 / M_PI)), 5);
            play->damagePlayer(play, -ROCK_DAMAGE);
            sProjectileHitCooldown = 25;
            rock->life = 0;
        }
        if (rock->pos.y <= sArenaFloorY + 12 || rock->life == 0) {
            rock->pos.y = sArenaFloorY + 12;
            func_80033480(play, &rock->pos, 30, 4, 120, 10, 1);
            rock->life = 0;
        }
    }
}

static void DrawDanceRocks(PlayState* play) {
    int i;
    OPEN_DISPS(play->state.gfxCtx);
    for (i = 0; i < ARRAY_COUNT(sRocks); i++) {
        DanceRock* rock = &sRocks[i];
        if (rock->life <= 0) continue;
        Matrix_Push();
        Matrix_Translate(rock->pos.x, rock->pos.y, rock->pos.z, MTXMODE_NEW);
        Matrix_RotateX(rock->life * 0.19f, MTXMODE_APPLY);
        Matrix_RotateY(rock->life * 0.13f, MTXMODE_APPLY);
        Matrix_Scale(rock->scale, rock->scale, rock->scale, MTXMODE_APPLY);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 155, 55, 255);
        gDPSetEnvColor(POLY_OPA_DISP++, 155, 255, 55, 255);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)(rock->kind ? object_efc_star_field_DL_000DE0 : gVolvagiaRockDL));
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawSpotlights(Actor* actor, PlayState* play) {
    int cone, side, i;
    f32 fade;
    if (!sPlaying || actor != &sBoss->actor || sStageLightRamp == 0 || play != sLightingPlay) return;
    if (!sSpotlightVtxReady) InitSpotlightVtx();
    fade = (f32)sStageLightRamp / 20.0f;
    if (fade > 1.0f) fade = 1.0f;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BOTH | G_FOG);
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, SHADE, SHADE, 0, PRIMITIVE, 0,
                      0, 0, 0, SHADE, SHADE, 0, PRIMITIVE, 0);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (u8)(255 * fade));
    Matrix_Push();
    Matrix_Translate(sSpotlightTarget.x, sArenaFloorY, sSpotlightTarget.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(actor->shape.rot.y), MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    for (cone = 0; cone < 2; cone++) {
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)sSpotlightVtx[cone], 17, 0);
        for (side = 0; side < 8; side++) {
            int next = (side + 1) & 7;
            gSP2Triangles(POLY_XLU_DISP++, 0, 1 + side, 1 + next, 0,
                          1 + side, 9 + side, 9 + next, 0);
            gSP1Triangle(POLY_XLU_DISP++, 1 + side, 9 + next, 1 + next, 0);
        }
    }
    if (sStunTimer == 0 && sMove == MOVE_DASH && sMoveFrame >= DASH_WINDUP) {
        Matrix_Translate(actor->world.pos.x, sArenaFloorY, actor->world.pos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(actor->shape.rot.y), MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)sDashConeVtx, 9, 0);
        for (side = 0; side < 8; side++) {
            gSP1Triangle(POLY_XLU_DISP++, 0, 1 + side, 1 + ((side + 1) & 7), 0);
        }
    }
    Matrix_Pop();
    for (i = 0; i < ARRAY_COUNT(sStarBalls); i++) {
        StarBall* ball = &sStarBalls[i];
        f32 scale = (play->gameplayFrames & 1) ? 6.0f : 5.25f;
        if (ball->life <= 0) continue;
        Matrix_Push();
        Matrix_Translate(ball->pos.x, ball->pos.y, ball->pos.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        Matrix_RotateZ(play->gameplayFrames * 1.1f + i, MTXMODE_APPLY);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
        if (ball->returning) gDPSetEnvColor(POLY_XLU_DISP++, 0, 255, 255, 0);
        else { Color_RGBA8 color = sElementColors[ball->element]; gDPSetEnvColor(POLY_XLU_DISP++, color.r, color.g, color.b, 0); }
        gDPPipeSync(POLY_XLU_DISP++);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gPhantomEnergyBallDL);
        Matrix_Pop();
    }
    if (sChargeAmount > 0 && sStunTimer == 0 && !sShowcase) {
        Vec3f hand = PoseLimbWorldPoint((BossGanon2*)actor, sMove == MOVE_OVERHEAD ? 5 : 9,
                                        (Vec3f){500,0,0});
        Color_RGBA8 color = sElementColors[sChargeElement];
        Matrix_Push();
        Matrix_Translate(hand.x, hand.y, hand.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(2+sChargeAmount*7,2+sChargeAmount*7,2+sChargeAmount*7,MTXMODE_APPLY);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++,0,0,255,255,255,255);
        gDPSetEnvColor(POLY_XLU_DISP++,color.r,color.g,color.b,0);
        gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_LOAD|G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gPhantomEnergyBallDL);
        Matrix_Pop();
    }
    gSPSetGeometryMode(POLY_XLU_DISP++, G_FOG);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawDanceRocks(play);
}

static void RegisterMusic(void) {
    const char* paths[] = { sIntroMusic, sEntryMusic, sMutedMusic, sFullMusic };
    int i;

    if (!SOH_MOD_API_HAS(sApi, RegisterSequence) || !SOH_MOD_API_HAS(sApi, GetSequenceId)) {
        return;
    }
    for (i = 0; i < ARRAY_COUNT(paths); i++) {
        if (sApi->GetSequenceId(paths[i]) == 0) {
            sApi->RegisterSequence(paths[i]);
        }
    }
}

static void PlayMusic(const char* path) {
    uint16_t id;

    if (!SOH_MOD_API_HAS(sApi, GetSequenceId)) {
        return;
    }
    id = sApi->GetSequenceId(path);
    if (id != 0) {
        Audio_QueueSeqCmd(((u32)SEQ_PLAYER_BGM_MAIN << 24) | id);
    }
}

static s16 LerpAngle(s16 from, s16 to, f32 t) {
    return (s16)(from + (s16)(to - from) * t);
}

static f32 RootYForFootContacts(BossGanon2* boss, const GanondorfDancePose* pose) {
    f32 sx = Math_SinS(boss->actor.shape.rot.y);
    f32 cz = Math_CosS(boss->actor.shape.rot.y);
    f32 targetY = sArenaFloorY - pose->footY;
    bool found = false;
    int i;

    for (i = 0; i < 2; i++) {
        const Vec3f* foot = &pose->feet[i];
        CollisionPoly* floor = NULL;
        s32 bgId;
        Vec3f probe = {
            boss->actor.world.pos.x + foot->x * cz + foot->z * sx,
            sArenaFloorY + 120.0f,
            boss->actor.world.pos.z - foot->x * sx + foot->z * cz,
        };
        f32 floorY = BgCheck_EntityRaycastFloor5(gPlayState, &gPlayState->colCtx, &floor, &bgId,
                                                  &boss->actor, &probe);
        if (floor != NULL && floorY > BGCHECK_Y_MIN && fabsf(floorY - sArenaFloorY) < 70.0f) {
            f32 candidate = floorY - foot->y;
            if (!found || candidate > targetY) targetY = candidate;
            found = true;
        }
    }
    return targetY;
}

static void ReleaseIntroCamera(BossGanon2* boss, PlayState* play) {
    Camera* mainCamera;

    if (boss->subCamId == 0) {
        return;
    }
    mainCamera = Play_GetCamera(play, CAM_ID_MAIN);
    mainCamera->eye = boss->subCamEye;
    mainCamera->eyeNext = boss->subCamEye;
    mainCamera->at = boss->subCamAt;
    func_800C08AC(play, boss->subCamId, 0);
    boss->subCamId = 0;
    func_80064534(play, &play->csCtx);
    Player_SetCsActionWithHaltedActors(play, &boss->actor, 7);
}

static void AimShowcaseCamera(BossGanon2* boss, PlayState* play) {
    Vec3f at;
    Vec3f eye;

    if (boss->subCamId == 0) {
        return;
    }
    at = boss->actor.world.pos;
    at.y = sAnchor.y + 100;
    eye = sAnchor;
    eye.y += 150.0f;
    eye.z += 370.0f;
    boss->subCamAt = at;
    boss->subCamEye = eye;
    Play_CameraSetAtEye(play, boss->subCamId, &at, &eye);
    Play_CameraSetFov(play, boss->subCamId, 60.0f);
}

static void ApplyPose(BossGanon2* boss, const GanondorfDancePose* poses, uint32_t frame, bool useRoot) {
    const GanondorfDancePose* pose = &poses[frame];
    s16 yaw = boss->actor.shape.rot.y;
    f32 targetY;
    int i;

    if (boss->skelAnime.jointTable == NULL) {
        return;
    }
    boss->skelAnime.jointTable[0] = sNeutralJoints[0];
    for (i = 0; i < DANCE_LIMBS; i++) {
        const Vec3s* now = &pose->joints[i];
        if (sMoveFrame < 6 && sFrame >= DESCENT_FRAMES) {
            f32 t = (f32)(sMoveFrame + 1) / 6.0f;
            boss->skelAnime.jointTable[i + 1].x = LerpAngle(sPreviousJoints[i + 1].x, now->x, t);
            boss->skelAnime.jointTable[i + 1].y = LerpAngle(sPreviousJoints[i + 1].y, now->y, t);
            boss->skelAnime.jointTable[i + 1].z = LerpAngle(sPreviousJoints[i + 1].z, now->z, t);
        } else {
            boss->skelAnime.jointTable[i + 1] = *now;
        }
    }
    boss->actor.world.rot.y = yaw;
    if (useRoot) {
        f32 x = pose->root.x - poses[0].root.x;
        f32 z = pose->root.z - poses[0].root.z;
        boss->actor.world.pos.x = sAnchor.x + x * Math_CosS(yaw) + z * Math_SinS(yaw);
        boss->actor.world.pos.z = sAnchor.z - x * Math_SinS(yaw) + z * Math_CosS(yaw);
    }
    targetY = RootYForFootContacts(boss, pose);
    // Small floor corrections keep a support foot grounded without shifting
    // the whole skeleton to a different height at each action boundary.
    if (targetY > sStableRootY + 7.0f) targetY = sStableRootY + 7.0f;
    if (targetY < sStableRootY - 7.0f) targetY = sStableRootY - 7.0f;
    if (targetY > boss->actor.world.pos.y + 1.75f) targetY = boss->actor.world.pos.y + 1.75f;
    if (targetY < boss->actor.world.pos.y - 1.75f) targetY = boss->actor.world.pos.y - 1.75f;
    if (sMove == MOVE_INTRO) targetY = sArenaFloorY - PoseSoleHeight(boss);
    boss->actor.world.pos.y = targetY;
    boss->actor.velocity.x = 0.0f;
    boss->actor.velocity.y = 0.0f;
    boss->actor.velocity.z = 0.0f;
    boss->actor.speedXZ = 0.0f;
    boss->actor.gravity = 0.0f;
}

static const GanondorfDancePose* MovePoses(int move, int* first, int* count) {
    *first = 0;
    switch (move) {
        case MOVE_INTRO: *count = ARRAY_COUNT(sBossIntroPoses); return sBossIntroPoses;
        case MOVE_IDLE: *count = 60; return sBossIdlePoses;
        case MOVE_OVERHEAD: *first = 60; *count = 60; return sBossIdlePoses;
        case MOVE_CHARGED_TURN: *count = ARRAY_COUNT(sBossSpinPoses); return sBossSpinPoses;
        case MOVE_IMMUNE: *count = ARRAY_COUNT(sBossImmunePoses); return sBossImmunePoses;
        case MOVE_DASH: *count = ARRAY_COUNT(sBossDashPoses); return sBossDashPoses;
        case MOVE_SPIN: *count = ARRAY_COUNT(sBossSpinPoses); return sBossSpinPoses;
        case MOVE_SPIN_REVERSE: *count = ARRAY_COUNT(sBossSpinReversePoses); return sBossSpinReversePoses;
        case MOVE_RETURN_SPIN: *count = ARRAY_COUNT(sBossSpinPoses); return sBossSpinPoses;
        case MOVE_SALSA_2: *count = ARRAY_COUNT(sBossSalsa2Poses); return sBossSalsa2Poses;
        case MOVE_SALSA_1: *count = ARRAY_COUNT(sBossSalsa1Poses); return sBossSalsa1Poses;
        case MOVE_SEXY_TURN: *first = 340; *count = 80; return sDancePoses;
        case MOVE_BACK: *first = 480; *count = 60; return sDancePoses;
        case MOVE_FORWARD: *first = 560; *count = 80; return sDancePoses;
        default: *count = ARRAY_COUNT(sDancePoses); return sDancePoses;
    }
}

static void BeginMove(BossGanon2* boss, int move) {
    int i;
    for (i = 0; i <= DANCE_LIMBS; i++) {
        sPreviousJoints[i] = boss->skelAnime.jointTable[i];
    }
    sAnchor = boss->actor.world.pos;
    sAnchor.y = sArenaFloorY;
    if (move == MOVE_RETURN_SPIN) sReturnStart = boss->actor.world.pos;
    sChargeAmount = 0;
    if (move == MOVE_IDLE) sChargeElement=(int)Rand_ZeroFloat(4);
    if (move == MOVE_OVERHEAD) sChargeElement=ELEMENT_FIRE;
    sMove = move;
    sMoveFrame = 0;
    sPreviousMoveFrame = -1;
    sDrawMoveFrame = 0;
    if (move == MOVE_DASH) sDashElapsed = 0;
    sMoveCount++;
}

// Phase two remains sealed until all four crew members have been defeated.
static bool CrewDefeated(void) {
    return sSummonedCount == PHANTOM_COUNT && sPhantomsAlive == 0;
}

static void BeginPhaseTwo(BossGanon2* boss) {
    sPhase = 2;
    sReflectedHits = 0;
    sSummonTimer = 80;
    sSummonedCount = 0;
    sVolleyRemaining = 0;
    BeginMove(boss, MOVE_RETURN_SPIN);
}

static bool DamageCucui(BossGanon2* boss, bool returnedBall, bool rearArrow) {
    bool chargedHit = returnedBall || (rearArrow && sPhase == 1);
    bool opensStun = false;
    if (sPhase == 2 && (!CrewDefeated() || (!chargedHit && sStunTimer == 0))) return false;
    if (chargedHit) {
        opensStun = ++sReflectedHits >= (sPhase == 2 ? 1 : 3);
        if (opensStun) sReflectedHits = 0;
        if (returnedBall && !opensStun) {
            Audio_PlayActorSound2(&boss->actor, NA_SE_EN_GANON_HIT_THUNDER);
            return true;
        }
    }
    sHealth -= returnedBall ? RETURN_DAMAGE : BACK_DAMAGE;
    if (sHealth < 1) sHealth = 1; // Final defeat sequence remains pending.
    Actor_SetColorFilter(&boss->actor, 0x4000, 255, 0, 12);
    Audio_PlayActorSound2(&boss->actor, returnedBall ? NA_SE_EN_GANON_DAMAGE2 : NA_SE_EN_GANON_DAMAGE1);
    if (opensStun) {
        int i;
        // Give Link a clear approach after the third return; remove lingering hazards.
        for (i = 0; i < ARRAY_COUNT(sStarBalls); i++) sStarBalls[i].life = 0;
        for (i = 0; i < ARRAY_COUNT(sRocks); i++) sRocks[i].life = 0;
        sStunTimer = STUN_DURATION;
        sStunStage = 0;
        sWaveTimer = 0;
        sChargeAmount = 0;
        sIdleCharged = false;
        sLinkStatusTimer = 0;
        GET_PLAYER(gPlayState)->actor.freezeTimer = 0;
        sVolleyRemaining = 0;
        sSpotlightChasing = false;
        sChaseStrikePending = false;
        Animation_MorphToPlayOnce(&boss->skelAnime, (AnimationHeader*)gGanondorfLightEnergyHitAnim, -3.0f);
        Audio_PlayActorSound2(&boss->actor, NA_SE_EN_GANON_HIT_THUNDER);
    }
    // A phase change must not consume the attack window earned by the third return.
    if (sHealth <= BOSS_PHASE2_HEALTH && sPhase == 1 && sStunTimer == 0) BeginPhaseTwo(boss);
    return true;
}

static ColliderCylinderInit sBossHurtboxInit = {
    { COLTYPE_HIT5, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0, 0, 0 }, { 0xFFCFFFFF, 0, 0 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
    { BOSS_HURT_RADIUS, BOSS_HURT_HEIGHT, 0, { 0, 0, 0 } },
};

static void StartBossHurtbox(BossGanon2* boss, PlayState* play) {
    sSavedBodyAC = boss->unk_424.base.acFlags;
    sSavedBodyOC = boss->unk_424.base.ocFlags1;
    sSavedWeaponAC = boss->unk_444.base.acFlags;
    sSavedWeaponAT = boss->unk_444.base.atFlags;
    sSavedWeaponOC = boss->unk_444.base.ocFlags1;
    Collider_InitCylinder(play, &sBossHurtbox);
    Collider_SetCylinder(play, &sBossHurtbox, &boss->actor, &sBossHurtboxInit);
    sBossHurtboxReady = true;
}

static void StopBossHurtbox(BossGanon2* boss, PlayState* play) {
    if (!sBossHurtboxReady) return;
    Collider_DestroyCylinder(play, &sBossHurtbox);
    sBossHurtboxReady = false;
    boss->unk_424.base.acFlags = sSavedBodyAC;
    boss->unk_424.base.ocFlags1 = sSavedBodyOC;
    boss->unk_444.base.acFlags = sSavedWeaponAC;
    boss->unk_444.base.atFlags = sSavedWeaponAT;
    boss->unk_444.base.ocFlags1 = sSavedWeaponOC;
}

static void UpdateBossHurtbox(BossGanon2* boss, PlayState* play) {
    boss->unk_424.base.acFlags &= ~(AC_ON | AC_HIT);
    boss->unk_424.base.ocFlags1 &= ~OC1_ON;
    boss->unk_444.base.acFlags &= ~(AC_ON | AC_HIT);
    boss->unk_444.base.atFlags &= ~AT_ON;
    boss->unk_444.base.ocFlags1 &= ~OC1_ON;
    if (!sBossHurtboxReady || sShowcase || sStageLightRamp == 0 || sPostIntroSpin || sMove == MOVE_INTRO) return;
    Collider_UpdateCylinder(&boss->actor, &sBossHurtbox);
    sBossHurtbox.dim.pos.y = (s16)sArenaFloorY;
    CollisionCheck_SetAC(play, &play->colChkCtx, &sBossHurtbox.base);
}

static ColliderCylinderInit sPhantomColliderInit = {
    { COLTYPE_HIT5, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_PLAYER,
      OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0, 0, 0 }, { 0xFFCFFFFF, 0, 0 },
      TOUCH_NONE, BUMP_ON, OCELEM_ON },
    { 32, 122, 0, { 0, 0, 0 } },
};

static void PhantomInit(Actor* actor, PlayState* play) {
    CucuiPhantom* phantom = (CucuiPhantom*)actor;
    phantom->index = actor->params;
    sCrew[phantom->index] = actor;
    phantom->danceMode = -1;
    phantom->handPos[0] = actor->world.pos;
    phantom->handPos[0].y = sArenaFloorY + 85;
    phantom->handPos[1] = phantom->handPos[0];
    phantom->life = 1;
    ActorShape_Init(&actor->shape, 0.0f, NULL, 0.0f);
    Actor_SetScale(actor, 0.008f);
    actor->targetMode = 3;
    Collider_InitCylinder(play, &phantom->collider);
    Collider_SetCylinder(play, &phantom->collider, actor, &sPhantomColliderInit);
    SkelAnime_Init(play, &phantom->skelAnime, (SkeletonHeader*)gPhantomGanonSkel,
                   (AnimationHeader*)gPhantomGanonNeutralAnim, NULL, NULL, 0);
    sPhantomsAlive++;
}

static void PhantomDestroy(Actor* actor, PlayState* play) {
    CucuiPhantom* phantom = (CucuiPhantom*)actor;
    if (sCrew[phantom->index] == actor) sCrew[phantom->index] = NULL;
    Collider_DestroyCylinder(play, &phantom->collider);
    SkelAnime_Free(&phantom->skelAnime, play);
    if (phantom->life > 0 && sPhantomsAlive > 0) sPhantomsAlive--;
}

static void SpawnPhantomBall(CucuiPhantom* phantom, Player* player, int hand) {
    int i;
    for (i = 8; i < ARRAY_COUNT(sStarBalls); i++) {
        StarBall* ball = &sStarBalls[i];
        Vec3f target;
        if (ball->life > 0) continue;
        ball->pos = phantom->handPos[hand];
        ball->returning = 0;
        ball->kind = 1;
        target = player->actor.world.pos;
        target.y = sArenaFloorY + 22.0f;
        ConfigureBallArc(ball, target);
        ball->arcHeight = 0.0f;
        ball->lateralBend = hand == 0 ? -95.0f : 95.0f;
        ball->ownerPhantom = phantom->index;
        break;
    }
}

static void PhantomUpdate(Actor* actor, PlayState* play) {
    CucuiPhantom* phantom = (CucuiPhantom*)actor;
    Player* player = GET_PLAYER(play);
    f32 dx = player->actor.world.pos.x - actor->world.pos.x;
    f32 dz = player->actor.world.pos.z - actor->world.pos.z;
    if (!sPlaying || sPhase != 2 || sBoss == NULL || sBoss->actor.update == NULL) {
        Actor_Kill(actor);
        return;
    }
    if (phantom->collider.base.acFlags & AC_HIT) {
        phantom->collider.base.acFlags &= ~AC_HIT;
        if (phantom->life > 0) {
            const ColliderInfo* hit = phantom->collider.info.acHitInfo;
            bool explosive = hit != NULL && (hit->toucher.dmgFlags & DMG_EXPLOSIVE) != 0;
            f32 hx = player->actor.world.pos.x - actor->world.pos.x;
            f32 hz = player->actor.world.pos.z - actor->world.pos.z;
            s16 angle = (s16)((s16)(atan2f(hx,hz)*(0x8000/M_PI)) - actor->shape.rot.y);
            if (explosive || ABS((s32)angle) >= 0x4000) {
                phantom->life = 0;
                if (sPhantomsAlive > 0) sPhantomsAlive--;
                EffectSsKiraKira_SpawnDispersed(play, &actor->world.pos, (Vec3f[]){ {0, 1, 0} },
                                                (Vec3f[]){ {0, 0, 0} },
                                                (Color_RGBA8[]){ {190, 175, 255, 255} },
                                                (Color_RGBA8[]){ {80, 30, 185, 255} }, 500, 24);
                Actor_Kill(actor);
                return;
            }
            EffectSsFhgFlash_SpawnShock(play,NULL,&actor->focus.pos,110,0);
        }
    }
    // Streams retargeted from the actual Blender preview onto object_gnd, not human joint indices.
    {
        const PhantomDancePose* poses;
        int mode = (play->gameplayFrames / 110 + phantom->index) % 6;
        int count, frame, j;
        float blend;
        if (mode == 0) { poses = sPhantomSalsaPoses; count = ARRAY_COUNT(sPhantomSalsaPoses); }
        else if (mode == 1) { poses = sPhantomSideTurnPoses; count = ARRAY_COUNT(sPhantomSideTurnPoses); }
        else if (mode == 2) { poses = sPhantomSpinPoses; count = ARRAY_COUNT(sPhantomSpinPoses); }
        else if (mode == 3) { poses = sPhantomDashPoses; count = ARRAY_COUNT(sPhantomDashPoses); }
        else if (mode == 4) { poses = sPhantomGlanceAPoses; count = ARRAY_COUNT(sPhantomGlanceAPoses); }
        else { poses = sPhantomGlanceBPoses; count = ARRAY_COUNT(sPhantomGlanceBPoses); }
        if (mode != phantom->danceMode) { phantom->danceMode = mode; phantom->danceFrame = 0; }
        frame = phantom->danceFrame++ % count;
        blend = frame < 6 ? 0.25f : 1.0f;
        phantom->skelAnime.jointTable[0] = (Vec3s){0};
        for (j = 0; j < 25; j++) {
            Vec3s goal = poses[frame].joints[j];
            Vec3s* joint = &phantom->skelAnime.jointTable[j+1];
            joint->x = LerpAngle(joint->x, goal.x, blend);
            joint->y = LerpAngle(joint->y, goal.y, blend);
            joint->z = LerpAngle(joint->z, goal.z, blend);
        }
        actor->world.pos.y += (sArenaFloorY - poses[frame].footY * 0.8f - actor->world.pos.y) * 0.4f;
        if (mode <= 3) {
            dx = player->actor.world.pos.x - actor->world.pos.x;
            dz = player->actor.world.pos.z - actor->world.pos.z;
            if (mode == 0 && (phantom->danceFrame == 13 || phantom->danceFrame == 37))
                SpawnPhantomBall(phantom,player,phantom->danceFrame == 37);
            if (mode == 1 && (phantom->danceFrame == 30 || phantom->danceFrame == 66))
                SpawnPhantomBall(phantom,player,(phantom->danceFrame == 66));
            if (mode == 2 && phantom->danceFrame == 48)
                SpawnPhantomBall(phantom,player,phantom->index & 1);
        } else {
            dx = sBoss->actor.world.pos.x - actor->world.pos.x;
            dz = sBoss->actor.world.pos.z - actor->world.pos.z;
        }
        actor->shape.rot.y = (s16)(atan2f(dx, dz) * (0x8000 / M_PI));
        actor->shape.rot.z = 0;
        if (mode == 3 && phantom->danceFrame >= 20 && phantom->danceFrame < 80) {
            f32 distance = sqrtf(dx * dx + dz * dz);
            if (distance > 85.0f) {
                actor->world.pos.x += dx / distance * 6.0f;
                actor->world.pos.z += dz / distance * 6.0f;
                KeepInsideArena(&actor->world.pos);
            }
        }
        if (phantom->contactCooldown > 0) phantom->contactCooldown--;
        if ((mode == 2 || mode == 3) && phantom->contactCooldown == 0 &&
            dx * dx + dz * dz < 72.0f * 72.0f) {
            Actor_SetPlayerKnockbackLargeNoDamage(play, actor, 8.0f, actor->shape.rot.y, 5.0f);
            play->damagePlayer(play, -16);
            phantom->contactCooldown = 35;
        }
    }
    actor->focus.pos = actor->world.pos;
    actor->focus.pos.y = sArenaFloorY + 92;
    Collider_UpdateCylinder(actor, &phantom->collider);
    phantom->collider.dim.pos.y = (s16)sArenaFloorY;
    CollisionCheck_SetAC(play, &play->colChkCtx, &phantom->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &phantom->collider.base);
}

static s32 PhantomOverrideLimb(PlayState* play, s32 limbIndex, Gfx** dl, Vec3f* pos, Vec3s* rot, void* data) {
    // The original spear is a separate limb and never belongs to this dance crew.
    if (limbIndex == 13) *dl = NULL;
    return 0;
}

static void PhantomPostLimb(PlayState* play, s32 limbIndex, Gfx** dl, Vec3s* rot, void* data) {
    CucuiPhantom* phantom = data;
    if (limbIndex == 7) Matrix_MultVec3f(&(Vec3f){500,0,0}, &phantom->handPos[0]);
    if (limbIndex == 11) Matrix_MultVec3f(&(Vec3f){500,0,0}, &phantom->handPos[1]);
}

static void PhantomDraw(Actor* actor, PlayState* play) {
    CucuiPhantom* phantom = (CucuiPhantom*)actor;
    static Gfx noEffectDL[] = { gsSPEndDisplayList() };
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, 45, 45, 45, 255);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)noEffectDL);
    SkelAnime_DrawSkeletonOpa(play, &phantom->skelAnime, PhantomOverrideLimb, PhantomPostLimb, phantom);
    CLOSE_DISPS(play->state.gfxCtx);

}

static void SpawnSalsaBall(BossGanon2* boss, Player* player, int hand) {
    int i;
    f32 side = hand == 0 ? -1.0f : 1.0f;
    for (i = 8; i < ARRAY_COUNT(sStarBalls); i++) {
        StarBall* ball = &sStarBalls[i];
        if (ball->life > 0) continue;
        ball->pos = PoseLimbWorldPoint(boss, hand == 0 ? 5 : 9, (Vec3f){500,0,0});
        ball->returning = 0;
        ball->kind = 1;
        {
            Vec3f target = player->actor.world.pos;
            target.y = sArenaFloorY + 22.0f;
            ConfigureBallArc(ball, target);
            ball->arcHeight = 0.0f;
            ball->lateralBend = side * 130.0f;
        }
        break;
    }
}

static StarBall* SpawnExtraBall(BossGanon2* boss, Player* player, bool sky) {
    int i;
    for (i = 8; i < ARRAY_COUNT(sStarBalls); i++) {
        StarBall* ball = &sStarBalls[i];
        Vec3f target;
        if (ball->life > 0) continue;
        ball->pos = PoseLimbWorldPoint(boss, sky ? 5 : 9, (Vec3f){500,0,0});
        ball->returning = 0;
        ball->kind = sky ? 4 : 3;
        target = player->actor.world.pos;
        if (sky) {
            float angle = sExtraVolleyClock * 0.73f;
            target.x += sinf(angle) * 90;
            target.z += cosf(angle) * 90;
            KeepInsideArena(&target);
        }
        target.y = sArenaFloorY + 22.0f;
        ConfigureBallArc(ball, target);
        if (sky) {
            ball->arcHeight = 260;
            ball->travelFrames = ball->fast ? 48 : 62;
            ball->life = ball->travelFrames + 7;
        } else {
            ball->velocity = (Vec3f){0, 2, 0};
            ball->life = 110;
        }
        return ball;
    }
    return NULL;
}

static bool SwordTouchesBall(Player* player, Vec3f from, Vec3f to) {
    int i;
    if (player->meleeWeaponState == 0) return false;
    for (i = 0; i < ARRAY_COUNT(player->meleeWeaponInfo); i++) {
        WeaponInfo* weapon = &player->meleeWeaponInfo[i];
        f32 vx, vy, vz, length2;
        int sample;
        if (!weapon->active) continue;
        vx = weapon->tip.x - weapon->base.x;
        vy = weapon->tip.y - weapon->base.y;
        vz = weapon->tip.z - weapon->base.z;
        length2 = vx * vx + vy * vy + vz * vz;
        if (length2 < 0.01f) continue;
        // Test the whole projectile step so a fast ball cannot cross the sword between updates.
        for (sample = 0; sample <= 2; sample++) {
            f32 fraction = sample * 0.5f;
            f32 wx = from.x + (to.x - from.x) * fraction - weapon->base.x;
            f32 wy = from.y + (to.y - from.y) * fraction - weapon->base.y;
            f32 wz = from.z + (to.z - from.z) * fraction - weapon->base.z;
            f32 t = (wx * vx + wy * vy + wz * vz) / length2;
            f32 dx, dy, dz;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            dx = wx - vx * t;
            dy = wy - vy * t;
            dz = wz - vz * t;
            if (dx * dx + dy * dy + dz * dz < BALL_SWORD_RADIUS * BALL_SWORD_RADIUS) return true;
        }
    }
    return false;
}

static void UpdateStarBalls(BossGanon2* boss, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Color_RGBA8 prim = { 255, 245, 155, 255 };
    Color_RGBA8 env = { 255, 110, 20, 255 };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    int i;

    if (sProjectileHitCooldown > 0) sProjectileHitCooldown--;
    for (i = 0; i < ARRAY_COUNT(sStarBalls); i++) {
        StarBall* ball = &sStarBalls[i];
        Vec3f previousPos;
        f32 dx, dz;
        if (ball->life <= 0) continue;
        previousPos = ball->pos;
        prim = (Color_RGBA8){ 255, 255, 255, 255 };
        env = ball->returning ? (Color_RGBA8){ 0, 255, 255, 255 } :
                               (Color_RGBA8){ 165, 255, 75, 255 };
        if (ball->returning) {
            Actor* target = &boss->actor;
            f32 dy, length;
            if (ball->targetPhantom >= 0) {
                target = sCrew[ball->targetPhantom];
                if (target == NULL || target->update == NULL) { ball->life = 0; continue; }
            }
            dy = target->focus.pos.y - ball->pos.y;
            dx = target->world.pos.x - ball->pos.x;
            dz = target->world.pos.z - ball->pos.z;
            length = sqrtf(dx * dx + dy * dy + dz * dz);
            if (length < BOSS_HURT_RADIUS + BALL_BOSS_HIT_RADIUS) {
                if (ball->targetPhantom >= 0) {
                    CucuiPhantom* phantom = (CucuiPhantom*)target;
                    if (phantom->life > 0) {
                        phantom->life = 0;
                        if (sPhantomsAlive > 0) sPhantomsAlive--;
                        Audio_PlayActorSound2(target, NA_SE_EN_FANTOM_HIT_THUNDER);
                        EffectSsFhgFlash_SpawnShock(play,NULL,&ball->pos,180,0);
                        Actor_Kill(target);
                    }
                } else if (!DamageCucui(boss, true, false)) {
                    EffectSsFhgFlash_SpawnShock(play,NULL,&ball->pos,160,0);
                    Audio_PlayActorSound2(&boss->actor, NA_SE_EN_FANTOM_HIT_THUNDER);
                }
                ball->life = 0;
                continue;
            }
            if (length < 1.0f) length = 1.0f;
            ball->velocity = (Vec3f){ dx / length * 12.0f, dy / length * 12.0f, dz / length * 12.0f };
            ball->pos.x += ball->velocity.x;
            ball->pos.y += ball->velocity.y;
            ball->pos.z += ball->velocity.z;
            ball->life--;
        } else if (ball->kind == 3) {
            // Gentle homing: capped speed and turn response so Link can evade or volley.
            float dy = player->actor.world.pos.y + 55 - ball->pos.y;
            float length;
            float speed = ball->fast ? 12.0f : 8.0f;
            dx = player->actor.world.pos.x - ball->pos.x;
            dz = player->actor.world.pos.z - ball->pos.z;
            length = sqrtf(dx*dx + dy*dy + dz*dz);
            if (length < 1) length = 1;
            ball->velocity.x += (dx / length * speed - ball->velocity.x) * 0.18f;
            ball->velocity.y += (dy / length * speed - ball->velocity.y) * 0.18f;
            ball->velocity.z += (dz / length * speed - ball->velocity.z) * 0.18f;
            ball->pos.x += ball->velocity.x;
            ball->pos.y += ball->velocity.y;
            ball->pos.z += ball->velocity.z;
            ball->life--;
        } else {
            if (ball->age < ball->travelFrames) {
                f32 t = (f32)(++ball->age) / ball->travelFrames;
                ball->pos = BallArcPoint(ball, t);
                if (ball->age == ball->travelFrames) {
                    ball->pos.y = sArenaFloorY + 22.0f;
                    EffectSsKiraKira_SpawnDispersed(play,&ball->pos,&zero,&zero,&prim,&env,350,12);
                }
            }
            ball->life--;
        }
        EffectSsKiraKira_SpawnFocused(play, &ball->pos, &zero, &zero, &prim, &env, 360, 10);
        if (!ball->returning && SwordTouchesBall(player, previousPos, ball->pos)) {
            ball->returning = 1;
            ball->life = 220;
            ball->targetPhantom = -1;
            { int target; for (target = 0; target < PHANTOM_COUNT; target++) {
                if (sCrew[target] && sCrew[target]->update && player->focusActor == sCrew[target]) {
                    ball->targetPhantom = target;
                    break;
                }
            } }
            Audio_PlayActorSound2(&boss->actor, NA_SE_EN_FANTOM_HIT_THUNDER);
            continue;
        }
        dx = ball->pos.x - player->actor.world.pos.x;
        dz = ball->pos.z - player->actor.world.pos.z;
        if (!ball->returning && sProjectileHitCooldown == 0 &&
            dx * dx + dz * dz < BALL_LINK_HURT_RADIUS * BALL_LINK_HURT_RADIUS &&
            ball->pos.y >= player->actor.world.pos.y + 25.0f &&
            ball->pos.y <= player->actor.world.pos.y + 85.0f) {
            Actor* source = &boss->actor;
            if (ball->ownerPhantom >= 0 && ball->ownerPhantom < PHANTOM_COUNT &&
                sCrew[ball->ownerPhantom] != NULL && sCrew[ball->ownerPhantom]->update != NULL)
                source = sCrew[ball->ownerPhantom];
            Actor_SetPlayerKnockbackLargeNoDamage(play, source, 11.0f, (s16)(i * 0x2000), 6.0f);
            play->damagePlayer(play, -BALL_DAMAGE);
            ApplyLinkElement(play,ball->element,ball->pos);
            if (ball->element == ELEMENT_ELECTRIC) sChaseStrikePending=true;
            sProjectileHitCooldown = 25;
            ball->life = 0;
        }
    }
}

static void StopLyrics(PlayState* play) {
    (void)play;
    sLyricCurrent = -1;
}

static void UpdateLyrics(PlayState* play) {
    uint32_t beat;
    int wanted = 0;
    int cumulative = 0;
    if (sFinalStage != 0 || sMove == MOVE_INTRO || sPostIntroSpin || sStageLightRamp == 0 ||
        play->actorCtx.titleCtx.durationTimer > 0 ||
        !sMusicStarted || sMusicFrame < MUSIC_LOOP_FRAME) return;
    if (sLyricStartFrame == 0) {
        uint32_t elapsed = sMusicFrame - MUSIC_LOOP_FRAME;
        uint32_t nextBeat = (elapsed * 60 + LOOP_FRAMES - 1) / LOOP_FRAMES;
        sLyricStartFrame = MUSIC_LOOP_FRAME + (nextBeat * LOOP_FRAMES + 59) / 60;
    }
    if (sMusicFrame < sLyricStartFrame) return;
    beat = (uint32_t)(((uint64_t)(sMusicFrame - sLyricStartFrame) * 60 / LOOP_FRAMES) % LYRIC_TOTAL_BEATS);
    for (wanted = 0; wanted < ARRAY_COUNT(sLyrics); wanted++) {
        cumulative += sLyrics[wanted].beats;
        if (beat < (uint32_t)cumulative) break;
    }
    if (wanted >= ARRAY_COUNT(sLyrics)) wanted = 0;
    sLyricCurrent = wanted;
}

static void OnFrameLyrics(void) {
    PlayState* play = gPlayState;
    Player* player;
    Actor* target;
    if (play == NULL || !sPlaying || play->sceneNum != SCENE_GANON_BOSS ||
        play->pauseCtx.state != 0 || play->pauseCtx.debugState != 0) return;
    player = GET_PLAYER(play);
    target = player->focusActor;
    // Let C-Up bring up the real Navi hint even when the game's target pointer
    // briefly drops during a dance. A focused Phantom takes priority.
    if ((play->state.input[0].press.button & BTN_CUP) &&
        play->msgCtx.msgMode == MSGMODE_NONE && play->msgCtx.msgLength == 0) {
        if (target == NULL || target->update == NULL || target->naviEnemyId == 0xFF)
            target = (sBoss != NULL && sBoss->actor.update != NULL) ? &sBoss->actor : NULL;
    }
    if (target != NULL && target->update != NULL && target->naviEnemyId != 0xFF &&
        (play->state.input[0].press.button & BTN_CUP) &&
        play->msgCtx.msgMode == MSGMODE_NONE && play->msgCtx.msgLength == 0) {
        Message_StartTextbox(play, 0x0600 + target->naviEnemyId, target);
        return;
    }
    UpdateLyrics(play);
}

static void PrepareLyricLines(const char* source, char* top, char* bottom) {
    char ascii[96];
    size_t n = 0;
    size_t split;
    const unsigned char* p = (const unsigned char*)source;
    while (*p != 0 && n + 1 < sizeof(ascii)) {
        if (*p == 0xC3 && p[1] != 0) {
            unsigned char ch = p[1];
            if (ch == 0xA1 || ch == 0x81) ascii[n++] = 'a';
            else if (ch == 0xA9 || ch == 0x89) ascii[n++] = 'e';
            else if (ch == 0xAD || ch == 0x8D) ascii[n++] = 'i';
            else if (ch == 0xB3 || ch == 0x93) ascii[n++] = 'o';
            else if (ch == 0xBA || ch == 0x9A) ascii[n++] = 'u';
            else if (ch == 0xB1 || ch == 0x91) ascii[n++] = 'n';
            p += 2;
        } else if (*p == 0xC2 && (p[1] == 0xA1 || p[1] == 0xBF)) {
            p += 2;
        } else if (*p < 0x80) {
            ascii[n++] = (char)*p++;
        } else {
            p++;
        }
    }
    ascii[n] = 0;
    split = n;
    if (split > 34) {
        split = 34;
        while (split > 0 && ascii[split] != ' ') split--;
        if (split == 0) split = 34;
    }
    for (size_t i = 0; i < split; i++) top[i] = ascii[i];
    top[split] = 0;
    if (split < n) {
        size_t start = ascii[split] == ' ' ? split + 1 : split;
        size_t i = 0;
        while (start < n && i < 34) bottom[i++] = ascii[start++];
        bottom[i] = 0;
    } else {
        bottom[0] = 0;
    }
}

static void DrawLyrics(PlayState* play) {
    GfxPrint printer;
    Gfx* plusOne;
    Gfx* polyOpaP;
    char top[36];
    char bottom[36];
    int x;
    if (play == NULL || !sPlaying || sLyricCurrent < 0 ||
        play->sceneNum != SCENE_GANON_BOSS || play->pauseCtx.state != 0 ||
        play->pauseCtx.debugState != 0 || play->msgCtx.msgMode != MSGMODE_NONE ||
        play->msgCtx.msgLength != 0) return;
    PrepareLyricLines(sLyrics[sLyricCurrent].text, top, bottom);
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, LYRIC_BOX_ALPHA);
    gDPFillRectangle(OVERLAY_DISP++, 20, bottom[0] ? 173 : 181, 300, 218);
    gDPPipeSync(OVERLAY_DISP++);
    plusOne = Graph_GfxPlusOne(polyOpaP = POLY_OPA_DISP);
    gSPDisplayList(OVERLAY_DISP++, plusOne);
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, plusOne);
    GfxPrint_SetColor(&printer, 250, 250, 250, 255);
    x = (320 - (int)(8 * strlen(top))) / 2;
    GfxPrint_SetPosPx(&printer, x, bottom[0] ? 184 : 191);
    GfxPrint_Printf(&printer, "%s", top);
    if (bottom[0]) {
        x = (320 - (int)(8 * strlen(bottom))) / 2;
        GfxPrint_SetPosPx(&printer, x, 200);
        GfxPrint_Printf(&printer, "%s", bottom);
    }
    plusOne = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
    gSPEndDisplayList(plusOne++);
    Graph_BranchDlist(polyOpaP, plusOne);
    POLY_OPA_DISP = plusOne;
    CLOSE_DISPS(play->state.gfxCtx);
}

static void BeginFinalBlow(BossGanon2* boss, PlayState* play) {
    Player* player = GET_PLAYER(play);
    int i;
    sFinalStage = 1;
    StopLyrics(play);
    sFinalTimer = 0;
    sStunTimer = 0;
    sReflectedHits = 0;
    sWaveTimer = 0;
    sChargeAmount = 0;
    sSpotlightChasing = false;
    sChaseStrikePending = false;
    for (i = 0; i < ARRAY_COUNT(sStarBalls); i++) sStarBalls[i].life = 0;
    for (i = 0; i < ARRAY_COUNT(sRocks); i++) sRocks[i].life = 0;
    boss->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    player->actor.world.pos.x = boss->actor.world.pos.x - Math_SinS(boss->actor.shape.rot.y) * 100.0f;
    player->actor.world.pos.y = sArenaFloorY;
    player->actor.world.pos.z = boss->actor.world.pos.z - Math_CosS(boss->actor.shape.rot.y) * 100.0f;
    player->actor.shape.rot.y = boss->actor.shape.rot.y;
    player->actor.world.rot.y = player->actor.shape.rot.y;
    Animation_MorphToPlayOnce(&boss->skelAnime, (AnimationHeader*)gGanondorfDefeatedStartAnim, -3.0f);
    func_80064520(play, &play->csCtx);
    sFinalCamera = Play_CreateSubCamera(play);
    if (sFinalCamera > 0) {
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, sFinalCamera, CAM_STAT_ACTIVE);
    }
    if (play->startPlayerCutscene != NULL) play->startPlayerCutscene(play, &boss->actor, 0x61);
    sApi->ReportBossDefeated(&boss->actor);
    Audio_PlayActorSound2(&boss->actor, NA_SE_EN_GANON_DOWN);
}

static void UpdateFinalBlow(BossGanon2* boss, PlayState* play) {
    static const Color_RGBA8 bloodPrim = {120,0,0,255};
    static const Color_RGBA8 bloodEnv = {120,0,0,255};
    int i;
    if (sFinalStage != 1) return;
    sFinalTimer++;
    SkelAnime_Update(&boss->skelAnime);
    boss->actor.world.pos.y = sArenaFloorY - PoseSoleHeight(boss);
    if (sFinalTimer == 16 || sFinalTimer == 32 || sFinalTimer == 48) {
        Vec3f pos = boss->actor.world.pos;
        pos.y = sArenaFloorY + 100.0f;
        Audio_PlayActorSound2(&boss->actor, NA_SE_EN_GANON_DAMAGE2);
        for (i = 0; i < 24; i++) {
            Vec3f velocity = { Rand_CenteredFloat(5.0f), Rand_ZeroFloat(3.0f) + 1.0f,
                               Rand_CenteredFloat(5.0f) };
            Vec3f accel = {0.0f,-0.6f,0.0f};
            func_8002836C(play,&pos,&velocity,&accel,(Color_RGBA8*)&bloodPrim,
                          (Color_RGBA8*)&bloodEnv,60,0,17);
        }
    }
    if (sFinalTimer == 58)
        Animation_MorphToPlayOnce(&boss->skelAnime,(AnimationHeader*)gGanondorfVomitStartAnim,-3.0f);
    if (sFinalTimer == 94) {
        Animation_MorphToPlayOnce(&boss->skelAnime,(AnimationHeader*)gGanondorfYellStartAnim,-3.0f);
        Audio_PlayActorSound2(&boss->actor,NA_SE_EN_GANON_DEAD);
    }
    if (sFinalTimer == 124) Audio_PlayActorSound2(&boss->actor,NA_SE_EN_GANON_TOKETU);
    if (sFinalTimer == 150) Audio_PlayActorSound2(&boss->actor,NA_SE_EN_FANTOM_DEAD);
    if (sFinalTimer >= 94) {
        f32 rise = (f32)(sFinalTimer - 94) * 3.0f;
        boss->actor.world.pos.y += rise;
    }
    if (sFinalTimer >= 110) {
        Vec3f pos = boss->actor.world.pos;
        Vec3f velocity = {0.0f,1.0f,0.0f};
        Vec3f accel = {0.0f,0.0f,0.0f};
        Color_RGBA8 prim = {180,145,255,255};
        Color_RGBA8 env = {55,15,155,255};
        pos.y += 80.0f;
        for (i = 0; i < 6; i++) {
            Vec3f speck = pos;
            speck.x += Rand_CenteredFloat(95.0f);
            speck.y += Rand_CenteredFloat(100.0f);
            speck.z += Rand_CenteredFloat(95.0f);
            EffectSsKiraKira_SpawnDispersed(play,&speck,&velocity,&accel,&prim,&env,500,12);
        }
        Actor_SetScale(&boss->actor, 0.01f * (170 - sFinalTimer) / 60.0f);
    }
    if (sFinalCamera > 0) {
        Vec3f at = boss->actor.world.pos;
        Vec3f eye = at;
        at.y += 75.0f;
        eye.x += Math_CosS(boss->actor.shape.rot.y) * 230.0f;
        eye.z -= Math_SinS(boss->actor.shape.rot.y) * 230.0f;
        eye.y += 105.0f;
        Play_CameraSetAtEye(play,sFinalCamera,&at,&eye);
        Play_CameraSetFov(play,sFinalCamera,55.0f);
    }
    if (sFinalTimer >= 170) {
        Actor_SetScale(&boss->actor,0.0f);
        boss->actor.draw = NULL;
        StopBossHurtbox(boss,play);
        RestoreArenaLighting();
        if (sFinalCamera > 0) {
            func_800C08AC(play,sFinalCamera,0);
            sFinalCamera = 0;
        }
        func_80064534(play,&play->csCtx);
        Player_SetCsActionWithHaltedActors(play,&boss->actor,7);
        Audio_QueueSeqCmd((0x1 << 28) | (SEQ_PLAYER_BGM_MAIN << 24) | 0x100FF);
        sFinalStage = 2;
        if (sWarpActorId >= 0) {
            Actor_Spawn(&play->actorCtx, play, sWarpActorId, ARENA_CENTER_X, sArenaFloorY + 2.0f,
                        ARENA_CENTER_Z, 0, 0, 0, 0);
        }
    }
}

static void DanceAction(BossGanon2* boss, PlayState* play) {
    Player* player = GET_PLAYER(play);
    int first, count;
    const GanondorfDancePose* poses;

    if (sFinalStage != 0) {
        UpdateFinalBlow(boss,play);
        return;
    }

    UpdateLinkStatus(play);
    UpdateRockWave(boss,play);
    if (sStunTimer == 0 && sReflectedHits > 0 && play->gameplayFrames % (sReflectedHits == 1 ? 8 : 3) == 0) {
        Vec3f spark = boss->actor.world.pos;
        spark.y = sArenaFloorY + 70 + Rand_ZeroFloat(90);
        spark.x += Rand_CenteredFloat(60); spark.z += Rand_CenteredFloat(60);
        EffectSsFhgFlash_SpawnShock(play,NULL,&spark,sStunTimer>0?180:80+sReflectedHits*45,0);
    }
    if (sStunTimer > 0) {
        bool finished = SkelAnime_Update(&boss->skelAnime);
        if (finished && sStunStage == 0) {
            Animation_MorphToPlayOnce(&boss->skelAnime,(AnimationHeader*)gGanondorfDownedAnim,-3);
            sStunStage=1;
        } else if (finished && sStunStage == 1) {
            Animation_MorphToPlayOnce(&boss->skelAnime,(AnimationHeader*)gGanondorfLandAnim,-3);
            Audio_PlayActorSound2(&boss->actor,NA_SE_EN_GANON_DOWN);
            sStunStage=2;
        } else if (finished && sStunStage == 2) {
            Animation_MorphToLoop(&boss->skelAnime,(AnimationHeader*)gGanondorfVulnerableAnim,-3);
            sStunStage=3;
        }
        boss->actor.world.pos.y = sArenaFloorY - PoseSoleHeight(boss);
        boss->actor.focus.pos = boss->actor.world.pos;
        boss->actor.focus.pos.y = sArenaFloorY + 45.0f;
        boss->actor.speedXZ=0; boss->actor.velocity=(Vec3f){0}; boss->actor.gravity=0; boss->unk_316=2;
        if (sStunStage == 3 && --sStunTimer == 0) {
            Audio_PlayActorSound2(&boss->actor,NA_SE_EN_GANON_RESTORE);
            if (sPhase==1 && sHealth<=BOSS_PHASE2_HEALTH) BeginPhaseTwo(boss);
            else BeginMove(boss,MOVE_IDLE);
        }
        UpdateStarBalls(boss,play);
        UpdateDanceRocks(boss,play);
        return;
    }
    if (sDashCooldown > 0) sDashCooldown--;
    if (sVolleyRemaining > 0) {
        if (sVolleyTimer > 0) sVolleyTimer--;
        else {
            SpawnSalsaBall(boss, player, sVolleyHand);
            sVolleyRemaining--;
            sVolleyTimer = 9;
        }
    }
    if (sSpotlightStunCooldown > 0) sSpotlightStunCooldown--;
    if (sPhase == 2 && sSummonedCount == PHANTOM_COUNT && sPhantomsAlive == 0) sEnraged = true;
    if (sPhase == 2 && sSummonTimer > 0 && sMove == MOVE_IMMUNE) {
        sSummonTimer--;
        if (sSummonTimer <= 48 && sSummonTimer % 12 == 0 && sSummonedCount < PHANTOM_COUNT &&
            sPhantomActorId >= 0) {
            int index = sSummonedCount;
            f32 angle = (f32)index * ((f32)M_PI / 2.0f) + (f32)M_PI / 4.0f;
            Vec3f pos = { ARENA_CENTER_X + cosf(angle) * 245.0f, sArenaFloorY + 65.0f,
                          ARENA_CENTER_Z + sinf(angle) * 245.0f };
            if (Actor_Spawn(&play->actorCtx, play, sPhantomActorId, pos.x, pos.y, pos.z, 0, 0, 0, index) != NULL)
                sSummonedCount++;
            else sSummonTimer += 12; // Retry after actor-capacity pressure clears.
        }
    }

    if (sFrame < DESCENT_FRAMES) {
        f32 turnT = (f32)(sFrame+1)/24.0f;
        f32 t = (f32)((s32)sFrame-23)/(DESCENT_FRAMES-24);
        if (turnT>1) turnT=1;
        if (t<0) t=0;
        f32 eased = t * t * (3.0f - 2.0f * t);
        int i;

        boss->actor.world.pos = sOriginalPosition;
        boss->actor.world.pos.y +=
            (sArenaFloorY - sBossIntroPoses[0].footY - sOriginalPosition.y) * eased;
        boss->actor.shape.yOffset = sOriginalYOffset * (1.0f - eased);
        boss->actor.world.rot.y = LerpAngle(sOriginalRotation.y, 0, turnT);
        boss->actor.shape.rot.y = boss->actor.world.rot.y;
        for (i = 0; i <= DANCE_LIMBS; i++) {
            const Vec3s* introJoint = i == 0 ? &sNeutralJoints[0] : &sBossIntroPoses[0].joints[i - 1];
            f32 blend = turnT;
            boss->skelAnime.jointTable[i].x = LerpAngle(sInitialJoints[i].x, introJoint->x, blend);
            boss->skelAnime.jointTable[i].y = LerpAngle(sInitialJoints[i].y, introJoint->y, blend);
            boss->skelAnime.jointTable[i].z = LerpAngle(sInitialJoints[i].z, introJoint->z, blend);
        }
        {
            f32 safeY = sArenaFloorY - PoseSoleHeight(boss);
            if (boss->actor.world.pos.y < safeY) boss->actor.world.pos.y = safeY;
        }
        sFrame++;
    } else {
        f32 dx, dz, distance;
        Vec3f previousPosition = boss->actor.world.pos;
        sFrame++;
        if (sMove == MOVE_INTRO && sMoveFrame >= ARRAY_COUNT(sBossIntroPoses)) {
            boss->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
            sPostIntroSpin = true;
            if (sStageLightRamp == 0) sStageLightRamp = 1;
            sSpotlightTarget = boss->actor.world.pos;
            BeginMove(boss, MOVE_SPIN);
        }
        poses = MovePoses(sMove, &first, &count);
        if (sMoveFrame >= (uint32_t)count) {
            if (sMove == MOVE_M5) {
                sMoveFrame = count - 1;
            } else if (sMove == MOVE_SALSA_2 && sVolleyRemaining > 0) {
                sMoveFrame = count-1;
            } else if (sMove == MOVE_IDLE) {
                sIdleCharged = true;
                BeginMove(boss,MOVE_CHARGED_TURN);
            } else if (sMove == MOVE_CHARGED_TURN) {
                f32 dx = player->actor.world.pos.x - boss->actor.world.pos.x;
                f32 dz = player->actor.world.pos.z - boss->actor.world.pos.z;
                if (dx * dx + dz * dz >= DASH_TRIGGER_DISTANCE * DASH_TRIGGER_DISTANCE &&
                    sDashCooldown == 0) {
                    BeginMove(boss, MOVE_DASH);
                    sDashCooldown = 180;
                } else {
                    switch (sAttackCycle++ % 6) {
                        case 0: BeginMove(boss,MOVE_IMMUNE); break;
                        case 1: BeginMove(boss,MOVE_SALSA_2); break;
                        case 2: BeginMove(boss,MOVE_SEXY_TURN); break;
                        case 3: BeginMove(boss,MOVE_BACK); break;
                        case 4: BeginMove(boss,MOVE_OVERHEAD); break;
                        default: BeginMove(boss,MOVE_SALSA_1); break;
                    }
                }
            } else if (sMove == MOVE_BACK) {
                BeginMove(boss, MOVE_FORWARD);
            } else if (sMove == MOVE_FORWARD) {
                BeginMove(boss, sChaseStrikePending ? MOVE_SEXY_TURN : MOVE_SALSA_2);
                sChaseStrikePending = false;
            } else if (sMove == MOVE_DASH) {
                if (sDashElapsed < 90) sMoveFrame = 18;
                else BeginMove(boss, MOVE_RETURN_SPIN);
            } else if (sMove == MOVE_IMMUNE && sPhase == 2 && sSummonedCount < PHANTOM_COUNT) {
                BeginMove(boss, MOVE_IMMUNE);
            } else if (sMove == MOVE_IMMUNE || sMove == MOVE_SEXY_TURN) {
                BeginMove(boss, (sMoveCount & 1) ? MOVE_SPIN : MOVE_SPIN_REVERSE);
            } else {
                if (sPostIntroSpin && sMove == MOVE_SPIN) {
                    sPostIntroSpin = false;
                    ReleaseIntroCamera(boss,play);
                    BeginMove(boss, sShowcase ? MOVE_M5 : MOVE_IDLE);
                } else {
                    BeginMove(boss, sPhase == 2 && sSummonTimer > 0 && sSummonedCount == 0 ?
                              MOVE_IMMUNE : MOVE_IDLE);
                }
            }
            poses = MovePoses(sMove, &first, &count);
        }
        if (sMove == MOVE_INTRO && sMoveFrame >= 52 && sStageLightRamp == 0) {
            sStageLightRamp=1;
            sSpotlightTarget=boss->actor.world.pos;
        }
        if (sMove == MOVE_INTRO && sStageLightRamp > 0 && !sTitleShown) {
            TitleCard_InitBossName(play, &play->actorCtx.titleCtx, (void*)sBossTitleTex,
                                   160, 180, 128, 40, true);
            sTitleShown = true;
        }
        dx = player->actor.world.pos.x - boss->actor.world.pos.x;
        dz = player->actor.world.pos.z - boss->actor.world.pos.z;
        distance = sqrtf(dx * dx + dz * dz);
        if (sMove == MOVE_BACK && distance > 210.0f && !sSpotlightChasing && sSpotlightStunCooldown == 0) {
            sSpotlightChasing = true;
            sSpotlightChaseAge = 0;
        }
        if (sSpotlightChasing) {
            f32 chaseDx = player->actor.world.pos.x - sSpotlightTarget.x;
            f32 chaseDz = player->actor.world.pos.z - sSpotlightTarget.z;
            f32 chaseDistance = sqrtf(chaseDx * chaseDx + chaseDz * chaseDz);
            f32 step = chaseDistance < 3.0f ? chaseDistance : 3.0f;
            if (chaseDistance > 0.01f) {
                sSpotlightTarget.x += chaseDx / chaseDistance * step;
                sSpotlightTarget.z += chaseDz / chaseDistance * step;
            }
            sSpotlightChaseAge++;
            if (sSpotlightChaseAge >= 100) { sSpotlightChasing=false; sSpotlightStunCooldown=100; }
            chaseDistance -= step;
            if (chaseDistance < SPOT_HIT_RADIUS && sSpotlightStunCooldown == 0 && sSpotlightChaseAge >= 12) {
                ApplyLinkElement(play,ELEMENT_ELECTRIC,player->actor.world.pos);
                sLinkStatusTimer=35;
                sSpotlightStunCooldown = 150;
                sSpotlightChasing = false;
                sChaseStrikePending = true;
                BeginMove(boss, MOVE_FORWARD);
            }
        }
        if (sChaseStrikePending && sMove == MOVE_FORWARD && distance < 125.0f) {
            BeginMove(boss, MOVE_SEXY_TURN);
            sChaseStrikePending = false;
        }
        if (sMove != MOVE_M5 && sMove != MOVE_INTRO && sMove != MOVE_RETURN_SPIN &&
            sMove != MOVE_OVERHEAD && sMove != MOVE_CHARGED_TURN &&
            !(sMove == MOVE_DASH && sMoveFrame >= DASH_WINDUP) && distance > 1.0f) {
            s16 targetYaw = (s16)(atan2f(dx, dz) * (0x8000 / M_PI));
            s16 turn = (s16)(targetYaw - boss->actor.shape.rot.y);
            if (turn > 0x900) turn = 0x900;
            if (turn < -0x900) turn = -0x900;
            boss->actor.shape.rot.y = sMove == MOVE_IDLE ? targetYaw : boss->actor.shape.rot.y + turn;
        }
        if (sMove == MOVE_DASH && sMoveFrame >= DASH_WINDUP) {
            f32 step;
            if (MoveBeat(DASH_WINDUP)) {
                if (distance > 1.0f) {
                    sDashDirection=(Vec3f){dx/distance,0,dz/distance};
                } else {
                    sDashDirection=(Vec3f){Math_SinS(boss->actor.shape.rot.y),0,
                                           Math_CosS(boss->actor.shape.rot.y)};
                }
                sDashRemaining=distance+100;
                if (sDashRemaining>650) sDashRemaining=650;
                boss->actor.shape.rot.y=(s16)(atan2f(dx,dz)*(0x8000/M_PI));
            }
            step=sDashRemaining<32?sDashRemaining:32;
            boss->actor.world.pos.x+=sDashDirection.x*step;
            boss->actor.world.pos.z+=sDashDirection.z*step;
            sDashRemaining-=step;
            KeepInsideArena(&boss->actor.world.pos);
            sAnchor = boss->actor.world.pos;
            sAnchor.y = sArenaFloorY;
            if (sMoveFrame % 3 == 0) {
                Vec3f dustPos = boss->actor.world.pos;
                Vec3f still = { 0.0f, 0.0f, 0.0f };
                Color_RGBA8 prim = { 195, 225, 255, 255 };
                Color_RGBA8 env = { 80, 125, 255, 255 };
                dustPos.y = sArenaFloorY + 18.0f;
                EffectSsKiraKira_SpawnFocused(play, &dustPos, &still, &still, &prim, &env, 350, 8);
            }
        } else if ((sMove == MOVE_FORWARD || sMove == MOVE_BACK) && distance > 1.0f) {
            f32 step = sMove == MOVE_FORWARD ? (sChaseStrikePending ? 8.0f : 4.8f) : -1.8f;
            boss->actor.world.pos.x += dx / distance * step;
            boss->actor.world.pos.z += dz / distance * step;
            KeepInsideArena(&boss->actor.world.pos);
            sAnchor = boss->actor.world.pos;
            sAnchor.y = sArenaFloorY;
        } else if (sMove == MOVE_RETURN_SPIN) {
            f32 t = (f32)(sMoveFrame + 1) / ARRAY_COUNT(sBossSpinPoses);
            t = t * t * (3.0f - 2.0f * t);
            boss->actor.world.pos.x = sReturnStart.x + (ARENA_CENTER_X - sReturnStart.x) * t;
            boss->actor.world.pos.z = sReturnStart.z + (ARENA_CENTER_Z - sReturnStart.z) * t;
            sAnchor = boss->actor.world.pos;
            sAnchor.y = sArenaFloorY;
        }
        if (sMove == MOVE_IMMUNE && MoveBeat(6)) {
            sWaveOrigin=boss->actor.world.pos;
            sWaveDirection=(Vec3f){distance>1?dx/distance:0,0,distance>1?dz/distance:1};
            sWaveTimer=32;sWaveStep=0;
        }
        if (sMove == MOVE_SEXY_TURN) {
          int i;
          for (i = 0; i < 8; i++) {
            StarBall* ball = &sStarBalls[i];
            s16 yaw = (s16)(boss->actor.shape.rot.y + i * 0x2000);
            Vec3f target;
            if (!MoveBeat(i * 9) || ball->life > 0) continue;
            ball->pos = PoseLimbWorldPoint(boss, (i & 1) ? 5 : 9, (Vec3f){500,0,0});
            ball->velocity.x = Math_SinS(yaw) * 8.0f;
            ball->velocity.y = 0.0f;
            ball->velocity.z = Math_CosS(yaw) * 8.0f;
            ball->life = 70;
            ball->kind = 0;
            ball->returning = 0;
            target = ball->pos;
            target.x += Math_SinS(yaw) * 430.0f;
            target.z += Math_CosS(yaw) * 430.0f;
            KeepInsideArena(&target);
            target.y = sArenaFloorY + 22.0f;
            ConfigureBallArc(ball, target);
            ball->arcHeight=170;
            ball->travelFrames=ball->fast?46:60;
            ball->life=ball->travelFrames+7;
          }
        }
        if (sMove == MOVE_SEXY_TURN && sMoveFrame >= 24 && sMoveFrame <= 32 && distance < TURN_HIT_RADIUS &&
            sProjectileHitCooldown == 0) {
            Actor_SetPlayerKnockbackLargeNoDamage(play, &boss->actor, 13.0f, boss->actor.shape.rot.y, 7.0f);
            play->damagePlayer(play, -SPIN_DAMAGE);
            sProjectileHitCooldown = 30;
        }
        if (sMove == MOVE_SALSA_2 && (MoveBeat(16) || MoveBeat(40))) {
            sVolleyHand = MoveBeat(16) ? 0 : 1;
            SpawnSalsaBall(boss, player, sVolleyHand);
            sVolleyRemaining = 2;
            sVolleyTimer = 9;
        }
        if (!sShowcase) {
            if (sMove == MOVE_IDLE) sChargeAmount=(f32)(sMoveFrame+1)/60;
            if (sMove == MOVE_CHARGED_TURN && MoveBeat(0) && sIdleCharged) {
                StarBall* ball=SpawnExtraBall(boss,player,false);
                if (ball) { ball->kind=1;ball->element=sChargeElement; }
                sIdleCharged=false;
            }
            if (sMove == MOVE_OVERHEAD) {
                sChargeAmount=sMoveFrame<42?(f32)(sMoveFrame+1)/42:0;
                if (MoveBeat(42)) {
                    StarBall* ball=SpawnExtraBall(boss,player,true);
                    if (ball) { ball->element=ELEMENT_FIRE;ball->arcHeight=150; }
                }
            }
            if (sMove == MOVE_BACK && MoveBeat(20)) {
                StarBall* ball=SpawnExtraBall(boss,player,false);
                if (ball) {
                    ball->kind=1;ball->element=ELEMENT_ELECTRIC;ball->fast=false;
                    ball->travelFrames=(int)(distance/6.0f);
                    if (ball->travelFrames<28) ball->travelFrames=28;
                    ball->life=ball->travelFrames+7;ball->arcHeight=20;
                }
            }
        }
        if (!sShowcase && sMove == MOVE_FORWARD &&
            (MoveBeat(12) || MoveBeat(36))) SpawnStepRocks(boss, play);
        poses = MovePoses(sMove, &first, &count);
        ApplyPose(boss, poses + first, sMoveFrame, sMove != MOVE_DASH && sMove != MOVE_FORWARD &&
                  sMove != MOVE_BACK && sMove != MOVE_SEXY_TURN && sMove != MOVE_RETURN_SPIN && sMove != MOVE_IDLE);
        KeepInsideArena(&boss->actor.world.pos);
        KeepOnArenaFloor(boss, play, previousPosition);
        sDrawMoveFrame = sMoveFrame;
        sPreviousMoveFrame = sMoveFrame;
        sMoveFrame += sEnraged && sMove != MOVE_SALSA_2 ? 2 : 1;
        if (sMove == MOVE_DASH && sDrawMoveFrame >= DASH_WINDUP) {
            f32 realDx = player->actor.world.pos.x - boss->actor.world.pos.x;
            f32 realDz = player->actor.world.pos.z - boss->actor.world.pos.z;
            sDashElapsed++;
            if (realDx * realDx + realDz * realDz <= DASH_CONTACT_DISTANCE * DASH_CONTACT_DISTANCE) {
                if (sProjectileHitCooldown == 0) {
                    Actor_SetPlayerKnockbackLargeNoDamage(play, &boss->actor, 16.0f, boss->actor.shape.rot.y, 8.0f);
                    play->damagePlayer(play, -DASH_DAMAGE);
                    sProjectileHitCooldown = 25;
                }
                BeginMove(boss, MOVE_RETURN_SPIN);
            } else if (sDashRemaining <= 0 || sDashElapsed >= 25) {
                BeginMove(boss, MOVE_RETURN_SPIN);
            }
        }
        if (sStageLightRamp > 0 && !sSpotlightChasing) {
            sSpotlightTarget.x += (boss->actor.world.pos.x - sSpotlightTarget.x) * 0.28f;
            sSpotlightTarget.z += (boss->actor.world.pos.z - sSpotlightTarget.z) * 0.28f;
        }
    }
    UpdateStarBalls(boss, play);
    UpdateDanceRocks(boss, play);
    boss->actor.velocity = (Vec3f){ 0.0f, 0.0f, 0.0f };
    boss->actor.speedXZ = 0.0f;
    boss->actor.gravity = 0.0f;
    boss->unk_324 = 255.0f;
    boss->unk_1B4 = sFrame < 24 ? 255.0f : (sFrame < 44 ? (44-sFrame)*12.0f : 0.0f);
    boss->unk_30C = 0.0f;
    boss->unk_339 = 0;
    boss->unk_316 = 2;
    if (boss->subCamId != 0) AimShowcaseCamera(boss,play);
}

static void StopDance(BossGanon2* boss) {
    if (!sPlaying || boss != sBoss) {
        return;
    }
    StopLyrics(gPlayState);
    StopBossHurtbox(boss, gPlayState);
    boss->actionFunc = sOriginalAction;
    boss->actor.world.pos = sOriginalPosition;
    boss->actor.world.rot = sOriginalRotation;
    boss->actor.shape.rot = sOriginalRotation;
    boss->actor.shape.yOffset = sOriginalYOffset;
    boss->actor.shape.shadowDraw = sOriginalShadowDraw;
    boss->actor.shape.shadowScale = sOriginalShadowScale;
    boss->actor.shape.shadowAlpha = sOriginalShadowAlpha;
    sOriginalAction = NULL;
    sPlaying = false;
    sFrame = 0;
    sMusicStarted = false;
    sPostIntroSpin = false;
    sPhase = 0;
    RestoreArenaLighting();
}

static void OnBossDamage(Actor* victim, ColliderInfo* attack, float* damage) {
    s16 angle;
    if (!sPlaying || sShowcase || sBoss == NULL || victim != &sBoss->actor || attack == NULL || damage == NULL) {
        return;
    }
    if (sFinalStage != 0) { *damage = 0.0f; return; }
    // Every accepted hit is applied immediately; there is no timed invulnerability.
    {
        Player* player = GET_PLAYER(gPlayState);
        f32 dx = player->actor.world.pos.x - sBoss->actor.world.pos.x;
        f32 dz = player->actor.world.pos.z - sBoss->actor.world.pos.z;
        angle = (s16)((s16)(atan2f(dx, dz) * (0x8000 / M_PI)) - sBoss->actor.shape.rot.y);
    }
    if (sStageLightRamp > 0 && ABS((s32)angle) >= 0x4000) {
        bool arrow = (attack->toucher.dmgFlags & DMG_ARROW) != 0;
        bool accepted = DamageCucui(sBoss,false,arrow);
        if (accepted && !arrow && sPhase == 2 && sHealth <= 1 && sStunTimer > 0 && sStunStage == 3)
            BeginFinalBlow(sBoss,gPlayState);
    }
    // Native beast damage state is not compatible with the human skeleton.
    *damage = 0.0f;
}

static void OnGanondorfUpdate(void* actorPtr) {
    BossGanon2* boss = (BossGanon2*)actorPtr;
    int i;

    // The final-battle actor uses Ganondorf's human skeleton before transforming.
    if (gPlayState == NULL || gPlayState->sceneNum != SCENE_GANON_BOSS || boss->actor.update == NULL) {
        return;
    }
    if (boss != sBoss) {
        sBoss = boss;
        sOriginalAction = NULL;
        sPlaying = false;
        sFrame = 0;
        sMusicStarted = false;
        sMusicFrame = 0;
    }
    if (sPlaying) {
        // The mod deals contact damage explicitly during attacks.
        UpdateBossHurtbox(boss, gPlayState);
        boss->actor.focus.pos = boss->actor.world.pos;
        boss->actor.focus.pos.y = sStunTimer > 0 ? sArenaFloorY + 45.0f :
                                    boss->actor.world.pos.y + 95.0f;
    }
    if (!Z64Boss_IsFightHere(sApi, BOSS_KEY, gPlayState)) {
        // Once the subcamera has been closed, the original transformation
        // action cannot safely resume at its old cutscene state.
        if (sPlaying && sFrame < DESCENT_FRAMES + ARRAY_COUNT(sBossIntroPoses)) {
            StopDance(boss);
        } else if (sPlaying && sStageLightRamp > 0) {
            BrightenArena(gPlayState);
        } else {
            RestoreArenaLighting();
        }
        return;
    }
    if (!sMusicStarted && boss->csState == 14 && boss->csTimer <= 1) {
        // Vanilla queues NA_BGM_OPENING_GANON immediately before this hook.
        PlayMusic(sIntroMusic);
        sMusicStarted = true;
        sMusicFrame = 0;
    }
    if (sMusicStarted) {
        if (sStageLightRamp > 0 && sStageLightRamp < 20) sStageLightRamp++;
        if (sStageLightRamp > 0) BrightenArena(gPlayState);
        if (sMusicFrame == MUSIC_ENTRY_FRAME) PlayMusic(sEntryMusic);
        if (sMusicFrame == MUSIC_LOOP_FRAME) PlayMusic(sMutedMusic);
        if (sMusicFrame == MUSIC_LOOP_FRAME + LOOP_FRAMES) PlayMusic(sFullMusic);
        sMusicFrame++;
    }
    if (sPlaying) {
        UpdateStageLights(boss, gPlayState);
        return;
    }
    // State 15 reaches frame 140 just before the cyan transformation starts.
    // State 16 is a fallback for builds that advance the cutscene earlier.
    if (boss->unk_337 != 0 ||
        !((boss->csState == 15 && boss->csTimer >= 139) ||
          (boss->csState == 16 && boss->csTimer <= 1))) {
        return;
    }

    StartBossHurtbox(boss, gPlayState);
    sOriginalAction = boss->actionFunc;
    sOriginalPosition = boss->actor.world.pos;
    sOriginalRotation = boss->actor.world.rot;
    sOriginalYOffset = boss->actor.shape.yOffset;
    sOriginalShadowDraw = boss->actor.shape.shadowDraw;
    sOriginalShadowScale = boss->actor.shape.shadowScale;
    sOriginalShadowAlpha = boss->actor.shape.shadowAlpha;
    boss->actor.shape.shadowDraw = ActorShadow_DrawCircle;
    boss->actor.shape.shadowScale = 115.0f;
    boss->actor.shape.shadowAlpha = 150;
    if (sBossNaviHintId != 0xFF) boss->actor.naviEnemyId = sBossNaviHintId;
    for (i = 0; i <= DANCE_LIMBS; i++) {
        sInitialJoints[i] = boss->skelAnime.jointTable[i];
        sPreviousJoints[i] = i == 0 ? sNeutralJoints[0] : sBossIntroPoses[0].joints[i - 1];
    }
    sAnchor = boss->actor.world.pos;
    // The standing animation puts its root above the actor origin. Resolve
    // the actual arena floor rather than Link's animated cutscene position.
    sArenaFloorY = FindArenaFloorY(boss, gPlayState);
    sStableRootY = sArenaFloorY - sBossIntroPoses[0].footY;
    sAnchor.y = sArenaFloorY;
    sFrame = 0;
    sMove = MOVE_INTRO;
    sMoveFrame = 0;
    sPreviousMoveFrame = -1;
    sDrawMoveFrame = 0;
    sMoveCount = 0;
    sAttackCycle = 0;
    sDashElapsed = 0;
    sDashCooldown = 0;
    sHealth = BOSS_MAX_HEALTH;
    sStunTimer = 0;
    sStunStage = 0;
    sFinalStage = 0;
    sFinalTimer = 0;
    sFinalCamera = 0;
    sTitleShown = false;
    sLyricCurrent = -1;
    sLyricStartFrame = 0;
    sLinkStatusTimer=0;sWaveTimer=0;sChargeAmount=0;sIdleCharged=false;
    Audio_PlayActorSound2(&boss->actor,NA_SE_EV_TRIFORCE_FLASH);
    sReflectedHits = 0;
    sShotSerial = 0;
    for (i = 0; i < PHANTOM_COUNT; i++) sCrew[i] = NULL;
    sExtraVolleyClock = 0;
    sStepCount = 0;
    for (i = 0; i < ARRAY_COUNT(sRocks); i++) sRocks[i].life = 0;
    sProjectileHitCooldown = 0;
    sVolleyRemaining = sVolleyTimer = 0;
    sSpotlightChaseAge = 0;
    sPostIntroSpin = false;
    sStageLightRamp = 0;
    sSpotlightTarget = boss->actor.world.pos;
    sSpotlightChasing = false;
    sSpotlightStunCooldown = 0;
    sChaseStrikePending = false;
    sPhase = 1;
    sSummonTimer = 0;
    sSummonedCount = 0;
    sPhantomsAlive = 0;
    sEnraged = false;
    sShowcase = false;
    for (i = 0; i < ARRAY_COUNT(sStarBalls); i++) sStarBalls[i].life = 0;
    sPlaying = true;
    boss->actionFunc = DanceAction;
    DanceAction(boss, gPlayState);
    UpdateBossHurtbox(boss, gPlayState);
}

static void OnGanondorfDestroy(void* actorPtr) {
    if (actorPtr == (void*)sBoss) {
        StopLyrics(gPlayState);
        StopBossHurtbox(sBoss, gPlayState);
        RestoreArenaLighting();
        sBoss = NULL;
        sOriginalAction = NULL;
        sPlaying = false;
        sFrame = 0;
        sMusicStarted = false;
        sPostIntroSpin = false;
        sPhase = 0;
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHModMenuWidget toggle;
    SOHActorDefinition phantom = { 0 };
    SOHActorDefinition warp = { 0 };

    if (sApi == NULL || !SOH_MOD_API_HAS(sApi, RegisterBoss)) {
        return;
    }
    sApi->RegisterBoss(&sCucuiBoss);
    if (SOH_MOD_API_HAS(sApi, RegisterActor)) {
        phantom.structSize = sizeof(phantom);
        phantom.key = PHANTOM_KEY;
        phantom.description = "Phantom Ganon (Boss_Ganondrof)";
        phantom.category = ACTORCAT_ENEMY;
        phantom.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE |
                             ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
        // OTR asset paths resolve independently of the room's object bank.
        phantom.objectId = OBJECT_GAMEPLAY_KEEP;
        phantom.instanceSize = sizeof(CucuiPhantom);
        phantom.init = PhantomInit;
        phantom.destroy = PhantomDestroy;
        phantom.update = PhantomUpdate;
        phantom.draw = PhantomDraw;
        phantom.naviHint = "Phantom Ganon&%cIt dances like Ganondorf. Strike&its back or use bomb arrows.^"
                           "%c%cReflect its light balls with your&sword to defeat it.";
        sPhantomActorId = sApi->RegisterActor(&phantom);
        warp.structSize = sizeof(warp);
        warp.key = WARP_KEY;
        warp.description = "Cucui Ganon blue warp";
        warp.category = ACTORCAT_PROP;
        warp.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
        warp.objectId = OBJECT_GAMEPLAY_KEEP;
        warp.instanceSize = sizeof(CucuiBlueWarp);
        warp.init = BlueWarpInit;
        warp.update = BlueWarpUpdate;
        warp.draw = BlueWarpDraw;
        sWarpActorId = sApi->RegisterActor(&warp);
    }
    sBossNaviHintId = ModMessages_RegisterNaviHint(BOSS_HINT_KEY,
        "Papi chulo Ganondorf&%cHe dances and throws light balls.&Strike his back. Reflect three&"
        "balls to stun him.^%c%cIn phase two, defeat all four&Phantoms first.");
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_BOSS_GANON2, OnGanondorfUpdate);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, OnFrameLyrics);
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawLyrics);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_BOSS_GANON2, DrawSpotlights);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDestroy, ACTOR_BOSS_GANON2, OnGanondorfDestroy);
    SOH_REGISTER_HOOK(sApi, OnAudioTablesReady, RegisterMusic);
    SOH_REGISTER_HOOK(sApi, OnCollisionResolveDamage, OnBossDamage);
    RegisterMusic();
    if (SOH_MOD_API_HAS(sApi, RegisterMenuSidebar) && SOH_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        sApi->RegisterMenuSidebar("Mods", "Gameplay", "gMods.Menu.Page", 2);
        toggle = (SOHModMenuWidget){ 0 };
        toggle.structSize = sizeof(toggle);
        toggle.section = "Mods";
        toggle.sidebar = "Gameplay";
        toggle.type = SOH_MOD_MENU_CHECKBOX;
        toggle.label = "Cucui Ganon battle";
        toggle.cvar = DANCE_CVAR;
        toggle.tooltip = "Ganondorf descends, performs the intro, then fights Link. Restart the scene to replay.";
        toggle.defaultInt = 0;
        sApi->RegisterMenuWidget(&toggle);

    }
}
