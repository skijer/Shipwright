/**
 * byrna_orb.c — Implementation. See byrna_orb.h for the design.
 *
 * Hijack pattern (as trident_charge_ball.c): Actor_Spawn(ACTOR_EN_LIGHTBOX) gives a
 * trivial real actor with the right lifetime, and update/draw are overwritten before
 * the pointer is handed back. State lives here because Actor_Spawn only allocates
 * sizeof(EnLightbox).
 *
 * FRAME UNITS: 20 Hz logic ticks (R_UPDATE_RATE = 3, game.c:437). 100 frames = 5 s.
 * Text-included from extended_equipment.c; everything is static but the exports.
 * Skijer's NEI
 */

#define BORB_HIT_MAGIC 4 // magic the orb spends eating one hit for Link
#define BORB_GRACE 5     // post-impact super-damage window, see header

#define BORB_ORBIT_RADIUS 18.0f
#define BORB_ORBIT_SPEED 0x0C00 // binang/frame, ~1.1 s per turn at 20 Hz
#define BORB_ORBIT_BOB 4.0f

#define BORB_FLY_SPEED 24.0f
#define BORB_FLY_HOMING 0.30f
#define BORB_SEEK_RANGE 800.0f
#define BORB_FLY_TIMEOUT 40
#define BORB_RETURN_SPEED 28.0f
#define BORB_CATCH_DIST 30.0f

#define BORB_RADIUS 18
#define BORB_HEIGHT 24
// FhgFlash light-ball scale, in the same units the trident's ball uses (160-300).
#define BORB_SCALE 220

#define BORB_SEED_SPEED 34.0f
#define BORB_SEED_LIFE 30
#define BORB_SEED_RADIUS 14
#define BORB_SEED_HEIGHT 18
#define BORB_SEED_SCALE 120
#define BORB_TINT_FRAMES 200

// Copied from the Rito updraft (rito_flight.inc.c), the other consumer of this cone
// and the one that already has the pitch sign convention right.
#define BORB_WIND_PITCH_UP (-0x4000)
#define BORB_WIND_HEIGHT 82.0f
#define BORB_WIND_RADIUS 26.0f
#define BORB_WIND_ALPHA 70
#define BORB_WIND_RIBBONS 5

// ⚠️ NO DISPLAY LIST, and Morpha's are specifically why.
//
// gMorphaCoreMembraneDL/Nucleus/Bubble load their vertices through a RAW SEGMENTED
// address into object_mo, and that object is only in RAM inside Morpha's own boss
// room. Drawing them anywhere else sent gSPVertex at whatever segment 6 happened to
// hold -> 0xC0000005 in Fast::Interpreter::GfxSpVertex (measured 2026-08-29).
//
// The archive tells you which DLs are safe: a resolvable one carries the referenced
// resource's NAME inside it (gPhantomEnergyBallDL, the trident's, has "trwDl."),
// and the Morpha DLs carry none. Same conclusion trident_charge_ball.c reached —
// its ball is FhgFlash light balls precisely so nothing has to be object-resident.
#define BORB_FX_LIGHTBALL_BLUE 4 // FHGFLASH_LIGHTBALL_BLUE
extern void EffectSsFhgFlash_SpawnLightBall(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale,
                                            u8 param);

typedef enum {
    BORB_STATE_NEAR = 0, // at the cane tip: Link is shielded, hits cost magic
    BORB_STATE_OUTBOUND,
    BORB_STATE_RETURN,
} BOrbState;

static struct {
    Actor* actor;
    Actor* target;
    u8 state;
    u8 colliderInited;
    s16 orbitAngle;
    s16 flyTimer;
    Vec3f velocity;
    ColliderCylinder collider;
} sOrb = { 0 };

static struct {
    Actor* actor;
    u8 colliderInited;
    s16 life;
    Vec3f velocity;
    ColliderCylinder collider;
} sSeed = { 0 };

static s16 sBOrbGraceTimer = 0;
static u8 sBOrbCharge = 0;
// Which enemies already paid: a second hit on the same one banks nothing.
static s16 sBOrbCredited[BYRNA_CHARGE_MAX] = { 0 };
// Actor the seed marked. The launched orb prefers it over the Z-target.
static Actor* sBOrbMarked = NULL;

static TornadoParams sBOrbWind;
static TornadoRibbons sBOrbWindRibbons;
static u8 sBOrbWindOn = 0;

// DMG_SLASH_MASTER twice over: a real weapon bit so every boss bumper accepts the
// hit (they key their super-damage path off BUMP_HIT, and a rejected projectile
// never registers at all), and each enemy's DamageTable then resolves it as a
// Master Sword strike — the damage this orb is specified to deal.
static ColliderCylinderInit sBOrbColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    {
        ELEMTYPE_UNK2,
        { DMG_SLASH_MASTER, 0x00, 0x01 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { BORB_RADIUS, BORB_HEIGHT, 0, { 0, 0, 0 } },
};

// The seed only marks. No AT at all, so it can never deal damage by accident.
static ColliderCylinderInit sBOrbSeedColliderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    {
        ELEMTYPE_UNK2,
        { 0x00000000, 0x00, 0x00 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_NONE,
        OCELEM_ON,
    },
    { BORB_SEED_RADIUS, BORB_SEED_HEIGHT, 0, { 0, 0, 0 } },
};

static u8 BOrb_TargetIsUsable(Actor* target) {
    return (target != NULL) && (target->update != NULL) && (target->colChkInfo.health > 0);
}

// The seed's mark outranks the lock-on: spending Z+R on something is a deliberate
// order, and it would be useless if the Z-target could override it.
static Actor* BOrb_AcquireTarget(PlayState* play, Actor* from) {
    Player* player = GET_PLAYER(play);
    Actor* found;

    if (BOrb_TargetIsUsable(sBOrbMarked)) {
        return sBOrbMarked;
    }
    sBOrbMarked = NULL;
    if (player != NULL && BOrb_TargetIsUsable(player->focusActor)) {
        return player->focusActor;
    }
    found = Actor_FindNearby(play, from, -1, ACTORCAT_BOSS, BORB_SEEK_RANGE);
    if (BOrb_TargetIsUsable(found)) {
        return found;
    }
    found = Actor_FindNearby(play, from, -1, ACTORCAT_ENEMY, BORB_SEEK_RANGE);
    return BOrb_TargetIsUsable(found) ? found : NULL;
}

// Home is the cane's tip, which the trail block in z_player_lib.c refreshes every
// frame — but ONLY while the weapon is drawn. Sheathed, that field holds whatever
// the last swing left there, so fall back to a point in front of Link instead of
// spawning the orb at a stale world position.
static void BOrb_HomePos(Player* player, Vec3f* out) {
    if (Player_GetMeleeWeaponHeld(player) != 0) {
        *out = player->meleeWeaponInfo[0].tip;
        return;
    }
    *out = player->actor.world.pos;
    out->y += 40.0f;
}

static void BOrb_CreditCharge(Actor* target) {
    s32 i;

    if (target == NULL || sBOrbCharge >= BYRNA_CHARGE_MAX) {
        return;
    }
    if (target->category == ACTORCAT_BOSS) {
        sBOrbCharge = BYRNA_CHARGE_MAX;
        return;
    }
    for (i = 0; i < sBOrbCharge; i++) {
        if (sBOrbCredited[i] == target->id) {
            return;
        }
    }
    sBOrbCredited[sBOrbCharge] = target->id;
    sBOrbCharge++;
}

static void BOrb_Sparkle(PlayState* play, Vec3f* pos, s16 scale, s32 life) {
    static Color_RGBA8 prim = { 235, 245, 255, 255 };
    static Color_RGBA8 env = { 120, 170, 220, 0 };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    EffectSsKiraKira_SpawnDispersed(play, pos, &zero, &zero, &prim, &env, scale, life);
}

static void BOrb_Home(Actor* self, Vec3f* goal, f32 speed, f32 lerp) {
    Vec3f to;
    f32 len;

    to.x = goal->x - self->world.pos.x;
    to.y = goal->y - self->world.pos.y;
    to.z = goal->z - self->world.pos.z;
    len = sqrtf(to.x * to.x + to.y * to.y + to.z * to.z);
    if (len < 1.0f) {
        return;
    }
    to.x = to.x / len * speed;
    to.y = to.y / len * speed;
    to.z = to.z / len * speed;

    sOrb.velocity.x += (to.x - sOrb.velocity.x) * lerp;
    sOrb.velocity.y += (to.y - sOrb.velocity.y) * lerp;
    sOrb.velocity.z += (to.z - sOrb.velocity.z) * lerp;

    self->world.pos.x += sOrb.velocity.x;
    self->world.pos.y += sOrb.velocity.y;
    self->world.pos.z += sOrb.velocity.z;
}

// The blob IS the effect burst: a dense light-ball core plus a sparkle skin, both
// emitted from the update. Nothing here needs an object resident in RAM.
static void BOrb_DrawBlob(PlayState* play, Vec3f* pos, s16 scale) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    EffectSsFhgFlash_SpawnLightBall(play, pos, &zero, &zero, scale, BORB_FX_LIGHTBALL_BLUE);
}

static void BOrb_Update(Actor* thisx, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (sOrb.actor != thisx || player == NULL) {
        Actor_Kill(thisx);
        return;
    }

    // Lazy: Collider_SetCylinder needs the actor live in the list, which it is not
    // yet inside Actor_Spawn.
    if (!sOrb.colliderInited) {
        Collider_InitCylinder(play, &sOrb.collider);
        Collider_SetCylinder(play, &sOrb.collider, thisx, &sBOrbColliderInit);
        sOrb.colliderInited = 1;
    }

    switch (sOrb.state) {
        case BORB_STATE_NEAR: {
            Vec3f home;

            BOrb_HomePos(player, &home);
            sOrb.orbitAngle += BORB_ORBIT_SPEED;
            thisx->world.pos.x = home.x + Math_SinS(sOrb.orbitAngle) * BORB_ORBIT_RADIUS;
            thisx->world.pos.z = home.z + Math_CosS(sOrb.orbitAngle) * BORB_ORBIT_RADIUS;
            thisx->world.pos.y = home.y + Math_SinS((s16)(sOrb.orbitAngle * 2)) * BORB_ORBIT_BOB;
            break;
        }

        case BORB_STATE_OUTBOUND: {
            Vec3f goal;

            if (sOrb.flyTimer == 0) {
                sOrb.state = BORB_STATE_RETURN;
                break;
            }
            sOrb.flyTimer--;

            if (!BOrb_TargetIsUsable(sOrb.target)) {
                // Nothing to chase: coast on the launch velocity rather than stall.
                thisx->world.pos.x += sOrb.velocity.x;
                thisx->world.pos.y += sOrb.velocity.y;
                thisx->world.pos.z += sOrb.velocity.z;
                break;
            }
            goal.x = sOrb.target->world.pos.x;
            goal.y = (sOrb.target->world.pos.y + sOrb.target->focus.pos.y) * 0.5f;
            goal.z = sOrb.target->world.pos.z;
            BOrb_Home(thisx, &goal, BORB_FLY_SPEED, BORB_FLY_HOMING);
            break;
        }

        case BORB_STATE_RETURN: {
            Vec3f home;
            f32 dx;
            f32 dy;
            f32 dz;

            BOrb_HomePos(player, &home);
            BOrb_Home(thisx, &home, BORB_RETURN_SPEED, 0.5f);
            dx = home.x - thisx->world.pos.x;
            dy = home.y - thisx->world.pos.y;
            dz = home.z - thisx->world.pos.z;
            if (sqrtf(dx * dx + dy * dy + dz * dz) < BORB_CATCH_DIST) {
                sOrb.state = BORB_STATE_NEAR;
                sOrb.velocity.x = sOrb.velocity.y = sOrb.velocity.z = 0.0f;
                Sfx_PlaySfxCentered(NA_SE_PL_CATCH_BOOMERANG);
            }
            break;
        }
    }

    BOrb_Sparkle(play, &thisx->world.pos, (sOrb.state == BORB_STATE_NEAR) ? 300 : 460, 6);
    Collider_UpdateCylinder(thisx, &sOrb.collider);

    // Only a launched orb attacks. An always-on AT at the tip would shred anything
    // Link walked past.
    if (sOrb.state != BORB_STATE_OUTBOUND) {
        return;
    }
    CollisionCheck_SetAT(play, &play->colChkCtx, &sOrb.collider.base);

    if (sOrb.collider.base.atFlags & AT_HIT) {
        sOrb.collider.base.atFlags &= ~AT_HIT;
        // Grace opens before anything else clears state: the boss reads BUMP_HIT
        // next frame and must still see this orb as active.
        sBOrbGraceTimer = BORB_GRACE;
        BOrb_CreditCharge(sOrb.target);
        if (sOrb.target == sBOrbMarked) {
            sBOrbMarked = NULL;
        }
        BOrb_Sparkle(play, &thisx->world.pos, 700, 12);
        sOrb.state = BORB_STATE_RETURN;
    }
}

static void BOrb_Draw(Actor* thisx, PlayState* play) {
    BOrb_DrawBlob(play, &thisx->world.pos, BORB_SCALE);
}

static void BOrbSeed_Kill(Actor* thisx) {
    sSeed.actor = NULL;
    sSeed.colliderInited = 0;
    Actor_Kill(thisx);
}

static void BOrbSeed_Update(Actor* thisx, PlayState* play) {
    if (sSeed.actor != thisx) {
        Actor_Kill(thisx);
        return;
    }
    if (!sSeed.colliderInited) {
        Collider_InitCylinder(play, &sSeed.collider);
        Collider_SetCylinder(play, &sSeed.collider, thisx, &sBOrbSeedColliderInit);
        sSeed.colliderInited = 1;
    }

    thisx->world.pos.x += sSeed.velocity.x;
    thisx->world.pos.y += sSeed.velocity.y;
    thisx->world.pos.z += sSeed.velocity.z;

    Collider_UpdateCylinder(thisx, &sSeed.collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &sSeed.collider.base);

    if (sSeed.collider.base.ocFlags1 & OC1_HIT) {
        Actor* hit = sSeed.collider.base.oc;

        sSeed.collider.base.ocFlags1 &= ~OC1_HIT;
        if (hit != NULL && hit->category != ACTORCAT_PLAYER) {
            // The same blue wash the Cane of Pacci puts on an Ultrahand target.
            Actor_SetColorFilter(hit, 0, 255, 0, BORB_TINT_FRAMES);
            sBOrbMarked = hit;
            BOrb_Sparkle(play, &thisx->world.pos, 500, 10);
            Sfx_PlaySfxCentered(NA_SE_EV_DIAMOND_SWITCH);
        }
        BOrbSeed_Kill(thisx);
        return;
    }

    if (sSeed.life == 0) {
        BOrbSeed_Kill(thisx);
        return;
    }
    sSeed.life--;
}

static void BOrbSeed_Draw(Actor* thisx, PlayState* play) {
    BOrb_DrawBlob(play, &thisx->world.pos, BORB_SEED_SCALE);
}

static Actor* BOrb_SpawnCarrier(PlayState* play, Vec3f* pos) {
    return Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHTBOX, pos->x, pos->y, pos->z, 0, 0, 0, 0);
}

static void BOrb_AimForward(Player* player, f32 speed, Vec3f* out) {
    f32 yaw = (f32)player->actor.shape.rot.y * (M_PI / 32768.0f);

    out->x = sinf(yaw) * speed;
    out->y = 0.0f;
    out->z = cosf(yaw) * speed;
}

u8 ByrnaOrb_PressR(PlayState* play) {
    Player* player;
    Vec3f pos;

    if (play == NULL) {
        return 0;
    }
    player = GET_PLAYER(play);
    if (player == NULL) {
        return 0;
    }

    // Out and holding a charge: R calls it home instead of throwing it again.
    if (sOrb.actor != NULL && sOrb.state == BORB_STATE_OUTBOUND && sBOrbCharge > 0) {
        sOrb.state = BORB_STATE_RETURN;
        return 1;
    }
    if (sOrb.actor != NULL && sOrb.state == BORB_STATE_OUTBOUND) {
        return 0;
    }

    if (sOrb.actor == NULL) {
        BOrb_HomePos(player, &pos);
        sOrb.actor = BOrb_SpawnCarrier(play, &pos);
        if (sOrb.actor == NULL) {
            return 0;
        }
        sOrb.colliderInited = 0;
        sOrb.orbitAngle = 0;
        sOrb.actor->update = BOrb_Update;
        sOrb.actor->draw = BOrb_Draw;
    }

    sOrb.target = BOrb_AcquireTarget(play, sOrb.actor);
    BOrb_AimForward(player, BORB_FLY_SPEED, &sOrb.velocity);
    sOrb.state = BORB_STATE_OUTBOUND;
    sOrb.flyTimer = BORB_FLY_TIMEOUT;
    Sfx_PlaySfxCentered(NA_SE_IT_BOOMERANG_THROW);
    return 1;
}

u8 ByrnaOrb_ThrowSeed(PlayState* play) {
    Player* player;
    Vec3f pos;

    if (play == NULL || sSeed.actor != NULL) {
        return 0;
    }
    player = GET_PLAYER(play);
    if (player == NULL) {
        return 0;
    }

    pos = player->actor.world.pos;
    pos.y += 40.0f;
    sSeed.actor = BOrb_SpawnCarrier(play, &pos);
    if (sSeed.actor == NULL) {
        return 0;
    }
    sSeed.colliderInited = 0;
    sSeed.life = BORB_SEED_LIFE;
    BOrb_AimForward(player, BORB_SEED_SPEED, &sSeed.velocity);
    sSeed.actor->update = BOrbSeed_Update;
    sSeed.actor->draw = BOrbSeed_Draw;
    Sfx_PlaySfxCentered(NA_SE_IT_SLING_SHOT);
    return 1;
}

u8 ByrnaOrb_TryAbsorb(PlayState* play) {
    Player* player;
    Vec3f pos;

    if (play == NULL || sOrb.actor == NULL || sOrb.state != BORB_STATE_NEAR) {
        return 0;
    }
    // Paid for in magic, not in the orb: it stays. Out of magic there is simply no
    // shield, which is the pressure that makes launching it the better play.
    if (!Magic_RequestChange(play, MAGIC_REQ(BORB_HIT_MAGIC), MAGIC_CONSUME_NOW)) {
        return 0;
    }

    player = GET_PLAYER(play);
    pos = (player != NULL) ? player->actor.world.pos : sOrb.actor->world.pos;
    pos.y += 30.0f;
    CollisionCheck_SpawnShieldParticlesMetal(play, &pos);
    BOrb_Sparkle(play, &pos, 600, 10);
    Sfx_PlaySfxCentered(NA_SE_IT_SHIELD_REFLECT_SW);
    return 1;
}

u8 ByrnaOrb_IsActive(void) {
    if (sBOrbGraceTimer > 0) {
        return 1;
    }
    // Only the launched orb claims super damage. One sitting at the tip must not,
    // or standing beside a boss would paralyse it every frame.
    return (sOrb.actor != NULL) && (sOrb.state == BORB_STATE_OUTBOUND);
}

void ByrnaOrb_Tick(void) {
    if (sBOrbGraceTimer > 0) {
        sBOrbGraceTimer--;
    }
    if (!BOrb_TargetIsUsable(sBOrbMarked)) {
        sBOrbMarked = NULL;
    }
}

void ByrnaOrb_Cleanup(void) {
    if (sOrb.actor != NULL) {
        Actor_Kill(sOrb.actor);
    }
    if (sSeed.actor != NULL) {
        Actor_Kill(sSeed.actor);
    }
    ByrnaOrb_Forget();
    sBOrbCharge = 0;
}

// Deliberately NOT Actor_Kill: on a scene load the actor heap is already wiped, so
// killing would follow a dangling pointer — and leaving the pointer non-NULL would
// block every future summon for the rest of the session.
void ByrnaOrb_Forget(void) {
    sOrb.actor = NULL;
    sOrb.target = NULL;
    sOrb.colliderInited = 0;
    sOrb.state = BORB_STATE_NEAR;
    sSeed.actor = NULL;
    sSeed.colliderInited = 0;
    sBOrbMarked = NULL;
    sBOrbGraceTimer = 0;
    sBOrbWindOn = 0;
}

u8 ByrnaOrb_GetCharge(void) {
    return sBOrbCharge;
}

u8 ByrnaOrb_IsCharged(void) {
    return sBOrbCharge >= BYRNA_CHARGE_MAX;
}

u8 ByrnaOrb_IsGuarding(void) {
    return (sOrb.actor != NULL) && (sOrb.state == BORB_STATE_NEAR);
}

void ByrnaOrb_OnLand(void) {
    sBOrbCharge = 0;
    sBOrbWindOn = 0;
}

// Only the spiral RIBBONS, which are sword-trail blures — no Tornado_Draw, so the
// cone geometry never appears. The params still drive where the ribbons spiral.
void ByrnaOrb_Draw(PlayState* play) {
    Player* player;

    if (play == NULL || !ByrnaOrb_IsCharged()) {
        if (sBOrbWindOn && play != NULL) {
            Tornado_RibbonsStop(play, &sBOrbWindRibbons);
        }
        sBOrbWindOn = 0;
        return;
    }
    player = GET_PLAYER(play);
    if (player == NULL) {
        return;
    }

    if (!sBOrbWindOn) {
        sBOrbWindOn = 1;
        sBOrbWind.spin = 0;
        sBOrbWind.scrollS = 0;
        sBOrbWind.scrollT = 0;
    }

    sBOrbWind.origin = player->actor.world.pos;
    sBOrbWind.yaw = player->actor.shape.rot.y;
    sBOrbWind.pitch = BORB_WIND_PITCH_UP;
    sBOrbWind.length = BORB_WIND_HEIGHT;
    sBOrbWind.radius = BORB_WIND_RADIUS;
    sBOrbWind.color.r = 210;
    sBOrbWind.color.g = 235;
    sBOrbWind.color.b = 255;
    sBOrbWind.color.a = BORB_WIND_ALPHA;
    sBOrbWind.spin += 0x0800;
    Tornado_AdvanceScroll(&sBOrbWind, 0, 12);

    Tornado_RibbonsUpdate(play, &sBOrbWindRibbons, &sBOrbWind, BORB_WIND_RIBBONS);
}
