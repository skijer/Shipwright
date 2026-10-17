// The four boss remains worn on Link's face, NEI's boss_remains for Ship. Odolwa: a run on A, his sword, shield,
// stance and trail, moths from the sword and his beetles on R+B, a moth cloud to fly on near soft soil. Goht: a
// bull charge on A, a quake pound on R+A, charged thunder on B, a bombchu on R+B. Gyorg: endless breath, a faster
// swim, fish on R (R+B on land), a whirlpool on B underwater. Twinmold: a giant's sword.
#include "remains.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/ResourceManagerHelpers.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"

// mm_assets' services: Majora's Mask's own sound engine.
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
#define MM_SFX_STOP_SERVICE "mm.sfx.stop"
typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);
typedef void (*MmSfxStopFunc)(uint16_t sfxId);

#define STORAGE_KEY "nei.mm_collectables"
#define GOHT_SWORD_FIELD "gohtStashedSword"

// MM sfx ids (2ship mm/include/sfx.h).
#define MM_SE_EN_MIBOSS_GND1_OLD 0x380C
#define MM_SE_EN_MIBOSS_JUMP1 0x3813
#define MM_SE_EN_MIBOSS_VOICE1_OLD 0x3815
#define MM_SE_EN_MIBOSS_VOICE2_OLD 0x3816
#define MM_SE_EN_COMMON_THUNDER_THR 0x384D
#define MM_SE_EN_BOMCHU_AIM 0x3855
#define MM_SE_EN_ICEB_FOOTSTEP_OLD 0x394A
#define MM_SE_EN_COMMON_THUNDER 0x394B
#define MM_SE_EN_MB_MOTH_FLY 0x399B
#define MM_SE_EN_PIRANHA_ATTACK 0x39F4

#define ODOLWA_RUN_MULTIPLIER 2.0f
#define ODOLWA_MOTH_MAGIC_COST 2
#define ODOLWA_BEETLE_MAGIC_COST 6
#define ODOLWA_SUMMON_DANCE_FRAMES 45
#define ODOLWA_SOIL_RANGE 120.0f
#define ODOLWA_FLIGHT_SPEED 8.0f
#define ODOLWA_FLIGHT_MAX_FRAMES 200
#define ODOLWA_FLIGHT_TURN_RATE 0x0A
#define ODOLWA_FLIGHT_PITCH_MAX 0x3800
#define ODOLWA_TRAIL_LENGTH 6

#define GOHT_CHARGE_SPEED 18.0f
#define GOHT_CHARGE_MAGIC_INTERVAL 15
#define GOHT_CHARGE_ANIM_SPEED 1.4f
#define GOHT_TURN_STEP 0x2AA
#define GOHT_JUMP_VELOCITY 15.5f
#define GOHT_JUMP_GRAVITY_BOOST 5.0f
#define GOHT_JUMP_ANIM_DELAY 3
#define GOHT_BOUNCE_SPEED (-16.0f)
#define GOHT_BOUNCE_HOP 6.0f
#define GOHT_THUNDER_MAGIC_COST 4
#define GOHT_THUNDER_CHARGE_MAX 45
#define GOHT_THUNDER_CHARGE_MIN 5
#define GOHT_THUNDER_TTL_MIN 12
#define GOHT_THUNDER_TTL_MAX 64
#define GOHT_THUNDER_DAMAGE_MIN 2
#define GOHT_THUNDER_DAMAGE_MAX 10
// MM's BGCHECKFLAG_PLAYER_800: with it the floor check stops hugging small drops, so a ramp launches the charge.
#define BGCHECKFLAG_NO_SNAP 0x800

#define GYORG_SWIM_MULTIPLIER 1.6f
#define GYORG_WHIRLPOOL_RADIUS 220.0f
#define GYORG_WHIRLPOOL_PULL 14.0f
#define GYORG_WHIRLPOOL_DRAIN_RATE 10
#define GYORG_FUNNEL_OFFSET_Y (-5.25f)

#define TWINMOLD_DAMAGE_MULTIPLIER 2

#define MM_ANIM_VALUES_PER_FRAME 67
#define MM_ANIM_BASE_TRANSL_X (-57)

// The moon children's masks are the only face-fitted remains geometry Majora's Mask has.
static const char* const sMaskDLs[REMAINS_COUNT] = {
    "__OTR__objects/object_ob/gMoonChildOdolwasMaskDL",
    "__OTR__objects/object_ob/gMoonChildGohtsMaskDL",
    "__OTR__objects/object_ob/gMoonChildGyorgsMaskDL",
    "__OTR__objects/object_ob/gMoonChildTwinmoldsMaskDL",
};
// z_en_js.c's per-mask transform, plus a correction measured on human Link's head.
static const f32 sMaskScales[REMAINS_COUNT] = { 0.5f, 0.5f, 0.48f, 0.45f };
static const f32 sMaskTranslateY[REMAINS_COUNT] = { 1400.0f, 1470.0f, 1670.0f, 1470.0f };
static const f32 sMaskTranslateZ[REMAINS_COUNT] = { 700.0f, 900.0f, 900.0f, 900.0f };
#define MASK_SCALE_CORRECTION 1.04f
#define MASK_OFFSET_Y (-510.0f)
#define MASK_OFFSET_Z 383.0f

typedef enum {
    ODOLWA_ANIM_READY,
    ODOLWA_ANIM_SWING_DANCE,
    ODOLWA_ANIM_MOTH_DANCE,
    ODOLWA_ANIM_CROUCH,
    ODOLWA_ANIM_COUNT,
} OdolwaAnim;

// Retargeted to Link's skeleton; they ship in this mod's own archive.
static const char* const sOdolwaAnimPaths[ODOLWA_ANIM_COUNT] = {
    "__OTR__misc/link_animetion/gPlayerAnim_mhr_npc_odolwa_ready",
    "__OTR__misc/link_animetion/gPlayerAnim_mhr_npc_odolwa_arm_swing_dance",
    "__OTR__misc/link_animetion/gPlayerAnim_mhr_npc_odolwa_moth_summon_dance",
    "__OTR__misc/link_animetion/gPlayerAnim_mhr_npc_odolwa_crouch",
};

typedef enum {
    GOHT_ANIM_CHARGE,
    GOHT_ANIM_JUMP,
    GOHT_ANIM_JUMP_END,
    GOHT_ANIM_COUNT,
} GohtAnim;

// Majora's Mask's own: the flee run for the charge, the Zora jump kick for the pound.
static const char* const sGohtAnimPaths[GOHT_ANIM_COUNT] = {
    "__OTR__misc/link_animetion/gPlayerAnim_cl_nigeru_Data",
    "__OTR__misc/link_animetion/gPlayerAnim_pz_jumpAT_Data",
    "__OTR__misc/link_animetion/gPlayerAnim_pz_jumpATend_Data",
};
static const char* const sGohtFallbackAnims[GOHT_ANIM_COUNT] = {
    gPlayerAnim_link_normal_run,
    gPlayerAnim_link_fighter_jump_kiru,
    gPlayerAnim_link_fighter_jump_kiru_finsh,
};

static const char sOdolwaSwordDL[] = "__OTR__objects/object_boss01/gOdolwaSwordDL";
static const char sOdolwaShieldDL[] = "__OTR__objects/object_boss01/gOdolwaShieldDL";
static const char sMajoraChantTex[] = "__OTR__objects/object_stk2/object_stk2_Tex_008B50";
static const char sGohtLightningMaterialDL[] = "__OTR__objects/object_boss_hakugin/gGohtLightningMaterialDL";
static const char sGohtLightningModelDL[] = "__OTR__objects/object_boss_hakugin/gGohtLightningModelDL";
static const char sGohtOrbMaterialDL[] = "__OTR__objects/object_boss_hakugin/gGohtLightOrbMaterialDL";
static const char sGohtOrbModelDL[] = "__OTR__objects/object_boss_hakugin/gGohtLightOrbModelDL";

s32 Player_GetMovementSpeedAndYaw(Player* player, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);

typedef struct {
    Vec3f pos;
    s16 yaw;
    u8 isValid;
    Vec3s joints[PLAYER_LIMB_MAX];
} TrailSample;

static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static MmSfxStopFunc sStopMmSfx;
static s32 sWornRemains = REMAINS_NONE;

static LinkAnimationHeader* sOdolwaAnims[ODOLWA_ANIM_COUNT];
static LinkAnimationHeader sGohtAnims[GOHT_ANIM_COUNT];
static s16* sGohtAnimFrames[GOHT_ANIM_COUNT];

static bool sIsOdolwaRunBoosted;
static s16 sOdolwaStepTimer;
static s16 sOdolwaShieldFireTimer;
static s16 sSummonLock;
static LinkAnimationHeader* sSummonAnim;
static f32 sSummonFrame;
static u16 sSummonChant;
static s16 sFlightState; // 0 grounded, 1 dancing the takeoff, 2 riding the cloud
static s16 sFlightWindup;
static s16 sFlightTimer;
static s16 sFlightPitch;
static s16 sFlightYaw;
static TrailSample sTrail[ODOLWA_TRAIL_LENGTH];
static s32 sTrailHead;

static bool sIsGohtCharging;
static s16 sGohtChargeCooldown;
static s16 sGohtStepTimer;
static s16 sGohtQuakeState; // 0 idle, 1 airborne, 2 landed
static s16 sGohtQuakeDamageFrames;
static s16 sGohtQuakeAnimDelay;
static s16 sGohtJumpPlayTimer;
static s16 sGohtRecoverFrames;
static f32 sGohtChargeAnimFrame;
static s16 sGohtChargeMagicTick;
static bool sOwnsNoSnapFlag;
static bool sIsGohtSwordStashed;
static s16 sGohtThunderCharge;
static bool sIsGohtThunderCharging;
static ColliderCylinder sGohtQuakeCollider;
static ColliderCylinder sGohtChargeCollider;
static bool sAreGohtCollidersReady;

static bool sIsWhirlpoolActive;
static s16 sWhirlpoolDrain;
static Vec3f sWhirlpoolPos;
static ColliderCylinder sWhirlpoolCollider;
static ColliderCylinder sWhirlpoolAuraCollider;
static bool sAreWhirlpoolCollidersReady;

// Friendly hits: AT_TYPE_PLAYER lands on every enemy and never on Link.
static ColliderCylinderInit sGohtQuakeColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { DMG_HAMMER_SWING, 0x00, 0x04 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_NONE },
    { 110, 60, -20, { 0, 0, 0 } },
};
static ColliderCylinderInit sGohtChargeColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { DMG_HAMMER_SWING, 0x00, 0x01 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_NONE },
    { 45, 60, 0, { 0, 0, 0 } },
};
static ColliderCylinderInit sWhirlpoolColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { DMG_DEKU_STICK, 0x00, 0x02 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_NONE },
    { 200, 120, -40, { 0, 0, 0 } },
};
// A harder ring on Link himself, so what the vortex drags in keeps getting hit and never reaches him.
static ColliderCylinderInit sWhirlpoolAuraColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { DMG_DEKU_STICK, 0x00, 0x04 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_NONE },
    { 95, 110, -45, { 0, 0, 0 } },
};

static Vtx sGohtConeVtx[] = {
    { { { 0, 0, 0 }, 0, { 512, 2048 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
    { { { 4000, 8000, 0 }, 0, { 0, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { 2828, 8000, 2828 }, 0, { 256, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { 0, 8000, 4000 }, 0, { 512, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { -2828, 8000, 2828 }, 0, { 768, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { -4000, 8000, 0 }, 0, { 1024, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { -2828, 8000, -2828 }, 0, { 1280, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { 0, 8000, -4000 }, 0, { 1536, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { 2828, 8000, -2828 }, 0, { 1792, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
};

// The Pegasus cone, in Majora's chant colors.
static Gfx sGohtConeDL[] = {
    gsDPSetCombineLERP(TEXEL1, PRIMITIVE, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, PRIMITIVE,
                       ENVIRONMENT, COMBINED, ENVIRONMENT, COMBINED, 0, SHADE, 0),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2),
    gsSPClearGeometryMode(G_CULL_BACK | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    gsDPSetPrimColor(0, 0x80, 255, 90, 60, 255),
    gsDPSetEnvColor(130, 10, 10, 0),
    gsSPDisplayList(0x08000001),
    gsSPVertex(sGohtConeVtx, 9, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(0, 3, 4, 0, 0, 4, 5, 0),
    gsSP2Triangles(0, 5, 6, 0, 0, 6, 7, 0),
    gsSP2Triangles(0, 7, 8, 0, 0, 8, 1, 0),
    gsSPEndDisplayList(),
};

// Boss DLs branch into segment 0x0C (eye and limb sub-lists); on Link they must return instead.
static Gfx sSegment0xCNoop[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

static s16 DegreesToBinang(s32 degrees) {
    return (s16)((degrees * 0x10000) / 360);
}

static bool IsWorn(s32 remains) {
    return sWornRemains == remains;
}

static u16 sHeldButtons;
static u16 sPressedButtons;
static u32 sInputFrame;

static bool IsSwimming(Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0;
}

static bool IsInWater(Player* player) {
    return IsSwimming(player) || (player->stateFlags2 & PLAYER_STATE2_UNDERWATER) != 0;
}

// Player_Update only filters input Link may act on; in a cutscene it skips the hook and the capture goes stale.
static void CaptureInput(Player* player, Input* input) {
    sHeldButtons = input->cur.button;
    sPressedButtons = input->press.button;
    sInputFrame = gPlayState != NULL ? gPlayState->gameplayFrames : 0;
    if (IsWorn(REMAINS_GYORG) && IsSwimming(player)) {
        input->cur.button &= ~(BTN_B | BTN_R);
        input->press.button &= ~(BTN_B | BTN_R);
    }
}

static bool HasFreshInput(PlayState* play) {
    return sInputFrame == play->gameplayFrames;
}

static bool IsHeld(PlayState* play, u16 buttons) {
    return HasFreshInput(play) && CHECK_BTN_ALL(sHeldButtons, buttons);
}

static bool IsPressed(PlayState* play, u16 buttons) {
    return HasFreshInput(play) && CHECK_BTN_ALL(sPressedButtons, buttons);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool SpendMagic(s16 cost) {
    if (!gSaveContext.isMagicAcquired || gSaveContext.magic < cost) {
        return false;
    }
    gSaveContext.magic -= cost;
    return true;
}

void Remains_PlayMmSfx(u16 sfxId, Vec3f* pos) {
    if (sPlayMmSfx == NULL) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
    }
    if (sPlayMmSfx != NULL) {
        sPlayMmSfx(sfxId, pos);
    }
}

void Remains_StopMmSfx(u16 sfxId) {
    if (sStopMmSfx == NULL) {
        sStopMmSfx = (MmSfxStopFunc)sApi->FindService(MM_SFX_STOP_SERVICE);
    }
    if (sStopMmSfx != NULL) {
        sStopMmSfx(sfxId);
    }
}

static LinkAnimationHeader* OdolwaAnimation(OdolwaAnim anim) {
    if (sOdolwaAnims[anim] == NULL && sApi->HasResource(sOdolwaAnimPaths[anim])) {
        sOdolwaAnims[anim] = ResourceMgr_LoadPlayerAnimAsHeader(sOdolwaAnimPaths[anim]);
    }
    return sOdolwaAnims[anim];
}

// A Majora's Mask player animation keeps its root elsewhere: OoT's base translation is forced into a private copy.
static LinkAnimationHeader* GohtAnimation(GohtAnim anim) {
    if (sGohtAnimFrames[anim] != NULL) {
        return &sGohtAnims[anim];
    }
    if (!sApi->HasResource(sGohtAnimPaths[anim])) {
        return (LinkAnimationHeader*)sGohtFallbackAnims[anim];
    }
    LinkAnimationHeader* source = ResourceMgr_LoadPlayerAnimAsHeader(sGohtAnimPaths[anim]);
    if (source == NULL || source->common.frameCount <= 0) {
        return (LinkAnimationHeader*)sGohtFallbackAnims[anim];
    }
    size_t values = (size_t)source->common.frameCount * MM_ANIM_VALUES_PER_FRAME;
    s16* frames = (s16*)malloc(values * sizeof(s16));
    if (frames == NULL) {
        return (LinkAnimationHeader*)sGohtFallbackAnims[anim];
    }
    memcpy(frames, source->segment, values * sizeof(s16));
    for (s32 frame = 0; frame < source->common.frameCount; frame++) {
        frames[frame * MM_ANIM_VALUES_PER_FRAME] = MM_ANIM_BASE_TRANSL_X;
        frames[frame * MM_ANIM_VALUES_PER_FRAME + 2] = 0;
    }
    sGohtAnimFrames[anim] = frames;
    sGohtAnims[anim].common.frameCount = source->common.frameCount;
    sGohtAnims[anim].segment = frames;
    return &sGohtAnims[anim];
}

// Ends on the last real frame: one past it is the bind pose.
static f32 LastRealFrame(LinkAnimationHeader* anim) {
    return MAX(Animation_GetLastFrame(anim) - 1.0f, 0.0f);
}

// ── Wearing ─────────────────────────────────────────────────────────────────

static void StopOdolwaSfx(void) {
    static const u16 sounds[] = { MM_SE_EN_MIBOSS_GND1_OLD, MM_SE_EN_MB_MOTH_FLY, MM_SE_EN_MIBOSS_VOICE1_OLD,
                                  MM_SE_EN_MIBOSS_VOICE2_OLD };

    for (u32 i = 0; i < ARRAY_COUNT(sounds); i++) {
        Remains_StopMmSfx(sounds[i]);
    }
}

static void TakeOff(void) {
    if (sWornRemains == REMAINS_ODOLWA) {
        StopOdolwaSfx();
    }
    sWornRemains = REMAINS_NONE;
}

void Remains_Toggle(PlayState* play, Player* player, s32 remains) {
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        return;
    }
    if (IsWorn(remains)) {
        TakeOff();
        Player_PlaySfx(&player->actor, NA_SE_PL_TAKE_OUT_SHIELD);
        return;
    }
    TakeOff();
    typedef void (*RemoveMaskFunc)(PlayState*,Player*);
    RemoveMaskFunc remove=SOH_MOD_API_HAS(sApi,FindService)?(RemoveMaskFunc)sApi->FindService("nei.masks.take_off"):NULL;
    if(remove)remove(play,player);
    sWornRemains = remains;
    player->currentMask = PLAYER_MASK_NONE;
    Player_PlaySfx(&player->actor, NA_SE_PL_CHANGE_ARMS);
}

void Remains_Forget(s32 remains) {
    if (IsWorn(remains)) {
        TakeOff();
    }
}

bool Remains_IsOdolwaFlying(void) {
    return sFlightState == 2;
}

s32 Remains_GetWorn(void) {return sWornRemains;}
static bool OdolwaOwnsShield(void) {return IsWorn(REMAINS_ODOLWA);}
static u32 RemainsSavedEnv(PlayState* play) {
 Gfx* begin=play->state.gfxCtx->polyOpa.bufp;Gfx* cursor=play->state.gfxCtx->polyOpa.p;
 while(cursor>begin){--cursor;if(((cursor->words.w0>>24)&255)==G_SETENVCOLOR)return (u32)cursor->words.w1;}
 return 0xFFFFFFFF;
}
static void RemainsRestoreEnv(PlayState* play,u32 color) {
 OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Opa(play->state.gfxCtx);
 gDPSetPrimColor(POLY_OPA_DISP++,0,0,255,255,255,255);
 gDPSetEnvColor(POLY_OPA_DISP++,color>>24,(color>>16)&255,(color>>8)&255,color&255);
 CLOSE_DISPS(play->state.gfxCtx);
}
static s16 sCraterTimer;static Vec3f sCraterPos;
static bool sGyorgFast;static s16 sGyorgYaw,sGyorgPitch;
static void DrawWornMask(PlayState* play) {
    if (CVarGetInteger("gMods.MMMasks.Hide",0) || !sApi->HasResource(sMaskDLs[sWornRemains])) {
        return;
    }
    f32 scale = sMaskScales[sWornRemains] * MASK_SCALE_CORRECTION;

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_RotateZYX(DegreesToBinang(180), DegreesToBinang(-90), DegreesToBinang(15), MTXMODE_APPLY);
    Matrix_Translate(0.0f, sMaskTranslateY[sWornRemains] + MASK_OFFSET_Y, sMaskTranslateZ[sWornRemains] + MASK_OFFSET_Z,
                     MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMaskDLs[sWornRemains]);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool IsHoldingSword(Player* player) {
    return player->leftHandType == PLAYER_MODELTYPE_LH_SWORD || player->leftHandType == PLAYER_MODELTYPE_LH_SWORD_2 ||
           player->leftHandType == PLAYER_MODELTYPE_LH_BGS;
}

// Odolwa's equipment is authored at boss scale; the fit was tuned in game.
static void DrawOdolwaEquipment(PlayState* play, const char* dl, f32 scale, Vec3s rot, Vec3f offset) {
    if (!sApi->HasResource(dl)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sSegment0xCNoop);
    Matrix_Push();
    Matrix_Translate(offset.x, offset.y, offset.z, MTXMODE_APPLY);
    Matrix_RotateZYX(rot.x, rot.y, rot.z, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawOnLimb(PlayState* play, Player* player, int32_t limbIndex) {
    if (sWornRemains == REMAINS_NONE || player->actor.scale.y < 0.0f) {
        return;
    }
    u32 saved=RemainsSavedEnv(play);
    if (limbIndex == PLAYER_LIMB_HEAD) {
        DrawWornMask(play);
        RemainsRestoreEnv(play,saved);
        return;
    }
    if (!IsWorn(REMAINS_ODOLWA)) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_L_HAND && IsHoldingSword(player)) {
        Vec3s rot = { 0, DegreesToBinang(7), DegreesToBinang(75) };
        DrawOdolwaEquipment(play, sOdolwaSwordDL, 0.4f, rot, (Vec3f){ -21.0f, 393.0f, -157.0f });
    } else if (limbIndex == PLAYER_LIMB_R_HAND && player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD) {
        Vec3s rot = { DegreesToBinang(-90), DegreesToBinang(90), 0 };
        DrawOdolwaEquipment(play, sOdolwaShieldDL, 0.7f, rot, (Vec3f){ 0.0f, 0.0f, 0.0f });
    } else if(limbIndex==PLAYER_LIMB_SHEATH && player->rightHandType!=PLAYER_MODELTYPE_RH_SHIELD && player->currentShield!=PLAYER_SHIELD_NONE) {
        Vec3s rot={DegreesToBinang(-90),DegreesToBinang(90),0};
        DrawOdolwaEquipment(play,sOdolwaShieldDL,.7f,rot,(Vec3f){0,0,0});
    }
    RemainsRestoreEnv(play,saved);
}

// Odolwa trades Link's sword and shield for his own, drawn over a bare hand; Goht carries no sword at all.
static void HideNativeEquipment(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    bool hidesSword = IsWorn(REMAINS_ODOLWA) || IsWorn(REMAINS_GOHT);
    if(IsWorn(REMAINS_ODOLWA) && limbIndex==PLAYER_LIMB_SHEATH) {
        static const ALIGN_ASSET(2) char bare[2][80]={"__OTR__objects/object_link_boy/gLinkAdultSheathNearDL","__OTR__objects/object_link_child/gLinkChildSheathNearDL"};
        static const ALIGN_ASSET(2) char sword[2][96]={"__OTR__objects/object_link_boy/gLinkAdultMasterSwordAndSheathNearDL","__OTR__objects/object_link_child/gLinkChildSwordAndSheathNearDL"};
        *dList=(Gfx*)(player->sheathType==PLAYER_MODELTYPE_SHEATH_18?sword[gSaveContext.linkAge]:bare[gSaveContext.linkAge]);
    }


    if (limbIndex == PLAYER_LIMB_L_HAND && hidesSword && IsHoldingSword(player)) {
        *dList = (Gfx*)(LINK_IS_ADULT ? gLinkAdultLeftHandClosedNearDL : gLinkChildLeftFistNearDL);
    } else if (limbIndex == PLAYER_LIMB_R_HAND && IsWorn(REMAINS_ODOLWA) &&
               player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD) {
        *dList = (Gfx*)(LINK_IS_ADULT ? gLinkAdultRightHandNearDL : gLinkChildRightHandNearDL);
    }
}

// ── Odolwa ──────────────────────────────────────────────────────────────────

static void ResolveIdleAnim(int32_t group, int32_t animType, LinkAnimationHeader** anim) {
    if (group != PLAYER_ANIMGROUP_wait || !IsWorn(REMAINS_ODOLWA)) {
        return;
    }
    LinkAnimationHeader* ready = OdolwaAnimation(ODOLWA_ANIM_READY);
    if (ready != NULL) {
        *anim = ready;
    }
}

// A locked dance Link cannot walk out of, looped long enough to cover the chant.
static void StartSummonDance(PlayState* play, Player* player, OdolwaAnim animId, u16 chant) {
    LinkAnimationHeader* anim = OdolwaAnimation(animId);

    if (anim == NULL) {
        return;
    }
    sSummonAnim = anim;
    sSummonFrame = 0.0f;
    sSummonLock = ODOLWA_SUMMON_DANCE_FRAMES;
    sSummonChant = chant;
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_LOOP,
                         -4.0f);
    player->linearVelocity = 0.0f;
    Remains_PlayMmSfx(chant, &player->actor.projectedPos);
}

static void EndSummonChant(void) {
    if (sSummonChant != 0) {
        Remains_StopMmSfx(sSummonChant);
        sSummonChant = 0;
    }
}

static void HoldSummonDance(PlayState* play, Player* player) {
    f32 last = Animation_GetLastFrame(sSummonAnim);

    sSummonLock--;
    player->linearVelocity = 0.0f;
    sSummonFrame += 1.0f;
    if (last > 0.0f && sSummonFrame > last) {
        sSummonFrame -= last;
    }
    LinkAnimation_Change(play, &player->skelAnime, sSummonAnim, 1.0f, sSummonFrame, last, ANIMMODE_ONCE, 0.0f);
    if (sSummonLock == 0) {
        EndSummonChant();
    }
}

// Soft soil stands in for Majora's Mask's deku flowers: only there does A summon the moths instead of running.
static bool IsNearSoftSoil(PlayState* play, Player* player) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_OBJ_BEAN && Actor_WorldDistXZToActor(&player->actor, actor) < ODOLWA_SOIL_RANGE) {
            return true;
        }
    }
    return false;
}

static void LiftOffOnCloud(PlayState* play, Player* player) {
    sFlightState = 2;
    sSummonLock = 0;
    EndSummonChant();
    sFlightTimer = ODOLWA_FLIGHT_MAX_FRAMES;
    sFlightYaw = player->actor.shape.rot.y;
    sFlightPitch = 0;
    player->actor.velocity.y = 4.0f;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    for (s32 i = 0; i < 8; i++) {
        Vec3f pos = player->actor.world.pos;
        pos.y -= 8.0f;
        RemainsAllyBug_SpawnCloud(play, &pos, player->actor.shape.rot.y);
    }
}

// Every exit restores gravity: the flight zeroed it, and Link would otherwise keep floating.
static void LandFromCloud(Player* player) {
    sFlightState = 0;
    player->actor.gravity = -2.0f;
    Remains_StopMmSfx(MM_SE_EN_MB_MOTH_FLY);
}

// The stick aims pitch and yaw like the Zora free swim, A flies forward, and the crouch pose stays frozen.
static void FlyOnCloud(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];
    bool isLeaving = IsHeld(play, BTN_R) || (IsGrounded(player) && !IsHeld(play, BTN_A));

    if (--sFlightTimer <= 0 || isLeaving) {
        LandFromCloud(player);
        return;
    }
    LinkAnimationHeader* crouch = OdolwaAnimation(ODOLWA_ANIM_CROUCH);
    if (crouch != NULL) {
        LinkAnimation_Change(play, &player->skelAnime, crouch, 0.0f, 0.0f, 0.0f, ANIMMODE_ONCE, 0.0f);
    }
    sFlightYaw -= (s16)(input->rel.stick_x * ODOLWA_FLIGHT_TURN_RATE);
    sFlightPitch -= (s16)(input->rel.stick_y * ODOLWA_FLIGHT_TURN_RATE);
    sFlightPitch = CLAMP(sFlightPitch, -ODOLWA_FLIGHT_PITCH_MAX, ODOLWA_FLIGHT_PITCH_MAX);
    player->actor.world.rot.y = sFlightYaw;
    player->actor.shape.rot.y = sFlightYaw;
    player->yaw = sFlightYaw;
    player->actor.gravity = 0.0f;
    if (!IsHeld(play, BTN_A)) {
        player->linearVelocity = 0.0f;
        player->actor.velocity.y = 0.0f;
        return;
    }
    player->linearVelocity = ODOLWA_FLIGHT_SPEED * Math_CosS(sFlightPitch);
    player->actor.velocity.y = ODOLWA_FLIGHT_SPEED * Math_SinS(sFlightPitch);
    if ((play->gameplayFrames & 7) == 0) {
        Remains_PlayMmSfx(MM_SE_EN_MB_MOTH_FLY, &player->actor.projectedPos);
    }
}

static void UpdateOdolwaFlight(PlayState* play, Player* player) {
    if (sSummonLock > 0 && sSummonAnim != NULL) {
        HoldSummonDance(play, player);
        if (sFlightState != 1) {
            return;
        }
    }
    bool canTakeOff = sFlightState == 0 && IsPressed(play, BTN_A) && !IsHeld(play, BTN_R) && IsGrounded(player) &&
                      play->msgCtx.msgMode == MSGMODE_NONE && IsNearSoftSoil(play, player);
    if (canTakeOff) {
        StartSummonDance(play, player, ODOLWA_ANIM_MOTH_DANCE, MM_SE_EN_MIBOSS_VOICE2_OLD);
        sFlightState = 1;
        sFlightWindup = ODOLWA_SUMMON_DANCE_FRAMES;
    } else if (sFlightState == 1) {
        player->linearVelocity = 0.0f;
        if (--sFlightWindup <= 0) {
            LiftOffOnCloud(play, player);
        }
    } else if (sFlightState == 2) {
        FlyOnCloud(play, player);
    }
}

static void SummonBeetles(PlayState* play, Player* player) {
    if (!SpendMagic(ODOLWA_BEETLE_MAGIC_COST)) {
        return;
    }
    for (s32 i = 0; i < 6; i++) {
        Vec3f pos = player->actor.world.pos;
        pos.x += Rand_CenteredFloat(70.0f);
        pos.y += 15.0f + Rand_ZeroFloat(30.0f);
        pos.z += Rand_CenteredFloat(70.0f);
        RemainsAllyBug_Spawn(play, &pos, (s16)Rand_CenteredFloat(60000.0f));
    }
    StartSummonDance(play, player, ODOLWA_ANIM_SWING_DANCE, MM_SE_EN_MIBOSS_VOICE1_OLD);
}

static void UpdateOdolwa(PlayState* play, Player* player) {
    sIsOdolwaRunBoosted = IsHeld(play, BTN_A) && !Remains_IsOdolwaFlying();
    if (sIsOdolwaRunBoosted && player->actor.speedXZ > 1.0f && --sOdolwaStepTimer <= 0) {
        Remains_PlayMmSfx(MM_SE_EN_MIBOSS_GND1_OLD, &player->actor.projectedPos);
        sOdolwaStepTimer = 8;
    }
    if (sOdolwaShieldFireTimer > 0) {
        sOdolwaShieldFireTimer--;
    }
    if (IsHeld(play, BTN_R) && IsPressed(play, BTN_B)) {
        SummonBeetles(play, player);
    }
    UpdateOdolwaFlight(play, player);
}

// A sword swing with a foe locked on sends a moth at it.
static void ResolveAction(PlayState* play, Player* player, int32_t action, bool* consumed, bool* startedAction) {
    if (action == SOH_PLAYER_ACTION_ROLL && (IsWorn(REMAINS_ODOLWA) || IsWorn(REMAINS_GOHT))) {
        *consumed = true;
        *startedAction = false;
        return;
    }
    bool isSwordMoth = action == SOH_PLAYER_ACTION_MELEE && IsWorn(REMAINS_ODOLWA) && player->focusActor != NULL &&
                       IsPressed(play, BTN_B) && !IsHeld(play, BTN_R);
    if (!isSwordMoth || !SpendMagic(ODOLWA_MOTH_MAGIC_COST)) {
        return;
    }
    Vec3f pos = player->bodyPartsPos[PLAYER_BODYPART_WAIST];
    RemainsAllyBug_SpawnProjectile(play, &pos, Actor_WorldYawTowardActor(&player->actor, player->focusActor));
    Remains_PlayMmSfx(MM_SE_EN_MB_MOTH_FLY, &player->actor.projectedPos);
}

// A blocked hit sends out the Fire Medallion's ground fire, when useful_medallions is installed.
static void FireFromShield(PlayState* play, Player* player, Actor* attacker) {
    if (!IsWorn(REMAINS_ODOLWA) || sOdolwaShieldFireTimer > 0) {
        return;
    }
    s16 fireId = sApi->GetActorId("sw97.magic_fire");
    if (fireId < 0) {
        return;
    }
    sOdolwaShieldFireTimer = 30;
    Actor_Spawn(&play->actorCtx, play, fireId, player->actor.world.pos.x, player->actor.world.pos.y,
                player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, 0);
}

// Ghosts of Link's own posed skeleton, tinted purple at the fog stage, which every limb combiner passes through.
static void DrawOdolwaTrail(Player* player, PlayState* play) {
    bool isRunning = IsWorn(REMAINS_ODOLWA) && sIsOdolwaRunBoosted && player->actor.speedXZ >= 1.0f;

    if (!isRunning) {
        for (s32 i = 0; i < ODOLWA_TRAIL_LENGTH; i++) {
            sTrail[i].isValid = false;
        }
        return;
    }
    s32 limbCount = MIN(player->skelAnime.limbCount, PLAYER_LIMB_MAX - 1);
    TrailSample* current = &sTrail[sTrailHead];
    current->pos = player->actor.world.pos;
    current->yaw = player->actor.shape.rot.y;
    current->isValid = true;
    memcpy(current->joints, player->skelAnime.jointTable, (limbCount + 1) * sizeof(Vec3s));
    sTrailHead = (sTrailHead + 1) % ODOLWA_TRAIL_LENGTH;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetFogColor(POLY_OPA_DISP++, 55, 12, 90, 255);
    gSPFogFactor(POLY_OPA_DISP++, 0, 120);
    for (s32 i = 0; i < ODOLWA_TRAIL_LENGTH; i++) {
        TrailSample* sample = &sTrail[i];
        if (!sample->isValid || sample == current) {
            continue;
        }
        Matrix_Translate(sample->pos.x, sample->pos.y, sample->pos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(sample->yaw), MTXMODE_APPLY);
        Matrix_Scale(player->actor.scale.x, player->actor.scale.y, player->actor.scale.z, MTXMODE_APPLY);
        SkelAnime_DrawFlexOpa(play, player->skelAnime.skeleton, sample->joints, player->skelAnime.dListCount,
                              Player_OverrideLimbDrawGameplayDefault, NULL, player);
    }
    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ── Goht ────────────────────────────────────────────────────────────────────

// In OoT the B button is derived from the sword equipment, so Goht unequips the sword itself and stashes both.
static void SyncGohtSword(PlayState* play) {
    u8 stash[2] = { ITEM_NONE, EQUIP_VALUE_SWORD_NONE };

    if (IsWorn(REMAINS_GOHT) && !sIsGohtSwordStashed) {
        stash[0] = gSaveContext.equips.buttonItems[0];
        stash[1] = (u8)CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
        sApi->StorageSet(STORAGE_KEY, GOHT_SWORD_FIELD, stash, sizeof(stash));
        Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
        gSaveContext.equips.buttonItems[0] = ITEM_NONE;
        gSaveContext.buttonStatus[0] = ITEM_NONE;
        Interface_LoadItemIcon1(play, 0);
        sIsGohtSwordStashed = true;
        return;
    }
    if (IsWorn(REMAINS_GOHT)) {
        return;
    }
    // Also recovers a sword left stashed by a save made while Goht was worn.
    sApi->StorageGet(STORAGE_KEY, GOHT_SWORD_FIELD, stash, sizeof(stash));
    if (stash[0] == ITEM_NONE || (!sIsGohtSwordStashed && gSaveContext.equips.buttonItems[0] != ITEM_NONE)) {
        sIsGohtSwordStashed = false;
        return;
    }
    if (stash[1] != EQUIP_VALUE_SWORD_NONE) {
        Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, stash[1]);
    }
    gSaveContext.equips.buttonItems[0] = stash[0];
    gSaveContext.buttonStatus[0] = stash[0];
    stash[0] = ITEM_NONE;
    stash[1] = EQUIP_VALUE_SWORD_NONE;
    sApi->StorageSet(STORAGE_KEY, GOHT_SWORD_FIELD, stash, sizeof(stash));
    Interface_LoadItemIcon1(play, 0);
    sIsGohtSwordStashed = false;
}

static void InitGohtColliders(PlayState* play, Player* player) {
    if (sAreGohtCollidersReady) {
        return;
    }
    Collider_InitCylinder(play, &sGohtQuakeCollider);
    Collider_SetCylinder(play, &sGohtQuakeCollider, &player->actor, &sGohtQuakeColliderInit);
    Collider_InitCylinder(play, &sGohtChargeCollider);
    Collider_SetCylinder(play, &sGohtChargeCollider, &player->actor, &sGohtChargeColliderInit);
    sAreGohtCollidersReady = true;
}

static void StrikeWith(PlayState* play, Player* player, ColliderCylinder* collider) {
    Collider_UpdateCylinder(&player->actor, collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &collider->base);
}

static Actor* FindEnemyInFront(PlayState* play, Player* player, f32 maxDist, s16 cone) {
    static const u8 categories[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };
    Actor* best = NULL;
    s16 bestDiff = cone;

    for (u32 i = 0; i < ARRAY_COUNT(categories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[categories[i]].head; actor != NULL; actor = actor->next) {
            if (actor->update == NULL || Actor_WorldDistXZToActor(&player->actor, actor) > maxDist) {
                continue;
            }
            s16 diff = ABS((s16)(Actor_WorldYawTowardActor(&player->actor, actor) - player->actor.shape.rot.y));
            if (diff < bestDiff) {
                bestDiff = diff;
                best = actor;
            }
        }
    }
    return best;
}

// Released after a long enough hold, the bolt reaches farther and hits harder the longer it charged.
static void UpdateGohtThunder(PlayState* play, Player* player) {
    bool canCast = gSaveContext.isMagicAcquired && gSaveContext.magic >= GOHT_THUNDER_MAGIC_COST;

    if (IsHeld(play, BTN_B) && !IsHeld(play, BTN_R) && canCast) {
        sGohtThunderCharge = MIN(sGohtThunderCharge + 1, GOHT_THUNDER_CHARGE_MAX);
        sIsGohtThunderCharging = true;
        if ((play->gameplayFrames & 7) == 0) {
            Remains_PlayMmSfx(MM_SE_EN_COMMON_THUNDER, &player->actor.projectedPos);
        }
        return;
    }
    if (sIsGohtThunderCharging && sGohtThunderCharge >= GOHT_THUNDER_CHARGE_MIN && SpendMagic(GOHT_THUNDER_MAGIC_COST)) {
        f32 charge = (f32)sGohtThunderCharge / (f32)GOHT_THUNDER_CHARGE_MAX;
        s16 ttl = GOHT_THUNDER_TTL_MIN + (s16)(charge * (GOHT_THUNDER_TTL_MAX - GOHT_THUNDER_TTL_MIN));
        s16 damage = GOHT_THUNDER_DAMAGE_MIN + (s16)(charge * (GOHT_THUNDER_DAMAGE_MAX - GOHT_THUNDER_DAMAGE_MIN));
        s16 yaw = player->actor.shape.rot.y;
        Actor* front = FindEnemyInFront(play, player, 900.0f, 0x5000);
        Vec3f pos = player->actor.world.pos;

        pos.x += Math_SinS(yaw) * 22.0f;
        pos.z += Math_CosS(yaw) * 22.0f;
        pos.y += 40.0f;
        RemainsAllyBug_SpawnThunderCharged(play, &pos,
                                           (front != NULL) ? Actor_WorldYawTowardActor(&player->actor, front) : yaw,
                                           ttl, damage);
        Remains_PlayMmSfx(MM_SE_EN_COMMON_THUNDER_THR, &player->actor.projectedPos);
    }
    sGohtThunderCharge = 0;
    sIsGohtThunderCharging = false;
}

// R+B throws one bombchu; R+B again sets it off where it stands.
static void UpdateGohtBombchu(PlayState* play, Player* player) {
    if (!IsHeld(play, BTN_R) || !IsPressed(play, BTN_B)) {
        return;
    }
    if (RemainsAllyChu_IsAlive()) {
        RemainsAllyChu_DetonateActive(play);
        return;
    }
    s16 yaw = player->actor.shape.rot.y;
    Vec3f pos = player->actor.world.pos;
    pos.x += Math_SinS(yaw) * 30.0f;
    pos.z += Math_CosS(yaw) * 30.0f;
    RemainsAllyChu_Spawn(play, &pos, yaw);
    Remains_PlayMmSfx(MM_SE_EN_BOMCHU_AIM, &player->actor.projectedPos);
}

static void StartGohtQuake(Player* player) {
    player->actor.velocity.y = GOHT_JUMP_VELOCITY;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    sGohtQuakeState = 1;
    sGohtQuakeAnimDelay = GOHT_JUMP_ANIM_DELAY;
    Remains_PlayMmSfx(MM_SE_EN_MIBOSS_JUMP1, &player->actor.projectedPos);
}

// Hitting a wall sets off a bomb there and bounces Link back off it, untouched by his own blast.
static void CrashIntoWall(PlayState* play, Player* player) {
    Vec3f pos = player->actor.world.pos;
    pos.x += Math_SinS(player->actor.shape.rot.y) * 30.0f;
    pos.z += Math_CosS(player->actor.shape.rot.y) * 30.0f;
    pos.y += 20.0f;
    EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, pos.x, pos.y, pos.z, 0,
                                      player->actor.shape.rot.y, 0, BOMB_BODY);
    if (bomb != NULL) {
        bomb->timer = 0;
    }
    player->invincibilityTimer = -40;
    player->linearVelocity = GOHT_BOUNCE_SPEED;
    player->actor.velocity.y = GOHT_BOUNCE_HOP;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    Player_PlaySfx(&player->actor, NA_SE_PL_BODY_BOUND);
    sIsGohtCharging = false;
    sGohtChargeCooldown = 25;
}

// Like a Goron roll it banks speed up a ramp and steers stiffly; magic only powers its hit and its cone.
static void RunGohtCharge(PlayState* play, Player* player) {
    f32 speedTarget;
    s16 yawTarget;

    if (IsGrounded(player)) {
        player->linearVelocity = GOHT_CHARGE_SPEED * Math_CosS(player->floorPitch);
        player->actor.velocity.y = GOHT_CHARGE_SPEED * Math_SinS(player->floorPitch);
    } else {
        player->linearVelocity = GOHT_CHARGE_SPEED;
    }
    if (Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, 0.018f, play)) {
        Math_ScaledStepToS(&player->actor.world.rot.y, yawTarget, GOHT_TURN_STEP);
    }
    player->actor.shape.rot.y = player->actor.world.rot.y;
    player->yaw = player->actor.world.rot.y;

    LinkAnimationHeader* run = GohtAnimation(GOHT_ANIM_CHARGE);
    f32 last = Animation_GetLastFrame(run);
    if (last <= 0.0f) {
        last = 8.0f;
    }
    sGohtChargeAnimFrame += GOHT_CHARGE_ANIM_SPEED;
    if (sGohtChargeAnimFrame >= last) {
        sGohtChargeAnimFrame -= last;
    }
    LinkAnimation_Change(play, &player->skelAnime, run, 1.0f, sGohtChargeAnimFrame, last, ANIMMODE_ONCE, 0.0f);
    if (--sGohtStepTimer <= 0) {
        Remains_PlayMmSfx(MM_SE_EN_ICEB_FOOTSTEP_OLD, &player->actor.projectedPos);
        sGohtStepTimer = 6;
    }
    if (gSaveContext.magic > 0) {
        StrikeWith(play, player, &sGohtChargeCollider);
        if (++sGohtChargeMagicTick >= GOHT_CHARGE_MAGIC_INTERVAL) {
            sGohtChargeMagicTick = 0;
            gSaveContext.magic--;
        }
    }
    if (player->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        CrashIntoWall(play, player);
    }
}

static void UpdateGohtQuake(PlayState* play, Player* player) {
    if (sGohtQuakeState == 1) {
        LinkAnimationHeader* jump = GohtAnimation(GOHT_ANIM_JUMP);
        f32 jumpLast = LastRealFrame(jump);
        if (sGohtQuakeAnimDelay > 0) {
            if (--sGohtQuakeAnimDelay == 0) {
                LinkAnimation_Change(play, &player->skelAnime, jump, 3.0f, 0.0f, jumpLast, ANIMMODE_ONCE, -3.0f);
                sGohtJumpPlayTimer = 5;
            }
        } else if (sGohtJumpPlayTimer > 0) {
            sGohtJumpPlayTimer--;
        } else {
            LinkAnimation_Change(play, &player->skelAnime, jump, 1.0f, jumpLast, jumpLast, ANIMMODE_LOOP, 0.0f);
        }
        if (player->actor.velocity.y <= 0.0f) {
            player->actor.velocity.y -= GOHT_JUMP_GRAVITY_BOOST;
        }
        if (IsGrounded(player) && player->actor.velocity.y <= 0.0f) {
            LinkAnimationHeader* land = GohtAnimation(GOHT_ANIM_JUMP_END);
            Actor_RequestQuake(play, 10, 16);
            Remains_PlayMmSfx(MM_SE_EN_ICEB_FOOTSTEP_OLD, &player->actor.projectedPos);
            LinkAnimation_Change(play, &player->skelAnime, land, 3.0f, 0.0f, LastRealFrame(land), ANIMMODE_ONCE, -2.0f);
            sCraterPos=player->actor.world.pos;sCraterTimer=180;
            sGohtQuakeState = 2;
            sGohtQuakeDamageFrames = 8;
        }
    } else if (sGohtQuakeState == 2) {
        if (sGohtQuakeDamageFrames-- > 0) {
            StrikeWith(play, player, &sGohtQuakeCollider);
        } else {
            sGohtQuakeState = 0;
            sGohtRecoverFrames = 8;
        }
    }
    // The pound ends in an attack pose; a few frames of plain idle reset it.
    if (sGohtRecoverFrames > 0 && !sIsGohtCharging && sGohtQuakeState == 0) {
        sGohtRecoverFrames--;
        LinkAnimation_Change(play, &player->skelAnime, (LinkAnimationHeader*)gPlayerAnim_link_normal_waitR_free, 1.0f,
                             0.0f, Animation_GetLastFrame((void*)gPlayerAnim_link_normal_waitR_free), ANIMMODE_LOOP,
                             -4.0f);
    }
}

static void ReleaseNoSnapFlag(Player* player) {
    if (sOwnsNoSnapFlag) {
        player->actor.bgCheckFlags &= ~BGCHECKFLAG_NO_SNAP;
        sOwnsNoSnapFlag = false;
    }
}

static void UpdateGoht(PlayState* play, Player* player) {
    bool isFree = !(player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) && play->msgCtx.msgMode == MSGMODE_NONE;

    InitGohtColliders(play, player);
    if (sGohtChargeCooldown > 0) {
        sGohtChargeCooldown--;
    }
    if (IsPressed(play, BTN_A) && IsHeld(play, BTN_R) && sGohtQuakeState == 0 && IsGrounded(player) && isFree) {
        StartGohtQuake(player);
    }
    // Starts only on the ground but keeps going through the air, so a ledge launches the charge.
    bool wantsCharge = IsHeld(play, BTN_A) && !IsHeld(play, BTN_R) && sGohtQuakeState == 0 &&
                       sGohtChargeCooldown == 0 && isFree;
    sIsGohtCharging = wantsCharge && (IsGrounded(player) || sIsGohtCharging);
    if (sIsGohtCharging) {
        player->actor.bgCheckFlags |= BGCHECKFLAG_NO_SNAP;
        sOwnsNoSnapFlag = true;
        RunGohtCharge(play, player);
    } else {
        sGohtChargeMagicTick = 0;
        ReleaseNoSnapFlag(player);
    }
    UpdateGohtQuake(play, player);
    UpdateGohtThunder(play, player);
    UpdateGohtBombchu(play, player);
}

static void DrawGohtCone(Player* player, PlayState* play) {
    if (!sIsGohtCharging || gSaveContext.magic <= 0 || !sApi->HasResource(sMajoraChantTex)) {
        return;
    }
    u32 frames = play->gameplayFrames;
    f32 sinY = Math_SinS(player->actor.shape.rot.y);
    f32 cosY = Math_CosS(player->actor.shape.rot.y);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(player->actor.world.pos.x + sinY * 59.0f, player->actor.world.pos.y + 20.0f,
                     player->actor.world.pos.z + cosY * 59.0f, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD((s16)-0x4000), MTXMODE_APPLY);
    Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(POLY_XLU_DISP++, sMajoraChantTex, G_IM_FMT_I, G_IM_SIZ_8b, 32, 64, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 6, G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock(POLY_XLU_DISP++, sMajoraChantTex, 0x0100, 1, G_IM_FMT_I, G_IM_SIZ_8b, 32, 64, 0,
                      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 6, 14, 14);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, -(s32)frames, (s32)(frames * 20), 0x20, 0x40, 1,
                                           -(s32)(frames * 2), (s32)(frames * 10), 0x20, 0x40));
    gSPDisplayList(POLY_XLU_DISP++, sGohtConeDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// BossHakugin's charging effect: two crossed bolts spinning around a light orb that grows with the charge.
static void DrawGohtChargingThunder(Player* player, PlayState* play) {
    if (!sIsGohtThunderCharging || sGohtThunderCharge <= 0 ||
        !sApi->HasResource(sGohtLightningModelDL) || !sApi->HasResource(sGohtOrbModelDL)) {
        return;
    }
    f32 charge = (f32)sGohtThunderCharge / (f32)GOHT_THUNDER_CHARGE_MAX;
    s16 yaw = player->actor.shape.rot.y;
    s16 spin = (s16)(play->gameplayFrames * 0x1000);
    Vec3f pos = { player->actor.world.pos.x + Math_SinS(yaw) * 22.0f, player->actor.world.pos.y + 40.0f,
                  player->actor.world.pos.z + Math_CosS(yaw) * 22.0f };
    f32 orb = 0.015f + 0.05f * charge;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 255, 255, 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sGohtLightningMaterialDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    for (s32 side = -1; side <= 1; side += 2) {
        Vec3s rot = { 0, yaw, 0 };
        Matrix_SetTranslateRotateYXZ(pos.x, pos.y, pos.z, &rot);
        Matrix_RotateY(BINANG_TO_RAD((s16)(0x1400 * side)), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD((s16)(0xC00 * side)), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(spin), MTXMODE_APPLY);
        Matrix_Scale(0.15f, 0.15f, 0.45f * charge, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sGohtLightningModelDL);
        Matrix_RotateZ(BINANG_TO_RAD((s16)0x4000), MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sGohtLightningModelDL);
    }
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetEnvColor(POLY_XLU_DISP++, 180, 255, 255, 0);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(orb, orb, orb, MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(spin), MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sGohtOrbMaterialDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sGohtOrbModelDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ── Gyorg ───────────────────────────────────────────────────────────────────

static void SummonFish(PlayState* play, Player* player) {
    for (s32 i = 0; i < 3; i++) {
        Vec3f pos = player->actor.world.pos;
        pos.x += Rand_CenteredFloat(90.0f);
        pos.z += Rand_CenteredFloat(90.0f);
        pos.y += 12.0f;
        RemainsAllyFish_Spawn(play, &pos, player->actor.shape.rot.y, (s16)i);
    }
    Remains_PlayMmSfx(MM_SE_EN_PIRANHA_ATTACK, &player->actor.projectedPos);
}

// A frozen enemy never registers its collider, so the paralysis lets one frame in fifteen through for the hits.
static void PullIntoWhirlpool(PlayState* play) {
    static const u8 categories[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };

    for (u32 i = 0; i < ARRAY_COUNT(categories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[categories[i]].head; actor != NULL; actor = actor->next) {
            f32 dist = Math_Vec3f_DistXZ(&actor->world.pos, &sWhirlpoolPos);
            if (actor->update == NULL || dist >= GYORG_WHIRLPOOL_RADIUS || dist <= 1.0f) {
                continue;
            }
            s16 pullYaw = Math_Vec3f_Yaw(&actor->world.pos, &sWhirlpoolPos);
            actor->world.pos.x += Math_SinS(pullYaw) * GYORG_WHIRLPOOL_PULL;
            actor->world.pos.z += Math_CosS(pullYaw) * GYORG_WHIRLPOOL_PULL;
            Math_ApproachF(&actor->world.pos.y, sWhirlpoolPos.y, 0.3f, GYORG_WHIRLPOOL_PULL * 0.6f);
            if ((play->gameplayFrames % 15) != 0) {
                actor->freezeTimer = 2;
            }
            Actor_SetColorFilter(actor, 0, 180, 0, 10);
        }
    }
}

// An AT latches AT_HIT on its first hit and stops connecting, so both rings are re-armed every frame.
static void RunWhirlpool(PlayState* play, Player* player) {
    if (!sAreWhirlpoolCollidersReady) {
        Collider_InitCylinder(play, &sWhirlpoolCollider);
        Collider_SetCylinder(play, &sWhirlpoolCollider, &player->actor, &sWhirlpoolColliderInit);
        Collider_InitCylinder(play, &sWhirlpoolAuraCollider);
        Collider_SetCylinder(play, &sWhirlpoolAuraCollider, &player->actor, &sWhirlpoolAuraColliderInit);
        sAreWhirlpoolCollidersReady = true;
    }
    // NEI forms the vortex at the mask's mouth, so it drags enemies onto Link's own damage ring.
    sWhirlpoolPos = player->actor.focus.pos;
    sWhirlpoolPos.y += GYORG_FUNNEL_OFFSET_Y;
    if (++sWhirlpoolDrain >= GYORG_WHIRLPOOL_DRAIN_RATE) {
        sWhirlpoolDrain = 0;
        gSaveContext.magic = MAX(gSaveContext.magic - 1, 0);
    }
    sWhirlpoolCollider.dim.pos.x = (s16)sWhirlpoolPos.x;
    sWhirlpoolCollider.dim.pos.y = (s16)sWhirlpoolPos.y;
    sWhirlpoolCollider.dim.pos.z = (s16)sWhirlpoolPos.z;
    sWhirlpoolCollider.base.atFlags = (sWhirlpoolCollider.base.atFlags | AT_ON | AT_TYPE_PLAYER) & ~AT_HIT;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sWhirlpoolCollider.base);
    Collider_UpdateCylinder(&player->actor, &sWhirlpoolAuraCollider);
    sWhirlpoolAuraCollider.base.atFlags = (sWhirlpoolAuraCollider.base.atFlags | AT_ON | AT_TYPE_PLAYER) & ~AT_HIT;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sWhirlpoolAuraCollider.base);
    if ((play->gameplayFrames & 7) == 0) {
        Player_PlaySfx(&player->actor, NA_SE_EV_DIVE_INTO_WATER);
    }
    PullIntoWhirlpool(play);
}

static void GyorgFastSwim(PlayState* play,Player* player) {
 bool active=player->actor.yDistToWater>20 && IsHeld(play,BTN_A);
 if(!active){if(sGyorgFast)player->actor.shape.rot.x=0;sGyorgFast=false;return;}
 if(!sGyorgFast){sGyorgYaw=player->actor.shape.rot.y;sGyorgPitch=0;}
 sGyorgFast=true;
 Input* input=&play->state.input[0];
 sGyorgYaw-=input->cur.stick_x*12;
 sGyorgPitch=CLAMP(sGyorgPitch+input->cur.stick_y*10,-0x3000,0x3000);
 Vec3f next=player->actor.world.pos;
 next.x+=Math_SinS(sGyorgYaw)*Math_CosS(sGyorgPitch)*9;
 next.z+=Math_CosS(sGyorgYaw)*Math_CosS(sGyorgPitch)*9;
 next.y+=Math_SinS(sGyorgPitch)*9;
 f32 surface=player->actor.world.pos.y+player->actor.yDistToWater;
 next.y=MIN(next.y,surface-15);
 if(player->actor.floorHeight>BGCHECK_Y_MIN)next.y=MAX(next.y,player->actor.floorHeight+15);
 Vec3f hit;CollisionPoly* poly=NULL;s32 bg;
 if(!BgCheck_AnyLineTest3(&play->colCtx,&player->actor.world.pos,&next,&hit,&poly,1,1,1,1,&bg))player->actor.world.pos=next;
 player->actor.prevPos=player->actor.world.pos;player->actor.velocity.y=0;player->linearVelocity=player->actor.speedXZ=0;
 player->actor.shape.rot.y=player->actor.world.rot.y=sGyorgYaw;player->actor.shape.rot.x=-sGyorgPitch;
 static const ALIGN_ASSET(2) char swim[]="__OTR__misc/link_animetion/gPlayerAnim_pz_fishswim_Data";
 if(sApi->HasResource(swim)) {
  LinkAnimationHeader* clip=ResourceMgr_LoadPlayerAnimAsHeader(swim);
  if(clip!=NULL && clip->common.frameCount>0) {
   AnimationContext_SetLoadFrame(play,clip,play->gameplayFrames%clip->common.frameCount,player->skelAnime.limbCount,player->skelAnime.jointTable);
   player->skelAnime.jointTable[0].x=MM_ANIM_BASE_TRANSL_X;player->skelAnime.jointTable[0].z=0;
  }
 }
 if((play->gameplayFrames&7)==0)EffectSsBubble_Spawn(play,&player->actor.world.pos,0,10,10,1);
}
typedef void (*ZoraSwimFunc)(PlayState*, Player*, bool);

// The zora mod lends its free swim to human Link, as NEI's Zora tunic swim did; may load after this mod.
static ZoraSwimFunc ZoraSwimService(void) {
    static ZoraSwimFunc swim;
    if (swim == NULL && SOH_MOD_API_HAS(sApi, FindService)) {
        swim = (ZoraSwimFunc)sApi->FindService("nei.zora.human_gyorg_swim");
    }
    return swim;
}

static void UpdateGyorg(PlayState* play, Player* player) {
    ZoraSwimFunc swim = ZoraSwimService();
    if (swim != NULL) {
        swim(play, player, true);
    } else {
        GyorgFastSwim(play, player);
    }
    bool isInWater = IsInWater(player);

    if (isInWater) {
        player->underwaterTimer = 0;
    }
    bool wantsFish = isInWater ? IsPressed(play, BTN_R) : (IsHeld(play, BTN_R) && IsPressed(play, BTN_B));
    if (wantsFish) {
        SummonFish(play, player);
    }
    sIsWhirlpoolActive = isInWater && IsHeld(play, BTN_B) && gSaveContext.magic > 0;
    if (sIsWhirlpoolActive) {
        RunWhirlpool(play, player);
    }
}

#include "remains_visuals.inc.c"

// ── Per frame ───────────────────────────────────────────────────────────────

// Whatever the remains changed in Link's physics goes back when it comes off, mid-flight or mid-charge.
static void ReleasePlayerOverrides(Player* player) {
    if (sFlightState == 2) {
        player->actor.gravity = -2.0f;
    }
    sFlightState = 0;
    sSummonLock = 0;
    EndSummonChant();
    sIsOdolwaRunBoosted = false;
    sIsGohtCharging = false;
    sGohtQuakeState = 0;
    sIsGohtThunderCharging = false;
    sGohtThunderCharge = 0;
    sIsWhirlpoolActive = false;
    ReleaseNoSnapFlag(player);
}

static void UpdateWornPowers(PlayState* play, Player* player) {
    switch (sWornRemains) {
        case REMAINS_ODOLWA:
            UpdateOdolwa(play, player);
            break;
        case REMAINS_GOHT:
            UpdateGoht(play, player);
            break;
        case REMAINS_GYORG:
            UpdateGyorg(play, player);
            break;
        case REMAINS_TWINMOLD:
        case REMAINS_NONE:
            break;
        default:
            sWornRemains = REMAINS_NONE;
            break;
    }
}

static void UpdateWornRemains(void) {
    if (gPlayState == NULL) {
        return;
    }
    PlayState* play = gPlayState;
    Player* player = GET_PLAYER(play);

    SyncGohtSword(play);
    // A native mask and a remains share Link's face: whichever went on last keeps it.
    if (sWornRemains != REMAINS_NONE && player->currentMask != PLAYER_MASK_NONE) {
        TakeOff();
    }
    if (!IsWorn(REMAINS_ODOLWA)) {
        if (sFlightState == 2 || sSummonLock > 0 || sIsOdolwaRunBoosted) {
            ReleasePlayerOverrides(player);
        }
    }
    if (!IsWorn(REMAINS_GOHT)) {
        sIsGohtCharging = false;
        sGohtQuakeState = 0;
        sIsGohtThunderCharging = false;
        ReleaseNoSnapFlag(player);
    }
    if (!IsWorn(REMAINS_GYORG)) {
        ZoraSwimFunc swim = ZoraSwimService();
        if (swim != NULL) {
            swim(play, player, false);
        }
        if (sGyorgFast) {
            player->actor.shape.rot.x = 0;
            sGyorgFast = false;
        }
        sIsWhirlpoolActive = false;
    }
    if (play->msgCtx.msgMode == MSGMODE_NONE) {
        UpdateWornPowers(play, player);
    } else {
        sIsWhirlpoolActive = false;
    }
    UpdateWhirlpoolFunnel(play, player);
}

static void DrawRemainsEffects(Actor* actor, PlayState* play) {
    DrawGroundCrater(play);
    DrawGyorgWhirlpool(play);
    Player* player = (Player*)actor;

    DrawOdolwaTrail(player, play);
    DrawGohtCone(player, play);
    DrawGohtChargingThunder(player, play);
}

static void ScaleMotion(Player* player, int32_t kind, f32* scale) {
    if (kind != SOH_PLAYER_MOTION_STICK_SPEED && kind != SOH_PLAYER_MOTION_SPEED_CAP) {
        return;
    }
    if (IsWorn(REMAINS_GYORG) && IsInWater(player)) {
        *scale *= GYORG_SWIM_MULTIPLIER;
    } else if (IsWorn(REMAINS_ODOLWA) && sIsOdolwaRunBoosted) {
        *scale *= ODOLWA_RUN_MULTIPLIER;
    }
}

static void StrengthenSword(PlayState* play, int32_t dmgFlags, u8* damage) {
    if (IsWorn(REMAINS_TWINMOLD)) {
        *damage = (u8)(*damage * TWINMOLD_DAMAGE_MULTIPLIER);
    }
}

static void SurviveChargeFall(bool* should, va_list args) {
    if (sIsGohtCharging) {
        *should = false;
    }
}

void Remains_ResetFile(void) {
    sWornRemains = REMAINS_NONE;
    sFlightState = 0;
    sSummonLock = 0;
    sSummonChant = 0;
    sIsOdolwaRunBoosted = false;
    sIsGohtCharging = false;
    sGohtQuakeState = 0;
    sIsGohtSwordStashed = false;
    sIsWhirlpoolActive = false;
    sOwnsNoSnapFlag = false;
    sAreGohtCollidersReady = false;
    sAreWhirlpoolCollidersReady = false;
    ForgetWhirlpoolFunnel();
}

static void ForgetSceneFunnel(int16_t sceneNum) {
    ForgetWhirlpoolFunnel();
}

void Remains_Init(const SOHModApi* api) {
    sApi = api;
    if(SOH_MOD_API_HAS(sApi,PublishService))sApi->PublishService("nei.remains.take_off",(void*)TakeOff);
    if(SOH_MOD_API_HAS(sApi,PublishService))sApi->PublishService("nei.mm_remains.shield_override",(void*)OdolwaOwnsShield);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, CaptureInput);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateWornRemains);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawOnLimb);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, HideNativeEquipment);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveAnim, ResolveIdleAnim);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, ScaleMotion);
    SOH_REGISTER_HOOK(sApi, OnPlayerActionHandler, ResolveAction);
    SOH_REGISTER_HOOK(sApi, OnPlayerShieldBlocked, FireFromShield);
    SOH_REGISTER_HOOK(sApi, OnResolveSwordDamage, StrengthenSword);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawRemainsEffects);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneFunnel);
    sApi->RegisterVB(VB_RECIEVE_FALL_DAMAGE, SurviveChargeFall);
}
