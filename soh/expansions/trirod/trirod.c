/**
 * trirod.c — Tri Rod behaviour v2. Skijer's NEI.
 * Compiled by #include inside item_cane_of_somaria.c's TU (after somaria_cubes.c,
 * cane_pacci.c and — via custom_items.c's ordering — after box_menu.c).
 *
 * The loop:
 *  - AIM at a scan source: prop rows light white, C learns them. Creature rows
 *    do NOT scan — they are learned by killing the source while the rod is
 *    drawn (Trirod_NotifyEnemyDown, bridged from Enemy_StartFinishingBlow).
 *  - HOLD L with the rod drawn: the Sheikah Slate's boxed-icon grid opens with
 *    every echo of the active list, learned ones selectable, the rest grayed.
 *    R taps still step the selection without the menu.
 *  - C casts the selected echo at the PLACEMENT GHOST — the pushable block's
 *    aiming verbatim: a camera-aimed spot in front of Link, floor-snapped,
 *    validity-checked, drawn blue/red. Where the echo has a single display list
 *    the ghost IS that DL, untextured; skeletal echoes show their miniature.
 *  - Aim at one of YOUR summons: red, C dismisses it (refunds triangles).
 *  - Budget: TRIROD_BUDGET triangles alive; overflow evicts the OLDEST (EoW).
 */

#include <stdio.h> // snprintf for the notification lines

// Defined further down this TU (item_cane_of_somaria.c) — forward declarations
// so the ghost can reuse the block's exact camera aim.
static s16 Cane_CameraYaw(PlayState* play, Player* player);

// ── Session state ────────────────────────────────────────────────────────────

typedef struct {
    Actor* actor;
    u8 echoIdx;
    u16 seq; // spawn order — lowest = oldest = first evicted
} TrirodSummon;

static TrirodSummon sTrirodPool[TRIROD_MAX_SUMMONS];
static u16 sTrirodSeq = 0;

// The press COMMITS a fully resolved body (water/land already chosen); the
// animation fires it frames later, immune to mid-swing selection changes.
static u8 sTrirodPendingEcho = 0xFF;
static s16 sTrirodPendingActor = -1;
static s16 sTrirodPendingParams = 0;
static Vec3f sTrirodPendingPos;
static s16 sTrirodPendingYaw;

// What the aim scan found this frame.
static Actor* sTrirodAimActor = NULL;
static s16 sTrirodAimSource = -1; // gTrirodScanSources row, -1 none
static u8 sTrirodAimIsOurs = 0;

// Placement ghost (the block's aiming, trirod-owned copies).
static Vec3f sTrirodGhostPos;
static s16 sTrirodGhostYaw = 0;
static u8 sTrirodGhostValid = 0;

// Hold-L wheel.
#define TRIROD_WHEEL_HOLD_FRAMES 8
static s16 sTrirodLHold = 0;
static u8 sTrirodWheelRows[TRIROD_ECHO_CAP]; // wheel position -> echo row
static s32 sTrirodWheelCount = 0;

// Scene fence: on a scene load every pooled pointer was freed with the arena.
static PlayState* sTrirodLastPlay = NULL;

// ── Pool ─────────────────────────────────────────────────────────────────────

void Trirod_CleanupPool(void) {
    for (u8 i = 0; i < TRIROD_MAX_SUMMONS; i++) {
        if ((sTrirodPool[i].actor != NULL) && (sTrirodPool[i].actor->update == NULL)) {
            sTrirodPool[i].actor = NULL;
        }
    }
}

static void Trirod_ForgetPool(void) {
    for (u8 i = 0; i < TRIROD_MAX_SUMMONS; i++) {
        sTrirodPool[i].actor = NULL;
    }
    sTrirodGhostValid = 0;
    sTrirodAimActor = NULL;
    sTrirodAimSource = -1;
    sTrirodAimIsOurs = 0;
    sTrirodPendingEcho = 0xFF;
    sTrirodLHold = 0;
}

static TrirodSummon* Trirod_FindSummon(Actor* actor) {
    if (actor == NULL) {
        return NULL;
    }
    for (u8 i = 0; i < TRIROD_MAX_SUMMONS; i++) {
        if (sTrirodPool[i].actor == actor) {
            return &sTrirodPool[i];
        }
    }
    return NULL;
}

static u8 Trirod_UsedCost(void) {
    u8 total = 0;

    for (u8 i = 0; i < TRIROD_MAX_SUMMONS; i++) {
        if (sTrirodPool[i].actor != NULL) {
            total += gTrirodEchoes[sTrirodPool[i].echoIdx].cost;
        }
    }
    return total;
}

static void Trirod_Dismiss(PlayState* play, TrirodSummon* entry) {
    if ((entry == NULL) || (entry->actor == NULL)) {
        return;
    }
    if (entry->actor->update != NULL) {
        Vec3f zero = { 0.0f, 0.0f, 0.0f };
        Vec3f pos = entry->actor->world.pos;

        pos.y += 20.0f;
        EffectSsBlast_SpawnWhiteShockwave(play, &pos, &zero, &zero);
        Actor_Kill(entry->actor);
    }
    entry->actor = NULL;
}

// EoW's over-budget rule: the OLDEST summon pays for the new one.
static void Trirod_EvictUntilFits(PlayState* play, u8 incomingCost) {
    while ((u8)(Trirod_UsedCost() + incomingCost) > TRIROD_BUDGET) {
        TrirodSummon* oldest = NULL;

        for (u8 i = 0; i < TRIROD_MAX_SUMMONS; i++) {
            if (sTrirodPool[i].actor == NULL) {
                continue;
            }
            if ((oldest == NULL) || (sTrirodPool[i].seq < oldest->seq)) {
                oldest = &sTrirodPool[i];
            }
        }
        if (oldest == NULL) {
            return;
        }
        Trirod_Dismiss(play, oldest);
    }
}

// ── Lists, folding, matching ─────────────────────────────────────────────────

static u8 Trirod_RowInActiveList(u8 row) {
    if (Nei_TrirodFullList()) {
        return 1;
    }
    return gTrirodEchoes[row].tier == TRIROD_TIER_CORE;
}

// The row a LEARN actually lights: extras fold onto their core while compressed.
static u8 Trirod_EffectiveRow(u8 row) {
    if (!Nei_TrirodFullList() && (gTrirodEchoes[row].tier == TRIROD_TIER_EXTRA)) {
        return gTrirodEchoes[row].foldsInto;
    }
    return row;
}

// First source matching this actor (id, masked params range, extra gate).
static s16 Trirod_MatchSource(Actor* actor, PlayState* play) {
    if (actor == NULL) {
        return -1;
    }
    for (u8 i = 0; i < gTrirodScanSourceCount; i++) {
        const TrirodScanSource* s = &gTrirodScanSources[i];

        if (actor->id != s->actorId) {
            continue;
        }
        if (s->matchMask != 0) {
            u16 masked = ((u16)actor->params) & s->matchMask;

            if ((masked < s->matchLo) || (masked > s->matchHi)) {
                continue;
            }
        }
        if ((s->extra != NULL) && !s->extra(actor, play)) {
            continue;
        }
        return (s16)i;
    }
    return -1;
}

static s32 Trirod_ScanFilter(Actor* actor) {
    if ((actor == NULL) || (actor->update == NULL) || (actor->id == ACTOR_PLAYER)) {
        return 0;
    }
    if (Trirod_FindSummon(actor) != NULL) {
        return 1; // our own — selectable for dismissal
    }
    // The extra() gates need `play`; the filter callback has no play parameter,
    // so it passes id+params candidates and the gate is applied on the result.
    for (u8 i = 0; i < gTrirodScanSourceCount; i++) {
        const TrirodScanSource* s = &gTrirodScanSources[i];

        if (actor->id != s->actorId) {
            continue;
        }
        if (s->matchMask != 0) {
            u16 masked = ((u16)actor->params) & s->matchMask;

            if ((masked < s->matchLo) || (masked > s->matchHi)) {
                continue;
            }
        }
        return 1;
    }
    return 0;
}

// The default scan set misses ACTORCAT_BG (Kibako2, lifts, mirrors), MISC
// (Leever) and ITEMACTION (fish, fairy, blue fire).
static const u8 sTrirodScanCats[] = {
    ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_NPC, ACTORCAT_BG, ACTORCAT_MISC, ACTORCAT_ITEMACTION, ACTORCAT_SWITCH,
};

// ── Selection ────────────────────────────────────────────────────────────────

static u8 Trirod_RowSelectable(u8 row) {
    return Trirod_RowInActiveList(row) && Nei_TrirodEchoLearned(row) && gTrirodEchoes[row].impl;
}

static u8 Trirod_SelNormalized(void) {
    u8 sel = Nei_TrirodGetSel();

    if ((sel < gTrirodEchoCount) && Trirod_RowSelectable(sel)) {
        return sel;
    }
    for (u8 i = 0; i < gTrirodEchoCount; i++) {
        if (Trirod_RowSelectable(i)) {
            Nei_TrirodSetSel(i);
            return i;
        }
    }
    return 0xFF;
}

void Trirod_Cycle(Player* p, PlayState* play, s8 dir) {
    u8 sel = Trirod_SelNormalized();

    (void)p;
    (void)play;
    // L is the wheel hold: it must never step, or opening the wheel would move
    // the selection first. Only R (dir > 0) cycles.
    if ((dir <= 0) || (sel == 0xFF)) {
        return;
    }
    for (u8 step = 1; step <= gTrirodEchoCount; step++) {
        u8 probe = (u8)((sel + step) % gTrirodEchoCount);

        if (Trirod_RowSelectable(probe)) {
            if (probe != sel) {
                const TrirodEcho* e = &gTrirodEchoes[probe];
                char msg[96];

                Nei_TrirodSetSel(probe);
                Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                snprintf(msg, sizeof(msg), "%s (%d)", e->name, e->cost);
                Nei_TrirodNotify(msg);
            }
            return;
        }
    }
}

// ── The wheel (Sheikah Slate UI) ─────────────────────────────────────────────

static void Trirod_OnWheelConfirm(s32 index) {
    if ((index >= 0) && (index < sTrirodWheelCount)) {
        u8 row = sTrirodWheelRows[index];

        if (Trirod_RowSelectable(row)) {
            Nei_TrirodSetSel(row);
        }
    }
}

// Every row of the ACTIVE list, in table order; learned+implemented ones are
// selectable, the rest sit grayed as collection progress (the slate does the
// same with locked runes).
static void Trirod_OpenWheel(PlayState* play) {
    BoxMenuEntry entries[TRIROD_ECHO_CAP];
    s32 selPos = 0;
    u8 sel = Trirod_SelNormalized();

    sTrirodWheelCount = 0;
    for (u8 row = 0; row < gTrirodEchoCount; row++) {
        if (!Trirod_RowInActiveList(row)) {
            continue;
        }
        entries[sTrirodWheelCount].iconPath = gTrirodEchoes[row].icon;
        entries[sTrirodWheelCount].iconSize = 32;
        entries[sTrirodWheelCount].enabled = Trirod_RowSelectable(row);
        sTrirodWheelRows[sTrirodWheelCount] = row;
        if (row == sel) {
            selPos = sTrirodWheelCount;
        }
        sTrirodWheelCount++;
    }
    if (sTrirodWheelCount > 0) {
        BoxMenu_Open(play, entries, sTrirodWheelCount, selPos, BTN_L, Trirod_OnWheelConfirm);
    }
}

// ── Aim (every equipped frame) ───────────────────────────────────────────────

// The block's aiming rays. Fallbacks only in case the header constants move.
#ifndef CANE_PLACE_RAY_UP
#define CANE_PLACE_RAY_UP 60.0f
#endif
#ifndef CANE_PLACE_RAY_DOWN
#define CANE_PLACE_RAY_DOWN 200.0f
#endif

void Trirod_Aim(Player* p, PlayState* play) {
    if (play != sTrirodLastPlay) {
        Trirod_ForgetPool(); // scene changed — every pooled pointer is poison
        sTrirodLastPlay = play;
    }
    Trirod_CleanupPool();

    // ---- HOLD L: the echo wheel. Read from cur.button — the press bit is
    // consumed by Z-target long before item code runs (the slate documents it).
    if (!BoxMenu_IsOpen()) {
        u16 held = play->state.input[0].cur.button;

        if (held & BTN_L) {
            if (sTrirodLHold < (TRIROD_WHEEL_HOLD_FRAMES + 1)) {
                sTrirodLHold++;
            }
            if (sTrirodLHold == TRIROD_WHEEL_HOLD_FRAMES) {
                Trirod_OpenWheel(play);
            }
        } else {
            sTrirodLHold = 0;
        }
    }

    // ---- Scan: sources to learn, summons to dismiss.
    sTrirodAimActor = TargetSelect_ScanCats(play, sTrirodScanCats, ARRAY_COUNT(sTrirodScanCats), Trirod_ScanFilter,
                                            TARGETSEL_DEFAULT_RANGE, TARGETSEL_DEFAULT_CONE);
    sTrirodAimSource = Trirod_MatchSource(sTrirodAimActor, play);
    sTrirodAimIsOurs = (Trirod_FindSummon(sTrirodAimActor) != NULL);

    if (sTrirodAimActor != NULL) {
        if (sTrirodAimIsOurs) {
            Actor_SetColorFilter(sTrirodAimActor, 0x4000, 255, 0, 4); // red: C dismisses
        } else if (sTrirodAimSource >= 0) {
            u8 row = gTrirodScanSources[sTrirodAimSource].echoIdx;
            u8 eff = Trirod_EffectiveRow(row);

            // Only SCANNABLE rows light up; creatures must be killed, and lighting
            // them white would promise a learn the press cannot deliver.
            if ((gTrirodEchoes[row].learn == TRIROD_LEARN_SCAN) && !Nei_TrirodEchoLearned(eff)) {
                Actor_SetColorFilter(sTrirodAimActor, 0x8000, 255, 0, 4); // white: C learns
            }
        }
    }

    // ---- Placement ghost: the pushable block's aiming, verbatim. Camera yaw,
    // fixed distance, floor snap, then the same clearance check the block runs.
    {
        s16 yaw = Cane_CameraYaw(play, p);
        Vec3f pos;
        Vec3f rayFrom;
        CollisionPoly* outPoly = NULL;
        s32 bgId = BGCHECK_SCENE;
        f32 floorY;

        pos.x = p->actor.world.pos.x + (Math_SinS(yaw) * CANE_PLACE_DIST);
        pos.y = p->actor.world.pos.y;
        pos.z = p->actor.world.pos.z + (Math_CosS(yaw) * CANE_PLACE_DIST);

        rayFrom = pos;
        rayFrom.y += CANE_PLACE_RAY_UP;
        floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &outPoly, &bgId, &p->actor, &rayFrom);

        sTrirodGhostYaw = yaw;
        if ((floorY <= BGCHECK_Y_MIN) || ((pos.y - floorY) > CANE_PLACE_RAY_DOWN)) {
            sTrirodGhostPos = pos;
            sTrirodGhostValid = 0;
        } else {
            pos.y = floorY;
            sTrirodGhostPos = pos;
            sTrirodGhostValid = CaneSummon_PlacementValid(play, CANE_SUMMON_BLOCK, &pos);
        }
    }

    // Soft periodic pulse marks OUR summons without eating their damage flashes.
    if ((play->gameplayFrames % 24) == 0) {
        for (u8 i = 0; i < TRIROD_MAX_SUMMONS; i++) {
            if ((sTrirodPool[i].actor != NULL) && (sTrirodPool[i].actor->update != NULL) &&
                (sTrirodPool[i].actor->colorFilterTimer == 0)) {
                Actor_SetColorFilter(sTrirodPool[i].actor, 0, 120, 0, 6);
            }
        }
    }
}

// ── The press ────────────────────────────────────────────────────────────────

// Water at the ghost decides the Octorok/Mad Scrub body.
static u8 Trirod_GhostInWater(PlayState* play) {
    f32 ySurface;
    WaterBox* box = NULL;

    if (WaterBox_GetSurface1(play, &play->colCtx, sTrirodGhostPos.x, sTrirodGhostPos.z, &ySurface, &box)) {
        return ySurface > sTrirodGhostPos.y;
    }
    return 0;
}

// 1 = consumed here (learn / dismiss / rejection); 0 = hand to the swing.
u8 Trirod_OnPress(Player* p, PlayState* play) {
    u8 sel;

    if (sTrirodAimIsOurs) {
        Trirod_Dismiss(play, Trirod_FindSummon(sTrirodAimActor));
        Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return 1;
    }

    // Learn — SCAN rows only; the fold decides which bit actually lights.
    if (sTrirodAimSource >= 0) {
        u8 row = gTrirodScanSources[sTrirodAimSource].echoIdx;
        u8 eff = Trirod_EffectiveRow(row);

        if ((gTrirodEchoes[row].learn == TRIROD_LEARN_SCAN) && !Nei_TrirodEchoLearned(eff)) {
            const TrirodEcho* e = &gTrirodEchoes[eff];
            char msg[96];

            Nei_TrirodLearnEcho(eff);
            Nei_TrirodSetSel(eff);
            Audio_PlaySoundGeneral(NA_SE_SY_GET_ITEM, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            snprintf(msg, sizeof(msg), "Echo learned: %s (%d)", e->name, e->cost);
            Nei_TrirodNotify(msg);
            return 1;
        }
    }

    sel = Trirod_SelNormalized();
    if (sel == 0xFF) {
        Nei_TrirodNotify("No echoes learned - scan props, defeat creatures with the rod drawn");
        return 1;
    }
    if (!sTrirodGhostValid) {
        return 1; // the red ghost already said no
    }

    {
        const TrirodEcho* e = &gTrirodEchoes[sel];
        s16 body = e->actorId;
        s16 params = e->spawnParams;
        s16 objectId = e->objectId;

        if ((e->spawnRule == TRIROD_SPAWN_WATER_OR_LAND) && (e->altActorId >= 0) && !Trirod_GhostInWater(play)) {
            body = e->altActorId;
            params = e->altParams;
            objectId = e->altObjectId;
        }

        // Object residency: requesting starts the load, this press is spent, the
        // next one lands — the Somaria block's rule.
        if (Object_GetIndex(&play->objectCtx, objectId) < 0) {
            Object_Spawn(&play->objectCtx, objectId);
            return 1;
        }

        Trirod_EvictUntilFits(play, e->cost);

        sTrirodPendingEcho = sel;
        sTrirodPendingActor = body;
        sTrirodPendingParams = params;
        sTrirodPendingPos = sTrirodGhostPos;
        sTrirodPendingYaw = sTrirodGhostYaw;
    }
    return 0; // proceed to the swing; its spawn frame calls Trirod_FireSummon
}

// ── The spawn (Cane_FireSkill case 6, on the animation's spawn frame) ────────

void Trirod_FireSummon(Player* p, PlayState* play) {
    const TrirodEcho* e;
    Actor* spawned = NULL;
    s16 slot = -1;

    (void)p;
    if (sTrirodPendingEcho >= gTrirodEchoCount) {
        return;
    }
    e = &gTrirodEchoes[sTrirodPendingEcho];

    for (u8 i = 0; i < TRIROD_MAX_SUMMONS; i++) {
        if (sTrirodPool[i].actor == NULL) {
            slot = (s16)i;
            break;
        }
    }
    if (slot < 0) {
        sTrirodPendingEcho = 0xFF;
        return;
    }

    // Block and Platform route through the Somaria summon system: it owns their
    // params quirks (switch-flag-free block) and the env-colour draw fix.
    if (sTrirodPendingActor == ACTOR_OBJ_OSHIHIKI) {
        spawned = CaneSummon_Spawn(play, CANE_SUMMON_BLOCK, &sTrirodPendingPos, sTrirodPendingYaw);
    } else if (sTrirodPendingActor == ACTOR_OBJ_LIFT) {
        Vec3f pos = sTrirodPendingPos;

        pos.y += 10.0f; // the slab floats a little, like its cane placement
        spawned = CaneSummon_Spawn(play, CANE_SUMMON_PLATFORM, &pos, sTrirodPendingYaw);
    } else {
        spawned =
            Actor_Spawn(&play->actorCtx, play, sTrirodPendingActor, sTrirodPendingPos.x, sTrirodPendingPos.y + e->yOff,
                        sTrirodPendingPos.z, 0, sTrirodPendingYaw, 0, sTrirodPendingParams);
    }

    if (spawned != NULL) {
        Vec3f zero = { 0.0f, 0.0f, 0.0f };
        Vec3f flash = sTrirodPendingPos;

        sTrirodPool[slot].actor = spawned;
        sTrirodPool[slot].echoIdx = sTrirodPendingEcho;
        sTrirodPool[slot].seq = sTrirodSeq++;

        flash.y += 20.0f;
        EffectSsBlast_SpawnWhiteShockwave(play, &flash, &zero, &zero);
        Audio_PlaySoundGeneral(NA_SE_PL_MAGIC_SOUL_BALL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        Actor_SetColorFilter(spawned, 0, 160, 0, 16);
    }
    sTrirodPendingEcho = 0xFF;
}

// ── Kill-to-learn (bridged from Enemy_StartFinishingBlow in z_actor.c) ───────

void Trirod_NotifyEnemyDown(PlayState* play, Actor* actor) {
    s16 src;
    u8 row;
    u8 eff;

    (void)play;
    if (!shSomariaActive || (Cane_GetType() != CANE_TYPE_TRIROD)) {
        return; // the rod must be DRAWN — that is the whole rando rule
    }
    if (Trirod_FindSummon(actor) != NULL) {
        return; // your own echo dying teaches nothing
    }
    src = Trirod_MatchSource(actor, play);
    if (src < 0) {
        return;
    }
    row = gTrirodScanSources[src].echoIdx;
    if (gTrirodEchoes[row].learn != TRIROD_LEARN_KILL) {
        return;
    }
    eff = Trirod_EffectiveRow(row);
    if (Nei_TrirodEchoLearned(eff)) {
        return;
    }
    {
        const TrirodEcho* e = &gTrirodEchoes[eff];
        char msg[96];

        Nei_TrirodLearnEcho(eff);
        Nei_TrirodSetSel(eff);
        Audio_PlaySoundGeneral(NA_SE_SY_GET_ITEM, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        snprintf(msg, sizeof(msg), "Echo learned: %s (%d)", e->name, e->cost);
        Nei_TrirodNotify(msg);
    }
}

// ── Ghost rendering ──────────────────────────────────────────────────────────

// Miniature billboard fallback for echoes with no single display list.
static Vtx sTrirodGhostVtx[] = {
    VTX(-1, 0, 0, 0, 32 << 5, 0, 0, 0, 255),
    VTX(1, 0, 0, 32 << 5, 32 << 5, 0, 0, 0, 255),
    VTX(1, 2, 0, 32 << 5, 0, 0, 0, 0, 255),
    VTX(-1, 2, 0, 0, 0, 0, 0, 0, 255),
};

static Gfx sTrirodGhostDL[] = {
    gsSPVertex(sTrirodGhostVtx, 4, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSPEndDisplayList(),
};

void Trirod_DrawPreview(PlayState* play, Player* p) {
    u8 sel = Trirod_SelNormalized();
    const TrirodEcho* e;
    f32 pulse;
    u8 valid;

    (void)p;
    if (BoxMenu_IsOpen()) {
        return; // the wheel owns the screen
    }
    if (sel == 0xFF) {
        return;
    }
    e = &gTrirodEchoes[sel];
    valid = sTrirodGhostValid;
    pulse = 0.94f + (0.06f * Math_SinS((s16)(play->gameplayFrames * 1500)));

    if (e->preview == TRIROD_PV_CUBE) {
        // The Somaria ghost verbatim (blue/red, breathing) — sized as block or slab.
        CaneSummonKind kind = (e->actorId == ACTOR_OBJ_LIFT) ? CANE_SUMMON_PLATFORM : CANE_SUMMON_BLOCK;

        CaneSummon_DrawPreview(play, kind, &sTrirodGhostPos, sTrirodGhostYaw, valid);
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    if ((e->preview == TRIROD_PV_DL) && (e->pvDL != NULL)) {
        // The target actor's own display list as an UNTEXTURED silhouette. A prop's
        // DL sets its own combiner and loads its own texture, so nothing set before
        // it can strip the texture — that state gets overwritten inside the DL. Fog
        // can: it is applied in the BLENDER, after the combiner, so a fog of near=0
        // far=1 replaces every fragment's colour with the fog colour no matter what
        // the DL sampled. That is the whole trick, and it is why the ghost is a
        // flat blue/red shape until the real actor spawns with its texture.
        Matrix_Translate(sTrirodGhostPos.x, sTrirodGhostPos.y, sTrirodGhostPos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(sTrirodGhostYaw), MTXMODE_APPLY);
        Matrix_Scale(e->pvScale * pulse, e->pvScale * pulse, e->pvScale * pulse, MTXMODE_APPLY);

        if (valid) {
            POLY_XLU_DISP = Gfx_SetFog(POLY_XLU_DISP, 90, 170, 255, 255, 0, 1);
        } else {
            POLY_XLU_DISP = Gfx_SetFog(POLY_XLU_DISP, 255, 70, 70, 255, 0, 1);
        }
        // Fog only takes effect with G_FOG on and a FOG_SHADE render mode; the DL's
        // own render mode wins if it sets one, so force ours after it via a
        // translucent, fogged pass mode.
        gSPSetGeometryMode(POLY_XLU_DISP++, G_FOG);
        gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);
        gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 110); // alpha of the ghost
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 110);
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)e->pvDL);
        // Put the scene's fog back — the next thing drawn on this list must not be
        // painted flat too.
        POLY_XLU_DISP = Gfx_SetFog(POLY_XLU_DISP, play->lightCtx.fogColor[0], play->lightCtx.fogColor[1],
                                   play->lightCtx.fogColor[2], 0, play->lightCtx.fogNear, play->lightCtx.fogFar);
    } else if (e->icon != NULL) {
        // Billboard miniature, tinted by validity like the cube.
        f32 bob = 4.0f * Math_SinS((s16)(play->gameplayFrames * 1200));

        Matrix_Translate(sTrirodGhostPos.x, sTrirodGhostPos.y + 14.0f + bob, sTrirodGhostPos.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_Scale(11.0f * pulse, 11.0f * pulse, 11.0f * pulse, MTXMODE_APPLY);

        gDPSetCombineLERP(POLY_XLU_DISP++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0,
                          TEXEL0, 0, PRIMITIVE, 0);
        if (valid) {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 210, 230, 255, 190);
        } else {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 120, 120, 170);
        }
        gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);
        gDPLoadTextureBlock(POLY_XLU_DISP++, e->icon, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0, G_TX_CLAMP, G_TX_CLAMP,
                            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, sTrirodGhostDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
