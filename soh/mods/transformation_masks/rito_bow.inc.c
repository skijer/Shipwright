/**
 * rito_bow.inc.c — the Rito's own bow, on B. Text-included after rito_flight.inc.c,
 * inside the same extern "C" body, and dispatched BEFORE it so a press in mid-air takes
 * the frame off the glide. Three arrows leave on the release, each with a target of its
 * own; switches come before enemies because a switch is the reason you are shooting.
 */

#define RITO_BOW_ANIM(name) "__OTR__misc/link_animetion/gPlayerAnim_mhr_bow_" name

#define RITO_BOW_CLIP_AIR RITO_BOW_ANIM("charge_attack09")
#define RITO_BOW_CLIP_ENTER_STILL RITO_BOW_ANIM("motion12")
#define RITO_BOW_CLIP_HOLD_STILL RITO_BOW_ANIM("idle04_loop")
#define RITO_BOW_CLIP_ENTER_MOVE RITO_BOW_ANIM("dash_attack07")
#define RITO_BOW_CLIP_HOLD_MOVE RITO_BOW_ANIM("run07_loop")
#define RITO_BOW_CLIP_RELEASE RITO_BOW_ANIM("dash_attack13")
#define RITO_BOW_CLIP_SEED RITO_BOW_ANIM("charge_attack11")
#define RITO_BOW_CLIP_AIR_TAP RITO_BOW_ANIM("attack04")

#define RITO_BOW_SPEED 1.25f
#define RITO_BOW_RELEASE_SPEED 1.5f
#define RITO_BOW_SEED_SPEED 2.0f
// stripY pins the root to each clip's OWN first frame, not to a shared one, so a clip
// authored lower than the set draws sunk. Same correction RITO_LAND_LIFT makes.
#define RITO_BOW_SEED_LIFT 1500.0f
#define RITO_BOW_RELEASE_LIFT -300.0f // the loose clip floats; this plants the feet on it
#define RITO_BOW_HOP_SPEED 7.0f       // the loose doubles as a dodge, carrying the rito off the shot
#define RITO_BOW_HOP_DECAY 0.5f
#define RITO_BOW_DODGE_IFRAMES -10 // negative is OOT's own grace window: invulnerable, no flashing

// Frames are LOGIC frames: this runs at 20Hz (R_UPDATE_RATE = 3), so 20 = 1 second.
#define RITO_BOW_VOLLEY 3        // arrows tracked at once; the air fan still throws three
#define RITO_BOW_LOOSE_FRAME 3   // into the AIR clip, where its arrows leave
#define RITO_BOW_LOOSE_LEAD 5.0f // frames before the release clip ends that the volley goes
#define RITO_BOW_RANGE 1400.0f   // how far the volley looks for something to hit
#define RITO_BOW_CONE 0x1555     // 30 each side of where LINK faces, so a 60 degree cone
#define RITO_BOW_TURN 0x1000     // per-frame steering: wider than a fan spoke, so it closes at once
#define RITO_BOW_ARROW_SPEED 150.0f
#define RITO_BOW_HIT_DIST \
    170.0f                       // one frame of travel: EnArrow moves 150 units a frame,
                                 // so a tighter sphere is jumped clean over
#define RITO_BOW_MOVE_SPEED 5.0f // walking with the bow drawn
#define RITO_BOW_MOVE_DEADZONE 10.0f
#define RITO_BOW_TURN_RATE 0x0C00 // how fast the body swings to the stick while drawn
#define RITO_BOW_ICE_RADIUS 90.0f // how near an arrival an Obj_Ice_Poly has to be to shatter
#define RITO_BOW_HANDSHAKE 4      // player->unk_A73: without it EnArrow_Shoot kills its own arrow
#define RITO_BOW_FAN_STEP 0x0720  // 10 degrees between spokes
#define RITO_BOW_FAN(i) ((s16)(((i)-1) * RITO_BOW_FAN_STEP))
#define RITO_BOW_TAP_FRAMES 5   // B let go inside this is a tap, not a draw
#define RITO_BOW_CHARGE_TWIN 20 // a second of hold reaches the twin homing shot
#define RITO_BOW_CHARGE_MAX_SHOTS 2
#define RITO_BOW_SNAP_DIST 400.0f // inside two frames of travel the assist stops holding back

// Obj_Switch's own layout (z_obj_switch.h): type in the low 3 bits, frozen in bit 7.
#define RITO_BOW_SWITCH_TYPE(actor) ((actor)->params & 7)
#define RITO_BOW_SWITCH_IS_TARGET(actor)                                                                               \
    ((RITO_BOW_SWITCH_TYPE(actor) == OBJSWITCH_TYPE_EYE) || (RITO_BOW_SWITCH_TYPE(actor) == OBJSWITCH_TYPE_CRYSTAL) || \
     (RITO_BOW_SWITCH_TYPE(actor) == OBJSWITCH_TYPE_CRYSTAL_TARGETABLE))

typedef enum {
    RITO_BOW_OFF = 0,
    RITO_BOW_AIR,     // held in the air: pinned, arrows straight down
    RITO_BOW_AIR_TAP, // tapped in the air: one arrow where Link faces
    RITO_BOW_ENTER,   // a ground entry clip is playing; the hold loop follows it
    RITO_BOW_HOLD,    // drawn on the ground, waiting for B to come up
    RITO_BOW_RELEASE, // the loose; the volley leaves RITO_BOW_LOOSE_LEAD frames before the end
    RITO_BOW_SEED,    // tapped on the ground: a deku seed thrown by hand
} RitoBowState;

// The actor pointer is only ever COMPARED against the live actor list, never
// dereferenced blind, so an arrow that dies between frames leaves nothing dangling.
typedef struct {
    Actor* arrow;
    Actor* target; // refreshed from the live list each frame, so a moving enemy stays hit
    u8 targetCat;
    Vec3f goal;
} RitoBowArrow;

typedef struct {
    u8 loaded;
    u8 state;
    u8 moving;               // which hold loop is running: 0 = idle04, 1 = run07
    u8 fired;                // this release has already loosed, so the lead frame cannot fire twice
    s16 charge;              // frames B has been down: picks tap, single shot or twin homing shot
    PlayerActionFunc action; // what OOT was running when the draw started
    s16 timer;
    LinkAnimationHeader* air;
    LinkAnimationHeader* enterStill;
    LinkAnimationHeader* holdStill;
    LinkAnimationHeader* enterMove;
    LinkAnimationHeader* holdMove;
    LinkAnimationHeader* release;
    LinkAnimationHeader* seed;
    LinkAnimationHeader* airTap;
    RitoBowArrow arrows[RITO_BOW_VOLLEY];
} RitoBow;

static RitoBow sRitoBow;

static LinkAnimationHeader* MmForm_RitoBowLoadClip(const char* path, f32 speed) {
    LinkAnimationHeader* raw;
    s16 frames;

    if ((path == NULL) || !ResourceMgr_FileExists(path)) {
        return NULL;
    }
    raw = ResourceMgr_LoadPlayerAnimAsHeader(path);
    if (raw == NULL) {
        return NULL;
    }
    // Resampling to fewer frames IS the speed-up, and the resampler rewrites the resource
    // in place: every path here is loaded exactly once, guarded by `loaded`.
    frames = (s16)(((f32)raw->common.frameCount / speed) + 0.5f);
    if (frames < 2) {
        frames = 2;
    }
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(path, 1, frames);
}

static void MmForm_RitoBowLoadClips(void) {
    if (sRitoBow.loaded) {
        return;
    }
    sRitoBow.loaded = 1;
    sRitoBow.air = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_AIR, RITO_BOW_SPEED);
    sRitoBow.enterStill = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_ENTER_STILL, RITO_BOW_SPEED);
    sRitoBow.holdStill = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_HOLD_STILL, RITO_BOW_SPEED);
    sRitoBow.enterMove = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_ENTER_MOVE, RITO_BOW_SPEED);
    sRitoBow.holdMove = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_HOLD_MOVE, RITO_BOW_SPEED);
    sRitoBow.release = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_RELEASE, RITO_BOW_RELEASE_SPEED);
    sRitoBow.seed = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_SEED, RITO_BOW_SEED_SPEED);
    sRitoBow.airTap = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_AIR_TAP, RITO_BOW_SPEED);
    if (sRitoBow.holdStill == NULL) {
        SPDLOG_WARN("[Rito] bow clips missing ({}) — B will not draw", RITO_BOW_CLIP_HOLD_STILL);
    }
}

static f32 MmForm_RitoBowDrawLift(Player* player) {
    if ((sRitoBow.seed != NULL) && (player->skelAnime.animation == sRitoBow.seed)) {
        return RITO_BOW_SEED_LIFT;
    }
    if ((sRitoBow.release != NULL) && (player->skelAnime.animation == sRitoBow.release)) {
        return RITO_BOW_RELEASE_LIFT;
    }
    return 0.0f;
}

extern "C" u8 MmForm_RitoBowIsOut(void) {
    return (gFormState.currentForm == MM_PLAYER_FORM_RITO) && (sRitoBow.state != RITO_BOW_OFF);
}

// Obj_Switch never fills focus.pos, so its origin plus a lift is the only aim point it has.
static void MmForm_RitoBowAimPoint(Actor* actor, Vec3f* out) {
    if (actor->id == ACTOR_OBJ_SWITCH) {
        out->x = actor->world.pos.x;
        out->y = actor->world.pos.y + 20.0f;
        out->z = actor->world.pos.z;
        return;
    }
    Math_Vec3f_Copy(out, &actor->focus.pos);
}

static u8 MmForm_RitoBowIsInSight(Actor* actor, s16 faceYaw) {
    s16 off;

    if ((actor->update == NULL) || (actor->xyzDistToPlayerSq > SQ(RITO_BOW_RANGE))) {
        return 0;
    }
    off = actor->yawTowardsPlayer + 0x8000 - faceYaw;
    return ABS(off) < RITO_BOW_CONE;
}

// Up to `max` DISTINCT actors, nearest first: arrows must never converge on one target.
static s32 MmForm_RitoBowFindTargets(PlayState* play, Actor** out, s32 max, s16 faceYaw) {
    static const u8 sPasses[] = { ACTORCAT_SWITCH, ACTORCAT_PROP, ACTORCAT_BG, ACTORCAT_ENEMY };
    s32 found = 0;
    s32 pass;

    for (pass = 0; (pass < (s32)ARRAY_COUNT(sPasses)) && (found < max); pass++) {
        u8 wantSwitch = (sPasses[pass] != ACTORCAT_ENEMY);

        // One actor per sweep: the list is unordered and three re-sweeps beat a sort.
        while (found < max) {
            Actor* best = NULL;
            f32 bestDist = SQ(RITO_BOW_RANGE);
            Actor* actor;
            s32 i;

            for (actor = play->actorCtx.actorLists[sPasses[pass]].head; actor != NULL; actor = actor->next) {
                if (wantSwitch && ((actor->id != ACTOR_OBJ_SWITCH) || !RITO_BOW_SWITCH_IS_TARGET(actor))) {
                    continue;
                }
                if (!MmForm_RitoBowIsInSight(actor, faceYaw) || (actor->xyzDistToPlayerSq >= bestDist)) {
                    continue;
                }
                for (i = 0; i < found; i++) {
                    if (out[i] == actor) {
                        break;
                    }
                }
                if (i < found) {
                    continue; // already carries an arrow of its own
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

// Ball-and-chain, not fire: shards on contact and the switch live the same frame.
// Obj_Ice_Poly is a CHILD of the switch it covers, which is how the switch is reached.
static void MmForm_RitoBowShatterIce(PlayState* play, Vec3f* at) {
    static Color_RGBA8 sIceWhite = { 250, 250, 250, 255 };
    static Color_RGBA8 sIceGray = { 180, 200, 230, 255 };
    s32 cat;

    for (cat = 0; cat < ACTORCAT_MAX; cat++) {
        Actor* actor = play->actorCtx.actorLists[cat].head;

        while (actor != NULL) {
            Actor* next = actor->next;

            if ((actor->id == ACTOR_OBJ_ICE_POLY) && (actor->update != NULL) &&
                (Math_Vec3f_DistXYZ(at, &actor->world.pos) < RITO_BOW_ICE_RADIUS)) {
                Vec3f vel = { 0.0f, 0.0f, 0.0f };
                Vec3f accel = { 0.0f, -1.0f, 0.0f };
                s32 i;

                for (i = 0; i < 8; i++) {
                    Vec3f pos;

                    pos.x = actor->world.pos.x + Rand_CenteredFloat(40.0f);
                    pos.y = actor->world.pos.y + (Rand_ZeroOne() * 70.0f);
                    pos.z = actor->world.pos.z + Rand_CenteredFloat(40.0f);
                    vel.x = Rand_CenteredFloat(6.0f);
                    vel.y = Rand_ZeroOne() * 6.0f;
                    vel.z = Rand_CenteredFloat(6.0f);
                    func_8002829C(play, &pos, &vel, &accel, &sIceWhite, &sIceGray, 350, 20);
                }
                // At the position, not on the actor: the actor is killed on the next line.
                Sfx_PlaySfxAtPos(&actor->world.pos, NA_SE_EV_ICE_BROKEN);
                if (actor->parent != NULL) {
                    actor->parent->params &= ~0x80; // the switch stops being frozen at once
                }
                Actor_Kill(actor);
            }
            actor = next;
        }
    }
}

static void MmForm_RitoBowClearArrows(void) {
    s32 i;

    for (i = 0; i < RITO_BOW_VOLLEY; i++) {
        sRitoBow.arrows[i].arrow = NULL;
        sRitoBow.arrows[i].target = NULL;
    }
}

// EnArrow_Shoot asks before it re-derives its yaw from the camera. Answering 1 both
// claims the arrow and lays its aim down, pitch included, which the camera would flatten.
extern "C" u8 MmForm_RitoBowClaimArrow(PlayState* play, Actor* arrow) {
    s32 i;

    for (i = 0; i < RITO_BOW_VOLLEY; i++) {
        if (sRitoBow.arrows[i].arrow == arrow) {
            return 1; // claimed, and left on its spoke: the tick is what bends it in
        }
    }
    return 0;
}

// A target that is still alive moves its own aim point along with it. Its pointer is
// only ever compared against its own category list, never dereferenced blind.
static void MmForm_RitoBowRefreshGoal(PlayState* play, RitoBowArrow* slot) {
    Actor* actor;

    if (slot->target == NULL) {
        return;
    }
    for (actor = play->actorCtx.actorLists[slot->targetCat].head; actor != NULL; actor = actor->next) {
        if (actor == slot->target) {
            MmForm_RitoBowAimPoint(actor, &slot->goal);
            return;
        }
    }
    slot->target = NULL;
}

static void MmForm_RitoBowTickArrows(PlayState* play) {
    s32 i;

    for (i = 0; i < RITO_BOW_VOLLEY; i++) {
        Actor* arrow;
        Actor* live = NULL;
        f32 dist;
        s16 turn;
        s16 dYaw;
        s16 dPitch;

        arrow = sRitoBow.arrows[i].arrow;
        if (arrow == NULL) {
            continue;
        }
        // Prove the pointer: an arrow that hit something is already back in the pool.
        for (live = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; live != NULL; live = live->next) {
            if (live == arrow) {
                break;
            }
        }
        if (live == NULL) {
            sRitoBow.arrows[i].arrow = NULL;
            continue;
        }
        if (sRitoBow.arrows[i].target == NULL) {
            continue; // claimed so the camera cannot touch it, but it never bends
        }
        MmForm_RitoBowRefreshGoal(play, &sRitoBow.arrows[i]);
        dist = Math_Vec3f_DistXYZ(&arrow->world.pos, &sRitoBow.arrows[i].goal);
        if (dist < RITO_BOW_HIT_DIST) {
            MmForm_RitoBowShatterIce(play, &sRitoBow.arrows[i].goal);
            sRitoBow.arrows[i].arrow = NULL;
            continue;
        }
        // Close in, the clamp comes off: an arrow that spent its arc swinging round from a
        // 45 degree spoke must not miss because it ran out of turn on the last frame.
        turn = (dist < RITO_BOW_SNAP_DIST) ? 0x7FFF : RITO_BOW_TURN;
        dYaw = Math_Vec3f_Yaw(&arrow->world.pos, &sRitoBow.arrows[i].goal) - arrow->world.rot.y;
        dPitch = Math_Vec3f_Pitch(&arrow->world.pos, &sRitoBow.arrows[i].goal) - arrow->world.rot.x;
        arrow->world.rot.y += CLAMP(dYaw, -turn, turn);
        arrow->world.rot.x += CLAMP(dPitch, -turn, turn);
        arrow->shape.rot = arrow->world.rot;
        // Rotating alone curves nothing: EnArrow_Fly rides the velocity vector laid down
        // once, so it is rebuilt each frame from the new heading. Gravity drop goes with it.
        Actor_SetProjectileSpeed(arrow, RITO_BOW_ARROW_SPEED);
    }
}

// Claiming the slot is not only for homing: an unclaimed arrow has its yaw re-derived
// from the CAMERA by EnArrow_Shoot, and every shot here is aimed off Link instead.
static Actor* MmForm_RitoBowSpawnArrow(PlayState* play, Player* player, s16 yaw, s16 pitch, s32 type) {
    Vec3f* from = &player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    Actor* arrow;
    s32 i;

    // EnArrow_Shoot kills any parentless arrow it finds while this countdown is clear.
    player->unk_A73 = RITO_BOW_HANDSHAKE;
    arrow = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ARROW, from->x, from->y, from->z, pitch, yaw, 0, type);
    if (arrow == NULL) {
        return NULL;
    }
    for (i = 0; i < RITO_BOW_VOLLEY; i++) {
        if (sRitoBow.arrows[i].arrow == NULL) {
            sRitoBow.arrows[i].arrow = arrow;
            sRitoBow.arrows[i].target = NULL;
            break;
        }
    }
    return arrow;
}

// The slot is found by pointer: a spawn that failed would otherwise shift every arrow
// after it onto the wrong target.
static void MmForm_RitoBowSetTarget(Actor* arrow, Actor* target) {
    s32 i;

    for (i = 0; i < RITO_BOW_VOLLEY; i++) {
        if (sRitoBow.arrows[i].arrow == arrow) {
            sRitoBow.arrows[i].target = target;
            sRitoBow.arrows[i].targetCat = target->category;
            MmForm_RitoBowAimPoint(target, &sRitoBow.arrows[i].goal);
            return;
        }
    }
}

// Spokes centred on where Link faces: a single shot goes straight out, a pair splits.
static s16 MmForm_RitoBowSpokeYaw(s16 faceYaw, s32 index, s32 count) {
    return faceYaw + (s16)((((index * 2) - (count - 1)) * RITO_BOW_FAN_STEP) / 2);
}

// One shot at the first charge, two homing ones at the second. The first is a plain
// arrow down Link's line unless something is locked on; the pair always chase.
static void MmForm_RitoBowLoose(PlayState* play, Player* player) {
    Actor* targets[RITO_BOW_CHARGE_MAX_SHOTS];
    Actor* locked = player->focusActor;
    s16 faceYaw = player->actor.shape.rot.y;
    s32 shots = (sRitoBow.charge >= RITO_BOW_CHARGE_TWIN) ? RITO_BOW_CHARGE_MAX_SHOTS : 1;
    u8 homes = (shots > 1) || (locked != NULL);
    s32 count = (locked != NULL) ? 0 : (homes ? MmForm_RitoBowFindTargets(play, targets, shots, faceYaw) : 0);
    s32 i;

    MmForm_RitoBowClearArrows();
    for (i = 0; i < shots; i++) {
        Actor* arrow =
            MmForm_RitoBowSpawnArrow(play, player, MmForm_RitoBowSpokeYaw(faceYaw, i, shots), 0, ARROW_NORMAL);
        Actor* target = (locked != NULL) ? locked : ((i < count) ? targets[i] : NULL);

        if ((arrow != NULL) && homes && (target != NULL)) {
            MmForm_RitoBowSetTarget(arrow, target);
        }
    }
}

static void MmForm_RitoBowPlay(PlayState* play, Player* player, LinkAnimationHeader* anim, u8 loop) {
    if (anim == NULL) {
        return;
    }
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim),
                         loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -4.0f);
    sRitoBow.timer = 0;
}

static s32 MmForm_RitoBowAdvance(PlayState* play, Player* player) {
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    sRitoBow.timer++;
    return LinkAnimation_Update(play, &player->skelAnime);
}

// Returns what the stick asks, and every aiming state passes it straight back as `advance`:
// a clip that shows the rito walking has to actually walk it, whichever clip that is.
static u8 MmForm_RitoBowGroundMove(PlayState* play, Player* player, u8 advance) {
    f32 stickMag;
    s16 stickAngle;

    func_80077D10(&stickMag, &stickAngle, &play->state.input[0]);
    player->linearVelocity = advance ? RITO_BOW_MOVE_SPEED : 0.0f;
    if (stickMag < RITO_BOW_MOVE_DEADZONE) {
        return 0;
    }
    Math_ScaledStepToS(&player->yaw, Camera_GetInputDirYaw(GET_ACTIVE_CAM(play)) + stickAngle, RITO_BOW_TURN_RATE);
    player->actor.world.rot.y = player->yaw;
    player->actor.shape.rot.y = player->yaw;
    return 1;
}

// The loose cuts every clip short, from anywhere on the ground. A tap never gets here:
// it is a throw, not a draw, and the charge is what tells the two apart.
static void MmForm_RitoBowStartRelease(PlayState* play, Player* player) {
    u8 tapped = (sRitoBow.charge < RITO_BOW_TAP_FRAMES) && (sRitoBow.seed != NULL);

    sRitoBow.state = tapped ? RITO_BOW_SEED : RITO_BOW_RELEASE;
    sRitoBow.fired = 0;
    MmForm_RitoBowPlay(play, player, tapped ? sRitoBow.seed : sRitoBow.release, 0);
}

// The tap throws instead of shooting. ARROW_SEED is vanilla's own weak projectile and
// EnArrow_Shoot already gives it 80 units a frame against an arrow's 150.
static void MmForm_RitoBowThrowSeed(PlayState* play, Player* player) {
    MmForm_RitoBowSpawnArrow(play, player, player->actor.shape.rot.y, 0, ARROW_SEED);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
}

static void MmForm_RitoBowEnd(Player* player) {
    sRitoBow.state = RITO_BOW_OFF;
    sRitoBow.moving = 0;
    sRitoBow.charge = 0;
    sRitoBow.timer = 0;
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
}

extern "C" void MmForm_RitoBowReset(void) {
    sRitoBow.state = RITO_BOW_OFF;
    sRitoBow.moving = 0;
    sRitoBow.timer = 0;
    MmForm_RitoBowClearArrows();
}

// Returns 1 when the bow owns the frame.
static u8 MmForm_RitoBowUpdate(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];
    u8 bHeld;

    if (gFormState.currentForm != MM_PLAYER_FORM_RITO) {
        return 0;
    }
    // Ahead of every early return below: a volley outlives the state that fired it.
    MmForm_RitoTickDamage(); // this controller is dispatched first, so it owns the tick
    MmForm_RitoBowTickArrows(play);
    MmForm_RitoBowLoadClips();
    if (MmForm_InputOwnedByMessage()) {
        return 0;
    }
    bHeld = CHECK_BTN_ALL(input->cur.button, BTN_B);

    if ((sRitoBow.state != RITO_BOW_OFF) &&
        (MmForm_RitoOotTookOver(player, sRitoBow.action) || (player->stateFlags1 & PLAYER_STATE1_IN_WATER))) {
        MmForm_RitoBowEnd(player);
        return 0;
    }
    // Unlike the other forms the rito is at the first charge from the opening frame; this
    // only ever counts up to the second one, and to telling a tap from a draw.
    if (bHeld && (sRitoBow.state != RITO_BOW_OFF)) {
        sRitoBow.charge++;
    }

    switch (sRitoBow.state) {
        case RITO_BOW_AIR:
            // Pinned for the length of the clip. Letting go early turns the whole thing into
            // the tap: one arrow out along Link's line instead of the volley downward.
            player->actor.velocity.y = 0.0f;
            player->actor.gravity = 0.0f;
            player->linearVelocity = 0.0f;
            if (!bHeld && (sRitoBow.charge < RITO_BOW_TAP_FRAMES) && (sRitoBow.airTap != NULL)) {
                sRitoBow.state = RITO_BOW_AIR_TAP;
                sRitoBow.fired = 0;
                MmForm_RitoBowPlay(play, player, sRitoBow.airTap, 0);
                return 1;
            }
            if (sRitoBow.timer == RITO_BOW_LOOSE_FRAME) {
                s32 i;

                MmForm_RitoBowClearArrows();
                // Straight down, and the fan tips in PITCH here: yaw would spread the
                // arrows sideways across a floor the rito is looking straight at.
                for (i = 0; i < RITO_BOW_VOLLEY; i++) {
                    MmForm_RitoBowSpawnArrow(play, player, player->actor.shape.rot.y, 0x4000 + RITO_BOW_FAN(i),
                                             ARROW_NORMAL);
                }
            }
            if (MmForm_RitoBowAdvance(play, player)) {
                // Straight back into the fall, morphed rather than cut: the flight
                // controller owns the air again from the next frame.
                player->actor.gravity = RITO_FLY_GRAVITY_DEFAULT;
                MmForm_RitoBowEnd(player);
                if (sRito.fly != NULL) {
                    MmForm_RitoPlay(play, player, sRito.fly, 1, 1.0f);
                }
                return 0;
            }
            return 1;

        case RITO_BOW_ENTER: {
            u8 wants = MmForm_RitoBowGroundMove(play, player, sRitoBow.moving);

            if (!bHeld) {
                MmForm_RitoBowStartRelease(play, player);
                return 1;
            }
            // The entry clip only STARTS the draw; a stance change drops into the loop.
            if (wants != sRitoBow.moving) {
                sRitoBow.moving = wants;
                sRitoBow.state = RITO_BOW_HOLD;
                MmForm_RitoBowPlay(play, player, wants ? sRitoBow.holdMove : sRitoBow.holdStill, 1);
                return 1;
            }
            if (MmForm_RitoBowAdvance(play, player)) {
                sRitoBow.state = RITO_BOW_HOLD;
                MmForm_RitoBowPlay(play, player, sRitoBow.moving ? sRitoBow.holdMove : sRitoBow.holdStill, 1);
            }
            return 1;
        }

        case RITO_BOW_HOLD: {
            u8 moving = MmForm_RitoBowGroundMove(play, player, sRitoBow.moving);

            if (!bHeld) {
                MmForm_RitoBowStartRelease(play, player);
                return 1;
            }
            // Instant: the two aiming stances swap loops with nothing in between.
            if (moving != sRitoBow.moving) {
                sRitoBow.moving = moving;
                MmForm_RitoBowPlay(play, player, moving ? sRitoBow.holdMove : sRitoBow.holdStill, 1);
                return 1;
            }
            MmForm_RitoBowAdvance(play, player);
            return 1;
        }

        case RITO_BOW_RELEASE: {
            f32 lead =
                (sRitoBow.release != NULL) ? (Animation_GetLastFrame(sRitoBow.release) - RITO_BOW_LOOSE_LEAD) : 0.0f;

            // A dodge as much as a shot. world.rot.y is what Actor_MoveXZGravity steers by
            // and shape.rot.y is the facing: writing only the first slides it back, still aiming.
            if (sRitoBow.timer == 0) {
                player->actor.world.rot.y =
                    (player->focusActor != NULL)
                        ? Math_Vec3f_Yaw(&player->focusActor->world.pos, &player->actor.world.pos)
                        : (s16)(player->actor.shape.rot.y + 0x8000);
                player->linearVelocity = RITO_BOW_HOP_SPEED;
                // Only ever deepened, never shortened: a grace already running outlasts this.
                if (player->invincibilityTimer > RITO_BOW_DODGE_IFRAMES) {
                    player->invincibilityTimer = RITO_BOW_DODGE_IFRAMES;
                }
            }
            Math_StepToF(&player->linearVelocity, 0.0f, RITO_BOW_HOP_DECAY);
            // Measured on the RESAMPLED clip: the 1.5x speed-up is already inside the number.
            if (!sRitoBow.fired && (player->skelAnime.curFrame >= lead)) {
                sRitoBow.fired = 1;
                MmForm_RitoBowLoose(play, player);
            }
            if (MmForm_RitoBowAdvance(play, player)) {
                if (!sRitoBow.fired) {
                    MmForm_RitoBowLoose(play, player); // a clip shorter than the lead itself
                }
                MmForm_RitoBowEnd(player);
                return 0;
            }
            return 1;
        }

        case RITO_BOW_SEED:
            player->linearVelocity = 0.0f;
            if (!sRitoBow.fired && (sRitoBow.timer >= RITO_BOW_LOOSE_FRAME)) {
                sRitoBow.fired = 1;
                MmForm_RitoBowClearArrows();
                MmForm_RitoBowThrowSeed(play, player);
            }
            if (MmForm_RitoBowAdvance(play, player)) {
                MmForm_RitoBowEnd(player);
                return 0;
            }
            return 1;

        case RITO_BOW_AIR_TAP:
            player->actor.velocity.y = 0.0f;
            player->actor.gravity = 0.0f;
            player->linearVelocity = 0.0f;
            if (!sRitoBow.fired && (sRitoBow.timer >= RITO_BOW_LOOSE_FRAME)) {
                sRitoBow.fired = 1;
                MmForm_RitoBowClearArrows();
                MmForm_RitoBowSpawnArrow(play, player, player->actor.shape.rot.y, 0, ARROW_NORMAL);
            }
            if (MmForm_RitoBowAdvance(play, player)) {
                player->actor.gravity = RITO_FLY_GRAVITY_DEFAULT;
                MmForm_RitoBowEnd(player);
                if (sRito.fly != NULL) {
                    MmForm_RitoPlay(play, player, sRito.fly, 1, 1.0f);
                }
                return 0;
            }
            return 1;

        default:
            break;
    }

    if (!CHECK_BTN_ALL(input->press.button, BTN_B) || (player->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE)) {
        return 0;
    }
    if (!MMFORM_ON_GROUND(player)) {
        if (sRitoBow.air == NULL) {
            return 0;
        }
        sRitoBow.state = RITO_BOW_AIR;
        sRitoBow.action = player->actionFunc;
        sRitoBow.charge = 0;
        MmForm_RitoBowPlay(play, player, sRitoBow.air, 0);
        return 1;
    }
    if (sRitoBow.holdStill == NULL) {
        return 0;
    }
    func_80839FFC(player, play);
    sRitoBow.action = player->actionFunc;
    sRitoBow.charge = 0;
    sRitoBow.moving = (fabsf(player->linearVelocity) > 1.0f);
    sRitoBow.state = RITO_BOW_ENTER;
    MmForm_RitoBowPlay(play, player, sRitoBow.moving ? sRitoBow.enterMove : sRitoBow.enterStill, 0);
    return 1;
}
