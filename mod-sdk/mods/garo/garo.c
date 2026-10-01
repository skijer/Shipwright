#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "overlays/actors/ovl_Bg_Ice_Shelter/z_bg_ice_shelter.h"

#define GARO_MASK_KEY "nei.garo_form_mask"
#define GARO_FORM_KEY "nei.garo"
// Cell 20 of the mask page is where Majora's Mask keeps Garo's Mask.
#define GARO_MASK_PAGE 1
#define GARO_MASK_SLOT 20

#define BG_ON_GROUND 1
#define BG_TOUCHING_WALL 8
#define STICK_FULL 80.0f
#define OCARINA_NOTE_NONE 0xFF
#define MM_OCARINA_NOTE_SERVICE "mm.ocarina.note"
#define MM_OCARINA_INSTRUMENT_IKANA_KING 5

#define SPIN_DAMAGE 2
#define SPIN_FRAMES 14
#define SPIN_TURNS 2
#define SPIN_YAW_RATE ((0x10000 * SPIN_TURNS) / SPIN_FRAMES)
#define SPIN_PLAY_SPEED 1.5f
#define SPIN_MOVE_SPEED (9.0f * 1.5f)
// The spin starts on the press and turns into the rod charge if B is still down this many frames later.
#define B_HOLD_FRAMES 9

#define DASH_SPEED 14.0f
#define DASH_DAMAGE 4
#define DASH_TURN 0x400

#define PARRY_DAMAGE 4
#define PARRY_STRIKE_FRAME 6
#define PARRY_FREEZE_FRAMES 60
// A frozen actor never re-registers its AC collider, so both counters thaw their target before landing.
#define PARRY_THAW_LEAD 2
#define BANISH_THAW_LEAD 3
#define RIPOSTE_OFFSET 45.0f
#define GUARD_MELEE_RANGE 160.0f
#define GUARD_HOLD_FRACTION 0.5f
#define GUARD_RETURN_SPEED 1.5f
// Shots are caught in the air: most projectiles delete themselves on impact.
#define GUARD_CATCH_RADIUS 130.0f
#define GUARD_CATCH_HEIGHT 40.0f
#define GUARD_CATCH_MIN_SPEED 1.0f

#define REFLECT_FRAMES 60
#define REFLECT_DAMAGE 4
#define REFLECT_HALF 10.0f
#define REFLECT_MIN_SPEED 8.0f
#define REFLECT_INVULNERABILITY 12
#define REFLECT_PUSH_OUT 35.0f

#define BANISH_COOLDOWN 300
#define BANISH_OFFSET 50.0f
#define BANISH_VANISH_FRAMES 8
#define BANISH_STUN_FRAMES 60
#define BANISH_SHADOW_FRAMES 9
#define BANISH_STUN_RADIUS 80.0f
#define BANISH_NO_TARGET_REACH 60.0f
#define SHADOW_STRIKE_DAMAGE 8
#define SHADOW_STRIKE_FRAME 12
#define SHADOW_STRIKE_WINDOW 4
#define SHADOW_STRIKE_PLAY_SPEED 2.5f

#define LAND_STRIKE_DAMAGE 8
#define LAND_STRIKE_FRAME 12
#define LAND_STRIKE_TAIL 3
#define HOP_MIN_AIR_FRAMES 3
#define JUMP_ATTACK_SLASH_FRAME 6
#define JUMP_GRAVITY (-1.2f)

#define ROD_RELEASE_COOLDOWN 10
#define ROD_CHARGE_MAX 120
#define ROD_CHARGE_TIER2 35
#define ROD_CHARGE_TIER3 85
#define ROD_LEVELS 3
#define ROD_MAGIC_COST 4
#define ROD_L2_DAMAGE 4
#define ROD_L3_DAMAGE 3
#define ROD_ORB_SPEED 12.0f
#define ROD_ORB_LIFETIME 60
#define ROD_BURST_LIFETIME (ROD_ORB_LIFETIME / 2)
#define ROD_BALL_CIRCLE_MAX 0.08f
#define ROD_BALL_SCALE_MAX 7.0f
#define ROD_BALL_AHEAD 55.0f
#define RETICLE_DIST 320.0f
#define RETICLE_SPREAD 26.0f
#define RETICLE_DOT_SCALE 0.55f

#define ORB_POOL_MAX 12
#define ORB_SEEKERS 4
#define ORB_SEEKER_DAMAGE 2
#define ORB_SEEKER_LIFETIME 70
#define ORB_SEEKER_SPEED 16.0f
#define ORB_SEEKER_TURN 0x1200
#define ORB_SEEKER_FAN 0x2000
#define ORB_BURST_BALLS 8
#define ORB_HOME_RANGE 700.0f
#define ORB_HOME_CONE 0.2f
#define ORB_TURN_RATE 0x0700
#define ORB_WAKE_SCALE 85
#define ORB_TRAIL_LEN 15
#define ORB_TRAIL_DRAWN 12
#define ORB_STREAK_SCALE 0.008f
#define ORB_MELT_RADIUS 60.0f
#define BIG_MAGIC_RAYS_MAX 6

#define ELEMENT_COUNT 6
#define ELEMENT_FIRE 0
#define ELEMENT_ICE 1
#define ELEMENT_LIGHT 2

#define LAUGH_CHANCE 0.20f
#define ENEMY_SNAPSHOT_MAX 64

#define DEATH_FLAMES 9
#define DEATH_FLAME_RADIUS 20.0f

// NEI's Garo stands on the Garo enemy's own 19-bone skeleton: Link's pose drives the body and legs, Garo's idle
// sways the robe and the blades. The enemy is drawn at 0.035, so the subtree takes 3.5 over Link's 0.01.
#define HYBRID_LIMBS 19
#define HYBRID_JOINTS (HYBRID_LIMBS + 1)
#define HYBRID_SUBTREE_SCALE 3.5f
#define HYBRID_JOINT_ROOT 1
#define HYBRID_JOINT_L_SWORD 4
#define HYBRID_JOINT_R_SWORD 6
#define HYBRID_JOINT_ROBE_LEFT 9
#define HYBRID_JOINT_ROBE_RIGHT 10
#define HYBRID_JOINT_R_THIGH 14
#define HYBRID_JOINT_R_FOOT 16
#define HYBRID_JOINT_L_THIGH 17
#define HYBRID_JOINT_L_FOOT 19
#define LEG_THICKNESS 1.3f
#define LINK_ROOT_HEIGHT_ADULT 3377.0f
#define LINK_ROOT_HEIGHT_CHILD 2376.0f
#define TRAIL_BLADE_LENGTH 900.0f
#define TRAIL_ELEM_DURATION 8

#define MM_ANIM_VALUES_PER_FRAME 67
#define MM_ANIM_BASE_TRANSL_X (-57)
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);
typedef bool (*MmOcarinaNoteFunc)(uint8_t instrumentId, uint8_t pitch, f32* bendFreq);

#define GARO_ANIM_PATH(name) "__OTR__objects/forms/garo/gPlayerAnim_garo_" name
static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__icon_item_static_yar/gItemIconGaroMaskTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__item_name_static/gItemNameGarosMaskENGTex";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/object_gi_mask09/gGiGarosMaskFaceDL";
static const ALIGN_ASSET(2) char sMaskDL[] = "__OTR__objects/object_mask_json/object_mask_json_DL_0004C0";
static const ALIGN_ASSET(2) char sMaskOnPath[] = "__OTR__misc/link_animetion/gPlayerAnim_cl_setmask_Data";
static const ALIGN_ASSET(2) char sHybridSkelPath[] = "__OTR__objects/object_jso/gGaroSkel";
static const ALIGN_ASSET(2) char sHybridIdlePath[] = "__OTR__objects/object_jso/gGaroIdleAnim";

#define GANON2 "__OTR__overlays/ovl_Boss_Ganon2/"
#define GANON "__OTR__overlays/ovl_Boss_Ganon/"
static const ALIGN_ASSET(2) char sOrbMaterialDL[] = GANON2 "gGanonLightOrbMaterialDL";
static const ALIGN_ASSET(2) char sOrbModelDL[] = GANON2 "gGanonLightOrbModelDL";
static const ALIGN_ASSET(2) char sBigMagicMaterialDL[] = GANON "gGanondorfLightBallMaterialDL";
static const ALIGN_ASSET(2) char sBigMagicBallDL[] = GANON "gGanondorfSquareDL";
static const ALIGN_ASSET(2) char sBigMagicFlecksDL[] = GANON "gGanondorfLightFlecksDL";
static const ALIGN_ASSET(2) char sBigMagicCircleDL[] = GANON "gGanondorfBigMagicBGCircleDL";
static const ALIGN_ASSET(2) char sBigMagicDotDL[] = GANON "gGanondorfDotDL";
static const ALIGN_ASSET(2) char sBigMagicRayDL[] = GANON "gGanondorfLightRayTriDL";
static const char* const sStreakDL[ORB_TRAIL_DRAWN] = {
    GANON "gGanondorfLightStreak12DL", GANON "gGanondorfLightStreak11DL", GANON "gGanondorfLightStreak10DL",
    GANON "gGanondorfLightStreak9DL",  GANON "gGanondorfLightStreak8DL",  GANON "gGanondorfLightStreak7DL",
    GANON "gGanondorfLightStreak6DL",  GANON "gGanondorfLightStreak5DL",  GANON "gGanondorfLightStreak4DL",
    GANON "gGanondorfLightStreak3DL",  GANON "gGanondorfLightStreak2DL",  GANON "gGanondorfLightStreak1DL",
};

typedef enum {
    ANIM_GUARD,
    ANIM_DASH,
    ANIM_SPIN,
    ANIM_COLLAPSE,
    ANIM_APPEAR,
    ANIM_TAKE_OUT_BOMB,
    ANIM_LAUGH,
    ANIM_APPEAR_DRAW_SWORDS,
    ANIM_BOUNCE,
    ANIM_JUMP_BACK,
    ANIM_SLASH_LOOP,
    ANIM_DRAW_SWORDS,
    ANIM_COUNT,
} GaroAnim;

static const char* const sAnimPaths[ANIM_COUNT] = {
    GARO_ANIM_PATH("guard"),    GARO_ANIM_PATH("dashAttack"),       GARO_ANIM_PATH("spinAttack"),
    GARO_ANIM_PATH("collapse"), GARO_ANIM_PATH("appear"),           GARO_ANIM_PATH("takeOutBomb"),
    GARO_ANIM_PATH("laugh"),    GARO_ANIM_PATH("appearDrawSwords"), GARO_ANIM_PATH("bounce"),
    GARO_ANIM_PATH("jumpBack"), GARO_ANIM_PATH("slashLoop"),        GARO_ANIM_PATH("drawSwords"),
};

typedef enum {
    GARO_MOVE_NONE,
    GARO_MOVE_SPIN,
    GARO_MOVE_ROD_AIM,
    GARO_MOVE_GUARD,
    GARO_MOVE_GUARD_RETURN,
    GARO_MOVE_RIPOSTE,
    GARO_MOVE_DASH,
    GARO_MOVE_BANISH_VANISH,
    GARO_MOVE_BANISH_SHADOW,
    GARO_MOVE_SHADOW_STRIKE,
    GARO_MOVE_LAUGH,
    GARO_MOVE_HOP,
    GARO_MOVE_BACKFLIP,
    GARO_MOVE_JUMP_ATTACK,
    GARO_MOVE_AIR_SLASH,
    GARO_MOVE_LAND_STRIKE,
} GaroMove;

typedef struct {
    bool isActive;
    Vec3f pos;
    s16 yaw;
    s16 pitch;
    s16 timer;
    u8 element;
    u8 damage;
    u32 dmgFlags;
    bool bursts;
    bool isSeeker;
    Actor* target;
    f32 ballCircle;
    f32 ballScale;
    Vec3f trailPos[ORB_TRAIL_LEN];
    Vec3f trailRot[ORB_TRAIL_LEN];
    s16 trailIndex;
} RodOrb;

// Link limb → hybrid joint. The shoulders also drive the robe's side panels, done apart in PoseHybrid.
static const s8 sLinkToHybridJoint[PLAYER_LIMB_MAX] = {
    -1, 1, 13, -1, 14, 15, 16, 17, 18, 19, 2, 12, -1, -1, 3, -1, 4, 5, -1, 6, -1, -1,
};

// Sword and robe bones keep Garo's own sway instead of Link's pose.
static bool IsGaroOwnedJoint(s8 joint) {
    return joint == HYBRID_JOINT_L_SWORD || joint == HYBRID_JOINT_R_SWORD || joint == 7 || joint == 8 || joint == 11;
}

// NEI's per-action map onto Igos du Ikana's bank (En_Osk, NA_SE_EN_BOSU_*): MM gives Garo no player voice, so
// every Link grunt is paired by tone with one of his.
static const u16 sGaroVoiceByAction[0x20] = {
    0x3A30, 0x3A4C, 0x3A4A, 0x3A2B, 0x3A2A, 0x3A3A, 0x3A2E, 0x3A90, 0x3A3A, 0x3A9C, 0x3A90,
    0x3A5B, 0x3A2E, 0x3A2B, 0x3A2F, 0x3A90, 0x3A90, 0x3A29, 0x3A4D, 0x3A31, 0x3A2A, 0x3A32,
    0x3A2E, 0x3A47, 0x3A2B, 0x3A2B, 0x3A3A, 0x3A90, 0x3A33, 0x3A45, 0x3A3D, 0x3A2E,
};

// The six SW97 arrow colours, in SW97's order; only fire, ice and light carry a vanilla arrow flag.
static const u8 sOrbPrim[ELEMENT_COUNT][3] = {
    { 255, 200, 0 }, { 170, 255, 255 }, { 255, 255, 255 }, { 0, 0, 0 }, { 255, 255, 170 }, { 170, 255, 255 },
};
static const u8 sOrbEnv[ELEMENT_COUNT][3] = {
    { 255, 0, 0 }, { 0, 0, 255 }, { 170, 170, 170 }, { 0, 0, 0 }, { 255, 255, 0 }, { 0, 255, 0 },
};
static const u8 sOrbCore[ELEMENT_COUNT][3] = {
    { 150, 25, 0 }, { 0, 70, 150 }, { 200, 200, 200 }, { 10, 0, 20 }, { 165, 150, 0 }, { 0, 120, 40 },
};
// FHGFLASH_LIGHTBALL_* colours, numeric because that enum is private to the effect overlay.
static const u8 sOrbWakeColor[ELEMENT_COUNT] = { 2, 1, 7, 5, 3, 0 };
static const f32 sRodLevelScale[ROD_LEVELS] = { 0.35f, 0.7f, 1.0f };
static const s16 sRodLevelRays[ROD_LEVELS] = { 0, BIG_MAGIC_RAYS_MAX / 2, BIG_MAGIC_RAYS_MAX };

static const char* const sRequiredHooks[] = {
    "OnPlayerActionHandler", "OnPlayerResolveLimbDraw",  "OnPlayerFilterInput",
    "OnActorPlaySfx",        "OnBgCheckRaycastFloor",    "OnSceneInit",
    "OnActorDraw",           "OnCollisionResolveDamage", "OnOcarinaNote",
    "OnOcarinaPlaybackNote",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static ColliderQuadInit sEscortQuadInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_QUAD },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, 0x00, 0x10 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
s32 func_80837B18(PlayState* play, Player* player, s32 damage);
int Player_IsZTargeting(Player* player);
void func_808911BC(BgIceShelter* ice);

static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static LinkAnimationHeader* sAnims[ANIM_COUNT];
static LinkAnimationHeader sMaskOnAnim;
static s16* sMaskOnFrames;

static u8 sMove;
static s16 sMoveTimer;
static s16 sBHeldFrames;
static s16 sSpinEntryYaw;
static s16 sHopAirFrames;
static s16 sHopYaw;
static bool sHasStruck;
static s16 sStrikeTailFrames;
static s16 sBanishCooldown;
static s16 sRodCooldown;
static Actor* sBanishTarget;
static Actor* sParryAttacker;
static Actor* sPendingParry;
static bool sIsParryPending;
static Vec3f sShadowStart;
static Vec3f sShadowEnd;
static Vec3f sShadowPos;
static s16 sShadowTimer;

static s16 sRodCharge;
static u8 sRodElement;
static bool sIsRodChimed;
static f32 sRodBallCircle;
static f32 sRodBallScale;
static s16 sRodBallRays;

static RodOrb sOrbs[ORB_POOL_MAX];
static ColliderQuad sOrbQuads[ORB_POOL_MAX];
static bool sAreOrbQuadsReady;
static Actor* sReflectedShot;
static s16 sReflectTimer;
static ColliderQuad sReflectQuad;
static bool sIsReflectQuadReady;

static s32 sTrailLeft = -1;
static s32 sTrailRight = -1;
static s8 sTrailAxisLeft = -1;
static s8 sTrailAxisRight = -1;

static SkelAnime sHybrid;
static Vec3s sHybridJoints[HYBRID_JOINTS];
static Vec3s sHybridMorph[HYBRID_JOINTS];
static bool sIsHybridReady;

static Actor* sEnemySnapshot[ENEMY_SNAPSHOT_MAX];
static s32 sEnemySnapshotCount;
static bool sIsLaughPending;
static bool sHaveDeathFlamesSpawned;
#define REVEALED_MAX 64
static Actor* sRevealed[REVEALED_MAX];
static s32 sRevealedCount;
static bool sIsOcarinaOut;
// Set on the game thread while the ocarina is out; the note hook runs on the audio thread and only reads it.
static MmOcarinaNoteFunc sChantVoice;
static f32 sChantBend = 1.0f;
static u8 sLastChantNote = OCARINA_NOTE_NONE;
static u8 sScaledDamage;

static void GaroAction(Player* player, PlayState* play);

static bool IsGaro(void) {
    return sApi != NULL && sApi->IsFormActive(GARO_FORM_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BG_ON_GROUND) != 0;
}

static bool IsGaroBusy(Player* player) {
    return player->actionFunc == GaroAction;
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 &
             (PLAYER_STATE1_DEAD | PLAYER_STATE1_LOADING | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM |
              PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER |
              PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_ITEM_CS |
              PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED | PLAYER_STATE1_CLIMBING_LADDER |
              PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE));
}

static bool IsPressed(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].press.button, button);
}

static bool IsHeld(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].cur.button, button);
}

static f32 GetStickMagnitude(PlayState* play) {
    f32 x = play->state.input[0].cur.stick_x;
    f32 y = play->state.input[0].cur.stick_y;

    return MIN(sqrtf((x * x) + (y * y)) / STICK_FULL, 1.0f);
}

static s16 GetStickYaw(PlayState* play) {
    f32 x = play->state.input[0].cur.stick_x;
    f32 y = play->state.input[0].cur.stick_y;

    return Math_Atan2S(y, -x) + Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
}

// Freezing the actor also stops its AC from registering, which is why both counters thaw it early.
static void MarkStunned(PlayState* play, Actor* target, s16 frames) {
    Vec3f accel = { 0.0f, -0.05f, 0.0f };
    Color_RGBA8 prim = { 140, 80, 220, 255 };
    Color_RGBA8 env = { 40, 10, 100, 0 };

    target->freezeTimer = frames;
    Actor_SetColorFilter(target, 0x0000, 0xF8, 0x0000, frames);
    for (s32 i = 0; i < 8; i++) {
        f32 angle = (f32)i * ((f32)M_PI * 2.0f / 8.0f);
        Vec3f pos = { target->world.pos.x + cosf(angle) * BANISH_STUN_RADIUS, target->world.pos.y + 30.0f,
                      target->world.pos.z + sinf(angle) * BANISH_STUN_RADIUS };
        Vec3f velocity = { cosf(angle) * 0.5f, 1.5f, sinf(angle) * 0.5f };
        EffectSsKiraKira_SpawnSmall(play, &pos, &velocity, &accel, &prim, &env);
    }
    Audio_PlayActorSound2(target, NA_SE_IT_SHIELD_REFLECT_SW);
}

// A counter holds its target by pointer, and the target may have died since.
static bool IsActorAlive(PlayState* play, Actor* actor) {
    if (actor == NULL) {
        return false;
    }
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* it = play->actorCtx.actorLists[category].head; it != NULL; it = it->next) {
            if (it == actor) {
                return true;
            }
        }
    }
    return false;
}

// ---- sound ----

// Looked up on use rather than in ModInit: mm_assets may load after this mod.
static MmSfxPlayFunc GetMmSfxPlayer(void) {
    if (sPlayMmSfx == NULL && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
    }
    return sPlayMmSfx;
}

static void SpeakWithIgosVoice(Actor* actor, int32_t kind, uint16_t* sfxId, bool* handled) {
    u16 action = (u16)(*sfxId - NA_SE_VO_LI_SWORD_N);
    MmSfxPlayFunc play = GetMmSfxPlayer();

    if (!IsGaro() || kind != SOH_ACTOR_SFX_VOICE) {
        return;
    }
    *handled = true;
    if (action < ARRAY_COUNT(sGaroVoiceByAction) && play != NULL) {
        play(sGaroVoiceByAction[action], &actor->projectedPos);
    }
}

static u16 GetChargeSfx(u8 element) {
    if (element == ELEMENT_FIRE) {
        return NA_SE_PL_ARROW_CHARGE_FIRE;
    }
    if (element == ELEMENT_ICE) {
        return NA_SE_PL_ARROW_CHARGE_ICE;
    }
    if (element == ELEMENT_LIGHT) {
        return NA_SE_PL_ARROW_CHARGE_LIGHT;
    }
    return NA_SE_IT_SWORD_CHARGE;
}

// ---- animation ----

static void LoadAnims(void) {
    for (s32 slot = 0; slot < ANIM_COUNT; slot++) {
        sAnims[slot] = ResourceMgr_LoadPlayerAnimAsHeader(sAnimPaths[slot]);
    }
}

// endFrame below zero plays to the last frame; a negative speed runs the clip backwards.
static void PlayGaroAnim(PlayState* play, Player* player, s32 slot, f32 startFrame, f32 endFrame, f32 speed) {
    LinkAnimationHeader* anim = sAnims[slot];

    if (anim == NULL) {
        return;
    }
    if (endFrame < 0.0f) {
        endFrame = Animation_GetLastFrame(anim);
    }
    LinkAnimation_Change(play, &player->skelAnime, anim, speed, startFrame, endFrame, ANIMMODE_ONCE, -2.0f);
}

static bool AdvanceGaroAnim(PlayState* play, Player* player) {
    return LinkAnimation_Update(play, &player->skelAnime);
}

static f32 GetAnimLastFrame(s32 slot) {
    return sAnims[slot] != NULL ? Animation_GetLastFrame(sAnims[slot]) : 0.0f;
}

// ---- strikes ----

// The Master Sword's flag is what every sword-vulnerable bumper accepts; the number itself is fixed past the
// enemy's damage table in FixStrikeDamage.
static void Strike(PlayState* play, Player* player, Vec3f* corners, u8 damage) {
    ColliderQuad* quad = &player->meleeWeaponQuads[0];

    Collider_ResetQuadAT(play, &quad->base);
    Collider_SetQuadVertices(quad, &corners[0], &corners[1], &corners[2], &corners[3]);
    quad->base.atFlags = AT_ON | AT_TYPE_PLAYER;
    quad->info.toucher.dmgFlags = DMG_SLASH_MASTER;
    quad->info.toucher.damage = damage;
    quad->info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;
    CollisionCheck_SetAT(play, &play->colChkCtx, &quad->base);
}

static void StopStriking(Player* player) {
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
}

// A slab in front of Garo, slanting from its near bottom edge to its far top one.
static void StrikeAhead(PlayState* play, Player* player, f32 halfWidth, f32 top, u8 damage) {
    f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
    f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
    Vec3f* pos = &player->actor.world.pos;
    f32 farX = pos->x + sinYaw * 60.0f;
    f32 farZ = pos->z + cosYaw * 60.0f;
    f32 nearX = pos->x + sinYaw * 10.0f;
    f32 nearZ = pos->z + cosYaw * 10.0f;
    Vec3f corners[4] = {
        { farX - cosYaw * halfWidth, pos->y + top, farZ + sinYaw * halfWidth },
        { farX + cosYaw * halfWidth, pos->y + top, farZ - sinYaw * halfWidth },
        { nearX + cosYaw * halfWidth, pos->y, nearZ - sinYaw * halfWidth },
        { nearX - cosYaw * halfWidth, pos->y, nearZ + sinYaw * halfWidth },
    };

    Strike(play, player, corners, damage);
}

// A full-height wall across his facing that marches outward: the plain slab is one thin line at any given
// distance and missed the enemy he had just appeared behind.
static void StrikeWallAhead(PlayState* play, Player* player, s16 liveFrame, u8 damage) {
    f32 distance = 15.0f + (f32)MAX(liveFrame, 0) * 12.0f;
    f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
    f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
    Vec3f* pos = &player->actor.world.pos;
    f32 centerX = pos->x + sinYaw * distance;
    f32 centerZ = pos->z + cosYaw * distance;
    Vec3f corners[4] = {
        { centerX - cosYaw * 45.0f, pos->y + 75.0f, centerZ + sinYaw * 45.0f },
        { centerX + cosYaw * 45.0f, pos->y + 75.0f, centerZ - sinYaw * 45.0f },
        { centerX + cosYaw * 45.0f, pos->y - 10.0f, centerZ - sinYaw * 45.0f },
        { centerX - cosYaw * 45.0f, pos->y - 10.0f, centerZ + sinYaw * 45.0f },
    };

    Strike(play, player, corners, damage);
}

// A quad is flat, so the landing blow turns it 30 degrees a live frame: half a turn covers every direction.
static void StrikeAround(PlayState* play, Player* player, s16 liveFrame, u8 damage) {
    s16 yaw = player->actor.shape.rot.y + (s16)(MAX(liveFrame, 0) * 0x1555);
    f32 dirX = Math_SinS(yaw) * 130.0f;
    f32 dirZ = Math_CosS(yaw) * 130.0f;
    Vec3f* pos = &player->actor.world.pos;
    Vec3f corners[4] = {
        { pos->x + dirX, pos->y + 130.0f, pos->z + dirZ },
        { pos->x - dirX, pos->y + 130.0f, pos->z - dirZ },
        { pos->x - dirX, pos->y - 20.0f, pos->z - dirZ },
        { pos->x + dirX, pos->y - 20.0f, pos->z + dirZ },
    };

    Strike(play, player, corners, damage);
}

// ---- sword trails ----

static void KillTrails(PlayState* play) {
    if (sTrailLeft >= 0) {
        Effect_Delete(play, sTrailLeft);
    }
    if (sTrailRight >= 0) {
        Effect_Delete(play, sTrailRight);
    }
    sTrailLeft = -1;
    sTrailRight = -1;
    sTrailAxisLeft = -1;
    sTrailAxisRight = -1;
}

static void SpawnTrails(PlayState* play) {
    EffectBlureInit1 blure;

    if (sTrailLeft >= 0) {
        return;
    }
    memset(&blure, 0, sizeof(blure));
    blure.p1StartColor[0] = 120;
    blure.p1StartColor[1] = 60;
    blure.p1StartColor[2] = 200;
    blure.p1StartColor[3] = 200;
    blure.p2StartColor[0] = 60;
    blure.p2StartColor[1] = 30;
    blure.p2StartColor[2] = 140;
    blure.p2StartColor[3] = 100;
    blure.p1EndColor[0] = 60;
    blure.p1EndColor[1] = 30;
    blure.p1EndColor[2] = 140;
    blure.p2EndColor[0] = 60;
    blure.p2EndColor[1] = 30;
    blure.p2EndColor[2] = 140;
    blure.elemDuration = TRAIL_ELEM_DURATION;
    Effect_Add(play, &sTrailLeft, EFFECT_BLURE1, 0, 0, &blure);
    Effect_Add(play, &sTrailRight, EFFECT_BLURE1, 0, 0, &blure);
}

static bool WantsTrails(void) {
    return sMove == GARO_MOVE_SPIN || sMove == GARO_MOVE_AIR_SLASH || sMove == GARO_MOVE_LAND_STRIKE ||
           sMove == GARO_MOVE_SHADOW_STRIKE || sMove == GARO_MOVE_RIPOSTE;
}

// The right blade's bone is mirrored, so each trail picks, once, the local axis that points down and away from
// Garo in the live pose.
static s8 PickTrailAxis(Player* player, Vec3f* base) {
    static const Vec3f sAxes[6] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    f32 outX = base->x - player->actor.world.pos.x;
    f32 outZ = base->z - player->actor.world.pos.z;
    f32 outLength = sqrtf(SQ(outX) + SQ(outZ));
    Vec3f want = { 0.0f, -1.0f, 0.0f };
    s8 best = 0;
    f32 bestScore = -1.0e9f;

    if (outLength > 0.001f) {
        want.x = outX / outLength * 0.6f;
        want.z = outZ / outLength * 0.6f;
    }
    for (s8 i = 0; i < 6; i++) {
        Vec3f local = { sAxes[i].x * TRAIL_BLADE_LENGTH, sAxes[i].y * TRAIL_BLADE_LENGTH,
                        sAxes[i].z * TRAIL_BLADE_LENGTH };
        Vec3f tip;
        Matrix_MultVec3f(&local, &tip);
        f32 dx = tip.x - base->x;
        f32 dy = tip.y - base->y;
        f32 dz = tip.z - base->z;
        f32 length = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));
        if (length < 0.001f) {
            continue;
        }
        f32 score = (dx * want.x + dy * want.y + dz * want.z) / length;
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

static void FeedTrail(Player* player, s32 effectIndex, s8* axis) {
    static const Vec3f sAxes[6] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    EffectBlure* trail = (EffectBlure*)Effect_GetByIndex(effectIndex);
    Vec3f origin = { 0.0f, 0.0f, 0.0f };
    Vec3f base;
    Vec3f tip;

    if (trail == NULL) {
        return;
    }
    Matrix_MultVec3f(&origin, &base);
    if (*axis < 0) {
        *axis = PickTrailAxis(player, &base);
    }
    Vec3f local = { sAxes[*axis].x * TRAIL_BLADE_LENGTH, sAxes[*axis].y * TRAIL_BLADE_LENGTH,
                    sAxes[*axis].z * TRAIL_BLADE_LENGTH };
    Matrix_MultVec3f(&local, &tip);
    EffectBlure_AddVertex(trail, &tip, &base);
}

// ---- rod orbs ----

static u32 GetOrbDmgFlags(u8 element) {
    if (element == ELEMENT_FIRE) {
        return DMG_ARROW_FIRE;
    }
    if (element == ELEMENT_ICE) {
        return DMG_ARROW_ICE;
    }
    if (element == ELEMENT_LIGHT) {
        return DMG_ARROW_LIGHT;
    }
    return DMG_ARROW_NORMAL;
}

static u8 GetRodLevel(s16 charge) {
    if (charge < ROD_CHARGE_TIER2) {
        return 1;
    }
    return charge < ROD_CHARGE_TIER3 ? 2 : 3;
}

static void AddOrb(const RodOrb* orb) {
    for (s32 i = 0; i < ORB_POOL_MAX; i++) {
        if (!sOrbs[i].isActive) {
            sOrbs[i] = *orb;
            sOrbs[i].isActive = true;
            return;
        }
    }
}

// Level 1 fires nothing; level 3 flies half as far and breaks into seekers, and costs double for them.
static void FireRodOrb(Player* player) {
    u8 level = GetRodLevel(sRodCharge);
    bool bursts = level >= 3;
    s16 cost = (s16)(ROD_MAGIC_COST * (bursts ? 2 : 1));
    RodOrb orb;

    sRodCharge = 0;
    sIsRodChimed = false;
    if (level < 2) {
        Audio_PlayActorSound2(&player->actor, NA_SE_IT_BOW_FLICK);
        return;
    }
    memset(&orb, 0, sizeof(orb));
    orb.pos = player->leftHandPos;
    if (orb.pos.y == 0.0f) {
        orb.pos = player->actor.world.pos;
        orb.pos.y += 30.0f;
    }
    Z64Aiming_GetDirection(player, &orb.yaw, &orb.pitch);
    orb.element = sRodElement;
    orb.damage = bursts ? ROD_L3_DAMAGE : ROD_L2_DAMAGE;
    orb.dmgFlags = GetOrbDmgFlags(sRodElement);
    orb.bursts = bursts;
    orb.timer = bursts ? ROD_BURST_LIFETIME : ROD_ORB_LIFETIME;
    orb.ballCircle = MAX(sRodBallCircle, ROD_BALL_CIRCLE_MAX * 0.35f);
    orb.ballScale = MAX(sRodBallScale, ROD_BALL_SCALE_MAX * 0.35f);
    if (gSaveContext.magic >= cost) {
        gSaveContext.magic -= cost;
    } else {
        orb.damage = 1;
    }
    AddOrb(&orb);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_ARROW_SHOT);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_MAGIC_ARROW_SHOT);
}

static Actor* FindNearestUnclaimed(PlayState* play, Vec3f* origin, Actor** claimed, s32 claimedCount) {
    Actor* best = NULL;
    f32 bestDistSq = SQ(ORB_HOME_RANGE);

    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; enemy != NULL; enemy = enemy->next) {
        bool isClaimed = false;
        for (s32 i = 0; i < claimedCount; i++) {
            isClaimed = isClaimed || claimed[i] == enemy;
        }
        f32 distSq = Math3D_Vec3fDistSq(&enemy->world.pos, origin);
        if (!isClaimed && enemy->update != NULL && distSq < bestDistSq) {
            bestDistSq = distSq;
            best = enemy;
        }
    }
    return best;
}

// Ganondorf's impact, then fragments fanned out that each claim a different enemy; with fewer enemies than
// fragments the claims start over, so the spares double up instead of flying at nothing.
static void BurstOrb(PlayState* play, RodOrb* source) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Actor* claimed[ORB_SEEKERS];
    s32 claimedCount = 0;
    u8 element = source->element % ELEMENT_COUNT;

    EffectSsFhgFlash_SpawnShock(play, NULL, &source->pos, 200, 0);
    for (s32 i = 0; i < ORB_BURST_BALLS; i++) {
        Vec3f velocity = { Rand_CenteredFloat(12.0f), Rand_ZeroFloat(8.0f) + 2.0f, Rand_CenteredFloat(12.0f) };
        EffectSsFhgFlash_SpawnLightBall(play, &source->pos, &velocity, &zero, (s16)(Rand_ZeroOne() * 60.0f) + 110,
                                        sOrbWakeColor[element]);
    }
    Audio_PlaySoundGeneral(NA_SE_IT_MAGIC_ARROW_SHOT, &source->pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    for (s32 i = 0; i < ORB_SEEKERS; i++) {
        Actor* target = FindNearestUnclaimed(play, &source->pos, claimed, claimedCount);
        if (target == NULL && claimedCount > 0) {
            claimedCount = 0;
            target = FindNearestUnclaimed(play, &source->pos, claimed, claimedCount);
        }
        RodOrb fragment;
        memset(&fragment, 0, sizeof(fragment));
        fragment.pos = source->pos;
        fragment.yaw = (s16)(source->yaw + (s16)(i * ORB_SEEKER_FAN) - ORB_SEEKER_FAN);
        fragment.pitch = (s16)(source->pitch - 0x0800);
        fragment.element = source->element;
        fragment.damage = ORB_SEEKER_DAMAGE;
        fragment.dmgFlags = source->dmgFlags;
        fragment.timer = ORB_SEEKER_LIFETIME;
        fragment.isSeeker = true;
        fragment.target = target;
        f32 cosPitch = Math_CosS(fragment.pitch);
        for (s32 t = 0; t < ORB_TRAIL_LEN; t++) {
            fragment.trailPos[t] = source->pos;
            fragment.trailRot[t].x = atan2f(-Math_SinS(fragment.pitch), cosPitch);
            fragment.trailRot[t].y = atan2f(Math_SinS(fragment.yaw) * cosPitch, Math_CosS(fragment.yaw) * cosPitch);
        }
        AddOrb(&fragment);
        if (target != NULL) {
            claimed[claimedCount++] = target;
        }
    }
}

// A fragment wheels onto the enemy it claimed; a plain shot only bends toward what is ahead of it, so one with
// nothing in front flies straight.
static void HomeOrb(PlayState* play, RodOrb* orb) {
    f32 forwardX = Math_SinS(orb->yaw) * Math_CosS(orb->pitch);
    f32 forwardY = -Math_SinS(orb->pitch);
    f32 forwardZ = Math_CosS(orb->yaw) * Math_CosS(orb->pitch);
    Actor* best = NULL;
    f32 bestDistSq = SQ(ORB_HOME_RANGE);

    if (orb->isSeeker && orb->target != NULL && IsActorAlive(play, orb->target)) {
        Vec3f aim = { orb->target->world.pos.x, orb->target->world.pos.y + orb->target->shape.yOffset,
                      orb->target->world.pos.z };
        Math_ScaledStepToS(&orb->yaw, Math_Vec3f_Yaw(&orb->pos, &aim), ORB_SEEKER_TURN);
        Math_ScaledStepToS(&orb->pitch, Math_Vec3f_Pitch(&orb->pos, &aim), ORB_SEEKER_TURN);
        return;
    }
    orb->target = NULL;
    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; enemy != NULL; enemy = enemy->next) {
        f32 dx = enemy->world.pos.x - orb->pos.x;
        f32 dy = (enemy->world.pos.y + enemy->shape.yOffset) - orb->pos.y;
        f32 dz = enemy->world.pos.z - orb->pos.z;
        f32 distSq = SQ(dx) + SQ(dy) + SQ(dz);
        if (enemy->update == NULL || distSq >= bestDistSq || distSq < 1.0f) {
            continue;
        }
        if (((dx * forwardX + dy * forwardY + dz * forwardZ) / sqrtf(distSq)) < ORB_HOME_CONE) {
            continue;
        }
        bestDistSq = distSq;
        best = enemy;
    }
    if (best == NULL) {
        return;
    }
    Vec3f aim = { best->world.pos.x, best->world.pos.y + best->shape.yOffset, best->world.pos.z };
    s16 turn = orb->isSeeker ? ORB_SEEKER_TURN : ORB_TURN_RATE;
    Math_ScaledStepToS(&orb->yaw, Math_Vec3f_Yaw(&orb->pos, &aim), turn);
    Math_ScaledStepToS(&orb->pitch, Math_Vec3f_Pitch(&orb->pos, &aim), turn);
}

// Red ice answers to who hit it, never to a damage flag, so the ice orb melts it through the block's own call.
static void MeltRedIce(PlayState* play, RodOrb* orb) {
    if ((orb->element % ELEMENT_COUNT) != ELEMENT_ICE) {
        return;
    }
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = actor->next) {
        BgIceShelter* ice = (BgIceShelter*)actor;
        f32 dx = actor->world.pos.x - orb->pos.x;
        f32 dz = actor->world.pos.z - orb->pos.z;
        if (actor->id != ACTOR_BG_ICE_SHELTER || ice->actionFunc == NULL || sqrtf(SQ(dx) + SQ(dz)) >= ORB_MELT_RADIUS) {
            continue;
        }
        func_808911BC(ice);
        Audio_PlayActorSound2(actor, NA_SE_EV_ICE_MELT);
    }
}

// Quads remember their actor and a new scene brings a new player, so they are rebuilt per scene.
static void PrepareEscortQuads(PlayState* play, Player* player) {
    if (sAreOrbQuadsReady) {
        return;
    }
    for (s32 i = 0; i < ORB_POOL_MAX; i++) {
        Collider_InitQuad(play, &sOrbQuads[i]);
        Collider_SetQuad(play, &sOrbQuads[i], &player->actor, &sEscortQuadInit);
    }
    Collider_InitQuad(play, &sReflectQuad);
    Collider_SetQuad(play, &sReflectQuad, &player->actor, &sEscortQuadInit);
    sAreOrbQuadsReady = true;
    sIsReflectQuadReady = true;
}

static void StampEscortQuad(PlayState* play, ColliderQuad* quad, Vec3f* center, f32 half, u32 dmgFlags, u8 damage) {
    Vec3f a = { center->x - half, center->y + half, center->z };
    Vec3f b = { center->x + half, center->y + half, center->z };
    Vec3f c = { center->x + half, center->y - half, center->z };
    Vec3f d = { center->x - half, center->y - half, center->z };

    Collider_ResetQuadAT(play, &quad->base);
    Collider_SetQuadVertices(quad, &a, &b, &c, &d);
    quad->base.atFlags = AT_ON | AT_TYPE_PLAYER;
    quad->info.toucher.dmgFlags = dmgFlags;
    quad->info.toucher.damage = damage;
    quad->info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;
    CollisionCheck_SetAT(play, &play->colChkCtx, &quad->base);
}

static void RecordOrbTrail(RodOrb* orb, f32 velX, f32 velY, f32 velZ) {
    orb->trailIndex = (orb->trailIndex + 1) % ORB_TRAIL_LEN;
    orb->trailPos[orb->trailIndex] = orb->pos;
    orb->trailRot[orb->trailIndex].y = atan2f(velX, velZ);
    orb->trailRot[orb->trailIndex].x = atan2f(velY, sqrtf(SQ(velX) + SQ(velZ)));
}

// A plain shot pierces; the level-3 ball breaks on its first hit, or at the end of its flight regardless.
static void UpdateOrbs(PlayState* play, Player* player) {
    for (s32 i = 0; i < ORB_POOL_MAX; i++) {
        RodOrb* orb = &sOrbs[i];

        if (!orb->isActive) {
            continue;
        }
        if (sAreOrbQuadsReady && (sOrbQuads[i].base.atFlags & AT_HIT)) {
            sOrbQuads[i].base.atFlags &= ~AT_HIT;
            if (orb->bursts) {
                BurstOrb(play, orb);
                orb->isActive = false;
                continue;
            }
        }
        HomeOrb(play, orb);
        f32 speed = orb->isSeeker ? ORB_SEEKER_SPEED : ROD_ORB_SPEED;
        f32 cosPitch = Math_CosS(orb->pitch);
        f32 velX = Math_SinS(orb->yaw) * cosPitch * speed;
        f32 velY = -Math_SinS(orb->pitch) * speed;
        f32 velZ = Math_CosS(orb->yaw) * cosPitch * speed;
        orb->pos.x += velX;
        orb->pos.y += velY;
        orb->pos.z += velZ;
        if (orb->isSeeker) {
            RecordOrbTrail(orb, velX, velY, velZ);
        }
        MeltRedIce(play, orb);
        if ((orb->timer & 3) == 0) {
            Vec3f zero = { 0.0f, 0.0f, 0.0f };
            EffectSsFhgFlash_SpawnLightBall(play, &orb->pos, &zero, &zero, ORB_WAKE_SCALE,
                                            sOrbWakeColor[orb->element % ELEMENT_COUNT]);
        }
        if (--orb->timer <= 0) {
            if (orb->bursts) {
                BurstOrb(play, orb);
            }
            orb->isActive = false;
            continue;
        }
        PrepareEscortQuads(play, player);
        StampEscortQuad(play, &sOrbQuads[i], &orb->pos, 8.0f, orb->dmgFlags | DMG_SLASH_MASTER, orb->damage);
    }
}

// ---- reflected shots ----

// There is no "projectile" flag: nearly every shot is something you cannot lock onto.
static bool IsRangedAttacker(Actor* actor) {
    if (actor == NULL) {
        return false;
    }
    return actor->category == ACTORCAT_EXPLOSIVE || !(actor->flags & ACTOR_FLAG_ATTENTION_ENABLED);
}

static void ReflectShot(Player* player, Actor* shot) {
    shot->world.rot.y += 0x8000;
    shot->shape.rot.y = shot->world.rot.y;
    shot->velocity.x = -shot->velocity.x;
    shot->velocity.z = -shot->velocity.z;
    shot->speedXZ = MAX(shot->speedXZ, REFLECT_MIN_SPEED);
    shot->world.pos.x += Math_SinS(shot->world.rot.y) * REFLECT_PUSH_OUT;
    shot->world.pos.z += Math_CosS(shot->world.rot.y) * REFLECT_PUSH_OUT;
    sReflectedShot = shot;
    sReflectTimer = REFLECT_FRAMES;
    player->invincibilityTimer = REFLECT_INVULNERABILITY;
    Audio_PlaySoundGeneral(NA_SE_IT_SHIELD_REFLECT_SW, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static bool IsShotIncoming(Actor* actor, Vec3f* chest) {
    f32 dx = chest->x - actor->world.pos.x;
    f32 dy = chest->y - actor->world.pos.y;
    f32 dz = chest->z - actor->world.pos.z;
    f32 velX = actor->velocity.x + Math_SinS(actor->world.rot.y) * actor->speedXZ;
    f32 velZ = actor->velocity.z + Math_CosS(actor->world.rot.y) * actor->speedXZ;

    if (SQ(dx) + SQ(dy) + SQ(dz) > SQ(GUARD_CATCH_RADIUS)) {
        return false;
    }
    return SQ(velX) + SQ(velZ) >= SQ(GUARD_CATCH_MIN_SPEED) && (velX * dx + velZ * dz) > 0.0f;
}

// Every category is swept: a shot can recategorise itself in its own init, as the Octorok's rock does.
static bool TryReflectIncoming(PlayState* play, Player* player) {
    Vec3f chest = { player->actor.world.pos.x, player->actor.world.pos.y + GUARD_CATCH_HEIGHT,
                    player->actor.world.pos.z };

    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        if (category == ACTORCAT_PLAYER || category == ACTORCAT_BG || category == ACTORCAT_DOOR ||
            category == ACTORCAT_CHEST) {
            continue;
        }
        for (Actor* actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            if (actor->update == NULL || actor == sReflectedShot || !IsRangedAttacker(actor) ||
                !IsShotIncoming(actor, &chest)) {
                continue;
            }
            ReflectShot(player, actor);
            return true;
        }
    }
    return false;
}

// The shot keeps its own look and collider; what hurts on the way back is a sword quad escorting it.
static void EscortReflectedShot(PlayState* play, Player* player) {
    if (sReflectTimer <= 0) {
        return;
    }
    sReflectTimer--;
    if (!IsActorAlive(play, sReflectedShot)) {
        sReflectedShot = NULL;
        sReflectTimer = 0;
        return;
    }
    PrepareEscortQuads(play, player);
    StampEscortQuad(play, &sReflectQuad, &sReflectedShot->world.pos, REFLECT_HALF, DMG_SLASH_MASTER, REFLECT_DAMAGE);
}

// ---- moves ----

static void StartMove(PlayState* play, Player* player, u8 move) {
    if (player->actionFunc != GaroAction) {
        Player_SetupAction(play, player, GaroAction, 0);
    }
    sMove = move;
    sMoveTimer = 0;
    sHasStruck = false;
    if (WantsTrails()) {
        SpawnTrails(play);
    }
}

// Ending a free spin keeps the heading he was travelling; one done on the spot hands back the entry facing.
static void SettleSpinFacing(Player* player) {
    player->actor.shape.rot.y = player->linearVelocity > 0.5f ? player->yaw : sSpinEntryYaw;
    player->yaw = player->actor.shape.rot.y;
}

static void ClearMove(PlayState* play, Player* player) {
    if (sMove == GARO_MOVE_SPIN) {
        SettleSpinFacing(player);
    }
    if (sMove == GARO_MOVE_ROD_AIM) {
        Z64Aiming_Release(player, play);
        player->upperLimbRot.x = 0;
        player->upperLimbRot.y = 0;
    }
    sMove = GARO_MOVE_NONE;
    sMoveTimer = 0;
    sParryAttacker = NULL;
    sBanishTarget = NULL;
    sStrikeTailFrames = 0;
    sHopAirFrames = 0;
    player->stateFlags2 &= ~(PLAYER_STATE2_DISABLE_DRAW | PLAYER_STATE2_DISABLE_ROTATION_ALWAYS);
    player->actor.gravity = JUMP_GRAVITY;
    StopStriking(player);
    KillTrails(play);
}

static void EndMove(PlayState* play, Player* player) {
    ClearMove(play, player);
    player->linearVelocity = 0.0f;
    func_80839FFC(player, play);
}

static void Freeze(Player* player) {
    player->linearVelocity = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.z = 0.0f;
}

// Airborne moves keep MIDAIR set: without it func_8083AA10 reads the leap as stepping off an edge and swaps in
// the fall.
static void KeepAirborne(Player* player) {
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->actor.gravity = JUMP_GRAVITY;
    player->yaw = sHopYaw;
}

static bool HasLanded(Player* player) {
    sHopAirFrames++;
    return IsGrounded(player) && sHopAirFrames >= HOP_MIN_AIR_FRAMES;
}

// -- spin and rod --

static void StartSpin(PlayState* play, Player* player) {
    sSpinEntryYaw = player->actor.shape.rot.y;
    sBHeldFrames = 0;
    StartMove(play, player, GARO_MOVE_SPIN);
    PlayGaroAnim(play, player, ANIM_SPIN, 0.0f, -1.0f, SPIN_PLAY_SPEED);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
}

static void ResetRodCharge(void) {
    sRodCharge = 0;
    sIsRodChimed = false;
    sRodBallCircle = 0.0f;
    sRodBallScale = 0.0f;
    sRodBallRays = 0;
}

static void StartRodAim(PlayState* play, Player* player) {
    StopStriking(player);
    SettleSpinFacing(player);
    ResetRodCharge();
    KillTrails(play);
    sMove = GARO_MOVE_ROD_AIM;
    sMoveTimer = 0;
    player->linearVelocity = 0.0f;
    PlayGaroAnim(play, player, ANIM_TAKE_OUT_BOMB, 0.0f, -1.0f, 1.0f);
    Z64Aiming_Request(player, play);
}

// The spin's shape turns on the timer, never as a rate: Z-target keeps dragging it toward the lock-on. Where he
// travels is `yaw`, the field Player_UpdateCommon copies into world.rot.y.
static void RunSpin(PlayState* play, Player* player) {
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    if (IsHeld(play, BTN_B) && ++sBHeldFrames >= B_HOLD_FRAMES) {
        StartRodAim(play, player);
        return;
    }
    sMoveTimer++;
    player->actor.shape.rot.y = (s16)(sSpinEntryYaw + sMoveTimer * SPIN_YAW_RATE);
    f32 stick = GetStickMagnitude(play);
    if (stick > 0.1f) {
        player->yaw = GetStickYaw(play);
        player->linearVelocity = stick * SPIN_MOVE_SPEED;
    } else {
        player->linearVelocity = 0.0f;
    }
    StrikeAhead(play, player, 35.0f, 55.0f, SPIN_DAMAGE);
    if (AdvanceGaroAnim(play, player)) {
        PlayGaroAnim(play, player, ANIM_SPIN, 0.0f, -1.0f, SPIN_PLAY_SPEED);
    }
    if (sMoveTimer >= SPIN_FRAMES) {
        EndMove(play, player);
    }
}

// The ball steps a size per damage tier, so what it looks like is what the shot will do.
static void GrowRodBall(void) {
    u8 level = GetRodLevel(sRodCharge);
    f32 fraction = sRodLevelScale[level - 1];
    s16 wantRays = sRodLevelRays[level - 1];

    Math_ApproachF(&sRodBallCircle, ROD_BALL_CIRCLE_MAX * fraction, 0.3f, 0.01f);
    Math_ApproachF(&sRodBallScale, ROD_BALL_SCALE_MAX * fraction, 0.3f, 1.0f);
    if ((sMoveTimer & 3) == 0 && sRodBallRays != wantRays) {
        sRodBallRays += sRodBallRays < wantRays ? 1 : -1;
    }
}

static void CycleRodElement(PlayState* play, Player* player) {
    if (IsPressed(play, BTN_L)) {
        sRodElement = (sRodElement + ELEMENT_COUNT - 1) % ELEMENT_COUNT;
        Audio_PlayActorSound2(&player->actor, NA_SE_SY_DECIDE);
    }
    if (IsPressed(play, BTN_R)) {
        sRodElement = (sRodElement + 1) % ELEMENT_COUNT;
        Audio_PlayActorSound2(&player->actor, NA_SE_SY_DECIDE);
    }
}

// A cancels the aim as it does the Deku bubble's; letting go of B fires.
static void RunRodAim(PlayState* play, Player* player) {
    Z64Aiming_Update(player, play);
    if (IsPressed(play, BTN_A)) {
        sRodCooldown = ROD_RELEASE_COOLDOWN;
        EndMove(play, player);
        return;
    }
    if (!IsHeld(play, BTN_B)) {
        FireRodOrb(player);
        sRodCooldown = ROD_RELEASE_COOLDOWN;
        EndMove(play, player);
        return;
    }
    sMoveTimer++;
    if (sRodCharge < ROD_CHARGE_MAX) {
        sRodCharge++;
        func_8002F974(&player->actor, GetChargeSfx(sRodElement) - SFX_FLAG);
    }
    GrowRodBall();
    if (!sIsRodChimed && sRodCharge >= ROD_CHARGE_TIER3) {
        Audio_PlayActorSound2(&player->actor, NA_SE_SY_SYNTH_MAGIC_ARROW);
        sIsRodChimed = true;
    }
    CycleRodElement(play, player);
    AdvanceGaroAnim(play, player);
}

// -- guard and counter --

static void StartGuardReturn(PlayState* play, Player* player) {
    sMove = GARO_MOVE_GUARD_RETURN;
    sMoveTimer = 0;
    PlayGaroAnim(play, player, ANIM_GUARD, GetAnimLastFrame(ANIM_GUARD) * GUARD_HOLD_FRACTION, 0.0f,
                 -GUARD_RETURN_SPEED);
}

// The attacker freezes, Garo appears behind it from wherever he stood and opens with the jump attack's swing.
static void StartRiposte(PlayState* play, Player* player, Actor* attacker) {
    if (attacker != NULL) {
        s16 yaw = attacker->shape.rot.y;

        MarkStunned(play, attacker, PARRY_FREEZE_FRAMES);
        player->actor.world.pos.x = attacker->world.pos.x - Math_SinS(yaw) * RIPOSTE_OFFSET;
        player->actor.world.pos.z = attacker->world.pos.z - Math_CosS(yaw) * RIPOSTE_OFFSET;
        player->actor.world.pos.y = attacker->world.pos.y;
        player->actor.shape.rot.y = yaw;
        player->yaw = yaw;
    }
    sParryAttacker = attacker;
    StartMove(play, player, GARO_MOVE_RIPOSTE);
    PlayGaroAnim(play, player, ANIM_DRAW_SWORDS, 0.0f, -1.0f, 1.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_FANTOM_WARP_S);
}

// A shooter across the room is not what touched him: its shot goes back and the stance ends, with no chase.
static void AnswerHit(PlayState* play, Player* player, Actor* attacker) {
    f32 distSq = attacker == NULL ? 0.0f
                                  : SQ(attacker->world.pos.x - player->actor.world.pos.x) +
                                        SQ(attacker->world.pos.z - player->actor.world.pos.z);

    if (IsRangedAttacker(attacker) || distSq > SQ(GUARD_MELEE_RANGE)) {
        if (IsRangedAttacker(attacker)) {
            ReflectShot(player, attacker);
        }
        StartGuardReturn(play, player);
        return;
    }
    StartRiposte(play, player, attacker);
}

static void RunGuard(PlayState* play, Player* player) {
    Freeze(player);
    AdvanceGaroAnim(play, player);
    if (TryReflectIncoming(play, player)) {
        StartGuardReturn(play, player);
        return;
    }
    if (sIsParryPending) {
        Actor* attacker = IsActorAlive(play, sPendingParry) ? sPendingParry : player->focusActor;

        sIsParryPending = false;
        sPendingParry = NULL;
        AnswerHit(play, player, attacker);
        return;
    }
    sMoveTimer++;
    if (!IsHeld(play, BTN_R)) {
        StartGuardReturn(play, player);
    }
}

static void RunGuardReturn(PlayState* play, Player* player) {
    Freeze(player);
    if (AdvanceGaroAnim(play, player)) {
        EndMove(play, player);
    }
}

static void ThawBeforeStrike(PlayState* play, Actor* target, s16 strikeFrame, s16 lead) {
    if (sMoveTimer >= strikeFrame - lead && IsActorAlive(play, target)) {
        target->freezeTimer = 0;
    }
}

static void RunRiposte(PlayState* play, Player* player) {
    Freeze(player);
    ThawBeforeStrike(play, sParryAttacker, PARRY_STRIKE_FRAME, PARRY_THAW_LEAD);
    if (sMoveTimer >= PARRY_STRIKE_FRAME) {
        StrikeAround(play, player, sMoveTimer - PARRY_STRIKE_FRAME, PARRY_DAMAGE);
        if (!sHasStruck) {
            sHasStruck = true;
            Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
        }
    }
    bool isDone = AdvanceGaroAnim(play, player);
    sMoveTimer++;
    if (isDone) {
        EndMove(play, player);
    }
}

// -- dash --

static void RunDash(PlayState* play, Player* player) {
    LinkAnimationHeader* dash = sAnims[ANIM_DASH];

    if (dash != NULL && player->skelAnime.animation != dash) {
        LinkAnimation_Change(play, &player->skelAnime, dash, 1.0f, 0.0f, Animation_GetLastFrame(dash), ANIMMODE_LOOP,
                             -4.0f);
    }
    if (GetStickMagnitude(play) > 0.3f) {
        Math_ScaledStepToS(&player->yaw, GetStickYaw(play), DASH_TURN);
    }
    player->actor.shape.rot.y = player->yaw;
    player->linearVelocity = DASH_SPEED;
    StrikeAhead(play, player, 25.0f, 60.0f, DASH_DAMAGE);
    AdvanceGaroAnim(play, player);
    sMoveTimer++;
    if ((player->actor.bgCheckFlags & BG_TOUCHING_WALL) || !IsHeld(play, BTN_A)) {
        EndMove(play, player);
    }
}

// -- banish --

static void StartBanish(PlayState* play, Player* player, Actor* target) {
    StartMove(play, player, GARO_MOVE_BANISH_VANISH);
    PlayGaroAnim(play, player, ANIM_COLLAPSE, 0.0f, -1.0f, 1.0f);
    sBanishTarget = target;
    sBanishCooldown = BANISH_COOLDOWN;
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_FANTOM_WARP_S);
    if (target != NULL && target->category == ACTORCAT_ENEMY) {
        MarkStunned(play, target, BANISH_STUN_FRAMES);
    }
}

// With nothing locked the shadow ball still crosses a short way ahead, and he reappears where he stood.
static void LaunchShadowBall(PlayState* play, Player* player) {
    Actor* target = sBanishTarget;

    sShadowStart = player->actor.world.pos;
    sShadowStart.y += 30.0f;
    if (IsActorAlive(play, target)) {
        sShadowEnd = target->world.pos;
    } else {
        sShadowEnd = player->actor.world.pos;
        sShadowEnd.x += Math_SinS(player->actor.shape.rot.y) * BANISH_NO_TARGET_REACH;
        sShadowEnd.z += Math_CosS(player->actor.shape.rot.y) * BANISH_NO_TARGET_REACH;
    }
    sShadowEnd.y += 30.0f;
    sShadowTimer = 0;
    sMove = GARO_MOVE_BANISH_SHADOW;
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_FANTOM_WARP_S);
}

static void RunBanishVanish(PlayState* play, Player* player) {
    Freeze(player);
    if (sMoveTimer < BANISH_VANISH_FRAMES) {
        player->actor.world.pos.y -= 1.0f;
    } else {
        player->stateFlags2 |= PLAYER_STATE2_DISABLE_DRAW;
    }
    bool isDone = AdvanceGaroAnim(play, player);
    sMoveTimer++;
    if (isDone || sMoveTimer >= BANISH_VANISH_FRAMES) {
        LaunchShadowBall(play, player);
    }
}

static void DropShadowWake(PlayState* play) {
    f32 backX = sShadowStart.x - sShadowEnd.x;
    f32 backZ = sShadowStart.z - sShadowEnd.z;
    f32 length = sqrtf(SQ(backX) + SQ(backZ));
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 110, 40, 190, 220 };
    Color_RGBA8 env = { 25, 0, 60, 0 };

    if (length > 0.001f) {
        backX = backX / length * 10.0f;
        backZ = backZ / length * 10.0f;
    }
    for (s32 i = 0; i < 2; i++) {
        Vec3f pos = { sShadowPos.x + backX + Rand_CenteredFloat(8.0f), sShadowPos.y + Rand_CenteredFloat(8.0f),
                      sShadowPos.z + backZ + Rand_CenteredFloat(8.0f) };
        EffectSsDust_Spawn(play, 0, &pos, &zero, &zero, &prim, &env, 90, -6, 8, 0);
    }
}

// The warp hum is a loop, asked for every frame of the crossing so it ends with it.
static void RunBanishShadow(PlayState* play, Player* player) {
    f32 t = MIN((f32)sShadowTimer / (f32)BANISH_SHADOW_FRAMES, 1.0f);

    Freeze(player);
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_DRAW;
    sShadowPos.x = sShadowStart.x + (sShadowEnd.x - sShadowStart.x) * t;
    sShadowPos.y = sShadowStart.y + (sShadowEnd.y - sShadowStart.y) * t;
    sShadowPos.z = sShadowStart.z + (sShadowEnd.z - sShadowStart.z) * t;
    func_8002F974(&player->actor, NA_SE_EV_FANTOM_WARP_L - SFX_FLAG);
    DropShadowWake(play);
    if (++sShadowTimer < BANISH_SHADOW_FRAMES) {
        return;
    }
    if (IsActorAlive(play, sBanishTarget)) {
        s16 yaw = sBanishTarget->shape.rot.y;

        player->actor.world.pos.x = sBanishTarget->world.pos.x - Math_SinS(yaw) * BANISH_OFFSET;
        player->actor.world.pos.z = sBanishTarget->world.pos.z - Math_CosS(yaw) * BANISH_OFFSET;
        player->actor.world.pos.y = sBanishTarget->world.pos.y;
        player->actor.shape.rot.y = yaw;
        player->yaw = yaw;
    }
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_DRAW;
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_FANTOM_WARP_S);
    sMove = GARO_MOVE_SHADOW_STRIKE;
    sMoveTimer = 0;
    sHasStruck = false;
    SpawnTrails(play);
    PlayGaroAnim(play, player, ANIM_APPEAR_DRAW_SWORDS, 0.0f, -1.0f, SHADOW_STRIKE_PLAY_SPEED);
}

// He never leaves before the strike has had its window: the fast arrival clip ends before it otherwise.
static void RunShadowStrike(PlayState* play, Player* player) {
    Freeze(player);
    player->invincibilityTimer = MAX(player->invincibilityTimer, 5);
    ThawBeforeStrike(play, sBanishTarget, SHADOW_STRIKE_FRAME, BANISH_THAW_LEAD);
    s16 live = sMoveTimer - SHADOW_STRIKE_FRAME;
    if (live >= 0 && live <= SHADOW_STRIKE_WINDOW) {
        StrikeWallAhead(play, player, live, SHADOW_STRIKE_DAMAGE);
        if (!sHasStruck) {
            sHasStruck = true;
            Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
        }
    } else {
        StopStriking(player);
    }
    bool isDone = AdvanceGaroAnim(play, player);
    sMoveTimer++;
    if (isDone && live > SHADOW_STRIKE_WINDOW) {
        EndMove(play, player);
    }
}

// -- leaps --

static void StartLeap(PlayState* play, Player* player, u8 move, s32 anim, s16 yaw, f32 xzSpeed, f32 ySpeed) {
    StartMove(play, player, move);
    PlayGaroAnim(play, player, anim, 0.0f, -1.0f, move == GARO_MOVE_JUMP_ATTACK ? 1.5f : 1.0f);
    sHopYaw = yaw;
    sHopAirFrames = 0;
    player->yaw = yaw;
    player->linearVelocity = xzSpeed;
    player->actor.velocity.y = ySpeed;
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    Audio_PlayActorSound2(&player->actor, NA_SE_VO_LI_AUTO_JUMP);
}

static void StartLandStrike(PlayState* play, Player* player) {
    sMove = GARO_MOVE_LAND_STRIKE;
    sMoveTimer = 0;
    sHasStruck = false;
    sStrikeTailFrames = 0;
    SpawnTrails(play);
    PlayGaroAnim(play, player, ANIM_DRAW_SWORDS, 0.0f, -1.0f, 1.0f);
}

static void StartAirSlash(PlayState* play, Player* player) {
    if (player->actionFunc != GaroAction) {
        sHopYaw = player->yaw;
        sHopAirFrames = 0;
    }
    StartMove(play, player, GARO_MOVE_AIR_SLASH);
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    PlayGaroAnim(play, player, ANIM_SLASH_LOOP, 0.0f, -1.0f, 1.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING);
}

// Hops and the back flip keep facing the lock-on while `yaw` carries them sideways or back.
static void RunHop(PlayState* play, Player* player) {
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    KeepAirborne(player);
    AdvanceGaroAnim(play, player);
    if (HasLanded(player)) {
        EndMove(play, player);
    }
}

// The leap turns into the air slash past its apex, and a short hop lands straight into the strike.
static void RunJumpAttack(PlayState* play, Player* player) {
    KeepAirborne(player);
    AdvanceGaroAnim(play, player);
    if (HasLanded(player)) {
        StartLandStrike(play, player);
    } else if (sHopAirFrames >= JUMP_ATTACK_SLASH_FRAME) {
        StartAirSlash(play, player);
    }
}

static void RunAirSlash(PlayState* play, Player* player) {
    KeepAirborne(player);
    if (AdvanceGaroAnim(play, player)) {
        PlayGaroAnim(play, player, ANIM_SLASH_LOOP, 0.0f, -1.0f, 1.0f);
    }
    sMoveTimer++;
    if (IsGrounded(player) && sMoveTimer >= 2) {
        Audio_PlayActorSound2(&player->actor, NA_SE_PL_ROLL_DUST);
        StartLandStrike(play, player);
    }
}

static void RunLandStrike(PlayState* play, Player* player) {
    Freeze(player);
    if (sMoveTimer >= LAND_STRIKE_FRAME) {
        StrikeAround(play, player, sMoveTimer - LAND_STRIKE_FRAME, LAND_STRIKE_DAMAGE);
        if (!sHasStruck) {
            sHasStruck = true;
            Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
        }
    }
    bool isDone = AdvanceGaroAnim(play, player);
    sMoveTimer++;
    if (isDone && ++sStrikeTailFrames > LAND_STRIKE_TAIL) {
        EndMove(play, player);
    }
}

// -- laugh --

static void RunLaugh(PlayState* play, Player* player) {
    bool isDone = AdvanceGaroAnim(play, player);
    bool wantsToMove =
        GetStickMagnitude(play) > 0.1f || IsPressed(play, BTN_A) || IsPressed(play, BTN_B) || IsPressed(play, BTN_R);

    if (isDone || wantsToMove) {
        EndMove(play, player);
    }
}

static void GaroAction(Player* player, PlayState* play) {
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    switch (sMove) {
        case GARO_MOVE_SPIN:
            RunSpin(play, player);
            break;
        case GARO_MOVE_ROD_AIM:
            RunRodAim(play, player);
            break;
        case GARO_MOVE_GUARD:
            RunGuard(play, player);
            break;
        case GARO_MOVE_GUARD_RETURN:
            RunGuardReturn(play, player);
            break;
        case GARO_MOVE_RIPOSTE:
            RunRiposte(play, player);
            break;
        case GARO_MOVE_DASH:
            RunDash(play, player);
            break;
        case GARO_MOVE_BANISH_VANISH:
            RunBanishVanish(play, player);
            break;
        case GARO_MOVE_BANISH_SHADOW:
            RunBanishShadow(play, player);
            break;
        case GARO_MOVE_SHADOW_STRIKE:
            RunShadowStrike(play, player);
            break;
        case GARO_MOVE_LAUGH:
            RunLaugh(play, player);
            break;
        case GARO_MOVE_HOP:
        case GARO_MOVE_BACKFLIP:
            RunHop(play, player);
            break;
        case GARO_MOVE_JUMP_ATTACK:
            RunJumpAttack(play, player);
            break;
        case GARO_MOVE_AIR_SLASH:
            RunAirSlash(play, player);
            break;
        case GARO_MOVE_LAND_STRIKE:
            RunLandStrike(play, player);
            break;
        default:
            EndMove(play, player);
            break;
    }
}

// ---- action overrides ----

static bool CanStartMove(Player* player) {
    return IsGaro() && !IsGaroBusy(player) && CanAct(player) && sRodCooldown == 0;
}

static int32_t HandleSlash(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_B)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (!CanStartMove(player) || !IsGrounded(player)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    StartSpin(play, player);
    return SOH_FORM_ACTION_STARTED;
}

// Z-targeted A falls through to the next handler, which is where the hops live.
static int32_t HandleDash(PlayState* play, Player* player) {
    if (!IsGaro() || !IsPressed(play, BTN_A) || Player_IsZTargeting(player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (!CanStartMove(player) || !IsGrounded(player)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    StartMove(play, player, GARO_MOVE_DASH);
    return SOH_FORM_ACTION_STARTED;
}

static void StartZTargetMove(PlayState* play, Player* player, s8 direction) {
    s16 facing = player->actor.shape.rot.y;

    if (direction == PLAYER_STICK_DIR_BACKWARD) {
        StartLeap(play, player, GARO_MOVE_BACKFLIP, ANIM_JUMP_BACK, facing + 0x8000, 12.0f, 5.8f);
    } else if (direction == PLAYER_STICK_DIR_LEFT || direction == PLAYER_STICK_DIR_RIGHT) {
        s16 side = direction == PLAYER_STICK_DIR_LEFT ? -0x4000 : 0x4000;
        StartLeap(play, player, GARO_MOVE_HOP, ANIM_BOUNCE, facing + side, 12.75f, 4.5f);
    } else if (direction == PLAYER_STICK_DIR_FORWARD) {
        StartLeap(play, player, GARO_MOVE_JUMP_ATTACK, ANIM_APPEAR, facing, MAX(player->linearVelocity, 10.0f), 7.5f);
    } else {
        StartBanish(play, player, player->focusActor);
    }
}

// Standing still with the stick forward, or neutral while the banish recharges, Z+A does nothing, as in NEI.
static int32_t HandleZTargetA(PlayState* play, Player* player) {
    s8 direction = player->controlStickDirections[player->controlStickDataIndex];
    bool isStillForward = direction == PLAYER_STICK_DIR_FORWARD && player->linearVelocity <= 0.5f;
    bool isBanishSpent = direction == PLAYER_STICK_DIR_NONE && sBanishCooldown > 0;

    if (!IsGaro() || !IsPressed(play, BTN_A)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (!CanStartMove(player) || !IsGrounded(player) || isStillForward || isBanishSpent) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    StartZTargetMove(play, player, direction);
    return SOH_FORM_ACTION_STARTED;
}

// The guard rises halfway through its clip and holds there; the second half plays backwards on release.
static int32_t HandleGuard(PlayState* play, Player* player) {
    if (!IsGaro() || !IsHeld(play, BTN_R) || IsHeld(play, BTN_B)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (!CanStartMove(player) || !IsGrounded(player)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    StartMove(play, player, GARO_MOVE_GUARD);
    PlayGaroAnim(play, player, ANIM_GUARD, 0.0f, GetAnimLastFrame(ANIM_GUARD) * GUARD_HOLD_FRACTION, 1.0f);
    sIsParryPending = false;
    sPendingParry = NULL;
    return SOH_FORM_ACTION_STARTED;
}

// Garo cannot lift anything: an offer with no item attached is a carry, and it is refused.
static int32_t RefuseCarry(PlayState* play, Player* player) {
    if (!IsGaro() || player->interactRangeActor == NULL || player->getItemId != GI_NONE) {
        return SOH_FORM_ACTION_VANILLA;
    }
    return SOH_FORM_ACTION_BLOCKED;
}

// ---- incoming damage ----

// Runs before Player_UpdateCommon consumes the hit: Garo takes it doubled, and the guard takes it without the
// knockback so its counter can answer.
static void HandleIncomingHit(Player* player, Input* input) {
    u8 damage = player->actor.colChkInfo.damage;

    (void)input;
    if (!IsGaro() || damage == 0) {
        sScaledDamage = 0;
        return;
    }
    if (!(player->cylinder.base.acFlags & AC_HIT)) {
        return;
    }
    if (damage != sScaledDamage) {
        sScaledDamage = (u8)MIN(damage * 2, 0xFF);
        player->actor.colChkInfo.damage = sScaledDamage;
    }
    if (sMove != GARO_MOVE_GUARD || gPlayState == NULL) {
        return;
    }
    func_80837B18(gPlayState, player, -player->actor.colChkInfo.damage);
    sPendingParry = player->cylinder.base.ac;
    sIsParryPending = true;
    player->actor.colChkInfo.damage = 0;
    player->cylinder.base.acFlags &= ~AC_HIT;
    sScaledDamage = 0;
}

// NEI's strikes hit for exactly their number whatever the enemy's table says; the orbs keep the table, since
// their element is what finds a weakness.
static void FixStrikeDamage(Actor* victim, ColliderInfo* attack, float* damage) {
    (void)victim;
    if (!IsGaro() || gPlayState == NULL) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    bool isStrike = sMove != GARO_MOVE_NONE && attack == &player->meleeWeaponQuads[0].info;
    bool isReflect = sIsReflectQuadReady && attack == &sReflectQuad.info;
    if (isStrike || isReflect) {
        *damage = attack->toucher.damage;
    }
}

// ---- world ----

// A Garo runs across water: while he is out of it and not rising, the surface is his floor.
static void WalkOnWater(CollisionContext* colCtx, Vec3f* pos, Actor* actor, CollisionPoly** poly, int32_t* bgId,
                        float* floorY) {
    PlayState* play = gPlayState;
    WaterBox* waterBox;
    f32 surface;

    if (play == NULL || !IsGaro() || actor != &GET_PLAYER(play)->actor) {
        return;
    }
    Player* player = (Player*)actor;
    if ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) || actor->velocity.y > 0.0f) {
        return;
    }
    if (!WaterBox_GetSurface1(play, colCtx, pos->x, pos->z, &surface, &waterBox) || surface <= *floorY) {
        return;
    }
    *floorY = surface;
}

// He sees what the Lens shows without its circle: lensActive would bring the overlay, which is what masks the
// reveal. In a hiding room the lens actors lose their flag and draw like any other; elsewhere they are fakes.
static void RevealHidden(PlayState* play) {
    if (play->roomCtx.curRoom.lensMode != LENS_MODE_HIDE_ACTORS) {
        return;
    }
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            if (!(actor->flags & ACTOR_FLAG_REACT_TO_LENS) || sRevealedCount >= REVEALED_MAX) {
                continue;
            }
            actor->flags &= ~ACTOR_FLAG_REACT_TO_LENS;
            sRevealed[sRevealedCount++] = actor;
        }
    }
}

static void ConcealRevealed(PlayState* play) {
    for (s32 i = 0; i < sRevealedCount; i++) {
        if (IsActorAlive(play, sRevealed[i])) {
            sRevealed[i]->flags |= ACTOR_FLAG_REACT_TO_LENS;
        }
    }
    sRevealedCount = 0;
}

// The real Lens still does its own job on top.
static void HideFakes(Actor* actor, PlayState* play, bool* drawVanilla) {
    if (!IsGaro() || !(actor->flags & ACTOR_FLAG_REACT_TO_LENS) || play->actorCtx.lensActive ||
        play->roomCtx.curRoom.lensMode == LENS_MODE_HIDE_ACTORS || actor->room != play->roomCtx.curRoom.num) {
        return;
    }
    *drawVanilla = false;
}

static bool IsHoldingOcarina(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_OCARINA || player->rightHandType == PLAYER_MODELTYPE_RH_OOT;
}

static MmOcarinaNoteFunc FindChantVoice(void) {
    if (!SOH_MOD_API_HAS(sApi, FindService)) {
        return NULL;
    }
    return (MmOcarinaNoteFunc)sApi->FindService(MM_OCARINA_NOTE_SERVICE);
}

// Igos du Ikana's chant, the voice MM gives the Elegy. The hook runs right after Ocarina triggered its own
// voice, so stopping that here always lands.
static void ChantNote(uint8_t note, float modulator, int8_t bend) {
    MmOcarinaNoteFunc voice = sChantVoice;

    (void)bend;
    if (voice != NULL) {
        sChantBend = modulator;
        if (note != OCARINA_NOTE_NONE) {
            Audio_StopSfxById(NA_SE_OC_OCARINA);
        }
        if (note != sLastChantNote) {
            voice(MM_OCARINA_INSTRUMENT_IKANA_KING, note, &sChantBend);
        }
    }
    sLastChantNote = note;
}

static void PlaybackChantNote(uint8_t note, float modulator) {
    ChantNote(note, modulator, 0);
}

static void SilenceChant(void) {
    MmOcarinaNoteFunc voice = sChantVoice;

    sChantVoice = NULL;
    if (voice != NULL) {
        voice(MM_OCARINA_INSTRUMENT_IKANA_KING, OCARINA_NOTE_NONE, &sChantBend);
    }
}

// The ocarina stays out across the message session, so the latch only lets go once no message is up.
static void UpdateOcarinaVoice(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsOcarinaOut = true;
    } else if (!isSessionOpen) {
        sIsOcarinaOut = false;
    }
    if (!sIsOcarinaOut) {
        if (sChantVoice != NULL) {
            SilenceChant();
        }
        return;
    }
    if (sChantVoice == NULL) {
        sChantVoice = FindChantVoice();
    }
}

// Any enemy that vanished since last frame counts as a kill for the taunt: a despawn only costs a dice roll.
static void WatchForKills(PlayState* play) {
    Actor* current[ENEMY_SNAPSHOT_MAX];
    s32 count = 0;

    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; enemy != NULL && count < ENEMY_SNAPSHOT_MAX;
         enemy = enemy->next) {
        current[count++] = enemy;
    }
    for (s32 i = 0; i < sEnemySnapshotCount && count > 0 && !sIsLaughPending; i++) {
        bool isStillThere = false;
        for (s32 j = 0; j < count; j++) {
            isStillThere = isStillThere || current[j] == sEnemySnapshot[i];
        }
        sIsLaughPending = !isStillThere;
    }
    memcpy(sEnemySnapshot, current, sizeof(Actor*) * count);
    sEnemySnapshotCount = count;
}

static void TryLaugh(PlayState* play, Player* player) {
    if (!sIsLaughPending || IsGaroBusy(player) || !CanAct(player) || !IsGrounded(player)) {
        return;
    }
    sIsLaughPending = false;
    if (Rand_ZeroOne() >= LAUGH_CHANCE || sAnims[ANIM_LAUGH] == NULL) {
        return;
    }
    StartMove(play, player, GARO_MOVE_LAUGH);
    PlayGaroAnim(play, player, ANIM_LAUGH, 0.0f, -1.0f, 1.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_VO_SK_LAUGH);
}

// Vanilla's jump reads no item buttons, so the air slash reads B here, off any jump or fall.
static void TryAirSlash(PlayState* play, Player* player) {
    if (IsGaroBusy(player) || IsGrounded(player) || !CanAct(player) || sRodCooldown > 0 || !IsPressed(play, BTN_B)) {
        return;
    }
    StartAirSlash(play, player);
}

// MM's Garo Master burns away in a ring of flames when he falls.
static void BurnOnDeath(PlayState* play, Player* player) {
    Vec3f accel = { 0.0f, 0.1f, 0.0f };

    if (!(player->stateFlags1 & PLAYER_STATE1_DEAD)) {
        sHaveDeathFlamesSpawned = false;
        return;
    }
    if (sHaveDeathFlamesSpawned) {
        return;
    }
    sHaveDeathFlamesSpawned = true;
    for (s32 i = 0; i < DEATH_FLAMES; i++) {
        f32 angle = (f32)i * ((f32)M_PI * 2.0f / (f32)DEATH_FLAMES);
        Vec3f pos = { player->actor.world.pos.x + cosf(angle) * DEATH_FLAME_RADIUS, player->actor.world.pos.y + 5.0f,
                      player->actor.world.pos.z + sinf(angle) * DEATH_FLAME_RADIUS };
        Vec3f velocity = { cosf(angle) * 0.5f, 1.5f, sinf(angle) * 0.5f };
        EffectSsDFire_Spawn(play, &pos, &velocity, &accel, 100, 35, 255, 8, 12);
    }
    Audio_PlayActorSound2(&player->actor, NA_SE_EN_STAL_DEAD);
}

// ---- hybrid body ----

static bool PrepareHybrid(PlayState* play) {
    if (sIsHybridReady) {
        return true;
    }
    SkeletonHeader* header = ResourceMgr_LoadSkeletonByName(sHybridSkelPath, NULL);
    if (header == NULL || header->limbCount + 1 != HYBRID_JOINTS) {
        return false;
    }
    SkelAnime_InitFlex(play, &sHybrid, (FlexSkeletonHeader*)sHybridSkelPath, (AnimationHeader*)sHybridIdlePath,
                       sHybridJoints, sHybridMorph, HYBRID_JOINTS);
    Animation_PlayLoop(&sHybrid, (AnimationHeader*)sHybridIdlePath);
    sIsHybridReady = true;
    return true;
}

// Garo's idle sways every bone, then Link's pose overwrites the body's; the robe's side panels follow the arms.
static void PoseHybrid(PlayState* play, Player* player) {
    Vec3s* link = player->skelAnime.jointTable;

    if (!PrepareHybrid(play) || link == NULL) {
        return;
    }
    SkelAnime_Update(&sHybrid);
    sHybridJoints[0] = link[0];
    for (s32 limb = 1; limb < PLAYER_LIMB_MAX; limb++) {
        s8 joint = sLinkToHybridJoint[limb];
        if (joint >= 0 && !IsGaroOwnedJoint(joint)) {
            sHybridJoints[joint] = link[limb];
        }
    }
    sHybridJoints[HYBRID_JOINT_ROBE_LEFT] = link[PLAYER_LIMB_L_SHOULDER];
    sHybridJoints[HYBRID_JOINT_ROBE_RIGHT] = link[PLAYER_LIMB_R_SHOULDER];
}

// Link's root is placed at Link's scale, then the whole Garo subtree takes the enemy's. A child's animations
// carry the root lower, so it is lifted back to adult hip height.
static s32 OverrideHybridLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg) {
    (void)play;
    (void)dList;
    (void)arg;
    if (limbIndex == HYBRID_JOINT_ROOT) {
        Vec3f root = *pos;
        if (!LINK_IS_ADULT) {
            root.y *= LINK_ROOT_HEIGHT_ADULT / LINK_ROOT_HEIGHT_CHILD;
        }
        Matrix_TranslateRotateZYX(&root, rot);
        Matrix_Scale(HYBRID_SUBTREE_SCALE, HYBRID_SUBTREE_SCALE, HYBRID_SUBTREE_SCALE, MTXMODE_APPLY);
        return true;
    }
    if (limbIndex == HYBRID_JOINT_R_THIGH || limbIndex == HYBRID_JOINT_L_THIGH) {
        Matrix_Scale(1.0f, LEG_THICKNESS, LEG_THICKNESS, MTXMODE_APPLY);
    } else if (limbIndex == HYBRID_JOINT_R_FOOT || limbIndex == HYBRID_JOINT_L_FOOT) {
        Matrix_Scale(1.0f, 1.0f / LEG_THICKNESS, 1.0f / LEG_THICKNESS, MTXMODE_APPLY);
    }
    return false;
}

// The blades only exist on this skeleton, so their trails are fed here, at the bones the player sees.
static void PostHybridLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* arg) {
    Player* player = (Player*)arg;

    (void)play;
    (void)dList;
    (void)rot;
    if (sTrailLeft < 0) {
        return;
    }
    if (limbIndex == HYBRID_JOINT_L_SWORD) {
        FeedTrail(player, sTrailLeft, &sTrailAxisLeft);
    } else if (limbIndex == HYBRID_JOINT_R_SWORD) {
        FeedTrail(player, sTrailRight, &sTrailAxisRight);
    }
}

// MM-format display lists index the backface-cull list through segment 0x0C.
static void DrawHybrid(PlayState* play, Player* player) {
    if (!sIsHybridReady) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    Matrix_Push();
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_Scale(player->actor.scale.x, player->actor.scale.y, player->actor.scale.z, MTXMODE_APPLY);
    SkelAnime_DrawFlexOpa(play, sHybrid.skeleton, sHybrid.jointTable, sHybrid.dListCount, OverrideHybridLimb,
                          PostHybridLimb, player);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// Link's own limbs stay hidden, but their post-draw still runs: feet, hands and focus keep their positions.
static void HideLinkBody(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    (void)player;
    (void)limbIndex;
    (void)limbDList;
    (void)pos;
    if (IsGaro()) {
        *dList = NULL;
    }
}

// ---- projectile draws ----

static void DrawBillboardOrb(PlayState* play, Vec3f pos, f32 scale, u8 element) {
    u8 e = element % ELEMENT_COUNT;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, sOrbPrim[e][0], sOrbPrim[e][1], sOrbPrim[e][2], 255);
    gDPSetEnvColor(POLY_XLU_DISP++, sOrbEnv[e][0], sOrbEnv[e][1], sOrbEnv[e][2], 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sOrbMaterialDL);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sOrbModelDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Additive light only brightens, so the dark core is alpha-blended in one cycle over the halo's glow.
static void DrawShadowBall(PlayState* play, Vec3f pos, f32 scale) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 185, 115, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 85, 20, 165, 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sOrbMaterialDL);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale * 1.9f, scale * 1.9f, scale * 1.9f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sOrbModelDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sOrbMaterialDL);
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_1CYCLE);
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 46, 0, 72, 255);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale * 0.8f, scale * 0.8f, scale * 0.8f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sOrbModelDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawRayFan(PlayState* play, Vec3f pos, f32 ballScale, s32 rays, const u8* prim, const u8* env) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_RotateY((play->gameplayFrames * 10.0f) / 1000.0f, MTXMODE_APPLY);
    gDPSetEnvColor(POLY_XLU_DISP++, env[0], env[1], env[2], 0);
    for (s32 i = 0; i < MIN(rays, BIG_MAGIC_RAYS_MAX); i++) {
        f32 angle = (f32)i * ((f32)M_PI * 2.0f / (f32)BIG_MAGIC_RAYS_MAX);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, prim[0], prim[1], prim[2], 200);
        Matrix_Push();
        Matrix_RotateY(angle, MTXMODE_APPLY);
        Matrix_RotateX(0.6f * ((i & 1) ? 1.0f : -1.0f), MTXMODE_APPLY);
        Matrix_RotateZ(angle * 0.5f, MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, ballScale * 1.6f, MTXMODE_APPLY);
        Matrix_Scale(ballScale * 0.115f, ballScale * 0.115f, ballScale * 0.032f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicRayDL);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// Ganondorf's big-magic ball tinted by element: flecks, backdrop, dot, the ball, and one ray per tier. Those DLs
// read their scrolls from segments 0x08 to 0x0A, so the order and the segment loads are verbatim.
static void DrawRodBall(PlayState* play, Vec3f pos, f32 circleScale, f32 ballScale, s32 rays, u8 element) {
    u8 e = element % ELEMENT_COUNT;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 frame = play->gameplayFrames;

    if (circleScale <= 0.001f) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Xlu(gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, sOrbPrim[e][0], sOrbPrim[e][1], sOrbPrim[e][2], 255);
    gDPSetEnvColor(POLY_XLU_DISP++, sOrbEnv[e][0], sOrbEnv[e][1], sOrbEnv[e][2], 128);
    Gfx* flecks =
        Gfx_TwoTexScrollEx(gfxCtx, 0, frame * -2, 0, 0x40, 0x40, 1, 0, frame * 0xA, 0x40, 0x40, -2, 0, 0, 0xA);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)flecks);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(circleScale, circleScale, circleScale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicFlecksDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, sOrbCore[e][0], sOrbCore[e][1], sOrbCore[e][2], 255);
    Gfx* circle = Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 0x20, 0x20, 1, 0, frame * -4, 0x20, 0x20, 0, 0, 0, -4);
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)circle);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicCircleDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, sOrbEnv[e][0], sOrbEnv[e][1], sOrbEnv[e][2], 255);
    Gfx* dot = Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 0x20, 0x20, 1, frame * 2, frame * -0x14, 0x40, 0x40, 0, 0, 2, -0x14);
    gSPSegment(POLY_XLU_DISP++, 0x0A, (uintptr_t)dot);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicDotDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, sOrbEnv[e][0], sOrbEnv[e][1], sOrbEnv[e][2], 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicMaterialDL);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(ballScale, ballScale, ballScale, MTXMODE_APPLY);
    Matrix_RotateZ((f32)frame * 0.14f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicBallDL);
    CLOSE_DISPS(gfxCtx);
    if (rays > 0) {
        DrawRayFan(play, pos, ballScale, rays, sOrbPrim[e], sOrbEnv[e]);
    }
}

// Ganondorf's lit streak: twelve tapering quads along the fragment's last path samples, matrices in segment 0x0D.
static void DrawOrbStreak(PlayState* play, RodOrb* orb) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Mtx* mtx = (Mtx*)Graph_Alloc(gfxCtx, ORB_TRAIL_DRAWN * sizeof(Mtx));
    const u8* env = sOrbEnv[orb->element % ELEMENT_COUNT];
    u8 alpha = orb->timer >= 8 ? 255 : (u8)((orb->timer * 255) / 8);

    if (mtx == NULL) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Xlu(gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, env[0], env[1], env[2], 128);
    gSPSegment(POLY_XLU_DISP++, 0x0D, (uintptr_t)mtx);
    for (s32 i = 0; i < ORB_TRAIL_DRAWN; i++) {
        s32 t = ((orb->trailIndex - i) + ORB_TRAIL_LEN) % ORB_TRAIL_LEN;
        Matrix_Translate(orb->trailPos[t].x, orb->trailPos[t].y, orb->trailPos[t].z, MTXMODE_NEW);
        Matrix_RotateY(orb->trailRot[t].y, MTXMODE_APPLY);
        Matrix_RotateX(-orb->trailRot[t].x, MTXMODE_APPLY);
        Matrix_Scale(ORB_STREAK_SCALE, ORB_STREAK_SCALE, ORB_STREAK_SCALE, MTXMODE_APPLY);
        Matrix_RotateY((f32)M_PI / 2.0f, MTXMODE_APPLY);
        Matrix_ToMtx(mtx, (char*)__FILE__, __LINE__);
        gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sStreakDL[i]);
        mtx++;
    }
    Matrix_Translate(orb->pos.x, orb->pos.y, orb->pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(6.0f, 6.0f, 6.0f, MTXMODE_APPLY);
    Matrix_RotateZ((f32)play->gameplayFrames * 0.2f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicMaterialDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBigMagicBallDL);
    CLOSE_DISPS(gfxCtx);
}

// The slingshot-style aim draws no crosshair, and Garo's first-person arms are hidden: four dots and a centre
// spread out as the shot charges.
static void DrawChargeAndReticle(PlayState* play, Player* player) {
    Vec3f head = player->actor.focus.pos;
    s16 yaw;
    s16 pitch;

    Z64Aiming_GetDirection(player, &yaw, &pitch);
    f32 cosPitch = Math_CosS(pitch);
    Vec3f forward = { Math_SinS(yaw) * cosPitch, -Math_SinS(pitch), Math_CosS(yaw) * cosPitch };
    Vec3f ball = { head.x + forward.x * ROD_BALL_AHEAD, head.y + forward.y * ROD_BALL_AHEAD,
                   head.z + forward.z * ROD_BALL_AHEAD };
    f32 pulse = 1.0f + 0.06f * Math_SinS(play->gameplayFrames * 0x1000);
    DrawRodBall(play, ball, sRodBallCircle * pulse, sRodBallScale * pulse, sRodBallRays, sRodElement);

    Vec3f center = { head.x + forward.x * RETICLE_DIST, head.y + forward.y * RETICLE_DIST,
                     head.z + forward.z * RETICLE_DIST };
    Vec3f right = { Math_SinS(yaw + 0x4000), 0.0f, Math_CosS(yaw + 0x4000) };
    Vec3f up = { right.y * forward.z - right.z * forward.y, right.z * forward.x - right.x * forward.z,
                 right.x * forward.y - right.y * forward.x };
    f32 spread = RETICLE_SPREAD * (1.0f + MIN((f32)sRodCharge / ROD_CHARGE_MAX, 1.0f) * 0.6f);
    DrawBillboardOrb(play, center, RETICLE_DOT_SCALE * 0.7f, sRodElement);
    for (s32 i = 0; i < 4; i++) {
        f32 offsetX = i == 0 ? spread : (i == 1 ? -spread : 0.0f);
        f32 offsetY = i == 2 ? spread : (i == 3 ? -spread : 0.0f);
        Vec3f dot = { center.x + right.x * offsetX + up.x * offsetY, center.y + right.y * offsetX + up.y * offsetY,
                      center.z + right.z * offsetX + up.z * offsetY };
        DrawBillboardOrb(play, dot, RETICLE_DOT_SCALE, sRodElement);
    }
}

static void DrawProjectiles(PlayState* play, Player* player) {
    if (sMove == GARO_MOVE_ROD_AIM) {
        DrawChargeAndReticle(play, player);
    }
    if (sMove == GARO_MOVE_BANISH_SHADOW) {
        f32 t = MIN((f32)sShadowTimer / (f32)BANISH_SHADOW_FRAMES, 1.0f);
        DrawShadowBall(play, sShadowPos, 3.4f + t * 1.6f);
    }
    for (s32 i = 0; i < ORB_POOL_MAX; i++) {
        RodOrb* orb = &sOrbs[i];
        if (!orb->isActive) {
            continue;
        }
        if (orb->isSeeker) {
            DrawOrbStreak(play, orb);
        } else {
            DrawRodBall(play, orb->pos, orb->ballCircle, orb->ballScale, BIG_MAGIC_RAYS_MAX, orb->element);
        }
    }
}

static void DrawGaro(PlayState* play, Player* player) {
    bool isBodyVisible =
        !(player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) && !(player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON);

    if (isBodyVisible) {
        DrawHybrid(play, player);
    }
    DrawProjectiles(play, player);
}

// ---- lifetime ----

static void ResetState(PlayState* play, Player* player) {
    if (player != NULL && play != NULL && IsGaroBusy(player)) {
        ClearMove(play, player);
    }
    sMove = GARO_MOVE_NONE;
    sIsParryPending = false;
    sPendingParry = NULL;
    sIsLaughPending = false;
    sEnemySnapshotCount = 0;
    sScaledDamage = 0;
    sBanishCooldown = 0;
    sRodCooldown = 0;
    ResetRodCharge();
    memset(sOrbs, 0, sizeof(sOrbs));
    sReflectedShot = NULL;
    sReflectTimer = 0;
}

static void EnterGaro(PlayState* play, Player* player) {
    ResetState(play, player);
    PrepareHybrid(play);
}

static void ExitGaro(PlayState* play, Player* player) {
    bool wasBusy = IsGaroBusy(player);

    ResetState(play, player);
    ConcealRevealed(play);
    SilenceChant();
    sIsOcarinaOut = false;
    if (wasBusy) {
        func_80839FFC(player, play);
    }
}

// Damage, a cutscene or the water can take the player from a move without passing through its end.
static void DropInterruptedMove(PlayState* play, Player* player) {
    if (sMove == GARO_MOVE_NONE || IsGaroBusy(player)) {
        return;
    }
    ClearMove(play, player);
}

static void UpdateGaro(PlayState* play, Player* player) {
    DropInterruptedMove(play, player);
    if (!WantsTrails() && sTrailLeft >= 0) {
        KillTrails(play);
    }
    if (sBanishCooldown > 0) {
        sBanishCooldown--;
    }
    if (sRodCooldown > 0) {
        sRodCooldown--;
    }
    UpdateOrbs(play, player);
    EscortReflectedShot(play, player);
    PoseHybrid(play, player);
    RevealHidden(play);
    UpdateOcarinaVoice(play, player);
    BurnOnDeath(play, player);
    WatchForKills(play);
    TryAirSlash(play, player);
    TryLaugh(play, player);
}

static void ForgetScene(int16_t sceneNum) {
    (void)sceneNum;
    sAreOrbQuadsReady = false;
    sIsReflectQuadReady = false;
    sIsHybridReady = false;
    sRevealedCount = 0;
    sTrailLeft = -1;
    sTrailRight = -1;
    Z64Aiming_Drop();
    ResetState(NULL, NULL);
}

// ---- mask ----

static bool CanWearMask(Player* player, PlayState* play) {
    return IsGrounded(player) &&
           !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_CUTSCENE));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(GARO_FORM_KEY);
}

// A light ninja's body, the Zora's grip: bushes and bomb flowers, nothing heavier.
static void ResolveGaroStrength(bool* should, va_list args) {
    int32_t* strength = va_arg(args, int32_t*);

    if (!IsGaro()) {
        return;
    }
    *strength = PLAYER_STR_BRACELET;
    *should = false;
}

// The Gerudo guards took Garo's Mask for one of their own while it was only worn, and still do.
static void PassAsGerudo(bool* should, va_list args) {
    (void)args;
    if (IsGaro()) {
        *should = true;
    }
}

static void RegisterMask(void) {
    SOHCustomItemDefinition mask = Z64Items_Define(GARO_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, GARO_MASK_PAGE, GARO_MASK_SLOT, 0);
    Z64Items_SetGetItemModel(&mask, sGetItemDL);
    Z64Items_SetTextbox(&mask, "You got %rGaro's Mask%w!&The face of the Garo, Ikana's ninja spies.^Wear it with "
                               "%y\xA1%w to become a %rGaro%w: twin blades, a guard that answers every blow, and a "
                               "rod of elemental magic.");
    Z64Items_SetPauseText(&mask, "%rGaro's Mask&%wPress %y\xA1%w to become a Garo.&%y\xA0%w: spin  Hold %y\xA0%w: rod "
                                 "(%y\xA2%w/%y\xA3%w element)  %y\x9F%w: dash  %y\xA3%w: guard");
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

static uint16_t ResolveGaroEquipment(int32_t equipType, uint16_t value) {
    if (equipType == EQUIP_TYPE_SWORD) {
        return EQUIP_VALUE_SWORD_NONE;
    }
    if (equipType == EQUIP_TYPE_SHIELD) {
        return EQUIP_VALUE_SHIELD_NONE;
    }
    return value;
}

static bool IsAllowedCustomItem(const char* customKey) {
    static const char* const sAllowed[] = {
        "nei.deku_form_mask", "nei.goron_form_mask",   "nei.zora_form_mask",   "nei.keaton_form_mask",
        "nei.mask_kafei",     "nei.fierce_deity_mask", "nei.gerudo_form_mask", "nei.whip",
        "nei.beetle",         "nei.switch_hook",       "nei.somaria",          "nei.rito_form_mask",
    };

    for (u32 i = 0; i < ARRAY_COUNT(sAllowed); i++) {
        if (strcmp(customKey, sAllowed[i]) == 0) {
            return true;
        }
    }
    return false;
}

// NEI's sSlotAllowedGaro: the ranged tools of a ninja spy, bottles and trade items.
static bool AllowsGaroItem(uint16_t item, const char* customKey) {
    if (customKey != NULL) {
        return IsAllowedCustomItem(customKey);
    }
    return item == ITEM_BOW || item == ITEM_ARROW_FIRE || item == ITEM_ARROW_ICE || item == ITEM_ARROW_LIGHT ||
           item == ITEM_OCARINA_FAIRY || item == ITEM_OCARINA_TIME || item == ITEM_HOOKSHOT || item == ITEM_LONGSHOT ||
           (item >= ITEM_BOTTLE && item <= ITEM_POE) || (item >= ITEM_WEIRD_EGG && item <= ITEM_CLAIM_CHECK);
}

static const SOHFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_MELEE, HandleSlash },        { SOH_PLAYER_ACTION_ROLL, HandleDash },
    { SOH_PLAYER_ACTION_ZTARGET_A, HandleZTargetA }, { SOH_PLAYER_ACTION_SHIELD, HandleGuard },
    { SOH_PLAYER_ACTION_RECEIVE_ITEM, RefuseCarry },
};

static void RegisterForm(void) {
    SOHFormDefinition garo = { 0 };

    garo.structSize = sizeof(garo);
    garo.key = GARO_FORM_KEY;
    garo.label = "Garo";
    garo.kind = SOH_FORM_KIND_LINK;
    garo.item = GARO_MASK_KEY;
    garo.motionScale = 1.0f;
    garo.resolveEquipment = ResolveGaroEquipment;
    garo.allowsButtonItem = AllowsGaroItem;
    garo.transformAnim = LoadMaskOnAnim();
    garo.transformMask = sMaskDL;
    garo.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    garo.actions = sActions;
    garo.actionCount = ARRAY_COUNT(sActions);
    garo.onEnter = EnterGaro;
    garo.onExit = ExitGaro;
    garo.update = UpdateGaro;
    garo.draw = DrawGaro;
    sApi->RegisterForm(&garo);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    LoadAnims();
    RegisterMask();
    RegisterForm();
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, HideLinkBody);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, HandleIncomingHit);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, SpeakWithIgosVoice);
    SOH_REGISTER_HOOK(sApi, OnBgCheckRaycastFloor, WalkOnWater);
    SOH_REGISTER_HOOK(sApi, OnActorDraw, HideFakes);
    SOH_REGISTER_HOOK(sApi, OnOcarinaNote, ChantNote);
    SOH_REGISTER_HOOK(sApi, OnOcarinaPlaybackNote, PlaybackChantNote);
    SOH_REGISTER_HOOK(sApi, OnCollisionResolveDamage, FixStrikeDamage);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    sApi->RegisterVB(VB_GERUDOS_BE_FRIENDLY, PassAsGerudo);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveGaroStrength);
}
