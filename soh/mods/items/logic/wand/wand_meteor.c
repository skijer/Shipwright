/**
 * wand_meteor.c — Meteor Rod (Skijer's NEI).
 *
 * A real En_Bom, thrown and red. Both axes of its motion are written rather than simulated: gravity
 * and friction are overwritten every frame, and EnBom keeps only its fuse and its explosion.
 */

#include "overlays/actors/ovl_En_Bom/z_en_bom.h" // EnBom.timer — never a hand-computed offset
#include <math.h>

extern int Player_IsZTargeting(Player* this); // z_player.c, and in no header

#define METEOR_SPAWN_DIST 30.0f
#define METEOR_SPAWN_HEIGHT 28.0f
#define METEOR_LAUNCH_SPEED 28.0f
#define METEOR_LAUNCH_RISE 6.0f
#define METEOR_FUSE_FRAMES 170

// Nothing detonates it for these first frames. Thrown from hand height it is still inside Link's own
// space, and the wall check at that range catches him, a step, or any slope underfoot.
#define METEOR_ARM_FRAMES 5

// The AT collider only exists while the bomb is ALREADY exploding, so enemy contact is proximity.
#define METEOR_ENEMY_TRIGGER_RADIUS 34.0f

// EnBom grows the bomb at timer == 67 EXACTLY (z_en_bom.c:268); a long fuse is not there for ages.
#define METEOR_BOMB_SCALE 0.01f

// Peak and speed both fall as 1/(1 + k*n): slow, where vanilla's 0.3 restitution dies in two hops.
#define METEOR_HOP_PERIOD 16.0f // frames per arc
#define METEOR_HOP_HEIGHT 46.0f // peak of the first arc, world units
#define METEOR_HOP_DECAY 0.3f   // higher = flattens and slows sooner

#define METEOR_STEER_RATE 0x900 // binang per frame the bomb may turn toward a lock-on

// Both on home.rot — En_Bom never touches it. Somaria's idiom. Hop time is METEOR_AIRBORNE until the
// throw lands, and the skip only starts there, so the bomb leaves the hand on vanilla gravity.
#define METEOR_HOP_TIME(actor) ((actor)->home.rot.x)
#define METEOR_AGE(actor) ((actor)->home.rot.z)
#define METEOR_AIRBORNE -1

#define METEOR_TINT_R 235
#define METEOR_TINT_G 45
#define METEOR_TINT_B 30
#define METEOR_TINT_MIX 255

static const u8 sMeteorEnemyCats[2] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };

static ActorFunc sMeteorBombDraw = NULL;
static ActorFunc sMeteorBombUpdate = NULL;

// Wrapped, not replaced: EnBom_Draw also billboards and runs Collider_UpdateSpheres, which is what
// gives the explosion its hitbox.
static void WandMeteor_TintDraw(Actor* thisx, PlayState* play) {
    if (sMeteorBombDraw == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, METEOR_TINT_R, METEOR_TINT_G, METEOR_TINT_B, METEOR_TINT_MIX);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, METEOR_TINT_R, METEOR_TINT_G, METEOR_TINT_B, METEOR_TINT_MIX);
    gSPGrayscale(POLY_XLU_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);

    sMeteorBombDraw(thisx, play);

    OPEN_DISPS(play->state.gfxCtx);
    gSPGrayscale(POLY_OPA_DISP++, false); // both buffers, or the rest of the frame comes out red too
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The lock-on is read live rather than captured at launch, so switching target mid-flight redirects
// the bomb and dropping Z lets it run straight on.
static Actor* WandMeteor_LockOn(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!Player_IsZTargeting(player) || (player->focusActor == NULL) || (player->focusActor->update == NULL)) {
        return NULL;
    }
    return player->focusActor;
}

// world.rot.y is the heading Actor_MoveXZGravity drives speedXZ along, so turning it steers the skip.
static void WandMeteor_Steer(Actor* thisx, PlayState* play) {
    Actor* target = WandMeteor_LockOn(play);

    if (target == NULL) {
        return;
    }
    Math_ScaledStepToS(&thisx->world.rot.y, Math_Vec3f_Yaw(&thisx->world.pos, &target->focus.pos), METEOR_STEER_RATE);
}

static u8 WandMeteor_TouchingEnemy(Actor* thisx, PlayState* play) {
    return TargetSelect_FindNearest(play, sMeteorEnemyCats, ARRAY_COUNT(sMeteorEnemyCats), NULL, &thisx->world.pos,
                                    METEOR_ENEMY_TRIGGER_RADIUS) != NULL;
}

static s16 WandMeteor_HopIndex(Actor* thisx) {
    return (s16)((f32)METEOR_HOP_TIME(thisx) / METEOR_HOP_PERIOD);
}

// speedXZ is re-stamped, not just seeded: EnBom_Move brakes at 1.0/frame while grounded
// (z_en_bom.c:169-172), and a bomb pinned near the floor is grounded on almost every frame.
static void WandMeteor_Hop(Actor* thisx) {
    f32 phase = (f32)METEOR_HOP_TIME(thisx) / METEOR_HOP_PERIOD;
    f32 falloff = 1.0f / (1.0f + (METEOR_HOP_DECAY * (f32)WandMeteor_HopIndex(thisx)));

    thisx->world.pos.y = thisx->floorHeight + (fabsf(sinf(M_PI * phase)) * METEOR_HOP_HEIGHT * falloff);
    thisx->velocity.y = 0.0f; // or the vanilla floor bounce fights the curve
    thisx->speedXZ = METEOR_LAUNCH_SPEED * falloff;
    METEOR_HOP_TIME(thisx)++;
}

static void WandMeteor_Update(Actor* thisx, PlayState* play) {
    // Read before the vanilla update: EnBom_Move consumes both bits as it bounces (z_en_bom.c:166, :176).
    u8 hitWall = (thisx->bgCheckFlags & BGCHECKFLAG_WALL) != 0;
    u8 landed = (thisx->bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) != 0;

    if (thisx->params == BOMB_BODY) {
        if (METEOR_AGE(thisx) < METEOR_ARM_FRAMES) {
            METEOR_AGE(thisx)++;
        } else if (hitWall || WandMeteor_TouchingEnemy(thisx, play)) {
            // Anything solid sets it off. Only the floor may be hit over and over — that is the skip.
            ((EnBom*)thisx)->timer = 0;
        }
        // The throw itself is vanilla gravity out of Link's hand; the skip begins where it lands.
        if (landed && (METEOR_HOP_TIME(thisx) == METEOR_AIRBORNE)) {
            METEOR_HOP_TIME(thisx) = 0;
        }
        WandMeteor_Steer(thisx, play);
    }

    sMeteorBombUpdate(thisx, play);

    if ((thisx->params == BOMB_BODY) && (METEOR_HOP_TIME(thisx) != METEOR_AIRBORNE) &&
        (thisx->floorHeight > BGCHECK_Y_MIN)) {
        WandMeteor_Hop(thisx);
    }
}

u8 WandMeteor_Cast(Player* player, PlayState* play) {
    Actor* target = WandMeteor_LockOn(play);
    s16 yaw =
        (target != NULL) ? Math_Vec3f_Yaw(&player->actor.world.pos, &target->focus.pos) : player->actor.shape.rot.y;
    f32 sn = Math_SinS(yaw);
    f32 cs = Math_CosS(yaw);
    Actor* bomb = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, player->actor.world.pos.x + (sn * METEOR_SPAWN_DIST),
                              player->actor.world.pos.y + METEOR_SPAWN_HEIGHT,
                              player->actor.world.pos.z + (cs * METEOR_SPAWN_DIST), 0, yaw, 0, BOMB_BODY);

    if (bomb == NULL) {
        return 0;
    }

    if (sMeteorBombUpdate == NULL) {
        sMeteorBombUpdate = bomb->update;
        sMeteorBombDraw = bomb->draw;
    }
    bomb->update = WandMeteor_Update;
    bomb->draw = WandMeteor_TintDraw;

    ((EnBom*)bomb)->timer = METEOR_FUSE_FRAMES;
    Actor_SetScale(bomb, METEOR_BOMB_SCALE);
    METEOR_HOP_TIME(bomb) = METEOR_AIRBORNE;
    METEOR_AGE(bomb) = 0;
    bomb->speedXZ = METEOR_LAUNCH_SPEED;
    bomb->velocity.y = METEOR_LAUNCH_RISE;
    bomb->world.rot.y = yaw;

    // NA_SE_IT_BOMB_IGNIT is continuous — vanilla only ever plays it `- SFX_FLAG`, so raw it loops.
    Audio_PlaySoundGeneral(NA_SE_PL_THROW, &bomb->world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return 1;
}
