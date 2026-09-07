/**
 * equip_kite_shield.c - Kite Shield (Extended Shield Slot 2)
 *
 * Takes over the slot the Gerudo Scimitar placeholder used to hold. The MODEL already exists
 * (objects/object_nei_kite_shield, drawn via ExtEquip_GetKiteShieldDL in extended_equipment.c).
 *
 * BEHAVIOR: SHIELD SURFING (BotW). PRESS R IN MID AIR with the Kite Shield equipped (on his back
 * or in his hand, either counts) and it drops under his feet and he rides it: downhill accelerates with no speed cap,
 * uphill bleeds speed, flat bleeds it very slowly. A narrow corridor is a grind rail — he snaps to its centre and is
 * boosted through it. A hops, B spins, B+R dismounts, C items still work.
 *
 * This file only holds the STATE and the predicates other translation-unit sites read
 * (fall damage in z_player.c, the shield draw in extended_equipment.c). The engine itself is
 * mods/equipment/kite_surf.c, which z_player.c includes much later — it needs Player_GetSlopeDirection,
 * Player_GetRelativePosition, Player_GetMovementSpeedAndYaw, func_80837948 and the GET_PLAYER_ANIM
 * tables, none of which exist yet at the line where the ext-equipment unity build is included.
 *
 * Included by ext_equip_behavior.c (unity build).
 */

// Board placement under the feet, applied from ExtEquip_DrawKiteSurfBoard on the ROOT limb.
// Defined here and not in kite_surf.c because extended_equipment.c draws it, and that file is
// included long before the engine is.
//
// These are the values dialled in from the "Configure Kite Shield" popup (2026-08-18) as ADULT —
// the ROOT limb's local space is far bigger than it looks, which is why the offsets run to
// thousands.
#define KSURF_BOARD_SCALE 61.23f
#define KSURF_BOARD_ROT_X 51.81f
#define KSURF_BOARD_ROT_Y 21.9f
#define KSURF_BOARD_ROT_Z 61.42f
#define KSURF_BOARD_OFF_X (-273.0f)
#define KSURF_BOARD_OFF_Y (-1106.82f)
#define KSURF_BOARD_OFF_Z 842.73f

// Child Link's whole limb space is 11/17 of adult's — that exact fraction is in the engine, as the
// child row of sAgeProperties (z_player.c: `70.0f * (11.0f / 17.0f)`). So the board does NOT get a
// second set of seven values: the adult ones are scaled by this and the child follows automatically
// whenever the adult placement is re-tuned. Rotations are angles and do not scale.
// (Measuring the child height by hand gave 0.6365 of the adult one — the same ratio by eye.)
#define KSURF_BOARD_CHILD_RATIO (11.0f / 17.0f)

// Extra waist crouch on TOP of the riding animation, in degrees. Default 0 since the pose became
// the vanilla downhill-slide clip, which already bends the knees — this is only here to lean him
// further over the board if the clip alone is not enough.
#define KSURF_CROUCH_DEG (-13.31f)

// Rotation of the UPPER body (torso, and with it the arms and head) while riding, in degrees.
// All three axes, all defaulting to 0 = untouched, so the stance is dialled in from the popup
// rather than guessed here.
#define KSURF_UPPER_ROT_X 9.08f
#define KSURF_UPPER_ROT_Y 10.15f
#define KSURF_UPPER_ROT_Z (-11.22f)

// Same three for the LOWER body (PLAYER_LIMB_LOWER carries both legs), so the stance can be split:
// hips one way, shoulders another. Applied on top of whatever the riding clip poses.
#define KSURF_LOWER_ROT_X (-15.49f)
#define KSURF_LOWER_ROT_Y 30.44f
#define KSURF_LOWER_ROT_Z (-15.49f)

// Live lean of the torso into the turn, in degrees, on TOP of the static rotations above. Small on
// purpose — it is body language, not a pose change. Driven by how hard he is actually turning, so
// it reads the stick while free riding and the strip's curve while on a rail.
#define KSURF_UPPER_LEAN_DEG 5.0f

// The same live turn signal, but for the LOWER body and much bigger: the hips swing round as he
// carves so it reads as him steering rather than sliding. Amplitude per axis in degrees, and the
// SIGN is part of the tuning — this limb's space does not map the way you would guess (turning one
// way lifts rather than swings), so a negative value here is a normal answer, not a mistake.
// Only one axis is on by default; move the 30 to whichever one reads as turning.
#define KSURF_LOWER_TURN_X 30.0f
#define KSURF_LOWER_TURN_Y 0.0f
#define KSURF_LOWER_TURN_Z 0.0f

typedef enum {
    /* 0 */ KSURF_OFF,
    /* 1 */ KSURF_MOUNT,    // playing the equip animation, board coming out
    /* 2 */ KSURF_RIDE,     // free riding
    /* 3 */ KSURF_RAIL,     // locked to the centre of a narrow corridor
    /* 4 */ KSURF_DISMOUNT, // getting off, control handed back next frame
} KiteSurfState;

typedef struct {
    /* state machine */
    u8 state;
    s16 timer;
    /* rider */
    s16 stopFrames; // consecutive grounded frames under KSURF_STOP_SPEED
    s16 spinFrames; // >0 while a spin attack owns the animation (PAUSE released)
    s16 leanPitch;  // smoothed floor pitch along the heading, applied to shape + limbs
    s16 leanRoll;   // smoothed turn lean of the waist
    s16 upperLean;  // smaller live lean of the torso into the turn (KSURF_UPPER_LEAN_DEG)
    f32 turn;       // -1..+1, how hard and which way he is carving; drives the lower-body swing
    /* rail */
    s16 railAxisYaw;
    s16 railMiss;   // consecutive frames a side probe came back empty
    s16 railDetach; // >0 suppresses rail detection after A let go of one
    /* board trick spin (the 360 the board sometimes throws on a hop) */
    s16 boardSpin;     // current extra yaw on the board model
    s16 boardSpinRate; // 0 = not spinning; sign picks which way it goes round
    /* takeover bookkeeping */
    void* ownedAction; // actionFunc at entry; a change means damage/cutscene stole the player
} KiteSurfCtx;

static KiteSurfCtx sKSurf = { KSURF_OFF };

// True whenever the surf owns the player at all (mount and dismount included).
// Read by func_80843E64 in z_player.c to cancel fall damage.
u8 KiteSurf_IsActive(void) {
    return sKSurf.state != KSURF_OFF;
}

// True only while actually riding — the board is under his feet and the hand/back
// shield must not draw. Read by ExtEquip_DrawShieldCommon.
u8 KiteSurf_IsRiding(void) {
    return (sKSurf.state == KSURF_RIDE) || (sKSurf.state == KSURF_RAIL);
}

// Crouch and lean the lower body over the board.
//
// Applied at DRAW time from Player_OverrideLimbDrawGameplayCommon and NOT by writing
// skelAnime.jointTable from the update hook: LinkAnimation_Update only QUEUES the joint fill into
// the animation context, which is processed after every actor has updated — anything the update
// hook wrote would be overwritten before it was ever drawn.
void KiteSurf_AdjustLimb(s32 limbIndex, Vec3s* rot) {
    if (!KiteSurf_IsRiding() || (rot == NULL)) {
        return;
    }
    if (sKSurf.spinFrames > 0) {
        // Hands off during a spin attack. The surf does NOT end for it — the board keeps drawing
        // and he keeps his speed — but the spin clip has to turn him cleanly, and the board hangs
        // off the ROOT limb so it comes round with him. Layering the riding crouch and twist on
        // top would fight the clip and stop it reading as one movement.
        return;
    }
    if (limbIndex == PLAYER_LIMB_WAIST) {
        rot->x += (s16)(KSURF_CROUCH_DEG * 182.04f) + (s16)(sKSurf.leanPitch / 2);
        rot->z += sKSurf.leanRoll;
    } else if (limbIndex == PLAYER_LIMB_LOWER) {
        // Stance of the bottom half — this limb carries both legs. Static pose plus the live
        // carve: sKSurf.turn is -1.0 .. +1.0 with how hard he is turning and which way, so each
        // axis just scales it by its own amplitude.
        rot->x += (s16)((KSURF_LOWER_ROT_X + (KSURF_LOWER_TURN_X * sKSurf.turn)) * 182.04f);
        rot->y += (s16)((KSURF_LOWER_ROT_Y + (KSURF_LOWER_TURN_Y * sKSurf.turn)) * 182.04f);
        rot->z += (s16)((KSURF_LOWER_ROT_Z + (KSURF_LOWER_TURN_Z * sKSurf.turn)) * 182.04f);
    } else if (limbIndex == PLAYER_LIMB_UPPER) {
        // Stance of the top half. UPPER is the torso and the arms and head hang off it, so turning
        // this one limb moves everything above the waist without touching the legs on the board.
        // All three axes are exposed because which one reads as "side-on" depends on the limb's
        // own rest orientation, and that is a thing to see rather than to reason about.
        rot->x += (s16)(KSURF_UPPER_ROT_X * 182.04f);
        rot->y += (s16)(KSURF_UPPER_ROT_Y * 182.04f);
        // Z carries the live lean into the turn on top of its static setting.
        rot->z += (s16)(KSURF_UPPER_ROT_Z * 182.04f) + sKSurf.upperLean;
    }
}

// Defined in mods/equipment/kite_surf.c (included late in z_player.c).
extern void KiteSurf_Tick(Player* player, PlayState* play);
extern void KiteSurf_Abort(Player* player);

// Per-frame behavior while the Kite Shield is the equipped ext shield.
static void KiteShield_Behavior(Player* player, PlayState* play) {
    KiteSurf_Tick(player, play);
}

// Called when the Kite Shield is unequipped — hand the player back mid-ride.
static void KiteShield_Cleanup(void) {
    if (sKSurf.state == KSURF_OFF) {
        return;
    }
    if (gPlayState != NULL) {
        KiteSurf_Abort(GET_PLAYER(gPlayState));
    }
    sKSurf.state = KSURF_OFF;
}
