/**
 * rito_flight.inc.c — the Rito form's flight mode.
 *
 * Text-included at the END of mm_player_form.cpp (same arrangement as
 * gerudo_mhr_combat.inc.c), so it sees gFormState without exporting it.
 *
 * THE MOVE
 *   press A            -> an updraft starts charging under the rito (wind builds)
 *   release A          -> it rides that wind straight up
 *   hold A in the air  -> glide: a Zora swim, in 3D, in the air. Free.
 *   let go of A        -> the wings stop and it falls
 * A is the whole interface: as a rito, A IS flight mode.
 *
 * HOW IT DRIVES LINK — copied from the Trident's flight (equip_trident.c), which
 * is the shipped precedent for "a clip-driven flying player":
 *   - clips are loaded RESAMPLED. An MHR clip is authored at its own rate and its
 *     frameCount means nothing to this engine; handing the raw header to
 *     LinkAnimation_Change is what makes a move look frozen or absurdly slow.
 *     Trident_LoadHalf does the same division and it is why its clips read right.
 *   - the clip plays on player->skelAnime with PLAYER_STATE3_PAUSE_ACTION_FUNC set
 *     at EVERY clip start (a vanilla path that clears it in between would
 *     otherwise advance the clip twice), and is ticked with LinkAnimation_Update.
 *   - the rito's body follows because MmForm_UsesOotAnim copies OOT's joints onto
 *     the form by default. These states are deliberately NOT listed as
 *     form-specific there, so the pose the clip puts on Link IS what gets drawn.
 *
 * The bow lives next door in rito_bow.inc.c, on B. The two compose for free: the bow is
 * UPPER BODY only, so it never contends for the clip this file drives.
 *
 * ORDER MATTERS: include AFTER gerudo_mhr_combat.inc.c — its MmForm_MhrLoadPath
 * is reused for the graceful "clip missing → move disabled" lookup.
 */

extern u8 MmForm_InputOwnedByMessage(void);

// z_player.c entry points this file drives — same declaration style as the block
// at the top of gerudo_mhr_combat.inc.c, which is included just before this one
// and therefore in the same linkage context. `func_80839FFC` is the clean idle
// action the Trident's flight also lays down before taking over.
extern void func_80839FFC(Player* player, PlayState* play);
// The upper-body guard and vanilla's own release for it. The bow needs the release:
// func_80834B5C only ever exits when R comes UP, and R is the bow's draw button.
extern void func_80834894(Player* player);
// Torso and arms from upperSkelAnime, legs from whatever skelAnime is playing —
// vanilla's own split. The bow next door rides it so it never takes the legs.
extern void ExtPlayer_CopyUpperBody(PlayState* play, Player* player);
extern s32 func_80834B5C(Player* player, PlayState* play);
extern s32 func_80834BD4(Player* player, PlayState* play);
extern LinkAnimationHeader* ExtPlayer_GetAnimGroupAnim(s32 group, s32 animType);
extern void ExtPlayer_SetAnimGroupAnim(s32 group, s32 animType, LinkAnimationHeader* anim);

// Defined further down; the reset above it has to be able to tear the ribbons down.
static void MmForm_RitoWindTrailsOff(PlayState* play);
// rito_bow.inc.c, text-included after this file: how far its running clip sits off the rest.
static f32 MmForm_RitoBowDrawLift(Player* player);

// ── clips ───────────────────────────────────────────────────────────────────
#define RITO_ANIM(name) "__OTR__misc/link_animetion/" name

#define RITO_CLIP_LAUNCH RITO_ANIM("gMonsterHunterRise_InsectGlaive_BackwardRisingDoubleChargedStaffCombo")
#define RITO_CLIP_FLY RITO_ANIM("gPlayerAnim_mhr_npc_takkuri_fly")
#define RITO_CLIP_LAND RITO_ANIM("gMonsterHunterRise_InsectGlaive_ForwardSingleAdvancingStaffSweep")
// One clip per hop direction, indexed by OOT's controlStickDirection
// (0 front, 1 side-left, 2 backflip, 3 side-right). Hand-picked, so the direction in
// the MHR name does NOT always match the hop's — the side pair reads better mirrored.
#define RITO_CLIP_HOP_FRONT \
    RITO_ANIM("gMonsterHunterRise_InsectGlaive_ForwardRisingMultiHitAerialStaffStrike_Variant08")
#define RITO_CLIP_HOP_LEFT RITO_ANIM("gMonsterHunterRise_InsectGlaive_RightHighAerialDoubleSilkbindStaffStrike")
#define RITO_CLIP_HOP_BACK \
    RITO_ANIM("gMonsterHunterRise_InsectGlaive_BackwardHighAerialMultiHitSilkbindStaffStrike_Variant06")
#define RITO_CLIP_HOP_RIGHT RITO_ANIM("gMonsterHunterRise_InsectGlaive_LeftHighAerialSingleSilkbindStaffStrike")
// A hop's strike is a one-shot: rather than freeze on its last frame for the rest of
// the fall, it settles into this ready pose and holds it until touchdown.
#define RITO_CLIP_HOP_SETTLE RITO_ANIM("gMonsterHunterRise_InsectGlaive_StationaryStaffReadyIdle_Variant10")
#define RITO_CLIP_THROW RITO_ANIM("gMonsterHunterRise_InsectGlaive_ForwardDoubleStaffStrike")

// Playback rate the MHR clips are resampled to, exactly like TRI_ANIM_SPEED.
#define RITO_ANIM_SPEED 2.0f
#define RITO_THROW_SPEED 3.0f // "1.5x" on top of the 2.0 the others are resampled at

// ── tuning ──────────────────────────────────────────────────────────────────
// Frames are LOGIC frames: this runs at 20Hz (R_UPDATE_RATE = 3), so 20 = 1 second.
#define RITO_CHARGE_MIN 6                           // shortest useful charge
#define RITO_CHARGE_FULL 24                         // fully charged updraft
#define RITO_LAUNCH_MIN 8.0f                        // a minimum charge is about one Roc's Feather
#define RITO_LAUNCH_MAX (RITO_ROCS_VELOCITY * 4.0f) // a full charge is 4x Roc's Feather
#define RITO_GLIDE_SPEED 4.5f                       // half of the Zora's swim, as asked
#define RITO_GLIDE_YAW 0x300                        // vs the swim's 0x640: turning is heavier in the air
#define RITO_TURN_RATE 5.0f                         // Pegasus-dash carve: a small, slow bank
#define RITO_TURN_DEADZONE 10.0f                    // Pegasus uses the same threshold
#define RITO_GLIDE_SINK -0.35f                      // gentle loss of height while gliding level
#define RITO_FLY_GRAVITY_DEFAULT -1.2f              // vanilla fall, the moment A is released
// Magic is charged for GETTING airborne, never for staying there: the launch bills
// once, gliding is free. (Roc's Feather / Roc's Cape in mid-air cost RITO_ROCS_AIR_COST
// each — that hook lives with those items, not here.)
#define RITO_LAUNCH_MAGIC_COST 12
#define RITO_ROCS_AIR_COST 12
// A Roc's used mid-glide breaks OUT of the glide for this long: the wings stop
// holding the rito level and it climbs on the item's own velocity, then settles
// back. Long enough for the 11.0f the items give to bleed down to the sink rate.
#define RITO_BOOST_FRAMES 16

#define RITO_DRAW_Y_BASE -1059.0f                        // 2 world units lower again
#define RITO_LAND_LIFT 1500.0f                           // +15 world units, so the landing clip stands normally
#define RITO_HOP_LAND_GUARD 4                            // frames before a hop may register a landing
#define RITO_HOP_SCALE 1.5f                              // roll     -> 1.5 Roc's Feather jumps
#define RITO_BACKFLIP_SCALE 1.0f                         // backflip -> exactly 1
#define RITO_ROCS_VELOCITY (LINK_IS_ADULT ? 7.5f : 7.0f) // RocsFeather.cpp's own values

typedef enum {
    RITO_FLY_OFF = 0,
    RITO_FLY_CHARGE, // A held: the updraft builds
    RITO_FLY_LAUNCH, // A released: riding it up
    RITO_FLY_GLIDE,  // A held in the air: Zora swim in 3D
    RITO_FLY_FALL,   // A released in the air: falling
    RITO_FLY_LAND,
    RITO_FLY_HOP,   // roll and the three dodge hops
    RITO_FLY_THROW, // mid-air item
} RitoFlyState;

typedef struct {
    u8 loaded;
    u8 state;
    s16 charge;  // frames A has been held on the ground
    s16 boost;   // frames left of a Roc's climb punched through the glide
    s16 windT;   // frames the updraft has been running; drives spin, scroll and the burst
    u8 hopArmed; // A has been released since the hop started, so it may glide now
    s16 magicTimer;
    s16 timer;
    s16 yaw;                    // heading, accumulated the same way
    PlayerActionFunc flyAction; // actionFunc at takeoff; a change means something took over
    LinkAnimationHeader* charge_;
    LinkAnimationHeader* launch;
    LinkAnimationHeader* fly;
    LinkAnimationHeader* land;
    LinkAnimationHeader* hopClips[4]; // by controlStickDirection: front, left, back, right
    LinkAnimationHeader* hopSettle;   // the pose a hop holds once its strike is done
    LinkAnimationHeader* throwItem;
} RitoFlight;

static RitoFlight sRito;

static struct {
    u8 installed;
    LinkAnimationHeader* savedLanding[PLAYER_ANIMTYPE_MAX];
    LinkAnimationHeader* savedShort[PLAYER_ANIMTYPE_MAX];
} sRitoAnimTables;

// Landing the way the Gerudo does it. She does NOT hand-play a touchdown clip: she
// swaps the clip OOT's own landing groups point at (PLAYER_ANIMGROUP_landing /
// short_landing, sMhrGroupBindings). Then EVERY landing uses it -- off a glide, off a
// plain fall, off a hop -- with vanilla still owning the landing logic. Hand-playing
// it, which is what this file did, only ever covered the glide path, which is why
// dropping out of the air landed on Link's animation.
static void MmForm_RitoInstallLanding(void) {
    s32 col;

    if (sRitoAnimTables.installed || (sRito.land == NULL)) {
        return;
    }
    sRitoAnimTables.installed = 1;
    for (col = 0; col < PLAYER_ANIMTYPE_MAX; col++) {
        sRitoAnimTables.savedLanding[col] = ExtPlayer_GetAnimGroupAnim(PLAYER_ANIMGROUP_landing, col);
        sRitoAnimTables.savedShort[col] = ExtPlayer_GetAnimGroupAnim(PLAYER_ANIMGROUP_short_landing, col);
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_landing, col, sRito.land);
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_short_landing, col, sRito.land);
    }
}

// Restored on form exit so nothing leaks into Link.
extern "C" void MmForm_RitoRestoreLanding(void) {
    s32 col;

    if (!sRitoAnimTables.installed) {
        return;
    }
    sRitoAnimTables.installed = 0;
    for (col = 0; col < PLAYER_ANIMTYPE_MAX; col++) {
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_landing, col, sRitoAnimTables.savedLanding[col]);
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_short_landing, col, sRitoAnimTables.savedShort[col]);
    }
}

// Resampled load — see the header note. Without this the clip plays at whatever
// rate it was authored at and reads as "no animation".
static LinkAnimationHeader* MmForm_RitoLoadClip(const char* path) {
    LinkAnimationHeader* raw;
    s16 frames;

    if ((path == NULL) || !ResourceMgr_FileExists(path)) {
        return NULL;
    }
    raw = ResourceMgr_LoadPlayerAnimAsHeader(path);
    if (raw == NULL) {
        return NULL;
    }
    frames = (s16)(((f32)raw->common.frameCount / RITO_ANIM_SPEED) + 0.5f);
    if (frames < 2) {
        frames = 2; // a 1-frame clip would finish the instant it starts
    }
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(path, 1, frames);
}

static void MmForm_RitoLoadClips(void) {
    if (sRito.loaded) {
        return;
    }
    sRito.loaded = 1;
    // The wing wind-up is OOT's own bow guard pose, taken as-is. It must NOT go through
    // MmForm_RitoLoadClip: that resampler rewrites the resource IN PLACE, so resampling a
    // vanilla clip would corrupt it for every Link that plays it afterwards.
    sRito.charge_ = (LinkAnimationHeader*)&gPlayerAnim_link_bow_defense_wait;
    sRito.launch = MmForm_RitoLoadClip(RITO_CLIP_LAUNCH);
    sRito.fly = MmForm_RitoLoadClip(RITO_CLIP_FLY);
    sRito.land = MmForm_RitoLoadClip(RITO_CLIP_LAND);
    sRito.hopClips[0] = MmForm_RitoLoadClip(RITO_CLIP_HOP_FRONT);
    sRito.hopClips[1] = MmForm_RitoLoadClip(RITO_CLIP_HOP_LEFT);
    sRito.hopClips[2] = MmForm_RitoLoadClip(RITO_CLIP_HOP_BACK);
    sRito.hopClips[3] = MmForm_RitoLoadClip(RITO_CLIP_HOP_RIGHT);
    sRito.hopSettle = MmForm_RitoLoadClip(RITO_CLIP_HOP_SETTLE);
    sRito.throwItem = MmForm_RitoLoadClip(RITO_CLIP_THROW);
    MmForm_RitoInstallLanding();
    if (sRito.fly == NULL) {
        SPDLOG_WARN("[Rito] flight clip missing ({}) — A will not fly", RITO_CLIP_FLY);
    }
}

// PAUSE is re-armed at every clip start, not once on entry: a vanilla path that
// cleared it in between would advance the clip a second time (Trident_StartClip's
// own reason for doing it here).
static void MmForm_RitoPlay(PlayState* play, Player* player, LinkAnimationHeader* anim, u8 loop, f32 speed) {
    f32 last;
    f32 start;
    f32 end;

    if (anim == NULL) {
        return;
    }
    last = Animation_GetLastFrame(anim);
    start = (speed < 0.0f) ? last : 0.0f; // a negative speed means "play it backwards"
    end = (speed < 0.0f) ? 0.0f : last;

    // PAUSE is re-armed at EVERY clip start, and again every frame in the states
    // below: Player_UpdateCommon clears it (MmForm_GerudoPlant carries the same note).
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;

    // ONE track: player->skelAnime. The rito's body follows because MmForm_UsesOotAnim
    // copies OOT's joints onto the form by default — so this action must NOT be listed
    // as form-specific there. Doing both (driving formSkelAnime *and* flagging the
    // action) is what broke the poses.
    LinkAnimation_Change(play, &player->skelAnime, anim, speed, start, end, loop ? ANIMMODE_LOOP : ANIMMODE_ONCE,
                         -4.0f);
    sRito.timer = 0;
}

// A health drop is the one damage signal that cannot be missed: PLAYER_STATE1_DAMAGED is
// raised and cleared inside a single action chain, and both controllers run after it.
static struct {
    s16 lastHealth;
    u8 hurt;
} sRitoDamage;

static void MmForm_RitoTickDamage(void) {
    s16 health = gSaveContext.health;

    sRitoDamage.hurt = (sRitoDamage.lastHealth > 0) && (health < sRitoDamage.lastHealth);
    sRitoDamage.lastHealth = health;
}

// OOT has the frame back. Asked before any MHR clip ticks, because a clip that keeps
// re-arming PAUSE_ACTION_FUNC pauses OOT's own recovery along with it.
static u8 MmForm_RitoOotTookOver(Player* player, PlayerActionFunc owned) {
    if (sRitoDamage.hurt) {
        return 1;
    }
    if (player->stateFlags1 & (PLAYER_STATE1_DAMAGED | PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE)) {
        return 1;
    }
    return (owned != NULL) && (player->actionFunc != owned);
}

// Advance both tracks. Returns 1 when a one-shot clip has finished.
static s32 MmForm_RitoAdvance(PlayState* play, Player* player) {
    s32 done;

    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC; // see the note above
    return LinkAnimation_Update(play, &player->skelAnime);
}

extern "C" void MmForm_RitoResetFlight(void) {
    MmForm_RitoRestoreLanding();
    if (gPlayState != NULL) {
        // The controller's own teardown never runs once the form is gone, so the
        // ribbons have to be killed from here or they outlive the transformation.
        MmForm_RitoWindTrailsOff(gPlayState);
    }
    sRito.state = RITO_FLY_OFF;
    sRito.charge = 0;
    sRito.boost = 0;
    sRito.windT = 0;
    sRito.hopArmed = 0;
    sRito.magicTimer = 0;
    sRito.timer = 0;
    sRito.yaw = 0;
    sRito.flyAction = NULL;
}

// Roc's Feather / Roc's Cape in mid-air: a rito may use them as many times as it
// likes, but each use costs magic. Answers 1 AND bills the cost, so the caller only
// has to ask once. Anything not a rito, on the ground, or short on magic gets 0 and
// keeps whatever limit it already had.
// How far below the actor the rito's model is drawn. The mesh is correct in Blender;
// it simply sits high on the shared rig, so this is a DRAW-time offset, the same tool
// the Deku uses for its flower depth. shape.yOffset is MODEL space and the actor draws
// at scale 0.01, so 1 world unit = 100 here.
//
// The landing clip is authored standing on the floor, so during it the model must come
// back UP by RITO_LAND_LIFT or the rito sinks through the ground as it touches down.
extern "C" f32 MmForm_RitoDrawYOffset(Player* player) {
    if (player == NULL) {
        return RITO_DRAW_Y_BASE;
    }
    if ((sRito.land != NULL) && (player->skelAnime.animation == sRito.land)) {
        return RITO_DRAW_Y_BASE + RITO_LAND_LIFT;
    }
    return RITO_DRAW_Y_BASE + MmForm_RitoBowDrawLift(player);
}

extern "C" u8 MmForm_RitoAirRocsAllowed(Player* player) {
    if ((gFormState.currentForm != MM_PLAYER_FORM_RITO) || (player == NULL)) {
        return 0;
    }
    if (MMFORM_ON_GROUND(player)) {
        return 0; // on the ground the item behaves exactly as it always has
    }
    if ((gSaveContext.magicCapacity <= 0) || (gSaveContext.magic < RITO_ROCS_AIR_COST)) {
        return 0;
    }
    gSaveContext.magic -= RITO_ROCS_AIR_COST;
    // The glide pins velocity.y every frame, so without this the item took the magic
    // and bought nothing. Arming here — the one place that knows a rito just paid —
    // keeps the item files from needing to know the flight exists at all.
    sRito.boost = RITO_BOOST_FRAMES;
    return 1;
}

extern "C" u8 MmForm_RitoIsFlying(void) {
    return (gFormState.currentForm == MM_PLAYER_FORM_RITO) &&
           ((sRito.state == RITO_FLY_LAUNCH) || (sRito.state == RITO_FLY_GLIDE) || (sRito.state == RITO_FLY_FALL));
}

// Magic is billed for TIME in the air, not per wing beat: hovering is not cheaper
// than going somewhere. 0 means the meter just ran dry.
static u8 MmForm_RitoSpendLaunchMagic(void) {
    if ((gSaveContext.magicCapacity <= 0) || (gSaveContext.magic < RITO_LAUNCH_MAGIC_COST)) {
        return 0;
    }
    gSaveContext.magic -= RITO_LAUNCH_MAGIC_COST;
    return 1;
}

// ── the updraft ─────────────────────────────────────────────────────────────
// Revali's Gale: the ground kicks (a quake), a burst of green flames erupts and
// then settles down to a few circling the rito, and full-height wind curtains snake
// upward around him for as long as the charge is held.
//
// THE COLUMN is the shared wind cone (object_tornado.h — the gust jar's, built to be
// reused), stood on its tip and aimed straight up. Its texture is intensity+alpha, so
// the colour is entirely ours, and colour.a is the whole cone's fade: that is the
// "semi-alpha" knob. Tornado_RibbonsUpdate wraps it in spiral streaks and owns their
// blure slots — the engine has 25 in total, so leaking them starves every other trail
// in the scene, which is why the stop path is not optional.
//
// Tornado_GetAxis is axis.y = -sin(pitch), so straight up is pitch -0x4000.
#define RITO_WIND_PITCH_UP (-0x4000)
#define RITO_WIND_HEIGHT 82.0f // tip at the feet, mouth this far above
#define RITO_WIND_RADIUS 30.0f // mouth radius
#define RITO_WIND_SPIN 0x0900  // roll about the column, per frame
#define RITO_WIND_SCROLL 18    // streaks travelling UP the column, in quarter-texels
#define RITO_WIND_RIBBONS 5    // spiral streaks wrapping it
#define RITO_WIND_ALPHA 70     // barely there: the cone is a hint, the streaks carry it
// The streaks spread WIDER than the cone they wrap. Feeding the ribbons their own copy of
// the params is all it takes — Tornado_RibbonsUpdate lays them out from p->radius.
#define RITO_WIND_RIBBON_SPREAD 2.0f
// The column swells as the charge fills, so how far along it is readable at a glance.
#define RITO_WIND_GROW_MIN 0.35f
static const Color_RGB8 sRitoWindColor = { 190, 255, 200 }; // green-white, Revali's own

// The flames: many on the burst, RITO_FLAME_KEEP left circling once it settles.
#define RITO_FLAME_MAX 8
#define RITO_FLAME_KEEP 4
#define RITO_FLAME_BURST_FRAMES 7 // logic frames the full burst stays up
#define RITO_FLAME_RADIUS 17.0f
#define RITO_FLAME_BURST_RADIUS 40.0f
#define RITO_FLAME_HEIGHT 12.0f
#define RITO_FLAME_DRIFT 0x0700
// Index 6 in EnLight's D_80A9E840 is the green flame. It must be POSITIVE: bit 15 is
// the "small candle" variant, and EnLight_Draw's candle branch hardcodes orange
// (255,200,0) — only the point light stays green there, which is why a negative param
// gave an orange mote with a green glow. An even index also skips the Y-flip that
// EnLight_Draw applies on `params & 1`.
#define RITO_LIGHT_PARAMS 6
#define RITO_LIGHT_SCALE 0.0010f // a torch flame is 0.0075; these are motes

// How long the whole thing takes to die away once the rito leaves the ground.
#define RITO_WIND_FADE_FRAMES 14.0f

// The kick that starts it. Same call shape as the Mortal Draw's (equip_pendant.c), but
// snappier and much shallower: a jolt you feel rather than a shake you watch.
#define RITO_QUAKE_SPEED 32000
#define RITO_QUAKE_AMPLITUDE 2
#define RITO_QUAKE_FRAMES 5

static TornadoParams sRitoWind;
static TornadoRibbons sRitoWindRibbons;
static u8 sRitoWindOn;
// Where the updraft was raised. The whole effect is pinned here and does NOT follow the
// rito: he rides the wind up and out of it, the column stays on the ground he left.
static Vec3f sRitoWindOrigin;
static f32 sRitoWindFade;
static f32 sRitoWindGrow;
static Actor* sRitoFlames[RITO_FLAME_MAX];
static f32 sRitoFlamePhase[RITO_FLAME_MAX];

static void MmForm_RitoWindTrailsOff(PlayState* play) {
    s32 i;

    sRitoWindOn = 0;
    Tornado_RibbonsStop(play, &sRitoWindRibbons);
    // En_Light has no lifetime of its own — nothing in EnLight_Update ever kills one —
    // so whoever spawns it owns it until they say otherwise.
    for (i = 0; i < RITO_FLAME_MAX; i++) {
        if (sRitoFlames[i] != NULL) {
            Actor_Kill(sRitoFlames[i]);
            sRitoFlames[i] = NULL;
        }
    }
}

// A scene change destroys every effect and actor for us, so the updraft's bookkeeping
// must be FORGOTTEN, not freed: MmForm_Init deliberately keeps gFormState alive across
// a transition while transformed, and killing a pointer into the old scene's arena is
// exactly how that turns into a crash.
extern "C" void MmForm_RitoWindClear(void) {
    memset(&sRitoWindRibbons, 0, sizeof(sRitoWindRibbons));
    memset(sRitoFlames, 0, sizeof(sRitoFlames));
    sRitoWindOn = 0;
    sRitoWindFade = 0.0f;
    sRitoWindGrow = RITO_WIND_GROW_MIN;
    sRito.windT = 0;
}

// Frozen on purpose. EnLight_Update is the ONLY thing that plays NA_SE_EV_TORCH and the
// only thing that puts the light's radius back every frame, so replacing it is what makes
// these motes silent and stops them washing the ground in a green disc. The billboard
// survives: EnLight_Draw derives it from the camera itself. The one thing lost is the
// flame texture's scroll, which at this size reads as a mote either way.
static void MmForm_RitoFlameUpdate(Actor* thisx, PlayState* play) {
}

static Actor* MmForm_RitoSpawnFlame(PlayState* play) {
    Actor* flame = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, sRitoWindOrigin.x, sRitoWindOrigin.y,
                               sRitoWindOrigin.z, 0, 0, 0, RITO_LIGHT_PARAMS);

    if (flame != NULL) {
        // EnLight_Init sizes it as a torch flame; these are motes.
        Actor_SetScale(flame, RITO_LIGHT_SCALE);
        // Re-seat the light as a NO-GLOW point of radius 0. EnLight_Init made it a glowing
        // one — that is the wide green disc on the floor. The node keeps pointing at this
        // same LightInfo, so EnLight_Destroy still has something valid to remove.
        Lights_PointNoGlowSetInfo(&((EnLight*)flame)->lightInfo, (s16)sRitoWindOrigin.x, (s16)sRitoWindOrigin.y,
                                  (s16)sRitoWindOrigin.z, 0, 0, 0, 0);
        flame->update = MmForm_RitoFlameUpdate;
    }
    return flame;
}

static void MmForm_RitoWindTrailsOn(PlayState* play, Player* player) {
    s16 quake;
    s32 i;

    if (sRitoWindOn) {
        return;
    }
    sRito.windT = 0;
    sRitoWindOn = 1;
    sRitoWindFade = 1.0f;
    sRitoWindGrow = RITO_WIND_GROW_MIN;
    sRitoWindOrigin = player->actor.world.pos; // pinned here for the rest of its life
    // Everything erupts at once; MmForm_RitoWindTick culls it back down to
    // RITO_FLAME_KEEP once the burst is over.
    for (i = 0; i < RITO_FLAME_MAX; i++) {
        sRitoFlamePhase[i] = ((f32)i * (2.0f * M_PI / RITO_FLAME_MAX)) + Rand_ZeroFloat(0.6f);
        sRitoFlames[i] = MmForm_RitoSpawnFlame(play);
    }
    quake = Quake_Add(Play_GetCamera(play, 0), 3);
    Quake_SetSpeed(quake, RITO_QUAKE_SPEED);
    Quake_SetQuakeValues(quake, RITO_QUAKE_AMPLITUDE, 0, 0, 0);
    Quake_SetCountdown(quake, RITO_QUAKE_FRAMES);
}

// One frame of the updraft: the column rolls and its streaks travel up it, the flames
// circle. The cone itself is emitted later, from MmForm_RitoWindDraw.
static void MmForm_RitoWindTick(PlayState* play) {
    u8 bursting = (sRito.windT < RITO_FLAME_BURST_FRAMES);
    s32 i;

    sRito.windT++;
    // The wind only holds while it is being built. The moment the rito rides it off the
    // ground everything left behind dies away instead of cutting out.
    if (sRito.state == RITO_FLY_CHARGE) {
        f32 filled = (f32)sRito.charge / RITO_CHARGE_FULL;

        sRitoWindFade = 1.0f;
        if (filled > 1.0f) {
            filled = 1.0f;
        }
        sRitoWindGrow = RITO_WIND_GROW_MIN + ((1.0f - RITO_WIND_GROW_MIN) * filled);
    } else {
        sRitoWindFade -= (1.0f / RITO_WIND_FADE_FRAMES);
        if (sRitoWindFade <= 0.0f) {
            MmForm_RitoWindTrailsOff(play);
            return;
        }
    }

    sRitoWind.origin = sRitoWindOrigin;
    sRitoWind.pitch = RITO_WIND_PITCH_UP;
    sRitoWind.length = RITO_WIND_HEIGHT * sRitoWindGrow;
    sRitoWind.radius = RITO_WIND_RADIUS * sRitoWindGrow;
    sRitoWind.color.r = sRitoWindColor.r;
    sRitoWind.color.g = sRitoWindColor.g;
    sRitoWind.color.b = sRitoWindColor.b;
    sRitoWind.color.a = (u8)(RITO_WIND_ALPHA * sRitoWindFade);
    sRitoWind.spin += RITO_WIND_SPIN;
    // Positive scrollT runs the pattern from the tip toward the mouth. The tip is on the
    // ground, so that is the streaks climbing — which is the whole point of the effect.
    Tornado_AdvanceScroll(&sRitoWind, 0, RITO_WIND_SCROLL);
    {
        TornadoParams spread = sRitoWind;

        spread.radius *= RITO_WIND_RIBBON_SPREAD;
        Tornado_RibbonsUpdate(play, &sRitoWindRibbons, &spread, RITO_WIND_RIBBONS);
    }

    for (i = 0; i < RITO_FLAME_MAX; i++) {
        f32 ang = sRitoFlamePhase[i] + (BINANG_TO_RAD(RITO_FLAME_DRIFT) * sRito.windT);
        f32 radius = bursting ? RITO_FLAME_BURST_RADIUS : RITO_FLAME_RADIUS;

        if (sRitoFlames[i] == NULL) {
            continue;
        }
        // Once the burst is spent only a few stay, gathered in close around the column.
        if (!bursting && (i >= RITO_FLAME_KEEP)) {
            Actor_Kill(sRitoFlames[i]);
            sRitoFlames[i] = NULL;
            continue;
        }
        sRitoFlames[i]->world.pos = sRitoWindOrigin;
        sRitoFlames[i]->world.pos.x += Math_SinF(ang) * radius;
        sRitoFlames[i]->world.pos.z += Math_CosF(ang) * radius;
        sRitoFlames[i]->world.pos.y += RITO_FLAME_HEIGHT;
        Actor_SetScale(sRitoFlames[i], RITO_LIGHT_SCALE * sRitoWindFade);
    }
}

// The water void-out borrows the glide clip: a rito that cannot swim keeps beating its
// wings all the way down. That handler lives far above this file, hence the getter.
extern "C" LinkAnimationHeader* MmForm_RitoFlyAnim(void) {
    MmForm_RitoLoadClips();
    return sRito.fly;
}

// Emitted from MmForm_Draw. Separate from the tick because the cone is geometry on the
// XLU list, and only the update side knows where it should be.
extern "C" void MmForm_RitoWindDraw(PlayState* play) {
    if (sRitoWindOn) {
        Tornado_Draw(play, &sRitoWind);
    }
}

static void MmForm_RitoEnterAir(PlayState* play, Player* player) {
    // A clean idle action underneath, then PAUSE on top of it, so nothing of
    // vanilla's runs while the flight owns Link (Trident_FlyEnter's opening).
    func_80839FFC(player, play);
    sRito.flyAction = player->actionFunc;
    player->actor.bgCheckFlags &= ~1; // leave the ground this frame
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->stateFlags1 |= PLAYER_STATE1_JUMPING;
    Camera_ChangeMode(GET_ACTIVE_CAM(play), CAM_MODE_JUMP);
    sRito.yaw = player->actor.shape.rot.y; // keep facing where it took off
}

static void MmForm_RitoRelease(Player* player, u8 land) {
    player->stateFlags3 &= ~(PLAYER_STATE3_PAUSE_ACTION_FUNC | PLAYER_STATE3_MIDAIR);
    player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;
    player->actor.gravity = RITO_FLY_GRAVITY_DEFAULT;
    player->actor.minVelocityY = -20.0f;
    sRito.boost = 0;
    sRito.state = land ? RITO_FLY_LAND : RITO_FLY_OFF;
}

// Zora free-swim, in the air. Straight off Odolwa's moth-cloud flight
// (BossRemains_OdolwaFlightTick): yaw and pitch are ACCUMULATED from the stick and
// INVERTED (stick up = nose down) — that is what makes it a free 3D swim instead of
// "aim somewhere and go". The rito flies slower and turns lazier than the cloud.
static void MmForm_RitoGlideMove(PlayState* play, Player* player) {
    Input* in = &play->state.input[0];

    // Steering is a slow BANK and NOTHING else: sides only, gentle enough that you
    // cannot spin round to look behind you. Rate and deadzone are the Pegasus Boots
    // dash's (equip_pegasus.c:265), which is the small-turn feel asked for.
    if (fabsf(in->rel.stick_x) > RITO_TURN_DEADZONE) {
        sRito.yaw -= (s16)(in->rel.stick_x * RITO_TURN_RATE);
    }
    player->actor.world.rot.y = sRito.yaw;
    player->actor.shape.rot.y = sRito.yaw;
    player->yaw = sRito.yaw;

    // Forward only. The stick does NOT aim up or down — no climbing, no diving; the
    // rito flies level and sinks slowly, and altitude comes from the launch alone.
    player->linearVelocity = RITO_GLIDE_SPEED;
    if (sRito.boost <= 0) {
        player->actor.velocity.y = RITO_GLIDE_SINK;
    }
    player->actor.gravity = 0.0f;
}

// ── the controller ──────────────────────────────────────────────────────────
// Returns 1 on the frames it is driving Link, mirroring MmForm_GerudoMhrUpdate.
static u8 MmForm_RitoFlightUpdate(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];
    u8 aHeld;

    if (gFormState.currentForm != MM_PLAYER_FORM_RITO) {
        return 0;
    }
    // The updraft outlives the state that raised it: the rito leaves the ground, the
    // column stays behind and dies away on its own clock. So it is ticked from ONE place,
    // ahead of every early return below, and MmForm_RitoWindTick is what ends it.
    if (sRitoWindOn) {
        MmForm_RitoWindTick(play);
    }
    MmForm_RitoLoadClips();
    if (MmForm_InputOwnedByMessage()) {
        return 0; // a textbox or the ocarina owns the buttons
    }
    aHeld = CHECK_BTN_ALL(input->cur.button, BTN_A);
    // Only the glide pins velocity.y, so only the glide needs breaking out of. Armed
    // anywhere else the boost has nothing to do, and leaving it set would fire a
    // phantom climb the next time a glide started.
    if ((sRito.boost > 0) && (sRito.state != RITO_FLY_GLIDE)) {
        sRito.boost = 0;
    }
    // One teardown point instead of one per exit: the moment the state stops being
    // "building or riding the wind", the ribbons go.

    // Water is a hard stop now that the rito sinks: hand it straight to the form's
    // void-out instead of letting a glide skim the surface forever.
    if ((sRito.state != RITO_FLY_OFF) && (player->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
        MmForm_RitoRelease(player, 0);
        return 0;
    }
    // The charge is exempt from the action-function half only: it installs its own idle
    // and would otherwise abort on its opening frame.
    if ((sRito.state != RITO_FLY_OFF) &&
        MmForm_RitoOotTookOver(player, (sRito.state == RITO_FLY_CHARGE) ? NULL : sRito.flyAction)) {
        MmForm_RitoRelease(player, 0);
        return 0;
    }

    switch (sRito.state) {
        case RITO_FLY_CHARGE: {
            // Wind builds under the rito while A is down. The clip is held on the
            // wings-up frame instead of looping.
            sRito.charge++;
            player->linearVelocity = 0.0f;
            // Tick it. LinkAnimation_Change only ARMS the clip — Update is what writes
            // the pose into the joint table, so a state that never ticked (this one)
            // showed no animation at all on the ground.
            MmForm_RitoAdvance(play, player);
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            if ((sRito.charge % 4) == 0) {
                Player_PlaySfx(&player->actor, NA_SE_EN_KAICHO_FLUTTER);
            }

            // A full column lets go by itself — there is no sitting on a charged updraft.
            if (aHeld && (sRito.charge < RITO_CHARGE_FULL)) {
                return 1; // keep charging
            }
            // Released: ride the wind up. A short tap gives a small hop of a launch.
            if (sRito.charge < RITO_CHARGE_MIN) {
                MmForm_RitoRelease(player, 0);
                return 0;
            }
            // THE one charge: getting off the ground. Nothing else about flying costs.
            if (!MmForm_RitoSpendLaunchMagic()) {
                Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
                MmForm_RitoRelease(player, 0);
                return 0;
            }
            {
                f32 t = (f32)sRito.charge / RITO_CHARGE_FULL;
                if (t > 1.0f) {
                    t = 1.0f;
                }
                MmForm_RitoEnterAir(play, player);
                player->actor.velocity.y = RITO_LAUNCH_MIN + ((RITO_LAUNCH_MAX - RITO_LAUNCH_MIN) * t);
                player->actor.gravity = 0.0f;
                sRito.state = RITO_FLY_LAUNCH;
                MmForm_RitoPlay(play, player, sRito.launch, 0, 1.0f);
                Player_PlaySfx(&player->actor, NA_SE_PL_ROLL);
            }
            return 1;
        }

        case RITO_FLY_LAUNCH:
            sRito.timer++;
            player->actor.velocity.y -= 0.8f; // the push runs out
            // Once the climb tops out, A decides: glide on, or fall.
            if ((player->actor.velocity.y <= 1.0f) || (sRito.timer > 20)) {
                if (aHeld) {
                    sRito.state = RITO_FLY_GLIDE;
                    MmForm_RitoPlay(play, player, sRito.fly, 1, 1.0f);
                } else {
                    sRito.state = RITO_FLY_FALL;
                    MmForm_RitoRelease(player, 0);
                    return 0;
                }
            }
            return 1;

        case RITO_FLY_GLIDE:
            if (MMFORM_ON_GROUND(player)) {
                // Hand back to vanilla: OOT runs its own landing, and the clip it
                // plays IS the rito's because of the group swap above.
                MmForm_RitoRelease(player, 0);
                Player_PlaySfx(&player->actor, NA_SE_PL_LAND);
                return 0;
            }
            // Let go of A and the wings stop — that is the whole fall condition.
            if (!aHeld) {
                MmForm_RitoRelease(player, 0);
                return 0;
            }
            MmForm_RitoGlideMove(play, player);
            // A Roc's punched through the glide (MmForm_RitoAirRocsAllowed armed it as
            // it took the magic): stop holding level, climb on the item's own velocity
            // with the launch clip and the updraft on, then settle back into the glide.
            if (sRito.boost > 0) {
                if (sRito.boost == RITO_BOOST_FRAMES) {
                    MmForm_RitoWindTrailsOn(play, player);
                    MmForm_RitoPlay(play, player, sRito.launch, 0, 1.0f);
                    Player_PlaySfx(&player->actor, NA_SE_PL_ROLL);
                }
                player->actor.velocity.y -= 0.8f; // the push runs out, exactly as the launch's does
                if ((--sRito.boost <= 0) || (player->actor.velocity.y <= RITO_GLIDE_SINK)) {
                    sRito.boost = 0;
                    // The column is not torn down here — it is left behind, mid-air this
                    // time, and fades on its own like the one raised from the ground.
                    MmForm_RitoPlay(play, player, sRito.fly, 1, 1.0f);
                }
            }
            MmForm_RitoAdvance(play, player);
            if ((++sRito.timer % 8) == 0) {
                Player_PlaySfx(&player->actor, NA_SE_EN_KAICHO_FLUTTER);
            }
            return 1;

        case RITO_FLY_LAND:
            if (MmForm_RitoAdvance(play, player) || (++sRito.timer > 16)) {
                sRito.state = RITO_FLY_OFF;
                player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
                return 0;
            }
            return 1;

        case RITO_FLY_HOP:
            // A during the hop turns it into real flight, so a hop can be extended —
            // but ONLY after A has been let go of once. Roll, backflip and side hop are
            // all STARTED by A, so the button is still down on the frames right after,
            // and testing it raw turned every single hop into an instant glide.
            if (!aHeld) {
                sRito.hopArmed = 1;
            }
            if (sRito.hopArmed && aHeld && (sRito.fly != NULL)) {
                MmForm_RitoEnterAir(play, player);
                sRito.state = RITO_FLY_GLIDE;
                MmForm_RitoPlay(play, player, sRito.fly, 1, 1.0f);
                return 1;
            }
            sRito.timer++;
            if (MMFORM_ON_GROUND(player) && (sRito.timer > RITO_HOP_LAND_GUARD)) {
                MmForm_RitoRelease(player, 0);
                Player_PlaySfx(&player->actor, NA_SE_PL_LAND);
                return 0;
            }
            // Comparing the running clip is the re-entry guard: the settle pose loops,
            // so Advance never reports it finished and this can only fire once.
            if (MmForm_RitoAdvance(play, player) && (sRito.hopSettle != NULL) &&
                (player->skelAnime.animation != sRito.hopSettle)) {
                MmForm_RitoPlay(play, player, sRito.hopSettle, 1, 1.0f);
                sRito.timer = RITO_HOP_LAND_GUARD; // RitoPlay zeroes it; the takeoff guard is spent
            }
            return 1;

        case RITO_FLY_THROW:
            if (MmForm_RitoAdvance(play, player) || (++sRito.timer > 20)) {
                sRito.state = RITO_FLY_OFF;
                player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
                return 0;
            }
            return 1;

        default:
            break;
    }

    // Idle: A starts the charge on the ground, or grabs flight straight away in the air.
    if (!CHECK_BTN_ALL(input->press.button, BTN_A) || (player->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE)) {
        return 0;
    }
    if (MMFORM_ON_GROUND(player)) {
        if ((sRito.charge_ == NULL) || (fabsf(player->linearVelocity) > 1.0f)) {
            return 0; // moving: leave the roll/hop path alone
        }
        func_80839FFC(player, play);
        sRito.flyAction = player->actionFunc;
        sRito.state = RITO_FLY_CHARGE;
        sRito.charge = 0;
        MmForm_RitoWindTrailsOn(play, player);
        MmForm_RitoPlay(play, player, sRito.charge_, 1, 1.0f); // already a wait loop
        return 1;
    }
    if (sRito.fly != NULL) { // gliding is free — holding A is the whole cost
        MmForm_RitoEnterAir(play, player);
        sRito.state = RITO_FLY_GLIDE;
        MmForm_RitoPlay(play, player, sRito.fly, 1, 1.0f);
        return 1;
    }
    return 0;
}

// Roll and every dodge hop → Roc's-Feather hops. z_player.c asks before starting one.
// `dir` is OOT's controlStickDirection (1 side-left, 2 backflip, 3 side-right), or -1
// for the roll, which is the forward hop and takes the front clip. Only the roll gets
// the taller 1.5x height; the three dodges are 1x.
extern "C" u8 MmForm_RitoTryHop(Player* player, PlayState* play, s32 dir) {
    u8 isRoll = (dir < 0);
    LinkAnimationHeader* clip;

    if (gFormState.currentForm != MM_PLAYER_FORM_RITO) {
        return 0;
    }
    MmForm_RitoLoadClips();
    clip = sRito.hopClips[isRoll ? 0 : (dir & 3)];
    if (clip == NULL) {
        return 0;
    }

    MmForm_RitoEnterAir(play, player);
    player->actor.velocity.y = RITO_ROCS_VELOCITY * (isRoll ? RITO_HOP_SCALE : RITO_BACKFLIP_SCALE);
    player->actor.gravity = RITO_FLY_GRAVITY_DEFAULT;
    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING; // ledges stay grabbable, as Roc's does
    // A hop that only goes up is not a dodge any more, so each direction keeps its
    // ground travel at OOT's own speed. Only the backflip is purely vertical, which is
    // the arc it already had. NOTE: the roll is dir -1 and `-1 & 1` is 1 in C, so the
    // side-hop test has to exclude it explicitly or the forward hop veers left.
    if (isRoll || (dir & 1)) {
        player->yaw = player->actor.shape.rot.y + (isRoll ? 0 : (dir << 0xE));
        player->linearVelocity = isRoll ? 6.0f : 8.5f;
        // PAUSE_ACTION_FUNC means no vanilla action syncs yaw into world.rot.y, and
        // world.rot.y is what Actor_MoveXZGravity actually steers by.
        player->actor.world.rot.y = player->yaw;
    }
    sRito.state = RITO_FLY_HOP;
    sRito.timer = 0;
    sRito.hopArmed = 0;
    MmForm_RitoPlay(play, player, clip, 0, 1.0f);
    Player_PlaySfx(&player->actor, NA_SE_PL_SKIP);
    return 1;
}

// Items the rito may throw while airborne: bombs, Deku nuts, the SW97 elemental
// seeds (slingshot ammo) and the boomerang. OOT still spawns and throws them —
// this only puts the clip on the body while that happens.
extern "C" u8 MmForm_RitoTryAirThrow(Player* player, PlayState* play, s32 itemAction) {
    if (gFormState.currentForm != MM_PLAYER_FORM_RITO) {
        return 0;
    }
    if (MMFORM_ON_GROUND(player) || (sRito.throwItem == NULL)) {
        return 0;
    }
    switch (itemAction) {
        case PLAYER_IA_BOMB:
        case PLAYER_IA_BOMBCHU:
        case PLAYER_IA_DEKU_NUT:
        case PLAYER_IA_SLINGSHOT:
        case PLAYER_IA_BOOMERANG:
            break;
        default:
            return 0;
    }
    sRito.state = RITO_FLY_THROW;
    sRito.timer = 0;
    sRito.flyAction = player->actionFunc; // whatever it interrupted is what it must yield to
    MmForm_RitoPlay(play, player, sRito.throwItem, 0, RITO_THROW_SPEED);
    return 1;
}
