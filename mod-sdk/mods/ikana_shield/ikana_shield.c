#include <string.h>
#include <math.h>
/**
 * Shield of Ikana: Majora's Mask's Mirror Shield. A guard landed right after raising it drains the attacker's soul
 * into Link, and once per area it refuses to let him die.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "overlays/actors/ovl_Mir_Ray/z_mir_ray.h"
#include "overlays/actors/ovl_Obj_Syokudai/z_obj_syokudai.h"
#include "objects/object_link_child/object_link_child.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

#define IKANA_SHIELD_KEY "nei.equip.ikana_shield"
#define GUARD_WINDOW 12
#define SOUL_DRAIN 4
#define SOUL_HEAL 8
#define REVIVE_HEALTH (3 * 16)
#define REVIVE_FLASH_FRAMES 20
#define GET_ITEM_SCALE 0.035f

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__icon_item_static_yar/gItemIconMirrorShieldTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gShieldOfIkanaNameTex";
// Majora's Mask's own child mirror shield, from the mm_assets package. Without it the vanilla Mirror stays.
static const ALIGN_ASSET(2) char sMmShieldInHandDL[] =
    "__OTR__objects/object_link_child/gLinkHumanRightHandHoldingMirrorShieldDL";
static const ALIGN_ASSET(2) char sMmShieldOnBackDL[] = "__OTR__objects/object_link_child/gLinkHumanMirrorShieldDL";

static const char* const sBareSheathDL[] = { "__OTR__objects/object_link_boy/gLinkAdultSheathNearDL",
                                             gLinkChildSheathNearDL };
static const char* const sSheathedSwordDL[] = { "__OTR__objects/object_link_boy/gLinkAdultMasterSwordAndSheathNearDL",
                                                gLinkChildSwordAndSheathNearDL };

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

static struct {
    s16 guardTimer;
    bool isGuarding;
    bool isDeathSaveUsed;
    s16 flashTimer;
} sIkana;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(IKANA_SHIELD_KEY);
}

// Asked on every limb of every frame; the archives mounted do not change within a session.
static bool HasMmModel(void) {
    static s8 sHasModel = -1;

    if (sHasModel < 0) {
        sHasModel = SOH_MOD_API_HAS(sApi, HasResource) && sApi->HasResource(sMmShieldInHandDL) &&
                    sApi->HasResource(sMmShieldOnBackDL);
    }
    return sHasModel;
}

static bool IsShieldOnBack(Player* player) {
    return player->sheathType == PLAYER_MODELTYPE_SHEATH_18 || player->sheathType == PLAYER_MODELTYPE_SHEATH_19;
}

static bool IsShieldInHand(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD;
}

static void PlaySfx(Player* player, u16 sfxId) {
    Audio_PlaySfxGeneral(sfxId, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void TrackGuard(Player* player) {
    bool isGuarding = (player->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0;

    if (isGuarding && !sIkana.isGuarding) {
        sIkana.guardTimer = 0;
    } else if (isGuarding) {
        sIkana.guardTimer++;
    } else {
        sIkana.guardTimer = 0;
    }
    sIkana.isGuarding = isGuarding;
}

static void FadeReviveFlash(PlayState* play) {
    if (sIkana.flashTimer <= 0 || --sIkana.flashTimer > 0) {
        return;
    }
    play->envCtx.fillScreen = false;
    play->envCtx.screenFillColor[3] = 0;
}

static ColliderCylinder sSoulHit;
static bool sSoulHitReady;
static Actor* sSoulTarget;
static s16 sSoulFrames;
static u32 sSwordElement;
static s16 sSwordElementFrames;
static const ALIGN_ASSET(2) char sMmReflection[] = "__OTR__objects/object_mir_ray/object_mir_ray_DL_0004B0";
static bool SeparateMirrors(void) { return CVarGetInteger("gMods.SeparateMirrorShields", 1) != 0; }
static bool OutsideTwinrova(PlayState* play) {
    for (Actor* a=play->actorCtx.actorLists[ACTORCAT_BOSS].head;a!=NULL;a=a->next) {
        if (a->id == ACTOR_BOSS_TW) { return false; }
    }
    return true;
}
static bool IsOotMirror(void) {
    const char* worn = sApi->GetWornEquipment(SOH_EQUIP_SLOT_SHIELD);
    return CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_MIRROR && (worn == NULL || worn[0] == 0);
}
static void TickSoulAndElement(PlayState* play, Player* player) {
    if (sSoulTarget != NULL && sSoulFrames > 0 && sSoulTarget->update != NULL) {
        if (!sSoulHitReady) { Collider_InitCylinder(play,&sSoulHit); sSoulHitReady=true; }
        static ColliderCylinderInit hit = {
            { COLTYPE_NONE, AT_ON|AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
            { ELEMTYPE_UNK0, {DMG_SLASH_MASTER,0,SOUL_DRAIN}, {0,0,0}, TOUCH_ON|TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
            {30,60,0,{0,0,0}}
        };
        Collider_SetCylinder(play,&sSoulHit,&player->actor,&hit);
        sSoulHit.dim.pos.x=(s16)sSoulTarget->world.pos.x;
        sSoulHit.dim.pos.y=(s16)sSoulTarget->world.pos.y;
        sSoulHit.dim.pos.z=(s16)sSoulTarget->world.pos.z;
        CollisionCheck_SetAT(play,&play->colChkCtx,&sSoulHit.base);
        --sSoulFrames;
    }
    if (!SeparateMirrors() || !OutsideTwinrova(play)) { sSwordElementFrames=0; sSwordElement=0; return; }
    if (sSwordElementFrames>0) {
        --sSwordElementFrames;
        if (player->meleeWeaponState>0) {
            player->meleeWeaponQuads[0].info.toucher.dmgFlags |= sSwordElement;
            player->meleeWeaponQuads[1].info.toucher.dmgFlags |= sSwordElement;
            if (sSwordElement == DMG_MAGIC_LIGHT) {
                player->meleeWeaponQuads[0].info.toucher.effect=3;
                player->meleeWeaponQuads[1].info.toucher.effect=3;
            }
        }
    }
    if (IsOotMirror() && (player->stateFlags1 & PLAYER_STATE1_SHIELDING)) {
        for (Actor* a=play->actorCtx.actorLists[ACTORCAT_PROP].head;a!=NULL;a=a->next) {
            if (a->id==ACTOR_OBJ_SYOKUDAI && Math_Vec3f_DistXYZ(&a->world.pos,&player->actor.world.pos)<65.0f) {
                ObjSyokudai* torch=(ObjSyokudai*)a;
                if (torch->litTimer!=0) {
                    torch->litTimer=1; sSwordElement=DMG_MAGIC_FIRE; sSwordElementFrames=200;
                    PlaySfx(player,NA_SE_IT_SHIELD_REFLECT_SW);
                }
            }
        }
    }
}
static void ForgetSoulActor(Actor* actor) {
    if (actor == sSoulTarget) { sSoulTarget=NULL; sSoulFrames=0; }
}
static void ResolveSoulDamage(Actor* victim, ColliderInfo* attack, float* damage) {
    if (attack == &sSoulHit.info && victim != sSoulTarget) { *damage = 0; return; }
    if (victim==sSoulTarget && attack==&sSoulHit.info) {
        *damage=SOUL_DRAIN;
        sSoulFrames=0; sSoulTarget=NULL;
        if (gPlayState!=NULL) { Health_ChangeBy(gPlayState,SOUL_HEAL); }
    }
}
static void AbsorbShieldElement(PlayState* play, Player* player, Actor* attacker) {
    if (!SeparateMirrors() || !IsOotMirror() || !OutsideTwinrova(play)) { return; }
    ColliderInfo* hit=player->shieldQuad.info.acHitInfo;
    u32 flags=hit!=NULL?hit->toucher.dmgFlags:0;
    if (attacker!=NULL && (attacker->id==ACTOR_EN_ANUBICE_FIRE || attacker->id==ACTOR_EN_FW)) { flags|=DMG_MAGIC_FIRE; }
    if (attacker!=NULL && (attacker->id==ACTOR_EN_FZ || attacker->id==ACTOR_EN_ICE_HONO)) { flags|=DMG_MAGIC_ICE; }
    if (hit != NULL && hit->toucher.effect == 3) { flags |= DMG_MAGIC_LIGHT; }
    u32 element=flags&(DMG_MAGIC_FIRE|DMG_ARROW_FIRE)?DMG_MAGIC_FIRE:
        flags&(DMG_MAGIC_ICE|DMG_ARROW_ICE)?DMG_MAGIC_ICE:flags&(DMG_MAGIC_LIGHT|DMG_ARROW_LIGHT)?DMG_MAGIC_LIGHT:0;
    if (element!=0) { sSwordElement=element; sSwordElementFrames=200; PlaySfx(player,NA_SE_IT_SHIELD_REFLECT_SW); }
}
static void DrawMmReflection(bool* should, va_list args) {
    MirRayShieldReflection* reflection=va_arg(args,MirRayShieldReflection*);
    if (!IsWorn() || !*should || gPlayState==NULL || !sApi->HasResource(sMmReflection)) { return; }
    PlayState* play=gPlayState;
    Matrix_Push();
    Matrix_Translate(reflection->pos.x,reflection->pos.y,reflection->pos.z,MTXMODE_NEW);
    Matrix_Scale(.01f,.01f,.01f,MTXMODE_APPLY);
    Matrix_Mult(&reflection->mtx,MTXMODE_APPLY);
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gDPSetRenderMode(POLY_XLU_DISP++,G_RM_FOG_SHADE_A,G_RM_AA_ZB_XLU_DECAL2);
    gDPSetPrimColor(POLY_XLU_DISP++,0,0,255,255,255,reflection->opacity);
    gSPDisplayList(POLY_XLU_DISP++,(Gfx*)sMmReflection);
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Pop();
    *should=false;
}

static void TickShield(void) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL) {
        return;
    }
    FadeReviveFlash(play);
    TickSoulAndElement(play, GET_PLAYER(play));
    if (!IsWorn()) {
        return;
    }
    player = GET_PLAYER(play);
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        return;
    }
    TrackGuard(player);
}

// Zeroing health is not enough: most enemies only die from their own AC_HIT handling, which the drain bypasses,
// so the kill is driven by hand. Bosses keep their scripted death and need a real last hit.
static void DrainSoul(PlayState* play, Player* player, Actor* attacker) {
    sSoulTarget=attacker; sSoulFrames=2;
    PlaySfx(player, NA_SE_EN_GANON_AT_RETURN);
}

static void DrainOnGuard(PlayState* play, Player* player, Actor* attacker) {
    if (!SeparateMirrors() || !IsWorn() || attacker == NULL || attacker == &player->actor) {
        return;
    }
    if (sIkana.isGuarding && sIkana.guardTimer <= GUARD_WINDOW) {
        DrainSoul(play, player, attacker);
    }
}

// Health_ChangeBy reports the loss before anything reads it, so a revive here means the game never sees a death.
static void SaveFromDeath(int16_t amount) {
    PlayState* play = gPlayState;

    if (amount >= 0 || play == NULL || !SeparateMirrors() || !IsWorn() || sIkana.isDeathSaveUsed || gSaveContext.health > 0) {
        return;
    }
    sIkana.isDeathSaveUsed = true;
    gSaveContext.health = REVIVE_HEALTH;
    play->envCtx.fillScreen = true;
    play->envCtx.screenFillColor[0] = 80;
    play->envCtx.screenFillColor[1] = 0;
    play->envCtx.screenFillColor[2] = 120;
    play->envCtx.screenFillColor[3] = 200;
    sIkana.flashTimer = REVIVE_FLASH_FRAMES;
    Audio_PlaySfxGeneral(NA_SE_EN_FANTOM_LAUGH, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static u32 SavedPlayerEnv(PlayState* play) {
    Gfx* begin=play->state.gfxCtx->polyOpa.bufp;
    Gfx* cursor=play->state.gfxCtx->polyOpa.p;
    while(cursor>begin) {--cursor;if(((cursor->words.w0>>24)&255)==G_SETENVCOLOR)return (u32)cursor->words.w1;}
    return 0xFFFFFFFF;
}
static void RestorePlayerEnv(PlayState* play,u32 rgba) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++,0,0,255,255,255,255);
    gDPSetEnvColor(POLY_OPA_DISP++,rgba>>24,(rgba>>16)&255,(rgba>>8)&255,rgba&255);
    CLOSE_DISPS(play->state.gfxCtx);
}
static bool RemainsOwnShield(void) {
    bool (*owns)(void)=SOH_MOD_API_HAS(sApi,FindService)?(bool(*)(void))sApi->FindService("nei.mm_remains.shield_override"):NULL;
    return owns!=NULL && owns();
}
static u32 sShieldEnv;
static void RestoreShieldState(PlayState* play,Player* player,int32_t limb) {
    RemoteVisualState visual;
    bool local = player == GET_PLAYER(gPlayState);
    bool visible = local ? IsWorn() : ReadRemoteVisual(player, &visual);
    if(visible && (!local || !RemainsOwnShield()) && (limb==PLAYER_LIMB_R_HAND || limb==PLAYER_LIMB_SHEATH)) {
        RestorePlayerEnv(play,sShieldEnv);
    }
}
static void HideVanillaShield(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    RemoteVisualState visual;
    bool local = player == GET_PLAYER(gPlayState);
    bool visible = local ? IsWorn() : ReadRemoteVisual(player, &visual);
    if (!visible || !HasMmModel() || (local && RemainsOwnShield())) {
        return;
    }
    sShieldEnv=SavedPlayerEnv(gPlayState);
    if (limbIndex == PLAYER_LIMB_R_HAND && IsShieldInHand(player)) {
        *dList = ResourceMgr_LoadGfxByName(sMmShieldInHandDL);
    } else if (limbIndex == PLAYER_LIMB_SHEATH && IsShieldOnBack(player)) {
        // Sheath 18 still carries the sword; only 19 is the empty sheath.
        *dList = ResourceMgr_LoadGfxByName(player->sheathType == PLAYER_MODELTYPE_SHEATH_18
                                               ? sSheathedSwordDL[gSaveContext.linkAge]
                                               : sBareSheathDL[gSaveContext.linkAge]);
    }
}

// On XLU on purpose: a foreign display list leaves its combiner on the pipe, and on OPA that blacks out every limb
// drawn after it.
static void DrawShieldOnBack(PlayState* play, Player* player, int32_t limbIndex) {
    RemoteVisualState visual;
    bool local = player == GET_PLAYER(gPlayState);
    bool visible = local ? IsWorn() : ReadRemoteVisual(player, &visual);
    if (!visible || !HasMmModel() || (local && RemainsOwnShield()) || limbIndex != PLAYER_LIMB_SHEATH ||
        !IsShieldOnBack(player) || IsShieldInHand(player) || player->rightHandType == PLAYER_MODELTYPE_RH_FF) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sMmShieldOnBackDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++,0,0,255,255,255,255);
    gDPSetEnvColor(POLY_XLU_DISP++,255,255,255,255);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The MM shield is modelled for the arm: spin about world up first, then stand it upright facing the camera.
static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    if (!HasMmModel()) {
        GetItem_Draw(play, GID_SHIELD_MIRROR);
        return;
    }
    u32 savedEnv=SavedPlayerEnv(play);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    Matrix_RotateZ(M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_RotateX(M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(GET_ITEM_SCALE, GET_ITEM_SCALE, GET_ITEM_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMmShieldOnBackDL);
    CLOSE_DISPS(play->state.gfxCtx);
    RestorePlayerEnv(play,savedEnv);
}

static void RenewDeathSave(int16_t sceneNum) {
    sSoulHitReady=false; sSoulTarget=NULL; sSoulFrames=0; sSwordElement=0; sSwordElementFrames=0;
    sIkana.isDeathSaveUsed = false;
    sIkana.flashTimer = 0;
}

static void ForgetGuard(const char* key) {
    sIkana.guardTimer = 0;
    sIkana.isGuarding = false;
}

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate", "OnPlayerUpdate",          "OnPlayerShieldBlocked",
                                              "OnPlayerHealthChange", "OnCollisionResolveDamage", "OnActorDestroy",    "OnPlayerResolveLimbDraw",
                                              "OnPlayerPostLimbDraw",    "OnSceneInit" };

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
    if (peerId == 0 || sApi->GetNetPeerState(peerId, "nei.ikana_shield.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
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
        visual.active = (IsWorn() && !RemainsOwnShield());
        if (visual.active) {

        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (!sRemoteVisualSent || memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) != 0) {
        if (sApi->SetNetPeerState("nei.ikana_shield.visual", &visual, sizeof(visual))) {
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
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.ikana_shield.visual", 1, ReceiveRemoteVisual);
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
    definition.key = IKANA_SHIELD_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rShield of Ikana&%wBlock with %y\xA3%w right as you raise it&"
                           "to drain the attacker's soul. Once&per area it holds death off.";
    definition.getItemText = "You got the %rShield of Ikana%w!&The mirror of a kingdom of&ghosts.^"
                             "It drinks the soul of whatever&strikes it in time, and once in&"
                             "every place it will not let you&fall.";
    definition.slot = SOH_EQUIP_SLOT_SHIELD;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_SHIELD;
    definition.column = 3;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_CHILD;
    definition.vanillaBase = EQUIP_VALUE_SHIELD_MIRROR;
    definition.toggles = 1;
    definition.onUnequip = ForgetGuard;
    definition.getItemEntry.drawFunc = DrawGetItem;
    // The equipment registry creates the pickup; this block also puts it into new seeds.
    static const SOHCustomItemRandomizer randomizer = {
        sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_EQUIP, 0, 1,
        IKANA_SHIELD_KEY, IKANA_SHIELD_KEY, "",
    };
    definition.randomizer = &randomizer;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickShield);
    SOH_REGISTER_HOOK(sApi, OnPlayerShieldBlocked, DrainOnGuard);
    SOH_REGISTER_HOOK(sApi, OnPlayerShieldBlocked, AbsorbShieldElement);
    SOH_REGISTER_HOOK(sApi, OnCollisionResolveDamage, ResolveSoulDamage);
    SOH_REGISTER_HOOK(sApi, OnActorDestroy, ForgetSoulActor);
    sApi->RegisterVB(VB_MIRRAY_DRAW_REFLECTION, DrawMmReflection);
    SOHModMenuWidget separate = {0};
    separate.structSize=sizeof(separate); separate.section="Mods"; separate.sidebar="Ikana Shield";
    separate.type=SOH_MOD_MENU_CHECKBOX; separate.label="Separate Mirror Shields";
    separate.cvar="gMods.SeparateMirrorShields"; separate.defaultInt=1;
    separate.tooltip="Enable Ikana soul damage and OoT mirror elemental absorption. Disable for vanilla behavior. Absorption is disabled in Twinrova fights.";
    sApi->RegisterMenuWidget(&separate);
    SOH_REGISTER_HOOK(sApi, OnPlayerHealthChange, SaveFromDeath);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, HideVanillaShield);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawShieldOnBack);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, RestoreShieldState);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, RenewDeathSave);
}
