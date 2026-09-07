#include "debugconsole.h"
#include <ship/utils/Utils.h>
#include "savestates.h"
#include "soh/ActorDB.h"

#include <vector>
#include <string>
#include <map>
#include <deque>
#include <cctype>
#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"                 // CVAR_RANDOMIZER_ENHANCEMENT (give_all fast)
#include "soh/Enhancements/enhancementTypes.h" // SGIA_* (Skip Get Item Animation)
#include <soh/Enhancements/item-tables/ItemTableManager.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/FleetShipCombo/FleetShipCombo.h"
#include "soh/Enhancements/cosmetics/CosmeticsEditor.h"
#include "soh/Enhancements/audio/AudioEditor.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"

#define Path _Path
#define PATH_HACK
#include <ship/utils/StringHelper.h>

#include <ship/window/Window.h>
#include <ship/Context.h>
#undef PATH_HACK
#undef Path

extern "C" {
#include <z64.h>
#include "variables.h"
#include "functions.h"
#include "macros.h"
extern PlayState* gPlayState;
}

#include "mods/nei_save.h"           // Skijer's NEI
#include "mods/extended_inventory.h" // Nei_FindByRg — clasifica un RG como item NEI (Skijer's NEI)

#include <libultraship/bridge.h>
#include <libultraship/libultraship.h>

#define CMD_REGISTER Ship::Context::GetRawInstance()->GetConsole()->AddCommand
// TODO: Commands should be using the output passed in.
#define ERROR_MESSAGE                                                                    \
    std::reinterpret_pointer_cast<Ship::ConsoleWindow>(                                  \
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Console")) \
        ->SendErrorMessage
#define INFO_MESSAGE                                                                     \
    std::reinterpret_pointer_cast<Ship::ConsoleWindow>(                                  \
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Console")) \
        ->SendInfoMessage

static bool ActorSpawnHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                              std::string* output) {
    if ((args.size() != 9) && (args.size() != 3) && (args.size() != 6)) {
        ERROR_MESSAGE("Not enough arguments passed to actorspawn");
        return 1;
    }

    if (gPlayState == nullptr) {
        ERROR_MESSAGE("PlayState == nullptr");
        return 1;
    }

    Player* player = GET_PLAYER(gPlayState);
    PosRot spawnPoint;
    const s16 nameId = ActorDB::Instance->RetrieveId(args[1]);
    s16 actorId = 0;
    if (nameId == -1) {
        try {
            actorId = std::stoi(args[1]);
        } catch ([[maybe_unused]] std::invalid_argument const& ex) {
            ERROR_MESSAGE("Invalid actor ID");
            return 1;
        }
    } else {
        actorId = nameId;
    }
    const s16 params = std::stoi(args[2]);

    spawnPoint = player->actor.world;

    switch (args.size()) {
        case 9:
            if (args[6][0] != ',') {
                spawnPoint.rot.x = std::stoi(args[6]);
            }
            if (args[7][0] != ',') {
                spawnPoint.rot.y = std::stoi(args[7]);
            }
            if (args[8][0] != ',') {
                spawnPoint.rot.z = std::stoi(args[8]);
            }
            [[fallthrough]];
        case 6:
            if (args[3][0] != ',') {
                spawnPoint.pos.x = static_cast<f32>(std::stoi(args[3]));
            }
            if (args[4][0] != ',') {
                spawnPoint.pos.y = static_cast<f32>(std::stoi(args[4]));
            }
            if (args[5][0] != ',') {
                spawnPoint.pos.z = static_cast<f32>(std::stoi(args[5]));
            }
    }

    if (Actor_Spawn(&gPlayState->actorCtx, gPlayState, actorId, spawnPoint.pos.x, spawnPoint.pos.y, spawnPoint.pos.z,
                    spawnPoint.rot.x, spawnPoint.rot.y, spawnPoint.rot.z, params) == NULL) {
        ERROR_MESSAGE("Failed to spawn actor. Actor_Spawn returned NULL");
        return 1;
    }
    return 0;
}

static bool KillPlayerHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>&,
                              std::string* output) {
    GameInteractionEffect::SetPlayerHealth effect;
    effect.parameters[0] = 0;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] You've met with a terrible fate, haven't you?");
        return 0;
    } else {
        ERROR_MESSAGE("[SOH] Command failed: Could not kill player.");
        return 1;
    }
}

static bool SetPlayerHealthHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                                   std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    int health;

    try {
        health = std::stoi(args[1]);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Health value must be an integer.");
        return 1;
    }

    if (health < 0) {
        ERROR_MESSAGE("[SOH] Health value must be a positive integer");
        return 1;
    }

    GameInteractionEffect::SetPlayerHealth effect;
    effect.parameters[0] = health;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Player health updated to %d", health);
        return 0;
    } else {
        ERROR_MESSAGE("[SOH] Command failed: Could not set player health.");
        return 1;
    }
}

static bool LoadSceneHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>&,
                             std::string* output) {
    gSaveContext.respawnFlag = 0;
    gSaveContext.seqId = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    return 0;
}

static bool RupeeHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                         std::string* output) {
    if (args.size() < 2) {
        return 1;
    }

    int rupeeAmount;
    try {
        rupeeAmount = std::stoi(args[1]);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Rupee count must be an integer.");
        return 1;
    }

    if (rupeeAmount < 0) {
        ERROR_MESSAGE("[SOH] Rupee count must be positive");
        return 1;
    }

    gSaveContext.rupees = rupeeAmount;

    INFO_MESSAGE("Set rupee count to %u", rupeeAmount);
    return 0;
}

static bool SetPosHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string> args,
                          std::string* output) {
    if (gPlayState == nullptr) {
        ERROR_MESSAGE("PlayState == nullptr");
        return 1;
    }

    Player* player = GET_PLAYER(gPlayState);

    if (args.size() == 1) {
        INFO_MESSAGE("Player position is [ %.2f, %.2f, %.2f ]", player->actor.world.pos.x, player->actor.world.pos.y,
                     player->actor.world.pos.z);
        return 0;
    }
    if (args.size() < 4)
        return 1;

    player->actor.world.pos.x = std::stof(args[1]);
    player->actor.world.pos.y = std::stof(args[2]);
    player->actor.world.pos.z = std::stof(args[3]);

    INFO_MESSAGE("Set player position to [ %.2f, %.2f, %.2f ]", player->actor.world.pos.x, player->actor.world.pos.y,
                 player->actor.world.pos.z);
    return 0;
}

// The raw reset, callable WITHOUT signaling the combo (used by the responder pump so a paired reset
// never ping-pongs). extern "C" so FleetSync's cross-game restart pump can call it.
extern "C" void FleetCombo_DoLocalReset(void) {
    if (gGameState == nullptr) {
        return;
    }
    SET_NEXT_GAMESTATE(gGameState, TitleSetup_Init, GameState);
    gGameState->running = false;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnExitGame>(gSaveContext.fileNum);
}

static bool ResetHandler(std::shared_ptr<Ship::Console> Console, std::vector<std::string> args, std::string* output) {
    if (gGameState == nullptr) {
        ERROR_MESSAGE("gGameState == nullptr");
        return 1;
    }
    FleetCombo_DoLocalReset();
    FleetShipCombo_SignalRestart(); // combo: restart the paired game too (no-op outside the combo)
    // Both games are restarting; make sure the one the player ends up looking at is THIS one. MM's
    // title screen is not a screen the combo ever shows.
    FleetShipCombo_YieldToOoT();
    return 0;
}

const static std::map<std::string, uint16_t> ammoItems{
    { "sticks", ITEM_STICK }, { "nuts", ITEM_NUT },         { "bombs", ITEM_BOMB }, { "seeds", ITEM_SLINGSHOT },
    { "arrows", ITEM_BOW },   { "bombchus", ITEM_BOMBCHU }, { "beans", ITEM_BEAN },
};

static bool AddAmmoHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                           std::string* output) {
    if (args.size() < 3) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    int amount;

    try {
        amount = std::stoi(args[2]);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("Ammo count must be an integer");
        return 1;
    }

    if (amount < 0) {
        ERROR_MESSAGE("Ammo count must be positive");
        return 1;
    }

    const auto& it = ammoItems.find(args[1]);
    if (it == ammoItems.end()) {
        ERROR_MESSAGE(
            "Invalid ammo type. Options are 'sticks', 'nuts', 'bombs', 'seeds', 'arrows', 'bombchus' and 'beans'");
        return 1;
    }

    GameInteractionEffect::AddOrTakeAmmo effect;
    effect.parameters[0] = amount;
    effect.parameters[1] = it->second;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Added ammo.");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not add ammo.");
        return 1;
    }
}

static bool TakeAmmoHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                            std::string* output) {
    if (args.size() < 3) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    int amount;

    try {
        amount = std::stoi(args[2]);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("Ammo count must be an integer");
        return 1;
    }

    if (amount < 0) {
        ERROR_MESSAGE("Ammo count must be positive");
        return 1;
    }

    const auto& it = ammoItems.find(args[1]);
    if (it == ammoItems.end()) {
        ERROR_MESSAGE(
            "Invalid ammo type. Options are 'sticks', 'nuts', 'bombs', 'seeds', 'arrows', 'bombchus' and 'beans'");
        return 1;
    }

    GameInteractionEffect::AddOrTakeAmmo effect;
    effect.parameters[0] = -amount;
    effect.parameters[1] = it->second;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Took ammo.");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not take ammo.");
        return 1;
    }
}

const static std::map<std::string, uint16_t> bottleItems{
    { "green_potion", ITEM_POTION_GREEN },
    { "red_potion", ITEM_POTION_RED },
    { "blue_potion", ITEM_POTION_BLUE },
    { "milk", ITEM_MILK },
    { "half_milk", ITEM_MILK_HALF },
    { "fairy", ITEM_FAIRY },
    { "bugs", ITEM_BUG },
    { "fish", ITEM_FISH },
    { "poe", ITEM_POE },
    { "big_poe", ITEM_BIG_POE },
    { "blue_fire", ITEM_BLUE_FIRE },
    { "rutos_letter", ITEM_LETTER_RUTO },
};

static bool BottleHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                          std::string* output) {
    if (args.size() < 3) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }

    unsigned int slot;
    try {
        slot = std::stoi(args[2]);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Bottle slot must be an integer.");
        return 1;
    }

    if ((slot < 1) || (slot > 4)) {
        ERROR_MESSAGE("Invalid slot passed");
        return 1;
    }

    const auto& it = bottleItems.find(args[1]);

    if (it == bottleItems.end()) {
        ERROR_MESSAGE("Invalid item passed");
        return 1;
    }

    gSaveContext.inventory.items[0x11 + slot] = static_cast<u8>(it->second);

    return 0;
}

static bool BHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                     std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }

    gSaveContext.equips.buttonItems[0] = std::stoi(args[1]);
    return 0;
}

static bool ItemHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                        std::string* output) {
    if (args.size() < 3) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }

    { // Skijer's NEI: dispatch so custom slots (>=24) hit gNeiSave, not OOB
        int neiSlot = std::stoi(args[1]);
        u8 neiItem = static_cast<u8>(std::stoi(args[2]));
        if (neiSlot >= 0 && neiSlot < 24) {
            gSaveContext.inventory.items[neiSlot] = neiItem;
        } else if (neiSlot >= 24 && neiSlot < 72) {
            Nei_SetOwnedItem((uint8_t)neiSlot, neiItem);
        }
    }

    return 0;
}

static bool GiveItemHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string> args,
                            std::string* output) {
    if (args.size() < 3) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    GetItemEntry getItemEntry = GET_ITEM_NONE;

    if (args[1].compare("vanilla") == 0) {
        getItemEntry = ItemTableManager::Instance->RetrieveItemEntry(MOD_NONE, std::stoi(args[2]));
    } else if (args[1].compare("randomizer") == 0) {
        getItemEntry = Rando::StaticData::RetrieveItem((RandomizerGet)std::stoi(args[2])).GetGIEntry_Copy();
    } else {
        ERROR_MESSAGE("[SOH] Invalid argument passed, must be 'vanilla' or 'randomizer'");
        return 1;
    }

    GiveItemEntryWithoutActor(gPlayState, getItemEntry);

    return 0;
}

// ── give por nombre + recorrido de validación ────────────────────────────────────────────────────
// `give_item` de arriba exige el id numérico, así que probar un item concreto obligaba a buscarlo en
// el enum. Esto es el espejo exacto de lo que 2ship tiene en su DebugConsole: dar por nombre, y
// recorrer la lista entera item a item para comprobar que cada uno se entrega, se dibuja y trae su
// icono y su texto. Mismos nombres de comando en los dos juegos. Skijer's NEI
static const std::map<RandomizerGet, const char*>& RgEnumNames() {
    static const std::map<RandomizerGet, const char*> names = {
#define RANDO_ENUM_ITEM(enumName) { enumName, #enumName },
#include "soh/Enhancements/randomizer/randomizerEnums/RandomizerGet.h"
#undef RANDO_ENUM_ITEM
    };
    return names;
}

static std::string NormalizeItemName(const std::string& str) {
    std::string out;
    for (char c : str) {
        if (std::isalnum((unsigned char)c)) {
            out += (char)std::tolower((unsigned char)c);
        }
    }
    return out;
}

// ─── Walk classification (Skijer's 13 categories, 2026-08-07) ────────────────────────────────────
// One WalkSpec per RG: which give_all/give_next category it belongs to and how many copies a full
// sweep queues (progressive chains queue one copy PER LEVEL; the obtainability dedup in the pump
// drops copies of levels already owned, so there are never repeats). category == NULL excludes the
// RG from walks entirely: win conditions, non-items, the retired Hylia's Grace, rows that only
// exist as resolution targets of a progressive chain, and shop-purchase duplicates. The FULL item
// list with per-item reasoning lives in GIVE_CATEGORIES.md at the repo root (generated by
// categorize.py — keep both in sync when touching this).
struct WalkSpec {
    const char* category;
    int copies;
};

static WalkSpec WalkSpecOfRg(RandomizerGet rg) {
    auto it = RgEnumNames().find(rg);
    std::string id = (it != RgEnumNames().end()) ? it->second : "";
    auto pre = [&](const char* p) { return id.rfind(p, 0) == 0; };
    auto in = [&](std::initializer_list<const char*> l) {
        for (const char* s : l) {
            if (id == s) {
                return true;
            }
        }
        return false;
    };
    static const WalkSpec kExcluded = { nullptr, 0 };

    // Exclusions: win conditions (credits warp), non-items, retired, resolution targets, shop rows.
    if (pre("RG_BUY_")) {
        return kExcluded;
    }
    if (in({ "RG_TRIFORCE",
             "RG_TRIFORCE_PIECE",
             "RG_HINT",
             "RG_SOLD_OUT",
             "RG_HYLIAS_GRACE",
             "RG_FAIRY_OCARINA",
             "RG_OCARINA_OF_TIME",
             "RG_BOMB_BAG",
             "RG_BIG_BOMB_BAG",
             "RG_BIGGEST_BOMB_BAG",
             "RG_FAIRY_BOW",
             "RG_BIG_QUIVER",
             "RG_BIGGEST_QUIVER",
             "RG_FAIRY_SLINGSHOT",
             "RG_BIG_BULLET_BAG",
             "RG_BIGGEST_BULLET_BAG",
             "RG_GORONS_BRACELET",
             "RG_SILVER_GAUNTLETS",
             "RG_GOLDEN_GAUNTLETS",
             "RG_SILVER_SCALE",
             "RG_GOLDEN_SCALE",
             "RG_ADULT_WALLET",
             "RG_GIANT_WALLET",
             "RG_TYCOON_WALLET",
             "RG_DEKU_NUT_CAPACITY_30",
             "RG_DEKU_NUT_CAPACITY_40",
             "RG_DEKU_STICK_CAPACITY_20",
             "RG_DEKU_STICK_CAPACITY_30",
             "RG_DEKU_STICK_BAG",
             "RG_DEKU_NUT_BAG",
             "RG_HOOKSHOT",
             "RG_LONGSHOT",
             "RG_MAGIC_SINGLE",
             "RG_MAGIC_DOUBLE",
             "RG_QUIVER_INF",
             "RG_BOMB_BAG_INF",
             "RG_BULLET_BAG_INF",
             "RG_STICK_UPGRADE_INF",
             "RG_NUT_UPGRADE_INF",
             "RG_MAGIC_INF",
             "RG_BOMBCHU_INF",
             "RG_WALLET_INF",
             "RG_ROCS_CAPE",
             "RG_PROGRESSIVE_GORONSWORD",
             "RG_ELEMENTAL_WAND",
             "RG_SHEIKAH_SLATE", // lo representan sus 4 RG_SLATE_RUNE_* (como la wand y sus rods)
             "RG_MM_TIME_PROGRESSIVE",
             "RG_MM_SONG_LULLABY_PROGRESSIVE",
             "RG_MM_STRAY_FAIRY",
             "RG_QUARTZ_OF_MOTION",
             "RG_CANE_PACCI_FLIP",
             "RG_CANE_SOMARIA_BLOCK",
             "RG_CANE_PACCI_STONE",
             "RG_CANE_SOMARIA_PLATFORM",
             "RG_CANE_PACCI_ULTRAHAND" })) {
        return kExcluded; // (Quartz y skills de cane los reparten sus cadenas: Agony x2 / Cane x6)
    }
    // The four NEI weapon chains: no category (their levels are the rows the category walks give),
    // but they DO carry a copy count, because `give_all progressive` hands out the PARENT so the
    // chain runs through the randomizer's own resolution — the same path a seed takes. Skijer's NEI
    if (id == "RG_PROGRESSIVE_KOKIRI_SWORD") {
        return { nullptr, 3 }; // Kokiri -> Razor -> Gilded
    }
    if (in({ "RG_PROGRESSIVE_MASTER_SWORD", "RG_PROGRESSIVE_BGS", "RG_PROGRESSIVE_HAMMER" })) {
        return { nullptr, 2 }; // base weapon -> its NEI upgrade
    }

    // MM (prefixes first — the cosmetic MM masks are ALSO in the NEI registry, so this must win).
    if (pre("RG_MM_MASK_")) {
        return { "mm_masks", 1 };
    }
    if (in({ "RG_MM_SOUL_ODOLWA", "RG_MM_SOUL_GOHT", "RG_MM_SOUL_GYORG", "RG_MM_SOUL_TWINMOLD",
             "RG_MM_SOUL_MAJORA" })) {
        return { "mm_dungeons", 1 }; // boss souls van con el relleno de mazmorra
    }
    if (pre("RG_MM_SOUL_") || id == "RG_MM_GREAT_SPIN_ATTACK") {
        return { "mm_skills", 1 }; // enemy souls habilitan algo, como las skills
    }
    if (pre("RG_MM_SONG_") || pre("RG_MM_OWL_") || pre("RG_MM_TINGLE_MAP_") || pre("RG_MM_FROG_") ||
        pre("RG_MM_TIME_") || pre("RG_MM_REMAINS_") || pre("RG_MM_GS_TOKEN_")) {
        return { "mm_collectables", 1 };
    }
    if (pre("RG_MM_STRAY_FAIRY_") || pre("RG_MM_SMALL_KEY_") || pre("RG_MM_BOSS_KEY_") || pre("RG_MM_MAP_") ||
        pre("RG_MM_COMPASS_")) {
        return { "mm_dungeons", 1 };
    }
    if (pre("RG_MM_")) {
        return { "mm_items", 1 }; // pictobox, keg, gold dust, trade quest, notebook
    }

    // NEI per-level weapon upgrades (Skijer): L1 queda como el arma vanilla en oot_items.
    if (in({ "RG_RAZOR_SWORD", "RG_GILDED_SWORD", "RG_TRUE_MASTER_SWORD", "RG_GREAT_FAIRY_SWORD", "RG_IRON_KNUCKLE_AXE",
             "RG_ULTRASHOT" })) {
        return { "oot_nei_upgrades", 1 };
    }

    // NEI custom items.
    if (id == "RG_CANE_OF_SOMARIA") {
        return { "nei_items", 6 }; // 6 skills en 1 slot: cada copia enciende la SIGUIENTE
    }
    if (pre("RG_SLATE_RUNE_")) {
        return { "nei_items", 1 }; // las 4 runas del slate: items hermanos como los RG_WAND_*
    }
    if (id == "RG_PROGRESSIVE_ROCS") {
        return { "nei_items", 2 }; // Roc's Feather (Skijer) -> Roc's Cape
    }
    if (pre("RG_EXT_") || pre("RG_SW97_") || pre("RG_NEI_SONG_") || pre("RG_WAND_") ||
        in({ "RG_WHIP",
             "RG_SPINNER",
             "RG_BOMB_ARROWS",
             "RG_FIRE_ROD",
             "RG_DEMISE_DESTRUCTION",
             "RG_DEKU_LEAF",
             "RG_TIME_GATE",
             "RG_BEETLE",
             "RG_SWITCH_HOOK",
             "RG_ICE_ROD",
             "RG_ZONAI_PERMAFROST",
             "RG_MOGMA_MITTS",
             "RG_GUST_JAR",
             "RG_BALL_AND_CHAIN",
             "RG_LANTERN",
             "RG_LIGHT_ROD",
             "RG_SHOVEL",
             "RG_DOMINION_ROD",
             "RG_DESIRE_SENSOR",
             "RG_MINISH_CAP",
             "RG_CHATEAU_ROMANI",
             "RG_POKEBALL",
             "RG_CLAWSHOT",
             "RG_MARIO_MASK",
             "RG_NET",
             "RG_BOTTOMLESS_BOTTLE",
             "RG_BOTTLE_WITH_MAGIC_MUSHROOM",
             "RG_PHANTOM_HOURGLASS",
             "RG_SHADOW_CRYSTAL",
             "RG_ROD_OF_SEASONS",
             "RG_SEASON_SPRING",
             "RG_SEASON_SUMMER",
             "RG_SEASON_AUTUMN",
             "RG_SEASON_WINTER" })) {
        return { "nei_items", 1 };
    }

    // OoT skills (antes que dungeons: los BEAN_SOUL contienen "_SOUL").
    if (pre("RG_SPEAK_") || id.find("_BEAN_SOUL") != std::string::npos ||
        in({ "RG_CLIMB", "RG_CRAWL", "RG_OPEN_CHEST", "RG_POWER_BRACELET", "RG_BRONZE_SCALE", "RG_CHILD_WALLET" })) {
        return { "oot_skills", 1 };
    }

    // OoT dungeons: boss souls + mapas/brújulas/llaves (incluye house keys y Skeleton Key).
    if (id.find("_SOUL") != std::string::npos || id.find("_MAP") != std::string::npos ||
        id.find("_COMPASS") != std::string::npos || id.find("_KEY") != std::string::npos) {
        return { "oot_dungeons", 1 };
    }

    // OoT collectables (pantalla de quest/collect): canciones, medallones, piedras, Agony, etc.
    if (id == "RG_STONE_OF_AGONY") {
        return { "oot_collectables", 2 }; // L1 piedra vanilla, L2 Quartz of Motion (mismo RG)
    }
    if (in({ "RG_ZELDAS_LULLABY",     "RG_EPONAS_SONG",
             "RG_SARIAS_SONG",        "RG_SUNS_SONG",
             "RG_SONG_OF_TIME",       "RG_SONG_OF_STORMS",
             "RG_MINUET_OF_FOREST",   "RG_BOLERO_OF_FIRE",
             "RG_SERENADE_OF_WATER",  "RG_REQUIEM_OF_SPIRIT",
             "RG_NOCTURNE_OF_SHADOW", "RG_PRELUDE_OF_LIGHT",
             "RG_KOKIRI_EMERALD",     "RG_GORON_RUBY",
             "RG_ZORA_SAPPHIRE",      "RG_FOREST_MEDALLION",
             "RG_FIRE_MEDALLION",     "RG_WATER_MEDALLION",
             "RG_SPIRIT_MEDALLION",   "RG_SHADOW_MEDALLION",
             "RG_LIGHT_MEDALLION",    "RG_GOLD_SKULLTULA_TOKEN",
             "RG_PIECE_OF_HEART",     "RG_HEART_CONTAINER",
             "RG_DOUBLE_DEFENSE",     "RG_GERUDO_MEMBERSHIP_CARD",
             "RG_GREG_RUPEE" })) {
        return { "oot_collectables", 1 };
    }

    // OoT junk (relleno consumible; el Ice Trap congela una vez al pasar).
    if (in({ "RG_RECOVERY_HEART",
             "RG_GREEN_RUPEE",
             "RG_BLUE_RUPEE",
             "RG_RED_RUPEE",
             "RG_PURPLE_RUPEE",
             "RG_HUGE_RUPEE",
             "RG_MILK",
             "RG_FISH",
             "RG_BOMBS_5",
             "RG_BOMBS_10",
             "RG_BOMBS_20",
             "RG_BOMBCHU_5",
             "RG_BOMBCHU_10",
             "RG_BOMBCHU_20",
             "RG_ARROWS_5",
             "RG_ARROWS_10",
             "RG_ARROWS_30",
             "RG_DEKU_NUTS_5",
             "RG_DEKU_NUTS_10",
             "RG_DEKU_SEEDS_30",
             "RG_DEKU_STICK_1",
             "RG_STICKS",
             "RG_NUTS",
             "RG_RED_POTION_REFILL",
             "RG_GREEN_POTION_REFILL",
             "RG_BLUE_POTION_REFILL",
             "RG_TREASURE_GAME_HEART",
             "RG_TREASURE_GAME_GREEN_RUPEE",
             "RG_ICE_TRAP" })) {
        return { "oot_junk", 1 };
    }

    // Todo lo demás: página de items/equipment vanilla de OoT. Copias por nivel para las cadenas
    // (contando los arranques por-setting: Bronze Scale con swim shuffle, Child Wallet con wallet
    // shuffle — la copia sobra y se descarta sola cuando el setting no está).
    int copies = 1;
    if (id == "RG_PROGRESSIVE_HOOKSHOT") {
        copies = 3; // Hookshot -> Longshot -> Ultrashot (the NEI level 3 resolves off the Longshot)
    } else if (id == "RG_PROGRESSIVE_OCARINA") {
        copies = 2; // fairy -> Ocarina of Time
    } else if (id == "RG_PROGRESSIVE_SCALE") {
        copies = 3; // bronze(si swim shuffle)->silver->gold
    } else if (in({ "RG_PROGRESSIVE_STRENGTH", "RG_PROGRESSIVE_NUT_UPGRADE", "RG_PROGRESSIVE_STICK_UPGRADE",
                    "RG_PROGRESSIVE_WALLET" })) {
        copies = 4; // grab->goron->silver->golden / bolsas+capacidades / child->adult->giant->tycoon
    } else if (id == "RG_PROGRESSIVE_BOMBCHU_BAG") {
        copies = 1;
    } else if (pre("RG_PROGRESSIVE_")) {
        copies = 3; // bomb bag / bow / slingshot / magic(simple->doble->INF)
    }
    return { "oot_items", copies };
}

// Categoría VIRTUAL "progressive": SOLO las cadenas, agrupadas por cadena y de nivel más bajo a más
// alto, para verlas subir una tras otra in-game (give_all progressive desfila con presentación; el
// dedup descarta los niveles que el save ya tenga). Los items también viven en su categoría normal.
static const RandomizerGet kProgressiveWalk[] = {
    // ALWAYS the parent item, never the per-level rows: a seed only ever places the progressive,
    // so giving the parent N times is what actually exercises the resolution the randomizer uses
    // (that is the whole point of testing chains without generating a seed).
    RG_PROGRESSIVE_KOKIRI_SWORD, // x3 Kokiri -> Razor -> Gilded
    RG_PROGRESSIVE_MASTER_SWORD, // x2 Master -> True Master
    RG_PROGRESSIVE_BGS,          // x2 Biggoron -> Great Fairy's
    RG_PROGRESSIVE_HAMMER,       // x2 Hammer -> Iron Knuckle's Axe
    RG_PROGRESSIVE_HOOKSHOT,     // x3 Hookshot -> Longshot -> Ultrashot
    RG_PROGRESSIVE_STRENGTH,     // x4
    RG_PROGRESSIVE_SCALE,        // x3
    RG_PROGRESSIVE_WALLET,       // x4
    RG_PROGRESSIVE_BOMB_BAG,
    RG_PROGRESSIVE_BOW,
    RG_PROGRESSIVE_SLINGSHOT, // x3 c/u
    RG_PROGRESSIVE_NUT_UPGRADE,
    RG_PROGRESSIVE_STICK_UPGRADE, // x4 c/u
    RG_PROGRESSIVE_MAGIC_METER,   // x3
    RG_PROGRESSIVE_OCARINA,       // x2
    RG_PROGRESSIVE_BOMBCHU_BAG,   // x1
    RG_STONE_OF_AGONY,            // x2 (piedra -> Quartz)
    RG_PROGRESSIVE_ROCS,          // x2 (pluma -> capa)
    RG_CANE_OF_SOMARIA,           // x6 (las 6 skills)
    // Los 6 rods de la Elemental Wand: items hermanos sobre un slot (cada uno con su textbox).
    RG_WAND_SAND_ROD,
    RG_WAND_TORNADO_ROD,
    RG_WAND_WATER_ROD,
    RG_WAND_METEOR_ROD,
    RG_WAND_STORM_ROD,
    RG_WAND_SHADOW_SCEPTER,
    // Las 4 runas del Sheikah Slate: items hermanos sobre un slot (cada una con su textbox).
    RG_SLATE_RUNE_BOMB,
    RG_SLATE_RUNE_MASTER_CYCLE,
    RG_SLATE_RUNE_STASIS,
    RG_SLATE_RUNE_CRYONIS,
};

static const char* kWalkUsage = "all|progressive|oot_items|oot_nei_upgrades|oot_collectables|oot_skills|"
                                "oot_dungeons|oot_junk|nei_items|mm_masks|mm_items|mm_collectables|"
                                "mm_dungeons|mm_skills|mm_junk";

// itemTable es un array indexado por RG, así que un RG sin fila devuelve una entrada en blanco en vez
// de fallar. Se filtran aquí para no recorrer (ni dar) cientos de items vacíos.
static bool RgHasTableEntry(RandomizerGet rg) {
    return Rando::StaticData::RetrieveItem(rg).GetRandomizerGet() != RG_NONE;
}

static std::vector<RandomizerGet> BuildWalkList(const std::string& category) {
    std::vector<RandomizerGet> list;
    if (category == "progressive") {
        // Orden explícito por cadena (L1 primero) en vez del orden del enum.
        for (RandomizerGet rg : kProgressiveWalk) {
            if (RgHasTableEntry(rg)) {
                list.push_back(rg);
            }
        }
        return list;
    }
    for (auto& [rg, name] : RgEnumNames()) {
        if (rg == RG_NONE || rg == RG_MAX || !RgHasTableEntry(rg)) {
            continue;
        }
        WalkSpec spec = WalkSpecOfRg(rg);
        if (spec.category == nullptr) {
            continue; // excluido del walk (triforce, targets de resolución, retirados...)
        }
        if (!category.empty() && category != "all" && category != spec.category) {
            continue;
        }
        list.push_back(rg);
    }
    return list;
}

static bool GiveRgWithPresentation(RandomizerGet rg) {
    if (gPlayState == nullptr) {
        ERROR_MESSAGE("gPlayState == nullptr");
        return false;
    }
    GetItemEntry entry = Rando::StaticData::RetrieveItem(rg).GetGIEntry_Copy();
    GiveItemEntryWithoutActor(gPlayState, entry);
    return true;
}

static std::string sWalkCategory = "all";
static std::vector<RandomizerGet> sWalkList;
static int sWalkIndex = -1;

// step: +1 siguiente, -1 anterior, 0 repetir el actual (siguiente nivel de una cadena progresiva).
static bool WalkStep(int step, const std::vector<std::string>& args, size_t categoryArg) {
    std::string requested = (args.size() > categoryArg) ? args[categoryArg] : "";
    if (!requested.empty() && requested != sWalkCategory) {
        sWalkCategory = requested;
        sWalkList.clear();
        sWalkIndex = -1;
    }
    if (sWalkList.empty()) {
        sWalkList = BuildWalkList(sWalkCategory);
        if (sWalkList.empty()) {
            ERROR_MESSAGE("[SOH] No items in category \"%s\"", sWalkCategory.c_str());
            return false;
        }
    }

    int next = sWalkIndex + step;
    if (step == 0 && sWalkIndex < 0) {
        next = 0;
    }
    if (next < 0) {
        ERROR_MESSAGE("[SOH] Already at the start of \"%s\"", sWalkCategory.c_str());
        return false;
    }
    if (next >= (int)sWalkList.size()) {
        INFO_MESSAGE("[SOH] End of \"%s\" (%d items). give_reset to start over.", sWalkCategory.c_str(),
                     (int)sWalkList.size());
        return false;
    }
    if (!GiveRgWithPresentation(sWalkList[next])) {
        return false;
    }

    sWalkIndex = next;
    RandomizerGet rg = sWalkList[sWalkIndex];
    auto it = RgEnumNames().find(rg);
    WalkSpec spec = WalkSpecOfRg(rg);
    INFO_MESSAGE("[SOH] %d/%d  %s  (%s, %s)", sWalkIndex + 1, (int)sWalkList.size(),
                 Rando::StaticData::RetrieveItem(rg).GetName().GetEnglish().c_str(),
                 spec.category != nullptr ? spec.category : "?", (it != RgEnumNames().end()) ? it->second : "?");
    return true;
}

static bool GiveByNameHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                              std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Usage: give <item name>  (e.g. \"give rocs feather\", \"give list roc\")");
        return 1;
    }

    bool listOnly = args[1] == "list";
    std::string query;
    for (size_t i = listOnly ? 2 : 1; i < args.size(); i++) {
        query += args[i];
    }
    std::string needle = NormalizeItemName(query);

    if (!listOnly && needle.empty()) {
        ERROR_MESSAGE("[SOH] No item name passed");
        return 1;
    }

    RandomizerGet exact = RG_NONE;
    std::vector<std::pair<RandomizerGet, std::string>> matches;

    for (auto& [rg, enumName] : RgEnumNames()) {
        if (rg == RG_NONE || rg == RG_MAX || !RgHasTableEntry(rg)) {
            continue;
        }
        std::string idName = NormalizeItemName(enumName);
        std::string displayName = NormalizeItemName(Rando::StaticData::RetrieveItem(rg).GetName().GetEnglish());

        if (!needle.empty() && (needle == idName || needle == displayName)) {
            exact = rg;
            break;
        }
        if (needle.empty() || idName.find(needle) != std::string::npos ||
            displayName.find(needle) != std::string::npos) {
            matches.emplace_back(rg,
                                 Rando::StaticData::RetrieveItem(rg).GetName().GetEnglish() + "  [" + enumName + "]");
        }
    }

    if (listOnly) {
        if (matches.empty()) {
            ERROR_MESSAGE("[SOH] No item matches \"%s\"", query.c_str());
            return 1;
        }
        INFO_MESSAGE("[SOH] %d item(s) match:", (int)matches.size());
        for (auto& [rg, label] : matches) {
            INFO_MESSAGE("  %s", label.c_str());
        }
        return 0;
    }

    if (exact == RG_NONE) {
        if (matches.empty()) {
            ERROR_MESSAGE("[SOH] No item matches \"%s\"", query.c_str());
            return 1;
        }
        if (matches.size() > 1) {
            ERROR_MESSAGE("[SOH] \"%s\" is ambiguous, %d matches:", query.c_str(), (int)matches.size());
            for (size_t i = 0; i < matches.size() && i < 20; i++) {
                ERROR_MESSAGE("  %s", matches[i].second.c_str());
            }
            if (matches.size() > 20) {
                ERROR_MESSAGE("  ...and %d more (use `give list %s`)", (int)matches.size() - 20, query.c_str());
            }
            return 1;
        }
        exact = matches[0].first;
    }

    if (!GiveRgWithPresentation(exact)) {
        return 1;
    }
    INFO_MESSAGE("[SOH] Giving %s", Rando::StaticData::RetrieveItem(exact).GetName().GetEnglish().c_str());
    return 0;
}

static bool GiveNextHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                            std::string* output) {
    return WalkStep(1, args, 1) ? 0 : 1;
}

static bool GivePrevHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                            std::string* output) {
    return WalkStep(-1, args, 1) ? 0 : 1;
}

static bool GiveAgainHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                             std::string* output) {
    return WalkStep(0, args, 1) ? 0 : 1;
}

static bool GiveResetHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                             std::string* output) {
    sWalkCategory = (args.size() > 1) ? args[1] : "all";
    sWalkList = BuildWalkList(sWalkCategory);
    sWalkIndex = -1;
    if (sWalkList.empty()) {
        ERROR_MESSAGE("[SOH] No items in category \"%s\". Categories: %s", sWalkCategory.c_str(), kWalkUsage);
        return 1;
    }
    INFO_MESSAGE("[SOH] Walk reset: \"%s\", %d items. give_next to start.", sWalkCategory.c_str(),
                 (int)sWalkList.size());
    return 0;
}

// (Las copias por cadena progresiva viven ahora en WalkSpecOfRg — una copia por nivel, ver
// GIVE_CATEGORIES.md.)

// give_all queue: Randomizer_Item_Give REJECTS vanilla GetItemEntries (modIndex != MOD_RANDOMIZER
// asserts and returns -1), which silently skipped every native OoT item — bow, bombs, medallions...
// The one give path that handles BOTH mod indexes AND plays the real presentation is the GI flow
// (GiveItemEntryWithoutActor), but only one item can be in flight, so give_all queues the RGs and a
// player-update pump hands them out one after another, exactly like the randomizer's own item queue
// (RandomizerOnPlayerUpdateForItemQueueHandler). Entries resolve at POP time so progressives step
// through their levels. Skijer's NEI
static std::deque<RandomizerGet> sGiveAllQueue;

static void EnsureGiveAllPump() {
    static bool sHooked = false;
    if (sHooked) {
        return;
    }
    sHooked = true;
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>([]() {
        if (sGiveAllQueue.empty() || gPlayState == nullptr) {
            return;
        }
        Player* player = GET_PLAYER(gPlayState);
        if (player == NULL || Player_InBlockingCsMode(gPlayState, player) ||
            player->stateFlags1 & PLAYER_STATE1_IN_ITEM_CS || player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM ||
            player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
            return;
        }
        // Espera a que el textbox anterior cierre DEL TODO: los flags del player se limpian un
        // frame antes que msgMode, y dar el siguiente item con el mensaje aún vivo pisa
        // player->getItemEntry — todos los textbox salían con la MISMA descripción.
        if (gPlayState->msgCtx.msgMode != MSGMODE_NONE) {
            return;
        }
        // Dedup ("evita repeateds"): a step the save already holds — a maxed chain, a unique item
        // already owned — is dropped instead of re-presenting the same level again. Progressive
        // chains queue a generous number of copies and rely on this to stop exactly at their top.
        while (!sGiveAllQueue.empty()) {
            RandomizerGet rg = sGiveAllQueue.front();
            sGiveAllQueue.pop_front();
            if (OTRGlobals::Instance->gRandomizer->GetItemObtainabilityFromRandomizerGet(rg) != CAN_OBTAIN) {
                continue;
            }
            GiveItemEntryWithoutActor(gPlayState, Rando::StaticData::RetrieveItem(rg).GetGIEntry_Copy());
            break;
        }
        if (sGiveAllQueue.empty()) {
            INFO_MESSAGE("[SOH] give_all queue finished.");
        }
    });
}

// Give an entry with NO presentation at all — no raise animation, no textbox. Same dispatch the
// Anchor uses (GiveItem.cpp): the entry's modIndex decides which grant function applies, which is
// the whole reason give_all cannot just call one of them. The "Skip Get Item Animation" enhancement
// does NOT reach here: it only rewrites the randomizer's own CHECK flow (hook_handlers.cpp swaps the
// give for an Item_DropCollectible), and a console give never goes through a check. Skijer's NEI
static void GiveEntrySilently(GetItemEntry entry) {
    if (entry.modIndex == MOD_RANDOMIZER) {
        Randomizer_Item_Give(gPlayState, entry);
    } else {
        if (entry.getItemId == GI_SWORD_BGS) {
            gSaveContext.bgsFlag = true; // vanilla BGS needs its flag or HasItem() says no
        }
        Item_Give(gPlayState, static_cast<uint8_t>(entry.itemId));
    }
}

static bool GiveAllHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                           std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Usage: give_all <%s|stop> [fast]", kWalkUsage);
        return 1;
    }
    if (args[1] == "stop") {
        INFO_MESSAGE("[SOH] give_all queue cleared (%d pending).", (int)sGiveAllQueue.size());
        sGiveAllQueue.clear();
        return 0;
    }
    if (gPlayState == nullptr) {
        ERROR_MESSAGE("gPlayState == nullptr");
        return 1;
    }

    std::vector<RandomizerGet> list = BuildWalkList(args[1]);
    if (list.empty()) {
        ERROR_MESSAGE("[SOH] No items in category \"%s\"", args[1].c_str());
        return 1;
    }
    // Sin presentación: "give_all <cat> fast", o el enhancement Skip Get Item Animation puesto en
    // "All Items" (que es lo que uno espera que aplique aquí aunque el rando no lo use en este
    // camino). Al ser síncrono cada entrega actualiza el save antes de la siguiente, así que las
    // cadenas progresivas siguen escalando nivel a nivel igual que en el modo con animación.
    bool fast = (args.size() > 2 && args[2] == "fast") ||
                CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"), SGIA_JUNK) == SGIA_ALL;

    int queued = 0;
    for (RandomizerGet rg : list) {
        // Una copia por NIVEL de cadena (WalkSpecOfRg); el dedup descarta niveles ya poseídos, así
        // que nunca hay repetidos.
        for (int i = 0; i < WalkSpecOfRg(rg).copies; i++) {
            if (fast) {
                if (OTRGlobals::Instance->gRandomizer->GetItemObtainabilityFromRandomizerGet(rg) != CAN_OBTAIN) {
                    continue;
                }
                GiveEntrySilently(Rando::StaticData::RetrieveItem(rg).GetGIEntry_Copy());
            } else {
                sGiveAllQueue.push_back(rg);
            }
            queued++;
        }
    }
    if (fast) {
        INFO_MESSAGE("[SOH] Gave %d items from \"%s\" with no animation.", queued, args[1].c_str());
        return 0;
    }
    EnsureGiveAllPump();
    INFO_MESSAGE("[SOH] Queued %d gives from \"%s\" — they present one after another "
                 "(give_all stop to cancel, give_all %s fast to skip the animation).",
                 queued, args[1].c_str(), args[1].c_str());
    return 0;
}

static bool EntranceHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                            std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }

    unsigned int entrance;

    try {
        entrance = std::stoi(args[1], nullptr, 16);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Entrance value must be a Hex number.");
        return 1;
    }

    gPlayState->nextEntranceIndex = entrance;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_INSTANT;
    gSaveContext.nextTransitionType = TRANS_TYPE_INSTANT;
    return 0;
}

static bool VoidHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                        std::string* output) {
    if (gPlayState != nullptr) {
        gSaveContext.respawn[RESPAWN_MODE_DOWN].tempSwchFlags = gPlayState->actorCtx.flags.tempSwch;
        gSaveContext.respawn[RESPAWN_MODE_DOWN].tempCollectFlags = gPlayState->actorCtx.flags.tempCollect;
        gSaveContext.respawnFlag = 1;
        gPlayState->transitionTrigger = TRANS_TRIGGER_START;
        gPlayState->nextEntranceIndex = gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex;
        gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    } else {
        ERROR_MESSAGE("gPlayState == nullptr");
        return 1;
    }
    return 0;
}

static bool ReloadHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                          std::string* output) {
    if (gPlayState != nullptr) {
        gPlayState->nextEntranceIndex = gSaveContext.entranceIndex;
        gPlayState->transitionTrigger = TRANS_TRIGGER_START;
        gPlayState->transitionType = TRANS_TYPE_INSTANT;
        gSaveContext.nextTransitionType = TRANS_TYPE_INSTANT;
    } else {
        ERROR_MESSAGE("gPlayState == nullptr");
        return 1;
    }
    return 0;
}

const static std::map<std::string, uint16_t> fw_options{ { "clear", 0 }, { "warp", 1 }, { "backup", 2 } };

static bool FWHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                      std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }

    const auto& it = fw_options.find(args[1]);
    if (it == fw_options.end()) {
        ERROR_MESSAGE("[SOH] Invalid option. Options are 'clear', 'warp', 'backup'");
        return 1;
    }

    if (gPlayState != nullptr) {
        FaroresWindData clear = {};
        switch (it->second) {
            case 0: // clear
                gSaveContext.fw = clear;
                INFO_MESSAGE("[SOH] Farore's wind point cleared! Reload scene to take effect.");
                return 0;
                break;
            case 1: // warp
                if (gSaveContext.respawn[RESPAWN_MODE_TOP].data > 0) {
                    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
                    gPlayState->nextEntranceIndex = gSaveContext.respawn[RESPAWN_MODE_TOP].entranceIndex;
                    gPlayState->transitionType = TRANS_TYPE_FADE_WHITE_FAST;
                } else {
                    ERROR_MESSAGE("Farore's wind not set!");
                    return 1;
                }
                return 0;
                break;
            case 2: // backup
                if (CVarGetInteger(CVAR_ENHANCEMENT("BetterFarore"), 0)) {
                    gSaveContext.fw = gSaveContext.ship.backupFW;
                    gSaveContext.fw.set = 1;
                    INFO_MESSAGE("[SOH] Backup FW data copied! Reload scene to take effect.");
                    return 0;
                } else {
                    ERROR_MESSAGE("Better Farore's Wind isn't turned on!");
                    return 1;
                }
                break;
        }
    } else {
        ERROR_MESSAGE("gPlayState == nullptr");
        return 1;
    }

    return 0;
}

static bool FileSelectHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                              std::string* output) {
    if (gGameState == nullptr) {
        ERROR_MESSAGE("gGameState == nullptr");
        return 1;
    }

    gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
    SET_NEXT_GAMESTATE(gGameState, FileChoose_Init, FileChooseContext);
    gGameState->running = false;
    return 0;
}

static bool QuitHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                        std::string* output) {
    Ship::Context::GetRawInstance()->GetWindow()->Close();
    return 0;
}

static bool SaveStateHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                             std::string* output) {
    unsigned int slot = OTRGlobals::Instance->gSaveStateMgr->GetCurrentSlot();
    const SaveStateReturn rtn = OTRGlobals::Instance->gSaveStateMgr->AddRequest({ slot, RequestType::SAVE });

    switch (rtn) {
        case SaveStateReturn::SUCCESS:
            INFO_MESSAGE("[SOH] Saved state to slot %u", slot);
            return 0;
        case SaveStateReturn::FAIL_WRONG_GAMESTATE:
            ERROR_MESSAGE("[SOH] Can not save a state outside of \"GamePlay\"");
            return 1;
        default:
            return 1;
    }
}

static bool LoadStateHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                             std::string* output) {
    unsigned int slot = OTRGlobals::Instance->gSaveStateMgr->GetCurrentSlot();
    const SaveStateReturn rtn = OTRGlobals::Instance->gSaveStateMgr->AddRequest({ slot, RequestType::LOAD });

    switch (rtn) {
        case SaveStateReturn::SUCCESS:
            INFO_MESSAGE("[SOH] Loaded state from slot (%u)", slot);
            return 0;
        case SaveStateReturn::FAIL_INVALID_SLOT:
            ERROR_MESSAGE("[SOH] Invalid State Slot Number (%u)", slot);
            return 1;
        case SaveStateReturn::FAIL_STATE_EMPTY:
            ERROR_MESSAGE("[SOH] State Slot (%u) is empty", slot);
            return 1;
        case SaveStateReturn::FAIL_WRONG_GAMESTATE:
            ERROR_MESSAGE("[SOH] Can not load a state outside of \"GamePlay\"");
            return 1;
        default:
            return 1;
    }
}

static bool StateSlotSelectHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                                   std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    uint8_t slot;

    try {
        slot = std::stoi(args[1], nullptr, 10);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] SaveState slot value must be a number.");
        return 1;
    }

    if (slot < 0) {
        ERROR_MESSAGE("[SOH] Invalid slot passed. Slot must be between 0 and 2");
        return 1;
    }

    OTRGlobals::Instance->gSaveStateMgr->SetCurrentSlot(slot);
    INFO_MESSAGE("[SOH] Slot %u selected", OTRGlobals::Instance->gSaveStateMgr->GetCurrentSlot());
    return 0;
}

static bool InvisibleHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                             std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    uint8_t state;

    try {
        state = std::stoi(args[1], nullptr, 10) == 0 ? 0 : 1;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Invisible value must be a number.");
        return 1;
    }

    GameInteractionEffect::InvisibleLink effect;
    GameInteractionEffectQueryResult result =
        state ? GameInteractor::ApplyEffect(effect) : GameInteractor::RemoveEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Invisible Link %s", state ? "enabled" : "disabled");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not %s Invisible Link.", state ? "enable" : "disable");
        return 1;
    }
}

static bool GiantLinkHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                             std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    uint8_t state;

    try {
        state = std::stoi(args[1], nullptr, 10) == 0 ? 0 : 1;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Giant value must be a number.");
        return 1;
    }

    GameInteractionEffect::ModifyLinkSize effect;
    effect.parameters[0] = GI_LINK_SIZE_GIANT;
    GameInteractionEffectQueryResult result =
        state ? GameInteractor::ApplyEffect(effect) : GameInteractor::RemoveEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Giant Link %s", state ? "enabled" : "disabled");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not %s Giant Link.", state ? "enable" : "disable");
        return 1;
    }
}

static bool MinishLinkHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                              std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    uint8_t state;

    try {
        state = std::stoi(args[1], nullptr, 10) == 0 ? 0 : 1;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Minish value must be a number.");
        return 1;
    }

    GameInteractionEffect::ModifyLinkSize effect;
    effect.parameters[0] = GI_LINK_SIZE_MINISH;
    GameInteractionEffectQueryResult result =
        state ? GameInteractor::ApplyEffect(effect) : GameInteractor::RemoveEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Minish Link %s", state ? "enabled" : "disabled");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not %s Minish Link.", state ? "enable" : "disable");
        return 1;
    }
}

static bool AddHeartContainerHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                                     std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    int hearts;

    try {
        hearts = std::stoi(args[1]);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Hearts value must be an integer.");
        return 1;
    }

    if (hearts < 0) {
        ERROR_MESSAGE("[SOH] Hearts value must be a positive integer");
        return 1;
    }

    GameInteractionEffect::ModifyHeartContainers effect;
    effect.parameters[0] = hearts;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Added %d heart containers", hearts);
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not add heart containers.");
        return 1;
    }
}

static bool RemoveHeartContainerHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                                        std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    int hearts;

    try {
        hearts = std::stoi(args[1]);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Hearts value must be an integer.");
        return 1;
    }

    if (hearts < 0) {
        ERROR_MESSAGE("[SOH] Hearts value must be a positive integer");
        return 1;
    }

    GameInteractionEffect::ModifyHeartContainers effect;
    effect.parameters[0] = -hearts;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Removed %d heart containers", hearts);
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not remove heart containers.");
        return 1;
    }
}

static bool GravityHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                           std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }

    GameInteractionEffect::ModifyGravity effect;

    try {
        effect.parameters[0] = static_cast<int32_t>(Ship::Math::clamp(
            static_cast<float>(std::stoi(args[1], nullptr, 10)), GI_GRAVITY_LEVEL_LIGHT, GI_GRAVITY_LEVEL_HEAVY));
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Gravity value must be a number.");
        return 1;
    }

    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Updated gravity.");
        return 0;
    } else {
        ERROR_MESSAGE("[SOH] Command failed: Could not update gravity.");
        return 1;
    }
}

static bool NoUIHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                        std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    uint8_t state;

    try {
        state = std::stoi(args[1], nullptr, 10) == 0 ? 0 : 1;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] No UI value must be a number.");
        return 1;
    }

    GameInteractionEffect::NoUI effect;
    GameInteractionEffectQueryResult result =
        state ? GameInteractor::ApplyEffect(effect) : GameInteractor::RemoveEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] No UI %s", state ? "enabled" : "disabled");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not %s No UI.", state ? "enable" : "disable");
        return 1;
    }
}

static bool FreezeHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                          std::string* output) {
    GameInteractionEffect::FreezePlayer effect;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Player frozen");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not freeze player.");
        return 1;
    }
}

static bool DefenseModifierHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                                   std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    GameInteractionEffect::ModifyDefenseModifier effect;

    try {
        effect.parameters[0] = std::stoi(args[1], nullptr, 10);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Defense modifier value must be a number.");
        return 1;
    }

    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Defense modifier set to %d", effect.parameters[0]);
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not set defense modifier.");
        return 1;
    }
}

static bool DamageHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                          std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    GameInteractionEffect::ModifyHealth effect;

    try {
        int value = std::stoi(args[1], nullptr, 10);
        if (value < 0) {
            ERROR_MESSAGE("[SOH] Invalid value passed. Value must be greater than 0");
            return 1;
        }

        effect.parameters[0] = -value;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Damage value must be a number.");
        return 1;
    }

    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Player damaged");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not damage player.");
        return 1;
    }
}

static bool HealHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                        std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    GameInteractionEffect::ModifyHealth effect;

    try {
        int value = std::stoi(args[1], nullptr, 10);
        if (value < 0) {
            ERROR_MESSAGE("[SOH] Invalid value passed. Value must be greater than 0");
            return 1;
        }

        effect.parameters[0] = value;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Damage value must be a number.");
        return 1;
    }

    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Player healed");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not heal player.");
        return 1;
    }
}

static bool FillMagicHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                             std::string* output) {
    GameInteractionEffect::FillMagic effect;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Magic filled");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not fill magic.");
        return 1;
    }
}

static bool EmptyMagicHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                              std::string* output) {
    GameInteractionEffect::EmptyMagic effect;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Magic emptied");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not empty magic.");
        return 1;
    }
}

static bool NoZHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                       std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    uint8_t state;

    try {
        state = std::stoi(args[1], nullptr, 10) == 0 ? 0 : 1;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] NoZ value must be a number.");
        return 1;
    }

    GameInteractionEffect::DisableZTargeting effect;
    GameInteractionEffectQueryResult result =
        state ? GameInteractor::ApplyEffect(effect) : GameInteractor::RemoveEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] NoZ " + std::string(state ? "enabled" : "disabled"));
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not " + std::string(state ? "enable" : "disable") + " NoZ.");
        return 1;
    }
}

static bool OneHitKOHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                            std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    uint8_t state;

    try {
        state = std::stoi(args[1], nullptr, 10) == 0 ? 0 : 1;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] One-hit KO value must be a number.");
        return 1;
    }

    GameInteractionEffect::OneHitKO effect;
    GameInteractionEffectQueryResult result =
        state ? GameInteractor::ApplyEffect(effect) : GameInteractor::RemoveEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] One-hit KO " + std::string(state ? "enabled" : "disabled"));
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not " + std::string(state ? "enable" : "disable") + " One-hit KO.");
        return 1;
    }
}

static bool PacifistHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                            std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    uint8_t state;

    try {
        state = std::stoi(args[1], nullptr, 10) == 0 ? 0 : 1;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Pacifist value must be a number.");
        return 1;
    }

    GameInteractionEffect::PacifistMode effect;
    GameInteractionEffectQueryResult result =
        state ? GameInteractor::ApplyEffect(effect) : GameInteractor::RemoveEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Pacifist " + std::string(state ? "enabled" : "disabled"));
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not " + std::string(state ? "enable" : "disable") + " Pacifist.");
        return 1;
    }
}

static bool PaperLinkHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                             std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    uint8_t state;

    try {
        state = std::stoi(args[1], nullptr, 10) == 0 ? 0 : 1;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Paper Link value must be a number.");
        return 1;
    }

    GameInteractionEffect::ModifyLinkSize effect;
    effect.parameters[0] = GI_LINK_SIZE_PAPER;
    GameInteractionEffectQueryResult result =
        state ? GameInteractor::ApplyEffect(effect) : GameInteractor::RemoveEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Paper Link " + std::string(state ? "enabled" : "disabled"));
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not " + std::string(state ? "enable" : "disable") + " Paper Link.");
        return 1;
    }
}

static bool RainstormHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                             std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    uint8_t state;

    try {
        state = std::stoi(args[1], nullptr, 10) == 0 ? 0 : 1;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Rainstorm value must be a number.");
        return 1;
    }

    GameInteractionEffect::WeatherRainstorm effect;
    GameInteractionEffectQueryResult result =
        state ? GameInteractor::ApplyEffect(effect) : GameInteractor::RemoveEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Rainstorm " + std::string(state ? "enabled" : "disabled"));
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not " + std::string(state ? "enable" : "disable") + " Rainstorm.");
        return 1;
    }
}

static bool ReverseControlsHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                                   std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    uint8_t state;

    try {
        state = std::stoi(args[1], nullptr, 10) == 0 ? 0 : 1;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Reverse controls value must be a number.");
        return 1;
    }

    GameInteractionEffect::ReverseControls effect;
    GameInteractionEffectQueryResult result =
        state ? GameInteractor::ApplyEffect(effect) : GameInteractor::RemoveEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Reverse controls " + std::string(state ? "enabled" : "disabled"));
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not " + std::string(state ? "enable" : "disable") +
                     " Reverse controls.");
        return 1;
    }
}

static bool UpdateRupeesHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                                std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    GameInteractionEffect::ModifyRupees effect;

    try {
        effect.parameters[0] = std::stoi(args[1], nullptr, 10);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Rupee value must be a number.");
        return 1;
    }

    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Rupees updated");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not update rupees.");
        return 1;
    }
}

static bool SpeedModifierHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                                 std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    GameInteractionEffect::ModifyMovementSpeedMultiplier effect;

    try {
        effect.parameters[0] = std::stoi(args[1], nullptr, 10);
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Speed modifier value must be a number.");
        return 1;
    }

    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Speed modifier updated");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not update speed modifier.");
        return 1;
    }
}

const static std::map<std::string, uint16_t> boots{
    { "kokiri", EQUIP_VALUE_BOOTS_KOKIRI },
    { "iron", EQUIP_VALUE_BOOTS_IRON },
    { "hover", EQUIP_VALUE_BOOTS_HOVER },
};

static bool BootsHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                         std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }

    const auto& it = boots.find(args[1]);
    if (it == boots.end()) {
        ERROR_MESSAGE("Invalid boot type. Options are 'kokiri', 'iron' and 'hover'");
        return 1;
    }

    GameInteractionEffect::ForceEquipBoots effect;
    effect.parameters[0] = it->second;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Boots updated.");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not update boots.");
        return 1;
    }
}

const static std::map<std::string, uint16_t> shields{
    { "deku", ITEM_SHIELD_DEKU },
    { "hylian", ITEM_SHIELD_HYLIAN },
    { "mirror", ITEM_SHIELD_MIRROR },
};

static bool GiveShieldHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                              std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }

    const auto& it = shields.find(args[1]);
    if (it == shields.end()) {
        ERROR_MESSAGE("Invalid shield type. Options are 'deku', 'hylian' and 'mirror'");
        return 1;
    }

    GameInteractionEffect::GiveOrTakeShield effect;
    effect.parameters[0] = it->second;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Gave shield.");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not give shield.");
        return 1;
    }
}

static bool TakeShieldHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                              std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }

    const auto& it = shields.find(args[1]);
    if (it == shields.end()) {
        ERROR_MESSAGE("Invalid shield type. Options are 'deku', 'hylian' and 'mirror'");
        return 1;
    }

    GameInteractionEffect::GiveOrTakeShield effect;
    effect.parameters[0] = it->second * -1;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Took shield.");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not take shield.");
        return 1;
    }
}

static bool KnockbackHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                             std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }
    GameInteractionEffect::KnockbackPlayer effect;

    try {
        int value = std::stoi(args[1], nullptr, 10);
        if (value < 0) {
            ERROR_MESSAGE("[SOH] Invalid value passed. Value must be greater than 0");
            return 1;
        }

        effect.parameters[0] = value;
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] Knockback value must be a number.");
        return 1;
    }

    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);
    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Knockback applied");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not apply knockback.");
        return 1;
    }
}

static bool ElectrocuteHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                               std::string* output) {
    GameInteractionEffect::ElectrocutePlayer effect;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Electrocuted player");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not electrocute player.");
        return 1;
    }
}

static bool BurnHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                        std::string* output) {
    GameInteractionEffect::BurnPlayer effect;
    GameInteractionEffectQueryResult result = GameInteractor::ApplyEffect(effect);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Burned player");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not burn player.");
        return 1;
    }
}

static bool CuccoStormHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                              std::string* output) {
    GameInteractionEffectQueryResult result = GameInteractor::RawAction::SpawnActor(ACTOR_EN_NIW, 0);

    if (result == GameInteractionEffectQueryResult::Possible) {
        INFO_MESSAGE("[SOH] Spawned cucco storm");
        return 0;
    } else {
        INFO_MESSAGE("[SOH] Command failed: Could not spawn cucco storm.");
        return 1;
    }
}

static bool GenerateRandoHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                                 std::string* output) {
    if (args.size() == 1) {
        if (GenerateRandomizer()) {
            return 0;
        }
    }

    try {
        uint32_t value = std::stoi(args[1], NULL, 10);
        std::string seed = "";
        if (args.size() == 3) {
            int testing = std::stoi(args[1], nullptr, 10);
            seed = "seed_testing_count";
        }

        if (GenerateRandomizer(seed + std::to_string(value))) {
            return 0;
        }
    } catch ([[maybe_unused]] std::invalid_argument const& ex) {
        ERROR_MESSAGE("[SOH] seed|count value must be a number.");
        return 1;
    }

    ERROR_MESSAGE("[SOH] Rando generation already in progress");
    return 1;
}

static constexpr std::array<std::pair<const char*, CosmeticGroup>, COSMETICS_GROUP_MAX> cosmetic_groups = { {
    { "link", COSMETICS_GROUP_LINK },
    { "mirror_shield", COSMETICS_GROUP_MIRRORSHIELD },
    { "swords", COSMETICS_GROUP_SWORDS },
    { "gloves", COSMETICS_GROUP_GLOVES },
    { "equipment", COSMETICS_GROUP_EQUIPMENT },
    { "keyring", COSMETICS_GROUP_KEYRING },
    { "small_keys", COSMETICS_GROUP_SMALL_KEYS },
    { "boss_keys", COSMETICS_GROUP_BOSS_KEYS },
    { "consumable", COSMETICS_GROUP_CONSUMABLE },
    { "hud", COSMETICS_GROUP_HUD },
    { "kaleido", COSMETICS_GROUP_KALEIDO },
    { "title", COSMETICS_GROUP_TITLE },
    { "npc", COSMETICS_GROUP_NPC },
    { "world", COSMETICS_GROUP_WORLD },
    { "magic", COSMETICS_GROUP_MAGIC },
    { "arrows", COSMETICS_GROUP_ARROWS },
    { "spin_attack", COSMETICS_GROUP_SPIN_ATTACK },
    { "trials", COSMETICS_GROUP_TRAILS },
    { "navi", COSMETICS_GROUP_NAVI },
    { "ivan", COSMETICS_GROUP_IVAN },
    { "message", COSMETICS_GROUP_MESSAGE },
} };

static bool CosmeticsHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                             std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }

    if (args[1].compare("reset") == 0) {
        if (args.size() == 2) {
            CosmeticsEditor_ResetAll();
        } else {
            for (const auto& [key, value] : cosmetic_groups) {
                if (args[2].compare(key) == 0) {
                    CosmeticsEditor_ResetGroup(value);
                    return 0;
                }
            }
            ERROR_MESSAGE("[SOH] Invalid argument passed, unrecognized group name");
            return 1;
        }
    } else if (args[1].compare("randomize") == 0) {
        if (args.size() == 2) {
            CosmeticsEditor_RandomizeAll();
        } else {
            for (const auto& [key, value] : cosmetic_groups) {
                if (args[2].compare(key) == 0) {
                    CosmeticsEditor_RandomizeGroup(value);
                    return 0;
                }
            }
            ERROR_MESSAGE("[SOH] Invalid argument passed, unrecognized group name");
            return 1;
        }
    } else {
        ERROR_MESSAGE("[SOH] Invalid argument passed, must be 'reset' or 'randomize'");
        return 1;
    }

    return 0;
}

static std::map<std::string, SeqType> sfx_groups = {
    { "bgm", SEQ_BGM_WORLD },     { "fanfares", SEQ_FANFARE }, { "events", SEQ_BGM_EVENT },
    { "battle", SEQ_BGM_BATTLE }, { "ocarina", SEQ_OCARINA },  { "instruments", SEQ_INSTRUMENT },
    { "sfx", SEQ_SFX },           { "voices", SEQ_VOICE },     { "custom", SEQ_BGM_CUSTOM },
};

static bool SfxHandler(std::shared_ptr<Ship::Console> Console, const std::vector<std::string>& args,
                       std::string* output) {
    if (args.size() < 2) {
        ERROR_MESSAGE("[SOH] Unexpected arguments passed");
        return 1;
    }

    if (args[1].compare("reset") == 0) {
        if (args.size() == 2) {
            AudioEditor_ResetAll();
        } else {
            for (const auto& [key, value] : sfx_groups) {
                if (args[2].compare(key) == 0) {
                    AudioEditor_ResetGroup(value);
                    return 0;
                }
            }
            ERROR_MESSAGE("[SOH] Invalid argument passed, unrecognized group name");
            return 1;
        }
    } else if (args[1].compare("randomize") == 0) {
        if (args.size() == 2) {
            AudioEditor_RandomizeAll();
        } else {
            for (const auto& [key, value] : sfx_groups) {
                if (args[2].compare(key) == 0) {
                    AudioEditor_RandomizeGroup(value);
                    return 0;
                }
            }
            ERROR_MESSAGE("[SOH] Invalid argument passed, unrecognized group name");
            return 1;
        }
    } else {
        ERROR_MESSAGE("[SOH] Invalid argument passed, must be 'reset' or 'randomize'");
        return 1;
    }

    return 0;
}

static bool AvailableChecksProcessUndiscoveredExitsHandler(std::shared_ptr<Ship::Console> Console,
                                                           const std::vector<std::string>& args, std::string* output) {
    const auto& logic = Rando::Context::GetInstance()->GetLogic();
    bool enabled = false;

    if (args.size() == 1) {
        enabled = !logic->ACProcessUndiscoveredExits;
    } else {
        try {
            enabled = std::stoi(args[1]);
        } catch ([[maybe_unused]] std::invalid_argument const& ex) {
            ERROR_MESSAGE("[SOH] Enable should be 0 or 1");
            return 1;
        }
    }

    logic->ACProcessUndiscoveredExits = enabled;
    INFO_MESSAGE("[SOH] Available Checks - Process Undiscovered Exits %s",
                 logic->ACProcessUndiscoveredExits ? "enabled" : "disabled");

    CheckTracker::RecalculateAvailableChecks();
    return 0;
}

static bool AvailableChecksRecalculateHandler(std::shared_ptr<Ship::Console> Console,
                                              const std::vector<std::string>& args, std::string* output) {
    RandomizerRegion startingRegion = RR_ROOT;
    RandoAgeTime startingAgeTime = RAT_NONE;

    if (args.size() > 1) {
        try {
            startingRegion = static_cast<RandomizerRegion>(std::stoi(args[1]));
        } catch ([[maybe_unused]] std::invalid_argument const& ex) {
            ERROR_MESSAGE("[SOH] Region should be a number");
            return 1;
        }

        if (startingRegion <= RR_NONE || startingRegion >= RR_MAX) {
            ERROR_MESSAGE("[SOH] Region should be between 1 and %d", RR_MAX - 1);
            return 1;
        }
    }

    if (args.size() > 2) {
        if (args[2] == "ChildDay") {
            startingAgeTime = RAT_CHILD_DAY;
        } else if (args[2] == "ChildNight") {
            startingAgeTime = RAT_CHILD_NIGHT;
        } else if (args[2] == "AdultDay") {
            startingAgeTime = RAT_ADULT_DAY;
        } else if (args[2] == "AdultNight") {
            startingAgeTime = RAT_ADULT_NIGHT;
        } else {
            ERROR_MESSAGE("[SOH] Age Time should be ChildDay, ChildNight, AdultDay, or AdultNight");
        }
    }

    CheckTracker::RecalculateAvailableChecks(startingRegion, startingAgeTime);
    return 0;
}

void DebugConsole_Init(void) {
    // Console
    CMD_REGISTER("file_select", { FileSelectHandler, "Returns to the file select." });
    CMD_REGISTER("reset", { ResetHandler, "Resets the game." });
    CMD_REGISTER("quit", { QuitHandler, "Quits the game." });

    // Save States
    CMD_REGISTER("save_state", { SaveStateHandler, "Save a state." });
    CMD_REGISTER("load_state", { LoadStateHandler, "Load a state." });
    CMD_REGISTER("set_slot", { StateSlotSelectHandler,
                               "Selects a SaveState slot",
                               {
                                   { "Slot number", Ship::ArgumentType::NUMBER },
                               } });

    // Map & Location
    CMD_REGISTER("void", { VoidHandler, "Voids out of the current map." });
    CMD_REGISTER("reload", { ReloadHandler, "Reloads the current map." });
    CMD_REGISTER("fw", { FWHandler,
                         "Spawns the player where Farore's Wind is set.",
                         {
                             { "clear|warp|backup", Ship::ArgumentType::TEXT },
                         } });
    CMD_REGISTER("entrance", { EntranceHandler,
                               "Sends player to the entered entrance (hex)",
                               {
                                   { "entrance", Ship::ArgumentType::NUMBER },
                               } });

    // Gameplay
    CMD_REGISTER("kill", { KillPlayerHandler, "Commit suicide." });

    CMD_REGISTER("map", { LoadSceneHandler, "Load up kak?" });

    CMD_REGISTER("rupee", { RupeeHandler,
                            "Set your rupee counter.",
                            {
                                { "amount", Ship::ArgumentType::NUMBER },
                            } });

    CMD_REGISTER("bItem", { BHandler,
                            "Set an item to the B button.",
                            {
                                { "Item ID", Ship::ArgumentType::NUMBER },
                            } });

    CMD_REGISTER("spawn",
                 { ActorSpawnHandler,
                   "Spawn an actor.",
                   {
                       { "actor name/id", Ship::ArgumentType::NUMBER }, // TODO there should be an actor_id arg type
                       { "data", Ship::ArgumentType::NUMBER },
                       { "x", Ship::ArgumentType::NUMBER, true },
                       { "y", Ship::ArgumentType::NUMBER, true },
                       { "z", Ship::ArgumentType::NUMBER, true },
                       { "rx", Ship::ArgumentType::NUMBER, true },
                       { "ry", Ship::ArgumentType::NUMBER, true },
                       { "rz", Ship::ArgumentType::NUMBER, true },
                   } });

    CMD_REGISTER("pos", { SetPosHandler,
                          "Sets the position of the player.",
                          {
                              { "x", Ship::ArgumentType::NUMBER, true },
                              { "y", Ship::ArgumentType::NUMBER, true },
                              { "z", Ship::ArgumentType::NUMBER, true },
                          } });

    CMD_REGISTER("addammo", { AddAmmoHandler,
                              "Adds ammo of an item.",
                              {
                                  { "sticks|nuts|bombs|seeds|arrows|bombchus|beans", Ship::ArgumentType::TEXT },
                                  { "count", Ship::ArgumentType::NUMBER },
                              } });

    CMD_REGISTER("takeammo", { TakeAmmoHandler,
                               "Removes ammo of an item.",
                               {
                                   { "sticks|nuts|bombs|seeds|arrows|bombchus|beans", Ship::ArgumentType::TEXT },
                                   { "count", Ship::ArgumentType::NUMBER },
                               } });

    CMD_REGISTER("bottle", { BottleHandler,
                             "Changes item in a bottle slot.",
                             {
                                 { "item", Ship::ArgumentType::TEXT },
                                 { "slot", Ship::ArgumentType::NUMBER },
                             } });

    CMD_REGISTER("give_item", { GiveItemHandler,
                                "Gives an item to the player as if it was given from an actor",
                                {
                                    { "vanilla|randomizer", Ship::ArgumentType::TEXT },
                                    { "giveItemID", Ship::ArgumentType::NUMBER },
                                } });

    // give por nombre + recorrido de validación (Skijer's NEI). Mismos comandos y MISMAS 13
    // categorías que 2ship; la lista completa por categoría vive en GIVE_CATEGORIES.md (raíz).
    CMD_REGISTER("give", { GiveByNameHandler,
                           "Gives an item by name, with its get-item presentation. `give list <filter>` lists "
                           "matches. Skijer's NEI",
                           { { "item name", Ship::ArgumentType::TEXT } } });
    CMD_REGISTER("give_next", { GiveNextHandler,
                                "Gives the next item of the walk, with its presentation.",
                                { { "category", Ship::ArgumentType::TEXT, true } } });
    CMD_REGISTER(
        "give_prev",
        { GivePrevHandler, "Goes back one item in the walk.", { { "category", Ship::ArgumentType::TEXT, true } } });
    CMD_REGISTER("give_again", { GiveAgainHandler,
                                 "Gives the CURRENT item again — next level of a progressive chain.",
                                 { { "category", Ship::ArgumentType::TEXT, true } } });
    CMD_REGISTER("give_reset", { GiveResetHandler,
                                 "Restarts the walk, optionally on another category.",
                                 { { "category", Ship::ArgumentType::TEXT, true } } });
    CMD_REGISTER("give_all",
                 { GiveAllHandler,
                   "Gives every item of a category, one presentation after another. Add "
                   "\"fast\" (or set Skip Get Item Animation to All Items) for no animation.",
                   { { "category", Ship::ArgumentType::TEXT }, { "fast", Ship::ArgumentType::TEXT, true } } });

    CMD_REGISTER("item", { ItemHandler,
                           "Sets item ID in arg 1 into slot arg 2. No boundary checks. Use with caution.",
                           {
                               { "slot", Ship::ArgumentType::NUMBER },
                               { "item id", Ship::ArgumentType::NUMBER },
                           } });

    CMD_REGISTER("invisible", { InvisibleHandler,
                                "Activate Link's Elvish cloak, making him appear invisible.",
                                {
                                    { "value", Ship::ArgumentType::NUMBER },
                                } });

    CMD_REGISTER("giant_link", { GiantLinkHandler,
                                 "Turn Link into a giant Lonky boi.",
                                 {
                                     { "value", Ship::ArgumentType::NUMBER },
                                 } });

    CMD_REGISTER("minish_link", { MinishLinkHandler,
                                  "Turn Link into a minish boi.",
                                  {
                                      { "value", Ship::ArgumentType::NUMBER },
                                  } });

    CMD_REGISTER("add_heart_container",
                 { AddHeartContainerHandler, "Give Link a heart! The maximum amount of hearts is 20!" });

    CMD_REGISTER("remove_heart_container",
                 { RemoveHeartContainerHandler, "Remove a heart from Link. The minimal amount of hearts is 3." });

    CMD_REGISTER("gravity", { GravityHandler,
                              "Set gravity level.",
                              {
                                  { "value", Ship::ArgumentType::NUMBER },
                              } });

    CMD_REGISTER("no_ui", { NoUIHandler,
                            "Disables the UI.",
                            {
                                { "value", Ship::ArgumentType::NUMBER },
                            } });

    CMD_REGISTER("freeze", { FreezeHandler, "Freezes Link in place" });

    CMD_REGISTER("defense_modifier", { DefenseModifierHandler,
                                       "Sets the defense modifier.",
                                       {
                                           { "value", Ship::ArgumentType::NUMBER },
                                       } });

    CMD_REGISTER("damage", { DamageHandler,
                             "Deal damage to Link.",
                             {
                                 { "value", Ship::ArgumentType::NUMBER },
                             } });

    CMD_REGISTER("heal", { HealHandler,
                           "Heals Link.",
                           {
                               { "value", Ship::ArgumentType::NUMBER },
                           } });

    CMD_REGISTER("fill_magic", { FillMagicHandler, "Fills magic." });

    CMD_REGISTER("empty_magic", { EmptyMagicHandler, "Empties magic." });

    CMD_REGISTER("no_z", { NoZHandler,
                           "Disables Z-button presses.",
                           {
                               { "value", Ship::ArgumentType::NUMBER },
                           } });

    CMD_REGISTER("ohko", { OneHitKOHandler,
                           "Activates one hit KO. Any damage kills Link and he cannot gain health in this mode.",
                           {
                               { "value", Ship::ArgumentType::NUMBER },
                           } });

    CMD_REGISTER("pacifist", { PacifistHandler,
                               "Activates pacifist mode. Prevents Link from using his weapon.",
                               {
                                   { "value", Ship::ArgumentType::NUMBER },
                               } });

    CMD_REGISTER("paper_link", { PaperLinkHandler,
                                 "Link but made out of paper.",
                                 {
                                     { "value", Ship::ArgumentType::NUMBER },
                                 } });

    CMD_REGISTER("rainstorm", { RainstormHandler, "Activates rainstorm." });

    CMD_REGISTER("reverse_controls", { ReverseControlsHandler,
                                       "Reverses the controls.",
                                       {
                                           { "value", Ship::ArgumentType::NUMBER },
                                       } });

    CMD_REGISTER("update_rupees", { UpdateRupeesHandler,
                                    "Adds rupees.",
                                    {
                                        { "value", Ship::ArgumentType::NUMBER },
                                    } });

    CMD_REGISTER("speed_modifier", { SpeedModifierHandler,
                                     "Sets the speed modifier.",
                                     {
                                         { "value", Ship::ArgumentType::NUMBER },
                                     } });

    CMD_REGISTER("boots", { BootsHandler,
                            "Activates boots.",
                            {
                                { "kokiri|iron|hover", Ship::ArgumentType::TEXT },
                            } });

    CMD_REGISTER("giveshield", { GiveShieldHandler,
                                 "Gives a shield and equips it when Link is the right age for it.",
                                 {
                                     { "deku|hylian|mirror", Ship::ArgumentType::TEXT },
                                 } });

    CMD_REGISTER("takeshield", { TakeShieldHandler,
                                 "Takes a shield and unequips it if Link is wearing it.",
                                 {
                                     { "deku|hylian|mirror", Ship::ArgumentType::TEXT },
                                 } });

    CMD_REGISTER("knockback", { KnockbackHandler,
                                "Knocks Link back.",
                                {
                                    { "value", Ship::ArgumentType::NUMBER },
                                } });

    CMD_REGISTER("electrocute", { ElectrocuteHandler, "Electrocutes Link." });

    CMD_REGISTER("burn", { BurnHandler, "Burns Link." });

    CMD_REGISTER("cucco_storm", { CuccoStormHandler, "Cucco Storm" });

    CMD_REGISTER("gen_rando", { GenerateRandoHandler,
                                "Generate a randomizer seed",
                                {
                                    { "seed|count", Ship::ArgumentType::NUMBER, true },
                                    { "testing", Ship::ArgumentType::NUMBER, true },
                                } });

    CMD_REGISTER("cosmetics", { CosmeticsHandler,
                                "Change cosmetics.",
                                {
                                    { "reset|randomize", Ship::ArgumentType::TEXT },
                                    { "group name", Ship::ArgumentType::TEXT, true },
                                } });

    CMD_REGISTER("sfx", { SfxHandler,
                          "Change SFX.",
                          {
                              { "reset|randomize", Ship::ArgumentType::TEXT },
                              { "group_name", Ship::ArgumentType::TEXT, true },
                          } });

    CMD_REGISTER("acpue", { AvailableChecksProcessUndiscoveredExitsHandler,
                            "Available Checks - Process Undiscovered Exits",
                            { { "enable", Ship::ArgumentType::NUMBER, true } } });

    Ship::Context::GetRawInstance()->GetConsole()->AddCommand(
        "acr", { AvailableChecksRecalculateHandler,
                 "Available Checks - Recalculate",
                 {
                     { "starting_region", Ship::ArgumentType::NUMBER, true },
                     { "ChildDay|ChildNight|AdultDay|AdultNight", Ship::ArgumentType::TEXT, true },
                 } });

    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}
