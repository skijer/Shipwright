/**
 * gerudo_mhr_combat.inc.c — Gerudo "Monster Hunter Rise / Dual Blades" moveset.
 *
 * Text-included at the END of mm_player_form.cpp, INSIDE its extern "C" block, so
 * everything here has C linkage and every MmForm_* helper above is in scope. It is
 * compiled as C++: no `this` as an identifier, no designated initializers.
 *
 * DESIGN (v3, 2026-08-18 — rebuilt from scratch after the audit):
 *
 *   Gerudo IS vanilla Link, re-skinned. She is Link-rigged (MmForm_Draw paints
 *   player->skelAnime's joints onto the gerudo skeleton), so the moveset is made
 *   by changing WHICH clip OOT plays, not by seizing the action function:
 *     - D_80853914[group][animType]  idle / walk / run / guard / landing ...
 *     - D_80854190[mwa]              every sword swing + recovery + hit window
 *     - D_80853D4C[dir]              sidehop / backflip   (VB_PLAYER_ANIM_SITE_DODGE_HOP)
 *     - sFidgetAnimations            low-health idle
 *     - D_808544B0                   light hit reactions
 *     - the charge stance arrays     hold-B charge
 *   OOT keeps owning movement, physics, collision, hit-stop, combo chaining and
 *   damage interruption. The whole B chain, the thrust, the guard, the hops, the
 *   jump slash and the charge are OOT actions wearing dual-blade clips.
 *
 *   What made the previous passes dead code, and what this file now relies on:
 *     1. Player_GetMeleeWeaponHeld returned 0 for every transformed form, and
 *        Player_ActionHandler_7 refused B for them → OOT's melee never ran for
 *        Gerudo. Both now let Gerudo through (GerudoMhr_MeleeWeaponIndex).
 *     2. Player_PostLimbDrawGameplay does NOT run for a form (MmForm_Draw draws
 *        the form skeleton instead), so the sword quads, the sword trail, the
 *        shieldQuad and upperLimbRot all have to be produced by the form's own
 *        draw callbacks — see the Gerudo blocks in MmForm_OverrideLimbDraw and
 *        MmForm_PostLimbDraw, fed from GerudoMhr_GetBladeGate().
 *     3. Player_UpdateCommon resets the melee quads' AT flags at its END, and this
 *        controller runs AFTER it (TransformMasks_Update) — so AT_HIT can only be
 *        read from a hook placed BEFORE that reset: GerudoMhr_ScanBladeHits.
 *     4. PLAYER_STATE3_PAUSE_ACTION_FUNC only skips actionFunc. Gravity, speedXZ,
 *        yaw and position still integrate every frame, so a paused clip must own
 *        motion in exactly one way (linearVelocity OR the clip's root, never both).
 *
 *   The controller below drives ONLY what OOT cannot express: rage enter, the
 *   rage roll, the rage parry, the front slash (normal + rage teleport), the
 *   aerial slash (normal + rage loop), and draw / sheathe.
 *
 * Buttons: B combo (4 hits; 2 in rage) · forward+B / B while sprinting = thrust ·
 * hold A = sprint, tap A = roll (rage: rage roll), tap A standing = sheathe ·
 * R = blade guard (rage: parry in the first frames) · L+R with a full meter = rage ·
 * L+B on the ground = front slash · A in the air = aerial slash · Z+A = jump slash ·
 * Z+side/back+A = hops/backflip · hold B = charge.
 * Skijer's NEI.
 */

// ===========================================================================
// Tunables (constants — nothing here is a CVar)
// ===========================================================================
#define GMHR_SHIELD_SPEED 2.0f // guard clips resampled to run this much faster
// The guard pose, dialled in on 2026-08-19 and baked. Binary angles (0x10000 = 360 deg);
// the degrees they came from are in the comments. Applied at DRAW time only — never to
// upperLimbRot, which feeds itself back through Math_ScaledStepToS and grows without end.
#define GMHR_SHIELD_ROT_Y 3969 // 21.8 deg — upper body yaw
#define GMHR_SHIELD_ROT_X 6954 // 38.2 deg — upper body pitch
#define GMHR_SHIELD_ROT_Z 9412 // 51.7 deg — upper body roll
#define GMHR_SHOULDER_L_ROT_X 0
#define GMHR_SHOULDER_L_ROT_Y 0
#define GMHR_SHOULDER_L_ROT_Z 0
#define GMHR_SHOULDER_R_ROT_X 0
#define GMHR_SHOULDER_R_ROT_Y 0
#define GMHR_SHOULDER_R_ROT_Z 0
#define GMHR_PARRY_WINDOW 10 // frames after the guard goes up where a hit becomes a parry (rage only)
#define GMHR_A_TAP_FRAMES 8  // A held longer than this = sprint, shorter = roll / sheathe
#define GMHR_SPRINT_MUL 1.5f
#define GMHR_RAGE_MAX 400
#define GMHR_DEMON_AXE_DAMAGE 8   // twice a Master Sword hit: it is a two-handed axe
#define GMHR_FURY_STRIKE_FRAME 80 // source frame of gs_wirebug_attack04 the axe lands on
// An L-attack costs a bar, so it hits like one. The flag set is wide on purpose: bosses
// each accept a different one, so offering hammer, giant-slash, spin and jump together
// means none of them can shrug the whole attack off.
#define GMHR_LATK_DAMAGE 30
#define GMHR_LATK_DMGFLAGS (DMG_HAMMER_SWING | DMG_SLASH_GIANT | DMG_SPIN_GIANT | DMG_JUMP_GIANT)
#define GMHR_FURY_MAGIC_FLAGS (DMG_MAGIC_FIRE | DMG_MAGIC_LIGHT)
#define GMHR_LATK_QUAKE_STRENGTH 6
#define GMHR_LATK_QUAKE_FRAMES 18
#define GMHR_LATK_DIM 140        // how dark the screen goes on impact
#define GMHR_LATK_DIM_FADE 10    // alpha shed per frame afterwards
#define GMHR_LUNGE_WAVE_FRAME 30 // source frame of gs_charge_attack01 that throws the wedge
#define GMHR_SPIN_HOME_SPEED 12.0f
#define GMHR_FURY_IFRAMES 60 // she is untouchable for the length of her own blast
#define GMHR_FURY_BLAST_RADIUS 320
#define GMHR_FURY_BLAST_HEIGHT 400
#define GMHR_FURY_BLAST_FRAMES 90.0f
// The meter IS the charge: an empty bar is a slap, a full one one-shots a stunned boss.
#define GMHR_FURY_DAMAGE_MIN 10
#define GMHR_FURY_DAMAGE_MAX 255
// A hit is worth a FRACTION of the tank, not a fixed number: capacity scales x1/x2/x4
// with magic, so a flat gain meant double magic took four times as many hits to fill.
// At 1/10, ten hits fill the bar at every level and magic buys uses instead of grind.
#define GMHR_RAGE_HITS_TO_FILL 10
#define GMHR_RAGE_CHARGE_MUL 2 // the charge cone fills the meter twice as fast
#define GMHR_CHARGE_SPEED 2.1f // both charge releases: the old 1.4 x1.5
// The release does NOT play at one rate. Source frames of ForwardTumbleDelayedCrossFinish:
// the throw itself snaps past between these two, at GMHR_CHARGE_FAST_MUL times the row's
// speed; everything either side of it keeps GMHR_CHARGE_SPEED. The thunder leaves the
// blades on GMHR_CHARGE_FAST_BEG, which is where the fast stretch starts.
#define GMHR_CHARGE_FAST_BEG 33
#define GMHR_CHARGE_FAST_END 53
#define GMHR_CHARGE_FAST_MUL 3.0f
// One PlayerAnimation frame: 3 root translation + 64 limb rotation s16. Needed up here
// because the clip builders below concatenate resampled ranges by hand.
#define GMHR_ANIM_S16_PER_FRAME 67
#define GMHR_CHARGE_RATE_MUL 3.0f // hold-B fills in a third of the time
// No separate duration any more: the meter IS the fuel and drains one point per frame,
// so GMHR_RAGE_MAX doubles as "how long demon mode lasts without magic" and the magic
// upgrades stretch it (GerudoMhr_RageCapacity).
#define GMHR_RAGE_HOP_MUL 1.5f
#define GMHR_ROLL_SPEED_MUL 2.0f      // Gerudo's roll covers twice the ground
#define GMHR_FRONT_SLASH_DIST 100.0f  // "same distance as the jump slash"
#define GMHR_RAGE_TELEPORT_DIST 40.0f // rage front slash with no target
#define GMHR_SPIN_CYL_RADIUS 45       // "a cylinder twice Link's size" (Link is 12)
#define GMHR_SPIN_CYL_HEIGHT 60
#define GMHR_WALL_MARGIN 14.0f
#define GMHR_TRAIL_KILL_DELAY 2
#define GMHR_COMBO_SPEED 1.7f                          // the B chain (2.5 read as too fast)
#define GMHR_DRAW_SPEED 1.5f                           // draw / sheathe
#define GMHR_SPRINT_ANIM_RATE (1.0f / GMHR_SPRINT_MUL) // the run cycle does NOT speed up while sprinting
#define GMHR_OOT_SWING_SPEED_COMP 1.5f                 // 1 / PLAYER_ANIM_ADJUSTED_SPEED (2/3): OOT plays swings slow
#define GMHR_HOP_SIDE_FRAMES 12                        // installed length of the side hops (OOT plays them at 2/3 too)
#define GMHR_HOP_BACK_FRAMES 18                        // backflip: longer air time
#define GMHR_HOP_LAND_FRAMES 8                         // the hop landings (slots 1/2 of D_80853D4C)

// ===========================================================================
// Clip paths + loaders
// ===========================================================================
#define MHRP(name) "__OTR__misc/link_animetion/gMonsterHunterRise_DualBlade_" name
// Demon mode draws from a DIFFERENT archive, mhr_weapons2_anims.o2r, and a different
// naming scheme: the Great Sword ("gs") and Hammer ("hm") families, e.g.
// MHRW("gs_dash_attack09"). The .o2r files in x64/Release/nei are auto-discovered (no
// source file names any of them), so nothing has to be registered — but if a demon clip
// ever comes back NULL, that archive not loading is the first thing to check.
#define MHRW(name) "__OTR__misc/link_animetion/gPlayerAnim_mhr_" name
// One demon clip lives with the Dual Blade set instead (mhr_anims.o2r).
#define MHRIG(name) "__OTR__misc/link_animetion/gMonsterHunterRise_InsectGlaive_" name

static LinkAnimationHeader* MmForm_MhrLoadPath(const char* path) {
    if (path == NULL || !ResourceMgr_FileExists(path))
        return NULL;
    return ResourceMgr_LoadPlayerAnimAsHeader(path);
}

// z_player.c entry points this file drives. Named `player`, never `this`.
extern void Player_SetupRoll(Player* player, PlayState* play);
extern void func_80839F90(Player* player, PlayState* play);
extern void Player_Action_808502D0(Player* player, PlayState* play);
extern void Player_Action_Roll(Player* player, PlayState* play);
extern void func_80837948(PlayState* play, Player* player, s32 mwa);
extern void func_8008EC70(Player* player);
extern void Player_RequestRumble(Player* player, s32 sourceStrength, s32 duration, s32 decreaseRate, s32 distSq);
extern s8 Player_ItemToItemAction(s32 item);

// ===========================================================================
// Section 1 — tables
// ===========================================================================
typedef struct {
    s16 beg, end; // inclusive SOURCE frames; end < beg = unused
} GMhrWin;
#define GMHR_NOWIN                \
    {                             \
        { -1, -1 }, { -1, -1 }, { \
            -1, -1                \
        }                         \
    }

// ---- locomotion / guard / landing (D_80853914 rows) -----------------------
typedef struct {
    s32 group;
    const char* path;
    const char* ragePath; // NULL = same in rage
    s16 frames;           // >0: force this length (the 29-frame blend rig)
    s16 srcStart, srcEnd; // inclusive sub-range (-1 = whole)
    f32 speedMul;         // >0 with frames==0: resample the range to run this many times faster
} GMhrGroupBinding;

// Only the weapon-drawn columns (1 = 1H, 3 = 2H). Column 0/4/5 stay Link's, so
// "stowed" still looks like Link. Player_SetModelGroup promotes Gerudo to
// column 1 whenever she is a fighter (blades out OR guarding).
#define GMHR_FIGHTER_COLUMNS_MASK ((1 << 1) | (1 << 3))

// The two stride lengths OOT's locomotion blend rig demands (see the table below).
#define GMHR_WALK_FRAMES 29
#define GMHR_RUN_FRAMES 20

static const GMhrGroupBinding sMhrGroupBindings[] = {
    { PLAYER_ANIMGROUP_wait, MHRP("StationaryReadyIdle_Variant03"), NULL, 0, -1, -1, 0.0f },
    // The WALK and RUN rows are NOT the same length, and getting that wrong eats
    // half the stride. func_80841EE4 drives the blend off ONE counter, unk_868,
    // which cycles 0..29 (func_8084029C), and it samples:
    //     walk  at  unk_868              -> the walk clip must be 29 frames
    //     run   at  unk_868 * (20/29)    -> the run  clip must be 20 frames
    // (func_80833438 sends damage_run and heavy_run through that same 20/29 line.)
    // A 29-frame run row means only its first 20 frames are ever reachable: the
    // second step gets cut and the cycle snaps back — "solo das el paso izquierdo".
    // The run row is swapped to GMHR_SPRINT_CLIP only while A is held (see
    // MmForm_GerudoTickSprintRow); OOT's walk/run blend then does the crossfade.
    { PLAYER_ANIMGROUP_walk, MHRP("ForwardCombatRun"), NULL, GMHR_WALK_FRAMES, -1, -1, 0.0f },
    { PLAYER_ANIMGROUP_run, MHRP("ForwardCombatRun"), NULL, GMHR_RUN_FRAMES, -1, -1, 0.0f },
    { PLAYER_ANIMGROUP_damage_run, MHRP("ForwardDoubleRushSlash_Variant15"), NULL, GMHR_RUN_FRAMES, -1, -1, 0.0f },
    // The guard: one flourish cut into OOT's three shield slots, at x2. OOT plays
    // these at a fixed 1.0, so resampling the range IS the speed control.
    { PLAYER_ANIMGROUP_defense, MHRP("DemonModeActivationFlourish"), NULL, 0, 1, 20, GMHR_SHIELD_SPEED },
    { PLAYER_ANIMGROUP_defense_wait, MHRP("DemonModeActivationFlourish"), NULL, 0, 30, 30,
      GMHR_SHIELD_SPEED }, // loop = the last frame only
    { PLAYER_ANIMGROUP_defense_end, MHRP("DemonModeActivationFlourish"), NULL, 0, 31, 45, GMHR_SHIELD_SPEED },
    { PLAYER_ANIMGROUP_landing, MHRP("ForwardSingleTwinSlash"), NULL, 0, -1, -1, 0.0f },
    { PLAYER_ANIMGROUP_short_landing, MHRP("ForwardSingleTwinSlash"), NULL, 0, -1, -1, 0.0f },
};
#define GMHR_GROUP_BINDING_COUNT ((s32)(sizeof(sMhrGroupBindings) / sizeof(sMhrGroupBindings[0])))

// Demon mode: the SAME rows in the SAME order, with the axe clips. Row-parallel is not a
// style choice — sMhrTables.savedGroup[i] is indexed by row, so the two tables have to
// line up or restoring vanilla puts the wrong clip back. The static_assert below is the
// guard. She has no dedicated demon idle or walk, so the charge-stance idle and the run
// cover them (both loop cleanly, which is what those slots need).
static const GMhrGroupBinding sMhrDemonGroupBindings[] = {
    { PLAYER_ANIMGROUP_wait, MHRW("gs_idle01_loop"), NULL, 0, -1, -1, 0.0f },
    { PLAYER_ANIMGROUP_walk, MHRW("gs_run01_loop"), NULL, GMHR_WALK_FRAMES, -1, -1, 0.0f },
    { PLAYER_ANIMGROUP_run, MHRW("gs_run01_loop"), NULL, GMHR_RUN_FRAMES, -1, -1, 0.0f },
    { PLAYER_ANIMGROUP_damage_run, MHRW("gs_run01_loop"), NULL, GMHR_RUN_FRAMES, -1, -1, 0.0f },
    // The guard is one 100-frame idle that already closes on its own first pose, so the
    // middle slot can be the WHOLE clip resampled and it loops without a seam.
    { PLAYER_ANIMGROUP_defense, MHRW("gs_idle22_loop"), NULL, 0, 0, 24, 3.0f },
    { PLAYER_ANIMGROUP_defense_wait, MHRW("gs_idle22_loop"), NULL, 40, -1, -1, 0.0f },
    { PLAYER_ANIMGROUP_defense_end, MHRW("gs_idle22_loop"), NULL, 0, 75, 99, 3.0f },
    { PLAYER_ANIMGROUP_landing, MHRW("gs_charge_attack14"), NULL, 0, 0, 30, 2.0f },
    { PLAYER_ANIMGROUP_short_landing, MHRW("gs_charge_attack14"), NULL, 0, 0, 30, 2.0f },
};
static_assert(sizeof(sMhrDemonGroupBindings) / sizeof(sMhrDemonGroupBindings[0]) == GMHR_GROUP_BINDING_COUNT,
              "demon locomotion table must be row-parallel to the dual-blade one");

// ---- swings (D_80854190 rows) -----------------------------------------------
// swingEnd: the frame the user calls "anim end" — the installed swing is CUT there,
// so OOT sees the animation finish at that frame: that is where the next B chains
// and where A can cancel into a roll. The frames after swingEnd become the row's
// RECOVERY (unk_04/unk_08), so the settle-back is the animator's own tail and
// never snaps. Windows are per hand, in SOURCE frames.
typedef struct {
    s32 mwa;
    const char* path;
    const char* ragePath;
    f32 speedMul; // >0: resample so it plays this many times faster
    s16 swingEnd; // -1 = whole clip is the swing (recovery = 4-frame hold of the last pose)
    GMhrWin L[3], R[3];
    GMhrWin rageL[3], rageR[3]; // used when ragePath != NULL
    u8 spin;                    // 1 = spin: a body cylinder instead of the blade quads
} GMhrMeleeBinding;

static const GMhrMeleeBinding sMhrMeleeBindings[] = {
    // B chain, in the order sGerudoComboRows walks it.
    // 1. AlternatingCrossSlashLeftRight — quad in front, R→L, 13-25; chain from 25.
    //    Rage: LeftRisingMultiHitChargedFlurry — quads every 20 frames from 0, alternating.
    { PLAYER_MWA_FORWARD_SLASH_1H,
      MHRP("AlternatingCrossSlashLeftRight"),
      MHRP("LeftRisingMultiHitChargedFlurry"),
      GMHR_COMBO_SPEED,
      25,
      { { 13, 25 }, { -1, -1 }, { -1, -1 } },
      { { 13, 25 }, { -1, -1 }, { -1, -1 } },
      { { 0, 19 }, { 40, 59 }, { -1, -1 } },
      { { 20, 39 }, { 60, 78 }, { -1, -1 } },
      0 },
    // 2. StationaryRisingSingleAerialSlash — quad L→R 1-28, end 28.
    { PLAYER_MWA_FORWARD_COMBO_1H,
      MHRP("StationaryRisingSingleAerialSlash"),
      NULL,
      GMHR_COMBO_SPEED,
      28,
      { { 1, 28 }, { -1, -1 }, { -1, -1 } },
      { { 1, 28 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    // 3. StationaryRightLeadDoubleTwinSlash — quad L→R 1-28, end 28.
    { PLAYER_MWA_RIGHT_SLASH_1H,
      MHRP("StationaryRightLeadDoubleTwinSlash"),
      NULL,
      GMHR_COMBO_SPEED,
      28,
      { { 1, 28 }, { -1, -1 }, { -1, -1 } },
      { { 1, 28 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    // 4. RightRisingTripleAerialSlash — SPIN: body cylinder 12-41, end 41, walks a few steps.
    { PLAYER_MWA_RIGHT_COMBO_1H,
      MHRP("RightRisingTripleAerialSlash"),
      NULL,
      GMHR_COMBO_SPEED,
      41,
      { { 12, 41 }, { -1, -1 }, { -1, -1 } },
      { { 12, 41 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      1 },
    // Rows OOT can still reach on its own (bumps a row on the third hit): keep them
    // dual-blade so a gerudo body never plays Link's swing.
    { PLAYER_MWA_LEFT_SLASH_1H,
      MHRP("StationaryRisingSingleAerialSlash"),
      NULL,
      GMHR_COMBO_SPEED,
      28,
      { { 1, 28 }, { -1, -1 }, { -1, -1 } },
      { { 1, 28 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    { PLAYER_MWA_LEFT_COMBO_1H,
      MHRP("StationaryRightLeadDoubleTwinSlash"),
      NULL,
      GMHR_COMBO_SPEED,
      28,
      { { 1, 28 }, { -1, -1 }, { -1, -1 } },
      { { 1, 28 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    // Thrust: forward with no lock-on, or B out of the sprint. x1.5. Rage: triple rush.
    { PLAYER_MWA_STAB_1H,
      MHRP("ForwardRisingLeftLeadDoubleAerialSlash"),
      MHRP("ForwardRisingTripleRushSlash"),
      1.5f,
      -1,
      { { 4, 30 }, { -1, -1 }, { -1, -1 } },
      { { 4, 30 }, { -1, -1 }, { -1, -1 } },
      { { 4, 50 }, { -1, -1 }, { -1, -1 } },
      { { 4, 50 }, { -1, -1 }, { -1, -1 } },
      0 },
    { PLAYER_MWA_STAB_COMBO_1H,
      MHRP("ForwardRisingLeftLeadDoubleAerialSlash"),
      MHRP("ForwardRisingTripleRushSlash"),
      1.5f,
      -1,
      { { 4, 30 }, { -1, -1 }, { -1, -1 } },
      { { 4, 30 }, { -1, -1 }, { -1, -1 } },
      { { 4, 50 }, { -1, -1 }, { -1, -1 } },
      { { 4, 50 }, { -1, -1 }, { -1, -1 } },
      0 },
    // Jump slash: OOT plays START in the air and FINISH on touchdown.
    { PLAYER_MWA_JUMPSLASH_START,
      MHRP("ForwardRisingDoubleAerialSlash_Variant18"),
      NULL,
      0.0f,
      -1,
      { { 2, 26 }, { -1, -1 }, { -1, -1 } },
      { { 2, 26 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    // Touchdown: her ordinary fighter landing clip, the same one in the landing groups.
    { PLAYER_MWA_JUMPSLASH_FINISH,
      MHRP("ForwardSingleTwinSlash"),
      NULL,
      0.0f,
      -1,
      { { 2, 40 }, { -1, -1 }, { -1, -1 } },
      { { 2, 40 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      1 },
    // Charge release (level 1): the tumble cross. Blades only, no body cylinder, and
    // OOT's SWORD_LUNGE flag is set on the way in so it throws her forward a bit.
    { PLAYER_MWA_SPIN_ATTACK_1H,
      MHRP("ForwardTumbleDelayedCrossFinish"),
      NULL,
      GMHR_CHARGE_SPEED,
      -1,
      { { 28, 40 }, { -1, -1 }, { -1, -1 } },
      { { 28, 40 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    // Stick-rotation quick spin (and the level-2 release): a real spin, body cylinder.
    { PLAYER_MWA_BIG_SPIN_1H,
      MHRP("ForwardRisingDoubleRushSlash_Variant21"),
      NULL,
      GMHR_CHARGE_SPEED,
      -1,
      { { 6, 60 }, { -1, -1 }, { -1, -1 } },
      { { 6, 60 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      1 },
};
#define GMHR_MELEE_BINDING_COUNT ((s32)(sizeof(sMhrMeleeBindings) / sizeof(sMhrMeleeBindings[0])))

// Demon mode, row-parallel (same mwa in the same slot — see the note on the locomotion
// table). Windows are SOURCE frames, measured off the clips: they are the frames where the
// arm chain's angular velocity peaks, which is where the axe is actually travelling. Only
// the LEFT hand carries a window because demon mode holds one weapon, not two.
// Speeds are picked so each row installs to roughly the length of the dual-blade row it
// replaces, so the rhythm of the fight does not change when she switches.
// Global trim on every demon clip. Applied at the three points a speed is consumed (the
// group resample, the swing resample, the controller clip) rather than baked into the
// tables, so the authored numbers stay readable and one edit re-times the whole moveset.
#define GMHR_DEMON_SPEED_SCALE 0.8f
#define GMHR_DEMON_COMBO_SPEED 2.0f
#define GMHR_DEMON_STAB_SPEED 3.0f
#define GMHR_DEMON_CHARGE_SPEED 4.0f // "muy rapida": 243 source frames down to about 10
static const GMhrMeleeBinding sMhrDemonMeleeBindings[] = {
    // B chain — three hits in demon mode (sGerudoRageComboRows).
    { PLAYER_MWA_FORWARD_SLASH_1H,
      MHRW("hm_charge_attack02"),
      NULL,
      GMHR_DEMON_COMBO_SPEED,
      -1,
      { { 0, 34 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    { PLAYER_MWA_FORWARD_COMBO_1H,
      MHRW("hm_charge_attack03"),
      NULL,
      GMHR_DEMON_COMBO_SPEED,
      -1,
      { { 20, 60 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    { PLAYER_MWA_RIGHT_SLASH_1H,
      MHRW("hm_charge_attack04"),
      NULL,
      GMHR_DEMON_COMBO_SPEED,
      -1,
      { { 25, 60 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    { PLAYER_MWA_RIGHT_COMBO_1H,
      MHRW("hm_charge_attack04"),
      NULL,
      GMHR_DEMON_COMBO_SPEED,
      -1,
      { { 25, 60 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    // Rows OOT can still reach on its own.
    { PLAYER_MWA_LEFT_SLASH_1H,
      MHRW("hm_charge_attack03"),
      NULL,
      GMHR_DEMON_COMBO_SPEED,
      -1,
      { { 20, 60 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    { PLAYER_MWA_LEFT_COMBO_1H,
      MHRW("hm_charge_attack04"),
      NULL,
      GMHR_DEMON_COMBO_SPEED,
      -1,
      { { 25, 60 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    // Thrust: a charge attack in its own right — it summons a level-1 thunder, quickly
    // (GerudoMhr_DemonStabThunder).
    { PLAYER_MWA_STAB_1H,
      MHRW("gs_charge_attack01"),
      NULL,
      GMHR_DEMON_STAB_SPEED,
      -1,
      { { 28, 40 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      1 },
    { PLAYER_MWA_STAB_COMBO_1H,
      MHRW("gs_charge_attack01"),
      NULL,
      GMHR_DEMON_STAB_SPEED,
      -1,
      { { 28, 40 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      1 },
    { PLAYER_MWA_JUMPSLASH_START,
      MHRW("gs_dash_attack09"),
      NULL,
      2.5f,
      70,
      { { 36, 39 }, { 64, 65 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      0 },
    { PLAYER_MWA_JUMPSLASH_FINISH,
      MHRW("gs_charge_attack14"),
      NULL,
      2.0f,
      30,
      { { 1, 10 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      1 },
    // Charge release: the lightning drops on her during this one, hence the space at the
    // front of the window (GerudoMhr_ChargeSummonFrame drives the strike).
    { PLAYER_MWA_SPIN_ATTACK_1H,
      MHRW("gs_wirebug_attack04"),
      NULL,
      GMHR_DEMON_CHARGE_SPEED,
      -1,
      { { 20, 28 }, { 80, 110 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      1 },
    // Quick spin: only 20 source frames, so it plays at its own rate.
    { PLAYER_MWA_BIG_SPIN_1H,
      MHRW("gs_dash_attack36"),
      NULL,
      1.0f,
      -1,
      { { 5, 18 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN,
      GMHR_NOWIN,
      GMHR_NOWIN,
      1 },
};
static_assert(sizeof(sMhrDemonMeleeBindings) / sizeof(sMhrDemonMeleeBindings[0]) == GMHR_MELEE_BINDING_COUNT,
              "demon swing table must be row-parallel to the dual-blade one");

// ---- evasive jumps (D_80853D4C, served through VB_PLAYER_ANIM_SITE_DODGE_HOP) ----
typedef struct {
    s32 dir; // EXTPLAYER_JUMP_*
    const char* path;
    const char* ragePath;
    s16 rageEnd; // trim of the rage clip (-1 = whole)
} GMhrJumpBinding;

static const GMhrJumpBinding sMhrJumpBindings[] = {
    // Taken literally from the user's mapping (his eye on the clips beats their names).
    { EXTPLAYER_JUMP_SIDE_L, MHRP("RightRisingTripleLateralSlash"), MHRP("RightHighAerialSingleSilkbindSlash"), 40 },
    { EXTPLAYER_JUMP_SIDE_R, MHRP("LeftDoubleLateralSlash"), MHRP("LeftHighAerialTripleSilkbindSlash_Variant21"), 40 },
    // The backflip is built by hand (two playback rates): see GerudoMhr_GetHopAnim.
    { EXTPLAYER_JUMP_BACKFLIP, MHRP("BackwardHighAerialLeftLeadDoubleSilkbindSlash"),
      MHRP("BackwardRisingDoubleSilkbindDash"), -1 },
};
#define GMHR_JUMP_BINDING_COUNT ((s32)(sizeof(sMhrJumpBindings) / sizeof(sMhrJumpBindings[0])))

static const GMhrJumpBinding sMhrDemonJumpBindings[] = {
    { EXTPLAYER_JUMP_SIDE_L, MHRW("gs_side_attack01"), NULL, -1 },
    { EXTPLAYER_JUMP_SIDE_R, MHRW("gs_side_attack02"), NULL, -1 },
    { EXTPLAYER_JUMP_BACKFLIP, MHRW("gs_back_attack01"), NULL, -1 },
};
static_assert(sizeof(sMhrDemonJumpBindings) / sizeof(sMhrDemonJumpBindings[0]) == GMHR_JUMP_BINDING_COUNT,
              "demon hop table must be row-parallel to the dual-blade one");

// ---- single clips ---------------------------------------------------------
#define GMHR_FALL_CLIP MHRP("StationaryReadyIdle_Variant05")      // free fall, fighter
#define GMHR_WALK_CLIP MHRP("ForwardCombatRun")                   // fighter locomotion, every speed
#define GMHR_SPRINT_CLIP MHRP("ForwardCombatRun_Variant02")       // hold-A sprint only
#define GMHR_CRIT_IDLE_CLIP MHRP("StationaryReadyIdle_Variant08") // low-health idle
#define GMHR_CHARGE_STANCE MHRP("LowExtendedChargeStance")
#define GMHR_ROLL_CLIP MHRP("ForwardAcrobaticEvasion") // the roll (OOT's roll action, our clip)
#define GMHR_ROLL_END 30                               // trim: the tail is dead frames
#define GMHR_HIT_LIGHT_CLIP MHRP("ForwardEvasiveStep") // "soft damage while not running"

// ---- controller clips (played by this file, OOT paused) --------------------
typedef enum {
    GMHR_CLIP_NONE = -1,
    GMHR_CLIP_RAGE_ENTER = 0,
    GMHR_CLIP_RAGE_ROLL,
    GMHR_CLIP_RAGE_PARRY,
    GMHR_CLIP_FRONT_SLASH,
    GMHR_CLIP_RAGE_FRONT_START,
    GMHR_CLIP_RAGE_FRONT_STRIKE,
    GMHR_CLIP_AERIAL,
    GMHR_CLIP_RAGE_AERIAL_LOOP,
    GMHR_CLIP_RAGE_AERIAL_END,
    GMHR_CLIP_SHEATHE,
    GMHR_CLIP_DRAW_STAND,
    GMHR_CLIP_DRAW_RUN,
    GMHR_CLIP_FURY,  // L+B — Urbosa's Fury
    GMHR_CLIP_LUNGE, // L+A
    GMHR_CLIP_SPIN,  // L+R
    GMHR_CLIP_MAX,
} GMhrClipId;

typedef struct {
    const char* path;
    f32 speed;
    u8 loop;
    GMhrWin L[3], R[3];
} GMhrClip;

// ORDER MUST MATCH GMhrClipId (positional — MSVC C++ has no array designators).
static const GMhrClip sMhrClips[] = {
    { MHRP("DemonModeActivationFlourish"), 2.0f, 0, GMHR_NOWIN, GMHR_NOWIN }, // RAGE_ENTER
    { MHRP("BackwardMultiHitRetreatSlash_Variant06"),
      1.3f,
      0,
      { { 1, 20 }, { -1, -1 }, { -1, -1 } },
      { { 1, 20 }, { -1, -1 }, { -1, -1 } } }, // RAGE_ROLL (cyl 41-75 by state)
    { MHRP("ForwardRisingMultiHitChargedFlurry_Variant06"),
      1.5f,
      0,
      { { 78, 84 }, { -1, -1 }, { -1, -1 } },
      { { 78, 84 }, { -1, -1 }, { -1, -1 } } },                                   // RAGE_PARRY (massive at 80)
    { MHRP("BackwardRisingDoubleAerialSlash"), 1.2f, 0, GMHR_NOWIN, GMHR_NOWIN }, // FRONT_SLASH (cylinder by state)
    { MHRP("StationaryRisingLeftLeadTripleAerialSlash"), 1.3f, 0, GMHR_NOWIN, GMHR_NOWIN }, // RAGE_FRONT_START
    { MHRP("StationaryRisingTripleAerialSlash_Variant19"),
      1.2f,
      0,
      { { 2, 40 }, { -1, -1 }, { -1, -1 } },
      { { 2, 40 }, { -1, -1 }, { -1, -1 } } }, // RAGE_FRONT_STRIKE
    { MHRP("StationaryRisingTripleAerialSlash"),
      1.2f,
      0,
      { { 2, 40 }, { -1, -1 }, { -1, -1 } },
      { { 2, 40 }, { -1, -1 }, { -1, -1 } } }, // AERIAL
    { MHRP("StationarySingleSustainedBladeAction"), 1.0f, 1, GMHR_NOWIN,
      GMHR_NOWIN }, // RAGE_AERIAL_LOOP (cylinder by state)
    { MHRP("StationaryRisingTripleAerialSlash"),
      1.2f,
      0,
      { { 2, 40 }, { -1, -1 }, { -1, -1 } },
      { { 2, 40 }, { -1, -1 }, { -1, -1 } } }, // RAGE_AERIAL_END
    { MHRP("ForwardDoubleTwinSlash"), GMHR_DRAW_SPEED, 0, GMHR_NOWIN,
      GMHR_NOWIN }, // SHEATHE (blades vanish at GMHR_SHEATHE_HIDE_FRAME)
    { MHRP("ForwardDoubleTwinSlash"), GMHR_DRAW_SPEED, 0, GMHR_NOWIN,
      GMHR_NOWIN }, // DRAW_STAND  (the sheathe, played backwards)
    { MHRP("ForwardTripleRushSlash"), GMHR_DRAW_SPEED, 0, GMHR_NOWIN, GMHR_NOWIN }, // DRAW_RUN (played reversed)
    // The three L-attacks. Same clips on both sides of the table: the axe is not a mode,
    // so which moveset is installed makes no difference to them.
    { MHRW("gs_wirebug_attack04"), 2.4f, 0, { { 20, 28 }, { 80, 110 }, { -1, -1 } }, GMHR_NOWIN }, // FURY
    { MHRW("gs_charge_attack01"), 2.4f, 0, { { 28, 60 }, { -1, -1 }, { -1, -1 } }, GMHR_NOWIN },   // LUNGE
    { MHRW("gs_dash_attack09"), 2.0f, 0, { { 36, 39 }, { 64, 90 }, { -1, -1 } }, GMHR_NOWIN },     // SPIN
};
static_assert(sizeof(sMhrClips) / sizeof(sMhrClips[0]) == GMHR_CLIP_MAX, "sMhrClips must have one row per GMhrClipId");

// Demon mode's controller clips. Same order as GMhrClipId. The front slash is one clip in
// demon mode rather than the start/teleport/strike triple, so the three FRONT rows share
// it; the state machine still walks them, it just never changes what is on screen.
static const GMhrClip sMhrDemonClips[] = {
    // Entering IS the unsheathe: she slams the axe into the ground. 151 frames, and it
    // closes on its own first pose, so there is no snap when the flourish ends.
    { MHRW("hm_jump07"), 3.0f, 0, GMHR_NOWIN, GMHR_NOWIN },                                    // RAGE_ENTER
    { MHRW("gs_dash_attack03"), 2.0f, 0, { { 15, 19 }, { -1, -1 }, { -1, -1 } }, GMHR_NOWIN }, // RAGE_ROLL
    { MHRW("gs_jump_attack10"),
      2.5f,
      0,
      { { 26, 32 }, { -1, -1 }, { -1, -1 } },
      GMHR_NOWIN }, // RAGE_PARRY (close range)
    { MHRW("gs_dash_attack16"), 2.5f, 0, { { 42, 77 }, { -1, -1 }, { -1, -1 } }, GMHR_NOWIN },  // FRONT_SLASH
    { MHRW("gs_dash_attack16"), 2.5f, 0, { { 42, 77 }, { -1, -1 }, { -1, -1 } }, GMHR_NOWIN },  // RAGE_FRONT_START
    { MHRW("gs_dash_attack16"), 2.5f, 0, { { 42, 77 }, { -1, -1 }, { -1, -1 } }, GMHR_NOWIN },  // RAGE_FRONT_STRIKE
    { MHRW("gs_charge_attack04"), 1.5f, 0, { { 0, 13 }, { -1, -1 }, { -1, -1 } }, GMHR_NOWIN }, // AERIAL
    { MHRW("gs_idle03_loop"), 1.0f, 1, GMHR_NOWIN, GMHR_NOWIN },                                // RAGE_AERIAL_LOOP
    { MHRW("gs_charge_attack04"), 1.5f, 0, { { 0, 13 }, { -1, -1 }, { -1, -1 } }, GMHR_NOWIN }, // RAGE_AERIAL_END
    { MHRW("gs_jump01"), 2.0f, 0, GMHR_NOWIN, GMHR_NOWIN },                                     // SHEATHE
    { MHRW("hm_jump07"), 3.0f, 0, GMHR_NOWIN, GMHR_NOWIN },                                     // DRAW_STAND
    { MHRW("hm_jump07"), 3.0f, 0, GMHR_NOWIN, GMHR_NOWIN },                                     // DRAW_RUN
    // The three L-attacks. Same clips on both sides of the table: the axe is not a mode,
    // so which moveset is installed makes no difference to them.
    { MHRW("gs_wirebug_attack04"), 2.4f, 0, { { 20, 28 }, { 80, 110 }, { -1, -1 } }, GMHR_NOWIN }, // FURY
    { MHRW("gs_charge_attack01"), 2.4f, 0, { { 28, 60 }, { -1, -1 }, { -1, -1 } }, GMHR_NOWIN },   // LUNGE
    { MHRW("gs_dash_attack09"), 2.0f, 0, { { 36, 39 }, { 64, 90 }, { -1, -1 } }, GMHR_NOWIN },     // SPIN
};
static_assert(sizeof(sMhrDemonClips) / sizeof(sMhrDemonClips[0]) == GMHR_CLIP_MAX,
              "sMhrDemonClips must have one row per GMhrClipId");

// ---- which table set is live -----------------------------------------------
// There is only one moveset. The axe is not a mode she enters, it is three L-attacks, and
// those are controller clips that pause OOT — they never read a table. Swapping the tables
// for them is what made her walk, guard and combo change weapon mid-attack.
// The sMhrDemon* tables are kept for the moment: they are the measured axe moveset, and
// re-deriving those windows would be a day's work if it ever comes back.
static u8 MmForm_GerudoDemon(void);
static const GMhrGroupBinding* MmForm_GerudoGroupTable(void) {
    return sMhrGroupBindings;
}
static const GMhrMeleeBinding* MmForm_GerudoMeleeTable(void) {
    return sMhrMeleeBindings;
}
static const GMhrJumpBinding* MmForm_GerudoJumpTable(void) {
    return sMhrJumpBindings;
}
static const GMhrClip* MmForm_GerudoClipTable(void) {
    return sMhrClips;
}

// Single clips that also change with the weapon.
#define GMHR_DEMON_FALL_CLIP MHRW("gs_idle03_loop")
#define GMHR_DEMON_IDLE_CLIP MHRW("gs_idle03_loop")
#define GMHR_DEMON_WALK_CLIP MHRW("gs_run01_loop")
#define GMHR_DEMON_SPRINT_CLIP MHRW("hm_dash_attack04")
#define GMHR_DEMON_CHARGE_START MHRW("gs_back_attack07")
#define GMHR_DEMON_CHARGE_STANCE MHRW("gs_idle03_loop")
#define GMHR_DEMON_ROLL_CLIP MHRW("gs_dash_attack03")
#define GMHR_DEMON_ROLL_END 35
// The long-range parry: launch, home in, land. Three clips, played as one move.
// 27 frames and it CLIMBS: the root gains 5150 units of height over the clip, which is
// the launch itself — this one clip is why the far parry gets off the ground. It is also
// the only insect glaive clip in the moveset, hence its own axe placement.
#define GMHR_DEMON_PARRY_FAR_LAUNCH MHRIG("ForwardHighAerialMultiHitSilkbindStaffStrike_Variant18")
#define GMHR_DEMON_PARRY_FAR_DIVE MHRW("hm_charge_attack12")
#define GMHR_DEMON_PARRY_FAR_LAND MHRW("hm_motion11")
// ForwardDoubleTwinSlash is 32 frames and the blades leave the hands here (the map
// says "deben desaparecer en frame 17"). Drawing is the same clip run backwards, so
// the SAME frame is where they come back — one number owns both directions.
#define GMHR_SHEATHE_HIDE_FRAME 17.0f

// ===========================================================================
// Section 2 — state
// ===========================================================================
typedef enum {
    GMHR_IDLE = 0,
    GMHR_RAGE_ENTER,
    GMHR_RAGE_EXIT, // unused since the axe became three L-attacks instead of a mode
    GMHR_FURY,      // L+B
    GMHR_LUNGE,     // L+A
    GMHR_SPIN,      // L+R
    GMHR_RAGE_ROLL,
    GMHR_RAGE_PARRY,
    GMHR_FRONT_SLASH,
    GMHR_RAGE_FRONT_START,
    GMHR_RAGE_FRONT_STRIKE,
    GMHR_AERIAL,
    GMHR_RAGE_AERIAL_LOOP,
    GMHR_RAGE_AERIAL_END,
    GMHR_SHEATHE,
    GMHR_DRAW,
} GMhrState;

static struct {
    u8 inited;
    GMhrState state;
    GMhrClipId clipId;
    s16 timer;
    f32 prevFrame;
    // visual root pin for the running clip
    u8 rootInit;
    s16 baseRootX, baseRootY, baseRootZ;
    // sword visibility override while drawing / sheathing (0xFF = follow the item)
    u8 swordsVisible;
    // per-frame blade gate handed to the draw callbacks
    u8 bladeMask;     // bit0 L, bit1 R
    u8 bladeOwnFlags; // 1 = write dmgFlags/damage ourselves (controller clip); 0 = keep OOT's
    u32 bladeDmgFlags;
    u8 bladeDamage;
    u8 trailOn;
    s16 trailKill;
    // body cylinder (spins, rage roll, front slash, rage aerial, parry stun)
    u8 cylOn;
    u32 cylDmgFlags;
    u8 cylDamage;
    s16 cylRadius; // 0 = the default spin radius; Urbosa's Fury grows it every frame
    s16 cylHeight; // 0 = the default spin height
    u8 furyDamage; // Fury's power, fixed at cast from how full the meter was
    // OOT-swing tracking
    u8 wasSwinging;
    // parry window
    s16 shieldFrames;
    // rage-front teleport
    Actor* frontTarget;
    // A tap/hold detector
    u8 aPending, aSprint;
    s16 aFrames;
    // frames airborne (the aerial slash needs a press made in the air, not the
    // ground press that started the hop / jump slash)
    s16 airFrames;
    // draw / sheathe: the item handed over at the end
    s32 pendingItem;
    s8 pendingIA;
} sMhr;

static struct {
    u8 active;
    s16 meter;
    s16 timer;
} sMhrRage;

static u8 sGerudoSprinting = 0;
static s32 sGerudoComboStep = 0;
static s32 sGerudoComboIdle = 0;
static u8 sGerudoLastFighter = 0xFF;
// Jump slash (GerudoMhr_TickJumpSlash): 0 rising, 1 frozen, 2 slashing down, 3 loop.
// sGerudoJsTrail is read by MmForm_GerudoTickOotSwing, which would otherwise put the
// blades out every frame — the jump slash is not one of OOT's ground swings.
static u8 sGerudoJsPhase = 0;
static u8 sGerudoJsTrail = 0;

static ColliderCylinder sMhrCyl;
static u8 sMhrCylInit = 0;

// Not const: Collider_SetCylinder takes a non-const pointer.
static ColliderCylinderInit sMhrCylInitData = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_SPIN_MASTER, 0x00, 0x02 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { GMHR_SPIN_CYL_RADIUS, GMHR_SPIN_CYL_HEIGHT, 0, { 0, 0, 0 } },
};

// ===========================================================================
// Section 3 — rage meter (public)
// ===========================================================================
// Which MHR weapon a demon clip was animated for. The hammer and the great sword hold
// their weapon at completely different angles, so the axe needs a placement per family or
// it only ever looks right in one of them. Derived from the clip PATH, which is the one
// thing every source of animation here has in common — table rows, controller clips and
// the on-demand loaders all name their clip.
typedef enum {
    GMHR_AXE_FAMILY_GS = 0, // gPlayerAnim_mhr_gs_*  (Great Sword)
    GMHR_AXE_FAMILY_HM,     // gPlayerAnim_mhr_hm_*  (Hammer)
    GMHR_AXE_FAMILY_IG,     // gMonsterHunterRise_InsectGlaive_*
    GMHR_AXE_FAMILY_MAX,
} GMhrAxeFamily;

static s32 MmForm_GerudoAxeFamilyOfPath(const char* path) {
    if (path == NULL)
        return GMHR_AXE_FAMILY_GS;
    if (strstr(path, "InsectGlaive") != NULL)
        return GMHR_AXE_FAMILY_IG;
    if (strstr(path, "_hm_") != NULL)
        return GMHR_AXE_FAMILY_HM;
    return GMHR_AXE_FAMILY_GS;
}

// Is the axe out? Derived from the state, never latched. A flag had to be cleared on
// every way out of an L-attack — the end of the clip, damage, leaving the form — and any
// path that forgot left her holding the axe with no blades for the rest of the game.
static u8 MmForm_GerudoDemon(void) {
    return (sMhr.state == GMHR_FURY) || (sMhr.state == GMHR_LUNGE) || (sMhr.state == GMHR_SPIN);
}

// Dialling switch for the Item Editor's axe panel: lets L enter demon mode without a full
// meter and stops the fuel draining, so the placement can be worked on without refilling
// every few seconds. It does NOT force the state on — L still has to be pressed, so the
// enter clip and the table swap run exactly as they do in play.
static u8 MmForm_GerudoForceDemon(void) {
    return CVarGetInteger("gItemEditor.GerudoAxe.FreeDemon", 0) != 0;
}

// "The axe is out." Everything outside this file asks through here — the hand DLs, the
// axe draw, the single trail — so it has to answer the same thing MmForm_GerudoDemon does
// inside it, or the state and what is on screen disagree.
u8 GerudoMhr_RageActive(void) {
    return (gFormState.currentForm == MM_PLAYER_FORM_GERUDO) && MmForm_GerudoDemon();
}
// The tank the L-attacks are paid out of. Magic is the upgrade: none / single / double
// buys x1 / x2 / x4, so magic buys USES.
s16 GerudoMhr_RageCapacity(void) {
    s32 level = gSaveContext.magicLevel;
    if (level < 0) {
        level = 0;
    }
    if (level > 2) {
        level = 2;
    }
    return (s16)(GMHR_RAGE_MAX << level);
}

// What one blade hit is worth.
static s16 GerudoMhr_RageGain(void) {
    s16 gain = (s16)(GerudoMhr_RageCapacity() / GMHR_RAGE_HITS_TO_FILL);
    return (gain < 1) ? 1 : gain;
}

u8 GerudoMhr_RageReady(void) {
    return (gFormState.currentForm == MM_PLAYER_FORM_GERUDO) && !sMhrRage.active &&
           (sMhrRage.meter >= (GerudoMhr_RageCapacity() / 4));
}
f32 GerudoMhr_RageFill(void) {
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0.0f;
    return (f32)sMhrRage.meter / (f32)GerudoMhr_RageCapacity();
}
// The single clips (the ones not in a table) also swap with the weapon.
static const char* MmForm_GerudoPick(const char* normal, const char* rage) {
    return (sMhrRage.active && (rage != NULL)) ? rage : normal;
}

// The charge release, built at three rates. ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange
// can only resample a range at ONE rate, so the clip is cut into three, each resampled on
// its own, and the results concatenated into one buffer — the same trick the two-rate
// backflip uses. Rebuilt on every install (they happen on form enter and on the rage
// flip, never per frame), so there is no stale cache to reason about.
static s16 sGerudoChargeSummonFrame = 0; // installed frame the thunder is thrown on

static LinkAnimationHeader* MmForm_GerudoChargeReleaseClip(const char* path, f32 baseMul, s16* outFrames) {
    static LinkAnimationHeader sClip = { { 0 }, NULL };
    static s16* sClipData = NULL;

    LinkAnimationHeader* raw = MmForm_MhrLoadPath(path);
    if (raw == NULL)
        return NULL;

    s16 last = raw->common.frameCount - 1;
    s16 beg = GMHR_CHARGE_FAST_BEG;
    s16 end = GMHR_CHARGE_FAST_END;
    if (end > last)
        end = last;
    if (beg > end)
        beg = end;

    s16 fa = (s16)(((f32)beg / baseMul) + 0.5f);                                      // source 0 .. beg-1
    s16 fb = (s16)(((f32)(end - beg + 1) / (baseMul * GMHR_CHARGE_FAST_MUL)) + 0.5f); // beg .. end
    s16 fc = (s16)(((f32)(last - end) / baseMul) + 0.5f);                             // end+1 .. last
    if (fa < 1)
        fa = 1;
    if (fb < 1)
        fb = 1;
    if (fc < 1)
        fc = 1;

    LinkAnimationHeader* pa = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(path, 0, 0, beg - 1, fa);
    LinkAnimationHeader* pb = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(path, 0, beg, end, fb);
    LinkAnimationHeader* pc =
        (end < last) ? ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(path, 0, end + 1, last, fc) : NULL;
    if ((pa == NULL) || (pb == NULL))
        return NULL;

    s32 na = pa->common.frameCount;
    s32 nb = pb->common.frameCount;
    s32 nc = (pc != NULL) ? pc->common.frameCount : 0;
    s32 total = na + nb + nc;

    s16* buf = (s16*)malloc(sizeof(s16) * GMHR_ANIM_S16_PER_FRAME * total);
    if (buf == NULL)
        return NULL;
    memcpy(buf, pa->segment, sizeof(s16) * GMHR_ANIM_S16_PER_FRAME * na);
    memcpy(buf + GMHR_ANIM_S16_PER_FRAME * na, pb->segment, sizeof(s16) * GMHR_ANIM_S16_PER_FRAME * nb);
    if (nc > 0) {
        memcpy(buf + GMHR_ANIM_S16_PER_FRAME * (na + nb), pc->segment, sizeof(s16) * GMHR_ANIM_S16_PER_FRAME * nc);
    }

    if (sClipData != NULL)
        free(sClipData);
    sClipData = buf;
    sClip.common.frameCount = (s16)total;
    sClip.segment = buf;
    sGerudoChargeSummonFrame = (s16)na;
    *outFrames = (s16)total;
    return &sClip;
}

// The installed frame En_M_Thunder is thrown on (0 = not built, use the generic rule).
// Demon mode's release is a different clip built at one rate, so it falls back to that
// generic rule rather than reusing the dual blades' measured split.
s16 GerudoMhr_ChargeSummonFrame(void) {
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    return MmForm_GerudoDemon() ? 0 : sGerudoChargeSummonFrame;
}

// ===========================================================================
// Section 4 — install / restore
// ===========================================================================
static struct {
    u8 installed;
    u8 rage;
    LinkAnimationHeader* savedGroup[GMHR_GROUP_BINDING_COUNT][PLAYER_ANIMTYPE_MAX];
    LinkAnimationHeader* savedMelee[GMHR_MELEE_BINDING_COUNT];
    LinkAnimationHeader* savedMeleeEnd[GMHR_MELEE_BINDING_COUNT];
    LinkAnimationHeader* savedMeleeEndLock[GMHR_MELEE_BINDING_COUNT];
    u8 savedHitStart[GMHR_MELEE_BINDING_COUNT];
    u8 savedHitEnd[GMHR_MELEE_BINDING_COUNT];
    LinkAnimationHeader* savedCritIdle[2];
    LinkAnimationHeader* savedHit[4];
    LinkAnimationHeader* savedCharge[EXTPLAYER_CHARGE_PHASE_MAX][2];
    LinkAnimationHeader* savedHopLand[4][2];  // D_80853D4C[dir][1..2]
    f32 swingScale[GMHR_MELEE_BINDING_COUNT]; // source frames per installed frame
} sMhrTables;

static s16 MmForm_GerudoGroupFrames(const GMhrGroupBinding* b) {
    if (b->frames > 0)
        return b->frames; // a forced length is the locomotion rig's, never ours to trim
    if ((b->speedMul <= 0.0f) || (b->srcStart < 0) || (b->srcEnd < b->srcStart))
        return 0;
    f32 mul = MmForm_GerudoDemon() ? (b->speedMul * GMHR_DEMON_SPEED_SCALE) : b->speedMul;
    s16 len = (s16)(b->srcEnd - b->srcStart + 1);
    s16 out = (s16)(((f32)len / mul) + 0.5f);
    return (out < 2) ? 2 : out;
}

void MmForm_GerudoInstallAnims(void) {
    if (sMhrTables.installed)
        return;

    for (s32 i = 0; i < GMHR_GROUP_BINDING_COUNT; i++) {
        const GMhrGroupBinding* b = &MmForm_GerudoGroupTable()[i];
        LinkAnimationHeader* anim = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(b->path, 0, b->srcStart, b->srcEnd,
                                                                                   MmForm_GerudoGroupFrames(b));
        for (s32 col = 0; col < PLAYER_ANIMTYPE_MAX; col++) {
            sMhrTables.savedGroup[i][col] = ExtPlayer_GetAnimGroupAnim(b->group, col);
            if ((anim != NULL) && (GMHR_FIGHTER_COLUMNS_MASK & (1 << col))) {
                ExtPlayer_SetAnimGroupAnim(b->group, col, anim);
            }
        }
        if (anim == NULL)
            SPDLOG_WARN("[GerudoMHR] missing locomotion clip {}", b->path);
    }

    for (s32 i = 0; i < GMHR_MELEE_BINDING_COUNT; i++) {
        const GMhrMeleeBinding* b = &MmForm_GerudoMeleeTable()[i];
        ExtPlayer_GetMeleeAnim(b->mwa, &sMhrTables.savedMelee[i], &sMhrTables.savedMeleeEnd[i],
                               &sMhrTables.savedMeleeEndLock[i], &sMhrTables.savedHitStart[i],
                               &sMhrTables.savedHitEnd[i]);
        sMhrTables.swingScale[i] = 1.0f;
        const char* path = b->path;
        LinkAnimationHeader* raw = MmForm_MhrLoadPath(path);
        if (raw == NULL) {
            SPDLOG_WARN("[GerudoMHR] missing swing clip {}", path);
            continue;
        }
        s16 last = raw->common.frameCount - 1;
        f32 mul = (b->speedMul > 0.0f) ? b->speedMul : 1.0f;
        if (MmForm_GerudoDemon())
            mul *= GMHR_DEMON_SPEED_SCALE;
        // Demon swings COMMIT: the clip is never cut, so OOT only chains the next hit when
        // the whole animation has run. The dual blades cut theirs early on purpose, which
        // is what makes that moveset cancellable and this one not.
        s16 swingEnd = ((b->swingEnd >= 0) && (b->swingEnd < last) && !MmForm_GerudoDemon()) ? b->swingEnd : last;
        // OOT plays every sword SWING through Player_AnimPlayOnceAdjusted, i.e. at 2/3
        // speed (Link's clips are authored for it). Ours are not, so the swing is
        // resampled 1.5x shorter to come out at real time. The recovery is played by
        // func_8083328C at 1.0, so it is NOT compensated.
        f32 swingMul = mul * GMHR_OOT_SWING_SPEED_COMP;
        s16 swingFrames = (s16)(((f32)(swingEnd + 1) / swingMul) + 0.5f);
        if (swingFrames < 2)
            swingFrames = 2;
        LinkAnimationHeader* swing;
        if ((b->mwa == PLAYER_MWA_SPIN_ATTACK_1H) && !MmForm_GerudoDemon()) {
            // The dual-blade charge release runs at three rates; the builder reports its
            // own length. GMHR_CHARGE_FAST_BEG/END are frames of THAT clip, so demon mode
            // — a different clip entirely — takes the plain single-rate path.
            swing = MmForm_GerudoChargeReleaseClip(path, swingMul, &swingFrames);
            if (swing == NULL) {
                swing = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(path, 0, 0, swingEnd, swingFrames);
            }
        } else {
            swing = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(path, 0, 0, swingEnd, swingFrames);
        }
        // Recovery = the clip's own tail (or a short hold of the last pose).
        LinkAnimationHeader* rec;
        if (swingEnd < last) {
            // mul, not swingMul: the recovery is played by func_8083328C at 1.0.
            s16 tailFrames = (s16)(((f32)(last - swingEnd) / mul) + 0.5f);
            if (tailFrames < 2)
                tailFrames = 2;
            rec = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(path, 0, swingEnd + 1, last, tailFrames);
        } else {
            rec = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(path, 0, last, last, 4);
        }
        sMhrTables.swingScale[i] = (f32)(swingEnd + 1) / (f32)swingFrames;
        // OOT's own two-number window: from the earliest to the latest window frame,
        // in installed frames. Our per-hand gate refines it in the draw callback.
        u8 useRage = sMhrRage.active && (b->ragePath != NULL);
        const GMhrWin* Lw = useRage ? b->rageL : b->L;
        const GMhrWin* Rw = useRage ? b->rageR : b->R;
        s16 first = 0x7FFF, lastW = -1;
        for (s32 k = 0; k < 3; k++) {
            if (Lw[k].end >= Lw[k].beg) {
                if (Lw[k].beg < first)
                    first = Lw[k].beg;
                if (Lw[k].end > lastW)
                    lastW = Lw[k].end;
            }
            if (Rw[k].end >= Rw[k].beg) {
                if (Rw[k].beg < first)
                    first = Rw[k].beg;
                if (Rw[k].end > lastW)
                    lastW = Rw[k].end;
            }
        }
        u8 hitStart = 0xFF, hitEnd = 0xFF;
        if (lastW >= 0) {
            hitStart = (u8)((f32)first / swingMul);
            hitEnd = (u8)((f32)lastW / swingMul + 0.999f);
            if (hitEnd >= swingFrames)
                hitEnd = (u8)(swingFrames - 1);
        }
        if (swing != NULL) {
            ExtPlayer_SetMeleeAnim(b->mwa, swing, rec, rec, hitStart, hitEnd);
        }
    }

    {
        LinkAnimationHeader* crit =
            ResourceMgr_LoadPlayerAnimAsHeaderInPlace(MmForm_GerudoPick(GMHR_CRIT_IDLE_CLIP, GMHR_DEMON_IDLE_CLIP), 0);
        const s32 slots[2] = { EXTPLAYER_FIDGET_CRIT_START, EXTPLAYER_FIDGET_CRIT_LOOP };
        for (s32 i = 0; i < 2; i++) {
            sMhrTables.savedCritIdle[i] = ExtPlayer_GetFidgetAnim(slots[i], 1);
            if (crit != NULL)
                ExtPlayer_SetFidgetAnim(slots[i], 1, crit);
        }
    }
    {
        // Light hit reactions (front/back, short) — the "soft damage" clip.
        LinkAnimationHeader* hit =
            ResourceMgr_LoadPlayerAnimAsHeaderInPlace(MmForm_GerudoPick(GMHR_HIT_LIGHT_CLIP, GMHR_DEMON_IDLE_CLIP), 0);
        for (s32 i = 0; i < 4; i++) {
            sMhrTables.savedHit[i] = ExtPlayer_GetHitAnim(i);
            if (hit != NULL)
                ExtPlayer_SetHitAnim(i, hit);
        }
    }
    {
        // The charge. OOT plays the START clip from frame 8 to its end and only then
        // starts accumulating charge (Player_Action_80844E68: nothing happens, not
        // even the release, until the start animation is over). A 173-frame stance in
        // that slot meant holding B did nothing visible for three seconds. So START is
        // a short slice and everything else is the held loop.
        // Demon mode has a separate windup clip instead of a slice of the stance, so the
        // ranges differ: gs_back_attack07 (40f) into gs_idle03_loop, which loops on its own.
        LinkAnimationHeader* start;
        LinkAnimationHeader* loop;
        if (MmForm_GerudoDemon()) {
            start = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(GMHR_DEMON_CHARGE_START, 0, -1, -1, 14);
            loop = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(GMHR_DEMON_CHARGE_STANCE, 0, -1, -1, 35);
        } else {
            start = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(GMHR_CHARGE_STANCE, 0, 0, 24, 14);
            loop = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(GMHR_CHARGE_STANCE, 0, 25, 172, 60);
        }
        for (s32 p = 0; p < EXTPLAYER_CHARGE_PHASE_MAX; p++) {
            for (s32 h = 0; h < 2; h++) {
                LinkAnimationHeader* pick =
                    (p == EXTPLAYER_CHARGE_START || p == EXTPLAYER_CHARGE_START_L) ? start : loop;
                sMhrTables.savedCharge[p][h] = ExtPlayer_GetChargeAnim(p, h);
                if (pick != NULL)
                    ExtPlayer_SetChargeAnim(p, h, pick);
            }
        }
    }

    {
        // Hop landings (slots 1/2 of every direction): the touchdown clip, short.
        // Slot 0 (the hop itself) is served through VB_PLAYER_ANIM_SITE_DODGE_HOP.
        LinkAnimationHeader* land = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(MHRP("ForwardSingleTwinSlash"), 0,
                                                                                   -1, -1, GMHR_HOP_LAND_FRAMES);
        for (s32 d = 0; d < 4; d++) {
            for (s32 k = 1; k <= 2; k++) {
                sMhrTables.savedHopLand[d][k - 1] = ExtPlayer_GetJumpAnim(d, k);
                if (land != NULL)
                    ExtPlayer_SetJumpAnim(d, k, land);
            }
        }
    }

    sMhrTables.rage = sMhrRage.active;
    sMhrTables.installed = 1;
    SPDLOG_INFO("[GerudoMHR] tables installed (rage={})", (int)sMhrTables.rage);
}

// MUST run on every path out of the form: these are global engine tables.
void MmForm_GerudoRestoreAnims(void) {
    if (!sMhrTables.installed)
        return;
    for (s32 i = 0; i < GMHR_GROUP_BINDING_COUNT; i++) {
        for (s32 col = 0; col < PLAYER_ANIMTYPE_MAX; col++) {
            ExtPlayer_SetAnimGroupAnim(sMhrGroupBindings[i].group, col, sMhrTables.savedGroup[i][col]);
        }
    }
    for (s32 i = 0; i < GMHR_MELEE_BINDING_COUNT; i++) {
        ExtPlayer_SetMeleeAnim(sMhrMeleeBindings[i].mwa, sMhrTables.savedMelee[i], sMhrTables.savedMeleeEnd[i],
                               sMhrTables.savedMeleeEndLock[i], sMhrTables.savedHitStart[i], sMhrTables.savedHitEnd[i]);
    }
    {
        const s32 slots[2] = { EXTPLAYER_FIDGET_CRIT_START, EXTPLAYER_FIDGET_CRIT_LOOP };
        for (s32 i = 0; i < 2; i++)
            ExtPlayer_SetFidgetAnim(slots[i], 1, sMhrTables.savedCritIdle[i]);
    }
    for (s32 i = 0; i < 4; i++)
        ExtPlayer_SetHitAnim(i, sMhrTables.savedHit[i]);
    for (s32 d = 0; d < 4; d++) {
        for (s32 k = 1; k <= 2; k++)
            ExtPlayer_SetJumpAnim(d, k, sMhrTables.savedHopLand[d][k - 1]);
    }
    for (s32 p = 0; p < EXTPLAYER_CHARGE_PHASE_MAX; p++) {
        for (s32 h = 0; h < 2; h++)
            ExtPlayer_SetChargeAnim(p, h, sMhrTables.savedCharge[p][h]);
    }
    sMhrTables.installed = 0;
    SPDLOG_INFO("[GerudoMHR] tables restored");
}

// ===========================================================================
// Section 5 — fighter / free, guard, weapon identity (public, called by OOT)
// ===========================================================================
// 1 while Gerudo is a fighter: blades in hand, or guarding.
u8 GerudoMhr_ForcesFighter(Player* player) {
    if (player == NULL)
        return 0;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    if (player->stateFlags1 & PLAYER_STATE1_SHIELDING)
        return 1;
    return Player_ActionToMeleeWeapon(player->heldItemAction) > 0;
}

// The melee-weapon index OOT should see for Gerudo (1..3 = Kokiri/Master/BGS,
// 0 = nothing). Player_GetMeleeWeaponHeld returns 0 for every other form; this is
// what lets OOT's whole sword pipeline run for her.
s32 GerudoMhr_MeleeWeaponIndex(Player* player) {
    if (player == NULL)
        return 0;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    s32 idx = Player_ActionToMeleeWeapon(player->heldItemAction);
    return (idx >= 1 && idx <= 3) ? idx : 0;
}

// Damage tier row for func_80837948 (0 Kokiri, 1 Master, 2 BGS). Rage hits for
// double: one tier up, which is exactly how OOT doubles sword damage.
s32 GerudoMhr_DamageTier(Player* player, s32 tier) {
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return tier;
    if (!sMhrRage.active)
        return tier;
    return (tier < 2) ? tier + 1 : 2;
}

// R = the blade guard, always, for Gerudo. No shield item needed.
u8 GerudoMhr_UsesBladeGuard(Player* player) {
    if (player == NULL)
        return 0;
    return gFormState.currentForm == MM_PLAYER_FORM_GERUDO;
}

// Draw-time upper-body offset for the guard (applied in MmForm_OverrideLimbDraw,
// on top of OOT's own upperLimbRot aim). NOT written into upperLimbRot: the
// shield action steps that field from its own previous value every frame, so an
// offset stored there compounds.
u8 GerudoMhr_GetShieldUpperRot(Player* player, Vec3s* out) {
    if (out == NULL || player == NULL)
        return 0;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    if (!(player->stateFlags1 & PLAYER_STATE1_SHIELDING))
        return 0;
    out->x = GMHR_SHIELD_ROT_X;
    out->y = GMHR_SHIELD_ROT_Y;
    out->z = GMHR_SHIELD_ROT_Z;
    return 1;
}

// Per-shoulder offset, same deal: the guard is a held pose, not an animation, so the arms
// can only be posed at draw time. Both shoulders are at rest in the dialled-in pose, so
// this reports "nothing to do" — the hook stays because the arms are the first thing to
// want touching if the guard is ever re-posed.
u8 GerudoMhr_GetShieldShoulderRot(Player* player, s32 limbIndex, Vec3s* out) {
    if (out == NULL || player == NULL)
        return 0;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    if (!(player->stateFlags1 & PLAYER_STATE1_SHIELDING))
        return 0;
    if (limbIndex == PLAYER_LIMB_L_SHOULDER) {
        out->x = GMHR_SHOULDER_L_ROT_X;
        out->y = GMHR_SHOULDER_L_ROT_Y;
        out->z = GMHR_SHOULDER_L_ROT_Z;
    } else if (limbIndex == PLAYER_LIMB_R_SHOULDER) {
        out->x = GMHR_SHOULDER_R_ROT_X;
        out->y = GMHR_SHOULDER_R_ROT_Y;
        out->z = GMHR_SHOULDER_R_ROT_Z;
    } else {
        return 0;
    }
    return (out->x != 0) || (out->y != 0) || (out->z != 0);
}

// The guard clips by phase (0 raise 1-20, 1 loop 21-30, 2 release 31-45), served
// straight to OOT's shield code paths (VB SHIELD_RAISE / SHIELD_LOOP, the release
// in Player_Action_80843188, and the Z-target upper-body guard in func_808346C4),
// so the guard does not depend on which animation column OOT happens to be reading.
LinkAnimationHeader* GerudoMhr_GetGuardAnim(Player* player, s32 phase) {
    if (player == NULL)
        return NULL;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return NULL;
    static const s16 sRange[3][2] = { { 1, 20 }, { 30, 30 }, { 31, 45 } };
    if (phase < 0 || phase > 2)
        return NULL;
    s16 len = (s16)(sRange[phase][1] - sRange[phase][0] + 1);
    s16 frames = (s16)(((f32)len / GMHR_SHIELD_SPEED) + 0.5f);
    if (frames < 2)
        frames = 2;
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(MHRP("DemonModeActivationFlourish"), 0, sRange[phase][0],
                                                          sRange[phase][1], frames);
}

// OOT's hold-B charge only starts if unk_844 (8 at swing start, -1 per frame) is
// EXACTLY 1 the frame after the swing ends — which is only true for Link's own
// 7-frame swings. Ours are 25-41 frames, so the counter is pinned to 3 for as long
// as the swing runs with B held: it then reads 2 on the swing's last frame, 1 on the
// first frame of the recovery, and Player_ActionHandler_8 starts the charge.
u8 GerudoMhr_HoldsChargeWindow(Player* player) {
    if (player == NULL)
        return 0;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    if (player->actionFunc != Player_Action_808502D0)
        return 0;
    if (player->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H)
        return 0;
    if (gPlayState == NULL)
        return 0;
    Input* in = &gPlayState->state.input[0];
    return CHECK_BTN_ALL(in->cur.button, BTN_B) ? 1 : 0;
}

// L is the modifier: while it is down, B belongs to the form (L+B = front slash),
// so OOT must not see it. TransformMasks_FilterB asks this on the input copy OOT
// reads; the controller reads the raw input and still sees the press.
// R+B is the front slash, so B must not reach OOT while R is held — otherwise the same
// press also starts an ordinary swing and the two fight over the animation. (Was L+B
// until demon mode took L over.)
// Forward with no lock-on, or out of a sprint: what used to launch the thrust now launches
// the front slash. Read in two places, so it lives here.
static u8 MmForm_GerudoWantsFrontSlash(Player* player) {
    if (sGerudoSprinting)
        return 1;
    return (player->focusActor == NULL) && (TransformMasks_GetStickMagnitude() >= 10.0f) &&
           (MmForm_GetStickDirection(player) == PLAYER_STICK_DIR_FORWARD);
}

// Asked from INSIDE Player_Action_Roll, where "is she rolling" is already true — testing
// actionFunc there as well only added a way for the guard to silently fail.
u8 GerudoMhr_RollCommits(Player* player) {
    return GerudoMhr_ForcesFighter(player);
}

static u8 MmForm_GerudoIsRolling(Player* player) {
    return player->actionFunc == Player_Action_Roll;
}

u8 GerudoMhr_LOwnsB(void) {
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    if (gPlayState == NULL)
        return 0;
    Input* in = &gPlayState->state.input[0];
    if (CHECK_BTN_ALL(in->cur.button, BTN_R)) {
        return 1;
    }
    return MmForm_GerudoWantsFrontSlash(GET_PLAYER(gPlayState));
}

// Are the scimitars drawn in the hands (gerudo_form.cpp reads this for the DLs)?
u8 GerudoMhr_SwordsOut(void) {
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    if (gPlayState == NULL)
        return 0;
    if (sMhr.swordsVisible != 0xFF)
        return sMhr.swordsVisible;
    Player* p = GET_PLAYER(gPlayState);
    if (p == NULL)
        return 0;
    return Player_ActionToMeleeWeapon(p->heldItemAction) > 0;
}

// Fighter is a model-group property OOT only recomputes on an item change; the
// guard flips it without one, so watch the verdict.
static void MmForm_GerudoTickFighter(Player* player) {
    u8 now = GerudoMhr_ForcesFighter(player);
    if (now == sGerudoLastFighter)
        return;
    sGerudoLastFighter = now;
    Player_SetModelGroup(player, Player_ActionToModelGroup(player, player->heldItemAction));
}

// ===========================================================================
// Section 6 — OOT-swing bridging (public, called by OOT)
// ===========================================================================
static const s32 sGerudoComboRows[] = {
    PLAYER_MWA_FORWARD_SLASH_1H,
    PLAYER_MWA_FORWARD_COMBO_1H,
    PLAYER_MWA_RIGHT_SLASH_1H,
    PLAYER_MWA_RIGHT_COMBO_1H,
};
static const s32 sGerudoRageComboRows[] = { PLAYER_MWA_FORWARD_SLASH_1H, PLAYER_MWA_FORWARD_COMBO_1H,
                                            PLAYER_MWA_RIGHT_SLASH_1H };
#define GMHR_COMBO_STEPS ((s32)(sizeof(sGerudoComboRows) / sizeof(sGerudoComboRows[0])))
#define GMHR_RAGE_COMBO_STEPS ((s32)(sizeof(sGerudoRageComboRows) / sizeof(sGerudoRageComboRows[0])))
#define GMHR_COMBO_RESET_FRAMES 40

// The reset timer measures the GAP BETWEEN hits, so it must not run during one. Demon
// swings are ~37 frames uncut, which is past GMHR_COMBO_RESET_FRAMES on its own: counting
// through the swing reset the chain before the player could ever press B again, and every
// hit came out as combo 1.
static void GerudoMhr_TickCombo(Player* player) {
    if (sGerudoComboStep == 0)
        return;
    if ((player != NULL) && (player->meleeWeaponState != 0)) {
        sGerudoComboIdle = 0;
        return;
    }
    if (++sGerudoComboIdle >= GMHR_COMBO_RESET_FRAMES) {
        sGerudoComboStep = 0;
        sGerudoComboIdle = 0;
    }
}

// The row func_80837948 swings. Only the ground chain and the thrust are ours;
// jump/flip slash and the spins keep the row OOT computed.
s32 GerudoMhr_NextComboMwa(Player* player, s32 requested) {
    if (!GerudoMhr_ForcesFighter(player))
        return requested;
    // OOT uses the SPIN_ATTACK row for BOTH the stick-rotation quick spin and the
    // level-1 charge release. The user wants them apart: the quick spin is the triple
    // rush (BIG_SPIN row), the release is the tumble cross (SPIN_ATTACK row). A charge
    // release always arrives with CHARGING_SPIN_ATTACK still set by the charge action.
    if ((requested == PLAYER_MWA_SPIN_ATTACK_1H) && !(player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
        return PLAYER_MWA_BIG_SPIN_1H;
    }
    if ((requested == PLAYER_MWA_SPIN_ATTACK_1H) || (requested == PLAYER_MWA_BIG_SPIN_1H)) {
        // Every charge release (level 1 AND 2) is the charge attack: the tumble cross,
        // thrown forward by OOT's own lunge (linearVelocity 15 on frame 0, decaying).
        player->stateFlags2 |= PLAYER_STATE2_SWORD_LUNGE;
        return PLAYER_MWA_SPIN_ATTACK_1H;
    }
    if ((requested >= PLAYER_MWA_SPIN_ATTACK_1H) && (requested <= PLAYER_MWA_BIG_SPIN_2H))
        return requested;
    if ((requested >= PLAYER_MWA_FLIPSLASH_START) && (requested <= PLAYER_MWA_JUMPSLASH_FINISH))
        return requested;
    if ((requested & 1) != 0)
        return requested; // two-handed rows: not ours

    s32 row;
    if (sMhrRage.active) {
        if (sGerudoComboStep >= GMHR_RAGE_COMBO_STEPS)
            sGerudoComboStep = 0;
        row = sGerudoRageComboRows[sGerudoComboStep];
        sGerudoComboStep = (sGerudoComboStep + 1) % GMHR_RAGE_COMBO_STEPS;
    } else {
        if (sGerudoComboStep >= GMHR_COMBO_STEPS)
            sGerudoComboStep = 0;
        row = sGerudoComboRows[sGerudoComboStep];
        sGerudoComboStep = (sGerudoComboStep + 1) % GMHR_COMBO_STEPS;
    }
    sGerudoComboIdle = 0;
    return row;
}

u8 GerudoMhr_OwnsComboRow(Player* player) {
    return GerudoMhr_ForcesFighter(player);
}

static const GMhrMeleeBinding* MmForm_GerudoBindingForMwa(s32 mwa, s32* outIndex) {
    for (s32 i = 0; i < GMHR_MELEE_BINDING_COUNT; i++) {
        if (MmForm_GerudoMeleeTable()[i].mwa == mwa) {
            if (outIndex != NULL)
                *outIndex = i;
            return &MmForm_GerudoMeleeTable()[i];
        }
    }
    return NULL;
}

static u8 MmForm_GerudoWinHit(const GMhrWin* w, f32 prev, f32 cur) {
    for (s32 i = 0; i < 3; i++) {
        if (w[i].end < w[i].beg)
            continue;
        if ((cur >= (f32)w[i].beg) && (prev <= (f32)w[i].end))
            return 1;
    }
    return 0;
}

// The blade gate for the draw callbacks. Returns 1 when the trail should be fed;
// mask = which blades may damage this frame; ownFlags = write dmgFlags/damage
// (controller clip) vs keep OOT's (OOT swing).
u8 GerudoMhr_GetBladeGate(u8* mask, u8* ownFlags, u32* dmgFlags, u8* damage) {
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    if (mask != NULL) {
        // Demon mode is a single two-handed axe: bit 1 (the right blade) never arms, or
        // she would hit twice with a weapon she is not holding.
        *mask = MmForm_GerudoDemon() ? (u8)(sMhr.bladeMask & 1) : sMhr.bladeMask;
    }
    if (ownFlags != NULL)
        *ownFlags = sMhr.bladeOwnFlags;
    if (dmgFlags != NULL)
        *dmgFlags = sMhr.bladeDmgFlags;
    if (damage != NULL)
        *damage = sMhr.bladeDamage;
    return sMhr.trailOn;
}

// Which family the clip on screen right now belongs to, so the axe can be placed for it.
// Asked once per frame by the draw. The order matters: a controller clip wins because it
// has paused OOT and owns the body; then a swing, because its row names the clip OOT is
// playing; otherwise it is locomotion, which is great sword throughout except the sprint.
s32 GerudoMhr_AxeFamily(Player* player) {
    if (!MmForm_GerudoDemon())
        return GMHR_AXE_FAMILY_GS;
    if (sMhr.clipId != GMHR_CLIP_NONE)
        return MmForm_GerudoAxeFamilyOfPath(MmForm_GerudoClipTable()[sMhr.clipId].path);
    if (player != NULL) {
        s32 idx = -1;
        const GMhrMeleeBinding* b = MmForm_GerudoBindingForMwa(player->meleeWeaponAnimation, &idx);
        if ((b != NULL) && (player->meleeWeaponState != 0))
            return MmForm_GerudoAxeFamilyOfPath(b->path);
    }
    if (sGerudoSprinting)
        return MmForm_GerudoAxeFamilyOfPath(GMHR_DEMON_SPRINT_CLIP);
    return GMHR_AXE_FAMILY_GS;
}

// Per-frame gate for the OOT swing that is running (trail spawn/kill, per-hand
// window, spin cylinder).
static void MmForm_GerudoTickOotSwing(Player* player, PlayState* play) {
    u8 swinging = (player->actionFunc == Player_Action_808502D0) && (player->meleeWeaponState != 0);
    if (sMhr.state != GMHR_IDLE)
        swinging = 0; // a controller clip owns the blades
    // Touching down ends the jump slash's claim on the trail; the landing row is a
    // normal ground swing from here on.
    if (MMFORM_ON_GROUND(player))
        sGerudoJsTrail = 0;

    if (swinging && !sMhr.wasSwinging) {
        MmForm_GerudoSpawnSlashTrails(play);
        sMhr.trailKill = 0;
        sMhr.prevFrame = -1.0f;
        sMhr.cylOn = 0;
    }
    if (!swinging && sMhr.wasSwinging) {
        sMhr.trailKill = GMHR_TRAIL_KILL_DELAY;
        sMhr.cylOn = 0;
    }
    sMhr.wasSwinging = swinging;

    if (!swinging) {
        // ...unless the jump slash is airborne and owns them (it runs from inside
        // Player_Action_80844AF4, which is not one of OOT's ground swing actions).
        if ((sMhr.state == GMHR_IDLE) && !sGerudoJsTrail) {
            sMhr.bladeMask = 0;
            sMhr.trailOn = 0;
        }
        return;
    }

    s32 idx = -1;
    const GMhrMeleeBinding* b = MmForm_GerudoBindingForMwa(player->meleeWeaponAnimation, &idx);
    sMhr.trailOn = 1;
    // OOT sets the tier flags in func_80837948 for the blades; the axe is a HAMMER, so
    // demon mode claims the flags itself and enemies that only yield to a hammer react.
    sMhr.bladeOwnFlags = MmForm_GerudoDemon() ? 1 : 0;
    if (MmForm_GerudoDemon()) {
        sMhr.bladeDmgFlags = DMG_HAMMER_SWING;
        sMhr.bladeDamage = GMHR_DEMON_AXE_DAMAGE;
    }
    if (b == NULL) {
        sMhr.bladeMask = 3; // an unbound row: both blades, OOT's own window
        return;
    }
    // installed frame → source frame
    f32 cur = player->skelAnime.curFrame * sMhrTables.swingScale[idx];
    f32 prev = (sMhr.prevFrame < 0.0f) ? cur - 1.0f : sMhr.prevFrame;
    if (prev > cur)
        prev = cur;
    sMhr.prevFrame = cur;

    u8 useRage = sMhrRage.active && (b->ragePath != NULL);
    const GMhrWin* Lw = useRage ? b->rageL : b->L;
    const GMhrWin* Rw = useRage ? b->rageR : b->R;
    u8 mask = 0;
    if (MmForm_GerudoWinHit(Lw, prev, cur))
        mask |= 1;
    if (MmForm_GerudoWinHit(Rw, prev, cur))
        mask |= 2;

    if (b->spin) {
        // Spins hit with a body cylinder, not the blades.
        sMhr.bladeMask = 0;
        sMhr.cylOn = (mask != 0);
        sMhr.cylDmgFlags = sMhrRage.active ? DMG_SPIN_GIANT : DMG_SPIN_MASTER;
        sMhr.cylDamage = sMhrRage.active ? 4 : 2;
    } else {
        sMhr.bladeMask = mask;
        sMhr.cylOn = 0;
    }
    // The trail only while a blade (or the spin) is live -- not through the wind-up,
    // and never on the thrust.
    sMhr.trailOn =
        ((sMhr.bladeMask != 0) || sMhr.cylOn) && (b->mwa != PLAYER_MWA_STAB_1H) && (b->mwa != PLAYER_MWA_STAB_COMBO_1H);
}

// The hold-B charge keeps OOT's En_M_Thunder sparks and levels; only its release
// ring is suppressed (z_en_m_thunder.c). No trail while charging.
static void MmForm_GerudoTickCharge(Player* player, PlayState* play) {
    (void)play;
    if ((sMhr.state == GMHR_IDLE) && (player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) &&
        (player->meleeWeaponState == 0)) {
        sMhr.trailOn = 0;
    }
}

// ===========================================================================
// Section 7 — hit scan (public; called from Player_UpdateCommon BEFORE the AT reset)
// ===========================================================================
// Only a live enemy pays into the meter. AT_HIT fires on anything a quad touches — a
// tree, a sign, a pot — and cutting grass was filling the bar for free. base.at is the
// actor our collider landed on (the field is "what it collided with as an AT collider").
static u8 MmForm_GerudoRageWorthy(Actor* victim) {
    if (victim == NULL)
        return 0;
    return (victim->category == ACTORCAT_ENEMY) || (victim->category == ACTORCAT_BOSS);
}

void GerudoMhr_ScanBladeHits(Player* player) {
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return;
    // Fury spent the whole meter to fire; letting its own blast refill it would make the
    // charge free against any crowd.
    if (sMhr.state == GMHR_FURY)
        return;
    u8 hit = 0;
    for (s32 i = 0; i < 2; i++) {
        if ((player->meleeWeaponQuads[i].base.atFlags & AT_HIT) &&
            MmForm_GerudoRageWorthy(player->meleeWeaponQuads[i].base.at)) {
            hit = 1;
        }
    }
    if (sMhrCylInit && (sMhrCyl.base.atFlags & AT_HIT)) {
        if (MmForm_GerudoRageWorthy(sMhrCyl.base.at)) {
            hit = 1;
        }
        Player_RequestRumble(player, 120, 20, 100, 0);
    }
    if (sMhrCylInit && gPlayState != NULL) {
        Collider_ResetCylinderAT(gPlayState, &sMhrCyl.base);
    }
    if (hit && !sMhrRage.active) {
        sMhrRage.meter += GerudoMhr_RageGain();
        if (sMhrRage.meter > GerudoMhr_RageCapacity()) {
            sMhrRage.meter = GerudoMhr_RageCapacity();
        }
    }
}

// How much faster hold-B charges for her (func_80844E3C scales OOT's 0.02 by this).
f32 GerudoMhr_ChargeRateMul(Player* player) {
    if (player == NULL)
        return 1.0f;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 1.0f;
    return (Player_ActionToMeleeWeapon(player->heldItemAction) > 0) ? GMHR_CHARGE_RATE_MUL : 1.0f;
}

// 1 while the charge release belongs to her: En_M_Thunder turns its ring into the
// forward wedge instead of dying.
u8 GerudoMhr_UsesConeBurst(Player* player) {
    if (player == NULL)
        return 0;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    // Demon mode's release is a bolt that falls ON her, not a wedge thrown forward, so it
    // does not want the cone (GerudoMhr_DemonThunderStrike owns that one).
    if (MmForm_GerudoDemon())
        return 0;
    return Player_ActionToMeleeWeapon(player->heldItemAction) > 0;
}

// Urbosa's Fury VFX follows gs_wirebug_attack04. The implementation is kept in
// gerudo_form.cpp because it is presentation only; this controller merely feeds it the
// source-frame clock and guarantees an inactive tick after interruption/end.
static f32 MmForm_GerudoCurFrame(void); // defined with the other clip helpers, further down

void GerudoMhr_DemonThunderStrike(PlayState* play, Player* player) {
    if ((play == NULL) || (player == NULL))
        return;
    u8 active = (sMhr.state == GMHR_FURY) && (sMhr.clipId == GMHR_CLIP_FURY);
    GerudoForm_TickUrbosaFuryVfx(play, player, active ? MmForm_GerudoCurFrame() : 0.0f, active);
}

// The screen half of an L-attack: a shake and a dark wash. envCtx.fillScreen is a latch,
// so MmForm_GerudoLAttackTickDim has to shed it or the world stays dimmed for good.
static void MmForm_GerudoLAttackImpact(PlayState* play, Player* player, u8 dim) {
    Actor_RequestQuake(play, GMHR_LATK_QUAKE_STRENGTH, GMHR_LATK_QUAKE_FRAMES);
    Player_RequestRumble(player, 255, 20, 150, 0);
    play->envCtx.fillScreen = true;
    play->envCtx.screenFillColor[0] = 0;
    play->envCtx.screenFillColor[1] = 0;
    play->envCtx.screenFillColor[2] = 40;
    play->envCtx.screenFillColor[3] = dim;
}

static void MmForm_GerudoLAttackTickDim(PlayState* play) {
    if (!play->envCtx.fillScreen)
        return;
    if (play->envCtx.screenFillColor[3] <= GMHR_LATK_DIM_FADE) {
        play->envCtx.screenFillColor[3] = 0;
        play->envCtx.fillScreen = false;
        return;
    }
    play->envCtx.screenFillColor[3] -= GMHR_LATK_DIM_FADE;
}

// L+R rides the lock-on in: it faces the target and closes the gap every frame, so it
// cannot miss. 0 with no target, and the caller stands still instead.
static u8 MmForm_GerudoSpinHome(Player* player) {
    Actor* t = player->focusActor;
    if ((t == NULL) || (t->update == NULL))
        return 0;

    f32 dx = t->world.pos.x - player->actor.world.pos.x;
    f32 dz = t->world.pos.z - player->actor.world.pos.z;
    f32 dist = sqrtf(dx * dx + dz * dz) - t->colChkInfo.cylRadius;

    player->yaw = Math_Vec3f_Yaw(&player->actor.world.pos, &t->world.pos);
    player->actor.shape.rot.y = player->yaw;
    player->actor.world.rot.y = player->yaw;
    player->linearVelocity = (dist > GMHR_SPIN_HOME_SPEED) ? GMHR_SPIN_HOME_SPEED : ((dist > 0.0f) ? dist : 0.0f);
    return 1;
}

// The cone's own hits: it is a separate actor with its own collider, so it never goes
// through GerudoMhr_ScanBladeHits. The charge attack is the fast way to rage — it pays
// GMHR_RAGE_CHARGE_MUL times what a blade hit pays.
void GerudoMhr_AddChargeRage(Actor* victim) {
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return;
    if (sMhrRage.active)
        return;
    if (!MmForm_GerudoRageWorthy(victim))
        return;
    sMhrRage.meter += GerudoMhr_RageGain() * GMHR_RAGE_CHARGE_MUL;
    if (sMhrRage.meter > GerudoMhr_RageCapacity()) {
        sMhrRage.meter = GerudoMhr_RageCapacity();
    }
}

// ===========================================================================
// Section 8 — A: sprint / roll / sheathe; hops; fall
// ===========================================================================
f32 GerudoMhr_RunSpeedMul(void) {
    return sGerudoSprinting ? GMHR_SPRINT_MUL : 1.0f;
}

// The run cycle keeps its cadence while sprinting: func_8084029C scales the
// walk/run frame advance by this (OOT otherwise ties it to linearVelocity).
f32 GerudoMhr_RunAnimRateMul(void) {
    return sGerudoSprinting ? GMHR_SPRINT_ANIM_RATE : 1.0f;
}

// The run row of the locomotion table follows the sprint: the walk clip normally,
// the sprint clip while A is held. Live table write, no restore needed — the
// full restore on the way out of the form puts vanilla back either way.
static void MmForm_GerudoTickSprintRow(void) {
    static u8 sLastSprint = 0xFF;
    if (!sMhrTables.installed) {
        sLastSprint = 0xFF;
        return;
    }
    if (sGerudoSprinting == sLastSprint)
        return;
    sLastSprint = sGerudoSprinting;
    // GMHR_RUN_FRAMES, not 29: this is the RUN row, and OOT samples it at 20/29 of
    // the stride counter. Installing 29 here throws away the second step.
    LinkAnimationHeader* clip = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(
        MmForm_GerudoDemon() ? (sGerudoSprinting ? GMHR_DEMON_SPRINT_CLIP : GMHR_DEMON_WALK_CLIP)
                             : (sGerudoSprinting ? GMHR_SPRINT_CLIP : GMHR_WALK_CLIP),
        0, -1, -1, GMHR_RUN_FRAMES);
    if (clip == NULL)
        return;
    for (s32 col = 0; col < PLAYER_ANIMTYPE_MAX; col++) {
        if (GMHR_FIGHTER_COLUMNS_MASK & (1 << col))
            ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_run, col, clip);
    }
}

// Player_SetupRoll asks this first. OOT fires the roll on the PRESS; we need the
// release to tell a tap from a hold, so the press is swallowed and the controller
// fires the roll (or the rage roll, or the sheathe) itself.
u8 GerudoMhr_SuppressRoll(Player* player) {
    if (!GerudoMhr_ForcesFighter(player))
        return 0;
    // Demon swings commit: no rolling out of one. The roll is the only cancel OOT offers
    // mid-swing, so refusing it here is what makes the axe feel heavy.
    if (MmForm_GerudoDemon() && (player->meleeWeaponState != 0))
        return 1;
    if (sMhr.aPending || sMhr.aSprint)
        return 1;
    // Frame one: the controller runs AFTER Player_UpdateCommon, so on the very frame
    // A is pressed the latch is not set yet — read the button itself.
    if (gPlayState == NULL)
        return 0;
    Input* in = &gPlayState->state.input[0];
    return CHECK_BTN_ALL(in->press.button, BTN_A) ? 1 : 0;
}

// Player_ActionHandler_Roll's "A while standing = put the sword away" branch:
// Gerudo sheathes through her own clip, so vanilla must not.
u8 GerudoMhr_OwnsPutaway(Player* player) {
    return GerudoMhr_ForcesFighter(player);
}

f32 GerudoMhr_HopSpeedMul(void) {
    return GerudoMhr_RageActive() ? GMHR_RAGE_HOP_MUL : 1.0f;
}

u8 GerudoMhr_WantsLongRoll(void) {
    return gFormState.currentForm == MM_PLAYER_FORM_GERUDO;
}

LinkAnimationHeader* GerudoMhr_GetRollAnim(void) {
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return NULL;
    if (MmForm_GerudoDemon()) {
        return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(GMHR_DEMON_ROLL_CLIP, 0, 0, GMHR_DEMON_ROLL_END, 0);
    }
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(GMHR_ROLL_CLIP, 0, 0, GMHR_ROLL_END, 0);
}

// Two-rate clip: [0, split-1] at mulA, [split, last] at mulB, glued into one
// header. OOT plays hops at 2/3, so both rates are compensated to come out real.
// The result lives in a static buffer per call site (one backflip = one buffer).
static LinkAnimationHeader* MmForm_GerudoTwoRateClip(const char* path, s16 split, f32 mulA, f32 mulB,
                                                     LinkAnimationHeader* cache, s16** cacheData) {
    if (cache->segment != NULL)
        return cache;
    LinkAnimationHeader* raw = MmForm_MhrLoadPath(path);
    if (raw == NULL)
        return NULL;
    s16 last = raw->common.frameCount - 1;
    if (split > last)
        split = last;
    s16 framesA = (s16)(((f32)split / (mulA * GMHR_OOT_SWING_SPEED_COMP)) + 0.5f);
    s16 framesB = (s16)(((f32)(last - split + 1) / (mulB * GMHR_OOT_SWING_SPEED_COMP)) + 0.5f);
    if (framesA < 1)
        framesA = 1;
    if (framesB < 1)
        framesB = 1;
    LinkAnimationHeader* a = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(path, 0, 0, split - 1, framesA);
    LinkAnimationHeader* b = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(path, 0, split, last, framesB);
    if (a == NULL || b == NULL)
        return NULL;
    s32 total = a->common.frameCount + b->common.frameCount;
    s16* buf = (s16*)malloc(sizeof(s16) * GMHR_ANIM_S16_PER_FRAME * total);
    if (buf == NULL)
        return NULL;
    memcpy(buf, a->segment, sizeof(s16) * GMHR_ANIM_S16_PER_FRAME * a->common.frameCount);
    memcpy(buf + GMHR_ANIM_S16_PER_FRAME * a->common.frameCount, b->segment,
           sizeof(s16) * GMHR_ANIM_S16_PER_FRAME * b->common.frameCount);
    *cacheData = buf;
    cache->common.frameCount = (s16)total;
    cache->segment = buf;
    return cache;
}

// Sidehop / backflip clip. The side hops get fixed short lengths (Link's own hop
// clips are 7 frames and OOT holds their last pose until touchdown); the backflip
// is the two-rate build the user asked for: 1.5x, then 2x from frame 31.
LinkAnimationHeader* GerudoMhr_GetHopAnim(s32 dir) {
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return NULL;
    if ((dir == EXTPLAYER_JUMP_BACKFLIP) && !sMhrRage.active) {
        static LinkAnimationHeader sBackflip = { { 0 }, NULL };
        static s16* sBackflipData = NULL;
        return MmForm_GerudoTwoRateClip(MHRP("BackwardHighAerialLeftLeadDoubleSilkbindSlash"), 31, 1.5f, 2.0f,
                                        &sBackflip, &sBackflipData);
    }
    for (s32 i = 0; i < GMHR_JUMP_BINDING_COUNT; i++) {
        const GMhrJumpBinding* b = &MmForm_GerudoJumpTable()[i];
        if (b->dir != dir)
            continue;
        u8 rage = sMhrRage.active && (b->ragePath != NULL);
        // Link's own hop clips are 7 (side) / 15 (back) frames and OOT plays them at
        // 2/3, then holds the last pose until touchdown. Squeezing a 32-frame slash
        // into 7 is a blur, so the hops get fixed short lengths of their own instead.
        s16 frames = (dir == EXTPLAYER_JUMP_BACKFLIP) ? GMHR_HOP_BACK_FRAMES : GMHR_HOP_SIDE_FRAMES;
        return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(rage ? b->ragePath : b->path, 0, -1,
                                                              rage ? b->rageEnd : -1, frames);
    }
    return NULL;
}

LinkAnimationHeader* GerudoMhr_GetFallAnim(Player* player) {
    if (!GerudoMhr_ForcesFighter(player))
        return NULL;
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlace(MmForm_GerudoPick(GMHR_FALL_CLIP, GMHR_DEMON_FALL_CLIP), 0);
}

// ---- the jump slash: a flying lunge at the target --------------------------
// OOT's jump slash is a short hop with the sword out. Hers is a BIG one: she leaves
// the ground hard and flies at the lock-on, blades lit, all the way in. Nothing here
// ever stops her in mid-air — that reads as standing still, not as an attack.
//   launch  PLAYER_MWA_JUMPSLASH_START's row (ForwardRisingDoubleAerialSlash_Variant18)
//           played once, high and aimed at the target
//   flight  the same clip's repeating frames (6-9) on loop, homing every frame
//   landing PLAYER_MWA_JUMPSLASH_FINISH's row = her ordinary fighter landing clip
//
// The loop range is measured, not guessed: in Variant18, frame 8 is frame 6 again and
// frame 9 is frame 7 again (pose distance 42.9k / 53.6k of s16 angle over 64 channels,
// against 80k+ for every other pair), so 6..9 is exactly two turns of the cycle and
// wrapping 9 -> 6 continues it without a step.
//
// Player_Action_80844AF4 re-stamps gravity = -1.2 at its top every frame and runs its
// air control (func_8083DFE0) after that, so the tick below is called from INSIDE that
// action, after both — anything written earlier would be overwritten the same frame.
#define GMHR_JS_VY_MUL 2.1f     // launch height (h ~ vy^2)
#define GMHR_JS_FLIGHT_MIN 6.0f // she never drifts: this is the slowest she ever flies
#define GMHR_JS_FLIGHT_MAX 20.0f
#define GMHR_JS_FLIGHT_LEAD 6.0f // aim to close the remaining ground in this many frames
#define GMHR_JS_LOOP_CLIP MHRP("ForwardRisingDoubleAerialSlash_Variant18")
#define GMHR_JS_LOOP_FIRST 6
#define GMHR_JS_LOOP_LAST 9

void GerudoMhr_AdjustJumpSlash(Player* player, s32 mwa) {
    if (player == NULL)
        return;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return;
    if ((mwa != PLAYER_MWA_JUMPSLASH_START) && (mwa != PLAYER_MWA_FLIPSLASH_START))
        return;

    player->actor.velocity.y *= GMHR_JS_VY_MUL;
    sGerudoJsPhase = 0;
    sGerudoJsTrail = 0;
}

// The lock-on, if it is still a live actor.
static Actor* MmForm_GerudoJumpTarget(Player* player) {
    Actor* t = player->focusActor;
    return ((t != NULL) && (t->update != NULL)) ? t : NULL;
}

// Fly at the lock-on: face it and cover the ground that is left. With no lock-on she
// keeps whatever heading and speed OOT gave her, which is the vanilla arc.
static void MmForm_GerudoJumpHome(Player* player, Actor* target) {
    if (target == NULL)
        return;
    f32 dx = target->world.pos.x - player->actor.world.pos.x;
    f32 dz = target->world.pos.z - player->actor.world.pos.z;
    f32 dist = sqrtf(dx * dx + dz * dz) - target->colChkInfo.cylRadius;
    if (dist < 0.0f)
        dist = 0.0f;

    player->yaw = Math_Vec3f_Yaw(&player->actor.world.pos, &target->world.pos);
    player->actor.shape.rot.y = player->yaw;
    player->actor.world.rot.y = player->yaw;

    f32 spd = dist / GMHR_JS_FLIGHT_LEAD;
    if (spd < GMHR_JS_FLIGHT_MIN)
        spd = GMHR_JS_FLIGHT_MIN;
    if (spd > GMHR_JS_FLIGHT_MAX)
        spd = GMHR_JS_FLIGHT_MAX;
    if (spd > dist)
        spd = dist; // never overshoot her own target
    player->linearVelocity = spd;
}

void GerudoMhr_TickJumpSlash(Player* player, PlayState* play) {
    if ((player == NULL) || (play == NULL))
        return;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return;
    if ((player->meleeWeaponAnimation != PLAYER_MWA_JUMPSLASH_START) &&
        (player->meleeWeaponAnimation != PLAYER_MWA_FLIPSLASH_START)) {
        return;
    }

    // Once the launch clip has run out, hold its own flight cycle instead of its
    // last pose, so the flight has motion in it for as long as it lasts.
    if ((sGerudoJsPhase == 0) && (player->skelAnime.curFrame >= player->skelAnime.endFrame)) {
        sGerudoJsPhase = 1;
        LinkAnimationHeader* loop = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(
            GMHR_JS_LOOP_CLIP, 0, GMHR_JS_LOOP_FIRST, GMHR_JS_LOOP_LAST, 0);
        if (loop != NULL) {
            LinkAnimation_Change(play, &player->skelAnime, loop, 1.0f, 0.0f, Animation_GetLastFrame(loop),
                                 ANIMMODE_LOOP, -3.0f);
        }
    }

    // Blades lit for the whole flight, and steering onto the target every frame.
    if (!sGerudoJsTrail) {
        sGerudoJsTrail = 1;
        MmForm_GerudoSpawnSlashTrails(play);
        Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_SWING);
        Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
    }
    sMhr.trailOn = 1;
    sMhr.bladeMask = 3;
    sMhr.bladeOwnFlags = 0; // OOT set the tier flags in func_80837948
    MmForm_GerudoJumpHome(player, MmForm_GerudoJumpTarget(player));
}

// ===========================================================================
// Section 9 — controller clip primitives
// ===========================================================================
// Is the controller driving a clip right now (MmForm_UsesOotAnim asks)?
u8 GerudoMhr_DrivingClip(void) {
    return (gFormState.currentForm == MM_PLAYER_FORM_GERUDO) && (sMhr.state != GMHR_IDLE);
}

static void MmForm_GerudoCylOff(void) {
    sMhr.cylOn = 0;
}

static void MmForm_GerudoCylOn(u32 dmgFlags, u8 damage) {
    sMhr.cylOn = 1;
    sMhr.cylRadius = 0;
    sMhr.cylHeight = 0;
    sMhr.cylDmgFlags = dmgFlags;
    sMhr.cylDamage = damage;
}

static void MmForm_GerudoBladesOff(Player* player) {
    sMhr.bladeMask = 0;
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
    player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
}

static void MmForm_GerudoStartClip(PlayState* play, Player* player, GMhrClipId id, u8 reversed) {
    if (id < 0 || id >= GMHR_CLIP_MAX)
        return;
    const GMhrClip* c = &MmForm_GerudoClipTable()[id];
    LinkAnimationHeader* anim = MmForm_MhrLoadPath(c->path);
    if (anim == NULL)
        return;
    f32 last = Animation_GetLastFrame(anim);
    f32 start = reversed ? last : 0.0f;
    f32 end = reversed ? 0.0f : last;
    f32 speed = MmForm_GerudoDemon() ? (c->speed * GMHR_DEMON_SPEED_SCALE) : c->speed;
    speed = reversed ? -speed : speed;

    // Pause OOT's actionFunc; the clip goes on BOTH tracks (the gerudo body draws
    // from formSkelAnime, the hand matrices for trail/quads come from player->skelAnime).
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    gFormState.goronAction = GORON_ACT_PUNCH_A;
    gFormState.actionTimer = 0;
    LinkAnimation_Change(play, &gFormState.formSkelAnime, anim, speed, start, end,
                         c->loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -6.0f);
    LinkAnimation_Change(play, &player->skelAnime, anim, speed, start, end, c->loop ? ANIMMODE_LOOP : ANIMMODE_ONCE,
                         -2.0f);

    sMhr.clipId = id;
    sMhr.timer = 0;
    sMhr.rootInit = 0;
    sMhr.prevFrame = reversed ? (last + 1.0f) : -1.0f;
    sMhr.bladeOwnFlags = 1;
    u8 isLAttack = (id == GMHR_CLIP_FURY) || (id == GMHR_CLIP_LUNGE) || (id == GMHR_CLIP_SPIN);
    // Urbosa's Fury is a spell as much as a swing: it also registers as magic, so the
    // things that only a spell can hurt go down to it.
    u32 latkFlags = (id == GMHR_CLIP_FURY) ? (GMHR_LATK_DMGFLAGS | GMHR_FURY_MAGIC_FLAGS) : GMHR_LATK_DMGFLAGS;
    sMhr.bladeDmgFlags = isLAttack ? latkFlags : (sMhrRage.active ? DMG_SLASH_GIANT : DMG_SLASH_MASTER);
    sMhr.bladeDamage = isLAttack ? GMHR_LATK_DAMAGE : (sMhrRage.active ? 4 : 2);
    sMhr.bladeMask = 0;
    sMhr.cylOn = 0;
    sMhr.trailOn = 0;
}

// Plant: no motion, facing locked. Called every frame of a planted clip because
// Player_UpdateCommon re-derives world.rot.y from player->yaw and clears PAUSE.
static void MmForm_GerudoPlant(Player* player) {
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    player->actor.shape.rot.y = sGerudoComboLockedYaw;
    player->actor.world.rot.y = sGerudoComboLockedYaw;
    player->yaw = sGerudoComboLockedYaw;
    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;
}

// Keep PAUSE while letting OOT integrate the linearVelocity we set (air / dashes).
static void MmForm_GerudoHold(Player* player) {
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    player->actor.world.rot.y = player->yaw;
}

// Freeze the visual root of both tracks so a paused clip never slides the body
// off the actor (the clips are root-frozen in the archive; this guards the rest).
static void MmForm_GerudoPinRoot(Player* player) {
    Vec3s* root = &player->skelAnime.jointTable[0];
    if (!sMhr.rootInit) {
        sMhr.rootInit = 1;
        sMhr.baseRootX = root->x;
        sMhr.baseRootY = root->y;
        sMhr.baseRootZ = root->z;
    }
    root->x = sMhr.baseRootX;
    root->z = sMhr.baseRootZ;
    if (gFormState.formSkelAnime.jointTable != NULL) {
        gFormState.formSkelAnime.jointTable[0].x = sMhr.baseRootX;
        gFormState.formSkelAnime.jointTable[0].z = sMhr.baseRootZ;
    }
}

// Advance both tracks; 1 when the player track finished (ONCE clips).
static s32 MmForm_GerudoAdvance(PlayState* play, Player* player) {
    LinkAnimation_Update(play, &gFormState.formSkelAnime);
    s32 done = LinkAnimation_Update(play, &player->skelAnime);
    MmForm_GerudoPinRoot(player);
    return done;
}

static f32 MmForm_GerudoCurFrame(void) {
    return gFormState.formSkelAnime.curFrame;
}

// Per-hand blade gate for the current controller clip (source frames == clip frames).
static void MmForm_GerudoTickClipBlades(void) {
    if (sMhr.clipId < 0 || sMhr.clipId >= GMHR_CLIP_MAX) {
        sMhr.bladeMask = 0;
        sMhr.trailOn = 0;
        return;
    }
    const GMhrClip* c = &MmForm_GerudoClipTable()[sMhr.clipId];
    f32 cur = MmForm_GerudoCurFrame();
    f32 prev = sMhr.prevFrame;
    if (prev < 0.0f || prev > cur)
        prev = cur;
    sMhr.prevFrame = cur;
    u8 mask = 0;
    if (MmForm_GerudoWinHit(c->L, prev, cur))
        mask |= 1;
    if (MmForm_GerudoWinHit(c->R, prev, cur))
        mask |= 2;
    sMhr.bladeMask = mask;
    sMhr.trailOn = (mask != 0) || sMhr.cylOn;
}

// Register the body cylinder for this frame (called every frame; no-op when off).
static void MmForm_GerudoSubmitCyl(PlayState* play, Player* player) {
    if (!sMhrCylInit) {
        Collider_InitCylinder(play, &sMhrCyl);
        Collider_SetCylinder(play, &sMhrCyl, &player->actor, &sMhrCylInitData);
        sMhrCylInit = 1;
    }
    if (!sMhr.cylOn)
        return;
    sMhrCyl.info.toucher.dmgFlags = sMhr.cylDmgFlags;
    sMhrCyl.info.toucher.damage = sMhr.cylDamage;
    sMhrCyl.dim.radius = (sMhr.cylRadius > 0) ? sMhr.cylRadius : GMHR_SPIN_CYL_RADIUS;
    sMhrCyl.dim.height = (sMhr.cylHeight > 0) ? sMhr.cylHeight : GMHR_SPIN_CYL_HEIGHT;
    // Centred on her, so a tall blast reaches what is above and below, not just level.
    sMhrCyl.dim.yShift = (s16)(-sMhrCyl.dim.height / 2);
    sMhrCyl.base.atFlags |= AT_ON;
    sMhrCyl.dim.pos.x = (s16)player->actor.world.pos.x;
    sMhrCyl.dim.pos.y = (s16)player->actor.world.pos.y;
    sMhrCyl.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sMhrCyl.base);
}

// A wall-checked forward step in world units (used by the front slash / rage roll).
static void MmForm_GerudoStepForward(PlayState* play, Player* player, f32 dist) {
    if (dist <= 0.0f)
        return;
    f32 sn = Math_SinS(player->actor.shape.rot.y);
    f32 cs = Math_CosS(player->actor.shape.rot.y);
    Vec3f from = player->actor.world.pos;
    from.y += 20.0f;
    Vec3f to;
    to.x = from.x + sn * (dist + GMHR_WALL_MARGIN);
    to.y = from.y;
    to.z = from.z + cs * (dist + GMHR_WALL_MARGIN);
    Vec3f hitPos;
    CollisionPoly* poly = NULL;
    s32 bgId;
    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hitPos, &poly, true, false, false, true, &bgId)) {
        return;
    }
    player->actor.world.pos.x += sn * dist;
    player->actor.world.pos.z += cs * dist;
    if (player->actor.floorHeight > -30000.0f && MMFORM_ON_GROUND(player)) {
        player->actor.world.pos.y = player->actor.floorHeight;
    }
}

// Hand the frame back to OOT. On the ground OOT is put back into its idle action so a
// clip started out of a swing or a run never resumes that action from mid-animation;
// in the air OOT's own fall/jump action simply continues.
static void MmForm_GerudoEndClip(PlayState* play, Player* player) {
    sMhr.state = GMHR_IDLE;
    sMhr.clipId = GMHR_CLIP_NONE;
    sMhr.timer = 0;
    sMhr.swordsVisible = 0xFF;
    sMhr.frontTarget = NULL;
    MmForm_GerudoBladesOff(player);
    MmForm_GerudoCylOff();
    sMhr.trailOn = 0;
    sMhr.trailKill = GMHR_TRAIL_KILL_DELAY;
    player->actor.gravity = -1.2f;
    player->actor.minVelocityY = -20.0f;
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
    MmForm_SetAction(GORON_ACT_IDLE, play, gFormState.idleAnim, 1.0f, ANIMMODE_LOOP);
    if (MMFORM_ON_GROUND(player)) {
        func_80839F90(player, play);
    }
}

// Deferred trail kill, so the last vertices are drawn.
static void MmForm_GerudoTickTrail(PlayState* play) {
    if (sMhr.trailKill > 0) {
        if (--sMhr.trailKill == 0) {
            MmForm_KillTrail(play, &gFormState.punchTrailEffectIndex, &gFormState.punchTrailActive);
            MmForm_KillTrail(play, &gFormState.punchTrailEffectIndexR, &gFormState.punchTrailActiveR);
        }
    }
}

// ===========================================================================
// Section 10 — draw / sheathe (public entry: Player_UseItem asks)
// ===========================================================================
static u8 MmForm_GerudoCanAct(Player* player) {
    if (player == NULL)
        return 0;
    if (sMhr.state != GMHR_IDLE)
        return 0;
    const u32 block = PLAYER_STATE1_TALKING | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE |
                      PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_DAMAGED |
                      PLAYER_STATE1_DEAD | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
                      PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_LOADING | PLAYER_STATE1_ON_HORSE |
                      PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON;
    if (player->stateFlags1 & block)
        return 0;
    if (player->stateFlags2 & PLAYER_STATE2_GRABBING_DYNAPOLY)
        return 0;
    if (player->csAction != 0)
        return 0;
    return 1;
}

// Returns 1 when Gerudo takes the item change herself: B in free with a sword on B
// (draw), A standing in fighter (sheathe, arrives here as ITEM_NONE).
u8 GerudoMhr_InterceptUseItem(PlayState* play, Player* player, s32 item) {
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    if (!MmForm_GerudoCanAct(player))
        return 0;
    if (!MMFORM_ON_GROUND(player))
        return 0;
    if (player->stateFlags1 & PLAYER_STATE1_SHIELDING)
        return 0;

    s8 ia = Player_ItemToItemAction(item);
    u8 holdsSword = Player_ActionToMeleeWeapon(player->heldItemAction) > 0;

    if ((item == ITEM_NONE) && holdsSword) {
        sGerudoComboLockedYaw = player->actor.shape.rot.y;
        MmForm_GerudoStartClip(play, player, GMHR_CLIP_SHEATHE, 0);
        sMhr.state = GMHR_SHEATHE;
        sMhr.pendingItem = ITEM_NONE;
        sMhr.pendingIA = PLAYER_IA_NONE;
        sMhr.swordsVisible = 1;
        Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_PUTAWAY);
        return 1;
    }
    if (!holdsSword && (Player_ActionToMeleeWeapon(ia) > 0)) {
        // Drawing WHILE RUNNING is handed back to OOT on purpose: its item change is
        // an UPPER-BODY animation, so the legs keep running through it. Taking the
        // whole body here is what nailed her to the floor mid-stride.
        if (fabsf(player->linearVelocity) > 4.0f)
            return 0;
        // Standing: the sheathe clip run BACKWARDS, so the blades travel back into
        // the hands. They stay hidden until the clip crosses the swap frame.
        sGerudoComboLockedYaw = player->actor.shape.rot.y;
        MmForm_GerudoStartClip(play, player, GMHR_CLIP_DRAW_STAND, 1);
        sMhr.state = GMHR_DRAW;
        sMhr.pendingItem = item;
        sMhr.pendingIA = ia;
        sMhr.swordsVisible = 0;
        return 1;
    }
    return 0;
}

// The clip for OOT's own item change (Player_StartChangingHeldItem): the running
// draw. Vanilla plays it on upperSkelAnime only — she keeps running, only the arms
// pull the blades — and its own change frame still swaps the item, so nothing here
// has to track state. Flipped negative so the clip runs backwards, the way the
// standing draw does. Everything that is not "empty hands -> blades" keeps Link's.
LinkAnimationHeader* GerudoMhr_GetItemChangeAnim(Player* player, s8 newIA, s32* itemChangeType) {
    if ((player == NULL) || (itemChangeType == NULL))
        return NULL;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return NULL;
    if (Player_ActionToMeleeWeapon(player->heldItemAction) > 0)
        return NULL;
    if (Player_ActionToMeleeWeapon(newIA) <= 0)
        return NULL;
    LinkAnimationHeader* anim = MmForm_MhrLoadPath(MmForm_GerudoClipTable()[GMHR_CLIP_DRAW_RUN].path);
    if (anim == NULL)
        return NULL;
    if (*itemChangeType > 0)
        *itemChangeType = -*itemChangeType;
    return anim;
}

// The hand-over, done the way OOT does it: heldItemId, heldItemAction and
// itemAction together, then the model group. Writing heldItemAction alone is what
// causes the equip/unequip loop.
static void MmForm_GerudoCommitItem(Player* player) {
    player->heldItemId = sMhr.pendingItem;
    player->heldItemAction = sMhr.pendingIA;
    func_8008EC70(player);
    sGerudoLastFighter = 0xFF;
}

// ===========================================================================
// Section 11 — the controller
// ===========================================================================
static void MmForm_GerudoTickA(Player* player, PlayState* play, Input* in, u8 onGround) {
    // A roll is OOT's action, so sMhr.state is still IDLE while it runs: without the roll
    // test the A latch re-armed mid-tumble and every press started the roll over itself.
    u8 enabled = GerudoMhr_ForcesFighter(player) && (sMhr.state == GMHR_IDLE) && !MmForm_GerudoIsRolling(player) &&
                 !(player->stateFlags1 & PLAYER_STATE1_SHIELDING);

    if (!enabled || !onGround) {
        sMhr.aPending = 0;
        sMhr.aSprint = 0;
        sMhr.aFrames = 0;
        sGerudoSprinting = 0;
        return;
    }

    if (CHECK_BTN_ALL(in->press.button, BTN_A)) {
        sMhr.aPending = 1;
        sMhr.aFrames = 0;
    }

    if (CHECK_BTN_ALL(in->cur.button, BTN_A)) {
        if (sMhr.aPending && (++sMhr.aFrames >= GMHR_A_TAP_FRAMES)) {
            sMhr.aPending = 0;
            sMhr.aSprint = 1;
        }
    } else {
        if (sMhr.aPending) {
            sMhr.aPending = 0;
            sMhr.aFrames = 0;
            u8 moving = TransformMasks_GetStickMagnitude() >= 10.0f;
            if (MmForm_GerudoCanAct(player)) {
                if (!moving) {
                    // Standing tap: sheathe.
                    if (Player_ActionToMeleeWeapon(player->heldItemAction) > 0) {
                        GerudoMhr_InterceptUseItem(play, player, ITEM_NONE);
                    }
                } else if (sMhrRage.active) {
                    // Rage roll: quad L→R 1-20, then the roll's travel 20-40, then the
                    // body cylinder 41-75. x1.3.
                    sGerudoComboLockedYaw = player->actor.shape.rot.y;
                    MmForm_GerudoStartClip(play, player, GMHR_CLIP_RAGE_ROLL, 0);
                    sMhr.state = GMHR_RAGE_ROLL;
                    MmForm_GerudoSpawnSlashTrails(play);
                    sMhr.trailOn = 1;
                    Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_SWING);
                } else {
                    Player_SetupRoll(player, play);
                }
            }
        }
        sMhr.aSprint = 0;
    }
    sGerudoSprinting = sMhr.aSprint;
}

static void MmForm_GerudoTickRage(Player* player) {
    // Nothing drains: the meter is a currency the L-attacks pay up front, not a fuel.
    // The testing switch REFILLS it rather than waiving the price, so the bar always
    // shows what an attack really costs.
    (void)player;
    if (MmForm_GerudoForceDemon()) {
        sMhrRage.meter = GerudoMhr_RageCapacity();
    }
}

// The guard collider. OOT's shieldQuad is only stamped by Link's own right-hand
// draw, which never runs for a form — so while SHIELDING it is stamped here.
static void MmForm_GerudoTickGuard(Player* player, PlayState* play) {
    if (player->stateFlags1 & PLAYER_STATE1_SHIELDING) {
        if (sMhr.shieldFrames < 1000)
            sMhr.shieldFrames++;
        Collider_ResetQuadAC(play, &player->shieldQuad.base);
        MmForm_ActivateFormShieldQuad(player, play);
    } else {
        sMhr.shieldFrames = 0;
    }
}

// Rage parry: a hit caught in the first frames of the guard, in rage. Called from
// OOT's damage path (func_808382DC) ahead of the knockback branch. Returns 1 when
// the hit is eaten.
u8 GerudoMhr_TryParry(PlayState* play, Player* player) {
    if (play == NULL || player == NULL)
        return 0;
    if (gFormState.currentForm != MM_PLAYER_FORM_GERUDO)
        return 0;
    if (!sMhrRage.active)
        return 0;
    if (!(player->stateFlags1 & PLAYER_STATE1_SHIELDING))
        return 0;
    if (sMhr.shieldFrames > GMHR_PARRY_WINDOW)
        return 0;
    if (sMhr.state != GMHR_IDLE)
        return 0;

    Vec3f sparkPos = player->actor.world.pos;
    sparkPos.y += 30.0f;
    CollisionCheck_SpawnShieldParticlesMetal(play, &sparkPos);
    Player_RequestRumble(player, 180, 20, 100, 0);
    player->stateFlags1 &= ~PLAYER_STATE1_SHIELDING;

    sGerudoComboLockedYaw = player->actor.shape.rot.y;
    MmForm_GerudoStartClip(play, player, GMHR_CLIP_RAGE_PARRY, 0);
    sMhr.state = GMHR_RAGE_PARRY;
    MmForm_GerudoSpawnSlashTrails(play);
    sMhr.trailOn = 1;
    // Stun: the first frames hit as a Deku Nut (no damage, stuns what reacts to it).
    MmForm_GerudoCylOn(DMG_DEKU_NUT, 0);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_L);
    return 1;
}

// Abort whatever clip is running and drop the input latches. Rage and its meter
// SURVIVE this: it is called on every OOT yield (a hit taken mid-move), and a hit
// must not cost the player the rage he paid a full meter for.
static void MmForm_GerudoReset(void) {
    u8 inited = sMhr.inited;
    memset(&sMhr, 0, sizeof(sMhr));
    sMhr.inited = inited;
    sMhr.state = GMHR_IDLE;
    sMhr.clipId = GMHR_CLIP_NONE;
    sMhr.swordsVisible = 0xFF;
    sMhr.prevFrame = -1.0f;
    sGerudoSprinting = 0;
    sGerudoComboStep = 0;
    sGerudoComboIdle = 0;
    sGerudoLastFighter = 0xFF;
    sGerudoJsPhase = 0;
    sGerudoJsTrail = 0;
    if (gPlayState != NULL) {
        MmForm_KillTrail(gPlayState, &gFormState.punchTrailEffectIndex, &gFormState.punchTrailActive);
        MmForm_KillTrail(gPlayState, &gFormState.punchTrailEffectIndexR, &gFormState.punchTrailActiveR);
    }
}

static void MmForm_GerudoMhrReset(void) {
    MmForm_GerudoReset();
}

// Full reset — leaving the form. Rage cannot survive: the tables are restored on
// the way out, so a live flag would install the rage moveset on the next transform
// without the meter having been paid.
static void MmForm_GerudoFormExit(void) {
    MmForm_GerudoReset();
    sMhr.inited = 0;
    sMhrRage.active = 0;
    sMhrRage.timer = 0;
    sMhrRage.meter = 0;
}

// DIAGNOSTIC (2026-08-19): "cada vez que cambio de action se rota 15 grados" — not
// cumulative, and it does not spring back, so it is neither a stray += nor a pose
// mismatch between her clips and Link's. Something is writing a one-off turn into the
// actor. This runs at the END of the frame (the controller is called from
// TransformMasks_Update, after all of Player_UpdateCommon) and prints any jump in
// shape.rot.y that the stick cannot account for, with the state that produced it.
// 15 deg = 2731 binang; the gate is set well below that so nothing is missed.
#define GMHR_YAW_WATCH_MIN 1400 // ~7.7 deg in one frame
static void MmForm_GerudoWatchYaw(Player* player) {
    static s16 sPrevShape = 0;
    static s16 sPrevYaw = 0;
    static u8 sPrimed = 0;

    s16 shape = player->actor.shape.rot.y;
    s16 yaw = player->yaw;

    if (sPrimed) {
        s16 dShape = shape - sPrevShape;
        s16 dYaw = yaw - sPrevYaw;
        f32 stick = TransformMasks_GetStickMagnitude();
        // A real turn from the stick is expected; only report jumps with no input behind
        // them, which is exactly the case he is describing (he is charging or guarding).
        if ((ABS(dShape) >= GMHR_YAW_WATCH_MIN) && (stick < 10.0f)) {
            SPDLOG_WARN("[GerudoYaw] shape {} -> {} (d {} = {:.1f} deg) | yaw d {} | stick {:.1f} | "
                        "mwa {} state {} | sf1 {:#x} sf2 {:#x} sf3 {:#x} | upperRot.y {} | locked {}",
                        sPrevShape, shape, dShape, dShape * (360.0f / 65536.0f), dYaw, stick,
                        player->meleeWeaponAnimation, player->meleeWeaponState, player->stateFlags1,
                        player->stateFlags2, player->stateFlags3, player->upperLimbRot.y, sGerudoComboLockedYaw);
        }
    }
    sPrevShape = shape;
    sPrevYaw = yaw;
    sPrimed = 1;
}

// Main entry — top of MmForm_UpdateActive's dispatch. 1 = this frame is ours.
static u8 MmForm_GerudoMhrUpdate(Player* player, PlayState* play) {
    if (player == NULL || play == NULL)
        return 0;
    if (!sMhr.inited) {
        MmForm_GerudoReset();
        sMhr.inited = 1;
    }

    Input* in = &play->state.input[0];
    u8 bPress = CHECK_BTN_ALL(in->press.button, BTN_B) != 0;
    u8 aPress = CHECK_BTN_ALL(in->press.button, BTN_A) != 0;
    u8 rHeld = CHECK_BTN_ALL(in->cur.button, BTN_R) != 0;
    u8 rPress = CHECK_BTN_ALL(in->press.button, BTN_R) != 0;
    u8 onGround = MMFORM_ON_GROUND(player) != 0;
    u8 fighter = GerudoMhr_ForcesFighter(player);

    if (onGround)
        sMhr.airFrames = 0;
    else if (sMhr.airFrames < 1000)
        sMhr.airFrames++;

    MmForm_GerudoTickFighter(player);
    MmForm_GerudoTickRage(player);
    MmForm_GerudoTickGuard(player, play);
    MmForm_GerudoTickTrail(play);
    GerudoMhr_TickCombo(player);
    MmForm_GerudoTickA(player, play, in, onGround);
    MmForm_GerudoTickSprintRow();
    MmForm_GerudoTickOotSwing(player, play);
    MmForm_GerudoTickCharge(player, play);
    MmForm_GerudoSubmitCyl(play, player);
    MmForm_GerudoWatchYaw(player);

    sMhr.timer++;

    // Always tick the presentation bridge, including outside Fury. The inactive call is
    // what restores scene lighting if the move ends or is interrupted between frames.
    GerudoMhr_DemonThunderStrike(play, player);

    // ============================ ACTIVE STATES ============================
    switch (sMhr.state) {
        case GMHR_IDLE:
            break;

        case GMHR_RAGE_ENTER:
            MmForm_GerudoPlant(player);
            if (player->invincibilityTimer > -10)
                player->invincibilityTimer = -10;
            if (MmForm_GerudoAdvance(play, player))
                MmForm_GerudoEndClip(play, player);
            return 1;

        // The three L-attacks. All planted: the axe commits, nothing cancels it. The axe
        // stays drawn until the clip is over, which is what sMhrRage.active means now.
        case GMHR_FURY: {
            static u8 sBlastOut = 0;
            f32 furyFrame = MmForm_GerudoCurFrame();
            MmForm_GerudoTickClipBlades();
            MmForm_GerudoPlant(player);
            GerudoMhr_DemonThunderStrike(play, player);
            MmForm_GerudoLAttackTickDim(play);
            // She rides her own explosion out: invulnerable for the whole attack.
            if (player->invincibilityTimer > -GMHR_FURY_IFRAMES) {
                player->invincibilityTimer = -GMHR_FURY_IFRAMES;
            }
            // The Din's-shaped part is the COLLIDER, and it grows out of her from the
            // first frame of the clip rather than appearing at the impact.
            MmForm_GerudoCylOn(GMHR_LATK_DMGFLAGS | GMHR_FURY_MAGIC_FLAGS, sMhr.furyDamage);
            f32 grow = furyFrame / GMHR_FURY_BLAST_FRAMES;
            if (grow > 1.0f) {
                grow = 1.0f;
            }
            sMhr.cylRadius = (s16)(GMHR_SPIN_CYL_RADIUS + (GMHR_FURY_BLAST_RADIUS - GMHR_SPIN_CYL_RADIUS) * grow);
            sMhr.cylHeight = (s16)(GMHR_SPIN_CYL_HEIGHT + (GMHR_FURY_BLAST_HEIGHT - GMHR_SPIN_CYL_HEIGHT) * grow);
            if (!sBlastOut && (furyFrame >= GMHR_FURY_STRIKE_FRAME)) {
                sBlastOut = 1;
                MmForm_GerudoLAttackImpact(play, player, GMHR_LATK_DIM);
            }
            if (MmForm_GerudoAdvance(play, player)) {
                sBlastOut = 0;
                MmForm_GerudoCylOff();
                MmForm_GerudoEndClip(play, player);
            }
            return 1;
        }

        // L+A throws Gerudo's own charge wedge partway through the lunge.
        case GMHR_LUNGE: {
            static u8 sWaveOut = 0;
            MmForm_GerudoTickClipBlades();
            MmForm_GerudoPlant(player);
            MmForm_GerudoLAttackTickDim(play);
            if (!sWaveOut && (MmForm_GerudoCurFrame() >= GMHR_LUNGE_WAVE_FRAME)) {
                sWaveOut = 1;
                Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, player->actor.world.pos.x,
                            player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, 1);
                MmForm_GerudoLAttackImpact(play, player, GMHR_LATK_DIM / 2);
            }
            if (MmForm_GerudoAdvance(play, player)) {
                sWaveOut = 0;
                MmForm_GerudoEndClip(play, player);
            }
            return 1;
        }

        // L+R homes: it never misses, and she keeps spinning on the way in.
        case GMHR_SPIN:
            MmForm_GerudoTickClipBlades();
            MmForm_GerudoLAttackTickDim(play);
            if (!MmForm_GerudoSpinHome(player)) {
                MmForm_GerudoPlant(player);
            }
            if (MmForm_GerudoAdvance(play, player)) {
                MmForm_GerudoEndClip(play, player);
            }
            return 1;

        case GMHR_RAGE_ROLL: {
            f32 f = MmForm_GerudoCurFrame();
            MmForm_GerudoTickClipBlades();
            if (f < 20.0f) {
                MmForm_GerudoPlant(player);
                MmForm_GerudoCylOff();
            } else if (f < 41.0f) {
                // The roll's travel over frames 20-40, wall-checked, with roll i-frames.
                MmForm_GerudoHold(player);
                player->linearVelocity = 0.0f;
                MmForm_GerudoStepForward(play, player, 9.0f * GMHR_ROLL_SPEED_MUL * 0.5f);
                MmForm_GerudoCylOff();
                if (player->invincibilityTimer > -6)
                    player->invincibilityTimer = -6;
            } else {
                MmForm_GerudoPlant(player);
                if (f <= 75.0f)
                    MmForm_GerudoCylOn(DMG_SPIN_GIANT, 4);
                else
                    MmForm_GerudoCylOff();
            }
            if (MmForm_GerudoAdvance(play, player))
                MmForm_GerudoEndClip(play, player);
            return 1;
        }

        case GMHR_RAGE_PARRY: {
            f32 f = MmForm_GerudoCurFrame();
            MmForm_GerudoPlant(player);
            MmForm_GerudoTickClipBlades();
            if (f > 8.0f && f < 78.0f)
                MmForm_GerudoCylOff();
            if (f >= 78.0f && f <= 84.0f) {
                // The massive hit at frame 80: both blades, double.
                sMhr.bladeDmgFlags = DMG_SLASH_GIANT;
                sMhr.bladeDamage = 8;
                MmForm_GerudoCylOn(DMG_SPIN_GIANT, 8);
            } else if (f > 84.0f) {
                MmForm_GerudoCylOff();
            }
            if (player->invincibilityTimer > -10)
                player->invincibilityTimer = -10;
            if (MmForm_GerudoAdvance(play, player))
                MmForm_GerudoEndClip(play, player);
            return 1;
        }

        case GMHR_FRONT_SLASH: {
            // Cylinder the size of a spin attack, travelling the jump slash's distance
            // over the first 30 frames.
            f32 f = MmForm_GerudoCurFrame();
            MmForm_GerudoHold(player);
            player->linearVelocity = 0.0f;
            if (f < 30.0f)
                MmForm_GerudoStepForward(play, player, GMHR_FRONT_SLASH_DIST / 30.0f);
            if (f >= 4.0f && f <= 60.0f)
                MmForm_GerudoCylOn(sMhrRage.active ? DMG_SPIN_GIANT : DMG_SPIN_MASTER, sMhrRage.active ? 4 : 2);
            else
                MmForm_GerudoCylOff();
            // Cancellable from frame 0. OOT already consumed this frame's press, so a
            // cancel that only ended the clip threw the input away and nothing came out:
            // B has to start the swing here itself.
            if (aPress || bPress) {
                MmForm_GerudoEndClip(play, player);
                if (bPress) {
                    func_80837948(play, player, PLAYER_MWA_FORWARD_SLASH_1H);
                }
                return 1;
            }
            if (MmForm_GerudoAdvance(play, player))
                MmForm_GerudoEndClip(play, player);
            return 1;
        }

        case GMHR_RAGE_FRONT_START: {
            // Wind-up, invulnerable; at the end appear in front of the target (or 40
            // units ahead) and strike.
            MmForm_GerudoPlant(player);
            if (player->invincibilityTimer > -10)
                player->invincibilityTimer = -10;
            if (MmForm_GerudoAdvance(play, player)) {
                Actor* t = sMhr.frontTarget;
                if (t != NULL && t->update != NULL) {
                    s16 yaw = Math_Vec3f_Yaw(&player->actor.world.pos, &t->world.pos);
                    f32 stop = t->colChkInfo.cylRadius + 30.0f;
                    Vec3f dst;
                    dst.x = t->world.pos.x - Math_SinS(yaw) * stop;
                    dst.z = t->world.pos.z - Math_CosS(yaw) * stop;
                    dst.y = player->actor.world.pos.y;
                    Vec3f from = player->actor.world.pos;
                    from.y += 20.0f;
                    Vec3f to = dst;
                    to.y += 20.0f;
                    Vec3f hitPos;
                    CollisionPoly* poly = NULL;
                    s32 bgId;
                    if (!BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hitPos, &poly, true, false, false, true,
                                                 &bgId)) {
                        player->actor.world.pos.x = dst.x;
                        player->actor.world.pos.z = dst.z;
                    }
                    sGerudoComboLockedYaw = yaw;
                } else {
                    MmForm_GerudoStepForward(play, player, GMHR_RAGE_TELEPORT_DIST);
                }
                MmForm_GerudoStartClip(play, player, GMHR_CLIP_RAGE_FRONT_STRIKE, 0);
                sMhr.state = GMHR_RAGE_FRONT_STRIKE;
                MmForm_GerudoSpawnSlashTrails(play);
                sMhr.trailOn = 1;
                Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_SWING);
            }
            return 1;
        }

        case GMHR_RAGE_FRONT_STRIKE:
            MmForm_GerudoPlant(player);
            MmForm_GerudoTickClipBlades();
            if (player->invincibilityTimer > -10)
                player->invincibilityTimer = -10;
            if (MmForm_GerudoAdvance(play, player))
                MmForm_GerudoEndClip(play, player);
            return 1;

        case GMHR_AERIAL:
            MmForm_GerudoHold(player);
            MmForm_GerudoTickClipBlades();
            if (MmForm_GerudoAdvance(play, player) || onGround)
                MmForm_GerudoEndClip(play, player);
            return 1;

        case GMHR_RAGE_AERIAL_LOOP:
            MmForm_GerudoHold(player);
            MmForm_GerudoCylOn(DMG_SPIN_GIANT, 4);
            if (player->invincibilityTimer > -10)
                player->invincibilityTimer = -10;
            MmForm_GerudoAdvance(play, player);
            if (onGround || sMhr.timer > 60) {
                MmForm_GerudoCylOff();
                MmForm_GerudoStartClip(play, player, GMHR_CLIP_RAGE_AERIAL_END, 0);
                sMhr.state = GMHR_RAGE_AERIAL_END;
                sMhr.trailOn = 1;
            }
            return 1;

        case GMHR_RAGE_AERIAL_END:
            if (onGround)
                MmForm_GerudoPlant(player);
            else
                MmForm_GerudoHold(player);
            MmForm_GerudoTickClipBlades();
            if (MmForm_GerudoAdvance(play, player))
                MmForm_GerudoEndClip(play, player);
            return 1;

        case GMHR_SHEATHE:
            MmForm_GerudoPlant(player);
            if (MmForm_GerudoCurFrame() >= GMHR_SHEATHE_HIDE_FRAME)
                sMhr.swordsVisible = 0;
            if (MmForm_GerudoAdvance(play, player)) {
                MmForm_GerudoCommitItem(player);
                MmForm_GerudoEndClip(play, player);
            }
            return 1;

        case GMHR_DRAW:
            MmForm_GerudoPlant(player);
            // Standing draw = the sheathe backwards, so the blades reappear at the very
            // frame they left. The running draw has no such frame: show them at once.
            if (!sMhr.swordsVisible &&
                ((sMhr.clipId != GMHR_CLIP_DRAW_STAND) || (MmForm_GerudoCurFrame() <= GMHR_SHEATHE_HIDE_FRAME))) {
                sMhr.swordsVisible = 1;
                Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_PICKOUT);
            }
            if (MmForm_GerudoAdvance(play, player)) {
                MmForm_GerudoCommitItem(player);
                MmForm_GerudoEndClip(play, player);
            }
            return 1;
    }

    // ============================ TRIGGERS ================================
    if (!MmForm_GerudoCanAct(player))
        return 0;

    // B+R: Urbosa's Fury. The meter IS its charge — it spends every point and the blast
    // scales with what was banked, so a full bar one-shots a stunned boss and an almost
    // empty one barely stings.
    if (fighter && onGround && rHeld && bPress && (sMhr.state == GMHR_IDLE) && !MmForm_GerudoIsRolling(player) &&
        (sMhrRage.meter > 0)) {
        f32 charge = (f32)sMhrRage.meter / (f32)GerudoMhr_RageCapacity();
        sMhr.furyDamage = (u8)(GMHR_FURY_DAMAGE_MIN + (GMHR_FURY_DAMAGE_MAX - GMHR_FURY_DAMAGE_MIN) * charge);
        sMhrRage.meter = 0;
        sGerudoComboStep = 0;
        sGerudoComboLockedYaw = player->actor.shape.rot.y;
        player->stateFlags1 &= ~PLAYER_STATE1_SHIELDING;
        MmForm_GerudoStartClip(play, player, GMHR_CLIP_FURY, 0);
        sMhr.state = GMHR_FURY;
        Player_PlayVoiceSfx(player, NA_SE_VO_LI_AUTO_JUMP);
        return 1;
    }

    // Forward + B on the ground: front slash, in the slot the thrust used to hold.
    if (fighter && onGround && bPress && (sMhr.state == GMHR_IDLE) && !MmForm_GerudoIsRolling(player) &&
        MmForm_GerudoWantsFrontSlash(player)) {
        sGerudoComboLockedYaw = player->actor.shape.rot.y;
        MmForm_GerudoStartClip(play, player, GMHR_CLIP_FRONT_SLASH, 0);
        sMhr.state = GMHR_FRONT_SLASH;
        MmForm_GerudoSpawnSlashTrails(play);
        sMhr.trailOn = 1;
        Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_SWING);
        Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
        return 1;
    }

    // A in the air: aerial slash (rage: the sustained loop, invulnerable, then the end).
    // airFrames >= 2: the press that STARTED a hop or jump slash also arrives here on
    // its first airborne frame, and must not be taken as an aerial slash.
    if (fighter && !onGround && aPress && (sMhr.airFrames >= 2) && (player->meleeWeaponState == 0) &&
        !(player->stateFlags2 & PLAYER_STATE2_HOPPING) &&
        !(player->stateFlags1 & (PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE))) {
        sGerudoComboLockedYaw = player->actor.shape.rot.y;
        MmForm_GerudoStartClip(play, player, sMhrRage.active ? GMHR_CLIP_RAGE_AERIAL_LOOP : GMHR_CLIP_AERIAL, 0);
        sMhr.state = sMhrRage.active ? GMHR_RAGE_AERIAL_LOOP : GMHR_AERIAL;
        MmForm_GerudoSpawnSlashTrails(play);
        sMhr.trailOn = 1;
        Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_SWING);
        Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
        return 1;
    }

    return 0;
}
