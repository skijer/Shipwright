/**
 * trirod_echoes.inc.c — the echo table (v2) and its scan sources. Skijer's NEI.
 *
 * TWO tables now:
 *   gTrirodEchoes      — what you can SUMMON (one row per echo; row index = save bit)
 *   gTrirodScanSources — what TEACHES each echo (N sources -> 1 echo)
 *
 * ORDER IS THE SAVE FORMAT. A row's index is its bit in trirodEchoesLo/Hi, so
 * gTrirodEchoes is APPEND-ONLY from v2 on: never reorder, never delete — retire a
 * row by removing its scan sources instead. (v1 -> v2 DID reorder, which is why
 * TRIROD_LAYOUT_VERSION exists and old masks are cleared on load.)
 *
 * The compressed/full split: TIER_CORE rows exist in both lists; TIER_EXTRA rows
 * exist only in the FULL list and carry `foldsInto` — while the list is
 * compressed, learning one lights its core row's bit instead (killing a Wolfos
 * teaches "Stalfos"). Sources for EXTRA rows come FIRST in gTrirodScanSources on
 * purpose: the first match wins, and the fold happens at learn time, so one
 * ordering serves both list modes.
 *
 * Learning rules (rando): TRIROD_LEARN_SCAN rows are learned by aiming + C.
 * TRIROD_LEARN_KILL rows are learned by KILLING a source while the rod is drawn
 * (Trirod_NotifyEnemyDown, bridged from Enemy_StartFinishingBlow).
 *
 * `impl == 0` rows are design slots whose summon is not wired yet (Light, Fairy,
 * Bean): learnable and visible in the wheel, drawn grayed, refuse to cast.
 *
 * Ghost previews: PV_DL rows draw the named OTR display list untextured at the
 * placement spot (soh DL symbols are self-contained resource paths — no object
 * bank or segment setup involved). pvScale values are the actors' own draw
 * scales where known and eyeballed otherwise — tune in-game. Skeletal actors
 * have no single body DL, so they fall back to the miniature billboard.
 */

#define ECHO_ICON(sym) "__OTR__textures/trirod/" sym
#define ECHO_ANY 0, 0, 0 // matchMask, matchLo, matchHi: any params
#define ECHO_NO_ALT -1, 0, -1
#define ECHO_NO_DL 0.0f, NULL

// clang-format off
const TrirodEcho gTrirodEchoes[] = {
    // ── CORE: props (learned by scanning) ───────────────────────────────────
    /* 0 Pot */
    { "Pot", "Pot", ECHO_ICON("gTrirodEchoObjTsuboTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_DL, TRIROD_SPAWN_FIXED,
      ACTOR_OBJ_TSUBO, 0, OBJECT_GAMEPLAY_KEEP, ECHO_NO_ALT, 0.0f, 0.15f,
      "__OTR__objects/gameplay_dangeon_keep/gPotDL" },
    /* 1 Crate */
    { "Crate", "Crate", ECHO_ICON("gTrirodEchoObjKibako2Tex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_DL, TRIROD_SPAWN_FIXED,
      ACTOR_OBJ_KIBAKO2, 0, OBJECT_KIBAKO2, ECHO_NO_ALT, 0.0f, 0.1f,
      "__OTR__objects/object_kibako2/gLargeCrateDL" },
    /* 2 Rock */
    { "Rock", "Rock", ECHO_ICON("gTrirodEchoEnIshiTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_DL, TRIROD_SPAWN_FIXED,
      ACTOR_EN_ISHI, 0, OBJECT_GAMEPLAY_FIELD_KEEP, ECHO_NO_ALT, 0.0f, 0.4f,
      "__OTR__objects/gameplay_field_keep/gFieldKakeraDL" },
    /* 3 Armos Statue (ARMOS_STATUE = 0: the pushable prop; the enemy is row 43) */
    { "Armos Statue", NULL, ECHO_ICON("gTrirodEchoEnAmTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_AM, 0, OBJECT_AM, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 4 Block — the Somaria pushable block; summon routes through CaneSummon_Spawn
       so it inherits the env-colour draw fix and switch-flag-free params. */
    { "Block", NULL, ECHO_ICON("gTrirodEchoObjOshihikiTex"), 2, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_CUBE, TRIROD_SPAWN_FIXED,
      ACTOR_OBJ_OSHIHIKI, 0, OBJECT_GAMEPLAY_DANGEON_KEEP, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 5 Platform — the Somaria slab, same routing. */
    { "Platform", NULL, ECHO_ICON("gTrirodEchoObjLiftTex"), 2, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_CUBE, TRIROD_SPAWN_FIXED,
      ACTOR_OBJ_LIFT, 0, OBJECT_D_LIFT, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 6 Bean — needs the pathless Obj_Bean rework (it self-destructs without a
       scene path and its Move overwrites world.pos from pathPoints every frame). */
    { "Bean", NULL, ECHO_ICON("gTrirodEchoObjBeanTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_PLANT_LIFT, 0, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_OBJ_BEAN, 0, OBJECT_MAMENOKI, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 7 Brazier — 0x2400: wooden torch, lit, no switch flag. Only a LIT torch
       teaches it (Trirod_SyokudaiIsLit source gate). */
    { "Brazier", "Brazier", ECHO_ICON("gTrirodEchoObjSyokudaiTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_FIRE_AURA, 1, TRIROD_PV_DL, TRIROD_SPAWN_FIXED,
      ACTOR_OBJ_SYOKUDAI, 0x2400, OBJECT_SYOKUDAI, ECHO_NO_ALT, 0.0f, 1.0f,
      "__OTR__objects/object_syokudai/gWoodenTorchDL" },
    /* 8 Light — the anti-Ganon key: Bigmirror-style light over Link. Design slot. */
    { "Light", NULL, ECHO_ICON("gTrirodEchoBgJyaBigmirrorTex"), 3, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_LIGHT_KEY, 0, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_BG_JYA_BIGMIRROR, 0, OBJECT_GAMEPLAY_KEEP, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 9 Fairy — spirit charger for the Keese (possession). Design slot: spawning a
       vanilla fairy today is a free heal, so it stays gated until the AI phase. */
    { "Fairy", NULL, ECHO_ICON("gTrirodEchoEnElfTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_FAIRY_CHARGE, 0, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_ELF, 0, OBJECT_GAMEPLAY_KEEP, ECHO_NO_ALT, 40.0f, ECHO_NO_DL },
    /* 10 Bomb Flower — BOMBFLOWER_FLOWER (-1): the regrowing plant. */
    { "Bomb Flower", NULL, ECHO_ICON("gTrirodEchoEnBombfTex"), 3, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_BOMBF, -1, OBJECT_BOMBF, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 11 Fish — FISH_DROPPED (0), the bottle-release body: the one Jabu-Jabu's
       cutscene watches for. */
    { "Fish", NULL, ECHO_ICON("gTrirodEchoEnFishTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_FISH, 0, OBJECT_GAMEPLAY_KEEP, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 12 Dog — params 0x8000 = "already following the player". NPC, so it scans. */
    { "Dog", NULL, ECHO_ICON("gTrirodEchoEnDogTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_DOG, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_DOG, 0x8000, OBJECT_DOG, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 13 Cucco — scanned, not killed: a cucco never dies. */
    { "Cucco", NULL, ECHO_ICON("gTrirodEchoEnNiwTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_BAIT_SWARM, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_NIW, 0, OBJECT_NIW, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },

    // ── CORE: creatures (learned by killing with the rod drawn) ─────────────
    /* 14 Keese — THE elemental vehicle (wind base; fire/ice/shadow/light/spirit
       by flying to a source first). One echo for every keese and guay. */
    { "Keese", "Keese", ECHO_ICON("gTrirodEchoEnFireflyTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_KAMIKAZE_ELEM, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_FIREFLY, 2, OBJECT_FIREFLY, ECHO_NO_ALT, 60.0f, ECHO_NO_DL },
    /* 15 Stalchild */
    { "Stalchild", NULL, ECHO_ICON("gTrirodEchoEnSkbTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_BOMB_THROWER, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_SKB, 0, OBJECT_SKB, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 16 Baby Dodongo */
    { "Baby Dodongo", NULL, ECHO_ICON("gTrirodEchoEnDodojrTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_SAPPER, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_DODOJR, 0, OBJECT_DODOJR, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 17 Shabom */
    { "Shabom", NULL, ECHO_ICON("gTrirodEchoEnBubbleTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_EXTINGUISHER, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_BUBBLE, 0, OBJECT_BUBBLE, ECHO_NO_ALT, 40.0f, ECHO_NO_DL },
    /* 18 Deku Baba */
    { "Deku Baba", NULL, ECHO_ICON("gTrirodEchoEnDekubabaTex"), 2, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_VANILLA, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_DEKUBABA, 0, OBJECT_DEKUBABA, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 19 Octorok / Mad Scrub — ONE echo, two bodies: water spot -> Octorok,
       dry spot -> Mad Scrub. */
    { "Octorok", "Octorok", ECHO_ICON("gTrirodEchoEnOkutaTex"), 2, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_TURRET, 1, TRIROD_PV_ICON, TRIROD_SPAWN_WATER_OR_LAND,
      ACTOR_EN_OKUTA, 0, OBJECT_OKUTA, ACTOR_EN_DEKUNUTS, 0, OBJECT_DEKUNUTS, 0.0f, ECHO_NO_DL },
    /* 20 Biri */
    { "Biri", "Zol", ECHO_ICON("gTrirodEchoEnBiliTex"), 1, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_VANILLA, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_BILI, 0, OBJECT_BL, ECHO_NO_ALT, 40.0f, ECHO_NO_DL },
    /* 21 Tektite — one echo (red+blue merged); summons the BLUE body because that
       is the one that skates on water. */
    { "Tektite", "Tektite", ECHO_ICON("gTrirodEchoEnTiteTex"), 2, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_WATER_RIDE, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_TITE, -2, OBJECT_TITE, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 22 Stinger — params 10 = lone wanderer. (En_Eiyer is the land/air body; the
       true water Stinger is the Weiyer extra, row 42.) */
    { "Stinger", NULL, ECHO_ICON("gTrirodEchoEnEiyerTex"), 2, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_FAST_SWIM, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_EIYER, 10, OBJECT_EI, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 23 Poe */
    { "Poe", "Ghini", ECHO_ICON("gTrirodEchoEnPohTex"), 2, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_LENS_LIGHT, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_POH, 0, OBJECT_POH, ECHO_NO_ALT, 30.0f, ECHO_NO_DL },
    /* 24 Beamos */
    { "Beamos", NULL, ECHO_ICON("gTrirodEchoEnVmTex"), 3, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_EYE_LASER, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_VM, 0, OBJECT_VM, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 25 Like Like */
    { "Like Like", NULL, ECHO_ICON("gTrirodEchoEnRrTex"), 2, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_SWALLOW, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_RR, 0, OBJECT_RR, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 26 Freezard */
    { "Freezard", NULL, ECHO_ICON("gTrirodEchoEnFzTex"), 2, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_ICE_MAGIC, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_FZ, 0, OBJECT_FZ, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 27 ReDead */
    { "ReDead", NULL, ECHO_ICON("gTrirodEchoEnRdTex"), 3, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_STUNNER, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_RD, 0, OBJECT_RD, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 28 Moblin (club) — ENMB_TYPE_CLUB (0). */
    { "Moblin", "Moblin", ECHO_ICON("gTrirodEchoEnMbTex"), 3, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_SMASHER, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_MB, 0, OBJECT_MB, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 29 Moblin (spear) — ENMB_TYPE_SPEAR_GUARD (-1), the pathless spear type.
       (SPEAR_PATROL follows scene paths and would crash summoned.) */
    { "Spear Moblin", NULL, ECHO_ICON("gTrirodEchoEnMbTex"), 3, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_LANCER, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_MB, -1, OBJECT_MB, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 30 Dodongo — charges and breathes flame (AI phase); vanilla body meanwhile. */
    { "Dodongo", NULL, ECHO_ICON("gTrirodEchoEnDodongoTex"), 2, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_FIRE_BREATHER, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_DODONGO, 0, OBJECT_DODONGO, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 31 Stalfos — STALFOS_TYPE_2 (type 0 is the INVISIBLE one). */
    { "Stalfos", NULL, ECHO_ICON("gTrirodEchoEnTestTex"), 3, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_ALLY_MELEE, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_TEST, 2, OBJECT_SK2, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 32 Iron Knuckle — params 1..3 are the armoured enemies; 0 is NABOORU. */
    { "Iron Knuckle", "Darknut", ECHO_ICON("gTrirodEchoEnIkTex"), 3, TRIROD_TIER_CORE, 0, TRIROD_LEARN_KILL,
      TRIROD_AI_HEAVY_BREAKER, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_IK, 2, OBJECT_IK, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },

    // ── EXTRA: full list only; folds onto its core row when compressed ──────
    /* 33 Sign -> Pot */
    { "Sign", NULL, ECHO_ICON("gTrirodEchoEnKanbanTex"), 1, TRIROD_TIER_EXTRA, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_DL, TRIROD_SPAWN_FIXED,
      ACTOR_EN_KANBAN, 0, OBJECT_KANBAN, ECHO_NO_ALT, 0.0f, 0.01f,
      "__OTR__objects/gameplay_keep/gSignRectangularDL" },
    /* 34 Grass -> Pot */
    { "Grass", NULL, ECHO_ICON("gTrirodEchoEnKusaTex"), 1, TRIROD_TIER_EXTRA, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_DL, TRIROD_SPAWN_FIXED,
      ACTOR_EN_KUSA, 0, OBJECT_GAMEPLAY_KEEP, ECHO_NO_ALT, 0.0f, 0.4f,
      "__OTR__objects/gameplay_field_keep/gFieldBushDL" },
    /* 35 Small Crate -> Crate */
    { "Small Crate", NULL, ECHO_ICON("gTrirodEchoObjKibakoTex"), 1, TRIROD_TIER_EXTRA, 1, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_DL, TRIROD_SPAWN_FIXED,
      ACTOR_OBJ_KIBAKO, 0, OBJECT_GAMEPLAY_DANGEON_KEEP, ECHO_NO_ALT, 0.0f, 0.15f,
      "__OTR__objects/gameplay_dangeon_keep/gSmallWoodenBoxDL" },
    /* 36 Boulder -> Rock (ROCK_LARGE = 1, the silver strength rock) */
    { "Boulder", "Boulder", ECHO_ICON("gTrirodEchoEnIshiTex"), 2, TRIROD_TIER_EXTRA, 2, TRIROD_LEARN_SCAN,
      TRIROD_AI_INERT, 1, TRIROD_PV_DL, TRIROD_SPAWN_FIXED,
      ACTOR_EN_ISHI, 1, OBJECT_GAMEPLAY_FIELD_KEEP, ECHO_NO_ALT, 0.0f, 0.5f,
      "__OTR__objects/gameplay_field_keep/gSilverRockDL" },
    /* 37 Flying Pot -> Pot */
    { "Flying Pot", "Flying Tile", ECHO_ICON("gTrirodEchoEnTuboTrapTex"), 1, TRIROD_TIER_EXTRA, 0, TRIROD_LEARN_SCAN,
      TRIROD_AI_VANILLA, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_TUBO_TRAP, 0, OBJECT_GAMEPLAY_DANGEON_KEEP, ECHO_NO_ALT, 40.0f, ECHO_NO_DL },
    /* 38 Leever -> Tektite */
    { "Leever", NULL, ECHO_ICON("gTrirodEchoEnReebaTex"), 1, TRIROD_TIER_EXTRA, 21, TRIROD_LEARN_KILL,
      TRIROD_AI_VANILLA, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_REEBA, 0, OBJECT_REEBA, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 39 Guay -> Keese */
    { "Guay", "Crow", ECHO_ICON("gTrirodEchoEnCrowTex"), 1, TRIROD_TIER_EXTRA, 14, TRIROD_LEARN_KILL,
      TRIROD_AI_KAMIKAZE_ELEM, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_CROW, 0, OBJECT_CROW, ECHO_NO_ALT, 60.0f, ECHO_NO_DL },
    /* 40 Bubble -> Keese */
    { "Bubble", NULL, ECHO_ICON("gTrirodEchoEnBbTex"), 1, TRIROD_TIER_EXTRA, 14, TRIROD_LEARN_KILL,
      TRIROD_AI_KAMIKAZE_ELEM, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_BB, -2, OBJECT_BB, ECHO_NO_ALT, 40.0f, ECHO_NO_DL },
    /* 41 Peahat -> Keese */
    { "Peahat", "Peahat", ECHO_ICON("gTrirodEchoEnPeehatTex"), 2, TRIROD_TIER_EXTRA, 14, TRIROD_LEARN_KILL,
      TRIROD_AI_VANILLA, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_PEEHAT, -1, OBJECT_PEEHAT, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 42 Weiyer -> Stinger (the REAL water body) */
    { "Weiyer", NULL, ECHO_ICON("gTrirodEchoEnWeiyerTex"), 2, TRIROD_TIER_EXTRA, 22, TRIROD_LEARN_KILL,
      TRIROD_AI_FAST_SWIM, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_WEIYER, 0, OBJECT_EI, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 43 Armos (enemy) -> Stalfos (ARMOS_ENEMY = 1) */
    { "Armos", NULL, ECHO_ICON("gTrirodEchoEnAmTex"), 2, TRIROD_TIER_EXTRA, 31, TRIROD_LEARN_KILL,
      TRIROD_AI_ALLY_MELEE, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_AM, 1, OBJECT_AM, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 44 Wolfos -> Stalfos */
    { "Wolfos", NULL, ECHO_ICON("gTrirodEchoEnWfTex"), 3, TRIROD_TIER_EXTRA, 31, TRIROD_LEARN_KILL,
      TRIROD_AI_ALLY_MELEE, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_WF, 0, OBJECT_WF, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 45 Lizalfos -> Stalfos (-1 = LIZALFOS_LONE, not a miniboss pair) */
    { "Lizalfos", "Lizalfos", ECHO_ICON("gTrirodEchoEnZfTex"), 3, TRIROD_TIER_EXTRA, 31, TRIROD_LEARN_KILL,
      TRIROD_AI_ALLY_MELEE, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_ZF, -1, OBJECT_ZF, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 46 Dinolfos -> Stalfos */
    { "Dinolfos", NULL, ECHO_ICON("gTrirodEchoEnZfTex"), 3, TRIROD_TIER_EXTRA, 31, TRIROD_LEARN_KILL,
      TRIROD_AI_ALLY_MELEE, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_ZF, -2, OBJECT_ZF, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 47 Gibdo -> ReDead (params -2 is the Gibdo skeleton; SAME stun effect — the
       requested example of a flavour duplicate) */
    { "Gibdo", "Gibdo", ECHO_ICON("gTrirodEchoEnRdTex"), 3, TRIROD_TIER_EXTRA, 27, TRIROD_LEARN_KILL,
      TRIROD_AI_STUNNER, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_RD, -2, OBJECT_RD, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 48 Bari -> Biri */
    { "Bari", NULL, ECHO_ICON("gTrirodEchoEnValiTex"), 2, TRIROD_TIER_EXTRA, 20, TRIROD_LEARN_KILL,
      TRIROD_AI_VANILLA, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_VALI, 0, OBJECT_VALI, ECHO_NO_ALT, 40.0f, ECHO_NO_DL },
    /* 49 Shell Blade -> Tektite */
    { "Shell Blade", NULL, ECHO_ICON("gTrirodEchoEnSbTex"), 2, TRIROD_TIER_EXTRA, 21, TRIROD_LEARN_KILL,
      TRIROD_AI_VANILLA, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_SB, 0, OBJECT_SB, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 50 Spike -> Tektite */
    { "Spike", "Caromadillo", ECHO_ICON("gTrirodEchoEnNyTex"), 2, TRIROD_TIER_EXTRA, 21, TRIROD_LEARN_KILL,
      TRIROD_AI_VANILLA, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_NY, 0, OBJECT_NY, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
    /* 51 Torch Slug -> Brazier (the brazier with legs) */
    { "Torch Slug", NULL, ECHO_ICON("gTrirodEchoEnBwTex"), 2, TRIROD_TIER_EXTRA, 7, TRIROD_LEARN_KILL,
      TRIROD_AI_FIRE_AURA, 1, TRIROD_PV_ICON, TRIROD_SPAWN_FIXED,
      ACTOR_EN_BW, 0, OBJECT_BW, ECHO_NO_ALT, 0.0f, ECHO_NO_DL },
};
// clang-format on

const u8 gTrirodEchoCount = (u8)ARRAY_COUNT(gTrirodEchoes);

_Static_assert(ARRAY_COUNT(gTrirodEchoes) <= TRIROD_ECHO_CAP, "gTrirodEchoes no longer fits the 64-bit learned mask");

// ── Source gates ─────────────────────────────────────────────────────────────

// Only a LIT torch teaches the Brazier. litTimer: 0 unlit, <0 permanent, >0 timed.
static u8 Trirod_SyokudaiIsLit(Actor* actor, PlayState* play) {
    (void)play;
    return ((ObjSyokudai*)actor)->litTimer != 0;
}

// ── Scan sources ─────────────────────────────────────────────────────────────
// EXTRA-row sources FIRST: the first match wins, and while the list is
// compressed the learn folds the row onto its core — one ordering serves both
// modes. Rows sharing an actor id rely on disjoint param ranges (documented per
// actor in the echo rows above).
// clang-format off
const TrirodScanSource gTrirodScanSources[] = {
    // extras
    { 33, ACTOR_EN_KANBAN, ECHO_ANY, NULL },
    { 34, ACTOR_EN_KUSA, ECHO_ANY, NULL },
    { 35, ACTOR_OBJ_KIBAKO, ECHO_ANY, NULL },
    { 36, ACTOR_EN_ISHI, 0x0001, 1, 1, NULL },
    { 37, ACTOR_EN_TUBO_TRAP, ECHO_ANY, NULL },
    { 38, ACTOR_EN_REEBA, 0x0001, 0, 0, NULL },
    { 39, ACTOR_EN_CROW, ECHO_ANY, NULL },
    { 40, ACTOR_EN_BB, ECHO_ANY, NULL },
    { 41, ACTOR_EN_PEEHAT, ECHO_ANY, NULL },
    { 42, ACTOR_EN_WEIYER, ECHO_ANY, NULL },
    { 43, ACTOR_EN_AM, 0x0001, 1, 1, NULL },
    { 44, ACTOR_EN_WF, ECHO_ANY, NULL },
    { 45, ACTOR_EN_ZF, 0xFFFF, 0xFFFF, 0xFFFF, NULL },
    { 46, ACTOR_EN_ZF, 0xFFFF, 0xFFFE, 0xFFFE, NULL },
    { 47, ACTOR_EN_RD, 0x00FF, 0xFE, 0xFE, NULL },
    { 48, ACTOR_EN_VALI, ECHO_ANY, NULL },
    { 49, ACTOR_EN_SB, ECHO_ANY, NULL },
    { 50, ACTOR_EN_NY, ECHO_ANY, NULL },
    { 51, ACTOR_EN_BW, ECHO_ANY, NULL },

    // core props
    { 0, ACTOR_OBJ_TSUBO, ECHO_ANY, NULL },
    { 1, ACTOR_OBJ_KIBAKO2, ECHO_ANY, NULL },
    { 2, ACTOR_EN_ISHI, 0x0001, 0, 0, NULL },
    { 3, ACTOR_EN_AM, 0x0001, 0, 0, NULL },
    { 4, ACTOR_OBJ_OSHIHIKI, ECHO_ANY, NULL }, // ANY pushable block — scene ones included
    { 5, ACTOR_BG_YDAN_HASI, ECHO_ANY, NULL }, // the Deku Tree sliding platform
    { 5, ACTOR_OBJ_LIFT, ECHO_ANY, NULL },
    { 5, ACTOR_BG_JYA_LIFT, ECHO_ANY, NULL },
    { 5, ACTOR_BG_MORI_ELEVATOR, ECHO_ANY, NULL },
    { 6, ACTOR_OBJ_BEAN, ECHO_ANY, NULL },
    { 7, ACTOR_OBJ_SYOKUDAI, ECHO_ANY, Trirod_SyokudaiIsLit },
    { 8, ACTOR_BG_JYA_BIGMIRROR, ECHO_ANY, NULL },
    { 8, ACTOR_BG_JYA_COBRA, ECHO_ANY, NULL },
    { 9, ACTOR_EN_ELF, ECHO_ANY, NULL },
    { 10, ACTOR_EN_BOMBF, ECHO_ANY, NULL },
    { 11, ACTOR_EN_FISH, ECHO_ANY, NULL },
    { 12, ACTOR_EN_DOG, ECHO_ANY, NULL },
    { 13, ACTOR_EN_NIW, ECHO_ANY, NULL },

    // core creatures (kill-to-learn)
    { 14, ACTOR_EN_FIREFLY, ECHO_ANY, NULL },
    { 15, ACTOR_EN_SKB, ECHO_ANY, NULL },
    { 16, ACTOR_EN_DODOJR, ECHO_ANY, NULL },
    { 17, ACTOR_EN_BUBBLE, ECHO_ANY, NULL },
    { 18, ACTOR_EN_DEKUBABA, ECHO_ANY, NULL },
    { 18, ACTOR_EN_KAREBABA, ECHO_ANY, NULL }, // the withered one teaches the same plant
    { 19, ACTOR_EN_OKUTA, ECHO_ANY, NULL },
    { 19, ACTOR_EN_DEKUNUTS, ECHO_ANY, NULL },
    { 20, ACTOR_EN_BILI, ECHO_ANY, NULL },
    { 21, ACTOR_EN_TITE, ECHO_ANY, NULL },
    { 22, ACTOR_EN_EIYER, ECHO_ANY, NULL },
    { 23, ACTOR_EN_POH, 0x00FF, 0, 1, NULL },
    { 24, ACTOR_EN_VM, ECHO_ANY, NULL },
    { 25, ACTOR_EN_RR, ECHO_ANY, NULL },
    { 26, ACTOR_EN_FZ, ECHO_ANY, NULL },
    { 27, ACTOR_EN_RD, 0x00FF, 0, 0x7F, NULL },
    { 28, ACTOR_EN_MB, 0xFFFF, 0, 0, NULL },
    { 29, ACTOR_EN_MB, 0xFFFF, 0xFFFF, 0xFFFF, NULL },
    { 30, ACTOR_EN_DODONGO, ECHO_ANY, NULL },
    { 31, ACTOR_EN_TEST, ECHO_ANY, NULL },
    { 32, ACTOR_EN_IK, 0x00FF, 1, 3, NULL },
};
// clang-format on

const u8 gTrirodScanSourceCount = (u8)ARRAY_COUNT(gTrirodScanSources);
