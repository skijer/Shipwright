// Twinmold's friendly shadow. Its actor owns its pose, colliders and projectiles;
// it never runs En_Torch2's hostile AI or changes Link's saved strength upgrade.
#include "objects/object_torch2/object_torch2.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "soh/ResourceManagerHelpers.h"
#include <stdlib.h>

s32 Player_SetupAction(PlayState*, Player*, PlayerActionFunc, s32);
void func_80839FFC(Player*, PlayState*);
LinkAnimationHeader* Player_GetIdleAnim(Player*);
void Player_AnimPlayLoop(PlayState*, Player*, LinkAnimationHeader*);
s8 Player_ItemToItemAction(s32 item);
#ifdef _WIN32
__declspec(dllimport) void EnArrow_Fly(EnArrow*, PlayState*);
#else
void EnArrow_Fly(EnArrow*, PlayState*);
#endif

#define TWIN_KEY "nei.mm_remains_twinmold"
#define TWIN_COST 24
#define TWIN_FOLLOW_DISTANCE 75.0f
#define TWIN_HELP_DISTANCE 95.0f
#define TWIN_SEARCH_DISTANCE 700.0f
#define TWIN_CUCCO_SHADOW_FRAMES 8
#define TWIN_CUCCO_GRAB_FRAME 2
#define TWIN_CUCCO_PICKUP_FRAMES 8

typedef enum { TWIN_ATTACK, TWIN_HELP, TWIN_FOLLOW, TWIN_WAIT, TWIN_MODE_COUNT } TwinMode;
typedef enum { TWIN_IDLE, TWIN_SLASH, TWIN_SHOOT, TWIN_ICE, TWIN_NUT, TWIN_BOOM, TWIN_BOMB,
               TWIN_SUPPORT, TWIN_LIFT, TWIN_CUCCO_PICKUP, TWIN_CUCCO_SHADOW } TwinAction;
typedef enum { TWIN_PLAYER_NONE, TWIN_PLAYER_SUMMON, TWIN_PLAYER_LAUNCH, TWIN_PLAYER_MENU } TwinPlayerAction;

typedef struct {
    Player model; // Actor must remain the prefix; only our callbacks update this Player.
    Vec3s pose[PLAYER_LIMB_MAX + 1];
    Vec3s morph[PLAYER_LIMB_MAX + 1];
    SkelAnime upper;
    Vec3s upperPose[PLAYER_LIMB_MAX + 1], upperMorph[PLAYER_LIMB_MAX + 1];
    ColliderCylinder blade;
    Actor* taskActor;
    Actor* carriedCucco;
    Vec3f taskPoint;
    Vec3f waitPoint;
    Vec3f shadowStart;
    TwinAction action;
    s16 ticks;
    s16 cooldown;
    s16 cuccoCooldown;
    s16 specialCooldown;
    s16 stunApproach;
    Actor* engagement;
    s16 emerge;
    u8 hit;
    u8 assist;
    u8 launchTask;
    u8 following;
    u8 carryingTogether;
    u8 animationReady;
    const char* clip;
} TwinLink;

typedef struct {
    Actor actor;
    ColliderCylinder collider;
    s16 ticks;
    u8 returning;
} TwinBoomerang;

static TwinLink* sTwinLink;
static s16 sTwinLinkId = -1, sTwinBoomId = -1;
static bool sTwinActive;
static TwinMode sTwinMode = TWIN_FOLLOW;
static TwinPlayerAction sTwinPlayerAction;
static s16 sTwinPlayerTicks;
static s16 sTwinLaunchYaw, sTwinLaunchPitch;
static Vec3f sTwinLaunchOrigin;
static Actor* sTwinLiftCarrier;
#define TWIN_MENU_OWNER "nei.twin_link.orders"
static bool sTwinMenu;
static u8 sTwinMenuSelection, sTwinMenuFrames, sTwinReleaseInput;
static bool sTwinMenuStickHeld;
static Vec3f sTwinMenuAnchor;
static s8 sTwinPendingMode = -1;
static bool sTwinWasSlashing;
static bool sTwinOrdersAfterMask, sTwinInitialOrders, sTwinPendingInitial;
static bool sTwinLiftRequested;
static Input sTwinInput;
static LinkAnimationHeader sTwinMaskAnimation;
static s16* sTwinMaskFrames;
static u8 sTwinFaceEyes[2][64 * 32];
static u8 sTwinFaceMouth[2][32 * 32];
static bool sTwinFaceReady[2];
static const ALIGN_ASSET(2) char sTwinMask[] = "__OTR__objects/object_ob/gMoonChildTwinmoldsMaskDL";
static const ALIGN_ASSET(2) char sTwinMaskOn[] = "__OTR__misc/link_animetion/gPlayerAnim_cl_setmask_Data";

static ColliderCylinderInit sTwinBladeInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { DMG_SLASH_MASTER, 0, 2 }, { 0, 0, 0 },
      TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 25, 50, 0, { 0, 0, 0 } },
};

static void TwinPlayerActionHandler(Player* player, PlayState* play);
static void TwinBeginPlayer(PlayState*, Player*, TwinPlayerAction);
static void TwinCloseMenu(PlayState* play);
static void TwinReleaseCucco(TwinLink*, PlayState*);
static bool TwinCanOfferLift(TwinLink*, PlayState*, Player*);
static void TwinBombDraw(Actor*, PlayState*);

// Link's head uses CI8, whereas Torch2's face material uses RGBA16 / IA16.
// Resample into Link's actual head palette once, preserving its mesh and UV layout.
static u16 TwinRead16(const u8* p) { return (u16)((p[0] << 8) | p[1]); }

static u8 TwinPaletteColor(const u8* palette, int r, int g, int b, int a) {
    int best = 0x7FFFFFFF; u8 index = 0;
    for (int i = 0; i < 256; ++i) {
        u16 color = TwinRead16(palette + i * 2);
        int dr = r - ((color >> 11) & 31), dg = g - ((color >> 6) & 31), db = b - ((color >> 1) & 31);
        int da = a - (color & 1);
        int distance = dr * dr + dg * dg + db * db + da * da * 4096;
        if (distance < best) { best = distance; index = (u8)i; }
    }
    return index;
}

static void TwinPrepareFace(void) {
    int age = LINK_IS_ADULT ? 0 : 1;
    if (sTwinFaceReady[age]) return;
    const char* tlut = age == 0 ? gLinkAdultHeadTLUT : gLinkChildSkinTLUT;
    if (!sApi->HasResource(tlut) || !sApi->HasResource(object_torch2Tex_0041C0) ||
        !sApi->HasResource(object_torch2Tex_0043C0) || ResourceMgr_TexIsRaw(tlut) ||
        ResourceMgr_TexIsRaw(object_torch2Tex_0041C0) || ResourceMgr_TexIsRaw(object_torch2Tex_0043C0)) return;
    const u8* palette = (const u8*)ResourceMgr_LoadTexOrDListByName(tlut);
    const u8* eyes = (const u8*)ResourceMgr_LoadTexOrDListByName(object_torch2Tex_0041C0);
    const u8* mouth = (const u8*)ResourceMgr_LoadTexOrDListByName(object_torch2Tex_0043C0);
    if (palette == NULL || eyes == NULL || mouth == NULL) return;
    u8 eyeColors[256], mouthColors[256];
    for (int i = 0; i < 256; ++i) {
        u16 rgba = TwinRead16(eyes + i * 2);
        eyeColors[i] = TwinPaletteColor(palette, (rgba >> 11) & 31, (rgba >> 6) & 31, (rgba >> 1) & 31, rgba & 1);
        int intensity = mouth[i * 2] >> 3;
        mouthColors[i] = TwinPaletteColor(palette, intensity, intensity, intensity, mouth[i * 2 + 1] >= 128);
    }
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 64; ++x) sTwinFaceEyes[age][y * 64 + x] = eyeColors[(y / 2) * 16 + x / 4];
        for (int x = 0; x < 32; ++x) sTwinFaceMouth[age][y * 32 + x] = mouthColors[(y / 2) * 16 + x / 2];
    }
    sTwinFaceReady[age] = true;
}

static void TwinStore(void) {
    if (SOH_MOD_API_HAS(sApi, StorageSet)) {
        u8 state[3] = { 1, sTwinActive, (u8)sTwinMode };
        sApi->StorageSet("mm_collectables", "twin_link", state, sizeof(state));
    }
}

static bool TwinActorIsLive(PlayState* play, Actor* candidate) {
    if (candidate == NULL) return false;
    for (int c = 0; c < ACTORCAT_MAX; ++c)
        for (Actor* a = play->actorCtx.actorLists[c].head; a != NULL; a = a->next)
            if (a == candidate) return a->update != NULL;
    return false;
}

static bool TwinBusy(PlayState* play, Player* player) {
    return gSaveContext.health <= 0 || (player->stateFlags1 &
        (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_TALKING |
         PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_ON_HORSE)) ||
        play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF;
}

static void TwinFinishPlayer(PlayState* play, Player* player) {
    bool owned = player->actionFunc == TwinPlayerActionHandler;
    if (sTwinLiftCarrier != NULL && player->actor.parent == sTwinLiftCarrier)
        player->actor.parent = NULL;
    if (owned && sTwinPlayerAction == TWIN_PLAYER_LAUNCH) player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    sTwinLiftCarrier = NULL;
    sTwinPlayerAction = TWIN_PLAYER_NONE;
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    if (owned && !(player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE))) {
        func_80839FFC(player, play);
        Player_AnimPlayLoop(play, player, Player_GetIdleAnim(player));
    }
}

static void TwinReset(void) {
    if (gPlayState != NULL && sTwinPlayerAction != TWIN_PLAYER_NONE)
        TwinFinishPlayer(gPlayState, GET_PLAYER(gPlayState));
    TwinCloseMenu(gPlayState);
    sTwinActive = false;
    sTwinMenu = 0;
    sTwinOrdersAfterMask = sTwinInitialOrders = sTwinPendingInitial = false;
    sTwinPendingMode = -1;
    sTwinWasSlashing = false;
    sTwinLiftRequested = false;
    if (gPlayState != NULL && sTwinLink != NULL && TwinActorIsLive(gPlayState, &sTwinLink->model.actor))
        Actor_Kill(&sTwinLink->model.actor);
    sTwinLink = NULL;
    Remains_Forget(REMAINS_TWINMOLD);
}

static void TwinLoad(void) {
    TwinReset();
    u8 state[3] = { 0 };
    if (SOH_MOD_API_HAS(sApi, StorageGet) &&
        sApi->StorageGet("mm_collectables", "twin_link", state, sizeof(state)) == sizeof(state) &&
        state[0] == 1 && state[1] == 1 && state[2] < TWIN_MODE_COUNT && sApi->IsCustomItemOwned(TWIN_KEY)) {
        sTwinActive = true; sTwinMode = state[2];
    }
}

static void TwinForget(const char* key) {
    Remains_Forget(REMAINS_TWINMOLD);
    TwinReset();
    TwinStore();
}

static bool TwinCanPay(void) {
    return gSaveContext.isMagicAcquired && gSaveContext.magic >= TWIN_COST &&
        (gSaveContext.magicState == MAGIC_STATE_IDLE || gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS);
}

static bool TwinPay(PlayState* play) {
    if (!TwinCanPay() || !Magic_RequestChange(play, TWIN_COST, MAGIC_CONSUME_NOW)) {
        Player_PlaySfx(&GET_PLAYER(play)->actor, NA_SE_SY_ERROR);
        return false;
    }
    // A one-off payment has no spell actor to finish the native flashing state.
    // Keep native cost resolution (including other mods), then settle only our request.
    if (!Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER))
        gSaveContext.magic = gSaveContext.magicTarget;
    Magic_Reset(play);
    return true;
}

static void TwinCloseMenu(PlayState* play) {
    bool wasOpen = sTwinMenu;
    sTwinMenu = false;
    if (SOH_MOD_API_HAS(sApi, ReleasePlayerInput)) sApi->ReleasePlayerInput(TWIN_MENU_OWNER);
    if (SOH_MOD_API_HAS(sApi, ReleaseTimeControl)) sApi->ReleaseTimeControl(TWIN_MENU_OWNER);
    if (wasOpen) { sTwinReleaseInput = 2; sTwinInitialOrders = false; }
    if (play != NULL && sTwinPlayerAction == TWIN_PLAYER_MENU)
        TwinFinishPlayer(play, GET_PLAYER(play));
}

static bool TwinOrderIsFree(s32 mode) {
    return sTwinInitialOrders || (sTwinActive && mode == sTwinMode);
}

static void TwinShowMenu(PlayState* play) {
    sTwinMenuSelection = sTwinActive ? sTwinMode : TWIN_FOLLOW;
    sTwinMenuAnchor = GET_PLAYER(play)->actor.world.pos;
    sTwinMenuFrames = 0;
    sTwinMenuStickHeld = true; // opening while moving requires centering the stick once
    sTwinMenu = true;
    sApi->BlockPlayerInput(TWIN_MENU_OWNER, 0xFFFF, true);
    sApi->RequestTimeControl(TWIN_MENU_OWNER, 1000, 0.0f, true);
    TwinBeginPlayer(play, GET_PLAYER(play), TWIN_PLAYER_MENU);
}

// Raw gameplay input still drives this picker while the filtered player input is blocked.
static void TwinMenuUpdate(PlayState* play) {
    Player* player = GET_PLAYER(play);
    if (TwinBusy(play, player) || play->pauseCtx.state != 0 || play->msgCtx.msgMode != MSGMODE_NONE ||
        player->actionFunc != TwinPlayerActionHandler) { TwinCloseMenu(play); return; }
    player->actor.world.pos = player->actor.prevPos = sTwinMenuAnchor;
    player->actor.velocity = (Vec3f){ 0, 0, 0 };
    player->actor.speedXZ = player->linearVelocity = player->pushedSpeed = 0;
    if (sTwinMenuFrames < 3) ++sTwinMenuFrames;
    if (sTwinMenuFrames < 3) return;
    Input* input = &play->state.input[0];
    u16 press = input->press.button;
    if (press & (BTN_B | BTN_START)) { TwinCloseMenu(play); return; }
    int x = input->cur.stick_x, y = input->cur.stick_y;
    bool neutral = ABS(x) < 25 && ABS(y) < 25;
    s32 col = sTwinMenuSelection & 1, row = sTwinMenuSelection >> 1;
    bool move = !neutral && !sTwinMenuStickHeld;
    if (neutral) sTwinMenuStickHeld = false;
    if (move) {
        if (ABS(x) >= ABS(y)) col = x > 0 ? 1 : 0;
        else row = y > 0 ? 0 : 1;
        sTwinMenuStickHeld = true;
    }
    if (press & BTN_DLEFT) col = 0;
    if (press & BTN_DRIGHT) col = 1;
    if (press & BTN_DUP) row = 0;
    if (press & BTN_DDOWN) row = 1;
    u8 selection = row * 2 + col;
    if (selection != sTwinMenuSelection) {
        sTwinMenuSelection = selection; Player_PlaySfx(&player->actor, NA_SE_SY_CURSOR);
    }
    if (press & BTN_A) {
        if (!TwinOrderIsFree(sTwinMenuSelection) && !TwinCanPay()) {
            Player_PlaySfx(&player->actor, NA_SE_SY_ERROR); return;
        }
        sTwinPendingMode = sTwinMenuSelection;
        sTwinPendingInitial = sTwinInitialOrders;
        TwinCloseMenu(play);
    }
}

static void TwinUse(PlayState* play, Player* player) {
    if (sTwinMenu || sTwinPendingMode >= 0 || sTwinPlayerAction != TWIN_PLAYER_NONE ||
        TwinBusy(play, player) || play->msgCtx.msgMode != MSGMODE_NONE ||
        player->heldActor != NULL || (player->stateFlags1 &
            (PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_CHARGING_SPIN_ATTACK |
             PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LADDER)) ||
        !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) return;
    if (!sTwinActive) {
        if (!TwinPay(play)) return;
        sTwinActive = true;
        sTwinMode = TWIN_FOLLOW;
        sTwinInitialOrders = true;
        TwinStore();
    }
    if (Remains_GetWorn() != REMAINS_TWINMOLD || sTwinInitialOrders) {
        if (Remains_GetWorn() != REMAINS_TWINMOLD) Remains_Toggle(play, player, REMAINS_TWINMOLD);
        sTwinOrdersAfterMask = true;
        TwinBeginPlayer(play, player, TWIN_PLAYER_SUMMON);
    } else TwinShowMenu(play);
}

static bool TwinUsesUpper(TwinLink* self) {
    return self->action == TWIN_SHOOT || self->action == TWIN_ICE || self->action == TWIN_BOOM || self->action == TWIN_NUT;
}

static void TwinPlay(TwinLink* self, PlayState* play, const char* path, bool loop, f32 speed) {
    SkelAnime* skel = TwinUsesUpper(self) ? &self->upper : &self->model.skelAnime;
    if (self->clip == path) { skel->playSpeed = speed; return; }
    self->clip = path;
    LinkAnimation_Change(play, skel, (LinkAnimationHeader*)path, speed, 0,
        MAX(0, Animation_GetLastFrame((void*)path) - 1), loop ? ANIMMODE_LOOP_INTERP : ANIMMODE_ONCE_INTERP, -3);
}

static s32 TwinSword(void) {
    s32 sword = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    return sword >= PLAYER_SWORD_KOKIRI && sword <= PLAYER_SWORD_BIGGORON ? sword : PLAYER_SWORD_NONE;
}

static void TwinEquipment(TwinLink* self, PlayState* play) {
    Player* link = GET_PLAYER(play);
    Player* model = &self->model;
    s32 sword = TwinSword(), group = PLAYER_MODELGROUP_DEFAULT, item = ITEM_NONE;
    if (self->action == TWIN_SHOOT || self->action == TWIN_ICE) {
        group = PLAYER_MODELGROUP_BOW_SLINGSHOT; item = LINK_IS_ADULT ? ITEM_BOW : ITEM_SLINGSHOT;
    } else if (self->action == TWIN_BOOM && self->ticks < 8) {
        group = PLAYER_MODELGROUP_BOOMERANG; item = ITEM_BOOMERANG;
    } else if ((self->action == TWIN_SLASH || (self->action == TWIN_IDLE && sTwinMode == TWIN_ATTACK)) && sword) {
        group = sword == PLAYER_SWORD_BIGGORON ? PLAYER_MODELGROUP_BGS : PLAYER_MODELGROUP_SWORD_AND_SHIELD;
        item = sword == PLAYER_SWORD_KOKIRI ? ITEM_SWORD_KOKIRI : sword == PLAYER_SWORD_MASTER ? ITEM_SWORD_MASTER : ITEM_SWORD_BGS;
        if (!LINK_IS_ADULT && link->currentShield == PLAYER_SHIELD_HYLIAN && sword != PLAYER_SWORD_BIGGORON)
            group = PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD;
    }
    bool refresh = model->modelGroup != group || model->heldItemId != item || model->currentShield != link->currentShield;
    model->currentShield = link->currentShield;
    model->currentSwordItemId = link->currentSwordItemId;
    model->currentTunic = link->currentTunic;
    model->currentBoots = link->currentBoots;
    model->heldItemId = item;
    model->itemAction = model->heldItemAction = Player_ItemToItemAction(item);
    if (refresh) Player_SetModelGroup(model, group);
    if (self->action == TWIN_SLASH) {
        self->blade.info.toucher.dmgFlags = sword == PLAYER_SWORD_KOKIRI ? DMG_SLASH_KOKIRI :
            sword == PLAYER_SWORD_BIGGORON ? DMG_SLASH_GIANT : DMG_SLASH_MASTER;
        self->blade.info.toucher.damage = sword == PLAYER_SWORD_KOKIRI ? 1 : sword == PLAYER_SWORD_BIGGORON ? 4 : 2;
    }
}

static void TwinStartAction(TwinLink* self, PlayState* play, TwinAction action) {
    self->action = action;
    self->ticks = 0;
    self->hit = false;
    self->model.actor.shape.shadowDraw = action == TWIN_CUCCO_SHADOW ? NULL : ActorShadow_DrawCircle;
    self->blade.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    TwinEquipment(self, play);
    const char* anim = gPlayerAnim_link_normal_wait_free;
    if (action == TWIN_SLASH) anim = TwinSword() == PLAYER_SWORD_BIGGORON ?
        gPlayerAnim_link_fighter_LLside_kiru : gPlayerAnim_link_fighter_Lside_kiru;
    else if (action == TWIN_SHOOT || action == TWIN_ICE) anim = gPlayerAnim_link_bow_bow_ready;
    else if (action == TWIN_BOOM) anim = gPlayerAnim_link_boom_throwR;
    else if (action == TWIN_NUT || action == TWIN_BOMB) anim = gPlayerAnim_link_normal_throw_free;
    else if (action == TWIN_SUPPORT) anim = gPlayerAnim_link_normal_pushing;
    else if (action == TWIN_LIFT || action == TWIN_CUCCO_PICKUP) anim = gPlayerAnim_link_normal_carryB_free;
    if (TwinUsesUpper(self))
        LinkAnimation_Change(play, &self->model.skelAnime, (LinkAnimationHeader*)gPlayerAnim_link_normal_wait_free,
            1, 0, MAX(0, Animation_GetLastFrame((void*)gPlayerAnim_link_normal_wait_free) - 1), ANIMMODE_LOOP_INTERP, -3);
    self->clip = NULL; // Repeated attacks must restart their clip, not remain at its final frame.
    TwinPlay(self, play, anim, action == TWIN_IDLE || action == TWIN_SUPPORT, action == TWIN_CUCCO_PICKUP ? 2.5f : 1);
}

static void TwinInitActor(Actor* actor, PlayState* play) {
    TwinLink* self = (TwinLink*)actor;
    Player* link = GET_PLAYER(play);
    Actor savedActor = *actor;
    self->model = *link;
    self->model.actor = savedActor;
    self->model.actor.scale = link->actor.scale;
    self->model.actor.flags |= ACTOR_FLAG_CAN_PRESS_SWITCHES;
    self->model.actor.gravity = -1;
    self->model.actor.minVelocityY = -16;
    self->model.actor.room = -1; // follows Link between rooms; scenes create a fresh actor
    self->model.actor.colChkInfo.mass = 80;
    self->model.skelAnime.jointTable = self->pose;
    self->model.skelAnime.morphTable = self->morph;
    self->upper = self->model.skelAnime;
    self->upper.jointTable = self->upperPose;
    self->upper.morphTable = self->upperMorph;
    self->model.skelAnime.initFlags = 0;
    self->model.skelAnime.movementFlags = ANIM_FLAG_NOMOVE;
    self->model.heldActor = NULL;
    self->model.focusActor = NULL;
    self->model.currentMask = PLAYER_MASK_NONE;
    self->model.stateFlags1 = self->model.stateFlags2 = self->model.stateFlags3 = 0;
    self->model.meleeWeaponState = 0;
    self->model.upperLimbRot = (Vec3s){ 0, 0, 0 };
    self->model.unk_6C2 = self->model.unk_6C4 = 0;
    if (link->skelAnime.jointTable != NULL && link->skelAnime.limbCount == PLAYER_LIMB_MAX) {
        memcpy(self->pose, link->skelAnime.jointTable, sizeof(Vec3s) * link->skelAnime.limbCount);
        memcpy(self->morph, self->pose, sizeof(self->pose));
        memcpy(self->upperPose, self->pose, sizeof(self->pose));
        memcpy(self->upperMorph, self->pose, sizeof(self->pose));
        self->animationReady = true;
    }
    self->waitPoint = actor->world.pos;
    self->emerge = 24;
    ActorShape_Init(&actor->shape, 0, ActorShadow_DrawCircle, 22);
    Collider_InitCylinder(play, &self->blade);
    Collider_SetCylinder(play, &self->blade, actor, &sTwinBladeInit);
    sTwinLink = self;
    if (self->animationReady) TwinStartAction(self, play, TWIN_IDLE);
}

static void TwinDestroyActor(Actor* actor, PlayState* play) {
    if (sTwinLiftCarrier == actor) TwinFinishPlayer(play, GET_PLAYER(play));
    TwinReleaseCucco((TwinLink*)actor, play);
    Collider_DestroyCylinder(play, &((TwinLink*)actor)->blade);
    if (sTwinLink == (TwinLink*)actor) sTwinLink = NULL;
}

static bool TwinCanWalkTo(PlayState* play, Actor* actor, Vec3f* point) {
    Vec3f from = actor->world.pos, to = *point, hit;
    CollisionPoly* poly;
    s32 bg;
    from.y += 35; to.y += 35;
    return !BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bg);
}

static void TwinWalk(TwinLink* self, PlayState* play, Vec3f* goal, f32 stop) {
    Actor* actor = &self->model.actor;
    f32 distance = Math_Vec3f_DistXZ(&actor->world.pos, goal);
    actor->speedXZ = 0;
    if (distance > stop) {
        s16 yaw = Actor_WorldYawTowardPoint(actor, goal);
        Math_SmoothStepToS(&actor->world.rot.y, yaw, 3, 0x1200, 0x80);
        actor->shape.rot.y = actor->world.rot.y;
        // A floor probe prevents walking into pits. Normal BG collision still handles walls and slopes.
        Vec3f probe = actor->world.pos;
        probe.x += Math_SinS(actor->world.rot.y) * 28;
        probe.z += Math_CosS(actor->world.rot.y) * 28;
        probe.y += 60;
        CollisionPoly* poly; s32 bg;
        f32 floor = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bg, actor, &probe);
        if (poly != NULL && floor > actor->world.pos.y - 60 && floor < actor->world.pos.y + 35)
            actor->speedXZ = MIN(8, (distance - stop) * .12f);
    }
    const char* run = sTwinMode == TWIN_HELP ? gPlayerAnim_link_normal_run_free : gPlayerAnim_link_fighter_run;
    const char* idle = gPlayerAnim_link_normal_wait_free;
    if (self->carriedCucco != NULL) { run = gPlayerAnim_link_normal_heavy_run_free; idle = gPlayerAnim_link_normal_carryB_wait; }
    TwinPlay(self, play, actor->speedXZ > .1f ? run : idle,
        true, actor->speedXZ > .1f ? MAX(.8f, actor->speedXZ * .16f) : 1);
    Actor_MoveXZGravity(actor);
    Actor_UpdateBgCheckInfo(play, actor, 28, 14, 60, 5);
}

static Actor* TwinEnemy(PlayState* play, TwinLink* self) {
    Player* link = GET_PLAYER(play);
    Actor* target = link->focusActor;
    if (TwinActorIsLive(play, target) && (target->category == ACTORCAT_ENEMY || target->category == ACTORCAT_BOSS) &&
        target->colChkInfo.health > 0 && Actor_WorldDistXZToActor(&self->model.actor, target) < TWIN_SEARCH_DISTANCE)
        return target;
    return RemainsAlly_FindNearestEnemy(play, &self->model.actor.world.pos, TWIN_SEARCH_DISTANCE);
}

static bool TwinHasNormalShot(void) {
    s32 item = LINK_IS_ADULT ? ITEM_BOW : ITEM_SLINGSHOT;
    return INV_CONTENT(item) == item && AMMO(item) > 0;
}

static bool TwinHasIce(void) {
    return LINK_IS_ADULT && INV_CONTENT(ITEM_BOW) == ITEM_BOW && INV_CONTENT(ITEM_ARROW_ICE) == ITEM_ARROW_ICE;
}

static void TwinFireArrow(TwinLink* self, PlayState* play, Actor* target, TwinAction action) {
    bool nut = action == TWIN_NUT, ice = action == TWIN_ICE;
    if (target == NULL) return;
    if (nut ? (INV_CONTENT(ITEM_NUT) != ITEM_NUT || AMMO(ITEM_NUT) <= 0) :
        ice ? !TwinHasIce() : !TwinHasNormalShot()) return;
    s32 type = nut ? ARROW_NUT : LINK_IS_ADULT ? ARROW_NORMAL : ARROW_SEED;
    Vec3f from = self->model.actor.world.pos, to = target->focus.pos;
    from.y += LINK_IS_ADULT ? 45 : 30;
    if (Math_Vec3f_DistXYZ(&to, &target->world.pos) > 180) { to = target->world.pos; to.y += 25; }
    EnArrow* arrow = (EnArrow*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ARROW, from.x, from.y, from.z,
        Math_Vec3f_Pitch(&from, &to), Math_Vec3f_Yaw(&from, &to), 0, type);
    if (arrow == NULL) return;
    // Use the native flight, without Link's release state or any ammo/magic debit.
    arrow->actionFunc = EnArrow_Fly;
    arrow->unk_210 = from;
    Actor_SetProjectileSpeed(&arrow->actor, type >= ARROW_SEED ? 80 : 150);
    arrow->timer = type >= ARROW_SEED ? 15 : 12;
    if (type >= ARROW_SEED) arrow->actor.shape.rot = (Vec3s){ 0, 0, 0 };
    if (ice) {
        // Ice damage uses the enemy's normal freeze response. ARROW_ICE's attached
        // effect resets the real player's magic on destruction, so a free ally shot
        // uses the ordinary arrow actor with the ice payload and a blue material.
        arrow->collider.info.toucher.dmgFlags = DMG_ARROW_ICE;
        Actor_SetColorFilter(&arrow->actor, 0, 255, 0, 80); // native blue filter, opaque buffer
    }
    if (nut || ice) self->stunApproach = 80;
    Player_PlaySfx(&self->model.actor, nut ? NA_SE_IT_DEKU : type == ARROW_SEED ? NA_SE_IT_SLING_SHOT :
        ice ? NA_SE_IT_MAGIC_ARROW_SHOT : NA_SE_IT_ARROW_SHOT);
}

static void TwinBoomInit(Actor* actor, PlayState* play) {
    TwinBoomerang* self = (TwinBoomerang*)actor;
    Actor_SetScale(actor, .01f);
    Collider_InitCylinder(play, &self->collider);
    Collider_SetCylinder(play, &self->collider, actor, &sTwinBladeInit);
    self->collider.info.toucher.dmgFlags = DMG_BOOMERANG;
    self->collider.info.toucher.damage = 0;
    self->collider.dim.radius = 20;
    self->collider.dim.height = 24;
    self->collider.dim.yShift = -12;
    actor->room = -1;
}

static void TwinBoomDestroy(Actor* actor, PlayState* play) {
    Collider_DestroyCylinder(play, &((TwinBoomerang*)actor)->collider);
}

static void TwinBoomUpdate(Actor* actor, PlayState* play) {
    TwinBoomerang* self = (TwinBoomerang*)actor;
    if (!sTwinActive || sTwinLink == NULL || ++self->ticks > 100) { Actor_Kill(actor); return; }
    if (sTwinMenu || play->pauseCtx.state != 0 || play->msgCtx.msgMode != MSGMODE_NONE) { --self->ticks; return; }
    if (self->ticks > 18 || (self->collider.base.atFlags & (AT_HIT | AT_BOUNCED))) self->returning = true;
    if (self->returning) {
        Vec3f hand = sTwinLink->model.actor.world.pos; hand.y += 35;
        actor->world.rot.y = Math_Vec3f_Yaw(&actor->world.pos, &hand);
        actor->world.rot.x = Math_Vec3f_Pitch(&actor->world.pos, &hand);
        if (Math_Vec3f_DistXYZ(&actor->world.pos, &hand) < 25) { Actor_Kill(actor); return; }
    }
    Vec3f from = actor->world.pos, hit; CollisionPoly* poly; s32 bg;
    Actor_SetProjectileSpeed(actor, 18);
    Actor_MoveXZGravity(actor);
    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &actor->world.pos, &hit, &poly, true, true, true, true, &bg)) {
        if (self->returning) { Actor_Kill(actor); return; }
        actor->world.pos = from; self->returning = true;
    }
    actor->shape.rot.y += 0x2800;
    Collider_UpdateCylinder(actor, &self->collider);
    if (!self->returning) CollisionCheck_SetAT(play, &play->colChkCtx, &self->collider.base);
}

static void TwinBoomDraw(Actor* actor, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBoomerangRefDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void TwinThrowBoom(TwinLink* self, PlayState* play, Actor* enemy) {
    if (sTwinBoomId < 0 || enemy == NULL || INV_CONTENT(ITEM_BOOMERANG) != ITEM_BOOMERANG) return;
    Vec3f from = self->model.actor.world.pos, to = enemy->focus.pos; from.y += 35;
    self->stunApproach = 80;
    Actor_Spawn(&play->actorCtx, play, sTwinBoomId, from.x, from.y, from.z,
        Math_Vec3f_Pitch(&from, &to), Math_Vec3f_Yaw(&from, &to), 0, 0);
}

static bool TwinCombat(TwinLink* self, PlayState* play) {
    Actor* actor = &self->model.actor;
    Actor* enemy = TwinEnemy(play, self);
    if (enemy == NULL) { self->engagement = NULL; return false; }
    f32 distance = Actor_WorldDistXZToActor(actor, enemy);
    if (distance > TWIN_SEARCH_DISTANCE || fabsf(enemy->world.pos.y - actor->world.pos.y) > 180 ||
        !TwinCanWalkTo(play, actor, &enemy->world.pos)) return false;
    if (self->engagement != enemy) {
        self->engagement = enemy; self->specialCooldown = 0; self->stunApproach = 0;
    }
    bool sword = TwinSword() != PLAYER_SWORD_NONE;
    bool normalShot = TwinHasNormalShot();
    bool boom = !LINK_IS_ADULT && INV_CONTENT(ITEM_BOOMERANG) == ITEM_BOOMERANG;
    bool ice = TwinHasIce();
    if (self->cooldown > 0) {
        TwinWalk(self, play, &enemy->world.pos, self->stunApproach > 0 || (!normalShot && !boom && !ice) ? 65 : 180);
        return true;
    }
    self->taskActor = enemy;
    if (self->specialCooldown == 0 && distance < 210 && INV_CONTENT(ITEM_NUT) == ITEM_NUT && AMMO(ITEM_NUT) > 0) {
        TwinStartAction(self, play, TWIN_NUT); self->specialCooldown = 120;
    } else if (self->specialCooldown == 0 && distance > 45 && (ice || boom)) {
        TwinStartAction(self, play, ice ? TWIN_ICE : TWIN_BOOM); self->specialCooldown = 120;
    } else if (self->stunApproach > 0 && sword && distance > 80) {
        self->taskActor = NULL; TwinWalk(self, play, &enemy->world.pos, 60);
    } else if (sword && distance <= 80) TwinStartAction(self, play, TWIN_SLASH);
    else if (distance > 120 && normalShot) TwinStartAction(self, play, TWIN_SHOOT);
    else if (distance > 90 && (boom || ice)) TwinStartAction(self, play, ice ? TWIN_ICE : TWIN_BOOM);
    else { self->taskActor = NULL; TwinWalk(self, play, &enemy->world.pos, 60); }
    return true;
}

static bool TwinBombable(Actor* actor) {
    switch (actor->id) {
        case ACTOR_BG_BREAKWALL: return (actor->params & 0xF) < 3;
        case ACTOR_BG_BOMBWALL: case ACTOR_OBJ_BOMBIWA: case ACTOR_BG_JYA_BOMBIWA:
        case ACTOR_BG_MIZU_BWALL: case ACTOR_BG_SPOT16_BOMBSTONE: case ACTOR_BG_SPOT00_BREAK:
            return true;
        default: return false;
    }
}

static bool TwinStrengthObject(Actor* actor) {
    return actor != NULL && (actor->id == ACTOR_EN_ISHI || actor->id == ACTOR_OBJ_OSHIHIKI ||
        actor->id == ACTOR_BG_HEAVY_BLOCK || actor->id == ACTOR_BG_PUSHBOX);
}

static Actor* TwinInteractObject(PlayState* play, Player* player) {
    if (TwinActorIsLive(play, player->heldActor) && TwinStrengthObject(player->heldActor))
        return player->heldActor;
    if (TwinActorIsLive(play, player->interactRangeActor) && TwinStrengthObject(player->interactRangeActor))
        return player->interactRangeActor;
    if (player->actor.wallBgId != BGCHECK_SCENE && (player->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
        DynaPolyActor* dyna = DynaPoly_GetActor(&play->colCtx, player->actor.wallBgId);
        if (dyna != NULL && TwinStrengthObject(&dyna->actor)) return &dyna->actor;
    }
    // Heavy actors can reject their carry offer before Player_GetStrength runs.
    // Approach them before that gate, instead of waiting for interactRangeActor.
    Actor* nearest = NULL; f32 best = 125;
    for (int c = 0; c < ACTORCAT_MAX; ++c)
        for (Actor* a = play->actorCtx.actorLists[c].head; a != NULL; a = a->next) {
            if (a->update == NULL || a->parent != NULL || !TwinStrengthObject(a)) continue;
            s16 angle = Actor_WorldYawTowardActor(&player->actor, a) - player->actor.shape.rot.y;
            f32 distance = Actor_WorldDistXZToActor(&player->actor, a);
            if (distance < best && ABS(angle) < 0x3000 && fabsf(a->world.pos.y - player->actor.world.pos.y) < 65) {
                nearest = a; best = distance;
            }
        }
    return nearest;
}

static LinkAnimationHeader* TwinMaskAnimation(void) {
    if (sTwinMaskFrames != NULL) return &sTwinMaskAnimation;
    if (!sApi->HasResource(sTwinMaskOn)) return NULL;
    LinkAnimationHeader* source = ResourceMgr_LoadPlayerAnimAsHeader(sTwinMaskOn);
    if (source == NULL || source->segment == NULL || source->common.frameCount <= 0) return NULL;
    size_t words = (size_t)source->common.frameCount * 67;
    sTwinMaskFrames = malloc(words * sizeof(s16));
    if (sTwinMaskFrames == NULL) return NULL;
    memcpy(sTwinMaskFrames, source->segment, words * sizeof(s16));
    for (int f = 0; f < source->common.frameCount; ++f) {
        sTwinMaskFrames[f * 67] = -57;
        sTwinMaskFrames[f * 67 + 2] = 0;
    }
    sTwinMaskAnimation.common.frameCount = source->common.frameCount;
    sTwinMaskAnimation.segment = sTwinMaskFrames;
    return &sTwinMaskAnimation;
}

static void TwinGiveHelpCommand(PlayState* play, Player* player) {
    TwinLink* self = sTwinLink;
    if (self == NULL || sTwinMode != TWIN_HELP || self->cooldown > 0 || self->action != TWIN_IDLE) return;
    Actor* nearest = NULL; f32 best = 200;
    for (int c = 0; c < ACTORCAT_MAX; ++c) {
        for (Actor* a = play->actorCtx.actorLists[c].head; a != NULL; a = a->next) {
            if (a->update == NULL || !TwinBombable(a)) continue;
            f32 dist = Actor_WorldDistXZToActor(&player->actor, a);
            s16 yaw = Actor_WorldYawTowardActor(&player->actor, a) - player->actor.shape.rot.y;
            if (dist < best && ABS(yaw) < 0x3000 && fabsf(a->world.pos.y - player->actor.world.pos.y) < 100) {
                nearest = a; best = dist;
            }
        }
    }
    self->taskActor = NULL; self->launchTask = false;
    if (nearest != NULL) {
        self->taskActor = nearest;
        self->taskPoint = nearest->world.pos;
        s16 yaw = Actor_WorldYawTowardActor(nearest, &player->actor);
        self->taskPoint.x += Math_SinS(yaw) * 45;
        self->taskPoint.z += Math_CosS(yaw) * 45;
        return;
    }
    // Launching is now an explicit A offer, not a slash-triggered wall task.
}

static void TwinBeginPlayer(PlayState* play, Player* player, TwinPlayerAction action) {
    sTwinPlayerAction = action;
    sTwinPlayerTicks = 0;
    Player_SetupAction(play, player, TwinPlayerActionHandler, 0);
    player->linearVelocity = player->actor.speedXZ = 0;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    LinkAnimationHeader* anim = (LinkAnimationHeader*)gPlayerAnim_link_magic_kaze1;
    if (action == TWIN_PLAYER_SUMMON) {
        LinkAnimationHeader* mask = TwinMaskAnimation();
        if (mask != NULL) anim = mask;
    } else if (action == TWIN_PLAYER_MENU) anim = (LinkAnimationHeader*)gPlayerAnim_link_normal_wait_free;
    else if (action == TWIN_PLAYER_LAUNCH) anim = (LinkAnimationHeader*)gPlayerAnim_link_normal_landing_roll_free;
    f32 frame = action == TWIN_PLAYER_LAUNCH ? MIN(8, Animation_GetLastFrame(anim)) : 0;
    LinkAnimation_Change(play, &player->skelAnime, anim, action == TWIN_PLAYER_LAUNCH ? 0 : 1, frame,
        action == TWIN_PLAYER_LAUNCH ? frame : MAX(0, Animation_GetLastFrame(anim) - 1), ANIMMODE_ONCE_INTERP, -4);
    if (action == TWIN_PLAYER_LAUNCH) {
        sTwinLiftCarrier = &sTwinLink->model.actor;
        player->actor.parent = sTwinLiftCarrier;
        player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
        sTwinLaunchYaw = player->actor.shape.rot.y;
        sTwinLaunchPitch = 0x1800;
        sTwinLaunchOrigin = player->actor.world.pos;
    }
}

static void TwinFacePlayer(TwinLink* self, Player* player) {
    Actor* actor = &self->model.actor;
    Math_SmoothStepToS(&actor->world.rot.y, Actor_WorldYawTowardActor(actor, &player->actor), 3, 0x1000, 0x80);
    actor->shape.rot.y = actor->world.rot.y;
}

static void TwinReleaseCucco(TwinLink* self, PlayState* play) {
    Actor* cucco = self->carriedCucco;
    if (play != NULL && TwinActorIsLive(play, cucco) && cucco->parent == &self->model.actor) {
        cucco->parent = NULL;
        cucco->velocity.y = 0;
        cucco->speedXZ = 0;
    }
    self->carriedCucco = NULL;
}

static void TwinPositionCucco(TwinLink* self, PlayState* play) {
    Actor* cucco = self->carriedCucco;
    if (!TwinActorIsLive(play, cucco) || cucco->parent != &self->model.actor) {
        // Native pickup transfers its parent to the real player at frame 4.
        self->carriedCucco = NULL;
        return;
    }
    Actor* actor = &self->model.actor;
    cucco->world.pos = actor->world.pos;
    cucco->world.pos.x += Math_SinS(actor->shape.rot.y) * 8;
    cucco->world.pos.z += Math_CosS(actor->shape.rot.y) * 8;
    cucco->world.pos.y += LINK_IS_ADULT ? 66 : 44;
    cucco->prevPos = cucco->world.pos;
    cucco->velocity = (Vec3f){ 0, 0, 0 };
    cucco->speedXZ = 0;
    cucco->floorHeight = actor->floorHeight;
}

static void TwinCuccoUpdated(void* updated) {
    if (sTwinLink != NULL && updated == sTwinLink->carriedCucco && gPlayState != NULL)
        TwinPositionCucco(sTwinLink, gPlayState);
}

static bool TwinRecallAtLink(TwinLink* self, PlayState* play, bool inFront) {
    Player* player = GET_PLAYER(play);
    Actor* actor = &self->model.actor;
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) return false;
    Vec3f pos = player->actor.world.pos;
    f32 offset = inFront ? 48 : -50;
    pos.x += Math_SinS(player->actor.shape.rot.y) * offset;
    pos.z += Math_CosS(player->actor.shape.rot.y) * offset;
    CollisionPoly* poly = NULL; s32 bg;
    Vec3f probe = pos; probe.y += 60;
    f32 floor = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bg, actor, &probe);
    if (poly == NULL || fabsf(floor - player->actor.world.pos.y) > 40 ||
        !TwinCanWalkTo(play, &player->actor, &pos)) {
        pos = player->actor.world.pos; // Link's grounded position is the fallback.
    } else pos.y = floor;
    actor->world.pos = actor->prevPos = pos;
    actor->velocity = (Vec3f){ 0, 0, 0 }; actor->speedXZ = 0;
    actor->freezeTimer = 0;
    Actor_UpdateBgCheckInfo(play, actor, 28, 14, 60, 5);
    self->taskActor = NULL; self->launchTask = self->assist = false;
    self->following = true; self->emerge = inFront ? 8 : 16;
    TwinStartAction(self, play, TWIN_IDLE);
    if (inFront) TwinFacePlayer(self, player);
    TwinPositionCucco(self, play);
    return true;
}

static bool TwinRecall(TwinLink* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* actor = &self->model.actor;
    if (sTwinMode == TWIN_WAIT || sTwinPlayerAction == TWIN_PLAYER_LAUNCH ||
        self->action == TWIN_CUCCO_SHADOW || self->action == TWIN_CUCCO_PICKUP ||
        (Actor_WorldDistXZToActor(actor, &player->actor) < 650 &&
         fabsf(actor->world.pos.y - player->actor.world.pos.y) < 240)) return false;
    return TwinRecallAtLink(self, play, false);
}

static void TwinBeginCuccoFetch(TwinLink* self, PlayState* play, Actor* cucco) {
    self->taskActor = cucco;
    self->shadowStart = self->model.actor.world.pos;
    self->assist = self->carryingTogether = false;
    TwinStartAction(self, play, TWIN_CUCCO_SHADOW);
    Audio_PlayActorSound2(&self->model.actor, NA_SE_EV_FANTOM_WARP_S);
}

static void TwinAbortCuccoFetch(TwinLink* self, PlayState* play) {
    self->taskActor = NULL;
    if (!TwinRecallAtLink(self, play, false)) {
        Actor* actor = &self->model.actor;
        actor->world.pos = actor->prevPos = self->shadowStart;
        actor->velocity = (Vec3f){ 0, 0, 0 }; actor->speedXZ = 0;
        Actor_UpdateBgCheckInfo(play, actor, 28, 14, 60, 5);
        TwinStartAction(self, play, TWIN_IDLE);
    }
    self->cuccoCooldown = 20;
}

static void TwinUpdateCuccoShadow(TwinLink* self, PlayState* play) {
    Actor* actor = &self->model.actor;
    Actor* cucco = TwinActorIsLive(play, self->taskActor) ? self->taskActor : NULL;
    if (cucco == NULL || cucco->id != ACTOR_EN_NIW || cucco->parent != NULL ||
        sTwinMode != TWIN_HELP || Actor_WorldDistXZToActor(&GET_PLAYER(play)->actor, cucco) > 1000) {
        TwinAbortCuccoFetch(self, play); return;
    }
    Vec3f end = cucco->world.pos;
    end.x -= Math_SinS(cucco->shape.rot.y) * 22;
    end.z -= Math_CosS(cucco->shape.rot.y) * 22;
    f32 t = (f32)(++self->ticks) / TWIN_CUCCO_SHADOW_FRAMES;
    if (t > 1) t = 1;
    actor->world.pos.x = self->shadowStart.x + (end.x - self->shadowStart.x) * t;
    actor->world.pos.y = self->shadowStart.y + (end.y - self->shadowStart.y) * t;
    actor->world.pos.z = self->shadowStart.z + (end.z - self->shadowStart.z) * t;
    actor->prevPos = actor->world.pos;
    actor->velocity = (Vec3f){ 0, 0, 0 }; actor->speedXZ = 0;
    actor->shape.rot.y = actor->world.rot.y = cucco->shape.rot.y;
    // Floor sampling shapes the visual only. Wall collision and gravity are bypassed in transit.
    Vec3f probe = actor->world.pos; probe.y += 80;
    CollisionPoly* poly = NULL; s32 bg;
    f32 floor = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bg, actor, &probe);
    if (poly != NULL && fabsf(floor - actor->world.pos.y) < 100) actor->world.pos.y = floor;
    if (self->ticks < TWIN_CUCCO_SHADOW_FRAMES) return;
    // Reappear behind the moving cucco, with a floor and room to stand.
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (attempt == 1) end = cucco->world.pos;
        probe = end; probe.y += 80; poly = NULL;
        floor = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bg, actor, &probe);
        if (poly != NULL && fabsf(floor - cucco->world.pos.y) < 40 &&
            TwinCanWalkTo(play, cucco, &end)) {
            end.y = floor;
            actor->world.pos = actor->prevPos = end;
            Actor_UpdateBgCheckInfo(play, actor, 28, 14, 60, 5);
            self->emerge = 6;
            TwinStartAction(self, play, TWIN_CUCCO_PICKUP);
            Audio_PlayActorSound2(actor, NA_SE_EV_FANTOM_WARP_S);
            return;
        }
    }
    TwinAbortCuccoFetch(self, play);
}

static bool TwinCanOfferLift(TwinLink* self, PlayState* play, Player* player) {
    if (self == NULL || !sTwinActive || sTwinMode != TWIN_HELP || sTwinMenu || sTwinPendingMode >= 0 ||
        sTwinPlayerAction != TWIN_PLAYER_NONE || self->action != TWIN_IDLE || self->cooldown > 0 ||
        self->carriedCucco != NULL || self->taskActor != NULL || self->assist ||
        TwinBusy(play, player) || play->pauseCtx.state != 0 || play->msgCtx.msgMode != MSGMODE_NONE ||
        player->actor.parent != NULL || player->heldActor != NULL || player->meleeWeaponState != 0 ||
        player->interactRangeActor != NULL || (player->stateFlags2 & PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER) ||
        player->doorType != PLAYER_DOORTYPE_NONE ||
        (player->stateFlags1 & (PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_SHIELDING |
            PLAYER_STATE1_CHARGING_SPIN_ATTACK | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LADDER)) ||
        !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        !(self->model.actor.bgCheckFlags & BGCHECKFLAG_GROUND) || fabsf(player->linearVelocity) > 1 ||
        Actor_WorldDistXZToActor(&self->model.actor, &player->actor) > 70 ||
        fabsf(self->model.actor.world.pos.y - player->actor.world.pos.y) > 20) return false;
    if (TwinActorIsLive(play, player->focusActor) && player->focusActor != &self->model.actor) return false;
    s16 angle = Actor_WorldYawTowardActor(&player->actor, &self->model.actor) - player->actor.shape.rot.y;
    return ABS(angle) < 0x3000 && TwinCanWalkTo(play, &self->model.actor, &player->actor.world.pos);
}

static bool TwinCarryTogether(TwinLink* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* object = player->heldActor != NULL ? player->heldActor : player->interactRangeActor;
    Actor* actor = &self->model.actor;
    if (sTwinMode != TWIN_HELP || !(player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) ||
        !TwinActorIsLive(play, object) || !TwinStrengthObject(object) ||
        Actor_WorldDistXZToActor(actor, &player->actor) > 120) return false;
    Vec3f beside = player->actor.world.pos;
    f32 separation = LINK_IS_ADULT ? 50 : 40;
    beside.x += Math_CosS(player->actor.shape.rot.y) * separation;
    beside.z -= Math_SinS(player->actor.shape.rot.y) * separation;
    Vec3f probe = beside; probe.y += 60;
    CollisionPoly* poly = NULL; s32 bg;
    f32 floor = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bg, actor, &probe);
    if (poly == NULL || fabsf(floor - player->actor.world.pos.y) > 40 ||
        !TwinCanWalkTo(play, &player->actor, &beside)) return false;
    // A held rock must not make the helper walk into/under its carrier.
    Math_StepToF(&actor->world.pos.x, beside.x, 12);
    Math_StepToF(&actor->world.pos.z, beside.z, 12);
    actor->world.pos.y = floor;
    actor->prevPos = actor->world.pos;
    actor->velocity = (Vec3f){ 0, 0, 0 }; actor->speedXZ = 0;
    actor->shape.rot.y = actor->world.rot.y = player->actor.shape.rot.y;
    Actor_UpdateBgCheckInfo(play, actor, 28, 14, 60, 5);
    self->assist = true; self->carryingTogether = true;
    self->taskActor = NULL;
    if (self->action != TWIN_IDLE) TwinStartAction(self, play, TWIN_IDLE);
    return true;
}

static void TwinHelp(TwinLink* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* actor = &self->model.actor;
    Actor* interact = TwinInteractObject(play, player);
    self->assist = false;
    if (!TwinActorIsLive(play, self->taskActor)) self->taskActor = NULL;
    if (self->carriedCucco != NULL) {
        TwinPositionCucco(self, play);
        // Bring it to Link; native A/Grab takes the actual cucco, not the clone.
        if (self->carriedCucco != NULL) {
            if (Actor_WorldDistXZToActor(actor, &player->actor) > 120) TwinRecallAtLink(self, play, true);
            TwinWalk(self, play, &player->actor.world.pos, 48);
            TwinFacePlayer(self, player);
            TwinPositionCucco(self, play);
            Actor_OfferGetItem(self->carriedCucco, play, GI_NONE, 80, 100);
            return;
        }
    }
    Actor* cucco = player->focusActor;
    if (TwinActorIsLive(play, cucco) && cucco->id == ACTOR_EN_NIW && cucco->params != 0xA &&
        cucco->parent == NULL && self->cuccoCooldown == 0 && Actor_WorldDistXZToActor(&player->actor, cucco) < TWIN_SEARCH_DISTANCE) {
        TwinBeginCuccoFetch(self, play, cucco);
        return;
    }
    if (self->taskActor != NULL && self->taskActor->id == ACTOR_EN_NIW) self->taskActor = NULL;
    if (self->taskActor != NULL && Actor_WorldDistXZToActor(&player->actor, self->taskActor) > 240)
        self->taskActor = NULL;
    if (self->taskActor != NULL) {
        TwinWalk(self, play, &self->taskPoint, 30);
        if (Math_Vec3f_DistXZ(&self->model.actor.world.pos, &self->taskPoint) < 38) {
            TwinStartAction(self, play, TWIN_BOMB);
        }
        return;
    }
    if (TwinActorIsLive(play, interact) && TwinStrengthObject(interact) &&
        Actor_WorldDistXZToActor(&player->actor, interact) < 220) {
        Vec3f beside = player->actor.world.pos;
        f32 separation = LINK_IS_ADULT ? 50 : 40;
        beside.x += Math_CosS(player->actor.shape.rot.y) * separation;
        beside.z -= Math_SinS(player->actor.shape.rot.y) * separation;
        TwinWalk(self, play, &beside, 12);
        self->assist = Math_Vec3f_DistXZ(&self->model.actor.world.pos, &beside) < 24 &&
            fabsf(self->model.actor.world.pos.y - player->actor.world.pos.y) < 45 &&
            TwinCanWalkTo(play, &self->model.actor, &player->actor.world.pos);
        if (self->assist) {
            actor->shape.rot.y = actor->world.rot.y = player->actor.shape.rot.y;
            TwinPlay(self, play, interact->id == ACTOR_OBJ_OSHIHIKI || interact->id == ACTOR_BG_PUSHBOX ?
                gPlayerAnim_link_normal_pushing : gPlayerAnim_link_normal_heavy_carry, true, 1);
        }
        return;
    }
    f32 distance = Actor_WorldDistXZToActor(actor, &player->actor);
    if (distance > 110) self->following = true;
    if (distance < 60) self->following = false;
    Vec3f goal = self->following ? player->actor.world.pos : actor->world.pos;
    TwinWalk(self, play, &goal, self->following ? 50 : 0);
    if (!self->following) TwinFacePlayer(self, player);
}

static void TwinStrength(bool* should, va_list args) {
    s32* value = va_arg(args, s32*);
    if (!sTwinActive || sTwinMode != TWIN_HELP || sTwinLink == NULL || !sTwinLink->assist ||
        gPlayState == NULL || TwinBusy(gPlayState, GET_PLAYER(gPlayState))) return;
    Player* link = GET_PLAYER(gPlayState);
    if (TwinInteractObject(gPlayState, link) == NULL ||
        Actor_WorldDistXZToActor(&sTwinLink->model.actor, &link->actor) > TWIN_HELP_DISTANCE) return;
    s32 upgrade = CUR_UPG_VALUE(UPG_STRENGTH);
    s32 base = *should ? (LINK_IS_ADULT ? upgrade : (upgrade != 0 ? PLAYER_STR_BRACELET : PLAYER_STR_NONE)) : *value;
    *value = MIN(PLAYER_STR_GOLD_G, base + 1);
    *should = false;
}

static void TwinPickupCucco(TwinLink* self, PlayState* play) {
    Actor* actor = &self->model.actor;
    Actor* cucco = TwinActorIsLive(play, self->taskActor) ? self->taskActor : NULL;
    ++self->ticks; actor->speedXZ = 0;
    if (cucco == NULL || (cucco->parent != NULL && cucco->parent != actor) ||
        (self->ticks <= TWIN_CUCCO_GRAB_FRAME && (Actor_WorldDistXZToActor(actor, cucco) > 65 ||
            fabsf(actor->world.pos.y - cucco->world.pos.y) > 40))) {
        self->taskActor = NULL;
        TwinStartAction(self, play, TWIN_IDLE);
    } else {
        if (self->ticks == TWIN_CUCCO_GRAB_FRAME) { cucco->parent = actor; self->carriedCucco = cucco; }
        TwinPositionCucco(self, play);
        if (self->ticks >= TWIN_CUCCO_PICKUP_FRAMES) {
            self->taskActor = NULL;
            TwinStartAction(self, play, TWIN_IDLE);
            if (TwinRecallAtLink(self, play, true)) Audio_PlayActorSound2(actor, NA_SE_EV_FANTOM_WARP_S);
        }
    }
    Actor_MoveXZGravity(actor);
    Actor_UpdateBgCheckInfo(play, actor, 28, 14, 60, 5);
}

static void TwinPrepareBomb(EnBom* bomb) {
    // Native bombs start at scale zero and grow at timer 67; our shorter fuse skips it.
    Actor_SetScale(&bomb->actor, .01f);
    bomb->timer = 40;
    bomb->actor.draw = TwinBombDraw;
}

static void TwinBombDraw(Actor* actor, PlayState* play) {
    if (actor->params != BOMB_BODY) return;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    func_8002EBCC(actor, play, 0);
    gSPGrayscale(POLY_XLU_DISP++, true);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, 24, 24, 32, 220);
    gDPSetEnvColor(POLY_XLU_DISP++, 18, 18, 24, 185);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 18, 18, 24, 185);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBombCapDL);
    Matrix_RotateZYX(0x4000, 0, 0, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBombBodyDL);
    Collider_UpdateSpheres(0, &((EnBom*)actor)->explosionCollider);
    gSPGrayscale(POLY_XLU_DISP++, false);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void TwinPlaceBomb(TwinLink* self, PlayState* play) {
    Vec3f p = self->taskPoint;
    EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM,
        p.x, self->model.actor.world.pos.y, p.z, 0, 0, 0, BOMB_BODY);
    if (bomb != NULL) TwinPrepareBomb(bomb);
    self->taskActor = NULL;
}

static void TwinUpdateAttack(TwinLink* self, PlayState* play) {
    Actor* actor = &self->model.actor;
    Actor* target = TwinActorIsLive(play, self->taskActor) ? self->taskActor : NULL;
    ++self->ticks;
    actor->speedXZ = 0;
    if (target != NULL) actor->shape.rot.y = actor->world.rot.y = Actor_WorldYawTowardActor(actor, target);
    if (self->action == TWIN_SLASH && self->ticks >= 3 && self->ticks <= 10 && !self->hit) {
        if (self->blade.base.atFlags & (AT_HIT | AT_BOUNCED)) self->hit = true;
        else {
            Vec3f point = actor->world.pos;
            point.x += Math_SinS(actor->shape.rot.y) * 38;
            point.z += Math_CosS(actor->shape.rot.y) * 38;
            self->blade.dim.pos = point;
            CollisionCheck_SetAT(play, &play->colChkCtx, &self->blade.base);
        }
    }
    if (self->ticks == 6 && (self->action == TWIN_SHOOT || self->action == TWIN_ICE)) {
        self->clip = NULL;
        TwinPlay(self, play, gPlayerAnim_link_bow_bow_shoot, false, 1);
    }
    if (self->ticks == 8) {
        if (self->action == TWIN_SHOOT || self->action == TWIN_ICE || self->action == TWIN_NUT)
            TwinFireArrow(self, play, target, self->action);
        else if (self->action == TWIN_BOOM) TwinThrowBoom(self, play, target);
        else if (self->action == TWIN_BOMB) TwinPlaceBomb(self, play);
    }
    if (self->ticks >= 20) {
        self->taskActor = NULL;
        self->cooldown = self->action == TWIN_SLASH ? 12 : 55;
        TwinStartAction(self, play, TWIN_IDLE);
    }
    Actor_MoveXZGravity(actor);
    Actor_UpdateBgCheckInfo(play, actor, 28, 14, 60, 5);
}

static void TwinUpdateActor(Actor* actor, PlayState* play) {
    TwinLink* self = (TwinLink*)actor;
    Player* link = GET_PLAYER(play);
    if (!sTwinActive || gSaveContext.health <= 0 || (link->stateFlags1 & PLAYER_STATE1_DEAD)) {
        Actor_Kill(actor); return; // TwinTick records the death in per-file storage.
    }
    bool heavyLift = sTwinMode == TWIN_HELP && (link->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) &&
        (TwinStrengthObject(link->heldActor) || TwinStrengthObject(link->interactRangeActor)) &&
        !(link->stateFlags1 & (PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_ON_HORSE)) &&
        play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF;
    if (sTwinMenu || play->pauseCtx.state != 0 || play->msgCtx.msgMode != MSGMODE_NONE ||
        (TwinBusy(play, link) && !heavyLift)) return;
    if (!self->animationReady) return;
    AnimationContext_SetNextQueue(play);
    TwinRecall(self, play);
    if (self->cooldown > 0) --self->cooldown;
    if (self->cuccoCooldown > 0) --self->cuccoCooldown;
    if (self->specialCooldown > 0) --self->specialCooldown;
    if (self->stunApproach > 0) --self->stunApproach;
    TwinEquipment(self, play);
    if (self->emerge > 0) --self->emerge;
    self->assist = false;
    self->carryingTogether = false;
    if (TwinCarryTogether(self, play)) {
        // Draw copies Link's current lift/wait/throw pose, after animation queues finish.
    } else if (self->action == TWIN_LIFT) {
        actor->speedXZ = 0;
        if (sTwinPlayerAction != TWIN_PLAYER_LAUNCH) {
            self->cooldown = 40;
            TwinStartAction(self, play, TWIN_IDLE);
        } else if (++self->ticks >= Animation_GetLastFrame((void*)gPlayerAnim_link_normal_carryB_free))
            TwinPlay(self, play, gPlayerAnim_link_normal_carryB_wait, true, 1);
        Actor_MoveXZGravity(actor);
        Actor_UpdateBgCheckInfo(play, actor, 28, 14, 60, 5);
    } else if (self->action == TWIN_CUCCO_SHADOW) {
        TwinUpdateCuccoShadow(self, play);
    } else if (self->action == TWIN_CUCCO_PICKUP) {
        TwinPickupCucco(self, play);
    } else if (self->action != TWIN_IDLE) TwinUpdateAttack(self, play);
    else {
        Vec3f goal = link->actor.world.pos;
        goal.x += Math_SinS(link->actor.shape.rot.y + 0xA000) * TWIN_FOLLOW_DISTANCE;
        goal.z += Math_CosS(link->actor.shape.rot.y + 0xA000) * TWIN_FOLLOW_DISTANCE;
        if (sTwinMode == TWIN_WAIT) goal = self->waitPoint;
        bool fighting = sTwinMode == TWIN_ATTACK && TwinCombat(self, play);
        if (fighting) { /* Combat already advanced motion or started an attack. */ }
        else if (sTwinMode == TWIN_HELP) {
            TwinHelp(self, play);
        } else TwinWalk(self, play, &goal, 20);
    }
    LinkAnimation_Update(play, &self->model.skelAnime);
    if (TwinUsesUpper(self)) {
        static u8 upperMap[PLAYER_LIMB_MAX] = { 0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1 };
        LinkAnimation_Update(play, &self->upper);
        AnimationContext_SetCopyTrue(play, self->model.skelAnime.limbCount, self->pose, self->upperPose, upperMap);
    }
}

// Draw current Link geometry (including child/custom skeleton), with Torch2 face textures
// and its translucent render list. Do not call Player_DrawImpl: that overwrites both with
// normal Link textures/tunic colours and can trigger equipment hooks for the real player.
static void TwinRootPosition(Vec3f* pos) {
    if (LINK_IS_ADULT) return;
    pos->x *= .64f;
    pos->y *= .64f;
    pos->z *= .64f;
}

static s32 TwinOverrideLimb(PlayState* play, s32 limb, Gfx** dl, Vec3f* pos, Vec3s* rot, void* data, Gfx** gfx) {
    TwinLink* self = data;
    Player* model = &self->model;
    // Link's animations use adult root translations for both skeletons. Native
    // Player rendering scales the child's root separately; omitting it floats him.
    if (limb == PLAYER_LIMB_ROOT) TwinRootPosition(pos);
    if (limb == PLAYER_LIMB_L_HAND && model->leftHandDLists != NULL) {
        int offset = model->leftHandType == PLAYER_MODELTYPE_LH_BGS && gSaveContext.swordHealth <= 0 ? 4 : 0;
        *dl = model->leftHandDLists[offset];
    } else if (limb == PLAYER_LIMB_R_HAND && model->rightHandDLists != NULL)
        *dl = model->rightHandDLists[model->rightHandType == PLAYER_MODELTYPE_RH_SHIELD ? model->currentShield * 4 : 0];
    else if (limb == PLAYER_LIMB_SHEATH && model->sheathDLists != NULL)
        *dl = model->sheathDLists[(model->sheathType == PLAYER_MODELTYPE_SHEATH_18 ||
            model->sheathType == PLAYER_MODELTYPE_SHEATH_19) ? model->currentShield * 4 : 0];
    else if (limb == PLAYER_LIMB_WAIST && model->waistDLists != NULL) *dl = model->waistDLists[0];
    if (*dl != NULL && ResourceMgr_OTRSigCheck((char*)*dl)) *dl = ResourceMgr_LoadGfxByName((char*)*dl);
    return false;
}

static void TwinPostLimb(PlayState* play, s32 limb, Gfx** dl, Vec3s* rot, void* data, Gfx** gfx) {
    if (limb != PLAYER_LIMB_HEAD || !sApi->HasResource(sTwinMask)) return;
    Matrix_Push();
    Matrix_Scale(.468f, .468f, .468f, MTXMODE_APPLY);
    Matrix_RotateZYX(0x8000, -0x4000, 0x0AAA, MTXMODE_APPLY);
    Matrix_Translate(0, 960, 1283, MTXMODE_APPLY);
    gSPMatrix((*gfx)++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList((*gfx)++, (Gfx*)sTwinMask);
    Matrix_Pop();
}

static f32 TwinCarryRenderOffset(Player* player) {
    if (gPlayState == NULL || player != GET_PLAYER(gPlayState) || !sTwinActive || sTwinMode != TWIN_HELP ||
        sTwinLink == NULL || !sTwinLink->carryingTogether ||
        !(player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR)) return 0;
    Actor* object = player->heldActor != NULL ? player->heldActor : player->interactRangeActor;
    if (!TwinActorIsLive(gPlayState, object) || !TwinStrengthObject(object)) return 0;
    return LINK_IS_ADULT ? 22 : 16;
}

static void TwinPlayerCarryLimb(Player* player, s32 limb, Gfx** dl, Gfx* original, Vec3f* pos) {
    if (limb != PLAYER_LIMB_ROOT) return;
    f32 offset = TwinCarryRenderOffset(player);
    if (offset > 0 && player->actor.scale.x > .00001f)
        pos->x += offset / player->actor.scale.x;
}

static void TwinPlayerCarryPost(PlayState* play, Player* player, s32 limb) {
    f32 offset = TwinCarryRenderOffset(player);
    if (offset == 0 || player->actor.scale.y < 0) return; // Reflection does not update held actors.
    Vec3f delta = { Math_CosS(player->actor.shape.rot.y) * offset, 0,
                   -Math_SinS(player->actor.shape.rot.y) * offset };
    if (limb == PLAYER_LIMB_R_HAND && player->heldActor != NULL) {
        // Native drawing attaches the rock to the shifted hands. Keep its original centre.
        player->heldActor->world.pos.x -= delta.x;
        player->heldActor->world.pos.z -= delta.z;
    } else if (limb == PLAYER_LIMB_HEAD) {
        player->actor.focus.pos.x -= delta.x;
        player->actor.focus.pos.z -= delta.z;
    }
}

static void TwinDrawCuccoShadow(TwinLink* self, PlayState* play) {
    Actor* actor = &self->model.actor;
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0,
                     0, 0, 0, COMBINED, 0, 0, 0, COMBINED);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 12, 5, 20, 230);
    Matrix_Translate(actor->world.pos.x, actor->world.pos.y + 1, actor->world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(actor->shape.rot.y * (M_PI / 0x8000), MTXMODE_APPLY);
    Matrix_Scale(.22f, 1, .30f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gCircleShadowDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void TwinDrawActor(Actor* actor, PlayState* play) {
    TwinLink* self = (TwinLink*)actor;
    if (self->action == TWIN_CUCCO_SHADOW) { TwinDrawCuccoShadow(self, play); return; }
    if (!self->animationReady || self->model.skelAnime.skeleton == NULL) return;
    Player* player = GET_PLAYER(play);
    f32 carryOffset = TwinCarryRenderOffset(player);
    if (carryOffset > 0 && player->skelAnime.jointTable != NULL &&
        player->skelAnime.limbCount == self->model.skelAnime.limbCount)
        memcpy(self->pose, player->skelAnime.jointTable, sizeof(Vec3s) * player->skelAnime.limbCount);
    Vec3f drawPos = actor->world.pos;
    if (carryOffset > 0) {
        drawPos.x = player->actor.world.pos.x - Math_CosS(player->actor.shape.rot.y) * carryOffset;
        drawPos.z = player->actor.world.pos.z + Math_SinS(player->actor.shape.rot.y) * carryOffset;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_SetTranslateRotateYXZ(drawPos.x, drawPos.y, drawPos.z, &actor->shape.rot);
    f32 rise = 1 - self->emerge / 24.0f;
    Matrix_Scale(actor->scale.x, actor->scale.y * MAX(.03f, rise), actor->scale.z, MTXMODE_APPLY);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 18, 18, 24, (u8)(185 * MAX(.1f, rise)));
    gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)D_80116280);
    TwinPrepareFace();
    int age = LINK_IS_ADULT ? 0 : 1;
    const void* eyes = sTwinFaceReady[age] ? (void*)sTwinFaceEyes[age] :
        (const void*)(LINK_IS_ADULT ? gLinkAdultEyesOpenTex : gLinkChildEyesOpenTex);
    const void* mouth = sTwinFaceReady[age] ? (void*)sTwinFaceMouth[age] :
        (const void*)(LINK_IS_ADULT ? gLinkAdultMouth1Tex : gLinkChildMouth1Tex);
    gSPSegment(POLY_XLU_DISP++, 8, (uintptr_t)eyes);
    gSPSegment(POLY_XLU_DISP++, 9, (uintptr_t)mouth);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, 28, 28, 36, 240);
    gSPGrayscale(POLY_XLU_DISP++, true);
    POLY_XLU_DISP = SkelAnime_DrawFlex(play, self->model.skelAnime.skeleton, self->pose,
        self->model.skelAnime.dListCount, TwinOverrideLimb, TwinPostLimb, self, POLY_XLU_DISP);
    gSPGrayscale(POLY_XLU_DISP++, false);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)(D_80116280 + 2));
    gSPSegment(POLY_XLU_DISP++, 8, (uintptr_t)(LINK_IS_ADULT ? gLinkAdultEyesOpenTex : gLinkChildEyesOpenTex));
    gSPSegment(POLY_XLU_DISP++, 9, (uintptr_t)(LINK_IS_ADULT ? gLinkAdultMouth1Tex : gLinkChildMouth1Tex));
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void TwinSpawn(PlayState* play, bool emerging) {
    if (!sTwinActive || sTwinLink != NULL || sTwinLinkId < 0) return;
    Player* player = GET_PLAYER(play);
    Vec3f pos = player->actor.world.pos;
    if (!emerging) {
        pos.x -= Math_SinS(player->actor.shape.rot.y) * 35;
        pos.z -= Math_CosS(player->actor.shape.rot.y) * 35;
    }
    Actor_Spawn(&play->actorCtx, play, sTwinLinkId, pos.x, pos.y, pos.z, 0, player->actor.shape.rot.y, 0, 0);
}

static void TwinPlayerActionHandler(Player* player, PlayState* play) {
    if (sTwinPlayerAction == TWIN_PLAYER_NONE || TwinBusy(play, player)) {
        TwinFinishPlayer(play, player); return;
    }
    if (sTwinPlayerAction == TWIN_PLAYER_MENU) {
        if (!sTwinMenu) { TwinFinishPlayer(play, player); return; }
        player->actor.world.pos = player->actor.prevPos = sTwinMenuAnchor;
        player->actor.velocity = (Vec3f){ 0, 0, 0 };
        player->actor.speedXZ = player->linearVelocity = player->pushedSpeed = 0;
        LinkAnimation_Update(play, &player->skelAnime);
        return;
    }
    if (sTwinPlayerTicks < 30000) ++sTwinPlayerTicks;
    player->actor.speedXZ = player->linearVelocity = 0;
    player->actor.velocity.y = 0;
    LinkAnimation_Update(play, &player->skelAnime);
    if (sTwinPlayerAction == TWIN_PLAYER_SUMMON) {
        if (sTwinPlayerTicks == 18)
            LinkAnimation_Change(play, &player->skelAnime, (LinkAnimationHeader*)gPlayerAnim_link_magic_kaze1,
                1, 0, MAX(0, Animation_GetLastFrame((void*)gPlayerAnim_link_magic_kaze1) - 1), ANIMMODE_ONCE_INTERP, -4);
        if (sTwinPlayerTicks == 30) TwinSpawn(play, true);
        if (sTwinPlayerTicks >= 56) {
            TwinFinishPlayer(play, player);
            if (sTwinOrdersAfterMask) {
                sTwinOrdersAfterMask = false;
                TwinShowMenu(play);
            }
        }
    } else if (sTwinPlayerAction == TWIN_PLAYER_LAUNCH) {
        if (sTwinLink == NULL || (sTwinInput.press.button & BTN_B)) {
            player->actor.world.pos = sTwinLaunchOrigin;
            player->actor.prevPos = player->actor.world.pos;
            TwinFinishPlayer(play, player); return;
        }
        // Move the actual player/collider/camera with the lift, not just his display list.
        Actor* carrier = &sTwinLink->model.actor;
        Vec3f held = carrier->world.pos;
        held.y += MIN(LINK_IS_ADULT ? 54 : 40, sTwinPlayerTicks * 3);
        Math_StepToF(&player->actor.world.pos.x, held.x, 8);
        Math_StepToF(&player->actor.world.pos.z, held.z, 8);
        player->actor.world.pos.y = held.y;
        player->actor.prevPos = player->actor.world.pos;
        player->actor.velocity = (Vec3f){ 0, 0, 0 };
        player->pushedSpeed = 0;
        player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
        sTwinLaunchYaw -= sTwinInput.cur.stick_x * 12;
        sTwinLaunchPitch = CLAMP(sTwinLaunchPitch + sTwinInput.cur.stick_y * 6, 0x0800, 0x3000);
        player->actor.shape.rot.y = player->actor.world.rot.y = sTwinLaunchYaw;
        if ((sTwinInput.press.button & BTN_A) && sTwinPlayerTicks > 18) {
            s16 yaw = sTwinLaunchYaw, pitch = sTwinLaunchPitch;
            TwinFinishPlayer(play, player);
            func_80838940(player, (LinkAnimationHeader*)gPlayerAnim_link_normal_run_jump,
                Math_SinS(pitch) * 22 + 5, play, NA_SE_VO_LI_AUTO_JUMP);
            player->actor.world.rot.y = player->actor.shape.rot.y = player->yaw = yaw;
            player->actor.velocity.y = Math_SinS(pitch) * 22 + 5;
            player->linearVelocity = player->actor.speedXZ = Math_CosS(pitch) * 18;
            player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
            player->stateFlags1 |= PLAYER_STATE1_JUMPING;
            player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
            sTwinLink->cooldown = 60;
            // func_80838940 already calls Player_PlayVoiceSfx with Link's age offset.
        }
    }
}

static void TwinInput(Player* player, Input* input) {
    sTwinInput = *input;
    if (gPlayState != NULL && (input->press.button & BTN_A) && TwinCanOfferLift(sTwinLink, gPlayState, player)) {
        sTwinLiftRequested = true;
        input->cur.button &= ~BTN_A;
        input->press.button &= ~BTN_A;
    }
    bool locked = sTwinMenu || sTwinPendingMode >= 0 || sTwinPlayerAction != TWIN_PLAYER_NONE || sTwinReleaseInput > 0;
    if (sTwinReleaseInput > 0) --sTwinReleaseInput;
    if (locked) {
        input->cur.button = input->prev.button = input->press.button = input->rel.button = 0;
        input->cur.stick_x = input->cur.stick_y = input->prev.stick_x = input->prev.stick_y = 0;
        input->press.stick_x = input->press.stick_y = input->rel.stick_x = input->rel.stick_y = 0;
    }
}

static void TwinApplyOrder(PlayState* play, TwinMode mode, bool initial) {
    if (!sTwinActive || mode >= TWIN_MODE_COUNT || TwinBusy(play, GET_PLAYER(play)) || mode == sTwinMode) return;
    if (!initial && !TwinPay(play)) return;
    sTwinMode = mode;
    if (sTwinLink != NULL) {
        if (sTwinLink->action == TWIN_CUCCO_SHADOW) TwinAbortCuccoFetch(sTwinLink, play);
        TwinReleaseCucco(sTwinLink, play);
        sTwinLink->taskActor = NULL; sTwinLink->launchTask = false; sTwinLink->assist = false;
        sTwinLink->waitPoint = sTwinLink->model.actor.world.pos;
        TwinStartAction(sTwinLink, play, TWIN_IDLE);
    }
    TwinStore();
}

static void TwinTick(void) {
    PlayState* play = gPlayState;
    if (play == NULL) { TwinCloseMenu(NULL); return; }
    Player* player = GET_PLAYER(play);
    if (gSaveContext.health <= 0 || (player->stateFlags1 & PLAYER_STATE1_DEAD)) {
        if (sTwinActive || sTwinPlayerAction != TWIN_PLAYER_NONE || sTwinMenu) { TwinReset(); TwinStore(); }
        return;
    }
    if (sTwinPlayerAction != TWIN_PLAYER_NONE && player->actionFunc != TwinPlayerActionHandler) {
        TwinFinishPlayer(play, player);
        sTwinOrdersAfterMask = false;
        player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    }
    if (sTwinMenu) { TwinMenuUpdate(play); if (sTwinMenu) return; }
    if (sTwinPendingMode >= 0 && play->msgCtx.msgMode == MSGMODE_NONE) {
        TwinMode mode = (TwinMode)sTwinPendingMode;
        bool initial = sTwinPendingInitial;
        sTwinPendingMode = -1; sTwinPendingInitial = false;
        TwinApplyOrder(play, mode, initial);
    }
    if (play->pauseCtx.state == 0 && play->msgCtx.msgMode == MSGMODE_NONE && !TwinBusy(play, player)) {
        if (sTwinPlayerAction == TWIN_PLAYER_NONE) TwinSpawn(play, false);
        bool slash = player->meleeWeaponState != 0;
        if (slash && !sTwinWasSlashing) TwinGiveHelpCommand(play, player);
        sTwinWasSlashing = slash;
        if (sTwinLiftRequested && TwinCanOfferLift(sTwinLink, play, player)) {
            TwinFacePlayer(sTwinLink, player);
            TwinStartAction(sTwinLink, play, TWIN_LIFT);
            TwinBeginPlayer(play, player, TWIN_PLAYER_LAUNCH);
        }
    }
    sTwinLiftRequested = false;
}

static void TwinPlayerUpdated(void) {
    if (gPlayState != NULL && TwinCanOfferLift(sTwinLink, gPlayState, GET_PLAYER(gPlayState)))
        Interface_SetDoAction(gPlayState, DO_ACTION_GRAB);
}

static void TwinScene(s16 scene) {
    TwinCloseMenu(NULL);
    sTwinLink = NULL;
    sTwinMenu = 0; sTwinPendingMode = -1; sTwinOrdersAfterMask = sTwinInitialOrders = sTwinPendingInitial = false;
    sTwinPlayerAction = TWIN_PLAYER_NONE; sTwinWasSlashing = false;
    sTwinLiftCarrier = NULL;
    sTwinLiftRequested = false;
}

static Vtx sTwinAimDot[] = {
    VTX(-2, -2, 0, 0, 0, 80, 210, 255, 200), VTX(2, -2, 0, 0, 0, 80, 210, 255, 200),
    VTX(2, 2, 0, 0, 0, 80, 210, 255, 200), VTX(-2, 2, 0, 0, 0, 80, 210, 255, 200),
};

static void TwinDrawTrajectory(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f origin = player->actor.world.pos, previous = origin;
    f32 speed = Math_CosS(sTwinLaunchPitch) * 18;
    f32 vy = Math_SinS(sTwinLaunchPitch) * 22 + 5;
    f32 gravity = player->actor.gravity < -.1f ? player->actor.gravity : -1.0f;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BOTH);
    gSPTexture(POLY_XLU_DISP++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 80, 210, 255, 200);
    for (int t = 3; t <= 48; t += 3) {
        Vec3f point = {
            origin.x + Math_SinS(sTwinLaunchYaw) * speed * t,
            origin.y + vy * t + .5f * gravity * t * t,
            origin.z + Math_CosS(sTwinLaunchYaw) * speed * t,
        };
        Vec3f hit; CollisionPoly* poly; s32 bg;
        bool blocked = BgCheck_EntityLineTest1(&play->colCtx, &previous, &point, &hit, &poly, true, true, true, true, &bg);
        if (blocked) point = hit;
        Matrix_Push();
        Matrix_Translate(point.x, point.y, point.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)sTwinAimDot, 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
        Matrix_Pop();
        if (blocked) break;
        previous = point;
    }
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
}

static const char* const sTwinModeNames[TWIN_MODE_COUNT] = { "Attack", "Help", "Follow", "Wait" };
static const char* const sTwinModeDescriptions[TWIN_MODE_COUNT] = {
    "Stun, then sword or ranged.", "Bombs, joint lifts and launch.", "Stay close to Link.", "Hold this position.",
};

static void TwinMenuRect(PlayState* play, int x, int y, int w, int h, u8 r, u8 g, u8 b, u8 alpha) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, r, g, b, alpha);
    gDPFillRectangle(OVERLAY_DISP++, x, y, x + w, y + h);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void TwinDrawMenu(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    TwinMenuRect(play, 20, 34, 280, 181, 8, 12, 24, 238);
    for (int mode = 0; mode < TWIN_MODE_COUNT; ++mode) {
        int x = 36 + (mode & 1) * 130, y = 87 + (mode >> 1) * 46;
        bool selected = mode == sTwinMenuSelection;
        TwinMenuRect(play, x, y, 118, 34, selected ? 235 : 70, selected ? 195 : 88, selected ? 80 : 115, 255);
        TwinMenuRect(play, x + 2, y + 2, 114, 30, selected ? 36 : 16, selected ? 44 : 22, selected ? 64 : 36, 255);
        if (sTwinActive && mode == sTwinMode) TwinMenuRect(play, x + 108, y + 5, 4, 4, 90, 230, 130, 255);
    }
    Gfx* branch = POLY_OPA_DISP;
    Gfx* list = Graph_GfxPlusOne(branch);
    gSPDisplayList(OVERLAY_DISP++, list);
    GfxPrint printer;
    GfxPrint_Init(&printer); GfxPrint_Open(&printer, list);
    const char* title = "TWIN LINK - ORDERS";
    GfxPrint_SetColor(&printer, 245, 225, 170, 255);
    GfxPrint_SetPosPx(&printer, (320 - (int)strlen(title) * 8) / 2, 47);
    GfxPrint_Printf(&printer, "%s", title);
    GfxPrint_SetColor(&printer, 210, 220, 235, 255);
    GfxPrint_SetPosPx(&printer, 40, 65);
    GfxPrint_Printf(&printer, "%s", sTwinInitialOrders ? "Choose your initial mode" : "Mode changes cost 24 MP");
    for (int mode = 0; mode < TWIN_MODE_COUNT; ++mode) {
        bool allowed = TwinOrderIsFree(mode) || TwinCanPay();
        GfxPrint_SetColor(&printer, allowed ? 245 : 130, allowed ? 245 : 140, allowed ? 250 : 155, 255);
        int x = 36 + (mode & 1) * 130 + (118 - (int)strlen(sTwinModeNames[mode]) * 8) / 2;
        GfxPrint_SetPosPx(&printer, x, 100 + (mode >> 1) * 46);
        GfxPrint_Printf(&printer, "%s", sTwinModeNames[mode]);
    }
    const char* description = sTwinModeDescriptions[sTwinMenuSelection];
    GfxPrint_SetColor(&printer, 200, 215, 235, 255);
    GfxPrint_SetPosPx(&printer, (320 - (int)strlen(description) * 8) / 2, 178);
    GfxPrint_Printf(&printer, "%s", description);
    GfxPrint_SetPosPx(&printer, 36, 199);
    GfxPrint_Printf(&printer, "Stick / D-pad   A: OK  B: Back");
    list = GfxPrint_Close(&printer); GfxPrint_Destroy(&printer);
    gSPEndDisplayList(list++);
    Graph_BranchDlist(branch, list); POLY_OPA_DISP = list;
    CLOSE_DISPS(play->state.gfxCtx);
}

static void TwinDrawHint(void) {
    PlayState* play = gPlayState;
    if (play == NULL) return;
    if (sTwinMenu) { TwinDrawMenu(play); return; }
    bool lift = sTwinPlayerAction == TWIN_PLAYER_LAUNCH && play->pauseCtx.state == 0;
    if (!lift) return;
    TwinDrawTrajectory(play);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx* branch = POLY_OPA_DISP;
    Gfx* list = Graph_GfxPlusOne(branch);
    gSPDisplayList(OVERLAY_DISP++, list);
    GfxPrint printer;
    GfxPrint_Init(&printer); GfxPrint_Open(&printer, list);
    GfxPrint_SetColor(&printer, 235, 235, 255, 255);
    GfxPrint_SetPosPx(&printer, 32, 190);
    GfxPrint_Printf(&printer, "%s", "Stick: Aim  A: Launch  B: Cancel");
    list = GfxPrint_Close(&printer); GfxPrint_Destroy(&printer);
    gSPEndDisplayList(list++);
    Graph_BranchDlist(branch, list); POLY_OPA_DISP = list;
    CLOSE_DISPS(play->state.gfxCtx);
}

static void TwinRegister(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterActor) || !SOH_MOD_API_HAS(sApi, BlockPlayerInput) ||
        !SOH_MOD_API_HAS(sApi, ReleasePlayerInput) || !SOH_MOD_API_HAS(sApi, RequestTimeControl) ||
        !SOH_MOD_API_HAS(sApi, ReleaseTimeControl)) return;
    SOHActorDefinition actor = { 0 };
    actor.structSize = sizeof(actor);
    actor.key = "nei.twin_link";
    actor.description = "Twinmold: friendly Shadow Link";
    actor.category = ACTORCAT_NPC;
    actor.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED | ACTOR_FLAG_CAN_PRESS_SWITCHES;
    actor.objectId = OBJECT_GAMEPLAY_KEEP;
    actor.instanceSize = sizeof(TwinLink);
    actor.init = TwinInitActor; actor.destroy = TwinDestroyActor; actor.update = TwinUpdateActor; actor.draw = TwinDrawActor;
    sTwinLinkId = sApi->RegisterActor(&actor);
    actor.key = "nei.twin_link.boomerang"; actor.description = "Twin Link's returning boomerang";
    actor.category = ACTORCAT_ITEMACTION; actor.instanceSize = sizeof(TwinBoomerang);
    actor.init = TwinBoomInit; actor.destroy = TwinBoomDestroy; actor.update = TwinBoomUpdate; actor.draw = TwinBoomDraw;
    sTwinBoomId = sApi->RegisterActor(&actor);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, TwinInput);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TwinPlayerUpdated);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, TwinPlayerCarryLimb);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, TwinPlayerCarryPost);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_NIW, TwinCuccoUpdated);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, TwinTick);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, TwinScene);
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, TwinDrawHint);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, TwinStrength);
}
