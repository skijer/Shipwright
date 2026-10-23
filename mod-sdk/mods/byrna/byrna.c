/** Cane of Byrna: the requested MHR actions, original-frame events, 1.4x playback and wrapped-angle interpolation. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "z64items.h"
#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "regs.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/ModApi/Player/PlayerHookTypes.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_dy_obj/object_dy_obj.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
#include "overlays/actors/ovl_Boss_Goma/z_boss_goma.h"
#include "overlays/actors/ovl_Boss_Dodongo/z_boss_dodongo.h"
#include "objects/object_kingdodongo/object_kingdodongo.h"
#define BYRNA_KEY "nei.equip.byrna"
#define BYRNA_SAMPLES 4
#define BYRNA_PLAY_SPEED 1.4f
#define BYRNA_NORMAL_DAMAGE 1
#define BYRNA_BOSS_DAMAGE 4
#define BYRNA_SWORD_FLAGS (DMG_SLASH_KOKIRI | DMG_SLASH_MASTER)
#define BYRNA_AIR_GRAVITY (-0.4f)
#define BYRNA_NORMAL_GRAVITY (-1.2f)
#define BYRNA_AIR_SPEED 18.0f
#define BYRNA_REBOUND_IMPULSE 6.0f
#define BYRNA_CYL_SEGMENTS 12
#define BYRNA_CYL_TRIS (BYRNA_CYL_SEGMENTS * 4)
#define CANE_POS_X (-887.74f)
#define CANE_POS_Y 988.20f
#define CANE_POS_Z 54.18f
#define CANE_ROT_X 1.0f
#define CANE_ROT_Z 49.7f
#define CANE_SCALE_X 7.76f
#define CANE_SCALE_Y 10.45f
#define CANE_SCALE_Z 7.71f
#define CANE_AXIS_MIN (-416.0f)
#define CANE_AXIS_MAX 223.0f
#define DEG_TO_BINANG 182.04f


typedef enum {
    CLIP_SHEATHE,
    CLIP_DRAW,
    CLIP_DASH,
    CLIP_SLASH,
    CLIP_AIR_B,
    CLIP_AIR_A,
    CLIP_REBOUND,
    CLIP_LAUNCH,
    CLIP_POUND,
    CLIP_HOP_RIGHT,
    CLIP_HOP_LEFT,
    CLIP_AIR_IDLE,
    CLIP_IDLE,
    CLIP_BACKFLIP,
    CLIP_WALK,
    CLIP_RUN,
    CLIP_MAX
} ClipId;
typedef struct { const char* path; bool ground; } ClipSource;
static const ClipSource sClipSources[CLIP_MAX] = {
    { "__OTR__misc/link_animetion/nei_byrna_ForwardMultiHitAerialStaffStrike", true },
    { "__OTR__misc/link_animetion/nei_byrna_ForwardRisingDoubleChargedStaffCombo", true },
    { "__OTR__misc/link_animetion/nei_byrna_ForwardRisingMultiHitAdvancingStaffSweep_Variant10", false },
    { "__OTR__misc/link_animetion/nei_byrna_ForwardRisingTripleAerialStaffStrike_Variant12", true },
    { "__OTR__misc/link_animetion/nei_byrna_LeftHighAerialMultiHitSilkbindStaffStrike", false },
    { "__OTR__misc/link_animetion/nei_byrna_ForwardSingleStaffTransition_Variant05", false },
    { "__OTR__misc/link_animetion/nei_byrna_LeftRisingDoubleChargedStaffCombo", false },
    { "__OTR__misc/link_animetion/nei_byrna_ForwardHighAerialTripleSilkbindStaffStrike_Variant12", false },
    { "__OTR__misc/link_animetion/nei_byrna_ForwardHighAerialMultiHitSilkbindStaffStrike_Variant20", false },
    { "__OTR__misc/link_animetion/nei_byrna_LeftMultiHitSilkbindVault", false },
    { "__OTR__misc/link_animetion/nei_byrna_RightRisingTripleSilkbindVault", false },
    { "__OTR__misc/link_animetion/nei_byrna_StationaryStaffReadyIdle_Variant02", false },
    { "__OTR__misc/link_animetion/nei_byrna_StationaryStaffReadyIdle_Variant14", true },
    { "__OTR__misc/link_animetion/nei_byrna_BackwardHighAerialSingleSilkbindStaffStrike", false },
    { "__OTR__misc/link_animetion/nei_byrna_ForwardStaffRun", true },
    { "__OTR__misc/link_animetion/nei_byrna_ForwardStaffRun", true },
};

static const ALIGN_ASSET(2) char sIcon[]="__OTR__textures/icon_item_custom/gItemIconCaneOfByrnaTex";
static const ALIGN_ASSET(2) char sName[]="__OTR__textures/item_name_custom/gCaneOfByrnaNameTex";
static const ALIGN_ASSET(2) char sCaneDL[]="__OTR__objects/object_somaria/g_byrna_cane_dl";
static const ALIGN_ASSET(2) char sCaneGiveDL[]="__OTR__objects/object_somaria/g_byrna_cane_give_dl";
static const char* const sGripDL[]={gLinkAdultLeftHandNearDL,gLinkChildLeftHandNearDL};
static const SOHModApi* sApi;
#define REMOTE_VISUAL_INTERVAL 5u

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual);
static void InitRemoteVisuals(void);
static uint32_t CaptureRemoteLimbEnv(PlayState* play);
static void RestoreRemoteLimbEnv(PlayState* play, uint32_t color);
s32 Player_SetupAction(PlayState*,Player*,PlayerActionFunc,s32);
void func_80839FFC(Player*,PlayState*);
void func_8008EC70(Player*);
int Player_IsZTargeting(Player*);
void Player_PlayVoiceSfx(Player*,u16);
s8 Player_ItemToItemAction(s32);
static void ApplyCaneMatrix(void);
#include "byrna_runtime.inc.c"
void BossGoma_SetupFloorStunned(BossGoma*);
void BossGoma_SetupFallStruckDown(BossGoma*);
void BossGoma_FloorStunned(BossGoma*,PlayState*);
void BossGoma_FallStruckDown(BossGoma*,PlayState*);
void BossGoma_FloorLandStruckDown(BossGoma*,PlayState*);
void BossGoma_CeilingMoveToCenter(BossGoma*,PlayState*);
void BossGoma_CeilingIdle(BossGoma*,PlayState*);
void BossGoma_CeilingPrepareSpawnGohmas(BossGoma*,PlayState*);
void BossGoma_CeilingSpawnGohmas(BossGoma*,PlayState*);
void BossGoma_Defeated(BossGoma*,PlayState*);
void BossDodongo_LayDown(BossDodongo*,PlayState*);
void BossDodongo_Vulnerable(BossDodongo*,PlayState*);
void BossDodongo_DeathCutscene(BossDodongo*,PlayState*);
static void StunByrnaBoss(Actor*,PlayState*);
#include "byrna_damage.inc.c"
#include "byrna_boss_stun.inc.c"
static void WieldWithBothHands(Player* player, int32_t itemAction, int32_t* modelGroup) {
    if (IsWorn() && IsSwordAction(itemAction)) {
        *modelGroup = PLAYER_MODELGROUP_BGS;
    }
}
static void GripCane(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (player != GET_PLAYER(gPlayState)) {
        RemoteVisualState visual;
        if (limbIndex == PLAYER_LIMB_L_HAND && ReadRemoteVisual(player, &visual)) {
            *dList = ResourceMgr_LoadGfxByName(sGripDL[gSaveContext.linkAge]);
        }
        return;
    }

    if (limbIndex == PLAYER_LIMB_L_HAND && IsWielding(player)) {
        *dList = ResourceMgr_LoadGfxByName(sGripDL[gSaveContext.linkAge]);
    }
}
static void ApplyCaneMatrix(void) {
    Matrix_Translate(CANE_POS_X, CANE_POS_Y, CANE_POS_Z, MTXMODE_APPLY);
    Matrix_RotateZYX((s16)(CANE_ROT_X * DEG_TO_BINANG), 0, (s16)(CANE_ROT_Z * DEG_TO_BINANG), MTXMODE_APPLY);
}
static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.20f, 0.20f, 0.20f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sCaneGiveDL);
    CLOSE_DISPS(play->state.gfxCtx);
}
#include "byrna_visuals.inc.c"
static bool sRemoteVisualReady;
static bool sRemoteVisualSent;
static uint32_t sRemoteVisualFrame;
static RemoteVisualState sLastRemoteVisual;

static void ReceiveRemoteVisual(const SOHNetMessage* message) {
    (void)message;
}

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual) {
    if (!sRemoteVisualReady || gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        return false;
    }
    uint32_t peerId = sApi->GetNetPeerIdForActor(&player->actor);
    if (peerId == 0 || sApi->GetNetPeerState(peerId, "nei.byrna.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->adult > 1 || visual->scene != gPlayState->sceneNum ||
        visual->flags[0] > 255 || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return false;
    }
    for (uint32_t i = 0; i < ARRAY_COUNT(visual->values); i++) {
        if (!isfinite(visual->values[i]) || fabsf(visual->values[i]) > 10000000.0f) {
            return false;
        }
    }
    return true;
}

static void PublishRemoteVisual(void) {
    if (!sRemoteVisualReady) {
        return;
    }
    RemoteVisualState visual = { 0 };
    PlayState* play = gPlayState;
    visual.scene = -1;
    if (play != NULL) {
        Player* player = GET_PLAYER(play);
        visual.scene = play->sceneNum;
        visual.adult = LINK_IS_ADULT;
        visual.active = (IsWielding(player));
        if (visual.active) {

        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (!sRemoteVisualSent || memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) != 0) {
        if (sApi->SetNetPeerState("nei.byrna.visual", &visual, sizeof(visual))) {
            sLastRemoteVisual = visual;
            sRemoteVisualSent = true;
            sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
        }
    }
}



static void InitRemoteVisuals(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterNetChannel) || !SOH_MOD_API_HAS(sApi, SetNetPeerState) ||
        !SOH_MOD_API_HAS(sApi, GetNetPeerState) || !SOH_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.byrna.visual", 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
    }
}



static uint32_t CaptureRemoteLimbEnv(PlayState* play) {
    Gfx* begin = play->state.gfxCtx->polyOpa.bufp;
    Gfx* cursor = play->state.gfxCtx->polyOpa.p;
    while (cursor > begin) {
        cursor--;
        if (((cursor->words.w0 >> 24) & 255) == G_SETENVCOLOR) {
            return (uint32_t)cursor->words.w1;
        }
    }
    return UINT32_MAX;
}

static void RestoreRemoteLimbEnv(PlayState* play, uint32_t color) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, color >> 24, (color >> 16) & 255, (color >> 8) & 255, color & 255);
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    CLOSE_DISPS(play->state.gfxCtx);
}

#include "byrna_register.inc.c"
