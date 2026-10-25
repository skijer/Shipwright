#include <string.h>
#include <math.h>
/**
 * Magic Tunic: while the wallet holds rupees every hit is paid in rupees instead of hearts, and heat and water
 * stop counting down. A third of each payment spills out as real rupees. Broke, it is a heavy black shirt.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_gi_clothes/object_gi_clothes.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "soh/ModApi/Player/PlayerHookTypes.h"

#define MAGIC_TUNIC_KEY "nei.equip.magic_tunic"
#define RUPEE_DRAIN_INTERVAL 30
#define BROKE_SPEED_SCALE 0.5f
#define ABSORB_INTANGIBILITY 20
#define SPILL_RADIUS_MIN 8.0f
#define SPILL_RADIUS_MAX 20.0f
// Rupees collect within 50 units vertically: spawning above that window keeps them from being swallowed at once.
#define SPILL_HEIGHT_MARGIN 40.0f

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconMagicTunicTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gMagicTunicNameTex";

void Player_SetIntangibility(Player* player, s32 timer);
void Rumble_Request(f32 distSq, u8 strength, u8 duration, u8 decreaseRate);

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
static void CaptureLocalTunicColor(Color_RGB8* color, ...);
static s16 sDrainTimer;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(MAGIC_TUNIC_KEY);
}

// The accumulator holds what the wallet is still paying out, so it counts against the balance.
static s16 GetSpendableRupees(void) {
    return gSaveContext.rupees + gSaveContext.rupeeAccumulator;
}

static bool HasMoney(void) {
    return IsWorn() && GetSpendableRupees() > 0;
}

static bool IsPlayerBusy(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                                   PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) != 0;
}

static s16 TakeSpillDenomination(s16* spill) {
    if (*spill >= 20) {
        *spill -= 20;
        return ITEM00_RUPEE_RED;
    }
    if (*spill >= 5) {
        *spill -= 5;
        return ITEM00_RUPEE_BLUE;
    }
    *spill -= 1;
    return ITEM00_RUPEE_GREEN;
}

static void SpillRupees(PlayState* play, Player* player, s16 paid) {
    s16 spill = (paid * 3 + 9) / 10;
    f32 spawnY = player->actor.world.pos.y + Player_GetHeight(player) + SPILL_HEIGHT_MARGIN;

    while (spill > 0) {
        s16 params = TakeSpillDenomination(&spill);
        s16 angle = (s16)Rand_CenteredFloat(65536.0f);
        f32 radius = SPILL_RADIUS_MIN + Rand_ZeroOne() * (SPILL_RADIUS_MAX - SPILL_RADIUS_MIN);
        Vec3f pos = { player->actor.world.pos.x + Math_CosS(angle) * radius, spawnY,
                      player->actor.world.pos.z + Math_SinS(angle) * radius };

        Item_DropCollectible(play, &pos, params);
    }
}

static void PayForHit(PlayState* play, Player* player, s16 damage) {
    s16 paid = MIN(damage, GetSpendableRupees());

    Rupees_ChangeBy(-paid);
    Sfx_PlaySfxCentered(NA_SE_IT_SHIELD_BOUND);
    SpillRupees(play, player, paid);
}

static void FeelAbsorbedHit(Player* player) {
    Player_SetIntangibility(player, ABSORB_INTANGIBILITY);
    Rumble_Request(0.0f, 180, 20, 100);
}

// Runs before Player_UpdateCommon consumes the hit: paid for, it plays like a block, with no knockback, no hurt
// animation and no voice.
static void AbsorbHit(Player* player, Input* input) {
    PlayState* play = gPlayState;

    if (play == NULL || !HasMoney()) {
        return;
    }
    if ((player->cylinder.base.acFlags & AC_HIT) && player->actor.colChkInfo.damage > 0) {
        PayForHit(play, player, player->actor.colChkInfo.damage);
        player->actor.colChkInfo.damage = 0;
        player->cylinder.base.acFlags &= ~AC_HIT;
        FeelAbsorbedHit(player);
        return;
    }
    if (player->knockbackType != PLAYER_KNOCKBACK_NONE && player->knockbackDamage > 0) {
        PayForHit(play, player, player->knockbackDamage);
        player->knockbackType = PLAYER_KNOCKBACK_NONE;
        player->knockbackDamage = 0;
        FeelAbsorbedHit(player);
    }
}

// Burning floors, falls and voids take health without a collider: the loss is handed back the moment it lands.
static void RefundOtherDamage(int16_t amount) {
    PlayState* play = gPlayState;

    if (amount >= 0 || play == NULL || !HasMoney()) {
        return;
    }
    gSaveContext.health = MIN(gSaveContext.health - amount, gSaveContext.healthCapacity);
    PayForHit(play, GET_PLAYER(play), -amount);
}

static void DrainWallet(void) {
    PlayState* play = gPlayState;

    if (play == NULL || !IsWorn() || IsPlayerBusy(GET_PLAYER(play))) {
        return;
    }
    if (GetSpendableRupees() <= 0) {
        sDrainTimer = 0;
        return;
    }
    if (++sDrainTimer >= RUPEE_DRAIN_INTERVAL) {
        sDrainTimer = 0;
        Rupees_ChangeBy(-1);
    }
}

static void WeighDownWhenBroke(Player* player, int32_t kind, float* scale) {
    if (kind == SOH_PLAYER_MOTION_STICK_SPEED && IsWorn() && GetSpendableRupees() <= 0) {
        *scale *= BROKE_SPEED_SCALE;
    }
}

static void SkipHeatAndWater(PlayState* play, int16_t* hazard) {
    if (!HasMoney()) {
        return;
    }
    if (*hazard == PLAYER_ENV_HAZARD_HOTROOM || *hazard == PLAYER_ENV_HAZARD_UNDERWATER_FLOOR ||
        *hazard == PLAYER_ENV_HAZARD_UNDERWATER_FREE) {
        *hazard = PLAYER_ENV_HAZARD_NONE;
    }
}

static void DyeTunic(bool* should, va_list args) {
    Color_RGB8* color;

    Player* player = va_arg(args, Player*);
    color = va_arg(args, Color_RGB8*);
    if (gPlayState == NULL) {
        return;
    }
    if (player != GET_PLAYER(gPlayState)) {
        RemoteVisualState visual;
        if (ReadRemoteVisual(player, &visual)) {
            color->r = visual.flags[0];
            color->g = visual.flags[1];
            color->b = visual.flags[2];
        }
        return;
    }
    if (!IsWorn()) {
        return;
    }
    if (GetSpendableRupees() > 0) {
        color->r = 235;
        color->g = 110;
        color->b = 20;
    } else {
        color->r = 20;
        color->g = 20;
        color->b = 20;
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, 235, 110, 20, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiTunicCollarDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiTunicDL);
    gSPGrayscale(POLY_OPA_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ResetDrain(const char* key) {
    sDrainTimer = 0;
}

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate", "OnPlayerUpdate", "OnPlayerFilterInput", "OnPlayerHealthChange",
                                              "OnPlayerResolveMotionScale", "OnResolveEnvHazard" };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static void CaptureLocalTunicColor(Color_RGB8* color, ...) {
    va_list args;
    bool apply = true;
    va_start(args, color);
    DyeTunic(&apply, args);
    va_end(args);
}

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
    if (peerId == 0 || sApi->GetNetPeerState(peerId, "nei.magic_tunic.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
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
        visual.active = (IsWorn());
        if (visual.active) {
            Color_RGB8 color = { 0 };
            CaptureLocalTunicColor(&color, player, &color);
            visual.flags[0] = color.r;
            visual.flags[1] = color.g;
            visual.flags[2] = color.b;
        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (!sRemoteVisualSent || memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) != 0) {
        if (sApi->SetNetPeerState("nei.magic_tunic.visual", &visual, sizeof(visual))) {
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
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.magic_tunic.visual", 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
    }
}




SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    InitRemoteVisuals();
    SOHCustomEquipDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = MAGIC_TUNIC_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rMagic Tunic&%wWhile you carry rupees, hits cost&rupees instead of hearts and heat&"
                           "and water stop counting down.";
    definition.getItemText = "You got the %rMagic Tunic%w!&Armor woven from the wallet&itself.^"
                             "Every blow is paid in rupees&and some of them spill out;&"
                             "with an empty purse it only&weighs you down.";
    definition.slot = SOH_EQUIP_SLOT_TUNIC;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_TUNIC;
    definition.column = 2;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_ANY;
    definition.vanillaBase = EQUIP_VALUE_TUNIC_KOKIRI;
    definition.toggles = 1;
    definition.onUnequip = ResetDrain;
    definition.getItemEntry.drawFunc = DrawGetItem;
    // The equipment registry creates the pickup; this block also puts it into new seeds.
    static const SOHCustomItemRandomizer randomizer = {
        sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_EQUIP, 0, 1,
        MAGIC_TUNIC_KEY, MAGIC_TUNIC_KEY, "",
    };
    definition.randomizer = &randomizer;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, DrainWallet);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, AbsorbHit);
    SOH_REGISTER_HOOK(sApi, OnPlayerHealthChange, RefundOtherDamage);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, WeighDownWhenBroke);
    SOH_REGISTER_HOOK(sApi, OnResolveEnvHazard, SkipHeatAndWater);
    sApi->RegisterVB(VB_APPLY_TUNIC_COLOR, DyeTunic);
}
