/**
 * equip_dragonscale.c - ZORA TUNIC swim (formerly the Water Dragon Scale item)
 *
 * The Water Dragon Scale no longer exists as an equipment item (Skijer, 2026-07-15): 1:1 MM Zora
 * swimming is now a PERMANENT vanilla effect of wearing the ZORA TUNIC. Activates the REAL Zora swim
 * mechanics via the public wrappers in mm_player_form.cpp — the same swim actions (idle, surface
 * walk, fast swim/barrel roll, dolphin jump) + the electric water barrier, buoyancy and speed ramp.
 * Link keeps his OOT model (formSkelAnime joints are synced to player->skelAnime).
 *
 * SCOPE — swim + water barrier ONLY:
 *   NO iron-style manual sink toggle (already excluded: MmForm_CheckBootToggle bails on
 *   zoraSwimEnabled; use Iron Boots to sink).
 *   NO Zora punch/boomerang (the ocean-floor block in MmForm_Action_SwimIdle is gated to the FULL
 *   Zora form; the tunic swim never reaches it).
 *   NO land anims / model swap (never sets currentForm; MmForm_ApplyFormProperties never runs).
 *
 * The Zora tunic is adult-only in vanilla OoT, which keeps the old "Adult Link only" property.
 * Included by ext_equip_behavior.c (unity build); ZoraTunicSwim_Update is called UNCONDITIONALLY
 * from the behavior update (not slot-gated — the gate is the worn tunic itself).
 */

// ---------------------------------------------------------------------------
// Blue sparkles when entering water with Dragon Scale
// ---------------------------------------------------------------------------
static s16 sDScaleSparkleTimer = 0;

static void DScale_TriggerSparkles(void) {
    sDScaleSparkleTimer = 30;
}

static void DScale_Draw(Player* p, PlayState* play) {
    if (sDScaleSparkleTimer <= 0)
        return;

    sDScaleSparkleTimer--;

    // Spawn blue sparkles around Link
    Color_RGBA8 primColor = { 100, 180, 255, 255 };
    Color_RGBA8 envColor = { 30, 80, 200, 255 };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };

    for (u8 i = 0; i < 2; i++) {
        Vec3f pos;
        pos.x = p->actor.world.pos.x + Rand_CenteredFloat(30.0f);
        pos.y = p->actor.world.pos.y + 20.0f + Rand_CenteredFloat(20.0f);
        pos.z = p->actor.world.pos.z + Rand_CenteredFloat(30.0f);

        Vec3f vel;
        vel.x = Rand_CenteredFloat(1.0f);
        vel.y = Rand_ZeroFloat(1.5f);
        vel.z = Rand_CenteredFloat(1.0f);

        EffectSsKiraKira_SpawnFocused(play, &pos, &vel, &accel, &primColor, &envColor, 400, 15);
    }
}

// ---------------------------------------------------------------------------
// Main Behavior Entry — runs EVERY frame (not slot-gated); the gate is the worn ZORA TUNIC.
// ---------------------------------------------------------------------------
static void DragonScale_Behavior(Player* player, PlayState* play) {
    // ZORA TUNIC is the activation condition now (the Water Dragon Scale item is gone). When the
    // tunic comes off mid-swim, exit cleanly back to OoT swimming.
    // Skijer's NEI boss_remains: GYORG'S REMAINS grants the same free Zora swim (MM's Nei_IsZoraSwim ||
    // BossRemains_IsGyorgWorn). This is the right seam — IsZoraSwimEnabled() is the "already swimming"
    // state flag, so the capability has to widen the gate here, not that accessor.
    extern s32 BossRemains_IsGyorgWorn(void);
    if ((CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC) != EQUIP_VALUE_TUNIC_ZORA) && !BossRemains_IsGyorgWorn()) {
        if (TransformMasks_IsZoraSwimEnabled())
            TransformMasks_DragonScaleExitSwim(player);
        return;
    }

    // If a real transformation mask is active, don't interfere
    if (TransformMasks_IsTransformed()) {
        if (TransformMasks_IsZoraSwimEnabled())
            TransformMasks_DragonScaleExitSwim(player);
        return;
    }

    // IRON BOOTS lock the Zora swim out entirely (Skijer 2026-07-28). Iron Boots mean
    // "sink and walk the floor" — no dash, no barrel roll, no dolphin jump. Bailing here
    // (instead of only refusing the A-press) also fixes the boots being silently
    // unequipped: MmForm_Action_SwimIdle's enter_fast_swim path force-writes
    // currentBoots = PLAYER_BOOTS_KOKIRI, which is MM-canon for the full Zora form but
    // stripped Link's real Iron Boots when the tunic swim reached it. Putting the boots
    // on mid-swim exits cleanly back to OOT's swimming on the same frame.
    if (player->currentBoots == PLAYER_BOOTS_IRON) {
        if (TransformMasks_IsZoraSwimEnabled())
            TransformMasks_DragonScaleExitSwim(player);
        return;
    }

    // Skip during cutscenes, death, etc.
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        if (TransformMasks_IsZoraSwimEnabled())
            TransformMasks_DragonScaleExitSwim(player);
        return;
    }

    // Don't override during climbing/ledge grab
    if (player->stateFlags1 &
        (PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_CLIMBING_LADDER)) {
        if (TransformMasks_IsZoraSwimEnabled())
            TransformMasks_DragonScaleExitSwim(player);
        return;
    }

    u8 inWater = (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0;

    if (inWater && player->actor.yDistToWater > 30.0f) {
        if (!TransformMasks_IsZoraSwimEnabled()) {
            // First frame in water: enter Zora swim (loads anims from mm.o2r)
            if (!TransformMasks_DragonScaleEnterSwim(play, player)) {
                return; // mm.o2r not available
            }
            DScale_TriggerSparkles();
        }
        // Run real Zora swim logic (same actions as Zora form)
        TransformMasks_DragonScaleSwimUpdate(play, player);
    } else {
        if (TransformMasks_IsZoraSwimEnabled()) {
            TransformMasks_DragonScaleExitSwim(player);
        }
    }
}
