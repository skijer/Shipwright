/**
 * transformation_masks.h - MM Transformation Masks for OOT
 *
 * Uses MmPlayer struct with hook system for OOT integration.
 * MmPlayer_InitFromOot() copies OOT Player -> MmPlayer
 * MmPlayer_Update() runs REAL MM code on MmPlayer
 * MmPlayer_SyncToOot() copies MmPlayer -> OOT Player
 */

#ifndef TRANSFORMATION_MASKS_H
#define TRANSFORMATION_MASKS_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// MM Player Form Enum (from 2Ship z64player.h)
// =============================================================================

typedef enum MmPlayerTransformation {
    MM_PLAYER_FORM_FIERCE_DEITY = 0,
    MM_PLAYER_FORM_GORON = 1,
    MM_PLAYER_FORM_ZORA = 2,
    MM_PLAYER_FORM_DEKU = 3,
    MM_PLAYER_FORM_HUMAN = 4,
    MM_PLAYER_FORM_PIKACHU = 5,
    MM_PLAYER_FORM_GARO = 6,
    MM_PLAYER_FORM_GERUDO = 7,
    // Rito — Link-rigged bird body from soh.o2r (objects/forms/rito), same deal as
    // Gerudo: full transformation cutscene + form state, but every gameplay system
    // stays vanilla Link. Appended at the END on purpose — every table indexed by
    // this enum keeps its existing rows valid, so no other form can shift.
    MM_PLAYER_FORM_RITO = 8,
    // Keaton — Link-rigged fox body from soh.o2r (objects/forms/keaton). Same
    // arrangement as Rito: full transformation cutscene, gameplay stays vanilla.
    // Its three tails are NOT limbs (21 is the hard ceiling) — they are drawn as
    // appendages with their own matrices, like the Bunny Hood ears.
    MM_PLAYER_FORM_KEATON = 9,
    // Kafei — Link-rigged human body from soh.o2r (objects/forms/kafei). Promoted from
    // a visual skin: as a skin the engine could not see him at all
    // (TransformMasks_IsTransformedAny() returned 0), so every form-gated system skipped
    // him and each one would have needed its own strcmp. Closest to Fierce Deity of all
    // the custom forms — he mirrors Link's own skeleton and carries no animations of his
    // own, so the vanilla clips drive him unchanged. The only one shipping BOTH ages.
    MM_PLAYER_FORM_KAFEI = 10,
    MM_PLAYER_FORM_MAX = 11
} MmPlayerTransformation;

// OOT mask type enum (for transformation mask identification)
typedef enum TransformMaskId {
    TRANSFORM_MASK_NONE = 0,
    TRANSFORM_MASK_GORON,
    TRANSFORM_MASK_ZORA,
    TRANSFORM_MASK_DEKU,
    TRANSFORM_MASK_FIERCE_DEITY,
    TRANSFORM_MASK_PIKACHU, // Pokeball-triggered (the Keaton Mask now belongs to the Keaton SKIN form)
    TRANSFORM_MASK_GARO,
    TRANSFORM_MASK_GERUDO,
    TRANSFORM_MASK_RITO,        // ITEM_RITO_MASK (shares the Farore's Wind cell)
    TRANSFORM_MASK_KEATON_FORM, // Keaton Mask (OoT or MM copy)
    TRANSFORM_MASK_KAFEI        // Kafei Mask (MM copy)
} TransformMaskId;

// =============================================================================
// MmPlayer Struct - Minimal version for hook system
// Full struct is in soh/mods/mm_sources/z64player.h
// =============================================================================

// Forward declare the full struct (defined in mm_sources/z64player.h)
struct MmPlayer;

// Simplified MmPlayer for the hook system - contains only fields we need to sync
typedef struct MmPlayerCore {
    // === Actor base (synced from OOT Actor) ===
    Vec3f worldPos;
    Vec3f prevPos;
    Vec3s shapeRot; // shape.rot in MM
    f32 scale;

    // === Movement (key differences from OOT) ===
    f32 speedXZ;   // MM: speedXZ, OOT: linearVelocity
    f32 ySpeed;    // Vertical velocity
    s16 yaw;       // Current facing
    s16 targetYaw; // Target facing

    // === State flags ===
    u32 stateFlags1;
    u32 stateFlags2;
    u32 stateFlags3; // MM has u32, OOT has u8 - CRITICAL DIFFERENCE

    // === Transformation system (MM-only) ===
    MmPlayerTransformation transformation;     // Current form
    MmPlayerTransformation prevTransformation; // Previous form (for cutscene)
    s16 transformationTimer;                   // Cutscene timer

    // === Form-specific fields ===
    union {
        struct {
            s16 actionVar1; // av1.actionVar1 - for Goron roll charge
            s16 actionVar2; // av2.actionVar2
        } av;
        struct {
            f32 rollSpeed;  // Goron roll speed
            u8 rollState;   // Goron roll state
            u8 spikeActive; // Spikes out?
        } goron;
    };

    // === Input (synced each frame) ===
    f32 controlStickMagnitude;
    s16 controlStickAngle;

    // === Collision ===
    f32 wallHeight;
    f32 ceilingHeight;
    f32 wallRadius;

    // === Health/Magic ===
    s8 health;
    s8 magic;

    // === Animation state (simplified) ===
    s32 skelAnimeFrameCount;
    f32 skelAnimeCurFrame;

} MmPlayerCore; // Minimal version

// Global MmPlayer instance
extern MmPlayerCore gMmPlayer;

// =============================================================================
// Hook System Functions
// =============================================================================

/**
 * Initialize MmPlayer from OOT Player state
 * Copies all relevant fields from OOT Player to MmPlayer
 * Call once when transformation starts
 */
void MmPlayer_InitFromOot(MmPlayerCore* mm, Player* ootPlayer, PlayState* play);

/**
 * Sync MmPlayer state back to OOT Player
 * Copies position, velocity, state flags back to OOT
 * Call after MmPlayer_Update each frame
 */
void MmPlayer_SyncToOot(MmPlayerCore* mm, Player* ootPlayer, PlayState* play);

/**
 * Update MmPlayer input from OOT input
 * Call each frame before MmPlayer_Update
 */
void MmPlayer_SyncInput(MmPlayerCore* mm, Player* ootPlayer, PlayState* play);

/**
 * Main MmPlayer update - runs MM action logic
 * Uses the synced MmPlayerCore state
 */
void MmPlayer_Update(MmPlayerCore* mm, PlayState* play);

// =============================================================================
// Transformation State
// =============================================================================

/**
 * Check if currently in MM transformation mode
 */
u8 MmPlayer_IsTransformed(void);

/**
 * Get current MM form
 */
MmPlayerTransformation MmPlayer_GetForm(void);

/**
 * Start transformation to a new form
 * @param targetForm The form to transform into
 * @param skipCutscene If true, skip the transformation cutscene
 */
void MmPlayer_StartTransformation(PlayState* play, MmPlayerTransformation targetForm, u8 skipCutscene);

// No-op action function (C linkage, replaces OOT actionFunc while transformed)
void MmForm_OotNoopAction(Player* thisx, PlayState* play);

// =============================================================================
// Pending Damage System
//
// OOT's func_808382DC in Player_UpdateCommon handles damage (AC_HIT) BEFORE
// TransformMasks_Update runs. Then Collider_ResetCylinderAC clears the AC_HIT
// flag. To let the MM form system handle its own damage:
//   1. func_808382DC saves hit info here and skips OOT processing when transformed
//   2. MmForm_CheckDamage reads this instead of checking AC_HIT directly
// =============================================================================
typedef struct {
    u8 hasPending;   // 1 if damage detected this frame, 0 otherwise
    s32 damage;      // actor.colChkInfo.damage
    u8 acHitEffect;  // actor.colChkInfo.acHitEffect
    Actor* attacker; // cylinder.base.ac (may be NULL)
} MmFormPendingDamage;

extern MmFormPendingDamage gMmFormPendingDamage;

// Core state queries
u8 TransformMasks_IsEnabled(void);
u8 TransformMasks_IsTransformed(void);
u8 TransformMasks_HasSkeleton(void);

// Redirect OOT voice SFX to MM equivalent voice for current form.
// Called from Player_PlayVoiceSfx when transformed, instead of suppressing.
void TransformMasks_PlayMmVoice(u16 ootVoiceSfxId, Vec3f* pos);

// Like TransformMasks_PlayMmVoice but reports whether a form voice played.
// Returns 1 if the active form has its own voice bank (the OOT voice must be
// suppressed); 0 if the caller should fall back to Link's OOT voice. Pass the
// OOT *base* (adult) voice id — the _KID ids at 0x6820+ are out of range here.
u8 TransformMasks_TryPlayMmVoice(u16 ootVoiceSfxId, Vec3f* pos);

// Redirect OOT step/walk SFX to MM form-specific sample (Deku/Zora/Goron).
// Returns 1 if the MM SFX was played and the OOT step should be skipped;
// returns 0 for FD/Garo/Gerudo/Human so OOT handles it normally. Called from
// Player_PlaySteppingSfx and Player_PlayFloorSfxByAge in z_player.c.
u8 TransformMasks_TryPlayMmStepSfx(u16 ootStepSfxId, Vec3f* pos);

// FD skin mode: returns true when FD is active (OOT handles gameplay, only DLs swapped)
u8 TransformMasks_IsFDSkinMode(void);

// Returns true if ANY form is active (including FD skin mode)
u8 TransformMasks_IsTransformedAny(void);

MmPlayerTransformation MmForm_GetCurrentForm(void);

// OCARINA_INSTRUMENT_* the song replay should be voiced with, MM's
// sPlayerFormOcarinaInstruments[CUR_FORM]. DEFAULT for forms whose instrument is not one
// of OoT's — those are voiced from the OnOcarinaPlaybackNote hook instead.
u8 MmForm_GetOcarinaPlaybackInstrument(void);

// Soundfont_0 instrument the song fanfare should voice its melody with, MM's
// sOcarinaSongFanfareIoData[CUR_FORM]. Only an MM fanfare sequence reads it.
u8 MmForm_GetSongFanfareInstrument(void);

// Play OoT's song jingle with the active form's voice instead of the ocarina sequence.
// Returns 1 when it took the song over, so the caller skips Audio_PlayFanfare.
s32 FormJingle_Start(s32 songId);
void FormJingle_Stop(void);

// Dragon Scale: Zora swim for non-Zora forms (Adult Link only)
u8 TransformMasks_IsZoraSwimEnabled(void);
void TransformMasks_SetZoraSwimEnabled(u8 enabled);

// =============================================================================
// Tunic effects without the tunic (Skijer 2026-07-28)
//
// Goron/Zora transform wearing the KOKIRI Tunic — the Goron/Zora Tunic is never
// equipped by a form any more. These two grant the tunics' gameplay effects (and
// only those) as a property of the form's body:
//   fire resistance  → Goron  (hot rooms, body burn, hot/lava floors)
//   water breathing  → Zora   (underwater timer never starts)
// Every OOT `currentTunic == PLAYER_TUNIC_GORON/ZORA` resistance check ORs these in.
// =============================================================================
u8 MmForm_HasFireResistance(void);
u8 MmForm_HasWaterBreathing(void);
u8 TransformMasks_HasFireResistance(void);
u8 TransformMasks_HasWaterBreathing(void);

// =============================================================================
// Shield decoupling (Skijer 2026-07-28)
//
// No form is affected by, or affects, the equipped shield. Forms never write
// player->currentShield and never read it for collision type / VFX / reflection;
// their guard works identically with any shield or with none. OOT's vanilla shield
// pipeline gates on currentShield, so it asks the form which mode it is in.
// =============================================================================
#define MMFORM_SHIELD_VANILLA 0    // human Link — OOT decides normally
#define MMFORM_SHIELD_FORM_GUARD 1 // form rides OOT's upper-body shield; ignore the equipment gates
#define MMFORM_SHIELD_BLOCK 2      // form owns R itself; OOT's shield actions must not engage
#define MMFORM_SHIELD_TWO_HANDED \
    3 // Fierce Deity: full vanilla shield pipeline, but always as if
      // holding the Biggoron's Sword (two-handed guard, no shield in
      // hand) and with the "must have a shield equipped" gates bypassed

u8 MmForm_GetShieldMode(void);
u8 TransformMasks_GetShieldMode(void);

// Water regime. Each OOT water gate used to re-derive its own form list inline, and they drifted.
#define MMFORM_WATER_VANILLA 0   // OOT owns water completely
#define MMFORM_WATER_SINK 1      // cannot swim — hop / curl / void out
#define MMFORM_WATER_ZORA_SWIM 2 // OOT's surface swim, but A is the fast swim

u8 MmForm_GetWaterMode(void);
u8 TransformMasks_GetWaterMode(void);

// Scales the launch a form's jump slash was given, alongside the Gerudo and Trident adjusters.
void MmForm_AdjustJumpSlash(Player* player, s32 mwa);

// Momentum moves that must carry the player off an edge instead of letting vanilla react.
u8 MmForm_IsGoronRolling(void);
u8 MmForm_IsDekuSpinning(void);

// ---------------------------------------------------------------------------
// Form animation tables (defined in z_player.c, next to the tables themselves).
//
// The way a form re-skins Link without touching his behaviour: OOT's own tables
// decide which animation each action plays, so a form swaps entries instead of
// writing action functions. D_80853914[group][animType] holds idle / walk / run /
// strafe / backwalk / turn / roll / landing / damage; D_80854190[mwa] holds every
// sword swing with its recovery pair and hit-frame window.
//
// animType column 1 is the "fighter" (weapon drawn) set, 0/4/5 the free-handed
// set, 3 the two-handed set — but Player_SetModelGroup demotes animType to 0
// when no shield is equipped, so a form with no shield should write every column
// of the groups it cares about rather than betting on one.
//
// Save/restore is the caller's job: read a slot before overwriting it and put the
// original back when the form ends.
// ---------------------------------------------------------------------------
// The spin-attack charge lives in six two-entry arrays of its own, outside both
// animation tables — one per phase, indexed [0] one-handed / [1] two-handed.
typedef enum ExtPlayerChargeAnimPhase {
    EXTPLAYER_CHARGE_START,     // windup
    EXTPLAYER_CHARGE_START_L,   // windup, left-foot variant
    EXTPLAYER_CHARGE_WAIT,      // held, standing
    EXTPLAYER_CHARGE_WAIT_END,  // release of the held pose
    EXTPLAYER_CHARGE_WALK,      // held, walking
    EXTPLAYER_CHARGE_SIDE_WALK, // held, strafing
    EXTPLAYER_CHARGE_PHASE_MAX
} ExtPlayerChargeAnimPhase;

LinkAnimationHeader* ExtPlayer_GetChargeAnim(s32 phase, s32 twoHanded);
void ExtPlayer_SetChargeAnim(s32 phase, s32 twoHanded, LinkAnimationHeader* anim);

LinkAnimationHeader* ExtPlayer_GetAnimGroupAnim(s32 group, s32 animType);
void ExtPlayer_SetAnimGroupAnim(s32 group, s32 animType, LinkAnimationHeader* anim);
void ExtPlayer_GetMeleeAnim(s32 mwa, LinkAnimationHeader** swing, LinkAnimationHeader** end,
                            LinkAnimationHeader** endLockOn, u8* hitStart, u8* hitEnd);
// NULL animation arguments and 0xFF hit-window arguments mean "leave that slot".
void ExtPlayer_SetMeleeAnim(s32 mwa, LinkAnimationHeader* swing, LinkAnimationHeader* end,
                            LinkAnimationHeader* endLockOn, u8 hitStart, u8 hitEnd);

// The four evasive jumps. dir: 0 front, 1 side-left, 2 backflip, 3 side-right.
// slot: 0 the jump itself, 1 its landing, 2 its landing when locked the other way.
// These live in their own table (D_80853D4C), not in D_80853914.
#define EXTPLAYER_JUMP_FRONT 0
#define EXTPLAYER_JUMP_SIDE_L 1
#define EXTPLAYER_JUMP_BACKFLIP 2
#define EXTPLAYER_JUMP_SIDE_R 3
LinkAnimationHeader* ExtPlayer_GetJumpAnim(s32 dir, s32 slot);
void ExtPlayer_SetJumpAnim(s32 dir, s32 slot, LinkAnimationHeader* anim);

// Fidget/idle-variation table, indexed by FidgetType (z_player.c); column 0 is the
// normal set and 1 the sword-drawn one. FIDGET_CRIT_HEALTH_START/_LOOP (7/8) are
// the low-health idle.
#define EXTPLAYER_FIDGET_CRIT_START 7
#define EXTPLAYER_FIDGET_CRIT_LOOP 8
LinkAnimationHeader* ExtPlayer_GetFidgetAnim(s32 fidget, s32 col);
void ExtPlayer_SetFidgetAnim(s32 fidget, s32 col, LinkAnimationHeader* anim);

// ---------------------------------------------------------------------------
// Gerudo Dual Blades (gerudo_mhr_combat.inc.c). Gerudo IS vanilla Link wearing
// other clips: these are the hooks OOT asks so its own sword/shield/roll/hop
// pipeline runs for her. All the behaviour lives in the .inc.c.
// ---------------------------------------------------------------------------
// Light hit-reaction table (D_808544B0): 0-3 short flinches, 4-7 the big ones.
LinkAnimationHeader* ExtPlayer_GetHitAnim(s32 index);
void ExtPlayer_SetHitAnim(s32 index, LinkAnimationHeader* anim);

// 1 while Gerudo is a fighter (blades in hand, or guarding). Player_SetModelGroup
// promotes her to the weapon-drawn animation column on it.
u8 GerudoMhr_ForcesFighter(Player* player);
// Her real sword index (1..3) for Player_GetMeleeWeaponHeld; 0 = nothing/not Gerudo.
s32 GerudoMhr_MeleeWeaponIndex(Player* player);
// Damage-tier row for func_80837948 (rage = one tier up = double).
s32 GerudoMhr_DamageTier(Player* player, s32 tier);
// Speed multiplier hooked into Player_GetMovementSpeedAndYaw (hold-A sprint).
f32 GerudoMhr_RunSpeedMul(void);
// Walk/run cycle frame-advance multiplier (func_8084029C): the sprint keeps cadence.
f32 GerudoMhr_RunAnimRateMul(void);
// 1 while her roll is running: it commits, so B must not cut it (Player_Action_Roll).
u8 GerudoMhr_RollCommits(Player* player);
// R = the blade guard, no shield item needed; the raise plays from frame 0.
u8 GerudoMhr_UsesBladeGuard(Player* player);
// The installed frame of the charge-release swing that throws the thunder wedge
// (source frame GMHR_CHARGE_FAST_BEG). 0 = the clip was not built; fall back.
s16 GerudoMhr_ChargeSummonFrame(void);
// Which MHR weapon family the demon clip on screen belongs to (0 = great sword,
// 1 = hammer, 2 = insect glaive). The axe is placed differently for each.
s32 GerudoMhr_AxeFamily(Player* player);
// Demon mode's charge release drops lightning on her; the visual hangs off this.
void GerudoMhr_DemonThunderStrike(PlayState* play, Player* player);
// Demon mode's tank: GMHR_RAGE_MAX scaled x1/x2/x4 by gSaveContext.magicLevel. The meter
// drains one point per frame while demon mode is up, so this is also its duration.
s16 GerudoMhr_RageCapacity(void);
// Hold-B charge rate multiplier (func_80844E3C): she charges three times as fast.
f32 GerudoMhr_ChargeRateMul(Player* player);
// En_M_Thunder asks: 1 when the charge release is hers — a third of a cylinder thrown
// forward out of the blades instead of the whole ring around Link.
u8 GerudoMhr_UsesConeBurst(Player* player);
// Her RIGHT hand matrix, captured in MmForm_PostLimbDraw. player->mf_9E0 is the LEFT
// hand (vanilla writes it at L_HAND — Link is left-handed); the charge glow needs both.
extern MtxF gGerudoRightHandMtx;
// ...and what that cone feeds back: it has its own collider, so its hits never reach
// GerudoMhr_ScanBladeHits. Worth GMHR_RAGE_CHARGE_MUL times a blade hit.
// Pass the actor the cone landed on: only enemies pay into the meter.
void GerudoMhr_AddChargeRage(Actor* victim);
// Draw-time upper-body offset while guarding (MmForm_OverrideLimbDraw applies it).
u8 GerudoMhr_GetShieldUpperRot(Player* player, Vec3s* out);
// Same, per shoulder (PLAYER_LIMB_L_SHOULDER / PLAYER_LIMB_R_SHOULDER). 0 = nothing to do.
u8 GerudoMhr_GetShieldShoulderRot(Player* player, s32 limbIndex, Vec3s* out);
// The B chain (4 hits, 2 in rage) and the thrust: which row func_80837948 swings.
s32 GerudoMhr_NextComboMwa(Player* player, s32 requested);
u8 GerudoMhr_OwnsComboRow(Player* player);
// Roll clip through VB_PLAYER_ANIM_SITE_ROLL; and the roll's speed factor.
LinkAnimationHeader* GerudoMhr_GetRollAnim(void);
u8 GerudoMhr_WantsLongRoll(void);
// Hold A = sprint, tap A = roll / sheathe. Player_SetupRoll asks this first and
// swallows the press; the controller fires the roll itself on a short release.
u8 GerudoMhr_SuppressRoll(Player* player);
// Player_ActionHandler_Roll's "A standing = put the sword away": Gerudo does that
// with her own clip, so vanilla must not.
u8 GerudoMhr_OwnsPutaway(Player* player);
// Player_UseItem asks: 1 when Gerudo takes the draw/sheathe herself.
u8 GerudoMhr_InterceptUseItem(PlayState* play, Player* player, s32 item);
// Player_StartChangingHeldItem asks: her clip for the RUNNING draw (upper body only,
// so she keeps running). NULL = keep Link's. Flips itemChangeType so it plays backwards.
LinkAnimationHeader* GerudoMhr_GetItemChangeAnim(Player* player, s8 newIA, s32* itemChangeType);
// Jump slash, called from inside Player_Action_80844AF4 (after its gravity stamp and
// its air control): rise -> hang at the apex -> drill down, homing on the lock-on.
void GerudoMhr_TickJumpSlash(Player* player, PlayState* play);
// Sidehop/backflip: rage travel factor + clip (VB_PLAYER_ANIM_SITE_DODGE_HOP).
f32 GerudoMhr_HopSpeedMul(void);
LinkAnimationHeader* GerudoMhr_GetHopAnim(s32 dir);
// Free-fall pose (VB_PLAYER_ANIM_SITE_FALL_WAIT).
LinkAnimationHeader* GerudoMhr_GetFallAnim(Player* player);
// Jump slash launch tweak (func_8083BA90): higher arc, aimed at the lock-on.
void GerudoMhr_AdjustJumpSlash(Player* player, s32 mwa);
// Called from Player_UpdateCommon BEFORE the melee quads' AT reset: the only place
// this frame's blade hits are still readable for the form (rage meter).
void GerudoMhr_ScanBladeHits(Player* player);
void KeatonForm_ScanBlock(Player* player);
// Draw-callback gate: trail on?, per-blade mask, and whether the form writes the
// quads' damage flags itself (controller clip) or keeps OOT's (OOT swing).
u8 GerudoMhr_GetBladeGate(u8* mask, u8* ownFlags, u32* dmgFlags, u8* damage);
// 1 while the controller drives a clip of its own (MmForm_UsesOotAnim asks).
u8 GerudoMhr_DrivingClip(void);
// Are the scimitars drawn in the hands (gerudo_form.cpp).
u8 GerudoMhr_SwordsOut(void);
// Guard clip by phase (0 raise, 1 loop, 2 release) for OOT's shield code paths.
LinkAnimationHeader* GerudoMhr_GetGuardAnim(Player* player, s32 phase);
// 1 while a Gerudo swing runs with B held: Player_UpdateCommon pins unk_844 so the
// hold-B charge is reachable after her long swings.
u8 GerudoMhr_HoldsChargeWindow(Player* player);
// 1 while L is held as Gerudo: TransformMasks_FilterB strips B from OOT's input copy.
u8 GerudoMhr_LOwnsB(void);
// The Rito's bow state, read by the draw path (reticle, and the shield hides).
u8 MmForm_RitoBowIsOut(void);
void MmForm_RitoBowReset(void);

// EnArrow asks this before re-deriving its yaw from the camera: a 1 means the arrow
// belongs to the Rito's volley and has just been given the aim, pitch included.
u8 MmForm_RitoBowClaimArrow(PlayState* play, Actor* arrow);
// A Rito may use Roc's Feather / Roc's Cape in mid-air as often as it likes, each use
// billed in magic instead of counted. ANSWERING 1 ALSO CHARGES IT, so ask exactly once
// and only when you are about to jump. Everyone else gets 0 and keeps their own limit.
u8 MmForm_RitoAirRocsAllowed(Player* player);
// 1 while the Rito is guarding with its own shield. The Mirror Shield predicates in
// z_player_lib.c defer to it, so every reflection site inherits the behaviour.
u8 MmForm_RitoShieldIsDrawn(void);
// The rage meter HUD (drawn under the magic bar). Call from Interface_Draw.
void GerudoMhr_DrawRageMeter(PlayState* play);

// Rage: charged by landing blades, L+R with the blades out. Swaps every table row.
u8 GerudoMhr_RageActive(void);
u8 GerudoMhr_RageReady(void);
f32 GerudoMhr_RageFill(void); // 0..1 — meter while charging, time left while active

// Rage parry: a hit caught in the guard's first frames, in rage. Called from
// func_808382DC ahead of both damage branches. 1 = the hit is eaten.
u8 GerudoMhr_TryParry(PlayState* play, Player* player);

// True when OOT already has a contextual meaning for the A button (open/enter door,
// speak/check/read, grab, climb, enter, drop/throw a carried actor, drop off a ledge).
// A form's custom A move must yield when this is set. Does NOT cover roll — callers
// that also need to yield to the roll gate on movement themselves. See
// GaroForm_VanillaWantsAButton for the narrower, grab-excluding Garo variant.
u8 TransformMasks_AButtonIsOffered(Player* player);

// Load a DL from mm.o2r with hash pre-resolution (safe for drawing)
void* TransformMasks_LoadMmDL(const char* path);
u8 TransformMasks_DragonScaleEnterSwim(void* play, void* player);
void TransformMasks_DragonScaleSwimUpdate(void* play, void* player);
void TransformMasks_DragonScaleExitSwim(void* player);

// Item restriction: returns true if item is allowed for current form
u8 TransformMasks_IsItemAllowed(s32 item);

// Slot restriction: returns true if inventory slot (0-71) is allowed for current form
u8 TransformMasks_IsSlotAllowed(u8 slot);

// Per-form C-button item use interception (called in z_player.c before Player_UseItem).
// If the current form has a handler for this item, calls it and returns 1 (skip Player_UseItem).
// If the current form is active but has NO handler for the item, also returns 1 (block use).
// Returns 0 when not transformed (fall through to normal Player_UseItem).
u8 TransformMasks_HandleFormItemUse(PlayState* play, Player* player, s32 item);

TransformMaskId TransformMasks_GetMaskType(s32 item);
void TransformMasks_HandleMaskUse(PlayState* play, Player* player, s32 item);
// Transform or swap skin, whichever this item asks for. 1 = handled, wear nothing.
u8 TransformMasks_TryFormFromItem(PlayState* play, Player* player, s32 item);

// Dev: trigger a transformation directly without a mask item. Toggles between
// the requested form and Human if already in that form. Currently used for Garo
// (no Garo Mask item exists yet). Skip-cutscene unconditional.
void MmForm_DevTransformTo(PlayState* play, Player* player, MmPlayerTransformation form);

void TransformMasks_Init(PlayState* play, Player* player);
void TransformMasks_Update(PlayState* play, Player* player);
void TransformMasks_Draw(PlayState* play, Player* player);

// Strip BTN_B from a Player_Update input copy before Player_UpdateCommon runs.
// Centralizes the cases where B is reserved by a custom system (currently:
// Blast Mask + Great Fairy Mask reactions, Garo attack kit). Called from
// z_player.c right after the input copy.
void TransformMasks_FilterB(Input* input);

/**
 * True while a textbox or the ocarina owns the buttons.
 *
 * TransformMasks_FilterB already strips A/B/C from the FILTERED input copy the forms
 * normally read, but forms that read the RAW play->state.input[0] (Garo's moveset
 * dispatcher, Pikachu's bindings) bypass it — they must check this themselves before
 * acting on a press, or they will fire their moveset while the player is playing notes.
 */
u8 MmForm_InputOwnedByMessage(void);

// Reset transformation state (call on scene transition, death, etc.)
void TransformMasks_Reset(void);

// Called on player death — deactivates Chateau Romani infinite magic.
void TransformMasks_OnDeath(void);

// =============================================================================
// Garo form hooks (defined in garo_form.cpp). Called from transformation_masks.c
// (OnDeath spawns 9 flame particles BEFORE MmForm_OnDeath rolls back the form)
// and from z_player.c revival sites (OnReset clears death-once flags).
// =============================================================================
void GaroForm_OnDeath(Player* player, PlayState* play);
void GaroForm_OnReset(void);

// Glass-cannon damage multiplier for Garo form (1.5x incoming). Returns 1.0f for
// non-Garo forms. Applied inside z_player.c func_80837B18_modified.
f32 MmForm_GetIncomingDamageMult(void);

// Mapping from PLAYER_LIMB index to PLAYER_BODYPART index (-1 = no bodypart).
// Mirrors OOT's D_80160000 system (z_player_lib.c) which fills bodyPartsPos
// sequentially during skeleton traversal; we use an explicit table instead.
// Defined once in mm_player_form.cpp; used by both MmForm_PostLimbDraw and
// garo_post_limb.cpp's GaroForm_PostLimbDraw (same Link rig, same mapping).
extern const s8 gPlayerLimbToBodyPart[PLAYER_LIMB_MAX];

// Kills a punch/sword trail EffectBlure slot if active: deletes the effect,
// clears the active flag, and resets the index to -1. No-op when inactive.
// Shared by the form action handlers (mm_player_form.cpp) and garo_form.cpp.
void MmForm_KillTrail(PlayState* play, s32* effectIndex, u8* active);

// MM player voice action codes (subset). Confirmed against mm_decomp
// sPlayerVoiceSfxOffsets at voicebank_table.h (NA_SE_VO_LI_*).
//   ATTACK : NA_SE_VO_LI_SWORD_N   = 0x6800 + 0x00 (sword swing grunt)
//   DAMAGE : NA_SE_VO_LI_DAMAGE_S  = 0x6800 + 0x05 (damage-taken cry)
//   DEATH  : NA_SE_VO_LI_DOWN      = 0x6800 + 0x0B (knockdown / death)
// The base MM voice bank has no dedicated laugh/taunt slot — Garo laugh
// uses an OOT fallback SFX (NA_SE_VO_SK_LAUGH, Skull Kid taunt).
#define VOICE_ACTION_ATTACK 0x00
#define VOICE_ACTION_DAMAGE 0x05
#define VOICE_ACTION_DEATH 0x0B

// Water entry: called when player enters deep water (swim depth).
// Returns 1 if swimming was blocked (Goron/Deku can't swim), 0 if allowed (Zora/FD).
u8 TransformMasks_OnWaterSwimAttempt(PlayState* play, Player* player);

// Camera height for current form (from MM Player_GetHeight). Returns 0 if not transformed.
f32 TransformMasks_GetFormHeight(void);

// Returns 1 if current form blocks ledge grab (only Goron). Returns 0 otherwise.
u8 TransformMasks_BlocksLedgeGrab(void);

// OOT processed control stick magnitude (0-60, normalized to circle).
// Defined in transformation_masks.c which is compiled inside z_player.c and sees the static.
f32 TransformMasks_GetStickMagnitude(void);

// OOT floor type (sFloorType from z_player.c).
// 2=hot room floor, 3=lava floor, 4=sand, 5=slippery, 7=water lilies, 9=void, 12=deep sand.
s32 TransformMasks_GetFloorType(void);

// =============================================================================
// MM Mask Wearing (non-transformation masks drawn on Link's head)
// =============================================================================

// Toggle wearing an MM mask. Transformation masks are handled separately.
void TransformMasks_WearToggle(PlayState* play, Player* player, s32 itemId);

// Draw the currently worn MM mask (call from PostLimbDraw for HEAD limb).
void TransformMasks_WearDraw(PlayState* play, Player* player);

// Per-mask effect update (call each frame).
void TransformMasks_WearUpdate(PlayState* play, Player* player);

// Get current worn MM mask item ID (ITEM_NONE if none).
s32 TransformMasks_WearGetCurrent(void);

// Clear worn MM mask (scene transition, death, etc.).
void TransformMasks_WearClear(void);

// Get pre-loaded FD sword beam DL for rendering (per-frame safe copy from mm.o2r)
// Returns NULL if not loaded. Caller uses with gSPDisplayList on POLY_XLU_DISP.
Gfx* TransformMasks_GetFDSwordBeamDL(PlayState* play);

// Zora fin DLs for boomerang visual override (set by mm_player_form.cpp, read by z_en_boom.c)
// NULL when Zora assets not loaded. EnBoom with params==1 uses L, params==2 uses R.
extern Gfx* gZoraFinBoomerangLDL;
extern Gfx* gZoraFinBoomerangRDL;

// FD melee weapon quad registration (sword damage). Called from MmForm_PostLimbDraw.
// Defined in z_player_lib.c since it accesses static variables (D_80126080, etc.)
void Player_FDMeleeWeaponPostLimb(PlayState* play, Player* player);

// =============================================================================
// Network Visual State Accessors (for Harpoon multiplayer sync)
// =============================================================================

// Model type for network: 0=Link, 1=Goron, 2=Zora, 3=Deku, 4=FD
u8 TransformMasks_GetModelType(void);

// MM stateFlags3 (spike mode, roll active, etc.)
u32 TransformMasks_GetMmStateFlags3(void);

// MM horizontal speed
f32 TransformMasks_GetMmSpeedXZ(void);

// MM form skeleton joint table (NULL if not transformed or skeleton not loaded)
Vec3s* TransformMasks_GetFormJointTable(void);

// Number of valid joints in the form joint table (0 if not transformed)
s32 TransformMasks_GetFormJointCount(void);

// Current action ID (GoronActionId enum in mm_player_form.cpp)
s32 TransformMasks_GetGoronAction(void);

// Eye blink index (0=open, 1=half, 2=closed)
u8 TransformMasks_GetEyeIndex(void);

// Goron ball squash/stretch deformation factor
f32 TransformMasks_GetRollSquash(void);

// Goron spike mode counter (0=off, >0=active)
s16 TransformMasks_GetRollSpikeActive(void);

// Goron charge level counter
s16 TransformMasks_GetRollChargeLevel(void);

// Returns the item/model scale multiplier for the current form.
// FD = 1.5f (actor.scale 0.015f vs standard 0.01f), all others = 1.0f.
// Custom item draw functions should multiply their model scale by this value
// so items appear proportional to the current form's body size.
f32 TransformMasks_GetItemScale(void);

#ifdef __cplusplus
}
#endif

#endif // TRANSFORMATION_MASKS_H
