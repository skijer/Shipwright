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
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_En_Light/z_en_light.h"
#include "overlays/actors/ovl_Obj_Switch/z_obj_switch.h"

#define RITO_MASK_KEY "nei.rito_form_mask"
#define ROCS_FEATHER_KEY "nei.rocs_feather"
#define ROCS_CAPE_KEY "nei.rocs_cape"
#define ITEM_BUTTON_COUNT 8
#define RITO_FORM_KEY "nei.rito"
#define RITO_MODEL_PATH "objects/forms/rito"
// Rito shares Bremen's cell in the Majora mask grid.
#define RITO_MASK_PAGE 1
#define RITO_MASK_SLOT 7
#define RITO_MASK_WHEEL_PRIORITY (-1)

#define BG_ON_GROUND 1
#define ANIM_VALUES_PER_FRAME 67
#define MM_ANIM_BASE_TRANSL_X (-57)
#define JUMP_GRAVITY (-1.2f)
#define OCARINA_INSTRUMENT_OFF 0
#define OCARINA_INSTRUMENT_HARP 4
#define OCARINA_NOTE_NONE 0xFF
// LinkAnimation advances R_UPDATE_RATE * 0.5 clip frames per update, and the rate is always 3.
#define CLIP_FRAMES_PER_UPDATE 1.5f

// The rig is MM's human skeleton, rooted at 2376, so an adult clip drives it a third too high; a child one fits.
#define RITO_ROOT_SCALE_ADULT 0.7036f
#define RITO_ROOT_SCALE_CHILD 1.0f
#define RITO_HEIGHT 40.0f

// Logic frames at 20 Hz.
#define A_HOLD_FRAMES 6
#define CHARGE_MIN 6
#define CHARGE_FULL 24
#define LAUNCH_MIN 8.0f
#define LAUNCH_ROCS_MULTIPLE 4.0f
#define LAUNCH_DECAY 0.8f
#define LAUNCH_MAX_FRAMES 20
#define LAUNCH_MAGIC_COST 12
#define GLIDE_SPEED 4.5f
#define GLIDE_SINK (-0.35f)
#define GLIDE_TURN_RATE 5.0f
#define GLIDE_TURN_DEADZONE 10.0f
#define HOP_LAND_GUARD 4
#define HOP_ROLL_SCALE 1.5f
#define HOP_ROLL_SPEED 6.0f
#define HOP_SIDE_SPEED 8.5f
#define FLAP_SFX_PERIOD 8
#define CHARGE_FLAP_PERIOD 4
// The rocs mod stands down for a form, so a Rito's Roc's is its own: in the air it is unlimited, paid in magic.
#define ROCS_JUMP_VELOCITY 11.0f
#define ROCS_AIR_MAGIC_COST 12
#define ROCS_BOOST_FRAMES 16

#define CLIP_SPEED 2.0f
#define BOW_SPEED 1.25f
#define BOW_RELEASE_SPEED 1.5f
#define BOW_SEED_SPEED 2.0f
#define THROW_SPEED 3.0f
// Each clip's root is pinned to its own first frame, so the ones authored lower or higher than the set are
// brought back to the feet here, in model units.
#define LAND_LIFT 1500.0f
#define BOW_SEED_LIFT 1500.0f
#define BOW_RELEASE_LIFT (-300.0f)

#define BOW_VOLLEY 3
#define BOW_TWIN_SHOTS 2
#define BOW_LOOSE_FRAME 3
#define BOW_LOOSE_LEAD 5.0f
#define BOW_RANGE 1400.0f
#define BOW_CONE 0x1555
#define BOW_TURN 0x1000
#define BOW_ARROW_SPEED 150.0f
// EnArrow travels 150 units a frame, so a tighter arrival sphere is jumped clean over.
#define BOW_HIT_DIST 170.0f
#define BOW_SNAP_DIST 400.0f
#define BOW_MOVE_SPEED 5.0f
#define BOW_MOVE_DEADZONE 10.0f
#define BOW_TURN_RATE 0x0C00
#define BOW_ICE_RADIUS 90.0f
// EnArrow_Shoot kills a parentless arrow unless player->unk_A73 is still counting down.
#define BOW_HANDSHAKE 4
#define BOW_FAN_STEP 0x0720
#define BOW_TAP_FRAMES 5
#define BOW_CHARGE_TWIN 20
#define BOW_HOP_SPEED 7.0f
#define BOW_HOP_DECAY 0.5f
#define BOW_DODGE_IFRAMES (-10)

#define SINK_DEPTH 30.0f
#define SINK_START (-2.0f)
#define SINK_GRAVITY (-4.0f)
#define SINK_FRAMES 9

// The shield plate is built at the Deku shield's footprint, so it only needs that DL's centre in each limb.
static const Vec3f sShieldInHand = { -20.0f, 115.5f, -145.5f };
static const Vec3f sShieldOnBack = { 608.0f, 10.0f, -142.5f };
#define HARP_SCALE 0.83f
static const Vec3f sHarpOffset = { -47.62f, 107.14f, 0.0f };
static const Vec3s sHarpRot = { -26527, -25356, -18205 };
#define BOW_STRING_Y (-360.4f)

// ---- updraft ----

// Tornado_GetAxis: axis.y = -sin(pitch), so straight up is -0x4000.
#define WIND_PITCH_UP (-0x4000)
#define WIND_HEIGHT 82.0f
#define WIND_RADIUS 30.0f
#define WIND_SPIN 0x0900
#define WIND_SCROLL 18
#define WIND_RIBBONS 5
#define WIND_RIBBON_MAX 6
#define WIND_RIBBON_SPREAD 2.0f
#define WIND_RIBBON_TURNS 0x18000
#define WIND_RIBBON_STEP (1.0f / 14.0f)
#define WIND_ALPHA 70
#define WIND_GROW_MIN 0.35f
#define WIND_FADE_FRAMES 14.0f
#define WIND_MOTES 8
#define WIND_MOTES_KEPT 4
#define WIND_BURST_FRAMES 7
#define WIND_MOTE_RADIUS 17.0f
#define WIND_MOTE_BURST_RADIUS 40.0f
#define WIND_MOTE_HEIGHT 12.0f
#define WIND_MOTE_DRIFT 0x0700
// En_Light's green flame; bit 15 would be the candle variant, drawn orange whatever the index says.
#define WIND_LIGHT_PARAMS 6
#define WIND_LIGHT_SCALE 0.0010f
#define WIND_QUAKE_SPEED 32000
#define WIND_QUAKE_AMPLITUDE 6
#define WIND_QUAKE_FRAMES 11
#define WIND_LOOP_SFX (NA_SE_EV_WIND_TRAP - SFX_FLAG)
// The mesh is baked tip at the origin, mouth toward +Y, and its texture is intensity only.
#define TORNADO_MODEL_LENGTH 100.0f
#define TORNADO_MODEL_RADIUS 47.0f
#define TORNADO_TEX_WIDTH 32
#define TORNADO_TEX_HEIGHT 64

#define ANIM_PATH(name) "__OTR__misc/link_animetion/" name
#define GLAIVE(name) ANIM_PATH("gMonsterHunterRise_InsectGlaive_" name)
#define BOW(name) ANIM_PATH("gPlayerAnim_mhr_bow_" name)

static const u16 sItemButtons[ITEM_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                     BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconRitoMaskTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gRitoMaskNameTex";
static const ALIGN_ASSET(2) char sMaskOnPath[] = ANIM_PATH("gPlayerAnim_cl_setmask_Data");
static const ALIGN_ASSET(2) char sShieldDL[] = "__OTR__objects/object_nei_rito_shield/gRitoShieldDL";
static const ALIGN_ASSET(2) char sBowDL[] = "__OTR__objects/object_nei_rito_bow/gRitoBowDL";
static const ALIGN_ASSET(2) char sBowStringDL[] = "__OTR__objects/object_link_boy/gLinkAdultBowStringDL";
static const ALIGN_ASSET(2) char sHarpDL[] = "__OTR__objects/object_xc/gSheikHarpDL";
static const ALIGN_ASSET(2) char sTornadoMatDL[] = "__OTR__objects/object_nei_tornado/mat_tornado_f3dlite_tornado";
static const ALIGN_ASSET(2) char sTornadoTriDL[] = "__OTR__objects/object_nei_tornado/tornado_mesh_tri_0";
static const ALIGN_ASSET(2) char sOpenHandDL[] = "__OTR__" RITO_MODEL_PATH "/object_link_boy/gLinkAdultRightHandNearDL";
static const ALIGN_ASSET(2) char sClosedHandDL[] = "__OTR__" RITO_MODEL_PATH
                                                   "/object_link_boy/gLinkAdultRightHandClosedNearDL";
static const ALIGN_ASSET(2) char sOpenLeftHandDL[] = "__OTR__" RITO_MODEL_PATH
                                                     "/object_link_boy/gLinkAdultLeftHandNearDL";
static const ALIGN_ASSET(2) char sClosedLeftHandDL[] = "__OTR__" RITO_MODEL_PATH
                                                       "/object_link_boy/gLinkAdultLeftHandClosedNearDL";

typedef enum {
    CLIP_LAUNCH,
    CLIP_FLY,
    CLIP_LAND,
    CLIP_HOP_FRONT,
    CLIP_HOP_LEFT,
    CLIP_HOP_BACK,
    CLIP_HOP_RIGHT,
    CLIP_HOP_SETTLE,
    CLIP_THROW,
    CLIP_BOW_AIR,
    CLIP_BOW_ENTER_STILL,
    CLIP_BOW_HOLD_STILL,
    CLIP_BOW_ENTER_MOVE,
    CLIP_BOW_HOLD_MOVE,
    CLIP_BOW_RELEASE,
    CLIP_BOW_SEED,
    CLIP_BOW_AIR_TAP,
    CLIP_HARP_RAISE,
    CLIP_HARP_PLAY,
    CLIP_COUNT,
} RitoClip;

typedef struct {
    const char* path;
    f32 speed;
    bool isRootPinned;
} ClipSource;

// Hops index by OoT's controlStickDirection: front, side-left, back, side-right. The side pair reads better
// mirrored, so a clip's name does not always match its hop.
static const ClipSource sClipSources[CLIP_COUNT] = {
    { GLAIVE("BackwardRisingDoubleChargedStaffCombo"), CLIP_SPEED, true },
    { ANIM_PATH("gPlayerAnim_mhr_npc_takkuri_fly"), CLIP_SPEED, true },
    { GLAIVE("ForwardSingleAdvancingStaffSweep"), CLIP_SPEED, true },
    { GLAIVE("ForwardRisingMultiHitAerialStaffStrike_Variant08"), CLIP_SPEED, true },
    { GLAIVE("RightHighAerialDoubleSilkbindStaffStrike"), CLIP_SPEED, true },
    { GLAIVE("BackwardHighAerialMultiHitSilkbindStaffStrike_Variant06"), CLIP_SPEED, true },
    { GLAIVE("LeftHighAerialSingleSilkbindStaffStrike"), CLIP_SPEED, true },
    { GLAIVE("StationaryStaffReadyIdle_Variant10"), CLIP_SPEED, true },
    { GLAIVE("ForwardDoubleStaffStrike"), THROW_SPEED, true },
    { BOW("charge_attack09"), BOW_SPEED, true },
    { BOW("motion12"), BOW_SPEED, true },
    { BOW("idle04_loop"), BOW_SPEED, true },
    { BOW("dash_attack07"), BOW_SPEED, true },
    { BOW("run07_loop"), BOW_SPEED, true },
    { BOW("dash_attack13"), BOW_RELEASE_SPEED, true },
    { BOW("charge_attack11"), BOW_SEED_SPEED, true },
    { BOW("attack04"), BOW_SPEED, true },
    { ANIM_PATH("gPlayerAnim_mhr_npc_sheik_pulling_out_harp"), 1.0f, false },
    { ANIM_PATH("gPlayerAnim_mhr_npc_sheik_playing_harp"), 1.0f, false },
};

typedef enum {
    RITO_MOVE_NONE,
    RITO_MOVE_CHARGE,
    RITO_MOVE_LAUNCH,
    RITO_MOVE_GLIDE,
    RITO_MOVE_HOP,
    RITO_MOVE_THROW,
    RITO_MOVE_BOW_ENTER,
    RITO_MOVE_BOW_HOLD,
    RITO_MOVE_BOW_RELEASE,
    RITO_MOVE_BOW_SEED,
    RITO_MOVE_BOW_AIR,
    RITO_MOVE_BOW_AIR_TAP,
    RITO_MOVE_SINK,
} RitoMove;

typedef enum {
    HARP_IDLE,
    HARP_RAISING,
    HARP_PLAYING,
    HARP_LOWERING,
} HarpPhase;

typedef struct {
    Actor* arrow;
    Actor* target;
    u8 targetCategory;
    Vec3f goal;
} HomingArrow;

typedef struct {
    Vec3f origin;
    f32 length;
    f32 radius;
    Color_RGBA8 color;
    s16 spin;
    s16 scrollT;
} WindCone;

typedef struct {
    WindCone cone;
    s32 ribbons[WIND_RIBBON_MAX];
    f32 ribbonPhase[WIND_RIBBON_MAX];
    bool areRibbonsOn;
    Actor* motes[WIND_MOTES];
    f32 motePhase[WIND_MOTES];
    f32 fade;
    f32 grow;
    s32 frames;
    bool isOn;
} Updraft;

static const char* const sRequiredHooks[] = {
    "OnPlayerActionHandler", "OnPlayerResolveAnim", "OnPlayerResolveLimbDraw", "OnPlayerPostLimbDraw", "OnOcarinaNote",
    "OnOcarinaPlaybackNote", "OnSceneInit",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
void Player_ZeroSpeedXZ(Player* player);
s32 Player_UpdateUpperBody(Player* player, PlayState* play);
int Player_IsZTargeting(Player* player);

static const SOHModApi* sApi;
static LinkAnimationHeader sClips[CLIP_COUNT];
static s16* sClipData[CLIP_COUNT];
static bool sAreClipsLoaded;
static LinkAnimationHeader sMaskOnAnim;
static s16* sMaskOnFrames;

static u8 sMove;
static s16 sMoveFrames;
static s16 sCharge;
static s16 sGlideYaw;
static bool sIsHopArmed;
static bool sIsAPending;
static s16 sAFrames;
static s16 sBoostFrames;
static bool sIsThrowPending;
static bool sHasLoosed;
static bool sIsBowMoving;
static Updraft sUpdraft;
static HomingArrow sArrows[BOW_VOLLEY];

static u8 sHarpPhase;
static bool sIsOcarinaOut;
static bool sWasNoteStruck;
static u8 sLastNote = OCARINA_NOTE_NONE;

static void RitoAction(Player* player, PlayState* play);
static bool TryAirBow(PlayState* play, Player* player);

static bool IsRito(void) {
    return sApi != NULL && sApi->IsFormActive(RITO_FORM_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BG_ON_GROUND) != 0;
}

static bool IsRitoBusy(Player* player) {
    return player->actionFunc == RitoAction;
}

static bool IsPressed(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].press.button, button);
}

static bool IsHeld(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].cur.button, button);
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 &
             (PLAYER_STATE1_DEAD | PLAYER_STATE1_LOADING | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM |
              PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER |
              PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_ITEM_CS |
              PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED | PLAYER_STATE1_CLIMBING_LADDER |
              PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_SHIELDING));
}

static bool IsFlightMove(void) {
    return sMove == RITO_MOVE_GLIDE || sMove == RITO_MOVE_LAUNCH || sMove == RITO_MOVE_HOP;
}

static bool IsBowOut(void) {
    return sMove >= RITO_MOVE_BOW_ENTER && sMove <= RITO_MOVE_BOW_AIR_TAP;
}

static f32 GetRocsVelocity(void) {
    return LINK_IS_ADULT ? 7.5f : 7.0f;
}

// ---- clips ----

// Resampling to fewer frames is what plays an MHR clip at its speed; its root is pinned to its first frame.
static void LoadClip(RitoClip id) {
    const ClipSource* source = &sClipSources[id];
    LinkAnimationHeader* raw =
        ResourceMgr_FileExists(source->path) ? ResourceMgr_LoadPlayerAnimAsHeader(source->path) : NULL;

    if (raw == NULL || raw->common.frameCount <= 0) {
        return;
    }
    s32 count = raw->common.frameCount;
    s32 frames = MAX((s32)((f32)count / source->speed + 0.5f), 2);
    s16* data = (s16*)malloc((size_t)frames * ANIM_VALUES_PER_FRAME * sizeof(s16));
    if (data == NULL) {
        return;
    }
    const s16* from = (const s16*)raw->segment;
    for (s32 f = 0; f < frames; f++) {
        s32 sourceFrame = MIN((f * count) / frames, count - 1);
        memcpy(&data[f * ANIM_VALUES_PER_FRAME], &from[sourceFrame * ANIM_VALUES_PER_FRAME],
               ANIM_VALUES_PER_FRAME * sizeof(s16));
    }
    for (s32 f = 1; f < frames; f++) {
        data[f * ANIM_VALUES_PER_FRAME + 0] = data[0];
        data[f * ANIM_VALUES_PER_FRAME + 2] = data[2];
        if (source->isRootPinned) {
            data[f * ANIM_VALUES_PER_FRAME + 1] = data[1];
        }
    }
    sClipData[id] = data;
    sClips[id].common.frameCount = (s16)frames;
    sClips[id].segment = data;
}

static void LoadClips(void) {
    if (sAreClipsLoaded) {
        return;
    }
    sAreClipsLoaded = true;
    for (s32 id = 0; id < CLIP_COUNT; id++) {
        LoadClip((RitoClip)id);
    }
}

static LinkAnimationHeader* GetClip(RitoClip id) {
    return sClipData[id] != NULL ? &sClips[id] : NULL;
}

static bool IsPlaying(Player* player, RitoClip id) {
    return sClipData[id] != NULL && player->skelAnime.animation == &sClips[id];
}

static void PlayClip(PlayState* play, Player* player, RitoClip id, u8 mode, f32 morph) {
    LinkAnimationHeader* clip = GetClip(id);

    if (clip == NULL) {
        return;
    }
    LinkAnimation_Change(play, &player->skelAnime, clip, 1.0f, 0.0f, Animation_GetLastFrame(clip), mode, morph);
}

// OoT's wings-up wait is the vanilla bow guard, played as it is: a vanilla clip is never resampled.
static void PlayChargePose(PlayState* play, Player* player) {
    LinkAnimationHeader* pose = (LinkAnimationHeader*)gPlayerAnim_link_bow_defense_wait;

    LinkAnimation_Change(play, &player->skelAnime, pose, 1.0f, 0.0f, Animation_GetLastFrame(pose), ANIMMODE_LOOP,
                         -4.0f);
}

// ---- updraft ----

// Its only job is to stay still: En_Light's update is what plays the torch sound and re-lights the floor disc.
static void HoldMote(Actor* thisx, PlayState* play) {
    (void)thisx;
    (void)play;
}

static Actor* SpawnMote(PlayState* play, Vec3f* origin) {
    Actor* mote =
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, origin->x, origin->y, origin->z, 0, 0, 0, WIND_LIGHT_PARAMS);

    if (mote == NULL) {
        return NULL;
    }
    Actor_SetScale(mote, WIND_LIGHT_SCALE);
    Lights_PointNoGlowSetInfo(&((EnLight*)mote)->lightInfo, (s16)origin->x, (s16)origin->y, (s16)origin->z, 0, 0, 0, 0);
    mote->update = HoldMote;
    return mote;
}

static void StartRibbons(PlayState* play) {
    EffectBlureInit1 init;
    const Color_RGBA8* color = &sUpdraft.cone.color;

    memset(&init, 0, sizeof(init));
    init.p1StartColor[0] = color->r;
    init.p1StartColor[1] = color->g;
    init.p1StartColor[2] = color->b;
    init.p1StartColor[3] = 255;
    init.p2StartColor[0] = color->r / 2;
    init.p2StartColor[1] = color->g / 2;
    init.p2StartColor[2] = color->b / 2;
    init.p1EndColor[0] = color->r;
    init.p1EndColor[1] = color->g;
    init.p1EndColor[2] = color->b;
    init.p2EndColor[0] = color->r / 2;
    init.p2EndColor[1] = color->g / 2;
    init.p2EndColor[2] = color->b / 2;
    init.elemDuration = 8;
    init.calcMode = 2;
    for (s32 i = 0; i < WIND_RIBBONS; i++) {
        sUpdraft.ribbons[i] = -1;
        Effect_Add(play, &sUpdraft.ribbons[i], EFFECT_BLURE1, 0, 0, &init);
        sUpdraft.ribbonPhase[i] = (f32)i / WIND_RIBBONS;
    }
    sUpdraft.areRibbonsOn = true;
}

static void StopRibbons(PlayState* play) {
    if (!sUpdraft.areRibbonsOn) {
        return;
    }
    for (s32 i = 0; i < WIND_RIBBONS; i++) {
        if (sUpdraft.ribbons[i] >= 0) {
            Effect_Delete(play, sUpdraft.ribbons[i]);
            sUpdraft.ribbons[i] = -1;
        }
    }
    sUpdraft.areRibbonsOn = false;
}

// Each streak is a blure fed along a parabolic spiral: it leaves the tip and flares out as it climbs.
static void FeedRibbons(PlayState* play) {
    f32 radius = sUpdraft.cone.radius * WIND_RIBBON_SPREAD;
    f32 width = radius * 0.16f + 3.0f;

    if (!sUpdraft.areRibbonsOn) {
        StartRibbons(play);
    }
    for (s32 i = 0; i < WIND_RIBBONS; i++) {
        f32 t = sUpdraft.ribbonPhase[i];
        s16 angle = (s16)((s32)sUpdraft.cone.spin + i * (0x10000 / WIND_RIBBONS) + (s32)(t * WIND_RIBBON_TURNS));
        f32 flare = radius * t * t;
        Vec3f base = sUpdraft.cone.origin;
        base.x += Math_CosS(angle) * flare;
        base.y += sUpdraft.cone.length * t;
        base.z += Math_SinS(angle) * flare;
        Vec3f tip = base;
        tip.y += width;
        EffectBlure* blure = sUpdraft.ribbons[i] >= 0 ? Effect_GetByIndex(sUpdraft.ribbons[i]) : NULL;
        if (blure != NULL) {
            EffectBlure_AddVertex(blure, &tip, &base);
        }
        sUpdraft.ribbonPhase[i] = t + WIND_RIBBON_STEP >= 1.0f ? t + WIND_RIBBON_STEP - 1.0f : t + WIND_RIBBON_STEP;
    }
}

static void StopUpdraft(PlayState* play) {
    sUpdraft.isOn = false;
    StopRibbons(play);
    for (s32 i = 0; i < WIND_MOTES; i++) {
        if (sUpdraft.motes[i] != NULL) {
            Actor_Kill(sUpdraft.motes[i]);
            sUpdraft.motes[i] = NULL;
        }
    }
}

// A scene change already destroyed every mote and effect: forgetting them is all that is left to do.
static void ForgetUpdraft(void) {
    memset(&sUpdraft, 0, sizeof(sUpdraft));
}

static void StartUpdraft(PlayState* play, Vec3f* origin) {
    if (sUpdraft.isOn) {
        return;
    }
    sUpdraft.isOn = true;
    sUpdraft.frames = 0;
    sUpdraft.fade = 1.0f;
    sUpdraft.grow = WIND_GROW_MIN;
    sUpdraft.cone.origin = *origin;
    for (s32 i = 0; i < WIND_MOTES; i++) {
        sUpdraft.motePhase[i] = (f32)i * (2.0f * M_PI / WIND_MOTES) + Rand_ZeroFloat(0.6f);
        sUpdraft.motes[i] = SpawnMote(play, origin);
    }
    s16 quake = Quake_Add(Play_GetCamera(play, 0), 3);
    Quake_SetSpeed(quake, WIND_QUAKE_SPEED);
    Quake_SetQuakeValues(quake, WIND_QUAKE_AMPLITUDE, 0, 0, 0);
    Quake_SetCountdown(quake, WIND_QUAKE_FRAMES);
}

static void OrbitMotes(bool isBursting) {
    for (s32 i = 0; i < WIND_MOTES; i++) {
        Actor* mote = sUpdraft.motes[i];
        if (mote == NULL) {
            continue;
        }
        if (!isBursting && i >= WIND_MOTES_KEPT) {
            Actor_Kill(mote);
            sUpdraft.motes[i] = NULL;
            continue;
        }
        f32 angle = sUpdraft.motePhase[i] + BINANG_TO_RAD(WIND_MOTE_DRIFT) * sUpdraft.frames;
        f32 radius = isBursting ? WIND_MOTE_BURST_RADIUS : WIND_MOTE_RADIUS;
        mote->world.pos = sUpdraft.cone.origin;
        mote->world.pos.x += Math_SinF(angle) * radius;
        mote->world.pos.z += Math_CosF(angle) * radius;
        mote->world.pos.y += WIND_MOTE_HEIGHT;
        Actor_SetScale(mote, WIND_LIGHT_SCALE * sUpdraft.fade);
    }
}

// The column only holds while it is being built; once the Rito rides it off the ground it dies away.
static void TickUpdraft(PlayState* play, bool isHolding, f32 fill) {
    if (!sUpdraft.isOn) {
        return;
    }
    bool isBursting = sUpdraft.frames < WIND_BURST_FRAMES;
    sUpdraft.frames++;
    if (isHolding) {
        sUpdraft.fade = 1.0f;
        sUpdraft.grow = WIND_GROW_MIN + (1.0f - WIND_GROW_MIN) * CLAMP(fill, 0.0f, 1.0f);
    } else {
        sUpdraft.fade -= 1.0f / WIND_FADE_FRAMES;
        if (sUpdraft.fade <= 0.0f) {
            StopUpdraft(play);
            return;
        }
    }
    sUpdraft.cone.length = WIND_HEIGHT * sUpdraft.grow;
    sUpdraft.cone.radius = WIND_RADIUS * sUpdraft.grow;
    sUpdraft.cone.color.r = 190;
    sUpdraft.cone.color.g = 255;
    sUpdraft.cone.color.b = 200;
    sUpdraft.cone.color.a = (u8)(WIND_ALPHA * sUpdraft.fade);
    sUpdraft.cone.spin += WIND_SPIN;
    sUpdraft.cone.scrollT = (s16)((sUpdraft.cone.scrollT + WIND_SCROLL) % (TORNADO_TEX_HEIGHT * 4));
    Audio_PlaySoundGeneral(WIND_LOOP_SFX, &sUpdraft.cone.origin, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    FeedRibbons(play);
    OrbitMotes(isBursting);
}

// The column stands on its tip; the matrix is built fresh, so it is pushed to leave the caller's untouched.
static void DrawUpdraft(PlayState* play) {
    if (!sUpdraft.isOn || !ResourceMgr_FileExists(sTornadoMatDL) || !ResourceMgr_FileExists(sTornadoTriDL)) {
        return;
    }
    const WindCone* cone = &sUpdraft.cone;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(cone->origin.x, cone->origin.y, cone->origin.z, MTXMODE_NEW);
    Matrix_RotateX(DEG_TO_RAD(90.0f) + BINANG_TO_RAD(WIND_PITCH_UP), MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(cone->spin), MTXMODE_APPLY);
    Matrix_Scale(cone->radius / TORNADO_MODEL_RADIUS, cone->length / TORNADO_MODEL_LENGTH,
                 cone->radius / TORNADO_MODEL_RADIUS, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoMatDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, cone->color.r, cone->color.g, cone->color.b, cone->color.a);
    gSPDisplayList(POLY_XLU_DISP++,
                   Gfx_TexScroll(play->state.gfxCtx, 0, (u32)cone->scrollT, TORNADO_TEX_WIDTH, TORNADO_TEX_HEIGHT));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTornadoTriDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- moves ----

static void StartMove(PlayState* play, Player* player, u8 move) {
    if (player->actionFunc != RitoAction) {
        Player_SetupAction(play, player, RitoAction, 0);
    }
    sMove = move;
    sMoveFrames = 0;
    sBoostFrames = 0;
    sIsAPending = false;
}

static void ClearMove(Player* player) {
    sMove = RITO_MOVE_NONE;
    sMoveFrames = 0;
    sBoostFrames = 0;
    player->actor.gravity = JUMP_GRAVITY;
    player->actor.minVelocityY = -20.0f;
}

// Airborne, the idle left behind is read by func_8083AA10 as a step off an edge and becomes OoT's own fall.
static void EndMove(PlayState* play, Player* player) {
    ClearMove(player);
    func_80839FFC(player, play);
}

// Player_SetupAction clears MIDAIR, and without it func_8083AA10 swaps an airborne move for the fall.
static void KeepAirborne(Player* player) {
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
}

static void TakeOff(PlayState* play, Player* player, u8 move) {
    StartMove(play, player, move);
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    KeepAirborne(player);
    Camera_ChangeMode(GET_ACTIVE_CAM(play), CAM_MODE_JUMP);
    sGlideYaw = player->actor.shape.rot.y;
}

static bool SpendLaunchMagic(void) {
    if (gSaveContext.magicCapacity <= 0 || gSaveContext.magic < LAUNCH_MAGIC_COST) {
        return false;
    }
    gSaveContext.magic -= LAUNCH_MAGIC_COST;
    return true;
}

static void StartGlide(PlayState* play, Player* player) {
    if (GetClip(CLIP_FLY) == NULL) {
        return;
    }
    TakeOff(play, player, RITO_MOVE_GLIDE);
    PlayClip(play, player, CLIP_FLY, ANIMMODE_LOOP, -4.0f);
}

static void StartCharge(PlayState* play, Player* player) {
    StartMove(play, player, RITO_MOVE_CHARGE);
    sCharge = 0;
    player->linearVelocity = 0.0f;
    PlayChargePose(play, player);
    StartUpdraft(play, &player->actor.world.pos);
}

static void Launch(PlayState* play, Player* player) {
    f32 fill = MIN((f32)sCharge / CHARGE_FULL, 1.0f);
    f32 top = GetRocsVelocity() * LAUNCH_ROCS_MULTIPLE;

    TakeOff(play, player, RITO_MOVE_LAUNCH);
    player->actor.velocity.y = LAUNCH_MIN + (top - LAUNCH_MIN) * fill;
    player->actor.gravity = 0.0f;
    PlayClip(play, player, CLIP_LAUNCH, ANIMMODE_ONCE, -4.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_PL_ROLL);
}

static void RunCharge(PlayState* play, Player* player) {
    sCharge++;
    player->linearVelocity = 0.0f;
    LinkAnimation_Update(play, &player->skelAnime);
    if (sCharge % CHARGE_FLAP_PERIOD == 0) {
        Audio_PlayActorSound2(&player->actor, NA_SE_EN_KAICHO_FLUTTER);
    }
    // A full column lets go by itself: there is no sitting on a charged updraft.
    if (IsHeld(play, BTN_A) && sCharge < CHARGE_FULL) {
        return;
    }
    if (sCharge < CHARGE_MIN) {
        EndMove(play, player);
        return;
    }
    if (!SpendLaunchMagic()) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        EndMove(play, player);
        return;
    }
    Launch(play, player);
}

// The upper body is what reads the item buttons: flying keeps it running, as OoT's own fall does.
static void RunLaunch(PlayState* play, Player* player) {
    if (TryAirBow(play, player)) {
        return;
    }
    KeepAirborne(player);
    Player_UpdateUpperBody(player, play);
    LinkAnimation_Update(play, &player->skelAnime);
    player->actor.velocity.y -= LAUNCH_DECAY;
    if (player->actor.velocity.y > 1.0f && ++sMoveFrames <= LAUNCH_MAX_FRAMES) {
        return;
    }
    if (IsHeld(play, BTN_A)) {
        StartGlide(play, player);
    } else {
        EndMove(play, player);
    }
}

// Steering is a slow bank and nothing else: forward only, level, sinking gently; altitude comes from the launch.
static void SteerGlide(PlayState* play, Player* player) {
    f32 stickX = play->state.input[0].rel.stick_x;

    if (fabsf(stickX) > GLIDE_TURN_DEADZONE) {
        sGlideYaw -= (s16)(stickX * GLIDE_TURN_RATE);
    }
    player->actor.world.rot.y = sGlideYaw;
    player->actor.shape.rot.y = sGlideYaw;
    player->yaw = sGlideYaw;
    player->linearVelocity = GLIDE_SPEED;
    player->actor.gravity = 0.0f;
}

// A Roc's used mid-glide climbs on the item's own velocity with the launch clip, then settles back level.
static void RiseOnBoost(PlayState* play, Player* player) {
    player->actor.velocity.y -= LAUNCH_DECAY;
    if (--sBoostFrames > 0 && player->actor.velocity.y > GLIDE_SINK) {
        return;
    }
    sBoostFrames = 0;
    PlayClip(play, player, CLIP_FLY, ANIMMODE_LOOP, -4.0f);
}

static void RunGlide(PlayState* play, Player* player) {
    if (IsGrounded(player)) {
        Audio_PlayActorSound2(&player->actor, NA_SE_PL_LAND);
        EndMove(play, player);
        return;
    }
    if (!IsHeld(play, BTN_A)) {
        EndMove(play, player);
        return;
    }
    if (TryAirBow(play, player)) {
        return;
    }
    KeepAirborne(player);
    SteerGlide(play, player);
    if (sBoostFrames > 0) {
        RiseOnBoost(play, player);
    } else {
        player->actor.velocity.y = GLIDE_SINK;
    }
    Player_UpdateUpperBody(player, play);
    LinkAnimation_Update(play, &player->skelAnime);
    if (++sMoveFrames % FLAP_SFX_PERIOD == 0) {
        Audio_PlayActorSound2(&player->actor, NA_SE_EN_KAICHO_FLUTTER);
    }
}

// Roll, side hops and the backflip become Roc's-Feather hops: the roll goes one and a half high, the rest one.
static void StartHop(PlayState* play, Player* player, s32 direction) {
    bool isRoll = direction == PLAYER_STICK_DIR_FORWARD;

    TakeOff(play, player, RITO_MOVE_HOP);
    player->actor.velocity.y = GetRocsVelocity() * (isRoll ? HOP_ROLL_SCALE : 1.0f);
    player->actor.gravity = JUMP_GRAVITY;
    player->linearVelocity = 0.0f;
    if (isRoll || (direction & 1)) {
        player->yaw = player->actor.shape.rot.y + (isRoll ? 0 : (direction << 0xE));
        player->linearVelocity = isRoll ? HOP_ROLL_SPEED : HOP_SIDE_SPEED;
        player->actor.world.rot.y = player->yaw;
    }
    sGlideYaw = player->yaw;
    sIsHopArmed = false;
    PlayClip(play, player, (RitoClip)(CLIP_HOP_FRONT + (direction & 3)), ANIMMODE_ONCE, -4.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_PL_SKIP);
}

// A starts every hop, so it only turns one into flight after it has been let go of once.
static void RunHop(PlayState* play, Player* player) {
    bool isAHeld = IsHeld(play, BTN_A);

    sIsHopArmed = sIsHopArmed || !isAHeld;
    if (sIsHopArmed && isAHeld && GetClip(CLIP_FLY) != NULL) {
        StartGlide(play, player);
        return;
    }
    if (IsGrounded(player) && ++sMoveFrames > HOP_LAND_GUARD) {
        Audio_PlayActorSound2(&player->actor, NA_SE_PL_LAND);
        EndMove(play, player);
        return;
    }
    if (TryAirBow(play, player)) {
        return;
    }
    KeepAirborne(player);
    player->yaw = sGlideYaw;
    Player_UpdateUpperBody(player, play);
    bool isStrikeDone = LinkAnimation_Update(play, &player->skelAnime);
    if (isStrikeDone && !IsPlaying(player, CLIP_HOP_SETTLE)) {
        PlayClip(play, player, CLIP_HOP_SETTLE, ANIMMODE_LOOP, -4.0f);
    }
}

static void SpawnRocsSparkles(PlayState* play, Player* player) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 255, 255, 200, 255 };
    Color_RGBA8 env = { 200, 200, 100, 0 };

    for (s32 i = 0; i < 5; i++) {
        s16 angle = (s16)(Rand_ZeroOne() * 0xFFFF);
        f32 distance = 5.0f + Rand_ZeroOne() * 5.0f;
        Vec3f pos = { player->actor.world.pos.x + Math_SinS(angle) * distance, player->actor.world.pos.y + 5.0f,
                      player->actor.world.pos.z + Math_CosS(angle) * distance };
        EffectSsKiraKira_SpawnSmall(play, &pos, &zero, &zero, &prim, &env);
    }
}

static u16 FindRocsButtons(void) {
    u16 buttons = 0;

    if (!SOH_MOD_API_HAS(sApi, GetEquippedCustomItem)) {
        return 0;
    }
    for (u8 button = 0; button < ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && (strcmp(equipped, ROCS_FEATHER_KEY) == 0 || strcmp(equipped, ROCS_CAPE_KEY) == 0)) {
            buttons |= sItemButtons[button];
        }
    }
    return buttons;
}

static void RocsJump(PlayState* play, Player* player) {
    TakeOff(play, player, RITO_MOVE_HOP);
    player->actor.velocity.y = ROCS_JUMP_VELOCITY;
    player->actor.gravity = JUMP_GRAVITY;
    sGlideYaw = player->yaw;
    sIsHopArmed = false;
    PlayClip(play, player, CLIP_HOP_FRONT, ANIMMODE_ONCE, -4.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_PL_SKIP);
    SpawnRocsSparkles(play, player);
}

// From the ground it is the item's own jump; in the air every use is another, and each costs magic.
static void TryRocs(PlayState* play, Player* player) {
    u16 buttons = FindRocsButtons();

    if (buttons == 0 || !(play->state.input[0].press.button & buttons) || (IsRitoBusy(player) && !IsFlightMove()) ||
        !CanAct(player)) {
        return;
    }
    if (IsGrounded(player)) {
        RocsJump(play, player);
        return;
    }
    if (gSaveContext.magicCapacity <= 0 || gSaveContext.magic < ROCS_AIR_MAGIC_COST) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return;
    }
    gSaveContext.magic -= ROCS_AIR_MAGIC_COST;
    if (sMove != RITO_MOVE_GLIDE) {
        RocsJump(play, player);
        return;
    }
    sBoostFrames = ROCS_BOOST_FRAMES;
    player->actor.velocity.y = ROCS_JUMP_VELOCITY;
    StartUpdraft(play, &player->actor.world.pos);
    PlayClip(play, player, CLIP_LAUNCH, ANIMMODE_ONCE, -4.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_PL_ROLL);
    SpawnRocsSparkles(play, player);
}

// ---- throws ----

static bool IsThrowable(s32 item) {
    return item == ITEM_BOMB || item == ITEM_BOMBCHU || item == ITEM_NUT || item == ITEM_SLINGSHOT ||
           item == ITEM_BOOMERANG;
}

// OoT spawns and throws the item itself; the Rito only puts its swing on the body while that happens. The swing
// starts from the form's update, not inside the upper body that is still using the item.
static void NoteAirThrow(bool* should, va_list args) {
    s32 item = va_arg(args, s32);

    if (!*should || !IsRito() || gPlayState == NULL || IsGrounded(GET_PLAYER(gPlayState)) || !IsThrowable(item)) {
        return;
    }
    sIsThrowPending = true;
}

static void StartThrow(PlayState* play, Player* player) {
    sIsThrowPending = false;
    if (IsGrounded(player) || GetClip(CLIP_THROW) == NULL || (IsRitoBusy(player) && !IsFlightMove())) {
        return;
    }
    sBoostFrames = 0;
    StartMove(play, player, RITO_MOVE_THROW);
    KeepAirborne(player);
    player->actor.gravity = JUMP_GRAVITY;
    PlayClip(play, player, CLIP_THROW, ANIMMODE_ONCE, -4.0f);
}

static void RunThrow(PlayState* play, Player* player) {
    if (IsGrounded(player)) {
        EndMove(play, player);
        return;
    }
    KeepAirborne(player);
    Player_UpdateUpperBody(player, play);
    if (!LinkAnimation_Update(play, &player->skelAnime)) {
        return;
    }
    if (IsHeld(play, BTN_A)) {
        StartGlide(play, player);
    } else {
        EndMove(play, player);
    }
}

// ---- bow ----

// Obj_Switch never fills focus.pos, so its origin plus a lift is the only aim point it has.
static void GetAimPoint(Actor* actor, Vec3f* out) {
    if (actor->id == ACTOR_OBJ_SWITCH) {
        *out = actor->world.pos;
        out->y += 20.0f;
        return;
    }
    *out = actor->focus.pos;
}

static bool IsShootableSwitch(Actor* actor) {
    s32 type = actor->params & 7;
    return actor->id == ACTOR_OBJ_SWITCH &&
           (type == OBJSWITCH_TYPE_EYE || type == OBJSWITCH_TYPE_CRYSTAL || type == OBJSWITCH_TYPE_CRYSTAL_TARGETABLE);
}

static bool IsInSight(Actor* actor, s16 faceYaw) {
    if (actor->update == NULL || actor->xyzDistToPlayerSq > SQ(BOW_RANGE)) {
        return false;
    }
    s16 offset = actor->yawTowardsPlayer + 0x8000 - faceYaw;
    return ABS(offset) < BOW_CONE;
}

static bool IsAlreadyChosen(Actor* actor, Actor** chosen, s32 count) {
    for (s32 i = 0; i < count; i++) {
        if (chosen[i] == actor) {
            return true;
        }
    }
    return false;
}

// Up to `max` distinct actors, nearest first; a switch comes before an enemy because it is why you shoot.
static s32 FindTargets(PlayState* play, Actor** out, s32 max, s16 faceYaw) {
    static const u8 sPasses[] = { ACTORCAT_SWITCH, ACTORCAT_PROP, ACTORCAT_BG, ACTORCAT_ENEMY };
    s32 found = 0;

    for (u32 pass = 0; pass < ARRAY_COUNT(sPasses) && found < max; pass++) {
        bool wantsSwitch = sPasses[pass] != ACTORCAT_ENEMY;
        while (found < max) {
            Actor* best = NULL;
            f32 bestDist = SQ(BOW_RANGE);
            for (Actor* actor = play->actorCtx.actorLists[sPasses[pass]].head; actor != NULL; actor = actor->next) {
                if ((wantsSwitch && !IsShootableSwitch(actor)) || !IsInSight(actor, faceYaw) ||
                    actor->xyzDistToPlayerSq >= bestDist || IsAlreadyChosen(actor, out, found)) {
                    continue;
                }
                best = actor;
                bestDist = actor->xyzDistToPlayerSq;
            }
            if (best == NULL) {
                break;
            }
            out[found++] = best;
        }
    }
    return found;
}

static void ClearArrows(void) {
    memset(sArrows, 0, sizeof(sArrows));
}

static bool IsInList(PlayState* play, u8 category, Actor* wanted) {
    for (Actor* actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
        if (actor == wanted) {
            return true;
        }
    }
    return false;
}

// Obj_Ice_Poly is a child of the switch it freezes: shattering it on arrival thaws the switch the same frame.
static void ShatterIce(PlayState* play, Vec3f* at) {
    static Color_RGBA8 sIceWhite = { 250, 250, 250, 255 };
    static Color_RGBA8 sIceGray = { 180, 200, 230, 255 };

    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        Actor* actor = play->actorCtx.actorLists[category].head;
        while (actor != NULL) {
            Actor* next = actor->next;
            if (actor->id == ACTOR_OBJ_ICE_POLY && actor->update != NULL &&
                Math_Vec3f_DistXYZ(at, &actor->world.pos) < BOW_ICE_RADIUS) {
                Vec3f accel = { 0.0f, -1.0f, 0.0f };
                for (s32 i = 0; i < 8; i++) {
                    Vec3f pos = actor->world.pos;
                    Vec3f vel = { Rand_CenteredFloat(6.0f), Rand_ZeroOne() * 6.0f, Rand_CenteredFloat(6.0f) };
                    pos.x += Rand_CenteredFloat(40.0f);
                    pos.y += Rand_ZeroOne() * 70.0f;
                    pos.z += Rand_CenteredFloat(40.0f);
                    func_8002829C(play, &pos, &vel, &accel, &sIceWhite, &sIceGray, 350, 20);
                }
                Sfx_PlaySfxAtPos(&actor->world.pos, NA_SE_EV_ICE_BROKEN);
                if (actor->parent != NULL) {
                    actor->parent->params &= ~0x80;
                }
                Actor_Kill(actor);
            }
            actor = next;
        }
    }
}

// Rotating an arrow curves nothing: EnArrow rides the velocity laid down at the shot, so it is rebuilt each frame.
static void SteerArrow(PlayState* play, HomingArrow* slot) {
    Actor* arrow = slot->arrow;

    if (!IsInList(play, slot->targetCategory, slot->target)) {
        slot->target = NULL;
        return;
    }
    GetAimPoint(slot->target, &slot->goal);
    f32 dist = Math_Vec3f_DistXYZ(&arrow->world.pos, &slot->goal);
    if (dist < BOW_HIT_DIST) {
        ShatterIce(play, &slot->goal);
        slot->arrow = NULL;
        return;
    }
    s16 turn = dist < BOW_SNAP_DIST ? 0x7FFF : BOW_TURN;
    s16 yaw = Math_Vec3f_Yaw(&arrow->world.pos, &slot->goal) - arrow->world.rot.y;
    s16 pitch = Math_Vec3f_Pitch(&arrow->world.pos, &slot->goal) - arrow->world.rot.x;
    arrow->world.rot.y += CLAMP(yaw, -turn, turn);
    arrow->world.rot.x += CLAMP(pitch, -turn, turn);
    arrow->shape.rot = arrow->world.rot;
    Actor_SetProjectileSpeed(arrow, BOW_ARROW_SPEED);
}

static void TickArrows(PlayState* play) {
    for (s32 i = 0; i < BOW_VOLLEY; i++) {
        HomingArrow* slot = &sArrows[i];
        if (slot->arrow == NULL) {
            continue;
        }
        if (!IsInList(play, ACTORCAT_ITEMACTION, slot->arrow)) {
            slot->arrow = NULL;
            continue;
        }
        if (slot->target != NULL) {
            SteerArrow(play, slot);
        }
    }
}

static Actor* SpawnArrow(PlayState* play, Player* player, s16 yaw, s16 pitch, s32 type) {
    Vec3f* from = &player->bodyPartsPos[PLAYER_BODYPART_L_HAND];

    player->unk_A73 = BOW_HANDSHAKE;
    Actor* arrow = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ARROW, from->x, from->y, from->z, pitch, yaw, 0, type);
    if (arrow == NULL) {
        return NULL;
    }
    for (s32 i = 0; i < BOW_VOLLEY; i++) {
        if (sArrows[i].arrow == NULL) {
            sArrows[i].arrow = arrow;
            sArrows[i].target = NULL;
            break;
        }
    }
    return arrow;
}

static void AimArrow(Actor* arrow, Actor* target) {
    for (s32 i = 0; i < BOW_VOLLEY; i++) {
        if (sArrows[i].arrow == arrow) {
            sArrows[i].target = target;
            sArrows[i].targetCategory = target->category;
            GetAimPoint(target, &sArrows[i].goal);
            return;
        }
    }
}

// One arrow down Link's line at the first charge, two homing ones at the second; a lock-on always homes.
static void LooseVolley(PlayState* play, Player* player) {
    Actor* targets[BOW_TWIN_SHOTS];
    Actor* locked = player->focusActor;
    s16 faceYaw = player->actor.shape.rot.y;
    s32 shots = sCharge >= BOW_CHARGE_TWIN ? BOW_TWIN_SHOTS : 1;
    bool isHoming = shots > 1 || locked != NULL;
    s32 count = (locked == NULL && isHoming) ? FindTargets(play, targets, shots, faceYaw) : 0;

    ClearArrows();
    for (s32 i = 0; i < shots; i++) {
        s16 spoke = faceYaw + (s16)(((i * 2 - (shots - 1)) * BOW_FAN_STEP) / 2);
        Actor* arrow = SpawnArrow(play, player, spoke, 0, ARROW_NORMAL);
        Actor* target = locked != NULL ? locked : (i < count ? targets[i] : NULL);
        if (arrow != NULL && isHoming && target != NULL) {
            AimArrow(arrow, target);
        }
    }
}

// Aiming states walk the clip they show: the stick turns the Rito toward it, relative to the camera.
static bool MoveWhileAiming(PlayState* play, Player* player, bool isAdvancing) {
    f32 stickMagnitude;
    s16 stickAngle;

    func_80077D10(&stickMagnitude, &stickAngle, &play->state.input[0]);
    player->linearVelocity = isAdvancing ? BOW_MOVE_SPEED : 0.0f;
    if (stickMagnitude < BOW_MOVE_DEADZONE) {
        return false;
    }
    Math_ScaledStepToS(&player->yaw, Camera_GetInputDirYaw(GET_ACTIVE_CAM(play)) + stickAngle, BOW_TURN_RATE);
    player->actor.world.rot.y = player->yaw;
    player->actor.shape.rot.y = player->yaw;
    return true;
}

static void StartBow(PlayState* play, Player* player) {
    sIsBowMoving = fabsf(player->linearVelocity) > 1.0f;
    StartMove(play, player, RITO_MOVE_BOW_ENTER);
    sCharge = 0;
    PlayClip(play, player, sIsBowMoving ? CLIP_BOW_ENTER_MOVE : CLIP_BOW_ENTER_STILL, ANIMMODE_ONCE, -4.0f);
}

static void StartBowAir(PlayState* play, Player* player) {
    StartMove(play, player, RITO_MOVE_BOW_AIR);
    KeepAirborne(player);
    sCharge = 0;
    sHasLoosed = false;
    PlayClip(play, player, CLIP_BOW_AIR, ANIMMODE_ONCE, -4.0f);
}

// A tap is a thrown seed, not a draw: the charge is what tells them apart.
static void StartLoose(PlayState* play, Player* player) {
    bool isTap = sCharge < BOW_TAP_FRAMES && GetClip(CLIP_BOW_SEED) != NULL;

    sMove = isTap ? RITO_MOVE_BOW_SEED : RITO_MOVE_BOW_RELEASE;
    sMoveFrames = 0;
    sHasLoosed = false;
    PlayClip(play, player, isTap ? CLIP_BOW_SEED : CLIP_BOW_RELEASE, ANIMMODE_ONCE, -4.0f);
}

static void SwapAimLoop(PlayState* play, Player* player, bool isMoving) {
    sIsBowMoving = isMoving;
    sMove = RITO_MOVE_BOW_HOLD;
    PlayClip(play, player, isMoving ? CLIP_BOW_HOLD_MOVE : CLIP_BOW_HOLD_STILL, ANIMMODE_LOOP, -4.0f);
}

static void RunBowAim(PlayState* play, Player* player) {
    bool wantsMove = MoveWhileAiming(play, player, sIsBowMoving);

    if (!IsHeld(play, BTN_B)) {
        StartLoose(play, player);
        return;
    }
    sCharge++;
    if (wantsMove != sIsBowMoving) {
        SwapAimLoop(play, player, wantsMove);
        return;
    }
    if (LinkAnimation_Update(play, &player->skelAnime) && sMove == RITO_MOVE_BOW_ENTER) {
        SwapAimLoop(play, player, sIsBowMoving);
    }
}

// The loose doubles as a dodge: it carries the Rito back off the shot with a short grace window.
static void RunBowRelease(PlayState* play, Player* player) {
    f32 lead =
        GetClip(CLIP_BOW_RELEASE) != NULL ? Animation_GetLastFrame(GetClip(CLIP_BOW_RELEASE)) - BOW_LOOSE_LEAD : 0.0f;

    if (sMoveFrames++ == 0) {
        player->actor.world.rot.y = player->focusActor != NULL
                                        ? Math_Vec3f_Yaw(&player->focusActor->world.pos, &player->actor.world.pos)
                                        : (s16)(player->actor.shape.rot.y + 0x8000);
        player->yaw = player->actor.world.rot.y;
        player->linearVelocity = BOW_HOP_SPEED;
        if (player->invincibilityTimer > BOW_DODGE_IFRAMES) {
            player->invincibilityTimer = BOW_DODGE_IFRAMES;
        }
    }
    Math_StepToF(&player->linearVelocity, 0.0f, BOW_HOP_DECAY);
    if (!sHasLoosed && player->skelAnime.curFrame >= lead) {
        sHasLoosed = true;
        LooseVolley(play, player);
    }
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        if (!sHasLoosed) {
            LooseVolley(play, player);
        }
        EndMove(play, player);
    }
}

static void RunBowSeed(PlayState* play, Player* player) {
    player->linearVelocity = 0.0f;
    if (!sHasLoosed && ++sMoveFrames >= BOW_LOOSE_FRAME) {
        sHasLoosed = true;
        ClearArrows();
        SpawnArrow(play, player, player->actor.shape.rot.y, 0, ARROW_SEED);
        Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
    }
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        EndMove(play, player);
    }
}

// Pinned in the air for the clip: a held draw fans three arrows straight down, a tap sends one ahead.
// A tap sends one arrow ahead: at the lock-on, else at the nearest thing in sight, else down Link's line.
static void LooseAirTap(PlayState* play, Player* player) {
    Actor* target = player->focusActor;
    s16 yaw = player->actor.shape.rot.y;
    s16 pitch = 0;

    if (target == NULL) {
        FindTargets(play, &target, 1, yaw);
    }
    if (target != NULL) {
        Vec3f goal;
        GetAimPoint(target, &goal);
        yaw = Math_Vec3f_Yaw(&player->bodyPartsPos[PLAYER_BODYPART_L_HAND], &goal);
        pitch = Math_Vec3f_Pitch(&player->bodyPartsPos[PLAYER_BODYPART_L_HAND], &goal);
    }
    Actor* arrow = SpawnArrow(play, player, yaw, pitch, ARROW_NORMAL);
    if (arrow != NULL && target != NULL) {
        AimArrow(arrow, target);
    }
}

// A held draw fans three arrows straight down, the fan tipped in pitch so they spread along Link's line.
static void LooseAirVolley(PlayState* play, Player* player) {
    for (s32 i = 0; i < BOW_VOLLEY; i++) {
        SpawnArrow(play, player, player->actor.shape.rot.y, (s16)(0x4000 + (i - 1) * BOW_FAN_STEP), ARROW_NORMAL);
    }
}

// Pinned in the air for the clip. Whether B let go inside the tap window decides the shot, so nothing leaves
// before that is known.
static void RunBowAir(PlayState* play, Player* player) {
    bool isBHeld = IsHeld(play, BTN_B);

    KeepAirborne(player);
    player->actor.velocity.y = 0.0f;
    player->actor.gravity = 0.0f;
    player->linearVelocity = 0.0f;
    if (isBHeld) {
        sCharge++;
    }
    if (sMove == RITO_MOVE_BOW_AIR && !isBHeld && sCharge < BOW_TAP_FRAMES && GetClip(CLIP_BOW_AIR_TAP) != NULL) {
        sMove = RITO_MOVE_BOW_AIR_TAP;
        sMoveFrames = 0;
        PlayClip(play, player, CLIP_BOW_AIR_TAP, ANIMMODE_ONCE, -4.0f);
        return;
    }
    bool isShotKnown = sMove == RITO_MOVE_BOW_AIR_TAP || sCharge >= BOW_TAP_FRAMES;
    if (!sHasLoosed && ++sMoveFrames >= BOW_LOOSE_FRAME && isShotKnown) {
        sHasLoosed = true;
        ClearArrows();
        if (sMove == RITO_MOVE_BOW_AIR_TAP) {
            LooseAirTap(play, player);
        } else {
            LooseAirVolley(play, player);
        }
    }
    if (!LinkAnimation_Update(play, &player->skelAnime)) {
        return;
    }
    if (!sHasLoosed) {
        LooseAirVolley(play, player);
    }
    if (IsHeld(play, BTN_A)) {
        StartGlide(play, player);
    } else {
        EndMove(play, player);
    }
}

static bool TryAirBow(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_B) || GetClip(CLIP_BOW_AIR) == NULL) {
        return false;
    }
    StartBowAir(play, player);
    return true;
}

// ---- water ----

// A bird is no better in water than a rolling boulder: it beats its wings all the way down until the void.
static void RunSink(PlayState* play, Player* player) {
    Player_ZeroSpeedXZ(player);
    LinkAnimation_Update(play, &player->skelAnime);
    if (++sMoveFrames == SINK_FRAMES) {
        Sfx_PlaySfxCentered(NA_SE_OC_ABYSS);
        Play_TriggerVoidOut(play);
    }
}

// OoT re-seats its swim on anyone deep in water, so the sink runs in its place through the action VB.
static void TrySink(PlayState* play, Player* player) {
    if (sMove == RITO_MOVE_SINK || !(player->stateFlags1 & PLAYER_STATE1_IN_WATER) ||
        player->actor.yDistToWater <= SINK_DEPTH ||
        (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_GETTING_ITEM))) {
        return;
    }
    sMove = RITO_MOVE_SINK;
    sMoveFrames = 0;
    player->actor.velocity.y = SINK_START;
    player->actor.gravity = SINK_GRAVITY;
    PlayClip(gPlayState, player, CLIP_FLY, ANIMMODE_LOOP, -4.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_DIVE_INTO_WATER);
}

static void DriveSink(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);

    if (!IsRito() || sMove != RITO_MOVE_SINK || gPlayState == NULL ||
        (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE))) {
        return;
    }
    *should = false;
    RunSink(gPlayState, player);
}

// ---- action ----

static void RitoAction(Player* player, PlayState* play) {
    switch (sMove) {
        case RITO_MOVE_CHARGE:
            RunCharge(play, player);
            break;
        case RITO_MOVE_LAUNCH:
            RunLaunch(play, player);
            break;
        case RITO_MOVE_GLIDE:
            RunGlide(play, player);
            break;
        case RITO_MOVE_HOP:
            RunHop(play, player);
            break;
        case RITO_MOVE_THROW:
            RunThrow(play, player);
            break;
        case RITO_MOVE_BOW_ENTER:
        case RITO_MOVE_BOW_HOLD:
            RunBowAim(play, player);
            break;
        case RITO_MOVE_BOW_RELEASE:
            RunBowRelease(play, player);
            break;
        case RITO_MOVE_BOW_SEED:
            RunBowSeed(play, player);
            break;
        case RITO_MOVE_BOW_AIR:
        case RITO_MOVE_BOW_AIR_TAP:
            RunBowAir(play, player);
            break;
        default:
            EndMove(play, player);
            break;
    }
}

static bool CanHoldA(Player* player) {
    return !IsRitoBusy(player) && CanAct(player) && IsGrounded(player) && !Player_IsZTargeting(player);
}

// A tap is a hop and a hold is the updraft, so the press is kept until the button either lets go or stays down.
static void HoldA(void) {
    sIsAPending = true;
    sAFrames = 0;
}

static int32_t HandleRoll(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_A) || !CanHoldA(player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    HoldA();
    return SOH_FORM_ACTION_BLOCKED;
}

static void TrackA(PlayState* play, Player* player) {
    if (!sIsAPending) {
        return;
    }
    if (!CanHoldA(player)) {
        sIsAPending = false;
        return;
    }
    if (IsHeld(play, BTN_A)) {
        if (++sAFrames >= A_HOLD_FRAMES) {
            sIsAPending = false;
            StartCharge(play, player);
        }
        return;
    }
    sIsAPending = false;
    StartHop(play, player, PLAYER_STICK_DIR_FORWARD);
}

static int32_t HandleZTargetA(PlayState* play, Player* player) {
    s8 direction = player->controlStickDirections[player->controlStickDataIndex];

    if (!IsPressed(play, BTN_A) || !CanAct(player) || !IsGrounded(player) || direction <= PLAYER_STICK_DIR_FORWARD) {
        return SOH_FORM_ACTION_VANILLA;
    }
    StartHop(play, player, direction);
    return SOH_FORM_ACTION_STARTED;
}

static int32_t HandleB(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_B) || !CanAct(player) || !IsGrounded(player) || GetClip(CLIP_BOW_HOLD_STILL) == NULL) {
        return SOH_FORM_ACTION_VANILLA;
    }
    StartBow(play, player);
    return SOH_FORM_ACTION_STARTED;
}

// OoT's standing list has no roll to ask and the air runs no list for A or B, so these are asked for here.
// Anything A opened this frame (a door, a sign, a ledge) has already raised a flag CanAct refuses.
static void TryFreeMoves(PlayState* play, Player* player) {
    if (IsRitoBusy(player) || !CanAct(player) || (player->stateFlags2 & PLAYER_STATE2_HOPPING)) {
        return;
    }
    if (IsGrounded(player)) {
        if (IsPressed(play, BTN_A) && !sIsAPending && CanHoldA(player)) {
            HoldA();
        }
        return;
    }
    if (TryAirBow(play, player)) {
        return;
    }
    if (IsPressed(play, BTN_A)) {
        StartGlide(play, player);
    }
}

// Damage, a cutscene or the water can take the action without passing through the move's end.
static void DropInterruptedMove(Player* player) {
    if (sMove == RITO_MOVE_NONE || sMove == RITO_MOVE_SINK || IsRitoBusy(player)) {
        return;
    }
    ClearMove(player);
}

// ---- landing ----

static void ResolveLanding(int32_t group, int32_t animType, LinkAnimationHeader** anim) {
    (void)animType;
    if (!IsRito() || (group != PLAYER_ANIMGROUP_landing && group != PLAYER_ANIMGROUP_short_landing)) {
        return;
    }
    LinkAnimationHeader* land = GetClip(CLIP_LAND);
    if (land != NULL) {
        *anim = land;
    }
}

// ---- ocarina ----

static bool IsHoldingOcarina(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_OCARINA || player->rightHandType == PLAYER_MODELTYPE_RH_OOT;
}

static bool IsPlayingOcarinaClip(Player* player, const char* name) {
    const char* anim = (const char*)player->skelAnime.animation;

    return anim != NULL && ResourceMgr_OTRSigCheck((char*)anim) && strstr(anim, name) != NULL;
}

// Runs inside the ocarina's own update; the pose consumes the edge on the game frame.
static void NoteStruck(uint8_t note, float modulator, int8_t bend) {
    (void)modulator;
    (void)bend;
    if (note != OCARINA_NOTE_NONE && note != sLastNote) {
        sWasNoteStruck = true;
    }
    sLastNote = note;
}

static void PlaybackNoteStruck(uint8_t note, float modulator) {
    NoteStruck(note, modulator, 0);
}

// Vanilla's ocarina action waits for the end of its intro and outro clips; Sheik's draw takes their place,
// forward and then backward, so that wait is what brings the harp out and puts it away.
static bool SwapOcarinaClip(PlayState* play, Player* player) {
    LinkAnimationHeader* raise = GetClip(CLIP_HARP_RAISE);

    if (IsPlayingOcarinaClip(player, "okarina_start")) {
        PlayClip(play, player, CLIP_HARP_RAISE, ANIMMODE_ONCE, -6.0f);
        sHarpPhase = HARP_RAISING;
        return true;
    }
    if (IsPlayingOcarinaClip(player, "okarina_end")) {
        LinkAnimation_Change(play, &player->skelAnime, raise, -1.0f, Animation_GetLastFrame(raise), 0.0f, ANIMMODE_ONCE,
                             -6.0f);
        sHarpPhase = HARP_LOWERING;
        return true;
    }
    return false;
}

// Each note plucks the playing clip on from where the last one stopped. It loops: that same action reopens
// the ocarina whenever a clip reports its end, which would wipe the notes of the song being played.
static void PoseHarp(PlayState* play, Player* player) {
    f32 pluckEnd = Animation_GetLastFrame(GetClip(CLIP_HARP_PLAY)) - CLIP_FRAMES_PER_UPDATE;

    if (!IsPlaying(player, CLIP_HARP_PLAY)) {
        PlayClip(play, player, CLIP_HARP_PLAY, ANIMMODE_LOOP, -4.0f);
        player->skelAnime.playSpeed = 0.0f;
        sHarpPhase = HARP_PLAYING;
    }
    bool isPluckDone = player->skelAnime.curFrame >= pluckEnd;
    if (sWasNoteStruck) {
        if (isPluckDone) {
            player->skelAnime.curFrame = 0.0f;
        }
        player->skelAnime.playSpeed = 1.0f;
    } else if (isPluckDone) {
        player->skelAnime.playSpeed = 0.0f;
    }
    sWasNoteStruck = false;
}

// The ocarina stays out across the message session, so the latch only lets go once no message is up.
static void UpdateHarp(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsOcarinaOut = true;
    } else if (!isSessionOpen) {
        if (sIsOcarinaOut) {
            Audio_OcaSetInstrument(OCARINA_INSTRUMENT_OFF);
        }
        sIsOcarinaOut = false;
        sHarpPhase = HARP_IDLE;
    }
    if (!sIsOcarinaOut || GetClip(CLIP_HARP_RAISE) == NULL || GetClip(CLIP_HARP_PLAY) == NULL ||
        SwapOcarinaClip(play, player)) {
        return;
    }
    bool isRaising = sHarpPhase == HARP_RAISING && IsPlaying(player, CLIP_HARP_RAISE);
    if (isRaising || sHarpPhase == HARP_LOWERING) {
        return;
    }
    if (isSessionOpen) {
        // Re-applied every frame: the message system sets the instrument again around opening, and the last
        // write wins.
        Audio_OcaSetInstrument(OCARINA_INSTRUMENT_HARP);
    }
    // A raise that ended hands over to the plucking even before an actor opens the session it was played for.
    if (isSessionOpen || sHarpPhase == HARP_RAISING) {
        PoseHarp(play, player);
    }
}

// ---- body ----

static bool IsPathNamed(Gfx* dList, const char* part) {
    return dList != NULL && ResourceMgr_OTRSigCheck((char*)dList) && strstr((const char*)dList, part) != NULL;
}

static bool IsHarpOut(void) {
    return sIsOcarinaOut && sHarpPhase != HARP_IDLE;
}

// The rig is one skeleton for both ages: a child's hand or waist list is swapped for the Rito's own, and the
// shield Link would carry is left to the Rito's plate, drawn after the limb.
static void ResolveRitoBody(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!IsRito()) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_ROOT && pos != NULL) {
        if (IsPlaying(player, CLIP_LAND)) {
            pos->y += LAND_LIFT;
        } else if (IsPlaying(player, CLIP_BOW_SEED)) {
            pos->y += BOW_SEED_LIFT;
        } else if (IsPlaying(player, CLIP_BOW_RELEASE)) {
            pos->y += BOW_RELEASE_LIFT;
        }
        return;
    }
    if (limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = NULL;
        return;
    }
    bool isLeft = limbIndex == PLAYER_LIMB_L_HAND;
    if (!isLeft && limbIndex != PLAYER_LIMB_R_HAND) {
        if (IsPathNamed(*dList, "objects/object_link_")) {
            *dList = limbDList;
        }
        return;
    }
    if (IsHarpOut()) {
        *dList = NULL;
        return;
    }
    bool isChildHand = !LINK_IS_ADULT && IsPathNamed(*dList, "object_link_child") && !IsPathNamed(*dList, "Holding") &&
                       !IsPathNamed(*dList, "Oot");
    if (!IsPathNamed(*dList, "Shield") && !isChildHand) {
        return;
    }
    bool isClosed = IsPathNamed(*dList, "Closed") || IsPathNamed(*dList, "Shield");
    const char* hand =
        isLeft ? (isClosed ? sClosedLeftHandDL : sOpenLeftHandDL) : (isClosed ? sClosedHandDL : sOpenHandDL);
    *dList = (Gfx*)hand;
}

// Mir_Ray reflects off shieldMf, so it is taken from the plate itself, in the hand or on the back. The sheath
// limb comes after the hand, so this capture outlives the one vanilla takes at the hand.
static void DrawShield(PlayState* play, Player* player, const Vec3f* offset) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(offset->x, offset->y, offset->z, MTXMODE_APPLY);
    Matrix_Get(&player->shieldMf);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sShieldDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawHarp(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(sHarpOffset.x, sHarpOffset.y, sHarpOffset.z, MTXMODE_APPLY);
    Matrix_RotateZYX(sHarpRot.x, sHarpRot.y, sHarpRot.z, MTXMODE_APPLY);
    Matrix_Scale(HARP_SCALE, HARP_SCALE, HARP_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sHarpDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// The held bow with its fist cut off, and vanilla's string authored across the full draw.
static void DrawBow(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBowDL);
    Matrix_Translate(0.0f, BOW_STRING_Y, 0.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBowStringDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// The form borrows the Mirror Shield's equipment for its reflections; the plate on screen is the Rito's own.
static void DrawRitoParts(PlayState* play, Player* player, int32_t limbIndex) {
    if (!IsRito()) {
        return;
    }
    bool isShieldInHand =
        player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD && (player->stateFlags1 & PLAYER_STATE1_SHIELDING);
    if (!IsBowOut() && ResourceMgr_FileExists(sShieldDL)) {
        if (limbIndex == PLAYER_LIMB_R_HAND && isShieldInHand) {
            DrawShield(play, player, &sShieldInHand);
        } else if (limbIndex == PLAYER_LIMB_SHEATH && !isShieldInHand) {
            DrawShield(play, player, &sShieldOnBack);
        }
    }
    if (limbIndex != PLAYER_LIMB_L_HAND) {
        return;
    }
    if (IsHarpOut() && ResourceMgr_FileExists(sHarpDL)) {
        DrawHarp(play);
    } else if (IsBowOut() && ResourceMgr_FileExists(sBowDL)) {
        DrawBow(play);
    }
}

// Mir_Ray only reflects while the right hand is in its shield model, which vanilla sets only with R up. The plate
// on the back reflects too, so an empty hand is kept in that model; its list is the Rito's closed hand.
static void RaiseShieldOnBack(Player* player) {
    bool isHandEmpty =
        player->rightHandType == PLAYER_MODELTYPE_RH_OPEN || player->rightHandType == PLAYER_MODELTYPE_RH_CLOSED;

    if (!isHandEmpty || player->heldActor != NULL || IsBowOut() || IsHarpOut()) {
        return;
    }
    player->rightHandType = PLAYER_MODELTYPE_RH_SHIELD;
}

static void DrawRito(PlayState* play, Player* player) {
    (void)player;
    DrawUpdraft(play);
}

// ---- form ----

static void ResetState(PlayState* play) {
    sMove = RITO_MOVE_NONE;
    sMoveFrames = 0;
    sCharge = 0;
    sIsAPending = false;
    sHarpPhase = HARP_IDLE;
    sWasNoteStruck = false;
    ClearArrows();
    if (play != NULL) {
        StopUpdraft(play);
    }
}

static void EnterRito(PlayState* play, Player* player) {
    (void)player;
    LoadClips();
    ResetState(play);
}

static void ExitRito(PlayState* play, Player* player) {
    bool wasBusy = IsRitoBusy(player);

    ResetState(play);
    if (sIsOcarinaOut) {
        Audio_OcaSetInstrument(OCARINA_INSTRUMENT_OFF);
        sIsOcarinaOut = false;
    }
    player->actor.gravity = JUMP_GRAVITY;
    if (wasBusy) {
        func_80839FFC(player, play);
    }
}

static void UpdateRito(PlayState* play, Player* player) {
    DropInterruptedMove(player);
    if (sIsThrowPending) {
        StartThrow(play, player);
    }
    TryRocs(play, player);
    TrackA(play, player);
    TryFreeMoves(play, player);
    RaiseShieldOnBack(player);
    TrySink(play, player);
    TickUpdraft(play, sMove == RITO_MOVE_CHARGE, (f32)sCharge / CHARGE_FULL);
    TickArrows(play);
    UpdateHarp(play, player);
}

static void ForgetScene(int16_t sceneNum) {
    (void)sceneNum;
    ForgetUpdraft();
    ClearArrows();
    sMove = RITO_MOVE_NONE;
    sIsAPending = false;
    sIsOcarinaOut = false;
    sHarpPhase = HARP_IDLE;
}

// No sword to swing and the Mirror Shield's properties for its plate; the stash gives Link his own back.
static uint16_t ResolveRitoEquipment(int32_t equipType, uint16_t value) {
    if (equipType == EQUIP_TYPE_SWORD) {
        return EQUIP_VALUE_SWORD_NONE;
    }
    if (equipType == EQUIP_TYPE_SHIELD) {
        return EQUIP_VALUE_SHIELD_MIRROR;
    }
    return value;
}

static bool IsAllowedCustomItem(const char* customKey) {
    static const char* const sAllowed[] = {
        "nei.deku_form_mask", "nei.goron_form_mask", "nei.zora_form_mask",   "nei.keaton_form_mask",
        "nei.mask_kafei",     "nei.garo_form_mask",  "nei.gerudo_form_mask", "nei.fierce_deity_mask",
        "nei.rocs_feather",   "nei.rocs_cape",       "nei.progressive_rocs",
    };

    for (u32 i = 0; i < ARRAY_COUNT(sAllowed); i++) {
        if (strcmp(customKey, sAllowed[i]) == 0) {
            return true;
        }
    }
    return false;
}

// What a Rito can throw or play with its wings: NEI's airborne throwables, the ocarina, bottles and the Lens.
static bool AllowsRitoItem(uint16_t item, const char* customKey) {
    if (customKey != NULL) {
        return IsAllowedCustomItem(customKey);
    }
    return item == ITEM_OCARINA_FAIRY || item == ITEM_OCARINA_TIME || item == ITEM_BOMB || item == ITEM_BOMBCHU ||
           item == ITEM_NUT || item == ITEM_SLINGSHOT || item == ITEM_BOOMERANG || item == ITEM_LENS ||
           (item >= ITEM_BOTTLE && item <= ITEM_POE);
}

static LinkAnimationHeader* LoadMaskOnAnim(void) {
    LinkAnimationHeader* source = ResourceMgr_LoadPlayerAnimAsHeader(sMaskOnPath);

    if (source == NULL || source->common.frameCount <= 0) {
        return NULL;
    }
    size_t values = (size_t)source->common.frameCount * ANIM_VALUES_PER_FRAME;
    s16* frames = (s16*)malloc(values * sizeof(s16));
    if (frames == NULL) {
        return NULL;
    }
    memcpy(frames, source->segment, values * sizeof(s16));
    for (s32 frame = 0; frame < source->common.frameCount; frame++) {
        s16* root = &frames[frame * ANIM_VALUES_PER_FRAME];
        root[0] = MM_ANIM_BASE_TRANSL_X;
        root[2] = 0;
    }
    free(sMaskOnFrames);
    sMaskOnFrames = frames;
    sMaskOnAnim.common.frameCount = source->common.frameCount;
    sMaskOnAnim.segment = frames;
    return &sMaskOnAnim;
}

static const SOHFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_ROLL, HandleRoll },
    { SOH_PLAYER_ACTION_ZTARGET_A, HandleZTargetA },
    { SOH_PLAYER_ACTION_MELEE, HandleB },
};

static void RegisterForm(void) {
    SOHFormDefinition rito = { 0 };

    rito.structSize = sizeof(rito);
    rito.key = RITO_FORM_KEY;
    rito.label = "Rito";
    rito.kind = SOH_FORM_KIND_LINK;
    rito.item = RITO_MASK_KEY;
    rito.modelPath = RITO_MODEL_PATH;
    rito.rootScaleAdult = RITO_ROOT_SCALE_ADULT;
    rito.rootScaleChild = RITO_ROOT_SCALE_CHILD;
    rito.height = RITO_HEIGHT;
    rito.motionScale = 1.0f;
    rito.resolveEquipment = ResolveRitoEquipment;
    rito.allowsButtonItem = AllowsRitoItem;
    rito.transformAnim = LoadMaskOnAnim();
    rito.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    rito.actions = sActions;
    rito.actionCount = ARRAY_COUNT(sActions);
    rito.onEnter = EnterRito;
    rito.onExit = ExitRito;
    rito.update = UpdateRito;
    rito.draw = DrawRito;
    sApi->RegisterForm(&rito);
}

// ---- mask ----

static bool CanWearMask(Player* player, PlayState* play) {
    (void)play;
    return IsGrounded(player) &&
           !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_CUTSCENE));
}

static void WearMask(PlayState* play, Player* player) {
    (void)play;
    (void)player;
    sApi->ToggleForm(RITO_FORM_KEY);
}

static void RegisterMask(void) {
    SOHCustomItemDefinition mask = Z64Items_Define(RITO_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, RITO_MASK_PAGE, RITO_MASK_SLOT, RITO_MASK_WHEEL_PRIORITY);
    Z64Items_SetTextbox(&mask, "You got the %rRito Mask%w!&The face of a proud bird of the north wind.^Wear it "
                               "with %y\xA1%w to become a %rRito%w: an updraft to ride, wings to glide and a bow "
                               "that never runs dry.");
    Z64Items_SetPauseText(&mask, "%rRito Mask&%wPress %y\xA1%w to become a Rito.&Hold %y\x9F%w: updraft (magic)  "
                                 "%y\x9F%w in the air: glide&%y\xA0%w: bow, tap for a seed  %y\xA3%w: shield");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_Register(sApi, &mask);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    RegisterMask();
    RegisterForm();
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveAnim, ResolveLanding);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveRitoBody);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawRitoParts);
    SOH_REGISTER_HOOK(sApi, OnOcarinaNote, NoteStruck);
    SOH_REGISTER_HOOK(sApi, OnOcarinaPlaybackNote, PlaybackNoteStruck);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    sApi->RegisterVB(VB_EXECUTE_PLAYER_ACTION_FUNC, DriveSink);
    sApi->RegisterVB(VB_CHANGE_HELD_ITEM_AND_USE_ITEM, NoteAirThrow);
}
