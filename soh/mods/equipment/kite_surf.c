/**
 * kite_surf.c — Kite Shield SHIELD SURFING engine.
 *
 * State and predicates live in mods/equipment/behaviors/equip_kite_shield.c, which rides the
 * ext-equipment unity build at the TOP of z_player.c. This file is included MUCH later, right after
 * Player_UpdateCommon, because everything it drives the player with is defined in between:
 * Player_GetSlopeDirection (8866), Player_GetRelativePosition (6423), Player_GetMovementSpeedAndYaw
 * (4847), Player_ProcessItemButtons (2901), func_80837948 (5261), the Player_ActionHandler_* family
 * and the GET_PLAYER_ANIM tables.
 *
 * KiteSurf_Tick is called from ExtEquip_UpdateBehavior (z_player.c:14342) — AFTER Player_UpdateCommon
 * has returned, the same post-action slot BossRemains_GohtPostAction uses so speed/anim overrides win.
 *
 * TAKEOVER CONTRACT (equip_trident.c:1465-1475, verified against z_player.c):
 *   · PLAYER_STATE3_PAUSE_ACTION_FUNC is re-asserted every frame — z_player.c:14094 clears it, and
 *     that clear runs before our hook, so a single write only ever buys the next frame.
 *   · Because the action func is paused, NOTHING advances skelAnime — we call LinkAnimation_Update
 *     ourselves.
 *   · PLAYER_STATE3_MIDAIR must be held while airborne or func_8083AA10 yanks Link into the fall
 *     action right through the pause.
 *   · linearVelocity + yaw written here become real world velocity next frame with or without the
 *     pause; the engine keeps doing gravity and scene collision, which is exactly what we want.
 *   · Player_SetupAction is NOT gated by the pause. actionFunc changing under us means damage or a
 *     cutscene took the player — abort and let it run.
 */

// ---------------------------------------------------------------------------
// Tunables — now IMMUTABLE (2026-08-20). These are the values that came out of tuning in game; the
// CVar reads that used to wrap them are gone, so nothing at runtime can move them any more. The
// "Configure Kite Shield" popup was removed first, and this is the second half of that decision:
// with no UI writing the CVars, leaving the reads in only meant a stale gItemEditor.KiteSurf.* in
// someone's config could silently override a dialled-in constant. Re-tuning = editing these.
// Same call that was made for the Sheikah Slate and the Trident.
// ---------------------------------------------------------------------------

#define KSURF_MOUNT_FRAMES 8
#define KSURF_SLOPE_ACCEL 18.28f // multiplied by (1 - floorNormal.y) and by the downhill alignment
#define KSURF_FRICTION 0.1f      // per frame toward 0 — "flat bleeds speed very slowly"
#define KSURF_STICK_ACCEL 0.09f  // the small shove that stops him getting stuck on flat ground
#define KSURF_TURN_MAX 1310.98f
#define KSURF_TURN_MIN 250.0f
#define KSURF_STOP_SPEED 0.6f
#define KSURF_STOP_FRAMES 10
#define KSURF_HOP_VEL 9.0f // a shade above a normal jump (~5.8-8.5), not a launch
#define KSURF_SPIN_FRAMES 24
#define KSURF_SPIN_CHANCE 0.552f      // odds a hop throws a board shuvit
#define KSURF_BOARD_SPIN_RATE 3000.0f // binang per frame — a full turn in about 22 frames
// Rail = a narrow, LONG piece of floor (a beam, a ledge, a raised path), found by floor raycasts
// looking for where the ground ends on either side. See KiteSurf_UpdateRail.
// The defaults below are MEASURED, not guessed. Parsing spot00's collision (the Hyrule Field
// fences) out of oot.o2r gives the shape of a real target:
//   · the long fence is a strip 20 units wide and 1500 long — a HALF-WIDTH OF 10;
//   · the ground beside it drops 40 on one side and 240 on the other;
//   · the shorter walls in the same scene are 40 wide with drops of 250+.
// So everything here is sized for half-widths of 10-20 and drops of 40, with margin under it so
// similar spots in other scenes qualify too.
#define KSURF_RAIL_PROBE 100.27f    // lateral reach; /SAMPLES this is a 5-unit resolution
#define KSURF_RAIL_MAX_WIDTH 47.03f // ground narrower than this across = a rail
#define KSURF_RAIL_EDGE_DROP 2.0f   // floor this much below his own already counts as "fell away"
#define KSURF_RAIL_AHEAD 10.0f      // the strip has to keep going this far ahead to count as LONG
#define KSURF_RAIL_AHEAD_MAX 90.0f  // ceiling on that, because the floor plane is extrapolated over it
#define KSURF_RAIL_PROBE_UP 30.0f   // the floor rays start this high above him
#define KSURF_RAIL_SAMPLES \
    8                                 // steps per side when hunting the edge — the resolution of both
                                      // the width test and the centring
#define KSURF_RAIL_RING 16            // probes in the ring that finds the strip's own direction
#define KSURF_RAIL_RING_RADIUS 26.23f // must stay ABOVE half of MAX_WIDTH or nothing is recognised
#define KSURF_RAIL_RING_MAX_HITS 9    // more of the ring than this on solid ground = open floor
// Two different gates on how far off his heading the strip may run, because entering and staying
// want opposite things. ENTERING stays fussy so a strip crossing his path does not yank him onto
// it. Once he is ON it, it has to be loose enough to follow a corner: the Hyrule Field switchback
// turns 56, 67, -80 and -87 degrees, and a single 67-degree gate refused the last two — which is
// exactly the "it drops me at the right angle" case.
#define KSURF_RAIL_MAX_APPROACH 0x4000 // 67 deg, for grabbing a rail in the first place
#define KSURF_RAIL_MAX_TURN 0x5800     // 124 deg, for following one round a corner
#define KSURF_RAIL_GAP_STEPS 3         // forward samples that bridge a gap between fence segments
#define KSURF_RAIL_ATTRACT 90.0f       // how far out the magnet looks for a rail to drag him onto
#define KSURF_RAIL_ATTRACT_DIRS 8      // directions it sweeps
#define KSURF_RAIL_ATTRACT_RISE 30.0f  // it only takes rails within this much of his own height
#define KSURF_RAIL_ATTRACT_PULL 6.0f   // units per frame dragged toward one
#define KSURF_RAIL_SPEED 8.27f         // flat boost while railing
#define KSURF_RAIL_SNAP 60.0f          // cap on the per-frame centring; big enough that it is a PIN
#define KSURF_RAIL_GRACE 4
#define KSURF_RAIL_TURN 12000.0f    // how fast the heading tracks the strip as it bends
#define KSURF_RAIL_DETACH_FRAMES 30 // rail detection stays off this long after A let go of one
#define KSURF_TURN_FULL 0x0A00      // turn rate that counts as a full-strength carve (sKSurf.turn = 1)
#define KSURF_TURN_SMOOTH 0.15f     // how fast that carve value chases the real turn rate
#define KSURF_LEAN_SCALE 0.75f      // how much of the floor pitch reaches the model
#define KSURF_POSE_FRAME 0.0f       // frame of the slope-slide clip we freeze on
#define KSURF_BONK_YAW 0x2000

// (KSURF_BOARD_* and KSURF_CROUCH_DEG live in equip_kite_shield.c — extended_equipment.c draws the
// board and z_player_lib.c poses the limbs, both of which come long before this file.)

#define KSURF_REMOUNT_LOCKOUT 25

// Frames left before going airborne can put him back on the board. Without it, dismounting in mid
// air (a bonk on a wall over a pit, B+R off a ledge) re-mounts on the very next frame forever.
static s16 sKSurfRemountLockout = 0;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

// The ISG-safe melee teardown: kill the swing state and the live blade quads, so nothing can keep
// an AT registered while we own the player — that is exactly how a stuck sword beam is born.
//
// It does NOT set meleeWeaponAnimation = -1. Player_StartDekuBubble (z_player.c:7796-7797) does,
// but only because it moves the action func off the melee attack in the same breath. On its own
// that is a crash: Player_Action_808502D0 opens with `&D_80854190[this->meleeWeaponAnimation]`, so
// a -1 left behind reads the struct BEFORE the table, hands the garbage LinkAnimationHeader* to
// Animation_GetLastFrame and dies inside ResourceMgr_OTRSigCheck. (Seen for real: entering the surf
// out of a jump slash, then dismounting — the pause is released and the stale melee action runs.)
// Parking on Player_Action_Idle is what makes the teardown safe, and it also gives the takeover
// check a known action to sit on.
static void KiteSurf_KillMeleeState(Player* player, PlayState* play) {
    player->meleeWeaponState = 0;
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->stateFlags2 &= ~PLAYER_STATE2_SPIN_ATTACKING;
    player->unk_858 = 0.0f; // spin charge amount
    Collider_ResetQuadAT(play, &player->meleeWeaponQuads[0].base);
    Collider_ResetQuadAT(play, &player->meleeWeaponQuads[1].base);

    // Flag 1 keeps the shield up — he is standing on one.
    Player_SetupAction(play, player, Player_Action_Idle, 1);
}

// The riding pose: the vanilla DOWNHILL SLOPE SLIDE clip, held on one frame. That is the animation
// with the bent knees and the low centre of gravity — the same one Player_HandleSlopes puts on when
// the ground gives way under him (z_player.c:8891) — so the surf reads as a slide instead of a man
// standing on a shield. Frozen rather than looped: playSpeed 0 every frame, because the engine
// happily re-drives it otherwise.
static void KiteSurf_HoldPose(Player* player, PlayState* play) {
    LinkAnimationHeader* pose = (LinkAnimationHeader*)&gPlayerAnim_link_normal_down_slope_slip;
    f32 frame = KSURF_POSE_FRAME;

    if (player->skelAnime.animation != pose) {
        LinkAnimation_Change(play, &player->skelAnime, pose, 0.0f, frame, frame, ANIMMODE_ONCE, -4.0f);
    }
    player->skelAnime.playSpeed = 0.0f;
}

// (The lower-body crouch/lean is KiteSurf_AdjustLimb in equip_kite_shield.c — it has to run at
// draw time, see the comment there.)

// Hand the player back. Safe to call from any state, including from KiteShield_Cleanup when the
// shield is unequipped mid-ride.
void KiteSurf_Abort(Player* player) {
    if (sKSurf.state == KSURF_OFF) {
        return;
    }
    sKSurf.state = KSURF_OFF;
    sKSurf.timer = 0;
    sKSurf.stopFrames = 0;
    sKSurf.spinFrames = 0;
    sKSurf.railMiss = 0;
    sKSurf.railDetach = 0;
    sKSurf.boardSpin = 0;
    sKSurf.boardSpinRate = 0;
    sKSurf.leanPitch = 0;
    sKSurf.leanRoll = 0;
    sKSurf.upperLean = 0;
    sKSurf.turn = 0.0f;
    sKSurf.ownedAction = NULL;
    sKSurfRemountLockout = KSURF_REMOUNT_LOCKOUT;

    if (player != NULL) {
        // MIDAIR goes back too, or the engine never gives him the fall action again and he floats
        // through his own landing (the same pair Trident_FlyExit hands back).
        player->stateFlags3 &= ~(PLAYER_STATE3_PAUSE_ACTION_FUNC | PLAYER_STATE3_MIDAIR);
        player->skelAnime.playSpeed = 1.0f;
        player->actor.shape.rot.x = 0;
    }
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

static void KiteSurf_Start(Player* player, PlayState* play) {
    // Cancel whatever swing was in the air. Done BEFORE the animation change so nothing
    // re-registers the quads behind us.
    KiteSurf_KillMeleeState(player, play);

    // Take control from THIS frame, not from the next one. Entry always happens in mid air, and an
    // airborne frame that is neither paused nor flagged MIDAIR gets the action func replaced out
    // from under us (see the MIDAIR note in KiteSurf_Tick) — which the takeover check below then
    // reads as "something stole the player" and the surf dies before it starts.
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC | PLAYER_STATE3_MIDAIR;

    sKSurf.state = KSURF_MOUNT;
    sKSurf.timer = 0;
    sKSurf.stopFrames = 0;
    sKSurf.spinFrames = 0;
    sKSurf.railMiss = 0;
    sKSurf.railDetach = 0;
    sKSurf.boardSpin = 0;
    sKSurf.boardSpinRate = 0;
    sKSurf.leanPitch = 0;
    sKSurf.leanRoll = 0;
    sKSurf.upperLean = 0;
    sKSurf.turn = 0.0f;
    sKSurf.ownedAction = (void*)player->actionFunc;

    Player_AnimChangeOnceMorph(play, player, GET_PLAYER_ANIM(PLAYER_ANIMGROUP_put, player->modelAnimType));
    Player_PlaySfx(&player->actor, NA_SE_IT_SHIELD_POSTURE);
}

// Everything that must be true to be allowed to ride at all.
static u8 KiteSurf_Allowed(Player* player) {
    if (player->stateFlags1 &
        (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING | PLAYER_STATE1_IN_ITEM_CS |
         PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_WATER | PLAYER_STATE1_CLIMBING_LADDER |
         PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HOOKSHOT_FALLING |
         PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_TALKING | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_DAMAGED |
         PLAYER_STATE1_INPUT_DISABLED)) {
        return 0;
    }
    if (player->actor.bgCheckFlags & BGCHECKFLAG_WATER) {
        return 0;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Rail detection — a rail is a piece of FLOOR that is narrow and long: a beam, a ledge, a fence
// top, a raised path. Not a corridor between two walls. So every probe here is a floor raycast
// looking for where the ground ENDS or falls away.
//
// THE AXIS IS FOUND FIRST, AND WITHOUT REFERENCE TO WHERE HE IS LOOKING. That is the fix for "it
// never grabs". Both earlier versions measured the strip sideways from his own heading, and that
// can only work when he is already lined up with it: come at a beam at an angle and the sideways
// cut crosses it diagonally and reads far wider than it is ("too wide, not a rail"), come at it
// square and the cut runs ALONG the beam and finds no edge at all. The one case it handled is the
// one case that needed no help.
//
// Instead, fire a RING of floor probes around him. On a narrow strip the probes that land on solid
// ground form two opposite arcs, pointing along the strip in both directions; on open ground they
// all land, which is how open ground is told apart for free. The arc whose middle is closest to the
// way he is already going wins — so a rail catches him travelling the way he was already travelling,
// and the same reading tracks the strip round a bend on later frames.
// ---------------------------------------------------------------------------

// Floor height under a world XZ, or BGCHECK_Y_MIN when there is nothing there.
static f32 KiteSurf_FloorAtWorld(PlayState* play, Player* player, f32 x, f32 z) {
    Vec3f probe;
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;

    probe.x = x;
    probe.y = player->actor.world.pos.y + KSURF_RAIL_PROBE_UP;
    probe.z = z;

    return BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &probe);
}

// Is the ground at this world XZ part of the strip he is standing on?
//
// Measured against the PLANE of the poly under his feet, not against a flat height. Rails slope:
// the Hyrule Field path is a switchback whose end ramps run at 17 and 19 degrees, which over the
// lookahead distance is 26 units of fall — twice the edge-drop budget. Comparing to a flat height
// read that as "the ground fell away, the strip has ended" and refused the rail on exactly the
// ramps. Against the strip's own plane a ramp reads as zero deviation, which is what it is.
//
// Symmetric on purpose: ground well ABOVE the plane is just as much an edge as ground below, so a
// sunken channel or a path hugging a wall counts as a rail too.
static u8 KiteSurf_OnStrip(PlayState* play, Player* player, f32 x, f32 z, f32 drop) {
    f32 y = KiteSurf_FloorAtWorld(play, player, x, z);
    Vec3f point;

    if (y <= BGCHECK_Y_MIN) {
        return 0; // nothing there at all
    }
    if (player->actor.floorPoly == NULL) {
        return 1;
    }

    point.x = x;
    point.y = y;
    point.z = z;
    return fabsf(CollisionPoly_GetPointDistanceFromPlane(player->actor.floorPoly, &point)) <= drop;
}

// On-strip test at an angle and distance from him.
static u8 KiteSurf_OnStripAt(PlayState* play, Player* player, s16 yaw, f32 dist, f32 drop) {
    return KiteSurf_OnStrip(play, player, player->actor.world.pos.x + (dist * Math_SinS(yaw)),
                            player->actor.world.pos.z + (dist * Math_CosS(yaw)), drop);
}

// ---------------------------------------------------------------------------
// The magnet. Detection above only ever looks at the ground he is ALREADY standing on, so a rail
// one step to the side is invisible to it. This sweeps for one nearby and drags him across onto it,
// after which the normal detection takes over and pins him.
//
// Deliberately HORIZONTAL only, and gated on the candidate being at roughly his own height: a
// fence top sits 40 to 240 units above the field beside it, and hauling him up onto that would mean
// launching him vertically through the fence's side. This grabs rails you are level with — drifting
// off the beam you were riding, the next ledge along, a path beside a path.
// ---------------------------------------------------------------------------

// Floor height AND the poly there, so a candidate spot can be judged against its own plane rather
// than against the one under Link.
static f32 KiteSurf_FloorPolyAt(PlayState* play, Player* player, f32 x, f32 z, CollisionPoly** outPoly) {
    Vec3f probe;
    s32 bgId = BGCHECK_SCENE;

    *outPoly = NULL;
    probe.x = x;
    probe.y = player->actor.world.pos.y + KSURF_RAIL_PROBE_UP;
    probe.z = z;

    return BgCheck_EntityRaycastFloor3(&play->colCtx, outPoly, &bgId, &probe);
}

// Has the ground fallen away from `poly`'s plane at this spot?
static u8 KiteSurf_OffPlane(PlayState* play, Player* player, CollisionPoly* poly, f32 x, f32 z, f32 drop) {
    CollisionPoly* ignored;
    f32 y = KiteSurf_FloorPolyAt(play, player, x, z, &ignored);
    Vec3f point;

    if (y <= BGCHECK_Y_MIN) {
        return 1; // no ground at all is as "off" as it gets
    }
    point.x = x;
    point.y = y;
    point.z = z;
    return fabsf(CollisionPoly_GetPointDistanceFromPlane(poly, &point)) > drop;
}

// Sweep for a rail near him and drag him toward it. Returns 1 while it is pulling.
static u8 KiteSurf_Attract(Player* player, PlayState* play, f32 drop) {
    f32 range = KSURF_RAIL_ATTRACT;
    f32 rise = KSURF_RAIL_ATTRACT_RISE;
    f32 pull = KSURF_RAIL_ATTRACT_PULL;
    f32 side = KSURF_RAIL_RING_RADIUS;
    s32 stepAng = 0x10000 / KSURF_RAIL_ATTRACT_DIRS;
    s16 heading = player->actor.shape.rot.y;
    s32 bestDiff = 0x7FFFFFFF;
    f32 bestX = 0.0f;
    f32 bestZ = 0.0f;
    u8 found = 0;
    s32 k;

    for (k = 0; k < KSURF_RAIL_ATTRACT_DIRS; k++) {
        s16 dir = (s16)(k * stepAng);
        f32 cx = player->actor.world.pos.x + (range * Math_SinS(dir));
        f32 cz = player->actor.world.pos.z + (range * Math_CosS(dir));
        CollisionPoly* poly;
        f32 cy = KiteSurf_FloorPolyAt(play, player, cx, cz, &poly);
        s16 across;
        s32 diff;

        if ((cy <= BGCHECK_Y_MIN) || (poly == NULL)) {
            continue;
        }
        if (fabsf(cy - player->actor.floorHeight) > rise) {
            continue; // a cliff or a rooftop, not something to slide across onto
        }

        // Narrow at that spot? Check across the line from him to it — a strip he can be dragged
        // sideways onto runs roughly along his own heading, so that line cuts it the short way.
        across = dir + 0x4000;
        if (!KiteSurf_OffPlane(play, player, poly, cx + (side * Math_SinS(across)), cz + (side * Math_CosS(across)),
                               drop) ||
            !KiteSurf_OffPlane(play, player, poly, cx - (side * Math_SinS(across)), cz - (side * Math_CosS(across)),
                               drop)) {
            continue;
        }

        diff = ABS((s16)(dir - heading));
        if (diff < bestDiff) {
            bestDiff = diff;
            bestX = cx;
            bestZ = cz;
            found = 1;
        }
    }

    if (!found) {
        return 0;
    }

    Math_StepToF(&player->actor.world.pos.x, bestX, pull);
    Math_StepToF(&player->actor.world.pos.z, bestZ, pull);
    return 1;
}

// How far out to one side of `yaw` the ground lasts before it ends or drops away.
// Returns 0 when it never does inside `reach` — solid ground out there, so this is not a strip.
static f32 KiteSurf_EdgeDistance(PlayState* play, Player* player, s16 yaw, f32 sign, f32 reach, f32 drop) {
    f32 step = reach / (f32)KSURF_RAIL_SAMPLES;
    s16 side = yaw + (s16)((sign > 0.0f) ? 0x4000 : -0x4000);
    s32 i;

    for (i = 1; i <= KSURF_RAIL_SAMPLES; i++) {
        f32 d = step * (f32)i;

        if (!KiteSurf_OnStripAt(play, player, side, d, drop)) {
            return d;
        }
    }
    return 0.0f;
}

// The ring. Returns 0 when this is not a narrow strip; otherwise outYaw is the strip's own
// direction, chosen as the arc closest to the way he is already heading.
static u8 KiteSurf_FindAxis(PlayState* play, Player* player, f32 drop, s16* outYaw) {
    u8 hit[KSURF_RAIL_RING];
    f32 radius = KSURF_RAIL_RING_RADIUS;
    s32 maxHits = (s32)(f32)KSURF_RAIL_RING_MAX_HITS;
    // Loose while riding one, fussy while looking for one — see the two defines.
    s32 maxApproach = (sKSurf.state == KSURF_RAIL) ? (s32)(f32)KSURF_RAIL_MAX_TURN : (s32)(f32)KSURF_RAIL_MAX_APPROACH;
    s32 stepAng = 0x10000 / KSURF_RAIL_RING;
    s16 heading = player->actor.shape.rot.y;
    s32 hits = 0;
    s32 origin = -1;
    s32 bestDiff = 0x7FFFFFFF;
    u8 found = 0;
    s32 pass;
    s32 k;

    // Two passes, the second at half the radius. On the thinnest rails — the Hyrule Field fence is
    // only 10 units either side of its centre line — a ring at full radius only ever catches the
    // two probes pointing exactly along the strip, and misses even those if he is a little off the
    // centre line. Pulling the ring in when the wide one comes back empty is what makes a fence
    // that thin catchable at all. It costs nothing on normal ground, where the first pass is full.
    for (pass = 0; pass < 2; pass++) {
        hits = 0;
        for (k = 0; k < KSURF_RAIL_RING; k++) {
            hit[k] = KiteSurf_OnStripAt(play, player, (s16)(k * stepAng), radius, drop);
            hits += hit[k];
        }
        if (hits != 0) {
            break;
        }
        radius *= 0.5f;
    }

    // Nothing to ride, or solid ground in every direction — that is open floor, not a strip.
    if ((hits == 0) || (hits > maxHits)) {
        return 0;
    }

    // Walk from a probe that BEGINS an arc, so an arc straddling index 0 is still seen whole.
    for (k = 0; k < KSURF_RAIL_RING; k++) {
        if (hit[k] && !hit[(k + KSURF_RAIL_RING - 1) % KSURF_RAIL_RING]) {
            origin = k;
            break;
        }
    }
    if (origin < 0) {
        return 0;
    }

    k = 0;
    while (k < KSURF_RAIL_RING) {
        s32 len = 0;
        s32 ang;
        s32 diff;

        if (!hit[(origin + k) % KSURF_RAIL_RING]) {
            k++;
            continue;
        }
        while ((len < KSURF_RAIL_RING) && hit[(origin + k + len) % KSURF_RAIL_RING]) {
            len++;
        }

        // Middle of this arc. Truncating the s32 to s16 IS the binang wrap.
        ang = (s32)((((f32)(origin + k)) + (((f32)len - 1.0f) * 0.5f)) * (f32)stepAng);
        diff = ABS((s16)((s16)ang - heading));

        if (diff < bestDiff) {
            bestDiff = diff;
            *outYaw = (s16)ang;
            found = 1;
        }
        k += len;
    }

    // Refuse a strip running across him: grabbing that would spin him on the spot rather than
    // carry him on the way he was going.
    return found && (bestDiff <= maxApproach);
}

static u8 KiteSurf_UpdateRail(Player* player, PlayState* play) {
    f32 maxWidth = KSURF_RAIL_MAX_WIDTH;
    f32 reach = KSURF_RAIL_PROBE;
    f32 drop = KSURF_RAIL_EDGE_DROP;
    f32 ahead = KSURF_RAIL_AHEAD;
    s16 axis;
    f32 dL;
    f32 dR;
    f32 centre;

    // --- 1. Which way does the strip run? Answered WITHOUT using his heading, which is what lets
    //        it grab from any approach angle instead of only when he is already lined up. ---
    if (!KiteSurf_FindAxis(play, player, drop, &axis)) {
        return 0;
    }

    // Publish the heading NOW, before the gates below can bail out. Standing right on a corner, the
    // width test reads across the new arm and finds the old arm's ground on one side, so it fails
    // for a frame or two — and those are exactly the frames that have to steer him round. The
    // KSURF_RAIL_GRACE window keeps him engaged through them, but only helps if the heading it
    // coasts on is the NEW one; coasting on the arm he came in along flies him straight off.
    if (sKSurf.state == KSURF_RAIL) {
        sKSurf.railAxisYaw = axis;
    }

    // --- 2. Narrow enough — measured across THAT AXIS, not across his heading. ---
    dL = KiteSurf_EdgeDistance(play, player, axis, -1.0f, reach, drop);
    dR = KiteSurf_EdgeDistance(play, player, axis, 1.0f, reach, drop);
    if ((dL == 0.0f) || (dR == 0.0f) || ((dL + dR) > maxWidth)) {
        return 0; // solid ground on a side means open floor, or a cliff — neither is a rail
    }
    centre = (dR - dL) * 0.5f; // + = the middle of the strip is to the axis's right

    // --- 3. LONG, not just narrow. Look further ahead the faster he goes: a fixed 60 units is
    //        barely two frames of warning at rail speed, and by the time the probe saw a bend he
    //        was already through it. ---
    if (ahead < (player->linearVelocity * 2.0f)) {
        ahead = player->linearVelocity * 2.0f;
    }
    if (ahead > KSURF_RAIL_AHEAD_MAX) {
        // The plane under his feet is extrapolated over this distance, and a switchback changes
        // slope long before it changes direction. Past this the prediction is worth less than the
        // warning it buys.
        ahead = KSURF_RAIL_AHEAD_MAX;
    }
    {
        // Sampled at several distances rather than only at the far end, so the seam between two
        // fence segments — or a post, or a missing collision quad — does not read as "the strip
        // ended" and throw him off mid-run. Any one of them landing is enough.
        s32 i;
        u8 goes = 0;

        for (i = 1; i <= KSURF_RAIL_GAP_STEPS; i++) {
            if (KiteSurf_OnStripAt(play, player, axis, ahead * ((f32)i / (f32)KSURF_RAIL_GAP_STEPS), drop)) {
                goes = 1;
                break;
            }
        }
        if (!goes) {
            return 0;
        }
    }

    sKSurf.railAxisYaw = axis;

    // --- 4. PIN him to the centre line. Not a pull — the whole lateral error is taken out every
    //        frame, so once a rail has him he cannot wander off the strip at all; the only ways off
    //        are the A hop, the strip ending, or a head-on wall. A soft pull could always be
    //        outrun on a bend, which is what "it doesn't stick me to it" was.
    //
    //        The correction is ONE distance along the axis's lateral vector: stepping x and z
    //        separately would make the pull depend on which way he happens to face. RailSnap is
    //        only a safety cap now, sized so that in practice the pin is complete — the error can
    //        never exceed half the strip's width anyway, and after the first frame it is tiny.
    {
        f32 snap = KSURF_RAIL_SNAP;
        f32 corr = CLAMP(centre, -snap, snap);
        s16 side = axis + 0x4000;

        player->actor.world.pos.x += corr * Math_SinS(side);
        player->actor.world.pos.z += corr * Math_CosS(side);
    }

    return 1;
}

// ---------------------------------------------------------------------------
// Anti-tunnelling.
//
// master_cycle.c sub-steps its own movement, but that cannot be reused verbatim here: the Master
// Cycle is an actor that integrates its own position, while the PLAYER is already moved by
// Player_ProcessSceneCollision inside Player_UpdateCommon — which has returned by the time this hook
// runs. Integrating again would move Link twice per frame. Instead sweep the distance he is about to
// cover and clamp the speed so a frame can never start on the far side of a wall. The speed itself
// stays uncapped in open terrain, which is the whole point of the mechanic.
// ---------------------------------------------------------------------------
static void KiteSurf_ClampToWall(Player* player, PlayState* play) {
    Vec3f posA;
    Vec3f posB;
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;
    f32 reach = player->linearVelocity + 10.0f;
    f32 sn = Math_SinS(player->actor.shape.rot.y);
    f32 cs = Math_CosS(player->actor.shape.rot.y);
    f32 dist;

    if (player->linearVelocity < 10.0f) {
        return; // the engine's own wall check already covers this much travel
    }

    posA.x = player->actor.world.pos.x;
    posA.y = player->actor.world.pos.y + 20.0f;
    posA.z = player->actor.world.pos.z;
    posB.x = posA.x + (sn * reach);
    posB.y = posA.y;
    posB.z = posA.z + (cs * reach);

    if (!BgCheck_EntityLineTest1(&play->colCtx, &posA, &posB, &hit, &poly, true, false, false, true, &bgId) ||
        (poly == NULL)) {
        return;
    }

    // Only a wall he is actually driving INTO. Without this a curved grind rail reads its own outer
    // side as an obstacle and brakes every frame. Same cos test master_cycle.c:774 makes.
    {
        s16 wallYaw = Math_Atan2S(COLPOLY_GET_NORMAL(poly->normal.z), COLPOLY_GET_NORMAL(poly->normal.x));

        if (Math_CosS(wallYaw - player->actor.shape.rot.y) > -0.5f) {
            return;
        }
    }

    dist = sqrtf(SQ(hit.x - posA.x) + SQ(hit.z - posA.z)) - 10.0f;
    if (dist < 0.0f) {
        dist = 0.0f;
    }
    if (player->linearVelocity > dist) {
        player->linearVelocity = dist;
    }
}

// A head-on wall at speed ends the ride. Same test Pegasus makes (equip_pegasus.c:311).
static u8 KiteSurf_HitWallHeadOn(Player* player) {
    s16 yawDiff;

    if (!(player->actor.bgCheckFlags & 0x200)) {
        return 0;
    }
    yawDiff = player->yaw - (s16)(player->actor.wallYaw + 0x8000);
    return (ABS(yawDiff) < KSURF_BONK_YAW) && (player->linearVelocity > 4.0f);
}

// ---------------------------------------------------------------------------
// Ride
// ---------------------------------------------------------------------------
static void KiteSurf_Ride(Player* player, PlayState* play) {
    Input* input = sControlInput;
    u8 grounded = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
    u8 pressedB = CHECK_BTN_ALL(input->press.button, BTN_B);
    f32 speedTarget = 0.0f;
    s16 yawTarget = player->actor.shape.rot.y;
    u8 hasStick = Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);

    // --- B + R dismounts; B alone spins ---
    if (pressedB) {
        if (CHECK_BTN_ALL(input->cur.button, BTN_R)) {
            sKSurf.state = KSURF_DISMOUNT;
            sKSurf.timer = 0;
            return;
        }
        if (Player_GetMeleeWeaponHeld(player) == 0) {
            // Nothing in his hand, so there is no spin to do — B means DRAW THE SWORD. Hand this
            // press to the vanilla item handling instead of eating it. Without this the button did
            // nothing at all while surfing: the pause keeps the blade sheathed, sheathed makes
            // Player_GetMeleeWeaponHeld return 0, and we were swallowing the press anyway.
            Player_ProcessItemButtons(player, play);
        } else if (sKSurf.spinFrames == 0) {
            func_80837948(play, player,
                          Player_HoldsTwoHandedWeapon(player) ? PLAYER_MWA_SPIN_ATTACK_2H : PLAYER_MWA_SPIN_ATTACK_1H);
            sKSurf.spinFrames = KSURF_SPIN_FRAMES;
            // func_80837948 changes the action func (equip_pendant.c:293 says so and it does) —
            // re-own it so the takeover check does not read the swing as a hostile steal.
            sKSurf.ownedAction = (void*)player->actionFunc;
        }
    }

    // --- A: hops off a rail (letting go of it), or just hops while free riding ---
    if (CHECK_BTN_ALL(input->press.button, BTN_A) && (grounded || (sKSurf.state == KSURF_RAIL))) {
        if (sKSurf.state == KSURF_RAIL) {
            // Let go. The lockout is what makes it stick: without it the very next frame's side
            // probes see the same corridor and grab it straight back.
            sKSurf.state = KSURF_RIDE;
            sKSurf.railDetach = KSURF_RAIL_DETACH_FRAMES;
        }
        player->actor.velocity.y = KSURF_HOP_VEL;
        player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        grounded = 0;
        Player_PlaySfx(&player->actor, NA_SE_PL_SKIP);

        // Sometimes the board throws a shuvit on the way up. Random on purpose — it is a flourish,
        // so it should not be something you can count on or spam.
        if ((sKSurf.boardSpinRate == 0) && (Rand_ZeroOne() < KSURF_SPIN_CHANCE)) {
            s16 rate = (s16)KSURF_BOARD_SPIN_RATE;

            sKSurf.boardSpinRate = (Rand_ZeroOne() < 0.5f) ? rate : (s16)-rate;
        }
    }

    // Board trick spin. Runs while he is off the ground and lands square: the spin is cosmetic, so
    // the board must never be left crooked under him once he is riding again.
    if (grounded) {
        sKSurf.boardSpinRate = 0;
    }
    if (sKSurf.boardSpinRate != 0) {
        sKSurf.boardSpin += sKSurf.boardSpinRate;
    } else if (sKSurf.boardSpin != 0) {
        Math_ScaledStepToS(&sKSurf.boardSpin, 0, 4000);
    }

    // --- C items keep working: the pause would otherwise swallow them. Skipped on a B frame
    //     (B is ours — spin / dismount — and this would also read it as a sword draw) and while a
    //     spin is running (the pause is off then, so the vanilla action func already called it). ---
    if (!pressedB && (sKSurf.spinFrames == 0)) {
        Player_ProcessItemButtons(player, play);
    }

    // --- Rail: a corridor takes the steering over and carries him through it ---
    if (sKSurf.railDetach > 0) {
        sKSurf.railDetach--;
    }

    if (grounded && (sKSurf.railDetach == 0) && KiteSurf_UpdateRail(player, play)) {
        sKSurf.state = KSURF_RAIL;
        sKSurf.railMiss = 0;
    } else if (grounded && (sKSurf.railDetach == 0) && (sKSurf.state != KSURF_RAIL) &&
               KiteSurf_Attract(player, play, KSURF_RAIL_EDGE_DROP)) {
        // Nothing under him, but there is a rail alongside — the magnet is dragging him across it.
        // No state change: once the drag puts him over the strip, the branch above picks it up on
        // its own and pins him.
    } else if (sKSurf.state == KSURF_RAIL) {
        // A short grace, because a doorway, a pillar gap or a seam in the wall drops one probe for
        // a frame or two without the corridor actually having ended.
        if (++sKSurf.railMiss >= KSURF_RAIL_GRACE) {
            sKSurf.state = KSURF_RIDE; // keeps the rail speed on the way out
        }
    }

    if (sKSurf.state == KSURF_RAIL) {
        // FOLLOW the geometry rather than snapping to it: KiteSurf_UpdateRail recomputes the
        // strip's own heading every frame, and this steers toward it. He is carried forward at the
        // rail speed with no stick input at all — the free-steering block below is skipped for the
        // whole time he is on one.
        //
        // The rate GROWS with how far off he is. A flat RailTurn is 27 degrees a frame, so an
        // 87-degree corner would take over three frames — and at rail speed that is 80 units
        // travelled while turning, on a segment only 126 units long. He would be off the far side
        // before he finished the turn. Scaling by the error means a gentle bend is still gentle
        // while a corner is taken in about one frame, which is what a grind rail should feel like.
        s16 delta = sKSurf.railAxisYaw - player->actor.shape.rot.y;
        s16 step = (s16)(KSURF_RAIL_TURN + (f32)(ABS(delta) >> 1));

        Math_SmoothStepToS(&player->actor.shape.rot.y, sKSurf.railAxisYaw, 2, step, 100);
        player->linearVelocity = KSURF_RAIL_SPEED;
    }

    // --- Free steering (BotW): the stick turns him, the slope only feeds speed ---
    if ((sKSurf.state != KSURF_RAIL) && hasStick) {
        f32 turnMax = KSURF_TURN_MAX;
        f32 speed = player->linearVelocity;
        s16 step;

        // Heavier the faster he goes.
        if (speed > 1.0f) {
            turnMax /= speed;
        }
        if (turnMax < KSURF_TURN_MIN) {
            turnMax = KSURF_TURN_MIN;
        }
        step = (s16)turnMax;
        Math_SmoothStepToS(&player->actor.shape.rot.y, yawTarget, 6, step, 100);
    }

    // --- Slope physics ---
    if (grounded && (player->actor.floorPoly != NULL)) {
        Vec3f slopeNormal;
        s16 downhillYaw;
        f32 steep;
        f32 align;

        // Deliberately NOT gated on SurfaceType_GetFloorEffect == 1 the way Player_HandleSlopes is:
        // every incline counts, which is what "muy generoso" means.
        Player_GetSlopeDirection(player->actor.floorPoly, &slopeNormal, &downhillYaw);

        steep = 1.0f - slopeNormal.y;
        align = Math_CosS(downhillYaw - player->actor.shape.rot.y);

        if (sKSurf.state != KSURF_RAIL) {
            player->linearVelocity += KSURF_SLOPE_ACCEL * steep * align;
            Math_StepToF(&player->linearVelocity, 0.0f, KSURF_FRICTION);

            if (hasStick) {
                player->linearVelocity += KSURF_STICK_ACCEL * (speedTarget / 9.0f);
            }
        }

        // floorPitch is already the signed pitch of the floor along the direction of travel,
        // recomputed every frame in Player_ProcessSceneCollision (z_player.c:13288).
        Math_SmoothStepToS(&sKSurf.leanPitch, (s16)(player->floorPitch * KSURF_LEAN_SCALE), 3, 0x300, 0x40);
        player->actor.shape.rot.x = sKSurf.leanPitch;
    } else {
        // Airborne: keep the momentum, let the engine's gravity do the rest.
        // (MIDAIR is held for every state up in KiteSurf_Tick, not here.)
        Math_SmoothStepToS(&sKSurf.leanPitch, 0, 3, 0x300, 0x40);
        player->actor.shape.rot.x = sKSurf.leanPitch;
    }

    // Lean into the turn. player->yaw still holds LAST frame's heading at this point (it is synced
    // to shape.rot.y further down), so this difference is exactly how hard he is turning right now
    // — which is the stick while free riding, and the strip's curve while on a rail.
    {
        s16 turnRate = player->actor.shape.rot.y - player->yaw;
        s16 want = (s16)(-turnRate * 2);
        s16 upperMax = (s16)(KSURF_UPPER_LEAN_DEG * 182.04f);
        f32 wantTurn;

        Math_SmoothStepToS(&sKSurf.leanRoll, CLAMP(want, -0x0A00, 0x0A00), 4, 0x200, 0x20);
        // The torso gets its own, much smaller share of the same signal: proportional while the
        // turn is gentle, saturating at UpperLeanDeg once he really leans on it.
        Math_SmoothStepToS(&sKSurf.upperLean, CLAMP(want, -upperMax, upperMax), 4, 0x200, 0x20);

        // Normalised carve, -1 .. +1, for the lower body. Kept as a fraction rather than an angle
        // so each of that limb's three axes can scale it by its own amplitude, sign included.
        // Smoothed by hand because Math_SmoothStepToS only works on s16.
        wantTurn = (f32)CLAMP(want, -KSURF_TURN_FULL, KSURF_TURN_FULL) / (f32)KSURF_TURN_FULL;
        sKSurf.turn += (wantTurn - sKSurf.turn) * KSURF_TURN_SMOOTH;
    }

    if (player->linearVelocity < 0.0f) {
        player->linearVelocity = 0.0f; // no reverse; NO upper cap by design
    }

    KiteSurf_ClampToWall(player, play);

    player->actor.speedXZ = player->linearVelocity;
    player->yaw = player->actor.shape.rot.y;

    // Riding hiss, scaled by speed — the sound the vanilla slope slide uses.
    if (grounded && (player->linearVelocity > 1.0f)) {
        func_800F4138(&player->actor.projectedPos, NA_SE_PL_SLIP_LEVEL - SFX_FLAG, player->actor.speedXZ);
    }

    // --- Exits --- (taking damage is handled by KiteSurf_Allowed: PLAYER_STATE1_DAMAGED aborts)
    if (KiteSurf_HitWallHeadOn(player)) {
        sKSurf.state = KSURF_DISMOUNT;
        sKSurf.timer = 0;
        return;
    }
    if (grounded && (player->linearVelocity < KSURF_STOP_SPEED)) {
        if (++sKSurf.stopFrames >= KSURF_STOP_FRAMES) {
            sKSurf.state = KSURF_DISMOUNT;
            sKSurf.timer = 0;
        }
    } else {
        sKSurf.stopFrames = 0;
    }
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------
void KiteSurf_Tick(Player* player, PlayState* play) {
    if (player == NULL) {
        return;
    }

    if (!KiteSurf_Allowed(player)) {
        KiteSurf_Abort(player);
        return;
    }

    // Not riding yet: PRESS R while airborne to get on the board.
    //
    // R and not "just be airborne", which is what this used to do — every jump, every step off a
    // ledge and every knockback mounted him, so the shield felt like it was equipping itself.
    // R is free here because the shield cannot be raised in mid air anyway, and it reads as the
    // shield button doing a shield thing. Having the Kite Shield equipped is the only other
    // requirement: on his back or in his hand both count, since the gate is the ext SHIELD SLOT and
    // not what he happens to be holding.
    if (sKSurf.state == KSURF_OFF) {
        if (sKSurfRemountLockout > 0) {
            // Only tick down on the ground, so a dismount over a pit does not simply re-arm
            // halfway through the fall.
            if (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
                sKSurfRemountLockout--;
            }
            return;
        }
        if ((sControlInput != NULL) && CHECK_BTN_ALL(sControlInput->press.button, BTN_R) &&
            !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !Player_InBlockingCsMode(play, player)) {
            KiteSurf_Start(player, play);
        }
        return;
    }

    // MIDAIR, held UNCONDITIONALLY for as long as the surf owns the player.
    //
    // func_8083AA10 runs from Player_UpdateCommon at z_player.c:13935 — OUTSIDE the action func, so
    // the pause never reaches it — and it swaps an airborne Link into the fall action unless this
    // flag is set (z_player.c:6996). Two ways that bites, both of which look like "the surf just
    // dies": missing it on the mount (entry is always mid air) meant it could never start at all,
    // and setting it only when already airborne is one frame too late for a hop, because this hook
    // runs AFTER func_8083AA10 — the frame he leaves the ground has no flag on it yet.
    //
    // Holding it while grounded costs nothing: line 6996 is the only place in the player that
    // READS this flag, everything else only sets or clears it.
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;

    // Damage, a cutscene or a scripted move calling Player_SetupAction goes straight through the
    // pause. If the action func is not the one we parked on, we no longer own the player.
    // Not while mounting: the entry frame is still settling and RIDE re-records the action anyway.
    if ((sKSurf.state != KSURF_MOUNT) && (sKSurf.spinFrames == 0) && (sKSurf.ownedAction != NULL) &&
        ((void*)player->actionFunc != sKSurf.ownedAction)) {
        KiteSurf_Abort(player);
        return;
    }

    // Yield the way SM64 Mario does (z_player.c:13987): a door, an NPC, a grab or a ledge has to be
    // able to complete, or the surf is a softlock waiting to happen.
    if (Player_ActionHandler_1(player, play) || Player_ActionHandler_Talk(player, play) ||
        Player_ActionHandler_2(player, play) || Player_ActionHandler_12(player, play)) {
        KiteSurf_Abort(player);
        return;
    }

    switch (sKSurf.state) {
        case KSURF_MOUNT:
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            LinkAnimation_Update(play, &player->skelAnime);
            if (++sKSurf.timer >= KSURF_MOUNT_FRAMES) {
                sKSurf.state = KSURF_RIDE;
                sKSurf.timer = 0;
                sKSurf.ownedAction = (void*)player->actionFunc;
            }
            break;

        case KSURF_RIDE:
        case KSURF_RAIL:
            if (sKSurf.spinFrames > 0) {
                // The spin owns the animation and the action func: leave the pause off and let the
                // vanilla swing play out. Speed is untouched, so he keeps sliding through it.
                sKSurf.spinFrames--;
                // The window is a ceiling, not the length — the swing ending early takes control
                // back at once. The 4-frame grace is because meleeWeaponState is still settling on
                // the frames right after func_80837948.
                if ((sKSurf.spinFrames == 0) ||
                    ((sKSurf.spinFrames < (KSURF_SPIN_FRAMES - 4)) && (player->meleeWeaponState == 0))) {
                    sKSurf.spinFrames = 0;
                    sKSurf.ownedAction = (void*)player->actionFunc;
                }
                KiteSurf_Ride(player, play);
                break;
            }
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            KiteSurf_HoldPose(player, play);
            LinkAnimation_Update(play, &player->skelAnime);
            KiteSurf_Ride(player, play);
            break;

        case KSURF_DISMOUNT:
            KiteSurf_KillMeleeState(player, play);
            Player_AnimChangeOnceMorph(play, player, GET_PLAYER_ANIM(PLAYER_ANIMGROUP_put, player->modelAnimType));
            Player_PlaySfx(&player->actor, NA_SE_IT_SHIELD_POSTURE);
            KiteSurf_Abort(player);
            break;
    }
}
