/**
 * wand_storm.c — Storm Rod (Skijer's NEI).
 *
 * Two casts in one rod, picked by whether Link has something locked on:
 *
 *   no lock-on  — the Song of Storms without the song. Oceff_Storm is only the visual cone the
 *                 ocarina spawns; En_Okarina_Effect is what rains, thunders and pulses env flag 5
 *                 for one frame, which is the pulse the grottos, bean spots, windmill and oasis all
 *                 watch. So that half is one spawn.
 *   locked on   — a thunder ray: a travelling AT cylinder that leaves a trail of real lightning.
 */

#include "../../helpers/combat_helper.h" // the ray's AT cylinder

// Defined in z_player.c further down this same translation unit, and in no header — every consumer
// declares it for itself (cane_pacci.c, equip_byrna.c, the elemental rods).
extern int Player_IsZTargeting(Player* this);

// Oceff_Storm is deliberately NOT spawned alongside the weather: its Destroy calls Magic_Reset,
// which would wipe the very meter this rod was just charged against.
#define WAND_STORM_OKARINA_PARAMS 1
#define WAND_STORM_SPAWN_Y_OFFSET -30.0f // where the ocarina puts it (z_oceff_storm.c:53-57)

#define STORM_RAY_SPEED 18.0f
#define STORM_RAY_LIFE 40
#define STORM_RAY_SPAWN_HEIGHT 30.0f
#define STORM_RAY_RADIUS 22.0f
#define STORM_RAY_HEIGHT 30.0f
#define STORM_RAY_DAMAGE 2

// The bolt is EffectSsLightning: gEffLightningDL with its own 8-frame texture animation, and it
// lives in gameplay_keep, so unlike Barinade's or Phantom Ganon's it needs no object loaded.
#define STORM_BOLT_SCALE 210 // the effect scales this by 0.01
#define STORM_BOLT_LIFE 6
#define STORM_BOLT_BRANCHES 2
#define STORM_BOLT_EVERY 2 // frames between bolts, so the trail crackles instead of smearing

// The effect's yaw is a SCREEN-SPACE roll applied after billboarding, not a world direction — En_Bili
// spreads it around a circle to make a starburst. Passing the ray's world yaw left the sprite near
// upright, which read as lightning falling from the sky. 0x4000 lays it along the ray instead.
#define STORM_BOLT_ROLL 0x4000
#define STORM_BOLT_ROLL_JITTER 4096.0f

static Color_RGBA8 sStormBoltPrim = { 255, 255, 255, 255 };
static Color_RGBA8 sStormBoltEnv = { 255, 255, 60, 255 };

static struct {
    Vec3f pos;
    Vec3f vel;
    s16 yaw;
    s16 pitch;
    s16 life;
    u8 active;
} sStormRay;

static ColliderCylinder sStormRayCol;
static u8 sStormRayColBuilt = 0;

static void WandStorm_RayConfig(CombatColliderConfig* cfg) {
    cfg->dmgFlags = DMG_BOOMERANG;
    cfg->damage = STORM_RAY_DAMAGE;
    cfg->effect = 0;
    cfg->radius = STORM_RAY_RADIUS;
    cfg->height = STORM_RAY_HEIGHT;
}

void WandStorm_Forget(void) {
    sStormRay.active = 0;
}

void WandStorm_Tick(PlayState* play, Player* player) {
    CombatColliderConfig cfg;

    if (!sStormRay.active) {
        return;
    }
    if ((--sStormRay.life <= 0) || (sStormRayCol.base.atFlags & AT_HIT)) {
        sStormRayCol.base.atFlags &= ~AT_HIT;
        sStormRay.active = 0;
        return;
    }

    sStormRay.pos.x += sStormRay.vel.x;
    sStormRay.pos.y += sStormRay.vel.y;
    sStormRay.pos.z += sStormRay.vel.z;

    // The bolt IS the visual: the effect draws and animates itself, so the rod owns no draw at all.
    if ((sStormRay.life % STORM_BOLT_EVERY) == 0) {
        s16 roll = STORM_BOLT_ROLL + (s16)Rand_CenteredFloat(STORM_BOLT_ROLL_JITTER);

        EffectSsLightning_Spawn(play, &sStormRay.pos, &sStormBoltPrim, &sStormBoltEnv, STORM_BOLT_SCALE, roll,
                                STORM_BOLT_LIFE, STORM_BOLT_BRANCHES);
    }

    WandStorm_RayConfig(&cfg);
    if (!sStormRayColBuilt) {
        sStormRayColBuilt = 1;
        Combat_InitCylinder(play, &sStormRayCol, &player->actor, &cfg);
    }
    Combat_UpdateCylinder(&sStormRayCol, &sStormRay.pos, &cfg);
    Combat_RegisterCollider(play, &sStormRayCol);
}

// Aimed at the lock-on when there is one, straight ahead otherwise.
static u8 WandStorm_FireRay(Player* player, PlayState* play, Actor* target) {
    Vec3f localVel = { 0.0f, 0.0f, STORM_RAY_SPEED };

    if (sStormRay.active) {
        return 0; // one ray at a time
    }

    sStormRay.pos = player->actor.world.pos;
    sStormRay.pos.y += STORM_RAY_SPAWN_HEIGHT;

    if (target != NULL) {
        // focus.pos, not world.pos: that is the point the game itself considers "where you aimed",
        // and it is what the fire/ice/light rods lock onto.
        sStormRay.yaw = Math_Vec3f_Yaw(&sStormRay.pos, &target->focus.pos);
        sStormRay.pitch = Math_Vec3f_Pitch(&sStormRay.pos, &target->focus.pos);
    } else {
        sStormRay.yaw = player->actor.shape.rot.y;
        sStormRay.pitch = 0;
    }

    // The elemental rods' own conversion (RodCommon_CalcVelocity): a +Z vector taken through
    // RotateY(yaw) then RotateX(pitch). Rebuilding it from Math_SinS by hand gets the pitch sign
    // backwards, which is why it is done with the matrix here too.
    Matrix_Push();
    Matrix_RotateY(BINANG_TO_RAD(sStormRay.yaw), MTXMODE_NEW);
    Matrix_RotateX(BINANG_TO_RAD(sStormRay.pitch), MTXMODE_APPLY);
    Matrix_MultVec3f(&localVel, &sStormRay.vel);
    Matrix_Pop();

    sStormRay.life = STORM_RAY_LIFE;
    sStormRay.active = 1;

    Audio_PlaySoundGeneral(NA_SE_IT_MAGIC_ARROW_SHOT, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return 1;
}

// En_Okarina_Effect kills itself on Init when a scene weather tag already owns the weather, so a
// NULL there is a legitimate "not now" and the cast reports failure rather than eating the magic.
static u8 WandStorm_CallStorm(Player* player, PlayState* play) {
    Actor* storm = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_OKARINA_EFFECT, player->actor.world.pos.x,
                               player->actor.world.pos.y + WAND_STORM_SPAWN_Y_OFFSET, player->actor.world.pos.z, 0, 0,
                               0, WAND_STORM_OKARINA_PARAMS);

    return (storm != NULL);
}

u8 WandStorm_Cast(Player* player, PlayState* play) {
    // Same lock-on test the fire rod uses, update included: a focusActor mid-Actor_Kill is a
    // dangling aim point.
    if (Player_IsZTargeting(player) && (player->focusActor != NULL) && (player->focusActor->update != NULL)) {
        return WandStorm_FireRay(player, play, player->focusActor);
    }
    return WandStorm_CallStorm(player, play);
}
