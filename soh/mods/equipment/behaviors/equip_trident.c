/**
 * equip_trident.c - Trident (Extended Sword Slot 3) — Monster Hunter Rise "Gunlance".
 *
 * Replaces the Iron Knuckle's Axe in this slot: the axe became the HAMMER UPGRADE
 * (WeaponUpgrade_HasHammerAxe -> IKAxe_Behavior, driven from ExtEquip_UpdateBehavior and
 * independent of the ext-equipment grid), so its code is untouched but no longer lives here.
 *
 * Moveset (spec of 2026-08-17). Everything that vanilla already has a pipeline for
 * is served THROUGH that pipeline by swapping clips — that is what makes it stable:
 *   B, B, B           the three-slash chain (Trident_NextComboMwa sequences the rows),
 *                     each step morphing into the next
 *   fwd + B (Z)       lunging thrust — only while the chain is idle
 *   B (hold)          vanilla's charge, re-skinned; three levels, the full one fires
 *                     the big-magic ball
 *   Z + A             jump: rising strike
 *   R                 vanilla shield, vanilla poses. Divine Shield as a child, Mirror
 *                     as an adult, whatever was equipped before cleared first
 *   R + B             the guard dash: run with the lance out front (Trident_TickDash)
 *   R + A (hold)      Phantom Ganon flight — the ONE thing vanilla has no action for,
 *                     so it is the only state driven by hand under PAUSE_ACTION_FUNC
 *   in flight         stick moves, R up, L down, B light ball, A launch at the target
 *
 * Anims: gunlance clips in mhr_anims.o2r at
 * __OTR__misc/link_animetion/gMonsterHunterRise_Gunlance_<Motion>. Modelled on the Gerudo
 * Dual Blades controller (transformation_masks/gerudo_mhr_combat.inc.c) for the table
 * swaps, and on Odolwa's moth flight (boss_remains.cpp) for flying human Link.
 *
 * FRAME UNITS: every timer here is a 20 Hz logic tick (R_UPDATE_RATE = 3). OOT's
 * game logic does NOT run at 60 Hz. 100 frames = 5 seconds.
 *
 * Included by ext_equip_behavior.c (unity build).
 *
 * Skijer's NEI
 */

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------
// "all anims x2": every clip that has no user-approved length is resampled to HALF
// its source frames. The melee rows keep their own explicit lengths — the user saw
// those swings and approved them ("los slashes se ven bien"), so they are not
// re-timed here.
#define TRI_ANIM_SPEED 2.0f
#define TRI_BALL_MAGIC_COST 24 // double FIRE_ROD_MAGIC_SPIN_BIG (12), the NEI precedent
// Vanilla's charge fills at 0.02/frame (func_80844E3C) = full in ~43 frames. The
// spec wants 5 s = 100 frames, so the fill is throttled to this rate in
// Trident_TickCharge. 0.85 is vanilla's own "full" threshold (func_80844BE4).
#define TRI_CHARGE_RATE 0.0085f
#define TRI_CHARGE_FULL 0.85f
// Three charge levels. Vanilla only knows two and both of its thresholds are 0.85:
// En_M_Thunder turns the glow from blue to orange there (EnMThunder_Draw's
// spinChargePercent test) and func_80844BE4 picks BIG_SPIN there too. Level 3 is
// "the bar actually filled" — unk_858 is stepped toward 1.0 and stops.
#define TRI_CHARGE_L2 0.85f  // glow turns orange
#define TRI_CHARGE_L3 0.995f // filled
// ...and then held there. Without this the second level is unusable: the bar is at
// 0.85 after 100 frames and full 18 frames later, so a 0.9 s window would be all
// there ever was of it. One extra second at full is what makes level 3 a decision.
#define TRI_CHARGE_L3_HOLD 20
// Level 1 / 2 release: a Din's Fire dome around Link, small enough to cover only
// him. The HITBOX is vanilla's own spin attack (En_M_Thunder), which is already
// small at level 1 and wide at level 2 — "el collider de spin attack pequeño" /
// "más rádio y pues ya sabes igual que vanilla".
// Din's Fire itself draws at 0.15 and fills the room; level 1 is meant to cover
// Link and no more, level 2 to reach about as far as vanilla's big spin ring. If
// they come out wrong these two are the only numbers to move.
#define TRI_DOME_SCALE_L1 0.025f
#define TRI_DOME_SCALE_L2 0.060f
#define TRI_DOME_FRAMES 14
// Max charge (level 3). Frame numbers are the 191-frame release clip's own.
#define TRI_MAX_IMMUNE_LAST 24  // 0..24 golden and untouchable
#define TRI_MAX_FAST_FROM 25    // from here the clip runs at
#define TRI_MAX_FAST_SPEED 3.0f // ...this speed
#define TRI_MAX_BURST_FRAME 64  // the ball goes off
#define TRI_MAX_BALL_DMG 12     // super damage, straight into a boss
// Flight
#define TRI_FLY_ENTER_HOLD 6 // frames R+A must be held to take off
#define TRI_FLY_SPEED 5.5f   // "tu speed walking normal"
#define TRI_FLY_CLIMB 4.0f   // R = up, L = down
#define TRI_FLY_MAGIC_COST 4 // per TRI_FLY_MAGIC_TICK frames, free with the Magic Cape
#define TRI_FLY_MAGIC_TICK 20
#define TRI_FLY_LAUNCH_SPEED 22.0f
#define TRI_FLY_LAUNCH_MAX 40    // frames before a launch that finds nothing gives up
#define TRI_FLY_LAUNCH_HIT 45.0f // distance to the target that counts as arriving
// Held straight launch: the clip ping-pongs across these two frames while A is down.
#define TRI_FLY_LAUNCH_LOOP_A 10
#define TRI_FLY_LAUNCH_LOOP_B 16
// Arc launch (with a lock-on). TRI_FLY_ARC_FALL is the per-frame gravity of the
// throw; the upward speed is SOLVED at entry so the parabola actually lands on the
// target, and TRI_FLY_ARC_MIN keeps a close target from getting a flat one.
#define TRI_FLY_ARC_FALL 1.4f
#define TRI_FLY_ARC_MIN 5.0f
// The air slam.
#define TRI_POUND_FALL (-22.0f)
#define TRI_POUND_RADIUS 110.0f
#define TRI_POUND_DMG 4
// The ring is a flat textured quad, not ring geometry, so it needs real size to read
// at all. These two are the knobs if it comes out too small or too big.
#define TRI_POUND_FX_SCALE 400
#define TRI_POUND_FX_STEP 90
#define TRI_FLY_SHOOT_FRAME 8 // frame of the shoot clip that releases the light ball
// Jump (Z+A): a heavy weapon hops short and low. Multipliers on vanilla's launch
// (xz 5.0 / y 5.0), plus the small step forward the landing pound takes.
#define TRI_JUMP_XZ_MUL 0.45f
#define TRI_JUMP_Y_MUL 0.7f
#define TRI_JUMP_FINISH_LUNGE 3.0f
// Lunge speed held through the thrust's windup. 15.0f is what OOT itself uses for
// PLAYER_STATE2_SWORD_LUNGE (z_player.c:17356), so the gunlance thrust travels
// exactly as far as a vanilla lunge — it just holds it for longer.
#define TRI_STAB_LUNGE_SPEED 15.0f
// R+B guard dash: run with the lance out front. Barely hurts — it is a way to cover
// ground without sheathing ("no harás mucho daño pero te ayudará a moverte sin
// guardar el item").
#define TRI_DASH_DMG 1
#define TRI_DASH_SPEED_MUL 1.2f // on top of whatever Link's own speed already is
// Link's plain run, the floor the multiplier applies to when the stick is neutral.
// A stick-derived target ABOVE this wins instead, which is how boots and any other
// speed modifier keep counting.
#define TRI_DASH_BASE 9.0f
#define TRI_DASH_START_FRAMES 14 // fallback if the wind-up clip is missing

// Damage. The trident's melee is Master-Sword class, expressed as a FLAG so each
// enemy's own DamageTable resolves it — the same reason the charge ball uses it.
#define TRI_MELEE_DMG 2

// ---------------------------------------------------------------------------
// Clip paths
//
// The Gunlance clips carry the animation catalog's semantic names inside the
// o2r, and the archive is packed root-frozen (the clips are stationary).
// ---------------------------------------------------------------------------
#define TRIP(name) "__OTR__misc/link_animetion/gMonsterHunterRise_Gunlance_" name

// Guard: the poses are VANILLA's now, so there is no guard idle here any more.
// R+B from the guard = slash 1 with the shield still up (vanilla crouch-stab).
#define TRIP_GUARD_STAB TRIP("StationarySingleGunlanceThrust") //  31f
// Draw / sheathe (vanilla item change, upper body).
#define TRIP_UNSHEATH TRIP("ForwardMultiHitWeaponTransition") //  60f
#define TRIP_SHEATH TRIP("ForwardRisingAerialMove")           //  59f
// Fighter walk/run.
//
// The clip is a good 45-frame two-step cycle and closes cleanly on itself (measured
// off the o2r: pose distance frame 0 -> 44 is 8290, well under one frame of ordinary
// motion at 15470). It is used two different ways, and only ONE of them is bound to a
// length:
//   · The locomotion TABLE (walk/run groups) is not played, it is SAMPLED from
//     unk_868, a phase accumulator that wraps at exactly 29.0 (z_player.c:9779) — so
//     that copy has to be resampled to 29.
//   · The guard dash plays it as an ordinary ANIMMODE_LOOP clip at its native 45,
//     where the engine's own loop handles the wrap and no length is imposed.
#define TRIP_WALK TRIP("ForwardWeaponRun") //  45f
// R+B guard dash: the lunge into it, then the pose held over the running legs.
#define TRIP_DASH_START TRIP("BackwardDoubleWeaponTransition_Variant07") //  29f, one-shot
#define TRIP_DASH_POSE TRIP("StationaryGuardIdle_Variant14")             // 133f, a held stance
// Phantom Ganon flight.
#define TRIP_FLY_START TRIP("ForwardDoubleChargedShellingMotion")         //  67f
#define TRIP_FLY_IDLE TRIP("StationaryGuardIdle_Variant10")               //  35f
#define TRIP_FLY_SHOOT_PRE TRIP("ForwardMultiHitWeaponTransition")        //  60f
#define TRIP_FLY_SHOOT TRIP("StationaryTripleWeaponTransition_Variant11") //  76f
#define TRIP_FLY_LAUNCH TRIP("ForwardSingleChargedShellingMotion")        //  69f
#define TRIP_FLY_POUND TRIP("ForwardRisingTripleChargedShellingMotion")   //  77f
#define TRIP_FLY_LAND TRIP("StationaryRisingAerialMove_Variant25")        //  57f
// The landing the jump slash already uses; the air slam borrows it to finish.
#define TRIP_JUMP_FINISH TRIP("ForwardRisingMultiHitAerialThrust") // 132f

// ---------------------------------------------------------------------------
// MELEE ANIMATION TABLE — the actual mechanism.
//
// The Trident does NOT steal the B button. Stealing B is the approach the Gerudo
// Dual Blades moveset tried and explicitly abandoned (see the tombstone comment in
// TransformMasks_FilterB): OOT's attack pipeline owns drawing the weapon, facing
// the target, chaining swings, the recovery and the putaway — take B away and you
// lose all of it, and your moveset additionally races actionFunc by one frame.
// That race IS the "sometimes it swings the sword, sometimes the trident" mix.
//
// Instead: B reaches OOT's normal pipeline, and we swap the CLIPS that pipeline
// plays. The pipeline then *is* the moveset. Same thing MmForm_GerudoInstallAnims
// does (gerudo_mhr_combat.inc.c:349).
//
// Both 1H and 2H rows are bound, so the moveset holds regardless of which sword
// the player actually has equipped underneath.
//
// ⚠️ These are GLOBAL engine tables. Restore is mandatory on every exit path or
// plain Link keeps swinging gunlance animations.
// ---------------------------------------------------------------------------
typedef struct {
    s32 mwa;
    const char* path;
    // Inclusive sub-range of the SOURCE clip, -1/-1 = the whole thing. This is how
    // one packed clip serves several rows: ForwardDoubleWeaponTransition is a single
    // 77-frame double thrust, and its two halves are slash 3's "anim 1" and "anim 2".
    s16 srcStart;
    s16 srcEnd;
    // Resample the (sub-)clip to this many frames. 0 = keep the source length.
    // This is how playback SPEED is set: ExtPlayer_SetMeleeAnim has no speed
    // argument, so a faster swing = the same motion resampled into fewer frames.
    // Gerudo does the same for its locomotion rows.
    s16 frames;
    // Damage window, IN SOURCE-CLIP FRAMES (the user's numbers are the original
    // clip's — confirmed). Install rescales them into the installed clip's own
    // frame space, so retuning `frames` never silently moves the hitbox.
    // -1 = keep whatever vanilla had for that row.
    s16 hitStartSrc; // first frame the quad actually damages
    s16 hitEndSrc;   // last frame the quad exists at all
    // Settle-back clip, NULL = vanilla's.
    //
    // ⚠️ MEDIDO 2026-08-17: NO poner aquí clips *WeaponTransition. Los tres que
    // llevaba (StationaryMultiHit/Double/TripleWeaponTransition) giran la RAÍZ del
    // esqueleto una vuelta entera (limb 0 rotY recorre 318°-327° del rango s16),
    // así que cada tajo acababa con Link girado 90° — el bug de "hace una anim de
    // recover que lo gira". Todos los clips que el usuario nombró en su spec tienen
    // ese recorrido a 0. La recovery de vanilla es corta y no gira: se queda esa.
    const char* recovery;
    const char* semantic;
} TridentMeleeBinding;

// Charge attack. The spin-attack rows are NOT used for the STANCE: the charge lives
// in its own six-phase table (ExtPlayer_SetChargeAnim), outside both animation tables.
//
// ⚠️ VERIFICADO: no existe una fase "charge max". Vanilla sólo tiene START / START_L /
// WAIT / WAIT_END / WALK / SIDE_WALK — la carga completa no cambia de pose, sólo cambia
// el remate. Por eso las poses de nivel 2 y 3 se instalan reescribiendo WAIT en caliente
// cuando la carga pasa cada umbral, y se restauran al soltar. Skijer's NEI
//
// Los tres niveles son tres variantes del MISMO guard idle, así que el cambio de pose
// se ve como un reajuste del mismo aguante — Trident_SetChargeLevel lo interpola
// ("usa un interpol para que link se posicione entre una y otra en los changes").
#define TRIP_CHARGE_START TRIP("StationarySingleWeaponTransition_Variant03") //  84f
#define TRIP_CHARGE_WAIT TRIP("StationaryGuardIdle_Variant03")               //  43f  nivel 1
#define TRIP_CHARGE_MAX TRIP("StationaryGuardIdle_Variant04")                //  43f  nivel 2
#define TRIP_CHARGE_MAX3 TRIP("StationaryGuardIdle_Variant05")               //  43f  nivel 3
// Remate de nivel 1 y 2: el mismo clip, lo que cambia es el radio de la cúpula.
#define TRIP_CHARGE_REL TRIP("BackwardMultiHitAerialThrust") //  38f
// Remate de carga máxima ("la anim que ya hace"): 191 frames, con los marcadores
// TRI_MAX_* medidos sobre ESTOS frames.
#define TRIP_CHARGE_END TRIP("BackwardHighAerialMultiHitSilkbindGunlanceStrike") // 191f
// Te golpean cargando.
#define TRIP_CHARGE_HURT TRIP("BackwardRisingAerialMove_Variant06") //  97f

// Slash 3, and then the chain closes on a THRUST.
//
// It used to close on a shell explosion instead: slash 3 was two clips, the second
// auto-chained without a press and detonated a burst in front of Link. That whole
// idea is gone ("puedes quitarle lo de las explosiones? mejor haz que el ataque final
// sea invocar una estocada al final del combo de B") — the fourth step is now a real
// fourth press that plays the lunging thrust, so the chain is 1 -> 2 -> 3 -> estocada
// and every step is something the player asked for.
#define TRIP_SLASH3 TRIP("ForwardDoubleWeaponTransition") // 77f
#define TRIP_THRUST TRIP("ForwardMultiHitLungingThrust")  // 79f

static const TridentMeleeBinding sTridentMeleeBindings[] = {
    // ── The chain: slash 1 -> 2 -> 3 -> thrust ────────────────────────────────
    // Which PLAYER_MWA_* holds which step is arbitrary: Trident_NextComboMwa picks
    // the row, OOT's stick-angle picker does not. sTridentComboRows is the order.
    // Both 1H and 2H rows are bound so the chain holds whatever sword is underneath.
    //
    // Frame counts are the ORIGINAL clip's (the user's numbers); Trident_ScaleFrame
    // maps them onto the installed length. `frames` values below are the ones the
    // user saw and approved — not re-timed to x2.

    // Slash 1 — 31f. Collider arms at source frame 8, stays to the end.
    { PLAYER_MWA_FORWARD_SLASH_1H, TRIP("StationarySingleGunlanceThrust"), -1, -1, 14, 8, 0, NULL, "slash1" },
    { PLAYER_MWA_FORWARD_SLASH_2H, TRIP("StationarySingleGunlanceThrust"), -1, -1, 14, 8, 0, NULL, "slash1" },
    // Slash 2 — 132f rising multi-hit. Vanilla's window kept.
    { PLAYER_MWA_FORWARD_COMBO_1H, TRIP("ForwardRisingMultiHitAerialThrust"), -1, -1, 20, -1, -1, NULL, "slash2" },
    { PLAYER_MWA_FORWARD_COMBO_2H, TRIP("ForwardRisingMultiHitAerialThrust"), -1, -1, 20, -1, -1, NULL, "slash2" },
    // Slash 3 — small prep quad, source frames 5..15.
    { PLAYER_MWA_RIGHT_SLASH_1H, TRIP_SLASH3, -1, -1, 24, 5, 15, NULL, "slash3" },
    { PLAYER_MWA_RIGHT_SLASH_2H, TRIP_SLASH3, -1, -1, 24, 5, 15, NULL, "slash3" },
    { PLAYER_MWA_LEFT_SLASH_1H, TRIP_SLASH3, -1, -1, 24, 5, 15, NULL, "slash3" },
    { PLAYER_MWA_LEFT_SLASH_2H, TRIP_SLASH3, -1, -1, 24, 5, 15, NULL, "slash3" },
    // NOT a chain step any more (the chain is three). Bound to the thrust anyway so
    // that if anything ever does reach these rows what comes out is a gunlance clip
    // and not Link's sword.
    { PLAYER_MWA_RIGHT_COMBO_1H, TRIP_THRUST, -1, -1, 26, 30, 0, NULL, "stab" },
    { PLAYER_MWA_RIGHT_COMBO_2H, TRIP_THRUST, -1, -1, 26, 30, 0, NULL, "stab" },
    { PLAYER_MWA_LEFT_COMBO_1H, TRIP_THRUST, -1, -1, 26, 30, 0, NULL, "stab" },
    { PLAYER_MWA_LEFT_COMBO_2H, TRIP_THRUST, -1, -1, 26, 30, 0, NULL, "stab" },

    // Thrust — 79f. Quad arms at source frame 30; the lunge is held until 25
    // (Trident_TickMelee — that is movement, not collider).
    { PLAYER_MWA_STAB_1H, TRIP_THRUST, -1, -1, 26, 30, 0, NULL, "stab" },
    { PLAYER_MWA_STAB_2H, TRIP_THRUST, -1, -1, 26, 30, 0, NULL, "stab" },
    { PLAYER_MWA_STAB_COMBO_1H, TRIP_THRUST, -1, -1, 26, 30, 0, NULL, "stab" },
    { PLAYER_MWA_STAB_COMBO_2H, TRIP_THRUST, -1, -1, 26, 30, 0, NULL, "stab" },

    // Jump (Z+A). START is airborne: rises, then from source frame 56 falls with the
    // pound wound up, holding its last frame until touchdown (PlayOnce holds).
    // FINISH is the landing: the pound itself (ground shock, Trident_TickMelee) and
    // a short lunge — "en jumpslash te hará avanzar más".
    { PLAYER_MWA_JUMPSLASH_START, TRIP("ForwardRisingTripleChargedShellingMotion"), -1, -1, 38, -1, -1, NULL,
      "jumpStart" },
    { PLAYER_MWA_JUMPSLASH_FINISH, TRIP("ForwardRisingMultiHitAerialThrust"), -1, -1, 24, 0, 0, NULL, "jumpFinish" },

    // Charge (B held) replaces the spin attack. Levels 1 AND 2 land on the SPIN_ATTACK
    // rows — vanilla would send level 2 to BIG_SPIN (its threshold is 0.85 for both the
    // orange glow and the row), but we need BIG_SPIN free for the third level, so
    // Trident_NextComboMwa sends it back here unless the bar actually filled. Which
    // level it was is remembered in sTri.releaseLevel and only changes the dome's size.
    { PLAYER_MWA_SPIN_ATTACK_1H, TRIP_CHARGE_REL, -1, -1, 19, -1, -1, NULL, "chargeLvl1" },
    { PLAYER_MWA_SPIN_ATTACK_2H, TRIP_CHARGE_REL, -1, -1, 19, -1, -1, NULL, "chargeLvl1" },
    { PLAYER_MWA_BIG_SPIN_1H, TRIP_CHARGE_END, -1, -1, 0, -1, -1, NULL, "chargeLvl2" },
    { PLAYER_MWA_BIG_SPIN_2H, TRIP_CHARGE_END, -1, -1, 0, -1, -1, NULL, "chargeLvl2" },
};

#define TRIDENT_MELEE_BINDING_COUNT (sizeof(sTridentMeleeBindings) / sizeof(sTridentMeleeBindings[0]))

static struct {
    u8 installed;
    LinkAnimationHeader* savedMelee[TRIDENT_MELEE_BINDING_COUNT];
    // What we PUT in each row, so Trident_CurrentRow can recognise the clip OOT is
    // playing without relying on a state flag that does not exist.
    LinkAnimationHeader* installedMelee[TRIDENT_MELEE_BINDING_COUNT];
    LinkAnimationHeader* savedMeleeEnd[TRIDENT_MELEE_BINDING_COUNT];
    LinkAnimationHeader* savedMeleeEndLock[TRIDENT_MELEE_BINDING_COUNT];
    u8 savedHitStart[TRIDENT_MELEE_BINDING_COUNT];
    u8 savedHitEnd[TRIDENT_MELEE_BINDING_COUNT];
    // Source and installed lengths, measured once at install so the per-frame tick
    // can map a source-space marker with plain arithmetic.
    s16 srcLen[TRIDENT_MELEE_BINDING_COUNT];
    s16 outLen[TRIDENT_MELEE_BINDING_COUNT];
    LinkAnimationHeader* savedCharge[EXTPLAYER_CHARGE_PHASE_MAX][2];
    // The three charge poses, kept so the WAIT phase can be swapped in place as the
    // charge climbs (see Trident_InstallChargeAnims / Trident_SetChargeLevel).
    LinkAnimationHeader* chargeStance[3]; // [0]=lvl1 [1]=lvl2 [2]=lvl3
    s8 chargeLevelShown;                  // -1 = none installed yet
} sTridentAnimTables = { 0 };

// How many frames the row ACTUALLY ends up with, so a source-space frame marker can
// be mapped onto it. Mirrors MmForm_GerudoBindingFrames: an explicit `frames` wins,
// otherwise the (sub-)range keeps its own length.
static s16 Trident_InstalledLen(const TridentMeleeBinding* b, LinkAnimationHeader* raw) {
    s16 srcLen;
    if (b->frames > 0) {
        return b->frames;
    }
    if ((b->srcStart >= 0) && (b->srcEnd >= b->srcStart)) {
        return (s16)(b->srcEnd - b->srcStart + 1);
    }
    srcLen = (raw != NULL) ? (s16)raw->common.frameCount : 0;
    return srcLen;
}

// Map a marker given in SOURCE-clip frames onto the installed clip. The user's
// numbers are the original clip's, so retuning `frames` must not move the hitbox.
//   -1 -> 0xFF ("keep vanilla's")
//    0 -> the installed clip's last frame ("to the end")
static u8 Trident_ScaleFrame(const TridentMeleeBinding* b, s16 srcFrame, LinkAnimationHeader* raw, s16 outLen) {
    s16 base;
    s16 srcLen;
    s32 scaled;

    if (srcFrame < 0) {
        return 0xFF;
    }
    if (outLen < 1) {
        outLen = 1;
    }
    if (srcFrame == 0) {
        return (u8)((outLen > 255) ? 255 : outLen);
    }

    // A sub-range row counts from its own first frame, not the packed clip's.
    base = (b->srcStart > 0) ? b->srcStart : 0;
    if ((b->srcStart >= 0) && (b->srcEnd >= b->srcStart)) {
        srcLen = (s16)(b->srcEnd - b->srcStart + 1);
    } else {
        srcLen = (raw != NULL) ? (s16)raw->common.frameCount : outLen;
    }
    if (srcLen < 1) {
        srcLen = 1;
    }

    scaled = ((s32)(srcFrame - base) * (s32)outLen) / (s32)srcLen;
    if (scaled < 0) {
        scaled = 0;
    }
    if (scaled > 255) {
        scaled = 255;
    }
    return (u8)scaled;
}

static void Trident_InstallChargeAnims(void);
static void Trident_RestoreChargeAnims(void);

static void Trident_InstallAnims(void) {
    if (sTridentAnimTables.installed) {
        return;
    }
    for (size_t i = 0; i < TRIDENT_MELEE_BINDING_COUNT; i++) {
        const TridentMeleeBinding* b = &sTridentMeleeBindings[i];
        LinkAnimationHeader* raw;
        LinkAnimationHeader* anim;
        LinkAnimationHeader* rec;
        s16 outLen;

        ExtPlayer_GetMeleeAnim(b->mwa, &sTridentAnimTables.savedMelee[i], &sTridentAnimTables.savedMeleeEnd[i],
                               &sTridentAnimTables.savedMeleeEndLock[i], &sTridentAnimTables.savedHitStart[i],
                               &sTridentAnimTables.savedHitEnd[i]);

        // The raw header is only read for its frameCount, which is what makes the
        // source-space markers survive a clip swap without a hand-kept table.
        raw = ResourceMgr_LoadPlayerAnimAsHeader(b->path);
        outLen = Trident_InstalledLen(b, raw);
        sTridentAnimTables.outLen[i] = outLen;
        sTridentAnimTables.srcLen[i] = ((b->srcStart >= 0) && (b->srcEnd >= b->srcStart))
                                           ? (s16)(b->srcEnd - b->srcStart + 1)
                                           : ((raw != NULL) ? (s16)raw->common.frameCount : outLen);

        anim = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(b->path, 1, b->srcStart, b->srcEnd, b->frames);
        sTridentAnimTables.installedMelee[i] = anim;
        if (anim == NULL) {
            // Row left vanilla. No log: this unity TU is C (z_player.c ->
            // extended_equipment.c -> ext_equip_behavior.c), so spdlog is out of reach.
            continue;
        }

        rec = (b->recovery != NULL) ? ResourceMgr_LoadPlayerAnimAsHeaderInPlace(b->recovery, 1) : NULL;
        // The recovery goes into BOTH slots: unk_04 is the normal settle-back and
        // unk_08 the one used while locked on. Passing NULL keeps vanilla's.
        ExtPlayer_SetMeleeAnim(b->mwa, anim, rec, rec, Trident_ScaleFrame(b, b->hitStartSrc, raw, outLen),
                               Trident_ScaleFrame(b, b->hitEndSrc, raw, outLen));
    }

    Trident_InstallChargeAnims();
    sTridentAnimTables.installed = 1;
}

// MUST run on every path out of the slot — unequip, death, save load. These are
// global engine tables: leaving them installed gives plain Link gunlance swings.
static void Trident_RestoreAnims(void) {
    if (!sTridentAnimTables.installed) {
        return;
    }
    for (size_t i = 0; i < TRIDENT_MELEE_BINDING_COUNT; i++) {
        ExtPlayer_SetMeleeAnim(sTridentMeleeBindings[i].mwa, sTridentAnimTables.savedMelee[i],
                               sTridentAnimTables.savedMeleeEnd[i], sTridentAnimTables.savedMeleeEndLock[i],
                               sTridentAnimTables.savedHitStart[i], sTridentAnimTables.savedHitEnd[i]);
    }
    Trident_RestoreChargeAnims();
    sTridentAnimTables.installed = 0;
}

// ---- the charge stance -----------------------------------------------------
// Six two-entry arrays in z_player.c, outside both animation tables — which is why
// the charge kept playing Link's own windup while every other action had changed.
//
// ⚠️ VERIFICADO: no hay fase "charge max". Vanilla no cambia de pose al llenarse la
// carga, sólo cambia el remate, así que la pose de máximo se instala reescribiendo
// WAIT en caliente (Trident_SetChargeLevel) y se restaura al soltar.
static const s32 sTridentChargePhases[] = {
    EXTPLAYER_CHARGE_START,    EXTPLAYER_CHARGE_START_L, EXTPLAYER_CHARGE_WAIT,
    EXTPLAYER_CHARGE_WAIT_END, EXTPLAYER_CHARGE_WALK,    EXTPLAYER_CHARGE_SIDE_WALK,
};
#define TRIDENT_CHARGE_PHASE_COUNT (sizeof(sTridentChargePhases) / sizeof(sTridentChargePhases[0]))

static void Trident_InstallChargeAnims(void) {
    LinkAnimationHeader* start = ResourceMgr_LoadPlayerAnimAsHeaderInPlace(TRIP_CHARGE_START, 1);
    LinkAnimationHeader* wait;

    sTridentAnimTables.chargeStance[0] = ResourceMgr_LoadPlayerAnimAsHeaderInPlace(TRIP_CHARGE_WAIT, 1);
    sTridentAnimTables.chargeStance[1] = ResourceMgr_LoadPlayerAnimAsHeaderInPlace(TRIP_CHARGE_MAX, 1);
    sTridentAnimTables.chargeStance[2] = ResourceMgr_LoadPlayerAnimAsHeaderInPlace(TRIP_CHARGE_MAX3, 1);
    sTridentAnimTables.chargeLevelShown = 0;
    wait = sTridentAnimTables.chargeStance[0];

    for (size_t i = 0; i < TRIDENT_CHARGE_PHASE_COUNT; i++) {
        s32 phase = sTridentChargePhases[i];
        // START / START_L are the windup; every other phase is the held stance, so
        // the pose reads the same standing, walking or strafing.
        LinkAnimationHeader* use =
            ((phase == EXTPLAYER_CHARGE_START) || (phase == EXTPLAYER_CHARGE_START_L)) ? start : wait;
        for (s32 h = 0; h < 2; h++) {
            sTridentAnimTables.savedCharge[phase][h] = ExtPlayer_GetChargeAnim(phase, h);
            if (use != NULL) {
                ExtPlayer_SetChargeAnim(phase, h, use);
            }
        }
    }
}

static void Trident_RestoreChargeAnims(void) {
    for (size_t i = 0; i < TRIDENT_CHARGE_PHASE_COUNT; i++) {
        s32 phase = sTridentChargePhases[i];
        for (s32 h = 0; h < 2; h++) {
            ExtPlayer_SetChargeAnim(phase, h, sTridentAnimTables.savedCharge[phase][h]);
        }
    }
    sTridentAnimTables.chargeLevelShown = 0;
}

// Move the held stance to `lvl` (0/1/2), in place. Only touches the table when the
// level actually changes, so it costs nothing on the frames in between.
//
// The table swap alone is NOT enough for the standing hold. Player_Action_80844E68
// only issues Player_AnimPlayLoop when the PREVIOUS clip ends, and a looping clip
// never ends — so the standing charge would keep playing whichever pose was current
// when the loop started, forever. The morphing LinkAnimation_Change below is what
// actually moves Link, and its -8 is the "interpol" between poses. The walking
// phases need none of it: Player_Action_80845000 re-reads the table every frame
// through LinkAnimation_BlendToJoint. Skijer's NEI
#define TRI_CHARGE_MORPH 8.0f

static void Trident_SetChargeLevel(PlayState* play, Player* player, s32 lvl) {
    LinkAnimationHeader* use;
    LinkAnimationHeader* was;

    if (lvl < 0) {
        lvl = 0;
    } else if (lvl > 2) {
        lvl = 2;
    }
    if (!sTridentAnimTables.installed || (sTridentAnimTables.chargeLevelShown == lvl)) {
        return;
    }
    use = sTridentAnimTables.chargeStance[lvl];
    if (use == NULL) {
        return;
    }
    was = sTridentAnimTables.chargeStance[sTridentAnimTables.chargeLevelShown];

    // Only the held phases: the windup keeps its own clip whatever the level.
    for (size_t i = 0; i < TRIDENT_CHARGE_PHASE_COUNT; i++) {
        s32 phase = sTridentChargePhases[i];
        if ((phase == EXTPLAYER_CHARGE_START) || (phase == EXTPLAYER_CHARGE_START_L)) {
            continue;
        }
        for (s32 h = 0; h < 2; h++) {
            ExtPlayer_SetChargeAnim(phase, h, use);
        }
    }
    sTridentAnimTables.chargeLevelShown = (s8)lvl;

    // Blend into the new pose, but only if the stance we are replacing is the one on
    // screen — otherwise a level change during the windup (or during a swing that
    // happened to overlap) would yank Link out of whatever he was doing.
    if ((play != NULL) && (player != NULL) && (was != NULL) && ((void*)player->skelAnime.animation == (void*)was)) {
        LinkAnimation_Change(play, &player->skelAnime, use, 1.0f, 0.0f, Animation_GetLastFrame(use), ANIMMODE_LOOP,
                             -TRI_CHARGE_MORPH);
    }
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
typedef enum {
    // ⚠️ Esta máquina de estados NO lleva la cadena de suelo, ni la guardia, ni la
    // carga, ni la estocada, ni el salto: todo eso lo reproduce la tubería de OOT
    // con los clips que instalamos, y lo que pasa dentro lo ven Trident_TickMelee /
    // Trident_TickCharge. Aquí viven las dos cosas para las que vanilla no tiene
    // ninguna acción: el vuelo de Phantom Ganon y la carrera de R+B.
    TRI_IDLE = 0,
    TRI_FLY_START,     // despegue (ForwardDoubleChargedShellingMotion)
    TRI_FLY_IDLE,      // planeo (StationaryGuardIdle_Variant10 en bucle)
    TRI_FLY_SHOOT_PRE, // B en vuelo: el preparativo
    TRI_FLY_SHOOT,     // ...y el clip que suelta la bola
    TRI_FLY_LAUNCH,    // A en vuelo: recta mantenida, o arco comprometido si hay target
    TRI_FLY_POUND,     // R+B en vuelo: se deja caer a plomo sobre el suelo
    // R+B: la carrera con la lanza por delante. Vanilla tampoco tiene esto — su R+B
    // es la estocada agachada, que se queda de reserva por si la entrada no cuaja.
    TRI_DASH_START, // el arranque (BackwardDoubleWeaponTransition_Variant07)
    TRI_DASH_RUN,   // corriendo: pose arriba, ciclo de carrera en las piernas
} TridentState;

// ⚠️ El límite es el ÚLTIMO estado de vuelo del enum. Añadir uno detrás sin tocar
// esto lo deja fuera del rango y Trident_Behavior lo despacha a la carrera de R+B.
#define TRI_IS_FLYING(s) (((s) >= TRI_FLY_START) && ((s) <= TRI_FLY_POUND))

static struct {
    u8 inited;
    TridentState state;
    s16 timer;

    // Custom-clip driver (flight only). prevFrame feeds the crossing tests.
    f32 prevFrame;
    u8 windowOpen;

    // Per-frame melee tick: which binding row OOT is playing and where it was.
    s8 meleeRow;
    f32 meleePrevFrame;
    u8 ballPaid;     // this charge release already paid / fired
    f32 chargePrev;  // unk_858 last frame — the fill throttle needs the delta
    u8 chargeShield; // the shield was raised for the charge; hand models to restore
    u8 ballArmed;    // magic paid this release: fire on the clip's last frame
    s8 chargeLvl;    // 1/2/3 while B is held; 0 = not charging
    s16 bHold;       // frames B has been down WITHOUT a release (mash vs hold)
    s16 fullHold;    // frames the bar has been full — level 3 needs TRI_CHARGE_L3_HOLD of them
    s8 releaseLevel; // the level the release row was entered with
    u8 hurtPending;  // hit while charging: swap in the stagger clip after the action func
    s16 goldTimer;   // frames the golden armour is on (immunity window)
    // The Din's Fire dome that closes a level 1/2 release.
    f32 domeScale;
    s16 domeTimer;
    // Big-magic ball (Trident_TickBigMagic / Trident_Draw)
    u8 bmActive;
    Vec3f bmAnchor;
    f32 bmCircle;
    f32 bmBall;
    f32 bmAlpha;
    s16 bmRays;
    s16 bmTimer;

    // Flight.
    PlayerActionFunc flyAction; // actionFunc at takeoff; a change means damage/cutscene took over
    s16 flyHold;                // frames R+A held toward takeoff
    s16 flyMagicTick;
    Actor* launchTarget;
    s16 launchTimer;
    u8 shot; // this shoot clip already released its ball
    // One field, two mutually exclusive launches: WITHOUT a lock-on it is where the
    // ping-pong across the clip's 10..16 window currently sits; WITH one it is the
    // upward speed solved at entry that makes the arc land on the target.
    f32 launchPhase;
    s8 launchPing; // ping-pong direction (straight launch only)

    // Locomotion install (walk/run rows) — only while the weapon is DRAWN.
    u8 locoInstalled;
    LinkAnimationHeader* savedWalk[PLAYER_ANIMTYPE_MAX];
    LinkAnimationHeader* savedRun[PLAYER_ANIMTYPE_MAX];
    u8 heavyBoots; // iron-boots REGs currently applied
} sTri = { 0 };

extern LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* path);
extern u8 ResourceMgr_FileExists(const char* resName);
// Declared in soh/ResourceManagerHelpers.h, which this TU does not pull in.
// stripY = 1: none of these clips may carry their own root translation, or the
// swing/flight would also teleport Link (OOT owns the movement).
extern LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlace(const char* animPath, u8 stripY);
extern LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(const char* animPath, u8 stripY,
                                                                               s16 frames);
// Inclusive sub-range (-1/-1 = whole clip) plus a target length. Cuts one packed
// clip into several engine rows — the guard's raise and hold come out of a single
// 147-frame idle this way.
extern LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(const char* animPath, u8 stripY,
                                                                           s16 firstFrame, s16 lastFrame,
                                                                           s16 targetFrames);
// Defined in z_player.c; no public header declares them, so they are pulled in by
// hand exactly as the other NEI player-side modules do (see equip_pendant.c and
// item_rod_*.c for the same externs).
extern void Player_RequestRumble(Player* this, s32 sourceStrength, s32 duration, s32 decreaseRate, s32 distSq);
extern void Player_PlayVoiceSfx(Player* this, u16 sfxId);
extern void Player_AnimPlayOnce(PlayState* play, Player* this, LinkAnimationHeader* anim);
extern void func_80837948(PlayState* play, Player* this, s32 meleeWeaponAnim);
extern void func_80839FFC(Player* this, PlayState* play);
extern s32 Player_GetMovementSpeedAndYaw(Player* this, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                         PlayState* play);
extern void Player_SetBootData(PlayState* play, Player* this);
// Vanilla's charge action. The guard dash enters it deliberately and then keeps
// unk_858 at 0, borrowing it as a stable host state (the Pegasus Boots trick).
extern void func_808377DC(PlayState* play, Player* this);
extern void Player_RequestQuake(PlayState* play, s32 speed, s32 y, s32 countdown);
// Intangibility, NOT invulnerability: the first stops damage AND the knockback that
// would tear the max-charge release in half; the second only stops the damage.
extern void Player_SetIntangibility(Player* this, s32 timer);
extern void Player_UseItem(PlayState* play, Player* this, s32 item);
extern LinkAnimationHeader* ExtPlayer_GetAnimGroupAnim(s32 group, s32 animType);
extern void ExtPlayer_SetAnimGroupAnim(s32 group, s32 animType, LinkAnimationHeader* anim);
// Torso+arms from upperSkelAnime over the legs of whatever skelAnime is playing —
// vanilla's own split (Player_UpdateUpperBody). Defined in z_player.c BELOW the unity
// include of this module because the limb map it needs is a static down there.
extern void ExtPlayer_CopyUpperBody(PlayState* play, Player* this);

// ---------------------------------------------------------------------------
// Clip loading
// ---------------------------------------------------------------------------
// The whole clip at HALF its source length ("all anims x2"), root frozen. Cached by
// the resource layer, so calling this every frame costs a map lookup.
static LinkAnimationHeader* Trident_LoadHalf(const char* path) {
    LinkAnimationHeader* raw;
    s16 frames;

    if ((path == NULL) || !ResourceMgr_FileExists(path)) {
        return NULL;
    }
    raw = ResourceMgr_LoadPlayerAnimAsHeader(path);
    if (raw == NULL) {
        return NULL;
    }
    frames = (s16)(((f32)raw->common.frameCount / TRI_ANIM_SPEED) + 0.5f);
    if (frames < 2) {
        frames = 2; // a 1-frame clip finishes the instant it starts
    }
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(path, 1, frames);
}

// A sub-range of a clip at half speed.
static LinkAnimationHeader* Trident_LoadHalfRange(const char* path, s16 first, s16 last) {
    s16 frames;
    if ((path == NULL) || !ResourceMgr_FileExists(path) || (last < first)) {
        return NULL;
    }
    frames = (s16)(((f32)(last - first + 1) / TRI_ANIM_SPEED) + 0.5f);
    if (frames < 2) {
        frames = 2;
    }
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(path, 1, first, last, frames);
}

// ---------------------------------------------------------------------------
// Custom clip playback — FLIGHT ONLY.
//
// Everything else in this file rides OOT's own actions. The flight has no vanilla
// action to ride, so it is the one place PLAYER_STATE3_PAUSE_ACTION_FUNC is held and
// player->skelAnime is driven by hand: Trident_StartClip puts a clip on, and
// Trident_Advance steps it EXACTLY once per frame (twice would step straight over
// short frame windows). Same arrangement as Odolwa's moth flight, which is the
// shipped precedent for a flying human Link (boss_remains.cpp).
// ---------------------------------------------------------------------------
static void Trident_StartClip(PlayState* play, Player* player, const char* path, u8 loop) {
    LinkAnimationHeader* anim = Trident_LoadHalf(path);

    if (anim == NULL) {
        return;
    }
    // Set at EVERY clip start, not once at state entry — a vanilla path that
    // cleared it in between would otherwise advance our clip a second time.
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim),
                         loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -4.0f);
    sTri.timer = 0;
    sTri.prevFrame = -1.0f;
}

static s32 Trident_Advance(PlayState* play, Player* player) {
    return LinkAnimation_Update(play, &player->skelAnime);
}

// ---------------------------------------------------------------------------
// Blade quads while under PAUSE (the launch). The quads themselves are stamped by
// the draw path (z_player_lib.c func_800906D4) whenever meleeWeaponState > 0 and
// the row is a melee one, which is unaffected by PAUSE — this only opens/closes
// that window.
// ---------------------------------------------------------------------------
static void Trident_QuadOn(Player* player) {
    player->meleeWeaponAnimation = PLAYER_MWA_STAB_1H; // any melee row (< SPIN) keeps the stamp alive
    player->meleeWeaponQuads[0].base.atFlags |= AT_ON;
    player->meleeWeaponQuads[1].base.atFlags |= AT_ON;
    player->meleeWeaponQuads[0].info.toucher.damage = TRI_MELEE_DMG;
    player->meleeWeaponQuads[1].info.toucher.damage = TRI_MELEE_DMG;
    player->meleeWeaponState = 1;
    sTri.windowOpen = 1;
}

static void Trident_QuadOff(Player* player) {
    if (sTri.windowOpen) {
        player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
        player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
        player->meleeWeaponState = 0;
        sTri.windowOpen = 0;
    }
}

// ---------------------------------------------------------------------------
// Energy ball (full charge release) and light ball (flight B)
// ---------------------------------------------------------------------------
// ⚠️ POR QUÉ LA BOLA NO SALÍA NUNCA.
//
// El remate NO puede pagar con Magic_RequestChange. Para cuando corre, el consumo
// del PROPIO spin attack ya está en marcha: func_80837530 le pasa a En_M_Thunder un
// coste de 2 en los params (arg2 = 0x200, z_player.c:5184), el actor lo reserva en
// cuanto unk_858 >= 0.1 con MAGIC_CONSUME_WAIT_PREVIEW, y al soltar pone
// gSaveContext.magicState = MAGIC_STATE_CONSUME_SETUP. Y MAGIC_CONSUME_NOW se
// RECHAZA — con pitido de error incluido — siempre que magicState no sea IDLE
// (z_parameter.c:3209). Devolvía false, ballArmed se quedaba a 0, y el disparo no
// llegaba a ejecutarse jamás. Llevaba ahí desde que la bola salía en el último frame.
//
// El coste se pliega ahora sobre el consumo que YA está corriendo, bajándole el
// objetivo, y sólo se abre uno nuevo si no hay nada en vuelo. Y la bola sale pase lo
// que pase con la magia: llegar hasta aquí cuesta siete segundos de carga con el
// escudo bajado, y quedarse sin nada después de eso es justo el fallo que se arregla.
static void Trident_PayBallMagic(PlayState* play) {
    s16 cost = MAGIC_REQ(TRI_BALL_MAGIC_COST);

    if (!gSaveContext.isMagicAcquired) {
        return;
    }
    if (gSaveContext.magicState == MAGIC_STATE_IDLE) {
        if (gSaveContext.magic < cost) {
            cost = gSaveContext.magic;
        }
        if (cost > 0) {
            Magic_RequestChange(play, cost, MAGIC_CONSUME_NOW);
        }
        return;
    }
    gSaveContext.magicTarget -= cost;
    if (gSaveContext.magicTarget < 0) {
        gSaveContext.magicTarget = 0;
    }
}

static void Trident_ReleaseBall(PlayState* play, Player* player) {
    // From wherever the big-magic ball is resting (Trident_TickBigMagic keeps it on
    // Link through the release), so the projectile is THAT ball leaving.
    Vec3f pos = sTri.bmActive ? sTri.bmAnchor : player->meleeWeaponInfo[0].tip;

    // The MAX variant: against a boss the ball itself goes after it for
    // TRI_MAX_BALL_DMG as a super hit; against anything else it lives exactly one
    // frame, so it visibly breaks on the NEXT one and its burst is what spawns the
    // seekers ("si no es boss invocará 4 de esos trails desde la bola al romperse en
    // frame 65").
    TridentChargeBall_SpawnMax(play, &pos, TRI_MAX_BALL_DMG);
    // ⚠️ NO NA_SE_IT_SWORD_CHARGE. Ese id es CONTINUO: vanilla sólo lo toca como
    // `NA_SE_IT_SWORD_CHARGE - SFX_FLAG` (func_800F4254, code_800EC960.c:4589), es
    // decir, se pide cada frame y se calla cuando dejas de pedirlo. Lanzarlo una vez
    // desde aquí arrancaba un bucle que nadie volvía a pedir NI a parar — de ahí "el
    // sonido de esas partículas nunca se para". El de Ganondorf al lanzar su big
    // magic es un disparo único (z_boss_ganon.c:2410) y es el que toca.
    Sfx_PlaySfxCentered(NA_SE_EN_GANON_THROW_MASIC);
    Player_RequestRumble(player, 255, 25, 150, 0);

    // La bola de Link se acaba AQUÍ, no cuando termina la animación. Ya no está
    // delante de él: está volando. Antes seguía dibujada hasta el final de la fila
    // porque el colapso estaba condicionado a !ballPaid, y ballPaid se queda a 1 toda
    // la fila — o sea que la condición no se cumplía nunca.
    sTri.bmActive = 0;
    sTri.bmCircle = 0.0f;
    sTri.bmBall = 0.0f;
    sTri.bmAlpha = 0.0f;
    sTri.bmRays = 0;
}

// ---------------------------------------------------------------------------
// The level 1 / 2 release: Din's Fire's own display list, shrunk to cover only
// Link, tinted the charge's yellow-green. There is no hitbox here on purpose —
// vanilla's spin attack fires on the same frame and IS the hitbox, already small
// at level 1 and wide at level 2 (En_M_Thunder's targetScale). Drawn by
// Trident_Draw off sTri.domeTimer / sTri.domeScale.
// ---------------------------------------------------------------------------
static void Trident_ShellDome(PlayState* play, Player* player) {
    sTri.domeScale = (sTri.releaseLevel >= 2) ? TRI_DOME_SCALE_L2 : TRI_DOME_SCALE_L1;
    sTri.domeTimer = TRI_DOME_FRAMES;
    Sfx_PlaySfxCentered(NA_SE_IT_BOMB_EXPLOSION);
    Player_RequestRumble(player, (sTri.releaseLevel >= 2) ? 220 : 140, 15, 120, 0);
    (void)play;
}

static void Trident_ShootLight(PlayState* play, Player* player) {
    Vec3f pos = player->meleeWeaponInfo[0].tip; // the lance tip

    TridentChargeBall_SpawnLight(play, &pos);
    Sfx_PlaySfxCentered(NA_SE_EN_FANTOM_MASIC1);
}

// ---------------------------------------------------------------------------
// The ground chain
//
// ⚠️ POR QUÉ NO BASTABA CON RELLENAR LAS SEIS FILAS. OOT no encadena 1→2→3. Su
// combo es otra cosa completamente:
//
//   · func_80837818 elige la fila por el ÁNGULO DEL STICK, no por la posición en
//     una secuencia. Con el stick al centro y SIN Z devuelve RIGHT_SLASH, no
//     FORWARD_SLASH — así que el "tajo 1" ni siquiera era el que salía.
//   · La fila _COMBO sólo se alcanza al TERCER golpe seguido: func_80837948 lleva
//     un contador (unk_845) y hace `arg2 += 2` cuando llega a 3.
//
// O sea que repartir tres tajos entre seis filas da un orden que depende de hacia
// dónde empujes, y el tercero casi nunca sale. Gerudo ya se topó con esto y lo
// resolvió secuenciando la fila ella misma (GerudoMhr_NextComboMwa) y apagando la
// regla del +2 con GerudoMhr_OwnsComboRow. Esto es lo mismo para el trident.
// ---------------------------------------------------------------------------
// TRES pasos, y el orden no es el de las filas: 1 -> 3 -> 2 de los de antes.
// La estocada dejó de ser el cuarto paso; vuelve a ser sólo lo que sale al empujar
// adelante con B, que es donde vanilla la pone.
static const s32 sTridentComboRows[] = {
    PLAYER_MWA_FORWARD_SLASH_1H, // 1  StationarySingleGunlanceThrust     (el 1 de antes)
    PLAYER_MWA_RIGHT_SLASH_1H,   // 2  ForwardDoubleWeaponTransition      (el 3 de antes)
    PLAYER_MWA_FORWARD_COMBO_1H, // 3  ForwardRisingMultiHitAerialThrust  (el 2 de antes)
};
#define TRIDENT_COMBO_STEPS ((s32)(sizeof(sTridentComboRows) / sizeof(sTridentComboRows[0])))

// Frames de silencio que cierran la cadena. Como en Gerudo: tiene que sobrevivir a
// un swing entero más su recovery, o el segundo golpe reinicia en el tajo 1.
#define TRIDENT_COMBO_RESET_FRAMES 40

static s32 sTridentComboStep = 0;
static s32 sTridentComboIdle = 0;

// Envejece la cadena. Llamada cada frame desde Trident_Behavior.
static void Trident_TickCombo(void) {
    if (sTridentComboStep != 0) {
        if (++sTridentComboIdle >= TRIDENT_COMBO_RESET_FRAMES) {
            sTridentComboStep = 0;
            sTridentComboIdle = 0;
        }
    }
}

// True mientras la cadena manda sobre la fila, para que la regla del "+2 al tercer
// golpe" de OOT se aparte: nos empujaría fuera de la secuencia.
u8 Trident_OwnsComboRow(Player* player) {
    return (gExtEquipState.currentExtSword == 3) && ExtEquip_IsEnabled() && (player != NULL) &&
           (Player_GetMeleeWeaponHeld(player) != 0);
}

// Should this swing MORPH into place instead of snapping?
//
// func_80837948 starts every swing with Player_AnimPlayOnceAdjusted, which is a hard
// cut: frame 0 of the new clip replaces whatever pose Link was in. Between three
// gunlance slashes that reads as a jump, because each one ends somewhere the next
// does not begin. Vanilla already ships the fix — Player_AnimChangeOnceMorphAdjusted
// is the same call with a -6 morph — so the chain rows just ask for that one instead
// and Link travels from the end of one into the start of the next.
//
// Only the CHAIN rows. The charge releases, the jump slash and the stab keep their
// hard start: those are single moves that begin from a settled pose, and a morph
// there only softens the impact. Skijer's NEI
u8 Trident_MorphsRow(Player* player, s32 mwa) {
    s32 i;

    if (!Trident_OwnsComboRow(player)) {
        return 0;
    }
    for (i = 0; i < TRIDENT_COMBO_STEPS; i++) {
        // The table holds the 1H rows; the 2H twin is the next id up (…_1H, …_2H).
        if ((mwa == sTridentComboRows[i]) || (mwa == (sTridentComboRows[i] + 1))) {
            return 1;
        }
    }
    return 0;
}

// La fila que toca. Devuelve `requested` sin tocar para todo lo que no es la
// cadena de suelo, igual que hace Gerudo.
s32 Trident_NextComboMwa(Player* player, s32 requested) {
    s32 row;

    if (!Trident_OwnsComboRow(player)) {
        return requested;
    }
    // ── Las cargas ────────────────────────────────────────────────────────────
    // Vanilla sólo distingue DOS remates y su umbral es 0.85 para ambos: ahí el glow
    // se pone naranja y ahí func_80844BE4 salta a BIG_SPIN. Nosotros queremos tres,
    // así que BIG_SPIN se reserva para la carga LLENA y el nivel 2 vuelve a la fila
    // de SPIN_ATTACK; lo único que cambia entre nivel 1 y 2 es el radio de la cúpula,
    // que sale de sTri.releaseLevel. El nivel se lee del que midió Trident_TickCharge
    // este frame, no de unk_858 — para cuando esto corre, la acción de carga ya puede
    // haberlo puesto a cero.
    if ((requested >= PLAYER_MWA_SPIN_ATTACK_1H) && (requested <= PLAYER_MWA_BIG_SPIN_2H)) {
        s32 lvl = (sTri.chargeLvl > 0) ? sTri.chargeLvl : 1;
        if ((requested >= PLAYER_MWA_BIG_SPIN_1H) && (lvl < 3)) {
            requested = PLAYER_MWA_SPIN_ATTACK_1H + (Player_HoldsTwoHandedWeapon(player) ? 1 : 0);
        }
        sTri.releaseLevel = (s8)lvl;
        return requested;
    }
    if ((requested >= PLAYER_MWA_FLIPSLASH_START) && (requested <= PLAYER_MWA_JUMPSLASH_FINISH)) {
        return requested;
    }
    // ⚠️ La estocada sólo se sirve con la cadena PARADA.
    //
    // Dejarla pasar siempre era el bug de "al intentar interrumpir el combo siempre
    // quiere hacer turn thrust": func_80837818 devuelve STAB_1H en cuanto hay
    // lock-on y el stick se mueve adelante, y eso es EXACTAMENTE lo que haces al
    // intentar reorientar o cortar el combo. Así que cualquier corrección de rumbo
    // a mitad de cadena se comía el tajo siguiente y salía la estocada.
    //
    // Con la cadena viva mandan los tajos y el stick sólo gira a Link; la estocada
    // vuelve a estar disponible en cuanto la cadena caduca (TRIDENT_COMBO_RESET_FRAMES).
    if ((requested == PLAYER_MWA_STAB_1H) || (requested == PLAYER_MWA_STAB_2H) ||
        (requested == PLAYER_MWA_STAB_COMBO_1H) || (requested == PLAYER_MWA_STAB_COMBO_2H)) {
        if (sTridentComboStep == 0) {
            return requested;
        }
        // Cae a la cadena: el paso que tocaba, no la estocada.
    }

    if (sTridentComboStep >= TRIDENT_COMBO_STEPS) {
        sTridentComboStep = 0;
    }
    row = sTridentComboRows[sTridentComboStep];
    sTridentComboStep = (sTridentComboStep + 1) % TRIDENT_COMBO_STEPS;
    sTridentComboIdle = 0;

    // Las filas de la tabla son las 1H; si Link lleva un arma a dos manos hay que
    // subir a su gemela, que es como OOT indexa (…_1H, …_2H, …_COMBO_1H, …).
    if (Player_HoldsTwoHandedWeapon(player)) {
        row++;
    }
    return row;
}

// Which binding row is currently playing, or -1.
//
// Matched on the ANIMATION POINTER, not on player->meleeWeaponAnimation. There is
// no "is attacking" state flag in OOT (checked: PLAYER_STATE1_* has none), and
// meleeWeaponAnimation keeps its last value long after the swing is over, so any
// marker keyed off it would keep firing while Link stands there. The pointer test
// is exact and self-limiting: it is true only while OOT is actually playing the
// clip this row installed.
static s32 Trident_CurrentRow(Player* player) {
    void* playing = (void*)player->skelAnime.animation;
    if (playing == NULL) {
        return -1;
    }
    for (size_t i = 0; i < TRIDENT_MELEE_BINDING_COUNT; i++) {
        if ((void*)sTridentAnimTables.installedMelee[i] == playing) {
            return (s32)i;
        }
    }
    return -1;
}

// A source-frame marker in the CURRENT row's installed frame space. Pure
// arithmetic off the lengths install already measured — no per-frame resource
// lookup, and no second copy of the rescale rule.
static f32 Trident_RowFrame(s32 row, s16 srcFrame) {
    const TridentMeleeBinding* b;
    s16 base;
    s16 srcLen;
    s16 outLen;

    if (row < 0) {
        return 0.0f;
    }
    b = &sTridentMeleeBindings[row];
    srcLen = sTridentAnimTables.srcLen[row];
    outLen = sTridentAnimTables.outLen[row];
    if ((srcLen < 1) || (outLen < 1)) {
        return 0.0f;
    }
    base = (b->srcStart > 0) ? b->srcStart : 0;
    return (f32)(((s32)(srcFrame - base) * (s32)outLen) / (s32)srcLen);
}

// Did this frame's step cross `mark`? A point test drops the mark whenever the
// clip advances by more than one frame, which resampled rows routinely do — the
// same reason MmForm_MhrWindowCrossed exists.
static u8 Trident_Crossed(f32 prev, f32 cur, f32 mark) {
    return (cur >= mark) && (prev < mark);
}

// ---------------------------------------------------------------------------
// ATTACK VOLUMES
//
// The lance's own blade quads follow the model (ExtEquip_TridentTrailBegin) and are
// a thin line along the shaft — fine for a thrust, useless for saying "this swing
// covers Link's left" or "this one sweeps everything in front". So each step of the
// chain gets a real box of its own, on top.
//
// Everything is written in LINK'S OWN FRAME and then turned by shape.rot.y, which is
// what makes each volume sit where the attack is pointing ("en dirección de la
// rotación del ataque"):
//     right  +X to Link's right      up  +Y      fwd  +Z where he faces
// `pitch` lays the box down toward the floor, for the sweep that finishes the chain.
//
// There is also a shield box on Link's RIGHT that stays live for every frame of the
// chain: an AC quad set up like the vanilla shield (metal, hard, bounces enemy
// attacks), so comboing does not leave that flank open.
// Skijer's NEI
// ---------------------------------------------------------------------------
typedef struct {
    f32 right;
    f32 up;
    f32 fwd;
    f32 halfW; // across, along Link's right axis
    f32 halfH; // along the up axis, after `pitch` tilts it
    f32 pitch; // radians; 0 = upright, >0 = laid forward and down
} TridentQuadBox;

// Step 1 — in front and a little high.
static const TridentQuadBox sTriBoxSlash1 = { 0.0f, 52.0f, 42.0f, 30.0f, 26.0f, 0.0f };
// Step 2 — out to the LEFT, covering that side of Link.
static const TridentQuadBox sTriBoxSlash3 = { -42.0f, 40.0f, 18.0f, 34.0f, 30.0f, 0.0f };
// Step 3 — straight ahead, bigger than the first, angled down, covering the whole front.
static const TridentQuadBox sTriBoxSlash2 = { 0.0f, 38.0f, 52.0f, 56.0f, 46.0f, 0.75f };
// The thrust — in front, narrow and long, which is what a thrust is.
static const TridentQuadBox sTriBoxStab = { 0.0f, 38.0f, 58.0f, 20.0f, 22.0f, 0.0f };
// The shield, on Link's right, for the whole chain.
static const TridentQuadBox sTriBoxGuard = { 30.0f, 40.0f, 10.0f, 22.0f, 30.0f, 0.0f };

static ColliderQuad sTriAtkQuad;
static ColliderQuad sTriGuardQuad;
static u8 sTriQuadsInited = 0;

static ColliderQuadInit sTriAtkQuadInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK2,
        { DMG_SLASH_MASTER, 0x00, TRI_MELEE_DMG },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

// Copied from vanilla's own shield quad (D_808546A0, z_player.c:12421) so it bounces
// exactly what a raised shield bounces.
static ColliderQuadInit sTriGuardQuadInit = {
    {
        COLTYPE_METAL,
        AT_NONE,
        AC_ON | AC_HARD | AC_TYPE_ENEMY,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000000, 0x00, 0x00 },
        { 0xDFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static void Trident_InitQuads(PlayState* play, Player* player) {
    if (sTriQuadsInited) {
        return;
    }
    Collider_InitQuad(play, &sTriAtkQuad);
    Collider_SetQuad(play, &sTriAtkQuad, &player->actor, &sTriAtkQuadInit);
    Collider_InitQuad(play, &sTriGuardQuad);
    Collider_SetQuad(play, &sTriGuardQuad, &player->actor, &sTriGuardQuadInit);
    sTriQuadsInited = 1;
}

static void Trident_PlaceQuad(ColliderQuad* quad, Player* player, const TridentQuadBox* box) {
    // Corner signs: 0 and 1 are the top edge, 3 and 2 the bottom one — the winding
    // Collider_SetQuadVertices expects (see item_switchhook.c for the same shape).
    static const f32 sCornerX[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
    static const f32 sCornerY[4] = { 1.0f, 1.0f, -1.0f, -1.0f };
    Vec3f v[4];
    f32 sinY = Math_SinS(player->actor.shape.rot.y);
    f32 cosY = Math_CosS(player->actor.shape.rot.y);
    f32 upY = cosf(box->pitch);
    f32 upZ = sinf(box->pitch);
    s32 i;

    for (i = 0; i < 4; i++) {
        f32 lx = box->right + (sCornerX[i] * box->halfW);
        f32 ly = box->up + (sCornerY[i] * box->halfH * upY);
        f32 lz = box->fwd + (sCornerY[i] * box->halfH * upZ);

        v[i].x = player->actor.world.pos.x + (lx * cosY) + (lz * sinY);
        v[i].y = player->actor.world.pos.y + ly;
        v[i].z = player->actor.world.pos.z + (lz * cosY) - (lx * sinY);
    }
    Collider_SetQuadVertices(quad, &v[0], &v[1], &v[2], &v[3]);
}

// Which box belongs to this row, or NULL for a row that has none.
static const TridentQuadBox* Trident_BoxForSemantic(const char* sem) {
    if (strcmp(sem, "slash1") == 0) {
        return &sTriBoxSlash1;
    }
    if (strcmp(sem, "slash3") == 0) {
        return &sTriBoxSlash3; // the chain's SECOND step
    }
    if (strcmp(sem, "slash2") == 0) {
        return &sTriBoxSlash2; // the chain's LAST step
    }
    if (strcmp(sem, "stab") == 0) {
        return &sTriBoxStab;
    }
    return NULL;
}

// Per frame, from Trident_TickMelee. `cur` is the row's own frame.
static void Trident_TickQuads(PlayState* play, Player* player, s32 row, const char* sem, f32 cur) {
    const TridentMeleeBinding* b = &sTridentMeleeBindings[row];
    const TridentQuadBox* box = Trident_BoxForSemantic(sem);
    f32 from;
    f32 to;

    if (box == NULL) {
        return;
    }
    Trident_InitQuads(play, player);

    // The same window the row already declares for the blade, so the box and the
    // lance agree about when the swing is dangerous. -1 = the whole row, 0 = to the end.
    from = (b->hitStartSrc > 0) ? Trident_RowFrame(row, b->hitStartSrc) : 0.0f;
    to = (b->hitEndSrc > 0) ? Trident_RowFrame(row, b->hitEndSrc) : (f32)sTridentAnimTables.outLen[row];

    if ((cur >= from) && (cur <= to)) {
        Trident_PlaceQuad(&sTriAtkQuad, player, box);
        CollisionCheck_SetAT(play, &play->colChkCtx, &sTriAtkQuad.base);
        if (sTriAtkQuad.base.atFlags & AT_HIT) {
            sTriAtkQuad.base.atFlags &= ~AT_HIT;
        }
    }

    // The shield stays up for every frame of the chain, window or not.
    if (Trident_BoxForSemantic(sem) != &sTriBoxStab) {
        Trident_PlaceQuad(&sTriGuardQuad, player, &sTriBoxGuard);
        CollisionCheck_SetAC(play, &play->colChkCtx, &sTriGuardQuad.base);
    }
}

static void Trident_TickMelee(PlayState* play, Player* player) {
    s32 row;
    f32 cur;
    f32 prev;
    const char* sem;

    row = Trident_CurrentRow(player);
    if (row < 0) {
        if (sTri.meleeRow >= 0) {
            // El remate de carga máxima corre a x3 desde su frame 25; la fila que
            // venga después no tiene por qué heredarlo.
            player->skelAnime.playSpeed = 1.0f;
        }
        sTri.meleeRow = -1;
        sTri.meleePrevFrame = 0.0f;
        sTri.ballPaid = 0; // fuera de un remate de carga: rearma el cobro
        sTri.ballArmed = 0;
        return;
    }

    cur = player->skelAnime.curFrame;
    prev = sTri.meleePrevFrame;

    if (row != sTri.meleeRow) {
        // New row: start the crossing test from before frame 0 so a marker sitting
        // on the very first frame still counts as crossed.
        sTri.meleeRow = (s8)row;
        prev = -1.0f;
    } else if (prev > cur) {
        prev = cur; // looped/restarted: do not span the wrap
    }
    sTri.meleePrevFrame = cur;
    sem = sTridentMeleeBindings[row].semantic;

    // The per-step attack box and the shield on Link's right.
    Trident_TickQuads(play, player, row, sem, cur);

    // ---- stab: keep the lunge alive until source frame 25 ---------------------
    // OOT bleeds linearVelocity off during a swing. The gunlance thrust is supposed
    // to carry its momentum through the windup, so it is re-asserted until the
    // mark; the quad only arms at 30, which the table already handles.
    if (strcmp(sem, "stab") == 0) {
        if (cur < Trident_RowFrame(row, 25)) {
            if (player->linearVelocity < TRI_STAB_LUNGE_SPEED) {
                player->linearVelocity = TRI_STAB_LUNGE_SPEED;
            }
        }
        return;
    }

    // ---- B mantenida: la bola de energía sustituye al spin attack -------------
    //
    // Nada intercepta B aquí, y es a propósito. Vanilla YA hace todo el trabajo:
    // detecta el hold, corre el temporizador de carga, dibuja el glow azul y
    // reproduce nuestras poses (las instaló Trident_InstallChargeAnims). Robar B
    // para reimplementar eso es justo lo que produjo la mezcla espada/trident en su
    // día. Lo único que cambia es QUÉ pasa al soltar: en vez del giro, la bola.
    //
    // Nivel 1 (carga corta) se queda como el remate de shelling y no gasta magia;
    // sólo la carga llena invoca la bola, que es la que cuesta las 24. El remate
    // lleva el recoil pedido: -10 de velocidad que se apaga solo porque estás quieto.
    // Nivel 3 — la carga llena. 191 frames, y los números son los del clip.
    if (strcmp(sem, "chargeLvl2") == 0) {
        // 0..24: la túnica se pone dorada y Link es intocable. INTANGIBILIDAD, no
        // invulnerabilidad: la segunda para el daño pero no el empujón, y un empujón
        // aquí le arrancaría el remate a medias. Se re-arma cada frame porque el
        // contador baja solo.
        if (cur <= (f32)TRI_MAX_IMMUNE_LAST) {
            sTri.goldTimer = 2;
            Player_SetIntangibility(player, 20);
        }
        // 25 en adelante, x3. El resto de la fila queda a velocidad normal porque el
        // siguiente LinkAnimation_Change (cualquier fila, incluido el idle) reinstala
        // playSpeed = 1.0 — aun así se restaura a mano al salir de la fila.
        if ((cur >= (f32)TRI_MAX_FAST_FROM) && (player->skelAnime.playSpeed < TRI_MAX_FAST_SPEED)) {
            player->skelAnime.playSpeed = TRI_MAX_FAST_SPEED;
        }
        if (Trident_Crossed(prev, cur, 0.0f) && !sTri.ballPaid) {
            // Se cobra al empezar; la bola sale en el frame 64. Hasta entonces sigue
            // pegada a Link (Trident_TickBigMagic la trae de la lanza levantada al
            // frente), que es el "debe quedarse enfrente de Link hasta terminar".
            Trident_PayBallMagic(play);
            sTri.ballArmed = 1;
            sTri.ballPaid = 1;
            player->linearVelocity = -10.0f;
            player->yaw = player->actor.shape.rot.y;
        }
        // Frame 64 exacto — con playSpeed 3 el frame se salta, así que el test es de
        // CRUCE y no de igualdad (para esto existe Trident_Crossed). La marca se
        // recorta al final de la fila para que un clip más corto de lo esperado
        // dispare igual en vez de tragarse el remate en silencio.
        if (sTri.ballArmed) {
            f32 last = (f32)sTridentAnimTables.outLen[row] - 1.0f;
            f32 mark = (f32)TRI_MAX_BURST_FRAME;
            if ((last >= 1.0f) && (mark > (last - 1.0f))) {
                mark = last - 1.0f;
            }
            if (Trident_Crossed(prev, cur, mark)) {
                sTri.ballArmed = 0;
                Trident_ReleaseBall(play, player);
            }
        }
        return;
    }

    // Niveles 1 y 2 — mismo clip, distinto radio. El golpe NO es nuestro: es el
    // spin attack de vanilla, que ya sale pequeño en el nivel 1 y grande en el 2
    // (EnMThunder targetScale 2/4 vs 4/8). Lo que ponemos es la cúpula.
    if (strcmp(sem, "chargeLvl1") == 0) {
        sTri.ballPaid = 0;
        if (Trident_Crossed(prev, cur, 0.0f)) {
            Trident_ShellDome(play, player);
        }
        return;
    }

    // ---- jump: the landing ---------------------------------------------------
    // START is airborne and needs nothing here (PlayOnce holds its last frame until
    // touchdown, which is the "se queda en loop del último frame"). FINISH plays on
    // the ground and lunges a bit — "en jumpslash te hará avanzar más". It used to
    // detonate a ground pound on this frame; that went with the rest of the
    // explosions. The row's own blade quad is armed frame 0 to the end, so the
    // landing still hits — it just hits with the lance instead of a blast.
    if (strcmp(sem, "jumpFinish") == 0) {
        if (Trident_Crossed(prev, cur, 0.0f)) {
            Player_RequestRumble(player, 255, 20, 150, 0);
            Player_RequestQuake(play, 27767, 5, 12);
            player->linearVelocity = TRI_JUMP_FINISH_LUNGE;
            player->yaw = player->actor.shape.rot.y;
        }
        return;
    }
}

// Player_HandleExitsAndVoids asks this to skip the void-out while Link is flying.
// The guard dash is NOT flying: it runs on the floor and must void out like anything
// else, or it would be a way to cross a pit.
u8 Trident_IsFlying(void) {
    return TRI_IS_FLYING(sTri.state) ? 1 : 0;
}

// The first 24 frames of the max-charge release: the tunic goes gold while Link is
// untouchable, so the immunity is readable instead of invisible. Asked by
// z_player_lib.c's tunic-colour block, next to the ext recolor tunics.
u8 Trident_GoldenArmor(void) {
    return (sTri.goldTimer > 0) ? 1 : 0;
}

// The jump itself: called from func_8083BA90 right after vanilla sets the launch
// velocities, next to GerudoMhr_AdjustJumpSlash. "Saltará muy poquito, todos sus hops
// se verán reducidos en fighter" — the gunlance is heavy, so the hop is short and
// low. Vanilla launches at xz 5.0 / y 5.0 (3.0 / 4.5 from a stand).
void Trident_AdjustJumpSlash(Player* player, s32 mwa) {
    // (Trident_Active is defined further down; same test inline.)
    if (!ExtEquip_IsEnabled() || (gExtEquipState.currentExtSword != 3) || (player == NULL)) {
        return;
    }
    if ((mwa != PLAYER_MWA_JUMPSLASH_START) && (mwa != PLAYER_MWA_FLIPSLASH_START)) {
        return;
    }
    if (Player_GetMeleeWeaponHeld(player) == 0) {
        return;
    }
    player->linearVelocity *= TRI_JUMP_XZ_MUL;
    player->actor.velocity.y *= TRI_JUMP_Y_MUL;
}

// ---------------------------------------------------------------------------
// Gating
// ---------------------------------------------------------------------------
static u8 Trident_Active(void) {
    return ExtEquip_IsEnabled() && (gExtEquipState.currentExtSword == 3);
}

// True while the player is in a state where a custom sword action may take over.
static u8 Trident_CanAct(Player* player) {
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_CLIMBING_LEDGE |
                               PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_IN_WATER)) {
        return 0;
    }
    if (player->stateFlags2 & PLAYER_STATE2_DIVING) {
        return 0;
    }
    // Only while actually wielding a melee weapon — the trident replaces SWORD
    // actions, so with the weapon put away everything stays vanilla.
    return Player_GetMeleeWeaponHeld(player) != 0;
}

static u8 Trident_IsSwordIA(s8 ia) {
    return (ia == PLAYER_IA_SWORD_MASTER) || (ia == PLAYER_IA_SWORD_KOKIRI) || (ia == PLAYER_IA_SWORD_BIGGORON);
}

// ---------------------------------------------------------------------------
// The guard — vanilla's shield action, and now vanilla's POSES too.
//
// R is OOT's own shield, unchanged and un-reskinned: Mirror forced by
// ExtEquip_SetSlot (Divine or Mirror, once), block handled by the vanilla shield quad, and the
// raise/hold/lower clips are Link's own ("haz que las poses de shield use las
// vanilla"). The gunlance guard idle that used to be served through
// VB_PLAYER_ANIM_SITE_SHIELD_RAISE / _LOOP is gone from those sites.
//
// R+B is no longer the crouch stab either — it is the guard dash (Trident_TickDash).
// Trident_GetGuardStabAnim stays hooked as the fallback for the frames where the dash
// cannot start, so what comes out is still a gunlance clip and not Link's sword.
// Nothing in THIS block holds PAUSE or drives skelAnime.
// ---------------------------------------------------------------------------

// R+B from the guard. Called from func_808428D8 in place of link_normal_defense_kiru:
// slash 1 with the shield still up. Only the FALLBACK now — R+B is the guard dash.
LinkAnimationHeader* Trident_GetGuardStabAnim(Player* player) {
    if (!Trident_Active() || (player == NULL) || (Player_GetMeleeWeaponHeld(player) == 0)) {
        return NULL;
    }
    return Trident_LoadHalf(TRIP_GUARD_STAB);
}

// ---------------------------------------------------------------------------
// Draw / sheathe — vanilla item change, upper body. Called from
// Player_StartChangingHeldItem once the vanilla clip has been picked. Both are
// forced to play FORWARD (vanilla plays some pairs backwards); the sheath has to
// run to its end before the change completes, which is vanilla's own rule.
// ---------------------------------------------------------------------------
LinkAnimationHeader* Trident_GetItemChangeAnim(Player* player, s8 newIA, s32* itemChangeType) {
    u8 fromSword;
    u8 toSword;

    if (!Trident_Active() || (player == NULL) || (itemChangeType == NULL)) {
        return NULL;
    }
    fromSword = Trident_IsSwordIA(player->heldItemAction);
    toSword = Trident_IsSwordIA(newIA);
    if (toSword && !fromSword) {
        *itemChangeType = ABS(*itemChangeType);
        return Trident_LoadHalf(TRIP_UNSHEATH);
    }
    if (fromSword && !toSword) {
        *itemChangeType = ABS(*itemChangeType);
        return Trident_LoadHalf(TRIP_SHEATH);
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// Heavy locomotion — while the weapon is DRAWN only.
//
// Walk/run rows: ForwardWeaponRun resampled to OOT's 29-frame stride (the walk
// and run blend at fixed frame ratios, see ResourceManagerHelpers.cpp), written
// to columns 0/1/3 because Player_SetModelGroup demotes a shieldless fighter to
// column 0. Same clip in both groups so the blend can never go out of phase.
//
// "Física real de iron boots": the iron REGs, applied every frame through
// Player_SetBootData with currentBoots swapped for the call. Only the REGs — the
// model stays whatever boots are equipped, and sinking never comes up because the
// weapon sheathes itself on entering water ("solo que se envaina al agua").
// ---------------------------------------------------------------------------
// ALL four anim types, not 0/1/3. Column 2 was left vanilla, so any state that put
// modelAnimType at 2 swapped the legs to Link's own run mid-stride — which reads
// exactly like the cycle breaking. Skijer's NEI
static const s32 sTridentLocoCols[] = { 0, 1, 2, 3 };
#define TRIDENT_LOCO_COL_COUNT ((s32)(sizeof(sTridentLocoCols) / sizeof(sTridentLocoCols[0])))

// The run cycle at its NATIVE length, for anything that plays it as a clip (the guard
// dash). The locomotion table wants the 29-frame resample instead — see TRIP_WALK.
static LinkAnimationHeader* Trident_LoadLoco(void) {
    if (!ResourceMgr_FileExists(TRIP_WALK)) {
        return NULL;
    }
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlace(TRIP_WALK, 1);
}

static void Trident_InstallLoco(void) {
    LinkAnimationHeader* walk;
    LinkAnimationHeader* run;
    s32 i;

    if (sTri.locoInstalled) {
        return;
    }
    // ⚠️ DOS LONGITUDES DISTINTAS DEL MISMO CLIP, y esto es lo que llevaba roto.
    //
    // Los dos grupos comparten la fase unk_868 (0..29) pero NO la muestrean igual:
    //   walk -> LinkAnimation_LoadToJoint(..., unk_868)              -> 29 frames
    //   run  -> LinkAnimation_LoadToJoint(..., unk_868 * (20/29))    -> 20 frames
    //                                            (z_player.c:10542)
    // Meter un clip de 29 en el grupo de carrera hace que sólo se vean sus frames
    // 0..20: el ciclo se corta a un tercio del final y vuelve a empezar. Eso es el
    // "al terminar el loop de una empieza la otra".
    walk =
        ResourceMgr_FileExists(TRIP_WALK) ? ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(TRIP_WALK, 1, 29) : NULL;
    run =
        ResourceMgr_FileExists(TRIP_WALK) ? ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(TRIP_WALK, 1, 20) : NULL;
    for (i = 0; i < TRIDENT_LOCO_COL_COUNT; i++) {
        s32 col = sTridentLocoCols[i];
        sTri.savedWalk[col] = ExtPlayer_GetAnimGroupAnim(PLAYER_ANIMGROUP_walk, col);
        sTri.savedRun[col] = ExtPlayer_GetAnimGroupAnim(PLAYER_ANIMGROUP_run, col);
        // Los DOS grupos, cada uno con su longitud. Dejar el de andar en vanilla
        // (como estuvo un momento) sólo cambiaba un corte por otro: el paso de Link
        // al andar y el del gunlance al correr, alternándose en la mezcla.
        if (walk != NULL) {
            ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_walk, col, walk);
        }
        if (run != NULL) {
            ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_run, col, run);
        }
    }
    sTri.locoInstalled = 1;
}

static void Trident_RestoreLoco(void) {
    s32 i;

    if (!sTri.locoInstalled) {
        return;
    }
    for (i = 0; i < TRIDENT_LOCO_COL_COUNT; i++) {
        s32 col = sTridentLocoCols[i];
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_walk, col, sTri.savedWalk[col]);
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_run, col, sTri.savedRun[col]);
    }
    sTri.locoInstalled = 0;
}

static void Trident_TickLoco(PlayState* play, Player* player, u8 drawn) {
    if (drawn) {
        u8 saved;

        Trident_InstallLoco();
        // Every frame: Player_SetBootData is also called by room changes and by
        // the hover/iron paths, and each of those would put the real boots back.
        saved = player->currentBoots;
        player->currentBoots = PLAYER_BOOTS_IRON;
        Player_SetBootData(play, player);
        player->currentBoots = saved;
        sTri.heavyBoots = 1;
    } else {
        Trident_RestoreLoco();
        if (sTri.heavyBoots) {
            sTri.heavyBoots = 0;
            Player_SetBootData(play, player); // the real boots' REGs again
        }
    }
}

// Entering water puts the weapon away — vanilla's item change, so it plays the
// sheath clip and finishes on its own; the heavy REGs go with it next frame.
static void Trident_TickWater(PlayState* play, Player* player) {
    if ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) && (Player_GetMeleeWeaponHeld(player) != 0) &&
        !(player->stateFlags1 & PLAYER_STATE1_START_CHANGING_HELD_ITEM) &&
        (player->itemAction == player->heldItemAction)) {
        Player_UseItem(play, player, ITEM_NONE);
    }
}

// ---------------------------------------------------------------------------
// The charge — vanilla's, watched.
//
// Vanilla's hold-B charge only STARTS if unk_844 (8 at swing start, -1 per frame)
// is exactly 1 the frame after the swing ends — true for Link's own 7-frame swings
// and for nothing else. Ours are 14-26 frames, so the counter is pinned to 3 for as
// long as a trident swing runs with B held: it reads 2 on the swing's last frame,
// 1 on the recovery's first, and Player_ActionHandler_8 starts the charge. Same
// fix, same reason, as GerudoMhr_HoldsChargeWindow. This is why "el charging de B
// nunca se activa" — the swings were simply too long for vanilla's window.
//
// ⚠️ PERO EL PIN NO PUEDE MIRAR SÓLO SI B ESTÁ PULSADA ESTE FRAME. Ese era el bug de
// "al hacer mashing en lugar de hacer el combo entra en charge": machacando B, el
// botón ESTÁ bajado en los frames de pulsación, así que el pin se renovaba, y en
// cuanto un hueco del mashing coincidía con el final de la fila el contador llegaba a
// 1 con B bajada y Player_ActionHandler_8 se llevaba el turno — el siguiente paso del
// combo se convertía en una carga.
//
// Y es también de dónde salía el "delay al iniciar el charge", por el otro lado del
// mismo mecanismo: cada vez que sueltas B con unk_844 > 0, func_8083C50C lo NIEGA
// (z_player.c:8127) y el contador tiene que volver a subir desde negativo antes de
// que nada pueda cargar. Machacar es soltar B muchas veces, así que dejaba el
// contador enterrado en negativo y la primera pulsación mantenida se comía esa
// remontada.
//
// La distinción es mantener vs. tocar: el pin sólo entra tras TRI_B_HOLD_MIN frames
// con B bajada SIN soltarla. Un toque de mashing dura 1-3 frames y no llega; una
// pulsación mantenida sí, y al pinear sobrescribe cualquier valor negativo que el
// mashing hubiera dejado, con lo que la carga arranca en cuanto acaba el tajo.
// ---------------------------------------------------------------------------
#define TRI_B_HOLD_MIN 5

u8 Trident_HoldsChargeWindow(Player* player) {
    s32 row;
    const char* sem;

    if (!Trident_Active() || (player == NULL) || (gPlayState == NULL)) {
        sTri.bHold = 0;
        return 0;
    }
    // The guard dash borrows the charge action as its host: it must never be handed a
    // real charge window on top, or holding R+B would fill the bar behind the run.
    if (sTri.state != TRI_IDLE) {
        sTri.bHold = 0;
        return 0;
    }

    // Called every frame from Player_UpdateCommon, which is what makes this the right
    // place to age the counter. (The || in the caller short-circuits past us only when
    // the Gerudo blades answer first, and those cannot be equipped at the same time.)
    if (CHECK_BTN_ALL(gPlayState->state.input[0].cur.button, BTN_B)) {
        if (sTri.bHold < TRI_B_HOLD_MIN) {
            sTri.bHold++;
        }
    } else {
        sTri.bHold = 0;
    }
    if (sTri.bHold < TRI_B_HOLD_MIN) {
        return 0;
    }

    row = Trident_CurrentRow(player);
    if (row < 0) {
        return 0;
    }
    sem = sTridentMeleeBindings[row].semantic;
    if ((strcmp(sem, "chargeLvl1") == 0) || (strcmp(sem, "chargeLvl2") == 0)) {
        return 0; // the release itself must not re-arm a charge
    }
    return 1;
}

// Fill throttle + the full-charge stance swap.
static void Trident_TickCharge(PlayState* play, Player* player) {
    // The guard dash sets CHARGING_SPIN_ATTACK itself to hold its host action open.
    // None of the charge machinery may run off that: no stance swap, no level, no
    // shield, no big-magic ball. Trident_Behavior already skips this while dashing;
    // the test is repeated here so nothing can reach it by another road.
    if (sTri.state != TRI_IDLE) {
        return;
    }
    if (player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) {
        s32 lvl;

        // Vanilla stepped unk_858 by 0.02 this frame (func_80844E3C, inside the
        // action func); pull it back so the effective rate is TRI_CHARGE_RATE.
        if ((sTri.chargePrev >= 0.0f) && (player->unk_858 > sTri.chargePrev + TRI_CHARGE_RATE)) {
            player->unk_858 = sTri.chargePrev + TRI_CHARGE_RATE;
        }
        sTri.chargePrev = player->unk_858;

        if (player->unk_858 >= TRI_CHARGE_L3) {
            if (sTri.fullHold < TRI_CHARGE_L3_HOLD) {
                sTri.fullHold++;
            }
        } else {
            sTri.fullHold = 0;
        }
        lvl = (sTri.fullHold >= TRI_CHARGE_L3_HOLD) ? 3 : ((player->unk_858 >= TRI_CHARGE_L2) ? 2 : 1);
        sTri.chargeLvl = (s8)lvl;
        Trident_SetChargeLevel(play, player, lvl - 1);

        // Armado mientras cargas: si la carga se rompe Y es porque te han dado, el
        // frame siguiente sale el traspié del gunlance.
        sTri.hurtPending = 1;

        // The charge is a GUARD stance (the clips are guard idles), so the shield
        // is up while it runs — "debe protegerte si tienes escudo". Exactly what
        // vanilla's own walking-shield upper action does every frame (func_80834B5C):
        // the flag plus the hand models, and PostLimbDraw registers the shield quad
        // off the RH_SHIELD hand. A block goes through the vanilla shield-block
        // branch, which switches the action — so the hit is stopped AND the charge is
        // lost, the trade the user chose ("bloqueo, pero el golpe cancela la carga").
        if ((player->currentShield != PLAYER_SHIELD_NONE) && !Player_HoldsTwoHandedWeapon(player)) {
            player->stateFlags1 |= PLAYER_STATE1_SHIELDING;
            Player_SetModelsForHoldingShield(player);
            sTri.chargeShield = 1;
        }
    } else {
        // Te golpean cargando. La carga se pierde por el camino de vanilla (el golpe
        // cambia de acción); lo único nuestro es poner ENCIMA el traspié del gunlance,
        // igual que el bash de parada y por la misma razón: vanilla ya instaló su
        // propia reacción unas líneas antes de que esto corra, así que hay que llegar
        // después. Se dispara UNA vez, en el frame en que la carga se rompe con
        // PLAYER_STATE1_DAMAGED puesto — soltar el botón limpia el armado sin más.
        if (sTri.hurtPending) {
            sTri.hurtPending = 0;
            if (player->stateFlags1 & PLAYER_STATE1_DAMAGED) {
                LinkAnimationHeader* stagger = Trident_LoadHalf(TRIP_CHARGE_HURT);
                if (stagger != NULL) {
                    Player_AnimPlayOnce(play, player, stagger);
                }
            }
        }
        sTri.chargePrev = -1.0f;
        sTri.chargeLvl = 0;
        sTri.fullHold = 0;
        Trident_SetChargeLevel(play, player, 0);
        if (sTri.chargeShield) {
            // Hand models back to the held item's group — what func_8008EC70 does
            // when the walking shield comes down. Without this the shield stays in
            // the hand after the release.
            sTri.chargeShield = 0;
            Player_SetModelGroup(player, Player_ActionToModelGroup(player, player->heldItemAction));
        }
    }
}

// ---------------------------------------------------------------------------
// The charge ball — Ganondorf's BIG MAGIC (BossGanon_DrawBigMagicCharge): the ball
// he summons over his head and charges for a long while. Same DLs, same colours
// (light flecks + magenta background circle + yellow dot + yellow-green light ball
// + light-ray fan), same overlay. Ours sits over LINK's head while B is held and
// grows with the charge; on the level-2 release it drifts down INTO his chest and
// stays there through the whole strike ("debe quedarse enfrente de Link hasta
// terminar la última animación... cómo que meterse en Link"), and leaves as the
// projectile on the clip's last frame (Trident_TickMelee). A short release lets it
// fade where it is.
//
// State is ticked here every frame (Trident_TickBigMagic, from the behavior) and
// only READ by Trident_Draw, which ExtEquip_DrawDispatch calls after the skeleton.
// ---------------------------------------------------------------------------
// The five layers themselves live in TridentBigMagic_Draw (trident_charge_ball.c,
// included earlier in this TU) because the thrown projectile draws the very same
// thing. Only the sizing and the anchor are ours.
//
// Ganondorf's own targets are circle 0.25->0.4 and ball 45 (arena-sized). Link's
// version is a head-sized one. These two must stay equal to TCB_BALL_CIRCLE_SCALE /
// TCB_BALL_DRAW_SCALE so the ball does not change size the instant it is released.
#define TRI_BM_CIRCLE_MAX 0.16f
#define TRI_BM_BALL_MAX 14.0f
#define TRI_BM_RAYS_MAX TBM_RAYS_MAX
#define TRI_BM_CHEST_UP 40.0f // the "in front of Link" resting point during the release
#define TRI_BM_CHEST_FWD 22.0f

// Din's Fire's own sphere, for the level 1 / 2 dome. Straight out of oot.o2r by OTR
// path — no object is loaded, same as everything else the trident draws. The texture
// goes in as its PATH and not as a resolved pointer so a retexture pack still applies.
//
// One thing this does NOT copy from MagicFire_Draw: it never writes into sSphereVtx.
// That is a SHARED vertex buffer and Din's Fire rewrites its alpha every frame it
// draws; two writers would fight, and there is no way to ask the resource layer how
// many vertices are in it, so a blind write is out. The dome fades on prim/env alpha
// and on its own scale instead.
#define TRI_DOME_TEX "__OTR__overlays/ovl_Magic_Fire/sTex"
#define TRI_DOME_MAT "__OTR__overlays/ovl_Magic_Fire/sMaterialDL"
#define TRI_DOME_MODEL "__OTR__overlays/ovl_Magic_Fire/sModelDL"
#define TRI_DOME_UP 18.0f

// (Math_ApproachF / Math_ApproachZeroF come from functions.h, already in scope.)

static void Trident_TickBigMagic(Player* player) {
    Vec3f want;
    s16 yaw = player->actor.shape.rot.y;
    u8 charging = (player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) && (sTri.state == TRI_IDLE) &&
                  (Player_GetMeleeWeaponHeld(player) != 0);
    // The level-2 release row is playing (the ball is armed or was just fired).
    u8 releasing = 0;
    {
        s32 row = Trident_CurrentRow(player);
        if ((row >= 0) && (strcmp(sTridentMeleeBindings[row].semantic, "chargeLvl2") == 0)) {
            releasing = 1;
        }
    }

    if (charging) {
        f32 t = player->unk_858 / TRI_CHARGE_FULL;
        if (t > 1.0f) {
            t = 1.0f;
        }
        // ON THE LANCE TIP, all the way through the charge. That is what makes the
        // ball read as summoned by the weapon and not by Link, and it is why it ends
        // up over his head anyway once the level 3 stance raises the lance ("la bola
        // aparece al principio mientras cargas en la punta de la lanza" / "la bola de
        // luz pasa de estar arriba de Link porque en la anim de lvl 3 la lanza estará
        // arriba"). The tip is refreshed every frame by the trail block in
        // z_player_lib.c, swinging or not.
        want = player->meleeWeaponInfo[0].tip;
        if (!sTri.bmActive) {
            sTri.bmAnchor = want; // first frame: no lerp from wherever it last was
            sTri.bmActive = 1;
        }
        Math_ApproachF(&sTri.bmAnchor.x, want.x, 0.5f, 30.0f);
        Math_ApproachF(&sTri.bmAnchor.y, want.y, 0.5f, 30.0f);
        Math_ApproachF(&sTri.bmAnchor.z, want.z, 0.5f, 30.0f);
        Math_ApproachF(&sTri.bmCircle, TRI_BM_CIRCLE_MAX * t, 0.3f, 0.02f);
        Math_ApproachF(&sTri.bmBall, TRI_BM_BALL_MAX * t, 0.3f, 2.0f);
        Math_ApproachF(&sTri.bmAlpha, 255.0f, 1.0f, 30.0f);
        // The ray fan is the "full" tell, like his: it only opens once the charge is in.
        if (t >= 1.0f) {
            if ((sTri.bmRays < TRI_BM_RAYS_MAX) && ((sTri.bmTimer & 3) == 0)) {
                sTri.bmRays++;
            }
        } else if (sTri.bmRays > 0) {
            sTri.bmRays--;
        }
        sTri.bmTimer++;
        return;
    }

    if (releasing && sTri.bmActive) {
        // Into the chest, and it stays there until the strike ends.
        want = player->actor.world.pos;
        want.x += Math_SinS(yaw) * TRI_BM_CHEST_FWD;
        want.z += Math_CosS(yaw) * TRI_BM_CHEST_FWD;
        want.y += TRI_BM_CHEST_UP;
        Math_ApproachF(&sTri.bmAnchor.x, want.x, 0.35f, 40.0f);
        Math_ApproachF(&sTri.bmAnchor.y, want.y, 0.35f, 40.0f);
        Math_ApproachF(&sTri.bmAnchor.z, want.z, 0.35f, 40.0f);
        if (sTri.bmRays < TRI_BM_RAYS_MAX) {
            sTri.bmRays++;
        }
        sTri.bmTimer++;
        // No fade-out here: the moment the projectile leaves, Trident_ReleaseBall
        // clears bmActive outright and this branch stops running. A release that
        // never fires (no charge left) still fades through the tail below.
        return;
    }

    // Not charging, not releasing: fade out where it is.
    Math_ApproachZeroF(&sTri.bmCircle, 1.0f, 0.02f);
    Math_ApproachZeroF(&sTri.bmBall, 1.0f, 2.0f);
    Math_ApproachZeroF(&sTri.bmAlpha, 1.0f, 30.0f);
    if (sTri.bmRays > 0) {
        sTri.bmRays--;
    }
    if ((sTri.bmCircle <= 0.0f) && (sTri.bmBall <= 0.0f)) {
        sTri.bmActive = 0;
        sTri.bmRays = 0;
    }
}

// The level 1 / 2 shell dome. MagicFire_Draw's sequence minus the fullscreen tint
// and minus the vertex writes (see TRI_DOME_TEX). Grows over the first third, holds,
// then fades — 14 frames start to finish.
static void Trident_DrawDome(Player* player, PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 frame = play->gameplayFrames;
    f32 age = 1.0f - ((f32)sTri.domeTimer / (f32)TRI_DOME_FRAMES); // 0 -> 1
    f32 s = sTri.domeScale * ((age < 0.35f) ? (age / 0.35f) : 1.0f);
    u8 alpha = (u8)(255.0f * ((age < 0.5f) ? 1.0f : (1.0f - ((age - 0.5f) * 2.0f))));

    if (s <= 0.0001f) {
        return;
    }

    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Xlu(gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 210, 255, 130, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 120, 255, 0, alpha);
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + TRI_DOME_UP, player->actor.world.pos.z,
                     MTXMODE_NEW);
    Matrix_Scale(s, s, s, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
    gDPLoadTextureBlock(POLY_XLU_DISP++, TRI_DOME_TEX, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 6, 6, 15, G_TX_NOLOD);
    gDPSetTile(POLY_XLU_DISP++, G_IM_FMT_I, G_IM_SIZ_8b, 8, 0, 1, 0, G_TX_NOMIRROR | G_TX_WRAP, 6, 14,
               G_TX_NOMIRROR | G_TX_WRAP, 6, 14);
    gDPSetTileSize(POLY_XLU_DISP++, 1, 0, 0, 252, 252);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)TRI_DOME_MAT);
    gSPDisplayList(POLY_XLU_DISP++,
                   Gfx_TwoTexScrollEx(gfxCtx, 0, (frame * 2) % 512, 511 - ((frame * 5) % 512), 64, 64, 1,
                                      (frame * 2) % 256, 255 - ((frame * 20) % 256), 32, 32, 2, -5, 2, -20));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)TRI_DOME_MODEL);
    CLOSE_DISPS(gfxCtx);
}

static void Trident_Draw(Player* player, PlayState* play) {
    if ((player == NULL) || (play == NULL)) {
        return;
    }
    if (sTri.domeTimer > 0) {
        Trident_DrawDome(player, play);
    }
    if (!sTri.bmActive) {
        return;
    }
    // Same five-layer draw the thrown ball uses, so the release is continuous.
    TridentBigMagic_Draw(play, &sTri.bmAnchor, sTri.bmCircle, sTri.bmBall, sTri.bmAlpha, sTri.bmRays,
                         (play->gameplayFrames * 10.0f) / 1000.0f);
}

// ---------------------------------------------------------------------------
// El escudo del gunlance
//
// El arma sólo se maneja con el Mirror Shield o a mano desnuda: si tienes el
// Mirror lo equipa, y si no, te quita el que llevaras.
//
// ⚠️ NO se guarda el escudo anterior ni se restaura al desequipar el trident.
// Es deliberado ("no lo pierdes del inventario pero no te restaura el escudo, lo
// que hace más riesgoso"): el escudo sigue en el inventario, pero sacar el
// trident con el Hylian puesto te deja sin escudo hasta que lo vuelvas a poner tú.
// Por eso esto NO tiene pareja en Trident_Cleanup, y no es un olvido.
// ---------------------------------------------------------------------------
// The Trident's shield rule (Divine or a Mirror, else bare) is applied ONCE by ExtEquip_SetSlot
// when the Trident goes on, and the kaleido/ExtEquip_Equip refuse other shields while it is worn —
// nothing here touches the shield slot per frame anymore.

// ---------------------------------------------------------------------------
// Phantom Ganon flight — the one custom state.
//
// R+A held on the ground takes off (needs magic, or the Magic Cape). In the air:
// stick moves at walking speed, R climbs, L descends, B fires a homing light ball,
// A launches Link at the lock-on (or the nearest enemy). Descend onto the ground
// to land. Every second in the air costs 4 magic unless the cape is owned.
//
// Mechanics, all verified against z_player.c / boss_remains.cpp:
//   · PAUSE_ACTION_FUNC: our clip is the only thing on skelAnime.
//   · PLAYER_STATE3_MIDAIR every frame: func_8083AA10 runs even under PAUSE and
//     would otherwise yank an airborne Link into the fall action.
//   · gravity 0 + velocity.y set here: Actor_UpdateVelocityXZGravity keeps it.
//   · linearVelocity + yaw set here: UpdateCommon turns them into world velocity
//     next frame regardless of PAUSE (the Gerudo plant note).
//   · a change of actionFunc while we hold PAUSE means damage / a cutscene took
//     over (Player_SetupAction is not gated by PAUSE) — abort and let it run.
// ---------------------------------------------------------------------------
static Actor* Trident_FindLaunchTarget(PlayState* play, Player* player) {
    Actor* t = player->focusActor;
    if ((t != NULL) && (t->update != NULL)) {
        return t;
    }
    t = Actor_FindNearby(play, &player->actor, -1, ACTORCAT_BOSS, 900.0f);
    if ((t != NULL) && (t->update != NULL)) {
        return t;
    }
    t = Actor_FindNearby(play, &player->actor, -1, ACTORCAT_ENEMY, 900.0f);
    if ((t != NULL) && (t->update != NULL)) {
        return t;
    }
    return NULL;
}

static void Trident_FlyEnter(PlayState* play, Player* player) {
    // A clean idle action underneath (clears the upper action, av vars, MIDAIR),
    // then PAUSE so it never runs — Trident_StartClip sets the flag.
    func_80839FFC(player, play);
    sTri.flyAction = player->actionFunc;
    Trident_StartClip(play, player, TRIP_FLY_START, 0);
    sTri.state = TRI_FLY_START;
    sTri.flyMagicTick = 0;
    sTri.launchTarget = NULL;
    sTri.shot = 0;
    player->linearVelocity = 0.0f;
    player->actor.velocity.y = 6.0f; // off the floor, so GROUND drops next frame
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    player->actor.gravity = 0.0f;
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    // (El zumbido de vuelo NO se lanza aquí: se pide cada frame en Trident_TickFlight.
    // Ver el comentario allí — lanzarlo una vez es lo que lo dejaba sonando para
    // siempre.)
}

// land = 1: touched down, play the landing and hand Link back to vanilla idle.
// land = 0: something else took over (damage, water, cutscene) — release and do NOT
// touch the animation, whoever took over owns it now.
static void Trident_FlyExit(PlayState* play, Player* player, u8 land) {
    Trident_QuadOff(player);
    player->stateFlags3 &= ~(PLAYER_STATE3_PAUSE_ACTION_FUNC | PLAYER_STATE3_MIDAIR);
    player->actor.gravity = -1.2f; // what a jump lands with (equip_pendant.c does the same)
    sTri.state = TRI_IDLE;
    sTri.launchTarget = NULL;
    sTri.flyHold = 0;
    if (land) {
        LinkAnimationHeader* landAnim = Trident_LoadHalf(TRIP_FLY_LAND);
        func_80839FFC(player, play);
        if (landAnim != NULL) {
            Player_AnimPlayOnce(play, player, landAnim); // idle action plays it out, then idles
        }
        player->linearVelocity = 0.0f;
        Player_RequestRumble(player, 120, 10, 100, 0);
    }
}

// Stick → yaw + speed, camera-relative, capped at walking speed. Faces the way
// it moves; standing still with a lock-on faces the target.
static void Trident_FlyMove(PlayState* play, Player* player) {
    f32 spd = 0.0f;
    s16 yaw = player->actor.shape.rot.y;

    // 0.0f = SPEED_MODE_LINEAR. The define lives in z_player.c below the unity
    // include of this file, so it is not visible here.
    Player_GetMovementSpeedAndYaw(player, &spd, &yaw, 0.0f, play);
    if (spd > 0.5f) {
        if (spd > TRI_FLY_SPEED) {
            spd = TRI_FLY_SPEED;
        }
        player->linearVelocity = spd;
        player->yaw = yaw;
        Math_ScaledStepToS(&player->actor.shape.rot.y, yaw, 2500);
    } else {
        player->linearVelocity = 0.0f;
        if ((player->focusActor != NULL) && (player->focusActor->update != NULL)) {
            Math_ScaledStepToS(&player->actor.shape.rot.y,
                               Actor_WorldYawTowardActor(&player->actor, player->focusActor), 2500);
        }
        player->yaw = player->actor.shape.rot.y;
    }
}

// The wind Link drags while climbing or dropping. No collider, no damage — it is
// only there to say the air is moving. Dust puffs trailing OPPOSITE the motion, so
// rising leaves them below and dropping leaves them above.
static Color_RGBA8 sTriWindPrim = { 235, 240, 255, 140 };
static Color_RGBA8 sTriWindEnv = { 130, 160, 200, 0 };

static void Trident_FlyWind(PlayState* play, Player* player, f32 vy) {
    Vec3f pos = player->actor.world.pos;
    Vec3f vel;
    Vec3f accel = { 0.0f, 0.0f, 0.0f };

    if ((play->gameplayFrames & 1) != 0) {
        return;
    }
    pos.x += Rand_CenteredFloat(18.0f);
    pos.z += Rand_CenteredFloat(18.0f);
    pos.y += (vy > 0.0f) ? 2.0f : 46.0f;
    vel.x = Rand_CenteredFloat(1.5f);
    vel.y = (vy > 0.0f) ? -2.0f : 2.0f;
    vel.z = Rand_CenteredFloat(1.5f);
    EffectSsDust_Spawn(play, 0, &pos, &vel, &accel, &sTriWindPrim, &sTriWindEnv, 60, 12, 8, 0);
}

// The slam lands. The ring is MM's own: EffectSsBlast IS gEffShockwaveDL
// (z_eff_ss_blast.c:39), and a ground ripple lays a second, flatter one on the floor
// — together they are the mark a ground pound leaves. Then the trident's own
// jump-slash landing clip closes it, and Link is back in vanilla's hands.
static void Trident_PoundImpact(PlayState* play, Player* player) {
    Vec3f pos = player->actor.world.pos;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    LinkAnimationHeader* finish;
    Actor* actor;

    Trident_QuadOff(player);
    player->stateFlags3 &= ~(PLAYER_STATE3_PAUSE_ACTION_FUNC | PLAYER_STATE3_MIDAIR);
    player->actor.gravity = -1.2f;
    player->linearVelocity = 0.0f;
    sTri.state = TRI_IDLE;
    sTri.launchTimer = 0;
    sTri.launchTarget = NULL;
    sTri.flyHold = 0;

    // ⚠️ SOBRE EL SUELO, no sobre el origen de Link, y GRANDE.
    //
    // Así es como lo invoca MM: su pound resuelve la posición con un raycast al suelo
    // (func_80835D2C con un offset de 45 adelante / 40 arriba, z_player.c:19855 en
    // 2ship) y ahí suelta el EffectSsBlast. Ponerlo en world.pos deja el anillo a la
    // altura de los pies de Link, que sobre cualquier desnivel es dentro del suelo o
    // flotando — de ahí que "no lo invocaba bien". actor.floorHeight es la Y del
    // polígono que tiene debajo, que es la misma respuesta sin el raycast a mano.
    //
    // Y con escala explícita: el shockwave es un QUAD PLANO con textura, no geometría
    // de anillo, así que al tamaño por defecto casi no se ve. La nota de 2ship sobre
    // este mismo DL dice justo eso ("needs a much larger scale"). El ripple de agua
    // que había encima se va: es de otro efecto y sólo ensuciaba.
    pos.y = player->actor.floorHeight + 2.0f;
    EffectSsBlast_SpawnWhiteCustomScale(play, &pos, &zero, &zero, TRI_POUND_FX_SCALE, TRI_POUND_FX_STEP, 12);
    Player_RequestQuake(play, 32967, 8, 24);
    Player_RequestRumble(player, 255, 30, 200, 0);
    Sfx_PlaySfxCentered(NA_SE_IT_BOMB_EXPLOSION);

    // The floor is what got hit, so everything standing on it takes it. This is NOT
    // the shell burst that was removed — no bomb, no VFX of its own; the shockwave
    // above is the whole visual and this just applies what it means.
    actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].head;
    while (actor != NULL) {
        if (Math_Vec3f_DistXYZ(&pos, &actor->world.pos) <= TRI_POUND_RADIUS) {
            actor->colChkInfo.damage = TRI_POUND_DMG;
            Actor_ApplyDamage(actor);
            Actor_SetColorFilter(actor, 0x4000, 0xC8, 0x0000, 12);
        }
        actor = actor->next;
    }

    func_80839FFC(player, play);
    finish = Trident_LoadHalf(TRIP_JUMP_FINISH);
    if (finish != NULL) {
        Player_AnimPlayOnce(play, player, finish);
    }
}

static void Trident_TickFlight(PlayState* play, Player* player) {
    Input* in = &play->state.input[0];
    u8 rHeld = CHECK_BTN_ALL(in->cur.button, BTN_R) != 0;
    u8 lHeld = CHECK_BTN_ALL(in->cur.button, BTN_L) != 0;
    u8 aHeld = CHECK_BTN_ALL(in->cur.button, BTN_A) != 0;
    u8 aPress = CHECK_BTN_ALL(in->press.button, BTN_A) != 0;
    u8 bPress = CHECK_BTN_ALL(in->press.button, BTN_B) != 0;
    s32 clipDone;
    f32 vy = 0.0f;

    // Something took over: damage knockback, a cutscene, death.
    if (player->actionFunc != sTri.flyAction) {
        Trident_FlyExit(play, player, 0);
        return;
    }
    if (!Trident_CanAct(player)) {
        Trident_FlyExit(play, player, 0);
        return;
    }

    // Held every frame — Player_SetupAction clears them and PAUSE does not stop
    // every path that calls it.
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC | PLAYER_STATE3_MIDAIR;
    player->actor.gravity = 0.0f;
    // Void-out guard #1: Player_HandleExitsAndVoids voids over a void-typed floor
    // once fallDistance (fallStartHeight - y) passes 200, and fallStartHeight is
    // only refreshed while grounded — so descending 200 units in flight over a pit
    // read as a fall into it. Keep the reference at the current height.
    player->fallStartHeight = player->actor.world.pos.y;
    sTri.timer++;

    // ⚠️ EL ZUMBIDO DE VUELO SE PIDE AQUÍ, CADA FRAME, Y POR ESO SE CALLA SOLO.
    //
    // NA_SE_EN_FANTOM_FLOAT es un id CONTINUO: vanilla no lo toca ni una sola vez sin
    // `- SFX_FLAG` (los cuatro sitios que lo usan lo hacen así — z_boss_ganon.c:2323,
    // z_boss_ganondrof.c:588, z_boss_mo.c:3516, z_fishing.c:2355). Lanzarlo UNA vez al
    // despegar, como estaba, arrancaba un bucle que nadie volvía a pedir ni a parar:
    // seguía sonando después de aterrizar, para siempre. Es el mismo defecto que tenía
    // NA_SE_IT_SWORD_CHARGE en el remate de carga.
    //
    // Pedido por frame se comporta como debe: suena mientras vuelas y se apaga solo en
    // cuanto esta función deja de correr, sea aterrizando, por daño o por quedarte sin
    // magia. Sin nada que apagar a mano en ninguna de las salidas.
    Actor_PlaySfx_Flagged(&player->actor, NA_SE_EN_FANTOM_FLOAT - SFX_FLAG);

    // Magic: 4 per second, free with the cape. Running dry brings Link down.
    if (!ExtEquip_CapeOwned()) {
        if (++sTri.flyMagicTick >= TRI_FLY_MAGIC_TICK) {
            sTri.flyMagicTick = 0;
            if (!Magic_RequestChange(play, TRI_FLY_MAGIC_COST, MAGIC_CONSUME_NOW)) {
                // Out of magic: drop. Gravity comes back and the ground check below
                // turns the fall into the landing when it arrives.
                Trident_FlyExit(play, player, 0);
                func_80839FFC(player, play);
                return;
            }
        }
    }

    // Advance the clip ONCE per frame, here and nowhere else.
    clipDone = Trident_Advance(play, player);

    switch (sTri.state) {
        case TRI_FLY_START:
            // Rise through the takeoff clip, then hover. On the way up he leaves a
            // lit streak under his feet — FhgFlash light balls dropped at the foot
            // position every other frame, which is the same effect the light ball and
            // the seekers trail with, so the whole Phantom Ganon kit reads as one
            // thing. Skijer's NEI
            vy = 3.0f;
            player->linearVelocity = 0.0f;
            if ((play->gameplayFrames & 1) == 0) {
                Vec3f foot = player->actor.world.pos;
                TridentChargeBall_DropSpark(play, &foot);
            }
            if (clipDone) {
                Trident_StartClip(play, player, TRIP_FLY_IDLE, 1);
                sTri.state = TRI_FLY_IDLE;
            }
            break;

        case TRI_FLY_IDLE:
            vy = rHeld ? TRI_FLY_CLIMB : (lHeld ? -TRI_FLY_CLIMB : 0.0f);
            Trident_FlyMove(play, player);
            if (vy != 0.0f) {
                Trident_FlyWind(play, player, vy);
            }
            // ⚠️ R+B BEFORE B. R is also "climb", so the slam has to be tested first
            // or holding R to rise and tapping B would always come out as the ball.
            // The trade, chosen deliberately: no light ball while climbing.
            if (bPress && rHeld) {
                Trident_StartClip(play, player, TRIP_FLY_POUND, 0);
                sTri.state = TRI_FLY_POUND;
                sTri.launchTimer = 0;
                player->linearVelocity = 0.0f;
                Sfx_PlaySfxCentered(NA_SE_IT_SWORD_SWING_HARD);
                Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
            } else if (bPress) {
                // The wind-up first, then the clip that actually throws the ball.
                Trident_StartClip(play, player, TRIP_FLY_SHOOT_PRE, 0);
                sTri.state = TRI_FLY_SHOOT_PRE;
            } else if (aPress) {
                sTri.launchTarget = Trident_FindLaunchTarget(play, player);
                Trident_StartClip(play, player, TRIP_FLY_LAUNCH, 0);
                sTri.launchTimer = 0;
                sTri.launchPhase = (f32)TRI_FLY_LAUNCH_LOOP_A;
                sTri.launchPing = 1;
                sTri.state = TRI_FLY_LAUNCH;
                // "un clean al rotation de link que hace que mire abajo": square the
                // body up and point it at the target.
                player->actor.shape.rot.x = 0;
                player->actor.shape.rot.z = 0;
                if (sTri.launchTarget != NULL) {
                    // SOLVE the throw here, once. Horizontal speed is fixed, so the
                    // flight lasts T = horiz / speed frames; the upward speed that
                    // lands on the target after T frames of TRI_FLY_ARC_FALL is
                    //     vy0 = dy/T + g*T/2
                    // A per-frame vy that ignores where the target IS gives a
                    // horizontal beeline with an unrelated bob on top — which is
                    // exactly the "primero recto y luego baja" this replaces.
                    Actor* t = sTri.launchTarget;
                    f32 dx = t->world.pos.x - player->actor.world.pos.x;
                    f32 dz = t->world.pos.z - player->actor.world.pos.z;
                    f32 dy = (((t->world.pos.y + t->focus.pos.y) * 0.5f) - player->actor.world.pos.y);
                    f32 horiz = sqrtf((dx * dx) + (dz * dz));
                    f32 flight = horiz / TRI_FLY_LAUNCH_SPEED;
                    s16 yaw = Actor_WorldYawTowardActor(&player->actor, sTri.launchTarget);

                    player->actor.shape.rot.y = yaw;
                    player->yaw = yaw;
                    if (flight < 4.0f) {
                        flight = 4.0f;
                    }
                    sTri.launchPhase = (dy / flight) + (TRI_FLY_ARC_FALL * flight * 0.5f);
                    if (sTri.launchPhase < TRI_FLY_ARC_MIN) {
                        sTri.launchPhase = TRI_FLY_ARC_MIN;
                    }
                }
            }
            break;

        case TRI_FLY_SHOOT_PRE:
            vy = rHeld ? TRI_FLY_CLIMB : (lHeld ? -TRI_FLY_CLIMB : 0.0f);
            Trident_FlyMove(play, player);
            if (vy != 0.0f) {
                Trident_FlyWind(play, player, vy);
            }
            if (clipDone) {
                Trident_StartClip(play, player, TRIP_FLY_SHOOT, 0);
                sTri.shot = 0;
                sTri.state = TRI_FLY_SHOOT;
            }
            break;

        case TRI_FLY_SHOOT:
            vy = rHeld ? TRI_FLY_CLIMB : (lHeld ? -TRI_FLY_CLIMB : 0.0f);
            Trident_FlyMove(play, player);
            if (vy != 0.0f) {
                Trident_FlyWind(play, player, vy);
            }
            if (!sTri.shot && Trident_Crossed(sTri.prevFrame, player->skelAnime.curFrame, (f32)TRI_FLY_SHOOT_FRAME)) {
                sTri.shot = 1;
                Trident_ShootLight(play, player);
            }
            sTri.prevFrame = player->skelAnime.curFrame;
            if (clipDone) {
                Trident_StartClip(play, player, TRIP_FLY_IDLE, 1);
                sTri.state = TRI_FLY_IDLE;
            }
            break;

        case TRI_FLY_POUND:
            // The wind-up plays out; from its last frame Link drops like a stone with
            // the lance live, and the touchdown at the bottom of this function turns
            // into the impact.
            player->linearVelocity = 0.0f;
            if (sTri.launchTimer == 0) {
                vy = 0.0f;
                if (clipDone) {
                    sTri.launchTimer = 1; // holds the last frame from here on
                    Trident_QuadOn(player);
                }
            } else {
                vy = TRI_POUND_FALL;
                sTri.launchTimer++;
            }
            break;

        case TRI_FLY_LAUNCH: {
            Actor* t = sTri.launchTarget;
            u8 hasTarget = ((t != NULL) && (t->update != NULL));

            sTri.launchTimer++;
            if (sTri.launchTimer == 1) {
                Trident_QuadOn(player);
                Sfx_PlaySfxCentered(NA_SE_EN_FANTOM_MASIC2);
            }

            if (!hasTarget) {
                // ── No lock-on: straight, and it lasts as long as you hold A. ──
                // The clip ping-pongs across its 10..16 window meanwhile. curFrame is
                // written AFTER Trident_Advance on purpose: the update already
                // consumed this frame's pose, so this is what the NEXT one reads.
                player->yaw = player->actor.shape.rot.y;
                player->linearVelocity = TRI_FLY_LAUNCH_SPEED;
                vy = 0.0f;

                if (aHeld) {
                    sTri.launchPhase += (f32)sTri.launchPing;
                    if (sTri.launchPhase >= (f32)TRI_FLY_LAUNCH_LOOP_B) {
                        sTri.launchPhase = (f32)TRI_FLY_LAUNCH_LOOP_B;
                        sTri.launchPing = -1;
                    } else if (sTri.launchPhase <= (f32)TRI_FLY_LAUNCH_LOOP_A) {
                        sTri.launchPhase = (f32)TRI_FLY_LAUNCH_LOOP_A;
                        sTri.launchPing = 1;
                    }
                    player->skelAnime.curFrame = sTri.launchPhase;
                } else if (clipDone) {
                    // Released, and the clip has finished playing itself out.
                    Trident_QuadOff(player);
                    Trident_StartClip(play, player, TRIP_FLY_IDLE, 1);
                    sTri.state = TRI_FLY_IDLE;
                    player->linearVelocity = 0.0f;
                    vy = 0.0f;
                }
                break;
            }

            // ── Lock-on: a committed ARC. Not cancellable — it lobs up and comes
            // back down onto the target, so letting go mid-flight would just drop
            // Link out of the sky halfway there.
            {
                Vec3f to;
                f32 horiz;
                f32 dist;

                to.x = t->world.pos.x - player->actor.world.pos.x;
                to.y = ((t->world.pos.y + t->focus.pos.y) * 0.5f) - player->actor.world.pos.y;
                to.z = t->world.pos.z - player->actor.world.pos.z;
                horiz = sqrtf((to.x * to.x) + (to.z * to.z));
                dist = sqrtf((horiz * horiz) + (to.y * to.y));

                if (dist > 1.0f) {
                    // ⚠️ Math_Atan2S takes (z, x) — Math_Vec3f_Yaw is Atan2S(dz, dx).
                    // (to.x, to.z) sent Link AWAY from the target.
                    s16 yaw = Math_Atan2S(to.z, to.x);
                    player->yaw = yaw;
                    player->actor.shape.rot.y = yaw;
                    player->linearVelocity = TRI_FLY_LAUNCH_SPEED;
                }
                // The parabola, running off the upward speed solved at entry. Nothing
                // recomputes it mid-flight: that is what keeps it one continuous throw.
                vy = sTri.launchPhase - ((f32)sTri.launchTimer * TRI_FLY_ARC_FALL);

                {
                    u8 bladeHit = (player->meleeWeaponQuads[0].base.atFlags & AT_HIT) ||
                                  (player->meleeWeaponQuads[1].base.atFlags & AT_HIT);
                    if ((dist <= TRI_FLY_LAUNCH_HIT) || (sTri.launchTimer >= TRI_FLY_LAUNCH_MAX) || bladeHit) {
                        // The ram is a BLADE hit, not a shell: no burst. The armed
                        // quads did the work on the way in; if Link arrived without
                        // the lance touching, the target still takes the hit.
                        if (!bladeHit && (dist <= TRI_FLY_LAUNCH_HIT)) {
                            t->colChkInfo.damage = TRI_MELEE_DMG;
                            Actor_ApplyDamage(t);
                            Actor_SetColorFilter(t, 0x4000, 0xC8, 0x0000, 8);
                            Sfx_PlaySfxCentered(NA_SE_IT_SWORD_STRIKE_HARD);
                        }
                        Trident_QuadOff(player);
                        Trident_StartClip(play, player, TRIP_FLY_IDLE, 1);
                        sTri.state = TRI_FLY_IDLE;
                        sTri.launchTarget = NULL;
                        player->linearVelocity = 0.0f;
                        vy = 0.0f;
                    }
                }
            }
            break;
        }

        default:
            Trident_FlyExit(play, player, 0);
            return;
    }

    player->actor.velocity.y = vy;

    // Touchdown.
    //
    // ⚠️ EL ARCO NO SE TOCA AQUÍ. Este test dispara en cuanto vy se vuelve negativa y
    // el suelo está cerca — y la segunda mitad de un arco es exactamente eso, así que
    // el aterrizaje se metía en medio del lanzamiento y lo cortaba: es el "cómo que
    // un action interrumpe el otro". Un arco termina donde dice su propio caso
    // (llegada, tope de tiempo, o la lanza tocando), en ningún otro sitio.
    //
    // La recta mantenida sí aterriza: volar de frente contra el suelo debe posarte.
    // Y el slam tiene su propio aterrizaje, que ES el movimiento.
    if ((sTri.state == TRI_FLY_LAUNCH) && (sTri.launchTarget != NULL)) {
        return;
    }
    if ((sTri.state != TRI_FLY_START) && (vy <= 0.0f) && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        if (sTri.state == TRI_FLY_POUND) {
            Trident_PoundImpact(play, player);
        } else {
            Trident_FlyExit(play, player, 1);
        }
    }
}

// ---------------------------------------------------------------------------
// R + B — the guard dash. A way to cover ground WITHOUT sheathing the lance, which
// is the whole point of it; the blade is live but barely scratches.
//
// ⚠️ ESTO NO USA PAUSE_ACTION_FUNC, y ese fue el fallo de la primera versión ("R+B
// holding no hace la carrera"). Montarlo sobre PAUSE con su propio actionFunc se caía
// sola: pulsar B con el escudo arriba hace que vanilla entre en su estocada agachada
// ANTES de que esto corra, así que el actionFunc ya había cambiado y el guardia de
// "algo me quitó la acción" abortaba la carrera en el primer frame.
//
// La forma que SÍ funciona en este repo son las Pegasus Boots (equip_pegasus.c), y
// esto es esa misma receta sin el coste de magia: se entra en la acción de CARGA de
// vanilla y se la deja de anfitriona — un estado estable que no se va solo — con
// unk_858 clavado a 0 para que no cargue nunca, y desde ahí se conducen a mano la
// animación y la velocidad. Sin banderas propias que mantener, sin actionFunc propio
// que defender.
//
// De paso arregla el bucle de las piernas por construcción: aquí el ciclo se REPRODUCE
// en ANIMMODE_LOOP a su longitud nativa y lo avanza el propio motor, así que no pasa
// por el remuestreo a 29 que exige la tabla de locomoción.
// Skijer's NEI
// ---------------------------------------------------------------------------
static void Trident_DashEnter(PlayState* play, Player* player) {
    LinkAnimationHeader* start = Trident_LoadHalf(TRIP_DASH_START);

    // Vanilla's charge action as the host. It is entered on purpose and then never
    // allowed to charge: unk_858 stays at 0 every frame below.
    func_808377DC(play, player);
    player->unk_858 = 0.0f;
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;

    sTri.state = TRI_DASH_START;
    sTri.timer = TRI_DASH_START_FRAMES;
    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->yaw = player->actor.shape.rot.y;

    if (start != NULL) {
        sTri.timer = (s16)Animation_GetLastFrame(start);
        LinkAnimation_Change(play, &player->skelAnime, start, 1.0f, 0.0f, Animation_GetLastFrame(start), ANIMMODE_ONCE,
                             -6.0f);
    }
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
}

// Hands Link back. Nothing to unwind but the collider window and the flags we set —
// there is no PAUSE and no borrowed actionFunc, which is the point.
static void Trident_DashExit(PlayState* play, Player* player, u8 resetAction) {
    Trident_QuadOff(player);
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    player->unk_858 = 0.0f;
    sTri.state = TRI_IDLE;
    if (resetAction) {
        func_80839FFC(player, play); // idle; it bleeds the speed off on its own
    }
}

// The stance on the torso, the run cycle on the legs. Same split the Pegasus dash
// uses: the frame is loaded ASYNC into upperJointTable (ready next frame) and then
// only the upper-body limbs are copied over whatever the legs are playing.
// upperJointTable IS upperSkelAnime.jointTable — SkelAnime_InitLink hands it that
// same buffer — so ExtPlayer_CopyUpperBody, which is vanilla's own limb map, reads
// exactly what was loaded here.
static void Trident_DashPose(PlayState* play, Player* player) {
    LinkAnimationHeader* pose =
        ResourceMgr_FileExists(TRIP_DASH_POSE) ? ResourceMgr_LoadPlayerAnimAsHeaderInPlace(TRIP_DASH_POSE, 1) : NULL;

    if (pose == NULL) {
        return;
    }
    // Crawl through the stance a frame at a time so the torso breathes instead of
    // freezing solid over the running legs.
    if (++sTri.timer > (s16)Animation_GetLastFrame(pose)) {
        sTri.timer = 0;
    }
    AnimationContext_SetLoadFrame(play, pose, sTri.timer, player->skelAnime.limbCount, player->upperJointTable);
    ExtPlayer_CopyUpperBody(play, player);
}

static void Trident_TickDash(PlayState* play, Player* player) {
    Input* in = &play->state.input[0];
    LinkAnimationHeader* legs;
    f32 stickX;
    f32 spd = 0.0f;
    s16 yaw = player->actor.shape.rot.y;
    f32 base;

    // Let go of R and it is over. Same for water, or for anything that took Link
    // somewhere this move has no business being.
    if (!CHECK_BTN_ALL(in->cur.button, BTN_R) || !Trident_CanAct(player)) {
        Trident_DashExit(play, player, 1);
        return;
    }

    // The host must never actually charge.
    player->unk_858 = 0.0f;
    player->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;

    if (sTri.state == TRI_DASH_START) {
        player->linearVelocity = 0.0f;
        player->actor.speedXZ = 0.0f;
        if (--sTri.timer <= 0) {
            sTri.state = TRI_DASH_RUN;
            sTri.timer = 0;
            Trident_QuadOn(player);
            player->meleeWeaponQuads[0].info.toucher.damage = TRI_DASH_DMG;
            player->meleeWeaponQuads[1].info.toucher.damage = TRI_DASH_DMG;
            Sfx_PlaySfxCentered(NA_SE_IT_SWORD_SWING_HARD);
        }
        return;
    }

    // ---- running ----------------------------------------------------------
    // Stick X steers, exactly like the Pegasus dash — it does NOT decide whether to
    // move. Forward is forward.
    stickX = in->rel.stick_x;
    if (fabsf(stickX) > 10.0f) {
        player->actor.shape.rot.y -= (s16)(stickX * 5.0f);
    }
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;

    // 1.2x Link's own speed. A stick-derived target ABOVE the plain-run reference
    // wins instead, which is how the boots and any other speed modifier keep counting.
    Player_GetMovementSpeedAndYaw(player, &spd, &yaw, 0.0f, play);
    base = (spd > TRI_DASH_BASE) ? spd : TRI_DASH_BASE;
    // Both fields: speedXZ is what the engine's wall check reads.
    player->linearVelocity = base * TRI_DASH_SPEED_MUL;
    player->actor.speedXZ = player->linearVelocity;

    // Legs. Changed ONCE and then left alone — the host action's own
    // LinkAnimation_Update advances it, and ANIMMODE_LOOP is what makes the cycle
    // wrap. Native length, no resample, so nothing about the 29-frame locomotion
    // table applies here.
    legs = Trident_LoadLoco();
    if ((legs != NULL) && (player->skelAnime.animation != (void*)legs)) {
        LinkAnimation_Change(play, &player->skelAnime, legs, 1.0f, 0.0f, Animation_GetLastFrame(legs), ANIMMODE_LOOP,
                             -6.0f);
    }

    Trident_DashPose(play, player);
    Actor_PlaySfx_Flagged(&player->actor, NA_SE_PL_WALK_GROUND - SFX_FLAG);
}

// ---------------------------------------------------------------------------
// Per-frame behavior while the Trident is the equipped ext sword.
// ---------------------------------------------------------------------------
static void Trident_Behavior(Player* player, PlayState* play) {
    Input* in;
    u8 drawn;

    if (player == NULL || play == NULL) {
        return;
    }

    if (!sTri.inited) {
        sTri.inited = 1;
        sTri.state = TRI_IDLE;
        // -1, not 0: row 0 is a real row, so a zeroed field would read as "already
        // in row 0" and swallow that row's frame-0 marker on the very first swing.
        sTri.meleeRow = -1;
        sTri.chargePrev = -1.0f;
    }

    // Swap OOT's melee clips for the gunlance ones. Idempotent, so re-running it
    // every frame costs a flag test; that also means it self-heals if anything
    // else stomped the tables. B is NEVER intercepted — OOT's pipeline drives the
    // whole combo, it just plays our animations.
    Trident_InstallAnims();

    drawn = Trident_CanAct(player);
    Trident_TickLoco(play, player, drawn);
    Trident_TickWater(play, player);

    if (TRI_IS_FLYING(sTri.state)) {
        Trident_TickFlight(play, player);
        return;
    }
    if (sTri.state != TRI_IDLE) {
        Trident_TickDash(play, player);
        return;
    }

    // Everything vanilla is playing for us, watched from here.
    Trident_TickMelee(play, player);
    Trident_TickCombo();
    Trident_TickCharge(play, player);
    Trident_TickBigMagic(player);
    if (sTri.goldTimer > 0) {
        sTri.goldTimer--;
    }
    if (sTri.domeTimer > 0) {
        sTri.domeTimer--;
    }

    if (!drawn) {
        sTri.flyHold = 0;
        return;
    }

    // R + A held → take off. Held, not pressed, so a stray tap while guarding does
    // not lift Link off the ground.
    in = &play->state.input[0];

    // R held + B PRESSED → the guard dash. Tested before the R+A hold so a deliberate
    // R+B can never be swallowed by the take-off counter.
    if (CHECK_BTN_ALL(in->cur.button, BTN_R) && CHECK_BTN_ALL(in->press.button, BTN_B) &&
        (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !(player->stateFlags1 & PLAYER_STATE1_DAMAGED)) {
        sTri.flyHold = 0;
        Trident_DashEnter(play, player);
        return;
    }

    if (CHECK_BTN_ALL(in->cur.button, BTN_R) && CHECK_BTN_ALL(in->cur.button, BTN_A)) {
        if (++sTri.flyHold >= TRI_FLY_ENTER_HOLD) {
            sTri.flyHold = 0;
            if ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !(player->stateFlags1 & PLAYER_STATE1_DAMAGED) &&
                (ExtEquip_CapeOwned() || (gSaveContext.magic > 0))) {
                Trident_FlyEnter(play, player);
            }
        }
    } else {
        sTri.flyHold = 0;
    }
}

// Called when the ext sword slot changes away from the Trident.
static void Trident_Cleanup(void) {
    // Unconditional: the tables are global engine state, so they must come back
    // even if the behavior never ran this session.
    Trident_RestoreAnims();
    Trident_RestoreLoco();

    if (!sTri.inited) {
        return;
    }
    // A flight in progress MUST be released here: nothing else clears
    // PLAYER_STATE3_PAUSE_ACTION_FUNC (Player_SetupAction does not — verified), so
    // swapping the slot mid-air would otherwise leave Link frozen in the sky for
    // good. Same for the iron REGs. The player pointer comes from gPlayState.
    if (gPlayState != NULL) {
        Player* player = GET_PLAYER(gPlayState);
        if (player != NULL) {
            if (TRI_IS_FLYING(sTri.state)) {
                Trident_FlyExit(gPlayState, player, 0); // release; the fall action takes it from here
            } else if (sTri.state != TRI_IDLE) {
                Trident_DashExit(gPlayState, player, 0); // same: PAUSE must not survive the slot change
            }
            if (sTri.heavyBoots) {
                Player_SetBootData(gPlayState, player);
            }
        }
    }
    sTri.state = TRI_IDLE;
    sTri.timer = 0;
    sTri.windowOpen = 0;
    sTri.ballPaid = 0;
    sTri.ballArmed = 0;
    sTri.bmActive = 0;
    sTri.chargePrev = -1.0f;
    sTri.chargeLvl = 0;
    sTri.fullHold = 0;
    sTri.bHold = 0;
    // The attack boxes go with the weapon. They are only ever submitted per frame
    // from Trident_TickQuads, so nothing is left registered — this just makes the
    // next equip set them up against a fresh PlayState.
    if (sTriQuadsInited && (gPlayState != NULL)) {
        Collider_DestroyQuad(gPlayState, &sTriAtkQuad);
        Collider_DestroyQuad(gPlayState, &sTriGuardQuad);
    }
    sTriQuadsInited = 0;
    sTri.releaseLevel = 0;
    sTri.hurtPending = 0;
    sTri.goldTimer = 0;
    sTri.domeTimer = 0;
    sTri.flyHold = 0;
    sTri.launchTarget = NULL;
    sTri.heavyBoots = 0;
}

// Called from the melee-hit dispatch while the Trident is equipped.
static void Trident_OnMeleeHit(Player* player, PlayState* play) {
    (void)player;
    (void)play;
    // Melee hits carry no extra effect: the trident's identity is the shelling
    // burst and the charged ball, both of which own their own impact handling.
}
