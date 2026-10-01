#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "textures/parameter_static/parameter_static.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"

#define GERUDO_MASK_KEY "nei.gerudo_form_mask"
#define GERUDO_FORM_KEY "nei.gerudo"
#define GERUDO_MODEL_PATH "objects/forms/gerudo"
// Gerudo shares Captain's Hat cell in the Majora mask grid.
#define GERUDO_MASK_PAGE 1
#define GERUDO_MASK_SLOT 21
#define GERUDO_MASK_WHEEL_PRIORITY (-1)

#define BG_ON_GROUND 1
#define FLOOR_TYPE_NO_JUMP 7
#define SLOPE_STEEP 1
#define STICK_MOVING 10.0f
#define STICK_FULL 60.0f
#define OCARINA_INSTRUMENT_OFF 0
#define OCARINA_INSTRUMENT_MALON 2
#define OCARINA_NOTE_NONE 0xFF
// LinkAnimation advances R_UPDATE_RATE * 0.5 clip frames per update, and the rate is always 3.
#define CLIP_FRAMES_PER_UPDATE 1.5f

#define A_TAP_FRAMES 8
#define SPRINT_SCALE 1.5f
#define ROLL_SCALE 2.0f
#define COMBO_STEPS 4
#define COMBO_RESET_FRAMES 40
#define LUNGE_SPEED 15.0f
#define CHARGE_RATE 0.06f
#define CHARGE_SPARK_COST 0x200
#define CHARGE_FAST_BEGIN 33.0f
#define CHARGE_FAST_END 53.0f
#define CHARGE_FAST_SCALE 3.0f
#define SPIN_INVULNERABILITY (-8)
#define SPIN_RADIUS 45
#define SPIN_HEIGHT 60
#define SPIN_DAMAGE 2
#define FRONT_SLASH_DISTANCE 100.0f
#define FRONT_SLASH_TRAVEL_FRAMES 30.0f
#define WALL_MARGIN 14.0f
#define JUMP_SLASH_LIFT 2.1f
#define JUMP_SLASH_SPEED_MIN 6.0f
#define JUMP_SLASH_SPEED_MAX 20.0f
#define JUMP_SLASH_LEAD 6.0f
#define JUMP_GRAVITY (-1.2f)
#define AERIAL_MIN_AIR_FRAMES 2
#define SHEATHE_HIDE_FRAME 17.0f
#define DRAW_SPEED 1.5f
#define STANDING_DRAW_SPEED 4.0f

#define RAGE_BASE 400
#define RAGE_HITS_TO_FILL 10
#define FURY_DAMAGE_MIN 10
#define FURY_DAMAGE_MAX 255
#define FURY_INVULNERABILITY 60
#define FURY_BLAST_RADIUS 320
#define FURY_BLAST_HEIGHT 400
#define FURY_BLAST_FRAMES 90.0f
#define FURY_STRIKE_FRAME 80.0f
#define FURY_DIM 140
#define FURY_DIM_FADE 10
#define FURY_DMG_FLAGS \
    (DMG_HAMMER_SWING | DMG_SLASH_GIANT | DMG_SPIN_GIANT | DMG_JUMP_GIANT | DMG_MAGIC_FIRE | DMG_MAGIC_LIGHT)

#define ANIM_VALUES_PER_FRAME 67
#define CLIP_CACHE_MAX 40
#define VOICE_SLOTS 4
#define VOICE_ADVANCE 0.5f
#define MM_ANIM_BASE_TRANSL_X (-57)

#define MHR(name) "__OTR__misc/link_animetion/gMonsterHunterRise_DualBlade_" name
#define CLIP_IDLE MHR("StationaryReadyIdle_Variant03")
#define CLIP_RUN MHR("ForwardCombatRun")
#define CLIP_SPRINT MHR("ForwardCombatRun_Variant02")
#define CLIP_DAMAGE_RUN MHR("ForwardDoubleRushSlash_Variant15")
#define CLIP_GUARD MHR("DemonModeActivationFlourish")
#define CLIP_LANDING MHR("ForwardSingleTwinSlash")
#define CLIP_ROLL MHR("ForwardAcrobaticEvasion")
#define CLIP_CHARGE MHR("LowExtendedChargeStance")
#define CLIP_JUMP_SLASH MHR("ForwardRisingDoubleAerialSlash_Variant18")
#define CLIP_HOP_LEFT MHR("RightRisingTripleLateralSlash")
#define CLIP_HOP_RIGHT MHR("LeftDoubleLateralSlash")
#define CLIP_BACKFLIP MHR("BackwardHighAerialLeftLeadDoubleSilkbindSlash")
#define CLIP_FRONT_SLASH MHR("BackwardRisingDoubleAerialSlash")
#define CLIP_AERIAL MHR("StationaryRisingTripleAerialSlash")
#define CLIP_SHEATHE MHR("ForwardDoubleTwinSlash")
#define CLIP_SOFT_HIT MHR("ForwardEvasiveStep")
#define CLIP_FALL MHR("StationaryReadyIdle_Variant05")
#define CLIP_CRIT_IDLE MHR("StationaryReadyIdle_Variant08")
// Malon's gMalonAdultSingAnim, retargeted onto Link by NEI's tools/bake_oot_npc_link_anims.py.
#define CLIP_SING "__OTR__misc/link_animetion/gPlayerAnim_mhr_npc_malon_sing"
#define CLIP_FURY "__OTR__misc/link_animetion/gPlayerAnim_mhr_gs_wirebug_attack04"
static const ALIGN_ASSET(2) char sMaskOnPath[] = "__OTR__misc/link_animetion/gPlayerAnim_cl_setmask_Data";
static const ALIGN_ASSET(2) char sMaskDL[] = "__OTR__objects/object_link_child/gLinkChildGerudoMaskDL";
static const ALIGN_ASSET(2) char sAdultEmptyHandDL[] = "__OTR__" GERUDO_MODEL_PATH
                                                       "/object_link_boy/gLinkAdultRightHandNearDL";
static const ALIGN_ASSET(2) char sChildEmptyHandDL[] = "__OTR__" GERUDO_MODEL_PATH
                                                       "/object_link_child/gLinkChildRightHandNearDL";
static const ALIGN_ASSET(2) char sScimitarDL[] = "__OTR__" GERUDO_MODEL_PATH
                                                 "/object_link_boy/gLinkAdultLeftHandHoldingMasterSwordNearDL";
static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_static/gItemIconMaskGerudoTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_static/gGerudoMaskItemNameENGTex";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/object_gi_gerudomask/gGiGerudoMaskDL";

typedef struct {
    s16 begin;
    s16 end;
} Window;

typedef struct {
    const char* clip;
    f32 speed;
    s16 swingEnd;
    Window left;
    Window right;
    bool isSpin;
    u8 mwa;
} SwingRow;

typedef enum {
    ROW_COMBO_1,
    ROW_COMBO_2,
    ROW_COMBO_3,
    ROW_COMBO_4,
    ROW_STAB,
    ROW_JUMP_LAND,
    ROW_CHARGE_RELEASE,
    ROW_QUICK_SPIN,
    ROW_COUNT,
} SwingRowId;

// NEI's measured dual-blade rows: windows are source frames, per hand; swingEnd is where the next hit chains
// and A rolls out, the rest of the clip is the recovery. A spin hits with a body cylinder instead of the blades.
static const SwingRow sRows[ROW_COUNT] = {
    { MHR("AlternatingCrossSlashLeftRight"), 1.7f, 25, { 13, 25 }, { 13, 25 }, false, PLAYER_MWA_FORWARD_SLASH_1H },
    { MHR("StationaryRisingSingleAerialSlash"), 1.7f, 28, { 1, 28 }, { 1, 28 }, false, PLAYER_MWA_FORWARD_COMBO_1H },
    { MHR("StationaryRightLeadDoubleTwinSlash"), 1.7f, 28, { 1, 28 }, { 1, 28 }, false, PLAYER_MWA_RIGHT_SLASH_1H },
    { MHR("RightRisingTripleAerialSlash"), 1.7f, 41, { 12, 41 }, { 12, 41 }, true, PLAYER_MWA_RIGHT_COMBO_1H },
    { MHR("ForwardRisingLeftLeadDoubleAerialSlash"), 1.5f, -1, { 4, 30 }, { 4, 30 }, false, PLAYER_MWA_STAB_1H },
    { CLIP_LANDING, 1.0f, -1, { 2, 40 }, { 2, 40 }, true, PLAYER_MWA_JUMPSLASH_FINISH },
    { MHR("ForwardTumbleDelayedCrossFinish"), 2.1f, -1, { 28, 40 }, { 28, 40 }, false, PLAYER_MWA_SPIN_ATTACK_1H },
    { MHR("ForwardRisingDoubleRushSlash_Variant21"), 2.1f, -1, { 6, 60 }, { 6, 60 }, true, PLAYER_MWA_BIG_SPIN_1H },
};

// z_player.c's D_80854488 by melee weapon (Master, Kokiri, Biggoron): slash, then jump.
static const u32 sTierDmgFlags[3][2] = {
    { DMG_SLASH_MASTER, DMG_JUMP_MASTER },
    { DMG_SLASH_KOKIRI, DMG_JUMP_KOKIRI },
    { DMG_SLASH_GIANT, DMG_JUMP_GIANT },
};
static const f32 sBladeLengths[3] = { 4000.0f, 3000.0f, 5500.0f };
#define BROKEN_KNIFE_LENGTH 1500.0f

static Vec3f sHandShieldCorners[4] = {
    { -4500.0f, -3000.0f, -600.0f },
    { 1500.0f, -3000.0f, -600.0f },
    { -4500.0f, 3000.0f, -600.0f },
    { 1500.0f, 3000.0f, -600.0f },
};

typedef struct {
    u16 sfxId;
    u8 variants;
} GerudoVoice;

// The Gerudo voice pack NEI shipped, by Link voice id; ids missing here stay silent, never Link's.
static const GerudoVoice sVoices[] = {
    { 0x6800, 4 }, { 0x6801, 2 }, { 0x6802, 2 }, { 0x6803, 2 }, { 0x6804, 1 }, { 0x6805, 6 }, { 0x6806, 1 },
    { 0x6808, 2 }, { 0x6809, 6 }, { 0x680A, 1 }, { 0x680B, 1 }, { 0x680E, 1 }, { 0x680F, 1 }, { 0x6810, 1 },
    { 0x6811, 2 }, { 0x6813, 1 }, { 0x6814, 1 }, { 0x6816, 5 }, { 0x6818, 1 }, { 0x6819, 1 }, { 0x681A, 1 },
    { 0x681C, 1 }, { 0x6820, 4 }, { 0x6822, 2 }, { 0x6823, 2 }, { 0x6824, 1 }, { 0x6825, 6 }, { 0x6826, 1 },
    { 0x6828, 2 }, { 0x6829, 6 }, { 0x682A, 1 }, { 0x682B, 1 }, { 0x682E, 1 }, { 0x682F, 1 }, { 0x6830, 1 },
    { 0x6831, 2 }, { 0x6833, 1 }, { 0x6834, 1 }, { 0x6836, 5 }, { 0x6838, 1 }, { 0x6839, 1 }, { 0x683A, 1 },
    { 0x683C, 1 },
};
#define VOICE_SAMPLES_MAX 96

static const char* const sRequiredHooks[] = {
    "OnPlayerActionHandler",
    "OnPlayerResolveAnim",
    "OnPlayerResolveMotionScale",
    "OnPlayerResolveLimbDraw",
    "OnPlayerPostLimbDraw",
    "OnActorPlaySfx",
    "OnInterfaceDrawEnd",
    "OnTransitionEnd",
    "OnSceneInit",
    "OnPlayerResolveAnimSite",
    "OnCollisionResolveDamage",
    "OnActorUpdate",
    "OnActorDraw",
    "OnActorDestroy",
    "OnOcarinaNote",
    "OnOcarinaPlaybackNote",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static ColliderCylinderInit sSpinCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_SPIN_MASTER, 0x00, SPIN_DAMAGE },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { SPIN_RADIUS, SPIN_HEIGHT, 0, { 0, 0, 0 } },
};

static EffectBlureInit2 sTrailInit = {
    0, 8, 0, { 255, 255, 255, 255 }, { 255, 255, 255, 64 }, { 255, 255, 255, 0 },    { 255, 255, 255, 0 }, 4,
    0, 2, 0, { 255, 255, 255, 255 }, { 255, 255, 255, 64 }, TRAIL_TYPE_MASTER_SWORD,
};

typedef enum {
    GERUDO_MOVE_NONE,
    GERUDO_MOVE_SWING,
    GERUDO_MOVE_CHARGE,
    GERUDO_MOVE_FRONT_SLASH,
    GERUDO_MOVE_JUMP_SLASH,
    GERUDO_MOVE_AERIAL,
    GERUDO_MOVE_FURY,
    GERUDO_MOVE_SHEATHE,
    GERUDO_MOVE_DRAW,
} GerudoMove;

typedef struct {
    const char* path;
    s16 first;
    s16 last;
    s16 frames;
    LinkAnimationHeader header;
    s16* data;
} CachedClip;

typedef struct {
    const s16* samples;
    s32 count;
} VoiceSample;

typedef struct {
    s32 sample;
    f32 position;
} VoiceSlot;

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
void func_80832318(Player* player);
void func_80833A20(Player* player, s32 newMeleeWeaponState);
void func_80837918(Player* player, s32 quadIndex, u32 dmgFlags);
void Player_SetupRoll(Player* player, PlayState* play);
s8 Player_ItemToItemAction(s32 item);
s32 Player_CanSpinAttack(Player* player);
s32 Player_GetMovementSpeedAndYaw(Player* player, f32* speedTarget, s16* yawTarget, f32 speedMode, PlayState* play);
void Player_SetInvulnerability(Player* player, s32 timer);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
void Player_PlayLandingSfx(Player* player);
void Player_RequestRumble(Player* player, s32 sourceStrength, s32 duration, s32 decreaseRate, s32 distSq);
s32 func_80842DF4(PlayState* play, Player* player);
s32 func_80843E64(PlayState* play, Player* player);
void func_8083DFE0(Player* player, f32* speedTarget, s16* yawTarget);
int Player_IsZTargeting(Player* player);
int16_t OTRGetRectDimensionFromLeftEdge(float v);
Gfx* Gfx_TextureIA8(Gfx* displayListHead, void* texture, s16 textureWidth, s16 textureHeight, s16 rectLeft, s16 rectTop,
                    s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy);
SoundFontSample* ResourceMgr_LoadAudioSample(const char* path);

static const SOHModApi* sApi;
static CachedClip sClipCache[CLIP_CACHE_MAX];
static s32 sClipCacheCount;
static LinkAnimationHeader sMaskOnAnim;
static s16* sMaskOnFrames;
static LinkAnimationHeader sBackflipClip;
static s16* sBackflipData;

static u8 sMove;
static u8 sRow;
static f32 sPrevFrame;
static bool sIsNextHitQueued;
static s32 sComboStep;
static s32 sComboIdleFrames;
static u8 sBladeMask;
static u32 sBladeDmgFlags;
static u8 sBladeDamage;
static bool sOwnsBladeFlags;
static bool sIsCylinderOn;
static u32 sCylinderDmgFlags;
static u8 sCylinderDamage;
static s16 sCylinderRadius;
static s16 sCylinderHeight;
static ColliderCylinder sSpinCylinder;
static bool sIsCylinderReady;
static s32 sTrailRight = -1;
static WeaponInfo sTrailRightInfo;
static s16 sLockedYaw;
static s8 sBladeVisibility = -1;
static s32 sPendingItem;
static s16 sAirFrames;
static s16 sJumpSlashFrames;
static bool sIsJumpLooping;
static bool sIsStanceLooping;

static bool sIsAPending;
static s16 sAFrames;
static bool sIsSprinting;

static s16 sRageMeter;
static u8 sFuryDamage;
static bool sHasFuryStruck;
static f32 sFuryPrevFrame = -1.0f;

static bool sIsOcarinaOut;
static bool sWasNoteStruck;
static u8 sLastNote = OCARINA_NOTE_NONE;
static bool sIsDesertOfferArmed;
static bool sIsDesertPromptShown;
static bool sIsDesertOfferToFortress;

static VoiceSample sVoiceSamples[VOICE_SAMPLES_MAX];
static s32 sVoiceSampleCount;
static s32 sVoiceFirstSample[ARRAY_COUNT(sVoices)];
static VoiceSlot sVoiceSlots[VOICE_SLOTS];
static volatile s32 sRequestedVoice = -1;
static volatile f32 sVoiceVolume = 1.0f;

static void GerudoAction(Player* player, PlayState* play);

static bool IsGerudo(void) {
    return sApi != NULL && sApi->IsFormActive(GERUDO_FORM_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BG_ON_GROUND) != 0;
}

static bool IsGerudoBusy(Player* player) {
    return player->actionFunc == GerudoAction;
}

static bool IsPressed(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].press.button, button);
}

static bool IsHeld(PlayState* play, u16 button) {
    return CHECK_BTN_ALL(play->state.input[0].cur.button, button);
}

static f32 GetStickMagnitude(PlayState* play) {
    f32 x = play->state.input[0].rel.stick_x;
    f32 y = play->state.input[0].rel.stick_y;

    return MIN(sqrtf(SQ(x) + SQ(y)), STICK_FULL);
}

static s8 GetStickDirection(Player* player) {
    return player->controlStickDirections[player->controlStickDataIndex];
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 &
             (PLAYER_STATE1_DEAD | PLAYER_STATE1_LOADING | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM |
              PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER |
              PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_ITEM_CS |
              PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED | PLAYER_STATE1_CLIMBING_LADDER |
              PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE));
}

static bool CanJumpFromFloor(PlayState* play, Player* player) {
    return play->roomCtx.curRoom.behaviorType1 != ROOM_BEHAVIOR_TYPE1_2 && player->actor.floorPoly != NULL &&
           func_80041D4C(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) != FLOOR_TYPE_NO_JUMP &&
           SurfaceType_GetSlope(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) != SLOPE_STEEP;
}

// 1..3 are the swords (Master, Kokiri, Biggoron); a stick or the hammer is not a pair of scimitars.
static bool HoldsSword(Player* player) {
    s32 weapon = Player_GetMeleeWeaponHeld(player);

    return weapon >= 1 && weapon <= 3;
}

static bool AreBladesOut(Player* player) {
    if (sBladeVisibility >= 0) {
        return sBladeVisibility != 0;
    }
    return HoldsSword(player);
}

// The guard counts as fighting too: it is the blades that block.
static bool IsFighter(Player* player) {
    return HoldsSword(player) || (player->stateFlags1 & PLAYER_STATE1_SHIELDING);
}

static s32 GetSwordTier(Player* player) {
    if (Player_HoldsBrokenKnife(player)) {
        return 1;
    }
    return CLAMP(Player_GetMeleeWeaponHeld(player) - 1, 0, 2);
}

// ---- clips ----

// NEI's in-place range loader: the MHR clips carry their root travel, so the root is pinned to the first
// frame's; nearest-frame resampling on purpose, since interpolating packed angles smears any that wraps.
static LinkAnimationHeader* LoadClip(const char* path, s16 first, s16 last, s16 frames) {
    for (s32 i = 0; i < sClipCacheCount; i++) {
        CachedClip* clip = &sClipCache[i];
        if (strcmp(clip->path, path) == 0 && clip->first == first && clip->last == last && clip->frames == frames) {
            return &clip->header;
        }
    }
    LinkAnimationHeader* source = ResourceMgr_LoadPlayerAnimAsHeader(path);
    if (source == NULL || source->common.frameCount <= 0 || sClipCacheCount >= CLIP_CACHE_MAX) {
        return NULL;
    }
    s32 count = source->common.frameCount;
    s32 begin = CLAMP(first < 0 ? 0 : first, 0, count - 1);
    s32 end = (last < 0 || last >= count) ? count - 1 : MAX(last, begin);
    s32 span = end - begin + 1;
    s32 outFrames = frames > 0 ? frames : span;
    s16* data = (s16*)malloc((size_t)outFrames * ANIM_VALUES_PER_FRAME * sizeof(s16));
    if (data == NULL) {
        return NULL;
    }
    const s16* frameData = (const s16*)source->segment + (size_t)begin * ANIM_VALUES_PER_FRAME;
    for (s32 f = 0; f < outFrames; f++) {
        s32 sourceFrame = MIN((f * span) / outFrames, span - 1);
        memcpy(&data[f * ANIM_VALUES_PER_FRAME], &frameData[sourceFrame * ANIM_VALUES_PER_FRAME],
               ANIM_VALUES_PER_FRAME * sizeof(s16));
    }
    for (s32 f = 1; f < outFrames; f++) {
        data[f * ANIM_VALUES_PER_FRAME + 0] = data[0];
        data[f * ANIM_VALUES_PER_FRAME + 2] = data[2];
    }
    CachedClip* clip = &sClipCache[sClipCacheCount++];
    clip->path = path;
    clip->first = first;
    clip->last = last;
    clip->frames = frames;
    clip->data = data;
    clip->header.common.frameCount = (s16)outFrames;
    clip->header.segment = data;
    return &clip->header;
}

static LinkAnimationHeader* LoadWholeClip(const char* path) {
    return LoadClip(path, -1, -1, 0);
}

// The backflip plays its lift at 1.5x and the rest at 2x; vanilla plays hops at 2/3, so both rates are
// compensated to come out in real time.
static LinkAnimationHeader* LoadBackflipClip(void) {
    const f32 compensation = 1.5f;
    const s16 split = 31;

    if (sBackflipClip.segment != NULL) {
        return &sBackflipClip;
    }
    LinkAnimationHeader* whole = LoadWholeClip(CLIP_BACKFLIP);
    if (whole == NULL) {
        return NULL;
    }
    s16 last = whole->common.frameCount - 1;
    s16 framesA = (s16)MAX((f32)split / (1.5f * compensation) + 0.5f, 1.0f);
    s16 framesB = (s16)MAX((f32)(last - split + 1) / (2.0f * compensation) + 0.5f, 1.0f);
    LinkAnimationHeader* partA = LoadClip(CLIP_BACKFLIP, 0, split - 1, framesA);
    LinkAnimationHeader* partB = LoadClip(CLIP_BACKFLIP, split, last, framesB);
    if (partA == NULL || partB == NULL) {
        return NULL;
    }
    s32 total = partA->common.frameCount + partB->common.frameCount;
    sBackflipData = (s16*)malloc((size_t)total * ANIM_VALUES_PER_FRAME * sizeof(s16));
    if (sBackflipData == NULL) {
        return NULL;
    }
    size_t sizeA = (size_t)partA->common.frameCount * ANIM_VALUES_PER_FRAME * sizeof(s16);
    memcpy(sBackflipData, partA->segment, sizeA);
    memcpy((u8*)sBackflipData + sizeA, partB->segment,
           (size_t)partB->common.frameCount * ANIM_VALUES_PER_FRAME * sizeof(s16));
    sBackflipClip.common.frameCount = (s16)total;
    sBackflipClip.segment = sBackflipData;
    return &sBackflipClip;
}

static void PlayClip(PlayState* play, Player* player, LinkAnimationHeader* clip, f32 speed, u8 mode, f32 morph) {
    if (clip == NULL) {
        return;
    }
    f32 last = Animation_GetLastFrame(clip);
    f32 start = speed >= 0.0f ? 0.0f : last;
    f32 end = speed >= 0.0f ? last : 0.0f;
    LinkAnimation_Change(play, &player->skelAnime, clip, speed, start, end, mode, morph);
    sPrevFrame = start;
}

// ---- locomotion ----

// Only while fighting: stowed, she moves exactly like Link. Walk and run feed OoT's blend rig, which samples
// the walk at 29 frames and the run at 20 of the same cycle, so both are resampled to those lengths.
static LinkAnimationHeader* GetFighterGroupClip(s32 group) {
    switch (group) {
        case PLAYER_ANIMGROUP_wait:
            return LoadWholeClip(CLIP_IDLE);
        case PLAYER_ANIMGROUP_walk:
            return LoadClip(CLIP_RUN, -1, -1, 29);
        case PLAYER_ANIMGROUP_run:
            return LoadClip(sIsSprinting ? CLIP_SPRINT : CLIP_RUN, -1, -1, 20);
        case PLAYER_ANIMGROUP_damage_run:
            return LoadClip(CLIP_DAMAGE_RUN, -1, -1, 20);
        case PLAYER_ANIMGROUP_defense:
            return LoadClip(CLIP_GUARD, 1, 20, 10);
        case PLAYER_ANIMGROUP_defense_wait:
            return LoadClip(CLIP_GUARD, 30, 30, 2);
        case PLAYER_ANIMGROUP_defense_end:
            return LoadClip(CLIP_GUARD, 31, 45, 8);
        case PLAYER_ANIMGROUP_landing:
        case PLAYER_ANIMGROUP_short_landing:
            return LoadWholeClip(CLIP_LANDING);
        case PLAYER_ANIMGROUP_landing_roll:
            return LoadClip(CLIP_ROLL, 0, 30, 0);
        default:
            return NULL;
    }
}

static void ResolveFighterAnim(int32_t group, int32_t animType, LinkAnimationHeader** anim) {
    (void)animType;
    if (!IsGerudo() || gPlayState == NULL || !IsFighter(GET_PLAYER(gPlayState))) {
        return;
    }
    LinkAnimationHeader* clip = GetFighterGroupClip(group);
    if (clip != NULL) {
        *anim = clip;
    }
}

static bool IsRolling(Player* player) {
    return player->skelAnime.animation == LoadClip(CLIP_ROLL, 0, 30, 0);
}

static void ScaleSprintAndRoll(Player* player, int32_t kind, float* scale) {
    (void)kind;
    if (!IsGerudo()) {
        return;
    }
    if (IsRolling(player)) {
        *scale *= ROLL_SCALE;
    } else if (sIsSprinting) {
        *scale *= SPRINT_SCALE;
    }
}

// Link's hops are 7 frames and OoT holds the last pose to touchdown; her slashes get a short length of their own.
static LinkAnimationHeader* GetHopClip(s32 index) {
    s32 direction = index / 3;
    s32 phase = index % 3;

    if (phase != 0) {
        return LoadClip(CLIP_LANDING, -1, -1, 8);
    }
    if (direction == PLAYER_STICK_DIR_BACKWARD) {
        return LoadBackflipClip();
    }
    return LoadClip(direction == PLAYER_STICK_DIR_LEFT ? CLIP_HOP_LEFT : CLIP_HOP_RIGHT, -1, -1, 12);
}

// Indices follow z_player.c: FidgetType * 2 + column, column 1 being the sword-in-hand one.
#define FIDGET_CRIT_START_ARMED (7 * 2 + 1)
#define FIDGET_CRIT_LOOP_ARMED (8 * 2 + 1)
#define SOFT_HIT_COUNT 4

static LinkAnimationHeader* GetSiteClip(int32_t site, int32_t index) {
    switch (site) {
        case SOH_PLAYER_ANIM_SITE_HOP:
            return GetHopClip(index);
        case SOH_PLAYER_ANIM_SITE_DAMAGE:
            return index < SOFT_HIT_COUNT ? LoadWholeClip(CLIP_SOFT_HIT) : NULL;
        case SOH_PLAYER_ANIM_SITE_FIDGET:
            if (index == FIDGET_CRIT_START_ARMED || index == FIDGET_CRIT_LOOP_ARMED) {
                return LoadWholeClip(CLIP_CRIT_IDLE);
            }
            return NULL;
        case SOH_PLAYER_ANIM_SITE_FALL:
            return IsFighter(GET_PLAYER(gPlayState)) ? LoadWholeClip(CLIP_FALL) : NULL;
        default:
            return NULL;
    }
}

static void ResolveSiteAnim(int32_t site, int32_t index, LinkAnimationHeader** anim) {
    if (!IsGerudo() || gPlayState == NULL) {
        return;
    }
    LinkAnimationHeader* clip = GetSiteClip(site, index);
    if (clip != NULL) {
        *anim = clip;
    }
}

// ---- rage ----

// Magic is the upgrade: none, single and double buy one, two and four bars of rage.
static s16 GetRageCapacity(void) {
    return (s16)(RAGE_BASE << CLAMP(gSaveContext.magicLevel, 0, 2));
}

// Only a live enemy pays: AT_HIT also fires on grass, pots and signs.
static bool IsRageWorthy(Actor* victim) {
    return victim != NULL && (victim->category == ACTORCAT_ENEMY || victim->category == ACTORCAT_BOSS);
}

static void GainRage(s32 hits) {
    s16 capacity = GetRageCapacity();
    sRageMeter = MIN(sRageMeter + MAX(capacity / RAGE_HITS_TO_FILL, 1) * hits, capacity);
}

// Read inside the action, before Player_UpdateCommon resets the quads' AT flags; Fury never refills itself.
static void BankBladeHits(Player* player) {
    bool isHit = false;

    if (sMove == GERUDO_MOVE_FURY) {
        return;
    }
    for (s32 quad = 0; quad < 2; quad++) {
        ColliderQuad* collider = &player->meleeWeaponQuads[quad];
        isHit = isHit || ((collider->base.atFlags & AT_HIT) && IsRageWorthy(collider->base.at));
    }
    if (sIsCylinderReady && (sSpinCylinder.base.atFlags & AT_HIT)) {
        isHit = isHit || IsRageWorthy(sSpinCylinder.base.at);
        sSpinCylinder.base.atFlags &= ~AT_HIT;
    }
    if (isHit) {
        GainRage(1);
    }
}

// A damage table caps a hit at 15; Fury's charged value is dealt as is, clamped to what the u8 health counter holds.
static void DealFuryDamage(Actor* victim, ColliderInfo* attack, float* damage) {
    if (!IsGerudo() || sMove != GERUDO_MOVE_FURY || victim == NULL || gPlayState == NULL) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    bool isFuryCollider = attack == &sSpinCylinder.info || attack == &player->meleeWeaponQuads[0].info ||
                          attack == &player->meleeWeaponQuads[1].info;
    if (isFuryCollider) {
        *damage = (float)MIN(sFuryDamage, 255 - victim->colChkInfo.damage);
    }
}

// ---- blades and body cylinder ----

static bool IsInWindow(const Window* window, f32 prev, f32 cur) {
    return window->end >= window->begin && cur >= (f32)window->begin && prev <= (f32)window->end;
}

static void SetBladeFlags(Player* player, u32 dmgFlags, u8 damage, bool ownsFlags) {
    sBladeDmgFlags = dmgFlags;
    sBladeDamage = damage;
    sOwnsBladeFlags = ownsFlags;
    func_80837918(player, 0, dmgFlags);
    func_80837918(player, 1, dmgFlags);
}

// meleeWeaponState is what Player_UpdateCommon registers the quads under and what plays the swing sound.
static void GateBlades(Player* player, u8 mask) {
    sBladeMask = mask;
    if (mask != 0 && player->meleeWeaponState == 0) {
        func_80833A20(player, 1);
    } else if (mask == 0) {
        player->meleeWeaponState = 0;
    }
}

static void TurnCylinderOn(u32 dmgFlags, u8 damage) {
    sIsCylinderOn = true;
    sCylinderDmgFlags = dmgFlags;
    sCylinderDamage = damage;
    sCylinderRadius = SPIN_RADIUS;
    sCylinderHeight = SPIN_HEIGHT;
}

static void SubmitCylinder(PlayState* play, Player* player) {
    if (!sIsCylinderReady) {
        Collider_InitCylinder(play, &sSpinCylinder);
        Collider_SetCylinder(play, &sSpinCylinder, &player->actor, &sSpinCylinderInit);
        sIsCylinderReady = true;
    }
    if (!sIsCylinderOn) {
        return;
    }
    sSpinCylinder.info.toucher.dmgFlags = sCylinderDmgFlags;
    sSpinCylinder.info.toucher.damage = sCylinderDamage;
    sSpinCylinder.dim.radius = sCylinderRadius;
    sSpinCylinder.dim.height = sCylinderHeight;
    sSpinCylinder.dim.yShift = (s16)(-sCylinderHeight / 2);
    sSpinCylinder.base.atFlags |= AT_ON;
    sSpinCylinder.dim.pos.x = (s16)player->actor.world.pos.x;
    sSpinCylinder.dim.pos.y = (s16)player->actor.world.pos.y;
    sSpinCylinder.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sSpinCylinder.base);
}

static void StopAllHits(Player* player) {
    GateBlades(player, 0);
    sIsCylinderOn = false;
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
    player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
}

// Vanilla lays both quads along the left blade; the left hook keeps quad 0 there and the right hand takes quad 1
// onto its own blade with the same geometry (z_player_lib.c's func_80090A28 and func_800906D4).
static void PlaceRightBlade(PlayState* play, Player* player) {
    static Vec3f sBases[3] = { { 0.0f, 400.0f, 0.0f }, { 0.0f, 1400.0f, -1000.0f }, { 0.0f, -400.0f, 1000.0f } };
    f32 length = Player_HoldsBrokenKnife(player) ? BROKEN_KNIFE_LENGTH : sBladeLengths[GetSwordTier(player)];
    Vec3f tips[3] = { { length, 400.0f, 0.0f },
                      { length + 1200.0f, -400.0f, 1000.0f },
                      { length + 1200.0f, 1400.0f, -1000.0f } };
    Vec3f tip;
    Vec3f base;

    Matrix_MultVec3f(&tips[0], &tip);
    Matrix_MultVec3f(&sBases[0], &base);
    if (func_80090480(play, NULL, &sTrailRightInfo, &tip, &base) && sTrailRight >= 0) {
        EffectBlure_AddVertex(Effect_GetByIndex(sTrailRight), &sTrailRightInfo.tip, &sTrailRightInfo.base);
    }
    Matrix_MultVec3f(&tips[2], &tip);
    Matrix_MultVec3f(&sBases[2], &base);
    ColliderQuad* quad = &player->meleeWeaponQuads[1];
    quad->info.toucher.dmgFlags = sBladeDmgFlags;
    if (sOwnsBladeFlags) {
        quad->info.toucher.damage = sBladeDamage;
    }
    func_80090480(play, quad, &player->meleeWeaponInfo[2], &tip, &base);
}

static void StopRightTrail(void) {
    if (sTrailRightInfo.active && sTrailRight >= 0) {
        EffectBlure_AddSpace(Effect_GetByIndex(sTrailRight));
    }
    sTrailRightInfo.active = 0;
}

static void GateBladesAtHands(PlayState* play, Player* player, int32_t limbIndex) {
    if (!IsGerudo()) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        if (!(sBladeMask & 1)) {
            player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
        } else if (sOwnsBladeFlags) {
            player->meleeWeaponQuads[0].info.toucher.damage = sBladeDamage;
        }
        return;
    }
    if (limbIndex != PLAYER_LIMB_R_HAND) {
        return;
    }
    if (sBladeMask & 2) {
        PlaceRightBlade(play, player);
    } else {
        player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
        StopRightTrail();
    }
}

// The blades are what block, shield or none: vanilla only stamps the shield quad for a real shield in hand.
static void GuardWithBlades(PlayState* play, Player* player, int32_t limbIndex) {
    Vec3f corners[4];

    if (!IsGerudo() || limbIndex != PLAYER_LIMB_R_HAND || !(player->stateFlags1 & PLAYER_STATE1_SHIELDING) ||
        player->currentShield != PLAYER_SHIELD_NONE) {
        return;
    }
    for (s32 i = 0; i < 4; i++) {
        Matrix_MultVec3f(&sHandShieldCorners[i], &corners[i]);
    }
    player->shieldQuad.base.colType = COLTYPE_METAL;
    Collider_SetQuadVertices(&player->shieldQuad, &corners[0], &corners[1], &corners[2], &corners[3]);
    CollisionCheck_SetAC(play, &play->colChkCtx, &player->shieldQuad.base);
    CollisionCheck_SetAT(play, &play->colChkCtx, &player->shieldQuad.base);
}

static void DrawAtHands(PlayState* play, Player* player, int32_t limbIndex) {
    GateBladesAtHands(play, player, limbIndex);
    GuardWithBlades(play, player, limbIndex);
}

// ---- moves ----

static void StartMove(PlayState* play, Player* player, u8 move) {
    Player_SetupAction(play, player, GerudoAction, 0);
    func_80832318(player);
    sMove = move;
    sLockedYaw = player->actor.shape.rot.y;
    sIsCylinderOn = false;
    sBladeMask = 0;
}

static void EndMove(PlayState* play, Player* player) {
    StopAllHits(player);
    StopRightTrail();
    sMove = GERUDO_MOVE_NONE;
    sBladeVisibility = -1;
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 &= ~(PLAYER_STATE2_SPIN_ATTACKING | PLAYER_STATE2_DISABLE_ROTATION_ALWAYS);
    player->actor.gravity = JUMP_GRAVITY;
    func_80839FFC(player, play);
}

static void Plant(Player* player) {
    player->actor.shape.rot.y = sLockedYaw;
    player->yaw = sLockedYaw;
    player->linearVelocity = 0.0f;
}

// A wall-checked step along her facing, for moves that travel without the stick.
static void StepForward(PlayState* play, Player* player, f32 distance) {
    f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
    f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
    Vec3f from = { player->actor.world.pos.x, player->actor.world.pos.y + 20.0f, player->actor.world.pos.z };
    Vec3f to = { from.x + sinYaw * (distance + WALL_MARGIN), from.y, from.z + cosYaw * (distance + WALL_MARGIN) };
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bgId;

    if (distance <= 0.0f ||
        BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bgId)) {
        return;
    }
    player->actor.world.pos.x += sinYaw * distance;
    player->actor.world.pos.z += cosYaw * distance;
}

static void EnsureRightTrail(PlayState* play) {
    if (sTrailRight < 0 || Effect_GetByIndex(sTrailRight) == NULL) {
        Effect_Add(play, &sTrailRight, EFFECT_BLURE2, 0, 0, &sTrailInit);
    }
}

// -- swings --

static s32 ChooseRow(PlayState* play, Player* player) {
    if (Player_CanSpinAttack(player)) {
        return ROW_QUICK_SPIN;
    }
    if (Player_IsZTargeting(player) && GetStickDirection(player) == PLAYER_STICK_DIR_FORWARD) {
        player->stateFlags2 |= PLAYER_STATE2_SWORD_LUNGE;
        return ROW_STAB;
    }
    s32 row = ROW_COMBO_1 + sComboStep;
    sComboStep = (sComboStep + 1) % COMBO_STEPS;
    sComboIdleFrames = 0;
    return row;
}

static void StartSwing(PlayState* play, Player* player, s32 row) {
    const SwingRow* swing = &sRows[row];
    s32 tier = GetSwordTier(player);
    bool isJump = row == ROW_JUMP_LAND;

    StartMove(play, player, GERUDO_MOVE_SWING);
    sRow = (u8)row;
    sIsNextHitQueued = false;
    player->unk_844 = 8;
    player->meleeWeaponAnimation = swing->mwa;
    player->yaw = player->actor.shape.rot.y;
    SetBladeFlags(player, sTierDmgFlags[tier][isJump ? 1 : 0], 0, false);
    EnsureRightTrail(play);
    PlayClip(play, player, LoadWholeClip(swing->clip), swing->speed, ANIMMODE_ONCE, -3.0f);
}

// The charge release throws its cross in a burst: that stretch plays three times faster than the rest.
static f32 GetSwingSpeed(const SwingRow* swing, f32 frame) {
    if (sRow == ROW_CHARGE_RELEASE && frame >= CHARGE_FAST_BEGIN && frame <= CHARGE_FAST_END) {
        return swing->speed * CHARGE_FAST_SCALE;
    }
    return swing->speed;
}

static void GateSwing(Player* player, const SwingRow* swing, f32 prev, f32 cur) {
    u8 mask = 0;

    if (IsInWindow(&swing->left, prev, cur)) {
        mask |= 1;
    }
    if (IsInWindow(&swing->right, prev, cur)) {
        mask |= 2;
    }
    // En_M_Thunder drops SPIN_ATTACKING on the release, and vanilla only lays a spin row's quads under it.
    if (sRow == ROW_CHARGE_RELEASE && mask != 0) {
        player->stateFlags2 |= PLAYER_STATE2_SPIN_ATTACKING;
    }
    if (swing->isSpin) {
        GateBlades(player, 0);
        sIsCylinderOn = mask != 0;
        if (sIsCylinderOn) {
            TurnCylinderOn(DMG_SPIN_MASTER, SPIN_DAMAGE);
        }
        return;
    }
    GateBlades(player, mask);
}

static void StartCharge(PlayState* play, Player* player);

// Past the swing's end B chains the next hit and A rolls out; B held to the very end becomes the charge.
static void RunSwing(PlayState* play, Player* player) {
    const SwingRow* swing = &sRows[sRow];
    f32 swingEnd = swing->swingEnd >= 0 ? (f32)swing->swingEnd : Animation_GetLastFrame(player->skelAnime.animation);

    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    if (func_80842DF4(play, player)) {
        sMove = GERUDO_MOVE_NONE;
        StopAllHits(player);
        return;
    }
    if ((player->stateFlags2 & PLAYER_STATE2_SWORD_LUNGE) && sPrevFrame <= 0.0f) {
        player->linearVelocity = LUNGE_SPEED;
        player->stateFlags2 &= ~PLAYER_STATE2_SWORD_LUNGE;
    }
    Math_StepToF(&player->linearVelocity, 0.0f, 5.0f);
    player->skelAnime.playSpeed = GetSwingSpeed(swing, player->skelAnime.curFrame);
    bool isDone = LinkAnimation_Update(play, &player->skelAnime);
    f32 cur = player->skelAnime.curFrame;
    GateSwing(player, swing, sPrevFrame, cur);
    sPrevFrame = cur;
    if (IsPressed(play, BTN_B)) {
        sIsNextHitQueued = true;
    }
    if (cur >= swingEnd && sRow <= ROW_COMBO_4) {
        if (sIsNextHitQueued) {
            StartSwing(play, player, ChooseRow(play, player));
            return;
        }
        if (IsPressed(play, BTN_A)) {
            StopAllHits(player);
            sMove = GERUDO_MOVE_NONE;
            Player_SetupRoll(player, play);
            return;
        }
    }
    if (!isDone) {
        return;
    }
    if (sRow <= ROW_COMBO_4 && IsHeld(play, BTN_B)) {
        StartCharge(play, player);
        return;
    }
    EndMove(play, player);
}

// -- charge --

static void SpawnChargeSparks(PlayState* play, Player* player) {
    Vec3f* waist = &player->bodyPartsPos[PLAYER_BODYPART_WAIST];

    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, waist->x, waist->y, waist->z, 0, 0, 0,
                Player_GetMeleeWeaponHeld(player) | CHARGE_SPARK_COST);
}

// The stance's opening is a short slice and the rest its held loop: OoT only accumulates once the opening ends.
static void StartCharge(PlayState* play, Player* player) {
    StartMove(play, player, GERUDO_MOVE_CHARGE);
    player->unk_858 = 0.0f;
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    PlayClip(play, player, LoadClip(CLIP_CHARGE, 0, 24, 14), 1.0f, ANIMMODE_ONCE, -6.0f);
    sIsStanceLooping = false;
    SpawnChargeSparks(play, player);
}

static void CreepWhileCharging(PlayState* play, Player* player) {
    f32 speedTarget;
    s16 yawTarget;

    Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, 0.0f, play);
    Math_AsymStepToF(&player->linearVelocity, speedTarget * 0.2f, 1.0f, 0.5f);
    if (speedTarget > 0.0f) {
        Math_ScaledStepToS(&player->yaw, yawTarget, 0x800);
    }
}

// Every release is the tumble cross, thrown forward by the lunge; En_M_Thunder answers the release itself.
static void ReleaseCharge(PlayState* play, Player* player) {
    player->stateFlags2 |= PLAYER_STATE2_SWORD_LUNGE;
    StartSwing(play, player, ROW_CHARGE_RELEASE);
    player->stateFlags2 |= PLAYER_STATE2_SPIN_ATTACKING;
    Player_SetInvulnerability(player, SPIN_INVULNERABILITY);
}

static void RunCharge(PlayState* play, Player* player) {
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    if (LinkAnimation_Update(play, &player->skelAnime) && !sIsStanceLooping) {
        sIsStanceLooping = true;
        PlayClip(play, player, LoadClip(CLIP_CHARGE, 25, 172, 60), 1.0f, ANIMMODE_LOOP, -4.0f);
    }
    if (!sIsStanceLooping) {
        return;
    }
    Math_StepToF(&player->unk_858, 1.0f, CHARGE_RATE);
    CreepWhileCharging(play, player);
    if (!IsHeld(play, BTN_B)) {
        ReleaseCharge(play, player);
    }
}

// -- front slash --

static void StartFrontSlash(PlayState* play, Player* player) {
    StartMove(play, player, GERUDO_MOVE_FRONT_SLASH);
    PlayClip(play, player, LoadWholeClip(CLIP_FRONT_SLASH), 1.2f, ANIMMODE_ONCE, -3.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
}

// Covers the jump slash's distance in its first 30 frames; A or B cancels it from the first frame.
static void RunFrontSlash(PlayState* play, Player* player) {
    Plant(player);
    bool isDone = LinkAnimation_Update(play, &player->skelAnime);
    f32 frame = player->skelAnime.curFrame;
    if (frame < FRONT_SLASH_TRAVEL_FRAMES) {
        StepForward(play, player, FRONT_SLASH_DISTANCE / FRONT_SLASH_TRAVEL_FRAMES);
    }
    sIsCylinderOn = frame >= 4.0f && frame <= 60.0f;
    if (sIsCylinderOn) {
        TurnCylinderOn(DMG_SPIN_MASTER, SPIN_DAMAGE);
    }
    if (IsPressed(play, BTN_B)) {
        sIsCylinderOn = false;
        StartSwing(play, player, ChooseRow(play, player));
        return;
    }
    if (isDone || IsPressed(play, BTN_A)) {
        EndMove(play, player);
    }
}

// -- jump slash --

// A big launch that flies at the lock-on, blades lit the whole way; with none she keeps vanilla's arc.
static void StartJumpSlash(PlayState* play, Player* player) {
    StartMove(play, player, GERUDO_MOVE_JUMP_SLASH);
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->meleeWeaponAnimation = PLAYER_MWA_JUMPSLASH_START;
    SetBladeFlags(player, sTierDmgFlags[GetSwordTier(player)][1], 0, false);
    EnsureRightTrail(play);
    PlayClip(play, player, LoadWholeClip(CLIP_JUMP_SLASH), 1.0f, ANIMMODE_ONCE, -3.0f);
    sIsJumpLooping = false;
    sJumpSlashFrames = 0;
    player->yaw = player->actor.shape.rot.y;
    player->linearVelocity = 5.0f;
    player->actor.velocity.y = 5.0f * JUMP_SLASH_LIFT;
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    player->hoverBootsTimer = 0;
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_L);
}

static void HomeOnTarget(Player* player, Actor* target) {
    f32 dx = target->world.pos.x - player->actor.world.pos.x;
    f32 dz = target->world.pos.z - player->actor.world.pos.z;
    f32 distance = MAX(sqrtf(SQ(dx) + SQ(dz)) - target->colChkInfo.cylRadius, 0.0f);

    player->yaw = Math_Vec3f_Yaw(&player->actor.world.pos, &target->world.pos);
    player->actor.shape.rot.y = player->yaw;
    player->linearVelocity =
        MIN(CLAMP(distance / JUMP_SLASH_LEAD, JUMP_SLASH_SPEED_MIN, JUMP_SLASH_SPEED_MAX), distance);
}

// The launch clip's frames 6-9 are two turns of the same cycle, so they loop for as long as the flight lasts.
static void RunJumpSlash(PlayState* play, Player* player) {
    f32 speedTarget;
    s16 yawTarget;

    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->actor.gravity = JUMP_GRAVITY;
    if (LinkAnimation_Update(play, &player->skelAnime) && !sIsJumpLooping) {
        sIsJumpLooping = true;
        PlayClip(play, player, LoadClip(CLIP_JUMP_SLASH, 6, 9, 0), 1.0f, ANIMMODE_LOOP, -3.0f);
    }
    GateBlades(player, 3);
    Actor* target = player->focusActor;
    if (target != NULL && target->update != NULL) {
        HomeOnTarget(player, target);
    } else {
        Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, 0.0f, play);
        func_8083DFE0(player, &speedTarget, &player->yaw);
    }
    if (++sJumpSlashFrames < 2 || !IsGrounded(player)) {
        return;
    }
    if (func_80843E64(play, player) >= 0) {
        Player_PlayLandingSfx(player);
        StartSwing(play, player, ROW_JUMP_LAND);
    }
}

// -- aerial slash --

static void StartAerial(PlayState* play, Player* player) {
    StartMove(play, player, GERUDO_MOVE_AERIAL);
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    SetBladeFlags(player, sTierDmgFlags[GetSwordTier(player)][0], 0, false);
    EnsureRightTrail(play);
    PlayClip(play, player, LoadWholeClip(CLIP_AERIAL), 1.2f, ANIMMODE_ONCE, -3.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
}

static void RunAerial(PlayState* play, Player* player) {
    static const Window sAerialWindow = { 2, 40 };

    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->actor.gravity = JUMP_GRAVITY;
    bool isDone = LinkAnimation_Update(play, &player->skelAnime);
    f32 cur = player->skelAnime.curFrame;
    GateBlades(player, IsInWindow(&sAerialWindow, sPrevFrame, cur) ? 3 : 0);
    sPrevFrame = cur;
    if (isDone || IsGrounded(player)) {
        EndMove(play, player);
    }
}

// -- Urbosa's Fury --

typedef struct {
    Color_RGBA8 gold;
    Color_RGBA8 orange;
    Color_RGBA8 blue;
    Color_RGBA8 deepBlue;
    Color_RGBA8 white;
    Color_RGBA8 paleBlue;
} FuryColors;

static FuryColors sFury = {
    { 255, 210, 45, 255 }, { 255, 90, 0, 180 },    { 85, 150, 255, 255 },
    { 25, 45, 190, 190 },  { 255, 255, 220, 255 }, { 180, 220, 255, 210 },
};

static bool HasCrossed(f32 prev, f32 cur, f32 mark) {
    return prev < mark && cur >= mark;
}

static Vec3f GetGroundPos(Player* player) {
    Vec3f pos = player->actor.world.pos;

    if (player->actor.floorHeight > BGCHECK_Y_MIN + 1.0f) {
        pos.y = player->actor.floorHeight;
    }
    return pos;
}

static void SpawnChargeBurst(PlayState* play, Player* player, s32 burst) {
    f32 progress = burst / 5.0f;
    f32 radius = 28.0f + progress * 48.0f;

    for (s32 i = 0; i < 3; i++) {
        s16 yaw = (s16)(burst * 0x1D00 + i * 0x5555 + play->gameplayFrames * 0x300);
        Vec3f pos = { player->actor.world.pos.x + Math_SinS(yaw) * radius,
                      player->actor.world.pos.y + 12.0f + i * 22.0f,
                      player->actor.world.pos.z + Math_CosS(yaw) * radius };
        bool isGold = (burst + i) % 3 == 0;
        Color_RGBA8* prim = isGold ? &sFury.gold : &sFury.blue;
        Color_RGBA8* env = isGold ? &sFury.orange : &sFury.deepBlue;
        Vec3f velocity = { -Math_SinS(yaw) * 1.4f, 1.2f + progress, -Math_CosS(yaw) * 1.4f };
        Vec3f accel = { 0.0f, -0.08f, 0.0f };
        EffectSsLightning_Spawn(play, &pos, prim, env, (s16)(75 + burst * 10), yaw, 10, 2);
        EffectSsKiraKira_SpawnFocused(play, &pos, &velocity, &accel, prim, &sFury.white, (s16)(260 + burst * 24), 14);
    }
}

static void SpawnCloudConvergence(PlayState* play, Player* player, s32 stage) {
    f32 radius = 82.0f - stage * 18.0f;

    for (s32 i = 0; i < 4; i++) {
        s16 yaw = (s16)(i * 0x4000 + stage * 0x1100);
        Vec3f pos = { player->actor.world.pos.x + Math_SinS(yaw) * radius,
                      player->actor.world.pos.y + 85.0f + stage * 45.0f,
                      player->actor.world.pos.z + Math_CosS(yaw) * radius };
        EffectSsLightning_Spawn(play, &pos, (i & 1) ? &sFury.gold : &sFury.blue, &sFury.paleBlue,
                                (s16)(125 + stage * 28), yaw, 12, 3);
    }
}

// The sky strike borrows OoT's storm layer: LAST switches a dry scene's lightning back off by itself.
static void StrikeFromSky(PlayState* play) {
    if (gLightningStrike.state != LIGHTNING_STRIKE_WAIT) {
        return;
    }
    Environment_AddLightningBolts(play, 3);
    gLightningStrike.flashRed = 210;
    gLightningStrike.flashGreen = 220;
    gLightningStrike.flashBlue = 255;
    gLightningStrike.flashAlphaTarget = 200;
    gLightningStrike.state = LIGHTNING_STRIKE_START;
    if (play->envCtx.lightningMode == LIGHTNING_MODE_OFF) {
        play->envCtx.lightningMode = LIGHTNING_MODE_LAST;
    }
}

static void ShakeGround(PlayState* play, s16 strength, s16 countdown) {
    s16 quake = Quake_Add(GET_ACTIVE_CAM(play), 3);

    Quake_SetSpeed(quake, 27767);
    Quake_SetQuakeValues(quake, strength, 0, 0, 0);
    Quake_SetCountdown(quake, countdown);
}

static void SpawnFuryImpact(PlayState* play, Player* player) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f ground = GetGroundPos(player);

    StrikeFromSky(play);
    for (s32 i = 0; i < 7; i++) {
        Vec3f column = { ground.x, ground.y + 18.0f + i * 48.0f, ground.z };
        EffectSsLightning_Spawn(play, &column, (i & 1) ? &sFury.white : &sFury.blue, &sFury.gold, (s16)(250 - i * 12),
                                (s16)(i * 0x1555), 13, 4);
    }
    EffectSsBlast_Spawn(play, &ground, &zero, &zero, &sFury.white, &sFury.orange, 80, 660, 42, 15);
    EffectSsBlast_Spawn(play, &ground, &zero, &zero, &sFury.paleBlue, &sFury.deepBlue, 110, 500, 24, 21);
    for (s32 i = 0; i < 12; i++) {
        s16 yaw = (s16)(i * 0x1555 + (i % 3) * 0x500);
        f32 reach = 24.0f + (i % 4) * 12.0f;
        Vec3f arc = { ground.x + Math_SinS(yaw) * reach, ground.y + 4.0f, ground.z + Math_CosS(yaw) * reach };
        Color_RGBA8* prim = (i % 4 == 0) ? &sFury.blue : &sFury.gold;
        f32 push = 2.0f + (i % 3) * 0.35f;
        Vec3f velocity = { Math_SinS(yaw) * push, 2.2f, Math_CosS(yaw) * push };
        Vec3f accel = { 0.0f, -0.16f, 0.0f };
        EffectSsLightning_Spawn(play, &arc, prim, &sFury.orange, 105, yaw, 11, 2);
        EffectSsKiraKira_SpawnFocused(play, &arc, &velocity, &accel, prim, &sFury.white, 420, 18);
    }
    ShakeGround(play, 7, 22);
    func_800AA000(0.0f, 255, 20, 150);
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_LIGHTNING);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_HAMMER_HIT);
}

static void SpawnResidualRing(PlayState* play, Player* player, s32 stage) {
    Vec3f ground = GetGroundPos(player);
    f32 radius = 58.0f + stage * 43.0f;

    for (s32 i = 0; i < 6; i++) {
        s16 yaw = (s16)(i * 0x2AAA + stage * 0x900 + play->gameplayFrames * 0x100);
        Vec3f pos = { ground.x + Math_SinS(yaw) * radius, ground.y + 5.0f, ground.z + Math_CosS(yaw) * radius };
        EffectSsLightning_Spawn(play, &pos, ((i + stage) & 1) ? &sFury.gold : &sFury.blue, &sFury.deepBlue,
                                (s16)(100 - stage * 10), yaw, 9, 2);
    }
}

// The room darkens to a storm, the strike cuts it for an instant flash, and a shallow blue dark drains away.
static f32 GetFuryDarkness(f32 frame) {
    if (frame >= 28.0f && frame < 68.0f) {
        return ((frame - 28.0f) / 40.0f) * 0.84f;
    }
    if (frame >= 68.0f && frame < FURY_STRIKE_FRAME) {
        return 0.84f;
    }
    if (frame >= 84.0f && frame < 116.0f) {
        return 0.52f * (1.0f - ((frame - 84.0f) / 32.0f));
    }
    return 0.0f;
}

// NEI's Urbosa's Fury timeline on the clip's own frames; every piece is a stock gameplay_keep effect.
static void TickFuryVfx(PlayState* play, Player* player, f32 frame) {
    static const f32 sConvergeMarks[] = { 57.0f, 67.0f, 75.0f };
    static const f32 sResidualMarks[] = { 88.0f, 98.0f, 108.0f };
    f32 prev = sFuryPrevFrame < 0.0f || frame < sFuryPrevFrame ? frame : sFuryPrevFrame;

    Environment_AdjustLights(play, GetFuryDarkness(frame), 850.0f, 0.2f, 0.9f);
    if (frame >= 34.0f && frame < FURY_STRIKE_FRAME) {
        func_8002F974(&player->actor, NA_SE_EN_BIRI_SPARK - SFX_FLAG);
    }
    for (s32 burst = 0; burst < 6; burst++) {
        if (HasCrossed(prev, frame, 36.0f + burst * 7.0f)) {
            SpawnChargeBurst(play, player, burst);
        }
    }
    for (s32 stage = 0; stage < 3; stage++) {
        if (HasCrossed(prev, frame, sConvergeMarks[stage])) {
            SpawnCloudConvergence(play, player, stage);
        }
        if (HasCrossed(prev, frame, sResidualMarks[stage])) {
            SpawnResidualRing(play, player, stage);
        }
    }
    if (HasCrossed(prev, frame, FURY_STRIKE_FRAME)) {
        SpawnFuryImpact(play, player);
    }
    sFuryPrevFrame = frame;
}

// The adjustment is a latch: an interrupted Fury has to hand the lights back, with intensity zero.
static void RestoreFuryLights(PlayState* play) {
    if (sFuryPrevFrame < 0.0f) {
        return;
    }
    Environment_AdjustLights(play, 0.0f, 850.0f, 0.2f, 0.9f);
    sFuryPrevFrame = -1.0f;
}

static void DimForImpact(PlayState* play, u8 dim) {
    play->envCtx.fillScreen = true;
    play->envCtx.screenFillColor[0] = 0;
    play->envCtx.screenFillColor[1] = 0;
    play->envCtx.screenFillColor[2] = 40;
    play->envCtx.screenFillColor[3] = dim;
}

static void FadeImpactDim(PlayState* play) {
    if (!play->envCtx.fillScreen) {
        return;
    }
    if (play->envCtx.screenFillColor[3] <= FURY_DIM_FADE) {
        play->envCtx.screenFillColor[3] = 0;
        play->envCtx.fillScreen = false;
        return;
    }
    play->envCtx.screenFillColor[3] -= FURY_DIM_FADE;
}

// The whole meter is spent, and the blast hits as hard as it was full.
static void StartFury(PlayState* play, Player* player) {
    f32 charge = (f32)sRageMeter / (f32)GetRageCapacity();

    sFuryDamage = (u8)(FURY_DAMAGE_MIN + (FURY_DAMAGE_MAX - FURY_DAMAGE_MIN) * charge);
    sRageMeter = 0;
    sComboStep = 0;
    sHasFuryStruck = false;
    player->stateFlags1 &= ~PLAYER_STATE1_SHIELDING;
    StartMove(play, player, GERUDO_MOVE_FURY);
    SetBladeFlags(player, FURY_DMG_FLAGS, sFuryDamage, true);
    EnsureRightTrail(play);
    PlayClip(play, player, LoadWholeClip(CLIP_FURY), 2.4f, ANIMMODE_ONCE, -6.0f);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_AUTO_JUMP);
}

// The blast is the collider, growing out of her from the first frame; she rides it out untouchable.
static void RunFury(PlayState* play, Player* player) {
    static const Window sFuryWindows[2] = { { 20, 28 }, { 80, 110 } };

    Plant(player);
    player->invincibilityTimer = MIN(player->invincibilityTimer, -FURY_INVULNERABILITY);
    bool isDone = LinkAnimation_Update(play, &player->skelAnime);
    f32 frame = player->skelAnime.curFrame;
    bool isBladeLive =
        IsInWindow(&sFuryWindows[0], sPrevFrame, frame) || IsInWindow(&sFuryWindows[1], sPrevFrame, frame);
    GateBlades(player, isBladeLive ? 1 : 0);
    sPrevFrame = frame;
    f32 grow = MIN(frame / FURY_BLAST_FRAMES, 1.0f);
    TurnCylinderOn(FURY_DMG_FLAGS, sFuryDamage);
    sCylinderRadius = (s16)(SPIN_RADIUS + (FURY_BLAST_RADIUS - SPIN_RADIUS) * grow);
    sCylinderHeight = (s16)(SPIN_HEIGHT + (FURY_BLAST_HEIGHT - SPIN_HEIGHT) * grow);
    TickFuryVfx(play, player, frame);
    if (!sHasFuryStruck && frame >= FURY_STRIKE_FRAME) {
        sHasFuryStruck = true;
        Player_RequestRumble(player, 255, 20, 150, 0);
        DimForImpact(play, FURY_DIM);
    }
    if (isDone) {
        RestoreFuryLights(play);
        EndMove(play, player);
    }
}

// -- sheathe and draw --

// The item changes the way OoT changes it: id, action and model group together, or it loops the change.
static void HandOver(Player* player, s32 item) {
    player->heldItemId = item;
    player->heldItemAction = item == ITEM_NONE ? PLAYER_IA_NONE : Player_ItemToItemAction(item);
    func_8008EC70(player);
}

static void StartSheathe(PlayState* play, Player* player) {
    StartMove(play, player, GERUDO_MOVE_SHEATHE);
    sBladeVisibility = 1;
    sPendingItem = ITEM_NONE;
    PlayClip(play, player, LoadWholeClip(CLIP_SHEATHE), DRAW_SPEED, ANIMMODE_ONCE, -3.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_PUTAWAY);
}

// Drawing is the sheathe run backwards, so the blades come back at the frame they left.
static void StartDraw(PlayState* play, Player* player, s32 item) {
    StartMove(play, player, GERUDO_MOVE_DRAW);
    sBladeVisibility = 0;
    sPendingItem = item;
    PlayClip(play, player, LoadWholeClip(CLIP_SHEATHE), -DRAW_SPEED, ANIMMODE_ONCE, -3.0f);
}

static void RunSheathe(PlayState* play, Player* player) {
    Plant(player);
    bool isDone = LinkAnimation_Update(play, &player->skelAnime);
    if (player->skelAnime.curFrame >= SHEATHE_HIDE_FRAME) {
        sBladeVisibility = 0;
    }
    if (isDone) {
        HandOver(player, sPendingItem);
        EndMove(play, player);
    }
}

static void RunDraw(PlayState* play, Player* player) {
    Plant(player);
    bool isDone = LinkAnimation_Update(play, &player->skelAnime);
    if (sBladeVisibility == 0 && player->skelAnime.curFrame <= SHEATHE_HIDE_FRAME) {
        sBladeVisibility = 1;
        Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_PICKOUT);
    }
    if (isDone) {
        HandOver(player, sPendingItem);
        EndMove(play, player);
    }
}

static void GerudoAction(Player* player, PlayState* play) {
    BankBladeHits(player);
    switch (sMove) {
        case GERUDO_MOVE_SWING:
            RunSwing(play, player);
            break;
        case GERUDO_MOVE_CHARGE:
            RunCharge(play, player);
            break;
        case GERUDO_MOVE_FRONT_SLASH:
            RunFrontSlash(play, player);
            break;
        case GERUDO_MOVE_JUMP_SLASH:
            RunJumpSlash(play, player);
            break;
        case GERUDO_MOVE_AERIAL:
            RunAerial(play, player);
            break;
        case GERUDO_MOVE_FURY:
            RunFury(play, player);
            break;
        case GERUDO_MOVE_SHEATHE:
            RunSheathe(play, player);
            break;
        case GERUDO_MOVE_DRAW:
            RunDraw(play, player);
            break;
        default:
            EndMove(play, player);
            break;
    }
}

// ---- action overrides ----

static bool WantsFrontSlash(PlayState* play, Player* player) {
    if (sIsSprinting) {
        return true;
    }
    return player->focusActor == NULL && GetStickMagnitude(play) >= STICK_MOVING &&
           GetStickDirection(player) == PLAYER_STICK_DIR_FORWARD;
}

static int32_t HandleSlash(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_B) || !HoldsSword(player) || !CanAct(player) || !IsGrounded(player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (IsHeld(play, BTN_R)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    if (WantsFrontSlash(play, player)) {
        StartFrontSlash(play, player);
    } else {
        StartSwing(play, player, ChooseRow(play, player));
    }
    return SOH_FORM_ACTION_STARTED;
}

// Vanilla fires the roll on the press; telling a tap from a hold needs the release, so the press is kept here.
static int32_t HandleA(PlayState* play, Player* player) {
    if (!IsPressed(play, BTN_A) || !HoldsSword(player) || Player_IsZTargeting(player) || !IsGrounded(player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    sIsAPending = true;
    sAFrames = 0;
    return SOH_FORM_ACTION_BLOCKED;
}

// A kept back by HandleA reaches this handler too, which would hop without a lock-on. Hops stay vanilla:
// ResolveSiteAnim hands them her clips.
static int32_t HandleZTargetA(PlayState* play, Player* player) {
    if (!Player_IsZTargeting(player)) {
        return sIsAPending ? SOH_FORM_ACTION_BLOCKED : SOH_FORM_ACTION_VANILLA;
    }
    if (!IsPressed(play, BTN_A) || !CanAct(player) || !IsGrounded(player) || !CanJumpFromFloor(play, player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (GetStickDirection(player) > PLAYER_STICK_DIR_FORWARD || !HoldsSword(player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    StartJumpSlash(play, player);
    return SOH_FORM_ACTION_STARTED;
}

// Standing, B draws with the sheathe clip run backwards; running, OoT's own upper-body change keeps her legs going.
static void InterceptDraw(bool* should, va_list args) {
    s32 item = va_arg(args, s32);
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (player == NULL || !IsGerudo() || !*should || HoldsSword(player) || IsGerudoBusy(player)) {
        return;
    }
    if (item < ITEM_SWORD_KOKIRI || item > ITEM_SWORD_BGS || !IsGrounded(player) || !CanAct(player) ||
        fabsf(player->linearVelocity) > 4.0f) {
        return;
    }
    *should = false;
    StartDraw(gPlayState, player, item);
}

// ---- per-frame input ----

static void TrackA(PlayState* play, Player* player) {
    bool isEnabled = HoldsSword(player) && !IsGerudoBusy(player) && IsGrounded(player) && !IsRolling(player) &&
                     !(player->stateFlags1 & PLAYER_STATE1_SHIELDING);

    if (!isEnabled) {
        sIsAPending = false;
        sIsSprinting = false;
        return;
    }
    if (IsHeld(play, BTN_A)) {
        if (sIsAPending && ++sAFrames >= A_TAP_FRAMES) {
            sIsAPending = false;
            sIsSprinting = true;
        }
        return;
    }
    sIsSprinting = false;
    if (!sIsAPending) {
        return;
    }
    sIsAPending = false;
    if (!CanAct(player)) {
        return;
    }
    if (GetStickMagnitude(play) >= STICK_MOVING) {
        Player_SetupRoll(player, play);
    } else {
        StartSheathe(play, player);
    }
}

// R then B spends the meter on Urbosa's Fury; vanilla's crouch stab loses the same press.
static void TryFury(PlayState* play, Player* player) {
    if (!HoldsSword(player) || IsGerudoBusy(player) || !IsGrounded(player) || !CanAct(player) || sRageMeter <= 0) {
        return;
    }
    if (IsHeld(play, BTN_R) && IsPressed(play, BTN_B)) {
        StartFury(play, player);
    }
}

// The press that started a hop or a jump arrives on its first airborne frame too, so it waits a frame.
static void TryAerial(PlayState* play, Player* player) {
    sAirFrames = IsGrounded(player) ? 0 : MIN(sAirFrames + 1, 1000);
    if (!HoldsSword(player) || IsGerudoBusy(player) || IsGrounded(player) || !CanAct(player) ||
        sAirFrames < AERIAL_MIN_AIR_FRAMES || !IsPressed(play, BTN_A) || player->meleeWeaponState != 0 ||
        (player->stateFlags2 & PLAYER_STATE2_HOPPING)) {
        return;
    }
    StartAerial(play, player);
}

static void CountComboGap(Player* player) {
    if (sComboStep == 0 || (IsGerudoBusy(player) && sMove == GERUDO_MOVE_SWING)) {
        return;
    }
    if (++sComboIdleFrames >= COMBO_RESET_FRAMES) {
        sComboStep = 0;
        sComboIdleFrames = 0;
    }
}

// ---- body ----

static void DrawScimitars(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    (void)limbDList;
    (void)pos;
    if (!IsGerudo() || !AreBladesOut(player)) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_L_HAND || limbIndex == PLAYER_LIMB_R_HAND) {
        *dList = (Gfx*)sScimitarDL;
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = NULL;
    }
}

// ---- voice ----

static void LoadVoices(void) {
    char path[64];

    for (u32 voice = 0; voice < ARRAY_COUNT(sVoices); voice++) {
        sVoiceFirstSample[voice] = sVoiceSampleCount;
        for (u8 variant = 0; variant < sVoices[voice].variants && sVoiceSampleCount < VOICE_SAMPLES_MAX; variant++) {
            snprintf(path, sizeof(path), "custom/samples/gerudo_voice_%04x_%d", sVoices[voice].sfxId, variant);
            SoundFontSample* sample = ResourceMgr_LoadAudioSample(path);
            VoiceSample* slot = &sVoiceSamples[sVoiceSampleCount++];
            slot->samples = sample != NULL ? (const s16*)sample->sampleAddr : NULL;
            slot->count = sample != NULL ? (s32)(sample->size / sizeof(s16)) : 0;
        }
    }
}

static s32 FindVoice(u16 sfxId) {
    for (u32 voice = 0; voice < ARRAY_COUNT(sVoices); voice++) {
        if (sVoices[voice].sfxId == sfxId) {
            return (s32)voice;
        }
    }
    return -1;
}

// She never speaks with Link's voice: an id the pack has no clip for stays silent.
static void SpeakAsGerudo(Actor* actor, int32_t kind, uint16_t* sfxId, bool* handled) {
    (void)actor;
    if (!IsGerudo() || kind != SOH_ACTOR_SFX_VOICE) {
        return;
    }
    *handled = true;
    s32 voice = FindVoice(*sfxId);
    if (voice < 0) {
        return;
    }
    sVoiceVolume = CVarGetInteger("gSettings.Volume.Master", 100) / 100.0f *
                   (CVarGetInteger("gSettings.Volume.SFX", 100) / 100.0f);
    sRequestedVoice = sVoiceFirstSample[voice] + (s32)(Rand_ZeroOne() * sVoices[voice].variants * 0.999f);
}

// Audio thread: only the request and the slots are touched here.
static void MixVoices(s16* samples, u32 frameCount) {
    s32 request = sRequestedVoice;
    f32 volume = sVoiceVolume;

    if (request >= 0) {
        sRequestedVoice = -1;
        for (s32 slot = 0; slot < VOICE_SLOTS; slot++) {
            if (sVoiceSlots[slot].sample < 0) {
                sVoiceSlots[slot].sample = request;
                sVoiceSlots[slot].position = 0.0f;
                break;
            }
        }
    }
    for (s32 slot = 0; slot < VOICE_SLOTS; slot++) {
        VoiceSlot* voice = &sVoiceSlots[slot];
        const VoiceSample* sample = voice->sample >= 0 ? &sVoiceSamples[voice->sample] : NULL;
        for (u32 frame = 0; sample != NULL && frame < frameCount; frame++) {
            s32 index = (s32)voice->position;
            if (sample->samples == NULL || index >= sample->count) {
                voice->sample = -1;
                break;
            }
            s32 value = (s32)(sample->samples[index] * volume);
            samples[frame * 2 + 0] = (s16)CLAMP(samples[frame * 2 + 0] + value, -32768, 32767);
            samples[frame * 2 + 1] = (s16)CLAMP(samples[frame * 2 + 1] + value, -32768, 32767);
            voice->position += VOICE_ADVANCE;
        }
    }
}

static bool IsHoldingOcarina(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_OCARINA || player->rightHandType == PLAYER_MODELTYPE_RH_OOT;
}

// Runs inside the ocarina's own update; it only records the edge, the pose consumes it on the game frame.
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

static bool IsPlayingOcarinaClip(Player* player, const char* name) {
    const char* anim = (const char*)player->skelAnime.animation;

    return anim != NULL && ResourceMgr_OTRSigCheck((char*)anim) && strstr(anim, name) != NULL;
}

// Vanilla opens the ocarina when its intro clip ends. She has no instrument to draw, so a clip that ends at
// once stands in for it and she starts singing straight away, like MM's forms without a draw animation.
static bool SkipOcarinaIntro(PlayState* play, Player* player) {
    LinkAnimationHeader* sing = LoadWholeClip(CLIP_SING);

    if (sing == NULL || !IsPlayingOcarinaClip(player, "okarina_start")) {
        return false;
    }
    LinkAnimation_Change(play, &player->skelAnime, sing, 1.0f, 0.0f, 0.0f, ANIMMODE_ONCE, -6.0f);
    return true;
}

// Malon's song, parked between notes: each note sings on from where the last one stopped. It loops: the
// ocarina action reopens the ocarina whenever a clip reports its end, which would wipe the song's notes.
static void PoseSinging(PlayState* play, Player* player) {
    LinkAnimationHeader* sing = LoadWholeClip(CLIP_SING);

    if (sing == NULL) {
        return;
    }
    f32 last = Animation_GetLastFrame(sing);
    f32 singEnd = last - CLIP_FRAMES_PER_UPDATE;
    if (player->skelAnime.animation != sing || player->skelAnime.endFrame < last) {
        LinkAnimation_Change(play, &player->skelAnime, sing, 0.0f, 0.0f, last, ANIMMODE_LOOP, -6.0f);
    }
    if (sWasNoteStruck) {
        if (player->skelAnime.curFrame >= singEnd) {
            player->skelAnime.curFrame = 0.0f;
        }
        player->skelAnime.playSpeed = 1.0f;
    } else if (player->skelAnime.curFrame >= singEnd) {
        player->skelAnime.playSpeed = 0.0f;
    }
    sWasNoteStruck = false;
}

// The ocarina stays out across the message session, so the latch only lets go once no message is up.
static void UpdateOcarinaVoice(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsOcarinaOut = true;
    } else if (!isSessionOpen) {
        if (sIsOcarinaOut) {
            Audio_OcaSetInstrument(OCARINA_INSTRUMENT_OFF);
        }
        sIsOcarinaOut = false;
    }
    if (!sIsOcarinaOut || SkipOcarinaIntro(play, player) || !isSessionOpen) {
        return;
    }
    // Re-applied every frame: the message system sets the instrument again around opening, and the last write wins.
    Audio_OcaSetInstrument(OCARINA_INSTRUMENT_MALON);
    PoseSinging(play, player);
}

// She sings with empty hands; Ocarina bakes the instrument into the right one.
static void HideOcarina(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    (void)limbDList;
    (void)pos;
    if (!IsGerudo() || limbIndex != PLAYER_LIMB_R_HAND || !sIsOcarinaOut || !IsHoldingOcarina(player)) {
        return;
    }
    *dList = (Gfx*)(LINK_IS_ADULT ? sAdultEmptyHandDL : sChildEmptyHandDL);
}

// ---- rage meter ----

// The magic meter's own pieces, one row under it: amber filling, pulsing white once Fury is affordable.
static void DrawRageMeter(PlayState* play) {
    if (!IsGerudo()) {
        return;
    }
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 width = gSaveContext.magicLevel >= 2 ? MAGIC_DOUBLE_METER : MAGIC_NORMAL_METER;
    s32 lineLength = CVarGetInteger("gCosmetics.HUD.Hearts.LineLength", 10);
    s16 barY = R_MAGIC_BAR_SMALL_Y;
    s16 startX = OTRGetRectDimensionFromLeftEdge(R_MAGIC_BAR_X);
    s16 midX = OTRGetRectDimensionFromLeftEdge(R_MAGIC_BAR_X + 8);
    s16 fillX = OTRGetRectDimensionFromLeftEdge(R_MAGIC_FILL_X);
    s16 fillWidth = (s16)CLAMP((f32)sRageMeter / GetRageCapacity() * width, 0.0f, (f32)width);
    bool isReady = sRageMeter >= GetRageCapacity() / 4;
    u8 pulse = (u8)(200 + 55 * Math_SinS(play->gameplayFrames * 3000));

    if (lineLength != 0 && (gSaveContext.healthCapacity - 1) / 0x10 >= lineLength) {
        barY = R_MAGIC_BAR_LARGE_Y + (R_MAGIC_BAR_LARGE_Y - R_MAGIC_BAR_SMALL_Y + 2) *
                                         ((gSaveContext.healthCapacity - 1) / (0x10 * lineLength) - 1);
    }
    if (gSaveContext.magicLevel != 0) {
        barY += 12;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->magicAlpha);
    gDPSetEnvColor(OVERLAY_DISP++, 100, 50, 50, 255);
    OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, (void*)gMagicMeterEndTex, 8, 16, startX, barY, 8, 16, 1 << 10, 1 << 10);
    OVERLAY_DISP =
        Gfx_TextureIA8(OVERLAY_DISP, (void*)gMagicMeterMidTex, 24, 16, midX, barY, width, 16, 1 << 10, 1 << 10);
    gDPLoadTextureBlock(OVERLAY_DISP++, gMagicMeterEndTex, G_IM_FMT_IA, G_IM_SIZ_8b, 8, 16, 0, G_TX_MIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 3, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, (startX + width + 8) << 2, barY << 2, (startX + width + 16) << 2,
                            (barY + 16) << 2, G_TX_RENDERTILE, 256, 0, 1 << 10, 1 << 10);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE);
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);
    if (isReady) {
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, pulse, pulse, 255, interfaceCtx->magicAlpha);
    } else {
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 240, 140, 20, interfaceCtx->magicAlpha);
    }
    gDPLoadMultiBlock_4b(OVERLAY_DISP++, gMagicMeterFillTex, 0, G_TX_RENDERTILE, G_IM_FMT_I, 16, 16, 0,
                         G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                         G_TX_NOLOD);
    if (fillWidth > 0) {
        gSPWideTextureRectangle(OVERLAY_DISP++, fillX << 2, (barY + 3) << 2, (fillX + fillWidth) << 2, (barY + 10) << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- desert ----

static bool IsSceneSettled(PlayState* play) {
    return play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF;
}

// The wasteland's wipes wait on the sandstorm's own alphas, which only advance while it is drawn: forcing it off
// mid-wipe would hang the transition, so it only goes off once the scene has settled.
static void ClearSandstorm(PlayState* play) {
    if (play->sceneNum == SCENE_HAUNTED_WASTELAND && IsSceneSettled(play)) {
        play->envCtx.sandstormState = SANDSTORM_OFF;
    }
}

// Every arrival in the wasteland, lost loop included, offers the crossing to the opposite side.
static void ArmDesertOffer(int16_t sceneNum) {
    sIsDesertOfferArmed = false;
    sIsDesertPromptShown = false;
    if (sceneNum != SCENE_HAUNTED_WASTELAND || !IsGerudo() || gPlayState == NULL) {
        return;
    }
    gPlayState->envCtx.sandstormState = SANDSTORM_OFF;
    sIsDesertOfferToFortress = gPlayState->curSpawn != 0;
    sIsDesertOfferArmed = true;
}

static void CrossDesert(PlayState* play) {
    play->nextEntranceIndex =
        sIsDesertOfferToFortress ? ENTR_GERUDOS_FORTRESS_GATE_EXIT : ENTR_DESERT_COLOSSUS_EAST_EXIT;
    play->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
    play->transitionTrigger = TRANS_TRIGGER_START;
}

static void CloseDesertPrompt(PlayState* play, Player* player) {
    Message_CloseTextbox(play);
    play->msgCtx.msgMode = MSGMODE_TEXT_DONE;
    sIsDesertPromptShown = false;
    player->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;
}

// B skips it; the choice box answers Yes or No.
static void TickDesertOffer(PlayState* play, Player* player) {
    if (sIsDesertPromptShown) {
        if (IsPressed(play, BTN_B)) {
            CloseDesertPrompt(play, player);
            return;
        }
        if (Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE || !Message_ShouldAdvance(play)) {
            return;
        }
        bool wantsToCross = play->msgCtx.choiceIndex == 0;
        CloseDesertPrompt(play, player);
        if (wantsToCross) {
            CrossDesert(play);
        }
        return;
    }
    bool isFree = play->msgCtx.msgMode == MSGMODE_NONE && IsSceneSettled(play) &&
                  !(player->stateFlags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING));
    if (!sIsDesertOfferArmed || !isFree || !SOH_MOD_API_HAS(sApi, ShowTextbox)) {
        return;
    }
    sIsDesertOfferArmed = false;
    const char* text = sIsDesertOfferToFortress ? "Cross the desert to the&Gerudo Fortress?\x1B%gYes&No%w"
                                                : "Cross the desert to the&Desert Colossus?\x1B%gYes&No%w";
    sIsDesertPromptShown = sApi->ShowTextbox(play, text, true);
    if (sIsDesertPromptShown) {
        player->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
    }
}

// Nabooru's Silver Gauntlets are a Gerudo's grip: silver rocks and huge blocks, not the golden pillars.
static void ResolveGerudoStrength(bool* should, va_list args) {
    int32_t* strength = va_arg(args, int32_t*);

    if (!IsGerudo()) {
        return;
    }
    *strength = PLAYER_STR_SILVER_G;
    *should = false;
}

static void BefriendGerudos(bool* should, va_list args) {
    (void)args;
    if (IsGerudo()) {
        *should = true;
    }
}

// The mask grants passage while it is worn, not the card.
static void WithholdMembershipCard(bool* should, va_list args) {
    (void)args;
    if (IsGerudo()) {
        *should = false;
    }
}

static void SpareFromJail(bool* should, va_list args) {
    (void)args;
    if (IsGerudo()) {
        *should = false;
    }
}

// ---- charge cone ----

// Her release is not the ring around her but its forward third, 1.5x wider, thrown when the blades cross.
#define CONE_RADIUS 1875.0f
#define CONE_HEIGHT 900.0f
#define CONE_ARC 0x5555
#define CONE_SEGMENTS 10
#define CONE_VERTEX_COUNT ((CONE_SEGMENTS + 1) * 2)
#define CONE_ARM_CAP 24
#define CONE_RAGE_HITS 2
// Every En_M_Thunder DL is drawn under this scale.
#define THUNDER_UNIT 0.02f

static EnMThunder* sCone;
static u8 sConeWaitFrames;
static bool sIsConeArmed;

// En_M_Thunder sets its target scale only on the release, so a non-zero one tells a release from a charge.
static bool IsThunderReleased(EnMThunder* thunder) {
    return thunder->unk_1C9 != 0;
}

static bool IsReleaseSwinging(void) {
    return sMove == GERUDO_MOVE_SWING && sRow == ROW_CHARGE_RELEASE;
}

static bool ShouldArmCone(Player* player) {
    return !IsReleaseSwinging() || player->skelAnime.curFrame >= CHARGE_FAST_BEGIN || sConeWaitFrames >= CONE_ARM_CAP;
}

// Undoes the tick the actor just ran, so its 16-frame life only starts with the throw.
static void HoldCone(EnMThunder* thunder) {
    thunder->unk_1AC = 1.0f;
    thunder->unk_1B0 = 0.0f;
    thunder->unk_1C4 = 8;
    Actor_SetScale(&thunder->actor, 0.0f);
    thunder->collider.base.atFlags &= ~AT_ON;
    sConeWaitFrames++;
}

// Runs after the actor submitted its ring collider; the AT pass reads it later in the frame, so it hits as the cone.
static void ShapeConeCollider(EnMThunder* thunder) {
    f32 radius = CONE_RADIUS * THUNDER_UNIT * thunder->actor.scale.x;
    s16 facing = thunder->actor.shape.rot.y + 0x8000;

    thunder->collider.base.atFlags |= AT_ON;
    thunder->collider.dim.radius = (s16)(radius * 0.62f);
    thunder->collider.dim.height = 70;
    thunder->collider.dim.yShift = -35;
    Collider_UpdateCylinder(&thunder->actor, &thunder->collider);
    thunder->collider.dim.pos.x += (s16)(Math_SinS(facing) * radius * 0.55f);
    thunder->collider.dim.pos.z += (s16)(Math_CosS(facing) * radius * 0.55f);
}

// A quick spin is released on spawn and keeps its ring; only the charged release becomes the cone.
static void ShapeChargeRelease(void* actor) {
    EnMThunder* thunder = (EnMThunder*)actor;

    if (!IsGerudo() || gPlayState == NULL || !IsThunderReleased(thunder)) {
        return;
    }
    if (thunder != sCone) {
        if (!IsReleaseSwinging()) {
            return;
        }
        sCone = thunder;
        sConeWaitFrames = 0;
        sIsConeArmed = false;
    }
    if (!sIsConeArmed && !ShouldArmCone(GET_PLAYER(gPlayState))) {
        HoldCone(thunder);
        return;
    }
    sIsConeArmed = true;
    ShapeConeCollider(thunder);
}

static void ForgetCone(void* actor) {
    if (actor == sCone) {
        sCone = NULL;
    }
}

static void PayConeRage(Actor* victim, ColliderInfo* attack, float* damage) {
    (void)damage;
    if (sCone != NULL && attack == &sCone->collider.info && IsRageWorthy(victim)) {
        GainRage(CONE_RAGE_HITS);
    }
}

static void SetConeVertex(Vtx* vertex, s16 x, s16 y, s16 z, s16 u, s16 v) {
    vertex->v.ob[0] = x;
    vertex->v.ob[1] = y;
    vertex->v.ob[2] = z;
    vertex->v.flag = 0;
    vertex->v.tc[0] = u;
    vertex->v.tc[1] = v;
    vertex->v.cn[0] = 255;
    vertex->v.cn[1] = 255;
    vertex->v.cn[2] = 255;
    vertex->v.cn[3] = 255;
}

// Two tiles across the arc on the ring's own scroll; tc is s10.5, so the endless counter is wrapped into one tile.
static Vtx* BuildConeWall(PlayState* play, f32 scroll) {
    Vtx* vertices = (Vtx*)Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * CONE_VERTEX_COUNT);
    f32 wrapped = scroll - 64.0f * floorf(scroll / 64.0f);

    if (vertices == NULL) {
        return NULL;
    }
    for (s32 i = 0; i <= CONE_SEGMENTS; i++) {
        f32 t = (f32)i / CONE_SEGMENTS;
        s16 angle = (s16)(-(CONE_ARC / 2) + (s32)(CONE_ARC * t));
        s16 x = (s16)(Math_SinS(angle) * CONE_RADIUS);
        s16 z = (s16)(Math_CosS(angle) * CONE_RADIUS);
        s16 u = (s16)((t * 2.0f * 64.0f + wrapped) * 32.0f);

        SetConeVertex(&vertices[i * 2], x, (s16)(-CONE_HEIGHT / 2), z, u, 32 * 32);
        SetConeVertex(&vertices[i * 2 + 1], x, (s16)(CONE_HEIGHT / 2), z, u, 0);
    }
    return vertices;
}

// gameplay_keep exports no symbol for the ring's texture, so its DLs run squashed to a point first: nothing is
// rasterised, but they leave the ring's texture, tiles and combiner loaded for the arc drawn after them.
static void DrawCone(EnMThunder* thunder, PlayState* play) {
    Vtx* vertices = BuildConeWall(play, thunder->unk_1B4);
    bool isGreat = thunder->unk_1C6 == 0;
    u8 alpha = (u8)(thunder->unk_1B0 * 255.0f);

    if (vertices == NULL) {
        return;
    }
    uintptr_t scrollDL = (uintptr_t)Gfx_TwoTexScrollEx(
        play->state.gfxCtx, 0, 0xFF - ((u8)(s32)(thunder->unk_1B4 * 30) & 0xFF), 0, 0x40, 0x20, 1,
        0xFF - ((u8)(s32)(thunder->unk_1B4 * 20) & 0xFF), 0, 8, 8, -30, 0, -20, 0);
    Gfx* ringWall = (Gfx*)(isGreat ? gSpinAttack3DL : gSpinAttack1DL);
    Gfx* ringRim = (Gfx*)(isGreat ? gSpinAttack4DL : gSpinAttack2DL);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08, scrollDL);
    if (isGreat) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, alpha);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, alpha);
    }
    Matrix_Push();
    Matrix_Scale(0.0f, 0.0f, 0.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, ringWall);
    gSPDisplayList(POLY_XLU_DISP++, ringRim);
    Matrix_Pop();

    // The actor faces behind her (+0x8000), so half a turn puts the arc in front.
    Matrix_Scale(THUNDER_UNIT, THUNDER_UNIT, THUNDER_UNIT, MTXMODE_APPLY);
    Matrix_RotateY(M_PI, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_CULL_BOTH);
    gSPVertex(POLY_XLU_DISP++, (uintptr_t)vertices, CONE_VERTEX_COUNT, 0);
    for (s32 i = 0; i < CONE_SEGMENTS; i++) {
        s32 bottom = i * 2;
        gSP2Triangles(POLY_XLU_DISP++, bottom, bottom + 1, bottom + 2, 0, bottom + 1, bottom + 3, bottom + 2, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawChargeRelease(Actor* actor, PlayState* play, bool* drawVanilla) {
    if (sCone == NULL || actor != &sCone->actor) {
        return;
    }
    *drawVanilla = false;
    if (sIsConeArmed) {
        DrawCone(sCone, play);
    }
}

// ---- lifetime ----

static void ResetMoves(PlayState* play, Player* player) {
    if (player != NULL && play != NULL && IsGerudoBusy(player)) {
        EndMove(play, player);
    }
    sMove = GERUDO_MOVE_NONE;
    sBladeMask = 0;
    sIsCylinderOn = false;
    sBladeVisibility = -1;
    sIsAPending = false;
    sIsSprinting = false;
    sComboStep = 0;
    sComboIdleFrames = 0;
}

static void EnterGerudo(PlayState* play, Player* player) {
    ResetMoves(play, player);
}

static void ExitGerudo(PlayState* play, Player* player) {
    ResetMoves(play, player);
    RestoreFuryLights(play);
    sRageMeter = 0;
    if (sIsOcarinaOut) {
        Audio_OcaSetInstrument(OCARINA_INSTRUMENT_OFF);
        sIsOcarinaOut = false;
    }
    if (sIsDesertPromptShown) {
        CloseDesertPrompt(play, player);
    }
    sIsDesertOfferArmed = false;
    if (sTrailRight >= 0) {
        Effect_Delete(play, sTrailRight);
        sTrailRight = -1;
    }
}

// Damage, a cutscene or the water can take the player from a move without passing through its end.
static void DropInterruptedMove(PlayState* play, Player* player) {
    if (sMove == GERUDO_MOVE_NONE || IsGerudoBusy(player)) {
        return;
    }
    if (sMove == GERUDO_MOVE_FURY) {
        RestoreFuryLights(play);
    }
    sMove = GERUDO_MOVE_NONE;
    sBladeVisibility = -1;
    StopAllHits(player);
    StopRightTrail();
}

static void UpdateGerudo(PlayState* play, Player* player) {
    DropInterruptedMove(play, player);
    FadeImpactDim(play);
    CountComboGap(player);
    TrackA(play, player);
    TryFury(play, player);
    TryAerial(play, player);
    SubmitCylinder(play, player);
    UpdateOcarinaVoice(play, player);
    ClearSandstorm(play);
    TickDesertOffer(play, player);
}

static void ForgetScene(int16_t sceneNum) {
    (void)sceneNum;
    sIsCylinderReady = false;
    sTrailRight = -1;
    sTrailRightInfo.active = 0;
    sFuryPrevFrame = -1.0f;
    sIsDesertPromptShown = false;
    sCone = NULL;
    ResetMoves(NULL, NULL);
}

// ---- mask ----

static bool CanWearMask(Player* player, PlayState* play) {
    return IsGrounded(player) &&
           !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_CUTSCENE));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(GERUDO_FORM_KEY);
}

// A vanilla GetItemEntry has no draw callback of its own, and the registry reads NULL as "no model".
static void DrawMaskGetItem(PlayState* play, GetItemEntry* entry) {
    (void)entry;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_26Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGetItemDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

GetItemEntry ItemTable_RetrieveEntry(s16 modIndex, s16 getItemID);

static void RegisterMask(void) {
    SOHCustomItemDefinition mask = Z64Items_Define(GERUDO_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, GERUDO_MASK_PAGE, GERUDO_MASK_SLOT, GERUDO_MASK_WHEEL_PRIORITY);
    mask.getItemEntry = ItemTable_RetrieveEntry(MOD_NONE, GI_MASK_GERUDO);
    mask.getItemEntry.textId = 0;
    mask.getItemEntry.drawFunc = DrawMaskGetItem;
    Z64Items_SetReplaces(&mask, ITEM_MASK_GERUDO);
    Z64Items_SetVanillaMode(&mask, SOH_VANILLA_ITEM_REPLACE, RG_GERUDO_MASK);
    Z64Items_SetTextbox(&mask, "You got the %rGerudo Mask%w!&The face of a desert warrior.^Wear it with %y\xA1%w to "
                               "become a %rGerudo%w: twin scimitars, a blade guard, and the fury of Urbosa.");
    Z64Items_SetPauseText(&mask, "%rGerudo Mask&%wPress %y\xA1%w to become a Gerudo.&%y\xA0%w: dual blades  Hold "
                                 "%y\x9F%w: sprint  %y\xA3%w+%y\xA0%w: Urbosa's Fury");
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
    { SOH_PLAYER_ACTION_MELEE, HandleSlash },
    { SOH_PLAYER_ACTION_ROLL, HandleA },
    { SOH_PLAYER_ACTION_ZTARGET_A, HandleZTargetA },
};

static void RegisterForm(void) {
    SOHFormDefinition gerudo = { 0 };

    gerudo.structSize = sizeof(gerudo);
    gerudo.key = GERUDO_FORM_KEY;
    gerudo.label = "Gerudo";
    gerudo.kind = SOH_FORM_KIND_LINK;
    gerudo.item = GERUDO_MASK_KEY;
    gerudo.modelPath = GERUDO_MODEL_PATH;
    gerudo.motionScale = 1.0f;
    gerudo.transformAnim = LoadMaskOnAnim();
    gerudo.transformMask = sMaskDL;
    gerudo.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    gerudo.actions = sActions;
    gerudo.actionCount = ARRAY_COUNT(sActions);
    gerudo.onEnter = EnterGerudo;
    gerudo.onExit = ExitGerudo;
    gerudo.update = UpdateGerudo;
    sApi->RegisterForm(&gerudo);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    for (s32 slot = 0; slot < VOICE_SLOTS; slot++) {
        sVoiceSlots[slot].sample = -1;
    }
    LoadVoices();
    RegisterMask();
    RegisterForm();
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveAnim, ResolveFighterAnim);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, ScaleSprintAndRoll);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, DrawScimitars);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawAtHands);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, SpeakAsGerudo);
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawRageMeter);
    SOH_REGISTER_HOOK(sApi, OnTransitionEnd, ArmDesertOffer);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveAnimSite, ResolveSiteAnim);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, HideOcarina);
    SOH_REGISTER_HOOK(sApi, OnOcarinaNote, NoteStruck);
    SOH_REGISTER_HOOK(sApi, OnOcarinaPlaybackNote, PlaybackNoteStruck);
    SOH_REGISTER_HOOK(sApi, OnCollisionResolveDamage, DealFuryDamage);
    SOH_REGISTER_HOOK(sApi, OnCollisionResolveDamage, PayConeRage);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_M_THUNDER, ShapeChargeRelease);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_EN_M_THUNDER, DrawChargeRelease);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDestroy, ACTOR_EN_M_THUNDER, ForgetCone);
    sApi->RegisterVB(VB_CHANGE_HELD_ITEM_AND_USE_ITEM, InterceptDraw);
    sApi->RegisterVB(VB_GERUDOS_BE_FRIENDLY, BefriendGerudos);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveGerudoStrength);
    sApi->RegisterVB(VB_GIVE_ITEM_GERUDO_MEMBERSHIP_CARD, WithholdMembershipCard);
    sApi->RegisterVB(VB_GERUDO_FIGHTER_THROW_LINK_TO_JAIL, SpareFromJail);
    if (SOH_MOD_API_HAS(sApi, RegisterAudioMix)) {
        sApi->RegisterAudioMix(MixVoices);
    }
}
