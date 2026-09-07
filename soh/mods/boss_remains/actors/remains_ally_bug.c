/**
 * remains_ally_bug.c - Odolwa's friendly "bug" summon ally (SoH port, runtime ActorDB id).
 *
 * KAMIKAZE, ephemeral: a small billboarded moth-bug that HOPS toward the nearest enemy and,
 * on reaching it, delivers a burst of AT_TYPE_PLAYER damage and dies (Actor_Kill). Odolwa's
 * remains spawns a few of these at once (each call to RemainsAllyBug_Spawn makes ONE bug).
 *
 * ---- Unity include (NOT in CMake/vcxproj) ----------------------------------------------
 * This .c is #included into boss_remains.cpp inside its `extern "C"` block, so the
 * lifecycle functions / RemainsAllyBug_Spawn get C linkage. It is therefore compiled as C++:
 *   - NEVER name a local `this` (reserved word in C++) -> the typed pointer is `self`.
 *   - asset symbols cast explicitly (no implicit ptr->ptr in C++).
 * remains_ally_common.c must be #included BEFORE this file in boss_remains.cpp so the
 * RemainsAlly_* helpers are defined.
 *
 * ---- Registration (SoH ActorDB, replaces MM's DEFINE_ACTOR table) ----------------------
 * No fixed ACTOR_ id exists in SoH for custom actors. boss_remains_actor_reg.cpp registers
 * the actor at runtime (ActorDB::Instance->AddEntry) and stores the id in gRemainsAllyBugId.
 * The Spawn helpers call BossRemains_EnsureActorsRegistered() first so the id is valid
 * lazily on first use. Lifecycle funcs are NON-static so the reg .cpp can extern "C" them.
 *
 * ---- Art (MM assets via mm.o2r) --------------------------------------------------------
 * All MM models/DLs are loaded through the MmAssets_* bridge (mm_asset_loader.h), NEVER by
 * handing "__OTR__" path strings to the gfx pipe (oot's resource index doesn't know them):
 *   ground beetle : objects/object_boss01/gOdolwaBugSkel + gOdolwaBugCrawlAnim (FLEX skel)
 *   moth sprite   : overlays/ovl_En_Tanron1/ovl_En_Tanron1_DL_001888 (setup) + _DL_001900 (model)
 *   thunder bolt  : objects/object_boss_hakugin/gGohtLightningMaterialDL + gGohtLightningModelDL
 * MmAssets_Load* returns NULL while mm.o2r isn't mounted yet — the proven MmSoul pattern
 * (randomizer/draw.cpp) applies: retry the load every Update, never latch a failure, and
 * skip skelanime/draw until everything resolved (mmAssetsReady).
 */

#include "remains_ally_common.h"

#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
// MmAssets_LoadSkeleton/LoadAnimation/LoadResource + MmSfx_PlayAtPos (MM sfx bridge).
#include "mods/transformation_masks/assets/mm_asset_loader.h"
// MM sfx id constants (MM_NA_SE_*). The enemy-bank ids this actor needs are not in the
// bridge header yet, so they are defined locally below from 2ship mm/include/sfx.h.
#include "mods/sound_translator/mm_sfx_ids.h"

// ---- MM sfx ids not present in mm_sfx_ids.h (values verified in 2ship mm/include/sfx.h) ----
#ifndef MM_NA_SE_EN_MB_MOTH_FLY
#define MM_NA_SE_EN_MB_MOTH_FLY 0x399B // mm/include/sfx.h:1486
#endif
#ifndef MM_NA_SE_EN_MB_MOTH_DEAD
#define MM_NA_SE_EN_MB_MOTH_DEAD 0x399C // mm/include/sfx.h:1487
#endif
#ifndef MM_NA_SE_EN_COMMON_THUNDER
#define MM_NA_SE_EN_COMMON_THUNDER 0x394B // mm/include/sfx.h:1406
#endif

#define BUG_ODOLWA_LIMB_MAX 0x15 // ODOLWA_BUG_LIMB_MAX (2ship object_boss01.h:430)
#define BUG_BEETLE_SCALE 0.025f  // Boss01_Bug uses 0.025

// ============================================================================
// Art paths (mm.o2r, NO "__OTR__" prefix — MmAssets_* adds/handles it) + tuning
// ============================================================================

static const char* const sBugSkelPath = "objects/object_boss01/gOdolwaBugSkel";
static const char* const sBugAnimPath = "objects/object_boss01/gOdolwaBugCrawlAnim";
static const char* const sMothSetupDLPath = "overlays/ovl_En_Tanron1/ovl_En_Tanron1_DL_001888";
static const char* const sMothModelDLPath = "overlays/ovl_En_Tanron1/ovl_En_Tanron1_DL_001900";
static const char* const sThunderMatDLPath = "objects/object_boss_hakugin/gGohtLightningMaterialDL";
static const char* const sThunderModelDLPath = "objects/object_boss_hakugin/gGohtLightningModelDL";

#define BUG_SEARCH_RANGE 600.0f // XZ radius to look for an enemy each frame
#define BUG_HOP_INTERVAL 15     // frames between upward hops
#define BUG_HOP_STRENGTH 4.5f   // velocity.y injected on a hop
#define BUG_CHASE_SPEED 4.2f    // XZ speed while homing on an enemy
#define BUG_IDLE_SPEED 2.5f     // XZ speed while following the player
#define BUG_HIT_DIST 30.0f      // XZ distance at which the kamikaze detonates
#define BUG_FOLLOW_DIST 55.0f   // idle: stop this close to the player
#define BUG_IDLE_LIFETIME 300   // frames the bug survives with nothing to fight
#define BUG_GRAVITY -1.0f       // pulls each hop back down
#define BUG_TERMINAL_VY -8.0f   // clamp on fall speed
#define BUG_DRAW_SCALE 4.5f     // billboard sprite scale — big + obvious (vanilla En_Tanron1 particle ~1.2)
#define BUG_DRAW_Y_LIFT 8.0f    // lift the sprite off its ground anchor
#define BUG_FLAP_RATE 0x1400    // wing-flap phase advance per frame (binang)

// Projectile mode (Odolwa sword-beam moth): fly straight forward like an FD sword beam.
#define BUG_MODE_PROJECTILE 1
#define BUG_PROJECTILE_SPEED 26.0f       // forward flight speed
#define BUG_PROJECTILE_TTL 45            // frames before it fizzles if it hits nothing
#define BUG_PROJECTILE_HOME_RANGE 550.0f // look this far for an enemy to curve toward
#define BUG_PROJECTILE_HOME_CONE 0x3000  // only home if the enemy is within ~66° of the flight path
#define BUG_PROJECTILE_HOME_STEP 0x600   // max yaw turn per frame toward it (gentle self-guiding)

// Thunder mode (Goht's R+A bolt): a lightning bolt that flies straight, PIERCES WALLS (no bgcheck /
// wall kill), homes like the beam, and hits harder. Visual = Goht's real lightning DLs, Goht cyan.
#define BUG_MODE_THUNDER 2
#define BUG_THUNDER_SPEED 34.0f
#define BUG_THUNDER_TTL 40
#define BUG_THUNDER_DAMAGE 0x06
#define BUG_THUNDER_SFX_CADENCE 8 // frames between crackle one-shots (bridge has no looped model)

// Cloud mode (Odolwa "Nimbus" flight): a moth that hovers UNDER Link in an orbiting ring, forming the
// cloud that carries him. It only exists while Link is flying (BossRemains_IsOdolwaFlying) and never
// attacks. hopTimer is repurposed as the orbit angle.
#define BUG_MODE_CLOUD 3
#define BUG_CLOUD_RADIUS 22.0f // ring radius under Link
#define BUG_CLOUD_BELOW 10.0f  // how far below Link the cloud sits
#define BUG_CLOUD_ORBIT 0x0300 // orbit angular speed (binang/frame)

// Pikmin ball (ground beetles trail Link like Gyorg's fish school): a loose spaced BALL BEHIND Link that
// only breaks formation to attack when Link holds still, converging on the enemy nearest to LINK.
#define BUG_BALL_DIST 50.0f    // base distance the ball trails behind Link
#define BUG_BALL_SPACING 26.0f // extra ring depth so they don't all sit at one radius
#define BUG_BALL_SEP 22.0f     // boids separation radius (keeps the ball from merging to a dot)
#define BUG_LINK_MOVE 2.5f     // Link linearVelocity above this = "moving" → re-form (don't peel off)

extern s32 BossRemains_IsOdolwaFlying(void);          // defined later in the same TU (boss_remains.cpp)
extern void BossRemains_EnsureActorsRegistered(void); // boss_remains_actor_reg.cpp (lazy ActorDB reg)

// Runtime ActorDB id — filled by BossRemains_EnsureActorsRegistered(); -1 until then.
s16 gRemainsAllyBugId = -1;

typedef struct RemainsAllyBug {
    /**/ Actor actor;
    /**/ SkelAnime skelAnime; // ground mode: Odolwa's bug (gOdolwaBugSkel) crawling skeleton
    /**/ Vec3s jointTable[BUG_ODOLWA_LIMB_MAX];
    /**/ Vec3s morphTable[BUG_ODOLWA_LIMB_MAX];
    /**/ ColliderCylinder collider;
    /**/ s16 spawnParams;                  // 0 = ground beetle, BUG_MODE_PROJECTILE/THUNDER/CLOUD otherwise
    /**/ s16 hopTimer;                     // frames until the next hop (CLOUD: orbit angle)
    /**/ s16 lifeTimer;                    // hard TTL countdown
    /**/ s16 flapTimer;                    // projectile (moth) wing-flap phase
                                           /* -- mm.o2r deferred-load state (SoH-only) -- */
    /**/ u8 mmAssetsReady;                 // all assets for THIS mode resolved (never latched false-forever)
    /**/ FlexSkeletonHeader* beetleSkel;   // ground mode
    /**/ AnimationHeader* beetleCrawlAnim; // ground mode
    /**/ Gfx* mothSetupDL;                 // projectile/cloud modes
    /**/ Gfx* mothModelDL;                 // projectile/cloud modes
    /**/ Gfx* thunderMatDL;                // thunder mode
    /**/ Gfx* thunderModelDL;              // thunder mode
} RemainsAllyBug;

// The reg .cpp cannot see this struct (it lives in this unity-included file), so it takes
// the instance size through this global — same trick sw97 uses with its SIZE defines.
size_t gRemainsAllyBugStructSize = sizeof(RemainsAllyBug);

// ============================================================================
// Collider — the crux of "friendly": friend/foe is the AT/AC TYPE bits, NOT category.
// AT_TYPE_PLAYER toucher hits every enemy (their body bumpers are AC_TYPE_PLAYER) and can
// NEVER hit Link (his body bumper is AC_TYPE_ENEMY). Masks copied from Ivan
// (z_en_partner.c) so the hit actually lands: the CollisionCheck gate needs a shared TYPE
// bit AND overlapping dmgFlags. AC_NONE => we never call CollisionCheck_SetAC, so the bug
// is invulnerable (the bumper init below is inert but kept as Ivan's value for parity).
// No OC so it never shoves actors around.
// MM->OoT field renames: COL_MATERIAL_NONE->COLTYPE_NONE, ELEM_MATERIAL_UNK0->ELEMTYPE_UNK0,
// ATELEM_*->TOUCH_*, ACELEM_NONE->BUMP_NONE.
// ============================================================================
static ColliderCylinderInit sRemainsAllyBugColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        // Ivan's toucher dmgFlags (deku-stick class hit, exists in OoT's 32-bit table) so
        // enemy bumpers accept it; damage 4 (identical amount to the MM file).
        { DMG_DEKU_STICK, 0x00, 0x04 },
        // Ivan's accept-all bumper mask — inert here (AC_NONE), kept for parity.
        { 0xF7CFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    // Radius 24 / height 44 — deliberately generous so the AT toucher OVERLAPS an enemy's AC bumper
    // a few frames BEFORE the proximity kill distance (BUG_HIT_DIST) fires; otherwise a too-small
    // cylinder would let the kamikaze self-destruct before the hit ever lands.
    { 24, 44, 0, { 0, 0, 0 } },
};

// MM DLs can branch into segment 0x0C (scene cull list). The moth sprite almost certainly
// does not, but bind it to a no-op gsSPEndDisplayList defensively so any stray 0x0C branch
// just returns instead of crashing.
static Gfx sBugSegment0xC_Noop[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

// ============================================================================
// FORWARD DECLARATIONS (non-static: extern "C"-visible for boss_remains_actor_reg.cpp)
// ============================================================================

void RemainsAllyBug_Init(Actor* thisx, PlayState* play);
void RemainsAllyBug_Destroy(Actor* thisx, PlayState* play);
void RemainsAllyBug_Update(Actor* thisx, PlayState* play);
void RemainsAllyBug_Draw(Actor* thisx, PlayState* play);

// ============================================================================
// DEFERRED MM ASSET LOAD (retry-until-ready, per the MmSoul pattern)
// ============================================================================

// Try to resolve every mm.o2r asset THIS mode needs. Returns true (and finishes the
// SkelAnime init for the ground beetle) once everything is loaded; false means mm.o2r
// isn't mounted yet — call again next frame, NEVER latch the failure.
static s32 RemainsAllyBug_TryLoadAssets(RemainsAllyBug* self, PlayState* play) {
    if (self->mmAssetsReady) {
        return true;
    }

    if (self->spawnParams == BUG_MODE_THUNDER) {
        self->thunderMatDL = (Gfx*)MmAssets_LoadResource(sThunderMatDLPath);
        self->thunderModelDL = (Gfx*)MmAssets_LoadResource(sThunderModelDLPath);
        if ((self->thunderMatDL == NULL) || (self->thunderModelDL == NULL)) {
            return false;
        }
    } else if (self->spawnParams != 0) { // PROJECTILE / CLOUD: the moth billboard pair
        self->mothSetupDL = (Gfx*)MmAssets_LoadResource(sMothSetupDLPath);
        self->mothModelDL = (Gfx*)MmAssets_LoadResource(sMothModelDLPath);
        if ((self->mothSetupDL == NULL) || (self->mothModelDL == NULL)) {
            return false;
        }
    } else { // ground beetle: FLEX skeleton + crawl anim
        self->beetleSkel = (FlexSkeletonHeader*)MmAssets_LoadSkeleton(sBugSkelPath);
        self->beetleCrawlAnim = (AnimationHeader*)MmAssets_LoadAnimation(sBugAnimPath);
        if ((self->beetleSkel == NULL) || (self->beetleCrawlAnim == NULL)) {
            return false;
        }
        SkelAnime_InitFlex(play, &self->skelAnime, self->beetleSkel, self->beetleCrawlAnim, self->jointTable,
                           self->morphTable, BUG_ODOLWA_LIMB_MAX);
        Animation_PlayLoop(&self->skelAnime, self->beetleCrawlAnim);
    }

    self->mmAssetsReady = true;
    return true;
}

// ============================================================================
// INIT / DESTROY
// ============================================================================

void RemainsAllyBug_Init(Actor* thisx, PlayState* play) {
    RemainsAllyBug* self = (RemainsAllyBug*)thisx;

    self->spawnParams = thisx->params; // 0 = ground beetle, BUG_MODE_PROJECTILE(1) = flying moth-beam
    self->mmAssetsReady = false;
    self->beetleSkel = NULL;
    self->beetleCrawlAnim = NULL;
    self->mothSetupDL = NULL;
    self->mothModelDL = NULL;
    self->thunderMatDL = NULL;
    self->thunderModelDL = NULL;

    if (self->spawnParams != 0) {
        // Projectile modes: 1 = sword-beam MOTH (billboard), 2 = Goht THUNDER bolt. Both fly
        // dead-straight (no gravity), short-lived.
        self->flapTimer = (s16)Rand_ZeroFloat(65535.0f);
        self->actor.shape.shadowDraw = NULL;
        self->actor.shape.shadowScale = 0.0f;
        Actor_SetScale(&self->actor, 0.01f);
        self->actor.gravity = 0.0f;
        self->actor.minVelocityY = 0.0f; // MM terminalVelocity -> OoT minVelocityY
        self->actor.speedXZ = (self->spawnParams == BUG_MODE_THUNDER) ? BUG_THUNDER_SPEED : BUG_PROJECTILE_SPEED;
        self->actor.world.rot.y = self->actor.shape.rot.y; // Actor_MoveXZGravity flies along this
        self->hopTimer = 0;
        self->lifeTimer = (self->spawnParams == BUG_MODE_THUNDER) ? BUG_THUNDER_TTL : BUG_PROJECTILE_TTL;

        if (self->spawnParams == BUG_MODE_CLOUD) {
            // Not a projectile: it hovers under Link. hopTimer = orbit angle (seeded from spawn rot), no
            // speed of its own, and no TTL — it lives until the flight ends (self-kills in Update).
            self->actor.speedXZ = 0.0f;
            self->hopTimer = self->actor.shape.rot.y;
            self->lifeTimer = 0;
            Actor_SetScale(&self->actor, 0.014f);
        }
    } else {
        // Ground = Odolwa's real BEETLE. The FLEX skeleton + crawl anim come from mm.o2r and may
        // not be mounted yet — the SkelAnime init happens inside TryLoadAssets once they resolve
        // (retried every Update). Everything not skeleton-dependent is set up here as in MM.
        Actor_SetScale(&self->actor, BUG_BEETLE_SCALE);
        ActorShape_Init(&self->actor.shape, 0.0f, ActorShadow_DrawCircle, 12.0f);
        self->actor.gravity = BUG_GRAVITY;
        self->actor.minVelocityY = BUG_TERMINAL_VY;
        self->hopTimer = (s16)Rand_ZeroFloat((f32)BUG_HOP_INTERVAL); // desync a group's hops
        self->lifeTimer = BUG_IDLE_LIFETIME;
    }

    RemainsAllyBug_TryLoadAssets(self, play); // first attempt; Update keeps retrying on NULL

    // Collider is initialised HERE (after the actor is linked), per the contract.
    Collider_InitCylinder(play, &self->collider);
    Collider_SetCylinder(play, &self->collider, &self->actor, &sRemainsAllyBugColliderInit);
    if (self->spawnParams == BUG_MODE_THUNDER) {
        // The bolt hits harder, and per the MM->OoT damage-class map thunder rides the
        // magic-fire dmg bit (damage AMOUNT identical to the MM file).
        self->collider.info.toucher.dmgFlags = DMG_MAGIC_FIRE;
        self->collider.info.toucher.damage = BUG_THUNDER_DAMAGE;
    }
}

void RemainsAllyBug_Destroy(Actor* thisx, PlayState* play) {
    RemainsAllyBug* self = (RemainsAllyBug*)thisx;

    Collider_DestroyCylinder(play, &self->collider);
}

// Boids separation among GROUND beetles: nudge apart so the Pikmin ball stays spaced instead of merging
// into one dot (mirrors RemainsAllyFish_Separate). Only ground bugs (spawnParams 0) push each other.
static void RemainsAllyBug_Separate(RemainsAllyBug* self, PlayState* play) {
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_MISC].head; a != NULL; a = a->next) {
        if ((a == &self->actor) || (a->id != gRemainsAllyBugId) || (((RemainsAllyBug*)a)->spawnParams != 0)) {
            continue;
        }
        f32 d = Math_Vec3f_DistXZ(&self->actor.world.pos, &a->world.pos);
        if ((d < BUG_BALL_SEP) && (d > 0.1f)) {
            s16 away = Math_Vec3f_Yaw(&a->world.pos, &self->actor.world.pos); // neighbor -> self
            f32 push = (BUG_BALL_SEP - d) * 0.25f;
            self->actor.world.pos.x += Math_SinS(away) * push;
            self->actor.world.pos.z += Math_CosS(away) * push;
        }
    }
}

// ============================================================================
// UPDATE — Pikmin ball behind Link (like Gyorg's fish), peeling off to swarm the
// nearest enemy to Link only while Link holds still; kamikaze on contact.
// ============================================================================

void RemainsAllyBug_Update(Actor* thisx, PlayState* play) {
    RemainsAllyBug* self = (RemainsAllyBug*)thisx;
    Actor* target;

    // mm.o2r may mount a few frames after us — keep retrying; AI runs regardless,
    // only the skeleton anim / draw wait on the assets.
    RemainsAllyBug_TryLoadAssets(self, play);

    if (self->spawnParams != 0) {
        self->flapTimer += BUG_FLAP_RATE; // moth wing-flap / thunder flicker phase
    } else if (self->mmAssetsReady) {
        SkelAnime_Update(&self->skelAnime); // crawl the beetle's legs
    }

    // CLOUD: hover under Link in an orbiting ring for as long as the flight lasts; self-kill when it ends.
    if (self->spawnParams == BUG_MODE_CLOUD) {
        if (!BossRemains_IsOdolwaFlying()) {
            Actor_Kill(&self->actor);
            return;
        }
        Player* pl = GET_PLAYER(play);
        if (pl != NULL) {
            self->hopTimer += BUG_CLOUD_ORBIT; // swirl the ring
            self->actor.world.pos.x = pl->actor.world.pos.x + (Math_SinS(self->hopTimer) * BUG_CLOUD_RADIUS);
            self->actor.world.pos.z = pl->actor.world.pos.z + (Math_CosS(self->hopTimer) * BUG_CLOUD_RADIUS);
            self->actor.world.pos.y = pl->actor.world.pos.y - BUG_CLOUD_BELOW;
            self->actor.shape.rot.y = self->hopTimer;
        }
        return;
    }

    if (self->hopTimer > 0) {
        self->hopTimer--;
    }

    // Hard TTL: every bug expires after BUG_IDLE_LIFETIME frames whether or not it ever reaches a
    // foe — so a target it can't actually get to (behind a wall, flying, unreachable) can't keep the
    // kamikaze alive forever. Kamikaze-on-contact (below) still ends it early on a successful hit.
    if (self->lifeTimer > 0) {
        self->lifeTimer--;
        if (self->lifeTimer == 0) {
            MmSfx_PlayAtPos(MM_NA_SE_EN_MB_MOTH_DEAD, &self->actor.projectedPos);
            Actor_Kill(&self->actor);
            return;
        }
    }

    // Projectile modes: fly dead-straight, damage the first enemy touched. Moth-beam (1) dies on
    // walls; Goht THUNDER (2) PIERCES WALLS (no bg check at all) and just times out.
    if (self->spawnParams != 0) {
        // Light self-guiding: if an enemy is roughly ahead, curve gently toward it (a little homing,
        // not a hard lock). Enemies behind/beside are ignored so the bolt still reads as forward-fired.
        Actor* homeTarget = RemainsAlly_FindNearestEnemy(play, &self->actor.world.pos, BUG_PROJECTILE_HOME_RANGE);
        if (homeTarget != NULL) {
            s16 toTarget = Actor_WorldYawTowardActor(&self->actor, homeTarget);
            s16 diff = toTarget - self->actor.world.rot.y;
            s16 absDiff = (diff < 0) ? -diff : diff;
            if (absDiff < BUG_PROJECTILE_HOME_CONE) {
                Math_SmoothStepToS(&self->actor.world.rot.y, toTarget, 3, BUG_PROJECTILE_HOME_STEP, 0);
                self->actor.shape.rot.y = self->actor.world.rot.y;
            }
        }
        Actor_MoveXZGravity(&self->actor); // gravity 0 → flies along world.rot.y
        if (self->spawnParams == BUG_MODE_PROJECTILE) {
            Actor_UpdateBgCheckInfo(play, &self->actor, 20.0f, 12.0f, 0.0f, 0x1); // 0x1 = wall check only
        } else {
            // thunder: NO bg check — it flies through walls; crackle as it travels. MM played this
            // as a looped/flagged sfx; the MM bridge has no flagged model, so re-fire the one-shot
            // on a frame cadence instead.
            if ((self->lifeTimer % BUG_THUNDER_SFX_CADENCE) == 0) {
                MmSfx_PlayAtPos(MM_NA_SE_EN_COMMON_THUNDER, &self->actor.projectedPos);
            }
        }
        Collider_UpdateCylinder(&self->actor, &self->collider);
        CollisionCheck_SetAT(play, &play->colChkCtx, &self->collider.base);
        if ((self->collider.base.atFlags & AT_HIT) ||
            ((self->spawnParams == BUG_MODE_PROJECTILE) && (self->actor.bgCheckFlags & BGCHECKFLAG_WALL))) {
            self->collider.base.atFlags &= ~AT_HIT;
            MmSfx_PlayAtPos(MM_NA_SE_EN_MB_MOTH_DEAD, &self->actor.projectedPos);
            Actor_Kill(&self->actor);
        }
        return;
    }

    // Pikmin ball: this bug's spot is BEHIND Link (facing reversed), fanned + ring-depthed by a stable
    // per-bug hash of its pointer, and boids-separated so the ball has volume. Like Gyorg's fish.
    Player* player = GET_PLAYER(play);
    Vec3f anchor = self->actor.world.pos;
    // MM player->speedXZ -> OoT player->linearVelocity (z64player.h).
    s32 linkMoving = (player != NULL) && (player->linearVelocity > BUG_LINK_MOVE);
    if (player != NULL) {
        s16 behindYaw = player->actor.shape.rot.y + 0x8000;
        s16 fan = (s16)((((s32)((uintptr_t)self >> 5) & 3) - 1) * 0x1800);
        f32 ringDist = BUG_BALL_DIST + (f32)(((uintptr_t)self >> 7) & 1) * BUG_BALL_SPACING;
        anchor = player->actor.world.pos;
        anchor.x += Math_SinS(behindYaw + fan) * ringDist;
        anchor.z += Math_CosS(behindYaw + fan) * ringDist;
        RemainsAllyBug_Separate(self, play);
    }

    // Peel off to swarm ONLY while Link holds still, converging on the enemy nearest to LINK (so they
    // gang up "en banco"); while he moves they re-form the ball and keep up.
    target = (linkMoving || (player == NULL))
                 ? NULL
                 : RemainsAlly_FindNearestEnemy(play, &player->actor.world.pos, BUG_SEARCH_RANGE);

    if (target != NULL) {
        // Hop cadence: inject upward velocity only when grounded so it reads as hopping. Done BEFORE
        // the move so Actor_MoveXZGravity (inside HomeTowardPos) applies this frame's hop.
        if ((self->hopTimer <= 0) && (self->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
            self->actor.velocity.y = BUG_HOP_STRENGTH;
            self->hopTimer = BUG_HOP_INTERVAL;
            // MM used the flagged (looped) fly sfx; the bridge has no flagged model, and the hop
            // cadence (every 15f) already gives a natural re-fire rhythm, so a one-shot per hop.
            MmSfx_PlayAtPos(MM_NA_SE_EN_MB_MOTH_FLY, &self->actor.projectedPos);
        }

        // Steer yaw toward the enemy and advance (Actor_MoveXZGravity keeps the hop's vy).
        RemainsAlly_HomeTowardPos(play, &self->actor, &target->world.pos, BUG_CHASE_SPEED);

        // Attack is live: position + register the AT collider AT THE NEW spot every frame so any
        // contact lands the AT_TYPE_PLAYER hit on the enemy (never on Link). Order matters — update
        // AFTER the move, otherwise the toucher lags a frame behind the body.
        Collider_UpdateCylinder(&self->actor, &self->collider);
        CollisionCheck_SetAT(play, &play->colChkCtx, &self->collider.base);

        // Kamikaze: die once the hit actually CONNECTED (AT_HIT is set by the previous frame's
        // collision pass → the enemy already took damage), or as a fallback once we are essentially
        // on top of it (covers enemies immune to the deku-stick damage type, so the bug doesn't
        // hover forever). Clear AT_HIT first so a re-used collider slot can't false-trigger.
        s32 hit = (self->collider.base.atFlags & AT_HIT) != 0;
        if (hit) {
            self->collider.base.atFlags &= ~AT_HIT;
        }
        if (hit || (Actor_WorldDistXZToActor(&self->actor, target) < BUG_HIT_DIST)) {
            MmSfx_PlayAtPos(MM_NA_SE_EN_MB_MOTH_DEAD, &self->actor.projectedPos);
            Actor_Kill(&self->actor);
            return;
        }
    } else if (player != NULL) {
        // Form up: hop toward this bug's spot in the ball behind Link. Sprint if far (keep up with him),
        // ease when near, hold when there — same speed ramp the fish school uses.
        if ((self->hopTimer <= 0) && (self->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
            self->actor.velocity.y = BUG_HOP_STRENGTH;
            self->hopTimer = BUG_HOP_INTERVAL;
        }

        f32 distToAnchor = Math_Vec3f_DistXZ(&self->actor.world.pos, &anchor);
        f32 sp = (distToAnchor > BUG_FOLLOW_DIST) ? BUG_CHASE_SPEED : (distToAnchor > 10.0f) ? BUG_IDLE_SPEED : 0.0f;
        RemainsAlly_HomeTowardPos(play, &self->actor, &anchor, sp);
    }
}

// ============================================================================
// DRAW — camera-facing billboard of Odolwa's moth sprite (mm.o2r DLs) / the
// beetle FLEX skeleton / Goht's real lightning bolt, per mode.
// ============================================================================

void RemainsAllyBug_Draw(Actor* thisx, PlayState* play) {
    RemainsAllyBug* self = (RemainsAllyBug*)thisx;

    if (!self->mmAssetsReady) {
        return; // mm.o2r not mounted yet — invisible this frame, retried in Update
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    // Defensive scene-cull segment bind (Odolwa's DLs can branch into 0x0C).
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sBugSegment0xC_Noop);

    if (self->spawnParams == BUG_MODE_THUNDER) {
        // Goht thunder — the ACTUAL Goht lightning bolt (gGohtLightningModelDL), rendered exactly like
        // BossHakugin_DrawLightningSegments: env = Goht's cyan (sLightningColor 0,255,255), prim = white,
        // and each segment drawn TWICE (second copy rotated 0x4000 on Z) for the cross-shaped bolt volume.
        // A short jagged chain trails the projectile head so it reads as a real forked bolt, not a sprite.
        const s32 kBoltSegments = 5;
        const f32 kSegSpacing = 70.0f;
        s16 yaw = self->actor.shape.rot.y;
        f32 fwdX = Math_SinS(yaw);
        f32 fwdZ = Math_CosS(yaw);
        u8 alpha = (play->gameplayFrames & 1) ? 255 : 160; // rapid lightning flicker

        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 255, 255, 0); // sLightningColor
        gSPDisplayList(POLY_XLU_DISP++, self->thunderMatDL);

        for (s32 s = 0; s < kBoltSegments; s++) {
            Vec3s rot;
            // Fixed per-segment zig-zag so the chain looks jagged (like the random offsets Goht bakes in).
            rot.x = (s16)(((s & 1) ? -0x0500 : 0x0500));
            rot.y = (s16)(yaw + ((s & 1) ? 0x0900 : -0x0900));
            rot.z = 0;
            Vec3f p;
            p.x = self->actor.world.pos.x - (fwdX * kSegSpacing * s);
            p.y = self->actor.world.pos.y;
            p.z = self->actor.world.pos.z - (fwdZ * kSegSpacing * s);

            Matrix_SetTranslateRotateYXZ(p.x, p.y, p.z, &rot);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, alpha);
            // MM MATRIX_FINALIZE_AND_LOAD -> SoH gSPMatrix + MATRIX_NEWMTX.
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, self->thunderModelDL);

            // MM Matrix_RotateZS(0x4000, APPLY) -> SoH Matrix_RotateZ takes radians (0x4000 = pi/2).
            Matrix_RotateZ((f32)M_PI / 2.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, self->thunderModelDL);
        }
    } else if ((self->spawnParams == BUG_MODE_PROJECTILE) || (self->spawnParams == BUG_MODE_CLOUD)) {
        // Sword-beam / carrying-cloud = the En_Tanron1 MOTH sprite as a camera-facing billboard, wing-flap.
        f32 flap = 0.75f + (0.30f * Math_SinS(self->flapTimer));
        gSPDisplayList(POLY_OPA_DISP++, self->mothSetupDL);
        Matrix_Translate(self->actor.world.pos.x, self->actor.world.pos.y + BUG_DRAW_Y_LIFT, self->actor.world.pos.z,
                         MTXMODE_NEW);
        Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
        Matrix_Scale(BUG_DRAW_SCALE * flap, BUG_DRAW_SCALE, BUG_DRAW_SCALE, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, self->mothModelDL);
    } else {
        // Ground = Odolwa's real BEETLE — its FLEX skeleton (gOdolwaBugSkel limbs) at the actor transform.
        SkelAnime_DrawFlexOpa(play, self->skelAnime.skeleton, self->skelAnime.jointTable, self->skelAnime.dListCount,
                              NULL, NULL, NULL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// ============================================================================
// SPAWN HELPERS (runtime ActorDB id — no ActorProfile table in SoH)
// ============================================================================

// One bug per call — Odolwa's remains action calls this a few times to make a swarm.
// rotY seeds the initial facing; params is 0 (reserved sub-mode, read in Init).
Actor* RemainsAllyBug_Spawn(PlayState* play, Vec3f* pos, s16 rotY) {
    if ((play == NULL) || (pos == NULL)) {
        return NULL;
    }
    BossRemains_EnsureActorsRegistered(); // lazy ActorDB registration on first use
    if (gRemainsAllyBugId < 0) {
        return NULL;
    }
    return Actor_Spawn(&play->actorCtx, play, gRemainsAllyBugId, pos->x, pos->y, pos->z, 0, rotY, 0, 0);
}

// Nimbus cloud moth: hovers under Link (rotY seeds its ring angle). Lives while the flight is active.
Actor* RemainsAllyBug_SpawnCloud(PlayState* play, Vec3f* pos, s16 rotY) {
    if ((play == NULL) || (pos == NULL)) {
        return NULL;
    }
    BossRemains_EnsureActorsRegistered();
    if (gRemainsAllyBugId < 0) {
        return NULL;
    }
    // Spread the ring: offset each moth's seed angle so 5 of them fan out around the circle.
    s16 spread = rotY + (s16)Rand_CenteredFloat(65535.0f);
    return Actor_Spawn(&play->actorCtx, play, gRemainsAllyBugId, pos->x, pos->y, pos->z, 0, spread, 0, BUG_MODE_CLOUD);
}

// FD-beam moth: spawn one in projectile mode (params = BUG_MODE_PROJECTILE) flying toward rotY.
Actor* RemainsAllyBug_SpawnProjectile(PlayState* play, Vec3f* pos, s16 rotY) {
    if ((play == NULL) || (pos == NULL)) {
        return NULL;
    }
    BossRemains_EnsureActorsRegistered();
    if (gRemainsAllyBugId < 0) {
        return NULL;
    }
    return Actor_Spawn(&play->actorCtx, play, gRemainsAllyBugId, pos->x, pos->y, pos->z, 0, rotY, 0,
                       BUG_MODE_PROJECTILE);
}

// Goht thunder bolt: flies toward rotY, pierces walls, hits harder (params = BUG_MODE_THUNDER).
Actor* RemainsAllyBug_SpawnThunder(PlayState* play, Vec3f* pos, s16 rotY) {
    if ((play == NULL) || (pos == NULL)) {
        return NULL;
    }
    BossRemains_EnsureActorsRegistered();
    if (gRemainsAllyBugId < 0) {
        return NULL;
    }
    return Actor_Spawn(&play->actorCtx, play, gRemainsAllyBugId, pos->x, pos->y, pos->z, 0, rotY, 0, BUG_MODE_THUNDER);
}

// Charged Goht thunder: same bolt, but the charge level sets its lifetime (→ how FAR it reaches) and its
// damage. Init already set the thunder defaults; we override them on the freshly-spawned actor.
Actor* RemainsAllyBug_SpawnThunderCharged(PlayState* play, Vec3f* pos, s16 rotY, s16 ttl, s16 damage) {
    if ((play == NULL) || (pos == NULL)) {
        return NULL;
    }
    BossRemains_EnsureActorsRegistered();
    if (gRemainsAllyBugId < 0) {
        return NULL;
    }
    Actor* a =
        Actor_Spawn(&play->actorCtx, play, gRemainsAllyBugId, pos->x, pos->y, pos->z, 0, rotY, 0, BUG_MODE_THUNDER);
    if (a != NULL) {
        RemainsAllyBug* self = (RemainsAllyBug*)a;
        self->lifeTimer = ttl; // TTL × speed = reach distance
        // MM collider.elem.atDmgInfo.damage -> OoT collider.info.toucher.damage.
        self->collider.info.toucher.damage = (u8)damage;
    }
    return a;
}
