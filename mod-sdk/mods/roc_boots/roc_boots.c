#include <math.h>
/**
 * Roc's Boots: half gravity, and a floor wherever Link lands on water or on lava, quicksand and the void.
 */

#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/object_gi_hoverboots/object_gi_hoverboots.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

#define ROC_BOOTS_KEY "nei.equip.roc_boots"
#define DEKU_LEAF_KEY "nei.deku_leaf"
#define BGCHECK_ON_GROUND 0x0001
#define GI_DL_CAPACITY 512

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconRocBootsTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gRocBootsNameTex";

s32 func_80838144(s32 floorType);
int func_8083816C(s32 floorType);

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

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(ROC_BOOTS_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECK_ON_GROUND) != 0;
}

// Hot floors burn on a timer (func_80838144); the other three sink Link or void him out (func_8083816C).
static bool IsDeadlyFloor(s32 floorType) {
    return func_80838144(floorType) >= 0 || func_8083816C(floorType);
}

static bool IsIntegratingGravity(Player* player) {
    return !(player->stateFlags1 &
             (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
              PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DEAD)) &&
           !(player->stateFlags3 & PLAYER_STATE3_FLYING_WITH_HOOKSHOT);
}

// The leaf pins the fall speed itself and halves it for these boots.
static bool IsGliding(void) {
    const char* held = CustomItemRegistry_GetHeldKey();

    return held != NULL && strcmp(held, DEKU_LEAF_KEY) == 0;
}

// Link's gravity is rewritten every frame before it is integrated, so the half is given back afterwards: half of
// g to the velocity and half of g's step to the position leave both exactly where g/2 would have put them.
static void HalveGravity(void) {
    PlayState* play = gPlayState;
    Player* player;
    f32 gravity;

    if (play == NULL || !IsWorn()) {
        return;
    }
    player = GET_PLAYER(play);
    gravity = player->actor.gravity;
    if (gravity >= 0.0f || IsGrounded(player) || !IsIntegratingGravity(player) || IsGliding()) {
        return;
    }
    if (player->actor.velocity.y <= player->actor.minVelocityY) {
        return;
    }
    player->actor.velocity.y -= gravity * 0.5f;
    player->actor.world.pos.y -= gravity * 0.5f * (R_UPDATE_RATE * 0.5f);
}

// While Link is out of the water and not rising, its surface is his floor.
static void WalkOnWater(CollisionContext* colCtx, Vec3f* pos, Actor* actor, CollisionPoly** poly, int32_t* bgId,
                        float* floorY) {
    PlayState* play = gPlayState;
    WaterBox* waterBox;
    f32 surface;

    if (play == NULL || !IsWorn() || actor != &GET_PLAYER(play)->actor) {
        return;
    }
    if ((((Player*)actor)->stateFlags1 & PLAYER_STATE1_IN_WATER) || actor->velocity.y > 0.0f) {
        return;
    }
    if (!WaterBox_GetSurface1(play, colCtx, pos->x, pos->z, &surface, &waterBox) || surface <= *floorY) {
        return;
    }
    *floorY = surface;
}

// Skipping the assignment keeps the previous floor type, which these boots never let be a deadly one.
static void StandOnDeadlyFloors(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;

    if (!IsWorn() || play == NULL || !IsGrounded(player) || player->actor.floorPoly == NULL) {
        return;
    }
    if (IsDeadlyFloor(SurfaceType_GetFloorType(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId))) {
        *should = false;
    }
}

// The hover boots' get-item model in one metallic gold.
static u32 GoldRamp(u32 rgba) {
    u8 r = (rgba >> 24) & 0xFF;
    u8 g = (rgba >> 16) & 0xFF;
    u8 b = (rgba >> 8) & 0xFF;
    f32 lum = 0.299f * r + 0.587f * g + 0.114f * b;

    return ((u32)CLAMP_MAX(lum * 1.6f, 255.0f) << 24) | ((u32)CLAMP_MAX(lum * 1.22f, 255.0f) << 16) |
           ((u32)CLAMP_MAX(lum * 0.5f, 255.0f) << 8) | (rgba & 0xFF);
}

static bool IsTwoWordCommand(u8 opcode) {
    return opcode == 0x20 || opcode == 0x24 || opcode == 0x25 || opcode == 0x27 || opcode == 0x31 ||
           opcode == 0x32 || opcode == 0x33 || opcode == 0x35 || opcode == 0x36 || opcode == 0x42;
}

// A local copy, never the shared resource: the vanilla Hover Boots must keep their own colours.
static Gfx* BuildGoldBootsDL(void) {
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
            sDL[count - 1].words.w1 = GoldRamp((u32)sDL[count - 1].words.w1);
        }
    }
    sBuilt = true;
    return sDL;
}

static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    Gfx* boots = BuildGoldBootsDL();

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

// Keep the exact env colour emitted for this player, including custom tunics and cosmetics.
static u32 CurrentPlayerEnv(PlayState* play) {
    Gfx* begin=play->state.gfxCtx->polyOpa.bufp;
    Gfx* cursor=play->state.gfxCtx->polyOpa.p;
    while (cursor>begin) {
        --cursor;
        if (((cursor->words.w0>>24)&255)==G_SETENVCOLOR) { return (u32)cursor->words.w1; }
    }
    return 0xFFFFFFFF;
}
// Visual-only golden hover boot overlays; currentBoots and vanilla equipment remain Kokiri boots.
static void DrawGoldBootsOnFeet(PlayState* play, Player* player, int32_t limbIndex) {
    RemoteVisualState visual;
    bool visible = player == GET_PLAYER(gPlayState) ? IsWorn() : ReadRemoteVisual(player, &visual);
    if (!visible || (limbIndex != PLAYER_LIMB_L_FOOT && limbIndex != PLAYER_LIMB_R_FOOT)) { return; }
    u32 savedEnv=CurrentPlayerEnv(play);
    const char* path = limbIndex == PLAYER_LIMB_L_FOOT ? gLinkAdultLeftHoverBootDL : gLinkAdultRightHoverBootDL;
    // Scope both the model transform and the tint; never leave Link's tunic env colour white.
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, 255, 198, 65, 255);
    gSPGrayscale(POLY_OPA_DISP++, true);
    Matrix_Push();
    if (!LINK_IS_ADULT) { Matrix_Scale(0.65f, 0.65f, 0.65f, MTXMODE_APPLY); }
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)path);
    Matrix_Pop();
    gSPGrayscale(POLY_OPA_DISP++, false);
    gDPSetEnvColor(POLY_OPA_DISP++, savedEnv>>24, (savedEnv>>16)&255, (savedEnv>>8)&255, savedEnv&255);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Taking off custom boots writes 0 in the equipment slot; vanilla's draw requires a valid boot index.
static void KeepBootDrawValid(Actor* actor, PlayState* play, bool* drawVanilla) {
    if (gPlayState == NULL || actor != &GET_PLAYER(gPlayState)->actor) {
        return;
    }

    if (actor != &GET_PLAYER(play)->actor) { return; }
    if (CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS) < EQUIP_VALUE_BOOTS_KOKIRI ||
        CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS) >= EQUIP_VALUE_BOOTS_MAX) {
        Inventory_ChangeEquipment(EQUIP_TYPE_BOOTS, EQUIP_VALUE_BOOTS_KOKIRI);
        Player_SetEquipmentData(play, (Player*)actor);
    }
}

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate", "OnPlayerUpdate", "OnBgCheckRaycastFloor", "OnPlayerPostLimbDraw", "OnActorDraw" };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

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
    if (peerId == 0 || sApi->GetNetPeerState(peerId, "nei.roc_boots.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
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

        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (!sRemoteVisualSent || memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) != 0) {
        if (sApi->SetNetPeerState("nei.roc_boots.visual", &visual, sizeof(visual))) {
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
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.roc_boots.visual", 1, ReceiveRemoteVisual);
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
    definition.key = ROC_BOOTS_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rRoc's Boots&%wYou fall at half speed, and water,&lava and quicksand hold you up.";
    definition.getItemText = "You got the %rRoc's Boots%w!&Boots as light as the great&bird's feathers.^"
                             "Every jump carries further, and&you can walk across water and&lava.";
    definition.slot = SOH_EQUIP_SLOT_BOOTS;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_BOOTS;
    definition.column = 3;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_ANY;
    definition.vanillaBase = EQUIP_VALUE_BOOTS_KOKIRI;
    definition.toggles = 1;
    definition.getItemEntry.drawFunc = DrawGetItem;
    // The equipment registry creates the pickup; this block also puts it into new seeds.
    static const SOHCustomItemRandomizer randomizer = {
        sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_EQUIP, 0, 1,
        ROC_BOOTS_KEY, ROC_BOOTS_KEY, "",
    };
    definition.randomizer = &randomizer;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, HalveGravity);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawGoldBootsOnFeet);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_PLAYER, KeepBootDrawValid);
    SOH_REGISTER_HOOK(sApi, OnBgCheckRaycastFloor, WalkOnWater);
    sApi->RegisterVB(VB_SET_STATIC_FLOOR_TYPE, StandOnDeadlyFloors);
}
