/**
 * trirod.h — The Trirod (Somaria chain, level 3). Skijer's NEI.
 *
 * Echoes of Wisdom's Tri Rod in OoT. The v2 design (2026-08-11):
 *
 *  - LEARNING is split from SUMMONING. A separate scan-source table maps N world
 *    actors onto one echo (the Platform is learned from the Deku Tree's sliding
 *    platform OR a lift; the Brazier only from a LIT Syokudai).
 *  - Props are learned by SCANNING (aim + C). Enemies are learned by KILLING them
 *    while the Trirod is the drawn cane — which is what forces the pot-throwing /
 *    dog-summoning loop in a fresh file.
 *  - TWO list modes: COMPRESSED (one echo per distinct effect) and FULL. Extra
 *    rows FOLD onto a core row while compressed: scanning a Wolfos there teaches
 *    "Stalfos" instead.
 *  - Selection is the Sheikah Slate's boxed-icon UI: HOLD L with the rod drawn
 *    and a grid of the miniatures appears (box_menu.c, extended to rows).
 *  - Placement is the pushable block's: a camera-aimed spot in front of Link,
 *    floor-snapped, validity-checked, with the target actor's display list drawn
 *    untextured as the ghost where one exists (miniature billboard otherwise).
 *
 * Compiled by #include from item_cane_of_somaria.c — nothing here is a vcxproj
 * entry. Everything runs inside that TU, after somaria_cubes.c / cane_pacci.c,
 * so the summon-pool helpers and preview globals are in scope on purpose.
 */

#ifndef TRIROD_H
#define TRIROD_H

#include "z64.h"

// ── Costs and limits ─────────────────────────────────────────────────────────
// EoW's triangle economy: echoes cost 1..3 triangles; summons may live up to the
// BUDGET at once, and summoning past it despawns the OLDEST until the new one
// fits — that is EoW behaviour, not an error.
#define TRIROD_BUDGET 6
#define TRIROD_MAX_SUMMONS 8

// The learned mask is two u32 in the save — the table is capped at 64 rows.
#define TRIROD_ECHO_CAP 64

// v2 reordered the table (rows deleted/merged), so saves from v1 carry bits that
// now mean different echoes. On load, anything below this version has its mask
// cleared — same idea as extBootsLayoutVersion.
#define TRIROD_LAYOUT_VERSION 2

// ── Row classification ───────────────────────────────────────────────────────
#define TRIROD_TIER_CORE 0  // in both lists
#define TRIROD_TIER_EXTRA 1 // FULL list only; folds onto `foldsInto` when compressed

#define TRIROD_LEARN_SCAN 0 // aim + C on a matching source actor
#define TRIROD_LEARN_KILL 1 // kill a matching source while the rod is drawn

#define TRIROD_SPAWN_FIXED 0
#define TRIROD_SPAWN_WATER_OR_LAND 1 // altActorId when the spot is NOT in water (Octorok/Mad Scrub)

// Ghost style at the placement spot. All three share the block's aiming and
// validity; they differ only in what is rendered there.
#define TRIROD_PV_ICON 0 // miniature billboard (skeletal actors have no single DL)
#define TRIROD_PV_DL 1   // the actor's own display list, drawn as an untextured ghost
#define TRIROD_PV_CUBE 2 // the block preview cube (Block echo)

// AI module ids — RESERVED. Phase 2+ installs a custom update per echo; until
// then every summon keeps its vanilla behaviour and this field only documents
// the design (see the plan's compressed list).
enum {
    TRIROD_AI_VANILLA = 0,
    TRIROD_AI_INERT,
    TRIROD_AI_ALLY_MELEE,
    TRIROD_AI_KAMIKAZE_ELEM, // the elemental Keese
    TRIROD_AI_BAIT_SWARM,
    TRIROD_AI_BOMB_THROWER,
    TRIROD_AI_SAPPER,
    TRIROD_AI_EXTINGUISHER,
    TRIROD_AI_TURRET,
    TRIROD_AI_STUNNER,
    TRIROD_AI_SMASHER,
    TRIROD_AI_LANCER,
    TRIROD_AI_FIRE_BREATHER,
    TRIROD_AI_SWALLOW,
    TRIROD_AI_EYE_LASER,
    TRIROD_AI_LENS_LIGHT,
    TRIROD_AI_WATER_RIDE,
    TRIROD_AI_FAST_SWIM,
    TRIROD_AI_HEAVY_BREAKER,
    TRIROD_AI_FIRE_AURA,
    TRIROD_AI_ICE_MAGIC,
    TRIROD_AI_PLANT_LIFT,
    TRIROD_AI_DOG,
    TRIROD_AI_LIGHT_KEY,
    TRIROD_AI_FAIRY_CHARGE,
};

// ── One echo (what you SUMMON) ───────────────────────────────────────────────
typedef struct {
    const char* name;
    const char* eowEcho; // Echoes of Wisdom echo this recreates; NULL = OoT-only
    const char* icon;    // 32x32 rgba32 miniature in soh.o2r; NULL = none yet
    u8 cost;             // triangles 1..3
    u8 tier;             // TRIROD_TIER_*
    u8 foldsInto;        // EXTRA only: core row index that absorbs it when compressed
    u8 learn;            // TRIROD_LEARN_*
    u8 ai;               // TRIROD_AI_* (reserved until the AI phase)
    u8 impl;             // 0 = summon not wired yet: learnable, shown grayed, uncastable
    u8 preview;          // TRIROD_PV_*
    u8 spawnRule;        // TRIROD_SPAWN_*
    s16 actorId;
    s16 spawnParams;
    s16 objectId;
    s16 altActorId; // WATER_OR_LAND: the land body (-1 = none)
    s16 altParams;
    s16 altObjectId;
    f32 yOff;         // spawn height above the placement spot (flyers)
    f32 pvScale;      // ghost DL scale (the actor's own draw scale)
    const char* pvDL; // OTR path of the ghost display list (PV_DL only)
} TrirodEcho;

// ── One scan source (what TEACHES it) ────────────────────────────────────────
typedef struct {
    u8 echoIdx;
    s16 actorId;
    // (u16)(target->params) & matchMask inside [matchLo, matchHi]; mask 0 = any.
    u16 matchMask;
    u16 matchLo;
    u16 matchHi;
    // Extra gate beyond id+params (a torch must be LIT), or NULL.
    u8 (*extra)(Actor* actor, PlayState* play);
} TrirodScanSource;

extern const TrirodEcho gTrirodEchoes[];
extern const u8 gTrirodEchoCount;
extern const TrirodScanSource gTrirodScanSources[];
extern const u8 gTrirodScanSourceCount;

// ── API consumed by item_cane_of_somaria.c ───────────────────────────────────
/** Per-frame: aim highlight, hold-L wheel, block-style placement ghost. */
void Trirod_Aim(Player* p, PlayState* play);
/** C press, BEFORE the generic cast. 1 = consumed (learn/dismiss/reject). */
u8 Trirod_OnPress(Player* p, PlayState* play);
/** R steps the selection (L is the wheel hold and never cycles). */
void Trirod_Cycle(Player* p, PlayState* play, s8 dir);
/** Fired by the cast animation at its spawn frame (Cane_FireSkill case 6). */
void Trirod_FireSummon(Player* p, PlayState* play);
/** Ghost at the placement spot. Called from the cane's draw hook. */
void Trirod_DrawPreview(PlayState* play, Player* p);
void Trirod_CleanupPool(void);
/** z_actor.c bridge: an enemy just entered its death blow. Kill-to-learn. */
void Trirod_NotifyEnemyDown(PlayState* play, Actor* actor);

// Save accessors (nei_save.cpp)
u8 Nei_TrirodEchoLearned(u8 idx);
void Nei_TrirodLearnEcho(u8 idx);
u8 Nei_TrirodLearnedCount(void);
u8 Nei_TrirodGetSel(void);
void Nei_TrirodSetSel(u8 idx);
u8 Nei_TrirodFullList(void);
void Nei_TrirodSetFullList(u8 on);
void Nei_TrirodGiveAll(void);
void Nei_TrirodClear(void);
void Nei_TrirodNotify(const char* msg);

// The echo wheel rides on box_menu.c, which custom_items.c unity-includes BEFORE
// the cane chain — its BoxMenuEntry / BoxMenu_* are already defined by the time
// trirod.c compiles. Redeclaring the anonymous-struct typedef here would be an
// incompatible-type error, so nothing is declared: trirod.c just uses them.

#endif // TRIROD_H
