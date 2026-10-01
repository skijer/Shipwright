#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_Obj_Oshihiki/z_obj_oshihiki.h"

#define FD_MASK_KEY "nei.fierce_deity_mask"
#define FD_FORM_KEY "nei.fierce_deity"
#define FD_BEAM_ACTOR_KEY "nei.fierce_deity_beam"
// No resources live here: a model path is what turns on the host's root scaling, as for Goron.
#define FD_MODEL_PATH "objects/forms/fierce_deity"
// Cell 23 of the mask page is where Majora's Mask keeps the Fierce Deity's Mask.
#define FD_MASK_PAGE 1
#define FD_MASK_SLOT 23

#define BG_ON_GROUND 1
#define FLOOR_TYPE_NO_JUMP 7
#define SLOPE_STEEP 1

// MM draws the Deity at one and a half times Link (func_80123140) and scales his body table to match.
#define FD_ACTOR_SCALE 0.015f
#define LINK_ACTOR_SCALE 0.01f
#define FD_ROOT_SCALE_CHILD (3377.0f / 2376.0f)
#define FD_HEIGHT 124.0f
#define FD_MOTION_SCALE 1.5f
#define FD_LIFT_SPEED 1.75f
#define FD_PUSH_SPEED 1.5f
// Obj_Oshihiki moves one 20-unit cell per push.
#define PUSH_CELL 20.0f
#define FD_TURN_RATE 1200
#define SLOW_ROOM_RUN_SPEED_LIMIT 500

// MM keeps the Deity's voice in its first bank, the ids OoT gives adult Link: same id, the Deity's sample.
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
#define LINK_VOICE_ACTIONS 0x20
typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

#define SWORD_LENGTH 5500.0f
#define SWORD_QUAD_REACH 1200.0f
#define BACK_SWORD_X (-1714.0f)
#define BACK_SWORD_Y (-310.0f)
#define BACK_SWORD_Z 78.0f
#define SLASH_DMG_FLAGS DMG_SLASH_GIANT
#define JUMP_DMG_FLAGS DMG_JUMP_GIANT
#define LUNGE_SPEED 15.0f
#define CHARGE_READY 0.85f
#define CHARGE_RATE 0.02f
#define CHARGE_MAGIC_COST 0x200
#define SPIN_INVULNERABILITY (-8)
// En_M_Thunder colours its charge glow by the sword type in its params: 3 is the two-handed blade.
#define SPIN_EFFECT_SWORD 3

#define BEAM_SPEED 80.0f
#define BEAM_FADE_RATE 0.05f
#define BEAM_TARGET_SCALE 12.0f
#define BEAM_DRAW_SCALE 0.02f
#define BEAM_HOMING_STEP 0x1000
#define BEAM_DAMAGE 4
// NEI's beam flag: OoT names that bit the Kokiri jump slash.
#define BEAM_DMG_FLAGS DMG_JUMP_KOKIRI

#define MM_ANIM_VALUES_PER_FRAME 67
#define MM_ANIM_BASE_TRANSL_X (-57)

#define OBJ_FD "__OTR__objects/object_link_boy/gLinkFierceDeity"
static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__icon_item_static_yar/gItemIconFierceDeityMaskTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__item_name_static/gItemNameFierceDeitysMaskENGTex";
static const ALIGN_ASSET(2) char sGetItemFaceDL[] = "__OTR__objects/object_gi_mask03/gGiFierceDeityMaskFaceDL";
static const ALIGN_ASSET(2) char sGetItemHairDL[] = "__OTR__objects/object_gi_mask03/gGiFierceDeityMaskHairAndHatDL";
static const ALIGN_ASSET(2) char sMaskDL[] = "__OTR__objects/gameplay_keep/gFierceDeityMaskDL";
static const ALIGN_ASSET(2) char sMaskOnPath[] = "__OTR__misc/link_animetion/gPlayerAnim_cl_setmask_Data";
static const ALIGN_ASSET(2) char sSwordBeamDL[] = "__OTR__objects/gameplay_keep/gSwordBeamDL";
static const ALIGN_ASSET(2) char sSwordIconTex[] = "__OTR__icon_item_static_yar/gItemIconFierceDeitySwordTex";
static const ALIGN_ASSET(2) char sEyesTex[] = OBJ_FD "EyesTex";
static const ALIGN_ASSET(2) char sMouthTex[] = OBJ_FD "MouthTex";
static const ALIGN_ASSET(2) char sSwordHandDL[] = OBJ_FD "LeftHandHoldingSwordDL";
static const ALIGN_ASSET(2) char sSwordDL[] = OBJ_FD "SwordDL";
static const ALIGN_ASSET(2) char sBottleHandDL[] = OBJ_FD "LeftHandHoldBottleDL";

static const char* const sFdLimbDL[PLAYER_LIMB_MAX] = {
    NULL,
    NULL,
    OBJ_FD "WaistDL",
    NULL,
    OBJ_FD "RightThighDL",
    OBJ_FD "RightShinDL",
    OBJ_FD "RightFootDL",
    OBJ_FD "LeftThighDL",
    OBJ_FD "LeftShinDL",
    OBJ_FD "LeftFootDL",
    NULL,
    OBJ_FD "HeadDL",
    OBJ_FD "HatDL",
    NULL,
    OBJ_FD "LeftShoulderDL",
    OBJ_FD "LeftForearmDL",
    OBJ_FD "LeftHandDL",
    OBJ_FD "RightShoulderDL",
    OBJ_FD "RightForearmDL",
    OBJ_FD "RightHandDL",
    NULL,
    OBJ_FD "TorsoDL",
};

// Joint translations of gLinkFierceDeitySkel, read from mm.o2r; same limb order as OoT Link.
static const Vec3f sFdJointPos[PLAYER_LIMB_MAX] = {
    { 0, 0, 0 },       { 0, 0, 0 },       { 0, 0, 0 },         { 945, 0, 0 },  { -399, 69, -249 }, { 1306, 0, 0 },
    { 1256, 5, 11 },   { -396, 76, 264 }, { 1304, 0, 0 },      { 1257, 6, 3 }, { 0, 21, -7 },      { 1392, -259, 0 },
    { -298, -700, 0 }, { 0, 0, 0 },       { 1039, -172, 680 }, { 919, 0, 0 },  { 754, 0, 0 },      { 1039, -173, -680 },
    { 919, 0, 0 },     { 754, 0, 0 },     { 978, -692, 342 },  { 0, 0, 0 },
};

typedef struct {
    const char* swing;
    const char* end;
    const char* endLocked;
    u8 hitStart;
    u8 hitEnd;
} Slash;

// The two-handed rows of z_player.c's D_80854190: the Deity swings like the Biggoron Sword.
static const Slash sSlashes[PLAYER_MWA_MAX] = {
    [PLAYER_MWA_FORWARD_SLASH_2H] = { gPlayerAnim_link_fighter_Lnormal_kiru, gPlayerAnim_link_fighter_Lnormal_kiru_end,
                                      gPlayerAnim_link_anchor_Lnormal_kiru_endR, 1, 4 },
    [PLAYER_MWA_FORWARD_COMBO_2H] = { gPlayerAnim_link_fighter_Lnormal_kiru_finsh,
                                      gPlayerAnim_link_fighter_Lnormal_kiru_finsh_end,
                                      gPlayerAnim_link_anchor_Lnormal_kiru_finsh_endR, 1, 7 },
    [PLAYER_MWA_RIGHT_SLASH_2H] = { gPlayerAnim_link_fighter_LLside_kiru, gPlayerAnim_link_fighter_LLside_kiru_end,
                                    gPlayerAnim_link_anchor_LLside_kiru_endL, 0, 5 },
    [PLAYER_MWA_RIGHT_COMBO_2H] = { gPlayerAnim_link_fighter_LLside_kiru_finsh,
                                    gPlayerAnim_link_fighter_LLside_kiru_finsh_end,
                                    gPlayerAnim_link_anchor_LLside_kiru_finsh_endR, 3, 8 },
    [PLAYER_MWA_LEFT_SLASH_2H] = { gPlayerAnim_link_fighter_LRside_kiru, gPlayerAnim_link_fighter_LRside_kiru_end,
                                   gPlayerAnim_link_anchor_LRside_kiru_endR, 0, 5 },
    [PLAYER_MWA_LEFT_COMBO_2H] = { gPlayerAnim_link_fighter_LRside_kiru_finsh,
                                   gPlayerAnim_link_fighter_LRside_kiru_finsh_end,
                                   gPlayerAnim_link_anchor_LRside_kiru_finsh_endL, 1, 5 },
    [PLAYER_MWA_STAB_2H] = { gPlayerAnim_link_fighter_Lpierce_kiru, gPlayerAnim_link_fighter_Lpierce_kiru_end,
                             gPlayerAnim_link_anchor_Lpierce_kiru_endL, 0, 3 },
    [PLAYER_MWA_STAB_COMBO_2H] = { gPlayerAnim_link_fighter_Lpierce_kiru_finsh,
                                   gPlayerAnim_link_fighter_Lpierce_kiru_finsh_end,
                                   gPlayerAnim_link_anchor_Lpierce_kiru_finsh_endR, 1, 8 },
    [PLAYER_MWA_JUMPSLASH_START] = { gPlayerAnim_link_fighter_Lpower_jump_kiru,
                                     gPlayerAnim_link_fighter_Lpower_jump_kiru_hit,
                                     gPlayerAnim_link_fighter_Lpower_jump_kiru_hit, 1, 11 },
    [PLAYER_MWA_JUMPSLASH_FINISH] = { gPlayerAnim_link_fighter_Lpower_jump_kiru_hit,
                                      gPlayerAnim_link_fighter_Lpower_jump_kiru_end,
                                      gPlayerAnim_link_fighter_Lpower_jump_kiru_end, 1, 2 },
    [PLAYER_MWA_SPIN_ATTACK_2H] = { gPlayerAnim_link_fighter_Lrolling_kiru, gPlayerAnim_link_fighter_Lrolling_kiru_end,
                                    gPlayerAnim_link_anchor_Lrolling_kiru_endR, 0, 15 },
    [PLAYER_MWA_BIG_SPIN_2H] = { gPlayerAnim_link_fighter_Wrolling_kiru, gPlayerAnim_link_fighter_Wrolling_kiru_end,
                                 gPlayerAnim_link_anchor_Lrolling_kiru_endR, 0, 16 },
};

// z_player.c's D_80854480, one step later: the two-handed row of each direction.
static const s8 sSlashByStick[] = {
    PLAYER_MWA_STAB_2H,
    PLAYER_MWA_RIGHT_SLASH_2H,
    PLAYER_MWA_RIGHT_SLASH_2H,
    PLAYER_MWA_LEFT_SLASH_2H,
};

#define CHARGE_START_ANIM ((LinkAnimationHeader*)gPlayerAnim_link_fighter_Lpower_kiru_start)
#define CHARGE_WAIT_ANIM ((LinkAnimationHeader*)gPlayerAnim_link_fighter_Lpower_kiru_wait)
#define CHARGE_CANCEL_ANIM ((LinkAnimationHeader*)gPlayerAnim_link_fighter_Lpower_kiru_wait_end)
#define CHARGE_WALK_ANIM ((LinkAnimationHeader*)gPlayerAnim_link_fighter_Lpower_kiru_walk)
#define CHARGE_SIDE_WALK_ANIM ((LinkAnimationHeader*)gPlayerAnim_link_fighter_Lpower_kiru_side_walk)

static const char* const sRequiredHooks[] = {
    "OnPlayerActionHandler",
    "OnPlayerResolveLimbDraw",
    "OnPlayerPostLimbDraw",
    "OnPlayerResolveFaceTextures",
    "OnPlayerResolveAgeProperties",
    "OnInterfaceResolveButtonIcon",
    "OnActorUpdate",
    "OnActorPlaySfx",
    "OnPlayerResolveMotionScale",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

typedef struct {
    Actor actor;
    ColliderCylinder collider;
    LightNode* light;
    LightInfo lightInfo;
    Actor* target;
    f32 life;
    f32 scroll;
} SwordBeam;

static ColliderCylinderInit sBeamColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { BEAM_DMG_FLAGS, 0x00, BEAM_DAMAGE },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { 0, 60, -30, { 0, 0, 0 } },
};

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
void func_80832318(Player* player);
void func_80837918(Player* player, s32 quadIndex, u32 dmgFlags);
void Player_AnimPlayOnceAdjusted(PlayState* play, Player* player, LinkAnimationHeader* anim);
void Player_AnimPlayLoop(PlayState* play, Player* player, LinkAnimationHeader* anim);
void Player_AnimChangeOnceMorph(PlayState* play, Player* player, LinkAnimationHeader* anim);
void Player_StartAnimMovement(PlayState* play, Player* player, s32 flags);
void Player_FinishAnimMovement(Player* player);
void Player_SetParallel(Player* player);
s32 Player_DecelerateToZero(Player* player);
s32 Player_GetMovementSpeedAndYaw(Player* player, f32* speedTarget, s16* yawTarget, f32 speedMode, PlayState* play);
s32 Player_CanSpinAttack(Player* player);
s32 Player_CheckHostileLockOn(Player* player);
s32 Player_StartCsAction(PlayState* play, Player* player);
void Player_SetInvulnerability(Player* player, s32 timer);
void Player_PlayJumpingSfx(Player* player);
void Player_PlayLandingSfx(Player* player);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
int Player_IsZTargeting(Player* player);
s32 func_80842DF4(PlayState* play, Player* player);
s32 func_8084285C(Player* player, f32 arg1, f32 arg2, f32 arg3);
s32 func_8084269C(PlayState* play, Player* player);
void func_8083C50C(Player* player);
void func_8083A098(Player* player, LinkAnimationHeader* anim, PlayState* play);
s32 func_80842964(Player* player, PlayState* play);
s32 func_80840058(Player* player, f32* speedTarget, s16* yawTarget, PlayState* play);
void func_8084029C(Player* player, f32 arg1);
s32 func_80843E64(PlayState* play, Player* player);
void func_8083DFE0(Player* player, f32* speedTarget, s16* yawTarget);
s32 Player_ActionHandler_12(Player* player, PlayState* play);
void Player_SetupRoll(Player* player, PlayState* play);

static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static PlayerAgeProperties sFdBody;
static LinkAnimationHeader sMaskOnAnim;
static s16* sMaskOnFrames;
static s16 sBeamActorId = -1;
static bool sIsSlashQueued;

static void ChargeAction(Player* player, PlayState* play);
static void ChargeWalkAction(Player* player, PlayState* play);
static void ChargeSideWalkAction(Player* player, PlayState* play);

static bool IsFierceDeity(void) {
    return sApi != NULL && sApi->IsFormActive(FD_FORM_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BG_ON_GROUND) != 0;
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 &
             (PLAYER_STATE1_DEAD | PLAYER_STATE1_LOADING | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM |
              PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER |
              PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_ITEM_CS |
              PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED | PLAYER_STATE1_CLIMBING_LADDER |
              PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_SHIELDING));
}

// The Deity wields the Biggoron's hand (ResolveFdEquipment), so vanilla draws it, puts it away and picks the
// two-handed stance; only the blade and its swings are his.
static bool HoldsDeitySword(Player* player) {
    return player->heldItemAction == PLAYER_IA_SWORD_BIGGORON;
}

// B off during a shop or a minigame keeps the blade still.
static bool CanSwing(Player* player) {
    return HoldsDeitySword(player) && gSaveContext.buttonStatus[0] != BTN_DISABLED && CanAct(player);
}

// Only a Giant's Knife wears (func_80842B7C), and the Deity's blade must never spend Link's.
static s32 CheckSwordRebound(PlayState* play, Player* player) {
    f32 swordHealth = gSaveContext.swordHealth;

    gSaveContext.swordHealth = 0.0f;
    s32 hasRebounded = func_80842DF4(play, player);
    gSaveContext.swordHealth = swordHealth;
    return hasRebounded;
}

static bool CanJumpFromFloor(PlayState* play, Player* player) {
    return play->roomCtx.curRoom.behaviorType1 != ROOM_BEHAVIOR_TYPE1_2 && player->actor.floorPoly != NULL &&
           func_80041D4C(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) != FLOOR_TYPE_NO_JUMP &&
           SurfaceType_GetSlope(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) != SLOPE_STEEP;
}

static bool IsJumpSlash(s32 mwa) {
    return mwa == PLAYER_MWA_JUMPSLASH_START || mwa == PLAYER_MWA_JUMPSLASH_FINISH;
}

// ---- sword beam ----

// A beam keeps its target by pointer, and the target may have died since the last frame.
static bool IsActorAlive(PlayState* play, Actor* actor) {
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* it = play->actorCtx.actorLists[category].head; it != NULL; it = it->next) {
            if (it == actor) {
                return true;
            }
        }
    }
    return false;
}

static void PlaceBeamLight(SwordBeam* beam) {
    u8 glow = (u8)(255.0f * beam->life);

    Lights_PointNoGlowSetInfo(&beam->lightInfo, beam->actor.world.pos.x, beam->actor.world.pos.y,
                              beam->actor.world.pos.z, glow, glow, (u8)(100.0f * beam->life), 800);
}

// MM's EnMThunder_Init for the Deity's beam: its disc faces back along its travel, hence the half turn.
static void InitBeam(Actor* actor, PlayState* play) {
    SwordBeam* beam = (SwordBeam*)actor;
    Player* player = GET_PLAYER(play);

    beam->life = 1.0f;
    beam->scroll = 0.0f;
    beam->target = NULL;
    Actor_SetScale(actor, 0.0f);
    actor->room = -1;
    actor->shape.rot.y = actor->world.rot.y + 0x8000;
    actor->shape.rot.x = -actor->world.rot.x;
    Collider_InitCylinder(play, &beam->collider);
    Collider_SetCylinder(play, &beam->collider, actor, &sBeamColliderInit);
    PlaceBeamLight(beam);
    beam->light = LightContext_InsertLight(play, &play->lightCtx, &beam->lightInfo);
    Audio_PlaySoundGeneral(NA_SE_IT_ROLLING_CUT_LV1, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void DestroyBeam(Actor* actor, PlayState* play) {
    SwordBeam* beam = (SwordBeam*)actor;

    Collider_DestroyCylinder(play, &beam->collider);
    if (beam->light != NULL) {
        LightContext_RemoveLight(play, &play->lightCtx, beam->light);
        beam->light = NULL;
    }
}

// NEI steers the beam toward what it was fired at, so a moving enemy is still hit; MM's flies straight.
static void SteerBeam(PlayState* play, SwordBeam* beam) {
    if (beam->target == NULL) {
        return;
    }
    if (!IsActorAlive(play, beam->target)) {
        beam->target = NULL;
        return;
    }
    Vec3f* aim = &beam->target->focus.pos;
    s16 yaw = Math_Vec3f_Yaw(&beam->actor.world.pos, aim) + 0x8000;
    s16 pitch = Math_Vec3f_Pitch(&beam->actor.world.pos, aim);

    Math_ScaledStepToS(&beam->actor.shape.rot.y, yaw, BEAM_HOMING_STEP);
    Math_ScaledStepToS(&beam->actor.world.rot.x, pitch, BEAM_HOMING_STEP);
    beam->actor.shape.rot.x = -beam->actor.world.rot.x;
}

// AT_HIT is read before this frame's SetAT, which clears it.
static bool HasBeamStruck(PlayState* play, SwordBeam* beam) {
    if (!(beam->collider.base.atFlags & AT_HIT)) {
        return false;
    }
    Actor* struck = beam->collider.base.at;
    if (struck != NULL) {
        Vec3f zero = { 0.0f, 0.0f, 0.0f };
        EffectSsBlast_SpawnWhiteCustomScale(play, &struck->focus.pos, &zero, &zero, 100, 250, 8);
    }
    return true;
}

static void UpdateBeam(Actor* actor, PlayState* play) {
    SwordBeam* beam = (SwordBeam*)actor;

    if (Math_StepToF(&beam->life, 0.0f, BEAM_FADE_RATE) || HasBeamStruck(play, beam)) {
        Actor_Kill(actor);
        return;
    }
    SteerBeam(play, beam);
    f32 flat = -BEAM_SPEED * Math_CosS(actor->world.rot.x);
    actor->world.pos.x += flat * Math_SinS(actor->shape.rot.y);
    actor->world.pos.z += flat * Math_CosS(actor->shape.rot.y);
    actor->world.pos.y += -BEAM_SPEED * Math_SinS(actor->world.rot.x);
    Math_SmoothStepToF(&actor->scale.x, BEAM_TARGET_SCALE, 0.6f, 2.0f, 0.0f);
    Actor_SetScale(actor, actor->scale.x);
    beam->scroll += 1.0f;

    beam->collider.dim.radius = (s16)(actor->scale.x * 5.0f);
    beam->collider.dim.pos.x = (s16)((Math_SinS(actor->shape.rot.y) * -5.0f * actor->scale.x) + actor->world.pos.x);
    beam->collider.dim.pos.y = (s16)actor->world.pos.y;
    beam->collider.dim.pos.z = (s16)((Math_CosS(actor->shape.rot.y) * -5.0f * actor->scale.z) + actor->world.pos.z);
    CollisionCheck_SetAT(play, &play->colChkCtx, &beam->collider.base);
    PlaceBeamLight(beam);
}

// MM's EnMThunder_Draw, regular sword beam colours.
static void DrawBeam(Actor* actor, PlayState* play) {
    SwordBeam* beam = (SwordBeam*)actor;
    f32 alphaFrac = beam->life > 0.9f ? 1.0f : beam->life * (10.0f / 9.0f);
    u16 scroll = 0x1FF - ((u16)(s32)(beam->scroll * 10.0f) & 0x1FF);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Scale(BEAM_DRAW_SCALE, BEAM_DRAW_SCALE, BEAM_DRAW_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Gfx* scrollDL = Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 16, 64, 1, 0, scroll, 32, 128);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)scrollDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, (u8)(alphaFrac * 255.0f));
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSwordBeamDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Every swing at something aimed at throws a beam for one unit of magic (MAGIC_CONSUME_DEITY_BEAM), as in MM.
static void FireBeam(PlayState* play, Player* player) {
    Actor* target = player->focusActor != NULL ? player->focusActor : play->actorCtx.targetCtx.arrowPointedActor;

    if (target == NULL || gSaveContext.magic <= 0 || sBeamActorId < 0) {
        return;
    }
    Vec3f* origin = &player->bodyPartsPos[PLAYER_BODYPART_WAIST];
    s16 pitch = Math_Vec3f_Pitch(origin, &target->focus.pos);
    Actor* beam = Actor_Spawn(&play->actorCtx, play, sBeamActorId, origin->x, origin->y, origin->z, pitch,
                              player->actor.shape.rot.y, 0, 0);
    if (beam == NULL) {
        return;
    }
    ((SwordBeam*)beam)->target = target;
    gSaveContext.magic--;
}

// ---- sword ----

static s32 ChooseSlash(Player* player) {
    s32 direction = player->controlStickDirections[player->controlStickDataIndex];

    if (Player_CanSpinAttack(player)) {
        return PLAYER_MWA_SPIN_ATTACK_2H;
    }
    if (direction <= PLAYER_STICK_DIR_NONE) {
        return Player_IsZTargeting(player) ? PLAYER_MWA_FORWARD_SLASH_2H : PLAYER_MWA_RIGHT_SLASH_2H;
    }
    s32 slash = sSlashByStick[direction];
    if (slash == PLAYER_MWA_STAB_2H) {
        player->stateFlags2 |= PLAYER_STATE2_SWORD_LUNGE;
        if (!Player_IsZTargeting(player)) {
            slash = PLAYER_MWA_FORWARD_SLASH_2H;
        }
    }
    return slash;
}

static void SwingAction(Player* player, PlayState* play);

// z_player.c's func_80837948 without the sword lookups: the third swing of a kind becomes its finisher.
static void StartSwing(PlayState* play, Player* player, s32 slash) {
    Player_SetupAction(play, player, SwingAction, 0);
    player->unk_844 = 8;
    if (slash != PLAYER_MWA_JUMPSLASH_FINISH) {
        func_80832318(player);
    }
    if (slash != player->meleeWeaponAnimation || player->unk_845 >= 3) {
        player->unk_845 = 0;
    }
    player->unk_845++;
    if (player->unk_845 >= 3 && slash + 2 < PLAYER_MWA_MAX && sSlashes[slash + 2].swing != NULL) {
        slash += 2;
    }
    player->meleeWeaponAnimation = slash;
    Player_AnimPlayOnceAdjusted(play, player, (LinkAnimationHeader*)sSlashes[slash].swing);
    if (slash != PLAYER_MWA_JUMPSLASH_START) {
        Player_StartAnimMovement(play, player, 0x209);
    }
    player->yaw = player->actor.shape.rot.y;
    u32 dmgFlags = IsJumpSlash(slash) ? JUMP_DMG_FLAGS : SLASH_DMG_FLAGS;
    func_80837918(player, 0, dmgFlags);
    func_80837918(player, 1, dmgFlags);
    FireBeam(play, player);
}

// Vanilla hides the Biggoron ending behind a two-handed check the Deity never passes, so it is kept as is.
static void FinishSwing(PlayState* play, Player* player, const Slash* slash) {
    const char* end = Player_CheckHostileLockOn(player) ? slash->endLocked : slash->end;
    u8 movementFlags = player->skelAnime.movementFlags;

    func_80832318(player);
    player->skelAnime.movementFlags = 0;
    func_8083A098(player, (LinkAnimationHeader*)end, play);
    player->skelAnime.movementFlags = movementFlags;
    player->stateFlags3 |= PLAYER_STATE3_FINISHED_ATTACKING;
}

static void StartSlash(PlayState* play, Player* player) {
    s32 slash = ChooseSlash(player);

    StartSwing(play, player, slash);
    if (slash >= PLAYER_MWA_SPIN_ATTACK_1H) {
        player->stateFlags2 |= PLAYER_STATE2_SPIN_ATTACKING;
        player->unk_858 = 0.5f;
        player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, player->bodyPartsPos[PLAYER_BODYPART_WAIST].x,
                    player->bodyPartsPos[PLAYER_BODYPART_WAIST].y, player->bodyPartsPos[PLAYER_BODYPART_WAIST].z, 0, 0,
                    0, SPIN_EFFECT_SWORD);
    }
}

// z_player.c's Player_Action_808502D0 over the Deity's own rows.
static void SwingAction(Player* player, PlayState* play) {
    const Slash* slash = &sSlashes[player->meleeWeaponAnimation];

    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    if (CheckSwordRebound(play, player)) {
        return;
    }
    func_8084285C(player, 0.0f, slash->hitStart, slash->hitEnd);
    if ((player->stateFlags2 & PLAYER_STATE2_SWORD_LUNGE) && LinkAnimation_OnFrame(&player->skelAnime, 0.0f)) {
        player->linearVelocity = LUNGE_SPEED;
        player->stateFlags2 &= ~PLAYER_STATE2_SWORD_LUNGE;
    }
    if (player->linearVelocity > 12.0f) {
        func_8084269C(play, player);
    }
    Math_StepToF(&player->linearVelocity, 0.0f, 5.0f);
    func_8083C50C(player);
    if (!LinkAnimation_Update(play, &player->skelAnime)) {
        return;
    }
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) && CanSwing(player)) {
        StartSlash(play, player);
        return;
    }
    FinishSwing(play, player, slash);
}

// A blade held back by B off must not fall to vanilla's swing, which would wear a Giant's Knife.
static int32_t HandleSlash(PlayState* play, Player* player) {
    bool wantsSlash = sIsSlashQueued || CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B);

    sIsSlashQueued = false;
    if (!IsFierceDeity() || !HoldsDeitySword(player) || !wantsSlash) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (!CanSwing(player)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    StartSlash(play, player);
    return SOH_FORM_ACTION_STARTED;
}

// ---- jump slash ----

// z_player.c's Player_Action_80844AF4: the landing turns the leap into its finishing blow.
static void JumpSlashAction(Player* player, PlayState* play) {
    f32 speedTarget;
    s16 yawTarget;

    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    player->actor.gravity = -1.2f;
    LinkAnimation_Update(play, &player->skelAnime);
    if (CheckSwordRebound(play, player)) {
        return;
    }
    func_8084285C(player, 6.0f, 7.0f, 99.0f);
    if (!IsGrounded(player)) {
        Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, 0.0f, play);
        func_8083DFE0(player, &speedTarget, &player->yaw);
        return;
    }
    if (func_80843E64(play, player) >= 0) {
        StartSwing(play, player, PLAYER_MWA_JUMPSLASH_FINISH);
        player->unk_845 = 3;
        Player_PlayLandingSfx(player);
    }
}

static void StartJumpSlash(PlayState* play, Player* player, f32 xzVelocity, f32 yVelocity) {
    StartSwing(play, player, PLAYER_MWA_JUMPSLASH_START);
    Player_SetupAction(play, player, JumpSlashAction, 0);
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->yaw = player->actor.shape.rot.y;
    player->linearVelocity = xzVelocity;
    player->actor.velocity.y = yVelocity;
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    player->hoverBootsTimer = 0;
    Player_PlayJumpingSfx(player);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_L);
}

// Z-target's A still owns the side hops and the back flip. Forward, the drawn blade leaps, or he rolls when it
// cannot swing, as Player_ActionHandler_10 does.
static int32_t HandleZTargetA(PlayState* play, Player* player) {
    s32 direction = player->controlStickDirections[player->controlStickDataIndex];

    if (!IsFierceDeity() || !HoldsDeitySword(player) || direction > PLAYER_STICK_DIR_FORWARD ||
        !Player_IsZTargeting(player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) || !CanJumpFromFloor(play, player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (!CanSwing(player)) {
        Player_SetupRoll(player, play);
        return SOH_FORM_ACTION_STARTED;
    }
    StartJumpSlash(play, player, 5.0f, 5.0f);
    return SOH_FORM_ACTION_STARTED;
}

// Vanilla's jump reads B for its jump slash (func_8083BBA0) through Player_UseItem, which KeepBForBlade holds
// back, so the Deity reads it here.
static void SlashFromJump(PlayState* play, Player* player) {
    bool isJumping =
        (player->stateFlags1 & PLAYER_STATE1_JUMPING) && !IsGrounded(player) && player->actionFunc != JumpSlashAction;

    if (!isJumping || !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) || !CanSwing(player)) {
        return;
    }
    StartJumpSlash(play, player, 3.0f, 4.5f);
}

// ---- spin charge ----

static void StepCharge(Player* player) {
    Math_StepToF(&player->unk_858, 1.0f, CHARGE_RATE);
}

// z_player.c's func_80844BE4: letting go of B throws the spin, the great one once the charge is full.
static bool ReleaseCharge(PlayState* play, Player* player) {
    if (Player_StartCsAction(play, player)) {
        player->stateFlags2 |= PLAYER_STATE2_SPIN_ATTACKING;
        return true;
    }
    if (CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B)) {
        return false;
    }
    s32 spin = (player->unk_858 >= CHARGE_READY || Player_CanSpinAttack(player)) ? PLAYER_MWA_BIG_SPIN_2H
                                                                                 : PLAYER_MWA_SPIN_ATTACK_2H;
    StartSwing(play, player, spin);
    Player_SetInvulnerability(player, SPIN_INVULNERABILITY);
    player->stateFlags2 |= PLAYER_STATE2_SPIN_ATTACKING;
    if (player->controlStickDirections[player->controlStickDataIndex] == PLAYER_STICK_DIR_FORWARD) {
        player->stateFlags2 |= PLAYER_STATE2_SWORD_LUNGE;
    }
    return true;
}

static void CancelCharge(PlayState* play, Player* player) {
    func_80839FFC(player, play);
    func_80832318(player);
    Player_AnimChangeOnceMorph(play, player, CHARGE_CANCEL_ANIM);
    player->yaw = player->actor.shape.rot.y;
}

static void StandCharging(PlayState* play, Player* player) {
    Player_SetupAction(play, player, ChargeAction, 1);
    player->unk_868 = 0.0f;
    Player_AnimPlayLoop(play, player, CHARGE_WAIT_ANIM);
    player->av2.actionVar2 = 1;
}

// z_player.c's Player_Action_80844E68.
static void ChargeAction(Player* player, PlayState* play) {
    f32 speedTarget;
    s16 yawTarget;

    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        Player_FinishAnimMovement(player);
        Player_SetParallel(player);
        player->stateFlags1 &= ~PLAYER_STATE1_PARALLEL;
        Player_AnimPlayLoop(play, player, CHARGE_WAIT_ANIM);
        player->av2.actionVar2 = -1;
    }
    Player_DecelerateToZero(player);
    if (func_80842964(player, play) || player->av2.actionVar2 == 0) {
        return;
    }
    StepCharge(player);
    if (player->av2.actionVar2 < 0) {
        if (player->unk_858 >= 0.1f) {
            player->unk_845 = 0;
            player->av2.actionVar2 = 1;
        } else if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B)) {
            CancelCharge(play, player);
        }
        return;
    }
    if (ReleaseCharge(play, player)) {
        return;
    }
    Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, 0.0f, play);
    s32 heading = func_80840058(player, &speedTarget, &yawTarget, play);
    if (heading > 0) {
        Player_SetupAction(play, player, ChargeWalkAction, 1);
    } else if (heading < 0) {
        Player_SetupAction(play, player, ChargeSideWalkAction, 1);
    }
}

// The walking charges' shared tail: creep toward the stick at a fifth of the speed, stand still once stopped.
static void CreepWhileCharging(PlayState* play, Player* player, f32 standingSpeed, bool isSideways) {
    f32 speedTarget;
    s16 yawTarget;

    StepCharge(player);
    Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, 0.0f, play);
    s32 heading = func_80840058(player, &speedTarget, &yawTarget, play);
    if (isSideways ? heading > 0 : heading < 0) {
        Player_SetupAction(play, player, isSideways ? ChargeWalkAction : ChargeSideWalkAction, 1);
        return;
    }
    if (heading == 0) {
        speedTarget = 0.0f;
        yawTarget = player->yaw;
    }
    s16 turn = yawTarget - player->yaw;
    s32 turnSize = ABS(turn);
    if (turnSize > 0x4000) {
        if (Math_StepToF(&player->linearVelocity, 0.0f, 1.0f)) {
            player->yaw = yawTarget;
        }
        return;
    }
    Math_AsymStepToF(&player->linearVelocity, speedTarget * 0.2f, 1.0f, 0.5f);
    Math_ScaledStepToS(&player->yaw, yawTarget, turnSize * 0.1f);
    if (speedTarget == 0.0f && player->linearVelocity == 0.0f && standingSpeed == 0.0f) {
        StandCharging(play, player);
    }
}

// z_player.c's Player_Action_80845000.
static void ChargeWalkAction(Player* player, PlayState* play) {
    s16 turn = player->yaw - player->actor.shape.rot.y;
    f32 speed = fabsf(player->linearVelocity);
    f32 legRate = MAX(speed * 1.5f, 1.5f);

    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    func_8084029C(player, (ABS(turn) < 0x4000 ? -1.0f : 1.0f) * legRate);
    LinkAnimation_BlendToJoint(play, &player->skelAnime, CHARGE_WAIT_ANIM, 0.0f, CHARGE_WALK_ANIM,
                               player->unk_868 * (21.0f / 29.0f), CLAMP(speed * 0.5f, 0.5f, 1.0f), player->blendTable);
    if (func_80842964(player, play) || ReleaseCharge(play, player)) {
        return;
    }
    CreepWhileCharging(play, player, 0.0f, false);
}

// z_player.c's Player_Action_80845308.
static void ChargeSideWalkAction(Player* player, PlayState* play) {
    f32 speed = fabsf(player->linearVelocity);

    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    if (speed == 0.0f) {
        speed = ABS(player->unk_87C) * 0.0015f;
        if (speed < 400.0f) {
            speed = 0.0f;
        }
        func_8084029C(player, (player->unk_87C >= 0 ? 1 : -1) * speed);
    } else {
        func_8084029C(player, MAX(speed * 1.5f, 1.5f));
    }
    LinkAnimation_BlendToJoint(play, &player->skelAnime, CHARGE_WAIT_ANIM, 0.0f, CHARGE_SIDE_WALK_ANIM,
                               player->unk_868 * (21.0f / 29.0f), CLAMP(speed * 0.5f, 0.5f, 1.0f), player->blendTable);
    if (func_80842964(player, play) || ReleaseCharge(play, player)) {
        return;
    }
    CreepWhileCharging(play, player, speed, true);
}

// z_player.c's Player_ActionHandler_8: B held through a swing until its countdown reaches one starts the charge.
static int32_t HandleSpinCharge(PlayState* play, Player* player) {
    if (!IsFierceDeity() || !CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (player->unk_844 != 1 || !CanSwing(player)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    Player_SetupAction(play, player, ChargeAction, 1);
    func_80832318(player);
    LinkAnimation_Change(play, &player->skelAnime, CHARGE_START_ANIM, 1.0f, 8.0f,
                         Animation_GetLastFrame(CHARGE_START_ANIM), ANIMMODE_ONCE, -9.0f);
    player->unk_858 = 0.0f;
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, player->bodyPartsPos[PLAYER_BODYPART_WAIST].x,
                player->bodyPartsPos[PLAYER_BODYPART_WAIST].y, player->bodyPartsPos[PLAYER_BODYPART_WAIST].z, 0, 0, 0,
                SPIN_EFFECT_SWORD | CHARGE_MAGIC_COST);
    return SOH_FORM_ACTION_STARTED;
}

// ---- blade ----

// The trail vertex vanilla added this frame carries the knife's tip; flags 8 on Link's trail copy it unsmoothed.
static void RetipNewestTrailVertex(EffectBlure* trail, Vec3f* knifeTip, Vec3f* bladeTip) {
    if (trail == NULL || trail->numElements <= 0) {
        return;
    }
    EffectBlureElement* newest = &trail->elements[trail->numElements - 1];
    if (newest->state != 1 || newest->p1.x != (s16)knifeTip->x || newest->p1.y != (s16)knifeTip->y ||
        newest->p1.z != (s16)knifeTip->z) {
        return;
    }
    newest->p1.x = (s16)bladeTip->x;
    newest->p1.y = (s16)bladeTip->y;
    newest->p1.z = (s16)bladeTip->z;
}

// Without a whole Giant's Knife vanilla places the broken one, 1500 long; this lays the Biggoron's 5500 back
// over it, as z_player_lib.c's func_80090A28 and func_800906D4 would for a whole blade.
static void StretchBrokenBlade(PlayState* play, Player* player) {
    static Vec3f sBases[3] = { { 0.0f, 400.0f, 0.0f }, { 0.0f, 1400.0f, -1000.0f }, { 0.0f, -400.0f, 1000.0f } };
    EffectBlure* trail = Effect_GetByIndex(player->meleeWeaponEffectIndex);
    Vec3f knifeTip = player->meleeWeaponInfo[0].tip;
    f32 reach = SWORD_LENGTH;
    Vec3f tip;
    Vec3f base;

    if (player->unk_845 >= 3) {
        reach *= 1.0f + ((9 - player->unk_845) * 0.1f);
    }
    reach += SWORD_QUAD_REACH;
    Vec3f tips[3] = { { SWORD_LENGTH, 400.0f, 0.0f }, { reach, -400.0f, 1000.0f }, { reach, 1400.0f, -1000.0f } };

    Matrix_MultVec3f(&tips[0], &tip);
    Matrix_MultVec3f(&sBases[0], &base);
    func_80090480(play, NULL, &player->meleeWeaponInfo[0], &tip, &base);
    RetipNewestTrailVertex(trail, &knifeTip, &player->meleeWeaponInfo[0].tip);
    if (trail != NULL) {
        EffectBlure_ChangeType(trail, TRAIL_TYPE_BIGGORON_SWORD);
    }
    if (player->meleeWeaponState <= 0 || (player->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H &&
                                          !(player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING))) {
        return;
    }
    for (s32 quad = 0; quad < 2; quad++) {
        Matrix_MultVec3f(&tips[quad + 1], &tip);
        Matrix_MultVec3f(&sBases[quad + 1], &base);
        func_80090480(play, &player->meleeWeaponQuads[quad], &player->meleeWeaponInfo[quad + 1], &tip, &base);
    }
}

// OoT's sheathed Master Sword is its held mesh moved by (-714, -310, 78) in the sheath limb (measured in
// oot.o2r); the Deity's blade is 2000 longer, so it rides 1000 higher to share the overhang with the hilt.
static void DrawSwordOnBack(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(BACK_SWORD_X, BACK_SWORD_Y, BACK_SWORD_Z, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sSwordDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawBlade(PlayState* play, Player* player, int32_t limbIndex) {
    if (!IsFierceDeity()) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_SHEATH && !HoldsDeitySword(player)) {
        DrawSwordOnBack(play);
    } else if (limbIndex == PLAYER_LIMB_L_HAND && player->meleeWeaponState != 0 && Player_HoldsBrokenKnife(player)) {
        StretchBrokenBlade(play, player);
    }
}

// ---- body ----

static bool NameHas(Gfx* dList, const char* part) {
    return dList != NULL && ResourceMgr_OTRSigCheck((char*)dList) && strstr((const char*)dList, part) != NULL;
}

// Vanilla's left hand says what is held: the Biggoron, whole or broken, is the Deity's blade. A child's
// Biggoron hand is the Master Sword one.
static const char* LeftHandDL(Gfx* vanilla) {
    if (NameHas(vanilla, "Bottle")) {
        return sBottleHandDL;
    }
    if (NameHas(vanilla, "Hammer") || NameHas(vanilla, "Boomerang")) {
        return NULL;
    }
    if (NameHas(vanilla, "Bgs") || NameHas(vanilla, "GiantsKnife") || NameHas(vanilla, "HoldingMasterSword")) {
        return sSwordHandDL;
    }
    return sFdLimbDL[PLAYER_LIMB_L_HAND];
}

static const char* RightHandDL(Gfx* vanilla) {
    if (NameHas(vanilla, "Holding") || NameHas(vanilla, "Ocarina") || NameHas(vanilla, "Slingshot")) {
        return NULL;
    }
    return sFdLimbDL[PLAYER_LIMB_R_HAND];
}

static void ResolveFdBody(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!IsFierceDeity() || limbIndex <= PLAYER_LIMB_NONE || limbIndex >= PLAYER_LIMB_MAX) {
        return;
    }
    if (limbIndex != PLAYER_LIMB_ROOT && pos != NULL) {
        *pos = sFdJointPos[limbIndex];
    }
    if (limbIndex == PLAYER_LIMB_COLLAR || limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = NULL;
        return;
    }
    const char* path = sFdLimbDL[limbIndex];
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        path = LeftHandDL(*dList);
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        path = RightHandDL(*dList);
    }
    if (path != NULL) {
        *dList = ResourceMgr_LoadGfxByName(path);
    }
}

static void ResolveFdFace(const char** eyes, const char** mouth) {
    if (!IsFierceDeity()) {
        return;
    }
    *eyes = sEyesTex;
    *mouth = sMouthTex;
}

// MM's sPlayerAgeProperties[FIERCE_DEITY]: Link's body measures at one and a half times, the rest stays his.
static void ResolveFdAgeProperties(Player* player, PlayerAgeProperties** properties) {
    if (!IsFierceDeity() || *properties == NULL) {
        return;
    }
    if (*properties != &sFdBody) {
        sFdBody = **properties;
    }
    sFdBody.ceilingCheckHeight = 84.0f;
    sFdBody.unk_08 = 1.5f;
    sFdBody.unk_0C = 166.5f;
    sFdBody.unk_10 = 105.0f;
    sFdBody.unk_14 = 119.1f;
    sFdBody.unk_18 = 88.5f;
    sFdBody.unk_1C = 61.5f;
    sFdBody.unk_20 = 28.5f;
    sFdBody.unk_24 = 54.0f;
    sFdBody.unk_28 = 75.0f;
    sFdBody.unk_2C = 84.0f;
    sFdBody.unk_30 = 102.0f;
    sFdBody.unk_34 = 70.0f;
    // MM's 27 breaks pushing: func_8083F524 stands him radius + 5 off the block and looks for it only 30 ahead.
    sFdBody.wallCheckRadius = 24.0f;
    sFdBody.unk_3C = 24.75f;
    sFdBody.unk_40 = 105.0f;
    *properties = &sFdBody;
}

// NEI's Deity lifts like a Goron wearing the Golden Gauntlets.
static void ResolveFdStrength(bool* should, va_list args) {
    int32_t* strength = va_arg(args, int32_t*);

    if (!IsFierceDeity()) {
        return;
    }
    *strength = PLAYER_STR_GOLD_G;
    *should = false;
}

// B always swings the Deity's blade, whatever it holds, so MM's icon replaces it.
static void ShowSwordOnB(PlayState* play, uint8_t button, uint16_t item, const char** iconPath) {
    (void)play;
    (void)item;
    if (button != 0 || !IsFierceDeity()) {
        return;
    }
    *iconPath = sSwordIconTex;
}

// ---- lifetime ----

static bool IsSwordAction(Player* player) {
    return player->actionFunc == SwingAction || player->actionFunc == JumpSlashAction ||
           player->actionFunc == ChargeAction || player->actionFunc == ChargeWalkAction ||
           player->actionFunc == ChargeSideWalkAction;
}

static void EnterFierceDeity(PlayState* play, Player* player) {
    (void)play;
    sIsSlashQueued = false;
    Actor_SetScale(&player->actor, FD_ACTOR_SCALE);
}

// The borrowed Biggoron leaves with the Deity: id, action and model group change together, as OoT changes them.
static void SheatheDeitySword(Player* player) {
    player->heldItemId = ITEM_NONE;
    player->heldItemAction = PLAYER_IA_NONE;
    func_8008EC70(player);
}

static void ExitFierceDeity(PlayState* play, Player* player) {
    bool wasSwinging = IsSwordAction(player);

    Actor_SetScale(&player->actor, LINK_ACTOR_SCALE);
    Player_SetBootData(play, player);
    func_80832318(player);
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    if (HoldsDeitySword(player)) {
        SheatheDeitySword(player);
    }
    if (wasSwinging) {
        func_80839FFC(player, play);
    }
}

// ---- strength ----

// Vanilla drops the body by the height left to climb times 100, a world unit only at Link's scale, so the drop
// is brought back to the Deity's.
static int32_t HandleLedge(PlayState* play, Player* player) {
    f32 restOffset = player->actor.shape.yOffset;
    if (!Player_ActionHandler_12(player, play)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    f32 drop = player->actor.shape.yOffset - restOffset;
    player->actor.shape.yOffset = restOffset + drop * (LINK_ACTOR_SCALE / player->actor.scale.y);
    return SOH_FORM_ACTION_STARTED;
}

static bool IsAnimNamed(LinkAnimationHeader* anim, const char* suffix) {
    if (anim == NULL || !ResourceMgr_OTRSigCheck((char*)anim)) {
        return false;
    }
    size_t length = strlen((const char*)anim);
    size_t suffixLength = strlen(suffix);
    return length >= suffixLength && strcmp((const char*)anim + length - suffixLength, suffix) == 0;
}

static bool IsLifting(Player* player) {
    LinkAnimationHeader* anim = player->skelAnime.animation;

    return (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) &&
           (IsAnimNamed(anim, "_carryB") || IsAnimNamed(anim, "silver_carry") || IsAnimNamed(anim, "heavy_carry"));
}

// The lift actions test frames by crossing, so a faster clip still catches the grab.
static void HurryLift(Player* player) {
    if (IsLifting(player) && player->skelAnime.playSpeed > 0.0f && player->skelAnime.playSpeed <= 1.0f) {
        player->skelAnime.playSpeed *= FD_LIFT_SPEED;
    }
}

// Runs after the block stepped its push; the Deity is carried the same stretch so his hands stay on it.
static void HurryPushedBlock(void* actor) {
    ObjOshihiki* block = (ObjOshihiki*)actor;

    if (!IsFierceDeity() || gPlayState == NULL || !(block->stateFlags & PUSHBLOCK_PUSH) || block->pushDist <= 0.0f) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (!(player->stateFlags2 & PLAYER_STATE2_MOVING_DYNAPOLY) || player->unk_3C4 != &block->dyna.actor) {
        return;
    }
    f32 before = block->pushDist;
    Math_StepToF(&block->pushDist, PUSH_CELL, block->pushSpeed * (FD_PUSH_SPEED - 1.0f));
    f32 step = (block->pushDist - before) * (block->direction >= 0.0f ? 1.0f : -1.0f);
    block->dyna.actor.world.pos.x += step * block->yawSin;
    block->dyna.actor.world.pos.z += step * block->yawCos;
    player->actor.world.pos.x += step * block->yawSin;
    player->actor.world.pos.z += step * block->yawCos;
}

// ---- stride and voice ----

// MM's func_80123140 with its PLAYER_BOOTS_FIERCE_DEITY row, less REG(39) that OoT lacks: a higher top speed,
// a slower start and turn, longer leaps off ledges. Iron and Hover Boots keep their rows for their puzzles.
static void WearFdBoots(PlayState* play, Player* player) {
    if (player->currentBoots != PLAYER_BOOTS_KOKIRI) {
        return;
    }
    REG(19) = 200;
    REG(27) = FD_TURN_RATE;
    REG(30) = 666;
    REG(32) = 200;
    REG(34) = 700;
    REG(35) = 366;
    REG(36) = 200;
    REG(37) = 600;
    REG(38) = 175;
    REG(43) = 800;
    R_RUN_SPEED_LIMIT = 1000;
    REG(68) = -100;
    REG(69) = 600;
    IREG(66) = 590;
    IREG(67) = 800;
    IREG(68) = 125;
    IREG(69) = 300;
    MREG(95) = 65;
    if (play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_2) {
        R_RUN_SPEED_LIMIT = SLOW_ROOM_RUN_SPEED_LIMIT;
    }
}

// MM scales the stick before the run cap and the host after it, so the cap shrinks by the same factor.
static void ResolveFdSpeedCap(Player* player, int32_t kind, float* scale) {
    (void)player;
    if (kind != SOH_PLAYER_MOTION_SPEED_CAP || !IsFierceDeity()) {
        return;
    }
    *scale /= FD_MOTION_SCALE;
}

static void SpeakWithFdVoice(Actor* actor, int32_t kind, uint16_t* sfxId, bool* handled) {
    uint16_t action = (uint16_t)(*sfxId - NA_SE_VO_LI_SWORD_N);

    if (kind != SOH_ACTOR_SFX_VOICE || action >= LINK_VOICE_ACTIONS || !IsFierceDeity()) {
        return;
    }
    if (sPlayMmSfx == NULL && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
    }
    *handled = sPlayMmSfx != NULL && sPlayMmSfx(*sfxId, &actor->projectedPos);
}

// Player_Init writes Link's scale back on every scene, so the Deity's is laid over it each frame, and
// Player_SetBootData his boots whenever the equipment or the room changes.
static void UpdateFierceDeity(PlayState* play, Player* player) {
    Actor_SetScale(&player->actor, FD_ACTOR_SCALE);
    WearFdBoots(play, player);
    SlashFromJump(play, player);
    HurryLift(player);
    sIsSlashQueued = false;
}

// ---- drawing the blade ----

// With the blade out, B is HandleSlash's alone: vanilla using the held item would start its own jump slash.
static void KeepBForBlade(bool* should, va_list args) {
    s32 item = va_arg(args, s32);

    if (!*should || item != ITEM_SWORD_BGS || !IsFierceDeity() || gPlayState == NULL) {
        return;
    }
    if (HoldsDeitySword(GET_PLAYER(gPlayState))) {
        *should = false;
    }
}

// OoT swings straight out of the draw only with a one-handed blade; the Deity's B draws and swings on the ground.
static void SwingOutOfDraw(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);

    if (*should || !IsFierceDeity() || !HoldsDeitySword(player) || !IsGrounded(player)) {
        return;
    }
    *should = true;
    sIsSlashQueued = true;
}

// ---- mask ----

static bool CanWearMask(Player* player, PlayState* play) {
    return IsGrounded(player) &&
           !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_CUTSCENE));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(FD_FORM_KEY);
}

static void DrawMaskGetItem(PlayState* play, GetItemEntry* entry) {
    (void)entry;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_26Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGetItemFaceDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGetItemHairDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterMask(void) {
    SOHCustomItemDefinition mask = Z64Items_Define(FD_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, FD_MASK_PAGE, FD_MASK_SLOT, 0);
    mask.getItemEntry.drawFunc = DrawMaskGetItem;
    Z64Items_SetTextbox(&mask, "You got the %rFierce Deity's Mask%w!&It holds the dark power of a god "
                               "of battle.^Wear it with %y\xA1%w to become the %rFierce Deity%w: a towering "
                               "warrior whose great blade sends out beams of light.");
    Z64Items_SetPauseText(&mask, "%rFierce Deity's Mask&%wPress %y\xA1%w to become the Fierce Deity.&%y\xA0%w: "
                                 "two-handed sword  With %y\xA4%w: sword beams (magic)");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_Register(sApi, &mask);
}

// ---- form ----

static LinkAnimationHeader* LoadMaskOnAnim(void) {
    LinkAnimationHeader* source = ResourceMgr_LoadPlayerAnimAsHeader(sMaskOnPath);

    if (source == NULL || source->common.frameCount <= 0) {
        return NULL;
    }
    size_t values = (size_t)source->common.frameCount * MM_ANIM_VALUES_PER_FRAME;
    s16* frames = (s16*)malloc(values * sizeof(s16));
    if (frames == NULL) {
        return NULL;
    }
    memcpy(frames, source->segment, values * sizeof(s16));
    for (s32 frame = 0; frame < source->common.frameCount; frame++) {
        s16* root = &frames[frame * MM_ANIM_VALUES_PER_FRAME];

        root[0] = MM_ANIM_BASE_TRANSL_X;
        root[2] = 0;
    }
    free(sMaskOnFrames);
    sMaskOnFrames = frames;
    sMaskOnAnim.common.frameCount = source->common.frameCount;
    sMaskOnAnim.segment = frames;
    return &sMaskOnAnim;
}

// The host stashes Link's own sword and gives it back on the way out, saved or not.
static uint16_t ResolveFdEquipment(int32_t equipType, uint16_t value) {
    if (equipType == EQUIP_TYPE_SWORD) {
        return EQUIP_VALUE_SWORD_BIGGORON;
    }
    if (equipType == EQUIP_TYPE_SHIELD) {
        return EQUIP_VALUE_SHIELD_NONE;
    }
    return value;
}

static bool IsAllowedCustomItem(const char* customKey) {
    static const char* const sAllowed[] = {
        "nei.deku_form_mask", "nei.goron_form_mask",  "nei.zora_form_mask",   "nei.keaton_form_mask",
        "nei.mask_kafei",     "nei.garo_form_mask",   "nei.gerudo_form_mask", "nei.rocs_feather",
        "nei.rocs_cape",      "nei.progressive_rocs", "nei.rod_fire",         "nei.rod_ice",
        "nei.rod_light",      "nei.ball_and_chain",   "nei.rito_form_mask",
    };

    for (u32 i = 0; i < ARRAY_COUNT(sAllowed); i++) {
        if (strcmp(customKey, sAllowed[i]) == 0) {
            return true;
        }
    }
    return false;
}

// NEI's sSlotAllowedFD: nuts, the Lens, the hammer and bottles, plus the Roc's, the rods and the ball and chain.
static bool AllowsFdItem(uint16_t item, const char* customKey) {
    if (customKey != NULL) {
        return IsAllowedCustomItem(customKey);
    }
    return item == ITEM_NUT || item == ITEM_LENS || item == ITEM_HAMMER || (item >= ITEM_BOTTLE && item <= ITEM_POE);
}

static const SOHFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_MELEE, HandleSlash },
    { SOH_PLAYER_ACTION_SPIN_CHARGE, HandleSpinCharge },
    { SOH_PLAYER_ACTION_ZTARGET_A, HandleZTargetA },
    { SOH_PLAYER_ACTION_LEDGE, HandleLedge },
};

static void RegisterForm(void) {
    SOHFormDefinition deity = { 0 };

    deity.structSize = sizeof(deity);
    deity.key = FD_FORM_KEY;
    deity.label = "Fierce Deity";
    deity.kind = SOH_FORM_KIND_LINK;
    deity.item = FD_MASK_KEY;
    deity.modelPath = FD_MODEL_PATH;
    deity.rootScaleAdult = 1.0f;
    deity.rootScaleChild = FD_ROOT_SCALE_CHILD;
    deity.height = FD_HEIGHT;
    deity.motionScale = FD_MOTION_SCALE;
    deity.resolveEquipment = ResolveFdEquipment;
    deity.allowsButtonItem = AllowsFdItem;
    deity.transformAnim = LoadMaskOnAnim();
    deity.transformMask = sMaskDL;
    deity.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    deity.actions = sActions;
    deity.actionCount = ARRAY_COUNT(sActions);
    deity.onEnter = EnterFierceDeity;
    deity.onExit = ExitFierceDeity;
    deity.update = UpdateFierceDeity;
    sApi->RegisterForm(&deity);
}

static void RegisterBeam(void) {
    SOHActorDefinition beam = { 0 };

    beam.structSize = sizeof(beam);
    beam.key = FD_BEAM_ACTOR_KEY;
    beam.description = "Fierce Deity sword beam";
    beam.category = ACTORCAT_ITEMACTION;
    beam.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    beam.objectId = OBJECT_GAMEPLAY_KEEP;
    beam.instanceSize = sizeof(SwordBeam);
    beam.init = InitBeam;
    beam.destroy = DestroyBeam;
    beam.update = UpdateBeam;
    beam.draw = DrawBeam;
    sBeamActorId = sApi->RegisterActor(&beam);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    RegisterBeam();
    RegisterMask();
    RegisterForm();
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveFdBody);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawBlade);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveFaceTextures, ResolveFdFace);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveAgeProperties, ResolveFdAgeProperties);
    SOH_REGISTER_HOOK(sApi, OnInterfaceResolveButtonIcon, ShowSwordOnB);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_OBJ_OSHIHIKI, HurryPushedBlock);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, SpeakWithFdVoice);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, ResolveFdSpeedCap);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveFdStrength);
    sApi->RegisterVB(VB_CHANGE_HELD_ITEM_AND_USE_ITEM, KeepBForBlade);
    sApi->RegisterVB(VB_USE_HELD_ITEM_AFTER_CHANGE, SwingOutOfDraw);
}
