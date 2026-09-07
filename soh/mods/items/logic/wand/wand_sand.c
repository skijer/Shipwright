/**
 * wand_sand.c — Sand Rod (Skijer's NEI).
 *
 * Sand slabs laid in front of Link, placeable in mid-air. Standing on one starts it crumbling: it
 * shrinks away in a puff of earth instead of shaking and dropping like the Obj_Lift it is built on.
 * That is the whole read of the rod — ground you make, that will not hold you for long.
 *
 * Obj_Lift is used for its slab mesh and its dynapoly (somaria_cubes.c:613-670 documents the trick).
 * Its update is NOT reused: ObjLift_Wait shakes, ObjLift_Fall drops the slab and sets a scene switch
 * flag on landing, and none of the three belong to a summon. Its destroy IS kept — that is what
 * unregisters the collision.
 */

#include "objects/object_d_lift/object_d_lift.h" // gCollapsingPlatformDL
#include "../../helpers/fx_helper.h"             // FX_SpawnRadialDust

// The ring is a spam limit, not the lifetime: a slab normally dies by being stood on.
#define SAND_MAX_SLABS 8

#define SAND_SLAB_SCALE 0.05f // ObjLift sScales[1], the small one
#define SAND_HOLD_INTERVAL 6  // frames between coverage checks while the button is held

// How far ahead the next slab lands, as a share of the slab's own reach. Under 1.0 so consecutive
// slabs overlap instead of leaving a seam to fall through.
#define SAND_STEP_FRACTION 0.85f

// Until the first slab has been measured, this is where one goes. Only ever used once.
#define SAND_FALLBACK_DIST 40.0f

// How long a slab lasts once Link is standing on it. Long enough to step off, short enough that a
// bridge has to be built forward instead of stood on.
#define SAND_CRUMBLE_FRAMES 24
#define SAND_DUST_EVERY 4

// One-shot earth sounds, NOT NA_SE_EV_SAND_STORM: every vanilla caller plays that one as
// `- SFX_FLAG`, the continuous variant re-issued every frame. Started raw it simply never stops.
#define SAND_SFX_TICKS 20

// The crumble countdown rides on home.rot.x. Once the update below is installed no ObjLift code ever
// runs on this actor again except its destroy, which only reads dyna.bgId — so the field is free.
// Same "state on a hijacked actor field" idiom as the Somaria summons.
#define SAND_CRUMBLE(actor) ((actor)->home.rot.x)

// ObjLift reads a scene switch flag out of (params >> 2) & 0x3F, and with params 0 that is flag 0.
// Only its Init matters here — the update that would WRITE the flag is replaced.
#define SAND_SWITCH_FLAG 0

// Desert sand over the slab's own stone texture, and the same colour for the dust.
#define SAND_ENV_R 214
#define SAND_ENV_G 178
#define SAND_ENV_B 112

static Actor* sSandSlabs[SAND_MAX_SLABS];
static u8 sSandNextSlot = 0;
static s16 sSandHoldTimer = 0;
static FX_Color sSandDustColor = { SAND_ENV_R, SAND_ENV_G, SAND_ENV_B, 255 };

// The slab is MEASURED off its own registered collision, not guessed: world units from the actor
// origin up to the surface Link stands on, and out to its nearest edge. Both are constant for the
// life of the game, so the first slab pays for it.
static f32 sSandTopOffset = 0.0f;
static f32 sSandReach = 0.0f;
static u8 sSandMeasured = 0;

static void WandSand_Measure(PlayState* play, Actor* slab) {
    s32 bgId = ((DynaPolyActor*)slab)->bgId;
    CollisionHeader* header;
    f32 halfX;
    f32 halfZ;

    if (sSandMeasured || (bgId < 0) || (bgId >= BG_ACTOR_MAX)) {
        return;
    }
    header = play->colCtx.dyna.bgActors[bgId].colHeader;
    if (header == NULL) {
        return;
    }
    halfX = (f32)(header->maxBounds.x - header->minBounds.x) * 0.5f * SAND_SLAB_SCALE;
    halfZ = (f32)(header->maxBounds.z - header->minBounds.z) * 0.5f * SAND_SLAB_SCALE;

    sSandTopOffset = (f32)header->maxBounds.y * SAND_SLAB_SCALE;
    sSandReach = (halfX < halfZ) ? halfX : halfZ;
    sSandMeasured = 1;
}

// Where the next slab wants to go: one step ahead of Link, with its walking surface at his feet.
// The player's own origin sits on the floor, which is why the slab is dropped by its top offset
// rather than by a number picked to look right.
static void WandSand_NextSpot(Player* player, Vec3f* out) {
    s16 yaw = player->actor.shape.rot.y;
    f32 dist = sSandMeasured ? (sSandReach * SAND_STEP_FRACTION) : SAND_FALLBACK_DIST;

    out->x = player->actor.world.pos.x + (Math_SinS(yaw) * dist);
    out->y = player->actor.world.pos.y - sSandTopOffset;
    out->z = player->actor.world.pos.z + (Math_CosS(yaw) * dist);
}

// Is that spot already standable? A slab that has started crumbling does not count — it is on its
// way out and the next one has to be laid before it goes.
static u8 WandSand_Covers(Vec3f* spot) {
    for (u8 i = 0; i < SAND_MAX_SLABS; i++) {
        Actor* slab = sSandSlabs[i];
        f32 dx;
        f32 dz;

        if ((slab == NULL) || (SAND_CRUMBLE(slab) != 0)) {
            continue;
        }
        dx = slab->world.pos.x - spot->x;
        dz = slab->world.pos.z - spot->z;
        if (((dx * dx) + (dz * dz)) < (sSandReach * sSandReach)) {
            return 1;
        }
    }
    return 0;
}

// Called while the slab is still alive — one frame later its slot belongs to somebody else.
static void WandSand_Drop(Actor* slab) {
    for (u8 i = 0; i < SAND_MAX_SLABS; i++) {
        if (sSandSlabs[i] == slab) {
            sSandSlabs[i] = NULL;
        }
    }
}

// How much of the slab is left, 1.0 while it is solid. Read by the draw and by nothing else — the
// collision deliberately does not follow it.
static f32 WandSand_Remaining(Actor* slab) {
    if (SAND_CRUMBLE(slab) <= 0) {
        return 1.0f;
    }
    return (f32)SAND_CRUMBLE(slab) / (f32)SAND_CRUMBLE_FRAMES;
}

/**
 * The slab's whole life. No shake and no fall: standing on it starts a countdown, and the countdown
 * eats the slab away.
 *
 * The countdown never touches actor->scale, only the draw — the dynapoly is built from the actor's
 * transform, so scaling the actor would shrink the floor out from under Link while he is still
 * standing on it. The platform stays full size and honest right up to the frame it is killed, and
 * ObjLift_Destroy takes the collision at exactly the moment the model stops being drawn.
 */
static void WandSand_SlabUpdate(Actor* thisx, PlayState* play) {
    if (SAND_CRUMBLE(thisx) == 0) {
        if (!DynaPolyActor_IsPlayerOnTop((DynaPolyActor*)thisx)) {
            return;
        }
        SAND_CRUMBLE(thisx) = SAND_CRUMBLE_FRAMES;
        // Fixed-world-pos, not the actor's: the slab is killed partway through the sound, and a
        // source pointed at a dead actor's position reads memory that is no longer its own.
        SoundSource_PlaySfxAtFixedWorldPos(play, &thisx->world.pos, SAND_SFX_TICKS, NA_SE_EV_FALL_DOWN_DIRT);
    }

    SAND_CRUMBLE(thisx)--;
    if ((SAND_CRUMBLE(thisx) % SAND_DUST_EVERY) == 0) {
        FX_SpawnRadialDust(play, &thisx->world.pos, 10.0f, 40.0f, 3, &sSandDustColor);
    }

    if (SAND_CRUMBLE(thisx) <= 0) {
        WandSand_Drop(thisx);
        Actor_Kill(thisx);
    }
}

static void WandSand_SlabDraw(Actor* thisx, PlayState* play) {
    f32 remaining = WandSand_Remaining(thisx);

    OPEN_DISPS(play->state.gfxCtx);
    // Components spelled out — a multi-value #define does not survive MSVC's macro expansion.
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, SAND_ENV_R, SAND_ENV_G, SAND_ENV_B, 255);
    CLOSE_DISPS(play->state.gfxCtx);

    // Applied to the matrix Actor_Draw already set up, and only here: Gfx_DrawDListOpa builds its
    // matrix from the current stack, so this shrinks the model and leaves the collision alone.
    Matrix_Scale(remaining, remaining, remaining, MTXMODE_APPLY);
    Gfx_DrawDListOpa(play, gCollapsingPlatformDL);
}

// The scene took the slabs with it. Pointers are dropped, never written through: a dead actor's
// slot may already belong to something else.
void WandSand_Forget(void) {
    for (u8 i = 0; i < SAND_MAX_SLABS; i++) {
        sSandSlabs[i] = NULL;
    }
    sSandNextSlot = 0;
    sSandHoldTimer = 0;
}

// Overflow only — a slab normally ends by crumbling under Link. This is the backstop that stops a
// held button from filling the actor list: the oldest is told to start crumbling and then let go of.
// It finishes on its own; the countdown lives on the actor, not here.
static void WandSand_Evict(u8 slot) {
    Actor* slab = sSandSlabs[slot];

    if ((slab != NULL) && (SAND_CRUMBLE(slab) == 0)) {
        SAND_CRUMBLE(slab) = SAND_CRUMBLE_FRAMES;
    }
    sSandSlabs[slot] = NULL;
}

u8 WandSand_Cast(Player* player, PlayState* play) {
    Vec3f pos;
    s16 yaw = player->actor.shape.rot.y;
    Actor* slab;
    u8 flagWasSet;

    if (Object_GetIndex(&play->objectCtx, OBJECT_D_LIFT) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_D_LIFT);
        return 0; // not resident yet this frame
    }

    // Placed blind, with no raycast on purpose: that is what lets a slab hang in mid-air over a gap.
    WandSand_NextSpot(player, &pos);

    // ObjLift_Init kills itself when SAND_SWITCH_FLAG is already set. Nothing else runs between
    // these two lines — this is inside the player's own update — so the flag is put straight back.
    flagWasSet = Flags_GetSwitch(play, SAND_SWITCH_FLAG) != 0;
    if (flagWasSet) {
        Flags_UnsetSwitch(play, SAND_SWITCH_FLAG);
    }
    slab = Actor_Spawn(&play->actorCtx, play, ACTOR_OBJ_LIFT, pos.x, pos.y, pos.z, 0, yaw, 0, 0);
    if (flagWasSet) {
        Flags_SetSwitch(play, SAND_SWITCH_FLAG);
    }

    if ((slab == NULL) || (slab->update == NULL)) {
        return 0;
    }

    // destroy is left alone: ObjLift_Destroy is what unregisters the dynapoly.
    slab->update = WandSand_SlabUpdate;
    slab->draw = WandSand_SlabDraw;
    Actor_SetScale(slab, SAND_SLAB_SCALE);
    slab->room = -1;
    SAND_CRUMBLE(slab) = 0;

    // The collision only exists once Init has registered it, so the very first slab is the one that
    // can be measured — and once measured, it is put where it should have gone. The bg system reads
    // the transform off the actor every frame, so moving it here is enough.
    WandSand_Measure(play, slab);
    if (sSandMeasured) {
        WandSand_NextSpot(player, &pos);
        slab->world.pos = pos;
    }

    WandSand_Evict(sSandNextSlot);
    sSandSlabs[sSandNextSlot] = slab;
    sSandNextSlot = (u8)((sSandNextSlot + 1) % SAND_MAX_SLABS);

    FX_SpawnRadialDust(play, &pos, 8.0f, 30.0f, 4, &sSandDustColor);
    SoundSource_PlaySfxAtFixedWorldPos(play, &pos, SAND_SFX_TICKS, NA_SE_EV_LAND_DIRT);
    return 1;
}

/**
 * Should the held button lay one right now? The caller casts, so a held slab is billed and gated
 * exactly like a pressed one.
 *
 * This is what makes the rod a road: it only lays a slab when the step ahead has nothing standable
 * under it, so holding the button and walking forward keeps producing ground for as long as you
 * keep going, and standing still stops costing magic.
 */
u8 WandSand_HoldElapsed(Player* player, u8 held) {
    Vec3f spot;

    if (!held) {
        sSandHoldTimer = 0;
        return 0;
    }
    if (--sSandHoldTimer > 0) {
        return 0;
    }
    sSandHoldTimer = SAND_HOLD_INTERVAL;

    WandSand_NextSpot(player, &spot);
    return !WandSand_Covers(&spot);
}
