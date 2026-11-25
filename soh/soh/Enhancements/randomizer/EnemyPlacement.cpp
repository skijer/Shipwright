#include "EnemyPlacement.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <sstream>

#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/window/Window.h>

#include "soh/cvar_prefixes.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/ShipUtils.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/dungeon.h"
#include "soh/ModApi/RandoItems/RandoItems.h"
#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/UIWidgets.hpp"

extern "C" {
#include "z64.h"
#include "variables.h"
}

namespace {

#define CVAR_ENEMY_POOL_EXCLUDED CVAR_RANDOMIZER_SETTING("EnemyPoolExcluded")
#define CVAR_SOUL_POOL_EXCLUDED CVAR_RANDOMIZER_SETTING("SoulPoolExcluded")

constexpr const char* kSaveSection = "enemyPlacement";
constexpr uint64_t kPlacementStream = 0x456E656D79526E64;

struct NativeRow {
    const char* key;
    const char* name;
    int16_t actorId;
    int16_t params;
    int32_t logicEnemy;
    const char* soulKey;
    uint32_t flags;
    const char* enhancementCvar;
};

// Host-only: stands on the water surface as well as on land (blue Tektites).
constexpr uint32_t ENEMY_SURFACE_WALKER = 1u << 16;

#define MINIBOSS SOH_ACTOR_ENEMY_MINIBOSS
#define UNDERWATER SOH_ACTOR_ENEMY_UNDERWATER
#define AQUATIC (SOH_ACTOR_ENEMY_NEEDS_WATER | SOH_ACTOR_ENEMY_UNDERWATER)
#define SURFACE SOH_ACTOR_ENEMY_NEEDS_WATER
#define LIST(name) CVAR_ENHANCEMENT("RandomizedEnemyList." name)

// clang-format off
const NativeRow kNativeRows[] = {
    { "oot.anubis",                "Anubis",                ACTOR_EN_ANUBICE_TAG, 1,                          RE_ANUBIS,             "oot.anubis",       0,        LIST("Anubis") },
    { "oot.armos",                 "Armos",                 ACTOR_EN_AM,          -1,                         RE_ARMOS,              "oot.armos",        0,        LIST("Armos") },
    { "oot.arwing",                "Arwing",                ACTOR_EN_CLEAR_TAG,   1,                          kNoLogicEnemy,         nullptr,            0,        LIST("Arwing") },
    { "oot.baby_dodongo",          "Baby Dodongo",          ACTOR_EN_DODOJR,      0,                          RE_DODONGO,            "oot.dodongo",      0,        LIST("BabyDodongo") },
    { "oot.bari",                  "Bari",                  ACTOR_EN_VALI,        -1,                         RE_BARI,               "oot.bari",         UNDERWATER, LIST("Bari") },
    { "oot.beamos",                "Beamos",                ACTOR_EN_VM,          1280,                       RE_BEAMOS,             "oot.beamos",       0,        LIST("Beamos") },
    { "oot.skulltula_big",         "Big Skulltula",         ACTOR_EN_ST,          1,                          RE_BIG_SKULLTULA,      "oot.skulltula",    0,        LIST("BigSkulltula") },
    { "oot.stalchild_big",         "Stalchild (Big)",       ACTOR_EN_SKB,         20,                         RE_STALCHILD,          "oot.stalchild",    0,        LIST("BigStalchild") },
    { "oot.biri",                  "Biri",                  ACTOR_EN_BILI,        0,                          RE_BARI,               "oot.bari",         UNDERWATER, LIST("Biri") },
    { "oot.iron_knuckle_black",    "Iron Knuckle (Black)",  ACTOR_EN_IK,          2,                          RE_IRON_KNUCKLE,       "oot.iron_knuckle", MINIBOSS, LIST("BlackKnuckle") },
    { "oot.tektite_blue",          "Blue Tektite",          ACTOR_EN_TITE,        -2,                         RE_TEKTITE,            "oot.tektite",      ENEMY_SURFACE_WALKER, LIST("BlueTektite") },
    { "oot.bubble_blue",           "Bubble",                ACTOR_EN_BB,          -1,                         RE_BLUE_BUBBLE,        "oot.bubble",       0,        LIST("Bubble") },
    { "oot.moblin_club",           "Club Moblin",           ACTOR_EN_MB,          0,                          kNoLogicEnemy,         nullptr,            0,        LIST("ClubMoblin") },
    { "oot.dark_link",             "Dark Link",             ACTOR_EN_TORCH2,      0,                          RE_DARK_LINK,          "oot.dark_link",    MINIBOSS, LIST("DarkLink") },
    { "oot.dinolfos",              "Dinolfos",              ACTOR_EN_ZF,          -2,                         RE_DINOLFOS,           "oot.lizalfos",     0,        LIST("Dinolfos") },
    { "oot.dodongo",               "Dodongo",               ACTOR_EN_DODONGO,     -1,                         RE_DODONGO,            "oot.dodongo",      0,        LIST("Dodongo") },
    { "oot.keese_fire",            "Fire Keese",            ACTOR_EN_FIREFLY,     1,                          RE_FIRE_KEESE,         "oot.keese",        0,        LIST("FireKeese") },
    { "oot.flare_dancer",          "Flare Dancer",          ACTOR_EN_FD,          0,                          RE_FLARE_DANCER,       "oot.flare_dancer", MINIBOSS, LIST("FlareDancer") },
    { "oot.floor_tile",            "Floor Tile",            ACTOR_EN_YUKABYUN,    0,                          kNoLogicEnemy,         nullptr,            0,        LIST("FloorTile") },
    { "oot.floormaster",           "Floormaster",           ACTOR_EN_FLOORMAS,    0,                          RE_FLOORMASTER,        "oot.wallmaster",   0,        LIST("Floormaster") },
    { "oot.peahat_flying",         "Flying Peahat",         ACTOR_EN_PEEHAT,      -1,                         RE_PEAHAT,             "oot.peahat",       0,        LIST("FlyingPeahat") },
    { "oot.flying_pot",            "Flying Pot",            ACTOR_EN_TUBO_TRAP,   0,                          kNoLogicEnemy,         nullptr,            0,        LIST("FlyingPot") },
    { "oot.freezard",              "Freezard",              ACTOR_EN_FZ,          0,                          RE_FREEZARD,           "oot.freezard",     0,        LIST("Freezard") },
    { "oot.gerudo_fighter",        "Gerudo Thief",          ACTOR_EN_GELDB,       0,                          RE_GERUDO_WARRIOR,     "oot.gerudo_fighter", MINIBOSS, LIST("GerudoFighter") },
    { "oot.gibdo",                 "Gibdo",                 ACTOR_EN_RD,          32766,                      RE_GIBDO,              "oot.redead",       0,        LIST("Gibdo") },
    { "oot.gohma_larva",           "Gohma Larva",           ACTOR_EN_GOMA,        7,                          RE_GOHMA_LARVA,        "oot.gohma_larva",  0,        LIST("GohmaLarva") },
    { "oot.guay",                  "Guay",                  ACTOR_EN_CROW,        0,                          RE_GUAY,               "oot.guay",         0,        LIST("Guay") },
    { "oot.keese_ice",             "Ice Keese",             ACTOR_EN_FIREFLY,     4,                          RE_KEESE,              "oot.keese",        0,        LIST("IceKeese") },
    { "oot.keese_fire_invisible",  "Invisible Fire Keese",  ACTOR_EN_FIREFLY,     static_cast<int16_t>(0x8001), RE_FIRE_KEESE,       "oot.keese",        0,        LIST("InvisFireKeese") },
    { "oot.floormaster_invisible", "Invisible Floormaster", ACTOR_EN_FLOORMAS,    static_cast<int16_t>(0x8000), RE_FLOORMASTER,      "oot.wallmaster",   0,        LIST("InvisFloormaster") },
    { "oot.keese_ice_invisible",   "Invisible Ice Keese",   ACTOR_EN_FIREFLY,     static_cast<int16_t>(0x8004), RE_KEESE,            "oot.keese",        0,        LIST("InvisIceKeese") },
    { "oot.keese_invisible",       "Invisible Keese",       ACTOR_EN_FIREFLY,     static_cast<int16_t>(0x8002), RE_KEESE,            "oot.keese",        0,        LIST("InvisKeese") },
    { "oot.redead_invisible",      "Invisible Redead",      ACTOR_EN_RD,          3,                          RE_REDEAD,             "oot.redead",       0,        LIST("InvisRedead") },
    { "oot.skulltula_invisible",   "Invisible Skulltula",   ACTOR_EN_ST,          2,                          RE_BIG_SKULLTULA,      "oot.skulltula",    0,        LIST("InvisSkulltula") },
    { "oot.stalfos_invisible",     "Invisible Stalfos",     ACTOR_EN_TEST,        0,                          RE_STALFOS,            "oot.stalfos",      0,        LIST("InvisStalfos") },
    { "oot.keese",                 "Keese",                 ACTOR_EN_FIREFLY,     2,                          RE_KEESE,              "oot.keese",        0,        LIST("Keese") },
    { "oot.deku_baba_large",       "Large Deku Baba",       ACTOR_EN_DEKUBABA,    1,                          RE_DEKU_BABA,          "oot.deku_baba",    0,        LIST("LargeBaba") },
    { "oot.like_like",             "Like-Like",             ACTOR_EN_RR,          0,                          RE_LIKE_LIKE,          "oot.like_like",    0,        LIST("LikeLike") },
    { "oot.lizalfos",              "Lizalfos",              ACTOR_EN_ZF,          -1,                         RE_LIZALFOS,           "oot.lizalfos",     0,        LIST("Lizalfos") },
    { "oot.mad_scrub",             "Mad Scrub",             ACTOR_EN_DEKUNUTS,    768,                        RE_MAD_SCRUB,          "oot.mad_scrub",    0,        LIST("MadScrub") },
    { "oot.wolfos",                "Wolfos (Normal)",       ACTOR_EN_WF,          0,                          RE_WOLFOS,             "oot.wolfos",       0,        LIST("NormalWolfos") },
    { "oot.peahat_larva",          "Peahat Larva",          ACTOR_EN_PEEHAT,      1,                          RE_PEAHAT,             "oot.peahat",       0,        LIST("PeahatLarva") },
    { "oot.redead",                "Redead",                ACTOR_EN_RD,          1,                          RE_REDEAD,             "oot.redead",       0,        LIST("Redead") },
    { "oot.tektite_red",           "Red Tektite",           ACTOR_EN_TITE,        -1,                         RE_TEKTITE,            "oot.tektite",      0,        LIST("RedTektite") },
    { "oot.shabom",                "Shabom",                ACTOR_EN_BUBBLE,      0,                          RE_SHABOM,             "oot.shabom",       0,        LIST("Shabom") },
    { "oot.shell_blade",           "Shell Blade",           ACTOR_EN_SB,          0,                          RE_SHELL_BLADE,        "oot.shell_blade",  UNDERWATER, LIST("ShellBlade") },
    { "oot.skulltula",             "Skulltula",             ACTOR_EN_ST,          0,                          RE_BIG_SKULLTULA,      "oot.skulltula",    0,        LIST("Skulltula") },
    { "oot.skull_kid",             "Skull Kid",             ACTOR_EN_SKJ,         4159,                       kNoLogicEnemy,         nullptr,            0,        LIST("SkullKid") },
    { "oot.deku_baba",             "Small Deku Baba",       ACTOR_EN_DEKUBABA,    0,                          RE_DEKU_BABA,          "oot.deku_baba",    0,        LIST("SmallBaba") },
    { "oot.stalchild",             "Stalchild (Small)",     ACTOR_EN_SKB,         1,                          RE_STALCHILD,          "oot.stalchild",    0,        LIST("SmallStalchild") },
    { "oot.moblin_spear",          "Spear Moblin",          ACTOR_EN_MB,          -1,                         kNoLogicEnemy,         nullptr,            0,        LIST("SpearMoblin") },
    { "oot.spike",                 "Spike",                 ACTOR_EN_NY,          0,                          RE_SPIKE,              "oot.spike",        UNDERWATER, LIST("Spike") },
    { "oot.stalfos",               "Stalfos",               ACTOR_EN_TEST,        2,                          RE_STALFOS,            "oot.stalfos",      0,        LIST("Stalfos") },
    { "oot.stinger",               "Stinger",               ACTOR_EN_EIYER,       10,                         RE_STINGER,            "oot.stinger",      0,        LIST("Stinger") },
    { "oot.tailpasaran",           "Tailpasaran",           ACTOR_EN_TP,          -1,                         kNoLogicEnemy,         nullptr,            0,        LIST("Tailparasan") },
    { "oot.torch_slug",            "Torch Slug",            ACTOR_EN_BW,          0,                          RE_TORCH_SLUG,         "oot.torch_slug",   0,        LIST("TorchSlug") },
    { "oot.wallmaster",            "Wallmaster",            ACTOR_EN_WALLMAS,     1,                          RE_WALLMASTER,         "oot.wallmaster",   0,        LIST("Wallmaster") },
    { "oot.iron_knuckle_white",    "Iron Knuckle (White)",  ACTOR_EN_IK,          3,                          RE_IRON_KNUCKLE,       "oot.iron_knuckle", MINIBOSS, LIST("WhiteKnuckle") },
    { "oot.wolfos_white",          "Wolfos (White)",        ACTOR_EN_WF,          1,                          RE_WHITE_WOLFOS,       "oot.wolfos",       0,        LIST("WhiteWolfos") },
    { "oot.deku_baba_withered",    "Withered Deku Baba",    ACTOR_EN_KAREBABA,    0,                          RE_WITHERED_DEKU_BABA, "oot.deku_baba",    0,        LIST("WitheredBaba") },
    { "oot.poe",                   "Poe",                   ACTOR_EN_POH,         0,                          RE_POE,                "oot.poe",          0,        nullptr },
    { "oot.octorok",               "Octorok",               ACTOR_EN_OKUTA,       0,                          RE_OCTOROK,            "oot.octorok",      SURFACE,  nullptr },
    { "oot.skullwalltula",         "Skullwalltula",         ACTOR_EN_SW,          0,                          RE_WALLTULA,           "oot.skulltula",    0,        nullptr },
    { "oot.stinger_water",         "Stinger (Water)",       ACTOR_EN_WEIYER,      0,                          RE_STINGER,            "oot.stinger",      AQUATIC,  nullptr },
    { "oot.deku_scrub_hint",       "Deku Scrub (Hint)",     ACTOR_EN_HINTNUTS,    0,                          kNoLogicEnemy,         nullptr,            0,        nullptr },
    { "oot.bubble_red",            "Bubble (Red)",          ACTOR_EN_BB,          -2,                         RE_BLUE_BUBBLE,        "oot.bubble",       0,        nullptr },
    { "oot.bubble_white",          "Bubble (White)",        ACTOR_EN_BB,          -3,                         RE_BLUE_BUBBLE,        "oot.bubble",       0,        nullptr },
    { "oot.bubble_green",          "Bubble (Green)",        ACTOR_EN_BB,          -4,                         RE_GREEN_BUBBLE,       "oot.bubble",       0,        nullptr },
    { "oot.peahat",                "Peahat",                ACTOR_EN_PEEHAT,      0,                          RE_PEAHAT,             "oot.peahat",       0,        nullptr },
    { "oot.stalfos_pair",          "Stalfos Pair",          ACTOR_EN_TEST,        5,                          RE_STALFOS,            "oot.stalfos",      MINIBOSS, nullptr },
};

const EnemySoul kNativeSouls[] = {
    { "oot.anubis",             "Anubis",             RG_SOUL_ENEMY_ANUBIS,             RAND_INF_SOUL_ENEMY_ANUBIS },
    { "oot.armos",              "Armos",              RG_SOUL_ENEMY_ARMOS,              RAND_INF_SOUL_ENEMY_ARMOS },
    { "oot.bari",               "Bari",               RG_SOUL_ENEMY_BARI,               RAND_INF_SOUL_ENEMY_BARI },
    { "oot.beamos",             "Beamos",             RG_SOUL_ENEMY_BEAMOS,             RAND_INF_SOUL_ENEMY_BEAMOS },
    { "oot.big_octo",           "Big Octo",           RG_SOUL_ENEMY_BIG_OCTO,           RAND_INF_SOUL_ENEMY_BIG_OCTO },
    { "oot.bubble",             "Bubble",             RG_SOUL_ENEMY_BUBBLE,             RAND_INF_SOUL_ENEMY_BUBBLE },
    { "oot.dark_link",          "Dark Link",          RG_SOUL_ENEMY_DARK_LINK,          RAND_INF_SOUL_ENEMY_DARK_LINK },
    { "oot.dead_hand",          "Dead Hand",          RG_SOUL_ENEMY_DEAD_HAND,          RAND_INF_SOUL_ENEMY_DEAD_HAND },
    { "oot.deku_baba",          "Deku Baba",          RG_SOUL_ENEMY_DEKU_BABA,          RAND_INF_SOUL_ENEMY_DEKU_BABA },
    { "oot.dodongo",            "Dodongo",            RG_SOUL_ENEMY_DODONGO,            RAND_INF_SOUL_ENEMY_DODONGO },
    { "oot.flare_dancer",       "Flare Dancer",       RG_SOUL_ENEMY_FLARE_DANCER,       RAND_INF_SOUL_ENEMY_FLARE_DANCER },
    { "oot.freezard",           "Freezard",           RG_SOUL_ENEMY_FREEZARD,           RAND_INF_SOUL_ENEMY_FREEZARD },
    { "oot.gerudo_fighter",     "Gerudo Fighter",     RG_SOUL_ENEMY_GERUDO_FIGHTER,     RAND_INF_SOUL_ENEMY_GERUDO_FIGHTER },
    { "oot.gohma_larva",        "Gohma Larva",        RG_SOUL_ENEMY_GOHMA_LARVA,        RAND_INF_SOUL_ENEMY_GOHMA_LARVA },
    { "oot.guay",               "Guay",               RG_SOUL_ENEMY_GUAY,               RAND_INF_SOUL_ENEMY_GUAY },
    { "oot.iron_knuckle",       "Iron Knuckle",       RG_SOUL_ENEMY_IRON_KNUCKLE,       RAND_INF_SOUL_ENEMY_IRON_KNUCKLE },
    { "oot.keese",              "Keese",              RG_SOUL_ENEMY_KEESE,              RAND_INF_SOUL_ENEMY_KEESE },
    { "oot.leever",             "Leever",             RG_SOUL_ENEMY_LEEVER,             RAND_INF_SOUL_ENEMY_LEEVER },
    { "oot.like_like",          "Like-Like",          RG_SOUL_ENEMY_LIKE_LIKE,          RAND_INF_SOUL_ENEMY_LIKE_LIKE },
    { "oot.lizalfos",           "Lizalfos",           RG_SOUL_ENEMY_LIZALFOS,           RAND_INF_SOUL_ENEMY_LIZALFOS },
    { "oot.mad_scrub",          "Mad Scrub",          RG_SOUL_ENEMY_MAD_SCRUB,          RAND_INF_SOUL_ENEMY_MAD_SCRUB },
    { "oot.octorok",            "Octorok",            RG_SOUL_ENEMY_OCTOROK,            RAND_INF_SOUL_ENEMY_OCTOROK },
    { "oot.parasitic_tentacle", "Parasitic Tentacle", RG_SOUL_ENEMY_PARASITIC_TENTACLE, RAND_INF_SOUL_ENEMY_PARASITIC_TENTACLE },
    { "oot.peahat",             "Peahat",             RG_SOUL_ENEMY_PEAHAT,             RAND_INF_SOUL_ENEMY_PEAHAT },
    { "oot.poe",                "Poe",                RG_SOUL_ENEMY_POE,                RAND_INF_SOUL_ENEMY_POE },
    { "oot.redead",             "Redead",             RG_SOUL_ENEMY_REDEAD,             RAND_INF_SOUL_ENEMY_REDEAD },
    { "oot.shabom",             "Shabom",             RG_SOUL_ENEMY_SHABOM,             RAND_INF_SOUL_ENEMY_SHABOM },
    { "oot.shell_blade",        "Shell Blade",        RG_SOUL_ENEMY_SHELLBLADE,         RAND_INF_SOUL_ENEMY_SHELLBLADE },
    { "oot.skulltula",          "Skulltula",          RG_SOUL_ENEMY_SKULLTULA,          RAND_INF_SOUL_ENEMY_SKULLTULA },
    { "oot.spike",              "Spike",              RG_SOUL_ENEMY_SPIKE,              RAND_INF_SOUL_ENEMY_SPIKE },
    { "oot.stalchild",          "Stalchild",          RG_SOUL_ENEMY_STALCHILD,          RAND_INF_SOUL_ENEMY_STALCHILD },
    { "oot.stalfos",            "Stalfos",            RG_SOUL_ENEMY_STALFOS,            RAND_INF_SOUL_ENEMY_STALFOS },
    { "oot.stinger",            "Stinger",            RG_SOUL_ENEMY_STINGER,            RAND_INF_SOUL_ENEMY_STINGER },
    { "oot.tektite",            "Tektite",            RG_SOUL_ENEMY_TEKTITE,            RAND_INF_SOUL_ENEMY_TEKTITE },
    { "oot.torch_slug",         "Torch Slug",         RG_SOUL_ENEMY_TORCH_SLUG,         RAND_INF_SOUL_ENEMY_TORCH_SLUG },
    { "oot.wallmaster",         "Wallmaster",         RG_SOUL_ENEMY_WALLMASTER,         RAND_INF_SOUL_ENEMY_WALLMASTER },
    { "oot.wolfos",             "Wolfos",             RG_SOUL_ENEMY_WOLFOS,             RAND_INF_SOUL_ENEMY_WOLFOS },
};

const EnemySpawnRow kEnemySpawns[] = {
#include "StaticData/EnemySpawns.inc"
};

const int16_t kEnemiesToRandomize[] = {
    ACTOR_EN_ANUBICE_TAG, ACTOR_EN_FIREFLY, ACTOR_EN_TEST,     ACTOR_EN_TITE,     ACTOR_EN_POH,       ACTOR_EN_OKUTA,
    ACTOR_EN_WALLMAS,     ACTOR_EN_DODONGO, ACTOR_EN_PEEHAT,   ACTOR_EN_ZF,       ACTOR_EN_GOMA,      ACTOR_EN_BUBBLE,
    ACTOR_EN_DODOJR,      ACTOR_EN_TORCH2,  ACTOR_EN_BILI,     ACTOR_EN_TP,       ACTOR_EN_ST,        ACTOR_EN_BW,
    ACTOR_EN_EIYER,       ACTOR_EN_MB,      ACTOR_EN_DEKUBABA, ACTOR_EN_AM,       ACTOR_EN_DEKUNUTS,  ACTOR_EN_VALI,
    ACTOR_EN_BB,          ACTOR_EN_YUKABYUN, ACTOR_EN_VM,      ACTOR_EN_FLOORMAS, ACTOR_EN_RD,        ACTOR_EN_SW,
    ACTOR_EN_FD,          ACTOR_EN_SB,      ACTOR_EN_KAREBABA, ACTOR_EN_RR,       ACTOR_EN_NY,        ACTOR_EN_IK,
    ACTOR_EN_TUBO_TRAP,   ACTOR_EN_FZ,      ACTOR_EN_WEIYER,   ACTOR_EN_HINTNUTS, ACTOR_EN_WF,        ACTOR_EN_SKB,
    ACTOR_EN_CROW,        ACTOR_EN_SKJ,
};
// clang-format on

#undef MINIBOSS
#undef UNDERWATER
#undef AQUATIC
#undef SURFACE
#undef LIST

struct SpawnInstance {
    int16_t sceneNum;
    int16_t roomNum;
    int16_t actorId;
    int16_t params;
    int16_t posX;
    int16_t posY;
    uint32_t roomFlags;
    bool mq;
};

enum class WaterClass {
    Dry,
    Surface,
    Submerged,
};

struct SlotSpawn {
    int16_t roomNum;
    int16_t posY;
    uint32_t roomFlags;
    WaterClass waterClass;
    bool timed;
};

struct Slot {
    int16_t originalActorId;
    bool mq;
    bool miniboss;
    bool hangsFromCeiling;
    std::vector<SlotSpawn> spawns;
};

// A mod port of the same enemy replaces the native row as a candidate; the native spawns stay sources.
const std::pair<const char*, const char*> kSupersededNatives[] = {
    { "oot.dinolfos", "mm.dinolfos" },
};

using SlotKey = std::pair<int16_t, std::string>;

EnemyPlacement sSeed;
EnemyPlacement sSave;
std::map<int32_t, std::map<int32_t, EnemyLogicPlan>> sLogicPlans;

EnemyKind FromNativeRow(const NativeRow& row) {
    EnemyKind kind{};
    kind.key = row.key;
    kind.name = row.name;
    kind.actorId = row.actorId;
    kind.params = row.params;
    kind.flags = row.flags;
    kind.logicEnemy = row.logicEnemy;
    kind.soulKey = row.soulKey;
    kind.enhancementCvar = row.enhancementCvar;
    kind.spawnable = row.enhancementCvar != nullptr;
    return kind;
}

EnemyKind FromCustom(const SOHCustomEnemy& enemy) {
    EnemyKind kind{};
    kind.key = enemy.key;
    kind.name = enemy.name;
    kind.actorId = enemy.actorId;
    kind.params = enemy.params;
    kind.spawnHeight = enemy.spawnHeight;
    kind.flags = enemy.flags;
    kind.logicEnemy = kNoLogicEnemy;
    kind.soulKey = enemy.soulKey;
    kind.logicKill = enemy.logicKill;
    kind.logicKillUserData = enemy.logicKillUserData;
    kind.spawnable = true;
    kind.custom = true;
    return kind;
}

const EnemyKind* FindNativeByKey(const std::string& key) {
    for (const EnemyKind& kind : EnemyCatalog_GetNatives()) {
        if (key == kind.key) {
            return &kind;
        }
    }
    return nullptr;
}

const EnemyKind* FindNativeVariant(int16_t actorId, int16_t params) {
    const EnemyKind* family = nullptr;
    for (const EnemyKind& kind : EnemyCatalog_GetNatives()) {
        if (kind.actorId != actorId) {
            continue;
        }
        if (kind.params == params) {
            return &kind;
        }
        if (family == nullptr) {
            family = &kind;
        }
    }
    return family;
}

int8_t LowByte(int16_t params) {
    return static_cast<int8_t>(params & 0xFF);
}

int16_t KeeseVariant(int16_t params) {
    int16_t invisible = params & 0x8000;
    int16_t type = params & 0x7FFF;

    if (type <= 1) {
        return static_cast<int16_t>(invisible | 1);
    }
    if (type == 4) {
        return static_cast<int16_t>(invisible | 4);
    }
    return static_cast<int16_t>(invisible | 2);
}

int16_t RedeadVariant(int16_t params) {
    int8_t type = LowByte(params);

    if (type <= -2) {
        return 32766;
    }
    return type == 3 ? 3 : 1;
}

int16_t BubbleVariant(int16_t params) {
    int8_t type = LowByte(params);
    return type <= -4 ? -4 : type;
}

// Maps any spawn params to the params of the catalog row of its variant.
int16_t NormalizeVariantParams(int16_t sceneNum, int16_t actorId, int16_t params) {
    switch (actorId) {
        case ACTOR_EN_FIREFLY:
            return KeeseVariant(params);
        case ACTOR_EN_RD:
            return RedeadVariant(params);
        case ACTOR_EN_BB:
            return BubbleVariant(params);
        case ACTOR_EN_ST:
            return params == 1 || params == 2 ? params : 0;
        case ACTOR_EN_ZF:
            return LowByte(params) == -2 ? -2 : -1;
        case ACTOR_EN_WF:
            return (params & 0xFF) == 1 ? 1 : 0;
        case ACTOR_EN_MB:
            return params == 0 ? 0 : -1;
        case ACTOR_EN_SKB:
            return params >= 20 ? 20 : 1;
        case ACTOR_EN_DEKUBABA:
            return params == 1 ? 1 : 0;
        case ACTOR_EN_PEEHAT:
            return params == -1 || params == 1 ? params : 0;
        case ACTOR_EN_FLOORMAS:
            return static_cast<int16_t>(params & 0x8000);
        case ACTOR_EN_TITE:
            return params == -1 ? -1 : -2;
        case ACTOR_EN_IK:
            return (params & 0xFF) == 3 ? 3 : 2;
        case ACTOR_EN_TEST:
            if (params == 0) {
                return 0;
            }
            return params == 5 && sceneNum == SCENE_FOREST_TEMPLE ? 5 : 2;
        default:
            return params;
    }
}

// Children that the enemy randomizer replaces through a spawner's VB hook.
std::vector<SpawnInstance> ExpandSpawnRow(const EnemySpawnRow& row) {
    SpawnInstance base{ static_cast<int16_t>(row.sceneId), static_cast<int16_t>(row.roomIndex),
                        static_cast<int16_t>(row.actorId), static_cast<int16_t>(row.params),
                        static_cast<int16_t>(row.posX),    static_cast<int16_t>(row.posY),
                        row.roomFlags,                     (row.roomFlags & ENEMY_SPAWN_ROW_MQ) != 0 };
    auto child = [&base](int16_t actorId, int16_t params) {
        SpawnInstance instance = base;
        instance.actorId = actorId;
        instance.params = params;
        return instance;
    };

    switch (base.actorId) {
        case ACTOR_BG_HAKA:
            if (base.sceneNum != SCENE_GRAVEYARD) {
                return {};
            }
            return { child(ACTOR_EN_POH, 1) };
        case ACTOR_BG_HAKA_TUBO:
            return { child(ACTOR_EN_FIREFLY, 2) };
        case ACTOR_BG_HAKA_HUTA:
            return { child(ACTOR_EN_FIREFLY, 2), child(ACTOR_EN_RD, 0xFD) };
        case ACTOR_EN_BLKOBJ:
            return { child(ACTOR_EN_TORCH2, 0) };
        case ACTOR_BG_MORI_BIGST:
            return { child(ACTOR_EN_TEST, 5) };
        case ACTOR_EN_ENCOUNT1: {
            int16_t spawnType = (base.params >> 11) & 0x1F;
            if (spawnType == 2) {
                return { child(ACTOR_EN_SKB, 0) };
            }
            if (spawnType == 3) {
                return { child(ACTOR_EN_WF, static_cast<int16_t>(0xFF00)) };
            }
            return {};
        }
        default:
            return { base };
    }
}

bool IsSeedSceneMasterQuest(int16_t sceneNum) {
    Rando::DungeonInfo* dungeon = Rando::Context::GetInstance()->GetDungeons()->GetDungeonFromScene(sceneNum);
    return dungeon != nullptr && dungeon->IsMQ();
}

// A dungeon scene has vanilla and MQ rows; only the version the seed plays counts.
bool IsRowInSeed(const EnemySpawnRow& row) {
    bool masterQuestRow = row.roomFlags & ENEMY_SPAWN_ROW_MQ;
    return masterQuestRow == IsSeedSceneMasterQuest(static_cast<int16_t>(row.sceneId));
}

std::vector<SpawnInstance> ExpandSpawnTable() {
    std::vector<SpawnInstance> instances;
    for (const EnemySpawnRow& row : kEnemySpawns) {
        if (!IsRowInSeed(row)) {
            continue;
        }
        std::vector<SpawnInstance> expanded = ExpandSpawnRow(row);
        instances.insert(instances.end(), expanded.begin(), expanded.end());
    }
    return instances;
}

bool IsExcludedFromClearRooms(int16_t actorId, int16_t params) {
    switch (actorId) {
        // Freezard - Child Link can only kill this with Deku Stick jumpslash or other equipment like bombs
        case ACTOR_EN_FZ:
        // Beamos - Needs bombs
        case ACTOR_EN_VM:
        // Shell Blade - It's annoying to kill these as Child Link with sword or Deku Stick
        case ACTOR_EN_SB:
        // Spike - Child Link can't kill these with sword or Deku Stick
        case ACTOR_EN_NY:
        // Arwing - Goes out of bounds way too easily, softlocking the player
        case ACTOR_EN_CLEAR_TAG:
        // Wallmaster - Not easily visible, often makes players think they're softlocked and that there's no enemies
        // left
        case ACTOR_EN_WALLMAS:
        // Dark Link - Goes out of bounds way too easily, softlocking the player
        case ACTOR_EN_TORCH2:
        // Flare dancer - Goes out of bounds way too easily, softlocking the player
        case ACTOR_EN_FD:
        // Anubis - Needs fire
        case ACTOR_EN_ANUBICE_TAG:
        // Withered Deku Baba - Stationary, can get stuck in air/on chests, softlock risk
        case ACTOR_EN_KAREBABA:
            return true;
        case ACTOR_EN_MB:
            return params == 0;
        default:
            return false;
    }
}

bool IsExcludedFromTimedRooms(int16_t actorId, int16_t params) {
    switch (actorId) {
        // Bari - Spawns 3 more enemies, potentially extremely difficult in timed rooms
        case ACTOR_EN_VALI:
            return true;
        default:
            return IsExcludedFromClearRooms(actorId, params);
    }
}

bool IsUnfitForClearRooms(const EnemyKind& enemy) {
    return IsExcludedFromClearRooms(enemy.actorId, enemy.params) || (enemy.flags & SOH_ACTOR_ENEMY_NOT_IN_CLEAR_ROOMS);
}

bool IsUnfitForTimedRooms(const EnemyKind& enemy) {
    return IsUnfitForClearRooms(enemy) || IsExcludedFromTimedRooms(enemy.actorId, enemy.params) ||
           (enemy.flags & SOH_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS);
}

std::set<std::string> ParseKeyList(const std::string& text) {
    std::set<std::string> keys;
    std::stringstream stream(text);
    std::string key;
    while (std::getline(stream, key, ',')) {
        key.erase(0, key.find_first_not_of(' '));
        key.erase(key.find_last_not_of(' ') + 1);
        if (!key.empty()) {
            keys.insert(key);
        }
    }
    return keys;
}

std::string JoinKeyList(const std::set<std::string>& keys) {
    std::string text;
    for (const std::string& key : keys) {
        text += text.empty() ? key : "," + key;
    }
    return text;
}

std::set<std::string> ReadKeyListCvar(const char* cvar) {
    return ParseKeyList(CVarGetString(cvar, ""));
}

bool IsSupersededByMod(const EnemyKind& native) {
    for (const auto& [nativeKey, modKey] : kSupersededNatives) {
        if (strcmp(native.key, nativeKey) == 0 && ActorRegistry_FindEnemyByKey(modKey) != nullptr) {
            return true;
        }
    }
    return false;
}

bool IsCandidateNative(const EnemyKind& native) {
    return native.spawnable && !IsSupersededByMod(native);
}

std::set<std::pair<int16_t, int16_t>> CollectTimedRooms() {
    std::set<std::pair<int16_t, int16_t>> timedRooms;
    for (const EnemySpawnRow& row : kEnemySpawns) {
        if (row.actorId == ACTOR_OBJ_ROOMTIMER && IsRowInSeed(row)) {
            timedRooms.insert({ static_cast<int16_t>(row.sceneId), static_cast<int16_t>(row.roomIndex) });
        }
    }
    return timedRooms;
}

WaterClass GetRowWaterClass(uint32_t roomFlags) {
    if (roomFlags & ENEMY_SPAWN_ROW_SUBMERGED) {
        return WaterClass::Submerged;
    }
    return (roomFlags & ENEMY_SPAWN_ROW_SURFACE) ? WaterClass::Surface : WaterClass::Dry;
}

// Aquatic natives move to their own depth wherever their row lies in water (Octorok to the surface).
WaterClass GetSpawnWaterClass(const EnemyKind& original, uint32_t roomFlags) {
    WaterClass rowClass = GetRowWaterClass(roomFlags);
    if (rowClass == WaterClass::Dry) {
        return rowClass;
    }
    bool needsWater = original.flags & SOH_ACTOR_ENEMY_NEEDS_WATER;
    bool underwater = original.flags & SOH_ACTOR_ENEMY_UNDERWATER;
    if (needsWater && underwater) {
        return WaterClass::Submerged;
    }
    if (needsWater || (original.flags & ENEMY_SURFACE_WALKER)) {
        return WaterClass::Surface;
    }
    return rowClass;
}

bool FitsWaterClass(const EnemyKind& candidate, WaterClass waterClass) {
    bool needsWater = candidate.flags & SOH_ACTOR_ENEMY_NEEDS_WATER;
    bool underwater = candidate.flags & SOH_ACTOR_ENEMY_UNDERWATER;
    switch (waterClass) {
        case WaterClass::Dry:
            return !needsWater;
        case WaterClass::Surface:
            return (needsWater && !underwater) || (candidate.flags & ENEMY_SURFACE_WALKER);
        case WaterClass::Submerged:
            return underwater;
        default:
            SPDLOG_WARN("[EnemyPlacement] Unknown water class {}", static_cast<int>(waterClass));
            return false;
    }
}

std::map<SlotKey, Slot> CollectSlots(uint8_t minibossMode) {
    std::map<SlotKey, Slot> slots;
    std::set<std::pair<int16_t, int16_t>> timedRooms = CollectTimedRooms();
    for (const SpawnInstance& spawn : ExpandSpawnTable()) {
        if (!EnemyRando_IsRandomizableSpawn(spawn.sceneNum, spawn.roomNum, spawn.actorId, spawn.params, spawn.posX,
                                            spawn.mq, true)) {
            continue;
        }
        std::optional<EnemyKind> original = EnemyCatalog_Classify(spawn.sceneNum, spawn.actorId, spawn.params);
        if (!original.has_value()) {
            continue;
        }
        bool miniboss = original->flags & SOH_ACTOR_ENEMY_MINIBOSS;
        if (miniboss && minibossMode == ENEMY_MINIBOSSES_VANILLA) {
            continue;
        }
        Slot& slot = slots[{ spawn.sceneNum, original->key }];
        slot.originalActorId = spawn.actorId;
        slot.mq = spawn.mq;
        slot.miniboss = miniboss;
        slot.hangsFromCeiling = spawn.actorId == ACTOR_EN_ST || (original->flags & SOH_ACTOR_ENEMY_NEEDS_CEILING);
        bool timed = timedRooms.contains({ spawn.sceneNum, spawn.roomNum });
        slot.spawns.push_back(
            { spawn.roomNum, spawn.posY, spawn.roomFlags, GetSpawnWaterClass(*original, spawn.roomFlags), timed });
    }
    return slots;
}

bool FitsSpawn(int16_t sceneNum, bool mq, const SlotSpawn& spawn, const EnemyKind& candidate) {
    if (!EnemyRando_IsAllowedAt(sceneNum, spawn.roomNum, mq, candidate, spawn.posY, false)) {
        return false;
    }
    if ((spawn.roomFlags & ENEMY_SPAWN_ROOM_CLEAR_TRIGGER) && IsUnfitForClearRooms(candidate)) {
        return false;
    }
    if (spawn.timed && IsUnfitForTimedRooms(candidate)) {
        return false;
    }
    return FitsWaterClass(candidate, spawn.waterClass);
}

bool FitsSlot(const SlotKey& slotKey, const Slot& slot, const EnemyKind& candidate, uint8_t minibossMode) {
    bool candidateMiniboss = candidate.flags & SOH_ACTOR_ENEMY_MINIBOSS;
    if (minibossMode == ENEMY_MINIBOSSES_VANILLA && candidateMiniboss) {
        return false;
    }
    if (minibossMode == ENEMY_MINIBOSSES_AMONG_THEMSELVES && candidateMiniboss != slot.miniboss) {
        return false;
    }
    // A Bari replacing Biri would respawn itself from every Bari death.
    if (slot.originalActorId == ACTOR_EN_BILI && candidate.actorId == ACTOR_EN_VALI) {
        return false;
    }
    if ((candidate.flags & SOH_ACTOR_ENEMY_NEEDS_CEILING) && !slot.hangsFromCeiling) {
        return false;
    }
    for (const SlotSpawn& spawn : slot.spawns) {
        if (!FitsSpawn(slotKey.first, slot.mq, spawn, candidate)) {
            return false;
        }
    }
    return true;
}

std::vector<EnemyKind> GatherPoolCandidates() {
    std::set<std::string> excluded = ReadKeyListCvar(CVAR_ENEMY_POOL_EXCLUDED);
    std::vector<EnemyKind> candidates;
    for (const EnemyKind& kind : EnemyCatalog_GetNatives()) {
        if (IsCandidateNative(kind) && !excluded.contains(kind.key)) {
            candidates.push_back(kind);
        }
    }
    for (const EnemyKind& kind : EnemyCatalog_GetCustoms()) {
        if (!excluded.contains(kind.key)) {
            candidates.push_back(kind);
        }
    }
    return candidates;
}

void AddToLogicPlan(EnemyLogicPlan& plan, const std::optional<EnemyKind>& replacement, bool onlySubmerged) {
    if (!replacement.has_value()) {
        plan.keepsOriginal = true;
        return;
    }
    for (const PlannedReplacement& known : plan.replacements) {
        if (strcmp(known.kind.key, replacement->key) == 0 && known.onlySubmerged == onlySubmerged) {
            return;
        }
    }
    plan.replacements.push_back({ *replacement, onlySubmerged });
}

struct ReplacedSpawn {
    SpawnInstance spawn;
    EnemyKind original;
    std::optional<EnemyKind> replacement;
};

std::vector<ReplacedSpawn> CollectReplacedSpawns() {
    std::vector<ReplacedSpawn> replaced;
    for (const SpawnInstance& spawn : ExpandSpawnTable()) {
        auto scene = sSeed.replacements.find(spawn.sceneNum);
        if (scene == sSeed.replacements.end()) {
            continue;
        }
        std::optional<EnemyKind> original = EnemyCatalog_Classify(spawn.sceneNum, spawn.actorId, spawn.params);
        if (!original.has_value() || original->logicEnemy == kNoLogicEnemy) {
            continue;
        }
        std::optional<EnemyKind> replacement;
        auto placed = scene->second.find(original->key);
        if (placed != scene->second.end() &&
            EnemyRando_IsRandomizableSpawn(spawn.sceneNum, spawn.roomNum, spawn.actorId, spawn.params, spawn.posX,
                                           spawn.mq, true)) {
            replacement = EnemyCatalog_Find(placed->second);
        }
        replaced.push_back({ spawn, *original, replacement });
    }
    return replaced;
}

std::set<SlotKey> CollectGroupsReplacedAboveWater(const std::vector<ReplacedSpawn>& replaced) {
    std::set<SlotKey> groups;
    for (const ReplacedSpawn& entry : replaced) {
        if (entry.replacement.has_value() &&
            GetSpawnWaterClass(entry.original, entry.spawn.roomFlags) != WaterClass::Submerged) {
            groups.insert({ entry.spawn.sceneNum, entry.original.key });
        }
    }
    return groups;
}

void RebuildLogicPlans() {
    sLogicPlans.clear();
    std::vector<ReplacedSpawn> replaced = CollectReplacedSpawns();
    std::set<SlotKey> aboveWater = CollectGroupsReplacedAboveWater(replaced);
    for (const ReplacedSpawn& entry : replaced) {
        bool onlySubmerged = !aboveWater.contains({ entry.spawn.sceneNum, entry.original.key });
        AddToLogicPlan(sLogicPlans[entry.spawn.sceneNum][entry.original.logicEnemy], entry.replacement,
                       onlySubmerged);
    }
}

void SetSeedPlacement(const EnemyPlacement& placement) {
    sSeed = placement;
    RebuildLogicPlans();
}

void ResetSaveState(bool) {
    sSave = {};
}

void SaveState(const SaveContext&, int, bool) {
    std::vector<std::tuple<int32_t, std::string, std::string>> entries;
    for (const auto& [sceneNum, scene] : sSave.replacements) {
        for (const auto& [original, replacement] : scene) {
            entries.emplace_back(sceneNum, original, replacement);
        }
    }
    std::vector<std::string> excludedSouls(sSave.soulPoolExcluded.begin(), sSave.soulPoolExcluded.end());

    SaveManager::Instance->SaveData("entryCount", entries.size());
    SaveManager::Instance->SaveArray("entries", entries.size(), [&entries](size_t index) {
        SaveManager::Instance->SaveStruct("", [&entries, index]() {
            SaveManager::Instance->SaveData("scene", std::get<0>(entries[index]));
            SaveManager::Instance->SaveData("original", std::get<1>(entries[index]));
            SaveManager::Instance->SaveData("replacement", std::get<2>(entries[index]));
        });
    });
    SaveManager::Instance->SaveData("soulCount", excludedSouls.size());
    SaveManager::Instance->SaveArray("soulPoolExcluded", excludedSouls.size(), [&excludedSouls](size_t index) {
        SaveManager::Instance->SaveData("", excludedSouls[index]);
    });
}

void LoadState() {
    sSave = {};
    size_t entryCount = 0;
    SaveManager::Instance->LoadData("entryCount", entryCount, static_cast<size_t>(0));
    SaveManager::Instance->LoadArray("entries", entryCount, [](size_t) {
        SaveManager::Instance->LoadStruct("", []() {
            int32_t sceneNum = -1;
            std::string original;
            std::string replacement;
            SaveManager::Instance->LoadData("scene", sceneNum, -1);
            SaveManager::Instance->LoadData("original", original, std::string());
            SaveManager::Instance->LoadData("replacement", replacement, std::string());
            if (sceneNum >= 0 && !original.empty() && !replacement.empty()) {
                sSave.replacements[sceneNum][original] = replacement;
            }
        });
    });
    size_t soulCount = 0;
    SaveManager::Instance->LoadData("soulCount", soulCount, static_cast<size_t>(0));
    SaveManager::Instance->LoadArray("soulPoolExcluded", soulCount, [](size_t) {
        std::string soulKey;
        SaveManager::Instance->LoadData("", soulKey, std::string());
        sSave.soulPoolExcluded.insert(soulKey);
    });
}

void SyncSeedOnLoad(int32_t) {
    if (IS_RANDO) {
        SetSeedPlacement(sSave);
    }
}

void RegisterEnemyPlacementSave() {
    SaveManager::Instance->AddInitFunction(ResetSaveState);
    SaveManager::Instance->AddSaveFunction(kSaveSection, 1, SaveState, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(kSaveSection, 1, LoadState);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>(SyncSeedOnLoad);
}

RegisterShipInitFunc sRegisterSave(RegisterEnemyPlacementSave);

struct PoolEntry {
    std::string key;
    std::string name;
};

void WriteKeyListCvar(const char* cvar, const std::set<std::string>& keys) {
    CVarSetString(cvar, JoinKeyList(keys).c_str());
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

bool ContainsIgnoringCase(const std::string& text, const char* fragment) {
    std::string_view needle(fragment);
    auto found = std::search(text.begin(), text.end(), needle.begin(), needle.end(),
                             [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
    return found != text.end();
}

void DrawPoolBulkButtons(const char* cvar, const std::vector<const PoolEntry*>& shown) {
    std::set<std::string> excluded = ReadKeyListCvar(cvar);
    if (UIWidgets::Button("All", UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR))) {
        for (const PoolEntry* entry : shown) {
            excluded.erase(entry->key);
        }
        WriteKeyListCvar(cvar, excluded);
    }
    ImGui::SameLine();
    if (UIWidgets::Button("None", UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR))) {
        for (const PoolEntry* entry : shown) {
            excluded.insert(entry->key);
        }
        WriteKeyListCvar(cvar, excluded);
    }
}

void DrawPoolList(const char* title, const char* cvar, const std::vector<PoolEntry>& entries) {
    static std::map<std::string, std::array<char, 64>> filters;
    std::array<char, 64>& filter = filters[title];
    ImGui::PushID(title);
    ImGui::SeparatorText(title);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
    ImGui::InputTextWithHint("##search", "Search", filter.data(), filter.size());

    std::vector<const PoolEntry*> shown;
    for (const PoolEntry& entry : entries) {
        if (ContainsIgnoringCase(entry.name, filter.data())) {
            shown.push_back(&entry);
        }
    }
    ImGui::SameLine();
    DrawPoolBulkButtons(cvar, shown);

    std::set<std::string> excluded = ReadKeyListCvar(cvar);
    if (ImGui::BeginTable("##pool", 2)) {
        for (const PoolEntry* entry : shown) {
            ImGui::TableNextColumn();
            bool included = !excluded.contains(entry->key);
            std::string label = entry->name + "##" + entry->key;
            if (UIWidgets::Checkbox(label.c_str(), &included, UIWidgets::CheckboxOptions().Color(THEME_COLOR))) {
                if (included) {
                    excluded.erase(entry->key);
                } else {
                    excluded.insert(entry->key);
                }
                WriteKeyListCvar(cvar, excluded);
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
}

bool IsMiniboss(const EnemyKind& kind) {
    return kind.flags & SOH_ACTOR_ENEMY_MINIBOSS;
}

std::vector<PoolEntry> ListEnemyPool(bool minibosses) {
    std::vector<PoolEntry> entries;
    for (const EnemyKind& kind : EnemyCatalog_GetNatives()) {
        if (IsCandidateNative(kind) && IsMiniboss(kind) == minibosses) {
            entries.push_back({ kind.key, kind.name });
        }
    }
    for (const EnemyKind& kind : EnemyCatalog_GetCustoms()) {
        if (IsMiniboss(kind) == minibosses) {
            entries.push_back({ kind.key, kind.name });
        }
    }
    return entries;
}

std::vector<std::pair<std::string, std::string>> CustomSoulEntries() {
    std::vector<std::pair<std::string, std::string>> entries;
    for (const EnemyKind& kind : EnemyCatalog_GetCustoms()) {
        if (kind.soulKey == nullptr || EnemyCatalog_FindNativeSoul(kind.soulKey) != nullptr) {
            continue;
        }
        bool listed = std::any_of(entries.begin(), entries.end(),
                                  [&kind](const auto& entry) { return entry.first == kind.soulKey; });
        if (!listed) {
            entries.emplace_back(kind.soulKey, kind.name);
        }
    }
    return entries;
}

} // namespace

const std::vector<EnemyKind>& EnemyCatalog_GetNatives() {
    static const std::vector<EnemyKind> natives = [] {
        std::vector<EnemyKind> kinds;
        for (const NativeRow& row : kNativeRows) {
            kinds.push_back(FromNativeRow(row));
        }
        return kinds;
    }();
    return natives;
}

std::vector<EnemyKind> EnemyCatalog_GetCustoms() {
    std::vector<EnemyKind> customs;
    for (uint32_t index = 0; index < ActorRegistry_GetEnemyCount(); index++) {
        customs.push_back(FromCustom(*ActorRegistry_GetEnemyAt(index)));
    }
    return customs;
}

std::optional<EnemyKind> EnemyCatalog_Find(const std::string& key) {
    const EnemyKind* native = FindNativeByKey(key);
    if (native != nullptr) {
        return *native;
    }
    const SOHCustomEnemy* custom = ActorRegistry_FindEnemyByKey(key.c_str());
    if (custom != nullptr) {
        return FromCustom(*custom);
    }
    return std::nullopt;
}

std::optional<EnemyKind> EnemyCatalog_Classify(int16_t sceneNum, int16_t actorId, int16_t params) {
    const SOHCustomEnemy* custom = ActorRegistry_FindEnemyById(actorId);
    if (custom != nullptr) {
        return FromCustom(*custom);
    }
    const EnemyKind* native = FindNativeVariant(actorId, NormalizeVariantParams(sceneNum, actorId, params));
    if (native == nullptr) {
        return std::nullopt;
    }
    return *native;
}

const std::vector<EnemySoul>& EnemyCatalog_GetNativeSouls() {
    static const std::vector<EnemySoul> souls(std::begin(kNativeSouls), std::end(kNativeSouls));
    return souls;
}

const EnemySoul* EnemyCatalog_FindNativeSoul(const std::string& soulKey) {
    for (const EnemySoul& soul : EnemyCatalog_GetNativeSouls()) {
        if (soulKey == soul.key) {
            return &soul;
        }
    }
    return nullptr;
}

const EnemySoul* EnemyCatalog_FindNativeSoulByFlag(RandomizerInf flag) {
    for (const EnemySoul& soul : EnemyCatalog_GetNativeSouls()) {
        if (soul.flag == flag) {
            return &soul;
        }
    }
    return nullptr;
}

bool EnemyRando_IsClearRoom(bool mq, int16_t sceneNum, int16_t roomNum) {
    switch (sceneNum) {
        case SCENE_DEKU_TREE:
            if (mq) {
                return roomNum == 4 || roomNum == 6 || roomNum == 9 || roomNum == 10;
            } else {
                return roomNum == 1 || roomNum == 9;
            }
        case SCENE_DODONGOS_CAVERN:
            if (mq) {
                return roomNum == 5 || roomNum == 6 || roomNum == 13 || roomNum == 14;
            } else {
                return roomNum == 15;
            }
        case SCENE_JABU_JABU:
            if (mq) {
                return roomNum == 11 || roomNum == 13 || roomNum == 14;
            } else {
                return roomNum == 8 || roomNum == 9;
            }
        case SCENE_FOREST_TEMPLE:
            if (mq) {
                return roomNum == 5 || roomNum == 6 || roomNum == 18 || roomNum == 21;
            } else {
                return roomNum == 6 || roomNum == 10 || roomNum == 18 || roomNum == 21;
            }
        case SCENE_FIRE_TEMPLE:
            if (mq) {
                return roomNum == 15 || roomNum == 17 || roomNum == 18 || roomNum == 24;
            } else {
                return roomNum == 3 || roomNum == 15 || roomNum == 24;
            }
        case SCENE_WATER_TEMPLE:
            if (mq) {
                return roomNum == 13 || roomNum == 18;
            } else {
                return roomNum == 13 || roomNum == 18 || roomNum == 19;
            }
        case SCENE_SPIRIT_TEMPLE:
            if (mq) {
                return roomNum == 1 || roomNum == 2 || roomNum == 4 || roomNum == 10 || roomNum == 15 ||
                       roomNum == 19 || roomNum == 20;
            } else {
                return roomNum == 1 || roomNum == 10 || roomNum == 17 || roomNum == 20 || roomNum == 27;
            }
        case SCENE_SHADOW_TEMPLE:
            if (mq) {
                return roomNum == 1 || roomNum == 6 || roomNum == 7 || roomNum == 11 || roomNum == 14 || roomNum == 20;
            } else {
                return roomNum == 1 || roomNum == 7 || roomNum == 11 || roomNum == 14 || roomNum == 16 ||
                       roomNum == 17 || roomNum == 19 || roomNum == 20;
            }
        case SCENE_INSIDE_GANONS_CASTLE:
            if (mq) {
                return roomNum == 0 || roomNum == 2 || roomNum == 5 || roomNum == 9;
            } else {
                return roomNum == 2 || roomNum == 5 || roomNum == 9;
            }
        case SCENE_ICE_CAVERN:
            if (mq) {
                return roomNum == 3 || roomNum == 7;
            } else {
                return roomNum == 1 || roomNum == 7;
            }
        case SCENE_GERUDO_TRAINING_GROUND:
            if (mq) {
                return roomNum == 10;
            } else {
                return roomNum == 3 || roomNum == 5 || roomNum == 10;
            }
        case SCENE_GANONS_TOWER:
            return true;
        case SCENE_GROTTOS:
            return roomNum == 2 || roomNum == 7;
        case SCENE_ROYAL_FAMILYS_TOMB:
            return roomNum == 0;
        default:
            return false;
    }
}

bool EnemyRando_IsTimedRoom(bool mq, int16_t sceneNum, int16_t roomNum) {
    switch (sceneNum) {
        case SCENE_JABU_JABU:
            return !mq && roomNum == 12;
        case SCENE_GERUDO_TRAINING_GROUND:
            if (mq) {
                return roomNum == 1 || roomNum == 3 || roomNum == 5 || roomNum == 7;
            } else {
                return roomNum == 1 || roomNum == 7;
            }
        case SCENE_GANONS_TOWER_COLLAPSE_INTERIOR:
            return roomNum == 1;
        default:
            return false;
    }
}

bool EnemyRando_IsAllowedAt(int16_t sceneNum, int16_t roomNum, bool mq, const EnemyKind& enemy, int16_t posY,
                            bool fromBari) {
    if (IsUnfitForClearRooms(enemy) && EnemyRando_IsClearRoom(mq, sceneNum, roomNum)) {
        return false;
    }

    if (IsUnfitForTimedRooms(enemy) && EnemyRando_IsTimedRoom(mq, sceneNum, roomNum)) {
        return false;
    }

    // Don't allow Lizalfos or Baris in Ganon's Tower because they would spawn up on the ceiling, becoming impossible to
    // kill.
    if (sceneNum == SCENE_GANONS_TOWER &&
        (enemy.actorId == ACTOR_EN_VALI || (enemy.actorId == ACTOR_EN_ZF && enemy.params == -1))) {
        return false;
    }

    // Don't allow Lizalfos in the first room of the interior of the castle collapse
    if (sceneNum == SCENE_GANONS_TOWER_COLLAPSE_INTERIOR && roomNum == 1 && enemy.actorId == ACTOR_EN_ZF &&
        enemy.params == -1) {
        return false;
    }

    // Don't allow Lizalfos in the silver rupee bridge room of spirit, as they spawn in the room above
    if (sceneNum == SCENE_SPIRIT_TEMPLE && roomNum == 2 && enemy.actorId == ACTOR_EN_ZF && enemy.params == -1) {
        return false;
    }

    // Don't allow big Stalchildren, big Peahats and Baris (big jellyfish) during the Gohma fight because they can clip
    // into Gohma and it crashes the game. Likely because Gohma on the ceiling can't handle collision with other
    // enemies.
    if (sceneNum == SCENE_DEKU_TREE_BOSS &&
        ((enemy.actorId == ACTOR_EN_SKB && enemy.params == 20) ||
         (enemy.actorId == ACTOR_EN_PEEHAT && enemy.params == -1) || (enemy.actorId == ACTOR_EN_VALI))) {
        return false;
    }

    // Don't allow the following enemies in the first spawn of the first room in MQ Fire Temple loop as when spawned and
    // they get stuck in the room above
    // - Lizalfos/Dinolfos, Bari: they drop in
    // - Skulltulla: they appear above
    // - Flying Peehat: they rise above the ceiling
    if (mq && sceneNum == SCENE_FIRE_TEMPLE && roomNum == 15 && posY == 64 &&
        (enemy.actorId == ACTOR_EN_ZF || enemy.actorId == ACTOR_EN_VALI || enemy.actorId == ACTOR_EN_ST ||
         enemy.actorId == ACTOR_EN_PEEHAT)) {
        return false;
    }

    // Don't allow Stalfos in the child spirit clear room as they jump out of bounds frequently
    if (sceneNum == SCENE_SPIRIT_TEMPLE && roomNum == 1 && enemy.actorId == ACTOR_EN_TEST) {
        return false;
    }

    // Don't allow baris to spawn another bari
    return !(fromBari && enemy.actorId == ACTOR_EN_VALI);
}

bool EnemyRando_IsRandomizableSpawn(int16_t sceneNum, int16_t roomNum, int16_t actorId, int16_t params, float posX,
                                    bool mq, bool isChild) {
    if (ActorRegistry_FindEnemyById(actorId) != nullptr) {
        return true;
    }
    if (std::find(std::begin(kEnemiesToRandomize), std::end(kEnemiesToRandomize), actorId) ==
        std::end(kEnemiesToRandomize)) {
        return false;
    }

    switch (actorId) {
        // Only randomize the main component of Electric Tailparasans, not the tail segments they spawn.
        case ACTOR_EN_TP:
            return params == -1;
        // Only randomize the initial Deku Scrub actor (single and triple attack), not the flower they spawn.
        case ACTOR_EN_DEKUNUTS:
            return params == -256 || params == 768;
        // Don't randomize the OoB wallmaster in the Silver Rupee room because it's only there to
        // not trigger unlocking the door after killing the other wallmaster in authentic gameplay.
        case ACTOR_EN_WALLMAS:
            return !(!mq && sceneNum == SCENE_GERUDO_TRAINING_GROUND && roomNum == 2 && posX == -2345);
        // Only randomize initial Floormaster actor (it can split and does some spawning on init).
        case ACTOR_EN_FLOORMAS:
            return params == 0 || params == -32768;
        // Only randomize the initial eggs, not the enemies that spawn from them.
        case ACTOR_EN_GOMA:
            return params >= 0 && params <= 9;
        // Only randomize Skullwalltulas, not Golden Skulltulas.
        case ACTOR_EN_SW:
            return params == 0;
        // Don't randomize Nabooru because it'll break the cutscene and the door.
        // Don't randomize Iron Knuckle in MQ Spirit Trial because it's needed to
        // break the thrones in the room to access a button.
        case ACTOR_EN_IK:
            return params != 1280 && !(mq && sceneNum == SCENE_INSIDE_GANONS_CASTLE && roomNum == 17);
        // Only randomize the initial spawn of the huge jellyfish. It spawns another copy when hit with a sword.
        case ACTOR_EN_VALI:
            return params == -1;
        // Don't randomize Lizalfos in Dodongo's Cavern because the gates won't work correctly otherwise.
        case ACTOR_EN_ZF:
            return params != 1280 && params != 1281 && params != 1536 && params != 1537;
        // Don't randomize the right baby dodongo on the first tunnel in Dodongo's Cavern as in vanilla you use them
        // isntead of bombs to blow up a wall
        case ACTOR_EN_DODOJR:
            return !(sceneNum == SCENE_DODONGOS_CAVERN && roomNum == 1 && posX == 1972);
        // Don't randomize the Wolfos in SFM because it's needed to open the gate.
        case ACTOR_EN_WF:
            return params != 7936;
        // Don't randomize the Stalfos in Forest Temple because other enemies fall through the hole and don't
        // trigger the platform. Don't randomize the Stalfos spawning on the boat in Shadow Temple, as
        // randomizing them places the new enemies down in the river.
        case ACTOR_EN_TEST:
            return params != 1 && !(sceneNum == SCENE_SHADOW_TEMPLE && roomNum == 21);
        // Only randomize the enemy variant of Armos Statue.
        // Leave one Armos unrandomized in the Spirit Temple room where an armos is needed to push down a
        // button.
        case ACTOR_EN_AM:
            return (params == -1 || params == 255) && !(sceneNum == SCENE_SPIRIT_TEMPLE && posX == 2141);
        // Don't randomize Shell Blades and Spikes in the underwater portion in Water Temple as it's impossible
        // to kill most other enemies underwater with just hookshot and they're required to be killed for a
        // grate to open.
        case ACTOR_EN_SB:
        case ACTOR_EN_NY:
            return !(!mq && sceneNum == SCENE_WATER_TEMPLE && roomNum == 2);
        // Don't randomize Skull Kids in Lost Woods as child as they're not enemies
        case ACTOR_EN_SKJ:
            return !(sceneNum == SCENE_LOST_WOODS && isChild);
        default:
            return true;
    }
}

void EnemyPlacement_AssignForSeed(uint64_t seed) {
    auto ctx = Rando::Context::GetInstance();
    EnemyPlacement placement;
    placement.soulPoolExcluded = ReadKeyListCvar(CVAR_SOUL_POOL_EXCLUDED);

    if (!ctx->GetOption(RSK_ENEMY_RANDOMIZER)) {
        SetSeedPlacement(placement);
        return;
    }
    uint8_t minibossMode = ctx->GetOption(RSK_ENEMY_RANDOMIZER_MINIBOSSES).Get();
    std::vector<EnemyKind> pool = GatherPoolCandidates();
    uint64_t randomState = 0;
    ShipUtils::RandInit(seed ^ kPlacementStream, &randomState);

    for (const auto& [slotKey, slot] : CollectSlots(minibossMode)) {
        std::vector<EnemyKind> fitting;
        for (const EnemyKind& candidate : pool) {
            if (FitsSlot(slotKey, slot, candidate, minibossMode)) {
                fitting.push_back(candidate);
            }
        }
        if (fitting.empty()) {
            continue;
        }
        EnemyKind chosen = ShipUtils::RandomElement(fitting, false, &randomState);
        placement.replacements[slotKey.first][slotKey.second] = chosen.key;
    }
    SetSeedPlacement(placement);
}

void EnemyPlacement_WriteSpoiler(nlohmann::ordered_json& spoiler) {
    for (const auto& [sceneNum, scene] : sSeed.replacements) {
        for (const auto& [original, replacement] : scene) {
            spoiler["enemyPlacement"][std::to_string(sceneNum)][original] = replacement;
        }
    }
    for (const std::string& soulKey : sSeed.soulPoolExcluded) {
        spoiler["soulPoolExcluded"].push_back(soulKey);
    }
}

void EnemyPlacement_ParseSpoiler(const nlohmann::json& spoiler) {
    EnemyPlacement placement;
    if (spoiler.contains("enemyPlacement") && spoiler["enemyPlacement"].is_object()) {
        for (const auto& [sceneText, scene] : spoiler["enemyPlacement"].items()) {
            int32_t sceneNum = std::stoi(sceneText);
            for (const auto& [original, replacement] : scene.items()) {
                if (replacement.is_string()) {
                    placement.replacements[sceneNum][original] = replacement.get<std::string>();
                }
            }
        }
    }
    if (spoiler.contains("soulPoolExcluded") && spoiler["soulPoolExcluded"].is_array()) {
        for (const auto& soulKey : spoiler["soulPoolExcluded"]) {
            if (soulKey.is_string()) {
                placement.soulPoolExcluded.insert(soulKey.get<std::string>());
            }
        }
    }
    SetSeedPlacement(placement);
}

void EnemyPlacement_InitSaveFile() {
    sSave = sSeed;
}

const EnemyPlacement& EnemyPlacement_GetSeed() {
    return sSeed;
}

const EnemyPlacement& EnemyPlacement_GetSave() {
    return sSave;
}

std::optional<EnemyKind> EnemyPlacement_FindSaveReplacement(int16_t sceneNum, const std::string& originalKey) {
    auto scene = sSave.replacements.find(sceneNum);
    if (scene == sSave.replacements.end()) {
        return std::nullopt;
    }
    auto placed = scene->second.find(originalKey);
    if (placed == scene->second.end()) {
        return std::nullopt;
    }
    std::optional<EnemyKind> replacement = EnemyCatalog_Find(placed->second);
    if (!replacement.has_value()) {
        SPDLOG_WARN("[EnemyPlacement] {} in scene {} is placed but no installed mod registers it", placed->second,
                    sceneNum);
    }
    return replacement;
}

bool EnemyPlacement_IsSoulExcluded(const EnemyPlacement& placement, const std::string& soulKey) {
    return placement.soulPoolExcluded.contains(soulKey);
}

bool EnemyPlacement_IsSeedSoulItemExcluded(RandomizerGet soul) {
    for (const EnemySoul& native : EnemyCatalog_GetNativeSouls()) {
        if (native.item == soul) {
            return EnemyPlacement_IsSoulExcluded(sSeed, native.key);
        }
    }
    return false;
}

std::vector<RandomizerGet> EnemyPlacement_GetSeedCustomSoulItems() {
    std::vector<RandomizerGet> items;
    for (const auto& [soulKey, name] : CustomSoulEntries()) {
        if (EnemyPlacement_IsSoulExcluded(sSeed, soulKey)) {
            continue;
        }
        int32_t soulItem = RandoItems_GetRandoGet(("soul." + soulKey).c_str());
        if (soulItem != RG_NONE) {
            items.push_back(static_cast<RandomizerGet>(soulItem));
        }
    }
    return items;
}

const EnemyLogicPlan* EnemyPlacement_FindLogicPlan(int32_t sceneNum, RandomizerEnemy original) {
    if (sceneNum < 0 || sLogicPlans.empty()) {
        return nullptr;
    }
    auto scene = sLogicPlans.find(sceneNum);
    if (scene == sLogicPlans.end()) {
        return nullptr;
    }
    auto plan = scene->second.find(static_cast<int32_t>(original));
    return plan != scene->second.end() ? &plan->second : nullptr;
}

void EnemyPlacement_DrawPoolMenu() {
    const bool isEnemyRandomizerOn = CVarGetInteger(CVAR_RANDOMIZER_SETTING("EnemyRandomizer"), RO_GENERIC_OFF);
    const bool areSoulsShuffled = CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleEnemySouls"), RO_GENERIC_OFF);
    if (!isEnemyRandomizerOn && !areSoulsShuffled) {
        ImGui::TextDisabled("Turn on the Enemy Randomizer or Shuffle Enemy Souls to choose their pools.");
        return;
    }
    if (isEnemyRandomizerOn) {
        DrawPoolList("Enemy Pool", CVAR_ENEMY_POOL_EXCLUDED, ListEnemyPool(false));
        const int32_t minibossMode =
            CVarGetInteger(CVAR_RANDOMIZER_SETTING("EnemyRandomizerMinibosses"), ENEMY_MINIBOSSES_VANILLA);
        if (minibossMode != ENEMY_MINIBOSSES_VANILLA) {
            DrawPoolList("Miniboss Pool", CVAR_ENEMY_POOL_EXCLUDED, ListEnemyPool(true));
        }
    }
    if (areSoulsShuffled) {
        std::vector<PoolEntry> souls;
        for (const EnemySoul& soul : EnemyCatalog_GetNativeSouls()) {
            souls.push_back({ soul.key, soul.name });
        }
        for (const auto& [soulKey, name] : CustomSoulEntries()) {
            souls.push_back({ soulKey, name });
        }
        DrawPoolList("Soul Pool", CVAR_SOUL_POOL_EXCLUDED, souls);
    }
}
