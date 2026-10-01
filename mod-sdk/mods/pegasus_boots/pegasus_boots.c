/**
 * Pegasus Boots: holding B where a spin attack would charge turns the charge into a blade-first dash, with a wind
 * cone in front that strikes while there is magic to pay for it. Walls, crates and trees stop it with a bonk.
 */

#include <math.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_gi_hoverboots/object_gi_hoverboots.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "overlays/ovl_Magic_Wind/ovl_Magic_Wind.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

#define PEGASUS_BOOTS_KEY "nei.equip.pegasus_boots"
#define PEGASUS_WINDUP_FRAMES 10
#define PEGASUS_DASH_SPEED 18.0f
#define PEGASUS_LEG_FRAMES_PER_UPDATE 3.0f
#define PEGASUS_STAB_FRAME 2
#define PEGASUS_BONK_DECEL 2.0f
#define PEGASUS_MAGIC_INTERVAL 15
#define PEGASUS_TURN_RATE 5.0f
#define PEGASUS_STICK_DEADZONE 10.0f
#define PEGASUS_COL_RADIUS 50
#define PEGASUS_COL_HEIGHT 80
#define PEGASUS_COL_FORWARD 40.0f
#define PEGASUS_CONE_FORWARD 80.0f
#define PEGASUS_CONE_HEIGHT 40.0f
#define PEGASUS_CONE_SCALE 0.015f
#define BGCHECK_ON_GROUND 0x0001
#define BGCHECK_TOUCHING_WALL 0x0200
#define GI_DL_CAPACITY 512

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconPegasusBootsTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gPegasusBootsNameTex";

void func_80853080(Player* player, PlayState* play);
void func_8002F974(Actor* actor, u16 sfxId);
void func_800AA000(f32 distSq, u8 strength, u8 duration, u8 decreaseRate);

typedef enum {
    PEGASUS_IDLE,
    PEGASUS_WINDUP,
    PEGASUS_RUNNING,
    PEGASUS_BONK,
} PegasusState;

static const SOHModApi* sApi;

static ColliderCylinder sWindCollider;

static ColliderCylinderInit sWindColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_SLASH, 0x00, 0x04 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { PEGASUS_COL_RADIUS, PEGASUS_COL_HEIGHT, 0, { 0, 0, 0 } },
};

static struct {
    PegasusState state;
    s16 timer;
    s16 magicTick;
    f32 legFrame;
    bool isColliderReady;
    bool smashedCrate;
    bool needsResetOnLanding;
} sPegasus;

// The torso and arms take the stab pose; everything from the waist down keeps running.
static u8 sUpperBodyMap[PLAYER_LIMB_MAX];
static Vec3s sStabPose[PLAYER_LIMB_BUF_COUNT];
static Vec3s sRunPose[PLAYER_LIMB_BUF_COUNT];

static Vtx sConeVtx[] = {
    VTX(0, 0, 0, 512, 2048, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(4000, 8000, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0x00),
    VTX(2828, 8000, 2828, 256, 0, 0xFF, 0xFF, 0xFF, 0x00),
    VTX(0, 8000, 4000, 512, 0, 0xFF, 0xFF, 0xFF, 0x00),
    VTX(-2828, 8000, 2828, 768, 0, 0xFF, 0xFF, 0xFF, 0x00),
    VTX(-4000, 8000, 0, 1024, 0, 0xFF, 0xFF, 0xFF, 0x00),
    VTX(-2828, 8000, -2828, 1280, 0, 0xFF, 0xFF, 0xFF, 0x00),
    VTX(0, 8000, -4000, 1536, 0, 0xFF, 0xFF, 0xFF, 0x00),
    VTX(2828, 8000, -2828, 1792, 0, 0xFF, 0xFF, 0xFF, 0x00),
};

// Segment 8 carries the per-frame texture scroll; the texture itself is loaded by path before this runs.
static Gfx sConeDL[] = {
    gsDPSetCombineLERP(TEXEL1, PRIMITIVE, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, PRIMITIVE,
                       ENVIRONMENT, COMBINED, ENVIRONMENT, COMBINED, 0, SHADE, 0),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2),
    gsSPClearGeometryMode(G_CULL_BACK | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    gsDPSetPrimColor(0, 0x80, 255, 255, 170, 255),
    gsDPSetEnvColor(100, 255, 50, 0),
    gsSPDisplayList(0x08000001),
    gsSPVertex(sConeVtx, 9, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(0, 3, 4, 0, 0, 4, 5, 0),
    gsSP2Triangles(0, 5, 6, 0, 0, 6, 7, 0),
    gsSP2Triangles(0, 7, 8, 0, 0, 8, 1, 0),
    gsSPEndDisplayList(),
};

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(PEGASUS_BOOTS_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECK_ON_GROUND) != 0;
}

static bool IsPlayerBusy(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                                   PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) != 0;
}

static bool HoldsSword(Player* player) {
    return (player->heldItemAction >= PLAYER_IA_SWORD_MASTER && player->heldItemAction <= PLAYER_IA_SWORD_BIGGORON) ||
           player->heldItemAction == PLAYER_IA_SWORD_KOKIRI;
}

static bool IsDashing(void) {
    return sPegasus.state == PEGASUS_RUNNING || sPegasus.state == PEGASUS_BONK;
}

static void PlaySfx(Player* player, u16 sfxId) {
    Audio_PlaySoundGeneral(sfxId, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// The dash rides the spin-charge walk; leaving that action live afterwards moves the stick against the locked
// facing, which is the inverted-controls state.
static void ResetToIdle(Player* player, PlayState* play) {
    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->unk_858 = 0.0f;
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
    func_80853080(player, play);
    sPegasus.needsResetOnLanding = false;
}

static void StopDash(Player* player, PlayState* play, bool resetAction) {
    bool wasDashing = IsDashing();

    sPegasus.state = PEGASUS_IDLE;
    sPegasus.timer = 0;
    sPegasus.magicTick = 0;
    sPegasus.smashedCrate = false;
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    sWindCollider.base.atFlags &= ~AT_ON;
    if (!resetAction || !wasDashing) {
        return;
    }
    if (IsGrounded(player)) {
        ResetToIdle(player, play);
    } else {
        sPegasus.needsResetOnLanding = true;
    }
}

static void StrikeWithWind(Player* player, PlayState* play) {
    Actor* struck;

    if (!sPegasus.isColliderReady) {
        Collider_InitCylinder(play, &sWindCollider);
        Collider_SetCylinder(play, &sWindCollider, &player->actor, &sWindColliderInit);
        sPegasus.isColliderReady = true;
    }
    sWindCollider.dim.pos.x = (s16)(player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * PEGASUS_COL_FORWARD);
    sWindCollider.dim.pos.y = (s16)player->actor.world.pos.y;
    sWindCollider.dim.pos.z = (s16)(player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * PEGASUS_COL_FORWARD);
    sWindCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sWindCollider.base);
    if (!(sWindCollider.base.atFlags & AT_HIT)) {
        return;
    }
    // Without this the wind breaks a crate before the wall touch registers, and the dash only sometimes stops.
    struck = sWindCollider.base.at;
    if (struck != NULL && (struck->id == ACTOR_OBJ_KIBAKO || struck->id == ACTOR_OBJ_KIBAKO2)) {
        sPegasus.smashedCrate = true;
    }
    PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
    sWindCollider.base.atFlags &= ~AT_HIT;
}

static void PayForWind(Player* player, PlayState* play) {
    if (gSaveContext.magic <= 0) {
        sWindCollider.base.atFlags &= ~AT_ON;
        sPegasus.magicTick = 0;
        return;
    }
    StrikeWithWind(player, play);
    if (++sPegasus.magicTick >= PEGASUS_MAGIC_INTERVAL) {
        sPegasus.magicTick = 0;
        gSaveContext.magic--;
    }
}

// Frame loads land at once, but the charge walk blends its legs through a queued interpolation that runs later
// and would overwrite them; both poses go in as queued copies behind it.
static void PoseDash(Player* player, PlayState* play) {
    f32 lastFrame = Animation_GetLastFrame((void*)gPlayerAnim_link_normal_run_free);

    sPegasus.legFrame += PEGASUS_LEG_FRAMES_PER_UPDATE;
    if (sPegasus.legFrame > lastFrame) {
        sPegasus.legFrame -= lastFrame;
    }
    AnimationContext_SetLoadFrame(play, (LinkAnimationHeader*)gPlayerAnim_link_normal_run_free,
                                  (s32)sPegasus.legFrame, player->skelAnime.limbCount, sRunPose);
    AnimationContext_SetCopyAll(play, player->skelAnime.limbCount, player->skelAnime.jointTable, sRunPose);
    AnimationContext_SetLoadFrame(play, (LinkAnimationHeader*)gPlayerAnim_link_fighter_Lpierce_kiru,
                                  PEGASUS_STAB_FRAME, player->skelAnime.limbCount, sStabPose);
    AnimationContext_SetCopyTrue(play, player->skelAnime.limbCount, player->skelAnime.jointTable, sStabPose,
                                 sUpperBodyMap);
}

static bool IsFacingWall(Player* player) {
    s16 yawToWall = player->yaw - (s16)(player->actor.wallYaw + 0x8000);

    return (player->actor.bgCheckFlags & BGCHECK_TOUCHING_WALL) && ABS(yawToWall) < 0x2000;
}

// The same signals a roll leaves: a crate wall breaks, a tree drops what it holds.
static bool HitsObstacle(Player* player, PlayState* play) {
    Actor* touched = player->cylinder.base.oc;

    if (sPegasus.smashedCrate) {
        sPegasus.smashedCrate = false;
        return true;
    }
    if (IsFacingWall(player)) {
        if (player->actor.wallBgId != BGCHECK_SCENE) {
            DynaPolyActor* wallActor = DynaPoly_GetActor(&play->colCtx, player->actor.wallBgId);

            if (wallActor != NULL) {
                wallActor->actor.home.rot.z = 1;
            }
        }
        return true;
    }
    if ((player->cylinder.base.ocFlags1 & OC1_HIT) && touched != NULL && touched->id == ACTOR_EN_WOOD02 &&
        ABS((s16)(player->actor.world.rot.y - touched->yawTowardsPlayer)) > 0x6000) {
        touched->home.rot.y = 1;
        return true;
    }
    return false;
}

// A roll's own bonk: the charge action stays underneath and the dash ends when hip_down has played out.
static void Bonk(Player* player, PlayState* play) {
    LinkAnimation_Change(play, &player->skelAnime, (LinkAnimationHeader*)gPlayerAnim_link_normal_hip_down_free, 1.0f,
                         0.0f, Animation_GetLastFrame((void*)gPlayerAnim_link_normal_hip_down_free), ANIMMODE_ONCE,
                         -6.0f);
    player->linearVelocity = -player->linearVelocity;
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    Quake_Add(Play_GetCamera(play, 0), 3);
    func_800AA000(255.0f, 20, 150, 0);
    PlaySfx(player, NA_SE_PL_BODY_HIT);
    PlaySfx(player, NA_SE_VO_LI_CLIMB_END);
    sWindCollider.base.atFlags &= ~AT_ON;
    sPegasus.state = PEGASUS_BONK;
}

static void UpdateIdle(Player* player, Input* input) {
    if (!CHECK_BTN_ALL(input->cur.button, BTN_B) || !IsGrounded(player) ||
        (player->stateFlags1 & PLAYER_STATE1_IN_WATER) || !HoldsSword(player)) {
        return;
    }
    if (player->unk_858 < 0.1f && !(player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
        return;
    }
    player->unk_858 = 0.0f;
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    sPegasus.state = PEGASUS_WINDUP;
    sPegasus.timer = PEGASUS_WINDUP_FRAMES;
    PlaySfx(player, NA_SE_PL_WALK_GROUND);
}

// A cancelled windup needs no reset: the charge action still owns Link and ends itself.
static void UpdateWindup(Player* player, PlayState* play, Input* input) {
    if (!CHECK_BTN_ALL(input->cur.button, BTN_B)) {
        StopDash(player, play, false);
        return;
    }
    player->unk_858 = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    if (--sPegasus.timer > 0) {
        return;
    }
    sPegasus.state = PEGASUS_RUNNING;
    sPegasus.magicTick = 0;
    sPegasus.legFrame = 0.0f;
    PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void UpdateRunning(Player* player, PlayState* play, Input* input) {
    if (!CHECK_BTN_ALL(input->cur.button, BTN_B) || (player->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
        StopDash(player, play, true);
        return;
    }
    player->unk_858 = 0.0f;
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    if (fabsf((f32)input->rel.stick_x) > PEGASUS_STICK_DEADZONE) {
        player->actor.shape.rot.y -= (s16)(input->rel.stick_x * PEGASUS_TURN_RATE);
    }
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
    player->linearVelocity = PEGASUS_DASH_SPEED;
    player->actor.speedXZ = PEGASUS_DASH_SPEED;

    PoseDash(player, play);
    PayForWind(player, play);
    func_8002F974(&player->actor, NA_SE_PL_WALK_GROUND - SFX_FLAG);
    if (HitsObstacle(player, play)) {
        Bonk(player, play);
    }
}

static void UpdateBonk(Player* player, PlayState* play) {
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    Math_StepToF(&player->linearVelocity, 0.0f, PEGASUS_BONK_DECEL);
    player->actor.speedXZ = fabsf(player->linearVelocity);
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        StopDash(player, play, true);
    }
}

static void TickDash(void) {
    PlayState* play = gPlayState;
    Player* player;
    Input* input;

    if (play == NULL || !IsWorn()) {
        return;
    }
    player = GET_PLAYER(play);
    input = &play->state.input[0];
    if (IsPlayerBusy(player)) {
        if (sPegasus.state != PEGASUS_IDLE) {
            StopDash(player, play, false);
        }
        sPegasus.needsResetOnLanding = false;
        return;
    }
    if (sPegasus.needsResetOnLanding && IsGrounded(player)) {
        ResetToIdle(player, play);
    }
    switch (sPegasus.state) {
        case PEGASUS_IDLE:
            UpdateIdle(player, input);
            break;
        case PEGASUS_WINDUP:
            UpdateWindup(player, play, input);
            break;
        case PEGASUS_RUNNING:
            UpdateRunning(player, play, input);
            break;
        case PEGASUS_BONK:
            UpdateBonk(player, play);
            break;
        default:
            StopDash(player, play, true);
            break;
    }
}

// Nothing Link is told to do lands while he is picking himself up off the floor.
static void HoldStillWhileBonked(Player* player, Input* input) {
    if (sPegasus.state != PEGASUS_BONK) {
        return;
    }
    input->cur.stick_x = 0;
    input->cur.stick_y = 0;
    input->rel.stick_x = 0;
    input->rel.stick_y = 0;
    input->press.button = 0;
    input->cur.button = 0;
}

static void DrawWindCone(Player* player, PlayState* play) {
    u32 frames = play->gameplayFrames;
    f32 sinY = Math_SinS(player->actor.shape.rot.y);
    f32 cosY = Math_CosS(player->actor.shape.rot.y);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(player->actor.world.pos.x + sinY * PEGASUS_CONE_FORWARD,
                     player->actor.world.pos.y + PEGASUS_CONE_HEIGHT,
                     player->actor.world.pos.z + cosY * PEGASUS_CONE_FORWARD, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD((s16)-0x4000), MTXMODE_APPLY);
    Matrix_Scale(PEGASUS_CONE_SCALE, PEGASUS_CONE_SCALE, PEGASUS_CONE_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(POLY_XLU_DISP++, sTex, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock(POLY_XLU_DISP++, sTex, 0x0100, 1, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0, G_TX_NOMIRROR | G_TX_WRAP,
                      G_TX_NOMIRROR | G_TX_WRAP, 6, 6, 14, 14);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, -(s32)frames, (s32)(frames * 20), 0x40, 0x40, 1,
                                           -(s32)(frames * 2), (s32)(frames * 10), 0x40, 0x40));
    gSPDisplayList(POLY_XLU_DISP++, sConeDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// The anklet is the hover boots dyed red, drawn where the body drew no boots of its own: these boots sit on a
// Kokiri base. The skeleton's limb matrices on segment 0x0D are still loaded right after the player draws.
static void DrawRedBoots(Player* player, PlayState* play) {
    if (!LINK_IS_ADULT || (player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON) ||
        (player->stateFlags2 & PLAYER_STATE2_CRAWLING)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, 210, 30, 30, 255);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gLinkAdultLeftHoverBootDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gLinkAdultRightHoverBootDL);
    gSPGrayscale(POLY_OPA_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawOnPlayer(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (!IsWorn() || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    DrawRedBoots(player, play);
    if (sPegasus.state == PEGASUS_RUNNING && gSaveContext.magic > 0) {
        DrawWindCone(player, play);
    }
}

// The hover boots' get-item model mapped onto the icon's crimson; a grayscale tint could only darken it.
static u32 CrimsonRamp(u32 rgba) {
    u8 r = (rgba >> 24) & 0xFF;
    u8 g = (rgba >> 16) & 0xFF;
    u8 b = (rgba >> 8) & 0xFF;
    f32 lum = 0.299f * r + 0.587f * g + 0.114f * b;

    return ((u32)CLAMP_MAX(lum * 1.7f, 255.0f) << 24) | ((u32)(lum * 0.35f) << 16) | ((u32)(lum * 0.42f) << 8) |
           (rgba & 0xFF);
}

static bool IsTwoWordCommand(u8 opcode) {
    return opcode == 0x20 || opcode == 0x24 || opcode == 0x25 || opcode == 0x27 || opcode == 0x31 ||
           opcode == 0x32 || opcode == 0x33 || opcode == 0x35 || opcode == 0x36 || opcode == 0x42;
}

// A local copy, never the shared resource: the vanilla Hover Boots must keep their own colours.
static Gfx* BuildCrimsonBootsDL(void) {
    static Gfx sDL[GI_DL_CAPACITY];
    static bool sBuilt;
    Gfx* source;
    s32 count = 0;

    if (sBuilt) {
        return sDL;
    }
    source = ResourceMgr_LoadGfxByName(gGiHoverBootsDL);
    if (source == NULL) {
        return NULL;
    }
    while (count < GI_DL_CAPACITY) {
        u8 opcode = (source[count].words.w0 >> 24) & 0xFF;

        sDL[count] = source[count];
        count++;
        if (opcode == G_ENDDL) {
            break;
        }
        if (IsTwoWordCommand(opcode) && count < GI_DL_CAPACITY) {
            sDL[count] = source[count];
            count++;
            continue;
        }
        if (opcode == G_SETPRIMCOLOR || opcode == G_SETENVCOLOR) {
            sDL[count - 1].words.w1 = CrimsonRamp((u32)sDL[count - 1].words.w1);
        }
    }
    sBuilt = true;
    return sDL;
}

static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    Gfx* boots = BuildCrimsonBootsDL();

    if (boots == NULL) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, boots);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The collider is bound to the player actor, and a new scene brings a new one.
static void ForgetSceneState(int16_t sceneNum) {
    sPegasus.state = PEGASUS_IDLE;
    sPegasus.isColliderReady = false;
    sPegasus.needsResetOnLanding = false;
}

static void ReleaseDash(const char* key) {
    if (gPlayState != NULL && sPegasus.state != PEGASUS_IDLE) {
        StopDash(GET_PLAYER(gPlayState), gPlayState, true);
    }
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayerFilterInput", "OnActorDrawEnd",
                                              "OnSceneInit" };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHCustomEquipDefinition definition = { 0 };

    for (s32 limb = PLAYER_LIMB_UPPER; limb < PLAYER_LIMB_MAX; limb++) {
        sUpperBodyMap[limb] = true;
    }

    definition.structSize = sizeof(definition);
    definition.key = PEGASUS_BOOTS_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rPegasus Boots&%wHold %y\xA0%w where a spin attack would charge and you dash instead. "
                           "The wind in front costs magic.";
    definition.getItemText = "You got the %rPegasus Boots%w!&Boots that never tire. Hold %y\xA0%w as a spin would "
                             "charge and you run blade first, behind a wind that strikes whatever stands in the way.";
    definition.slot = SOH_EQUIP_SLOT_BOOTS;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_BOOTS;
    definition.column = 1;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_ANY;
    definition.vanillaBase = EQUIP_VALUE_BOOTS_KOKIRI;
    definition.toggles = 1;
    definition.onUnequip = ReleaseDash;
    definition.getItemEntry.drawFunc = DrawGetItem;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickDash);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, HoldStillWhileBonked);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawOnPlayer);
}
