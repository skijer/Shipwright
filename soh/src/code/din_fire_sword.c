#include "global.h"
#include "align_asset_macro.h"
#include "din_fire_sword.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "mods/transformation_masks/transformation_masks.h"
#include <libultraship/bridge/resourcebridge.h>
#include <math.h>
#include "mods/extended_equipment.h"
#include "mods/items/logic/weapon_upgrades.h"
extern s32 BossRemains_IsOdolwaWorn(void);
extern s32 BossRemains_IsGohtWorn(void);

#define SWORD_ROOT "objects/din_fire_sword/poc1/"
static const ALIGN_ASSET(2) char sCoreTexture[] = "__OTR__" SWORD_ROOT "CoreTex";
static const ALIGN_ASSET(2) char sFlameTexture[] = "__OTR__" SWORD_ROOT "FlameTex";
static const char* sCoreDL[] = { SWORD_ROOT "adult/CoreDL", SWORD_ROOT "child/CoreDL", SWORD_ROOT "bgs/CoreDL",
                                 SWORD_ROOT "broken/CoreDL" };
static const char* sFlameDL[] = { SWORD_ROOT "adult/FlameDL", SWORD_ROOT "child/FlameDL", SWORD_ROOT "bgs/FlameDL",
                                  SWORD_ROOT "broken/FlameDL" };
static const char* sCoreVertices[] = { SWORD_ROOT "adult/CoreVertices", SWORD_ROOT "child/CoreVertices",
                                       SWORD_ROOT "bgs/CoreVertices", SWORD_ROOT "broken/CoreVertices" };
static const char* sFlameVertices[] = { SWORD_ROOT "adult/FlameVertices", SWORD_ROOT "child/FlameVertices",
                                        SWORD_ROOT "bgs/FlameVertices", SWORD_ROOT "broken/FlameVertices" };
static const char* sEquipment[] = {
    "objects/object_link_boy/DinSleekEquipmentPOC1_OOT_Adult/SwordDL",
    "objects/object_link_child/DinSleekEquipmentPOC1_OOT_Child/SwordDL",
};

static struct {
    PlayState* play;
    Player* player;
    u32 lastFrame;
    u16 phase;
    s16 scene;
    s32 age;
} sSword;

static struct {
    PlayState* play;
    Player* player;
    Mtx* handMatrix;
} sDraw;

void DinFireSword_BeginPlayerDraw(PlayState* play, Player* player) {
    sDraw.play = play != NULL && player == GET_PLAYER(play) ? play : NULL;
    sDraw.player = sDraw.play != NULL ? player : NULL;
    sDraw.handMatrix = NULL;
}

static s32 DinFireSword_Profile(Player* player) {
    if (player->heldItemAction == PLAYER_IA_SWORD_BIGGORON && player->leftHandType == PLAYER_MODELTYPE_LH_BGS)
        return gSaveContext.swordHealth <= 0.0f ? 3 : 2;
    if (player->leftHandType == PLAYER_MODELTYPE_LH_SWORD &&
        player->heldItemAction == (LINK_IS_ADULT ? PLAYER_IA_SWORD_MASTER : PLAYER_IA_SWORD_KOKIRI))
        return gSaveContext.linkAge;
    return -1;
}

static s32 DinFireSword_Eligible(PlayState* play, Player* player) {
    if (play == NULL || player == NULL || player != GET_PLAYER(play) ||
        !CVarGetInteger(CVAR_ENHANCEMENT("DinFireSword"), 0) || !CVarGetInteger(CVAR_SETTING("AltAssets"), 1) ||
        TransformMasks_IsTransformedAny() || BossRemains_IsOdolwaWorn() || BossRemains_IsGohtWorn() ||
        GameInteractor_InvisibleLinkActive() || player->actor.scale.y <= 0.0f || player->csAction != 0 ||
        (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_WATER)) ||
        (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        DinFireSword_Profile(player) < 0) {
        return false;
    }
    return ResourceMgr_FileExists(sEquipment[gSaveContext.linkAge]);
}

static s32 DinFireSword_SameContext(PlayState* play, Player* player) {
    return sSword.play == play && sSword.player == player && sSword.scene == play->sceneNum &&
           sSword.age == gSaveContext.linkAge;
}

void DinFireSword_Reset(void) {
    sSword.play = NULL;
    sSword.player = NULL;
    sSword.phase = 0;
    sSword.lastFrame = UINT32_MAX;
    DinFireSword_BeginPlayerDraw(NULL, NULL);
}

void DinFireSword_Update(PlayState* play, Player* player) {
    if (play == NULL || player == NULL || player != GET_PLAYER(play))
        return;
    if (!DinFireSword_Eligible(play, player)) {
        DinFireSword_Reset();
        return;
    }
    if (!DinFireSword_SameContext(play, player) ||
        (sSword.lastFrame != UINT32_MAX && play->gameplayFrames < sSword.lastFrame)) {
        DinFireSword_Reset();
        sSword.play = play;
        sSword.player = player;
        sSword.scene = play->sceneNum;
        sSword.age = gSaveContext.linkAge;
    }
    if (play->pauseCtx.state != 0 || play->pauseCtx.debugState != 0 || sSword.lastFrame == play->gameplayFrames)
        return;
    sSword.lastFrame = play->gameplayFrames;
    sSword.phase = (sSword.phase + 1) & 1023;
}

static s32 DinFireSword_Load(s32 age, Gfx** core, Gfx** flame) {
    const char* paths[] = { sCoreDL[age],        sFlameDL[age],        sCoreVertices[age],
                            sFlameVertices[age], SWORD_ROOT "CoreTex", SWORD_ROOT "FlameTex" };
    for (size_t i = 0; i < ARRAY_COUNT(paths); ++i) {
        if (!ResourceMgr_FileExists(paths[i]))
            return false;
    }
    *core = ResourceMgr_LoadGfxByName(sCoreDL[age]);
    *flame = ResourceMgr_LoadGfxByName(sFlameDL[age]);
    return *core != NULL && *flame != NULL && ResourceGetDataByName(sCoreVertices[age]) != NULL &&
           ResourceGetDataByName(sFlameVertices[age]) != NULL && ResourceGetDataByName(SWORD_ROOT "CoreTex") != NULL &&
           ResourceGetDataByName(SWORD_ROOT "FlameTex") != NULL;
}

uint32_t DinFireSword_DamageFlags(PlayState* play, Player* player, uint32_t original) {
    if (!CVarGetInteger(CVAR_ENHANCEMENT("DinFireSwordDamage"), 0) || !(original & DMG_SWORD) ||
        (original & ~((u32)DMG_SWORD)) || !DinFireSword_Eligible(play, player) || ExtEquip_ShouldHideSwordDL() ||
        (player->heldItemAction == PLAYER_IA_SWORD_KOKIRI && WeaponUpgrade_KokiriLevel()) ||
        (player->heldItemAction == PLAYER_IA_SWORD_BIGGORON && WeaponUpgrade_HasGreatFairy()))
        return original;
    Gfx *core, *flame;
    if (!DinFireSword_Load(DinFireSword_Profile(player), &core, &flame))
        return original;
    // Keep sword-only collision masks and sword recognition intact. The owned
    // hit's table lookup below resolves fire without replacing sword power.
    return original | DMG_ARROW_FIRE;
}

// Crouch stabs intentionally reuse the previous strike's damage in vanilla.
// Remember that original value separately so toggling fire never poisons it.
static struct {
    PlayState* play;
    Player* player;
    s16 scene;
    s32 age;
    u32 original[2], applied[2];
    u8 known[2];
} sDamage;

static void DinFireSword_DamageContext(PlayState* play, Player* player) {
    if (sDamage.play != play || sDamage.player != player || sDamage.scene != play->sceneNum ||
        sDamage.age != gSaveContext.linkAge) {
        sDamage.play = play;
        sDamage.player = player;
        sDamage.scene = play->sceneNum;
        sDamage.age = gSaveContext.linkAge;
        sDamage.known[0] = sDamage.known[1] = false;
    }
}

uint32_t DinFireSword_SetDamageFlags(PlayState* play, Player* player, int quad, uint32_t original) {
    if (play == NULL || player == NULL || player != GET_PLAYER(play) || quad < 0 || quad > 1)
        return original;
    DinFireSword_DamageContext(play, player);
    sDamage.original[quad] = original;
    sDamage.applied[quad] = DinFireSword_DamageFlags(play, player, original);
    sDamage.known[quad] = true;
    return sDamage.applied[quad];
}

void DinFireSword_RefreshDamage(PlayState* play, Player* player) {
    if (play == NULL || player == NULL || player != GET_PLAYER(play))
        return;
    DinFireSword_DamageContext(play, player);
    for (int quad = 0; quad < 2; ++quad) {
        u32 flags = player->meleeWeaponQuads[quad].info.toucher.dmgFlags;
        if (sDamage.known[quad] && flags == sDamage.applied[quad])
            flags = sDamage.original[quad];
        player->meleeWeaponQuads[quad].info.toucher.dmgFlags = DinFireSword_SetDamageFlags(play, player, quad, flags);
    }
}

uint32_t DinFireSword_OriginalDamageFlags(PlayState* play, const ColliderInfo* hitInfo) {
    if (hitInfo == NULL)
        return 0;
    u32 flags = hitInfo->toucher.dmgFlags;
    if (play == NULL || sDamage.play != play || sDamage.player == NULL || sDamage.player != GET_PLAYER(play) ||
        sDamage.scene != play->sceneNum || sDamage.age != gSaveContext.linkAge)
        return flags;
    for (int quad = 0; quad < 2; ++quad) {
        if (hitInfo == &sDamage.player->meleeWeaponQuads[quad].info && sDamage.known[quad] &&
            flags == sDamage.applied[quad] && flags == (sDamage.original[quad] | DMG_ARROW_FIRE))
            return sDamage.original[quad];
    }
    return flags;
}

uint8_t DinFireSword_DamageEntry(PlayState* play, Actor* target, const ColliderInfo* hitInfo, uint32_t receiverFlags,
                                 uint8_t vanillaEntry) {
    if (target == NULL || target->colChkInfo.damageTable == NULL || hitInfo == NULL)
        return vanillaEntry;
    u32 original = DinFireSword_OriginalDamageFlags(play, hitInfo);
    if (original == hitInfo->toucher.dmgFlags)
        return vanillaEntry;

    DamageTable* table = target->colChkInfo.damageTable;
    u8 fireEntry = table->table[0x0B];
    // This receiver admitted only the added fire bit (e.g. a fire-only target).
    if (!(original & receiverFlags))
        return (receiverFlags & DMG_ARROW_FIRE) ? fireEntry : vanillaEntry;

    int index = 0;
    for (u32 flags = original; flags > 1; flags >>= 1)
        ++index;
    u8 swordEntry = table->table[index];
    if (!(receiverFlags & DMG_ARROW_FIRE))
        return swordEntry;

    // Anubis ignores sword-table damage and only dies on its FIRE effect.
    // Freezard's zero-damage Kokiri row is inert despite its normal-hit effect.
    if (target->id == ACTOR_EN_ANUBICE ||
        (((swordEntry == 0) || (target->id == ACTOR_EN_FZ && !(swordEntry & 0xF))) && (fireEntry & 0xF)))
        return fireEntry;

    // These actors share the ordinary damage/recoil path with their fire
    // reaction. Keep the sword's power and add that actor's own fire effect.
    // Other effects are not interchangeable: Baba cutting, jellyfish shock,
    // Armos KILL and Lizalfos projectile recoil must retain the sword row.
    if ((swordEntry >> 4) == 0 && (fireEntry & 0xF)) {
        switch (target->id) {
            case ACTOR_EN_WF:       // FIRE = 14; normal hit plus burn timer.
            case ACTOR_EN_WALLMAS:  // FIRE = 2; normal hit plus flame effect.
            case ACTOR_EN_FLOORMAS: // FIRE = 2; normal hit plus flame effect.
            case ACTOR_EN_CROW:     // FIRE = 2; normal fall plus flames.
            case ACTOR_EN_DEKUNUTS: // FIRE = 2; normal hit plus flame effect.
            case ACTOR_EN_PEEHAT:   // FIRE = 12; normal hit plus flames/filter.
            case ACTOR_EN_FIREFLY:  // FIRE_ARROW = 15; normal fall, ice Keese combust.
                return (swordEntry & 0xF) | (fireEntry & 0xF0);
        }
    }
    return swordEntry;
}

static void DinFireSword_Material(Gfx** display, s32 flame, Color_RGB8 core, Color_RGB8 outer, u16 phase) {
    s32 scroll = (phase * (flame ? 5 : 3)) & 127;
    u8 alpha = flame ? (u8)((0.96f + 0.04f * sinf(phase * 0.71f)) * 255.0f) : 255;
    gSPClearGeometryMode((*display)++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gDPSetCycleType((*display)++, G_CYC_2CYCLE);
    gDPSetRenderMode((*display)++, G_RM_PASS, flame ? G_RM_AA_ZB_XLU_SURF2 : G_RM_AA_ZB_OPA_SURF2);
    gDPSetTextureLUT((*display)++, G_TT_NONE);
    gDPSetTextureFilter((*display)++, G_TF_BILERP);
    gDPSetAlphaCompare((*display)++, G_AC_NONE);
    gSPTexture((*display)++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    if (flame) {
        gDPSetCombineLERP((*display)++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, SHADE, 0, 0, 0, 0,
                          COMBINED, COMBINED, 0, PRIMITIVE, 0);
    } else {
        // A continuous hot blade beneath the transparent tongues. The source
        // sword stays intact; the private close-fitting core covers its metal.
        gDPSetCombineLERP((*display)++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, SHADE, 0, 0, 0, COMBINED,
                          COMBINED, 0, PRIMITIVE, 0);
    }
    // Logical dimensions stay native. Named resources carry physical HD size.
    gDPLoadTextureBlock((*display)++, flame ? sFlameTexture : sCoreTexture, G_IM_FMT_I, G_IM_SIZ_8b, 64, 32, 0,
                        G_TX_WRAP, G_TX_WRAP, 6, 5, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetTileSize((*display)++, 0, 0, scroll, 63 << 2, scroll + (31 << 2));
    gDPSetPrimColor((*display)++, 0, 0, core.r, core.g, core.b, alpha);
    gDPSetEnvColor((*display)++, outer.r, outer.g, outer.b, 255);
}

static void DinFireSword_DrawLayers(PlayState* play, s32 profile, u16 phase, Mtx* matrix) {
    Gfx* coreDL;
    Gfx* flameDL;
    if (matrix == NULL || !DinFireSword_Load(profile, &coreDL, &flameDL))
        return;
    const Color_RGB8 core =
        CVarGetColor24(CVAR_COSMETIC("Custom.DinFireSwordCore.Value"), (Color_RGB8){ 255, 225, 122 });
    const Color_RGB8 outer =
        CVarGetColor24(CVAR_COSMETIC("Custom.DinFireSwordOuter.Value"), (Color_RGB8){ 255, 43, 3 });

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, matrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    DinFireSword_Material(&POLY_OPA_DISP, false, core, outer, phase);
    gSPDisplayList(POLY_OPA_DISP++, coreDL);
    gDPPipeSync(POLY_OPA_DISP++);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, matrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    DinFireSword_Material(&POLY_XLU_DISP, true, core, outer, phase);
    gSPDisplayList(POLY_XLU_DISP++, flameDL);
    gDPPipeSync(POLY_XLU_DISP++);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
}

void DinFireSword_Draw(PlayState* play, Player* player) {
    if (play == NULL || player == NULL || player != GET_PLAYER(play))
        return;
    if (player->actor.scale.y < 0.0f)
        return;
    if (!DinFireSword_Eligible(play, player)) {
        DinFireSword_Reset();
        return;
    }
    if (!DinFireSword_SameContext(play, player) || sDraw.play != play || sDraw.player != player)
        return;
    // Record the exact hand transform here, inside its own interpolation scope.
    // Later mask/boot draws can change the interpolation base as well as the
    // current matrix. Reusing this recorded matrix preserves sword alignment.
    OPEN_DISPS(play->state.gfxCtx);
    sDraw.handMatrix = MATRIX_NEWMTX(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
}

void DinFireSword_DrawAfterPlayer(PlayState* play, Player* player) {
    if (sDraw.play != play || sDraw.player != player)
        return;
    Mtx* matrix = sDraw.handMatrix;
    DinFireSword_BeginPlayerDraw(NULL, NULL);
    if (matrix == NULL || !DinFireSword_Eligible(play, player) || !DinFireSword_SameContext(play, player))
        return;
    // Stay in OPA for the core so room transparency still composites over it.
    // No custom body colors need to be guessed or overwritten at the hand.
    DinFireSword_DrawLayers(play, DinFireSword_Profile(player), sSword.phase, matrix);
}

// The custom pedestal calls this only while drawing its selected sword and
// with that sword's placement matrix current. Never creates its own actor.
void DinFireSword_DrawPedestal(PlayState* play) {
    if (play == NULL || !CVarGetInteger(CVAR_ENHANCEMENT("DinFireSword"), 0) ||
        !CVarGetInteger(CVAR_SETTING("AltAssets"), 1) || !ResourceMgr_FileExists(sEquipment[LINK_AGE_ADULT]))
        return;
    OPEN_DISPS(play->state.gfxCtx);
    DinFireSword_DrawLayers(play, LINK_AGE_ADULT, play->gameplayFrames & 1023, MATRIX_NEWMTX(play->state.gfxCtx));
    CLOSE_DISPS(play->state.gfxCtx);
}
