/**
 * item_lantern.c - Poe Lantern: catch fire, illuminate, apply elemental effects
 *
 * Uses gPoeLanternDL from object_poh. Bottle-swing action catches fire from
 * nearby sources. Fire persists until extinguished in Kaleido (long-press C).
 * When lit, adds a real point light source to Player.
 *
 * Swing freezes player in place (like bottle) until animation finishes.
 * On fire catch: plays catch animation → shows typed message → waits for close.
 */

#include "z64.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "../helpers/fx_helper.h"
#include "macros.h"
#include "functions.h"
#include "item_lantern.h"
#include "../../nei_save.h" // Skijer's NEI
#include "objects/object_poh/object_poh.h"
#include "objects/gameplay_keep/gameplay_keep.h"
// Fire sources whose flame COLOUR has to be read off the actor itself (see
// Lantern_DetectFireType): the Poe-sister torches and the Poes' lanterns.
#include "overlays/actors/ovl_Bg_Po_Syokudai/z_bg_po_syokudai.h"
#include "overlays/actors/ovl_En_Po_Sisters/z_en_po_sisters.h"
#include "overlays/actors/ovl_En_Poh/z_en_poh.h"
#include "overlays/actors/ovl_Obj_Syokudai/z_obj_syokudai.h"
#include "overlays/actors/ovl_En_Light/z_en_light.h" // summoned flame: light type + colour params

// ── Global: catch message pending ──────────────────────────────────────────
// Set to fire type (1-4) when fire is caught. ItemMessages.cpp reads this
// to build the catch message. Reset to 0 after message is shown.
u8 gLanternCatchPending = 0;

// ── Catchable fire source table ─────────────────────────────────────────────
// Only used as the "is this actor a fire source at all" filter. The fire TYPE is
// resolved from the actor's real flame colour in Lantern_DetectFireType — the
// table's type is just the fallback for sources with a single fixed colour.

static const LanternCatchEntry sCatchableFires[] = {
    { ACTOR_OBJ_SYOKUDAI, LANTERN_FIRE_REGULAR }, // Lit torch
    { ACTOR_EN_BW, LANTERN_FIRE_REGULAR },        // Torch slug
    { ACTOR_EN_LIGHT, LANTERN_FIRE_REGULAR },     // General flame (colour from params)
    { ACTOR_EN_ICE_HONO, LANTERN_FIRE_BLUE },     // Blue fire
    { ACTOR_EN_POH, LANTERN_FIRE_POE },           // Poe / composer brother lantern
    { ACTOR_EN_PO_SISTERS, LANTERN_FIRE_POE },    // Poe sister torch (Forest Temple)
    { ACTOR_EN_PO_FIELD, LANTERN_FIRE_POE },      // Field Poe
    { ACTOR_EN_PO_DESERT, LANTERN_FIRE_POE },     // Desert Poe (Haunted Wasteland guide)
    { ACTOR_BG_PO_SYOKUDAI, LANTERN_FIRE_POE },   // Poe-sister torch stand (Forest Temple)
};
#define CATCHABLE_COUNT (sizeof(sCatchableFires) / sizeof(sCatchableFires[0]))

// The four Poe-sister colours, in the order both the torch stands (BgPoSyokudai
// flameColor) and the sisters themselves (EnPoSisters unk_194) use them.
// Joelle's red burns as ordinary fire — the lantern has no separate red flame.
static const u8 sPoeColorToFire[4] = {
    LANTERN_FIRE_POE,     // 0 purple — Meg
    LANTERN_FIRE_REGULAR, // 1 red    — Joelle
    LANTERN_FIRE_BLUE,    // 2 blue   — Beth
    LANTERN_FIRE_GREEN,   // 3 green  — Amy
};

// En_Light flame colour per params & 0xF, mirroring D_80A9E840 in z_en_light.c.
static const u8 sEnLightFire[16] = {
    LANTERN_FIRE_REGULAR, // 0  orange
    LANTERN_FIRE_REGULAR, // 1  orange
    LANTERN_FIRE_BLUE,    // 2  blue
    LANTERN_FIRE_GREEN,   // 3  green
    LANTERN_FIRE_REGULAR, // 4  orange (small)
    LANTERN_FIRE_REGULAR, // 5  orange
    LANTERN_FIRE_GREEN,   // 6  green
    LANTERN_FIRE_BLUE,    // 7  blue
    LANTERN_FIRE_REGULAR, // 8  deep red
    LANTERN_FIRE_REGULAR, // 9  red-orange
    LANTERN_FIRE_REGULAR, // 10 yellow
    LANTERN_FIRE_GREEN,   // 11 yellow-green
    LANTERN_FIRE_POE,     // 12 pink
    LANTERN_FIRE_POE,     // 13 purple
    LANTERN_FIRE_BLUE,    // 14 blue
    LANTERN_FIRE_BLUE,    // 15 cyan
};

// ── Light source statics ────────────────────────────────────────────────────

static LightNode* sLanternLightNode = NULL;
static LightInfo sLanternLightInfo;

// ── Helpers ─────────────────────────────────────────────────────────────────

// Map a live flame colour onto one of the four lantern fires. Poes recolour their
// lantern as they change state (red while attacking, green while fleeing, pale
// white while idle), so the colour has to be classified, not looked up.
static LanternFireType Lantern_ClassifyColor(s32 r, s32 g, s32 b) {
    s32 max = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    s32 min = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);

    // Near-neutral (white / pale yellow): the Poe's own ghost-fire.
    if ((max - min) < 60) {
        return LANTERN_FIRE_POE;
    }
    // Magenta / violet: red and blue both strong with green sitting below both.
    if ((r >= 90) && (b >= 90) && (g < r) && (g < b)) {
        return LANTERN_FIRE_POE;
    }
    if ((g >= r) && (g >= b)) {
        return LANTERN_FIRE_GREEN;
    }
    if ((b > r) && (b > g)) {
        return LANTERN_FIRE_BLUE;
    }
    return LANTERN_FIRE_REGULAR; // red / orange / yellow all burn as regular fire
}

static LanternFireType Lantern_DetectFireType(Actor* actor, PlayState* play) {
    u32 i;

    switch (actor->id) {
        // Forest Temple golden torch stands. flameColor is the Poe-sister index
        // (params >> 8 at Init; params itself is masked down to the switch flag
        // afterwards, so the struct field is the only place the colour survives).
        case ACTOR_BG_PO_SYOKUDAI: {
            BgPoSyokudai* torch = (BgPoSyokudai*)actor;

            if (!Flags_GetSwitch(play, actor->params)) {
                return LANTERN_FIRE_NONE; // unlit stand — nothing to take
            }
            return sPoeColorToFire[torch->flameColor & 3];
        }

        // The sisters carry the same four flames on their own torches.
        case ACTOR_EN_PO_SISTERS: {
            EnPoSisters* sister = (EnPoSisters*)actor;

            return sPoeColorToFire[sister->unk_194 & 3];
        }

        // Poes and the composer brothers: take whatever their lantern is burning
        // right now (Sharp/Flat idle green, attack red, flee blue-green).
        case ACTOR_EN_POH: {
            EnPoh* poe = (EnPoh*)actor;

            return Lantern_ClassifyColor(poe->lightColor.r, poe->lightColor.g, poe->lightColor.b);
        }

        case ACTOR_EN_LIGHT:
            return sEnLightFire[actor->params & 0xF];

        // Torches only give fire while actually burning (litTimer < 0 = lit for good).
        case ACTOR_OBJ_SYOKUDAI: {
            ObjSyokudai* torch = (ObjSyokudai*)actor;

            return (torch->litTimer != 0) ? LANTERN_FIRE_REGULAR : LANTERN_FIRE_NONE;
        }

        default:
            break;
    }

    for (i = 0; i < CATCHABLE_COUNT; i++) {
        if (actor->id == sCatchableFires[i].actorId) {
            return sCatchableFires[i].fireType;
        }
    }
    return LANTERN_FIRE_NONE;
}

// The lit lantern is a real light source: it burns in the flame's own colour, from
// the hand that is actually holding it, and flickers like every other flame in the
// game instead of sitting at a constant brightness.
static void Lantern_UpdateLight(Player* p, PlayState* play) {
    u8 fireType = gCustomItemState.lanternFireType;
    f32 flicker;
    Vec3f lightPos;
    s16 radius;
    u8 r;
    u8 g;
    u8 b;

    if ((fireType == LANTERN_FIRE_NONE) || (fireType >= LANTERN_FIRE_MAX)) {
        if (sLanternLightNode != NULL) {
            LightContext_RemoveLight(play, &play->lightCtx, sLanternLightNode);
            sLanternLightNode = NULL;
        }
        return;
    }

    // While the lantern is drawn in hand the light comes off the lantern itself;
    // stowed, it falls back to Link's chest height so the glow does not disappear.
    if (gCustomItemState.lanternEquipped || gCustomItemState.lanternSwinging) {
        lightPos = p->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    } else {
        lightPos = p->actor.world.pos;
        lightPos.y += 40.0f;
    }

    flicker = 0.8f + (Rand_ZeroOne() * 0.2f);
    r = (u8)(sLanternLightColors[fireType][0] * flicker);
    g = (u8)(sLanternLightColors[fireType][1] * flicker);
    b = (u8)(sLanternLightColors[fireType][2] * flicker);
    radius = (s16)(LANTERN_LIGHT_RADIUS * (0.9f + (flicker * 0.1f)));

    Lights_PointNoGlowSetInfo(&sLanternLightInfo, (s16)lightPos.x, (s16)lightPos.y, (s16)lightPos.z, r, g, b, radius);

    if (sLanternLightNode == NULL) {
        sLanternLightNode = LightContext_InsertLight(play, &play->lightCtx, &sLanternLightInfo);
    }
}

static void Lantern_RemoveLight(PlayState* play) {
    if (sLanternLightNode != NULL) {
        LightContext_RemoveLight(play, &play->lightCtx, sLanternLightNode);
        sLanternLightNode = NULL;
    }
}

// ── Save sync helpers ───────────────────────────────────────────────────────

static void Lantern_SyncToSave(void) {
    Nei_Save()->lanternFireType = gCustomItemState.lanternFireType; // Skijer's NEI
    // Mark this fire type as ever-captured so the kaleido selector can offer it
    // again after the player extinguishes / swaps. lanternFireType 0 = "none",
    // which is always implicitly available so we don't track it as a bit.
    if (gCustomItemState.lanternFireType > 0 && gCustomItemState.lanternFireType < 8) {
        Nei_Save()->lanternCapturedTypes |= (1 << gCustomItemState.lanternFireType); // Skijer's NEI
    }
}

// No per-frame sync from save. The save value is loaded into gCustomItemState
// once at file load time via SaveManager (see SaveManager.cpp LoadBase).
// Runtime is always authoritative; save is updated on catch via Lantern_SyncToSave.

// ── Poe capture ─────────────────────────────────────────────────────────────
// Taking a Poe's fire takes the Poe with it, and it has to leave the world in the same
// state killing it would. A bare Actor_Kill skips everything the actor does on death —
// for the Forest Temple sisters that is the switch flag that lights her torch and counts
// her towards Meg, so capturing one used to lose the progress a sword kill gives.
static void Lantern_CapturePoe(Actor* actor, PlayState* play) {
    switch (actor->id) {
        case ACTOR_EN_PO_SISTERS: {
            EnPoSisters* sister = (EnPoSisters*)actor;

            // Straight out of func_80ADB17C (the sister's own death): light her torch,
            // clear Meg's "sisters still alive" flag, hand the room's environment back.
            Flags_SetSwitch(play, actor->params);
            SoundSource_PlaySfxAtFixedWorldPos(play, &actor->world.pos, 30, NA_SE_EV_FLAME_IGNITION);
            if (sister->unk_194 == 0) { // Meg
                Flags_UnsetSwitch(play, 0x1B);
            }
            play->envCtx.unk_BF = 0xFF;
            Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
            break;
        }

        case ACTOR_EN_POH:
            // Sharp and Flat are NPCs, not enemies — never kill a composer for his fire.
            if ((actor->params == EN_POH_SHARP) || (actor->params == EN_POH_FLAT)) {
                return;
            }
            break;

        case ACTOR_EN_PO_FIELD:
        case ACTOR_EN_PO_DESERT:
            break;

        default:
            return; // torches and loose flames are not consumed by the catch
    }

    Audio_PlayActorSound2(actor, NA_SE_EN_PO_LAUGH2);
    Actor_Kill(actor);
}

// ── Fire Catch (during swing catch window) ──────────────────────────────────

u8 Lantern_TryCatch(Player* p, PlayState* play) {
    Vec3f playerPos = p->actor.world.pos;
    s16 playerYaw = p->actor.shape.rot.y;

    // BG and MISC are in the list because the desert Poe lives in BG and the field Poe
    // moves itself to MISC once it is following the player.
    static const u8 categories[] = { ACTORCAT_ITEMACTION, ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_BG, ACTORCAT_MISC };

    for (u32 c = 0; c < ARRAY_COUNT(categories); c++) {
        Actor* actor = play->actorCtx.actorLists[categories[c]].head;
        while (actor != NULL) {
            if (actor->update != NULL) {
                f32 dx = actor->world.pos.x - playerPos.x;
                f32 dz = actor->world.pos.z - playerPos.z;
                f32 distSq = dx * dx + dz * dz;

                if (distSq < SQ(LANTERN_CATCH_RANGE)) {
                    s16 angleToActor = Math_Atan2S(dx, dz);
                    s16 angleDiff = angleToActor - playerYaw;
                    if (angleDiff < 0)
                        angleDiff = -angleDiff;
                    if (angleDiff > 0x4000)
                        angleDiff = 0x7FFF - angleDiff;

                    if (angleDiff < 0x4000) {
                        LanternFireType type = Lantern_DetectFireType(actor, play);
                        if (type != LANTERN_FIRE_NONE) {
                            gCustomItemState.lanternFireType = type;
                            Lantern_SyncToSave();
                            Audio_PlayActorSound2(&p->actor, NA_SE_EV_FLAME_IGNITION);

                            // Poe catch = the Poe goes with its fire (like a bottled fairy).
                            // Keyed on the ACTOR, not on the fire colour: a red sister hands
                            // over regular fire and still has to die for it.
                            Lantern_CapturePoe(actor, play);

                            return 1; // caught!
                        }
                    }
                }
            }
            actor = actor->next;
        }
    }
    return 0;
}

// Lantern_TryCatch and Lantern_ApplyFireEffects are called directly
// from Player_Action_SwingLantern in z_player.c (unity build — same TU).

// ── Fire Effects (swing while lit) ──────────────────────────────────────────

// En_Light params index the colour table in z_en_light.c (params & 0xF):
// 0 = orange, 2 = blue, 3 = green, 13 = pink/violet.
static const s16 sLanternFlameParam[] = {
    0x0000, // NONE — unused
    0x0000, // REGULAR — orange fire
    0x0002, // BLUE — icy blue fire
    0x000D, // POE — pink flame over a violet glow (shadow fire)
    0x0003, // GREEN — green fire
};

// ── Per-fire-type summoned-flame behaviour ──────────────────────────────────
// Every fire summons a flame and every fire keeps DMG_ARROW_FIRE so it can light
// torches; what changes is how long it burns, what else it inflicts and whether it
// hurts at all. damage 0 still registers the hit (torches, cobwebs, ice) but deals
// nothing — that is how green fire stays harmless without losing its fire identity.
typedef struct {
    s16 lifetime;
    u32 dmgFlags;
    u8 damage;
} LanternFireConfig;

static const LanternFireConfig sFireConfig[] = {
    /* NONE    */ { 0, 0, 0 },
    /* REGULAR */ { LANTERN_FLAME_LIFETIME, DMG_ARROW_FIRE, 2 },
    // BLUE also carries the ice flags so enemies freeze; the red-ice melt itself is
    // the job of the real En_Ice_Hono spawned alongside (Bg_Ice_Shelter only melts
    // for an actor whose id IS En_Ice_Hono, no damage flag can stand in for it).
    /* BLUE    */ { LANTERN_FLAME_LIFETIME, DMG_ARROW_FIRE | DMG_ARROW_ICE | DMG_MAGIC_ICE, 2 },
    /* POE     */ { LANTERN_FLAME_LIFETIME, DMG_ARROW_FIRE | DMG_ARROW_LIGHT, 2 },
    // Green is the healing fire: it burns three times as long and never hurts anything.
    /* GREEN   */ { LANTERN_FLAME_LIFETIME * 3, DMG_ARROW_FIRE, 0 },
};

// Swing AT collider (follows lantern arc during swing frames)
static ColliderCylinder sSwingCol;
static u8 sSwingColInited = 0;

static void Lantern_InitSwingCollider(Player* p, PlayState* play) {
    if (sSwingColInited)
        return;

    static ColliderCylinderInit sColInit = {
        { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
        { ELEMTYPE_UNK2, { 0, 0x01, 0 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
        { 25, 40, 0, { 0, 0, 0 } }
    };

    Collider_InitCylinder(play, &sSwingCol);
    Collider_SetCylinder(play, &sSwingCol, &p->actor, &sColInit);
    sSwingColInited = 1;
}

// Flame tracking + AT collider system (included for readability)
#include "item_lantern_flames.inc"

// Summon this fire's flame in front of Link. EVERY fire type summons one now.
static void Lantern_SpawnFireActor(Player* p, PlayState* play) {
    u8 fireType = gCustomItemState.lanternFireType;
    if (fireType == LANTERN_FIRE_NONE || fireType >= LANTERN_FIRE_MAX)
        return;

    s16 yaw = p->actor.shape.rot.y;
    f32 dist = 30.0f;
    f32 fx = p->actor.world.pos.x + Math_SinS(yaw) * dist;
    f32 fy = p->actor.world.pos.y; // Floor level
    f32 fz = p->actor.world.pos.z + Math_CosS(yaw) * dist;

    if (fireType == LANTERN_FIRE_BLUE) {
        // Blue fire keeps the real blue-fire actor alongside its flame: red ice only
        // melts for an actor whose id IS En_Ice_Hono (Bg_Ice_Shelter checks the id, not
        // a damage flag), and it spreads and falls on its own — so it is left untracked
        // and unscaled, exactly like a bottle release.
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ICE_HONO, fx, fy + 30.0f, fz, 0, 0, 0, 0);
    }

    Actor* flame =
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, fx, fy, fz, 0, 0, 0, sLanternFlameParam[fireType]);

    if (flame != NULL) {
        // No halo: En_Light asks for a GLOW light, which is the bright disc torches
        // have around them. A hand-thrown flame just lights the room.
        ((EnLight*)flame)->lightInfo.type = LIGHT_POINT_NOGLOW;
        Lantern_TrackFlame(flame, fireType, play); // seeds the size + per-fire collider
    }
}

// ── Grass/Bush Burn System ──────────────────────────────────────────────────
// Grass catches fire with visible flame particles, burns for BURN_TIME frames.
// While burning: pushes PLAYER upward (thermal updraft) if nearby.
// Fire spreads to nearby grass/bushes. After burn: grass destroyed.

#define LANTERN_BURN_TIME 80 // Frames to burn (~4 sec at 20fps)
#define LANTERN_BURN_SPREAD_RANGE 80.0f
#define LANTERN_UPDRAFT_RANGE 60.0f // Player gets launched if within this range
#define LANTERN_UPDRAFT_FORCE 8.0f  // Upward velocity applied to player
#define LANTERN_MAX_BURNING 16

typedef struct {
    Actor* actor;
    s16 timer;
} BurningEntry;

static BurningEntry sBurning[LANTERN_MAX_BURNING];

static u8 Lantern_IsBurning(Actor* actor) {
    for (s32 i = 0; i < LANTERN_MAX_BURNING; i++) {
        if (sBurning[i].actor == actor && sBurning[i].timer > 0)
            return 1;
    }
    return 0;
}

static void Lantern_Ignite(Actor* actor) {
    if (Lantern_IsBurning(actor))
        return;
    for (s32 i = 0; i < LANTERN_MAX_BURNING; i++) {
        if (sBurning[i].timer <= 0) {
            sBurning[i].actor = actor;
            sBurning[i].timer = LANTERN_BURN_TIME;
            return;
        }
    }
}

// Called every frame from CustomItems_Update — updates swing flame despawn + all burning grass/bushes
void Lantern_UpdateBurning(PlayState* play) {
    Player* p = GET_PLAYER(play);

    // ── Despawn all tracked flames (swing + grass) ──
    Lantern_UpdateFlames(play);

    // ── Update each burning grass entry ──
    for (s32 i = 0; i < LANTERN_MAX_BURNING; i++) {
        if (sBurning[i].timer <= 0)
            continue;
        Actor* actor = sBurning[i].actor;

        // Actor already dead?
        if (actor == NULL || actor->update == NULL) {
            sBurning[i].timer = 0;
            sBurning[i].actor = NULL;
            continue;
        }

        sBurning[i].timer--;

        // ── Spawn visible flame on grass (first frame only) ──
        // Burns in the lantern's own colour and grows in, like every other flame.
        if (sBurning[i].timer == LANTERN_BURN_TIME - 1) {
            u8 grassFire = gCustomItemState.lanternFireType;

            if ((grassFire != LANTERN_FIRE_NONE) && (grassFire < LANTERN_FIRE_MAX)) {
                Actor* grassFlame =
                    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, actor->world.pos.x, actor->world.pos.y,
                                actor->world.pos.z, 0, 0, 0, sLanternFlameParam[grassFire]);
                if (grassFlame != NULL) {
                    ((EnLight*)grassFlame)->lightInfo.type = LIGHT_POINT_NOGLOW;
                    Lantern_TrackFlame(grassFlame, grassFire, play);
                }
            }
        }

        // ── Updraft: push PLAYER upward if near burning grass ──
        {
            f32 dx = p->actor.world.pos.x - actor->world.pos.x;
            f32 dz = p->actor.world.pos.z - actor->world.pos.z;
            if ((dx * dx + dz * dz) < SQ(LANTERN_UPDRAFT_RANGE)) {
                if (p->actor.velocity.y < LANTERN_UPDRAFT_FORCE) {
                    p->actor.velocity.y = LANTERN_UPDRAFT_FORCE;
                }
            }
        }

        // ── Spread fire to nearby grass/bushes (1 second delay = 60 frames in) ──
        if (sBurning[i].timer == LANTERN_BURN_TIME - 60) {
            Actor* other = play->actorCtx.actorLists[ACTORCAT_PROP].head;
            while (other != NULL) {
                if (other->update != NULL && other != actor &&
                    (other->id == ACTOR_EN_KUSA || other->id == ACTOR_OBJ_MURE3)) {
                    f32 odx = other->world.pos.x - actor->world.pos.x;
                    f32 odz = other->world.pos.z - actor->world.pos.z;
                    if ((odx * odx + odz * odz) < SQ(LANTERN_BURN_SPREAD_RANGE)) {
                        Lantern_Ignite(other);
                    }
                }
                other = other->next;
            }
        }

        // ── Burn complete: destroy grass ──
        if (sBurning[i].timer <= 0) {
            Actor_Kill(actor);
            sBurning[i].actor = NULL;
        }
    }
}

// Ignite nearby grass/bushes in range (called ONCE on swing)
static void Lantern_IgniteNearbyGrass(Player* p, PlayState* play) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].head;
    while (actor != NULL) {
        if (actor->update != NULL && (actor->id == ACTOR_EN_KUSA || actor->id == ACTOR_OBJ_MURE3)) {
            f32 dx = actor->world.pos.x - p->actor.world.pos.x;
            f32 dz = actor->world.pos.z - p->actor.world.pos.z;
            if ((dx * dx + dz * dz) < SQ(LANTERN_EFFECT_RANGE)) {
                Lantern_Ignite(actor);
            }
        }
        actor = actor->next;
    }
}

// ── In-hand test ────────────────────────────────────────────────────────────
// "In Link's hand" = the exact condition CustomItems_Draw uses to draw the lantern
// on him: it is on a button AND it has been taken out (pressing its button sets
// lanternEquipped; Handle_Lantern drops it again as soon as another item action owns
// the hand). Anything that gates on the lantern being HELD must use this, so the
// effect and the model can never disagree.
static u8 Lantern_IsInHand(void) {
    if (!IsItemEquipped(ITEM_LANTERN)) {
        return 0;
    }
    return gCustomItemState.lanternEquipped || gCustomItemState.lanternSwinging;
}

// ── Poe fire: reveal ALL invisible Poes ─────────────────────────────────────

// Poe fire lens — runs from CustomItems_Update (ALWAYS, even unequipped).
//
// The lantern owns actorCtx.lensFromLantern and NOTHING else: it must not touch
// lensActive, magicState or the magic meter. Sharing those with the real Lens of
// Truth is what broke both items — the lantern requested MAGIC_CONSUME_LENS with
// no Lens on a button, so z_parameter tore the state down again on the very next
// frame (state IDLE + lensActive false), and pressing the Lens saw lensActive
// already true and toggled it straight back OFF. z_actor.c now ORs this flag with
// lensActive, so the Poe fire and the Lens work independently and stack.
// The Garo form sees through the world for free, the same way the Poe fire does.
// It is OR'd into this gate rather than driving actorCtx.lensActive from
// garo_form.cpp: two owners toggling the same flag would each undo the other on
// the frames the other one wanted it, and every reason the lens can be on
// without the Lens of Truth belongs in one place.
u8 GaroForm_HasPassiveLens(void);

void Lantern_UpdateLens(PlayState* play) {
    // While HOLDING IN HAND — literally: this is the same test CustomItems_Draw uses to
    // put the lantern in Link's fist, so the shadow lens is on exactly while you can SEE
    // the lantern being held. Press its button to take it out; it stays out until another
    // item takes the hand (drawing the sword) or the lantern leaves the buttons.
    u8 wantLens =
        ((gCustomItemState.lanternFireType == LANTERN_FIRE_POE) && Lantern_IsInHand()) || GaroForm_HasPassiveLens();

    // Vanilla drops the lens during real cutscenes; match that. Player_InCsMode is NOT used
    // as the test — it is also true for item cutscenes, textboxes and any state that sets
    // PLAYER_STATE1_IN_CUTSCENE (the lantern's own fire-catch does), which would blink the
    // lens off during ordinary play.
    if (play->csCtx.state != CS_STATE_IDLE) {
        wantLens = 0;
    }

    // Drive the REAL lens instead of running a lens of our own. A parallel flag had to
    // re-implement every place the engine consults lensActive (collection, draw gate) and
    // any one of them being missed meant nothing appeared. Owning actorCtx.lensActive means
    // the shadow fire IS the Lens of Truth, and the only difference left is the circle
    // overlay, which Actor_DrawLensActors skips while lensFromLantern is set.
    //
    // The magic meter is still never touched: the drain lives in the MAGIC_STATE_CONSUME_LENS
    // branch of z_parameter, and that state is only entered by the Lens ITEM
    // (Magic_RequestChange). Leaving magicState IDLE is exactly what makes this a free lens.
    if (wantLens) {
        play->actorCtx.lensActive = true;
        play->actorCtx.lensFromLantern = 1;
    } else if (play->actorCtx.lensFromLantern) {
        play->actorCtx.lensFromLantern = 0;
        // Only take the lens down if it is ours — a real Lens of Truth session (the item
        // holds magicState in CONSUME_LENS) must keep running.
        if (gSaveContext.magicState != MAGIC_STATE_CONSUME_LENS) {
            play->actorCtx.lensActive = false;
        }
    }
}

// ── All fire effects combined (called ONCE per swing on first active frame) ─

void Lantern_ApplyFireEffects(Player* p, PlayState* play) {
    // Spawn one fire actor in front of Link
    Lantern_SpawnFireActor(p, play);
    Audio_PlayActorSound2(&p->actor, NA_SE_EV_FLAME_IGNITION);

    // All fire types: ignite nearby grass (burn over time + spread + updraft)
    Lantern_IgniteNearbyGrass(p, play);
}

// Update swing collider + trail VFX during active swing frames
static void Lantern_UpdateSwing(Player* p, PlayState* play) {
    if (!gCustomItemState.lanternSwinging)
        return;
    if (gCustomItemState.lanternCatchState != 0)
        return; // In catch/message state — no collider
    u8 fireType = gCustomItemState.lanternFireType;
    if (fireType == LANTERN_FIRE_NONE)
        return;

    s32 frame = gCustomItemState.lanternSwingFrame;

    // Only active collider during swing active frames
    if (frame >= LANTERN_CATCH_START && frame <= LANTERN_CATCH_END) {
        Lantern_InitSwingCollider(p, play);

        // Same per-fire rules as the summoned flames; sFireConfig always keeps
        // DMG_ARROW_FIRE so every fire type can light a torch.
        sSwingCol.info.toucher.dmgFlags = sFireConfig[fireType].dmgFlags;
        sSwingCol.info.toucher.damage = 0;

        // Collider follows arc in front of Link
        s16 yaw = p->actor.shape.rot.y;
        f32 t = (f32)(frame - LANTERN_CATCH_START) / (f32)(LANTERN_CATCH_END - LANTERN_CATCH_START);
        s16 arcYaw = (s16)(yaw + (s16)(0x3000 * (0.5f - t))); // sweep ±30°
        f32 reach = 25.0f;

        Vec3f tipPos;
        tipPos.x = p->actor.world.pos.x + Math_SinS(arcYaw) * reach;
        tipPos.y = p->actor.world.pos.y + 35.0f;
        tipPos.z = p->actor.world.pos.z + Math_CosS(arcYaw) * reach;

        sSwingCol.dim.pos.x = (s16)tipPos.x;
        sSwingCol.dim.pos.y = (s16)tipPos.y;
        sSwingCol.dim.pos.z = (s16)tipPos.z;
        CollisionCheck_SetAT(play, &play->colChkCtx, &sSwingCol.base);

        // A torch lit by this swing keeps the lantern's colour
        Lantern_TintTorchesNear(play, &tipPos, LANTERN_TORCH_TINT_RANGE, fireType);

        // Fire trail VFX (scale *.3: was 30, now 9)
        EffectSsEnFire_SpawnVec3f(play, &p->actor, &tipPos, 9, 0, 0, -1);
    }
}

// ── Green Fire Passive Healing ──────────────────────────────────────────────

// Green fire restores health AND magic while Link stands still.
//
// Stillness is measured from how far Link actually MOVED since last frame, not from
// actor.speedXZ/velocity.y: those read differently depending on the state Link is in
// (climbing, riding, being pushed, standing on a moving platform), and a single frame
// of a non-zero reading was enough to reset the counter forever. A position delta
// cannot lie. The green shimmer plays the whole time the counter is charging, so the
// regen is visible before the first tick lands.
static void Lantern_UpdateGreenHeal(Player* p, PlayState* play) {
    static Color_RGBA8 greenPrim = { 80, 255, 120, 255 };
    static Color_RGBA8 greenEnv = { 40, 200, 80, 200 };
    static Vec3f sLastPos = { 0.0f, 0.0f, 0.0f };
    Vec3f vel = { 0.0f, 2.0f, 0.0f };
    Vec3f accel = { 0.0f, -0.1f, 0.0f };
    Vec3f sparkPos;
    f32 dx = p->actor.world.pos.x - sLastPos.x;
    f32 dy = p->actor.world.pos.y - sLastPos.y;
    f32 dz = p->actor.world.pos.z - sLastPos.z;
    u8 still = ((dx * dx) + (dy * dy) + (dz * dz)) < SQ(LANTERN_GREEN_STILL_EPS);

    sLastPos = p->actor.world.pos;

    if (gCustomItemState.lanternFireType != LANTERN_FIRE_GREEN) {
        gCustomItemState.lanternHealTimer = 0;
        return;
    }

    if (!still) {
        gCustomItemState.lanternHealTimer = 0; // moving — start the count over
        return;
    }

    gCustomItemState.lanternHealTimer++;

    // Charging shimmer: a spark every few frames while the warmth builds up
    if ((gCustomItemState.lanternHealTimer % 6) == 0) {
        sparkPos.x = p->actor.world.pos.x + Rand_CenteredFloat(20.0f);
        sparkPos.y = p->actor.world.pos.y + 10.0f + Rand_ZeroFloat(30.0f);
        sparkPos.z = p->actor.world.pos.z + Rand_CenteredFloat(20.0f);
        EffectSsKiraKira_SpawnFocused(play, &sparkPos, &vel, &accel, &greenPrim, &greenEnv, 400, 12);
    }

    if (gCustomItemState.lanternHealTimer >= LANTERN_GREEN_HEAL_RATE) {
        gCustomItemState.lanternHealTimer = 0;
        Health_ChangeBy(play, 4);                                  // 1/4 heart
        Magic_RequestChange(play, LANTERN_GREEN_MAGIC, MAGIC_ADD); // and a sliver of magic

        // Bigger burst on the tick itself
        sparkPos.x = p->actor.world.pos.x + Rand_CenteredFloat(20.0f);
        sparkPos.y = p->actor.world.pos.y + 30.0f + Rand_ZeroFloat(20.0f);
        sparkPos.z = p->actor.world.pos.z + Rand_CenteredFloat(20.0f);
        EffectSsKiraKira_SpawnFocused(play, &sparkPos, &vel, &accel, &greenPrim, &greenEnv, 600, 20);
    }
}

// ── Passive upkeep — runs ALWAYS from CustomItems_Update ────────────────────
// Light and green-fire healing are properties of the FIRE, not of holding the
// item: they used to hang off Handle_Lantern, so taking the lantern off every
// button left a lit light node frozen in mid-air and stopped the healing.
void Lantern_UpdatePassive(PlayState* play) {
    Player* p = GET_PLAYER(play);

    Lantern_UpdateLight(p, play);
    Lantern_UpdateGreenHeal(p, play);
}

// ── Public API ──────────────────────────────────────────────────────────────

// Called from ExtInv_GetItemIcon (extended_inventory.c) to get fire type
// without needing to include custom_items.h from the kaleido unity build.
u8 Lantern_GetFireType(void) {
    return gCustomItemState.lanternFireType;
}

void Player_InitLanternIA(PlayState* play, Player* this) {
    // Nothing special needed on equip
}

void Handle_Lantern(Player* p, PlayState* play) {
    ItemInputState input;
    ItemInput_Update(&input, ITEM_LANTERN, p, play);

    // Fire type is loaded from save at file load (SaveManager LoadBase).
    // Runtime gCustomItemState.lanternFireType is authoritative after that.

    // ── Carrying rule ───────────────────────────────────────────────────
    // A LIT lantern rides in Link's hand for as long as nothing else owns that hand:
    // it is a burning light source, not something you fish out for one swing. That
    // makes "in hand" reachable without swinging first, which is what the shadow
    // fire's lens and the in-hand light both gate on (Lantern_IsInHand).
    // Any other item action — drawing the sword included — puts it away, and so does
    // taking the lantern off every button (handled by the draw pass).
    // Handle_Lantern only runs while the lantern IS on a button, so no extra check here.
    if (p->heldItemAction != PLAYER_IA_LANTERN && p->heldItemAction != PLAYER_IA_NONE) {
        gCustomItemState.lanternEquipped = 0;
    } else if (gCustomItemState.lanternFireType != LANTERN_FIRE_NONE) {
        gCustomItemState.lanternEquipped = 1;
    }

    // Swing collider/VFX only — the light and the green-fire healing are passive and
    // run from Lantern_UpdatePassive (CustomItems_Update) so they survive unequipping.
    Lantern_UpdateSwing(p, play);

    // Poe lens handled by Lantern_UpdateLens (runs from CustomItems_Update, always)

    if (!input.wasEquipped)
        return;
    if (ItemInput_IsBlocked(p, play))
        return;

    // ── Start swing on C-button press ───────────────────────────────────
    // Entire swing/catch/message flow handled by Player_Action_SwingLantern in z_player.c
    if (input.isPressed) {
        extern void Player_StartLanternSwing(Player * this, PlayState * play);
        Player_StartLanternSwing(p, play);
    }
}

s32 Player_UpperAction_Lantern(Player* this, PlayState* play) {
    return 0;
}

// ── Draw ────────────────────────────────────────────────────────────────────

void CustomItems_DrawLantern(Player* p, PlayState* play) {
    Vec3f handPos = p->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    s16 handYaw = p->actor.shape.rot.y;
    u8 fireType = gCustomItemState.lanternFireType;
    u8 lit = ((fireType != LANTERN_FIRE_NONE) && (fireType < LANTERN_FIRE_MAX));
    static u8 sFlameTexScroll = 0;
    f32 flicker;

    sFlameTexScroll++;

    OPEN_DISPS(play->state.gfxCtx);

    // Common transform: hand position, flipped 180° (was upside down), scale 0.4
    Matrix_Translate(handPos.x, handPos.y, handPos.z, MTXMODE_NEW);
    Matrix_RotateY(handYaw * (M_PI / 32768.0f), MTXMODE_APPLY);
    Matrix_RotateX(M_PI, MTXMODE_APPLY);                 // Flip 180° — DL was upside down
    Matrix_Scale(0.004f, 0.004f, 0.004f, MTXMODE_APPLY); // 0.01 * 0.4 = 0.004

    if (lit) {
        // ── LIT: the Poe lantern DL is built to be env-tinted by whatever flame it
        // holds (that is how En_Poh recolours it), so the glass takes the fire colour
        // and pulses with the same flicker the point light uses. ──
        flicker = 0.8f + (Rand_ZeroOne() * 0.2f);

        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPSetEnvColor(POLY_OPA_DISP++, (u8)(sLanternLightColors[fireType][0] * flicker),
                       (u8)(sLanternLightColors[fireType][1] * flicker),
                       (u8)(sLanternLightColors[fireType][2] * flicker), 255);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gPoeLanternDL);

        // ── Actual flame burning inside the lantern, billboarded at the camera and
        // scrolling exactly like a torch flame (same DL, same scroll rate). ──
        Matrix_Translate(handPos.x, handPos.y + LANTERN_FLAME_Y_OFFSET, handPos.z, MTXMODE_NEW);
        Matrix_RotateY((s16)((Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - handYaw) + 0x8000) * (M_PI / 32768.0f),
                       MTXMODE_APPLY);
        Matrix_Scale(LANTERN_FLAME_SCALE, LANTERN_FLAME_SCALE, LANTERN_FLAME_SCALE, MTXMODE_APPLY);

        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0, (sFlameTexScroll * -20) & 0x1FF, 32,
                                      128, 0, 0, 0, -20));
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, sLanternFlamePrim[fireType][0], sLanternFlamePrim[fireType][1],
                        sLanternFlamePrim[fireType][2], 255);
        gDPSetEnvColor(POLY_XLU_DISP++, sLanternFlameEnv[fireType][0], sLanternFlameEnv[fireType][1],
                       sLanternFlameEnv[fireType][2], 0);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    } else {
        // ── UNLIT: Draw semi-transparent, dark tint ──
        Gfx_SetupDL_27Xlu(play->state.gfxCtx);
        gDPSetEnvColor(POLY_XLU_DISP++, 40, 40, 50, 120);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gPoeLanternDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
