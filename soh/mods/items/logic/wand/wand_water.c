/**
 * wand_water.c — Water Rod (Skijer's NEI).
 *
 * A rideable water column. En_Siofuki is already a DynaPoly whose top face is standable, so the rod
 * does not build a platform — it borrows the Water Temple's geyser and drives its height.
 *
 * Its own update is replaced rather than reused. The vanilla action functions poll scene switch
 * flags, fire OnePointCutscene_Init, and run a push field that blows the player off the top — none
 * of which a summoned lift wants, and all of which live behind file-static functions anyway.
 */

#include "overlays/actors/ovl_En_Siofuki/z_en_siofuki.h" // EnSiofuki.targetHeight / currentHeight

#define WATER_SPAWN_DIST 70.0f
#define WATER_RIDE_HEIGHT 240.0f
#define WATER_REST_HEIGHT 10.0f

// shape.rot.x is read as `maxHeight = rot.x * 40.0f` before Init zeroes the rotation. The rod drives
// the height itself, but the field still wants a sane value.
#define WATER_MAX_HEIGHT_ROT 6

// EnSiofuki's own smoothing constants (z_en_siofuki.c:183).
#define WATER_STEP_SCALE 0.8f
#define WATER_STEP_MAX 3.0f
#define WATER_STEP_MIN 0.01f

// The type nibble in params. Anything but 0 or 1 makes Init kill the actor.
#define WATER_PARAMS_RAISING 0x0000

static Actor* sWaterGeyser = NULL;

// The vanilla oscillation, kept because it is what sells the column as water rather than a lift.
static void WandWater_GeyserUpdate(Actor* thisx, PlayState* play) {
    EnSiofuki* geyser = (EnSiofuki*)thisx;
    f32 bob = Math_SinS((s16)(play->gameplayFrames * 0x800)) * 4.0f;

    Math_SmoothStepToF(&geyser->currentHeight, geyser->targetHeight, WATER_STEP_SCALE, WATER_STEP_MAX, WATER_STEP_MIN);
    thisx->world.pos.y = geyser->initPosY + geyser->currentHeight + bob;
    Actor_PlaySfx_Flagged(thisx, NA_SE_EV_FOUNTAIN - SFX_FLAG);
}

// The scene took the geyser with it. The pointer is dropped, never written through.
void WandWater_Forget(void) {
    sWaterGeyser = NULL;
}

static u8 WandWater_IsAlive(void) {
    return (sWaterGeyser != NULL) && (sWaterGeyser->update == WandWater_GeyserUpdate);
}

u8 WandWater_Cast(Player* player, PlayState* play) {
    Vec3f pos;
    s16 yaw = player->actor.shape.rot.y;
    EnSiofuki* geyser;

    // Already up: the cast is the lift control, so it just flips which way the column is going.
    if (WandWater_IsAlive()) {
        geyser = (EnSiofuki*)sWaterGeyser;
        geyser->targetHeight = (geyser->targetHeight > WATER_REST_HEIGHT) ? WATER_REST_HEIGHT : WATER_RIDE_HEIGHT;
        return 1;
    }

    if (Object_GetIndex(&play->objectCtx, OBJECT_SIOFUKI) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_SIOFUKI);
        return 0; // not resident yet this frame
    }

    pos.x = player->actor.world.pos.x + (Math_SinS(yaw) * WATER_SPAWN_DIST);
    pos.y = player->actor.world.pos.y;
    pos.z = player->actor.world.pos.z + (Math_CosS(yaw) * WATER_SPAWN_DIST);

    sWaterGeyser = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_SIOFUKI, pos.x, pos.y, pos.z, WATER_MAX_HEIGHT_ROT, 0, 0,
                               WATER_PARAMS_RAISING);
    if (sWaterGeyser == NULL) {
        return 0;
    }
    // Init kills itself in room 10 behind a Water Temple switch. That path leaves no draw function,
    // and unlike Obj_Lift there is no accessible one to put back — so the cast simply fails.
    if (sWaterGeyser->draw == NULL) {
        sWaterGeyser = NULL;
        return 0;
    }

    sWaterGeyser->update = WandWater_GeyserUpdate;
    sWaterGeyser->room = -1;

    geyser = (EnSiofuki*)sWaterGeyser;
    geyser->initPosY = pos.y;
    geyser->currentHeight = 0.0f;
    geyser->targetHeight = WATER_REST_HEIGHT;
    return 1;
}
