// ── Flame Tracking + AT Collider System ─────────────────────────────────────
// Every summoned flame gets a collider driven by sFireConfig: all of them light
// torches (DMG_ARROW_FIRE), each adds its own element on top, and green does zero
// damage. Flames ignite small and grow into full size, and any torch that is burning
// next to one takes that fire's colour (see Lantern_GetTorchTint).

typedef struct {
    Actor* actor;
    s16 timer;
    ColliderCylinder col;
    u8 colInited;
    u8 fireType;
    f32 scale; // current size, stepped up to LANTERN_FLAME_SCALE_FULL
} FlameEntry;

static FlameEntry sFlames[LANTERN_MAX_FLAMES];

static const ColliderCylinderInit sFlameColInit = {
    // AT_TYPE_OTHER rides along with AT_TYPE_PLAYER so the flame also registers on
    // "other"-type bumpers (the ones ordinary player attacks never reach), the way a
    // loose flame in the world does.
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER | AT_TYPE_OTHER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_ARROW_FIRE, 0x01, 0x02 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 30, 80, 0, { 0, 0, 0 } }
};

// ── Torch tinting ───────────────────────────────────────────────────────────
// Torches lit by the lantern burn in the lantern's colour. The link is kept in a
// side table instead of a field on ObjSyokudai so no vanilla struct has to change;
// ObjSyokudai_Draw asks for the tint every frame (see Lantern_GetTorchTint).

typedef struct {
    Actor* actor;
    u8 fireType;
} TorchTintEntry;

static TorchTintEntry sTorchTints[LANTERN_MAX_TINTED_TORCHES];

static void Lantern_TintTorch(Actor* torch, u8 fireType) {
    s32 i;
    s32 free = -1;

    for (i = 0; i < LANTERN_MAX_TINTED_TORCHES; i++) {
        if (sTorchTints[i].actor == torch) {
            sTorchTints[i].fireType = fireType;
            return;
        }
        if ((free < 0) && (sTorchTints[i].actor == NULL)) {
            free = i;
        }
    }
    if (free < 0) {
        free = 0; // oldest slot loses its tint, never the newest torch
    }
    sTorchTints[free].actor = torch;
    sTorchTints[free].fireType = fireType;
}

// Tint every LIT torch around a point. Called from wherever the lantern can set one
// alight: the swing collider and each summoned flame.
static void Lantern_TintTorchesNear(PlayState* play, Vec3f* pos, f32 range, u8 fireType) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].head;

    if ((fireType == LANTERN_FIRE_NONE) || (fireType >= LANTERN_FIRE_MAX)) {
        return;
    }

    while (actor != NULL) {
        if ((actor->id == ACTOR_OBJ_SYOKUDAI) && (actor->update != NULL) && (((ObjSyokudai*)actor)->litTimer != 0)) {
            f32 dx = actor->world.pos.x - pos->x;
            f32 dy = actor->world.pos.y - pos->y;
            f32 dz = actor->world.pos.z - pos->z;

            if (((dx * dx) + (dz * dz) < SQ(range)) && (dy > -range) && (dy < range * 2.0f)) {
                Lantern_TintTorch(actor, fireType);
            }
        }
        actor = actor->next;
    }
}

// Called from ObjSyokudai_Draw. Returns 1 and fills prim/env when this torch was lit
// by the lantern; 0 leaves the vanilla orange alone.
u8 Lantern_GetTorchTint(Actor* torch, u8* prim, u8* env) {
    s32 i;

    for (i = 0; i < LANTERN_MAX_TINTED_TORCHES; i++) {
        if (sTorchTints[i].actor != torch) {
            continue;
        }
        // The pointer may have been recycled by a different actor after a room swap,
        // and a snuffed-out torch forgets its colour.
        if ((torch->id != ACTOR_OBJ_SYOKUDAI) || (((ObjSyokudai*)torch)->litTimer == 0)) {
            sTorchTints[i].actor = NULL;
            return 0;
        }
        prim[0] = sLanternFlamePrim[sTorchTints[i].fireType][0];
        prim[1] = sLanternFlamePrim[sTorchTints[i].fireType][1];
        prim[2] = sLanternFlamePrim[sTorchTints[i].fireType][2];
        env[0] = sLanternFlameEnv[sTorchTints[i].fireType][0];
        env[1] = sLanternFlameEnv[sTorchTints[i].fireType][1];
        env[2] = sLanternFlameEnv[sTorchTints[i].fireType][2];
        return 1;
    }
    return 0;
}

static void Lantern_TrackFlame(Actor* flame, u8 fireType, PlayState* play) {
    if (flame == NULL)
        return;
    if (fireType >= LANTERN_FIRE_MAX)
        return;

    s32 slot = -1;
    for (s32 i = 0; i < LANTERN_MAX_FLAMES; i++) {
        if (sFlames[i].timer <= 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = 0;
        if (sFlames[0].actor != NULL && sFlames[0].actor->update != NULL) {
            Actor_Kill(sFlames[0].actor);
        }
        sFlames[0].actor = NULL;
        sFlames[0].timer = 0;
        sFlames[0].colInited = 0;
    }

    sFlames[slot].actor = flame;
    sFlames[slot].timer = sFireConfig[fireType].lifetime;
    sFlames[slot].fireType = fireType;
    sFlames[slot].scale = LANTERN_FLAME_SCALE_SEED;

    if (!sFlames[slot].colInited) {
        Collider_InitCylinder(play, &sFlames[slot].col);
        sFlames[slot].colInited = 1;
    }
    Collider_SetCylinder(play, &sFlames[slot].col, flame, &sFlameColInit);
    sFlames[slot].col.info.toucher.dmgFlags = sFireConfig[fireType].dmgFlags;
    sFlames[slot].col.info.toucher.damage = sFireConfig[fireType].damage;

    // Ignite small — Lantern_UpdateFlames grows it to full size over the next frames.
    flame->scale.x *= LANTERN_FLAME_SCALE_SEED;
    flame->scale.y *= LANTERN_FLAME_SCALE_SEED;
    flame->scale.z *= LANTERN_FLAME_SCALE_SEED;
}

void Lantern_UpdateFlames(PlayState* play) {
    if (play == NULL)
        return;

    for (s32 i = 0; i < LANTERN_MAX_FLAMES; i++) {
        if (sFlames[i].timer <= 0)
            continue;

        if (sFlames[i].actor == NULL || sFlames[i].actor->update == NULL) {
            sFlames[i].timer = 0;
            sFlames[i].actor = NULL;
            continue;
        }

        sFlames[i].timer--;

        // Grow from the ignition spark to full size
        if (sFlames[i].scale < LANTERN_FLAME_SCALE_FULL) {
            f32 prev = sFlames[i].scale;

            sFlames[i].scale += LANTERN_FLAME_GROW_STEP;
            if (sFlames[i].scale > LANTERN_FLAME_SCALE_FULL) {
                sFlames[i].scale = LANTERN_FLAME_SCALE_FULL;
            }
            if (prev > 0.0f) {
                f32 factor = sFlames[i].scale / prev;

                sFlames[i].actor->scale.x *= factor;
                sFlames[i].actor->scale.y *= factor;
                sFlames[i].actor->scale.z *= factor;
            }
        }

        // Update collider position to follow flame + register AT
        if (sFlames[i].colInited) {
            sFlames[i].col.dim.pos.x = (s16)sFlames[i].actor->world.pos.x;
            sFlames[i].col.dim.pos.y = (s16)sFlames[i].actor->world.pos.y;
            sFlames[i].col.dim.pos.z = (s16)sFlames[i].actor->world.pos.z;
            CollisionCheck_SetAT(play, &play->colChkCtx, &sFlames[i].col.base);
        }

        // Any torch burning next to this flame took its fire — colour it to match
        Lantern_TintTorchesNear(play, &sFlames[i].actor->world.pos, LANTERN_TORCH_TINT_RANGE, sFlames[i].fireType);

        if (sFlames[i].timer <= 0) {
            if (sFlames[i].actor->update != NULL) {
                Actor_Kill(sFlames[i].actor);
            }
            sFlames[i].actor = NULL;
        }
    }
}
